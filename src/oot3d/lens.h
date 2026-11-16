// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/widescreen.h"

namespace OOT3D::Lens {
using Widescreen::Row;
// zelda_keep models 0x60/0x61 are complementary masks; 0x62 is the cover.
// All three contain this same eight-vertex, three-panel mesh (18 indices).
// Match geometry and screen transform, never texture addresses or pixels,
// so replacement textures keep working.
constexpr std::array<Row,8> Vertices{{
    {480,-480,0,1},{800,-480,0,1},{480,480,0,1},{800,480,0,1},
    {-480,-480,0,1},{-480,480,0,1},{-800,-480,0,1},{-800,480,0,1}}};
inline bool Candidate(unsigned count,unsigned textures) {
    // Mask and cover materials have one and two textures respectively. The
    // game overrides their depth/alpha/stencil state for the visibility passes;
    // the exact mesh below identifies them independently of those overrides.
    return count==18 && (textures==1 || textures==3);
}
inline bool Near(float a,float b) {return std::isfinite(a) && std::abs(a-b)<.00001f;}
inline bool Transform(const std::array<Row,4>& projection,const std::array<Row,3>& model) {
    constexpr std::array<Row,3> screen{{{0,-2.f/240,0,1},{-2.f/400,0,0,1},{0,0,0,1}}};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
        if(!Near(projection[r==2?3:r][c],screen[r][c]))return false;
    for(float v:projection[2])if(!std::isfinite(v))return false;
    const float s=model[2][2];
    if(!std::isfinite(s) || s<.2499f || s>1.5f)return false;
    // Native init 463294 sets (200,120,2), Z rotation 180 and scale .25.
    // Accept the render pass's view-space Z while preserving its animated size.
    const std::array<Row,3> expected{{{-s,0,0,200},{0,-s,0,120},{0,0,s,model[2][3]}}};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
        if(!Near(model[r][c],expected[r][c]))return false;
    return true;
}
inline bool Geometry(const std::array<Row,8>& vertices) {
    unsigned seen=0;
    for(const auto& v:vertices) {
        bool found=false;
        for(unsigned i=0;i<Vertices.size();++i)if(v==Vertices[i]){seen|=1u<<i;found=true;break;}
        if(!found)return false;
    }
    return seen==255;
}
inline bool ClipGeometry(const std::array<Row,18>& vertices,
                         const std::array<Row,4>& projection,const std::array<Row,3>& model) {
    if(!Transform(projection,model))return false;
    std::array<std::array<float,2>,8> corners{};
    for(unsigned i=0;i<Vertices.size();++i) {
        const float x=model[0][0]*Vertices[i][0]+200;
        const float y=model[1][1]*Vertices[i][1]+120;
        corners[i]={1-2*y/240,1-2*x/400}; // Rotated PICA screen.
    }
    unsigned seen=0;
    for(const auto& v:vertices) {
        for(float value:v)if(!std::isfinite(value))return false;
        if(!Near(v[3],1) || !Near(v[2],vertices[0][2]))return false;
        bool found=false;
        for(unsigned i=0;i<corners.size();++i)
            if(Near(v[0],corners[i][0]) && Near(v[1],corners[i][1])) {
                seen|=1u<<i;found=true;break;
            }
        if(!found)return false;
    }
    return seen==255;
}
}
