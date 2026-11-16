// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_frame_asset.h"
#include "oot3d/pause_prompts.h"
#include "oot3d/hud_layout.h"
#include "oot3d/loading_icon_state.h"

namespace OOT3D::LoadingIcon {
constexpr unsigned Width=64,Height=64;
using Pixels=std::array<std::uint32_t,Width*Height/2>;
inline std::shared_ptr<const Pixels> pixels;
inline auto GetPixels() {return std::atomic_load(&pixels);}
inline std::optional<Pixels> MakePixels(std::span<const std::uint8_t> source) {
    using MapFrame::LE32;
    if(source.size()!=72+Width*Height*2 || LE32(source,0)!=0x62787463 ||
       LE32(source,4)!=source.size() || LE32(source,8)!=1 || LE32(source,20)!=72 ||
       LE32(source,44)!=(Width|(Height<<16)) || LE32(source,48)!=0x80336752)return {};
    Pixels out;for(unsigned i=0;i<out.size();++i)out[i]=LE32(source,72+4*i);
    return out;
}
using Geometry=PausePrompts::QuadGeometry<3>;
inline Geometry Draw(std::int64_t now,float opacity,HUD::Pose pose,Anchor base=TopAnchor()) {
    if(!std::isfinite(pose.scale) || pose.scale<=0)pose={};
    const bool large=base.half>8;
    const std::array<std::array<float,2>,3> small{{{0,48},{16,48},{16,32}}};
    const std::array<std::array<float,2>,3> big{{{32,32},{0,0},{32,0}}};
    const std::array<float,3> phases{8,0,16};
    const float size=large?32:16,half=base.half*pose.scale;
    Geometry g;
    for(unsigned i=0;i<3;++i) {
        const auto uv=(large?big:small)[i];
        g.Add(base.x+pose.x-half,base.y+pose.y-half,2*half,2*half,
            (uv[0]+.5f)/64,(uv[1]+size-.5f)/64,(uv[0]+size-.5f)/64,(uv[1]+.5f)/64);
        // Original throbber's three staggered triangle pulses, at native
        // 60 Hz presentation cadence with continuous values between ticks.
        float a=std::fmod(std::max<std::int64_t>(now,0)*.00006+phases[i],24.f)*.125f;
        if(a>=1)a=2-a;
        a=std::max(a,0.f)*std::clamp(opacity,0.f,1.f);
        for(unsigned v=0;v<4;++v)g.colors[i*16+v*4+3]=a;
    }
    return g;
}
}
