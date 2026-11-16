// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/debris_interpolation.h"
namespace OOT3D::Interpolation::WarpTrail {
namespace F=MM3D::ForestParticleInterpolation;
namespace W=MM3D::WaterParticleInterpolation;
using F::Vertex;
using Key=std::array<std::uint32_t,4>; // texture and native trail RGB
// Each trail node emits two overlapping squares. Native random offsets are
// integral, leaving the fractional anchor stable when old nodes are removed.
inline bool SameAnchor(const F::Quad& a,const F::Quad& b) {
    const auto x=F::Center(a),y=F::Center(b);float distance=0;
    for(unsigned c=0;c<3;++c) {
        const float d=x[c]-y[c];distance+=d*d;
        if(std::abs(d-std::round(d))>.002f)return false;
    }
    return distance<40*40;
}
inline std::optional<std::vector<F::Quad>> Decode(std::span<const Vertex> inputs,
                                                 std::span<const std::uint32_t> order) {
    auto quads=W::DecodeQuads(inputs,order);
    if(!quads || quads->empty() || quads->size()>30 || quads->size()%2)return {};
    const auto color=quads->front().vertices[0][1];
    for(float c:color)if(!std::isfinite(c) || c<0 || c>1)return {};
    if(!F::Near(color[3],1))return {};
    for(auto& q:*quads) {
        const auto original=q;unsigned used=0;
        for(unsigned i=0;i<4;++i) {
            const auto& v=original.vertices[i];unsigned corner=4;
            // Warp destinations tint the same paired-square trail. Preserve
            // that uniform native tint without restricting it to purple.
            if(v[1]!=color)return {};
            for(unsigned k=0;k<4;++k)
                if(F::Near(v[2][0],float(k%2)) && F::Near(v[2][1],float(1-k/2)))corner=k;
            if(corner==4 || (used&(1u<<corner)))return {};
            used|=1u<<corner;q.vertices[corner]=v;q.indices[corner]=original.indices[i];
        }
        float uu=0,vv=0,uv=0;
        for(unsigned c=0;c<3;++c) {
            const float u=q.vertices[1][0][c]-q.vertices[0][0][c];
            const float v=q.vertices[2][0][c]-q.vertices[0][0][c];
            uu+=u*u;vv+=v*v;uv+=u*v;
        }
        // Zero-size nodes are intentional: interpolate their flicker as well.
        if(uu>40*40 || std::abs(uu-vv)>.1f || std::abs(uv)>.1f)return {};
    }
    for(unsigned i=0;i<quads->size();i+=2)
        if(!SameAnchor((*quads)[i],(*quads)[i+1]))return {};
    return quads;
}
inline std::optional<unsigned> Match(const F::Quad& q,const std::vector<F::Quad>& others) {
    std::optional<unsigned> found;
    for(unsigned i=0;i<others.size();i+=2)if(SameAnchor(q,others[i])) {
        if(found)return {}; // Ambiguous nodes retain native geometry.
        found=i;
    }
    return found;
}
class History {
    struct Entry {std::uint64_t frame{};std::vector<F::Quad> quads;};
    std::map<Key,Entry> entries;
public:
    void Reset(){entries.clear();}
    std::optional<std::vector<Vertex>> Apply(std::uint64_t frame,unsigned texture,
        std::span<const Vertex> inputs,std::span<const std::uint32_t> order) {
        const auto current=Decode(inputs,order);
        if(!current){
            std::erase_if(entries,[&](const auto& e){return e.first[0]==texture;});
            return {};
        }
        Key key{texture};
        for(unsigned c=0;c<3;++c)key[c+1]=std::bit_cast<std::uint32_t>(current->front().vertices[0][1][c]);
        for(auto i=entries.begin();i!=entries.end();)
            if(i->second.frame>frame || frame-i->second.frame>1)i=entries.erase(i);else ++i;
        if(!entries.contains(key) && entries.size()>=8)return {};
        auto& old=entries[key];
        if(old.frame==frame){old.quads.clear();return {};}
        std::vector<Vertex> out(inputs.begin(),inputs.end());bool matched=false;
        if(old.frame+1==frame)for(unsigned i=0;i<current->size();i+=2) {
            const auto match=WarpTrail::Match((*current)[i],old.quads);
            if(!match || WarpTrail::Match(old.quads[*match],*current)!=i ||
               !SameAnchor((*current)[i+1],old.quads[*match+1]))continue;
            for(unsigned j=0;j<2;++j) {
                const auto& q=(*current)[i+j];const auto& previous=old.quads[*match+j];auto half=q;
                if(sample_fraction<1) {
                    for(unsigned k=0;k<4;++k)for(unsigned c=0;c<3;++c)
                        half.vertices[k][0][c]=SampleMix(previous.vertices[k][0][c],q.vertices[k][0][c]);
                    F::PreserveSpinSize(half,q,previous,sample_fraction);
                }
                for(unsigned k=0;k<4;++k)out[q.indices[k]]=half.vertices[k];
            }
            matched=true;
        }
        old={frame,*current};return matched?std::optional{out}:std::nullopt;
    }
};
}
