// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace OOT3D::Rumble {
inline std::atomic<bool> enabled{false}, dirty{false}, running{false};
inline std::atomic<int> strength{100};
inline std::atomic<int> output_status{0}; // 0 untested, 1 supported, -1 unavailable.
inline std::atomic<bool> sync_impacts_to_present{false};
using Clock = std::chrono::steady_clock;

// OoT's durations/decrease rates are in 60 Hz retraces, not game updates.
// Reference: zeldaret/oot src/code/z_rumble.c and ovl_player_actor/z_player.c.
// Modern motors receive the envelope amplitude instead of N64 on/off PWM.
struct Envelope {
    int amplitude = 0, hold = 0, decay = 1;
    Clock::time_point start{};
    int Sample(Clock::time_point now) const {
        const auto ticks = std::chrono::duration<double>(now - start).count() * 60.;
        return int(std::clamp(amplitude - std::max(0., ticks - hold) * decay,
                              0., double(amplitude)));
    }
};
struct Cue {int amplitude,hold,decay;};
inline Cue BombExplosion(float distance_to_player) {
    // En_Bom supplies horizontal distance. N64 passes that unsquared to a
    // distSq API and takes its square root, making falloff almost imperceptible.
    // Use actual world distance with the original 1000-unit attenuation range.
    if(!std::isfinite(distance_to_player) || distance_to_player<0 ||
        distance_to_player>=1000.f)return {0,20,150};
    const int distance=int(distance_to_player);
    return {255-distance*255/1000,20,150};
}

class Mixer {
public:
    void Clear() { envelopes = {}; override_envelope = {}; }
    // N64 Rumble_Override replaces the previous override, even with a weaker
    // pulse. Ordinary requests continue aging underneath it (sys_rumble.c).
    void Override(Cue cue, Clock::time_point now) {
        if (cue.amplitude <= 0 || cue.decay <= 0) return;
        override_envelope = {std::clamp(cue.amplitude, 0, 255),
                             std::clamp(cue.hold, 0, 255),
                             std::clamp(cue.decay, 1, 255), now};
    }
    void Request(int amplitude, int hold, int decay, Clock::time_point now) {
        if (amplitude <= 0 || decay <= 0) return;
        auto slot = std::min_element(envelopes.begin(), envelopes.end(),
            [now](const auto& a, const auto& b) { return a.Sample(now) < b.Sample(now); });
        if (slot->Sample(now) > amplitude) return;
        *slot = {std::clamp(amplitude, 0, 255), std::clamp(hold, 0, 255),
                 std::clamp(decay, 1, 255), now};
    }
    std::uint16_t Sample(Clock::time_point now, int percent) const {
        int amplitude = 0;
        for (const auto& e : envelopes) amplitude = std::max(amplitude, e.Sample(now));
        if (const int override_amplitude = override_envelope.Sample(now); override_amplitude > 0)
            amplitude = override_amplitude;
        return std::uint16_t(amplitude * 257 * std::clamp(percent, 0, 100) / 100);
    }
private:
    std::array<Envelope, 16> envelopes{};
    Envelope override_envelope{};
};
inline Mixer mixer;
// Collision logic can run ahead of the displayed image. Keep wall impacts
// pending until the corresponding native image is submitted for presentation;
// the interpolated midpoint still shows Link approaching the wall.
class ImpactQueue {
public:
    void Clear() { pending = {}; }
    void Add(std::uint64_t frame, Clock::time_point now, Cue cue={255,20,150}) {
        auto slot = std::find_if(pending.begin(), pending.end(),
            [](const auto& entry) { return !entry.active; });
        if (slot == pending.end()) slot = pending.begin();
        *slot = {true, frame, now, cue};
    }
    void Present(std::uint64_t frame, Clock::time_point now, Mixer& output) {
        for (auto& entry : pending) {
            if (!entry.active) continue;
            // Do not replay a backlog after a long stall or suspended window.
            if (now - entry.requested > std::chrono::milliseconds(500)) entry.active = false;
            else if (frame >= entry.frame) {
                output.Request(entry.cue.amplitude,entry.cue.hold,entry.cue.decay,now);
                entry.active = false;
            }
        }
    }
private:
    struct Entry { bool active{}; std::uint64_t frame{}; Clock::time_point requested{}; Cue cue{}; };
    std::array<Entry, 16> pending{};
};
inline ImpactQueue impacts;
struct FrameStamp { std::uint64_t frame{}, epoch{}; };
inline std::uint64_t epoch = 0;
inline std::mutex mutex;
inline void ClearLocked() { mixer.Clear(); impacts.Clear(); ++epoch; }
inline void Clear() { std::lock_guard lock{mutex}; ClearLocked(); }
inline FrameStamp CaptureFrame(std::uint64_t frame) {
    std::lock_guard lock{mutex}; return {frame, epoch};
}
inline void Presented(FrameStamp stamp) {
    std::lock_guard lock{mutex};
    if (stamp.epoch == epoch && enabled.load() && running.load())
        impacts.Present(stamp.frame, Clock::now(), mixer);
}
inline void Request(int amplitude, int hold, int decay) {
    std::lock_guard lock{mutex};
    if (enabled.load() && running.load()) mixer.Request(amplitude, hold, decay, Clock::now());
}
inline void Override(Cue cue) {
    std::lock_guard lock{mutex};
    if (enabled.load() && running.load()) mixer.Override(cue, Clock::now());
}
inline std::uint16_t Sample() {
    std::lock_guard lock{mutex};
    if (!enabled.load() || !running.load()) { ClearLocked(); return 0; }
    return mixer.Sample(Clock::now(), strength.load());
}
inline void Damage(std::int32_t change, unsigned health) {
    // First-pass approximation of normal hit feedback. Exclude healing, empty
    // health and single-unit elemental drains; do not scale with difficulty.
    if (health && change <= -4) Request(change <= -20 ? 180 : 120, 20,
                                       change <= -20 ? 100 : 10);
}
inline void PresentedCue(Cue cue,std::uint64_t completed_frame) {
    std::lock_guard lock{mutex};
    if (!enabled.load() || !running.load()) return;
    if (sync_impacts_to_present.load()) impacts.Add(completed_frame + 1, Clock::now(),cue);
    else mixer.Request(cue.amplitude,cue.hold,cue.decay,Clock::now());
}
inline void Bonk(std::uint64_t completed_frame = 0) {
    PresentedCue({255,20,150},completed_frame);
}
} // namespace OOT3D::Rumble
