// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"

namespace OOT3D::MinimapControl {
// Native gameplay maps are 128x128 ETC1A4 or tiled RGBA4 textures. Their visible drawing
// occupies different portions of that square; alpha, not the quad dimensions,
// identifies the edge to anchor. No texture pixels are written or bundled.
struct TextureBounds {
    float left{},bottom{},right{},top{}; // Normalized native texture coordinates.
    bool operator==(const TextureBounds&) const=default;
};
using AlphaBlocks=std::array<std::uint64_t,1024>;
inline std::optional<TextureBounds> VisibleBounds(const AlphaBlocks& alpha,TextureBounds crop) {
    const int x0=std::max(0,int(std::floor(crop.left*128))),
        y0=std::max(0,int(std::floor((1-crop.top)*128))),
        x1=std::min(128,int(std::ceil(crop.right*128))),
        y1=std::min(128,int(std::ceil((1-crop.bottom)*128)));
    int left=128,bottom=128,right=0,top=0;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
        const unsigned block=((y/8)*16+x/8)*4+(y%8)/4*2+(x%8)/4;
        if(((alpha[block]>>(4*(4*(x%4)+y%4)))&15)==0)continue;
        left=std::min(left,x);bottom=std::min(bottom,y);
        right=std::max(right,x+1);top=std::max(top,y+1);
    }
    if(right<=left || top<=bottom)return {};
    // PICA samples row height-1-t (see the software rasterizer). Encoded
    // row zero is the top of this map, whereas native UV v=1 is its top.
    // Convert the crop AND the resulting bounds between those conventions.
    return TextureBounds{std::max(crop.left,left/128.f),std::max(crop.bottom,1-top/128.f),
        std::min(crop.right,right/128.f),std::min(crop.top,1-bottom/128.f)};
}
struct AlphaCache {
    AlphaBlocks data{};
    TextureBounds crop{};
    std::optional<TextureBounds> bounds;
    bool initialized=false;
    std::optional<TextureBounds> Observe(const AlphaBlocks& next,TextureBounds next_crop) {
        // Addresses can be reused during area changes or loading a savestate.
        // Compare the alpha payload, including when the same model stays live.
        if(!initialized || data!=next || crop!=next_crop) {
            data=next;crop=next_crop;bounds=VisibleBounds(data,crop);initialized=true;
        }
        return bounds;
    }
};
inline thread_local AlphaCache alpha_cache;
using RgbaWords=std::array<TitleScreen::U32,8192>;
inline unsigned RgbaTexel(const RgbaWords& data,unsigned x,unsigned y) {
    const unsigned morton=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|
        ((x&4)<<2)|((y&4)<<3);
    const unsigned pixel=((y/8)*16+x/8)*64+morton;
    return (data[pixel/2]>>(16*(pixel%2)))&0xffff;
}
inline bool IsCornerMarker(const RgbaWords& data,unsigned x,unsigned y) {
    // mizusin_03_b1 contains a lone opaque red texel at (127,127), far
    // outside its panel. Such corner markers must not become map anchors.
    // Preserve all other colors and any corner texel attached to artwork.
    if((x!=0 && x!=127) || (y!=0 && y!=127) || RgbaTexel(data,x,y)!=0xf00f)return false;
    const unsigned nx=x==0?1:126,ny=y==0?1:126;
    return !(RgbaTexel(data,nx,y)&15) && !(RgbaTexel(data,x,ny)&15) &&
           !(RgbaTexel(data,nx,ny)&15);
}
struct RgbaCache {
    RgbaWords data{};
    AlphaBlocks alpha{};
    bool initialized=false;
    const AlphaBlocks& Observe(const RgbaWords& next) {
        if(initialized && data==next)return alpha;
        data=next;initialized=true;alpha.fill(0);
        // RGBA4 uses Morton order within each 8x8 tile, unlike ETC1A4's
        // four column-major 4x4 alpha blocks. Normalize only the alpha nibbles
        // so both formats use the same crop, orientation and edge detection.
        for(unsigned tile=0;tile<256;++tile)for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
            if(IsCornerMarker(data,(tile%16)*8+x,(tile/16)*8+y))continue;
            const unsigned morton=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|
                ((x&4)<<2)|((y&4)<<3);
            const unsigned pixel=tile*64+morton;
            const auto a=(data[pixel/2]>>(16*(pixel%2)))&15;
            alpha[tile*4+(y/4)*2+x/4]|=std::uint64_t(a)<<(4*(4*(x%4)+y%4));
        }
        return alpha;
    }
};
inline thread_local RgbaCache rgba_cache;
template<class M> std::optional<TextureBounds> TextureEdges(M& m,TitleScreen::U32 mesh,
                                                          const std::array<float,8>& uv) {
    const auto format=m.Read(mesh+0x164).value_or(~0u);
    if(m.Read(mesh+0x15c)!=std::optional<TitleScreen::U32>{128|(128u<<16)} ||
       (format!=13 && format!=4))return {}; // PICA ETC1A4 / RGBA4.
    const auto address=m.Read(mesh+0x158).value_or(0);
    if(!TitleScreen::Pointer(address,format==4?32768:16384))return {};
    TextureBounds crop{1,1,0,0};
    for(unsigned i=0;i<8;i+=2) {
        if(!std::isfinite(uv[i]) || !std::isfinite(uv[i+1]) ||
           uv[i]<0 || uv[i]>1 || uv[i+1]<0 || uv[i+1]>1)return {};
        crop.left=std::min(crop.left,uv[i]);crop.right=std::max(crop.right,uv[i]);
        crop.bottom=std::min(crop.bottom,uv[i+1]);crop.top=std::max(crop.top,uv[i+1]);
    }
    if(crop.right<=crop.left || crop.top<=crop.bottom)return {};
    if(format==4) {
        RgbaWords words;
        for(unsigned i=0;i<words.size();++i) {
            const auto word=m.Read(address+4*i);
            if(!word)return {};
            words[i]=*word;
        }
        return alpha_cache.Observe(rgba_cache.Observe(words),crop);
    }
    AlphaBlocks alpha;
    for(unsigned i=0;i<alpha.size();++i) {
        const auto lo=m.Read(address+16*i),hi=m.Read(address+16*i+4);
        if(!lo || !hi)return {};
        alpha[i]=*lo|(std::uint64_t(*hi)<<32);
    }
    return alpha_cache.Observe(alpha,crop);
}
// Native 2fc40c supplies UVs in the same four-vertex order as the geometry.
// Solve that mapping instead of assuming texture orientation: mirrored maps
// and cropped UV ranges must keep their own visible screen-space edge.
inline std::optional<std::array<float,2>> VisibleCorner(const std::array<float,8>& xy,
        const std::array<float,8>& uv,TextureBounds bounds) {
    const float du1=uv[2]-uv[0],dv1=uv[3]-uv[1],du2=uv[4]-uv[0],dv2=uv[5]-uv[1];
    const float det=du1*dv2-du2*dv1;
    if(!std::isfinite(det) || std::abs(det)<1e-6f)return {};
    const auto point=[&](float u,float v) {
        const float a=((u-uv[0])*dv2-(v-uv[1])*du2)/det;
        const float b=(du1*(v-uv[1])-dv1*(u-uv[0]))/det;
        return std::array{xy[0]+a*(xy[2]-xy[0])+b*(xy[4]-xy[0]),
                          xy[1]+a*(xy[3]-xy[1])+b*(xy[5]-xy[1])};
    };
    const auto fourth=point(uv[6],uv[7]);
    if(std::abs(fourth[0]-xy[6])>.01f || std::abs(fourth[1]-xy[7])>.01f)return {};
    std::array<float,2> corner{-1e6f,-1e6f};
    for(float u:{bounds.left,bounds.right})for(float v:{bounds.bottom,bounds.top}) {
        const auto p=point(u,v);
        if(!std::isfinite(p[0]) || !std::isfinite(p[1]))return {};
        corner[0]=std::max(corner[0],p[0]);corner[1]=std::max(corner[1],p[1]);
    }
    return corner;
}
}
