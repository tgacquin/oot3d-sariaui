// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/rumble.h"
#include "oot3d/fishing_rumble.h"
#include "oot3d/interpolation_state.h"
#include "oot3d/title_hooks.h"

namespace OOT3D::Rumble {
using TitleScreen::U32;
constexpr U32 FirstSVC = 0xfff580;
// Verified USA Rev 1 counterparts of zeldaret/oot's player rumble requests.
// Contact hooks run only on native accepted impacts, never on every sword hit.
// Roll impact deliberately precedes the randomizer's PlayerBonk patch at 492cd0.
constexpr std::array<TitleScreen::Site, 22> Sites{{
    {0x492c98, 0xe5d40002, 0xe59f11a8},
    {0x36f59c, 0xe59f302c, 0xe59f202c}, // Player audio, restricted to verified callers below.
    {0x132fa0, 0xe1a00004, 0xeb08a8ca}, // Shield accepted a blocked hit.
    {0x2b9940, 0xe5d401a9, 0xe2846c01}, // Melee AT_BOUNCED rebound.
    {0x2ba15c, 0xed850a87, 0xe5d60378}, // Accepted melee collision with scenery.
    {0x336364, 0xe5941224, 0xe3a00000}, // Projectile is released, not just drawn/aimed.
    {0x488b14, 0xe28400bc, 0xecbd8b02}, // Hookshot travel, after its startup delay.
    {0x2bc550, 0xe5d40002, 0xe5981004}, // Damaging landing; native impact table in r8.
    {0x2bc5dc, 0xe1a00004, 0xeb023dd5}, // Non-damaging landing, native filtered distance r6.
    {0x35d4d8, 0xe3a00014, 0xe1ca03b8}, // Electric-shock reaction accepted.
    {0x252578, 0xe5d40002, 0xe3500002}, // Owned Shard of Agony proximity pulse.
    {0x1d590c, 0xe5840290, 0xe5841004}, // Bomb switches to its explosion action, once.
    {0x252584, 0xe5950734, 0xe3500000}, // Shard sound and icon pulse.
    {0x1fb8e0, 0xe59a1000, 0xe3a03e11}, // Fish bite, original unscaled timer in r0.
    {0x1fbb54, 0xe3a00064, 0xe5c8901a}, // Loach bite, both lure branches converge.
    {0x1fbcec, 0xee100a10, 0xe59a0000}, // Successful hook, native strength in s0.
    {0x1fbddc, 0xe51f1564, 0xee100a10}, // Accepted underwater struggle.
    {0x1fbf64, 0xe59a0000, 0xe51f7a48}, // Accepted jump.
    {0x1fc744, 0xe3a0100a, 0xe1c702ba}, // Escape after hooking.
    {0x1fc838, 0xe3a00006, 0xe1c50bb0}, // Catch.
    {0x3b678c, 0xe3a0000b, 0xe5c40009}, // Warning for leaving with rod.
    {0x3b6410, 0xe3a00014, 0xe5c40009}, // Sinking lure discovered.
}};
inline U32 Instruction(U32 pc, U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000 | (FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& site:Sites)
        if(m.Read(site.pc)!=site.instruction || m.Read(site.pc+4)!=site.following)return false;
    return m.Read(0x492c94)==0xebfb7240 && m.Read(0x492ca0)==0xe3500002 &&
           m.Read(0x36f5d0)==0x54ac24 && m.Read(0x25291c)==0x48435000 &&
           m.Read(0x252920)==0x4a742400 && m.Read(0x252590)==0x1b048d8d &&
           m.Read(0x252598)==0xeb06ded2 && m.Read(0x25259c)==0xe1a0000a;
}
template<class M> unsigned Byte(M& m,U32 at) {
    return (m.Read(at&~3u).value_or(0)>>((at&3)*8))&255;
}
inline std::optional<Cue> SoundCue(U32 caller,U32 sound) {
    if(sound==0x100011a && (caller==0x1ebd6c || caller==0x2093e0 || caller==0x488a58))
        return Cue{255,10,250}; // Hookshot returns to Link's hand.
    if(sound==0x10000fc && (caller==0x2b9e98 || caller==0x4c2ffc))
        return Cue{255,20,150}; // Hammer hits wall/ground.
    if(sound==0x1000075 && caller==0x35d448)return Cue{255,10,40}; // Frozen.
    return {};
}
template<class M, class R> void Handle(M& m, R& r,unsigned site=0,U32 s0=0) {
    if (site >= 13 && site < Sites.size()) { Fishing::Handle(m, r, site - 13, s0); return; }
    U32 actor=r[4];std::optional<Cue> cue;
    // Replay exactly one displaced instruction; flags, LR, SP and FP state
    // remain native. The wall hook replays its one VSTR with the original s0.
    switch(site) {
    case 0:r[0]=Byte(m,actor+2);cue=Cue{255,20,150};break;
    case 1:
        actor=r[0];cue=SoundCue(r[14],r[1]);r[3]=m.Read(0x36f5d0).value_or(0);break;
    case 2:r[0]=actor;cue=Cue{180,20,100};break;
    case 3:r[0]=Byte(m,actor+0x1a9);cue=Cue{180,20,100};break;
    case 4:m.Write(r[5]+0x21c,s0);cue=Cue{180,20,100};break;
    case 5: {
        actor=r[6];r[1]=m.Read(r[4]+0x224).value_or(0);
        const auto item=Byte(m,actor+0x1a9);
        if(r[1])cue=(item==16 || item==17)?Cue{255,20,150}:Cue{150,10,150};
        break;
    }
    case 6:r[0]=actor+0xbc;cue=Cue{100,2,100};break;
    case 7: {
        r[0]=Byte(m,actor+2);
        const auto entry=m.Read(r[8]);
        if(entry)cue=Cue{int((*entry>>8)&255),int((*entry>>16)&255),int(*entry>>24)};
        break;
    }
    case 8:
        r[0]=actor;
        if(r[6]>200 && r[6]<400)cue=Cue{255,25,255};
        break;
    case 9:r[0]=20;cue=Cue{255,80,150};break;
    case 10:r[0]=Byte(m,actor+2);cue=Cue{120,20,10};break;
    case 11: {
        m.Write(actor+0x290,r[0]); // Native STR, preserving all registers/flags.
        if(TitleScreen::Pointer(actor,0x294) &&
           (m.Read(actor).value_or(0)&0xffffff)==0x030010 &&
           (m.Read(actor+0x1c).value_or(0)&65535)==1 && r[0]==0x3c4438) {
            const auto distance=m.Read(actor+0x98);
            if(distance)PresentedCue(BombExplosion(std::bit_cast<float>(*distance)),Interpolation::guest_frames.load());
        }
        return; // Bomb actor category is explosive, not player.
    }
    case 12:
        if(enabled.load())r[15]=0x25259c; // Keep proximity rumble; bypass only native sound/icon requests.
        else r[0]=m.Read(r[5]+0x734).value_or(0);
        return;
    default:return;
    }
    if(cue && TitleScreen::Pointer(actor,0x1ac) && Byte(m,actor+2)==2)
        PresentedCue(*cue,Interpolation::guest_frames.load());
}
} // namespace OOT3D::Rumble
