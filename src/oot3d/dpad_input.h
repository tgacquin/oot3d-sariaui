// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/dpad_layout.h"
#include "oot3d/hud_fade.h"
#include "oot3d/pause_menu.h"
#include "oot3d/shop_controls.h"

namespace OOT3D::DPad {
using TitleScreen::U32;
constexpr U32 Save=0x587958, Main=0x50af34, UI=0x5043d4;
constexpr U32 Right=0x10, Left=0x20, Up=0x40, Down=0x80, Directions=0xf0;
constexpr U32 FirstSVC=0xfff0e0, UseItem=0x34d688;
// After native HUD/input update, before its epilogue. The native item routine
// handles equip/unequip, physics, Link's model and the equipment sound together.
constexpr TitleScreen::Site Site{0x42f534,0xe28dd038,0xecbd8b02};
// Replace only the input predicates, retaining the surrounding native action
// restrictions and transitions. Ordinary touchscreen calls still run unchanged.
constexpr U32 HitTest=0x33f428;
constexpr std::array<TitleScreen::Site,5> ActionSites{{
    {0x42e934,0xebfc42bb,0xe3500000}, // Ocarina
    {0x42e9a4,0xebfc429f,0xe3500000}, // View, normal
    {0x42e9e8,0xebfc428e,0xe3500000}, // View, alternate player state
    {0x42f104,0xebfc40c7,0xe3500000}, // View confirmation
    {0x42f18c,0xebfc40a5,0xe3500000}, // Ocarina confirmation
}};
constexpr TitleScreen::Site UpdateSite{0x42df1c,0xe59f5adc,0xed2d8b02};
constexpr TitleScreen::Site AcceptedSite{0x42e914,0xe5850034,0xea00004b};
constexpr U32 UpdateSVC=FirstSVC+1+ActionSites.size();
constexpr U32 AcceptedSVC=UpdateSVC+1;
template<class M> unsigned Byte(M& m,U32 at) {
    return (m.Read(at&~3u).value_or(~0u)>>((at&3)*8))&255;
}
template<class M> bool CodeMatches(M& m) {
    constexpr std::array<std::pair<U32,U32>,16> guards{{
        {Site.pc,Site.instruction},{Site.pc+4,Site.following},
        {UseItem,0xe92d4ff0},{UseItem+4,0xe35200fe},
        {0x34d718,0xe3540045},{0x34d754,0xe3540046},
        {0x349258,Save},{0x42ea00,Main},
        {0x42e994,0xe3a03034},{0x42e930,0xe3a00000},
        {0x42f400,0xe3a00e23},{0x42f404,0xe19010f8}, // Native Navi display flag.
        {0x42f114,0xe3a00e23},{0x42f118,0xe19000f8},
        {0x42f11c,0xe2800001},{0x42f120,0xe5850038}}};
    for(auto [at,word]:guards)if(m.Read(at)!=std::optional<U32>{word})return false;
    for(const auto& site:ActionSites)
        if(m.Read(site.pc)!=std::optional<U32>{site.instruction} ||
           m.Read(site.pc+4)!=std::optional<U32>{site.following})return false;
    if(m.Read(UpdateSite.pc)!=std::optional<U32>{UpdateSite.instruction} ||
       m.Read(UpdateSite.pc+4)!=std::optional<U32>{UpdateSite.following})return false;
    if(m.Read(AcceptedSite.pc)!=std::optional<U32>{AcceptedSite.instruction} ||
       m.Read(AcceptedSite.pc+4)!=std::optional<U32>{AcceptedSite.following})return false;
    return true;
}
inline U32 Instruction(U32 pc,U32 word) {
    if(pc==Site.pc && word==Site.instruction)return 0xef000000|FirstSVC;
    if(pc==UpdateSite.pc && word==UpdateSite.instruction)return 0xef000000|UpdateSVC;
    if(pc==AcceptedSite.pc && word==AcceptedSite.instruction)return 0xef000000|AcceptedSVC;
    for(unsigned i=0;i<ActionSites.size();++i)
        if(pc==ActionSites[i].pc && word==ActionSites[i].instruction)
            return 0xef000000|(FirstSVC+1+i);
    return word;
}
template<class M> U32 Player(M& m,U32 play) {
    // Same live player pointer used by the main camera.
    return m.Read(play+0x364+0xd8).value_or(0);
}
template<class M> bool Gameplay(M& m) {
    const U32 play=m.Read(UI+0xc).value_or(0);
    return HUDFade::GameplayReady(m,play) && !PauseMenu::Active(m) &&
        m.Read(UI+0x20)==std::optional<U32>{0} &&
        m.Read(Main+0x34)==std::optional<U32>{2};
}
template<class M> bool NaviCalling(M& m) {
    const U32 play=m.Read(UI+0xc).value_or(0);
    // Native HUD at 0x42f400 selects the Navi sprite when this s16 is nonzero;
    // its confirmation path publishes that same value + 1 for the player.
    return TitleScreen::Pointer(play,0x2dd8) &&
        (m.Read(play+0x100).value_or(0)&0xffff)==0x0203 &&
        (m.Read(play+0x2dd4).value_or(0)&0xffff)!=0;
}
template<class M> unsigned TextureVariant(M& m) {
    return Byte(m,Save+0x93)==8?1:0;
}
inline std::atomic<bool> look_requested{false};
inline void RequestLook() {look_requested.store(true);}
template<class M> bool Collected(M& m,Element item) {
    if(item==Ocarina) {const auto v=Byte(m,Save+0x93);return v==7 || v==8;}
    if(item==Iron || item==Hover) {
        const auto word=m.Read(Save+0xb4);
        return word && ((*word>>16)&(item==Iron?0x2000:0x4000));
    }
    return true;
}
template<class M> bool Owned(M& m,Element item) {
    if(item==Look)return true;
    if(item==Navi)return NaviCalling(m);
    if(item==Ocarina) {const auto v=Byte(m,Save+0x93);return v==7 || v==8;}
    const auto word=m.Read(Save+0xb4);
    const unsigned bit=item==Iron?0x2000:0x4000;
    const unsigned id=item==Iron?0x45:0x46,slot=item==Iron?0xa4:0xa5;
    return m.Read(Save+4)==std::optional<U32>{0} && word && ((*word>>16)&bit) &&
        Byte(m,Save+slot)==id;
}
template<class M> bool BootsAllowed(M& m,Element item) {
    if((item!=Iron && item!=Hover) || !Gameplay(m) || !Owned(m,item))return false;
    const U32 play=m.Read(UI+0xc).value_or(0),player=Player(m,play);
    if(!TitleScreen::Pointer(player,0x1750))return false;
    const auto flags=m.Read(player+0x1710),flags2=m.Read(player+0x1714),ui=m.Read(Main+0x68);
    // Keep swimming available (Iron Boots are needed underwater), but reject
    // cutscenes, death, item-get, riding, climbing and scripted player actions.
    return flags && flags2 && ui && !(*flags&0x20938630) &&
        !(*flags2&0x40000) && !(*ui&0xe0400000) && Byte(m,player+0x1749)==0;
}
struct Input {
    U32 previous=0,consumed=0,context=0;
    unsigned pending_item=ElementCount;
    unsigned confirming=ElementCount;
    std::int64_t pending_time=0;
    std::uint64_t sequence=0,update_sequence=0;
    // HID owns the pending request; an accepted action belongs to the current
    // native update and must survive a HID poll while Main is in state 10/11.
    void Cancel() {pending_item=ElementCount;context=0;}
    template<class M> void Filter(M& m,U32& buttons,bool live,std::int64_t now) {
        const bool look=look_requested.exchange(false);
        const U32 raw=buttons,edge=raw&~previous;previous=raw;
        consumed&=raw;
        const bool valid=live && CodeMatches(m);
        const bool shop=Shops::navigation_input.Poll(m,raw,valid && !PauseMenu::Active(m));
        if(!valid) {Cancel();consumed=0;return;}
        if(shop) {
            Cancel();consumed|=raw&Directions;buttons&=~consumed;
            return;
        }
        const bool dpad=GetLayout().enabled;
        if(!dpad && pending_item!=Look)Cancel();
        buttons&=~consumed;
        const U32 play=m.Read(UI+0xc).value_or(0);
        if(!Gameplay(m) || (raw&(2|4|8))) {Cancel();return;}
        if(context && context!=play)Cancel();
        context=play;
        if(pending_item!=ElementCount && (now<pending_time || now-pending_time>150000))
            pending_item=ElementCount;
        const U32 direction=dpad?raw&Directions:0;
        if(dpad){buttons&=~Directions;consumed|=direction;}
        // One action per press; diagonals never equip two conflicting boots.
        if(direction && !(direction&(direction-1)) && (edge&direction)) {
            pending_item=ElementCount;
            const auto item=direction==Up?Navi:direction==Right?Ocarina:
                            direction==Left?Iron:Hover;
            if((item==Look || item==Ocarina || item==Navi)?Owned(m,item):BootsAllowed(m,item)) {
                pending_item=item;pending_time=now;++sequence;
            }
        }
        if(look){pending_item=Look;pending_time=now;++sequence;}
    }
};
inline Input input;
template<class M,class R> void BeginUpdate(M& m,R& r) {
    input.update_sequence=input.sequence;
    input.confirming=ElementCount;
    r[5]=m.Read(0x42ea00).value_or(Main); // Displaced LDR r5,[pc,#0xadc].
}
template<class M,class R> void AcceptAction(M& m,R& r) {
    m.Write(r[5]+0x34,r[0]); // Displaced STR r0,[r5,#0x34].
    // States 10/11 only highlight a pressed touchscreen button. The actual
    // action is published by their confirmation branches. Complete those in
    // this update for a D-pad press, preserving all native restriction checks.
    if(input.confirming==Ocarina && r[0]==10)r[15]=0x42f150;
    else if((input.confirming==Look || input.confirming==Navi) && r[0]==11)r[15]=0x42f0cc;
}
template<class M,class R> void HandleAction(M& m,R& r,unsigned site,std::int64_t now) {
    const U32 next=ActionSites[site].pc+4;
    const auto item=(site==0 || site==4)?Ocarina:Look;
    r[14]=next; // Displaced BL, including LR on the shortcut path.
    if(site>=3) {
        if(input.confirming==item || (site==3 && input.confirming==Navi)) {
            const auto action=input.confirming;
            input.confirming=ElementCount;
            // An expired Navi request must never turn into View.
            const bool allowed=action!=Navi || NaviCalling(m);
            r[0]=allowed?1:0;r[15]=next;
            // Right-stick click always requests View (1), even while Navi
            // calls. D-pad Up follows the original flag+1 publication path.
            if(action==Look)r[15]=0x42f120;
        } else r[15]=HitTest;
        return;
    }
    // Revalidate at consumption, since HID and game updates have different
    // cadences. A release before this update must not lose the queued press.
    if(input.pending_item!=ElementCount &&
       ((!GetLayout().enabled && input.pending_item!=Look) || !CodeMatches(m) || !Gameplay(m) ||
        input.context!=m.Read(UI+0xc).value_or(0) ||
        now<input.pending_time || now-input.pending_time>150000))
        input.Cancel();
    if(input.pending_item==item || (item==Look && input.pending_item==Navi)) {
        const auto action=static_cast<Element>(input.pending_item);
        input.pending_item=ElementCount;
        if(Owned(m,action)) {
            input.confirming=action;r[0]=1;r[15]=next;return;
        }
    }
    r[15]=HitTest;
}
struct BootsHook {
    // Store the return frame on the guest stack so it survives a savestate.
    static constexpr U32 Frame=80,Cookie=0x44504144;
    template<class M,class R> void Handle(M& m,R& r,std::int64_t now) {
        const U32 sp=r[13];
        if(m.Read(sp+64)==std::optional<U32>{Cookie} &&
           m.Read(sp+68)==std::optional<U32>{sp^Cookie} &&
           m.Read(sp+52)==std::optional<U32>{sp+Frame} &&
           m.Read(sp+60)==std::optional<U32>{Site.pc+4}) {
            m.Write(sp+64,0);
            for(unsigned i=0;i<16;++i)r[i]=m.Read(sp+4*i).value_or(0);
            r[13]+=0x38; // Displaced epilogue instruction.
            return;
        }
        const auto item=static_cast<Element>(input.pending_item);
        const auto time=input.pending_time;const U32 context=input.context;
        input.confirming=ElementCount;
        // HID can run after this update's action checks. Preserve a new press
        // until the next update, instead of discarding it before it was seen.
        if((item==Look || item==Ocarina || item==Navi) && input.sequence!=input.update_sequence) {
            r[13]+=0x38;return;
        }
        // Also discard View requests rejected before their input predicate.
        // A blocked press must not fire later when the restriction clears.
        input.pending_item=ElementCount;
        const U32 play=m.Read(UI+0xc).value_or(0);
        if(!GetLayout().enabled || now<time || now-time>150000 || play!=context ||
           !CodeMatches(m) || !BootsAllowed(m,item) || sp<Frame ||
           !TitleScreen::Pointer(sp-Frame,Frame)) {r[13]+=0x38;return;}
        for(U32 at=0;at<Frame;at+=4)if(!m.Read(sp-Frame+at)){r[13]+=0x38;return;}
        for(unsigned i=0;i<16;++i)m.Write(sp-Frame+4*i,r[i]);
        m.Write(sp-Frame+64,Cookie);m.Write(sp-Frame+68,(sp-Frame)^Cookie);
        r[13]=sp-Frame;r[0]=play;r[1]=Player(m,play);r[2]=item==Iron?0x45:0x46;
        r[14]=Site.pc;r[15]=UseItem;
    }
};
}
