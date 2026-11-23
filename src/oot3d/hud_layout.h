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
struct DropShadow {
    bool enabled=false;
    float x=1.5f,y=2.f,blur=1.f,strength=.6f;
};
struct CarrotShadow {
    bool enabled=false;
    float x=1.5f,y=1.4f,opacity=.8f;
};
struct SpriteContour {
    bool enabled=false;
    float width=1.f,height=1.f,opacity=.65f;
    float x=0,y=0,softness=0;
    std::array<float,3> color{};
};
struct HeartContours {
    SpriteContour shadow{};
    SpriteContour outline{.color={1,1,1}};
    SpriteContour double_defense{.color={1,1,1}};
    bool Enabled() const {return shadow.enabled || outline.enabled || double_defense.enabled;}
};
struct RupeeDigitOutline {
    bool enabled=false;
    static constexpr float MinThickness=.25f, MaxThickness=3.f;
    float thickness=1.f;
    std::array<float,3> color{.08f,.08f,.08f};
    bool operator==(const RupeeDigitOutline&) const = default;
};
struct AmmoBackdrop {
    bool enabled=false;
    float x=0,y=1,width=1.25f,height=1.25f,softness=.75f,opacity=.3f;
    bool operator==(const AmmoBackdrop&) const=default;
};
// Bright fill texel in menu_top_parts00 (RGBA4 0x2f2f).
inline constexpr std::array NativeMagicFill{2.f/15,1.f,2.f/15};
struct Layout : std::array<Pose, ElementCount> {
    UserHUD::Padding padding{};
    UserHUD::Layout PlayerAdjustments() const {
        auto personal=UserHUD::GetLayout();
        personal.top+=padding.top;
        personal.bottom+=padding.bottom;
        personal.left+=padding.left;
        personal.right+=padding.right;
        return personal;
    }
    float overall_opacity=1,minimap_opacity=1; // Player multipliers for gameplay draws.
    float overall_scale=1; // Authored size at F2 100%.
    bool reposition_area_titles=true;
    float area_title_y=-72.f;
    static constexpr float MinAreaTitleScale=.25f, MaxAreaTitleScale=3.f;
    float area_title_scale=1.f;
    bool hd_numbers = true;
    bool hd_numbers_bold = false;
    bool hd_rupee_numbers = true;
    float counter_distance=20; // Vertical distance between the two native counter pivots at 1x.
    float rupee_number_gap = 0; // Additional icon-to-counter gap before group scaling.
    Pose hd_rupee_text{}; // Local adjustment of digits only, relative to the Rupees pose.
    float hd_rupee_spacing = 0; // Additional digit advance before text/group scaling.
    HDFont::RupeeShadow hd_rupee_shadow{};
    bool hd_timer_numbers = true;
    Pose hd_timer_text{};
    bool hd_ammo_numbers = true, hd_score_numbers = true;
    Pose hd_ammo_text{}, hd_score_text{};
    Pose hd_pause_ammo_text{};
    bool hd_skulltula_numbers=true;
    Pose hd_skulltula_text{};
    float hd_skulltula_spacing=0;
    AmmoBackdrop hd_ammo_backdrop{},hd_rupee_backdrop{};
    float hd_timer_spacing=0, hd_ammo_spacing=0, hd_score_spacing=0;
    bool minimap_lock_corner = true; // Room changes and scaling keep the bottom-right fixed.
    Pose outside_minimap{};
    Pose navi_text{}; // The separate native Navi label, not the D-pad icon.
    std::array<std::array<float,2>,2> shop_prompt_offsets{};
    CarrotShadow carrot_shadow{};
    float carrot_opacity = 1;
    float carrot_spacing = 0; // Extra pixels per gap, centered before row scaling.
    float carrot_animation_delay = 0; // Seconds after spending a carrot.
    HeartContours heart_contours{};
    static constexpr std::array NativeHeartColor{1.f,0.f,0.f};
    std::array<float,3> heart_color=NativeHeartColor;
    float heart_brightness=1.f;
    bool CustomHeartColor() const {return heart_brightness!=1.f || heart_color!=NativeHeartColor;}
    SpriteContour rupee_contour{},magic_contour{};
    float magic_border_roundness=0;
    DropShadow magic_shadow{};
    RupeeDigitOutline rupee_digit_outline{};
    bool OutlineHDSkulltula() const {return hd_numbers && hd_skulltula_numbers && rupee_digit_outline.enabled;}
    bool OutlineHDTimer() const {return hd_numbers && hd_timer_numbers && rupee_digit_outline.enabled;}
    bool OutlineHDRupees() const {return hd_numbers && hd_rupee_numbers && rupee_digit_outline.enabled;}
    float heart_spacing = 0; // Additional pixels per column, before HUD scaling.
    float magic_one_row_y = 2.f, magic_two_row_y = 13.f; // Offsets in pixels at native heart scale.
    float magic_width = 1; // Relative horizontal length, preserving the fill fraction.
    float magic_height = 1; // Green fill height; top edge stays fixed.
    float magic_border = 1; // Fill padding, independent of fill height.
    float magic_padding_ratio = 1; // Converts authored padding into the scaled meter's local units.
    std::array<float,3> magic_fill_color=NativeMagicFill;
    float magic_background_opacity = 1; // Black interior only; preserves native HUD fades.
};
// Local digit transform for the gameplay rupee counter.
struct RupeeDigitStyle {
    float gap{},spacing{};
    Pose text{};
    float OffsetX(unsigned column) const {return gap+text.x+spacing*column*text.scale;}
};
inline RupeeDigitStyle RupeeDigits(const Layout& layout) {
    const bool hd=layout.hd_numbers && layout.hd_rupee_numbers;
    return {layout.rupee_number_gap,hd?layout.hd_rupee_spacing:0.f,hd?layout.hd_rupee_text:Pose{}};
}
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
struct ButtonPlacement {
    Pose action;
    HUDButtons::Layout buttons;
    std::array<float,2> TopRight() const {
        // Native A rectangle is 38x38 around (368,214). Include temporarily
        // hidden bubbles so changing equipment cannot move the anchor.
        float right=ActionAnchorX+action.x+19*action.scale;
        float top=ActionAnchorY+action.y-19*action.scale;
        const auto pivot=HUDButtons::ItemPivot(buttons);
        for(unsigned i=HUDButtons::B;i<HUDButtons::Count;++i) {
            const auto p=i==HUDButtons::B?buttons.buttons[i].pose:HUDButtons::ItemPose(buttons,i,pivot);
            const float radius=HUDButtons::BubbleWidth(i)*p.scale*.5f;
            right=std::max(right,p.x+radius);
            top=std::min(top,p.y-radius*HUDButtons::BubbleAspectY);
        }
        return {right,top};
    }
};
inline ButtonPlacement PlaceButtons(Pose a,HUDButtons::Layout buttons,const UserHUD::Layout& offsets) {
    auto& b=buttons.buttons[HUDButtons::B].pose;
    if(offsets.button_scale!=0) {
        const auto pivot=HUDButtons::ItemPivot(buttons);
        const float ix=pivot[0]+buttons.item_x,iy=pivot[1]+buttons.item_y;
        const float ax=ActionAnchorX+a.x,ay=ActionAnchorY+a.y;
        const float cx=(ax+b.x+4*ix)/6,cy=(ay+b.y+4*iy)/6;
        const float extra=offsets.button_scale/100.f;
        a.x+=(ax-cx)*extra;a.y+=(ay-cy)*extra;
        b.x+=(b.x-cx)*extra;b.y+=(b.y-cy)*extra;
        buttons.item_x+=(ix-cx)*extra;buttons.item_y+=(iy-cy)*extra;
    }
    a=UserHUD::Apply(a,offsets[UserHUD::AButton]);
    b=UserHUD::Apply(b,offsets[UserHUD::BButton],4);
    const auto group=UserHUD::Apply(PausePrompts::Pose{buttons.item_x,buttons.item_y,buttons.item_scale},
                                    offsets[UserHUD::ItemButtons],4);
    buttons.item_x=group.x;buttons.item_y=group.y;buttons.item_scale=group.scale;
    for(unsigned i=0;i<4;++i)buttons.user_items[i]=offsets[UserHUD::XButton+i];
    return {a,buttons};
}
inline ButtonPlacement EffectiveButtons(const Pose& a,const HUDButtons::Layout& buttons,
                                        const UserHUD::Layout& personal,const UserHUD::Layout& offsets) {
    auto placed=PlaceButtons(a,buttons,offsets);
    if(offsets.overall_scale!=1) {
        // Measure both layouts after local size limits, offsets and padding.
        // Keep the actual outside edges fixed while the entire cluster resizes.
        const auto reference=PlaceButtons(a,buttons,UserHUD::EffectiveOffsets(personal,1)).TopRight();
        const auto actual=placed.TopRight();
        const float dx=reference[0]-actual[0],dy=reference[1]-actual[1];
        placed.action.x+=dx;placed.action.y+=dy;
        auto& b=placed.buttons.buttons[HUDButtons::B].pose;
        b.x+=dx;b.y+=dy;
        placed.buttons.item_x+=dx;placed.buttons.item_y+=dy;
    }
    return placed;
}
// The native counter pivots after routing are (12,212) and (12,192).
inline void PlaceCounters(Layout& hud,const UserHUD::Layout& personal,float overall) {
    const auto& group=personal[UserHUD::CounterGroup];
    auto& wallet=hud[Rupees];
    wallet=UserHUD::Apply(wallet,personal[UserHUD::Rupees]);
    const float original_scale=wallet.scale;
    wallet.scale=std::clamp(original_scale*overall*UserHUD::Factor(group),.1f,3.f);
    wallet.x=std::clamp(wallet.x+group.x+personal.left,-400.f,400.f);
    wallet.y=std::clamp(wallet.y+18*(original_scale-wallet.scale)+group.y-personal.bottom,-240.f,240.f);
    const float distance=std::clamp(hud.counter_distance+personal.counter_distance_offset,
                                    0.f,UserHUD::Layout::MaxCounterDistance);
    hud[Keys]={wallet.x,std::clamp(wallet.y+20-distance*wallet.scale,-240.f,240.f),wallet.scale};
}

