// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_fade.h"
#include "oot3d/hud_layout.h"
#include "oot3d/interpolation_state.h"
#include <span>
#include <vector>

namespace OOT3D::CarrotAnimation {
constexpr std::uint32_t DismountAction=0x4c502c;
struct Input {
    std::uint32_t play{},player{},horse{};
    unsigned count{},generation{};
    std::uint64_t frame{};
    bool visible=false,valid=false;
};
inline std::mutex mutex;
inline Input input;
template<class M> void Publish(M& m,bool paused) {
    Input next;
    next.frame=Interpolation::guest_frames.load();next.generation=Interpolation::generation.load();
    next.play=m.Read(HUDFade::UI+0xc).value_or(0);
    next.player=m.Read(next.play+0x20ac).value_or(0);
    next.horse=m.Read(next.player+0x124).value_or(0);
    next.valid=!paused && HUDFade::GameplayReady(m,next.play) &&
        TitleScreen::Pointer(next.player,0x1718) &&
        m.Read(DismountAction)==0xe92d41f0 && m.Read(0x4c5114)==0xe3c00502;
    // 42df88/42df8c pass play+2ba4 to 444544; +274 is the HUD spur count.
    const auto count=m.Read(next.play+0x2ba4+0x274);
    next.count=count?*count&255:0;
    // The dismount action starts while native horseback HUD mode is still 5.
    next.visible=next.valid && m.Read(HUDFade::Action+0x48)==5 &&
        m.Read(next.player+0x1708)!=DismountAction &&
        TitleScreen::Pointer(next.horse,0x278) && count && next.count<=6;
    std::scoped_lock lock{mutex};input=next;
}
inline Input Get() {std::scoped_lock lock{mutex};return input;}
constexpr std::int64_t FadeUs=150000,SpendUs=160000;
inline std::int64_t DelayUs(float seconds) {
    return std::isfinite(seconds)?std::int64_t(std::clamp(seconds,0.f,2.f)*1000000):0;
}
// Each carrot occupies a 24x24 atlas cell. Keep linear texture sampling
// half a source texel inside it so adjacent rows cannot leak along the edge.
template<class Q> auto InsetUV(const Q& quad,unsigned vertex) {
    auto uv=quad[vertex].tex_coord0;
    for(unsigned axis=0;axis<2;++axis) {
        float lo=quad[0].tex_coord0[axis],hi=lo;
        for(const auto& v:quad) {lo=std::min(lo,v.tex_coord0[axis]);hi=std::max(hi,v.tex_coord0[axis]);}
        const float inset=(hi-lo)/48.f;
        uv[axis]=std::clamp(uv[axis],lo+inset,hi-inset);
    }
    return uv;
}
template<class V> struct History {
    using Quad=std::array<V,6>;
    std::array<Quad,6> row{},old{};
    std::array<std::optional<std::int64_t>,6> spent{};
    Input prior{};
    std::int64_t last{},changed{};
    float from=0;
    bool seen=false,target=false,cached=false;
    void Reset() {*this={};}
    float Opacity(std::int64_t now) const {
        const float step=std::clamp(float(now-changed)/FadeUs,0.f,1.f);
        return target?std::min(1.f,from+step):std::max(0.f,from-step);
    }
    bool Update(std::span<const V> vertices,Input next,std::int64_t now,
                std::uint64_t frame,unsigned generation,float delay_seconds=0) {
        if(!next.valid || next.generation!=generation || next.frame>frame || frame-next.frame>1 || vertices.size()!=36) {
            Reset();return false;
        }
        if(!seen || prior.play!=next.play || prior.player!=next.player || prior.generation!=generation ||
           (prior.visible && next.visible && prior.horse!=next.horse) ||
           now<last || now-last>250000)Reset();
        if(!seen) {seen=true;changed=now;}
        if(next.visible!=target) {from=Opacity(now);changed=now;target=next.visible;}
        if(next.visible) {
            if(cached && prior.visible && prior.horse==next.horse && next.count<prior.count)
                for(unsigned i=next.count;i<prior.count;++i) {spent[i]=now+DelayUs(delay_seconds);old[i]=row[i];}
            for(unsigned i=0;i<6;++i) {
                if(i<next.count)spent[i].reset();
                std::copy_n(vertices.begin()+i*6,6,row[i].begin());
            }
            cached=true;
        }
        prior=next;last=now;return true;
    }
    void AppendSprites(std::vector<V>& out,std::int64_t now,float configured_opacity=1) const {
        const float opacity=Opacity(now)*(std::isfinite(configured_opacity)?std::clamp(configured_opacity,0.f,1.f):1.f);
        if(!cached || opacity<=0)return;
        for(unsigned i=0;i<6;++i) {
            const float t=spent[i]?std::clamp(float(now-*spent[i])/SpendUs,0.f,1.f):1;
            const float bounce=spent[i] && t>0 && t<1?std::sin(t*3.14159265f):0;
            // Keep the charged artwork until an optional delay elapses,
            // then switch straight to the native spent sprite. Consumption
            // translates only screen Y: no scale pulse or opacity crossfade.
            const bool previous=spent[i] && now<*spent[i];
            for(unsigned j=0;j<6;++j) {
                auto v=row[i][j];
                if(previous) {
                    v.tex_coord0=old[i][j].tex_coord0;
                    v.tex_coord1=old[i][j].tex_coord1;
                    v.tex_coord2=old[i][j].tex_coord2;
                }
                v.tex_coord0=InsetUV(previous?old[i]:row[i],j);
                // The native top projection's first clip axis is screen Y.
                v.position[0]+=(3.f/120)*bounce*v.position[3];
                v.color[3]*=opacity;out.push_back(v);
            }
        }
    }
    void Append(std::vector<V>& out,std::int64_t now,const HUD::CarrotShadow& shadow={},float configured_opacity=1) const {
        if(!shadow.enabled) {AppendSprites(out,now,configured_opacity);return;}
        const auto finite=[](float v,float fallback,float lo,float hi) {
            return std::isfinite(v)?std::clamp(v,lo,hi):fallback;
        };
        const float strength=finite(shadow.strength,.6f,0,1);
        if(strength==0) {AppendSprites(out,now,configured_opacity);return;}
        std::vector<V> sprites;AppendSprites(sprites,now,configured_opacity);
        const float dx=finite(shadow.x,1.5f,-12,12),dy=finite(shadow.y,2,-12,12);
        const float blur=finite(shadow.blur,1,0,4);
        // Reuse the sprite alpha mask, with black vertex tint. A normalized
        // 5x5 binomial kernel softens the silhouette without a rectangular panel.
        constexpr std::array<float,5> weights{1,4,6,4,1};
        const int radius=blur>0?2:0;
        for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x) {
            const float weight=radius?weights[x+2]*weights[y+2]/256.f:1.f;
            const float alpha=radius?1-std::pow(1-std::min(strength,.9999f),weight):strength;
            for(auto v:sprites) {
                v.position[0]-=(dy+y*blur/2)/120*v.position[3];
                v.position[1]-=(dx+x*blur/2)/200*v.position[3];
                v.color[0]=v.color[1]=v.color[2]=0;
                v.color[3]*=alpha;out.push_back(v);
            }
        }
        // All silhouettes precede the full row, so one carrot's shadow can
        // never cover its neighbor. Vertical bounce and HUD fades match.
        out.insert(out.end(),sprites.begin(),sprites.end());
    }

};
}
