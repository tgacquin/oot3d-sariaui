// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/screen_model_hooks.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::HUD;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void F(U32 a,float v) {words[a]=std::bit_cast<U32>(v);}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
int main() {
    Memory m;RoutingState state;std::array<U32,16> r{};
    constexpr U32 rupees=0x8000000,keys=0x8010000,helper=0x8020000,vertices=0x8030000,
        timer=0x8040000,score=0x8050000,icons=0x8060000,number=0x8070000,
        timer_model=0x8100000,score_model=0x8110000,icon_model=0x8120000,number_model=0x8130000,
        view=0x8200000;
    m.words[Main+0x1c]=rupees;m.words[Main+0x20]=keys;m.words[Main+0x34]=1;
    m.words[rupees+8]=m.words[keys+8]=helper;
    const auto prepare=[&] {r[4]=Main;Handle(m,r,3,{},state);};
    prepare();assert(state.preparing_counter && !state.keys_ready && r[0]==rupees);
    prepare();assert(state.counter_ready && state.preparing_keys && r[0]==keys && r[15]==0x2fc950);
    prepare();assert(state.counter_ready && state.keys_ready && !state.preparing_keys && !state.counter_stage);
    // Native key draw is excluded at state >= 12, even when rupees remain visible.
    m.words[Main+0x34]=0x14;prepare();prepare();assert(state.counter_ready && !state.keys_ready);
    m.words[Main+0x34]=0x17;prepare();assert(!state.counter_ready && !state.keys_ready);
    // Missing rupees must not prevent preparing the independently owned keys.
    m.words[Main+0x34]=1;m.words[Main+0x1c]=0;prepare();
    assert(state.preparing_keys && r[0]==keys);prepare();assert(state.keys_ready && !state.counter_ready);
    const auto quads=[&](unsigned count,float x,float y) {
        m.words[helper]=count;m.words[helper+0x10]=vertices;
        for(unsigned q=0;q<count;++q) {
            const std::array<float,12> v{x,y,7,x+18,y,7,x,y+18,7,x+18,y+18,7};
            for(unsigned i=0;i<12;++i)m.F(vertices+48*q+4*i,v[i]);
        }
    };
    Layout layout;layout.hd_numbers=false; // This fixture checks native digit geometry.
    layout[Keys]={60,-10,1.5f};
    quads(92,2,151);m.words[Main+4]=helper;r[4]=Main;Handle(m,r,0,layout,state);
    const float key_x=m.F(vertices+27*48),key_y=m.F(vertices+27*48+4);
    assert(Near(key_x,72) && Near(key_y,182));
    quads(4,20,151);r[4]=keys;Handle(m,r,2,layout,state);
    assert(Near(m.F(vertices)-key_x,18*1.5f) && Near(m.F(vertices+4),key_y));
    // Full-sized hidden digits cannot be pulled into view by the new controls.
    quads(4,320,151);Handle(m,r,2,layout,state);
    assert(m.F(vertices)==m.F(vertices+12));
    // Queue the freshly prepared key number on top, and suppress its bottom
    // duplicate by the same upload hash used for the rupee counter.
    constexpr U32 queue=0x5c0ba8,entries=queue+5*0x60+0x214,priorities=queue+5*0x60+0x4b4;
    m.words[Main+4]=0;m.words[keys+0x10]=number_model;m.words[number_model]=0x4ebd60;
    m.words[helper+0x14]=vertices+0x1000;m.words[helper+0x18]=vertices+0x2000;
    for(unsigned i=0;i<4*32;i+=4)m.F(vertices+0x1000+i,.25f);
    for(unsigned i=0;i<8*64;i+=4)m.F(vertices+0x2000+i,1.f);
    for(unsigned i=0;i<24;++i)m.words[entries+4*i]=m.words[priorities+4*i]=0;
    m.words[queue+0x28]=0;r[0]=queue;r[1]=5;Handle(m,r,4,layout,state);
    const auto mesh=Mesh(m,helper);assert(mesh);
    assert(m.words[queue+0x28]==1 && m.words[entries]==number_model && IsRouted(mesh->hash,4));
    m.words[queue+0x28]=0;r[0]=queue;r[1]=5;Handle(m,r,4,layout,state,true);
    assert(m.words[queue+0x28]==0);
    m.words[Action+0x2c]=timer;m.words[timer+0x10]=timer_model;
    m.words[Action+0x30]=score;m.words[score+0x10]=score_model;
    m.words[Action+0x40]=icons;m.words[icons+8]=icon_model;
    m.words[icons+0xc]=number;m.words[number+0x10]=number_model;
    m.words[icons+0x428]=2;
    constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
    for(U32 model:{timer_model,score_model,icon_model,number_model}) {
        m.words[model]=0x4ebd60;
        for(unsigned i=0;i<12;++i)m.F(model+0xc+4*i,identity[i]);
    }
    layout[Timer]={23,-31,1.4f};layout[Minigame]={-40,15,.7f};SetLayout(layout,false);
    // The native timer symbol and its separate digits receive exactly the
    // same screen transform; likewise score symbols, item icons and numbers.
    for(auto [model,element,q]:std::array<std::tuple<U32,Element,unsigned>,4>{{
            {timer_model,Timer,5},{score_model,Minigame,6},{icon_model,Minigame,6},{number_model,Minigame,6}}}) {
        assert(ScreenModels::ExtraHUDModel(m,model)==element);
        for(unsigned i=0;i<12;++i)m.F(view+4*i,identity[i]);
        const auto before=m.words;
        const auto t=ScreenModels::Select(m,model,{},{});assert(t);
        assert(ScreenModels::Apply(m,view,model,*t));
        const float x=190,y=220;
        const float expected_x=m.F(view)*x+m.F(view+12),expected_y=m.F(view+20)*y+m.F(view+28);
        quads(8,x,y);m.words[Action+0x18]=helper;r[4]=Action;Handle(m,r,1,layout,state);
        assert(Near(m.F(vertices+48*q),expected_x) && Near(m.F(vertices+48*q+4),expected_y));
        for(unsigned i=0;i<12;++i)assert(m.words[model+0xc+4*i]==before.at(model+0xc+4*i));
    }
    assert(!ScreenModels::ExtraHUDModel(m,0) && !ScreenModels::ExtraHUDModel(m,keys));
    // Unknown item-controller kinds must not capture unrelated number owners.
    m.words[icons+0x428]=7;assert(!ScreenModels::ExtraHUDModel(m,number_model));
    m.words[icons+0x428]=0;m.words[icons+0x224]=1;
    assert(ScreenModels::ExtraHUDModel(m,number_model)==Minigame);
    m.words[icons+0x224]=65;assert(!ScreenModels::ExtraHUDModel(m,number_model));
    // The shard's base and animated glow scale about a shared center;
    // the targeting hint has its own independent, persistent pose.
    constexpr U32 shard=0x8300000,glow=0x8310000,target=0x8320000;
    m.words[Action+0x80]=shard;m.words[Action+0x88]=glow;m.words[Action+0x78]=target;
    layout[ShardAgony]={90,-12,1.7f};layout[TargetIndicator]={-75,43,.4f};SetLayout(layout,false);
    for(U32 model:{shard,glow,target}) {
        m.words[model]=0x4ebd60;
        const auto element=model==target?TargetIndicator:ShardAgony;
        assert(ScreenModels::ExtraHUDModel(m,model)==element);
        const auto t=ScreenModels::Select(m,model,{},{});assert(t);
        assert(t->x==layout[element].x && t->y==layout[element].y && t->sx==layout[element].scale && t->sy==t->sx);
        assert(t->ax==(model==target?TargetAnchorX:ShardAnchorX));
        assert(t->ay==(model==target?TargetAnchorY:ShardAnchorY));
    }
    // Gallery mode: move the native B bubble with its item/count and hide all
    // other action quads. Other special B modes retain their ordinary HUD.
    constexpr U32 action_model=0x8380000;
    m.words[icons+0x428]=2; // Restore the valid gallery item/count after guard tests.
    m.words[Action+0x20]=action_model;m.words[action_model]=0x4ebd60;
    auto buttons=HUDButtons::GetLayout();buttons.buttons[HUDButtons::B].pose={260,70,1.5f};
    HUDButtons::SetLayout(buttons,false);
    UserHUD::Layout user{};user[UserHUD::BButton]={12,-4,20};UserHUD::SetLayout(user,false);
    for(unsigned mode=0;mode<7;++mode) {
        m.words[Action+0x48]=mode;quads(8,190,220);r[4]=Action;
        const bool special_b=mode>=1 && mode<=5;
        assert(ScreenModels::HideSpecialBWhenPaused(m,icon_model)==special_b);
        assert(ScreenModels::HideSpecialBWhenPaused(m,number_model)==special_b);
        assert(!ScreenModels::HideSpecialBWhenPaused(m,timer_model));
        assert(!ScreenModels::HideSpecialBWhenPaused(m,action_model));
        Handle(m,r,1,layout,state);
        if(special_b) {
            const auto t=ScreenModels::Select(m,icon_model,{},{});assert(t);
            const auto bubble=ScreenModels::Select(m,action_model,{},{});
            const auto ammo=ScreenModels::Select(m,number_model,{},{});
            assert(!bubble && ammo && Near(t->sx,1.8f));
            assert(t->x==ammo->x && t->y==ammo->y && t->sx==ammo->sx);
            assert(Near(t->ax+t->x,272) && Near(t->ay+t->y,66));
            assert(Near(m.F(vertices+7*48),272+(190-336)*1.8f));
            assert(Near(m.F(vertices+7*48+4),66+(220-215.5f)*1.8f));
            if(mode==4)for(unsigned q=0;q<7;++q)assert(m.F(vertices+q*48)==m.F(vertices+q*48+12));
        } else assert(m.F(vertices+7*48+12)-m.F(vertices+7*48)==18);
    }
    // Fishing and horseback share the native -35 A offset. Both must land
    // exactly on the ordinary layout at every scale, including the label.
    // Other modes must not inherit that correction. Keep native press offsets.
    for(unsigned mode:{0u,3u,5u,6u})for(float scale:{.1f,.64f,1.f,3.f}) {
        m.words[Action+0x48]=mode;
        layout[AButton]={-20,240,scale};
        quads(8,350,mode==3 || mode==5?175:210);r[4]=Action;Handle(m,r,1,layout,state);
        for(unsigned q=0;q<2;++q) {
            assert(Near(m.F(vertices+48*q),368+(350-368)*scale-20));
            assert(Near(m.F(vertices+48*q+4),214+(210-214)*scale+240));
        }
    }
    m.words[Action+0x48]=5;
    UserHUD::SetLayout({},false);
    for(const auto pose:std::array<PausePrompts::Pose,3>{{{90,80,.1f},{260,70,1.5f},{-400,-240,4}}}) {
        buttons.buttons[HUDButtons::B].pose=pose;HUDButtons::SetLayout(buttons,false);
        quads(8,312,192);Handle(m,r,1,layout,state);
        const auto t=ScreenModels::Select(m,icon_model,{},{});assert(t);
        const auto ammo=ScreenModels::Select(m,number_model,{},{});assert(ammo);
        assert(Near(m.F(vertices+7*48),pose.x-24*pose.scale));
        assert(Near(m.F(vertices+7*48+4),pose.y-23.5f*pose.scale));
        assert(t->sx==pose.scale && t->sx==ammo->sx && t->x==ammo->x && t->y==ammo->y);
    }
    buttons.buttons[HUDButtons::B].enabled=false;HUDButtons::SetLayout(buttons,false);
    quads(8,312,192);Handle(m,r,1,layout,state);
    assert(m.F(vertices+7*48)==m.F(vertices+7*48+12));
    assert(ScreenModels::Select(m,icon_model,{},{})->sx==0);
    // Fresh material alpha suppression includes the shard/glow and target hint,
    // but leaves the gallery icon and ammunition material untouched.
    constexpr U32 renderer=0x8400000,stack=0x8500000;
    r[4]=renderer;r[13]=stack;m.words[Action+0x48]=4;
    m.words[keys+0x10]=0; // Number model below belongs only to the gallery item.
    for(U32 model:{shard,glow,target,timer_model,score_model,icon_model,number_model}) {
        m.words[renderer+0x354]=model;m.F(stack+0x38,.8f);
        Handle(m,r,8,layout,state);
        assert(Near(m.F(stack+0x38),(model==icon_model || model==number_model)?.8f:0.f));
    }
    m.words[Action+0x48]=0;
    HUDButtons::SetLayout({},false);UserHUD::SetLayout({},false);
    // Clock alpha follows A's vertex alpha on each fresh upload. Timer
    // digits follow A's effective material alpha and the shared return fade.
    m.words[Action+0x48]=0;
    constexpr U32 colors=0x8420000;
    m.words[Action+0x18]=helper;m.words[helper+0x18]=colors;
    for(auto [at,word]:HUDFade::Guards)m.words[at]=word;
    // Native timer-off uses transparent colors on an otherwise visible quad.
    // Include inactive/full/fractional visibility with every A fade phase.
    for(float native_clock:{0.f,.5f,1.f})for(float opacity:{0.f,.25f,.75f,1.f}) {
        quads(8,190,220);
        for(unsigned i=0;i<8*16;++i)m.F(colors+i*4,1);
        for(unsigned i=0;i<4;++i)m.F(colors+i*16+12,opacity);
        for(unsigned i=0;i<4;++i)m.F(colors+5*64+i*16+12,native_clock);
        r[4]=Action;Handle(m,r,1,layout,state);
        for(unsigned i=0;i<4;++i) {
            assert(Near(m.F(colors+5*64+i*16+12),native_clock*opacity));
            assert(m.F(colors+5*64+i*16)==1); // RGB is untouched.
        }
        state.fade.action_opacity=opacity;state.hud_return.returning=true;
        state.hud_return.opacity=.5f;state.hud_return.hidden=false;
        m.words[renderer+0x354]=timer_model;m.F(stack+0x38,1);
        r[4]=renderer;r[13]=stack;Handle(m,r,8,layout,state);
        assert(Near(m.F(stack+0x38),opacity*.5f));
        state.hud_return.hidden=true;Handle(m,r,8,layout,state);
        assert(m.F(stack+0x38)==0);
    }
    SetLayout({},false);assert(!ScreenModels::Select(m,timer_model,{},{}));
    std::cout<<"Key preparation gates, icon/count pairing, native hiding, timer/minigame ownership and shared transforms passed\n";
}
