// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/screen_model_hooks.h"
#include "oot3d/interpolation_state.h"

namespace OOT3D::TextboxClose {
using TitleScreen::U32;
constexpr auto Message=ScreenModels::Message;
constexpr std::int64_t Duration=166667;
constexpr U32 PresentSite=0x300554;
// MM MessageWindow/close: scale 1 at frame 0, 1.05 at frame 1, 0 at frame 5.
inline float Scale(std::int64_t elapsed) {
    const float frame=std::clamp(float(elapsed)*30/1000000,0.f,5.f);
    return frame<=1?1+.05f*frame:1.05f*(5-frame)/4;
}
struct State {
    U32 model{},mesh{},pane{},play{},message{},scene{};
    unsigned generation{};
    std::int64_t captured{},started{},draw_time{};
    std::array<float,12> view{},matrix{};
    bool ready=false,closing=false;
    bool pending=false;
    std::array<U32,16> saved{};
    std::array<U32,64> saved_fp{};
    U32 saved_flags{},saved_fpscr{};
    void Reset(){*this={};}
    template<class M> bool Owned(M& m) const {
        return m.Read(Message)==0x4ed0a0 && m.Read(Message+8)==play &&
            m.Read(HUDFade::UI+0xc)==play && m.Read(play+0x104)==scene &&
            m.Read(Message+0xf40)==message && m.Read(Message+0x56c)==pane &&
            m.Read(pane)==0x4ed234 && m.Read(pane+4)==Message+0x168 &&
            m.Read(pane+0x64)==model && m.Read(model)==0x4ebd60 && m.Read(model+4)==mesh;
    }
    template<class M> void Capture(M& m,U32 candidate,U32 source_view,std::int64_t now) {
        if(!Textboxes::GetLayout().mm_close || closing)return;
        const auto state=m.Read(Message+0xf38).value_or(0)&255;
        if(state<3 || state>=14 || !(m.Read(Message+0xc).value_or(0)&255))return;
        const U32 p=m.Read(Message+0x56c).value_or(0);
        if(!TitleScreen::Pointer(p,0xb4) || m.Read(p)!=0x4ed234 ||
            m.Read(p+4)!=Message+0x168 || m.Read(p+0x64)!=candidate)return;
        std::array<float,12> v{},mat{};
        if(!TitleScreen::ReadFloats(m,source_view,v) || !TitleScreen::ReadFloats(m,candidate+12,mat))return;
        model=candidate;mesh=m.Read(model+4).value_or(0);pane=p;
        play=m.Read(Message+8).value_or(0);scene=m.Read(play+0x104).value_or(0);message=m.Read(Message+0xf40).value_or(0);
        view=v;matrix=mat;captured=now;generation=Interpolation::generation.load();ready=true;
    }
    template<class M,class R> void Draw(M& m,R& r,std::int64_t now,bool supported) {
        r[1]=m.Read(r[0]+0xc).value_or(0)&255; // Displaced LDRB.
        if(r[0]!=Message)return;
        const auto state=m.Read(Message+0xf38).value_or(0)&255;
        if(!supported || !Textboxes::GetLayout().mm_close || !ready ||
           generation!=Interpolation::generation.load() || now<captured || !Owned(m)) {Reset();return;}
        if(state!=0 && state!=14) {closing=false;return;}
        if(!closing) {
            if(now-captured>100000){Reset();return;}
            closing=true;started=now;
        }
        std::array<float,12> current{};
        if(now-started>=Duration || !TitleScreen::ReadFloats(m,model+12,current) || current!=matrix) {Reset();return;}
        // Tail-call the still-owned plate model only. Native close and text
        // cleanup already ran; no message state, timers or visibility are revived.
        r[0]=model;r[15]=0x3f9680;
    }
    template<class M,class R,class F> void Present(M& m,R& r,F& f,U32& flags,U32& fpscr,
                                                  std::int64_t now,bool supported) {
        // This call site runs every top-screen UI pass, after the scene-local
        // message draw. That scene-local object is NOT the global dialogue
        // owner, so its entry hook cannot observe normal dialogue closing.
        if(pending) {
            std::copy(saved.begin(),saved.end(),r.begin());
            std::copy(saved_fp.begin(),saved_fp.end(),f.begin());
            flags=saved_flags;fpscr=saved_fpscr;pending=false;
        } else {
            auto probe=r;probe[0]=Message;probe[15]=PresentSite+4;
            Draw(m,probe,now,supported);
            if(probe[15]==0x3f9680) {
                std::copy_n(r.begin(),16,saved.begin());
                std::copy_n(f.begin(),64,saved_fp.begin());
                saved_flags=flags;saved_fpscr=fpscr;draw_time=now;pending=true;
                r[0]=model;r[14]=PresentSite;r[15]=0x3f9680;return;
            }
        }
        r[0]=m.Read(r[13]+0x40).value_or(0); // Displaced LDR r0,[sp,#40].
        r[15]=PresentSite+4;
    }
    template<class M,class R> bool Transform(M& m,R& r,std::int64_t now) {
        // Scheduling and drawing span guest instructions. Keep the scheduled
        // close phase even if the animation expires before the model hook;
        // otherwise that final draw falls through to a full-size native plate.
        const auto phase_time=pending?draw_time:now;
        const auto state=m.Read(Message+0xf38).value_or(0)&255;
        if(!closing || r[0]!=model || !Owned(m) || (state!=0 && state!=14) ||
           generation!=Interpolation::generation.load() || now<phase_time ||
           phase_time<started || phase_time-started>=Duration)return false;
        r[2]=model+12;
        for(unsigned i=0;i<12;++i)m.Write(r[1]+4*i,std::bit_cast<U32>(view[i]));
        const float scale=Scale(phase_time-started);
        ScreenModels::Apply(m,r[1],model,{0,0,scale,scale});
        return true;
    }
};
}
