// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pickup_fog_hooks.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using U32=std::uint32_t;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a){auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Write(U32 a,U32 v){words[a]=v;}
};
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);assert(f);
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(f),{}};
    const auto word=[&](size_t a){assert(a+4<=bytes.size());U32 v=0;for(unsigned i=0;i<4;++i)v|=U32(bytes[a+i])<<(8*i);return v;};
    constexpr U32 cmb=0x8000000,renderer=0x8010000,skl=0x8020000,wrapper=0x8030000,play=0x8040000;
    unsigned models=0,primitives=0,torches=0;
    for(size_t a=0;a+32<bytes.size();a+=4) {
        if(word(a)!=0x20626d63)continue;
        const std::string name(reinterpret_cast<const char*>(&bytes[a+16]),16);
        const bool torch=name.starts_with("torch4_modelT");
        if(!torch && !name.starts_with("drop_gi_"))continue;
        const unsigned size=word(a+4);assert(size && a+size<=bytes.size());
        Memory m;for(unsigned i=0;i<size;i+=4)m.Write(cmb+i,word(a+i));
        m.Write(renderer,0x4ebd98);m.Write(renderer+8,skl);m.Write(skl+4,wrapper);m.Write(wrapper,cmb+0x44);
        m.Write(0x5043e0,play);m.Write(0x5be5b8+0xfc,play+0x188);m.Write(play+0x104,0x55);
        for(unsigned i=0;i<4;++i)m.Write(play+0x188+i*4,std::array<U32,4>{0,400,0,240}[i]);
        for(unsigned b=0;b<10;++b)for(unsigned i=0;i<12;++i)
            m.Write(renderer+0x90+48*b+4*i,std::bit_cast<U32>(i==3?float(70+b):float(i==0||i==5||i==10)));
        std::array<U32,16> r{};r[6]=renderer;
        for(unsigned off=0;off+24<size;off+=4) {
            if(m.Read(cmb+off)!=0x736d7270)continue;
            r[11]=cmb+off;PickupFog::draws.clear();Interpolation::guest_frames=80;Interpolation::generation=7;
            const auto before=m.words;
            PickupFog::Capture(m,r,1000000);assert(m.words==before && PickupFog::draws.size()==1);
            auto d=PickupFog::draws.back();assert(d.count && d.count<=10 && d.indices);
            assert(PickupFog::Match(d.palette,d.indices,80,7,0x55,1000000)==(torch?PickupFog::Torch:PickupFog::Surface));
            assert(PickupFog::Match(d.palette,d.indices,81,7,0x55,1033333));
            assert(!PickupFog::Match(d.palette,d.indices+1,80,7,0x55,1000000));
            assert(!PickupFog::Match(d.palette,d.indices,82,7,0x55,1066666));
            assert(!PickupFog::Match(d.palette,d.indices,80,8,0x55,1000000));
            assert(!PickupFog::Match(d.palette,d.indices,80,7,0x56,1000000));
            assert(!PickupFog::Match(d.palette,d.indices,80,7,0x55,999999));
            assert(!PickupFog::Match(d.palette,d.indices,80,7,0x55,1100001));
            d.palette[d.count-1][3]+=1;assert(!PickupFog::Match(d.palette,d.indices,80,7,0x55,1000000));
            PickupFog::draws.clear();m.Write(0x5be5b8+0xfc,0);PickupFog::Capture(m,r,1000000);assert(PickupFog::draws.empty());
            m.Write(0x5be5b8+0xfc,play+0x188);m.Write(cmb+0x10,0);PickupFog::Capture(m,r,1000000);assert(PickupFog::draws.empty());
            m.words=before;++primitives;
        }
        if(torch)++torches;else ++models;
    }
    assert((models==15 && torches==2) || (models==0 && torches==1));
    assert(primitives>=models+torches);
    std::cout<<models<<" collectible and "<<torches<<" torch CMBs / "<<primitives<<" primitives: ownership, full palette, world view, freshness and isolation passed\n";
}
