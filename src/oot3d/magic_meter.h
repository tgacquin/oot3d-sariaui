// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace OOT3D::MagicMeter {
// The renderer supplies only the verified native HUD's four magic quads.
// Interpolate the fill edge on the extra presentation frame, never game state.
template<class Vertex> struct History {
    std::array<Vertex,24> previous{};
    std::uint64_t frame=0;
    std::int64_t time=0;
    unsigned epoch=0;
    bool seen=false;
    template<class Same> bool Apply(std::span<Vertex> meter,std::uint64_t next,
            std::int64_t now,unsigned generation,Same same) {
        if(meter.size()!=24){seen=false;return false;}
        std::array<Vertex,24> current;
        std::copy(meter.begin(),meter.end(),current.begin());
        bool valid=seen && next==frame+1 && now-time>=20000 && now-time<=50000 && generation==epoch;
        for(unsigned i=0;valid && i<24;++i) {
            valid=same(previous[i],current[i]);
            // The frame and left edge must stay still: do not smear HUD moves,
            // fades, upgrades, hidden geometry, or an editor layout change.
            if(i<20 || i==22)valid &= previous[i].position==current[i].position;
            for(unsigned c=0;c<4;++c)
                valid &= std::isfinite(previous[i].position[c]) && std::isfinite(current[i].position[c]);
        }
        float length=0;
        std::array<float,4> axis{};
        for(unsigned c=0;c<4;++c) {
            axis[c]=current[17].position[c]-current[1].position[c];
            length+=axis[c]*axis[c];
        }
        valid &= length>1e-12f;
        // Only changes along the meter's width are fill changes. A collapsed
        // or parked fill and a camera/projection change must not be revived.
        for(unsigned i:{20u,21u,23u})if(valid) {
            float dot=0;
            for(unsigned c=0;c<4;++c)dot+=(current[i].position[c]-previous[i].position[c])*axis[c];
            for(unsigned c=0;c<4;++c)
                valid &= std::abs(current[i].position[c]-previous[i].position[c]-axis[c]*dot/length)<.00001f;
        }
        bool changed=false;
        if(valid)for(unsigned i:{20u,21u,23u})for(unsigned c=0;c<4;++c) {
            changed |= previous[i].position[c]!=current[i].position[c];
            meter[i].position[c]=(previous[i].position[c]+current[i].position[c])*.5f;
        }
        previous=current;frame=next;time=now;epoch=generation;seen=true;
        return changed;
    }
};
inline bool Valid(const HUD::Layout& l) {
    return std::isfinite(l.magic_width) && l.magic_width>=.5f && l.magic_width<=4 &&
        std::isfinite(l.magic_height) && l.magic_height>=1 && l.magic_height<=8 &&
        std::isfinite(l.magic_border) && l.magic_border>=1 && l.magic_border<=4 &&
        std::isfinite(l.magic_background_opacity) && l.magic_background_opacity>=0 && l.magic_background_opacity<=1;
}
inline bool Changed(const HUD::Layout& l) {
    return Valid(l) && (l.magic_width!=1 || l.magic_height!=1 || l.magic_border!=1 || l.magic_background_opacity!=1);
}
struct Patch {
    unsigned quad;
    float x0,y0,x1,y1,v0,v1;
};
// Native meter: three 8px-high frame pieces, with 2px margins around
// the 4px-high fill. Single/double magic have outer widths 82/164.
inline std::array<Patch,10> Patches(float width,float fraction,const HUD::Layout& l) {
    const float b=l.magic_border,h=l.magic_height;
    width*=l.magic_width;
    const float cap=std::min(8*b,width*.5f);
    const std::array<float,4> x{0,cap,width-cap,width};
    const std::array<float,4> y{0,2*b,2*b+4*h,4*b+4*h};
    constexpr std::array<float,4> v{0,.25f,.75f,1};
    std::array<Patch,10> out{};
    for(unsigned q=0;q<3;++q)for(unsigned row=0;row<3;++row)
        out[q*3+row]={q,x[q],y[row],x[q+1],y[row+1],v[row],v[row+1]};
    out[9]={3,2*b,2*b,2*b+(width-4*b)*std::clamp(fraction,0.f,1.f),
        2*b+4*h,0,1};
    return out;
}
// Input is exactly the four verified magic quads, after the native vertex
// shader: TL,BL,TR, TR,BL,BR. Work in their own screen axes so rotation,
// user HUD scale/position and the native projection all remain intact.
// interpolate blends all shader varyings; background opacity multiplies native alpha.
template<class Vertex,class Interpolate>
bool Append(std::span<const Vertex> input,const HUD::Layout& l,
            std::vector<Vertex>& output,Interpolate interpolate) {
    if(input.size()!=24 || !Changed(l))return false;
    std::array<float,4> origin{},dx{},dy{};
    for(unsigned c=0;c<4;++c) {
        origin[c]=input[0].position[c];
        dx[c]=(input[2].position[c]-origin[c])/8;
        dy[c]=(input[1].position[c]-origin[c])/8;
        if(!std::isfinite(origin[c]) || !std::isfinite(dx[c]) || !std::isfinite(dy[c]))return false;
    }
    float xx=0,yy=0,xy=0;
    for(unsigned c=0;c<4;++c) {xx+=dx[c]*dx[c];yy+=dy[c]*dy[c];xy+=dx[c]*dy[c];}
    if(xx<1e-12f || yy<1e-12f || std::abs(xy)>.001f*std::sqrt(xx*yy))return false;
    const auto coordinate=[&](const Vertex& vertex,const auto& axis,float length) {
        float value=0;
        for(unsigned c=0;c<4;++c)value+=(vertex.position[c]-origin[c])*axis[c];
        return value/length;
    };
    const float width=coordinate(input[17],dx,xx);
    if(!std::isfinite(width) || (std::abs(width-82)>.05f && std::abs(width-164)>.05f))return false;
    const auto at=[&](const Vertex& vertex,float x,float y) {
        return std::abs(coordinate(vertex,dx,xx)-x)<.05f &&
               std::abs(coordinate(vertex,dy,yy)-y)<.05f;
    };
    const std::array<float,4> boundaries{0,8,width-8,width};
    for(unsigned q=0;q<3;++q) {
        const auto* v=input.data()+q*6;
        if(!at(v[0],boundaries[q],0) || !at(v[2],boundaries[q+1],0) ||
           !at(v[1],boundaries[q],8) || !at(v[5],boundaries[q+1],8))return false;
    }
    const float fill_width=coordinate(input[20],dx,xx)-coordinate(input[18],dx,xx);
    if(!std::isfinite(fill_width) || fill_width<-.05f || fill_width>width-4+.05f)return false;
    // Empty/hidden native fills can collapse to a point; never revive them.
    const bool fill_visible=fill_width>0 && at(input[18],2,2) && at(input[19],2,6);
    if(fill_width>0 && !fill_visible)return false;
    const auto patches=Patches(width,fill_visible?fill_width/(width-4):0,l);
    for(const auto& p:patches) {
        if(p.quad==3 && !fill_visible)continue;
        const auto* source=input.data()+p.quad*6;
        const auto emit=[&](float u0,float u1,float v0,float v1,bool background) {
            if(background && l.magic_background_opacity==0)return;
            std::array<Vertex,4> corners;
            for(unsigned i=0;i<4;++i) {
                const float u=i%2?u1:u0,v=i/2?v1:v0;
                corners[i]=interpolate(interpolate(source[0],source[2],u),
                                       interpolate(source[1],source[5],u),v);
                const float x=p.x0+(p.x1-p.x0)*u;
                const float y=p.y0+(p.y1-p.y0)*(v-p.v0)/(p.v1-p.v0);
                for(unsigned c=0;c<4;++c)corners[i].position[c]=origin[c]+dx[c]*x+dy[c]*y;
                if(background)corners[i].color[3]*=l.magic_background_opacity;
            }
            for(unsigned index:{0u,2u,1u,1u,2u,3u})output.push_back(corners[index]);
        };
        if(p.quad==3 || l.magic_background_opacity==1) {
            emit(0,1,p.v0,p.v1,false);
            continue;
        }
        // The native 8px frame has a one-texel white rim. Its black interior
        // includes the extra pixel between that rim and the 4px green fill.
        // Slice both cap edges and the top/bottom strips so alpha changes never
        // dim the rim or the separately drawn green fill. Keep native UVs for
        // custom texture replacements, as with height and border controls.
        const std::array<float,3> us{0,p.quad==0?.125f:.875f,1};
        const bool cap=p.quad!=1,split_y=p.v0==0 || p.v1==1;
        const std::array<float,3> vs{p.v0,p.v0==0?.125f:.875f,p.v1};
        for(unsigned x=0;x<(cap?2u:1u);++x)for(unsigned y=0;y<(split_y?2u:1u);++y) {
            const float u0=cap?us[x]:0,u1=cap?us[x+1]:1;
            const float v0=split_y?vs[y]:p.v0,v1=split_y?vs[y+1]:p.v1;
            const bool inside_x=p.quad==1 || (p.quad==0?x==1:x==0);
            emit(u0,u1,v0,v1,inside_x && v0>=.125f && v1<=.875f);
        }
    }
    return true;
}
}
