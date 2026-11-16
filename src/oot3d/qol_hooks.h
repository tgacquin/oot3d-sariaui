// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/qol.h"
#include "oot3d/hud_fade.h"
#include "oot3d/pause_menu.h"

namespace OOT3D::QOL {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff1a0,LadderAction=0x4be20c,StoneKill=0x374428;
// USA Rev 1. Hook the native request and speed calculation directly.
constexpr std::array<TitleScreen::Site,6> Sites{{
    {0x267e00,0xe59f034c,0xe1d004bc}, // Navi's unsolicited reminder branch.
    {0x4be328,0xed800a10,0xe1d603f8}, // Climb animation's freshly computed speed.
    {0x471754,0xebfc0f48,0xe28dd00c}, // Low-health beep, pulsing gain.
    {0x471778,0xebfc0f3f,0xe28dd00c}, // Low-health beep, initial gain.
    {0x3cb2dc,0xe5960710,0xe59f10b8}, // Automatic look after placing a Bombchu.
    {0x24be0c,0xe28dd018,0xe8bd81f0}, // Tutorial stone: initialized, entering its epilogue.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    // Use the game's normal actor removal: disable update/draw/targeting,
    // retain its destructor, and let the actor manager release resources.
    constexpr std::array<U32,7> kill{0xe3a01000,0xe5801140,0xe580113c,
        0xe5901004,0xe3c11001,0xe5801004,0xe12fff1e};
    for(unsigned i=0;i<kill.size();++i)if(m.Read(StoneKill+4*i)!=kill[i])return false;
    return m.Read(0x268154)==0x587958 && m.Read(0x267f8c)==0xe1c5a9bc &&
        m.Read(0x267f90)==0xe1a01007 && m.Read(0x4c11ec)==LadderAction &&
        m.Read(LadderAction)==0xe92d4ff0 && m.Read(LadderAction+4)==0xe1a04000 &&
        m.Read(0x4717d0)==0x010004a0 && m.Read(0x3cb2d4)==0xe35000da &&
        m.Read(0x3cb35c)==0xecbd8b04 &&
        m.Read(0x50cd84+0x1d9*32+20)==0x526438 &&
        m.Read(0x526438)==0x601d9 && m.Read(0x526438+8)==0x194 &&
        m.Read(0x526438+16)==0x24bd08 && m.Read(0x526438+20)==0x24be24;
}
template<class M> bool Climbing(M& m,U32 player) {
    if(!TitleScreen::Pointer(player,0x2a4c) || m.Read(player+0x1708)!=LadderAction)return false;
    const auto mode=m.Read(player+0x2234),step=m.Read(player+0x2238);
    // All surfaces handled by the native climbing action, including vines.
    // Negative steps are mounting/dismounting animations, not climbing.
    return mode && step && (*step&0xffff)<=1;
}
template<class M> bool Gameplay(M& m,U32 play) {
    return HUDFade::GameplayReady(m,play) && !PauseMenu::Active(m) &&
        m.Read(PauseMenu::UI+0x20)==0;
}
template<class M> void Poll(M& m,U32 buttons,bool live,std::int64_t now) {
    const U32 play=m.Read(PauseMenu::UI+0xc).value_or(0);
    U32 player=0;
    const bool eligible=fast_ladder.load() && live && !(buttons&(4|8)) &&
        CodeMatches(m) && Gameplay(m,play) &&
        Climbing(m,player=m.Read(play+0x43c).value_or(0));
    taps.Poll((buttons&2)!=0,eligible,player,now);
}
template<class M,class R> void Handle(M& m,R& r,U32 speed,unsigned site,std::int64_t now) {
    if(site==5) {
        // Never cancel the shared Actor_Spawn routine. This actor-specific
        // epilogue runs only after collision/skeleton initialization finishes.
        // Native adult-Link initialization already uses this same kill routine.
        r[13]+=0x18; // Displaced ADD sp,sp,#0x18.
        if(hide_sheikah_stones.load() && TitleScreen::Pointer(r[4],0x3f4) &&
           (m.Read(r[4]).value_or(0)&0xffff)==0x1d9) {
            r[0]=r[4];r[14]=Sites[5].pc+4;r[15]=StoneKill;
        }
    } else if(site==0) {
        // Reached with no requested Navi dialogue and no focus actor. Preserve
        // targeting/scripted dialogue; skip timed and long-play reminders.
        if(disable_navi.load())r[15]=0x267f8c;
        else r[0]=m.Read(0x268154).value_or(0);
    } else if(site==1) {
        const float native=std::bit_cast<float>(speed);
        if(taps.Ready(r[4],now) && Climbing(m,r[4]) && Gameplay(m,r[10]) &&
           std::isfinite(native) && std::abs(native)<=4.f) {
            const bool boost=taps.Boost(r[4],now);
            // MM caps the stick-derived magnitude at 3, then doubles it for
            // the press update and one following update (table+0x190 = 2).
            const float base=std::copysign(std::min(std::abs(native),3.f),native);
            speed=std::bit_cast<U32>(base*((boost && (r[7]!=0 || r[8]!=0))?2.f:1.f));
        } else taps.Clear();
        // Animation root motion advances with this speed. Native collision,
        // rung completion and top/bottom transitions still run.
        m.Write(r[0]+0x40,speed);
    } else if(site<4) {
        if(!disable_heart_beep.load()) {
            r[14]=Sites[site].pc+4;r[15]=0x37547c;
        }
        // Skipping only the sound call leaves the heartbeat visuals intact.
    } else if(site==4) {
        // Reached only after releasing actor 0xda. Skip the automatic look
        // setup, including its camera easing fields, through the native epilogue.
        // Ordinary bomb placement and manually requested first person are separate.
        if(no_bombchu_first_person.load())r[15]=0x3cb35c;
        else r[0]=m.Read(r[6]+0x710).value_or(0);
    }
}
}
