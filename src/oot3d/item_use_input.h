// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_fade.h"
#include "oot3d/pause_menu.h"

namespace OOT3D::ItemUse {
using TitleScreen::U32;
constexpr U32 Main=0x50af34,FirstSVC=0xfff190;
constexpr std::array<TitleScreen::Site,6> Sites{{
    {0x42ead8,0xebfc4252,0xe3500000}, // I input predicate.
    {0x42eb24,0xebfc423f,0xe3500000}, // II input predicate.
    {0x42eb04,0xe5850034,0xe5856044}, // I accepted after native restrictions.
    {0x42eb50,0xe5850034,0xe5856048}, // II accepted after native restrictions.
    {0x42ec4c,0xe5dd001c,0xe3500000}, // I held/released.
    {0x42ec80,0xe5dd001c,0xe3500000}, // II held/released.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    return m.Read(0x42ea00)==Main && m.Read(0x42ea2c)==0x587958;
}
template<class M> bool Gameplay(M& m) {
    const U32 play=m.Read(PauseMenu::UI+0xc).value_or(0);
    return HUDFade::GameplayReady(m,play) && !PauseMenu::Active(m) &&
        m.Read(PauseMenu::UI+0x20)==0;
}
struct Input {
    unsigned previous=0,held=0,pending=0,confirming=0,active=0;
    U32 context=0,active_context=0;
    std::int64_t time=0;
    template<class M> void Poll(M& m,unsigned triggers,U32 buttons,bool live,std::int64_t now) {
        triggers&=3;const unsigned edges=triggers&~previous;previous=triggers;
        const U32 play=m.Read(PauseMenu::UI+0xc).value_or(0);
        const unsigned state=m.Read(Main+0x34).value_or(0);
        if(active && (play!=active_context || state!=(active==1?5u:6u)))active=0;
        const bool ready=live && CodeMatches(m) && Gameplay(m) && !(buttons&(4|8));
        held=ready?triggers:0;
        if(!ready || state!=2) {pending=0;return;}
        if(edges && (triggers==1 || triggers==2)) {pending=triggers;context=play;time=now;}
    }
    template<class M,class R> void Handle(M& m,R& r,unsigned site,std::int64_t now) {
        const unsigned slot=(site&1)+1;
        if(site<2) {
            confirming=0;r[14]=Sites[site].pc+4;r[15]=0x33f428;
            if(pending!=slot)return;
            pending=0;
            if(r[5]!=Main || !Gameplay(m) || m.Read(Main+0x34)!=2 ||
               context!=m.Read(PauseMenu::UI+0xc).value_or(0) || now<time || now-time>150000)return;
            // Feed the native predicate, then keep its empty-slot and disabled
            // item checks, pressed/held flags, and player item-use code intact.
            confirming=slot;r[0]=1;r[15]=Sites[site].pc+4;return;
        }
        if(site<4) {
            m.Write(r[5]+0x34,r[0]);
            if(r[5]==Main && confirming==slot && r[0]==slot+4) {
                active=slot;active_context=context;
            }
            confirming=0;return;
        }
        r[0]=(m.Read((r[13]+0x1c)&~3u).value_or(0)>>(((r[13]+0x1c)&3)*8))&255;
        if(r[5]==Main && active==slot) {
            const bool down=Gameplay(m) && active_context==m.Read(PauseMenu::UI+0xc).value_or(0) &&
                (held&(1u<<(slot-1)));
            r[0]=down?1:0;
            if(!down)active=0;
        }
    }
};
inline Input input;
}
