// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace OOT3D::MapTimeTint {
using Color=std::array<float,3>;
inline constexpr Color Neutral{1,1,1};
// MM3D's world-map lighting uses these nine clock intervals (005967c4).
// These gentler RGB multipliers are for OoT's flat artwork, not its 3D lights.
inline Color At(std::uint16_t time) {
    constexpr std::array<Color,4> colors{{{1,.90f,.82f},Neutral,{1,.82f,.72f},{.50f,.56f,.72f}}};
    struct Interval {unsigned end,from,to;};
    constexpr std::array<Interval,9> intervals{{
        {0x2aaa,3,3},{0x3555,3,0},{0x4000,0,0},{0x5555,0,1},
        {0xaaaa,1,1},{0xb555,1,2},{0xc000,2,2},{0xcaaa,2,3},{0x10000,3,3}}};
    unsigned start=0;
    for(const auto& interval:intervals) {
        if(time<interval.end) {
            const float t=float(time-start)/float(interval.end-start);
            Color result{};
            for(unsigned c=0;c<3;++c)result[c]=std::lerp(colors[interval.from][c],colors[interval.to][c],t);
            return result;
        }
        start=interval.end;
    }
    return Neutral;
}
template<class M> Color Read(M& m,bool enabled) {
    // Verified against native sun/moon positioning at 004594c0: a halfword
    // clock at Save+0xc. The upper halfword includes the MQ flag; ignore it.
    if(!enabled || m.Read(0x4596a0)!=0x587958 || m.Read(0x4594e4)!=0xe1d600bc)return Neutral;
    const auto clock=m.Read(0x587964);
    return clock?At(std::uint16_t(*clock)):Neutral;
}
}
