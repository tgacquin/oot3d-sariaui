// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_menu.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a){auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v){assert(words.contains(a));words[a]=v;}
    void F(U32 a,float f){words[a]=std::bit_cast<U32>(f);}
    float F(U32 a){return std::bit_cast<float>(words.at(a));}
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
    for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(CodeMatches(m));assert(m.Read(0x480e5c)==0x506cb0);
    constexpr U32 root=0x506cb0,play=0x8000000,model=0x8100000,mesh=0x8200000,
        helper=0x8300000,src=0x8400000,dst=0x8500000;
    m.words[UI+0xc]=play;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;
    m.words[play+0x100]=3;m.words[0x5066f8+0x34]=m.words[0x50446c+0x18]=0;
    m.words[root+0x38]=2;m.words[0x4fda6c+0x50]=0;
    m.words[root]=mesh;m.words[root+4]=model;m.words[root+0x10]=helper;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[helper]=30;
    assert(m.F(root+0x6f0+12*8)==188 && m.F(root+0x6f4+12*8)==152);
    for(unsigned q=0;q<30;++q) {
        const float x=m.F(root+0x6f0+q*8),y=m.F(root+0x6f4+q*8);
        const float w=m.F(root+0x7e0+q*8),h=m.F(root+0x7e4+q*8);
        const std::array<float,12> p{x,y,-3,x+w,y,-3,x,y+h,-3,x+w,y+h,-3};
        for(unsigned i=0;i<12;++i){m.F(src+q*48+i*4,p[i]);m.F(dst+q*48+i*4,0);}
        assert(WorldLocationQuad(m,mesh,q,30)==(q>=12 && q<=17));
    }
    Layout layout;layout[Map]={17,-8,.85f};SetLayout(layout,false);motion_x=0;
    assert(CopyPositions(m,mesh,dst,src,30*48));
    std::array<float,360> baseline;for(unsigned i=0;i<baseline.size();++i)baseline[i]=m.F(dst+4*i);
    for(float scale:{.25f,.7f,1.f,2.f,3.f}) {
        layout[WorldLocationPanel]={11,-5,scale};SetLayout(layout,false);
        std::vector<AnimatedQuad> pending;assert(CopyPositions(m,mesh,dst,src,30*48,&pending));
        const auto anchor=Move(252,172,Map,layout[Map]);
        for(unsigned q=0;q<30;++q)for(unsigned i=0;i<12;i+=3) {
            const unsigned at=q*12+i;
            if(q>=12 && q<=17) {
                assert(std::abs(m.F(dst+at*4)-(anchor[0]+(baseline[at]-anchor[0])*scale+11/Scale))<.001f);
                assert(std::abs(m.F(dst+(at+1)*4)-(anchor[1]+(baseline[at+1]-anchor[1])*scale-5/Scale))<.001f);
                assert(m.F(dst+(at+2)*4)==-3);
            } else for(unsigned c=0;c<3;++c)assert(m.F(dst+(at+c)*4)==baseline[at+c]);
        }
        unsigned panels=0;for(const auto& q:pending)if(q.part==WorldLocationPanel)++panels;
        assert(panels==6);
    }
    assert(CopyPositions(m,mesh,dst,src,30*48));
    std::array<float,360> before;for(unsigned i=0;i<before.size();++i)before[i]=m.F(dst+4*i);
    layout.world_skulltula_x=21;layout.world_skulltula_y=-9;SetLayout(layout,false);
    assert(CopyPositions(m,mesh,dst,src,30*48));
    for(unsigned q=0;q<30;++q)for(unsigned v=0;v<4;++v) {
        const auto i=q*12+v*3;
        assert(std::abs(m.F(dst+i*4)-before[i]-(q>=16 && q<=17?21/Scale:0))<.001f);
        assert(std::abs(m.F(dst+(i+1)*4)-before[i+1]-(q>=16 && q<=17?-9/Scale:0))<.001f);
    }
    layout.world_skulltula_scale=1.5f;SetLayout(layout,false);
    assert(CopyPositions(m,mesh,dst,src,30*48));
    const auto group_center=MoveWorldLocation(200,172,layout);
    for(unsigned q=0;q<30;++q)for(unsigned v=0;v<4;++v) {
        const auto i=q*12+v*3;const bool group=q==16 || q==17;
        const float x=group?group_center[0]+(before[i]-group_center[0])*1.5f+21/Scale:before[i];
        const float y=group?group_center[1]+(before[i+1]-group_center[1])*1.5f-9/Scale:before[i+1];
        assert(std::abs(m.F(dst+i*4)-x)<.001f);
        assert(std::abs(m.F(dst+(i+1)*4)-y)<.001f);
    }
    motion_x=-43.5f;assert(Motion(WorldLocationPanel)==Motion(Map));
    // Native state 9 slides the box by timer*18, but resets the Skulltula
    // independently. Normalize before visibility tests (the first frame can
    // park the box below 240), then move every visible part together at 60 Hz.
    motion_x=0;SetLayout({},false);
    constexpr U32 offsets=0x8600000;
    m.words[helper+0x1c]=offsets;m.words[root+0x38]=9;
    for(unsigned q=12;q<=17;++q) {
        const float dy=q==17?0:90;
        m.F(offsets+q*8,0);m.F(offsets+q*8+4,dy);
        for(unsigned i=1;i<12;i+=3)m.F(src+q*48+i*4,m.F(src+q*48+i*4)+dy);
    }
    const auto native=m.words;
    for(float y:{16.f,13.25f,8.f,2.5f,0.f}) {
        world_panel_y=y;Hook hook;
        assert(CopyPositions(m,mesh,dst,src,30*48,&hook.pending));hook.Animate(m);
        for(unsigned q=12;q<=17;++q)for(unsigned i=1;i<12;i+=3) {
            const float expected=std::bit_cast<float>(native.at(src+q*48+i*4))-(q==17?0:90)+y/Scale;
            assert(std::abs(m.F(dst+q*48+i*4)-expected)<.001f);
        }
        for(unsigned q=12;q<=17;++q)for(unsigned i=0;i<12;++i)
            assert(m.words.at(src+q*48+i*4)==native.at(src+q*48+i*4));
    }
    // Do not reveal an unavailable Skulltula parked horizontally by the game.
    m.F(offsets+17*8,400);
    for(unsigned i=0;i<12;i+=3)m.F(src+17*48+i*4,m.F(src+17*48+i*4)+400);
    assert(CopyPositions(m,mesh,dst,src,30*48));
    for(unsigned i=3;i<12;i+=3)assert(m.F(dst+17*48+i*4)==m.F(dst+17*48));
    world_panel_y=0;
    m.words[0x4fda6c+0x50]=1;assert(!WorldLocationQuad(m,mesh,15,30));
    m.words[0x4fda6c+0x50]=0;m.words[helper]=50;assert(!WorldLocationQuad(m,mesh,15,30));
    std::cout<<"Location panel: verified native quads, grouped background/name/icons, independent pose, unchanged map/cards, motion and owner guards passed\n";
}
