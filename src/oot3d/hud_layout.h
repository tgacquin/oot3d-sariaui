// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <mutex>
#include <algorithm>
#include "oot3d/user_hud.h"
#include "oot3d/hud_buttons_layout.h"
#include "oot3d/hd_rupee_font.h"

namespace OOT3D::HUD {
struct Pose { float x = 0, y = 0, scale = 1; };
enum Element { Hearts, Rupees, Magic, AButton, LoadingIcon, Minimap, Keys, Timer, Minigame, ShardAgony, TargetIndicator, Carrots, ElementCount };
struct CarrotShadow {
    bool enabled=false;
    float x=1.5f,y=2.f,blur=1.f,strength=.6f;
};
struct AmmoBackdrop {
    bool enabled=false;
    float x=0,y=1,width=1.25f,height=1.25f,softness=.75f,opacity=.3f;
    bool operator==(const AmmoBackdrop&) const=default;
};
struct Layout : std::array<Pose, ElementCount> {
    bool reposition_area_titles=true;
    float area_title_y=-72.f;
    bool hd_numbers = true;
    bool hd_numbers_bold = false;
    bool hd_rupee_numbers = true;
    float rupee_number_gap = 0; // Additional icon-to-counter gap before group scaling.
    Pose hd_rupee_text{}; // Local adjustment of digits only, relative to the Rupees pose.
    float hd_rupee_spacing = 0; // Additional digit advance before text/group scaling.
    HDFont::RupeeShadow hd_rupee_shadow{};
    bool hd_timer_numbers = true;
    HDFont::RupeeShadow hd_timer_shadow{};
    Pose hd_timer_text{};
    bool hd_ammo_numbers = true, hd_score_numbers = true;
    Pose hd_ammo_text{}, hd_score_text{};
    Pose hd_pause_ammo_text{};
    bool hd_skulltula_numbers=true;
    Pose hd_skulltula_text{};
    float hd_skulltula_spacing=0;
    AmmoBackdrop hd_ammo_backdrop{},hd_rupee_backdrop{},hd_timer_backdrop{};
    float hd_timer_spacing=0, hd_ammo_spacing=0, hd_score_spacing=0;
    bool minimap_lock_corner = true; // Room changes and scaling keep the bottom-right fixed.
    Pose outside_minimap{};
    Pose navi_text{}; // The separate native Navi label, not the D-pad icon.
    std::array<std::array<float,2>,2> shop_prompt_offsets{};
    CarrotShadow carrot_shadow{};
    float carrot_opacity = 1;
    float carrot_spacing = 0; // Extra pixels per gap, centered before row scaling.
    float carrot_animation_delay = 0; // Seconds after spending a carrot.
    float heart_spacing = 0; // Additional pixels per column, before HUD scaling.
    float magic_width = 1; // Relative horizontal length, preserving the fill fraction.
    float magic_height = 1; // Green fill height; top edge stays fixed.
    float magic_border = 1; // Native frame thickness, independent of fill height.
    float magic_background_opacity = 1; // Black interior only; preserves native HUD fades.
};
inline void MinimapBottomRight(Pose& p) {p.x=264;p.y=0;} // Visible corner (392,232).
// One-time upgrade from padding-based placement. Only recover corners that
// are outside the native HUD canvas; subsequent intentional offsets are kept.
inline void RecoverLegacyMinimap(Pose& p,bool locked) {
    if(!locked)return;
    if(p.x< -128 || p.x>272)p.x=std::clamp(p.x,-120.f,264.f);
    if(p.y< -232 || p.y>8)p.y=std::clamp(p.y,-224.f,0.f);
}
inline std::mutex layout_mutex;
inline Layout layout{};
inline std::atomic<bool> dirty{false};
inline Layout GetLayout() {
    std::scoped_lock lock{layout_mutex};
    return layout;
}
// A is stored as an offset from its native center; B uses absolute centers.
inline constexpr float ActionAnchorX=368, ActionAnchorY=214;
inline std::array<float,2> ActionGroupDelta(const Pose& a,const PausePrompts::Pose& b,float percent) {
    const float extra=percent/100.f;
    return {(ActionAnchorX+a.x-b.x)*.5f*extra,(ActionAnchorY+a.y-b.y)*.5f*extra};
}
static_assert(static_cast<int>(Carrots)==static_cast<int>(UserHUD::Carrots));
inline Layout GetEffectiveLayout() {
    auto result=GetLayout();const auto offsets=UserHUD::GetEffectiveOffsets();
    if(offsets.button_scale!=0) {
        const auto delta=ActionGroupDelta(result[AButton],HUDButtons::GetLayout().buttons[HUDButtons::B].pose,offsets.button_scale);
        result[AButton].x+=delta[0];result[AButton].y+=delta[1];
    }
    for(unsigned i=0;i<result.size();++i)result[i]=UserHUD::Apply(result[i],offsets[i]);
    if(offsets.heart_spacing!=0)result.heart_spacing=std::clamp(result.heart_spacing+offsets.heart_spacing,-8.f,20.f);
    result.outside_minimap=UserHUD::Apply(result.outside_minimap,offsets[UserHUD::OutsideMinimap]);
    if(offsets.magic_height!=0)result.magic_height=offsets.magic_height;
    if(offsets.magic_border!=0)result.magic_border=offsets.magic_border;
    result.magic_width*=offsets.magic_width;
    result.hd_numbers=UserHUD::HDNumbers(result.hd_numbers);
    return result;
}
inline void SetLayout(const Layout& value, bool save = true) {
    std::scoped_lock lock{layout_mutex};
    layout = value;
    if (save) dirty = true;
}
} // namespace OOT3D::HUD

namespace OOT3D::HUDButtons {
inline Layout GetEffectiveLayout() {
    auto result=GetLayout();const auto offsets=UserHUD::GetEffectiveOffsets();
    if(offsets.button_scale!=0) {
        const auto delta=HUD::ActionGroupDelta(HUD::GetLayout()[HUD::AButton],result.buttons[B].pose,offsets.button_scale);
        result.buttons[B].pose.x-=delta[0];result.buttons[B].pose.y-=delta[1];
    }
    result.buttons[B].pose=UserHUD::Apply(result.buttons[B].pose,offsets[UserHUD::BButton],4);
    const auto group=UserHUD::Apply(PausePrompts::Pose{result.item_x,result.item_y,result.item_scale},
                                    offsets[UserHUD::ItemButtons],4);
    result.item_x=group.x;result.item_y=group.y;result.item_scale=group.scale;
    for(unsigned i=0;i<4;++i)result.user_items[i]=offsets[UserHUD::XButton+i];
    return result;
}
}
