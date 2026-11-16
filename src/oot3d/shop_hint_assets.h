// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompts.h"
#include "oot3d/map_frame_asset.h"

namespace OOT3D::ShopHint {
constexpr unsigned Width=64,Height=32;
using Pixels=std::array<std::uint32_t,Width*Height>;
inline std::shared_ptr<const Pixels> pixels;
inline auto GetPixels() {return std::atomic_load(&pixels);}
constexpr unsigned Tiled(unsigned x,unsigned y,unsigned width) {
    unsigned m=0;for(unsigned bit=0;bit<3;++bit)
        m|=((x>>bit)&1)<<(2*bit)|((y>>bit)&1)<<(2*bit+1);
    return ((y/8)*(width/8)+x/8)*64+m;
}
inline std::optional<Pixels> MakePixels(std::span<const std::uint8_t> archive) {
    using MapFrame::LE32;
    if(archive.size()<24 || LE32(archive,0)!=0x0152415a)return {};
    const unsigned count=archive[10]|unsigned(archive[11])<<8;
    const auto files=LE32(archive,16),offsets=LE32(archive,20);
    if(!count || count>4096 || files>archive.size() || offsets>archive.size() ||
       count*8>archive.size()-files || count*4>archive.size()-offsets)return {};
    Pixels out{};unsigned found=0;
    // Native shop initialization loads texture indices 29 and 30 from keep:
    // 1039e8..103a04. The arrow is tinted red by 35ddb4; the stick is white.
    constexpr std::array<std::string_view,2> names{
        "cursor/tex/h_arrow_cursor_1_txt.ctxb","cursor/tex/h_arrow_cursor_2_txt.ctxb"};
    for(unsigned i=0;i<count;++i)for(unsigned tile=0;tile<2;++tile) {
        const auto name=names[tile];const auto text=LE32(archive,files+i*8+4);
        if(text>=archive.size() || name.size()+1>archive.size()-text ||
           archive[text+name.size()] || !std::equal(name.begin(),name.end(),archive.begin()+text))continue;
        const auto size=LE32(archive,files+i*8),at=LE32(archive,offsets+i*4);
        const unsigned w=tile?16:32;
        if(at>archive.size() || size>archive.size()-at || size!=72+w*Height*2)return {};
        const auto source=archive.subspan(at,size);
        if(LE32(source,0)!=0x62787463 || LE32(source,4)!=size || LE32(source,8)!=1 ||
           LE32(source,20)!=72 || LE32(source,36)!=w*Height*2 ||
           LE32(source,44)!=(w|(Height<<16)) || LE32(source,48)!=0x14016758)return {};
        for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<w;++x) {
            const auto p=72+Tiled(x,y,w)*2;const unsigned a=source[p],l=source[p+1];
            out[Tiled(x+tile*32,y,Width)]=(l<<24)|(l<<16)|(l<<8)|a;
        }
        found|=1u<<tile;
    }
    return found==3?std::optional{out}:std::nullopt;
}
}
