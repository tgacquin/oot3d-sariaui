// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"
#include "oot3d/pause_selection.h"
#include "oot3d/file_select_timing.h"
#include "oot3d/boss_challenge_layout.h"

namespace OOT3D::PauseFramerate {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff120, Renderer=0x5be5b8;
// Original US code: frame deadline/accounting, preview angle, item-flight
// easing/duration, input repeat and the equip-completion effect's clock.
constexpr std::array<TitleScreen::Site,38> Sites{{
    {0x41af74,0xe2800002,0xe585008c},
    {0x3fd3c4,0xe2811002,0xe0800001},
    {0x425704,0xeeb80ac0,0xee200a2b},
    {0x4258c8,0xe2800001,0xe1c901b8},
    {0x4343b8,0xe0811fa1,0xe08000c1},
    {0x4343d8,0xe0811fa1,0xe08000c1},
    {0x434450,0xe0811fa1,0xe08000c1},
    {0x434470,0xe0811fa1,0xe08000c1},
    {0x4344a8,0xe3500004,0xc3a0000a},
    {0x434a0c,0xe3500006,0xe5840080},
    {0x42dc30,0xe352000c,0xb2822001},
    {0x42dc48,0xe3520003,0xe5862014},
    {0x42dc80,0xe351000c,0xb2811001},
    {0x42dc98,0xe3510003,0xe586101c},
    {0x434888,0xeef80ac0,0xed1f0a2b},
    {0x4348a4,0xeef80ae0,0xee000ac1},
    {0x4348f4,0xeef80ac0,0xed9f0ace},
    {0x434910,0xeef80ae0,0xee300a80},
    {0x43492c,0xeeb80ac0,0xee300ac0},
    {0x434948,0xeeb80ac0,0xee300ac0},
    {0x43499c,0xeef80ac0,0xed9f0aa4},
    {0x4349b8,0xeef80ae0,0xee300a80},
    {0x4349d4,0xeeb80ac0,0xee300ac0},
    {0x4349f0,0xeeb80ac0,0xee300ac0},
    {0x434a6c,0xeeb80ac0,0xee300ac0},
    {0x434a88,0xeeb80ac0,0xee300ac0},
    {0x434acc,0xeeb80ac0,0xee300ac0},
    {0x434ae8,0xeeb80ac0,0xee300ac0},
    {0x439430,0xe3a00001,0xe5850024}, // Prompt ready: retain opening until the fade completes.
    {0x43a65c,0xe3100001,0xe320f000}, // Post-save Continue/Quit: native pressed-A test.
    {0x43b168,0xe3a03000,0xe58d3000}, // Skip footer stone touch selection.
    {0x43b408,0xe3500002,0x1a00001c}, // Stop Down at the last settings row.
    {0x43b574,0xe5950030,0xe3500003}, // A accepts Options from every row.
    {0x43b0b4,0xe3a00001,0xe5850028}, // Options -> Save preparation.
    {0x2eb494,0xe594010c,0xeb001ded}, // Selected icon rectangle before helper copy.
    {0x41d548,0x13a0000c,0x1a000002}, // Boss selector initial held-input delay.
    {0x41d558,0xd3a00003,0xe25cc001}, // Boss selector repeat interval.
    {0x4346d4,0xe5846080,0xe2856008}, // Landing pulse begins after equip commit.

}};
// Save/Options use their own integer slide clock (owner+2c). Count in
// 60 Hz units, then evaluate the native positions at half-frame intervals.
// origin is the constant term in each original position expression; keeping
// it intact matters for panels sliding in and the offset prompt sliding out.
constexpr U32 SaveFirstSVC=0xfff160;
struct SaveSite {TitleScreen::Site code;float origin;};
constexpr std::array<SaveSite,32> SaveSites{{
    {{0x43935c,0xeeb80ac0,0xed8d0a07},-64},
    {{0x439398,0xeeb80ac0,0xed8d0a06},320},
    {{0x4393d8,0xeeb80ac0,0xed8d0a07},64},
    {{0x4393f8,0xe3500004,0xba00000d},0},
    {{0x4397cc,0xeeb80ac0,0xed8d0a07},0},
    {{0x439804,0xeeb80ac0,0xed8d0a06},0},
    {{0x439840,0xeeb80ac0,0xed8d0a07},0},
    {{0x439860,0xe3500004,0xbafffef3},0},
    {{0x43a0ec,0xeeb80ac0,0xed8d0a07},0},
    {{0x43a124,0xeeb80ac0,0xed8d0a06},0},
    {{0x43a160,0xeeb80ac0,0xed8d0a07},0},
    {{0x43a180,0xe3500004,0xba000015},0},
    {{0x43a220,0xeeb80ac0,0xed8d0a06},0},
    {{0x43a25c,0xeeb80ac0,0xed8d0a07},0},
    {{0x43a27c,0xe3500004,0xbaffffd6},0},
    {{0x43a2d4,0xeeb80ac0,0xed8d0a06},0},
    {{0x43a310,0xeeb80ac0,0xed8d0a07},0},
    {{0x43a330,0xe3500004,0xbaffffa9},0},
    {{0x43a378,0xe3500002,0xe584002c},0},
    {{0x43a4cc,0xeeb80ac0,0xed8d0a06},320},
    {{0x43a50c,0xeeb80ac0,0xed8d0a06},320},
    {{0x43a538,0xe3500004,0xbaffff27},0},
    {{0x43ab94,0xeeb80ac0,0xed8d0a06},0},
    {{0x43abd4,0xeeb80ac0,0xed8d0a07},-168},
    {{0x43ac0c,0xeeb80ac0,0xed8d0a06},0},
    {{0x43ac38,0xe3500004,0xba000008},0},
    {{0x43af58,0xeeb80ac0,0xed8d0a06},320},
    {{0x43af98,0xeeb80ac0,0xed8d0a07},64},
    {{0x43afb8,0xe3500004,0x03a01002},0},
    {{0x43affc,0xeeb80ac0,0xed8d0a06},0},
    {{0x43b038,0xeeb80ac0,0xed8d0a07},0},
    {{0x43b058,0xe3500004,0x12800001},0},
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    for(unsigned i=0;i<SaveSites.size();++i)
        if(pc==SaveSites[i].code.pc && word==SaveSites[i].code.instruction)return 0xef000000|(SaveFirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    for(const auto& s:SaveSites)
        if(m.Read(s.code.pc)!=std::optional<U32>{s.code.instruction} ||
           m.Read(s.code.pc+4)!=std::optional<U32>{s.code.following})return false;
    // Pane timing shares the existing description hook; require it too.
    return m.Read(0x2f2d7c)==std::optional<U32>{0xeeb00a68} &&
           m.Read(0x41af84)==std::optional<U32>{0x54cc34};
}
inline U32 Compare(U32 cpsr,U32 a,U32 b) {
    const U32 result=a-b;
    return (cpsr&0x0fffffff)|(result&0x80000000)|(result==0?0x40000000:0)|
        (a>=b?0x20000000:0)|(((a^b)&(a^result)&0x80000000)>>3);
}
struct Hook {
    FileSelectTiming::Hook file;
    PauseSelection::State selection;
    bool fast=false, death_fast=false, death_half=false,boss_fast=false;
    template<class M> static bool BossSelector(M& m) {
        const U32 state=m.Read(BossChallenge::Owner+8).value_or(0)&255;
        return BossChallenge::Loaded(m) && !BossChallenge::InBattle(m) && ((state>=1 && state<=7) || state==13);
    }
    float preview_fraction=0;
    U32 preview_owner=0;
    std::array<float,4> remainder{};
    std::int64_t frame_us=0,landing_start_us=0;
    bool landing_clock=false;
    float LandingFrame() const {
        // Native setup displays the unexpanded copy for one 30 Hz tick,
        // then samples 0..5 over six ticks. Preserve that initial hold too.
        // Microsecond timestamp rounding must not turn an exact pixel into
        // e.g. 47.9999 before the native float-to-integer conversion.
        const float ticks=std::round(float(frame_us-landing_start_us)*.03f)/1000.f;
        return std::clamp(ticks-1.f,0.f,5.f);
    }

    template<class M,class R,class F>
    void HandleSave(M& m,R& r,F& f,U32& cpsr,unsigned site) {
        const auto& s=SaveSites[site];
        const bool active=fast && SaveMenu::Active(m);
        if(s.code.instruction==0xeeb80ac0) {
            const float value=float(std::bit_cast<std::int32_t>(f[0]));
            f[0]=std::bit_cast<U32>(active?(value+s.origin)*.5f:value);
            if(active && m.Read(SaveMenu::Owner+0x28)==std::optional<U32>{2}) {
                // Hold the prompt at rest. Button slide offsets remain native
                // so parked label variants stay hidden after the transition;
                // fresh vertex uploads replace only their visible motion.
                switch(site) {
                case 0: case 4: case 8:
                    f[0]=std::bit_cast<U32>(0.f);break;
                case 23:f[0]=std::bit_cast<U32>(-168.f);break;
                default:break;
                }
            }
        } else {
            const U32 limit=s.code.instruction&255; // Native CMP r0,#2 / #4.
            cpsr=Compare(cpsr,r[0],active?limit*2:limit);
            if(active && site==3 && m.Read(SaveMenu::Owner+0x28)==std::optional<U32>{2}) {
                // Construct the localized prompt on the first entrance frame,
                // not after the stones stop. Do not allocate it again at the end.
                if(m.Read(SaveMenu::Owner+0x1c).value_or(0)==0)cpsr=Compare(cpsr,8,8);
                else if(r[0]>=8)r[15]=0x439430;
            }
        }
    }

    template<class M,class R,class F>
    void Handle(M& m,R& r,F& f,U32& cpsr,unsigned site,std::int64_t now=0) {
        if(site==37) {
            m.Write(r[4]+0x80,r[6]); // Displaced STR.
            landing_clock=fast && r[4]==0x5066f8 && frame_us>0;
            landing_start_us=frame_us;
            return;
        }
        if(site==35 || site==36) {
            const bool boss=boss_fast && r[5]==BossChallenge::Owner;
            const bool z=(cpsr&0x40000000)!=0;
            const bool n=(cpsr&0x80000000)!=0,v=(cpsr&0x10000000)!=0;
            if((site==35 && !z) || (site==36 && (z || n!=v)))
                r[0]=site==35?(boss?24:12):(boss?7:3);
            return;
        }
        if(site==0)selection.Track(m,now);
        if(site==34){selection.Apply(m,r,now);return;}
        const bool custom_options=SaveMenu::on_top.load() && SaveMenu::OptionsActive(m);
        if(site==30) {
            r[3]=0;
            if(custom_options) {r[0]=0;r[10]=4;r[15]=0x43b244;}
            return;
        }
        if(site==31) {
            cpsr=Compare(cpsr,r[0],2);
            if(custom_options && r[0]>=2)r[15]=0x43bbe4;
            return;
        }
        if(site==32) {r[0]=custom_options?4:m.Read(r[5]+0x30).value_or(0);return;} // Native OK tail keeps changed settings.
        if(site==33) {if(custom_options)SaveMenu::returning_from_options=true;r[0]=1;return;}
        if(site==29) {
            // Displaced TST r0,#1 preserves carry/overflow and clears N.
            const bool back=(r[0]&2) && r[4]==SaveMenu::Owner &&
                m.Read(SaveMenu::Owner+0x28)==2 && m.Read(SaveMenu::Owner+0x24)==11;
            cpsr=(cpsr&~0xc0000000u)|((back || (r[0]&1))?0:0x40000000u);
            if(back) {
                // Same native Continue tail as A on the right stone. Never
                // activate Quit merely because it was highlighted when B hit.
                r[0]=14;r[15]=0x43a674;
            }
            return;
        }
        if(site==0) {
            frame_us=now;
            if(!PauseMenu::Active(m) || now<landing_start_us ||
               m.Read(0x5066f8+0x34)!=std::optional<U32>{11})landing_clock=false;
            // Latch once: closing the menu during this frame must not change
            // the matching swap's accounting. File select has its own clocks.
            fast=r[5]==Renderer && PauseMenu::Active(m);
            boss_fast=r[5]==Renderer && BossSelector(m);
            const bool was_death=death_fast;
            death_fast=r[5]==Renderer && DeathMenu::CodeMatches(m) && DeathMenu::FastTransition(m);
            death_half=death_fast && was_death && !death_half;
            DeathMenu::half_frame=death_fast;DeathMenu::extra_frame=death_half;
            SaveMenu::animate=fast;
            if(!SaveMenu::Active(m) || (m.Read(SaveMenu::Owner+0x28)==2 &&
               m.Read(SaveMenu::Owner+0x24)==1))SaveMenu::returning_from_options=false;
            file.Frame(r[5]==Renderer && (FileSelect::Active(m) || FileSelect::NameActive(m)) &&
                FileSelectTiming::CodeMatches(m));
            if(!fast) {preview_fraction=0;preview_owner=0;remainder={};}
            if(m.Read(0x5066f8+0x34)!=std::optional<U32>{9})remainder={};
            r[0]+=(fast || death_fast || file.active || boss_fast)?1:2;
        } else if(site==28) {
            r[0]=fast && SaveMenu::Active(m) && m.Read(SaveMenu::Owner+0x2c).value_or(8)<8?0:1;
        } else if(site==1) {
            r[1]+=((fast || death_fast || file.active || boss_fast) && r[4]==Renderer)?1:2;
        } else if(site==2) {
            if(preview_owner!=r[4]) {preview_owner=r[4];preview_fraction=0;}
            const float angle=float(std::bit_cast<std::int32_t>(f[0]));
            f[0]=std::bit_cast<U32>(angle+(fast?preview_fraction:0.f));
        } else if(site==3) {
            // Carry fractional degrees into the next native matrix. Changing
            // speed changes the increment, never the current orientation.
            if(fast) {
                preview_fraction+=.5f*PauseMenu::PreviewRotationSpeed(PauseMenu::GetLayout().preview_rotation_speed);
                const auto whole=unsigned(preview_fraction);
                r[0]+=whole;preview_fraction-=float(whole);
            } else {++r[0];preview_fraction=0;}
        } else if(site>=4 && site<=7) {
            const auto delta=std::bit_cast<std::int32_t>(r[1]);
            r[1]+=r[1]>>31; // Original signed divide rounding correction.
            if(fast) {
                auto& residual=remainder[site-4];
                const float step=float(delta)*.2928932188134524f+residual;
                const auto whole=static_cast<std::int32_t>(step);
                residual=step-float(whole);
                r[0]+=whole;
                r[15]=Sites[site].pc+8; // Replaces the following native half-step too.
            }
        } else if(site>=8 && site<=13) {
            const unsigned reg=site<=9?0:site<=11?2:1;
            constexpr std::array<U32,6> normal{4,6,12,3,12,3};
            constexpr std::array<U32,6> twice{10,12,24,6,24,6};
            // Flight exits after old count > 4 (six updates): > 10 gives twelve.
            cpsr=Compare(cpsr,r[reg],(fast || (site>=10 && file.active))?twice[site-8]:normal[site-8]);
            if(site==9 && fast && landing_clock) {
                // Use emulated time, not the number of calls to the menu
                // dispatcher. Duplicate updates cannot shorten the pulse.
                cpsr=Compare(cpsr,frame_us-landing_start_us>=200000?12:0,12);
            }
        } else if(site>=14 && site<=27) {
            // VCVT.F32.S32: use half-frame units for the native glow opacity,
            // dimensions and offsets, including both swapped-item effects.
            const U32 word=Sites[site].instruction;
            const unsigned dst=((word>>12)&15)*2+((word>>22)&1);
            const unsigned src=(word&15)*2+((word>>5)&1);
            const float value=float(std::bit_cast<std::int32_t>(f[src]));
            float sample=value*(fast?.5f:1.f);
            if(fast && landing_clock) {
                const float factor=(site==16 || site==17 || site==20 || site==21)?6.f:
                    site>=18?3.f:1.f;
                sample=(site>=24?6.f:LandingFrame())*factor;
            }
            f[dst]=std::bit_cast<U32>(sample);
        }
    }
};
}
