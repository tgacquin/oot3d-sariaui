// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/user_camera.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>

namespace OOT3D::Camera {
using U32 = std::uint32_t;
// Live normal gameplay uses 52 degrees; the projection routine's zero-FOV
// fallback is 60 degrees and is not the gameplay reference.
constexpr float NativeFov = 52.f, MaximumFov = 77.f, FirstPersonMaximumFov = 62.f;
inline std::atomic<float> desired_fov{NativeFov};
inline std::atomic<bool> dirty{false};
inline float Sanitize(float value) {
    return std::isfinite(value) ? std::clamp(value, NativeFov, MaximumFov) : NativeFov;
}
inline float ConfiguredFov() {
    return UserCamera::Apply(UserCamera::Fov,Sanitize(desired_fov.load()),NativeFov,MaximumFov);
}
inline float Widen(float native, float configured) {
    if (!std::isfinite(native) || native <= 0.f || native >= 170.f) return native;
    configured = Sanitize(configured);
    if (configured == NativeFov) return native;
    constexpr float half_radians = 3.14159265358979323846f / 360.f;
    // Preserve scripted zooms by applying a relative change in projection space.
    return std::atan(std::tan(native * half_radians) *
                     std::tan(configured * half_radians) / std::tan(NativeFov * half_radians)) /
           half_radians;
}
constexpr U32 FirstSVC = 0xfff030;
struct Site { U32 pc, instruction, following; };
// USA Rev 1, primary and distant-geometry perspective projections. Both consume
// s16 without changing it: the secondary path may inherit the primary FOV.
constexpr std::array<Site, 2> Sites{{
    {0x478ebc, 0xee280a2b, 0xee200a2a},
    {0x478f60, 0xee280a2b, 0xedd4ba09},
}};
inline U32 Instruction(U32 pc, U32 word) {
    for (unsigned i = 0; i < Sites.size(); ++i)
        if (pc == Sites[i].pc && word == Sites[i].instruction)
            return 0xef000000 | (FirstSVC + i);
    return word;
}
template<class M> bool CodeMatches(M& memory) {
    for (const auto& site : Sites)
        if (memory.Read(site.pc) != std::optional<U32>{site.instruction} ||
            memory.Read(site.pc + 4) != std::optional<U32>{site.following}) return false;
    return memory.Read(0x478dec) == std::optional<U32>{0xe92d40f0} &&
           memory.Read(0x479258) == std::optional<U32>{std::bit_cast<U32>(60.f)} &&
           memory.Read(0x47925c) == std::optional<U32>{0x3c8efa35};
}
template<class M> bool GameplayView(M& memory, U32 view) {
    // Play publishes its current owner on every update. Reject other users of
    // the shared View routine (orthographic HUD, menus, model previews).
    const auto play = memory.Read(0x5043e0);
    if (!play || *play < 0x100000 || *play > 0x3ffffc00 || (*play & 3) ||
        view != *play + 0x188) return false;
    // View stores rotated bounds: 400 first, then 240.
    return memory.Read(view) == std::optional<U32>{0} &&
           memory.Read(view + 4) == std::optional<U32>{400} &&
           memory.Read(view + 8) == std::optional<U32>{0} &&
           memory.Read(view + 12) == std::optional<U32>{240};
}
template<class M> bool FirstPersonView(M& memory, U32 view) {
    if (!GameplayView(memory, view)) return false;
    const U32 play = view - 0x188, camera = play + 0x364;
    const auto active = memory.Read(play + 0xa64), status = memory.Read(camera + 0x188);
    if (!active || (*active & 0xffff) != 0 || !status || (*status & 0xffff) != 7 ||
        memory.Read(camera + 0xd4) != std::optional<U32>{play}) return false;
    const auto mode = memory.Read(camera + 0x18c);
    if (!mode) return false;
    // Native look, bow and slingshot modes; stale targeting flags must not
    // prevent the cap. Normal-mode entry can publish first-person state first.
    if ((*mode & 0xffff) == 6 || (*mode & 0xffff) == 7 || (*mode & 0xffff) == 11) return true;
    if ((*mode & 0xffff) != 0) return false;
    const U32 player = memory.Read(camera + 0xd8).value_or(0);
    return player >= 0x100000 && player <= 0x3fffe8e8 && !(player & 3) &&
        (memory.Read(player + 0x1710).value_or(0) & 0x100000) != 0;
}
template<class M> float EffectiveFov(M& memory, U32 view, float native, float configured) {
    if (!GameplayView(memory, view)) return native;
    // Use the loaded scene, including when an entrance has been randomized.
    const auto scene = memory.Read(view - 0x188 + 0x104);
    const U32 id = scene ? (*scene & 0xffff) : ~U32{0};
    float ceiling = 0.f;
    switch (id) {
    case 0x23: // Temple of Time courtyard, child day and night.
    case 0x24: ceiling = 62.f; break;
    case 0x25: ceiling = 56.f; break; // Adult ruins.
    case 0x2d: ceiling = 58.f; break; // Kokiri Shop.
    case 0x34: ceiling = 55.f; break; // Link's house.
    }
    if (FirstPersonView(memory, view))
        ceiling = ceiling ? std::min(ceiling, FirstPersonMaximumFov) : FirstPersonMaximumFov;
    if (ceiling) configured = std::min(Sanitize(configured), ceiling);
    const float widened = Widen(native, configured);
    // Preserve narrower scripted views and cap both world projections.
    return ceiling && std::isfinite(widened) && widened > 0.f && widened < 170.f ?
        std::min(widened, ceiling) : widened;
}
template<class M, class F> void Handle(M& memory, U32 view, F& fp, float configured,
                                      bool gameplay_lens=false,float gameplay_fov=NativeFov) {
    const float native = std::bit_cast<float>(fp[16]);
    const float degrees_to_radians = std::bit_cast<float>(fp[23]);
    // Orbit and targeting share the configured lens. Native dialogue/scripted
    // zooms still use the relative widening path. Do not write back the lens:
    // the native camera must retain its own transition state.
    const float base = gameplay_lens && std::isfinite(native) && native>0.f && native<170.f ?
        gameplay_fov : native;
    const float adjusted = GameplayView(memory, view) ? EffectiveFov(memory, view, base, configured) : native;
    // Displaced VMUL.F32 s0,s16,s23. Leave s16 and all guest camera fields intact
    // so repeated draws, inherited secondary FOV, and savestates cannot accumulate zoom.
    fp[0] = std::bit_cast<U32>(adjusted * degrees_to_radians);
}
} // namespace OOT3D::Camera
