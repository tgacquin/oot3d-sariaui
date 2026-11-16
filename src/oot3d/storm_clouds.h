// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/widescreen.h"
namespace OOT3D::StormClouds {
template<class Regs> bool Candidate(const Regs& r) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    if(r.pipeline.num_vertices!=4 || unsigned(r.pipeline.triangle_topology.Value())!=1 ||
       r.vs.GetRegisterForAttribute(0)!=0 || r.vs.GetRegisterForAttribute(1)!=3 ||
       r.vs.GetRegisterForAttribute(2)!=4 || !t.main_config.texture0_enable ||
       !t.main_config.texture1_enable || t.main_config.texture2_enable ||
       unsigned(t.texture0_format.Value())!=5 || unsigned(t.texture1_format.Value())!=5 ||
       m.depth_test_enable || m.depth_write_enable || m.stencil_test.enable ||
       !m.alphablend_enable || unsigned(m.alpha_blending.factor_source_rgb.Value())!=6 ||
       unsigned(m.alpha_blending.factor_dest_rgb.Value())!=7 ||
       unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0)return false;
    for(const auto& texture:t.GetTextures())if(texture.enabled) {
        const auto& c=texture.config;
        if(c.width!=64 || c.height!=64 || unsigned(c.wrap_s.Value())!=2 || unsigned(c.wrap_t.Value())!=2)return false;
    }
    const auto stages=t.GetTevStages();
    for(unsigned i=0;i<6;++i) {
        const auto& s=stages[i];
        if(s.sources_raw!=(i==0?0x00430043u:i==1?0x0fff0fefu:0x0fff0fffu) ||
           s.ops_raw!=(i==0?0x90009u:i==1?2u:0u) || s.modifiers_raw || s.scales_raw)return false;
    }
    return true;
}
inline bool Geometry(const std::array<Widescreen::Row,4>& projection,
                     const std::array<Widescreen::Row,3>& model,
                     const std::array<Widescreen::Row,4>& vertices) {
    // Native storm overlay uses a literal 400x240 quad, not the unit mesh
    // used by fades. Prove its complete screen coverage before expanding it.
    const std::array<Widescreen::Row,4> expected{{{0,-2.f/240,0,1},{-2.f/400,0,0,1},
                                                {0,0,1.f/3,-1.f/3},{0,0,0,1}}};
    const std::array<Widescreen::Row,3> transform{{{1,0,0,0},{0,1,0,0},{0,0,1,-1}}};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        if(!std::isfinite(projection[r][c]) || std::abs(projection[r][c]-expected[r][c])>1e-6f)return false;
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
        if(!std::isfinite(model[r][c]) || std::abs(model[r][c]-transform[r][c])>1e-6f)return false;
    return vertices==std::array<Widescreen::Row,4>{{{0,0,0,1},{400,0,0,1},{0,240,0,1},{400,240,0,1}}};
}
}
