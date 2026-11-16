// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_menu.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <map>
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
};
int main() {
    assert((OverlayTint({{0,.25f,.5f},0})==std::array<float,3>{1,1,1}));
    assert((OverlayTint({{0,.25f,.5f},1})==std::array<float,3>{0,.25f,.5f}));
    assert((OverlayTint({{0,.5f,1},.5f})==std::array<float,3>{.5f,.75f,1}));
    assert((OverlayTint({{0,0,0},std::numeric_limits<float>::quiet_NaN()})==std::array<float,3>{1,1,1}));
    Memory m;constexpr U32 model=0x8000000,mesh=0x8010000,helper=0x8020000,src=0x8030000,dst=0x8040000,play=0x8050000;
    m.words[UI+0xc]=play;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;m.words[play+0x100]=3;
    m.words[0x5066f8+0x20]=model;m.words[0x5066f8+0x24]=helper;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[helper]=37;m.words[helper+0x18]=src;
    for(unsigned i=0;i<37*16;++i) {m.words[src+4*i]=std::bit_cast<U32>(float((i%13)+1)/13);m.words[dst+4*i]=0;}
    const auto original=m.words;
    Layout layout;layout.header_overlay={{0,1,0},1};layout.slots_overlay={{1,0,0},.5f};SetLayout(layout,false);
    assert(CopySlotColors(m,mesh,dst,src,37*64));
    for(unsigned i=0;i<37*16;++i) {
        float expected=std::bit_cast<float>(original.at(src+4*i));
        if(i/16==12 && i%4<3)expected*=OverlayTint(layout.slots_overlay)[i%4];
        assert(m.words.at(dst+4*i)==std::bit_cast<U32>(expected));
        assert(m.words.at(src+4*i)==original.at(src+4*i));
    }
    const auto once=m.words;assert(CopySlotColors(m,mesh,dst,src,37*64));assert(m.words==once);
    assert(!CopySlotColors(m,mesh+4,dst,src,37*64));assert(!CopySlotColors(m,mesh,dst,src,36*64));
    layout.slots_overlay.opacity=0;SetLayout(layout,false);assert(!CopySlotColors(m,mesh,dst,src,37*64));
    layout.slots_overlay.opacity=1;SetLayout(layout,false);
    Hook hook;std::array<U32,16> regs{};regs[0]=dst;regs[4]=mesh;regs[8]=src;regs[7]=37*64;regs[15]=Sites[9].pc+4;
    hook.Handle(m,regs,9);assert(regs[1]==src && regs[15]==0x2f99f0);
    // Shared plate seam and native slot crops; icon and tab UVs remain exact.
    constexpr U32 uvsrc=0x8060000,uvdst=0x8070000;
    m.words[helper+0x14]=uvsrc;
    constexpr std::array<float,16> plate{0,1,124.f/256,1,0,56.f/256,124.f/256,56.f/256,
        124.f/256,1,0,1,124.f/256,56.f/256,0,56.f/256};
    for(unsigned i=0;i<37*8;++i) {m.words[uvsrc+4*i]=std::bit_cast<U32>(i<16?plate[i]:float(i)/300);m.words[uvdst+4*i]=0;}
    // ROM tables 5066f8+438 / +310, used by native constructor 46b534.
    constexpr std::array<std::array<float,4>,12> slots{{
        {128,152,8,56},{134,152,1,56},{128,208,8,-56},{134,152,1,56},
        {140,136,52,52},{140,136,52,52},{168,88,24,48},{192,88,-24,48},
        {124,0,44,44},{168,88,24,48},{192,88,-24,48},{124,0,44,44}}};
    for(unsigned q=0;q<slots.size();++q)for(unsigned v=0;v<4;++v) {
        const auto s=slots[q];
        m.words[uvsrc+(q+13)*32+v*8]=std::bit_cast<U32>((s[0]+(v%2)*s[2])/256);
        m.words[uvsrc+(q+13)*32+v*8+4]=std::bit_cast<U32>(1-(s[1]+(v/2)*s[3])/256);
    }
    const auto uv_before=m.words;
    assert(CopyPanelUVs(m,mesh,uvdst,uvsrc,37*32));
    for(unsigned i=0;i<37*8;++i) {
        const bool inset=i==2 || i==6 || i==8 || i==12;
        if(i/8<13 || i/8>24)
            assert(m.words.at(uvdst+4*i)==(inset?std::bit_cast<U32>(123.5f/256):uv_before.at(uvsrc+4*i)));
        assert(m.words.at(uvsrc+4*i)==uv_before.at(uvsrc+4*i));
    }
    const auto out=[&](unsigned q,unsigned c){return std::bit_cast<float>(m.words.at(uvdst+q*32+c*4));};
    for(unsigned q:{14u,16u})for(unsigned c=0;c<8;c+=2)assert(out(q,c)==134.5f/256);
    for(unsigned q:{17u,18u}) {
        assert(out(q,0)==140.5f/256 && out(q,2)==191.5f/256);
        assert(out(q,1)==1-136.5f/256 && out(q,5)==1-187.5f/256);
    }
    for(unsigned q:{21u,24u})assert(out(q,1)==1-.5f/256 && out(q,5)==1-43.5f/256);
    assert(out(19,2)==out(20,0) && out(22,2)==out(23,0)); // Mirrored joins agree.
    assert(out(15,1)<out(15,5)); // Reversed V is retained.
    const auto uv_once=m.words;assert(CopyPanelUVs(m,mesh,uvdst,uvsrc,37*32));assert(m.words==uv_once);
    assert(!CopyPanelUVs(m,mesh+4,uvdst,uvsrc,37*32));
    m.words[uvsrc]=std::bit_cast<U32>(.5f);assert(!CopyPanelUVs(m,mesh,uvdst,uvsrc,37*32));
    m.words[uvsrc]=0;regs[0]=uvdst;regs[4]=mesh;regs[7]=uvsrc;regs[6]=37*32;regs[15]=Sites[10].pc+4;
    hook.Handle(m,regs,10);assert(regs[1]==uvsrc && regs[15]==0x317db8);
    m.words[play+0x100]=0;assert(!CopySlotColors(m,mesh,dst,src,37*64));
    assert(!CopyPanelUVs(m,mesh,uvdst,uvsrc,37*32));
    std::cout<<"Panel seam UVs, independent tint, ownership, unchanged icons/alpha, and fresh uploads passed\n";
}
