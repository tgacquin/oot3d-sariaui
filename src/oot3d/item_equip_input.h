// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"

namespace OOT3D::ItemEquip {
using TitleScreen::U32;
constexpr U32 Items=0x5066f8,FirstSVC=0xfff180;
constexpr std::array<TitleScreen::Site,4> Sites{{
    {0x2ec4f4,0xeb013c3c,0xe3100b01}, // Browsing: native X/Y quick equip input.
    {0x2ec438,0xeb013c6b,0xeddf9ab7}, // After Choose: native target confirmation.
    {0x2ec5ac,0xebfffb17,0xe3500000}, // Quick equip destination ready, before restrictions.
    {0x2ec450,0xe1c507d8,0xe0812081}, // Target-grid coordinates for chosen-item equip.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} || m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return m.Read(0x2ec704)==std::optional<U32>{Items} &&
        m.Read(0x2ec734)==std::optional<U32>{0x587958};
}
template<class M> bool Ready(M& m) {
    const auto state=m.Read(Items+0x34).value_or(0);
    return PauseMenu::Active(m) && !SaveMenu::Active(m) && !PauseMenu::Closing(m) &&
        m.Read(0x50446c+0x18)==std::optional<U32>{0} && (state==2 || state==8);
}
struct Input {
    unsigned previous=0,pending=0,assigning=0;
    U32 context=0;
    std::int64_t time=0;
    template<class M> void Poll(M& m,unsigned triggers,U32 buttons,bool live,std::int64_t now) {
        triggers&=3;const unsigned edges=triggers&~previous;previous=triggers;
        if(!live || !CodeMatches(m) || !Ready(m) || (buttons&0xf0f)) {pending=assigning=0;return;}
        if(edges && (triggers==1 || triggers==2)) {
            pending=triggers;context=m.Read(PauseMenu::UI+0xc).value_or(0);time=now;
        }
    }
    template<class M,class R> void Handle(M& m,R& r,unsigned site,std::int64_t now) {
        if(site<2) {
            assigning=0;
            r[14]=Sites[site].pc+4;r[15]=0x33b5ec; // Original BL GetPressed.
            const unsigned request=pending;pending=0;
            if(!request || r[5]!=Items || !Ready(m) ||
               m.Read(Items+0x34)!=std::optional<U32>{site==0?2u:8u} ||
               context!=m.Read(PauseMenu::UI+0xc).value_or(0) || now<time || now-time>150000)return;
            assigning=request;
            r[0]=site==0?0x400:1;r[15]=Sites[site].pc+4;
            return;
        }
        if(site==2) {
            r[14]=Sites[site].pc+4;r[15]=0x2eb210; // Native compatibility/restriction check.
            if(assigning && r[5]==Items) {
                // Native grid slot 5 is I, 23 is II. X/Y retain slots 11/17.
                m.Write(Items+0x54,assigning==1?5:23);
                m.Write(Items+8,271u|((assigning==1?7u:191u)<<16));
            }
        } else {
            r[0]=m.Read(r[5]+0x78).value_or(0);r[1]=m.Read(r[5]+0x7c).value_or(0); // Displaced LDRD.
            if(assigning && r[5]==Items) {r[0]=5;r[1]=assigning==1?0:3;}
        }
        assigning=0;
    }
};
inline Input input;
}