// Native clock starts at (159,216) and is 20x20. The five timer cells
// start at (182,215), advance by 10 and have 11x21 geometry.
inline void PlaceTimerAndCarrots(Layout& hud,float authored_scale,float overall) {
    const float delta=authored_scale-hud[Timer].scale;
    if(delta==0 && overall<=1)return;
    const bool hd=hud.hd_numbers && hud.hd_timer_numbers;
    const auto text=hd?hud.hd_timer_text:Pose{};
    const float spacing=hd?hud.hd_timer_spacing:0;
    const float left=23+text.x;
    const float right=left+(51+4*spacing)*text.scale;
    const float center=(std::min(0.f,left)+std::max(20.f,right))*.5f;
    const float bottom=std::max(20.f,-1+text.y+21*text.scale);
    hud[Timer].x+=center*delta;
    hud[Timer].y+=bottom*delta;
    if(overall<=1)return;
    // The clock grows upward around its bottom edge. The six 24px carrots
    // grow around (200,20) after routing, so reserve room for both HUD rows.
    const auto& timer=hud[Timer];
    auto& carrots=hud[Carrots];
    const float half_width=(72.f+2.5f*hud.carrot_spacing)*carrots.scale;
    const float row_x=200+carrots.x;
    const float clock_x=159+timer.x;
    if(row_x+half_width<=clock_x+std::min(0.f,left)*timer.scale ||
       row_x-half_width>=clock_x+std::max(20.f,right)*timer.scale)return;
    const float clock_y=216+timer.y;
    if(20+carrots.y>=clock_y+bottom*timer.scale)return;
    const float timer_top=clock_y+std::min(0.f,text.y-1)*timer.scale;
    const float shadow=hud.carrot_shadow.enabled && hud.carrot_shadow.opacity>0 ?
        std::max(0.f,hud.carrot_shadow.y):0.f;
    carrots.y=std::min(carrots.y,timer_top-2.f-shadow-12.f*carrots.scale-20.f);
}
static_assert(static_cast<int>(Carrots)==static_cast<int>(UserHUD::Carrots));
// Scale a routed top-left HUD pivot about the authored heart origin (12,10).
// Apply player offsets and edge padding after this transform.
inline void ScaleAboutHearts(float& x,float& y,float pivot_x,float pivot_y,
                             const Pose& hearts,float scale) {
    x+=(pivot_x+x-(12+hearts.x))*(scale-1);
    y+=(pivot_y+y-(10+hearts.y))*(scale-1);
}
inline Layout GetEffectiveLayout() {
    auto result=GetLayout();
    const auto personal=result.PlayerAdjustments();
    result.overall_opacity=personal.overall_opacity;
    result.minimap_opacity=personal.minimap_opacity;
    const auto offsets=UserHUD::EffectiveOffsets(personal,result.overall_scale*personal.overall_scale);
    if(offsets.overall_scale!=1) {
        // After native screen routing, hearts pivot at (12,10) and magic at
        // (12,36), plus their authored offsets. Scale both around the heart
        // anchor so their gap grows with them, including a second heart row.
        // User translations and edge padding remain applied afterward.
        ScaleAboutHearts(result[Magic].x,result[Magic].y,12,36,result[Hearts],offsets.overall_scale);
    }
    PlaceCounters(result,personal,offsets.overall_scale);
    const float authored_timer_scale=result[Timer].scale;
    const float authored_magic_scale=result[Magic].scale;
    const float authored_heart_scale=result[Hearts].scale;
    if(offsets.button_scale!=0 || offsets.overall_scale!=1)
        result[AButton]=EffectiveButtons(result[AButton],HUDButtons::GetLayout(),personal,offsets).action;
    else result[AButton]=UserHUD::Apply(result[AButton],offsets[AButton]);
    for(unsigned i=0;i<result.size();++i)
        if(i!=AButton && i!=Keys && i!=Rupees)result[i]=UserHUD::Apply(result[i],offsets[i]);
    result.magic_padding_ratio=authored_magic_scale/result[Magic].scale;
    result.hd_numbers=UserHUD::HDNumbers(result.hd_numbers);
    PlaceTimerAndCarrots(result,authored_timer_scale,offsets.overall_scale);
    if(result[Hearts].scale!=authored_heart_scale && authored_heart_scale>0) {
        const float ratio=result[Hearts].scale/authored_heart_scale;
        auto& contours=result.heart_contours;
        for(auto* layer:{&contours.outline,&contours.double_defense,&contours.shadow}) {
            layer->width*=ratio;layer->height*=ratio;
            layer->x*=ratio;layer->y*=ratio;
        }
    }
    if(offsets.heart_spacing!=0)result.heart_spacing=std::clamp(result.heart_spacing+offsets.heart_spacing,-8.f,20.f);
    result.outside_minimap=UserHUD::Apply(result.outside_minimap,offsets[UserHUD::OutsideMinimap]);
    if(offsets.magic_height!=0)result.magic_height=offsets.magic_height;
    if(offsets.magic_outer_thickness!=0) {
        result.magic_contour.enabled=true;
        result.magic_contour.width=result.magic_contour.height=offsets.magic_outer_thickness;
    }
    result.magic_width*=offsets.magic_width;
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
    const auto hud=HUD::GetLayout();
    const auto personal=hud.PlayerAdjustments();
    const auto offsets=UserHUD::EffectiveOffsets(personal,hud.overall_scale*personal.overall_scale);
    return HUD::EffectiveButtons(hud[HUD::AButton],GetLayout(),personal,offsets).buttons;
}
}
