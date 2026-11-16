// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/textbox_close.h"
#include "oot3d/pause_description.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::TextboxClose;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a){auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Write(U32 a,U32 v){words[a]=v;}
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);
    U32 word;for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(PauseDescription::CodeMatches(m));
    assert(Scale(0)==1 && std::abs(Scale(33333)-1.05f)<.00001f && Scale(Duration)==0);
    assert(Scale(100000)<1 && Scale(100000)>0 && Scale(-1)==1);
    constexpr U32 pane=0x8000000,model=0x8010000,view=0x8020000,play=0x8030000;
    m.words[Message]=0x4ed0a0;m.words[Message+0xc]=1;m.words[Message+0xf38]=6;
    m.words[HUDFade::UI+0xc]=play;m.words[play+0x104]=1;
    m.words[Message+8]=play;m.words[Message+0xf40]=0x312;m.words[Message+0x56c]=pane;
    m.words[pane]=0x4ed234;m.words[pane+4]=Message+0x168;m.words[pane+0x64]=model;
    m.words[model]=0x4ebd60;m.words[model+4]=0x8040000;
    for(unsigned i=0;i<12;++i) {
        m.words[model+12+4*i]=std::bit_cast<U32>(i==3?200.f:i==7?196.f:float(i==0||i==5||i==10));
        m.words[view+4*i]=std::bit_cast<U32>(float(i==0||i==5||i==10));
    }
    auto layout=Textboxes::GetLayout();layout.mm_close=true;Textboxes::SetLayout(layout,false);
    State c;c.Capture(m,model,view,1000000);assert(c.ready);
    const auto cached=c;
    for(unsigned state:{0u,14u}) {
        c=cached;m.words[Message+0xf38]=state;
        std::array<U32,16> r{};r[0]=Message;r[14]=0x1234;r[15]=0x42cbac;
        const auto before=m.words;
        c.Draw(m,r,1033333,true);
        assert(c.closing && r[0]==model && r[15]==0x3f9680 && r[14]==0x1234 && m.words==before);
        r[0]=model;r[1]=view;assert(c.Transform(m,r,1066666));
        assert(std::abs(std::bit_cast<float>(*m.Read(view))-1.05f)<.00001f);
        assert(std::abs(std::bit_cast<float>(*m.Read(view+12))+10)<.001f); // Anchored center.
        for(const auto& [a,v]:before)if(a<view||a>=view+48)assert(m.Read(a)==v);
        for(unsigned i=0;i<12;++i)m.words[view+i*4]=before.at(view+i*4);
        r[0]=Message;r[15]=0x42cbac;c.Draw(m,r,1200001,true);assert(!c.ready && r[15]==0x42cbac);
    }
    c=cached;std::array<U32,16> r{};r[0]=Message;r[15]=0x42cbac;
    c.Draw(m,r,1200000,true);assert(!c.ready); // Stale plate cannot reappear.
    c=cached;++m.words[Message+0xf40];c.Draw(m,r,1033333,true);assert(!c.ready);--m.words[Message+0xf40];
    c=cached;layout.mm_close=false;Textboxes::SetLayout(layout,false);c.Draw(m,r,1033333,true);assert(!c.ready);
    layout.mm_close=true;Textboxes::SetLayout(layout,false);c=cached;
    ++Interpolation::generation;c.Draw(m,r,1033333,true);assert(!c.ready);--Interpolation::generation;
    c=cached;c.Draw(m,r,1033333,false);assert(!c.ready);
    c=cached;++m.words[play+0x104];c.Draw(m,r,1033333,true);assert(!c.ready);--m.words[play+0x104];
    c=cached;++m.words[model+4];c.Draw(m,r,1033333,true);assert(!c.ready);--m.words[model+4];
    c=cached;m.words[Message+0xf38]=14;c.Draw(m,r,1033333,true);assert(c.closing);
    m.words[Message+0xf38]=2;r[0]=Message;r[15]=0x42cbac;c.Draw(m,r,1066666,true);
    assert(!c.closing && r[15]==0x42cbac);
    // Actual top-screen continuation arrives with a scene-local message in
    // r0, not Message. Closing the global dialogue must still submit its plate.
    c=cached;m.words[Message+0xf38]=0;r={};r[0]=play+0x32c0;r[13]=0x9000000;
    r[14]=PresentSite;r[15]=PresentSite+4;m.words[r[13]+0x40]=0x12345678;
    for(unsigned i=1;i<13;++i)r[i]=0xcafe0000+i;
    std::array<U32,64> fp{};for(unsigned i=0;i<64;++i)fp[i]=0x12340000+i;
    U32 flags=0xa0000010,fpscr=0x01000000;
    const auto oldr=r;const auto oldfp=fp;
    c.Present(m,r,fp,flags,fpscr,1033333,true);
    assert(c.pending && r[0]==model && r[15]==0x3f9680 && r[14]==PresentSite);
    r.fill(0);fp.fill(0);flags=fpscr=0;
    c.Present(m,r,fp,flags,fpscr,1033333,true);
    assert(!c.pending && r[0]==0x12345678 && r[15]==PresentSite+4);
    for(unsigned i=1;i<15;++i)assert(r[i]==oldr[i]);
    assert(fp==oldfp && flags==0xa0000010 && fpscr==0x01000000);
    // Captured failure: the plate was submitted just before the close ends,
    // then its transform ran after expiry and exposed the native 1x plate.
    for(const auto delay:{1LL,1000LL,33333LL}) {
        c=cached;c.closing=true;c.started=1033333;
        r=oldr;fp=oldfp;flags=0xa0000010;fpscr=0x01000000;
        const auto submit=c.started+Duration-1;
        c.Present(m,r,fp,flags,fpscr,submit,true);assert(c.pending);
        const auto before=m.words;
        r[0]=model;r[1]=view;
        assert(c.Transform(m,r,submit+delay));
        assert(std::abs(std::bit_cast<float>(*m.Read(view))-Scale(Duration-1))<1e-7f);
        for(const auto& [a,v]:before)if(a<view||a>=view+48)assert(m.Read(a)==v);
        m.words[Message+0xf38]=2;assert(!c.Transform(m,r,submit+delay));
        m.words[Message+0xf38]=0;
        for(unsigned i=0;i<12;++i)m.words[view+i*4]=before.at(view+i*4);
        c.Present(m,r,fp,flags,fpscr,submit+delay,true);
        assert(!c.pending && r[0]==0x12345678 && fp==oldfp);
        c.Present(m,r,fp,flags,fpscr,submit+delay,true);
        assert(!c.pending && !c.ready); // No extra draw after completion.
    }
    assert(m.Read(0x300550)==0xeb04b194); // Native scene-local draw precedes this site.
    Textboxes::SetLayout({},false);
    std::cout<<"MM close curve, native signatures, plate-only draw, anchored transform, cleanup isolation and cancellation checks passed\n";
}
