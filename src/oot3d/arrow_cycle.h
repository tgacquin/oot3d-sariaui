// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/control_swap.h"
#include "oot3d/item_use_input.h"

namespace OOT3D::ArrowCycle {
using TitleScreen::U32;
constexpr U32 SVC=0xfff430,Save=0x587958;
constexpr TitleScreen::Site Site{0x1e1dbc,0xe28dd040,0xecbd8b02};
constexpr U32 Nock=0x33603c,MagicReset=0x34708c,Kill=0x374428,RaiseBow=0x3604f0;
inline U32 Instruction(U32 pc,U32 word) {
    return pc==Site.pc && word==Site.instruction?0xef000000|SVC:word;
}
template<class M> bool CodeMatches(M& m) {
    return m.Read(Site.pc)==Site.instruction && m.Read(Site.pc+4)==Site.following &&
        m.Read(Site.pc+8)==0xe8bd81f0 && m.Read(0x1e1b58)==0xe1a04000 &&
        m.Read(0x1e1b5c)==0xe1a06001 && m.Read(Nock)==0xe92d43f0 &&
        m.Read(MagicReset)==0xe59f1020 && m.Read(RaiseBow)==0xe92d4070 &&
        m.Read(RaiseBow+4)==0xe1a04000 &&
        m.Read(Kill)==0xe3a01000 && m.Read(Kill+4)==0xe5801140;
}
template<class M> unsigned Byte(M& m,U32 at) {
    return (m.Read(at&~3u).value_or(~0u)>>((at&3)*8))&255;
}
template<class M> void SetByte(M& m,U32 at,unsigned value) {
    const auto w=m.Read(at&~3u);if(!w)return;
    const unsigned shift=(at&3)*8;
    m.Write(at&~3u,(*w&~(255u<<shift))|((value&255)<<shift));
}
template<class M> void SetHalf(M& m,U32 at,unsigned value) {
    SetByte(m,at,value);SetByte(m,at+1,value>>8);
}
template<class M> bool Bow(M& m,U32 play,U32 player) {
    const unsigned type=Byte(m,player+0x1a9);
    return TitleScreen::Pointer(player,0x2a4c) && m.Read(play+0x43c)==player &&
        (m.Read(player).value_or(~0u)&0xffff)==0 && type>=8 && type<=11 &&
        !(m.Read(player+0x1710).value_or(~0u)&(1u<<23)); // No horseback bow.
}
template<class M> unsigned Next(M& m,unsigned current,int direction) {
    constexpr std::array<unsigned,3> inventory_slots{3,4,10},items{3,4,12};
    unsigned type=current<3?current:(direction>0?2:0);
    for(unsigned i=0;i<3;++i) {
        type=(type+3+direction)%3;
        if(type==0 || Byte(m,Save+0x8c+inventory_slots[type])==items[type])return type;
    }
    return current;
}
struct Input {
    U32 previous=0,consumed=0,context=0,player=0;
    int pending=0;
    std::int64_t time=0;
    bool enabled=false;
    template<class M> void Poll(M& m,U32& buttons,bool live,std::int64_t now) {
        const U32 raw=buttons&0x300,edges=raw&~previous;previous=raw;
        const bool on=ControlSwap::cycle_arrows.load();
        if(on!=enabled)consumed|=raw;
        enabled=on;consumed&=raw;
        const U32 play=m.Read(PauseMenu::UI+0xc).value_or(0),link=m.Read(play+0x43c).value_or(0);
        if(live && on && CodeMatches(m) && ItemUse::Gameplay(m) && Bow(m,play,link) && !(buttons&12)) {
            const U32 presses=edges&~consumed;
            consumed|=raw;
            if(presses && raw!=0x300) {pending=presses&0x200?-1:1;context=play;player=link;time=now;}
        } else pending=0;
        buttons&=~consumed; // A held cycle button cannot become target/shield on bow exit.
    }
};
inline Input input;
// Like the randomizer's Arrow_HandleSwap, run after the native player update.
// Native calls own actor teardown, magic effects and the new arrow allocation.
struct Hook {
    struct Call {U32 pc=0,a=0,b=0,c=0;};
    std::array<Call,3> calls{};
    unsigned count=0,index=0;
    bool calling=false;
    std::array<U32,16> saved{};
    std::array<U32,64> ext{};
    U32 flags=0,fpflags=0,pending_player=0,pending_play=0;
    void Add(U32 pc,U32 a,U32 b=0,U32 c=0) {calls[count++]={pc,a,b,c};}
    template<class M,class R,class F> void Handle(M& m,R& r,F& f,U32& cpsr,U32& fpscr,std::int64_t now) {
        if(calling) {
            r=saved;f=ext;cpsr=flags;fpscr=fpflags;
        } else {
            count=index=0;
            const U32 player=r[4],play=r[6];
            if(ItemUse::Gameplay(m) && Bow(m,play,player)) {
                if(pending_player==player && pending_play==play && (m.Read(Save+0x1580).value_or(~0u)&65535)==0) {
                    Add(Nock,player,play);pending_player=pending_play=0;input.pending=0;
                } else if(ControlSwap::cycle_arrows.load() && input.pending && input.player==player &&
                          input.context==play && now>=input.time && now-input.time<=150000) {
                    const unsigned current=Byte(m,player+0x1a9)-8;
                    const unsigned next=Next(m,current,input.pending);input.pending=0;
                    const unsigned bow=Byte(m,Save+0x8f);
                    if(next!=current && (bow==3 || (bow>=0x38 && bow<=0x3a))) {
                        const unsigned item=next?0x37+next:3;
                        SetByte(m,Save+0x8f,item);SetByte(m,player+0x1aa,item);
                        SetByte(m,player+0x1a9,next+8);SetByte(m,player+0x1ac,next+8);
                        const U32 held=m.Read(player+0x1224).value_or(0);
                        const unsigned type=m.Read(held+0x1c).value_or(~0u)&65535;
                        if(TitleScreen::Pointer(held,0x144) && (m.Read(held).value_or(~0u)&65535)==0x16 && type>=2 && type<=5 &&
                           m.Read(held+0x13c).value_or(0)!=0) {
                            const auto magic_state=m.Read(Save+0x1580).value_or(0)&65535;
                            if(magic_state==1 || magic_state==2) {
                                SetByte(m,Save+0x47,m.Read(Save+0x1588).value_or(0)&65535);
                                SetHalf(m,Save+0x1580,3);
                            }
                            constexpr std::array<unsigned,4> costs{0,4,4,8};
                            const unsigned capacity=m.Read(Save+0x1584).value_or(0)&65535;
                            SetByte(m,Save+0x47,std::min(Byte(m,Save+0x47)+costs[type-2],capacity));
                            if(type>=3 && m.Read(held+0x128)==0)Add(MagicReset,play);
                            Add(Kill,held);Add(RaiseBow,player+0x1764,play,0x1b1);
                            pending_player=player;pending_play=play;
                        }
                    }
                }
            } else {pending_player=pending_play=0;input.pending=0;}
            if(count) {saved=r;ext=f;flags=cpsr;fpflags=fpscr;}
        }
        if(index<count) {
            const auto call=calls[index++];calling=true;
            r[0]=call.a;r[1]=call.b;r[2]=call.c;r[14]=Site.pc;r[15]=call.pc;
        } else {calling=false;r[13]+=0x40;r[15]=Site.pc+4;}
    }
};
}
