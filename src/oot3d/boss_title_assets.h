// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/shop_hint_assets.h"
#include "oot3d/ui_language.h"

namespace OOT3D::BossTitle {
constexpr unsigned Width=256,Height=512;
using Pixels=std::array<std::uint32_t,Width*Height>;
inline std::shared_ptr<const Pixels> pixels;
inline auto GetPixels() {return std::atomic_load(&pixels);}
constexpr std::array<std::u16string_view,8> Archives{u"zelda_goma.zar",u"zelda_kdodongo.zar",
    u"zelda_bv.zar",u"zelda_fantomHG.zar",u"zelda_fd.zar",u"zelda_mo.zar",u"zelda_sst.zar",u"zelda_tw.zar"};
constexpr std::array<std::string_view,8> Names{"tex/g_boss_name1_usen.ctxb","tex/g_boss_name2_usen.ctxb",
    "tex/g_boss_name3_usen.ctxb","tex/g_boss_name4_usen.ctxb","tex/g_boss_name5_usen.ctxb",
    "tex/g_boss_name6_usen.ctxb","tex/g_boss_name8_usen.ctxb","tex/g_boss_name7_usen.ctxb"};
// Keep every title in one immutable atlas: changing selection changes UVs only.
inline bool LoadTitle(Pixels& out,unsigned index,std::span<const std::uint8_t> archive,unsigned language=1) {
    using MapFrame::LE32;
    if(index>=Names.size() || archive.size()<24 || LE32(archive,0)!=0x0152415a)return false;
    const unsigned count=archive[10]|unsigned(archive[11])<<8;
    const auto files=LE32(archive,16),offsets=LE32(archive,20);
    if(!count || count>4096 || files>archive.size() || offsets>archive.size() ||
       count*8>archive.size()-files || count*4>archive.size()-offsets)return false;
    for(unsigned i=0;i<count;++i) {
        auto name=std::string(Names[index]);name.replace(name.size()-9,4,UILanguage::Suffix(language));const auto text=LE32(archive,files+i*8+4);
        if(text>=archive.size() || name.size()+1>archive.size()-text || archive[text+name.size()] ||
           !std::equal(name.begin(),name.end(),archive.begin()+text))continue;
        const auto size=LE32(archive,files+i*8),at=LE32(archive,offsets+i*4);
        if(at>archive.size() || size>archive.size()-at || size!=72+256*64*2)return false;
        const auto source=archive.subspan(at,size);
        if(LE32(source,0)!=0x62787463 || LE32(source,4)!=size || LE32(source,8)!=1 ||
           LE32(source,20)!=72 || LE32(source,36)!=256*64*2 ||
           LE32(source,44)!=(256|(64<<16)) || LE32(source,48)!=0x14016758)return false;
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<256;++x) {
            const auto p=72+ShopHint::Tiled(x,y,256)*2;const unsigned a=source[p],l=source[p+1];
            out[ShopHint::Tiled(x,index*64+y,Width)]=(l<<24)|(l<<16)|(l<<8)|a;
        }
        return true;
    }
    return false;
}
}
