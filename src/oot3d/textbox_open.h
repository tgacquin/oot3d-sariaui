// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/screen_model_hooks.h"
#include "oot3d/pause_menu.h"

namespace OOT3D::TextboxOpening {
template<class M> bool MapScreen(M& m) {
    return PauseMenu::Active(m) && PauseMenu::MapPage(m);
}
template<class M> bool FinishAtFullSize(M& m) {
    constexpr auto Message=ScreenModels::Message;
    if(!MapScreen(m) || m.Read(Message)!=0x4ed0a0 ||
       !(m.Read(Message+0xc).value_or(0)&255) ||
       (m.Read(Message+0xf38).value_or(0)&255)!=2)return false;
    const auto pane=m.Read(Message+0x56c).value_or(0);
    if(!TitleScreen::Pointer(pane,0xb4) || m.Read(pane)!=0x4ed234 ||
       m.Read(pane+4)!=Message+0x168 || !m.Read(pane+0xc).value_or(0) ||
       (m.Read(pane+0x24).value_or(0)&0x60)!=0x60)return false;
    // Use the evaluated sample, not the clock (which is advanced after
    // evaluation). This matches the frame that actually reaches full size,
    // including fractional updates and custom opening speeds.
    for(auto at:{pane+0x3c,pane+0x40}) {
        const auto bits=m.Read(at);if(!bits)return false;
        const float scale=std::bit_cast<float>(*bits);
        if(!std::isfinite(scale) || scale<1)return false;
    }
    const auto end=m.Read(pane+0x1c);if(!end)return false;
    const float duration=std::bit_cast<float>(*end);
    if(!std::isfinite(duration) || duration<=0 || duration>60)return false;
    // Let the normal dispatcher finish opening and retire its animation.
    // Never write the message state or bypass native text preparation.
    m.Write(pane+0x18,*end);
    return true;
}
// Called at 494ed4, after the native QAN evaluator copies its channels to
// the stack. message/anim.qan opening scales grow from .1 to 1.2, then
// settle at 1. Clamp only that temporary scale sample; retain depth,
// visibility and the pane's separately applied base size. The dispatcher
// separately ends the opening wait once this evaluated sample is full-size.
template<class M,class R> void Apply(M& m,const R& r,bool supported) {
    constexpr auto Message=ScreenModels::Message;
    const auto pane=r[4],stack=r[13];
    if(!supported || !MapScreen(m) ||
       m.Read(Message)!=0x4ed0a0 || !(m.Read(Message+0xc).value_or(0)&255) ||
       (m.Read(Message+0xf38).value_or(0)&255)!=2 ||
       !TitleScreen::Pointer(pane,0xb4) || m.Read(Message+0x56c)!=pane ||
       m.Read(pane)!=0x4ed234 || m.Read(pane+4)!=Message+0x168)return;
    const auto flags=m.Read(stack+0x58).value_or(0);
    for(unsigned axis=0;axis<2;++axis) {
        if(!(flags&(0x20u<<axis)))continue;
        const auto at=stack+0x70+4*axis;
        const auto bits=m.Read(at);if(!bits)continue;
        const float scale=std::bit_cast<float>(*bits);
        if(std::isfinite(scale) && scale>1)m.Write(at,std::bit_cast<TitleScreen::U32>(1.f));
    }
}
}
