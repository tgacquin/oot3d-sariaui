// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_state.h"
#include <algorithm>
#include <vector>

namespace OOT3D::PickupFog {
using Matrix=std::array<float,12>;
struct Draw {
    std::array<Matrix,10> palette{};
    unsigned count{},indices{};
    std::uint64_t frame{};
    unsigned epoch{},scene{};
    std::int64_t time{};
};
inline std::mutex mutex;
inline std::vector<Draw> draws;
inline void Publish(const Draw& draw) {
    std::scoped_lock lock{mutex};
    std::erase_if(draws,[&](const Draw& d){return d.epoch!=draw.epoch || d.frame>draw.frame || draw.frame-d.frame>1;});
    if(draws.size()<256)draws.push_back(draw);
}
inline bool Match(const std::array<Matrix,10>& palette,unsigned indices,std::uint64_t frame,unsigned epoch,unsigned scene,std::int64_t now) {
    std::scoped_lock lock{mutex};
    for(const auto& d:draws) {
        if(d.indices!=indices || d.epoch!=epoch || d.scene!=scene || d.frame>frame || frame-d.frame>1 || now<d.time || now-d.time>100000)continue;
        bool match=true;
        for(unsigned b=0;b<d.count;++b)for(unsigned i=0;i<12;++i)
            match &= std::isfinite(palette[b][i]) && std::isfinite(d.palette[b][i]) &&
                std::abs(palette[b][i]-d.palette[b][i])<=(i%4==3?.03f:1e-5f);
        if(match)return true;
    }
    return false;
}
template<class Regs> bool Candidate(const Regs& r) {
    // World depth and exact native model/palette/index count identify pickups.
    return unsigned(r.texturing.fog_mode.Value())==0 &&
        r.framebuffer.output_merger.depth_test_enable &&
        r.texturing.main_config.texture0_enable;
}
}
