// Adapted from the MM3D presentation widescreen layout.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <atomic>
#include <array>
#include <cstdint>
#include <cmath>
#include "common/math_util.h"

namespace OOT3D::Widescreen {
constexpr float NativeAspect = 400.f / 240.f;
constexpr float Aspect = 16.f / 9.f;
constexpr float HorizontalScale = NativeAspect / Aspect; // 15/16
inline std::atomic<bool> enabled{false};
inline std::atomic<bool> dirty{false};
inline std::atomic<bool> supported_layout{false};
inline bool Active() { return enabled.load() && supported_layout.load(); }
inline bool IsTopViewport(unsigned width,unsigned height) {
    // Captured OoT3D top draws use 480x400, while bottom draws use 240x320.
    // Inspect guest dimensions, not cache surface offsets or resolution-scaled extents.
    return height==400 && (width==240 || width==480);
}

// Captured USA Rev 1 clear: untextured triangle strip, unconditional depth
// write and RGBA write. Narrowing this quad leaves stale depth in the new side
// columns, so world geometry fails its depth test there.
inline bool ClearState(std::uint64_t shader, std::uint64_t swizzle, unsigned count,
                       unsigned topology, unsigned depth, unsigned textures,
                       bool alpha_test, bool stencil_test) {
    return shader == 0x6352a3be33cb99c6ULL && swizzle == 0xba817d0ef27276d0ULL &&
           count == 4 && topology == 1 && depth == 0x1f11 && textures == 0 &&
           !alpha_test && !stencil_test;
}
// Native fades and letterbox bars share the queue's straight-alpha material
// without depth writes. Geometry checks distinguish them from solid UI panels.
inline bool FadeState(std::uint64_t shader, std::uint64_t swizzle, unsigned count,
                      unsigned topology, unsigned depth, unsigned textures,
                      bool alpha_test, bool stencil_test,
                      const std::array<unsigned, 4>& blend) {
    return shader == 0x6352a3be33cb99c6ULL && swizzle == 0xba817d0ef27276d0ULL &&
           count == 4 && topology == 1 && depth == 0xf10 && textures == 0 &&
           !alpha_test && !stencil_test &&
           (blend == std::array<unsigned,4>{1,0,6,7} || blend == std::array<unsigned,4>{1,0,1,0});
}
using Row = std::array<float, 4>;
inline bool ClearGeometry(const std::array<Row, 4>& projection,
                          const std::array<Row, 3>& model,
                          const std::array<Row, 4>& vertices) {
    const auto near = [](const Row& a, const Row& b) {
        for (unsigned i = 0; i < 4; ++i)
            if (!std::isfinite(a[i]) || std::abs(a[i] - b[i]) > .000001f) return false;
        return true;
    };
    return near(projection[0], {0, -2.f/240, 0, 1}) &&
           near(projection[1], {-2.f/400, 0, 0, 1}) &&
           near(projection[3], {0, 0, 0, 1}) &&
           near(model[0], {1, 0, 0, 0}) && near(model[1], {0, 1, 0, 0}) &&
           near(model[2], {0, 0, 1, -10}) &&
           vertices == std::array<Row, 4>{{{0,0,0,1}, {400,0,0,1}, {0,240,0,1}, {400,240,0,1}}};
}

inline bool FadeGeometry(const std::array<Row, 4>& projection,
                         const std::array<Row, 3>& model,
                         const std::array<Row, 4>& vertices) {
    const auto near = [](const Row& a, const Row& b) {
        for (unsigned i = 0; i < 4; ++i)
            if (!std::isfinite(a[i]) || std::abs(a[i] - b[i]) > .000001f) return false;
        return true;
    };
    // The fade scales a unit square to 400x240 and centers it on the display.
    // Do not depend on its color or alpha: black/white and partial fades all
    // need identical coverage throughout the transition.
    const float z = model[2][3];
    const float depth = projection[2][2] * z + projection[2][3];
    return std::isfinite(depth) && depth >= -1 && depth <= 1 &&
           projection[2][0] == 0 && projection[2][1] == 0 &&
           near(projection[0], {0, -2.f/240, 0, 1}) &&
           near(projection[1], {-2.f/400, 0, 0, 1}) &&
           near(projection[3], {0, 0, 0, 1}) &&
           near(model[0], {400, 0, 0, 200}) && near(model[1], {0, 240, 0, 120}) &&
           near(model[2], {0, 0, 1, z}) &&
           vertices == std::array<Row,4>{{{-.5f,-.5f,0,1}, {.5f,-.5f,0,1},
                                        {-.5f,.5f,0,1}, {.5f,.5f,0,1}}};
}

// CPU-shaded full-screen quads, after native projection.
inline bool FullScreenClipGeometry(const std::array<Row, 6>& vertices, float depth) {
    unsigned corners = 0;
    for (const auto& v : vertices) {
        if (std::abs(std::abs(v[0]) - 1.f) > .000001f ||
            std::abs(std::abs(v[1]) - 1.f) > .000001f ||
            std::abs(v[2] - depth) > .000001f || v[3] != 1.f) return false;
        for (float value : v) if (!std::isfinite(value)) return false;
        corners |= 1u << ((v[0] > 0 ? 1 : 0) | (v[1] > 0 ? 2 : 0));
    }
    return corners == 15;
}

inline bool ClearClipGeometry(const std::array<Row, 6>& vertices) {
    return FullScreenClipGeometry(vertices, -1.f);
}
inline bool FadeClipGeometry(const std::array<Row, 6>& vertices) {
    const float depth = vertices.front()[2];
    return depth >= -1 && depth <= 1 && FullScreenClipGeometry(vertices, depth);
}

// 2ea9e0 submits [0,400] x [0,height] and [0,400] x [240-height,240]
// through 2e74c0/2d3f2c. The queue scales a unit square and centers each bar.
// Pass 2 uses a different depth range from fades, so validate its screen-space
// shape independently of fade depth. Only width is expanded by the renderer.
inline bool LetterboxGeometry(const std::array<Row,4>& projection,
                             const std::array<Row,3>& model,
                             const std::array<Row,4>& vertices) {
    const auto near=[](const Row& a,const Row& b) {
        for(unsigned i=0;i<4;++i)
            if(!std::isfinite(a[i]) || std::abs(a[i]-b[i])>.000001f)return false;
        return true;
    };
    const float height=model[1][1];
    if(!std::isfinite(height) || height<=0 || height>=120)return false;
    if(!near(projection[0],{0,-2.f/240,0,1}) ||
       !near(projection[1],{-2.f/400,0,0,1}) ||
       !near(projection[3],{0,0,0,1}) ||
       !near(model[0],{400,0,0,200}) ||
       !(near(model[1],{0,height,0,height/2}) ||
         near(model[1],{0,height,0,240-height/2})) ||
       !near(model[2],{0,0,1,model[2][3]}))return false;
    for(float v:projection[2])if(!std::isfinite(v))return false;
    const float depth=projection[2][2]*model[2][3]+projection[2][3];
    if(projection[2][0]!=0 || projection[2][1]!=0 || depth < -1 || depth > 1)return false;
    return vertices==std::array<Row,4>{{{-.5f,-.5f,0,1},{.5f,-.5f,0,1},
                                      {-.5f,.5f,0,1},{.5f,.5f,0,1}}};
}
inline bool LetterboxClipGeometry(const std::array<Row,6>& vertices) {
    // PICA rotates the display: clip X is vertical, clip Y is horizontal.
    float low=1,high=-1;
    const float depth=vertices[0][2];
    for(const auto& v:vertices) {
        for(float value:v)if(!std::isfinite(value))return false;
        if(std::abs(std::abs(v[1])-1)>.000001f || v[3]!=1 ||
           std::abs(v[2]-depth)>.000001f || depth < -1 || depth > 1)return false;
        low=std::min(low,v[0]);high=std::max(high,v[0]);
    }
    if(high<=low || high-low>=1 || low < -1.000001f || high>1.000001f ||
       (std::abs(low+1)>.000001f && std::abs(high-1)>.000001f))return false;
    unsigned corners=0;
    for(const auto& v:vertices) {
        const bool upper=std::abs(v[0]-high)<.000001f;
        if(!upper && std::abs(v[0]-low)>.000001f)return false;
        corners|=1u<<((upper?1:0)|(v[1]>0?2:0));
    }
    return corners==15;
}

// Fit the wider top screen in its available slot. Keep separate neighboring
// screens outside the slot; an existing overlapping bottom-screen inset stays an inset.
inline Common::Rectangle<unsigned> FitTop(Common::Rectangle<unsigned> top,
    const Common::Rectangle<unsigned>& bottom, unsigned window_width, bool bottom_enabled) {
    unsigned left=0, right=window_width;
    if(bottom_enabled && bottom.top<top.bottom && bottom.bottom>top.top) {
        if(bottom.left>=top.right) right=std::min(right,bottom.left);
        if(bottom.right<=top.left) left=std::max(left,bottom.right);
    }
    if(right<=left || !top.GetHeight()) return top;
    const float h=std::min(float(top.GetHeight()),float(right-left)/Aspect);
    const unsigned height=std::max(1u,unsigned(std::lround(h)));
    const unsigned width=std::min(right-left,std::max(1u,unsigned(std::lround(h*Aspect))));
    const float cx=(float(top.left)+top.right)*.5f;
    const unsigned x=unsigned(std::clamp(cx-width*.5f,float(left),float(right-width)));
    const unsigned y=top.top+(top.GetHeight()-height)/2;
    return {x,y,x+width,y+height};
}
}
