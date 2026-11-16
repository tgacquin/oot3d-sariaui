// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/randomizer/activity.h"
#include "oot3d/title_hooks.h"

namespace OOT3D::RoomJoinChime {
using TitleScreen::U32;
constexpr U32 Sound=0x01000487,Play=0x37547c;
template<class M> bool CodeMatches(M& m) {
    return m.Read(Play)==0xe92d4fff && m.Read(Play+4)==0xe24dd01c &&
        m.Read(0x54ac20)==0x3f800000 && m.Read(0x54ac24)==0 &&
        m.Read(0x1148f8)==0x3f400000;
}
// Enter only from the native file-menu or multiplayer update on the game
// thread. Network callbacks merely queue arrivals; they never touch guest CPU.
struct Hook {
    std::array<U32,16> saved{};
    std::array<U32,64> saved_fp{};
    std::array<U32,2> stack{};
    U32 saved_flags=0,saved_fpscr=0,return_pc=0;
    unsigned epoch=0;
    bool pending=false;
    std::int64_t next_us=0;
    template<class M,class R,class F> bool Frame(M& m,R& r,F& fp,U32& flags,U32& fpscr,
            U32 pc,unsigned process_epoch,std::int64_t now) {
        if(epoch!=process_epoch) {*this={};epoch=process_epoch;}
        if(pending) {
            if(pc!=return_pc || r[13]!=saved[13]-8)return false;
            m.Write(saved[13]-8,stack[0]);m.Write(saved[13]-4,stack[1]);
            std::copy(saved.begin(),saved.end(),r.begin());
            std::copy(saved_fp.begin(),saved_fp.end(),fp.begin());
            flags=saved_flags;fpscr=saved_fpscr;pending=false;
            return false; // Resume the original update exactly once.
        }
        if(now<next_us || !CodeMatches(m) || r[13]<8 || (r[13]&7))return false;
        const auto a=m.Read(r[13]-8),b=m.Read(r[13]-4);
        if(!a || !b || !Randomizer::Activity::TakeJoinChime())return false;
        std::copy_n(r.begin(),16,saved.begin());std::copy_n(fp.begin(),64,saved_fp.begin());
        saved_flags=flags;saved_fpscr=fpscr;stack={*a,*b};return_pc=pc;pending=true;
        next_us=now+1000000; // Separate simultaneous arrivals instead of stacking sounds.
        r[13]-=8;m.Write(r[13],0x1148f8);m.Write(r[13]+4,0x54ac24);
        r[0]=Sound;r[1]=0;r[2]=4;r[3]=0x54ac20;
        r[14]=pc;r[15]=Play;return true;
    }
};
}
