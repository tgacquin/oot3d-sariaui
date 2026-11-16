// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include "oot3d/hd_rupee_font.h"
#include "oot3d/map_frame_asset.h"

namespace OOT3D::HDFont {
// challenge00 has its own 12x21 digits at (336,208), and punctuation
// one 24-pixel row below. Keep all other challenge artwork unchanged.
inline std::array<Common::AtomicSharedPtr<const RupeeAtlas>,3> boss_timer_atlases;
std::vector<std::uint8_t> MakeBossTimerAtlas(std::span<const std::uint8_t> rgba,RupeeShadow shadow={},bool bold=false);
std::shared_ptr<const RupeeAtlas> StyledBossTimerAtlas(std::uint64_t hash,RupeeShadow shadow,bool bold=false);
template<class Decode> std::shared_ptr<RupeeAtlas> DecodeBossTimerAtlas(
    std::span<const std::uint8_t> b,Decode decode) {
    using MapFrame::LE32;using MapFrame::LE64;
    if(b.size()!=72+512*256 || LE32(b,0)!=0x62787463 || LE32(b,4)!=b.size() ||
       LE32(b,8)!=1 || LE32(b,20)!=72 || LE32(b,36)!=512*256 ||
       LE32(b,44)!=(512|(256<<16)) || LE32(b,48)!=0x675b)return {};
    auto out=std::make_shared<RupeeAtlas>();out->native.resize(512*256*4);
    for(unsigned y=0;y<256;++y)for(unsigned x=0;x<512;++x) {
        const auto at=72+((y/8)*64+x/8)*64+((y%8)/4*2+(x%8)/4)*16;
        const auto rgb=decode(LE64(b,at+8),x%4,y%4);
        const auto p=(y*512+x)*4;
        for(unsigned c=0;c<3;++c)out->native[p+c]=rgb[c];
        out->native[p+3]=((LE64(b,at)>>(4*(4*(x%4)+y%4)))&15)*17;
    }
    return out;
}
}
