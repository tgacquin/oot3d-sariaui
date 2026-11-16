// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_fade.h"
#include "oot3d/ocarina_input.h"
#include "oot3d/save_menu.h"

namespace OOT3D::HUDReturn {
using TitleScreen::U32;
// One return envelope for native A and every relocated HUD draw. A itself
// can jump straight to alpha=1 after a menu, so its raw alpha is not a clock.
struct State {
    U32 context=0;
    std::int64_t last_us=0;
    std::optional<std::int64_t> started;
    float opacity=0;
    bool seen=false,returning=false,hidden=false;
    template<class M> void Track(M& m,bool paused,std::int64_t now) {
        const U32 play=m.Read(HUDFade::UI+0xc).value_or(0);
        if(!Ocarina::CodeMatches(m) || !TitleScreen::Pointer(play,0x7f28) ||
           (m.Read(play+0x100).value_or(0)&0xffff)!=0x0203 ||
           !HUDFade::Zero(m,play+0x5c2c,0xff00) ||
           !HUDFade::Zero(m,play+0x7f10,0xff0000)) {
            *this={};return;
        }
        if(play!=context || now<last_us)*this={};
        context=play;last_us=now;
        hidden=paused || SaveMenu::Active(m) || Ocarina::State(m)!=0;
        if(hidden) {seen=true;returning=false;started.reset();opacity=0;return;}
        const U32 main=m.Read(HUDFade::Main+0x34).value_or(0);
        if(returning && main==2 && opacity==1)seen=false;
        returning=seen;
    }
    template<class M> void Action(M& m,U32 model,U32 alpha,std::int64_t now,
                                   const HUDFade::State& fade) {
        if((!hidden && !returning) ||
           m.Read(HUDFade::Action+0x20)!=std::optional<U32>{model} ||
           !HUDFade::CodeMatches(m))return;
        const auto word=m.Read(alpha);if(!word)return;
        const float native=std::bit_cast<float>(*word);
        if(!std::isfinite(native) || native<0 || native>1)return;
        // Native A may still draw during instrument/menu cleanup, before
        // the shared return starts. Match the relocated HUD's hidden phase
        // too, otherwise A stays visible and never appears to fade back in.
        if(hidden || !HUDFade::GameplayReady(m,context) || fade.action_opacity<=0) {
            m.Write(alpha,std::bit_cast<U32>(0.f));return;
        }
        if(!started)started=now;
        opacity=std::clamp(float(now-*started)/HUDFade::FadeUs,0.f,1.f);
        // This is the fresh shader-uniform alpha, never the native material.
        m.Write(alpha,std::bit_cast<U32>(native*opacity));
    }
};
// 2ef684/2ef9b4 animate helper+1c offsets independently of the source
// rectangles. Remove that translation only from the fresh upload, retaining
// native heart pulses, magic fill, hidden/unowned geometry and source memory.
template<class M> void Stationary(M& m,U32 helper,unsigned first,unsigned count) {
    const auto total=m.Read(helper);
    const U32 positions=m.Read(helper+0x10).value_or(0),offsets=m.Read(helper+0x1c).value_or(0);
    if(!total || *total!=92 || first+count>*total ||
       !TitleScreen::Pointer(positions,*total*48) || !TitleScreen::Pointer(offsets,*total*8))return;
    for(unsigned q=first;q<first+count;++q) {
        std::array<float,2> offset;
        std::array<float,12> vertices;
        if(!TitleScreen::ReadFloats(m,offsets+q*8,offset) ||
           !TitleScreen::ReadFloats(m,positions+q*48,vertices))continue;
        if(std::abs(offset[0])>400 || std::abs(offset[1])>400)continue;
        for(unsigned v=0;v<4;++v)for(unsigned axis=0;axis<2;++axis)
            m.Write(positions+q*48+(v*3+axis)*4,
                std::bit_cast<U32>(vertices[v*3+axis]-offset[axis]));
    }
}
template<class M> void RupeeDigits(M& m,U32 counter,U32 helper) {
    const auto digits=m.Read(counter+4),total=m.Read(helper);
    const U32 source=m.Read(helper+0xc).value_or(0),positions=m.Read(helper+0x10).value_or(0);
    if(!digits || !total || *digits==0 || *digits>4 || *total!=*digits ||
       !TitleScreen::Pointer(source,*total*48) || !TitleScreen::Pointer(positions,*total*48))return;
    // 2ef9b4 parks the rupee number at (20,400); 2efd88 restores (20,110).
    // Match those source rectangles so leading-digit hiding offsets survive.
    for(unsigned q=0;q<*total;++q) {
        std::array<float,12> original,upload;
        if(!TitleScreen::ReadFloats(m,source+q*48,original) ||
           !TitleScreen::ReadFloats(m,positions+q*48,upload))continue;
        if(original[0]!=20+10*(*digits-q-1) || original[1]!=400 ||
           original[3]-original[0]!=11 || original[4]!=400)continue;
        for(unsigned v=0;v<4;++v)
            m.Write(positions+q*48+(v*3+1)*4,std::bit_cast<U32>(upload[v*3+1]-290));
    }
}
}
