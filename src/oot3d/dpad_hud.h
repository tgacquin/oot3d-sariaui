// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/dpad_input.h"
#include "oot3d/guest_ui_mesh.h"

namespace OOT3D::DPad {
constexpr float DisabledOpacity=.35f;
inline float NaviOpacity(std::int64_t now_us,float speed=1.f) {
    // A 1.2-second cycle with soft turns at 30% and full opacity.
    const double phase=std::fmod(double(now_us)*speed/1200000.,1.);
    return .3f+.7f*float(.5+.5*std::cos(phase*6.283185307179586));
}
inline bool Valid(const Pose& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x)<=400 &&
        std::abs(p.y)<=240 && std::isfinite(p.scale_x) && std::isfinite(p.scale_y) &&
        p.scale_x>=.1f && p.scale_x<=4 && p.scale_y>=.1f && p.scale_y<=4;
}
struct Geometry {
    std::array<float,5*12> positions{};
    std::array<float,5*8> uvs{};
    std::array<float,5*16> colors{};
    unsigned count=0;
    void Add(const Pose& group,const Pose& local,float x,float y,float size,
             float tx,float ty,float extent,float opacity) {
        if(count==5 || opacity<0 || !Valid(group) || !Valid(local))return;
        const float cx=36+group.x+(x+local.x)*group.scale_x;
        const float cy=88+group.y+(y+local.y)*group.scale_y;
        const float w=size*group.scale_x*local.scale_x/2,h=size*group.scale_y*local.scale_y/2;
        const std::array<float,12> v{cx-w,cy+h,0,cx-w,cy-h,0,cx+w,cy+h,0,cx+w,cy-h,0};
        const float u0=(tx+.5f)/TextureWidth,u1=(tx+extent-.5f)/TextureWidth;
        // Native UI UVs count down from the texture's top. The atlas builder
        // addresses PICA rows from the bottom, so convert across the FULL
        // texture height (not just the individual sprite's height).
        const float v0=1-(ty+.5f)/TextureHeight,v1=1-(ty+extent-.5f)/TextureHeight;
        const std::array<float,8> uv{u0,v0,u0,v1,u1,v0,u1,v1};
        std::copy(v.begin(),v.end(),positions.begin()+count*12);
        std::copy(uv.begin(),uv.end(),uvs.begin()+count*8);
        for(unsigned i=0;i<16;++i)colors[count*16+i]=i%4==3?opacity:1;
        ++count;
    }
};
// Display-only continuity: the player's update clears naviTextId before the
// HUD draw. Preserve an already visible call only while melee remains active.
struct NaviDisplay {
    U32 play{},player{};
    unsigned generation{};
    std::int64_t time{};
    bool visible=false;
    template<class M> bool Update(M& m,std::int64_t now,unsigned current_generation) {
        const U32 current_play=m.Read(UI+0xc).value_or(0);
        const U32 current_player=Player(m,current_play);
        if(!Gameplay(m) || !TitleScreen::Pointer(current_player,0x2228)) {
            *this={};return false;
        }
        if(play!=current_play || player!=current_player || generation!=current_generation ||
           now<time || now-time>100000)visible=false;
        play=current_play;player=current_player;generation=current_generation;time=now;
        const bool melee=(m.Read(player+0x2224).value_or(0)>>24)!=0;
        visible=NaviCalling(m) || (melee && visible);
        return visible;
    }
};
template<class M> Geometry MakeGeometry(M& m,const Layout& layout,float alpha,std::int64_t now_us=0,
                                               NaviDisplay* navi_display=nullptr,unsigned generation=0) {
    Geometry g;
    const bool navi_visible=navi_display?navi_display->Update(m,now_us,generation):NaviCalling(m);
    if(!layout.enabled || !std::isfinite(alpha) || alpha<=0)return g;
    alpha=std::clamp(alpha,0.f,1.f);
    g.Add(layout.group,{},0,0,18,0,0,104,alpha);
    constexpr std::array<std::array<float,4>,4> placement{{
        {0,-20,128,0},{20,0,192,0},{-20,0,128,64},{0,20,192,64}}};
    for(unsigned i=0;i<ElementCount;++i) {
        const auto p=placement[i];
        const bool navi=i==Look;
        const auto item=navi?Navi:static_cast<Element>(i);
        const bool collected=Collected(m,item);
        const bool available=navi?navi_visible:
            (item==Iron || item==Hover)?BootsAllowed(m,item):Owned(m,item);
        // Keep the slot in the mesh so call changes never shift other icons.
        g.Add(layout.group,layout.icons[i],p[0],p[1],20,p[2],p[3],navi?48:42,
              alpha*(!collected?0:available?(navi?NaviOpacity(now_us,layout.navi_pulse_speed):1):navi?0:DisabledOpacity));
    }
    return g;
}
using Mesh=GuestUI::Hook<Geometry,TextureWidth,TextureHeight>;
}
