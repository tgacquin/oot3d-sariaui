// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_framerate.h"
#include <map>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace OOT3D;
using TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at){auto i=words.find(at);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 at,U32 v){words[at]=v;}
    float F(U32 at){return std::bit_cast<float>(words.at(at));}
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream in(argv[1],std::ios::binary);assert(in);U32 word;
    for(U32 at=0x100000;in.read(reinterpret_cast<char*>(&word),4);at+=4)m.words[at]=word;
    assert(PauseFramerate::CodeMatches(m));
    constexpr U32 play=0x800000,owner=0x810000,sp=0x820000;
    m.words[PauseMenu::UI+0xc]=play;m.words[play+0x100]=3;
    m.words[PauseMenu::UI+0x14]=1;m.words[PauseMenu::UI+0x20]=0;m.words[PauseMenu::UI+0x24]=1;
    m.words[0x54ac4c]=0x01000000;m.words[owner+0x10c]=0x830000;
    PauseMenu::Layout layout;layout.selection_grow_seconds=.2f;PauseMenu::SetLayout(layout,false);
    for(unsigned page:{0u,1u,2u}) {
        m.words[0x5066f8+0x34]=page==1?0:2;m.words[0x50446c+0x18]=page==1?2:0;
        const U32 root=page==1?0x50446c:0x5066f8,selected=root+(page==1?0x20:page==2?0xa0:0x64);
        m.words[root+(page==1?0x10:page==2?0x40:0x3c)]=owner;m.words[selected]=page==2?1:2;
        if(page==2)m.words[root+0x3c]=0;
        m.words[root+0x9c]=0;m.words[root+0x38]=page==2?2:0;m.words[root+0x60]=0;
        PauseSelection::State state;state.Track(m,1000000);
        std::array<U32,16> r{};r[4]=owner;r[7]=2;r[13]=sp;
        const auto reset=[&]{
            const float half=page==1?25.f:26.f;
            const std::array<float,12> v{41-half,51-half,0,41+half,51-half,0,41-half,51+half,0,41+half,51+half,0};
            for(unsigned i=0;i<12;++i)m.Write(sp+4+4*i,std::bit_cast<U32>(v[i]));
        };
        for(auto [time,width]:std::array<std::pair<int,float>,3>{{{1000000,42},{1100000,page==1?46.f:47.f},{1200000,page==1?50.f:52.f}}}) {
            reset();state.Apply(m,r,time);
            assert(std::abs(m.F(sp+16)-m.F(sp+4)-width)<.0001f);
            assert(std::abs((m.F(sp+16)+m.F(sp+4))*.5f-41)<.0001f);
            const auto once=m.words;reset();state.Apply(m,r,time);assert(m.words==once);
            assert(r[0]==0x830000);
        }
        m.words[selected]=page==2?0:3;state.Track(m,1300000);m.words[selected]=page==2?1:2;
        reset();state.Apply(m,r,1400000);assert(std::abs(m.F(sp+16)-m.F(sp+4)-42)<.0001f);
        r[7]=3;reset();const auto unrelated=m.words;state.Apply(m,r,1400000);assert(m.words==unrelated);
        m.words[0x54ac4c]=0;state.Track(m,1500000);assert(state.selections[page].owner==0);m.words[0x54ac4c]=0x01000000;
    }
    assert(PauseSelection::State::Scale(0,0)==1);
    std::cout<<"Selected 2D item/gear growth: native guards, duration, center, repeated draws, reselection and isolation passed\n";
}
