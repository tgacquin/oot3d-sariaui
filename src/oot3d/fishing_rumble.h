// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/rumble.h"
#include "oot3d/title_hooks.h"
#include <bit>
#include <optional>

namespace OOT3D::Rumble::Fishing {
using TitleScreen::U32;
constexpr U32 Globals = 0x53142c;
enum Event : unsigned { Bite, LoachBite, Hook, Struggle, Jump, Escape, Catch, Warning, Discovery };

// zeldaret/oot ovl_Fishing/z_fishing.c. All durations are 60 Hz retraces.
inline Cue BiteCue(bool sinking, unsigned timer) { return {sinking ? 60 : 180, int(timer * 3), 10}; }
inline Cue HookCue(float strength) { return {int(std::clamp(strength, 0.f, 255.f)), 120, 5}; }
inline Cue StruggleCue(float length, bool tired, U32 seed) {
    int amplitude = length >= 70 ? 255 : length >= 60 ? 230 : length >= 50 ? 200 : length >= 40 ? 170 : 140;
    // The 3DS removed this duration RNG call. Vary its original 10..14 range
    // without consuming guest RNG, which would change fish behavior.
    seed ^= seed >> 16; seed *= 0x7feb352d; seed ^= seed >> 15;
    return {tired ? amplitude * 3 / 4 : amplitude, 10 + int(seed % 5), 5};
}
inline Cue JumpCue(float length) { return {length >= 60 ? 255 : length >= 50 ? 200 : 180, 90, 2}; }
template<class M> unsigned Byte(M& m, U32 at) {
    return (m.Read(at & ~3u).value_or(0) >> ((at & 3) * 8)) & 255;
}
template<class M, class R> std::optional<Cue> EventCue(M& m, const R& r, unsigned event, U32 s0) {
    const U32 play = m.Read(0x5043e0).value_or(0), actor = r[4];
    if (!TitleScreen::Pointer(play, 0x108) || (m.Read(play + 0x104).value_or(0) & 65535) != 73)
        return {};
    if (event == Warning || event == Discovery)
        return actor == Globals ? std::optional<Cue>{{150, 10, 10}} : std::nullopt;
    if (!TitleScreen::Pointer(actor, 0x208) || (m.Read(actor).value_or(0) & 65535) != 0xfe)
        return {};
    const float length = std::bit_cast<float>(m.Read(actor + 0x204).value_or(0));
    if (!std::isfinite(length) || length <= 0 || length > 300) return {};
    const bool sinking = Byte(m, Globals + 0x16) == 2;
    switch (event) {
    case Bite: return r[0] >= 10 && r[0] <= 79 ? std::optional<Cue>{BiteCue(sinking, r[0])} : std::nullopt;
    case LoachBite: return BiteCue(sinking, 30);
    case Hook: {
        const float amplitude = std::bit_cast<float>(s0);
        return std::isfinite(amplitude) && amplitude > 0 ? std::optional<Cue>{HookCue(amplitude)} : std::nullopt;
    }
    case Struggle:
        if (r[7] != 7 && r[7] != 15) return {};
        return StruggleCue(length, r[7] == 15, m.Read(actor + 0x1b4).value_or(0) ^ actor);
    case Jump: return JumpCue(length);
    case Escape: case Catch: return Cue{1, 3, 1};
    default: return {};
    }
}
template<class M, class R> void Handle(M& m, R& r, unsigned event, U32 s0) {
    const auto cue = EventCue(m, r, event, s0);
    // Replay the displaced instruction even when disabled or context rejected.
    switch (event) {
    case Bite: r[1] = m.Read(r[10]).value_or(0); break;
    case LoachBite: r[0] = 100; break;
    case Hook: r[0] = s0; break;
    case Struggle: r[1] = m.Read(0x1fb880).value_or(0); break;
    case Jump: r[0] = m.Read(r[10]).value_or(0); break;
    case Escape: r[1] = 10; break;
    case Catch: r[0] = 6; break;
    case Warning: r[0] = 11; break;
    case Discovery: r[0] = 20; break;
    }
    if (cue) Override(*cue);
}
} // namespace OOT3D::Rumble::Fishing
