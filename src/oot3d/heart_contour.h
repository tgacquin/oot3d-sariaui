// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"
#include <cmath>
#include <vector>

namespace OOT3D::HUD {
// Extend the original quad and retain its atlas bounds. All layers are
// composited in one sample before the native HUD fade is applied.
template<class V> bool PrepareContourQuad(V* v,float left,float right,float top,float bottom,
                                          float marker,float digit_radius=0) {
    if(!(v[0].position[3]>0))return false;
    const float w=v[0].position[3];
    const auto x=[&](unsigned i){return -v[i].position[1]*200/w;};
    const auto y=[&](unsigned i){return -v[i].position[0]*120/w;};
    const float x0=x(0),y0=y(0),x1=x(5),y1=y(5);
    if(x0==x1 || y0==y1)return false;
    const unsigned horizontal=std::abs(x(1)-x0)>std::abs(x(2)-x0)?1:2;
    const unsigned vertical=3-horizontal;
    const auto uv0=v[0].tex_coord0,uv1=v[5].tex_coord0;
    if(uv0[0]==uv1[0] || uv0[1]==uv1[1])return false;
    const bool swapped=v[horizontal].tex_coord0[0]==uv0[0];
    const float du=(uv1[0]-uv0[0])/(swapped?y(vertical)-y0:x(horizontal)-x0);
    const float dv=(uv1[1]-uv0[1])/(swapped?x(horizontal)-x0:y(vertical)-y0);
    if(!std::isfinite(du) || !std::isfinite(dv))return false;
    if(digit_radius>0) {
        const float u=digit_radius/(256*std::abs(du)),v_radius=digit_radius/(128*std::abs(dv));
        right=swapped?v_radius:u; bottom=swapped?u:v_radius;
        left=-right; top=-bottom;
    }
    for(unsigned i=0;i<6;++i) {
        const float dx=x(i)==std::min(x0,x1)?left:right;
        const float dy=y(i)==std::min(y0,y1)?top:bottom;
        v[i].position[1]-=dx*w/200;
        v[i].position[0]-=dy*w/120;
        v[i].tex_coord0[0]+=du*(swapped?dy:dx);
        v[i].tex_coord0[1]+=dv*(swapped?dx:dy);
        v[i].tex_coord1={std::min(uv0[0],uv1[0]),std::min(uv0[1],uv1[1])};
        v[i].tex_coord2={std::max(uv0[0],uv1[0]),std::max(uv0[1],uv1[1])};
        v[i].view={du,dv,swapped?1.f:0.f};
        v[i].tex_coord0_w=marker;
    }
    return true;
}

template<class V> bool PrepareRupeeContour(V* quad,const SpriteContour& s) {
    if(!s.enabled || s.opacity<=0)return false;
    return PrepareContourQuad(quad,std::min(0.f,s.x-s.width),std::max(0.f,s.x+s.width),
                              std::min(0.f,s.y-s.height),std::max(0.f,s.y+s.height),-.375f);
}

template<class V> bool PrepareRupeeDigits(std::vector<V>& vertices,unsigned count,const RupeeDigitOutline& s) {
    if(!s.enabled || !count || count%6 || count>24 || count>vertices.size())return false;
    for(auto& v:vertices)v.tex_coord0_w=0;
    bool styled=false;
    for(auto i=vertices.size()-count;i<vertices.size();i+=6)
        styled|=PrepareContourQuad(vertices.data()+i,0,0,0,0,-.4375f,
                                   std::clamp(s.thickness,s.MinThickness,s.MaxThickness));
    return styled;
}

template<class V> bool PrepareHeartContours(std::vector<V>& vertices,std::size_t first,
                                            const HeartContours& contours) {
    if(!contours.Enabled() || first>vertices.size() || vertices.size()-first<120)return false;
    bool styled=false;
    for(unsigned q=0;q<20;++q) {
        auto* v=vertices.data()+first+6*q;
        const bool defense=(v[0].tex_coord0[1]+v[5].tex_coord0[1])*.5f>1.f-78.f/256.f;
        if(defense && contours.double_defense.enabled)
            for(unsigned i=0;i<6;++i)v[i].tex_coord0[1]-=80.f/256.f;
        const auto& outline=defense?contours.double_defense:contours.outline;
        float left=0,right=0,top=0,bottom=0;
        bool visible_layer=false;
        for(const auto* layer:{&contours.shadow,&outline}) {
            if(!layer->enabled || layer->opacity<=0)continue;
            visible_layer=true;
            left=std::min(left,layer->x-layer->width);
            right=std::max(right,layer->x+layer->width);
            top=std::min(top,layer->y-layer->height);
            bottom=std::max(bottom,layer->y+layer->height);
        }
        if(visible_layer)
            styled|=PrepareContourQuad(v,left,right,top,bottom,defense?-.25f:-.125f);
    }
    return styled;
}
}
