// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_shadow.h"

namespace OOT3D::DungeonFloorLine {
using TitleScreen::U32;
constexpr unsigned Steps=16,Quads=Steps*2+1;
struct Geometry {
    std::array<float,Quads*12> positions{};
    std::array<float,Quads*8> uvs{};
    std::array<float,Quads*16> colors{};
    unsigned count=0;
};
inline Geometry Make(const PauseMenu::Layout& layout,float alpha,unsigned floor_rows) {
    Geometry g;
    const auto& line=layout.dungeon_floor_line;
    const auto& floors=layout[PauseMenu::DungeonFloors];
    if(!line.enabled || !PauseMenu::Valid(line.pose) || !PauseMenu::Valid(floors) ||
       !std::isfinite(line.width+line.height+line.opacity+line.roundness+alpha))return g;
    const float scale=line.pose.scale*floors.scale;
    const float w=std::clamp(line.width,0.f,80.f)*scale;
    const unsigned count=std::popcount(floor_rows&31u);
    if(!count)return g;
    const float spacing=layout.dungeon_spacing[1];
    const float factor=std::isfinite(spacing)?std::clamp(spacing,.2f,3.f):1;
    // Match the compacted native row centers. Include the 44px floor well
    // and 8px padding at each end, independent of selection/growth/fades.
    // Retain the authored height as a maximum for existing shorter lines.
    const float fitted_height=(46.f*(count-1)*factor+44+16)*PauseMenu::Scale*floors.scale;
    const float h=std::min(std::clamp(line.height,0.f,600.f)*scale,fitted_height);
    const float opacity=std::clamp(line.opacity,0.f,1.f)*std::clamp(alpha,0.f,1.f);
    if(w<=0 || h<=0 || opacity<=0)return g;
    const float x=200+(PauseMenu::Anchors[PauseMenu::DungeonFloors][0]-160)*PauseMenu::Scale+floors.x+line.pose.x;
    const float y=120+PauseMenu::OffsetY+floors.y+line.pose.y;
    const float radius=std::min(w,h)*.5f*std::clamp(line.roundness,0.f,1.f);
    const auto band=[&](float top,float bottom,float inset_top,float inset_bottom) {
        if(bottom<=top)return;
        const std::array<float,12> p{x-w/2+inset_top,top,0,x+w/2-inset_top,top,0,
            x-w/2+inset_bottom,bottom,0,x+w/2-inset_bottom,bottom,0};
        std::copy(p.begin(),p.end(),g.positions.begin()+g.count*12);
        std::fill_n(g.uvs.begin()+g.count*8,8,.5f);
        for(unsigned i=0;i<16;++i)g.colors[g.count*16+i]=i%4==3?opacity:1;
        ++g.count;
    };
    // Non-overlapping trapezoids keep opacity uniform, including at cap joins.
    for(unsigned i=0;i<Steps && radius>0;++i) {
        const float a=float(i)/Steps,b=float(i+1)/Steps;
        const auto inset=[&](float t){return radius*(1-std::sqrt(std::max(0.f,1-(1-t)*(1-t))));};
        band(y-h/2+radius*a,y-h/2+radius*b,inset(a),inset(b));
        band(y+h/2-radius*b,y+h/2-radius*a,inset(b),inset(a));
    }
    band(y-h/2+radius,y+h/2-radius,0,0);
    return g;
}
inline constexpr auto Pixels=[] {std::array<U32,64> pixels{};pixels.fill(0x000000ff);return pixels;}();
using Hook=GuestUI::Hook<Geometry,8,8>;
}
