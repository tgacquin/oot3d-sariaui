// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/dpad_layout.h"
#include "oot3d/map_frame_asset.h"
#include "oot3d/assets/restoration_dpad.h"

namespace OOT3D::DPad {
constexpr unsigned Tiled(unsigned x,unsigned y) {
    unsigned m=0;
    for(unsigned bit=0;bit<3;++bit)m|=((x>>bit)&1)<<(bit*2)|((y>>bit)&1)<<(bit*2+1);
    return ((y/8)*(TextureWidth/8)+x/8)*64+m;
}
inline bool ValidAtlas(std::span<const std::uint8_t> b,unsigned width,unsigned height,
                       unsigned format,unsigned bytes) {
    using namespace MapFrame;
    return b.size()==72+bytes && LE32(b,0)==0x62787463 && LE32(b,4)==b.size() &&
        LE32(b,8)==1 && LE32(b,16)==24 && LE32(b,20)==72 &&
        LE32(b,24)==0x20786574 && LE32(b,28)==48 && LE32(b,32)==1 &&
        LE32(b,36)==bytes && LE32(b,44)==(width|(height<<16)) &&
        LE32(b,48)==format && LE32(b,52)==0;
}
// Coordinates identify cells in the decoded atlas, top to bottom. Nintendo
// stores these icons vertically reversed inside their cells.
// Reverse each cell while copying, without moving it to another atlas row.
template<class Sample>
Pixels MakeTexture(bool time,Sample sample) {
    Pixels pixels{};
    for(unsigned y=0;y<104;++y)for(unsigned x=0;x<104;++x)
        pixels[Tiled(x,y)]=Assets::RestorationDpad[(103-y)*104+x];
    const auto copy=[&](bool menu,unsigned sx,unsigned sy,unsigned w,unsigned h,
                        unsigned dx,unsigned dy) {
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
            pixels[Tiled(dx+x,dy+h-1-y)]=sample(menu,sx+x,sy+h-1-y);
    };
    // Native Navi cell: UV (176,0), 48x48, counted from the atlas bottom.
    copy(true,176,208,48,48,128,0);
    copy(false,time?336:294,470,42,42,192,0);
    copy(false,378,260,42,42,128,64); // Native item IDs 0x45 and 0x46.
    copy(false,420,260,42,42,192,64);
    return pixels;
}
}
