// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_shadow.h"
#include "oot3d/map_frame.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace OOT3D;
using TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    template<class T> void Floats(U32 a,const T& v) {for(unsigned i=0;i<v.size();++i)words[a+4*i]=std::bit_cast<U32>(v[i]);}
};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
int main() {
    using namespace PauseMenu;
    Memory m;
    constexpr U32 bg=0x8100000,mesh=0x8110000,desc=0x8120000,live=0x14900000;
    constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
    m.words[UI+0x10]=bg;m.words[bg]=0x4ebd60;m.words[bg+4]=mesh;
    m.words[mesh]=desc;m.words[desc+12]=4;m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=live;
    m.Floats(bg+12,identity);m.Floats(live,std::array<float,12>{0,0,0,320,0,0,0,240,0,320,240,0});
    m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=0;
    Layout layout;layout.map_backgrounds={{{{-90,25,.6f},.2f},{{110,-15,1.2f},.7f}}};
    layout[Background]={300,100,2.5f};layout.stack_background=false;layout.background_darkness=1;
    SetLayout(layout,false);motion_x=-30;
    Hook hook;hook.drawing=true;std::array<U32,16> regs{};
    for(unsigned i=0;i<4;++i) {
        const auto p=hook.BackgroundPose(m,layout);const auto b=layout.map_backgrounds[i/2].pose;
        assert(Near(p.x,b.x-30) && Near(p.y,b.y-(i%2?240*Scale*b.scale:0)) && p.scale==b.scale);
        regs[15]=Sites[5].pc+4;hook.Handle(m,regs,5);
        assert(hook.background_copy==(i<3));
        assert(regs[15]==(i<3?0x3f9680:Sites[5].pc+4));
    }
    assert(hook.background_index==0);
    const auto original=m.words;
    auto dim=PauseShadow::Panels(m,layout);assert(dim.count==2 && original==m.words);
    for(unsigned i=0;i<2;++i) {
        auto p=layout.map_backgrounds[i].pose;p.x-=30;
        auto box=PauseShadow::ToTop({0,0,320,240},p);box.top-=240*Scale*p.scale;
        assert(Near(dim.positions[i*12],box.left) && Near(dim.positions[i*12+4],box.top));
        assert(Near(dim.positions[i*12+6],box.right) && Near(dim.positions[i*12+1],box.bottom));
        assert(dim.colors[i*16+3]==layout.map_backgrounds[i].darkness);
    }
    // Dungeon dimmers are independent from the two overworld dimmers.
    m.words[0x506cb0+0x38]=2;m.words[0x4fda6c+0x50]=1;
    layout.dungeon_background_darkness={.85f,.9f};
    auto dungeon_dim=PauseShadow::Panels(m,layout);assert(dungeon_dim.count==2);
    assert(dungeon_dim.colors[3]==.85f && dungeon_dim.colors[19]==.9f);
    assert(dungeon_dim.positions==dim.positions);
    m.words[0x4fda6c+0x50]=0;
    assert(PauseShadow::Panels(m,layout).colors==dim.colors);
    layout[Background]={-300,-200,.1f};layout.background_darkness=0;layout.stack_background=true;
    assert(PauseShadow::Panels(m,layout).positions==dim.positions);
    assert(PauseShadow::Panels(m,layout).colors==dim.colors);
    m.words[0x50446c+0x18]=2;hook.background_index=0;
    auto p=hook.BackgroundPose(m,layout);assert(Near(p.x,layout[Background].x-30));
    auto gear=PauseShadow::Panels(m,layout);assert(gear.count==1); // Only the shared background shadow.
    layout.map_backgrounds[0].darkness=1;layout.map_backgrounds[1].pose.x=-350;
    assert(PauseShadow::Panels(m,layout).positions==gear.positions);
    assert(PauseShadow::Panels(m,layout).colors==gear.colors);

    // A tiled map yields one continuous pair of shadows. Hidden quads and
    // unrelated/shared backgrounds do not move the detected outer edges.
    constexpr U32 mapmodel=0x8130000,mapmesh=0x8140000,mapdesc=0x8150000,maplive=0x14901000;
    m.words[0x50446c+0x18]=0;m.words[0x506cb0+4]=mapmodel;
    m.words[mapmodel]=0x4ebd60;m.words[mapmodel+4]=mapmesh;m.words[mapmesh]=mapdesc;
    m.words[mapdesc+12]=12;m.words[mapmesh+0x124]=1;m.words[mapmesh+0x1a4]=maplive;
    m.Floats(mapmodel+12,identity);
    layout[Map]={-50,30,.5f};SetLayout(layout,false);motion_x=-20;
    for(unsigned q=0;q<3;++q) {
        std::array<float,12> v=q==2?std::array<float,12>{400,240,0,400,240,0,400,240,0,400,240,0}:
            std::array<float,12>{float(q*160),0,0,float((q+1)*160),0,0,float(q*160),198,0,float((q+1)*160),198,0};
        for(unsigned j=0;j<12;j+=3) {auto point=AnimatedMove(v[j],v[j+1],Map,layout[Map]);v[j]=point[0];v[j+1]=point[1];}
        m.Floats(maplive+q*48,v);
    }
    PauseShadow::MapEdges edges;edges.Observe(m,bg);assert(!edges.bounds);
    edges.Observe(m,mapmodel);assert(edges.bounds);
    assert(Near(edges.bounds->right-edges.bounds->left,320*.5f*Scale));
    assert(edges.Make(layout).count==0);
    layout.map_side_shadows=true;layout.map_shadow={2,.6f};
    auto shadow=edges.Make(layout);assert(shadow.count==2);
    assert(Near(shadow.positions[6],edges.bounds->left) && Near(shadow.positions[12],edges.bounds->right));
    assert(shadow.uvs[0]>shadow.uvs[4] && shadow.uvs[8]<shadow.uvs[12]);
    assert(shadow.colors[3]==.6f && shadow.colors[19]==.6f);
    assert(Near(shadow.positions[6]-shadow.positions[0],8*2*Scale*.5f));
    edges.Reset();assert(edges.Make(layout).count==0);
    m.words[0x5066f8+0x34]=2;edges.Observe(m,mapmodel);assert(!edges.bounds);
    // Darkening changes RGB only, leaving cutout transparency and shape intact.
    auto frame=OOT3D::MapFrame::Make(layout,300,200);layout.map_frame.darkness=.75f;
    auto dark=OOT3D::MapFrame::Make(layout,300,200);assert(frame.positions==dark.positions && frame.uvs==dark.uvs);
    for(unsigned i=0;i<16;++i)assert(dark.colors[i]==(i%4==3?1:.25f));
    motion_x=0;SetLayout({},false);
    std::cout<<"Map-only stacks, independent dimmers, tiled map side shadows, page isolation and border darkness passed\n";
}
