// Adapted from DekUI's emulator-side presentation pipeline.
// Copyright 2026 MM3D presentation contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include "oot3d/interpolation_rate.h"
#include <cstdint>
#include <mutex>
#include <optional>
#include "oot3d/interpolation_math.h"
namespace OOT3D::Interpolation {
using Math::Matrix;
inline std::atomic<bool> enabled{true}, preview{false}, present_60{true}, cpu_vertices{false};
inline std::atomic<bool> settings_dirty{false};
inline std::atomic<float> phase{.5f},distance_limit{250},angle_limit{45};
inline std::atomic<unsigned> generation{0},samples{0},pairs{0},applied{0},rejected{0};
inline std::atomic<unsigned> replayed_draws{0},presented_pairs{0},fallback_frames{0};
enum class ReplayFailure {None,Sample,CameraMismatch,History,LensCut,CameraCut,DrawLens,DrawCamera,
                          Target,Draw,Viewport,Cpu,Layer,Count};
inline constexpr std::array ReplayFailureNames{"None","Missing/stale camera sample","Camera mismatch",
    "Waiting for history","Lens cut","Camera cut","Different draw lens","Different draw camera",
    "Different render target","Draw failure","Viewport","CPU replay","Replay layer"};
inline std::atomic<ReplayFailure> last_failure{ReplayFailure::None};
enum class CaptureResult {Off,WorldInactive,OtherView,ReadFailure,InvalidTransform,Ready};
inline constexpr std::array CaptureResultNames{"Off","World/menu gate","Other View or viewport bounds",
    "Unreadable view matrix","Invalid view transform","Captured world camera"};
inline std::atomic<CaptureResult> capture_result{CaptureResult::Off};
inline std::atomic<unsigned> native_lens_draws{0},warped_lens_draws{0},attached_view_draws{0};
// Count the first failure per guest frame, not every subsequent draw. This
// distinguishes unavailable scene history from renderer/projection failures.
inline std::array<std::atomic<unsigned>,unsigned(ReplayFailure::Count)> replay_failures{};
inline std::atomic<std::uint64_t> guest_frames{0};
inline std::atomic<std::int64_t> guest_time_us{0};
// Presentation fixes such as waterfall fog also need the scene when replay is off.
inline std::atomic<std::uint32_t> native_scene{~0u};
inline bool Replaying() {return enabled.load() && present_60.load();}
struct CameraSample {
    Matrix view{};
    std::uint32_t owner{},scene{};
    unsigned epoch{};
    std::int64_t time{};
    bool valid{};
    // The native View builds three lenses (scene, distant geometry, and the
    // fixed-FOV draw path) and a camera-relative view for foreground models.
    // Keep their identities instead of assuming every draw uses one lens.
    std::array<Matrix,3> projections{};
    bool projections_valid{};
    Matrix attached_view{};
    bool attached_valid{};
    // Shop buying cameras cut on entry/exit even below geometric cut limits.
    std::uint32_t cut_group{};
    // Exit-only cut policy: entering or remaining in first person still pairs.
    bool first_person{};
    float native_fov{};
};
// Native camera modes: free look, adult aim (bow/hookshot), child aim
// (slingshot). Z aiming, hookshot travel and boomerang aim are third person.
inline bool FirstPersonMode(std::uint32_t mode) {
    return mode==6 || mode==7 || mode==11;
}
inline std::mutex camera_mutex;
inline CameraSample camera_sample;
inline void Publish(CameraSample value) {std::scoped_lock lock{camera_mutex};camera_sample=value;}
inline CameraSample CameraSnapshot() {std::scoped_lock lock{camera_mutex};return camera_sample;}
struct ReplayDiagnostic {
    Matrix camera{},projection{};
    CameraSample sample{};
    std::int64_t time{};
};
inline std::mutex diagnostic_mutex;
inline ReplayDiagnostic diagnostic;
inline void RecordDiagnostic(const Matrix& camera,const Matrix& projection,
                             const CameraSample& sample,std::int64_t time) {
    std::scoped_lock lock(diagnostic_mutex);diagnostic={camera,projection,sample,time};
}
inline ReplayDiagnostic DiagnosticSnapshot() {std::scoped_lock lock(diagnostic_mutex);return diagnostic;}
enum class Presentation { Native, Pair, Skip };
enum class ReplayStatus {Off,Settings,Waiting,Incomplete,Cadence,Active};
inline std::atomic<ReplayStatus> replay_status{ReplayStatus::Off};
class PresentationPacer {
    std::optional<std::int64_t> next;
    int rate=60;
public:
    static constexpr std::int64_t Period=16666667;
    std::int64_t Deadline(std::int64_t now,bool paired,int fps=60) {
        fps=std::clamp(fps,30,120);
        if(fps!=rate){next.reset();rate=fps;}
        const std::int64_t period=(1000000000LL+fps/2)/fps;
        if(!paired){next.reset();return now;}
        // Late frames resume immediately; adding a new buffer delay here creates
        // visible hitches and grows queue latency. Rebase to avoid rapid catch-up.
        if(!next || now-*next>=period/4)next=now;
        const auto deadline=*next;
        *next+=period;
        return deadline;
    }
};
class PresentationClock {
    std::optional<std::uint64_t> frame;
    std::int64_t time{};
    unsigned stable{};
    bool paired{};
    bool guest_timed{};
public:
    void Reset() { frame.reset();stable=0;paired=false;guest_timed=false; }
    Presentation Next(std::uint64_t next,std::int64_t now,bool eligible,
                      std::optional<std::int64_t> guest_ms=std::nullopt) {
        if(frame && *frame==next)return paired?Presentation::Skip:Presentation::Native;
        const auto timestamp=guest_ms.value_or(now);
        const auto elapsed=timestamp-time;
        const bool continuous=frame && next==*frame+1 && guest_timed==guest_ms.has_value() &&
            elapsed>=25 && elapsed<=50;
        // Cadence describes the guest frames, including frames whose private
        // replay is incomplete. A pause/menu transition can temporarily lose
        // its midpoint while continuing at 30 Hz; requiring two more eligible
        // frames afterward hides most of a short opening/closing animation.
        // Use completed-buffer emulated timestamps when available. Host waits
        // (notably the pause backdrop capture) do not change the game's rate.
        // Eligibility still gates every pair; a guest 60 Hz interval, frame
        // gap, guest-time stall/rewind or clock-source change resets cadence.
        stable=continuous?std::min(stable+1,2u):0;
        frame=next;time=timestamp;guest_timed=guest_ms.has_value();paired=eligible && stable>=2;
        return paired?Presentation::Pair:Presentation::Native;
    }
};
}
