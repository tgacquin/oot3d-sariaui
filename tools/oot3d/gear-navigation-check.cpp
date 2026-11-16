// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_menu.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
};
int main(int argc,char**argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
    for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(CodeMatches(m));
    // Screenshot layout: quest rewards above the equipment grid; two vertical
    // upgrade groups form one column on their right. Different panel scales.
    Layout l{};l[GearLeft]={-91.4f,-23.2f,.74f};l[GearRight]={-214.4f,64.2f,.73f};
    l[GearBottomLeft]={-26.3f,-96.3f,.74f};l[GearBottomRight]={-155.9f,-10.7f,.74f};
    l.gear_vertical={true,true};SetLayout(l,false);
    auto points=GearFocusPoints(m,l);assert(points);
    constexpr U32 none=0xffffffff,up=0x40,right=0x10,down=0x80,left=0x20;
    const auto expect=[&](U32 from,U32 key,U32 to){assert(GearNeighbor(*points,from,key)==to);};
    for(U32 i=19;i<25;++i){expect(i,down,i+1);expect(i+1,up,i);}
    expect(19,up,none);expect(25,down,none);
    for(U32 i=19;i<=25;++i)expect(i,right,none);
    // Cross-group exits follow the new layout, in both directions.
    expect(0,up,18);expect(18,down,0);expect(2,right,22);expect(22,left,2);
    expect(5,right,23);expect(23,left,5);expect(8,right,25);expect(25,left,8);
    expect(20,left,10);expect(10,right,20);expect(17,right,21);expect(21,left,17);
    for(U32 i=0;i<9;++i) {
        if(i%3<2)expect(i,right,i+1);
        if(i%3>0)expect(i,left,i-1);
        if(i<6)expect(i,down,i+3);
        if(i>=3)expect(i,up,i-3);
    }
    expect(0,left,none);expect(6,down,none);expect(8,down,none);
    expect(0,right|down,4);expect(4,left|up,0);
    expect(0,0,none);expect(0,1,none);expect(0,up|down,none);
    std::array<bool,27> reached{};reached[0]=true;
    for(unsigned pass=0;pass<27;++pass)for(U32 i=0;i<27;++i)if(reached[i])
        for(U32 key:{up,right,down,left}) {
            const auto next=GearNeighbor(*points,i,key);if(next!=none)reached[next]=true;
        }
    for(bool reachable:reached)assert(reachable);
    Layout horizontal{};
    for(Part p:{GearLeft,GearRight,GearBottomLeft,GearBottomRight})horizontal[p]={20,10,1};
    const auto rows=GearFocusPoints(m,horizontal);assert(rows);
    for(U32 i=22;i<25;++i) {
        assert(GearNeighbor(*rows,i,right)==i+1);
        assert(GearNeighbor(*rows,i+1,left)==i);
    }
    // Every result lies in the requested direction, with no near-perpendicular
    // jumps when the user reaches a screen edge.
    for(U32 i=0;i<27;++i)for(U32 key:{up,right,down,left}) {
        const U32 n=GearNeighbor(*points,i,key);if(n==none)continue;
        const auto a=(*points)[i],b=(*points)[n];
        if(key==up)assert(b[1]<a[1]);if(key==down)assert(b[1]>a[1]);
        if(key==left)assert(b[0]<a[0]);if(key==right)assert(b[0]>a[0]);
    }
    // The backing panel does not affect focus. A common translation also
    // leaves neighbors unchanged; changing group positions is read immediately.
    auto moved=l;moved.gear_panel={200,-100,2,.5f};
    for(Part p:{GearLeft,GearRight,GearBottomLeft,GearBottomRight}){moved[p].x+=20;moved[p].y-=10;}
    const auto shifted=GearFocusPoints(m,moved);assert(shifted);
    for(U32 i=0;i<27;++i)for(U32 key:{up,right,down,left})
        assert(GearNeighbor(*shifted,i,key)==GearNeighbor(*points,i,key));
    moved=l;moved[GearBottomLeft].x-=250;
    const auto relocated=GearFocusPoints(m,moved);assert(relocated);
    assert(GearNeighbor(*relocated,20,left)!=10);
    // Hook retains native repeat filtering and resumes at native selection /
    // sound / equip code without writing the selected index itself.
    m.words[UI+0xc]=0x8000000;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;
    m.words[0x8000100]=3;m.words[0x50446c+0x18]=2;m.words[0x50446c+0x20]=21;
    Hook hook;std::array<U32,16> r{};r[0]=down;r[15]=Sites[8].pc+4;
    hook.Handle(m,r,8);assert(r[4]==22 && r[5]==0x50446c && r[6]==0 && r[15]==0x438850);
    assert(m.words.at(0x50446c+0x20)==21);
    for(bool supported:{false,true})for(U32 state:{0u,2u,3u}) {
        m.words[0x50446c+0x18]=state;r[0]=down;r[15]=Sites[8].pc+4;
        hook.Handle(m,r,8,supported);
        assert(r[15]==(supported && state==2?0x438850:Sites[8].pc+4));
    }
    m.words[0x50446c+0x18]=2;SetLayout({},false);r[15]=Sites[8].pc+4;hook.Handle(m,r,8);
    assert(r[15]==Sites[8].pc+4 && r[4]==0x504a74);
    m.words.erase(0x504bd4);assert(!GearFocusPoints(m,l));
    std::cout<<"Gear spatial navigation, screenshot layout, vertical columns, boundaries, diagonals and native fallback passed\n";
}
