// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/dungeon_sides.h"
#include "video_core/texture/etc1.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
namespace P=OOT3D::DungeonParchment;
using TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    template<class T> void Floats(U32 a,const T& v) {for(unsigned i=0;i<v.size();++i)words[a+4*i]=std::bit_cast<U32>(v[i]);}
};
int main(int argc,char** argv) {
    assert(argc==3);
    P::Paper paper;paper.Set(std::vector<P::Color>(64*64,{160,120,70,255}),64,64);
    std::array<P::Rect,4> boxes{{{0,0,64,64},{20,0,84,64},{0,20,64,84},{20,20,84,84}}};
    for(float blur:{0.f,5.f,18.f,80.f})for(unsigned count=1;count<=4;++count) {
        const std::span<const P::Rect> pieces{boxes.data(),count};
        const auto actual=paper.At(30,30,pieces,blur);
        for(unsigned c=0;c<4;++c)assert(std::abs(actual[c]-P::Color{160,120,70,255}[c])<.001f); // Never darkens with stacking.
        assert(paper.At(-1,30,pieces,blur)[3]==0);
    }
    // The sharp grid follows one shared field, even with fractional positions
    // and unequal scales. Per-piece color weights cannot duplicate a line.
    std::vector<P::Color> grid(96*96);
    for(unsigned y=0;y<96;++y)for(unsigned x=0;x<96;++x) {
        const float ink=(x%24==0 || y%24==0)?40.f:0;
        grid[y*96+x]={180-ink,130-ink,80-ink,255};
    }
    paper.Set(grid,96,96);boxes={{{0,0,96,96},{13.2f,-8.5f,140,110},{-10,17,70,130},{20,50,150,150}}};
    for(float x:{20.f,23.f,24.f,25.f,50.f,73.f}) {
        const float u=paper.grid.left+P::Mirror(x*95/96-paper.grid.left,paper.grid.right-paper.grid.left);
        const float v=paper.grid.top+P::Mirror(35.f*95/96-paper.grid.top,paper.grid.bottom-paper.grid.top);
        const auto native=P::Sample(paper.original,96,96,u,v),soft=P::Sample(paper.soft,96,96,u,v);
        auto flat=paper;flat.original=flat.soft;
        const auto a=paper.At(x,35,boxes,18),b=flat.At(x,35,boxes,18);
        for(unsigned c=0;c<3;++c)assert(std::abs((a[c]-b[c])-(native[c]-soft[c]))<.0001f);
    }
    Memory m;constexpr U32 model=0x8100000,mesh=0x8110000,desc=0x8120000,live=0x14900000,helper=0x8130000,texture=0x14a00000;
    m.words[0x5066f8+0x34]=m.words[0x50446c+0x18]=0;m.words[0x506cb0+0x38]=2;m.words[0x4fda6c+0x50]=1;
    m.words[0x506cb0+8]=mesh;m.words[0x506cb0+0xc]=model;m.words[0x506cb0+0x14]=helper;m.words[helper]=50;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[mesh]=desc;m.words[desc+12]=200;
    m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=live;m.words[mesh+0x158]=texture;
    m.words[mesh+0x15c]=512|(256u<<16);m.words[mesh+0x164]=13;
    const std::array<float,12> positions{56,32,0,264,32,0,56,208,0,264,208,0};m.Floats(live,positions);
    m.Floats(live+200*12,std::array<float,8>{.5f,1,464.f/512,1,.5f,80.f/256,464.f/512,80.f/256});
    std::array<float,16> color;color.fill(.8f);for(unsigned c=3;c<16;c+=4)color[c]=.6f;m.Floats(live+200*20,color);
    std::ifstream in(argv[1],std::ios::binary);assert(in);std::vector<unsigned char> atlas{std::istreambuf_iterator<char>(in),{}};
    assert(atlas.size()==131144);
    for(unsigned i=0;i<131072;i+=4)m.words[texture+i]=MapFrame::LE32(atlas,i+72);
    unsigned decodes=0;
    const auto decode=[&](std::uint64_t bits,unsigned x,unsigned y){++decodes;const auto rgb=Pica::Texture::SampleETC1Subtile(bits,x,y);return std::array{rgb.r(),rgb.g(),rgb.b()};};
    PauseMenu::Layout layout;PauseMenu::SetLayout(layout,false);PauseMenu::motion_x=0;
    P::Hook hook;hook.Observe(m,model,decode);assert(!hook.ready && decodes==0); // Existing layout untouched.
    layout.dungeon_parchments={{{true,{-80,0,1}},{true,{80,0,1}},{true,{0,48,.75f}}}};
    PauseMenu::SetLayout(layout,false);const auto before=m.words;
    hook.Observe(m,model,decode);assert(hook.ready && hook.composed && decodes==208*176);
    assert(m.words==before && hook.prepared.colors==color);
    auto image=hook.composed;hook.Observe(m,model,decode);assert(hook.composed==image && decodes==208*176);
    // A new hook can use the prepared image on its first map draw. A layout
    // edit or a different atlas at the same address must use the live source.
    const P::Recipe recipe(layout,{56,32,264,208});
    const auto cached=std::make_shared<P::Warm>(P::Warm{recipe.key,
        std::make_shared<const std::vector<U32>>(hook.compressed),recipe.Render(hook.paper)});
    assert(*cached->pixels==*image);
    P::warm.store(cached);
    P::Hook first_open;
    first_open.Observe(m,model,decode);
    assert(first_open.ready && first_open.composed==cached->pixels && first_open.timings.rebuilds==0);
    assert(decodes==208*176);
    std::cout<<"Dungeon first observation with prepared parchment: "<<first_open.timings.observe_ms
             <<" ms; cold composition alone: "<<hook.timings.compose_ms<<" ms\n";
    auto edited=layout;edited.parchment_blend+=3;PauseMenu::SetLayout(edited,false);
    first_open.Observe(m,model,decode);
    assert(first_open.ready && first_open.composed!=cached->pixels && first_open.timings.rebuilds==1);
    PauseMenu::SetLayout(layout,false);
    m.words[texture+8]^=1;
    first_open.Observe(m,model,decode);
    assert(first_open.ready && first_open.composed!=cached->pixels && first_open.timings.rebuilds==2);
    m.words[texture+8]^=1;
    P::warm.store(std::shared_ptr<const P::Warm>{});
    assert(decodes==3*208*176);decodes=208*176;
    // Motion/fades never trigger texture rebuild, and generated UV orientation
    // agrees with the other generated GuestUI textures (top UV=0).
    for(float motion:{-237.17f,-123.3f,-12.5f,0.f}) {
        PauseMenu::motion_x=motion;auto moved=positions;for(unsigned i=0;i<4;++i)moved[i*3]+=motion/PauseMenu::Scale;
        m.Floats(live,moved);hook.Observe(m,model,decode);assert(hook.composed==image);
        assert(std::abs(hook.prepared.positions[0]-hook.bounds.left-motion/PauseMenu::Scale)<.0001f);
        assert(hook.prepared.uvs[1]==0 && hook.prepared.uvs[5]==1);
    }
    PauseMenu::motion_x=0;m.Floats(live,positions);
    layout.parchment_blend=40;PauseMenu::SetLayout(layout,false);hook.Observe(m,model,decode);
    assert(hook.composed!=image && decodes==208*176);
    std::ofstream out(argv[2],std::ios::binary);out<<"P6\n"<<P::Width<<' '<<P::Height<<"\n255\n";
    for(unsigned y=0;y<P::Height;++y)for(unsigned x=0;x<P::Width;++x) {
        const auto pixel=(*hook.composed)[P::TiledOffset(x,P::Height-1-y)];const float a=(pixel&255)/255.f;
        for(unsigned c=0;c<3;++c)out.put(char(std::lround(((pixel>>(24-8*c))&255)*a+35*(1-a))));
    }
    m.words[texture+8]^=1;hook.Observe(m,model,decode);assert(decodes==2*208*176); // Same pointer, replaced texture.
    m.words[0x4fda6c+0x50]=0;hook.Observe(m,model,decode);assert(!hook.ready); // Overworld excluded.
    std::cout<<"Four-paper blending, shared grid, alpha, live native atlas, cache, movement, fades and safe defaults passed\n";
}
