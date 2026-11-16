// SPDX-License-Identifier: GPL-2.0-or-later
// Captured OoT3D forest sprites and shallow-water rings. Shared DekUI geometry
// helpers are used only after validating OoT's material and attribute layout.
#pragma once
#include "oot3d/interpolation_rate.h"
#include <atomic>
#include "oot3d/effect_strip_replay.h"
#include "oot3d/debris_interpolation.h"
#include "oot3d/green_particle_interpolation.h"
#include "oot3d/warp_sparkle_interpolation.h"
#include "oot3d/warp_trail_interpolation.h"
#include "oot3d/water_sprite_interpolation.h"
#include "mm3d/water_particle_interpolation.h"
namespace OOT3D::Interpolation::SurfaceEffects {
namespace F = MM3D::ForestParticleInterpolation;
namespace W = MM3D::WaterParticleInterpolation;
namespace R = MM3D::RippleInterpolation;
using F::Vertex;
using F::Quad;
inline std::atomic<bool> enabled{true};
inline std::atomic<unsigned> forest_pairs{}, ring_pairs{}, shockwave_pairs{}, rejected_batches{};
// Native draw -> camera replay -> valid connector mapping -> changed vertices.
// A frame replay can succeed while a particular effect remains unchanged.
struct ReplayCounts {std::atomic<unsigned> seen{},attempted{},canonical{},changed{};};
inline std::array<ReplayCounts,16> replay_counts;

// Numeric PICA enums: strip=1, IA8=7, src-alpha=6, one=1, add=0.
// No texture addresses are hardcoded: bindings change with scene allocation.
template<class Regs> unsigned Candidate(const Regs& r, std::uint64_t shader, std::uint64_t swizzle) {
    if(enabled && shader==0x6352a3be33cb99c6ULL && swizzle==0xba817d0ef27276d0ULL)
        if(const auto kind=WaterSprites::Candidate(r))return kind;
    if (enabled && shader == 0x6352a3be33cb99c6ULL && swizzle == 0xba817d0ef27276d0ULL &&
        Debris::Candidate(r)) return 3;
    if (enabled && shader == 0x6352a3be33cb99c6ULL && swizzle == 0xba817d0ef27276d0ULL &&
        GreenParticles::Candidate(r)) return 5;
    if (!enabled || shader != 0x6352a3be33cb99c6ULL || swizzle != 0xba817d0ef27276d0ULL ||
        unsigned(r.pipeline.triangle_topology.Value()) != 1 || !r.pipeline.num_vertices ||
        r.pipeline.num_vertices > 768 || r.pipeline.num_vertices % 6 ||
        r.vs.GetRegisterForAttribute(0) != 0 || r.vs.GetRegisterForAttribute(1) != 2 ||
        r.vs.GetRegisterForAttribute(2) != 3) return 0;
    const auto& t = r.texturing;
    const auto& m = r.framebuffer.output_merger;
    if (!t.main_config.texture0_enable || t.main_config.texture1_enable || t.main_config.texture2_enable ||
        unsigned(t.texture0_format.Value()) != 7 ||
        !m.alphablend_enable || !m.depth_test_enable || m.depth_write_enable || m.stencil_test.enable ||
        unsigned(m.alpha_blending.factor_source_rgb.Value()) != 6 ||
        unsigned(m.alpha_blending.factor_dest_rgb.Value()) != 1 ||
        unsigned(m.alpha_blending.blend_equation_rgb.Value()) != 0 ||
        t.tev_stage0.sources_raw != 0x0ee00ee3 || t.tev_stage0.modifiers_raw != 0 ||
        t.tev_stage0.ops_raw != 0 || t.tev_stage0.scales_raw != 2 ||
        t.tev_stage1.sources_raw != 0x0eee0efe || t.tev_stage1.modifiers_raw != 0 ||
        t.tev_stage1.ops_raw != 1) return 0;
    if(t.texture0.width==16 && t.texture0.height==16 &&
       unsigned(t.texture0.wrap_s.Value())==3 && unsigned(t.texture0.wrap_t.Value())==3)return 6;
    if(t.texture0.width!=64)return 0;
    if (t.texture0.height == 64 && unsigned(t.texture0.wrap_s.Value()) == 0 &&
        unsigned(t.texture0.wrap_t.Value()) == 0)
        // Trails and forest sprites share this material. Use the paired-node
        // geometry below to distinguish them; vertex/combiner tints are native data.
        return r.pipeline.num_vertices<=180 && r.pipeline.num_vertices%12==0 ? 7 : 1;
    if (t.texture0.height == 32 && unsigned(t.texture0.wrap_s.Value()) == 3 &&
        unsigned(t.texture0.wrap_t.Value()) == 3) return 2;
    if (r.pipeline.num_vertices == 6 && t.texture0.height == 64 &&
        unsigned(t.texture0.wrap_s.Value()) == 3 && unsigned(t.texture0.wrap_t.Value()) == 3 &&
        t.tev_stage2.sources_raw == 0x0ee00f30 && t.tev_stage2.ops_raw == 8) return 4;
    return 0;
}
inline std::optional<std::vector<Quad>> Forest(std::span<const Vertex> inputs,
                                              std::span<const std::uint32_t> order) {
    auto quads = W::DecodeQuads(inputs, order);
    if (!quads || quads->empty() || quads->size() > 32) return {};
    const auto kind = F::Kind(quads->front().vertices[0]);
    if (kind != 1 && kind != 2) return {};
    for (auto& q : *quads) {
        // Captured Kokiri strips run down each square before crossing it.
        // Normalize by UV for matching/spin math, retaining the real indices
        // so replay writes back into the unchanged native submission order.
        Quad ordered{};unsigned used=0;
        for (unsigned i=0;i<4;++i) {
            if(F::Kind(q.vertices[i])!=kind)return {};
            unsigned corner=4;
            for(unsigned k=0;k<4;++k)
                if(F::Near(q.vertices[i][2][0],float(k%2)) &&
                   F::Near(q.vertices[i][2][1],float(1-k/2)))corner=k;
            if(corner==4 || (used&(1u<<corner)))return {};
            used|=1u<<corner;ordered.vertices[corner]=q.vertices[i];ordered.indices[corner]=q.indices[i];
        }
        q=ordered;
        float uu=0,vv=0,uv=0;
        for (unsigned c=0;c<3;++c) {
            const float u=q.vertices[1][0][c]-q.vertices[0][0][c];
            const float v=q.vertices[2][0][c]-q.vertices[0][0][c];
            uu+=u*u;vv+=v*v;uv+=u*v;
        }
        // OoT keeps dormant, zero-size sprites in its fixed 32-slot batches.
        if (uu>256 || std::abs(uu-vv)>.1f || std::abs(uv)>.1f) return {};
    }
    return quads;
}
// Kokiri's first sixteen slots form two ordered eight-node trails per color.
// They advance by half the same-color node spacing each tick, making nearest
// neighbor matching ambiguous (and sometimes pairing a tip with a small dot).
// Recognize the full chain before using its stable submission slots.
inline bool ForestTrail(const std::vector<Quad>& quads,unsigned base) {
    if(quads.size()!=32 || (base!=0 && base!=8))return false;
    const unsigned kind=F::Kind(quads[base].vertices[0]);
    if(kind!=1 && kind!=2)return false;
    std::array<float,3> last_step{};
    for(unsigned i=0;i<8;++i) {
        const auto& q=quads[base+i];
        const float size=Debris::Size(q),expected=kind==1 && i==0?8.f:3.2f;
        if(std::abs(size-expected)>.01f ||
           !F::Near(q.vertices[0][1][3],100.f/255))return false;
        if(!i)continue;
        const auto a=F::Center(quads[base+i-1]),b=F::Center(q);
        std::array<float,3> step{};float distance=0,dot=0,previous=0;
        for(unsigned c=0;c<3;++c) {
            step[c]=b[c]-a[c];distance+=step[c]*step[c];
            dot+=step[c]*last_step[c];previous+=last_step[c]*last_step[c];
        }
        if(distance<1 || distance>36 || (i>1 && dot<.8f*std::sqrt(distance*previous)))return false;
        last_step=step;
    }
    return true;
}
inline bool ForestTrailPair(const std::vector<Quad>& now,const std::vector<Quad>& old,unsigned base) {
    if(!ForestTrail(now,base) || !ForestTrail(old,base))return false;
    for(unsigned i=base;i<base+8;++i) {
        if(!F::Compatible(now[i],old[i]))return false;
        // A slot swap or respawn cannot borrow the neighboring node's history.
        if(i>base) {
            const auto a=F::Center(now[i]),b=F::Center(old[i]);
            const auto c=F::Center(now[i-1]),d=F::Center(old[i-1]);
            float change=0;for(unsigned k=0;k<3;++k)change+=std::pow((a[k]-b[k])-(c[k]-d[k]),2);
            if(change>1)return false;
        }
    }
    return true;
}
inline std::optional<W::Rings> Rings(std::span<const Vertex> inputs,
                                    std::span<const std::uint32_t> order) {
    const auto quads=W::DecodeQuads(inputs,order);
    if (!quads || quads->size()%2) return {};
    W::Rings out;
    // The ring matcher expects corners across each half, while native strips
    // can run down it. Resolve corners by UV, preserving the original indices
    // so interpolation writes back into the unchanged native strip ordering.
    for (const auto& q:*quads) {
        std::array<unsigned,4> corners{};unsigned used=0;
        for(unsigned i=0;i<4;++i) {
            const auto& v=q.vertices[i];unsigned corner=4;
            for(unsigned k=0;k<4;++k)
                if(F::Near(v[2][0],float(1-k%2)) && F::Near(v[2][1],float(k/2)))corner=k;
            if(corner==4 || (used&(1u<<corner)))return {};
            corners[corner]=i;used|=1u<<corner;
        }
        for(auto i:corners) {
            const auto& v=q.vertices[i];
            out.mesh.points.push_back({v[0][0],v[0][1],v[0][2]});
            out.mesh.colors.push_back(v[1]);out.indices.push_back(q.indices[i]);
        }
    }
    for(unsigned i=0;i<out.mesh.points.size();i+=8)if(!R::Describe(out.mesh,i))return {};
    out.mesh.points.push_back({});out.mesh.colors.push_back({});
    return out;
}
inline std::optional<Quad> Shockwave(std::span<const Vertex> inputs,
                                     std::span<const std::uint32_t> order) {
    const auto quads = W::DecodeQuads(inputs, order);
    if (!quads || quads->size() != 1) return {};
    // Replay inputs retain the native strip ordering, which can run down
    // rather than across the quad. Identify corners by UV and retain their
    // original indices for writeback; never change the submitted strip.
    Quad q{};unsigned used=0;
    for (unsigned i=0; i<4; ++i) {
        const auto& v=quads->front().vertices[i];
        unsigned corner=4;
        for(unsigned k=0;k<4;++k)
            if(F::Near(v[2][0],float(k%2)*2) && F::Near(v[2][1],float(1-k/2)*2))corner=k;
        if(corner==4 || (used&(1u<<corner)))return {};
        used|=1u<<corner;q.vertices[corner]=v;q.indices[corner]=quads->front().indices[i];
    }
    // Native waves align to the floor normal, including sloped terrain.
    // Validate a nearly square, perpendicular pair of edges in 3D instead
    // of requiring constant Y and an axis-aligned square in X/Z.
    std::array<float,3> u{},v{};float uu=0,vv=0,uv=0;
    for(unsigned c=0;c<3;++c) {
        u[c]=q.vertices[1][0][c]-q.vertices[0][0][c];
        v[c]=q.vertices[2][0][c]-q.vertices[0][0][c];
        uu+=u[c]*u[c];vv+=v[c]*v[c];uv+=u[c]*v[c];
        if(std::abs(q.vertices[3][0][c]-q.vertices[1][0][c]-
                    q.vertices[2][0][c]+q.vertices[0][0][c])>.01f)return {};
    }
    if(uu<1 || vv<1 || uu>512*512 || vv>512*512 ||
       std::abs(uu-vv)>.01f*std::max(uu,vv) || std::abs(uv)>.001f*std::sqrt(uu*vv))return {};
    return q;
}
class History {
    struct Entry { std::uint64_t frame{}; std::vector<Quad> quads; };
    std::map<std::array<std::uint32_t,2>,Entry> forest;
    std::map<unsigned,Entry> shockwaves;
    R::History rings;
    GreenParticles::History green;
    WarpSparkles::History sparkles;
    WarpTrail::History trail;
    WaterSprites::History water_sprites;
    unsigned epoch{}, scene{};
public:
    void Reset() { forest.clear();shockwaves.clear();rings={};green.Reset();sparkles.Reset();trail.Reset();water_sprites.Reset(); }
    auto SparkleColor() const {return sparkles.color;}
    const auto& OldSpriteInputs() const {return water_sprites.old_inputs;}
    void Context(unsigned next_epoch,unsigned next_scene) {
        if(epoch!=next_epoch || scene!=next_scene)Reset();
        epoch=next_epoch;scene=next_scene;
        green.Context(next_epoch,next_scene);
    }
    std::optional<std::vector<Vertex>> Apply(unsigned kind,std::uint64_t frame,unsigned texture,
        std::span<const Vertex> inputs,std::span<const std::uint32_t> order,WarpSparkles::Color constant={}) {
        std::vector<Vertex> out(inputs.begin(),inputs.end());
        if(kind>=8 && kind<=15)return water_sprites.Apply(kind,frame,texture,inputs,order);
        if(kind==7) {
            const bool transport=bool(WarpTrail::Decode(inputs,order));
            auto result=trail.Apply(frame,texture,inputs,order);
            if(transport)return result;
            // Small forest batches can share the material and vertex count.
            // Keep their existing decoder/history when paired nodes do not fit.
            kind=1;
        }
        if(kind==6)return sparkles.Apply(frame,texture,inputs,order,constant);
        if(kind==5)return green.Apply(frame,texture,inputs,order);
        if(kind==4) {
            const auto current=Shockwave(inputs,order);
            if(!current){shockwaves.erase(texture);++rejected_batches;return {};}
            for(auto it=shockwaves.begin();it!=shockwaves.end();)
                if(it->second.frame>frame || frame-it->second.frame>1)it=shockwaves.erase(it);else ++it;
            if(!shockwaves.contains(texture) && shockwaves.size()>=8)return {};
            auto& previous=shockwaves[texture];
            if(previous.frame==frame){previous.quads.clear();return {};}
            bool valid=previous.frame+1==frame && previous.quads.size()==1;
            if(valid) {
                const auto& old=previous.quads.front();
                const auto a=F::Center(*current),b=F::Center(old);
                for(unsigned c=0;c<3;++c)if(std::abs(a[c]-b[c])>.03f ||
                    // Bomb light fades across the expanding ring (~.021 per
                    // captured tick). Blend that tint along with its alpha.
                    std::abs(current->vertices[0][1][c]-old.vertices[0][1][c])>.05f)valid=false;
                const float growth=Debris::Size(*current)-Debris::Size(old);
                const float fade=current->vertices[0][1][3]-old.vertices[0][1][3];
                if(growth<-.01f || growth>40 || fade>.001f || fade<-.15f)valid=false;
                if(valid)for(unsigned k=0;k<4;++k)for(unsigned c=0;c<4;++c) {
                    out[current->indices[k]][0][c]=SampleMix(old.vertices[k][0][c],current->vertices[k][0][c]);
                    out[current->indices[k]][1][c]=SampleMix(old.vertices[k][1][c],current->vertices[k][1][c]);
                }
            }
            previous={frame,{*current}};
            if(valid){++shockwave_pairs;return out;}
            return {};
        }
        if(kind==2) {
            const auto current=Rings(inputs,order);
            if(!current){++rejected_batches;return {};}
            const auto half=rings.Apply(frame,texture,epoch,current->mesh,sample_fraction);
            if(!half)return {};
            for(unsigned i=0;i<current->indices.size();++i) {
                auto& v=out[current->indices[i]];
                for(unsigned c=0;c<3;++c)v[0][c]=half->points[i][c];
                v[1]=half->colors[i];
            }
            ++ring_pairs;return out;
        }
        const auto current=Forest(inputs,order);
        if(!current){++rejected_batches;return {};}
        for(auto it=forest.begin();it!=forest.end();)
            if(it->second.frame>frame || frame-it->second.frame>1)it=forest.erase(it);else ++it;
        const std::array<std::uint32_t,2> key{texture,F::Kind(current->front().vertices[0])};
        if(!forest.contains(key) && forest.size()>=8)return {};
        auto& previous=forest[key];
        if(previous.frame==frame){previous.quads.clear();return {};}
        bool changed=false;
        const std::array<bool,2> trails{ForestTrail(*current,0),ForestTrail(*current,8)};
        const std::array<bool,2> trail_pairs{ForestTrailPair(*current,previous.quads,0),
                                           ForestTrailPair(*current,previous.quads,8)};
        if(previous.frame+1==frame)for(unsigned i=0;i<current->size();++i) {
            std::optional<unsigned> match;
            if(i<16 && trails[i/8]) {
                if(trail_pairs[i/8])match=i;
            } else {
                match=F::Match((*current)[i],previous.quads);
                if(match && F::Match(previous.quads[*match],*current)!=i)match.reset();
            }
            if(!match)continue;
            auto half=(*current)[i];const auto& old=previous.quads[*match];
            for(unsigned v=0;v<4;++v)for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
                half.vertices[v][a][c]=SampleMix(old.vertices[v][a][c],half.vertices[v][a][c]);
            // Keep the mean size while blending orientation, including normal
            // camera-facing changes. Averaging corners alone shrinks a rotation.
            F::PreserveSpinSize(half,(*current)[i],old,sample_fraction);
            for(unsigned v=0;v<4;++v)out[half.indices[v]]=half.vertices[v];
            changed=true;
        }
        previous={frame,*current};
        if(changed){++forest_pairs;return out;}
        return {};
    }
};
}
