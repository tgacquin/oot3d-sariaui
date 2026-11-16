// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
namespace OOT3D::ControlSwap {
inline std::atomic<bool> enabled{false},dirty{false};
inline std::atomic<bool> shoulders{false},cycle_arrows{false};
// Native OoT's pad word omits ZL/ZR. The mode menu needs either physical
// shoulder pair before gameplay remapping/filtering. -1 uses native input
// when live controller polling is unavailable (e.g. movie playback).
inline std::atomic<int> file_menu_shoulders{-1};
struct Shoulders {
    bool previous=false;
    std::uint32_t consumed=0;
    void Filter(std::uint32_t& buttons,unsigned& triggers,bool live) {
        file_menu_shoulders.store(live?int((buttons&0x300)!=0 || (triggers&3)!=0):-1);
        const bool swap=live && shoulders.load();
        std::uint32_t packed=(buttons&0x300)|((triggers&3)<<14);
        if(swap!=previous)consumed|=packed;
        previous=swap;consumed&=packed;packed&=~consumed;
        const unsigned l=(packed>>9)&1,r=(packed>>8)&1;
        buttons=(buttons&~0x300u)|(swap?((packed&(1u<<14))>>5)|((packed&(1u<<15))>>7):packed&0x300);
        triggers=swap?(l|(r<<1)):(packed>>14)&3;
    }
};
inline Shoulders shoulder_input;
constexpr std::uint32_t Remap(std::uint32_t buttons,bool swap) {
    return swap?(buttons&~12u)|((buttons&4)<<1)|((buttons&8)>>1):buttons;
}
struct Input {
    bool previous=false;
    std::uint32_t consumed=0;
    void Filter(std::uint32_t& buttons,bool gameplay,bool ocarina=false) {
        // Songs always uses physical Start; the pause-menu swap applies only
        // outside ocarina. Suppress held keys when changing mappings below.
        const bool swap=enabled.load() && !ocarina && gameplay;
        if(swap!=previous)consumed|=buttons&12;
        previous=swap;consumed&=buttons;buttons&=~consumed;
        buttons=Remap(buttons,swap);
    }
};
inline Input input;
}
