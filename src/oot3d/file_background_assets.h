// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_frame_asset.h"
#include <atomic>
#include <memory>

namespace OOT3D::FileBackground {
constexpr unsigned Width=512,Height=256,Format=12;
using Pixels=std::array<std::uint32_t,Width*Height/8>;
inline std::shared_ptr<const Pixels> pixels;
inline auto GetPixels() {return std::atomic_load(&pixels);}
inline std::optional<Pixels> MakePixels(std::span<const std::uint8_t> source) {
    using MapFrame::LE32;
    if(source.size()!=72+Width*Height/2 || LE32(source,0)!=0x62787463 ||
       LE32(source,4)!=source.size() || LE32(source,8)!=1 || LE32(source,20)!=72 ||
       LE32(source,44)!=(Width|(Height<<16)) || LE32(source,48)!=0x675a)return {};
    Pixels out;
    // Keep the original ETC1 payload and dimensions for native texture-pack lookup.
    for(unsigned i=0;i<out.size();++i)out[i]=LE32(source,72+4*i);
    return out;
}
}
