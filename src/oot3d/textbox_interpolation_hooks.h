// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/textbox_interpolation.h"
#include "oot3d/screen_model_hooks.h"
#include "oot3d/interpolation_state.h"
#include "oot3d/surface_capture.h"
#include "oot3d/pause_menu.h"

namespace OOT3D::TextboxInterpolation {
template<class M> void Capture(M& m,std::uint32_t model,std::uint32_t view) {
    const auto layout=Textboxes::GetLayout();
    if(!Interpolation::Replaying() && !Interpolation::SurfaceCapture::active.load()) {
        bool arrow=false;
        for(unsigned index:{2u,3u}) {
            const auto pane=m.Read(ScreenModels::Message+0x568+4*index).value_or(0);
            arrow |= pane && (m.Read(pane+0x64)==model || m.Read(pane+0x68)==model);
        }
        if(!arrow)return;
    }
    const auto element=ScreenModels::MessageElement(m,model,false);
    if(!element || (!Interpolation::Replaying() && !Interpolation::SurfaceCapture::active.load() &&
        !FilteredArrow(*element)))return;
    struct Trace {
        Interpolation::SurfaceCapture::TextboxSample sample;
        ~Trace(){Interpolation::SurfaceCapture::ObserveTextbox(std::move(sample));}
    } trace;
    auto& diagnostic=trace.sample;
    diagnostic.frame=Interpolation::guest_frames.load();diagnostic.generation=Interpolation::generation.load();
    diagnostic.model=model;diagnostic.pane=*element;
    diagnostic.state=m.Read(ScreenModels::Message+0xf38).value_or(0)&255;
    diagnostic.stage="replay_disabled";
    if(!Interpolation::Replaying() && !FilteredArrow(*element))return;
    const auto mesh=m.Read(model+4).value_or(0),desc=m.Read(mesh).value_or(0);
    const auto count=m.Read(desc+12),flags=m.Read(desc+0x1c),current=m.Read(mesh+0x124);
    diagnostic.mesh=mesh;diagnostic.flags=flags.value_or(0);diagnostic.vertices=count.value_or(0);
    diagnostic.stage="buffer_format";
    if(!count || !flags || !current || *current>=2 || !(*flags&0x80) || (*flags&0x18))return;
    const unsigned capacity=*count?*count:4;
    const bool strip=!(*flags&0x20);
    const unsigned vertices=MeshVertices(m.Read(model+0x174).value_or(0),strip);
    diagnostic.vertices=vertices;diagnostic.stage="vertex_count";
    // Native 487d00 -> 48ba0c sets pane255's index count from the revealed
    // character count (488428: 6*n-2). Its allocated buffers hold the whole page.
    const bool lettering=*element==255; // Message+0x964, validated pane/model ownership above.
    if(!vertices || capacity%4 || capacity>MaxVertices || vertices>capacity ||
       (!lettering && vertices!=capacity))return;
    // Native 0x313ad8 selects the current GPU upload. UI buffers pack XYZ,
    // UV and RGBA with normals disabled (0x80). Descriptor source pointers
    // may refer to expired stack storage and must not identify a later draw.
    const auto live=m.Read(mesh+0x1a0+4*(*current)).value_or(0);
    diagnostic.buffer=live;diagnostic.stage="live_buffer";
    if(!TitleScreen::Pointer(live,capacity*36))return;
    Route route;route.frame=Interpolation::guest_frames.load();route.generation=Interpolation::generation.load();
    route.vertices=vertices;route.hash=HUD::HashStart;
    route.source={model,mesh,m.Read(ScreenModels::Message+0x978).value_or(0),*element};
    // The indicator survives native page/text retirement for its short fade.
    // Its pane ownership is stable across those text-ID changes.
    if(FilteredArrow(*element))route.source[2]=m.Read(ScreenModels::Message+0x568+4*(*element)).value_or(0);
    route.strip=strip;route.message=m.Read(ScreenModels::Message+0xf40).value_or(~0u);
    if(lettering) {
        const auto engine=m.Read(ScreenModels::Message+0xeb4).value_or(0)&255;
        const auto play=m.Read(ScreenModels::Message+8).value_or(0);
        route.reveal_clock=std::bit_cast<float>(m.Read(ScreenModels::Message+0xed4).value_or(0));
        route.typewriter=layout.smooth_reveal && diagnostic.state==4 && engine>=2 && engine<=4 &&
            !(m.Read(ScreenModels::Message+0xec4).value_or(0)&0x00ff0000) &&
            !(m.Read(play+0x18).value_or(0)&3) && !PauseMenu::Active(m);
    }
    diagnostic.stage="buffer_read";
    for(unsigned slot=0;slot<3;++slot) {
        const unsigned stride=slot==0?12:slot==1?8:16;
        const auto base=live+capacity*(slot==0?0:slot==1?12:20);
        for(unsigned i=0;i<vertices*stride;i+=4) {
            const auto word=m.Read(base+i);if(!word)return;
            route.hash=HUD::HashWord(route.hash,*word);
        }
    }
    diagnostic.hash=route.hash;diagnostic.stage="matrix_read";
    std::array<float,12> v{},model_matrix{};
    if(!TitleScreen::ReadFloats(m,view,v) || !TitleScreen::ReadFloats(m,model+12,model_matrix))return;
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c) {
        float value=c==3?v[r*4+3]:0;
        for(unsigned k=0;k<3;++k)value+=v[r*4+k]*model_matrix[k*4+c];
        if(!std::isfinite(value))return;
        route.matrix[r][c]=value;
    }
    route.layout=HUD::HashWord(HUD::HashStart,layout.smooth_reveal);
    for(float f:{layout.x,layout.y,layout.width,layout.scale,layout.indicator.x,layout.indicator.y,layout.indicator.scale,layout.sign_y})
        route.layout=HUD::HashWord(route.layout,std::bit_cast<std::uint32_t>(f));
    diagnostic.matrix=route.matrix;diagnostic.stage="published";
    Publish(route);
}
}
