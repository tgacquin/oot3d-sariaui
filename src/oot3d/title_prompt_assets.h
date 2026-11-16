// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include "oot3d/ui_language.h"

namespace OOT3D::TitlePrompt {
constexpr unsigned Width=256,Height=32;
constexpr std::string_view Text="PRESS START";
inline constexpr std::array<std::string_view,3> Labels{"PRESS START","APPUYEZ SUR START","PULSA START"};
struct Assets {
    std::array<std::uint32_t,Width*Height> pixels{};
    struct Glyph {unsigned x=0,width=0;};
    std::array<Glyph,24> glyphs{};
    unsigned width=0,native_spacing=2,length=0;
};
inline Common::AtomicSharedPtr<const Assets> assets;
inline auto GetAssets() {return assets.load();}
constexpr unsigned Tiled(unsigned x,unsigned y) {
    unsigned m=0;
    for(unsigned b=0;b<3;++b)m|=((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1);
    return ((y/8)*(Width/8)+x/8)*64+m;
}
inline std::optional<Assets> MakeAssets(std::span<const std::uint8_t> font,unsigned language=1) {
    if(font.size()<16 || font[0]!='Q' || font[1]!='B' || font[2]!='F' || font[3]!='1' ||
       font[12]!=4 || font[13]!=16 || font[14]!=16 || font[15]>8)return {};
    const unsigned count=font[4]|unsigned(font[5])<<8,sheets=font[6]|unsigned(font[7])<<8;
    const unsigned bitmap=16+count*8;
    if(!count || count>4096 || !sheets || sheets>4096 || font.size()!=bitmap+sheets*128)return {};
    Assets out;
    std::array<std::uint8_t,Width*20> alpha{};
    unsigned pen=2,index=0;
    out.native_spacing=font[15];
    const auto label=Labels[UILanguage::Index(language)];
    out.length=label.size();
    for(char c:label) {
        unsigned record=16;
        for(;record<bitmap;record+=8)
            if((font[record]|unsigned(font[record+1])<<8)==unsigned(c))break;
        if(record==bitmap)return {};
        const unsigned glyph=font[record+2]|unsigned(font[record+3])<<8;
        const unsigned left=font[record+4],width=font[record+5];
        if(glyph>=sheets || !width || left+width>16 || pen+width+2>Width)return {};
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*16+left+x;
            alpha[(y+2)*Width+pen+x]=((font[bitmap+glyph*128+pixel/2]>>(pixel%2?0:4))&15)*17;
        }
        out.glyphs[index++]={pen,width};
        pen+=width+font[15];
    }
    out.width=pen-font[15]+2;
    // Same glyph spacing and one-pixel shadow as the existing textbox-font prompts.
    for(unsigned y=0;y<20;++y)for(unsigned x=0;x<out.width;++x) {
        const unsigned a=alpha[y*Width+x];
        const unsigned shadow=(x && y)?alpha[(y-1)*Width+x-1]*4/5:0;
        const unsigned combined=a+shadow*(255-a)/255;
        const unsigned white=combined?a*255/combined:0;
        out.pixels[Tiled(x,Height-1-y)]=(white<<24)|(white<<16)|(white<<8)|combined;
    }
    return out;
}
}
