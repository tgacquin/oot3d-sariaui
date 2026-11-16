// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_rate.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace OOT3D::CircleWipe {
// Captured native circular screen wipe: nine indexed vertices, thirteen strip
// indices, IA8 128-square texture, orthographic CMB shader and no depth buffer.
// No texture address is fixed: scene loading can relocate the resource.
template<class Regs> bool Candidate(const Regs& r,std::uint64_t shader,std::uint64_t swizzle) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    return shader==0x6352a3be33cb99c6ULL && swizzle==0xba817d0ef27276d0ULL &&
        r.pipeline.num_vertices==13 && unsigned(r.pipeline.triangle_topology.Value())==1 &&
        r.vs.GetRegisterForAttribute(0)==0 && r.vs.GetRegisterForAttribute(1)==3 &&
        t.main_config.texture0_enable && !t.main_config.texture1_enable && !t.main_config.texture2_enable &&
        t.texture0.width==128 && t.texture0.height==128 && unsigned(t.texture0_format.Value())==5 &&
        unsigned(t.texture0.wrap_s.Value())==0 && unsigned(t.texture0.wrap_t.Value())==0 &&
        !m.depth_test_enable && !m.depth_write_enable && !m.stencil_test.enable && m.alphablend_enable &&
        unsigned(m.alpha_blending.factor_source_rgb.Value())==6 &&
        unsigned(m.alpha_blending.factor_dest_rgb.Value())==7 &&
        unsigned(m.alpha_blending.blend_equation_rgb.Value())==0 &&
        t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.modifiers_raw==0 &&
        t.tev_stage0.ops_raw==0x10001 && t.tev_stage0.scales_raw==0;
}
using Projection=std::array<std::array<float,4>,4>;
template<class Vertex> struct History {
    std::vector<Vertex> previous;
    std::uint64_t frame{};
    std::int64_t time{};
    unsigned generation{},scene{},texture{},target{};
    Projection projection{};
    bool ambiguous=false;
    void Reset() {*this={};}
    bool Apply(std::vector<Vertex>& vertices,std::uint64_t next,std::int64_t now,unsigned epoch,
               unsigned next_scene,unsigned image,unsigned framebuffer,const Projection& lens) {
        if(vertices.empty() || vertices.size()>33 || lens[3]!=std::array<float,4>{0,0,0,1}) {Reset();return false;}
        if(!previous.empty() && next==frame) {ambiguous=true;return false;}
        bool pair=!ambiguous && !previous.empty() && next==frame+1 && now-time>=20000 && now-time<=50000 &&
            epoch==generation && scene==next_scene && image==texture && framebuffer==target &&
            projection==lens && previous.size()==vertices.size();
        auto native=vertices;
        if(pair)for(unsigned i=0;i<vertices.size();++i) {
            const auto& a=previous[i];const auto& b=vertices[i];
            // The circle expands via UVs. Screen positions and tint stay fixed;
            // its tiny native Z movement only orders the overlay above the scene.
            for(unsigned c:{0u,1u,3u})pair &= a.position[c]==b.position[c];
            for(unsigned c=0;c<4;++c)pair &= a.color[c]==b.color[c] && std::isfinite(b.color[c]);
            pair &= std::isfinite(a.position[2]) && std::isfinite(b.position[2]) &&
                std::abs(a.position[2]-b.position[2])<.1f;
            for(unsigned c=0;c<2;++c)pair &= std::isfinite(a.tex_coord0[c]) && std::isfinite(b.tex_coord0[c]) &&
                std::abs(a.tex_coord0[c]-b.tex_coord0[c])<=
                    std::max(1.f,.5f*std::max(std::abs(a.tex_coord0[c]),std::abs(b.tex_coord0[c])));
            // CmbVShader also derives UV1/UV2 from the animated input UV.
            // Texture units 1/2 are disabled by Candidate, so those varying
            // outputs cannot affect this draw. Requiring them to be identical
            // rejects every real moving frame (synthetic vertices hid this).
            // Retain unused outputs verbatim; only texture 0 needs blending.
            pair &= a.tex_coord0_w==b.tex_coord0_w;
        }
        if(pair)for(unsigned i=0;i<vertices.size();++i) {
            for(unsigned c=0;c<2;++c)vertices[i].tex_coord0[c]=Interpolation::SampleMix(previous[i].tex_coord0[c],vertices[i].tex_coord0[c]);
            vertices[i].position[2]=Interpolation::SampleMix(previous[i].position[2],vertices[i].position[2]);
        }
        previous=std::move(native);frame=next;time=now;generation=epoch;scene=next_scene;
        texture=image;target=framebuffer;projection=lens;ambiguous=false;return pair;
    }
};
}
