// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompt_layout.h"
#include "oot3d/user_hud.h"
#include <cmath>

namespace OOT3D::HUDButtons {
enum Element { B, X, Y, I, II, Count };
constexpr std::array Names{"b","x","y","i","ii"};
constexpr std::array Labels{"B button","X item button","Y item button","I item button","II item button"};
struct Button {bool enabled=true;PausePrompts::Pose pose{};};
struct Layout {
    bool show_items=true;
    std::array<UserHUD::Adjustment,4> user_items{}; // Effective layout only, screen-space offsets.
    float minigame_b_x=0; // Extra screen-space offset; ordinary B stays unchanged.
    PausePrompts::Pose b_ammo{}; // Independent placement within the B item group.
    float item_x=0,item_y=0,item_scale=1;
    float counter_scale=1,counter_x=0,counter_y=0;
    float diamond_rotation=0,diamond_spacing=34;
    PausePrompts::Pose item_labels{},item_icons{};
    std::array<Button,Count> buttons{{{true,{336,215.5f,1}},
        {true,{304,124,1}},{true,{270,158,1}},{true,{304,192,1}},{true,{338,158,1}}}};
};
inline void ArrangeDiamond(Layout& l) {
    float cx=0,cy=0;
    for(unsigned i=X;i<Count;++i) {cx+=l.buttons[i].pose.x/4;cy+=l.buttons[i].pose.y/4;}
    const float angle=l.diamond_rotation*3.14159265358979323846f/180;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    constexpr std::array<std::array<float,2>,4> directions{{{0,-1},{-1,0},{0,1},{1,0}}};
    for(unsigned i=X;i<Count;++i) {
        const auto d=directions[i-X];
        auto& p=l.buttons[i].pose;
        p.x=cx+l.diamond_spacing*(d[0]*cosine-d[1]*sine);
        p.y=cy+l.diamond_spacing*(d[0]*sine+d[1]*cosine);
    }
}
inline constexpr float BubbleWidth(unsigned i) {return i==B?48.f:40.f;}
inline constexpr float BubbleAspectY=47.f/48.f;
inline std::array<float,2> ItemPivot(const Layout& l) {
    std::array<float,2> center{};
    for(unsigned i=X;i<Count;++i) {
        center[0]+=l.buttons[i].pose.x/4;
        center[1]+=l.buttons[i].pose.y/4;
    }
    return center;
}
inline PausePrompts::Pose ItemPose(const Layout& l,unsigned i,const std::array<float,2>& pivot) {
    auto p=l.buttons[i].pose;
    p.x=pivot[0]+(p.x-pivot[0])*l.item_scale+l.item_x;
    p.y=pivot[1]+(p.y-pivot[1])*l.item_scale+l.item_y;
    p.scale*=l.item_scale;
    const auto personal=UserHUD::Sanitize(l.user_items[i-X]);
    p.x+=personal.x;p.y+=personal.y;
    if(personal.scale!=0)p.scale=std::clamp(p.scale*UserHUD::Factor(personal),.1f,16.f);
    return p;
}
inline std::mutex mutex;
inline Layout layout;
inline std::atomic<bool> dirty{false};
inline Layout GetLayout() {std::scoped_lock lock{mutex};return layout;}
// Composed alongside the native A-button layout in hud_layout.h.
inline Layout GetEffectiveLayout();
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{mutex};layout=value;if(save)dirty=true;
}
}
