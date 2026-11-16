// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_frame_asset.h"
#include <optional>
#include "oot3d/ui_language.h"

namespace OOT3D::DungeonLocation {
constexpr unsigned Width=256,Height=512;
using PaperPixels=std::array<std::uint32_t,256*256/4>; // Native ETC1A4.
using TitlePixels=std::array<std::uint32_t,Width*Height>;
struct Assets {PaperPixels paper{};TitlePixels titles{};unsigned available=0;};
inline std::shared_ptr<const Assets> assets;
inline auto GetAssets() {return std::atomic_load(&assets);}
constexpr std::array<std::u16string_view,12> Archives{u"ydan.zar",u"ddan.zar",u"bdan.zar",u"bmori1.zar",
    u"hidan.zar",u"mizusin.zar",u"jyasinzou.zar",u"hakadan.zar",u"hakadan_ch.zar",
    u"ice_doukutu.zar",u"men.zar",u"ganontika.zar"};
constexpr std::array<std::string_view,12> Titles{"dg_deku","dg_dod","dg_jab2","dg_forest","dg_fire",
    "dg_water","dg_soul","dg_dark","dg_idosoko","dg_ice","dg_gerudo","dg_ganon"};
constexpr unsigned Tiled(unsigned x,unsigned y,unsigned width=Width) {
    unsigned m=0;for(unsigned b=0;b<3;++b)m|=((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1);
    return ((y/8)*(width/8)+x/8)*64+m;
}
inline std::optional<unsigned> TitleIndex(unsigned scene) {
    if(scene<10)return scene;
    if(scene==11)return 10;
    if(scene==10 || scene==13 || scene==14 || scene==15)return 11;
    if(scene>=17 && scene<=22)return scene-17;
    if(scene==23)return 7; // Bongo Bongo precedes Twinrova in the scene table.
    if(scene==24)return 6;
    return {};
}
inline bool LoadPaper(Assets& out,std::span<const std::uint8_t> b) {
    using MapFrame::LE32;
    if(b.size()!=72+256*256 || LE32(b,0)!=0x62787463 || LE32(b,4)!=b.size() ||
       LE32(b,8)!=1 || LE32(b,20)!=72 || LE32(b,44)!=(256|(256<<16)) || LE32(b,48)!=0x675b)return false;
    for(unsigned i=0;i<out.paper.size();++i)out.paper[i]=LE32(b,72+i*4);
    return true;
}
inline bool LoadTitle(Assets& out,unsigned index,std::span<const std::uint8_t> b,unsigned language=1) {
    using MapFrame::LE32;
    if(index>=Titles.size() || b.size()<24 || LE32(b,0)!=0x0152415a)return false;
    const unsigned count=b[10]|unsigned(b[11])<<8;
    const auto files=LE32(b,16),offsets=LE32(b,20);
    if(!count || count>4096 || files>b.size() || offsets>b.size() ||
       count*8>b.size()-files || count*4>b.size()-offsets)return false;
    const std::string name=".\\name\\"+std::string(Titles[index])+"_"+std::string(UILanguage::Suffix(language))+".ctxb";
    for(unsigned i=0;i<count;++i) {
        const auto text=LE32(b,files+i*8+4);
        if(text>=b.size() || name.size()+1>b.size()-text || b[text+name.size()] ||
           !std::equal(name.begin(),name.end(),b.begin()+text))continue;
        const auto size=LE32(b,files+i*8),at=LE32(b,offsets+i*4);
        if(at>b.size() || size>b.size()-at || size!=72+256*32*2)return false;
        const auto source=b.subspan(at,size);
        if(LE32(source,0)!=0x62787463 || LE32(source,4)!=size || LE32(source,8)!=1 ||
           LE32(source,20)!=72 || LE32(source,36)!=256*32*2 ||
           LE32(source,44)!=(256|(32<<16)) || LE32(source,48)!=0x14016758)return false;
        for(unsigned y=0;y<32;++y)for(unsigned x=0;x<256;++x) {
            const auto p=72+Tiled(x,y)*2;const unsigned a=source[p],l=source[p+1];
            out.titles[Tiled(x,index*32+y)]=(l<<24)|(l<<16)|(l<<8)|a;
        }
        out.available|=1u<<index;return true;
    }
    return false;
}
}
