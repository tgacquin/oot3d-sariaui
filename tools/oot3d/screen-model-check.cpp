// SPDX-License-Identifier: GPL-2.0-or-later
// c++ -std=c++20 -Isrc tools/oot3d/screen-model-check.cpp -o /tmp/oot-screen-check
#include "oot3d/screen_model_hooks.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
using namespace OOT3D;
using namespace OOT3D::ScreenModels;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {
        auto it=words.find(at);return it==words.end()?std::nullopt:std::optional<U32>{it->second};
    }
    void Write(U32 at,U32 value) {assert(words.contains(at));words[at]=value;}
    float Float(U32 at) {return std::bit_cast<float>(words.at(at));}
    void Matrix(U32 at, const std::array<float,12>& matrix) {
        for(unsigned i=0;i<12;++i)words[at+i*4]=std::bit_cast<U32>(matrix[i]);
    }
};
constexpr U32 model=0x10000000,view=0x10001000,pane=0x10002000,text=0x10003000;
constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
bool Near(float a,float b){return std::abs(a-b)<.001f;}
int main(int argc,char**argv) {
    Memory m;
    if(argc>1) {
        std::ifstream f(argv[1],std::ios::binary);assert(f);U32 word;
        for(U32 a=0x100000;f.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
        assert(CodeMatches(m));m.words[Sites[0].pc+4]^=1;assert(!CodeMatches(m));m.words.clear();
    }
    for(unsigned i=0;i<Sites.size();++i) {
        assert(Instruction(Sites[i].pc,Sites[i].instruction)==(0xef000000|FirstSVC+i));
        assert(Instruction(Sites[i].pc,0)==0);
    }
    // Hide every gameplay minimap layer and the action atlas (player/entry
    // arrows) during pause, without hiding the independent page title.
    constexpr U32 marker=0x10004000,title=0x10005000;
    m.words[model]=m.words[title]=0x4ebd60;
    assert(!HideDuringPause(m,model));
    m.words[Minimap+8]=model;assert(IsMinimapModel(m,model) && HideDuringPause(m,model));
    m.words.erase(Minimap+8);
    for(U32 off:{0x20u,0x28u}) {
        m.words[Minimap+off]=marker;m.words[marker+4]=model;
        assert(IsMinimapModel(m,model) && HideDuringPause(m,model));m.words.erase(Minimap+off);
    }
    m.words[HUD::Action+0x20]=model;m.words[HUD::Action+0x28]=title;
    assert(HideDuringPause(m,model) && !HideDuringPause(m,title) && !HideDuringPause(m,0));
    auto setup=[&](unsigned slot) {
        m.words.clear();m.words[model]=0x4ebd60;m.Matrix(model+0xc,identity);
        m.words[model+0x18]=std::bit_cast<U32>(200.f);
        m.words[model+0x28]=std::bit_cast<U32>(195.f);
        m.Matrix(view,identity);
        m.words[Message]=0x4ed0a0;m.words[Message+0xf38]=5;
        m.words[Message+0x568+slot*4]=pane;
        m.words[pane]=0x4ed234;m.words[pane+4]=Message+0x168;
        m.words[pane+0x64]=model;
    };
    std::array<U32,16> regs{};regs[1]=view;
    auto run=[&](const Textboxes::Layout& box,const HUD::Pose& map=HUD::Pose{}) {
        regs[0]=model;Handle(m,regs,box,map);assert(regs[2]==model+0xc);
    };
    // Sign offset covers the plate, auxiliary panes, arrow and lettering;
    // dialogue and missing/other style IDs retain their prior positions.
    for(unsigned slot:{1u,2u,4u,256u})for(unsigned style:{0u,1u,2u}) {
        setup(slot==256?4:slot);m.words[Message+0xf48]=style;
        if(slot==256){m.words.erase(pane+0x64);m.words[Message+0x974]=text;
            m.words[Message+0x978]=0;m.words[text+8]=model;}
        Textboxes::Layout signs;signs.sign_y=-75;
        run(signs);assert(m.Float(view+28)==(style==1?-75.f:0.f));
    }
    setup(1);auto native=m.words;
    run({});assert(m.words==native);
    Textboxes::Layout box{10,-40,1.5f,{-20,-10,2}};
    run(box);assert(m.Float(view)==1.5f && m.Float(view+20)==1);
    // The center moves by X only: 1.5*200 - 90 == 210.
    assert(m.Float(view+12)==-90 && m.Float(view+28)==-40);
    for(const auto& [at,value]:native)
        if(at<view || at>=view+48)assert(m.words.at(at)==value);
    const auto widened=m.words;
    m.words=native;run(box);assert(m.words==widened); // Fresh frame, no accumulated width.
    m.words=native;run({});assert(m.words==native); // Exact reset.
    setup(2);run(box);assert(m.Float(view)==2 && m.Float(view+20)==2);
    assert(m.Float(view+12)==-210 && m.Float(view+28)==-245);
    // Text and auxiliary message panes translate without scaling or spacing changes.
    setup(4);run(box);assert(m.Float(view)==1 && m.Float(view+20)==1);
    assert(m.Float(view+12)==10 && m.Float(view+28)==-40);
    setup(4);m.words.erase(pane+0x64);m.words[Message+0x974]=text;
    m.words[Message+0x978]=0;m.words[text+8]=model;
    run(box);assert(m.Float(view)==1 && m.Float(view+12)==10);
    // Overall scale shares a center across plate, lettering and indicator.
    setup(1);auto scaled=box;scaled.scale=1.5f;run(scaled);
    assert(Near(m.Float(view),2.25f) && Near(m.Float(view+20),1.5f));
    assert(Near(m.Float(view+12),-240) && Near(m.Float(view+28),-137.5f));
    // Give the arrow a different origin from the background and combine both scales.
    setup(2);constexpr U32 plate=0x10004000,background=0x10005000;
    m.words[Message+0x56c]=plate;m.words[plate]=0x4ed234;m.words[plate+4]=Message+0x168;
    m.words[plate+0x64]=background;m.words[background]=0x4ebd60;
    m.Matrix(background+0xc,{1,0,0,200,0,1,0,195,0,0,1,0});
    m.Matrix(model+0xc,{1,0,0,300,0,1,0,210,0,0,1,0});
    run(scaled);assert(Near(m.Float(view),3) && Near(m.Float(view+20),3));
    assert(Near(3*300+m.Float(view+12),330));
    assert(Near(3*210+m.Float(view+28),162.5f));
    m.Matrix(view,identity);m.words.erase(pane+0x64);m.words[Message+0x974]=text;
    m.words[Message+0x978]=0;m.words[text+8]=model;run(scaled);
    assert(Near(m.Float(view),1.5f) && Near(m.Float(view+20),1.5f));
    assert(Near(m.Float(view+12),-90) && Near(m.Float(view+28),-137.5f));
    setup(1);native=m.words;scaled.scale=0;run(scaled);assert(m.words==native);
    // Unrelated/shared menu text, inactive dialogue, invalid config, missing memory.
    setup(1);m.words[pane+4]=0x10008000;native=m.words;run(box);assert(m.words==native);
    setup(1);m.words[Message+0xf38]=0;native=m.words;run(box);assert(m.words==native);
    setup(1);native=m.words;auto invalid=box;invalid.width=std::numeric_limits<float>::quiet_NaN();
    run(invalid);assert(m.words==native);
    setup(1);m.words.erase(view+4);native=m.words;run(box);assert(m.words==native);
    // Map image and both markers share one screen-space anchor, never the rupee model.
    for(unsigned off:{8u,0x20u,0x28u}) {
        setup(1);m.words[Message+0xf38]=0;
        if(off==8)m.words[Minimap+off]=model;
        else {m.words[Minimap+off]=text;m.words[text+4]=model;}
        run({}, {80,-20,1.5f});
        assert(m.Float(view)==1.5f && m.Float(view+20)==1.5f);
        assert(m.Float(view+12)==60 && m.Float(view+28)==-122);
    }
    setup(1);m.words[Message+0xf38]=0;m.words[HUD::Action+0x78]=model;
    native=m.words;run({}, {80,-20,1.5f});assert(m.words==native);
    // The minigame weapon and its independent ammo model follow B together.
    for(unsigned mode=0;mode<=5;++mode)for(bool ammo:{false,true}) {
        setup(1);m.words[Message+0xf38]=0;m.words[HUD::Action+0x48]=mode;
        m.words[HUD::Action+0x40]=text;
        if(ammo){m.words[text+0x428]=2;m.words[text+0xc]=pane;m.words[pane+0x10]=model;}
        else m.words[text+8]=model;
        HUDButtons::Layout buttons;buttons.buttons[HUDButtons::B].pose={340,210,1.5f};
        HUDButtons::SetLayout(buttons,false);const auto baseline=Select(m,model,{},{});
        buttons.minigame_b_x=20;HUDButtons::SetLayout(buttons,false);
        const auto moved=Select(m,model,{},{});
        if(mode==0){assert(!baseline && !moved);continue;}
        assert(baseline && moved);
        assert(moved->x==baseline->x+(mode<=4?20:0));
        assert(moved->y==baseline->y && moved->sx==baseline->sx && moved->sy==baseline->sy);
    }
    HUDButtons::SetLayout({},false);
    // Pre-compose in screen space, including a rotated view. Preserve depth.
    setup(1);m.Matrix(view,{0,-2,0,12,3,0,0,15,0,0,1,-10});
    run(box);
    assert(Near(m.Float(view+12),92) && Near(m.Float(view+28),-255));
    assert(m.Float(view+16)==4.5f && m.Float(view+4)==-2);
    assert(m.Float(view+40)==1 && m.Float(view+44)==-10);
    std::cout << "Textbox/minimap ownership, proportions, transforms, reset and code signatures passed\n";
}
