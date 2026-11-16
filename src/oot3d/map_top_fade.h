// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_shadow.h"

namespace OOT3D::MapTopFade {
// Coordinates and settings are in top-screen pixels, shared by both maps.
inline float Opacity(float y,float top,const PauseMenu::MapTopFade& fade) {
    if(!fade.enabled || !std::isfinite(y+top+fade.opacity+fade.height+fade.y+fade.softness) ||
       fade.height<=0)return 0;
    const float t=std::clamp((y-top-fade.y)/fade.height,0.f,1.f);
    return std::clamp(fade.opacity,0.f,1.f)*
        std::pow(1-t*t*(3-2*t),std::clamp(fade.softness,.2f,4.f));
}
constexpr unsigned Steps=32;
struct Geometry {
    std::array<float,(Steps+1)*12> positions{};
    std::array<float,(Steps+1)*8> uvs{};
    std::array<float,(Steps+1)*16> colors{};
    unsigned count=0;
};
inline Geometry Make(const PauseMenu::MapTopFade& fade,PauseShadow::Rect paper,float alpha) {
    Geometry g;
    if(!fade.enabled || !std::isfinite(paper.left+paper.right+paper.top+paper.bottom+alpha+
        fade.y+fade.height+fade.opacity+fade.softness) || fade.height<=0 || alpha<=0 ||
        paper.left>=paper.right || paper.top>=paper.bottom)return g;
    for(float c:fade.color)if(!std::isfinite(c))return g;
    const float start=paper.top+fade.y;
    const auto band=[&](float top,float bottom) {
        top=std::max(top,paper.top);bottom=std::min(bottom,paper.bottom);
        if(bottom<=top)return;
        const float a=Opacity(top,paper.top,fade)*std::clamp(alpha,0.f,1.f);
        const float b=Opacity(bottom,paper.top,fade)*std::clamp(alpha,0.f,1.f);
        if(a<=0 && b<=0)return;
        const std::array<float,12> p{paper.left,top,0,paper.right,top,0,
            paper.left,bottom,0,paper.right,bottom,0};
        std::copy(p.begin(),p.end(),g.positions.begin()+g.count*12);
        std::fill_n(g.uvs.begin()+g.count*8,8,.5f);
        for(unsigned i=0;i<4;++i) {
            for(unsigned c=0;c<3;++c)g.colors[g.count*16+i*4+c]=std::clamp(fade.color[c],0.f,1.f);
            g.colors[g.count*16+i*4+3]=i<2?a:b;
        }
        ++g.count;
    };
    band(paper.top,start);
    for(unsigned i=0;i<Steps;++i)
        band(start+fade.height*i/Steps,start+fade.height*(i+1)/Steps);
    return g;
}
inline constexpr auto Pixels=[] {std::array<TitleScreen::U32,64> p{};p.fill(0xffffffff);return p;}();
using Hook=GuestUI::Hook<Geometry,8,8>;
}
