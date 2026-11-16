// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompts.h"
#include "oot3d/map_frame_asset.h"

namespace OOT3D::RoomPlayerIcon {
constexpr unsigned Width=32,Height=32;
using Pixels=std::array<std::uint32_t,Width*Height>;
using Geometry=PausePrompts::QuadGeometry<1>;
inline std::shared_ptr<const Pixels> pixels;
inline auto GetPixels() {return std::atomic_load(&pixels);}
constexpr unsigned Tiled(unsigned x,unsigned y) {
    unsigned m=0;for(unsigned b=0;b<3;++b)m|=((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1);
    return ((y/8)*(Width/8)+x/8)*64+m;
}
// Original dungeon-map portrait, [496,512) x [88,104), ETC1A4.
// Transparent padding isolates it from the adjacent chest/Skulltula artwork.
template<class Decode> std::shared_ptr<const Pixels> Extract(std::span<const std::uint8_t> b,Decode decode) {
    using MapFrame::LE32;using MapFrame::LE64;
    if(b.size()!=72+512*256 || LE32(b,0)!=0x62787463 || LE32(b,4)!=b.size() ||
       LE32(b,8)!=1 || LE32(b,20)!=72 || LE32(b,36)!=512*256 ||
       LE32(b,44)!=(512|(256<<16)) || LE32(b,48)!=0x675b)return {};
    auto out=std::make_shared<Pixels>();
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
        const unsigned sx=496+x,sy=88+y;
        const auto at=72+((sy/8)*64+sx/8)*64+((sy%8)/4*2+(sx%8)/4)*16;
        const auto rgb=decode(LE64(b,at+8),sx%4,sy%4);
        const unsigned alpha=((LE64(b,at)>>(4*(4*(sx%4)+sy%4)))&15)*17;
        (*out)[Tiled(x+8,Height-1-(y+8))]=(unsigned(rgb[0])<<24)|(unsigned(rgb[1])<<16)|(unsigned(rgb[2])<<8)|alpha;
    }
    return out;
}
}
