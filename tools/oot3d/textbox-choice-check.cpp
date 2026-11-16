// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_description.h"
#include "oot3d/textbox_interpolation_hooks.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::TextboxIndicator;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a){auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Write(U32 a,U32 v){words[a]=v;}
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);
    U32 word;for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.Write(a,word);
    assert(PauseDescription::CodeMatches(m));
    assert(m.Read(0x305670)==0xe2840044); // Choice update passes Message+44.
    assert(m.Read(0x48b8b0)==0xe5900530 && m.Read(0x2c453c)==0xe5900530);
    assert(m.Read(0x2c4544)==0xe5c0106c); // Hide pane, not its model/resources.
    constexpr U32 pane=0x8000000,model=0x8010000,view=0x8020000,play=0x8030000,
        mesh=0x8040000,desc=0x8050000,buffer=0x8060000,alpha=0x8070000;
    m.Write(Message,0x4ed0a0);m.Write(Message+0x574,pane);m.Write(Message+8,play);m.Write(Message+0xf38,7);
    m.Write(HUDFade::UI+0xc,play);m.Write(play+0x104,0x55);m.Write(pane,0x4ed234);
    m.Write(pane+4,Message+0x168);m.Write(pane+0x64,model);m.Write(model,0x4ebd60);m.Write(model+4,mesh);
    m.Write(mesh,desc);m.Write(desc+12,4);m.Write(desc+0x1c,0x80);m.Write(mesh+0x124,0);m.Write(mesh+0x1a0,buffer);
    m.Write(model+0x174,4);
    for(unsigned off:{0x158u,0x15cu,0x164u})m.Write(mesh+off,0x12340000+off);
    for(unsigned i=0;i<36;++i)m.Write(buffer+4*i,std::bit_cast<U32>(float(i)));
    for(unsigned i=0;i<12;++i) {
        m.Write(model+12+4*i,std::bit_cast<U32>(i==3?80.f:i==7?196.f:float(i==0||i==5||i==10)));
        m.Write(view+4*i,std::bit_cast<U32>(float(i==0||i==5||i==10)));
    }
    auto layout=Textboxes::GetLayout();layout.indicator_animation.visible=false;
    layout.indicator_animation.spring=true;layout.indicator_animation.speed=.25f;
    Textboxes::SetLayout(layout,false); // Advance-icon settings must not hide/spring choices.
    ChoiceState choice;State advance;
    assert(!advance.Identify(m,model) && choice.Identify(m,model));
    std::array<U32,16> r{};r[0]=model;r[1]=view;r[13]=0x8080000;
    std::array<U32,64> fp{};for(unsigned i=0;i<64;++i)fp[i]=0x12340000+i;
    U32 flags=0xa0000010,fpscr=0x1000000;
    m.Write(alpha,std::bit_cast<U32>(1.f));choice.Material(m,model,alpha,1000000,true);assert(*m.Read(alpha)==0);
    assert(choice.Transform(m,r,1000000,true) && choice.ready);
    m.Write(alpha,std::bit_cast<U32>(1.f));choice.Material(m,model,alpha,1040000,true);
    assert(*m.Read(alpha)==std::bit_cast<U32>(.5f));
    m.Write(alpha,std::bit_cast<U32>(1.f));choice.Material(m,model,alpha,1080000,true);
    assert(*m.Read(alpha)==std::bit_cast<U32>(1.f));
    m.Write(play+0x18,1);choice.Press(m,1080000);assert(!choice.animation.pressed);
    m.Write(play+0x18,2);choice.Press(m,1080000);assert(!choice.animation.pressed);
    assert(choice.animation.Scale(1125000,ChoiceState::Settings())==(std::array<float,2>{1,1}));
    Interpolation::enabled=false;TextboxInterpolation::Capture(m,model,view);
    assert(TextboxInterpolation::routes.back().source[3]==3);
    assert(TextboxInterpolation::routes.back().source[2]==pane);Interpolation::enabled=true;
    choice.Present(m,r,fp,flags,fpscr,1080000,true);assert(!choice.pending);
    auto visible=choice;
    choice.Present(m,r,fp,flags,fpscr,1090000,true);assert(choice.pending && !choice.animation.visible);
    choice.Resume(r,fp,flags,fpscr);
    choice.Present(m,r,fp,flags,fpscr,1130000,true);assert(choice.pending);
    m.Write(alpha,std::bit_cast<U32>(1.f));choice.Material(m,model,alpha,1131000,true);
    assert(*m.Read(alpha)==std::bit_cast<U32>(.5f));choice.Resume(r,fp,flags,fpscr);
    choice.Present(m,r,fp,flags,fpscr,1170000,true);assert(!choice.pending);
    // Simultaneous retained advance/choice draws must restore the continuation
    // exactly once, without repeating either model or losing CPU state.
    Textboxes::SetLayout({},false);
    constexpr U32 advance_pane=pane+0x1000,advance_model=model+0x1000;
    m.Write(Message+0x570,advance_pane);m.Write(advance_pane,0x4ed234);
    m.Write(advance_pane+4,Message+0x168);m.Write(advance_pane+0x64,advance_model);
    m.Write(advance_model,0x4ebd60);m.Write(advance_model+4,mesh);
    for(unsigned i=0;i<12;++i)m.Write(advance_model+12+4*i,*m.Read(model+12+4*i));
    PauseDescription::Hook hook;hook.choice=visible;
    m.Write(alpha,std::bit_cast<U32>(1.f));hook.indicator.Material(m,advance_model,alpha,1000000,true);
    r[0]=advance_model;r[1]=view;hook.indicator.Transform(m,r,1080000,true);hook.indicator.seen=false;
    r[0]=play+0x32c0;r[14]=TextboxClose::PresentSite;r[15]=TextboxClose::PresentSite+4;
    m.Write(r[13]+0x40,0x12345678);const auto original=r;
    const auto saved_fp=fp;const auto before=m.words;
    hook.Present(m,r,fp,flags,fpscr,1090000,true);assert(hook.indicator.pending && r[0]==advance_model);
    r.fill(0);fp.fill(0);flags=fpscr=0;
    hook.Present(m,r,fp,flags,fpscr,1090001,true);assert(hook.choice.pending && r[0]==model && !hook.indicator.pending);
    r.fill(0);fp.fill(0);flags=fpscr=0;
    hook.Present(m,r,fp,flags,fpscr,1090002,true);
    assert(!hook.choice.pending && !hook.indicator.pending && !hook.close.pending);
    assert(r[0]==0x12345678 && r[15]==TextboxClose::PresentSite+4);
    for(unsigned i=1;i<15;++i)assert(r[i]==original[i]);
    assert(fp==saved_fp && flags==0xa0000010 && fpscr==0x1000000 && m.words==before);
    std::cout<<"Choice pane native signatures, independent visibility, no spring, fades, interpolation-off filtering and retained-draw continuation passed\n";
}
