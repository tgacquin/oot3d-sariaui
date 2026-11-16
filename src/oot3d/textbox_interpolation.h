// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_rate.h"
#include "oot3d/hud_routing.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace OOT3D::TextboxInterpolation {
// Native 0x3135e8 uploads the composed model/view matrix at c20.
constexpr unsigned ModelViewRow=20;
constexpr unsigned MaxVertices=4096;
// Native UI meshes use either triangle lists or quad strips joined by
// two degenerate indices. The latter submit 6*q-2 indices, including 4
// for the single-quad textbox plate.
inline unsigned MeshVertices(unsigned indices,bool strip) {
    if(strip) {
        if(indices<4 || indices>MaxVertices/4*6-2 || (indices+2)%6)return 0;
        return (indices+2)/6*4;
    }
    return indices && indices%6==0 && indices<=MaxVertices/4*6?indices/6*4:0;
}
inline bool FilteredArrow(unsigned pane) {return pane==2 || pane==3;}
using Source=std::array<std::uint32_t,4>; // model, mesh, message, pane
using Matrix=std::array<std::array<float,4>,3>;
struct Route {
    std::uint64_t frame{},hash{},layout{};
    unsigned generation{},vertices{};
    Source source{};
    Matrix matrix{};
    bool typewriter=false,strip=false;
    unsigned message{};
    float reveal_clock{};
};
inline std::mutex mutex;
inline std::vector<Route> routes;
inline bool Recent(std::uint64_t submitted,std::uint64_t frame) {
    return submitted<=frame && frame-submitted<=1;
}
inline void Publish(const Route& route) {
    std::scoped_lock lock{mutex};
    std::erase_if(routes,[&](const auto& r){return r.generation!=route.generation ||
        r.frame>route.frame || route.frame-r.frame>3;});
    if(routes.size()<512)routes.push_back(route);
}
inline bool Active(std::uint64_t frame,unsigned generation) {
    std::scoped_lock lock{mutex};
    return std::any_of(routes.begin(),routes.end(),[&](const auto& r){
        return r.generation==generation && Recent(r.frame,frame);
    });
}
inline std::optional<Route> Find(std::uint64_t hash,unsigned vertices,const Matrix& matrix,
                                 std::uint64_t frame,unsigned generation) {
    std::scoped_lock lock{mutex};
    std::optional<Route> found;
    for(const auto& r:routes) {
        if(r.generation!=generation || !Recent(r.frame,frame) || r.hash!=hash || r.vertices!=vertices)continue;
        bool match=true;
        for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)
            match &= std::isfinite(matrix[i][j]) && std::abs(matrix[i][j]-r.matrix[i][j])<=.02f;
        if(!match)continue;
        if(found && (found->source!=r.source || found->layout!=r.layout))return {};
        found=r;
    }
    return found;
}
// Work on private intermediate-frame vertices only. Native glyphs are prepared
// ahead of time; the typewriter submits a growing prefix of their quad strip.
// Keep complete glyphs, with every native endpoint and script update untouched.
inline unsigned ExpandedGlyphVertices(unsigned glyphs,bool strip) {
    return glyphs?(strip?(6*glyphs-4)*3:6*glyphs):0;
}
template<class Vertex> struct History {
    struct Entry {
        Source source{};
        std::uint64_t frame{},layout{};
        std::int64_t time{};
        unsigned generation{},texture{},target{};
        std::array<std::array<float,4>,4> projection{};
        std::vector<Vertex> vertices;
        bool depth_motion=false,ambiguous=false;
        unsigned glyphs{},message{};
        bool typewriter=false,strip=false;
        float reveal_clock{};
    };
    std::vector<Entry> entries;
    unsigned rejection=0; // 1 first, 2 duplicate, 4 cadence, 8 layout, 16 shape/projection, 32 UV/depth, 64 discontinuity
    void Reset() {entries.clear();}
    // The native opening bounce animates Z alongside scale. For owned UI
    // with depth testing/writes disabled, that is continuous presentation,
    // not a layer change. Keep strict depth matching for depth-tested draws.
    bool Apply(std::vector<Vertex>& out,const Route& route,std::uint64_t frame,std::int64_t time,
               unsigned texture,unsigned target,const std::array<std::array<float,4>,4>& projection,
               bool depth_motion=false) {
        rejection=1;
        std::erase_if(entries,[&](const auto& e){return e.generation!=route.generation ||
            e.frame>frame || frame-e.frame>1;});
        if(out.empty() || out.size()>(MaxVertices/4*6-2-2)*3)return false;
        Entry current{route.source,frame,route.layout,time,route.generation,texture,target,projection,out,depth_motion};
        current.glyphs=route.vertices/4;current.message=route.message;
        current.typewriter=route.typewriter;current.strip=route.strip;current.reveal_clock=route.reveal_clock;
        for(auto& e:entries) {
            if(e.source!=route.source || e.texture!=texture || e.target!=target)continue;
            if(e.frame==frame) {e.ambiguous=true;rejection=2;return false;}
            // Accept only an append of the same page's prefix. The native
            // fractional budget also rules out instant-text/skip bursts, even
            // when a control code turns instant mode back off in the same tick.
            const float budget=route.reveal_clock-e.reveal_clock;
            const unsigned added=current.glyphs>e.glyphs?current.glyphs-e.glyphs:0;
            const bool reveal=route.typewriter && e.typewriter && e.message==route.message &&
                route.source[3]==255 && route.strip==e.strip && e.glyphs && added && added<=32 &&
                std::isfinite(budget) && budget>0 && budget<=32 && added<=std::ceil(budget) &&
                e.vertices.size()==ExpandedGlyphVertices(e.glyphs,e.strip) &&
                out.size()==ExpandedGlyphVertices(current.glyphs,route.strip);
            rejection=(e.ambiguous?2u:0u) |
                (e.frame+1!=frame || time-e.time<20000 || time-e.time>50000?4u:0u) |
                (e.layout!=route.layout?8u:0u) |
                (e.projection!=projection || (!reveal && e.vertices.size()!=out.size()) || e.depth_motion!=depth_motion?16u:0u);
            bool compatible=rejection==0;
            if(compatible)for(unsigned i=0;i<e.vertices.size();++i) {
                const auto& a=e.vertices[i];const auto& b=out[i];
                if(!(a.tex_coord0==b.tex_coord0 && a.tex_coord1==b.tex_coord1 &&
                    a.tex_coord2==b.tex_coord2 && (depth_motion || a.position[2]==b.position[2]) && a.position[3]==b.position[3]))rejection|=32;
                compatible=rejection==0;
                for(unsigned c=0;c<4;++c) {
                    const bool continuous=std::isfinite(a.position[c]) && std::isfinite(b.position[c]) &&
                        std::abs(a.position[c]-b.position[c])<=std::abs(b.position[3])*1.5f &&
                        std::isfinite(a.color[c]) && std::isfinite(b.color[c]) &&
                        std::abs(a.color[c]-b.color[c])<=.5f;
                    if(!continuous)rejection|=64;
                    compatible &= continuous;
                }
            }
            if(compatible && !reveal)for(unsigned i=0;i<e.vertices.size();++i)for(unsigned c=0;c<4;++c) {
                // The choice arrow snaps to the selected row. Keep color/alpha
                // blending for fades, but never add a position between options.
                if(route.source[3]!=3)
                    out[i].position[c]=Interpolation::SampleMix(e.vertices[i].position[c],out[i].position[c]);
                out[i].color[c]=Interpolation::SampleMix(e.vertices[i].color[c],out[i].color[c]);
            }
            if(compatible && reveal) {
                // Round up so the first new glyph is immediate; the rest of a
                // batch appears over subsequent presentation samples. At phase1
                // every native glyph is present, preserving overall speed.
                const float phase=std::isfinite(Interpolation::sample_fraction)?
                    std::clamp(Interpolation::sample_fraction,0.f,1.f):1.f;
                const unsigned shown=e.glyphs+unsigned(std::ceil(added*phase));
                out.resize(ExpandedGlyphVertices(shown,route.strip));
            }
            e=std::move(current);return compatible;
        }
        if(entries.size()<256)entries.push_back(std::move(current));
        return false;
    }
};
}
