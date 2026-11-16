// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"
#include "oot3d/screen_model_hooks.h"

namespace OOT3D::MapLocation {
using TitleScreen::U32;
constexpr U32 Owner=0x4fc648;
// 0x44d11c creates the location preview. 0x44bd34 creates the parchment,
// "Current Location" label and localized location name in two separate models.
template<class M> std::optional<PauseMenu::Part> Select(M& m,U32 model) {
    if(!TitleScreen::Pointer(model,0x1e4) ||
       m.Read(model)!=std::optional<U32>{0x4ebd60})return {};
    const U32 preview=m.Read(Owner+0x3c).value_or(0);
    if(TitleScreen::Pointer(preview,0xc) &&
       m.Read(preview+4)==std::optional<U32>{model})return PauseMenu::LocationMap;
    const U32 banner=m.Read(Owner+0x38).value_or(0);
    if(TitleScreen::Pointer(banner,0x18) &&
       (m.Read(banner+4)==std::optional<U32>{model} ||
        m.Read(banner+0x10)==std::optional<U32>{model}))return PauseMenu::LocationBanner;
    return {};
}
inline ScreenModels::Transform Transform(PauseMenu::Part part,const PauseMenu::Layout& layout,
        bool bottom_projection) {
    const auto p=PauseMenu::Valid(layout[part])?layout[part]:PauseMenu::Pose{};
    const auto a=PauseMenu::Anchors[part];
    const float sx=bottom_projection?.8f:1.f;
    const float rise=part==PauseMenu::LocationBanner?PauseMenu::world_location_y.load():0;
    return {p.x*sx+a[0]*(sx-1),p.y+rise,p.scale*sx,p.scale,a[0],a[1],true};
}
// The native material is a fresh stack copy for each draw. Apply the map's
// clock tint to both title-banner models here, preserving their existing fade
// even when they reuse vertex colors across multiple frames.
template<class M> bool Material(M& m,U32 model,U32 alpha_address) {
    const float opacity=PauseMenu::world_location_alpha.load();
    if(!PauseMenu::Active(m) || !PauseMenu::MapPage(m) ||
       PauseMenu::DungeonPage(m) ||
       Select(m,model)!=PauseMenu::LocationBanner)return false;
    auto tint=MapTimeTint::Read(m,PauseMenu::GetLayout().world_map_time_tint);
    for(auto& channel:tint)channel=std::lerp(1.f,channel,.5f);
    if(opacity==1 && tint==MapTimeTint::Neutral)return false;
    const auto word=m.Read(alpha_address);
    if(!word)return false;
    const float alpha=std::bit_cast<float>(*word);
    if(!std::isfinite(alpha))return false;
    std::array<float,3> rgb{};
    if(tint!=MapTimeTint::Neutral) {
        // Native stack RGBA is at sp+0x2c; alpha is sp+0x38.
        if(!TitleScreen::Pointer(alpha_address-12,16) ||
           !TitleScreen::ReadFloats(m,alpha_address-12,rgb))return false;
        for(float value:rgb)if(!std::isfinite(value))return false;
        for(unsigned c=0;c<3;++c)
            m.Write(alpha_address-12+c*4,std::bit_cast<U32>(rgb[c]*tint[c]));
    }
    m.Write(alpha_address,std::bit_cast<U32>(alpha*opacity));
    return true;
}
}
