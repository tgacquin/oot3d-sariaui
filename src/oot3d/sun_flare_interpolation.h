// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_rate.h"
#include <vector>

namespace OOT3D::SunFlare {
enum class Layer { Circles, Rays, Wash };

template<class Regs, class Uniforms>
std::optional<Layer> Identify(const Regs& r,const Uniforms& uniforms,std::uint64_t shader,std::uint64_t swizzle) {
    const auto n=r.pipeline.num_vertices;
    if((n!=72 && n!=10 && n!=4) || shader!=0x6352a3be33cb99c6ULL || swizzle!=0xba817d0ef27276d0ULL ||
       unsigned(r.pipeline.triangle_topology.Value())!=1)return {};
    const auto& t=r.texturing;
    const auto& m=r.framebuffer.output_merger;
    const auto& blend=m.alpha_blending;
    if(uniforms.f[3].w.ToFloat32()!=1 || std::abs(uniforms.f[2].z.ToFloat32()-1.f/3)>.00001f ||
       std::abs(uniforms.f[2].w.ToFloat32()+1.f/3)>.00001f ||
       t.main_config.texture1_enable || t.main_config.texture2_enable || m.depth_test_enable ||
       m.depth_write_enable || m.stencil_test.enable || !m.alphablend_enable ||
       unsigned(blend.factor_source_rgb.Value())!=6 || unsigned(blend.blend_equation_rgb.Value())!=0 ||
       t.tev_stage0.sources_raw!=0x0e300e30 || t.tev_stage0.modifiers_raw || t.tev_stage0.scales_raw)return {};
    if(n==4) {
        if(!t.main_config.texture0_enable && unsigned(blend.factor_dest_rgb.Value())==7 &&
           t.tev_stage0.ops_raw==0 && uniforms.f[20].x.ToFloat32()==400 &&
           uniforms.f[21].y.ToFloat32()==240)return Layer::Wash;
    } else if(t.main_config.texture0_enable && unsigned(blend.factor_dest_rgb.Value())==1 &&
        t.tev_stage0.ops_raw==0x10001 && t.texture0.width==128 && t.texture0.height==128 &&
        unsigned(t.texture0_format.Value())==12 && unsigned(t.texture0.wrap_s.Value())==0 &&
        unsigned(t.texture0.wrap_t.Value())==(n==72?0u:3u))return n==72?Layer::Circles:Layer::Rays;
    return {};
}

template<class V> struct History {
    struct Sample {
        std::vector<V> vertices;
        std::uint64_t frame{};
        std::array<unsigned,5> key{}; // Scene, camera owner, cut group, target and flare atlas.
    };
    std::array<Sample,3> layers;
    bool Apply(Layer layer,std::vector<V>& vertices,std::uint64_t frame,const std::array<unsigned,5>& key) {
        auto& previous=layers[unsigned(layer)];
        // The untextured wash follows the two textured strips in the native draw order.
        if(layer==Layer::Wash && (layers[1].frame!=frame || layers[1].key!=key))return false;
        bool pair=previous.frame+1==frame && previous.key==key && !vertices.empty() &&
                  vertices.size()==previous.vertices.size();
        auto native=vertices;
        if(pair) {
            for(unsigned i=0;i<vertices.size();++i) {
                const auto& old=previous.vertices[i];
                auto& v=vertices[i];
                for(unsigned c=0;c<2;++c)v.position[c]=Interpolation::SampleMix(old.position[c],v.position[c]);
                for(unsigned c=0;c<4;++c)v.color[c]=Interpolation::SampleMix(old.color[c],v.color[c]);
            }
        }
        previous={std::move(native),frame,key};
        return pair;
    }
};
}
