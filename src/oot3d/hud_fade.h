// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"

namespace OOT3D::HUDFade {
using TitleScreen::U32;
constexpr U32 Queue=0x5c0ba8;
constexpr U32 UI=0x5043d4, Main=0x50af34, Action=0x4fc648;
constexpr std::int64_t FadeUs=150000;
// USA Rev 1: 2d3f2c takes an unused solid quad from Queue+f4,
// writes its center/size and RGBA, and inserts it into the selected pass.
// 3339e8 selects 400x240 for top passes and 320x240 for bottom passes.
constexpr std::array<std::pair<U32,U32>,23> Guards{{
    {0x2d3f2c,0xe92d43f0}, {0x2d3f84,0xe59140f4},
    {0x2d3ffc,0xe28400f0}, {0x2d4004,0xe880100e},
    {0x333a00,0xe3510006}, {0x3008d4,Queue}, {0x3fb9fc,0xed8d0a0e},
    {0x41ebf8,UI}, {0x41e968,0xe585400c},
    {0x41dedc,0xe5d00100}, {0x41dee4,0x05d60101},
    {0x42ea00,Main}, {0x42ea28,0x7f12},
    {0x2e2e80,0xe5d00025}, {0x2e2ef4,0xe5d40f12}, {0x2e2f20,0xe5da002d},
    {0x42b9bc,Action}, {0x42b858,0xe5940018}, {0x42bc24,0xe5970020},
    {0x2fc3e4,0xe5900018}, {0x4228b8,0xe3a01003},
    {0x4228c0,0xe3a01004}, {0x4228b0,0xe3a01005}}};
template<class M> bool CodeMatches(M& m) {
    for(auto [at,word]:Guards)if(m.Read(at)!=std::optional<U32>{word})return false;
    return true;
}
template<class M> float Visibility(M& m,unsigned pass) {
    const auto count=m.Read(Queue+0x14+4*pass),used=m.Read(Queue+8);
    if(!count || *count>24 || !used || *used>24)return 1;
    float visibility=1;
    for(unsigned i=0;i<*count;++i) {
        const auto entry=m.Read(Queue+0x214+pass*0x60+i*4);
        if(!entry || !TitleScreen::Pointer(*entry,0x174))continue;
        bool pooled=false;
        for(unsigned j=0;j<*used;++j)
            pooled|=m.Read(Queue+0xf4+j*4)==entry;
        if(!pooled)continue; // Never read stale, allocated-but-unused covers.
        std::array<float,6> rect;
        std::array<float,4> rgba;
        if(!TitleScreen::ReadFloats(m,*entry+0x3c,rect) ||
           !TitleScreen::ReadFloats(m,*entry+0xf0,rgba))continue;
        const float width=pass<6?400.f:320.f;
        if(rect!=std::array<float,6>{width/2,120,0,width,240,1})continue;
        // Scene blackouts/whiteouts only; partial rectangles and colored
        // gameplay effects are not a HUD visibility signal.
        if((rgba[0]!=0 && rgba[0]!=1) || rgba[1]!=rgba[0] || rgba[2]!=rgba[0] ||
           rgba[3]<0 || rgba[3]>1)continue;
        visibility*=1-rgba[3];
    }
    return visibility;
}
template<class M> bool Zero(M& m,U32 at,U32 mask) {
    const auto word=m.Read(at);
    return word && (*word&mask)==0;
}
template<class M> bool GameplayReady(M& m,U32 play) {
    // 41e944 publishes the CURRENT context before top drawing. Main's cached
    // play pointer can still refer to the previous scene during file loading.
    if(!TitleScreen::Pointer(play,0x104))return false;
    const auto context=m.Read(play+0x100);
    if(!context || (*context&0xffff)!=0x0203)return false;
    if(!TitleScreen::Pointer(play,0x7f28))return false;
    const auto running=m.Read(play+0x7f24),state=m.Read(Main+0x34);
    // 2e2e60 stops updating when +7f25 is zero. +5c2d requests a scene
    // transition, +7f12 stays active through its renderer's fade/black hold.
    // Neither requires a pooled fullscreen cover to exist in this frame.
    if(!running || (*running&0xff00)==0 || !state || *state==0 || *state>=0x17)
        return false;
    return Zero(m,play+0x5c2c,0xff00) && Zero(m,play+0x7f10,0xff0000);
}
struct State {
    float action_opacity=0,opacity=0,progress=0;
    bool override_alpha=false,initialized=false;
    U32 context=0;
    std::int64_t last_us=0;
};
// 300328 draws pass 3, the A button, pass 4, then our pass-5 HUD. Reset
// only the capture before each eye's A draw; the fade survives across frames.
inline void Begin(State& state) {state.action_opacity=0;state.override_alpha=false;}
template<class M> void CaptureAction(M& m,State& state,U32 model,U32 alpha) {
    if(!model || m.Read(Action+0x20)!=std::optional<U32>{model} ||
       m.Read(model)!=std::optional<U32>{0x4ebd60} || !CodeMatches(m))return;
    const U32 helper=m.Read(Action+0x18).value_or(0);
    if(!TitleScreen::Pointer(helper,0x20) || m.Read(helper)!=std::optional<U32>{8})return;
    const U32 colors=m.Read(helper+0x18).value_or(0);
    std::array<float,16> circle;
    if(!TitleScreen::ReadFloats(m,colors,circle))return;
    const auto word=m.Read(alpha);if(!word)return;
    const float material=std::bit_cast<float>(*word),vertex=circle[3];
    if(!std::isfinite(material) || material<0 || material>1 || vertex<0 || vertex>1 ||
       circle[7]!=vertex || circle[11]!=vertex || circle[15]!=vertex)return;
    state.action_opacity=material*vertex;
}
// Follow the actual A draw, including native fades. Pass 3 is behind the A
// button; bottom-screen dimming and disabled touch input do not dim it either.
// Only pass 4 overlays both the A button and the scene before our HUD draw.
template<class M> void Update(M& m,State& state,bool paused,std::int64_t now_us,
                              std::optional<float> return_opacity={}) {
    state.override_alpha=CodeMatches(m);
    if(!state.override_alpha) {state={};return;}
    const U32 play=m.Read(UI+0xc).value_or(0);
    const bool reset=!state.initialized || state.context!=play || now_us<state.last_us ||
                     now_us-state.last_us>250000;
    const float step=reset?0:float(now_us-state.last_us)/FadeUs;
    if(reset)state.progress=0;
    state.initialized=true;state.context=play;state.last_us=now_us;
    const float cover=Visibility(m,4);
    if(paused || !GameplayReady(m,play) || cover==0) {
        // Never carry a fading HUD into a loading hold or a fully covered scene.
        state.progress=state.opacity=0;return;
    }
    // A-button visibility is the target. Move at a fixed rate in both
    // directions, including reversals, without advancing per material/eye.
    const float target=state.action_opacity;
    // The shared menu-return clock also fades native A before this pass.
    if(return_opacity)state.progress=target*std::clamp(*return_opacity,0.f,1.f);
    else state.progress+=std::clamp(target-state.progress,-step,step);
    state.opacity=state.progress*cover;
}
// Replace only the routed draw's fresh stack alpha. Multiplying the old HUD
// alpha would retain its dim state even when the A button is fully opaque.
// Native materials, vertex colors and the bottom-screen draw remain intact.
template<class M> void Apply(M& m,U32 alpha,const State& state) {
    if(state.override_alpha && m.Read(alpha))
        m.Write(alpha,std::bit_cast<U32>(state.opacity));
}
}
