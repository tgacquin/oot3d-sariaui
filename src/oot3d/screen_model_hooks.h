// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_hooks.h"
#include "oot3d/textbox_layout.h"
#include "oot3d/hud_buttons_layout.h"

namespace OOT3D::ScreenModels {
using TitleScreen::U32;
using TitleScreen::Pointer;
constexpr U32 FirstSVC = 0xfff050;
constexpr U32 Message = 0x5c1878, Minimap = 0x4fda6c;
// Just before native view * model multiplication. The view is a fresh stack
// copy; changing it leaves every live model, layout, and animation untouched.
constexpr std::array<TitleScreen::Site, 1> Sites{{
    {0x3fb918, 0xe280200c, 0xe28d0020},
}};
inline U32 Instruction(U32 pc, U32 word) {
    for (unsigned i = 0; i < Sites.size(); ++i)
        if (pc == Sites[i].pc && word == Sites[i].instruction)
            return 0xef000000 | (FirstSVC + i);
    return word;
}
template<class M> bool CodeMatches(M& memory) {
    for (const auto& s : Sites)
        if (memory.Read(s.pc) != std::optional<U32>{s.instruction} ||
            memory.Read(s.pc + 4) != std::optional<U32>{s.following}) return false;
    return memory.Read(0x4ebd6c) == std::optional<U32>{0x3f9680} &&
           memory.Read(0x42bef0) == std::optional<U32>{Message} &&
           memory.Read(0x441f70) == std::optional<U32>{Minimap};
}
struct Transform { float x = 0, y = 0, sx = 1, sy = 1, ax = 0, ay = 0; bool anchored = false; };
inline bool Valid(const Textboxes::Layout& l) {
    return HUD::Valid({l.x,l.y,1}) && HUD::Valid(l.indicator) &&
           std::isfinite(l.width) && l.width >= 1 && l.width <= 3 && std::isfinite(l.scale) && l.scale >= .1f && l.scale <= 3 &&
        std::isfinite(l.sign_y) && std::abs(l.sign_y)<=240;
}
// Minimap art, chest/item markers, and the gameplay action atlas are separate
// native models. Pause titles use Action+0x28 and must remain visible.
template<class M> bool IsMinimapModel(M& m,U32 model) {
    return MinimapControl::IsModel(m,model);
}
template<class M> bool HideDuringPause(M& m,U32 model) {
    return IsMinimapModel(m,model) ||
        (Pointer(model,0x1e4) && m.Read(model)==std::optional<U32>{0x4ebd60} &&
         m.Read(HUD::Action+0x20)==std::optional<U32>{model});
}
// Compose uniform box scale around the plate center with per-pane scaling.
// Lettering keeps its aspect ratio; background width remains independent.
template<class M> std::optional<Transform> ScaleBox(M& m, U32 model,
        const Textboxes::Layout& box, Transform t) {
    if (box.scale == 1) return t;
    const U32 plate = m.Read(Message + 0x56c).value_or(0);
    if (!Pointer(plate, 0xb4) || m.Read(plate) != std::optional<U32>{0x4ed234} ||
        m.Read(plate+4) != std::optional<U32>{Message+0x168}) return {};
    const U32 background = m.Read(plate+0x64).value_or(0);
    if (!Pointer(background, 0x1e4) || m.Read(background) != std::optional<U32>{0x4ebd60}) return {};
    std::array<float,12> origin, center;
    if (!TitleScreen::ReadFloats(m,model+0xc,origin) ||
        !TitleScreen::ReadFloats(m,background+0xc,center)) return {};
    t.x = box.x + box.scale * (t.x-box.x + (1-t.sx)*(origin[3]-center[3]));
    t.y = box.y + box.scale * (t.y-box.y + (1-t.sy)*(origin[7]-center[7]));
    t.sx *= box.scale; t.sy *= box.scale;
    t.ax = center[3]; t.ay = center[7]; t.anchored = true;
    return t;
}
// Pane index, or 256 for the message's lettering/counter models. Ownership is
// useful independently of a non-default editor pose when deferring a draw.
template<class M> std::optional<unsigned> MessageElement(M& m,U32 model,bool visible=true) {
    if (!Pointer(model, 0x1e4) || m.Read(model) != std::optional<U32>{0x4ebd60}) return {};
    if (m.Read(Message) != std::optional<U32>{0x4ed0a0}) return {};
    const auto state = m.Read(Message + 0xf38).value_or(0) & 255;
    if (visible && (state == 0 || state == 1 || state == 14 || state == 15)) return {};
    for (unsigned i = 0; i < 256; ++i) {
        const U32 pane = m.Read(Message + 0x568 + i*4).value_or(0);
        if (!Pointer(pane,0xb4) || m.Read(pane) != std::optional<U32>{0x4ed234} ||
            m.Read(pane+4) != std::optional<U32>{Message+0x168}) continue;
        if (m.Read(pane+0x64) != std::optional<U32>{model} &&
            m.Read(pane+0x68) != std::optional<U32>{model}) continue;
        return i;
    }
    const U32 text = m.Read(Message+0x974).value_or(0);
    if (!Pointer(text,0x534) || m.Read(Message+0x978) == std::optional<U32>{0xffffffff}) return {};
    if (m.Read(text+8) == std::optional<U32>{model}) return 256;
    const auto count = m.Read(text+0x224).value_or(0);
    if (count > 128) return {};
    for (unsigned i=0;i<count;++i) {
        const U32 entry=m.Read(text+0xc+i*4).value_or(0);
        if (Pointer(entry,0x14) && m.Read(entry+0x10) == std::optional<U32>{model})
            return 256;
    }
    return {};
}
using HUD::ExtraHUDModel;
template<class M> bool HideSpecialBWhenPaused(M& m,U32 model) {
    return HUD::UsesBLayout(m) && ExtraHUDModel(m,model)==HUD::Minigame;
}
template<class M> std::optional<Transform> Select(M& m, U32 model,
        Textboxes::Layout box, const HUD::Pose& map) {
    if(HUD::UsesBLayout(m) && ExtraHUDModel(m,model)==HUD::Minigame) {
        // The gallery/horseback weapon and ammo are separate from the A/B atlas. Match
        // the B bubble's per-quad transform without moving A or the minimap.
        const auto button=HUD::SpecialBButton(m);
        const auto p=button.pose;
        if(std::isfinite(p.x) && std::abs(p.x)<=500 &&
           std::isfinite(p.y) && std::abs(p.y)<=240 &&
           std::isfinite(p.scale) && p.scale>=.1f && p.scale<=4)
            return Transform{p.x-336,p.y-215.5f,
            button.enabled?p.scale:0,button.enabled?p.scale:0,336,215.5f,true};
        return {};
    }
    if(const auto element=ExtraHUDModel(m,model)) {
        const auto p=HUD::GetEffectiveLayout()[*element];
        if(HUD::Valid(p) && (p.x!=0 || p.y!=0 || p.scale!=1)) {
            const auto anchor=*element==HUD::Timer?std::array{HUD::TimerAnchorX,HUD::TimerAnchorY}:
                *element==HUD::ShardAgony?std::array{HUD::ShardAnchorX,HUD::ShardAnchorY}:
                *element==HUD::TargetIndicator?std::array{HUD::TargetAnchorX,HUD::TargetAnchorY}:
                std::array{HUD::MinigameAnchorX,HUD::MinigameAnchorY};
            return Transform{p.x,p.y,p.scale,p.scale,anchor[0],anchor[1],true};
        }
    }
    if (HUD::Valid(map) && IsMinimapModel(m,model)) {
        const auto p=MinimapControl::Place(m,map,HUD::GetEffectiveLayout().minimap_lock_corner);
        if(p.x!=0 || p.y!=0 || p.scale!=1)
            return Transform{p.x,p.y,p.scale,p.scale,p.ax,p.ay,true};
    }
    if (!Valid(box))return {};
    // 0x2cd974 passes Message+f48 to the plate selector. Type 1 is wooden
    // signage (verified against native message 0x312, Deku Tree Meadow sign).
    if(m.Read(Message+0xf48)==std::optional<U32>{1})box.y+=box.sign_y;
    if (box.x == 0 && box.y == 0 && box.width == 1 && box.indicator.x == 0 &&
        box.indicator.y == 0 && box.indicator.scale == 1 && box.scale == 1) return {};
    const auto element=MessageElement(m,model);if(!element)return {};
    if(*element==256 && box.x==0 && box.y==0 && box.scale==1)return {};
    Transform t{box.x,box.y};
    if(*element==1)t.sx=box.width;
    if(*element==2) {
        t.x+=box.indicator.x;t.y+=box.indicator.y;
        t.sx=t.sy=box.indicator.scale;
    }
    return ScaleBox(m,model,box,t);
}
// Compose the screen-space adjustment into a fresh view matrix. Native code
// then multiplies it by the untouched model and copies uniforms into commands.
// No guest pointer or temporary override survives the draw or a state load.
template<class M> bool Apply(M& memory, U32 view, U32 model, const Transform& t) {
    std::array<float,12> v, m;
    if (!TitleScreen::ReadFloats(memory,view,v) ||
        !TitleScreen::ReadFloats(memory,model+0xc,m)) return false;
    const float ax=t.anchored?t.ax:m[3], ay=t.anchored?t.ay:m[7];
    const float tx=t.x+ax*(1-t.sx), ty=t.y+ay*(1-t.sy);
    for(unsigned row=0;row<3;++row) {
        const unsigned i=row*4;
        v[i+3]+=v[i]*tx+v[i+1]*ty;
        v[i]*=t.sx;v[i+1]*=t.sy;
    }
    for(unsigned i=0;i<12;++i) memory.Write(view+i*4,std::bit_cast<U32>(v[i]));
    return true;
}
template<class M, class R> void Handle(M& memory, R& regs,
        const Textboxes::Layout& box, const HUD::Pose& map) {
    const U32 model=regs[0];
    regs[2]=model+0xc; // Displaced ADD r2,r0,#0xc; flags unchanged.
    if (auto transform=Select(memory,model,box,map))
        Apply(memory,regs[1],model,*transform);
}
} // namespace OOT3D::ScreenModels
