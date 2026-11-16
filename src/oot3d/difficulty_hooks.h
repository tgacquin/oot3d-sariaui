// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/difficulty.h"
#include "oot3d/rumble.h"
#include "oot3d/title_hooks.h"

namespace OOT3D::Difficulty {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff1b0;
constexpr std::array<TitleScreen::Site,13> Sites{{
    {0x352e24,0xe1d504b4,0xe0800004}, // Final damage after double defense.
    {0x353624,0xeb0080a9,0xe1b04000}, // Stationary collectible drop.
    {0x36e048,0xeb001620,0xe1b04000}, // Thrown collectible drop.
    {0x374868,0xebfffc18,0xe1b04000}, // Random drop table.
    {0x3749b4,0xebfffbc5,0xe1b04000}, // Forced random-table drop.
    {0x1d7338,0xe12fff32,0xe2841c09}, // Initialized fairy update callback.
    {0x4968b4,0xe3a00d05,0xe1c10bb2}, // Manual bottled-fairy recovery amount.
    {0x2523e4,0xebfdc751,0xe3500000}, // Death: look for/consume bottled fairy.
    {0x352e20,0xb1a040c4,0xe1d504b4}, // Earned enhanced-defense reduction.
    {0x4962a8,0x13a00d05,0x11c50bb2}, // Red/blue potion pending healing.
    {0x34af48,0xe3a00d05,0xe2871a07}, // Bottled-fairy revival healing.
    {0x252414,0xe1c108bc,0xe3a00000}, // Death only after the fairy scan fails.
    {0x352dcc,0xe5d5000e,0xe3500000}, // Native MQ damage gate; use the shared multiplier instead.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)
            return (word&0xf0000000)|0x0f000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    return m.Read(0x352e70)==0x587958 && m.Read(0x374428)==0xe3a01000 &&
        m.Read(0x37442c)==0xe5801140 && m.Read(0x374430)==0xe580113c &&
        m.Read(0x5230bc)==0x1d732c && m.Read(0x4968f0)==0x588e58 &&
        m.Read(0x1c41cc)==0x587958 && m.Read(0x1c414c)==0xe3530018;
}
template<class M> void StoreHalf(M& m,U32 at,U32 value) {
    const unsigned shift=(at&2)*8;
    if(const auto old=m.Read(at&~3u))m.Write(at&~3u,(*old&~(0xffffu<<shift))|((value&0xffff)<<shift));
}
template<class M,class R> void Handle(M& m,R& r,unsigned site) {
    if(site==0) {
        r[0]=m.Read(r[5]+0x44).value_or(0)&0xffff;
        Rumble::Damage(std::bit_cast<std::int32_t>(r[4]), r[0]);
        r[4]=static_cast<U32>(Damage(std::bit_cast<std::int32_t>(r[4])));
    } else if(site<=4) {
        // Native callers explicitly accept a null spawn. Filter the resolved
        // drop after native ammo/health substitutions; never write save flags.
        const auto params=m.Read(r[13]+8);
        if(r[2]==0x15 && params && BlockDrop(*params))r[0]=0;
        else {r[14]=Sites[site].pc+4;r[15]=0x3738d0;}
    } else if(site==5) {
        const auto params=m.Read(r[0]+0x1c);
        r[14]=Sites[site].pc+4;
        // Let the native actor destructor release its initialized resources.
        // Keep decorative fairies and the revive animation's state machine.
        const bool blocked=params && ((disable_fairies.load() && RecoveryFairy(*params&0xffff)) ||
            (disable_fairy_drops.load() && (*params&0xffff)==2));
        r[15]=blocked?0x374428:r[2];
    } else if(site==6) {
        r[0]=disable_fairies.load()?0:Healing(bottled_fairy_hearts.load());
    } else if(site==7) {
        // Take the existing no-fairy death path without consuming a bottle or
        // starting a resurrection that can never finish.
        if(disable_fairies.load())r[0]=0;
        else {r[14]=Sites[site].pc+4;r[15]=0x1c4130;}
    } else if(site==8) {
        // The original LT condition is retained on the SVC, so this executes
        // only for damage while the earned defense upgrade is present.
        r[4]=U32(Defense(std::bit_cast<std::int32_t>(r[4])));
    } else if(site==9) {
        r[0]=0x140;
        const auto action=m.Read(r[4]+0x1ac);
        if(action && (*action&255)==0x25)r[0]=Healing(red_potion_hearts.load());
        if(action && (*action&255)==0x26)r[0]=Healing(blue_potion_hearts.load());
    } else if(site==10) {
        r[0]=Healing(bottled_fairy_hearts.load());
    } else if(site==12) {
        r[0]=0; // Bypass only the local MQ damage doubling; preserve the save mode.
    } else if(site==11) {
        if(lose_rupees_on_death.load() && (m.Read(0x58799c).value_or(1)&0xffff)==0) {
            // Clear the pending HUD rupee change as well as the wallet. Leave
            // adjoining magic/equipment fields and native save behavior alone.
            StoreHalf(m,0x5879a0,0);StoreHalf(m,0x588eb4,0);
        }
        StoreHalf(m,r[1]+0x8c,r[0]);
    }
}
}
