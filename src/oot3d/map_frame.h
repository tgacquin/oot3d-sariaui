// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_cutout.h"
#include "oot3d/guest_ui_mesh.h"
#include "oot3d/pause_menu.h"
#include "oot3d/screen_model_hooks.h"

namespace OOT3D::MapFrame {
// Cutout geometry already includes its own position and scale. Only the fixed
// screen projection and shared pause slide belong in its draw transform.
inline ScreenModels::Transform DrawTransform() {
    return {.8f*PauseMenu::motion_x.load(),PauseMenu::OffsetY,
        .8f*PauseMenu::Scale,PauseMenu::Scale,160,120,true};
}
struct Geometry {
    std::array<float,12> positions{};
    std::array<float,8> uvs{};
    std::array<float,16> colors{};
    unsigned count=0;
};
inline Geometry Make(const PauseMenu::Layout& layout,int width,int height) {
    Geometry g;const auto& p=layout.map_frame;
    if(!p.enabled || width<=0 || height<=0 || width>SourceWidth || height>SourceHeight ||
       !std::isfinite(p.cut_x) || !std::isfinite(p.cut_y) || !std::isfinite(p.cut_scale) ||
       std::abs(p.cut_x)>2000 || std::abs(p.cut_y)>2000 || p.cut_scale<.05f || p.cut_scale>10)return g;
    // One quad. The selected pixel rectangle keeps its aspect ratio, with no
    // section stretching, mirrored corners, or inherited legacy edge offsets.
    const float w=width*p.cut_scale,h=height*p.cut_scale;
    const float left=160-w/2+p.cut_x/PauseMenu::Scale,top=120-h/2+p.cut_y/PauseMenu::Scale;
    g.positions={left,top,0,left+w,top,0,left,top+h,0,left+w,top+h,0};
    const float u=float(width)/Width,v=float(height)/Height;
    g.uvs={0,0,u,0,0,v,u,v};
    const float brightness=1-(std::isfinite(p.darkness)?std::clamp(p.darkness,0.f,1.f):0);
    for(unsigned i=0;i<16;++i)g.colors[i]=i%4==3?1:brightness;
    g.count=1;return g;
}
struct Hook : GuestUI::Hook<Geometry,Width,Height> {
    std::shared_ptr<const CutImage> uploaded;
    template<class M,class R> void Begin(M& m,R& r,TitleScreen::U32 return_pc) {
        const auto cut=GetCutImage();
        if(!cut || !PauseMenu::Active(m) || !PauseMenu::on_top.load() ||
           (!PauseMenu::MapPage(m) || PauseMenu::DungeonPage(m)))return;
        if(cut!=uploaded && Owned(m)) {
            // Reuse the allocation on Apply; memory writes invalidate GPU cache.
            for(unsigned i=0;i<cut->pixels->size();++i)if(!m.Read(texture+4*i))return;
            for(unsigned i=0;i<cut->pixels->size();++i)m.Write(texture+4*i,(*cut->pixels)[i]);
        }
        uploaded=cut;
        GuestUI::Hook<Geometry,Width,Height>::Begin(m,r,return_pc,
            Make(PauseMenu::GetLayout(),cut->width,cut->height),*cut->pixels,
            m.Read(0x5066f8+0x20).value_or(0));
    }
};
}
