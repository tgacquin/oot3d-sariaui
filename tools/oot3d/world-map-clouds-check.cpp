// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/world_map_clouds.h"
#include "video_core/texture/etc1.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
namespace W=OOT3D::WorldMap;
using TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    template<class T> void Floats(U32 a,const T& v) {for(unsigned i=0;i<v.size();++i)words[a+4*i]=std::bit_cast<U32>(v[i]);}
};
int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream in(argv[1],std::ios::binary);assert(in);
    const std::vector<unsigned char> data{std::istreambuf_iterator<char>(in),{}};
    assert(data.size()==65608 && MapFrame::LE32(data,44)==(256|(256u<<16)) && MapFrame::LE32(data,48)==0x675b);
    const auto decode=[](std::uint64_t bits,unsigned x,unsigned y) {
        const auto c=Pica::Texture::SampleETC1Subtile(bits,x,y);return std::array{c.r(),c.g(),c.b()};
    };
    constexpr U32 world=0x506cb0,map_model=0x8100000,map_mesh=0x8110000,map_desc=0x8120000,
        map_helper=0x8130000,map_live=0x14900000,owner=0x8140000,model=0x8150000,
        mesh=0x8160000,helper=0x8170000,desc=0x8180000,live=0x14a00000,texture=0x14b00000;
    Memory m;
    m.words[0x5066f8+0x34]=m.words[0x50446c+0x18]=0;m.words[world+0x38]=2;m.words[0x4fda6c+0x50]=0;
    m.words[world]=map_mesh;m.words[world+4]=map_model;m.words[world+0x10]=map_helper;
    m.words[map_helper]=30;m.words[map_model]=0x4ebd60;m.words[map_model+4]=map_mesh;
    m.words[map_mesh]=map_desc;m.words[map_desc+12]=120;m.words[map_mesh+0x124]=0;m.words[map_mesh+0x1a0]=map_live;
    const std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
    m.Floats(map_model+0xc,identity);m.Floats(model+0xc,identity);
    const std::array<float,12> positions{0,0,0,320,0,0,0,240,0,320,240,0};
    m.Floats(map_live,positions);
    for(unsigned q=0;q<30;++q)
        m.Floats(map_live+120*12+q*32,q?std::array<float,8>{.8f,.9f,.9f,.9f,.8f,.8f,.9f,.8f}:
            std::array<float,8>{0,1,320.f/512,1,0,16.f/256,320.f/512,16.f/256});
    m.words[world+0x28]=owner;m.words[owner]=mesh;m.words[owner+4]=model;m.words[owner+8]=helper;
    m.words[helper]=16;m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[mesh]=desc;
    m.words[desc+12]=64;m.words[desc+0x1c]=0x1e4;m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=live;m.words[mesh+0x158]=texture;
    m.words[mesh+0x15c]=256|(256u<<16);m.words[mesh+0x164]=13;
    std::array<float,16> color;color.fill(1);
    const std::array<float,8> uv{0,1,1,1,0,0,1,0};
    for(unsigned q=0;q<16;++q) {
        m.Floats(live+q*48,q?std::array<float,12>{}:positions);
        m.Floats(live+64*12+q*32,uv);m.Floats(live+64*20+q*64,color);
    }
    for(unsigned i=0;i<65536;i+=4)m.words[texture+i]=MapFrame::LE32(data,i+72);
    PauseMenu::Layout layout;layout.world_map_time_tint=true;PauseMenu::SetLayout(layout,false);
    W::Clouds clouds;const auto before=m.words;
    assert(clouds.Prepare(m,model,decode) && before==m.words);
    const auto original=clouds.composed;
    assert(clouds.Prepare(m,model,decode) && clouds.composed==original);
    // Actual native cloud descriptor: constant color, position/UV streams only.
    // Unrelated heap data immediately after the buffer must never be sampled.
    m.words[desc+0x1c]=0x1ec;
    for(unsigned i=0;i<64*16;i+=4)m.words.erase(live+64*20+i);
    assert(clouds.Prepare(m,model,decode) && clouds.composed==original);
    for(unsigned i=0;i<64*16;i+=4)m.words[live+64*20+i]=0x7fc00000; // Poison, not colors.
    assert(clouds.Prepare(m,model,decode) && clouds.composed==original);
    auto depth=positions;for(unsigned i=0;i<4;++i)depth[3*i+2]=.5f;
    m.Floats(live,depth);assert(clouds.Prepare(m,model,decode) && clouds.composed==original);
    assert(clouds.prepared.positions[2]==.5f);m.Floats(live,positions);
    m.words[desc+0x1c]=0x1e4;
    for(unsigned q=0;q<16;++q)m.Floats(live+64*20+q*64,color);
    // A collapsed native cloud stays hidden, and native vertices/UVs/flags are never written.
    m.Floats(live,std::array<float,12>{});assert(clouds.Prepare(m,model,decode));
    for(const auto p:*clouds.composed)assert((p&255)==0);
    m.Floats(live,positions);assert(clouds.Prepare(m,model,decode) && *clouds.composed==*original);
    // Changing discovery alpha while the draw remains active invalidates the cache.
    auto invisible=color;for(unsigned i=0;i<4;++i)invisible[4*i+3]=0;
    m.Floats(live+64*20,invisible);assert(clouds.Prepare(m,model,decode));
    for(const auto p:*clouds.composed)assert((p&255)==0);
    m.Floats(live+64*20,color);assert(clouds.Prepare(m,model,decode));
    const auto unfaded=clouds.composed;
    auto faded=color;for(unsigned i=0;i<4;++i)faded[4*i+3]=.4f;
    for(unsigned q=0;q<16;++q)m.Floats(live+64*20+q*64,faded);
    assert(clouds.Prepare(m,model,decode) && clouds.composed==unfaded && clouds.prepared.colors[3]==.4f);
    // A moved/scaled map and its clouds keep the exact same texture-space mask/cache.
    auto moved=positions;
    for(unsigned i=0;i<4;++i){moved[3*i]=moved[3*i]*1.5f+17;moved[3*i+1]=moved[3*i+1]*1.5f-8;}
    m.Floats(map_live,moved);m.Floats(live,moved);
    assert(clouds.Prepare(m,model,decode) && clouds.composed==unfaded);
    assert(clouds.prepared.positions[0]==17 && clouds.prepared.positions[7]==352);
    m.Floats(map_live,positions);m.Floats(live,positions);
    // Different model translations must not shift the mask relative to the map.
    auto translated=identity;translated[3]=12;translated[7]=-6;
    m.Floats(model+0xc,translated);moved=positions;
    for(unsigned i=0;i<4;++i){moved[3*i]-=12;moved[3*i+1]+=6;}
    m.Floats(live,moved);assert(clouds.Prepare(m,model,decode) && clouds.composed==unfaded);
    assert(clouds.prepared.positions[0]==-12 && clouds.prepared.positions[1]==6);
    m.Floats(model+0xc,identity);m.Floats(live,positions);
    // Tint both night and sunset exactly once. Preserve cloud alpha.
    m.words[0x4596a0]=0x587958;m.words[0x4594e4]=0xe1d600bc;
    const auto artwork_builds=clouds.lighting.rebuilds;
    for(U32 time:{0u,0xb555u}) {
        m.words[0x587964]=time;const auto tint=MapTimeTint::At(time);
        assert(clouds.Prepare(m,model,decode));
        assert(clouds.lighting.rebuilds==artwork_builds); // Discovery/outline raster is retained across clock changes.
        for(unsigned i=0;i<original->size();++i) {
            assert(((*clouds.composed)[i]&255)==((*original)[i]&255));
            for(unsigned c=0;c<3;++c) {
                const float expected=float(((*original)[i]>>(24-8*c))&255)*tint[c];
                assert(std::abs(float(((*clouds.composed)[i]>>(24-8*c))&255)-expected)<=1);
            }
        }
    }
    layout.world_map_time_tint=false;layout.map_darkness=.35f;PauseMenu::SetLayout(layout,false);
    assert(clouds.Prepare(m,model,decode));
    const float brightness=PauseMenu::MapBrightness(layout.map_darkness);
    for(unsigned i=0;i<original->size();++i)for(unsigned c=0;c<3;++c)
        assert(std::abs(float(((*clouds.composed)[i]>>(24-8*c))&255)-float(((*original)[i]>>(24-8*c))&255)*brightness)<=1);
    // Saved concave/rounded outline and inward feather clip cloud alpha too.
    W::Outline outline;outline.cut.hole={{30,20},{290,20},{290,100},{180,100},{180,220},{30,220}};
    outline.feather=12;outline.rounding=8;
    std::atomic_store(&W::outline,std::shared_ptr<const W::Outline>(std::make_shared<W::Outline>(outline)));
    assert(clouds.Prepare(m,model,decode));
    const auto ordinary_mask=clouds.composed;
    m.Floats(map_live+120*12,std::array<float,8>{320.f/512,1,0,1,320.f/512,16.f/256,0,16.f/256});
    const auto mirrored_native=m.words;
    assert(clouds.Prepare(m,model,decode) && m.words==mirrored_native);
    assert(*clouds.composed!=*ordinary_mask);
    const auto mirrored_mask=clouds.composed;
    assert(clouds.Prepare(m,model,decode) && clouds.composed==mirrored_mask);
    m.Floats(map_live+120*12,std::array<float,8>{0,1,320.f/512,1,0,16.f/256,320.f/512,16.f/256});
    assert(clouds.Prepare(m,model,decode) && *clouds.composed==*ordinary_mask);
    const auto boundary=W::Boundary(outline);
    unsigned removed=0,retained=0,feathered=0;
    for(unsigned y=0;y<DungeonParchment::Height;++y)for(unsigned x=0;x<DungeonParchment::Width;++x) {
        const auto at=DungeonParchment::TiledOffset(x,DungeonParchment::Height-1-y);
        const float mask=W::Alpha(boundary,outline.feather,(x+.5f)*320/DungeonParchment::Width,(y+.5f)*240/DungeonParchment::Height);
        const auto a=(*clouds.composed)[at]&255,old=(*original)[at]&255;
        assert(std::abs(float(a)-old*mask)<=1);
        removed+=old>0 && a==0;retained+=a>0 && mask==1;feathered+=a>0 && a<old;
    }
    assert(removed && retained && feathered);
    const auto clipped=clouds.composed;
    layout.world_map_top_fade.enabled=true;PauseMenu::SetLayout(layout,false);
    assert(clouds.Prepare(m,model,decode) && *clouds.composed!=*clipped);
    for(unsigned i=0;i<clipped->size();++i)assert(((*clouds.composed)[i]&255)==((*clipped)[i]&255));
    // Unrelated owners, dungeon maps, unsupported formats and disabled controls fall back.
    assert(!clouds.Prepare(m,map_model,decode));
    m.words[0x4fda6c+0x50]=1;assert(!clouds.Prepare(m,model,decode));m.words[0x4fda6c+0x50]=0;
    m.words[mesh+0x164]=12;assert(!clouds.Prepare(m,model,decode));m.words[mesh+0x164]=13;
    m.words[world+0x38]=0;assert(!clouds.Prepare(m,model,decode));
    PauseMenu::world_exit_hold=true;assert(clouds.Prepare(m,model,decode));PauseMenu::world_exit_hold=false;
    m.words[world+0x38]=2;
    // A failed guest allocation must resume the original draw, not hide clouds.
    m.words[model+0x1dc]=0x8190000;
    std::array<U32,16> registers{};registers[0]=model;registers[14]=0x123456;
    const auto allocation_source=m.words;
    assert(clouds.NativeDraw(m,registers,true,decode) && clouds.Pending());
    registers[0]=0;clouds.Resume(m,registers);
    assert(!clouds.Pending() && registers[0]==model && registers[15]==0x3f9684 &&
        registers[1]==0x54c8c8 && registers[14]==0x123456 && m.words==allocation_source);
    layout.world_map_outline=false;layout.world_map_top_fade.enabled=false;
    layout.map_darkness=0;PauseMenu::SetLayout(layout,false);assert(!clouds.Prepare(m,model,decode));
    std::cout<<"World-map clouds: native atlas, discovery visibility, outline/feather, day/night tint, brightness, top fade, pose/fade cache and safe fallback passed\n";
}
