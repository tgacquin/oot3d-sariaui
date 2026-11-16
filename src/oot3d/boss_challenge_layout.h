// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/screen_model_hooks.h"
#include <atomic>
#include <mutex>

namespace OOT3D::BossChallenge {
using TitleScreen::U32;
constexpr U32 Owner=0x5c3f58, NativeLayout=Owner+0x918;
enum Part {Prompt,Stones,Preview,QuitPrompt,Quit,Continue,Timer,Background,Background2,LeftStick,LeftArrow,RightStick,RightArrow,BossName,SelectPrompt,BackPrompt,FailedTitle,Border,Footer,ClearedTitle,NewRecord,Count};
constexpr std::array Names{"prompt","stones","preview","quit_prompt","quit","continue","timer","background","background2","left_stick","left_arrow","right_stick","right_arrow","boss_name","select_prompt","back_prompt","failed_title","border","footer","cleared_title","new_record"};
constexpr std::array Labels{"Boss selection prompt","Boss stones","Boss preview panel",
    "Quit challenge prompt","Quit stone","Continue stone","Challenge timer","Brown background panel 1","Brown background panel 2","Left navigation hint","Left red arrow","Right navigation hint","Right red arrow",
    "Selected boss name","A Select prompt","B Back prompt","Challenge Failed title","Border cutout","Bottom border strip","Challenge Cleared title","New Record stamp"};
struct Pose {float x=0,y=0,scale=1;};
struct Layout : std::array<Pose,Count> {
    bool two_columns=true;
    bool horizontal=false;
    float spacing_x=4,spacing_y=2;
    std::array<float,2> background_darkness{.2f,.2f};
    bool border_enabled=true,border_bottom=true;
    float panel_darkness=0,border_darkness=0,corner_radius=0;
    float inner_shadow_width=4,inner_shadow_opacity=.35f;
    float shadow_opacity=.55f,shadow_softness=10;
    Layout() {
        (*this)[Prompt]={0,-8,.8f};
        (*this)[Stones]={140,8,.6f};
        (*this)[Preview]={-76,0,.56f};
        (*this)[Timer]={0,22,1};
        (*this)[Background]={0,0,1.7f};
        (*this)[Background2]={240,0,1.7f};
        (*this)[LeftStick]={68,183,1};(*this)[LeftArrow]={48,183,1};
        (*this)[RightStick]={168,183,1};(*this)[RightArrow]={188,183,1};
        (*this)[BossName]={124,190,.85f};
        (*this)[SelectPrompt]={120,220,1};(*this)[BackPrompt]={220,220,1};
    }
};
inline std::mutex mutex;
inline Layout layout;
inline std::atomic<bool> dirty{false},on_top{false};
inline Layout GetLayout() {std::scoped_lock lock{mutex};return layout;}
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{mutex};layout=value;if(save)dirty=true;
}
inline bool Valid(const Pose& p) {
    return std::isfinite(p.x) && std::abs(p.x)<=400 && std::isfinite(p.y) &&
        std::abs(p.y)<=240 && std::isfinite(p.scale) && p.scale>=.1f && p.scale<=3;
}
template<class M> bool InBattle(M& m) {
    // This is the native challenge predicate (0035b164), including individual
    // rematches as well as the full gauntlet. Do not use scene/boss identities.
    const auto mode=m.Read(Owner+0x3ec);
    return mode && std::int32_t(*mode)>=0 && std::int32_t(*mode)<9 &&
        ((m.Read(Owner+8).value_or(0)>>16)&255)!=0;
}
template<class M> bool Loaded(M& m) {
    const auto flags=m.Read(Owner+8).value_or(0);
    return ((flags>>8)&255)!=0 && ((flags>>16)&255)!=0 &&
        m.Read(NativeLayout+4).value_or(0)!=0;
}
inline ScreenModels::Transform Transform(const Pose& p,float ax,float ay,float route=0) {
    if(!Valid(p))return {route,0};
    return {route+p.x,p.y,p.scale,p.scale,ax,ay,true};
}
inline std::array<float,2> NativeStone(unsigned i) {
    return {56.f+104*(i%3),68.f+52*(i/3)};
}
inline std::array<float,2> StoneCenter(unsigned i,const Layout& l) {
    const unsigned columns=l.horizontal?9:l.two_columns?2:1;
    const float dx=104*l[Stones].scale+std::clamp(l.spacing_x,-50.f,200.f);
    const float dy=52*l[Stones].scale+std::clamp(l.spacing_y,-25.f,120.f);
    return {160+l[Stones].x+(float(i%columns)-(columns-1)*.5f)*dx,
            120+l[Stones].y+(float(i/columns)-float((9+columns-1)/columns-1)*.5f)*dy};
}
inline ScreenModels::Transform StoneTransform(unsigned i,const Layout& l) {
    if(i>=9 || !Valid(l[Stones]) || !std::isfinite(l.spacing_x) || !std::isfinite(l.spacing_y))return {40,0};
    const auto native=NativeStone(i),center=StoneCenter(i,l);
    return {40+center[0]-native[0],center[1]-native[1],l[Stones].scale,l[Stones].scale,native[0],native[1],true};
}
// Native direction order: right, left, up, down. Native availability checks
// still skip locked bosses. Left/right traverse every boss in order; up/down
// retain the configured grid rows.
inline U32 Neighbor(U32 current,U32 direction,bool two_columns,bool horizontal=false) {
    if(current>=9 || direction>=4)return 0xffffffff;
    const U32 columns=horizontal?9:two_columns?2:1;
    if(direction==0)return (current+1)%9;
    if(direction==1)return (current+8)%9;
    if(direction==2)return current>=columns?current-columns:0xffffffff;
    return current+columns<9?current+columns:0xffffffff;
}
}
