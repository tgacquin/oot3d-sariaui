// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>

namespace OOT3D::Assets {
// Original black-to-transparent edge texture, embedded in the mod. The smooth
// quadratic falloff has no seam at its transparent end. Repeat along the edge;
// stretch only across the shadow's user-controlled width. No game art required.
inline constexpr auto PanelShadow = [] {
    std::array<std::uint32_t,64*8> pixels{};
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<64;++x) {
        unsigned morton=0;
        for(unsigned bit=0;bit<3;++bit)
            morton|=((x>>bit)&1)<<(bit*2)|((y>>bit)&1)<<(bit*2+1);
        const unsigned alpha=(255*(63-x)*(63-x)+1984)/3969;
        // PICA tiled RGBA8 is little-endian ABGR; RGB is black.
        pixels[(x/8)*64+morton]=alpha;
    }
    return pixels;
}();
}
