// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/debris_interpolation.h"
namespace OOT3D::Interpolation::WarpSparkles {
namespace F=MM3D::ForestParticleInterpolation;
namespace W=MM3D::WaterParticleInterpolation;
using F::Vertex;
using Color=std::array<float,4>;
using Key=std::array<std::uint32_t,4>; // texture and native particle RGB group
inline std::optional<std::vector<F::Quad>> Decode(std::span<const Vertex> inputs,
                                                 std::span<const std::uint32_t> order) {
    auto quads=W::DecodeQuads(inputs,order);
    if(!quads || quads->empty() || quads->size()>128)return {};
    const auto color=quads->front().vertices[0][1];
    for(auto& q:*quads) {
        const auto original=q;unsigned used=0;
        for(unsigned i=0;i<4;++i) {
            const auto& v=original.vertices[i];unsigned corner=4;
            for(unsigned k=0;k<4;++k)
                if(F::Near(v[2][0],float(k%2)*2) && F::Near(v[2][1],float(1-k/2)*2))corner=k;
            if(corner==4 || (used&(1u<<corner)) || v[1]!=color)return {};
            used|=1u<<corner;q.vertices[corner]=v;q.indices[corner]=original.indices[i];
        }
        for(float c:color)if(c<0 || c>1.001f)return {};
        const float size=Debris::Size(q);float vv=0,uv=0;
        for(unsigned c=0;c<3;++c) {
            const float u=q.vertices[1][0][c]-q.vertices[0][0][c];
            const float v=q.vertices[2][0][c]-q.vertices[0][0][c];vv+=v*v;uv+=u*v;
        }
        if(size<.25f || size>16 || std::abs(vv-size*size)>.02f || std::abs(uv)>.02f)return {};
    }
    return quads;
}
inline std::optional<unsigned> Match(const F::Quad& q,const std::vector<F::Quad>& others) {
    float best=1e9f,second=best;std::optional<unsigned> found;
    const auto center=F::Center(q);
    for(unsigned i=0;i<others.size();++i) {
        const auto& p=others[i];const auto old=F::Center(p);
        const float a=Debris::Size(q),b=Debris::Size(p);
        if(std::abs(a-b)>.05f)continue;
        float distance=0,dot=0;
        for(unsigned c=0;c<3;++c) {
            distance+=(center[c]-old[c])*(center[c]-old[c]);
            dot+=(q.vertices[1][0][c]-q.vertices[0][0][c])*(p.vertices[1][0][c]-p.vertices[0][0][c]);
        }
        // Native rotation is about twenty degrees/tick. Reject respawns,
        // ambiguous neighbours and discontinuous motion rather than streaking.
        if(distance>16 || dot<.7f*a*b)continue;
        if(distance<best){second=best;best=distance;found=i;}else second=std::min(second,distance);
    }
    return second>best*1.5f+.01f?found:std::nullopt;
}
class History {
    struct Entry {std::uint64_t frame{};std::vector<F::Quad> quads;Color color{};};
    std::map<Key,Entry> entries;
public:
    std::optional<Color> color;
    void Reset(){entries.clear();color.reset();}
    std::optional<std::vector<Vertex>> Apply(std::uint64_t frame,unsigned texture,
        std::span<const Vertex> inputs,std::span<const std::uint32_t> order,Color constant) {
        color.reset();const auto current=Decode(inputs,order);if(!current)return {};
        for(float c:constant)if(!std::isfinite(c) || c<0 || c>1)return {};
        Key key{texture};for(unsigned c=0;c<3;++c)key[c+1]=std::bit_cast<std::uint32_t>(current->front().vertices[0][1][c]);
        for(auto i=entries.begin();i!=entries.end();)
            if(i->second.frame>frame || frame-i->second.frame>1)i=entries.erase(i);else ++i;
        if(!entries.contains(key) && entries.size()>=32)return {};
        auto& old=entries[key];
        if(old.frame==frame){old.quads.clear();return {};}
        bool valid=old.frame+1==frame && !old.quads.empty();
        for(unsigned c=0;c<4;++c)valid &= std::abs(constant[c]-old.color[c])<.16f;
        std::vector<Vertex> out(inputs.begin(),inputs.end());unsigned matched=0;
        if(valid)for(unsigned i=0;i<current->size();++i) {
            const auto& q=(*current)[i];const auto match=WarpSparkles::Match(q,old.quads);
            if(!match || WarpSparkles::Match(old.quads[*match],*current)!=i)continue;
            auto half=q;
            if(sample_fraction<1) {
                for(unsigned k=0;k<4;++k)for(unsigned c=0;c<3;++c)
                    half.vertices[k][0][c]=SampleMix(old.quads[*match].vertices[k][0][c],q.vertices[k][0][c]);
                F::PreserveSpinSize(half,q,old.quads[*match],sample_fraction);
            }
            for(unsigned k=0;k<4;++k)out[q.indices[k]]=half.vertices[k];
            ++matched;
        }
        // Only interpolate the shared combiner fade when all particles match;
        // newly spawned particles must retain their native initial opacity.
        if(matched==current->size()) {
            Color blend;for(unsigned c=0;c<4;++c)blend[c]=SampleMix(old.color[c],constant[c]);
            color=blend;
        }
        old={frame,*current,constant};return matched?std::optional{out}:std::nullopt;
    }
};
}
