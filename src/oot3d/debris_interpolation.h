// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_rate.h"
#include <atomic>
#include "mm3d/water_particle_interpolation.h"

namespace OOT3D::Interpolation::Debris {
namespace F = MM3D::ForestParticleInterpolation;
namespace W = MM3D::WaterParticleInterpolation;
using Vertex = std::array<std::array<float, 4>, 4>; // position, normal, color, UV
inline std::atomic<unsigned> pairs{};
// Gohma capture: 70–80 flakes per batch, falling up to 24.45 units/tick.
// Keep bounds within the shared 768-vertex replay capacity.

template<class Regs> bool Candidate(const Regs& r) {
    const auto& t = r.texturing;
    const auto& m = r.framebuffer.output_merger;
    return r.pipeline.num_vertices && r.pipeline.num_vertices <= 768 &&
        r.pipeline.num_vertices % 6 == 0 && unsigned(r.pipeline.triangle_topology.Value()) == 1 &&
        r.vs.GetRegisterForAttribute(0) == 0 && r.vs.GetRegisterForAttribute(1) == 1 &&
        r.vs.GetRegisterForAttribute(2) == 2 && r.vs.GetRegisterForAttribute(3) == 3 &&
        t.main_config.texture0_enable && !t.main_config.texture1_enable && !t.main_config.texture2_enable &&
        t.texture0.width == 32 && t.texture0.height == 32 && unsigned(t.texture0_format.Value()) == 13 &&
        unsigned(t.texture0.wrap_s.Value()) == 0 && unsigned(t.texture0.wrap_t.Value()) == 0 &&
        m.depth_test_enable && !m.depth_write_enable && !m.stencil_test.enable && m.alphablend_enable &&
        unsigned(m.alpha_blending.factor_source_rgb.Value()) == 6 &&
        unsigned(m.alpha_blending.factor_dest_rgb.Value()) == 7 &&
        unsigned(m.alpha_blending.blend_equation_rgb.Value()) == 0 &&
        t.tev_stage0.sources_raw == 0x0e300e30 && t.tev_stage0.modifiers_raw == 0 &&
        t.tev_stage0.ops_raw == 0x10001 && t.tev_stage0.scales_raw == 0 &&
        t.tev_stage1.sources_raw == 0x0fff0fff && t.tev_stage1.ops_raw == 0;
}
inline float Size(const F::Quad& q) {
    float s = 0;
    for (unsigned c = 0; c < 3; ++c) {
        const float d = q.vertices[1][0][c] - q.vertices[0][0][c];
        s += d*d;
    }
    return std::sqrt(s);
}
inline std::array<float, 3> Normal(const F::Quad& q) {
    std::array<float, 3> u{}, v{}, n{};
    for (unsigned c = 0; c < 3; ++c) {
        u[c] = q.vertices[1][0][c] - q.vertices[0][0][c];
        v[c] = q.vertices[2][0][c] - q.vertices[0][0][c];
    }
    for (unsigned c = 0; c < 3; ++c) n[c] = u[(c+1)%3]*v[(c+2)%3] - u[(c+2)%3]*v[(c+1)%3];
    return n;
}
inline std::optional<std::vector<F::Quad>> Decode(std::span<const Vertex> inputs,
                                                std::span<const std::uint32_t> order) {
    std::vector<F::Vertex> canonical(inputs.size());
    for (unsigned i = 0; i < inputs.size(); ++i) canonical[i] = {inputs[i][0], inputs[i][2], inputs[i][3]};
    auto quads = W::DecodeQuads(canonical, order);
    if (!quads || quads->empty() || quads->size() > 128) return {};
    for (const auto& q : *quads) {
        const float size = Size(q);
        if (size < 1 || size > 16) return {};
        const auto n = Normal(q);
        float vv = 0, nn = 0, dot = 0;
        for (unsigned c = 0; c < 3; ++c) {
            const float v = q.vertices[2][0][c] - q.vertices[0][0][c];
            vv += v*v; nn += n[c]*n[c];
            dot += n[c]*inputs[q.indices[0]][1][c];
        }
        if (std::abs(vv-size*size) > .15f || std::abs(std::sqrt(nn)-size*size) > .15f ||
            std::abs(std::abs(dot)-size*size*size) > 1.f) return {};
        const auto& uv = q.vertices[0][2];
        if ((!F::Near(uv[0], 0) && !F::Near(uv[0], .5f)) ||
            (!F::Near(uv[1], .5f) && !F::Near(uv[1], 1))) return {};
        // CPU strips run down the leaf first; older accelerated fixtures run
        // across it. Keep the submitted winding/indices and validate either
        // complete layout, rather than rejecting every live CPU leaf batch.
        // Death batches also contain whole-texture leaves (including native
        // clamped UVs outside [0,1]), mixed with the half-size atlas cells.
        const bool down=F::Near(q.vertices[1][2][0],uv[0]);
        const float extent=down?uv[1]-q.vertices[1][2][1]:q.vertices[1][2][0]-uv[0];
        if(!F::Near(extent,.5f) && !F::Near(extent,1.f))return {};
        for (unsigned k = 0; k < 4; ++k) {
            const auto& v = q.vertices[k];
            if (!F::Near(v[2][0], uv[0]+extent*(down?k/2:k%2)) ||
                !F::Near(v[2][1], uv[1]-extent*(down?k%2:k/2))) return {};
            for (unsigned c = 0; c < 4; ++c) {
                if (!F::Near(v[1][c], 1) || !std::isfinite(inputs[q.indices[k]][1][c]) ||
                    !F::Near(inputs[q.indices[k]][1][c], inputs[q.indices[0]][1][c])) return {};
            }
        }
    }
    return quads;
}
inline std::optional<unsigned> Match(const F::Quad& q, const std::vector<F::Quad>& others, bool newer) {
    std::optional<unsigned> index;
    float best = std::numeric_limits<float>::infinity(), second = best;
    for (unsigned i = 0; i < others.size(); ++i) {
        const auto& a = newer ? q : others[i];
        const auto& b = newer ? others[i] : q;
        const auto ac = F::Center(a), bc = F::Center(b);
        // Spider flakes fall vertically; the same material's Deku Baba leaves
        // drift sideways and initially rise. Both keep their individual size.
        // Order changes at births/deaths; never associate by slot or atlas cell.
        if (std::abs(ac[0]-bc[0]) > 2 || std::abs(ac[2]-bc[2]) > 2 ||
            ac[1]-bc[1] > 3 || ac[1]-bc[1] < -26 || std::abs(Size(a)-Size(b)) > .02f) continue;
        bool continuous = true;
        for (unsigned edge : {1u, 2u}) {
            float dot = 0;
            for (unsigned c = 0; c < 3; ++c)
                dot += (a.vertices[edge][0][c]-a.vertices[0][0][c]) *
                       (b.vertices[edge][0][c]-b.vertices[0][0][c]);
            if (dot < .5f*Size(a)*Size(b)) continuous = false;
        }
        if (!continuous) continue;
        const float score = (ac[1]-bc[1])*(ac[1]-bc[1]) +
            16*((ac[0]-bc[0])*(ac[0]-bc[0])+(ac[2]-bc[2])*(ac[2]-bc[2]));
        if (score < best) {second = best; best = score; index = i;}
        else if (score < second) second = score;
    }
    return second > best*1.5f+.01f ? index : std::nullopt;
}
class History {
    struct Entry {std::uint64_t frame{}; std::vector<F::Quad> previous,current;std::vector<bool> used;};
    std::map<unsigned, Entry> entries;
    unsigned epoch{}, scene{};
public:
    void Reset() {entries.clear();}
    void Context(unsigned e, unsigned s) {if (e != epoch || s != scene) Reset(); epoch=e; scene=s;}
    std::optional<std::vector<Vertex>> Apply(std::uint64_t frame, unsigned texture,
        std::span<const Vertex> inputs, std::span<const std::uint32_t> order) {
        const auto quads = Decode(inputs, order);
        if (!quads) {entries.erase(texture); return {};}
        for (auto it=entries.begin(); it!=entries.end();)
            if (it->second.frame>frame || frame-it->second.frame>1) it=entries.erase(it); else ++it;
        if (!entries.contains(texture) && entries.size()>=8) return {};
        auto& old = entries[texture];
        // Native particle lists split at 128 quads. Accumulate all draws of
        // this texture for the next frame; a second draw is not a reset.
        if(old.frame!=frame) {
            old.previous=old.frame+1==frame?std::move(old.current):std::vector<F::Quad>{};
            old.current.clear();old.used.assign(old.previous.size(),false);old.frame=frame;
        }
        std::vector<Vertex> out(inputs.begin(), inputs.end());
        bool changed=false;
        for (unsigned i=0; i<quads->size(); ++i) {
            const auto& q=(*quads)[i];
            const auto match=Match(q, old.previous, true);
            if (!match || old.used[*match] || Match(old.previous[*match], *quads, false)!=i) continue;
            old.used[*match]=true;
            auto half=q;
            for (unsigned k=0; k<4; ++k) for (unsigned c=0; c<3; ++c)
                half.vertices[k][0][c]=SampleMix(old.previous[*match].vertices[k][0][c],q.vertices[k][0][c]);
            F::PreserveSpinSize(half, q, old.previous[*match],sample_fraction);
            const auto n=Normal(half), native=Normal(q);
            float dot=0, length=0;
            for (unsigned c=0; c<3; ++c) {dot+=native[c]*inputs[q.indices[0]][1][c];length+=n[c]*n[c];}
            const float scale=(dot<0 ? -1.f : 1.f)*Size(half)/std::sqrt(length);
            for (unsigned k=0; k<4; ++k) for (unsigned c=0; c<3; ++c) {
                out[q.indices[k]][0][c]=half.vertices[k][0][c];
                out[q.indices[k]][1][c]=n[c]*scale;
            }
            // Keep native atlas cells: averaging UVs would display unrelated art.
            changed=true;
        }
        if(old.current.size()+quads->size()<=512)
            old.current.insert(old.current.end(),quads->begin(),quads->end());
        if (changed) {++pairs;return out;}
        return {};
    }
};
}
