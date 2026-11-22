// SPDX-License-Identifier: GPL-2.0-or-later
// c++ -std=c++20 -Isrc tools/oot3d/minimap-control-check.cpp -o /tmp/oot-minimap-check
#include "oot3d/hud_hooks.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <map>
using namespace OOT3D;
using namespace OOT3D::MinimapControl;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {
        const auto it=words.find(a);return it==words.end()?std::nullopt:std::optional<U32>{it->second};
    }
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void Float(U32 a,float v) {words[a]=std::bit_cast<U32>(v);}
    float Float(U32 a) {return std::bit_cast<float>(words.at(a));}
};
bool Near(float a,float b){return std::abs(a-b)<.00001f;}
int main() {
    Reset();assert(Opacity()==1);
    RequestToggle();Consume(false);assert(shown); // Menus/title reject, never defer.
    Consume(true);assert(shown);
    RequestToggle();Consume(true);assert(!shown && Opacity()==0);
    assert(Opacity()==0); // Repeated draw/second eye stays hidden.
    RequestToggle();Consume(true);assert(shown && Opacity()==1);
    RequestToggle();RequestToggle();Consume(true);assert(shown && Opacity()==1);
    RequestToggle();Consume(true);assert(Opacity()==0);
    Reset();assert(Opacity()==1); // New session starts visible.

    Memory m;
    constexpr U32 model=0x10000000,renderer=0x10001000,stack=0x10002000;
    constexpr U32 marker=0x10003000,marker_model=0x10004000;
    m.words[Owner+8]=model;m.words[model]=0x4ebd60;
    m.words[Owner+0x20]=marker;m.words[marker+4]=marker_model;
    m.words[marker_model]=0x4ebd60;
    m.Float(stack+0x38,.6f);
    Material(m,model,stack+0x38,.5f);assert(Near(m.Float(stack+0x38),.3f));
    m.Float(stack+0x38,.8f);Material(m,marker_model,stack+0x38,0);
    assert(m.Float(stack+0x38)==0);
    m.Float(stack+0x38,.8f);const auto original=m.words;
    Material(m,model+0x100,stack+0x38,0);assert(m.words==original);
    m.Float(stack+0x38,0);Material(m,model,stack+0x38,1);assert(m.Float(stack+0x38)==0);

    // Exercise the production material hook; native model material stays intact.
    m.words[renderer+0x354]=model;m.Float(model+0xfc,.6f);m.Float(stack+0x38,.6f);
    HUD::RoutingState routing;Reset();
    RequestToggle();Consume(true);
    std::array<U32,16> regs{};regs[4]=renderer;regs[13]=stack;
    HUD::Handle(m,regs,8,{},routing,false,1075000);
    assert(regs[1]==stack+0x2c && m.Float(stack+0x38)==0);
    // A second click restores the full native opacity even at the same time.
    RequestToggle();Consume(true);m.Float(stack+0x38,.6f);
    HUD::Handle(m,regs,8,{},routing,false,1075000);
    assert(Near(m.Float(stack+0x38),.6f));
    assert(Near(m.Float(model+0xfc),.6f));

    constexpr U32 action_model=0x10005000,helper=0x10006000,mesh=0x10007000;
    constexpr U32 src=0x10008000,dst=0x10009000;
    m.words[Action+0x20]=action_model;m.words[Action+0x18]=helper;
    m.words[action_model]=0x4ebd60;m.words[action_model+4]=mesh;
    m.words[helper]=8;m.words[helper+0x18]=src;
    for(unsigned i=0;i<128;++i){m.Float(src+4*i,(i%4==3)?.8f:.25f);m.Float(dst+4*i,-1);}
    const auto before=m.words;
    assert(CopyArrows(m,mesh,dst,src,512,.5f));
    for(unsigned i=0;i<128;++i) {
        assert(m.words.at(src+4*i)==before.at(src+4*i));
        const bool arrow=i>=32 && i<64 && i%4==3;
        assert(Near(m.Float(dst+4*i),arrow?.4f:m.Float(src+4*i)));
    }
    assert(CopyArrows(m,mesh,dst,src,512,.5f));
    assert(Near(m.Float(dst+35*4),.4f)); // No compounded fade.
    assert(CopyArrows(m,mesh,dst,src,512,0));assert(m.Float(dst+35*4)==0);
    const auto hidden=m.words;
    assert(!CopyArrows(m,mesh+4,dst,src,512,.5f));assert(m.words==hidden);
    assert(!CopyArrows(m,mesh,dst,src,256,.5f));
    assert(!CopyArrows(m,mesh,src,src,512,.5f));
    assert(!CopyArrows(m,mesh,dst,src,512,1)); // Native copy restores arrows.
    // Room/floor replacement: old map pending teardown, absent map, async
    // load, zero-alpha first frame, then native fade-in. The A button shares
    // the upload but must remain untouched throughout.
    constexpr U32 map_mesh=0x1000a000,map_helper=0x1000b000,map_colors=0x1000c000;
    m.words[Owner+0x50]=1;m.words[Owner+0x60]=0;m.words[Owner+0x58]=0;
    m.words[Owner+4]=map_mesh;m.words[model+4]=map_mesh;
    m.words[Owner+0x18]=map_helper;m.words[map_helper]=1;m.words[map_helper+0x18]=map_colors;
    m.Float(model+0xfc,1);
    const auto map_fade=[&](float alpha) {for(unsigned v=0;v<4;++v)m.Float(map_colors+v*16+12,alpha);};
    map_fade(1);assert(DungeonMarkerOpacity(m)==1);
    const auto arrows=[&](float alpha) {
        const auto original=m.words;
        assert(CopyArrows(m,mesh,dst,src,512,1));
        for(unsigned i=0;i<128;++i) {
            assert(m.words.at(src+4*i)==original.at(src+4*i));
            const bool arrow=i>=32 && i<64 && i%4==3;
            assert(Near(m.Float(dst+4*i),arrow?std::min(.8f,alpha):m.Float(src+4*i)));
        }
        m.Float(stack+0x38,.6f);Material(m,marker_model,stack+0x38,1);
        assert(Near(m.Float(stack+0x38),std::min(.6f,alpha)));
    };
    m.words[Owner+0x60]=1;arrows(0);m.words[Owner+0x60]=0;
    m.words[Owner+8]=0;arrows(0);m.words[Owner+8]=model;
    m.words[Owner+0x58]=1;arrows(0);m.words[Owner+0x58]=0;
    for(float alpha:{0.f,.1f,.5f,.9f}) {map_fade(alpha);arrows(alpha);}
    map_fade(1);assert(!CopyArrows(m,mesh,dst,src,512,1));
    // Existing scene fades are capped, not multiplied a second time.
    m.Float(model+0xfc,.4f);arrows(.4f);m.Float(model+0xfc,1);
    map_fade(0);m.words[Owner+0x50]=0;
    assert(DungeonMarkerOpacity(m)==1 && !CopyArrows(m,mesh,dst,src,512,1)); // Overworld unchanged.
    auto personal=UserHUD::GetLayout();personal.overall_opacity=.5f;personal.minimap_opacity=.25f;
    UserHUD::SetLayout(personal,false);
    m.words[renderer+0x354]=model;
    for(unsigned dungeon:{0u,1u}) {
        m.words[Owner+0x50]=dungeon;map_fade(1);
        m.Float(stack+0x38,.8f);
        HUD::Handle(m,regs,8,HUD::GetEffectiveLayout(),routing);
        assert(Near(m.Float(stack+0x38),.1f));
        assert(CopyArrows(m,mesh,dst,src,512,1));
        assert(Near(m.Float(dst+35*4),.2f) && Near(m.Float(dst+3*4),m.Float(src+3*4)));
    }
    UserHUD::SetLayout({},false);
    m.Float(src+35*4,std::numeric_limits<float>::quiet_NaN());
    const auto invalid=m.words;assert(!CopyArrows(m,mesh,dst,src,512,.5f));assert(m.words==invalid);
    Reset();
    std::cout<<"Minimap requests, instant hide/show, native material preservation and isolated arrow uploads passed\n";
}
