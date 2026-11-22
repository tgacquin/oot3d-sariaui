// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include "oot3d/hud_layout.h"

namespace OOT3D::Shops {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff2a0, ReturnToKeeper=0x1c2ecc, KeeperMenu=0x19157c;
constexpr std::array<TitleScreen::Site,19> Sites{{
    {0x103fc0,0xe12fff33,0xe1a00004}, // EnOssan action dispatch.
    {0x191598,0xeb07950e,0xe3500004}, // Keeper menu's message readiness query.
    {0x34785c,0xebfe2317,0xe28dd024}, // Shared float rectangle for shop prompts and area titles.
    {0x16d01c,0xecbd8b02,0xe8bd8070}, // Left pan's unfinished-frame return.
    {0x191560,0xecbd8b02,0xe8bd8070}, // Right pan's unfinished-frame return.
    {0x1ff388,0xeb05dd92,0xe3500005}, // Shelf message readiness queries.
    {0x216698,0xeb0580ce,0xe3500005},
    {0x16cf7c,0xeb08195f,0xed940adf}, // Camera approach: left, right, keeper.
    {0x1914c0,0xeb07880e,0xed940adf},
    {0x1c2ef0,0xeb06c182,0xed940adf},
    {0x103fc4,0xe1a00004,0xeb09ca25}, // Action returned: publish destination text now.
    {0x16d018,0xea07fb85,0xecbd8b02}, // Avoid reopening that text when the pan settles.
    {0x19155c,0xea076a34,0xecbd8b02},
    {0x355ef8,0xeb0057cd,0xea000008},
    {0x355f08,0xeb0057c9,0xe320f000},
    {0x355f20,0xeb0057c3,0xe3a00001},
    {0x35e04c,0xe3530000,0x012fff1e}, // Shared gold-bracket draw for every shop type.
    {0x103c94,0xed960a0f,0xe3a00000}, // Shop horizontal input, before native repeat accumulation.
    {0x103cb0,0xed960a10,0xe5c402d1}, // Shop vertical input.
}};
constexpr std::array<TitleScreen::Site,2> PanInputSites{{
    {0x16cfc8,0xb58402c8,0xba000012},
    {0x19150c,0x358402c8,0x3a000012},
}};
constexpr std::array<TitleScreen::Site,8> PromptSites{{
    {0x35de38,0xebff3c23,0xe3560000}, // Mirrored left/right arrows, then sticks.
    {0x35de80,0xebff3c11,0xe3550000},
    {0x35dec8,0xebff3bff,0xe3560000},
    {0x35df10,0xebff3bed,0xe28dd014},
    {0x35df60,0xebff3bd9,0xe3560000}, // Normal left/right arrows, then sticks.
    {0x35dfa4,0xebff3bc8,0xe3550000},
    {0x35dfe8,0xebff3bb7,0xe3560000},
    {0x35e02c,0xebff3ba6,0xe28dd014},
}};
inline U32 Instruction(U32 pc,U32 word) {
    // Preserve the native accumulator during the shelf pan so held navigation
    // can reach its normal repeat threshold and new flicks aren't discarded.
    for(const auto& s:PanInputSites)
        if(pc==s.pc && word==s.instruction)return 0xe320f000;
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    for(const auto& s:PromptSites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    for(const auto& s:PanInputSites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    return m.Read(0x52da00)==ReturnToKeeper && m.Read(0x52d9e8)==KeeperMenu &&
        m.Read(0x191650)==0xe59412c8 && m.Read(0x191654)==0xe59f01e0 &&
        // Stack frames used below: sprite helper 0x88, integer-to-float helper 0x48.
        m.Read(0x32cecc)==0xe92d40f0 && m.Read(0x32ced0)==0xed2d8b06 &&
        m.Read(0x32ced4)==0xe24dd05c && m.Read(0x32d140)==0xeb006992 &&
        m.Read(0x4792ac)==0xe1a04001 && m.Read(0x47946c)==0xe1d410f6 &&
        m.Read(0x479500)==0xebfb38a2 && m.Read(0x479528)==0x50c8e8 &&
        m.Read(0x479464)==0xe5932054 && m.Read(0x47948c)==0xe2407001 &&
        m.Read(0x347790)==0xe92d43f0 && m.Read(0x3477a0)==0xed2d8b02 &&
        m.Read(0x3477a4)==0xe24dd024 && m.Read(0x347848)==0xe28d3014 &&
        m.Read(0x52d9f0)==0x16cf58 && m.Read(0x52d9f4)==0x19149c &&
        m.Read(0x52d9f8)==0x1ff2dc && m.Read(0x52d9fc)==0x2165ec &&
        m.Read(0x16cf58)==0xe92d4070 && m.Read(0x16cf64)==0xed2d8b02 &&
        m.Read(0x19149c)==0xe92d4070 && m.Read(0x1914a8)==0xed2d8b02 &&
        m.Read(0x1ff3b4)==0xe5d402d0 && m.Read(0x2166c4)==0xe5d402d0 &&
        m.Read(0x34e27c)==0xe52d4004 && m.Read(0x34e280)==0xe5d122fa &&
        m.Read(0x35e04c)==0xe3530000 && m.Read(0x35e050)==0x012fff1e;
}
template<class M> bool Actor(M& m,U32 actor) {
    return TitleScreen::Pointer(actor,0x584) && (m.Read(actor).value_or(~0u)&0xffff)==0x3d &&
        m.Read(actor+0x228)==0x103c70;
}
// HID directions must be captured before gameplay shortcuts consume them.
// Revalidate the observed native shop on every poll and every use; never
// infer shop input from the game's already-filtered button word.
struct NavigationInput {
    U32 actor=0,play=0,held=0;
    U32 directions=0,pending_press=0,tapped=0;
    std::int64_t press_us=0,poll_us=0,tick_press_us=0;
    void ClearPress() {
        directions=pending_press=tapped=0;
        press_us=poll_us=tick_press_us=0;
    }
    void BeginTick() {
        // A short press can arrive and be released between shelf updates.
        // Deliver it once. A held direction still uses the native accumulator.
        const bool recent=press_us>0 && poll_us>=press_us && poll_us-press_us<=50000;
        tapped=recent?pending_press:0;
        tick_press_us=recent && pending_press?press_us:0;
        pending_press=0;
    }
    template<class M> bool Active(M& m) const {
        if(!Actor(m,actor) || !TitleScreen::Pointer(play,0x20b0) ||
           m.Read(0x5043e0)!=play)return false;
        const auto state=m.Read(actor+0x2a0).value_or(~0u)&0xffff;
        return state==2 || (state>=4 && state<=8);
    }
    template<class M> void Observe(M& m,U32 next_actor,U32 next_play) {
        if(actor!=next_actor || play!=next_play){held=0;ClearPress();}
        actor=next_actor;play=next_play;
        if(!Active(m))held=0;
    }
    template<class M> bool Poll(M& m,U32 raw,bool live,U32 stick=0,std::int64_t now=0) {
        const bool active=live && Active(m);
        held=active?raw&0xf0:0;
        const auto state=active?m.Read(actor+0x2a0).value_or(0)&0xffff:0;
        if((state==6 || state==7) && !(raw&~0xf00000f0u) && now>0) {
            if(now<poll_us)ClearPress();
            const U32 current=held?held:stick;
            if(current&~directions){press_us=now;pending_press=current;}
            directions=current;poll_us=now;
        } else ClearPress();
        return active;
    }
};
inline NavigationInput navigation_input;
template<class M> U32 NavigationAxis(M& m,U32 actor,U32 play,unsigned axis) {
    const U32 native=m.Read(play+0x3c+axis*4).value_or(0);
    if(!Actor(m,actor) || !TitleScreen::Pointer(play,0x20b0) || m.Read(0x5043e0)!=play)return native;
    const unsigned state=m.Read(actor+0x2a0).value_or(~0u)&0xffff;
    if(state!=2 && (state<4 || state>8))return native;
    const U32 positive=axis?0x40:0x10,negative=axis?0x80:0x20;
    const float stick=std::bit_cast<float>(native),magnitude=std::abs(stick);
    U32 held=0;
    if(navigation_input.actor==actor && navigation_input.play==play) {
        held=navigation_input.held;
        if(!(held&(positive|negative)) && magnitude<=15)
            held|=navigation_input.tapped&(positive|negative);
    }
    // Match an ordinary stick deflection. Opposing directions cancel, and
    // only the pressed axis overrides the stick. Shared player input is never
    // written: gameplay shortcuts and purchase buttons retain native behavior.
    if(held&(positive|negative)) {
        // Native stick X is already mirrored before the shop reads it. Raw
        // HID D-pad directions need the same transform (the flag also drives
        // the shop's mirrored prompts at 0x35dde4).
        const bool mirrored=!axis && ((m.Read(0x587958+0xc).value_or(0)>>16)&255);
        const float direction=float(int(bool(held&positive))-int(bool(held&negative)));
        return std::bit_cast<U32>((mirrored?-80.f:80.f)*direction);
    }
    // The native fresh-press and browsing checks require magnitude >30.
    // Admit smaller deliberate movements (>15) through that same path;
    // preserve larger deflections and their existing repeat speed.
    if(std::isfinite(stick) && magnitude>15 && magnitude<=30)
        return std::bit_cast<U32>(std::copysign(31.f,stick));
    return native;
}
struct DescriptionContext {
    U32 actor=0,play=0;
    template<class M> void Observe(M& m,U32 next_actor,U32 next_play) {
        actor=Actor(m,next_actor)?next_actor:0;
        play=actor?next_play:0;
    }
    template<class M> bool Active(M& m) const {
        constexpr U32 message=0x5c1878;
        if(!actor || !Actor(m,actor) || !TitleScreen::Pointer(play,0x20b0) ||
           m.Read(0x5043e0)!=play || m.Read(message)!=0x4ed0a0 ||
           !(m.Read(message+0xc).value_or(0)&255))return false;
        const auto state=m.Read(actor+0x2a0).value_or(~0u)&0xffff;
        // Browsing and camera pans only. Greetings, talking, buying, selling,
        // receipt/error messages and leaving the shop keep native timing.
        if(state!=2 && (state<4 || state>8))return false;
        const auto id=m.Read(message+0xf40).value_or(~0u);
        const bool mask_shop=(m.Read(actor+0x1c).value_or(~0u)&0xffff)==10;
        if(mask_shop?(id==0x70a2 || id==0x70ad):id==0x83)return true;
        // Accept the old or new item description during a replacement. Do not
        // assume a fixed message-ID range: randomizer items can change these.
        for(unsigned i=0;i<8;++i) {
            const auto item=m.Read(actor+0x2a4+i*4).value_or(0);
            if(!TitleScreen::Pointer(item,0x200) || (m.Read(item).value_or(~0u)&0xffff)!=4)continue;
            const auto text=m.Read(item+0x114);
            if(text && id==(*text>>16))return true;
        }
        return false;
    }
};
template<class M> void PlaceAreaTitle(M& m,U32 sp,U32 rect) {
    // The caller keeps the title descriptor in r4, saved at sp+0x2c.
    const U32 title=m.Read(sp+0x2c).value_or(0);
    if(!TitleScreen::Pointer(title,8))return;
    const auto xy=m.Read(title+4);
    if(!xy)return;
    const auto layout=HUD::GetLayout();
    const int y=static_cast<std::int16_t>(*xy>>16);
    const int dy=UserHUD::AreaTitleY(y,layout.reposition_area_titles,layout.area_title_y)-y;
    const float scale=layout.area_title_scale;
    if((scale==1 && dy==0) || !std::isfinite(scale))return;
    // The sprite helper converts quarter-pixel coordinates as unsigned.
    // Recover signed edges from the title draw's saved r5, r6 and r7 so
    // wide banners may extend offscreen even during their native animation.
    const auto left=m.Read(sp+0x30), right=m.Read(sp+0x34), bottom=m.Read(sp+0x38);
    const auto native_y_offset=m.Read(0x50c8e8+0x54);
    if(!left || !right || !bottom || !native_y_offset)return;
    const auto pixels=[](U32 edge){return std::bit_cast<std::int32_t>(edge)*.25f;};
    const float bottom_y=pixels(*bottom);
    // Native bottom is one quarter-pixel inside the symmetric lower edge.
    const float top_y=2.f*(y+float(std::bit_cast<std::int32_t>(*native_y_offset)))-bottom_y-.25f;
    const std::array bounds{top_y,bottom_y,pixels(*left),pixels(*right)};
    // Resize the fresh animated bounds about their midpoint. Moving the
    // finished rectangle also lets a negative Y offset cross the screen edge.
    for(unsigned axis=0;axis<4;axis+=2) {
        const float center=(bounds[axis]+bounds[axis+1])*.5f;
        const float radius=(bounds[axis+1]-bounds[axis])*.5f*scale;
        const float position=center+(axis==0?dy:0);
        m.Write(rect+4*axis,std::bit_cast<U32>(position-radius));
        m.Write(rect+4*(axis+1),std::bit_cast<U32>(position+radius));
    }
}
template<class M> void PlaceSpriteRect(M& m,U32 sp,U32 rect) {
    // 347790 serves multiple callers. Match the stack-local draw rectangle and saved LR.
    if(!TitleScreen::Pointer(sp,0x48) || rect!=sp+0x14)return;
    const auto sprite_caller=m.Read(sp+0x44);
    if(sprite_caller==0x479504) {PlaceAreaTitle(m,sp,rect);return;}
    if(sprite_caller!=0x32d144 || !TitleScreen::Pointer(sp,0xd0))return;
    const auto caller=m.Read(sp+0xcc);
    unsigned prompt=0;
    while(prompt<PromptSites.size() && caller!=PromptSites[prompt].pc+4)++prompt;
    if(prompt==PromptSites.size() || !Actor(m,m.Read(sp+0xbc).value_or(0)))return;
    const unsigned side=(prompt%2)^(prompt<4?1:0);
    const auto offset=HUD::GetLayout().shop_prompt_offsets[side];
    std::array<float,4> bounds;
    if(!TitleScreen::ReadFloats(m,rect,bounds) || !std::isfinite(offset[0]) || !std::isfinite(offset[1]))return;
    // The early API converts signed fixed-point coordinates back to unsigned
    // floats. Offsetting there wraps any negative edge to ~1 billion pixels.
    // Move the resulting float rectangle instead; preserve size, UVs and alpha.
    for(unsigned i=0;i<bounds.size();++i) {
        const unsigned axis=i<2?1:0; // top, bottom, left, right
        const float limit=axis?240.f:400.f;
        m.Write(rect+i*4,std::bit_cast<U32>(bounds[i]+std::clamp(offset[axis],-limit,limit)));
    }
}
template<class M> bool ContinueBrowsing(M& m,U32 actor,U32 play) {
    if(!Actor(m,actor) || !TitleScreen::Pointer(play,0x20b0) || m.Read(0x5043e0)!=play)return false;
    const U32 state=m.Read(actor+0x2a0).value_or(~0u)&0xffff;
    if(state!=2 && state!=8)return false;
    const auto angle=m.Read(actor+0x37c),input=m.Read(play+0x3c),held=m.Read(actor+0x2c8),
        fresh=m.Read(actor+0x2d0),pressed=m.Read(play+0x18);
    // HID includes circle-pad directions in the pressed mask. Those are the
    // very flicks we want; only non-direction buttons retain native priority.
    if(!angle || !input || !held || !fresh || !pressed || (*pressed&~0xf00000f0u)!=0)return false;
    const float pan=std::bit_cast<float>(*angle),stick=std::bit_cast<float>(NavigationAxis(m,actor,play,0));
    const auto accumulated=std::bit_cast<std::int32_t>(*held);
    if(!std::isfinite(pan) || std::abs(pan)>30 || !std::isfinite(stick) ||
       std::abs(stick)<=30 || std::abs(stick)>160 || accumulated*double(stick)<=0)return false;
    // A second flick is immediate. Holding through the center uses the same
    // accumulated-stick threshold as item navigation, once the keeper is near.
    // Releasing leaves the native center stop, talk and quit menu intact.
    if(!(*fresh&255) && !((state==2 || std::abs(pan)<=10) && std::abs(double(accumulated))>500))return false;
    // An empty shelf must not trap us in an input-only frame: let the normal
    // return animation continue. Native selection handles row/slot preference.
    const unsigned first=stick<0?4:0;
    for(unsigned i=first;i<first+4;++i) {
        const U32 item=m.Read(actor+0x2a4+i*4).value_or(0);
        if(TitleScreen::Pointer(item,0x200) && (m.Read(item).value_or(~0u)&0xffff)==4)return true;
    }
    return false;
}
template<class M> bool BrowseShelf(M& m,U32 actor,U32 play) {
    if(!Actor(m,actor) || !TitleScreen::Pointer(play,0x20b0) || m.Read(0x5043e0)!=play)return false;
    const auto state=m.Read(actor+0x2a0).value_or(~0u)&0xffff;
    if(state<4 || state>7 || (m.Read(play+0x18).value_or(~0u)&~0xf00000f0u))return false;
    const unsigned selected=(m.Read(actor+0x2f8).value_or(~0u)>>16)&255;
    if(selected>=8 || ((state==4 || state==6)!=(selected>=4)))return false;
    const auto item=m.Read(actor+0x2a4+selected*4).value_or(0);
    if(!TitleScreen::Pointer(item,0x200) || (m.Read(item).value_or(~0u)&0xffff)!=4)return false;
    const auto fresh=m.Read(actor+0x2d0).value_or(0);
    for(unsigned axis=0;axis<2;++axis) {
        const auto input=m.Read(play+0x3c+axis*4),held=m.Read(actor+0x2c8+axis*4);
        if(!input || !held)continue;
        const auto stick=std::bit_cast<float>(NavigationAxis(m,actor,play,axis));
        const auto accumulated=std::bit_cast<std::int32_t>(*held);
        if(std::isfinite(stick) && std::abs(stick)>30 && std::abs(stick)<=160 &&
           double(accumulated)*stick>0 && (((fresh>>(axis*8))&255) || std::abs(double(accumulated))>500))return true;
    }
    return false;
}
template<class M,class R,class F> void Handle(M& m,R& r,F& fp,unsigned site) {
    if(site==17 || site==18) {
        navigation_input.Observe(m,r[4],r[6]);
        if(site==17)navigation_input.BeginTick();
        fp[0]=NavigationAxis(m,r[4],r[6],site-17); // Displaced VLDR s0.
    } else if(site==0) {
        // Keep the native function's prologue/epilogue, slot selection, sound,
        // camera pan and item-description transition. No synthetic purchases.
        r[14]=Sites[0].pc+4;
        r[15]=r[3]==ReturnToKeeper && ContinueBrowsing(m,r[0],r[1])?KeeperMenu:r[3];
    } else if(site==1) {
        if(ContinueBrowsing(m,r[4],r[7])) {
            // Only directional browsing in the return/keeper-menu states.
            // Do not make the new input wait for the center prompt's text to
            // finish opening either. Talk/quit still use the native checks.
            r[5]=r[4]+0x200;r[6]=0;r[15]=0x191650;
        } else {r[14]=Sites[1].pc+4;r[15]=0x3769d8;}
    } else if(site==2) {
        PlaceSpriteRect(m,r[13],r[3]);
        r[14]=Sites[site].pc+4;r[15]=0x2d04c0;
    } else if(site==3 || site==4) {
        const U32 actor=r[4],play=r[5],sp=r[13];
        // Replay VPOP d8. When navigating, finish this pan frame's epilogue
        // and tail-call the native shelf handler with the same return address.
        fp[16]=m.Read(sp).value_or(0);fp[17]=m.Read(sp+4).value_or(0);r[13]+=8;
        if(BrowseShelf(m,actor,play)) {
            for(unsigned i=0;i<3;++i)r[4+i]=m.Read(sp+8+i*4).value_or(0);
            r[14]=m.Read(sp+20).value_or(0);r[13]+=16;
            r[0]=actor;r[1]=play;r[15]=site==3?0x1ff2dc:0x2165ec;
        }
    } else if(site==5 || site==6) {
        if(BrowseShelf(m,r[4],r[6])) {
            // Directional selection only. Do not bypass the item-return
            // animation above this point or the native A/B purchase handling.
            r[15]=site==5?0x1ff3b4:0x2166c4;
        } else {r[14]=Sites[site].pc+4;r[15]=0x3769d8;}
    } else if(site>=7 && site<=9) {
        // Two extra native approach steps, followed by the original step:
        // 1.5x the previous two-step pan, without speeding up item/input updates.
        // Preserve the game's timestep and easing, including its final snap.
        const auto play=m.Read(0x51b2f4).value_or(0);
        const auto step=m.Read(play+0x110);
        const auto value=m.Read(r[0]);
        if(Actor(m,r[4]) && r[0]==r[4]+0x37c && step && value) {
            const float dt=float(std::int16_t(*step&0xffff));
            const float current=std::bit_cast<float>(*value),target=std::bit_cast<float>(fp[0]);
            const float rate=std::bit_cast<float>(fp[1]),cap=std::bit_cast<float>(fp[2]);
            if(dt>0 && dt<=3 && std::isfinite(current) && std::isfinite(target) &&
               rate==.5f && cap==10.f) {
                const float limit=dt*cap*(1.f/3.f);
                float advanced=current;
                for(unsigned i=0;i<2;++i) {
                    const float difference=target-advanced;
                    const float delta=std::abs(difference)<.00001f?difference:difference*(dt*rate*(1.f/3.f));
                    advanced+=std::clamp(delta,-limit,limit);
                }
                m.Write(r[0],std::bit_cast<U32>(advanced));
            }
        }
        r[14]=Sites[site].pc+4;r[15]=0x373500;
    } else if(site==10) {
        r[0]=r[4];
    } else if(site>=11 && site<=15) {
        const bool same=m.Read(0x5c1878)==0x4ed0a0 &&
            (m.Read(0x5c1884).value_or(0)&255) && m.Read(0x5c27b8)==r[1];
        if(site<=12) {r[15]=same?r[14]:0x36be34;}
        else if(!same) {r[14]=Sites[site].pc+4;r[15]=0x36be34;}
    }
}
struct UpdateHook : DescriptionContext {
    unsigned before=~0u;
    bool observed=false,pending=false;
    std::array<U32,16> saved{};
    std::array<U32,64> saved_fp{};
    U32 saved_flags=0,saved_fpscr=0;
    struct CursorCall {
        bool pending=false;
        U32 pan_actor=0,pan_play=0;
        unsigned pan_state=0;
        std::array<U32,16> saved{};
        std::array<U32,64> fp{};
        U32 flags=0,fpscr=0;
    } cursor;
    template<class M,class R,class F> void Handle(M& m,R& r,F& fp,U32& flags,U32& fpscr,unsigned site) {
        if(site==16) {
            if(cursor.pending) {
                r=cursor.saved;fp=cursor.fp;flags=cursor.flags;fpscr=cursor.fpscr;
                cursor.pending=false;
            } else {
                const U32 shop=r[1],context=r[0];
                const auto state=m.Read(shop+0x2a0).value_or(~0u)&0xffff;
                const unsigned selected=(m.Read(shop+0x2f8).value_or(~0u)>>16)&255;
                const U32 item=selected<8?m.Read(shop+0x2a4+selected*4).value_or(0):0;
                const bool valid=Actor(m,shop) && TitleScreen::Pointer(context,0x20b0) &&
                   m.Read(0x5043e0)==context && r[2]==shop+0x2d4 &&
                   selected<8 && TitleScreen::Pointer(item,0x200) && (m.Read(item).value_or(~0u)&0xffff)==4;
                const bool panning=valid && (state==4 || state==5) && (state==4)==(selected>=4);
                if(panning) {cursor.pan_actor=shop;cursor.pan_play=context;cursor.pan_state=state;}
                // The pan changes to state 6/7 before that state's first
                // update enables the native cursor. Bridge only this handoff;
                // do not override later purchase/item-return cursor hiding.
                const bool handoff=valid && cursor.pan_actor==shop && cursor.pan_play==context &&
                    cursor.pan_state>=4 && state==cursor.pan_state+2 &&
                    (state==6)==(selected>=4) && r[3]==0;
                // Native shelf updates position BEFORE changing selection.
                // Refresh visible settled cursors here so this draw follows
                // the new item immediately, rather than one 30 Hz tick later.
                const bool browsing=valid && (state==6 || state==7) &&
                    (state==6)==(selected>=4) && r[3]!=0;
                if(!panning && !handoff) {cursor.pan_actor=cursor.pan_play=0;cursor.pan_state=0;}
                if(panning || handoff || browsing) {
                    // Reuse the native item's world-position/eye-offset routine
                    // every draw, including selections during/after the pan.
                    // Force only the draw argument: the native visibility byte
                    // also controls item behavior and must remain untouched.
                    cursor.saved=r;cursor.fp=fp;cursor.flags=flags;cursor.fpscr=fpscr;
                    if(!browsing)cursor.saved[3]=1;
                    cursor.pending=true;r[14]=Sites[site].pc;r[15]=0x34e27c;return;
                }
            }
            // Replay CMP r3,#0. Keep the caller's LR: this entry is shared by
            // eight shop draw paths, all of which must return to their own tail.
            flags=(flags&0x0fffffffu)|(r[3]&0x80000000u)|
                (r[3]==0?0x40000000u:0u)|0x20000000u;
            return;
        } else if(site==0) {
            Observe(m,r[0],r[1]);
            before=m.Read(actor+0x2a0).value_or(~0u)&0xffff;observed=true;
        } else if(site==10) {
            if(pending) {
                r=saved;fp=saved_fp;flags=saved_flags;fpscr=saved_fpscr;pending=false;
            } else if(observed) {
                observed=false;
                const auto after=m.Read(actor+0x2a0).value_or(~0u)&0xffff;
                U32 message=0;
                if(actor==r[4] && Actor(m,actor) && m.Read(0x5043e0)==play && before!=after &&
                   (before==2 || (before>=4 && before<=8))) {
                    if(after==4 || after==5) {
                        const unsigned selected=(m.Read(actor+0x2f8).value_or(~0u)>>16)&255;
                        const U32 item=selected<8?m.Read(actor+0x2a4+4*selected).value_or(0):0;
                        if(TitleScreen::Pointer(item,0x200) && (m.Read(item).value_or(~0u)&0xffff)==4)
                            message=m.Read(item+0x114).value_or(0)>>16;
                    } else if(after==8) {
                        const bool mask=(m.Read(actor+0x1c).value_or(~0u)&0xffff)==10;
                        const unsigned masks=m.Read(0x588864).value_or(0)>>16;
                        message=mask?((masks&0xf00)==0xf00?0x70ad:0x70a2):0x83;
                    }
                }
                if(message && m.Read(0x5c27b8)!=message) {
                    saved=r;saved_fp=fp;saved_flags=flags;saved_fpscr=fpscr;pending=true;
                    r[0]=play;r[1]=message;r[14]=Sites[10].pc;r[15]=0x36be34;return;
                }
            }
        }
        Shops::Handle(m,r,fp,site);
    }
};
}
