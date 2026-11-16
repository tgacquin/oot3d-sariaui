// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>

namespace OOT3D::UserHUD {
// These are offsets from the authored layout, never replacement editor poses.
// Keep the first entries in HUD::Element order; persisted names are stable.
enum Element { Hearts, Rupees, Magic, AButton, LoadingIcon, Minimap, Keys, Timer,
    Minigame, ShardAgony, TargetIndicator, Carrots, OutsideMinimap, BButton,
    ItemButtons, DPad, XButton, YButton, IButton, IIButton, Count };
constexpr std::array Names{"hearts", "rupees", "magic", "a_button", "loading_icon",
    "minimap", "keys", "timer", "minigame", "shard_agony", "target_indicator",
    "carrots", "outside_minimap", "b_button", "item_buttons", "dpad", "x_button", "y_button", "i_button", "ii_button"};
constexpr std::array Labels{"Hearts", "Rupees", "Magic bar", "A button", "Loading icon",
    "Minimap", "Small keys", "Timer", "Minigame HUD", "Shard of Agony",
    "Targeting indicator", "Horse carrots", "Outside minimap", "B button",
    "Item buttons (X, Y, I, II)", "D-pad", "X button", "Y button", "I button", "II button"};
// Stable persisted indices remain available for old configurations. F2 exposes
// one map adjustment and no independent targeting/minigame-counter controls.
constexpr std::array EditableElements{Hearts,Rupees,Magic,Minimap,Keys,Timer,ShardAgony,Carrots};
constexpr std::array ButtonElements{AButton,BButton,XButton,YButton,IButton,IIButton,DPad};
struct Adjustment { float x=0, y=0, scale=0; }; // Scale is a percentage change.
inline constexpr int SidePaddingLimit=100,VerticalPaddingLimit=40;
struct Layout : std::array<Adjustment, Count> {
    int top=0,bottom=0,left=0,right=0;
    float button_scale=0;
    float heart_spacing=0; // Extra spacing relative to the authored heart row.
    // Zero inherits the authored magic dimensions; width is a relative factor.
    float magic_height=0,magic_border=0,magic_width=1;
};
inline std::mutex mutex;
inline Layout layout{};
inline std::atomic<bool> dirty{false};
inline std::atomic<bool> classic_file_select{false};
inline std::atomic<int> hd_numbers_override{-1}; // -1 inherits the authored default.
inline bool HDNumbers(bool authored) {
    const int value=hd_numbers_override.load();return value<0?authored:value!=0;
}
inline int AreaTitleY(int native_y,bool enabled,float offset) {
    // Native 0x1922cc uses Y=192 for area titles; scene IDs <=0x18
    // override it with Y=120 at 0x19230c. Cutscene titles use Y=120 too.
    if(native_y!=192 || !enabled || !std::isfinite(offset))return native_y;
    return native_y+int(std::lround(std::clamp(offset,-240.f,240.f)));
}
inline float Clean(float value,float low,float high) {
    return std::isfinite(value)?std::clamp(value,low,high):0.f;
}
inline Adjustment Sanitize(Adjustment a) {
    return {Clean(a.x,-400,400),Clean(a.y,-240,240),Clean(a.scale,-90,200)};
}
inline Layout GetLayout() {std::scoped_lock lock{mutex};return layout;}
inline Layout GetEffectiveOffsets() {
    auto result=GetLayout();
    // Anchor groups to their authored HUD edge, independently of user offsets.
    // Centered timers and carrots receive vertical padding only.
    for(auto e:{Hearts,Magic,DPad,ShardAgony,Rupees,Keys})result[e].x+=result.left;
    for(auto e:{AButton,BButton,ItemButtons,TargetIndicator,Minimap,OutsideMinimap,LoadingIcon,Minigame})result[e].x-=result.right;
    for(auto e:{Hearts,Magic,DPad,ShardAgony,AButton,BButton,ItemButtons,TargetIndicator,Carrots})result[e].y+=result.top;
    for(auto e:{Rupees,Keys,Minimap,OutsideMinimap,LoadingIcon,Timer,Minigame})result[e].y-=result.bottom;
    result[OutsideMinimap]=result[Minimap];
    for(auto e:{AButton,BButton,ItemButtons,DPad})
        result[e].scale=100.f*((1.f+result[e].scale/100.f)*(1.f+result.button_scale/100.f)-1.f);
    return result;
}
inline void SetLayout(Layout value,bool save=true) {
    for(auto& a:value)a=Sanitize(a);
    value.button_scale=Clean(value.button_scale,-90,200);
    value.heart_spacing=Clean(value.heart_spacing,-28,28);
    value.magic_height=value.magic_height==0?0:Clean(value.magic_height,1,8);
    value.magic_border=value.magic_border==0?0:Clean(value.magic_border,1,4);
    value.magic_width=std::isfinite(value.magic_width)?std::clamp(value.magic_width,.5f,4.f):1.f;
    value.top=std::clamp(value.top,-VerticalPaddingLimit,VerticalPaddingLimit);
    value.bottom=std::clamp(value.bottom,-VerticalPaddingLimit,VerticalPaddingLimit);
    value.left=std::clamp(value.left,-SidePaddingLimit,SidePaddingLimit);
    value.right=std::clamp(value.right,-SidePaddingLimit,SidePaddingLimit);
    std::scoped_lock lock{mutex};layout=value;if(save)dirty=true;
}
inline float Factor(const Adjustment& a) {return 1.f+a.scale/100.f;}
template<class Pose> Pose Apply(Pose p,const Adjustment& a,float maximum_scale=3.f) {
    // Exact identity matters: merely opening F2 must not change the authored HUD.
    if(a.x!=0)p.x=std::clamp(p.x+a.x,-400.f,400.f);
    if(a.y!=0)p.y=std::clamp(p.y+a.y,-240.f,240.f);
    if(a.scale!=0)p.scale=std::clamp(p.scale*Factor(a),.1f,maximum_scale);
    return p;
}
} // namespace OOT3D::UserHUD
