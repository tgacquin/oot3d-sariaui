// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "oot3d/debris_interpolation.h"
#include "oot3d/interpolation_rate.h"

namespace OOT3D::Interpolation::CombatSprites {
namespace F = MM3D::ForestParticleInterpolation;
namespace W = MM3D::WaterParticleInterpolation;
using Color = std::array<float, 4>;
inline constexpr unsigned Charge = 16, Streaks = 17;

inline bool ChargeTint(const Color& vertex, const Color& tint) {
    return tint[3] == 0 && (tint[0] > 0 || tint[1] > 0) &&
           F::Near(vertex[0], -tint[0]) && F::Near(vertex[1], -tint[1]) && vertex[2] == 0;
}

struct Batch {
    std::vector<F::Quad> quads;
    Color constant;
};

inline bool Continuous(unsigned kind, const Batch& current, const Batch& previous) {
    if (current.quads.size() != previous.quads.size()) return false;
    for (unsigned c = 0; c < 4; ++c)
        if (std::abs(current.constant[c] - previous.constant[c]) > .2f) return false;
    unsigned directions = 0;
    for (unsigned i = 0; i < current.quads.size(); ++i) {
        const auto& now = current.quads[i];
        const auto& old = previous.quads[i];
        const auto a = F::Center(now), b = F::Center(old);
        float distance = 0, dot = 0, uu = 0, vv = 0;
        for (unsigned c = 0; c < 3; ++c) {
            distance += (a[c]-b[c])*(a[c]-b[c]);
            const float u = now.vertices[2][0][c] - now.vertices[0][0][c];
            const float v = old.vertices[2][0][c] - old.vertices[0][0][c];
            dot += u*v;
            uu += u*u;
            vv += v*v;
        }
        if (kind == Charge) {
            const auto alpha = now.vertices[0][1][3] - old.vertices[0][1][3];
            const auto shrink = std::sqrt(vv) - std::sqrt(uu);
            // Fixed eye position through a charge, including the final collapse.
            if (distance > .01f || shrink < -.02f || shrink > 12.f || alpha < 0 || alpha > .05f)
                return false;
        } else {
            // Each impact emits its own ordered fan of thin streaks. Its rays
            // retain their individual directions even when overlapping fans reorder.
            if (distance > 64.f*64.f) return false;
            if (uu > .01f && vv > .01f) {
                if (dot < .995f*std::sqrt(uu*vv)) return false;
                ++directions;
            }
        }
    }
    return kind == Charge || directions >= 2;
}

class History {
    struct Entry {
        std::uint64_t frame{};
        std::vector<Batch> current, previous;
        std::vector<bool> used;
    };
    std::map<std::array<unsigned, 2>, Entry> entries;
public:
    std::optional<Color> constant;
    void Reset() { entries.clear(); constant.reset(); }

    std::optional<std::vector<F::Vertex>> Apply(unsigned kind, std::uint64_t frame, unsigned texture,
            std::span<const F::Vertex> inputs, std::span<const std::uint32_t> order, Color tint) {
        constant.reset();
        auto decoded = W::DecodeQuads(inputs, order);
        if (!decoded || decoded->empty() || (kind == Charge && decoded->size() != 1)) return {};
        for (const auto& q : *decoded) {
            for (unsigned k = 0; k < 4; ++k)
                if (q.vertices[k][2][0] != float(k/2) || q.vertices[k][2][1] != float(1-k%2))
                    return {};
            if (kind == Charge) {
                const auto& color = q.vertices[0][1];
                if (!ChargeTint(color, tint)) return {};
            }
        }
        for (auto it = entries.begin(); it != entries.end();)
            if (it->second.frame > frame || frame - it->second.frame > 1) it = entries.erase(it);
            else ++it;
        const std::array key{kind, texture};
        if (!entries.contains(key) && entries.size() >= 16) return {};
        auto& e = entries[key];
        if (e.frame != frame) {
            e.previous = e.frame+1 == frame ? std::move(e.current) : std::vector<Batch>{};
            e.current.clear();
            e.used.assign(e.previous.size(), false);
            e.frame = frame;
        }
        Batch now{std::move(*decoded), tint};
        std::optional<unsigned> match;
        for (unsigned i = 0; i < e.previous.size(); ++i) {
            if (e.used[i] || !Continuous(kind, now, e.previous[i])) continue;
            if (match) { match.reset(); break; }
            match = i;
        }
        std::optional<std::vector<F::Vertex>> result;
        if (match) {
            const auto& old = e.previous[*match];
            e.used[*match] = true;
            result.emplace(inputs.begin(), inputs.end());
            for (unsigned i = 0; i < now.quads.size(); ++i) for (unsigned k = 0; k < 4; ++k) {
                auto& v = (*result)[now.quads[i].indices[k]];
                const auto& p = old.quads[i].vertices[k];
                for (unsigned c = 0; c < 3; ++c) v[0][c] = std::lerp(p[0][c], v[0][c], sample_fraction);
                for (unsigned c = 0; c < 4; ++c) v[1][c] = std::lerp(p[1][c], v[1][c], sample_fraction);
            }
            constant = tint;
            for (unsigned c = 0; c < 4; ++c) (*constant)[c] = std::lerp(old.constant[c], tint[c], sample_fraction);
        }
        if (e.current.size() < 32) e.current.push_back(std::move(now));
        return result;
    }
};
}
