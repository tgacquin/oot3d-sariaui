// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"
#include "oot3d/magic_lens_pulse.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace OOT3D::MagicMeter {
// The HUD shader supplies an opaque black texel for the meter background.
inline constexpr float BackgroundMarker = .125f;
inline bool HasLayers(const HUD::Layout& l) {
    return (l.magic_contour.enabled && l.magic_contour.opacity>0) ||
           (l.magic_shadow.enabled && l.magic_shadow.strength>0);
}
// These unlit 2D atlas batches use neither view vectors nor projective UVs.
// Initialize the shader markers before drawing the meter or copying heart contours.
template<class V> void PrepareHudColors(std::span<V> vertices,unsigned fill_quad,const HUD::Layout& layout) {
    const bool custom_fill=layout.magic_fill_color!=HUD::NativeMagicFill;
    const bool hearts=layout.CustomHeartColor();
    for(auto& vertex:vertices)vertex.tex_coord0_w=0;
    // Also remove the atlas rim when native geometry is hidden or changing
    // shape and cannot be rebuilt by Append.
    const float opacity=std::isfinite(layout.magic_background_opacity)?
        std::clamp(layout.magic_background_opacity,0.f,1.f):1.f;
    for(auto& vertex:vertices.subspan((fill_quad-3)*6,18)) {
        vertex.tex_coord0_w=BackgroundMarker;
        vertex.view[0]=0;
        vertex.view[1]=opacity;
    }
    if(hearts) {
        // The 20 heart quads immediately follow the fill in the native 92-quad top HUD batch.
        for(auto& vertex:vertices.subspan((fill_quad+1)*6,20*6)) {
            vertex.tex_coord0_w=.5f;
            vertex.tex_coord2[0]=layout.heart_brightness;
            for(unsigned c=0;c<3;++c)vertex.view[c]=layout.heart_color[c];
        }
    }
    if(!custom_fill)return;
    for(auto& vertex:vertices.subspan(fill_quad*6,6)) {
        vertex.tex_coord0_w=-2;
        for(unsigned c=0;c<3;++c)vertex.view[c]=layout.magic_fill_color[c];
    }
}
// The renderer supplies only the verified native HUD's four magic quads.
// Interpolate the fill edge on the extra presentation frame, never game state.
template<class Vertex> struct History {
    std::array<Vertex,24> previous{};
    std::array<std::array<float,4>,3> start_edge{};
    bool moving=false;
    std::uint64_t frame=0;
    std::int64_t time=0;
    unsigned epoch=0;
    bool seen=false;
    bool Observe(std::span<const Vertex> meter,std::uint64_t next,
            std::int64_t now,unsigned generation) {
        moving=false;
        if(meter.size()!=24){seen=false;return false;}
        std::array<Vertex,24> current;
        std::copy(meter.begin(),meter.end(),current.begin());
        bool valid=seen && next==frame+1 && now-time>=20000 && now-time<=50000 && generation==epoch;
        for(unsigned i=0;valid && i<24;++i) {
            // The frame and left edge must stay still: do not smear HUD moves,
            // upgrades, hidden geometry, or an editor layout change.
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
        // A native file-entry fade can change opacity on every tick while the
        // fill is growing. Sample only the edge geometry; this draw retains
        // its own atlas coordinates, visibility and color.
        valid = valid && current[18].color[3]>0 && current[18].position!=current[19].position &&
                 previous[18].position!=previous[19].position;
        if(valid)for(unsigned k=0;k<3;++k)for(unsigned c=0;c<4;++c) {
            const unsigned i=RightEdge[k];
            start_edge[k][c]=previous[i].position[c];
            moving |= start_edge[k][c]!=current[i].position[c];
        }
        previous=current;frame=next;time=now;epoch=generation;seen=true;
        return moving;
    }

    static constexpr std::array<unsigned,3> RightEdge{20,21,23};
    void Sample(std::span<Vertex,24> meter,float phase) const {
        if(!moving)return;
        for(unsigned k=0;k<RightEdge.size();++k)for(unsigned c=0;c<4;++c) {
            auto& edge=meter[RightEdge[k]].position[c];
            edge=std::lerp(start_edge[k][c],edge,phase);
        }
    }
};
inline bool Valid(const HUD::Layout& l) {
    return std::isfinite(l.magic_width) && l.magic_width>=.5f && l.magic_width<=4 &&
        std::isfinite(l.magic_height) && l.magic_height>=1 && l.magic_height<=8 &&
        std::isfinite(l.magic_border) && l.magic_border>=0 && l.magic_border<=4 &&
        std::isfinite(l.magic_padding_ratio) && l.magic_padding_ratio>0 &&
        std::isfinite(l.magic_background_opacity) && l.magic_background_opacity>=0 && l.magic_background_opacity<=1;
}
// Input is exactly the four verified magic quads, after the native vertex
// shader: TL,BL,TR, TR,BL,BR. Work in their own screen axes so rotation,
// user HUD scale/position and the native projection all remain intact.
// interpolate blends all shader varyings; background opacity multiplies native alpha.
template<class Vertex,class Interpolate>
bool Append(std::span<const Vertex> input,const HUD::Layout& l,
            std::vector<Vertex>& output,Interpolate interpolate,float frame_brightness=1.f) {
    const auto& contour=l.magic_contour;
    if(input.size()!=24 || !Valid(l))return false;
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
    const float padding=2*l.magic_border*l.magic_padding_ratio;
    // Keep the authored frame thickness at every HUD scale. At the smallest
    // sizes reserve one local unit for the fill so it still shows the amount.
    const float bar_width=std::max(width*l.magic_width,2*padding+1);
    const float bar_height=std::max(4*(l.magic_height+l.magic_border),2*padding+1);
    const float fill_height=bar_height-2*padding;
    // Work in border widths, so a one-unit inset is the inner edge on all
    // sides. Legacy contours can still have different horizontal/vertical widths.
    float unit_x=0,unit_y=0,half_x=0,half_y=0,inner_radius=0;
    if(HasLayers(l) && origin[3]>0 &&
       std::any_of(input.begin(),input.begin()+18,[](const Vertex& v){return v.color[3]>0;})) {
        const float pixels_x=std::hypot(dx[0]*120.f,dx[1]*200.f)/origin[3];
        const float pixels_y=std::hypot(dy[0]*120.f,dy[1]*200.f)/origin[3];
        if(pixels_x>0 && pixels_y>0) {
            const bool bordered=contour.enabled && contour.opacity>0 && contour.width>0 && contour.height>0;
            const float thickness_x=bordered?contour.width:1.f,thickness_y=bordered?contour.height:1.f;
            unit_x=thickness_x/pixels_x;
            unit_y=thickness_y/pixels_y;
            half_x=bar_width/(2*unit_x);
            half_y=bar_height/(2*unit_y);
            const float rim_width=bordered?1.f:0.f;
            const float radius=bordered?std::clamp(l.magic_border_roundness,0.f,1.f)*(1+std::min(half_x,half_y)):0;
            inner_radius=std::max(radius-1,0.f);
            const auto top=interpolate(input[6],input[8],.5f);
            const auto bottom=interpolate(input[7],input[11],.5f);
            const auto rim=interpolate(top,bottom,1.f/16);
            const auto add_layer=[&](bool shadow) {
                const auto& settings=l.magic_shadow;
                const float feather=shadow?settings.blur/std::sqrt(thickness_x*thickness_y):0.f;
                const float offset_x=(bordered?contour.x:0)+(shadow?settings.x:0);
                const float offset_y=(bordered?contour.y:0)+(shadow?settings.y:0);
                constexpr std::array<unsigned,4> source{0,1,14,17}; // TL, BL, TR, BR
                std::array<Vertex,4> quad;
                for(unsigned i=0;i<4;++i) {
                    auto& v=quad[i];
                    v=rim;
                    const float px=(i>=2?1:-1)*(half_x+rim_width+feather);
                    const float py=(i%2?1:-1)*(half_y+rim_width+feather);
                    const float x=(half_x+px)*unit_x+offset_x/pixels_x;
                    const float y=(half_y+py)*unit_y+offset_y/pixels_y;
                    for(unsigned c=0;c<4;++c)v.position[c]=origin[c]+dx[c]*x+dy[c]*y;
                    for(unsigned c=0;c<3;++c)v.color[c]=shadow?0.f:contour.color[c]*frame_brightness;
                    v.color[3]=input[source[i]].color[3]*(shadow?settings.strength:contour.opacity);
                    v.tex_coord0_w=shadow?-5.f:-4.f;
                    v.tex_coord1={px,py};
                    v.tex_coord2={half_x+(shadow?rim_width:0),half_y+(shadow?rim_width:0)};
                    v.view[0]=radius;
                    v.view[1]=shadow?feather:contour.softness;
                }
                for(unsigned i:{0u,1u,2u,2u,1u,3u})output.push_back(quad[i]);
            };
            if(l.magic_shadow.enabled && l.magic_shadow.strength>0)add_layer(true);
            if(bordered)add_layer(false);
        }
    }
    const auto frame_begin=output.size();
    const auto emit=[&](std::array<unsigned,4> source,float left,float top,float right,float bottom,bool background) {
        std::array<Vertex,4> corners;
        for(unsigned i=0;i<4;++i) {
            auto& v=corners[i];
            v=input[source[i]];
            const float x=i>=2?right:left;
            const float y=i%2?bottom:top;
            for(unsigned c=0;c<4;++c)v.position[c]=origin[c]+dx[c]*x+dy[c]*y;
            if(background) {
                v.tex_coord0_w=BackgroundMarker;
                v.view[0]=inner_radius;
                v.view[1]=l.magic_background_opacity;
            }
        }
        for(unsigned i:{0u,1u,2u,2u,1u,3u})output.push_back(corners[i]);
    };
    if(l.magic_background_opacity>0)emit({0,1,14,17},0,0,bar_width,bar_height,true);
    if(fill_visible) {
        const float fraction=std::clamp(fill_width/(width-4),0.f,1.f);
        emit({18,19,20,23},padding,padding,padding+(bar_width-2*padding)*fraction,padding+fill_height,false);
    }
    if(inner_radius>0)for(auto& v:std::span{output}.subspan(frame_begin)) {
        v.tex_coord1={coordinate(v,dx,xx)/unit_x-half_x,coordinate(v,dy,yy)/unit_y-half_y};
        v.tex_coord2={half_x,half_y};
        if(v.tex_coord0_w!=BackgroundMarker)v.tex_coord0_w=v.tex_coord0_w== -2.f ? -(6+inner_radius) : 1+inner_radius;
    }
    return true;
}
}
