// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_extension.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <map>
using namespace OOT3D;
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Write(U32 a,U32 v) {words.at(a)=v;}
    void F(U32 a,float v) {words[a]=std::bit_cast<U32>(v);}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
int main() {
    Memory m;constexpr U32 play=0x8100000,model=0x8200000,mesh=0x8300000,
        helper=0x8400000,src=0x8500000,dst=0x8600000,uvs=0x8700000,world=0x506cb0;
    m.words[UI+0xc]=play;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;
    m.words[play+0x100]=3;m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=0;
    m.words[world+0x38]=2;m.words[0x4fda6c+0x50]=0;
    m.words[world]=mesh;m.words[world+4]=model;m.words[world+0x10]=helper;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[helper]=30;
    m.words[helper+0x14]=uvs;m.words[helper+0x18]=src;
    for(unsigned q=0;q<30;++q) {
        const std::array<float,8> uv=q==0?std::array<float,8>{0,1,.625f,1,0,.0625f,.625f,.0625f}:
            std::array<float,8>{.75f,.5f,.8f,.5f,.75f,.55f,.8f,.55f};
        for(unsigned i=0;i<8;++i)m.F(uvs+q*32+4*i,uv[i]);
        for(unsigned i=0;i<16;++i) {m.F(src+q*64+4*i,i%4==3?.4f:.8f);m.F(dst+q*64+4*i,0);}
    }
    Layout layout;layout.map_extension={true,80,18,0};
    for(float darkness:{0.f,.25f,.75f,1.f}) {
        layout.map_darkness=darkness;SetLayout(layout,false);
        const auto before=m.words;
        assert(CopyWorldMapColors(m,mesh,dst,src,30*64)==(darkness!=0));
        if(darkness==0) {assert(m.words==before);continue;}
        for(unsigned q=0;q<30;++q)for(unsigned i=0;i<16;++i) {
            const auto a=src+q*64+4*i;
            assert(m.words.at(a)==before.at(a));
            const float expected=i%4==3?.4f:.8f*(q==0?1-darkness:1);
            assert(std::abs(m.F(dst+q*64+4*i)-expected)<.00001f);
        }
        const auto once=m.words;assert(CopyWorldMapColors(m,mesh,dst,src,30*64));
        assert(m.words==once); // Upload always derives from the fresh source.
        const auto geometry=OOT3D::MapExtension::Make({0,0,320,240},layout.map_extension,.4f,darkness);
        for(unsigned i=0;i<32;++i)assert(geometry.colors[i]==(i%4==3?.4f:1-darkness));
    }
    layout.map_darkness=0;SetLayout(layout,false);world_location_alpha=.25f;
    assert(CopyWorldMapColors(m,mesh,dst,src,30*64));
    for(unsigned q=0;q<30;++q)for(unsigned i=0;i<16;++i) {
        const float expected=i%4==3?((q>=12 && q<=17)?.1f:.4f):.8f;
        assert(std::abs(m.F(dst+q*64+4*i)-expected)<.00001f);
    }
    world_location_alpha=1;
    // Clock tint affects only the artwork RGB, combines with manual darkness,
    // and always derives from the untinted source rather than accumulating.
    layout.world_map_time_tint=true;layout.map_darkness=.25f;SetLayout(layout,false);
    m.words[0x4596a0]=0x587958;m.words[0x4594e4]=0xe1d600bc;
    for(U32 time:{0u,0x3000u,0x4000u,0x8000u,0xb555u,0xc555u,0xffffu}) {
        m.words[0x587964]=0x01010000|time; // Unrelated save flags must not affect time.
        assert(CopyWorldMapColors(m,mesh,dst,src,30*64));
        const auto tint=MapTimeTint::At(std::uint16_t(time));
        for(unsigned q=0;q<30;++q)for(unsigned i=0;i<16;++i) {
            const float expected=i%4==3?.4f:.8f*(q==0?.75f*tint[i%4]:1);
            assert(std::abs(m.F(dst+q*64+4*i)-expected)<.00001f);
            assert(m.F(src+q*64+4*i)==(i%4==3?.4f:.8f));
        }
        const auto once=m.words;assert(CopyWorldMapColors(m,mesh,dst,src,30*64));assert(m.words==once);
    }
    assert(MapTimeTint::At(0x8000)==MapTimeTint::Neutral);
    const auto sunset=MapTimeTint::At(0xb555),night=MapTimeTint::At(0);
    assert(sunset[0]>sunset[1] && sunset[1]>sunset[2]);
    assert(night[0]<night[2] && night[2]<1);
    // No discontinuity at midnight or any interval boundary.
    assert(MapTimeTint::At(0)==MapTimeTint::At(0xffff));
    for(unsigned t=1;t<65536;++t)for(unsigned c=0;c<3;++c)
        assert(std::abs(MapTimeTint::At(t)[c]-MapTimeTint::At(t-1)[c])<.001f);
    layout.map_darkness=0;SetLayout(layout,false);
    m.words.erase(0x587964);assert(!CopyWorldMapColors(m,mesh,dst,src,30*64));
    m.words[0x587964]=0;m.words[0x4594e4]^=1;
    assert(!CopyWorldMapColors(m,mesh,dst,src,30*64));
    m.words[0x4594e4]^=1;
    layout.world_map_time_tint=false;SetLayout(layout,false);
    const auto disabled=m.words;assert(!CopyWorldMapColors(m,mesh,dst,src,30*64));assert(m.words==disabled);
    layout.world_map_time_tint=true;SetLayout(layout,false);
    const auto before=m.words;
    m.words[0x4fda6c+0x50]=1;assert(!CopyWorldMapColors(m,mesh,dst,src,30*64));
    m.words=before;m.words[UI+0x24]=0;assert(!CopyWorldMapColors(m,mesh,dst,src,30*64));
    m.words=before;assert(!CopyWorldMapColors(m,mesh+4,dst,src,30*64));
    assert(MapBrightness(std::numeric_limits<float>::quiet_NaN())==1);
    std::cout<<"Map darkness: parchment and extensions match; markers, labels, alpha and source preserved; no accumulation or dungeon/gameplay leakage\n";
}
