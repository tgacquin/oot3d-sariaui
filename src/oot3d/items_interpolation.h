// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "oot3d/interpolation_rate.h"
#include <map>
#include <vector>

namespace OOT3D::Interpolation::MenuGeometry {
// The guest can alternate vertex buffers for the same pane. Match its material,
// geometry size and occurrence, then verify each triangle's atlas coordinates.
using Material = std::array<std::uint32_t, 9>;
template<class Vertex>
class History {
    using Batches = std::map<Material, std::vector<std::vector<Vertex>>>;
    Batches previous, current;
    std::uint64_t frame{};
    unsigned vertex_count{}, draw_count{};
public:
    bool Apply(std::uint64_t next, const Material& material, const std::vector<Vertex>& native,
               std::vector<Vertex>& result) {
        if (frame != next) {
            previous.clear();
            if (next == frame + 1) previous.swap(current);
            current.clear();
            frame = next;
            vertex_count = draw_count = 0;
        }
        if (native.empty() || native.size() % 3 || native.size() > 32768 - vertex_count ||
            draw_count >= 256) return false;
        vertex_count += native.size();
        ++draw_count;
        auto& batches = current[material];
        const auto occurrence = batches.size();
        batches.push_back(native);
        const auto it = previous.find(material);
        if (it == previous.end() || occurrence >= it->second.size()) return false;
        const auto& before = it->second[occurrence];
        if (before.size() != native.size()) return false;
        bool paired = false;
        for (std::size_t i = 0; i < native.size(); i += 3) {
            std::array<std::array<float,2>,3> from{},to{};
            bool continuous = true;
            for (unsigned j = 0; j < 3; ++j) {
                const auto& a = before[i+j];
                const auto& b = native[i+j];
                const float aw = a.position[3], bw = b.position[3];
                if (!(aw > 0 && bw > 0)) { continuous = false; break; }
                for (unsigned c = 0; c < 2; ++c) {
                    from[j][c] = a.position[c]/aw;
                    to[j][c] = b.position[c]/bw;
                    if (a.tex_coord0[c] != b.tex_coord0[c] ||
                        std::abs(from[j][c] - to[j][c]) > .65f) continuous = false;
                }
            }
            if (!continuous) continue;
            // File select hides icons by collapsing their vertices at a parking
            // position. A midpoint to or from that position exposes the icon.
            const auto area = [](const auto& p) {
                return (p[1][0]-p[0][0])*(p[2][1]-p[0][1]) -
                       (p[1][1]-p[0][1])*(p[2][0]-p[0][0]);
            };
            const float start = area(from), end = area(to);
            if (!((start > 0 && end > 0) || (start < 0 && end < 0))) continue;
            paired = true;
            if (sample_fraction >= 1) continue;
            for (unsigned j = 0; j < 3; ++j) {
                const auto& a = before[i+j];
                const auto& b = native[i+j];
                if (a.position == b.position && a.color == b.color) continue;
                if (result.empty()) result = native;
                for (unsigned c = 0; c < 4; ++c) {
                    result[i+j].position[c] = SampleMix(a.position[c], b.position[c]);
                    result[i+j].color[c] = SampleMix(a.color[c], b.color[c]);
                }
            }
        }
        return paired;
    }
};
}
