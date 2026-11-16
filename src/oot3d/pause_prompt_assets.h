// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompt_layout.h"
#include "oot3d/map_frame_asset.h"
#include <optional>
#include <string_view>

namespace OOT3D::PausePrompts {
constexpr unsigned Tiled(unsigned x,unsigned y) {
    unsigned m=0;
    for(unsigned b=0;b<3;++b)m|=((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1);
    return ((y/8)*(TextWidth/8)+x/8)*64+m;
}
inline std::optional<Assets> MakeAssets(std::span<const std::uint8_t> atlas,
                                      std::span<const std::uint8_t> font,
                                      std::span<const std::uint8_t> cursor={},unsigned language=1) {
    using MapFrame::LE32;
    constexpr unsigned bytes=IconWidth*IconHeight*2;
    if(atlas.size()!=72+bytes || LE32(atlas,0)!=0x62787463 ||
       LE32(atlas,4)!=atlas.size() || LE32(atlas,8)!=1 || LE32(atlas,16)!=24 ||
       LE32(atlas,20)!=72 || LE32(atlas,24)!=0x20786574 || LE32(atlas,28)!=48 ||
       LE32(atlas,32)!=1 || LE32(atlas,36)!=bytes ||
       LE32(atlas,44)!=(IconWidth|(IconHeight<<16)) ||
       LE32(atlas,48)!=0x80336752 || LE32(atlas,52)!=0)return {};
    // QBF1: 16-byte header, 8-byte codepoint/glyph/crop records, then
    // 16x16 four-bit glyphs. Pixels are row-major, high nibble first.
    if(font.size()<16 || LE32(font,0)!=0x31464251 ||
       font[12]!=4 || font[13]!=16 || font[14]!=16 || font[15]>8)return {};
    const unsigned count=font[4]|unsigned(font[5])<<8,sheets=font[6]|unsigned(font[7])<<8;
    const unsigned bitmap=16+count*8;
    if(!count || count>4096 || !sheets || sheets>4096 ||
       font.size()!=bitmap+sheets*128)return {};
    Assets out{};
    out.native_spacing=font[15];
    if(!cursor.empty()) {
        if(cursor.size()!=72+128*128*2 || LE32(cursor,0)!=0x62787463 ||
           LE32(cursor,4)!=cursor.size() || LE32(cursor,8)!=1 || LE32(cursor,20)!=72 ||
           LE32(cursor,44)!=(128|(128<<16)) || LE32(cursor,48)!=0x80336752)return {};
        for(unsigned i=0;i<out.cursor.size();++i)out.cursor[i]=LE32(cursor,72+i*4);
        out.cursor_ready=true;
    }
    for(unsigned i=0;i<out.icons.size();++i)out.icons[i]=LE32(atlas,72+i*4);
    for(unsigned row=0;row<Text.size();++row) {
        const auto label=LocalizedText[UILanguage::Index(language)][row];
        out.lengths[row]=label.size();
        std::array<std::uint8_t,TextWidth*20> alpha{};
        unsigned pen=2,index=0;
        for(char32_t c:label) {
            unsigned record=16;
            for(;record<bitmap;record+=8)
                if((font[record]|unsigned(font[record+1])<<8)==unsigned(c))break;
            if(record==bitmap)return {};
            const unsigned glyph=font[record+2]|unsigned(font[record+3])<<8;
            const unsigned left=font[record+4],width=font[record+5];
            if(index>=out.glyphs[row].size() || glyph>=sheets || !width || left+width>16 ||
               pen+width+2>TextWidth)return {};
            out.glyphs[row][index++]={pen,width,unsigned(c)};
            for(unsigned y=0;y<16;++y)for(unsigned x=0;x<width;++x) {
                const unsigned pixel=y*16+left+x;
                const unsigned value=(font[bitmap+glyph*128+pixel/2]>>(pixel%2?0:4))&15;
                alpha[(y+2)*TextWidth+pen+x]=value*17;
            }
            pen+=width+font[15];
        }
        out.widths[row]=pen-font[15]+2;
        // Native textbox glyphs with a one-pixel down/right drop shadow.
        for(unsigned y=0;y<20;++y)for(unsigned x=0;x<out.widths[row];++x) {
            const unsigned a=alpha[y*TextWidth+x];
            const unsigned shadow=(x && y)?alpha[(y-1)*TextWidth+x-1]*4/5:0;
            const unsigned combined=a+shadow*(255-a)/255;
            const unsigned white=combined?a*255/combined:0;
            // Match the native UI material's vertical texture flip, as with
            // the extracted map frame. UVs then address labels top to bottom.
            out.text[Tiled(x,TextHeight-1-(row*TextRowHeight+y))]=
                (white<<24)|(white<<16)|(white<<8)|combined;
        }
    }
    return out;
}
}
