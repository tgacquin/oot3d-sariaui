// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"
#include "oot3d/textbox_layout.h"
#include "oot3d/textbox_close.h"
#include "oot3d/textbox_open.h"
#include "oot3d/textbox_indicator.h"

namespace OOT3D::PauseDescription {
using TitleScreen::U32;
constexpr U32 Message=0x5c1878, FirstSVC=0xfff0a0;
constexpr std::array<TitleScreen::Site,9> Sites{{
    {0x4807fc,0xe1a04000,0xe5d0000c}, // Message update, after native register save.
    {0x480810,0xe5d40f38,0xe350000f}, // Message-state dispatch.
    {0x480b9c,0xe28dd00c,0xe8bd8030}, // Common update epilogue.
    {0x480b8c,0xe28dd00c,0xe2840044}, // Retired-text cleanup tail call.
    {0x48b5c4,0xe1a06001,0xe1a09002}, // Native reveal loop's character budget.
    {0x2f2d7c,0xeeb00a68,0xe1a00004}, // Pane animation delta, s0 <- s17.
    {TextboxClose::PresentSite,0xe59d0040,0xe320f000}, // Top-screen UI draw continuation.
    {0x480930,0xe59f3270,0xe59f2270}, // Accepted page-advance A/B input (state 5).
    {0x48821c,0xe2840044,0xebf9f54b}, // Accepted final/linked-message A/B (state 6).
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|FirstSVC+i;
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return m.Read(0x480854)==std::optional<U32>{0x480ab0} && // State 13: replacement wait.
           m.Read(0x48082c)==std::optional<U32>{0x4808e4} && // State 3: build text.
           m.Read(0x487ee4)==std::optional<U32>{0xebf9ab67}; // Plate uses the pane clock.
}
template<class M> U32 Byte(M& m,U32 at) {
    return (m.Read(at&~3u).value_or(0)>>((at&3)*8))&255;
}
template<class M> bool Description(M& m) {
    if(!PauseMenu::Active(m) || m.Read(Message)!=std::optional<U32>{0x4ed0a0} ||
       !Byte(m,Message+0xc))return false;
    const auto page=m.Read(0x5066f8+0x34).value_or(0);
    const auto gear=m.Read(0x50446c+0x18).value_or(0);
    const auto id=m.Read(Message+0xf40).value_or(~0u);
    // Items (2eb9d8) and Gear (438964) both select 0x700 + item ID.
    // Limit this to their opening/selection/equip states, excluding map
    // descriptions, page exits and normal conversation messages.
    const bool items=page==1 || page==2 || page==8 || page==13;
    const bool equipment=page==0 && (gear==1 || gear==2 || gear==3 || gear==9);
    return (items || equipment) && id>=0x700 && id<0x800;
}
struct Hook {
    TextboxClose::State close;
    TextboxIndicator::State indicator;
    TextboxIndicator::ChoiceState choice;
    template<class M,class R,class F> void Present(M& m,R& r,F& f,U32& flags,U32& fpscr,
                                                  std::int64_t now,bool supported) {
        // Chain retained draws once, then return to the native continuation.
        // Each model restores the complete CPU state before the next begins.
        if(choice.pending)choice.Resume(r,f,flags,fpscr);
        else {
            if(indicator.pending)indicator.Resume(r,f,flags,fpscr);
            else {
                close.Present(m,r,f,flags,fpscr,now,supported);
                if(!close.pending)indicator.Present(m,r,f,flags,fpscr,now,supported);
            }
            if(!close.pending && !indicator.pending)
                choice.Present(m,r,f,flags,fpscr,now,supported);
        }
    }
    bool updating=false;
    bool message_updating=false,starting_text=false,advance_candidate=false;
    U32 stack=0;
    unsigned passes=0;
    template<class M,class R,class F> void Handle(M& m,R& r,F& f,unsigned site,bool supported=true,
                                                 bool half_frame=false,bool shop_description=false,
                                                 std::int64_t now=0) {
        const bool description=Description(m) || shop_description;
        if(site==0) {
            r[4]=r[0]; // Displaced MOV.
            updating=supported && r[4]==Message && description;
            message_updating=supported && r[4]==Message;
            starting_text=false;
            const auto state=Byte(m,Message+0xf38);
            const auto play=m.Read(Message+8).value_or(0);
            // Only an input edge while waiting on a page/choice may bootstrap
            // the next message. Scripted delays and text already typing stay native.
            advance_candidate=message_updating && !updating && !PauseMenu::Active(m) &&
                m.Read(Message)==0x4ed0a0 && Byte(m,Message+0xc) &&
                (state==5 || state==6 || state==7) && (m.Read(play+0x18).value_or(0)&3);
            stack=r[13]-12;passes=0;
            return;
        }
        if(site==7 || site==8) {
            if(supported && r[4]==Message)indicator.Press(m,now);
            if(site==7)r[3]=m.Read(0x480ba8).value_or(0);
            else r[0]=r[4]+0x44;
            return;
        }
        if(site==4) {
            r[6]=r[1]; // Displaced MOV; NPC lettering retains its normal speed.
            if(supported && updating && r[0]==Message+0xeac && description)
                r[6]=std::max(r[6],4096u); // Native parser still stops at page/control boundaries.
            else if(supported && message_updating && advance_candidate &&
                    r[0]==Message+0xeac && Byte(m,Message+0xf38)==4) {
                // The click belongs to the previous page. Reveal one native
                // character now; do not also interpret it as "skip this page".
                r[6]=1;r[2]=0;
            }
            return;
        }
        if(site==5) {
            f[0]=f[17]; // Displaced VMOV s0,s17; no FPSCR changes.
            // All native pane clocks use a 30 Hz delta. Pause now executes
            // them at 60 Hz. Preserve native duration before applying user speed.
            if(half_frame)f[0]=std::bit_cast<U32>(std::bit_cast<float>(f[0])*.5f);
            if(supported && m.Read(Message)==std::optional<U32>{0x4ed0a0} &&
               Byte(m,Message+0xc) && Byte(m,Message+0xf38)==2 &&
               m.Read(Message+0x56c)==std::optional<U32>{r[4]}) {
                const float delta=std::bit_cast<float>(f[0]);
                if(std::isfinite(delta) && delta>0 && delta<=2)
                    f[0]=std::bit_cast<U32>(delta*Textboxes::OpeningSpeed(Textboxes::GetLayout().opening_speed));
            }
            return;
        }
        const bool active=supported && updating && r[4]==Message && r[13]==stack && description;
        const bool next_text=supported && message_updating && advance_candidate &&
            r[4]==Message && r[13]==stack &&
            (Byte(m,Message+0xf38)==3 || Byte(m,Message+0xf38)==13);
        if(site==2 && supported && message_updating && r[4]==Message && r[13]==stack) {
            if(next_text)starting_text=true;
            if(TextboxOpening::FinishAtFullSize(m)) {
                starting_text=true;
                if(++passes<8) {r[15]=Sites[1].pc;return;}
            } else if(starting_text) {
                const auto state=Byte(m,Message+0xf38),text=Byte(m,Message+0xeb4);
                // Run native preparation and the first reveal tick this update.
                // Ordinary dialogue keeps its normal per-character budget;
                // stop before another reveal tick, input wait or page advance.
                const bool prepare=(advance_candidate && state==13) || state==3 || (state==4 && (text==1 || text==2));
                if(state==4 && text==2)starting_text=false;
                if(prepare && ++passes<8) {r[15]=Sites[1].pc;return;}
                starting_text=false;
            }
        }
        if(site==1) {
            r[0]=Byte(m,r[4]+0xf38); // Displaced LDRB.
            if((active || (message_updating && advance_candidate && r[4]==Message && r[13]==stack)) && r[0]==13) {
                // Let the native replacement branch perform all its cleanup
                // setup this update instead of waiting three updates first.
                if(m.Read(Message+0xfa4))m.Write(Message+0xfa4,1);
            }
            return;
        }
        if(site==3 && (active || next_text)) {
            // The native cleanup normally pops the update stack and tail-calls
            // 487cb8. Call that same cleanup with the stack intact, then return
            // through the common epilogue so pending text can finish this frame.
            r[0]=Message+0x44;r[14]=Sites[2].pc;r[15]=0x487cb8;
            return;
        }
        if(site==2 && active) {
            const auto state=Byte(m,Message+0xf38),text=Byte(m,Message+0xeb4);
            // Drain preparation/reveal only. Never replay an opening animation,
            // input wait, page advance, or close. Bound retries if resources lag.
            if((state==3 || (state==4 && (text==1 || text==2))) && ++passes<8) {
                r[15]=Sites[1].pc;return;
            }
        }
        if(site==2 || site==3) {
            r[13]+=12; // Displaced ADD sp,sp,#12.
            updating=false;
            message_updating=starting_text=advance_candidate=false;
        }
    }
};
}
