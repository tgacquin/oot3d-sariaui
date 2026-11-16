// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_rate.h"
#include <atomic>
#include "oot3d/debris_interpolation.h"
namespace OOT3D::Interpolation::GreenParticles {
namespace F=MM3D::ForestParticleInterpolation;
namespace W=MM3D::WaterParticleInterpolation;
using F::Vertex;
inline std::atomic<unsigned> pairs{};
template<class Regs> bool Candidate(const Regs& r) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    return r.pipeline.num_vertices && r.pipeline.num_vertices<=384 && r.pipeline.num_vertices%6==0 &&
        unsigned(r.pipeline.triangle_topology.Value())==1 && r.vs.GetRegisterForAttribute(0)==0 &&
        r.vs.GetRegisterForAttribute(1)==2 && r.vs.GetRegisterForAttribute(2)==3 &&
        t.main_config.texture0_enable && !t.main_config.texture1_enable && !t.main_config.texture2_enable &&
        t.texture0.width==32 && t.texture0.height==32 && unsigned(t.texture0_format.Value())==5 &&
        unsigned(t.texture0.wrap_s.Value())==0 && unsigned(t.texture0.wrap_t.Value())==0 &&
        m.depth_test_enable && !m.depth_write_enable && !m.stencil_test.enable && m.alphablend_enable &&
        unsigned(m.alpha_blending.factor_source_rgb.Value())==6 &&
        unsigned(m.alpha_blending.factor_dest_rgb.Value())==7 &&
        unsigned(m.alpha_blending.blend_equation_rgb.Value())==0 &&
        t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.modifiers_raw==0 &&
        t.tev_stage0.ops_raw==0x10001 && t.tev_stage0.scales_raw==0 &&
        t.tev_stage1.sources_raw==0x0fff0fff && t.tev_stage1.ops_raw==0;
}
inline std::optional<std::vector<F::Quad>> Decode(std::span<const Vertex> inputs,
                                                std::span<const std::uint32_t> order) {
    auto quads=W::DecodeQuads(inputs,order,false);
    if(!quads || quads->empty() || quads->size()>64)return {};
    for(auto& q:*quads) {
        // Native CPU strips can run down the sprite. Match UV corners before
        // comparing particles across frames, keeping the native input indices
        // and each corner's lighting attached to the corresponding vertex.
        const auto original=q;unsigned used=0;
        for(unsigned i=0;i<4;++i) {
            const auto& v=original.vertices[i];unsigned corner=4;
            for(unsigned k=0;k<4;++k)
                if(F::Near(v[2][0],float(k%2)) && F::Near(v[2][1],float(1-k/2)))corner=k;
            if(corner==4 || (used&(1u<<corner)))return {};
            used|=1u<<corner;q.vertices[corner]=v;q.indices[corner]=original.indices[i];
        }
        const float size=Debris::Size(q);float vv=0,uv=0;
        if(size<.1f || size>32)return {};
        for(unsigned c=0;c<3;++c) {
            const float u=q.vertices[1][0][c]-q.vertices[0][0][c];
            const float v=q.vertices[2][0][c]-q.vertices[0][0][c];vv+=v*v;uv+=u*v;
        }
        if(std::abs(vv-size*size)>.1f || std::abs(uv)>.1f)return {};
        for(unsigned k=0;k<4;++k) {
            const auto& v=q.vertices[k];const auto& color=v[1];
            if(!F::Near(v[2][0],float(k%2)) || !F::Near(v[2][1],float(1-k/2)) ||
                !F::Near(color[3],q.vertices[0][1][3]))return {};
            // The I8 blood sprite is tinted by vertex color. Its material and
            // quad layout identify the effect; green dominance excluded red,
            // blue and other enemy variants (and green under dim lighting).
            for(unsigned c=0;c<3;++c)if(color[c]<0 || color[c]>1)return {};
        }
    }
    return quads;
}
inline std::optional<unsigned> Match(const F::Quad& q,const std::vector<F::Quad>& others,bool newer) {
    float best=std::numeric_limits<float>::infinity(),second=best;std::optional<unsigned> result;
    for(unsigned i=0;i<others.size();++i) {
        const auto& a=newer?q:others[i];const auto& b=newer?others[i]:q;
        const float fade=a.vertices[0][1][3]-b.vertices[0][1][3];
        if(fade>.001f || fade<-.06f)continue;
        bool colors=true;
        for(unsigned k=0;k<4;++k)for(unsigned c=0;c<3;++c)
            if(std::abs(a.vertices[k][1][c]-b.vertices[k][1][c])>.04f)colors=false;
        if(!colors)continue;
        const auto ac=F::Center(a),bc=F::Center(b);float distance=0;
        for(unsigned c=0;c<3;++c)distance+=(ac[c]-bc[c])*(ac[c]-bc[c]);
        if(distance>144)continue;
        if(distance<best){second=best;best=distance;result=i;}else if(distance<second)second=distance;
    }
    return second>best*1.5f+.01f?result:std::nullopt;
}
class History {
    struct Entry {std::uint64_t frame{};std::vector<F::Quad> quads;};
    std::map<unsigned,Entry> entries;unsigned epoch{},scene{};
public:
    void Reset(){entries.clear();}
    void Context(unsigned e,unsigned s){if(epoch!=e || scene!=s)Reset();epoch=e;scene=s;}
    std::optional<std::vector<Vertex>> Apply(std::uint64_t frame,unsigned texture,
        std::span<const Vertex> inputs,std::span<const std::uint32_t> order) {
        const auto quads=Decode(inputs,order);if(!quads){entries.erase(texture);return {};}
        for(auto it=entries.begin();it!=entries.end();)
            if(it->second.frame>frame || frame-it->second.frame>1)it=entries.erase(it);else ++it;
        if(!entries.contains(texture) && entries.size()>=8)return {};
        auto& old=entries[texture];if(old.frame==frame){old.quads.clear();return {};}
        std::vector<Vertex> out(inputs.begin(),inputs.end());bool changed=false;
        if(old.frame+1==frame)for(unsigned i=0;i<quads->size();++i) {
            const auto& q=(*quads)[i];const auto match=Match(q,old.quads,true);
            if(!match || Match(old.quads[*match],*quads,false)!=i)continue;
            auto half=q;
            for(unsigned k=0;k<4;++k)for(unsigned a=0;a<2;++a)for(unsigned c=0;c<4;++c)
                half.vertices[k][a][c]=SampleMix(old.quads[*match].vertices[k][a][c],q.vertices[k][a][c]);
            F::PreserveSpinSize(half,q,old.quads[*match],sample_fraction);
            for(unsigned k=0;k<4;++k)out[q.indices[k]]=half.vertices[k];
            changed=true;
        }
        old={frame,*quads};if(changed){++pairs;return out;}return {};
    }
};
}
