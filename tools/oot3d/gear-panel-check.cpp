// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/gear_panel.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::GearPanel;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 w) {assert(words.contains(a));words[a]=w;}
    void F(U32 a,float f) {words[a]=std::bit_cast<U32>(f);}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
int main(int argc,char**argv) {
    Memory m;
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
        for(U32 at=0x100000;file.read(reinterpret_cast<char*>(&word),4);at+=4)m.words[at]=word;
        assert(PauseMenu::CodeMatches(m));m.words.clear();
    }
    for(float width:{.1f,1.f,3.f})for(float height:{.1f,1.f,3.f}) {
        const auto g=Make({37,-21,width,height},{.8f,.7f,.6f,.5f},-40);
        assert(g.count==9);
        assert(Near(g.positions[8*12+9]-g.positions[0],320*width));
        assert(Near(g.positions[8*12+10]-g.positions[1],188*height));
        for(unsigned q=0;q<9;++q) {
            const auto* p=g.positions.data()+q*12;
            if(q%3!=1)assert(Near(p[3]-p[0],2));
            if(q/3!=1)assert(Near(p[7]-p[1],2));
            if(q%3<2)assert(Near(p[3],p[12]));
            if(q<6)assert(Near(p[7],g.positions[(q+3)*12+1]));
            for(unsigned i=0;i<8;i+=2) {
                assert(g.uvs[q*8+i]>=0 && g.uvs[q*8+i]<158.f/512);
                assert(g.uvs[q*8+i+1]>1-144.f/256 && g.uvs[q*8+i+1]<=1);
            }
            for(unsigned i=0;i<4;++i)assert(g.colors[q*16+i*4+3]==.5f);
        }
        assert(g.uvs[2*8]>g.uvs[2*8+2]); // Right border mirrors left.
        assert(g.uvs[6*8+1]<g.uvs[6*8+5]); // Bottom border mirrors top.
    }
    assert(Make({0,0,0,1},{1,1,1,1},0).count==0);
    constexpr U32 play=0x8000000,source=0x8100000,mesh=0x8200000,helper=0x8300000;
    constexpr U32 src=0x8400000,dst=0x8500000,block=0x8600000,model=0x8700000,desc=0x8800000;
    m.words[PauseMenu::UI+0xc]=play;m.words[PauseMenu::UI+0x14]=1;m.words[PauseMenu::UI+0x20]=0;
    m.words[PauseMenu::UI+0x24]=1;m.words[play+0x100]=3;
    m.words[Owner]=mesh;m.words[Owner+4]=source;m.words[Owner+0xc]=helper;m.words[Owner+0x18]=2;
    for(U32 i=0;i<0x1e4;i+=4)m.words[source+i]=m.words[model+i]=0;
    for(U32 i=0;i<0x1b8;i+=4)m.words[mesh+i]=0;
    for(U32 i=0;i<0x120;i+=4)m.words[desc+i]=0;
    for(U32 i=0;i<AllocationSize;i+=4)m.words[block+i]=0;
    m.words[source]=0x4ebd60;m.words[source+4]=mesh;m.words[source+0x1dc]=0x8900000;
    m.words[mesh]=desc;m.words[mesh+0x158]=0x14001000;
    m.words[helper]=41;m.words[helper+0x18]=0x8a00000;
    for(unsigned i=0;i<4;++i)m.F(0x8a00000+4*i,1);
    for(unsigned q=0;q<41;++q) {
        const std::array<float,12> v{20,40,0,30,40,0,20,50,0,30,50,0};
        for(unsigned i=0;i<12;++i){m.F(src+q*48+i*4,v[i]);m.F(dst+q*48+i*4,0);}
    }
    const auto native=m.words;
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,41*48));
    for(unsigned q=0;q<41;++q) {
        const bool background=q==0 || q==5 || q==8 || q==15;
        const bool pressed_border=q>=32 && q<=40;
        assert((m.F(dst+q*48)==m.F(dst+q*48+12))==(background || pressed_border));
        for(unsigned i=0;i<12;++i)assert(m.words.at(src+q*48+i*4)==native.at(src+q*48+i*4));
        assert(!PauseMenu::GearBackground(m,mesh+4,q,41));
        assert(!PauseMenu::GearBackground(m,mesh,q,37));
    }
    // Splitting preserves the original outer rectangle, including edited size.
    const PauseMenu::GearPanel old{35,-26,.7f,1.4f};
    const auto split=PauseMenu::SplitGearPanel(old);
    const auto old_g=Make(old,{1,1,1,1},0);
    const auto upper=Make(split[0],{1,1,1,1},0),lower=Make(split[1],{1,1,1,1},0);
    assert(Near(old_g.positions[1],upper.positions[1]));
    assert(Near(old_g.positions[8*12+10],lower.positions[8*12+10]));
    assert(Near(upper.positions[8*12+10],lower.positions[1]));
    PauseMenu::Layout two;const auto base=Current(m,two);assert(base.count==18);
    two.gear_panel_lower.x+=23;two.gear_panel_lower.width*=.8f;
    const auto edit=Current(m,two);
    for(unsigned i=0;i<9*12;++i)assert(base.positions[i]==edit.positions[i]);
    assert(base.positions[9*12]!=edit.positions[9*12]);
    Hook hook;std::array<U32,16> regs{};regs[15]=PauseMenu::Sites[14].pc+4;
    const auto saved=regs;const auto before=m.words;
    hook.Begin(m,regs,PauseMenu::Sites[14].pc,Current(m,{}));
    assert(regs[15]==0x313ce0);regs[0]=block;hook.Resume(m,regs);
    assert(regs[15]==0x348f34 && m.words.at(block+Descriptor+0xc)==72);
    m.words[block+0x128]=2;hook.Resume(m,regs);
    assert(regs[15]==0x34897c && m.words.at(block+0x158)==0x14001000);
    m.words[model]=0x4ebd60;m.words[model+8]=0x8b00000;m.words[0x8b00358]=0x8900000;regs[0]=model;hook.Resume(m,regs);
    assert(hook.Owned(m) && regs[15]==0x36759c);
    hook.Resume(m,regs);assert(regs[15]==0x317d1c);
    hook.Resume(m,regs);assert(regs[15]==0x2f9934);
    hook.Resume(m,regs);assert(regs[15]==0x3f9680 && hook.Drawing());
    hook.Resume(m,regs);assert(regs==saved && !hook.Pending());
    for(const auto& [at,word]:before)
        if((at<block || at>=block+AllocationSize) && (at<model || at>=model+0x1e4))assert(m.words.at(at)==word);
    hook.Begin(m,regs,PauseMenu::Sites[14].pc,Current(m,{}));assert(regs[15]==0x36759c);
    m.words[block+Cookie]=0;assert(!hook.Owned(m));
    m.words[Owner+0x18]=0;assert(Current(m,{}).count==0);
    std::cout<<"Gear panel borders, atlas bounds, overlay isolation, independent buffers and state recovery passed\n";
}
