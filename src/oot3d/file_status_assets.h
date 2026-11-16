// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompts.h"
#include <optional>
#include <string>

namespace OOT3D::FileStatusText {
constexpr unsigned Width=256,Height=512,CellWidth=18,CellHeight=24,Columns=Width/CellWidth;
struct Assets {
    struct Glyph {unsigned code,x,y,width;};
    std::array<std::uint32_t,Width*Height> pixels{};
    std::vector<Glyph> glyphs;
    const Glyph* Find(unsigned code) const {
        for(const auto& glyph:glyphs)if(glyph.code==code)return &glyph;
        if(code!='?')return Find('?');
        return nullptr;
    }
};
inline std::shared_ptr<const Assets> assets;
inline auto GetAssets() {return std::atomic_load(&assets);}
constexpr unsigned Tiled(unsigned x,unsigned y) {
    unsigned m=0;
    for(unsigned b=0;b<3;++b)m|=((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1);
    return ((y/8)*(Width/8)+x/8)*64+m;
}
inline std::optional<Assets> MakeAssets(std::span<const std::uint8_t> font) {
    if(font.size()<16 || font[0]!='Q' || font[1]!='B' || font[2]!='F' || font[3]!='1' ||
       font[12]!=4 || font[13]!=16 || font[14]!=16)return {};
    const unsigned count=font[4]|unsigned(font[5])<<8,sheets=font[6]|unsigned(font[7])<<8;
    const unsigned bitmap=16+count*8;
    if(!count || count>Columns*(Height/CellHeight) || !sheets || font.size()!=bitmap+sheets*128)return {};
    Assets out;
    std::vector<std::uint8_t> alpha(Width*Height);
    for(unsigned i=0;i<count;++i) {
        const unsigned at=16+i*8,code=font[at]|unsigned(font[at+1])<<8;
        const unsigned sheet=font[at+2]|unsigned(font[at+3])<<8,left=font[at+4],width=font[at+5];
        if(sheet>=sheets || left+width>16)return {};
        if(!width)continue;
        const unsigned x=(i%Columns)*CellWidth,y=(i/Columns)*CellHeight;
        out.glyphs.push_back({code,x,y,width});
        for(unsigned dy=0;dy<16;++dy)for(unsigned dx=0;dx<width;++dx) {
            const unsigned pixel=dy*16+left+dx;
            alpha[(y+2+dy)*Width+x+1+dx]=((font[bitmap+sheet*128+pixel/2]>>(pixel%2?0:4))&15)*17;
        }
    }
    if(!out.Find('?'))return {};
    for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x) {
        const unsigned a=alpha[y*Width+x],shadow=(x && y)?alpha[(y-1)*Width+x-1]*4/5:0;
        const unsigned combined=a+shadow*(255-a)/255,white=combined?a*255/combined:0;
        out.pixels[Tiled(x,Height-1-y)]=(white<<24)|(white<<16)|(white<<8)|combined;
    }
    return out;
}
// Seeds may contain accented characters. Consume each UTF-8 codepoint once;
// unsupported characters use the native question-mark glyph, never raw bytes.
inline std::vector<unsigned> Codepoints(std::string_view value) {
    std::vector<unsigned> out;
    for(std::size_t i=0;i<value.size() && out.size()<144;) {
        unsigned c=static_cast<unsigned char>(value[i++]);
        if(c>=128) {
            const unsigned extra=c>=0xf0 && c<=0xf4?3:c>=0xe0 && c<0xf0?2:c>=0xc2 && c<0xe0?1:0;
            unsigned code=extra?c&((1u<<(6-extra))-1):'?';
            bool valid=extra && i+extra<=value.size();
            for(unsigned j=0;valid && j<extra;++j) {
                const unsigned next=static_cast<unsigned char>(value[i+j]);
                if((next&0xc0)!=0x80)valid=false;else code=(code<<6)|(next&63);
            }
            if(valid) {i+=extra;c=code;}else c='?';
        }
        out.push_back(c<32?' ':c);
    }
    return out;
}
inline float Measure(const Assets& font,std::string_view value,float scale=.75f) {
    float width=0;
    for(auto c:Codepoints(value))if(const auto glyph=font.Find(c))width+=(glyph->width+1)*scale;
    return width;
}
using Geometry=PausePrompts::QuadGeometry<288>; // Seed text plus three outlined file badges.
inline Geometry Text(const Assets& font,std::string_view value,float x,float bottom,float max_width,float scale=.75f) {
    const auto codes=Codepoints(value);
    unsigned lines=1;float pen=0;
    for(auto c:codes)if(const auto glyph=font.Find(c)) {
        const float advance=(glyph->width+1)*scale;
        if(pen && pen+advance>max_width) {++lines;pen=0;}
        pen+=advance;
    }
    Geometry out;pen=0;float y=bottom-15*scale/ .75f-(lines-1)*18*scale;
    for(auto c:codes)if(const auto glyph=font.Find(c)) {
        const float advance=(glyph->width+1)*scale;
        if(pen && pen+advance>max_width) {pen=0;y+=18*scale;}
        out.Add(x+pen,y,(glyph->width+2)*scale,20*scale,
            float(glyph->x)/Width,(glyph->y+.5f)/Height,
            float(glyph->x+glyph->width+2)/Width,(glyph->y+19.5f)/Height);
        pen+=advance;
    }
    return out;
}
}
