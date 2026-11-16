// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include "oot3d/title_layout.h"

namespace OOT3D::TitleScreen {
using U32 = std::uint32_t;
constexpr std::uint64_t ProgramId = 0x0004000000033500ULL;
constexpr U32 FirstSVC = 0xfff000;
struct Site { U32 pc, instruction, following; };
// USA Rev 1: fresh title model matrices, then title sprite submission.
constexpr std::array<Site, 8> Sites{{
    {0x1da694, 0xe28d1088, 0xeb065ed0},
    {0x1da88c, 0xe28d10b8, 0xeb065e52},
    {0x1da988, 0xe28d1058, 0xeb065e13},
    {0x2e7aa8, 0xe3a01006, 0xe88d0060},
    {0x43fdf8, 0xe1a04000, 0xe59f137c}, // Title UI draw begins after native update.
    {0x44014c, 0xe28dd050, 0xecbd8b02}, // Both title UI draw return paths.
    {0x440174, 0xe28dd050, 0xecbd8b02},
    {0x2e7ab4, 0xe28dd0c8, 0xe8bd8070}, // Prompt sprite has been queued.
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
    return memory.Read(0x52c328) == std::optional<U32>{0x00060171} &&
           memory.Read(0x52c344) == std::optional<U32>{0x1da4f4} &&
           memory.Read(0x2e7ab0) == std::optional<U32>{0xeb007ac6};
}
inline bool Valid(const Pose& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.scale) &&
           std::abs(p.x) <= 400 && std::abs(p.y) <= 240 && p.scale >= .1f && p.scale <= 3;
}
inline bool Pointer(U32 address, unsigned bytes) {
    return address >= 0x100000 && !(address & 3) && address <= 0x3fffffff - bytes;
}
template<std::size_t N, class M>
bool ReadFloats(M& memory, U32 address, std::array<float, N>& values) {
    if (!Pointer(address, N * 4)) return false;
    for (unsigned i = 0; i < N; ++i) {
        const auto word = memory.Read(address + i * 4);
        if (!word) return false;
        values[i] = std::bit_cast<float>(*word);
        if (!std::isfinite(values[i]) || std::abs(values[i]) > 100000) return false;
    }
    return true;
}
template<class M> bool Matrix(M& memory, U32 address, const Pose& pose) {
    if (!Valid(pose)) return false;
    std::array<float, 12> matrix;
    if (!ReadFloats(memory, address, matrix)) return false;
    // Screen-oriented title models: scale around their own native origin.
    // Preserve depth and the animated model supplied afresh by the game.
    for (unsigned row = 0; row < 2; ++row)
        for (unsigned col = 0; col < 3; ++col) matrix[row * 4 + col] *= pose.scale;
    matrix[3] += pose.x * .1f;
    matrix[7] -= pose.y * .1f;
    for (unsigned i = 0; i < matrix.size(); ++i)
        memory.Write(address + i * 4, std::bit_cast<U32>(matrix[i]));
    return true;
}
template<class M> bool Prompt(M& memory, U32 record, U32 rect, const Pose& pose) {
    if (!Valid(pose)) return false;
    const auto controller = memory.Read(0x50bb50);
    if (!controller || !Pointer(*controller, 0x49c) || record != *controller + 0x10c)
        return false;
    // This record is the 208x32 Press Start atlas sprite, not the background,
    // save-selection choices, or transition objects sharing this renderer.
    if (memory.Read(record + 8) != std::optional<U32>{std::bit_cast<U32>(208.f)} ||
        memory.Read(record + 12) != std::optional<U32>{std::bit_cast<U32>(32.f)}) return false;
    std::array<float, 4> bounds;
    if (!ReadFloats(memory, rect, bounds)) return false;
    const float cx = (bounds[2] + bounds[3]) * .5f;
    const float cy = (bounds[0] + bounds[1]) * .5f;
    // Center the native 320-wide layout on the 400-wide top screen. Put the
    // prompt below the logo; offsets remain relative to this default placement.
    bounds[0] = cy + 70.f + pose.y + (bounds[0] - cy) * pose.scale;
    bounds[1] = cy + 70.f + pose.y + (bounds[1] - cy) * pose.scale;
    bounds[2] = cx + 40.f + pose.x + (bounds[2] - cx) * pose.scale;
    bounds[3] = cx + 40.f + pose.x + (bounds[3] - cx) * pose.scale;
    for (unsigned i = 0; i < bounds.size(); ++i)
        memory.Write(rect + i * 4, std::bit_cast<U32>(bounds[i]));
    return true;
}
// Read the live title actor from the controller's play-state actor list. No cached
// actor pointer can survive a title restart, save load, or actor destruction.
template<class M> std::optional<float> CopyrightAlpha(M& memory, U32 controller) {
    if (!Pointer(controller, 0x49c) || memory.Read(0x50bb50) != std::optional<U32>{controller})
        return std::nullopt;
    const auto play = memory.Read(controller);
    if (!play || !Pointer(*play, 0x20d0)) return std::nullopt;
    auto actor = memory.Read(*play + 0x20cc); // ActorContext + category 6 head.
    for (unsigned count = 0; count < 128 && actor && Pointer(*actor, 0x1e0); ++count) {
        const auto identity = memory.Read(*actor);
        if (identity && (*identity & 0x00ffffff) == 0x00060171 &&
            memory.Read(*actor + 0x140) == std::optional<U32>{0x1da4f4}) {
            const auto model = memory.Read(*actor + 0x1ac);
            const auto alpha = memory.Read(*actor + 0x1d8);
            if (!model || !Pointer(*model, 4) || !alpha) return std::nullopt;
            const float value = std::bit_cast<float>(*alpha);
            if (!std::isfinite(value) || value < 0 || value > 255) return std::nullopt;
            return value * (1.f / 255.f); // Same normalization as the copyright material.
        }
        const auto next = memory.Read(*actor + 0x130);
        if (next == actor) return std::nullopt;
        actor = next;
    }
    return std::nullopt;
}
struct FadeOverride {
    U32 controller = 0, original_alpha = 0;
    template<class M> void Begin(M& memory, U32 candidate) {
        if (controller) return; // Draw is not expected to recurse.
        const auto alpha = CopyrightAlpha(memory, candidate);
        if (!alpha) return;
        const auto original = memory.Read(candidate + 0x144);
        if (!original) return;
        const float native_alpha = std::bit_cast<float>(*original);
        if (!std::isfinite(native_alpha)) return;
        controller = candidate;
        original_alpha = *original;
        // Copyright controls the visibility envelope; the game's pulse and
        // selected-Start flash remain in the native sprite opacity. The native
        // oscillator reverses its 1/30 step without clamping at either endpoint;
        // rejecting that overshoot would expose an unfaded frame every cycle.
        // Clamp only this draw's opacity, then restore the exact native value.
        memory.Write(controller + 0x144,
                     std::bit_cast<U32>(*alpha * std::clamp(native_alpha, 0.f, 1.f)));
    }
    template<class M> void End(M& memory, U32 candidate) {
        if (!controller || candidate != controller) return;
        // Sprite submission has copied the color; restore the animation's own
        // state before its next update. This also covers the zero-opacity gate.
        if (memory.Read(0x50bb50) == std::optional<U32>{controller} &&
            memory.Read(controller + 0x144)) memory.Write(controller + 0x144, original_alpha);
        controller = 0;
    }
};
template<class M, class R> void Handle(M& memory, R& regs, unsigned site, const Layout& poses,
                                       FadeOverride& fade) {
    if (site < 3) {
        constexpr std::array<U32, 3> offsets{0x88, 0xb8, 0x58};
        regs[1] = regs[13] + offsets[site]; // displaced ADD; flags unchanged
        Matrix(memory, regs[1], poses[site == 2 ? 1 : 0]);
    } else if (site == 3) {
        regs[1] = 6; // displaced MOV; every non-prompt sprite stays on bottom
        if (Prompt(memory, regs[4], regs[3], poses[2])) regs[1] = 5;
    } else if (site == 4) {
        regs[4] = regs[0]; // displaced MOV r4,r0
        fade.Begin(memory, regs[4]);
    } else if (site == 5 || site == 6) {
        fade.End(memory, regs[4]);
        regs[13] += 0x50; // displaced ADD sp,sp,#0x50; flags unchanged
    } else if (site == 7) {
        regs[13] += 0xc8; // displaced ADD; sprite return, including non-title sprites
    }
}
} // namespace OOT3D::TitleScreen
