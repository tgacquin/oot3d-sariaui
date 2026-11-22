// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace OOT3D::AmmoBackdrop {
inline bool Enabled(const HUD::Layout& l,bool rupees=false) {
    const auto& b=rupees?l.hd_rupee_backdrop:l.hd_ammo_backdrop;
    return l.hd_numbers && (rupees?l.hd_rupee_numbers:l.hd_ammo_numbers) && b.enabled &&
        std::isfinite(b.opacity) && b.opacity>0;
}
// Shader-output geometry: native material opacity is already in vertex alpha.
// One soft oval per complete counter, preceding its unchanged digit triangles.
// Both native and custom counters take this path after their final transforms.
template<class V> bool Prepend(std::vector<V>& vertices,const HUD::AmmoBackdrop& b) {
    if(!b.enabled || !std::isfinite(b.opacity) || b.opacity<=0 ||
       vertices.empty() || vertices.size()>48 || vertices.size()%6)return false;
    float lo[2]{INFINITY,INFINITY},hi[2]{-INFINITY,-INFINITY},alpha=0;
    const V* sample=nullptr;
    for(unsigned q=0;q<vertices.size();q+=6) {
        float a[2]{INFINITY,INFINITY},z[2]{-INFINITY,-INFINITY},opacity=0;
        float uv_min[2]{INFINITY,INFINITY},uv_max[2]{-INFINITY,-INFINITY};
        for(unsigned j=0;j<6;++j) {
            const auto& v=vertices[q+j];
            if(!std::isfinite(v.position[3]) || v.position[3]<=0)return false;
            for(unsigned c=0;c<2;++c) {
                const float p=v.position[c]/v.position[3],uv=v.tex_coord0[c];
                if(!std::isfinite(p) || !std::isfinite(uv))return false;
                a[c]=std::min(a[c],p);z[c]=std::max(z[c],p);
                uv_min[c]=std::min(uv_min[c],uv);uv_max[c]=std::max(uv_max[c],uv);
            }
            opacity=std::max(opacity,v.color[3]);
        }
        // Ignore unused, collapsed, fully transparent or parked leading digits.
        if(z[0]-a[0]<.00001f || z[1]-a[1]<.00001f || opacity<=0 ||
           uv_max[0]-uv_min[0]<.00001f || uv_max[1]-uv_min[1]<.00001f ||
           z[0]<=-1 || a[0]>=1 || z[1]<=-1.5f || a[1]>=1.5f)continue;
        sample=&vertices[q];alpha=std::max(alpha,opacity);
        for(unsigned c=0;c<2;++c){lo[c]=std::min(lo[c],a[c]);hi[c]=std::max(hi[c],z[c]);}
    }
    if(!sample)return false;
    const auto finite=[](float v,float fallback,float low,float high) {
        return std::isfinite(v)?std::clamp(v,low,high):fallback;
    };
    // PICA's rotated top projection uses clip X for screen Y, clip Y for X.
    const float cx=(lo[0]+hi[0])*.5f-finite(b.y,1,-20,20)/120;
    const float cy=(lo[1]+hi[1])*.5f-finite(b.x,0,-20,20)/200;
    const float rx=(hi[0]-lo[0])*.5f*finite(b.height,1.25f,.25f,4);
    const float ry=(hi[1]-lo[1])*.5f*finite(b.width,1.25f,.25f,4);
    const float softness=finite(b.softness,.75f,0,1);
    const float opacity=std::clamp(alpha,0.f,1.f)*std::clamp(b.opacity,0.f,1.f);
    const auto& a0=sample[0].position;const auto& a1=sample[1].position;const auto& a2=sample[2].position;
    const bool reverse=(a1[0]/a1[3]-a0[0]/a0[3])*(a2[1]/a2[3]-a0[1]/a0[3])-
        (a1[1]/a1[3]-a0[1]/a0[3])*(a2[0]/a2[3]-a0[0]/a0[3])<0;
    const auto point=[&](float radius,unsigned angle) {
        V v=*sample;const float t=angle*2*std::numbers::pi_v<float>/32;
        v.position[0]=(cx+std::cos(t)*rx*radius)*v.position[3];
        v.position[1]=(cy+std::sin(t)*ry*radius)*v.position[3];
        v.tex_coord0[0]=252.f/256;v.tex_coord0[1]=1-4.f/128;
        v.color[0]=v.color[1]=v.color[2]=0;
        const float f=softness>0?std::clamp((1-radius)/softness,0.f,1.f):1;
        v.color[3]=opacity*f*f*(3-2*f);return v;
    };
    std::vector<V> out;out.reserve(vertices.size()+32*6*8);
    for(unsigned ring=0;ring<8;++ring)for(unsigned angle=0;angle<32;++angle) {
        const float inner=ring/8.f,outer=(ring+1)/8.f;
        const auto a=point(inner,angle),c=point(outer,angle),d=point(outer,angle+1),e=point(inner,angle+1);
        if(reverse)out.insert(out.end(),{a,d,c,a,e,d});
        else out.insert(out.end(),{a,c,d,a,d,e});
    }
    out.insert(out.end(),vertices.begin(),vertices.end());vertices=std::move(out);return true;
}
}
