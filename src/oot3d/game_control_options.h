// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include <atomic>

namespace OOT3D::GameControlOptions {
using TitleScreen::U32;
inline std::atomic<bool> hold_targeting{true}, invert_camera_y{false}, motion_controls{true};
inline std::atomic<bool> dirty{false};
constexpr U32 Save = 0x587958;

template<class M> void Update(M& m) {
    const auto play = m.Read(0x5043d4 + 0xc).value_or(0);
    if (!TitleScreen::Pointer(play, 0x104) ||
        (m.Read(play + 0x100).value_or(0) & 0xffff) != 0x0203) return;

    // Loading a file or a savestate replaces the live options. Mirror the
    // global preferences into the same three bytes the native controls read.
    const auto apply = [&](U32 offset, bool value) {
        const auto word = m.Read(Save + (offset & ~3u));
        if (word && ((*word >> (8 * (offset & 3))) & 255u) != unsigned(value))
            m.Write8(Save + offset, value ? 1 : 0);
    };
    apply(0x2d, hold_targeting.load());
    apply(0x13d8, invert_camera_y.load());
    apply(0x0f, !motion_controls.load()); // The game encodes motion enabled as zero.
}
} // namespace OOT3D::GameControlOptions
