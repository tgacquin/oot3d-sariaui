// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompt_layout.h"
namespace OOT3D::Ocarina {
// Shared by mesh construction and both sides of slide interpolation.
constexpr unsigned QuadCount=108,ShadowLayers=5,AtlasDrawOffset=2+12*ShadowLayers;
constexpr unsigned DrawQuadCount=QuadCount+AtlasDrawOffset;
using Pose=PausePrompts::Pose;
enum Part {Stones,Notes,Name,SongsPrompt,BackPrompt,Count};
constexpr std::array Names{"stones","notes","name","songs_prompt","back_prompt"};
constexpr std::array Labels{"Song stones","Notes panel","Song name box",
                             "Start: Songs","B: Cancel / Back"};
struct SidePanel {
    float right=320,opacity=1,border=5;
};
struct PromptVignette {
    bool enabled=true;
    float strength=.55f,width=128,height=72,softness=.9f,x=0,y=0;
};
struct Layout : std::array<Pose,Count> {
    PromptVignette prompt_vignette;
    SidePanel panel;
    bool two_columns=false,bottom_panels=false;
    float row_spacing=0,column_spacing=0,stone_size=1;
    unsigned Columns() const {return two_columns?2:3;}
    Layout() {(*this)[Stones]={0,0,.75f};(*this)[Notes]={0,-6,.8f};
        (*this)[Name]={0,-10,.8f};(*this)[SongsPrompt]={38,220,1};(*this)[BackPrompt]={300,220,1};}
};
inline std::mutex mutex;
inline Layout layout;
inline std::atomic<bool> dirty{false},on_top{false};
inline std::atomic<float> slide_x{0};
inline Layout GetLayout() {std::scoped_lock lock{mutex};return layout;}
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{mutex};layout=value;if(save)dirty=true;
}
}
