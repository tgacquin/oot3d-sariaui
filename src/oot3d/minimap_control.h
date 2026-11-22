// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include "oot3d/title_hooks.h"
#include "oot3d/hud_layout.h"
#include "oot3d/minimap_bounds.h"
#include "oot3d/hud_fade.h"
#include "oot3d/ocarina_input.h"
#include "oot3d/save_menu.h"

namespace OOT3D::MinimapControl {
using TitleScreen::U32;
constexpr U32 Owner=0x4fda6c, Action=0x4fc648;
template<class M> HUD::Pose LayoutPose(M& m,const HUD::Layout& layout) {
    // Native 42c0a4 sets +50=1 for room/floor dungeon maps and +50=0 for
    // field maps (including paths such as the Sacred Forest Meadow).
    // Follow the controller drawing the map, not a cached scene-number guess.
    return m.Read(Owner+0x50)==std::optional<U32>{0}?layout.outside_minimap:layout[HUD::Minimap];
}
constexpr U32 FirstSVC=0xfff100, Resume=0x42e6bc, NativeToggle=0x2f009c;
constexpr TitleScreen::Site Site{0x42e60c,0xe320f000,0xeb004c10};
constexpr std::array<TitleScreen::Site,3> FadeSites{{
    {0x42bb04,0xe59f63e4,0xe5d60f38}, // Before native map visibility gates.
    {0x441f24,0xe5940008,0x0a000002}, // Map submission, after loading gate.
    {0x42b9d4,0xe1a05000,0xeb005965}, // HUD entry, BEFORE all early-return gates.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<FadeSites.size();++i)
        if(pc==FadeSites[i].pc && word==FadeSites[i].instruction)
            return 0xef000000|(FirstSVC+1+i);
    return pc==Site.pc && word==Site.instruction ? 0xef000000|FirstSVC : word;
}
template<class M> bool CodeMatches(M& m) {
    constexpr std::array<std::pair<U32,U32>,8> guards{{
        {Site.pc,Site.instruction},{Site.pc+4,Site.following},{Resume,0xe59f0344},
        {NativeToggle,0xe59f103c},{0x2f00b8,0xe5810068},{0x2f00e0,Action},
        {0x2f00ec,0x01000499},{0x2f00f0,0x01000498}}};
    for(auto [a,v]:guards)if(m.Read(a)!=std::optional<U32>{v})return false;
    for(const auto& s:FadeSites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return true;
}
inline std::atomic<unsigned> requests{0};
inline std::atomic<bool> shown{true};
// Frontend requests are consumed at the native map-input update. HID discards
// requests made outside gameplay so they cannot fire after a menu closes.
inline void RequestToggle() { requests.fetch_add(1); }
inline bool Consume(bool gameplay) {
    const unsigned count=requests.exchange(0);
    if(gameplay && (count&1)){shown.store(!shown.load());return true;}
    return false;
}
inline void Reset() {requests.store(0);shown.store(true);}
// Stick-click visibility is immediate for map art and markers alike. Native
// scene/HUD fades still multiply this value at their existing draw hooks.
inline float Opacity() {return shown.load()?1.f:0.f;}
// Keep a recently submitted map alive only for the instrument's exit fade.
// No source material, loading state, or native visibility flag is changed.
struct InstrumentFade {
    U32 context=0,scene=0,model=0;
    std::int64_t last=0,submitted=-1000000;
    float opacity=1,return_from=0;
    std::optional<std::int64_t> return_started;
    bool initialized=false,hidden=false,retain=false,forced=false,retained_draw=false;
    void Advance(bool instrument,std::int64_t now) {
        const float step=initialized?std::clamp(float(now-last)/HUDFade::FadeUs,0.f,1.f):0.f;
        if(instrument && !hidden)retain=initialized && now>=submitted && now-submitted<100000;
        if(!instrument)retain=false;
        hidden=instrument;
        if(instrument) {
            opacity=std::max(0.f,opacity-step);return_started.reset();
        }
        last=now;initialized=true;
    }
    template<class M,class R> void Handle(M& m,R& r,unsigned site,bool paused,std::int64_t now) {
        if(site==1) {
            r[6]=m.Read(0x42bef0).value_or(0); // Displaced LDR.
            // Entry already submitted the outgoing map. If native code also
            // reaches this block, do not enqueue the same models twice.
            if(retained_draw)r[15]=0x42bb94;
            return;
        }
        if(site==2) {
            r[0]=m.Read(r[4]+8).value_or(0);
            if(!forced && r[0]) {
                submitted=now;
                // Start on actual submission, not on instrument cleanup:
                // native HUD gates can withhold the map for several frames.
                if(!hidden && opacity<1) {
                    if(!return_started) {return_started=now;return_from=opacity;}
                    opacity=std::clamp(return_from+float(now-*return_started)/HUDFade::FadeUs,0.f,1.f);
                }
            }
            forced=false;
            return;
        }
        r[5]=r[0]; // Displaced MOV at HUD entry.
        retained_draw=false;
        const U32 play=m.Read(HUDFade::UI+0xc).value_or(0);
        const U32 next_scene=m.Read(play+0x104).value_or(0);
        const U32 next_model=m.Read(Owner+8).value_or(0);
        if(paused || SaveMenu::Active(m) || !Ocarina::CodeMatches(m) ||
           !TitleScreen::Pointer(play,0x7f28) || r[5]!=play ||
           (m.Read(play+0x100).value_or(0)&0xffff)!=0x0203 ||
           !HUDFade::Zero(m,play+0x5c2c,0xff00) ||
           !HUDFade::Zero(m,play+0x7f10,0xff0000)) {*this={};return;}
        if(context!=play || scene!=next_scene || model!=next_model || now<last ||
           (initialized && now-last>250000)) *this={};
        context=play;scene=next_scene;model=next_model;
        Advance(Ocarina::State(m)!=0,now);
        if(hidden && retain && opacity>0 && next_model &&
           m.Read(Owner+0x60)==std::optional<U32>{0} &&
           m.Read(Action+0x68)==std::optional<U32>{1} && Opacity()>0) {
            // Enter the original draw function with the same caller frame.
            // Its own room-loading gate and model/marker checks still run.
            forced=retained_draw=true;r[14]=0x42b9d8;r[15]=0x441f08;
        }
    }
};
// Replace the old hold-Down / any-D-pad restore block. Run the actual native
// toggle for its hide/show sound, then keep native map submission enabled so
// our visibility gate can hide the map and all its markers together.
struct ToggleHook {
    static constexpr U32 Frame=80,Cookie=0x4d415054;
    template<class M,class R> void Handle(M& m,R& r,bool gameplay) {
        const U32 sp=r[13];
        if(m.Read(sp+64)==std::optional<U32>{Cookie} &&
           m.Read(sp+68)==std::optional<U32>{sp^Cookie} &&
           m.Read(sp+52)==std::optional<U32>{sp+Frame} &&
           m.Read(sp+60)==std::optional<U32>{Site.pc+4}) {
            m.Write(sp+64,0);
            for(unsigned i=0;i<16;++i)r[i]=m.Read(sp+4*i).value_or(0);
            m.Write(Action+0x68,1);r[15]=Resume;return;
        }
        if(!gameplay){Consume(false);r[15]=Resume;return;}
        const auto native=m.Read(Action+0x68);
        if(!native || *native>1){Consume(false);return;}
        m.Write(Action+0x68,1);
        if(m.Read(0x50af34+0x7c))m.Write(0x50af34+0x7c,0);
        r[15]=Resume;
        if(!requests.load())return;
        // Same map-availability predicate as native 0x2f00f4.
        if(!m.Read(Owner+8).value_or(0)){Consume(false);return;}
        if(sp<Frame || !TitleScreen::Pointer(sp-Frame,Frame)){Consume(false);return;}
        for(U32 at=0;at<Frame;at+=4)if(!m.Read(sp-Frame+at)){Consume(false);return;}
        if(!Consume(true))return;
        // Store the return frame on the guest stack so a savestate during
        // the native audio call still returns safely.
        for(unsigned i=0;i<16;++i)m.Write(sp-Frame+4*i,i==15?Site.pc+4:r[i]);
        m.Write(sp-Frame+64,Cookie);m.Write(sp-Frame+68,(sp-Frame)^Cookie);
        m.Write(Action+0x68,shown.load()?0:1);
        r[13]=sp-Frame;r[14]=Site.pc;r[15]=NativeToggle;
    }
};
template<class M> bool IsModel(M& m,U32 model) {
    if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=std::optional<U32>{0x4ebd60})return false;
    if(m.Read(Owner+8)==std::optional<U32>{model})return true;
    for(U32 off:{0x20u,0x28u}) {
        const U32 marker=m.Read(Owner+off).value_or(0);
        if(TitleScreen::Pointer(marker,8) && m.Read(marker+4)==std::optional<U32>{model})return true;
    }
    return false;
}
// The native map helper contains a room/area-specific rectangle plus a
// separate animation offset (0x2f9a1c). Read these original inputs so arrows
// and model draws use the same corner even before the map upload is rebuilt.
// Do not cache a room's dimensions across loads or change its source geometry.
template<class M> std::optional<std::array<float,2>> NativeCorner(M& m) {
    const U32 model=m.Read(Owner+8).value_or(0),helper=m.Read(Owner+0x18).value_or(0);
    const auto mesh=m.Read(Owner+4);
    if(!mesh || !TitleScreen::Pointer(*mesh,0x1b8) ||
       !TitleScreen::Pointer(model,0x1e4) || !TitleScreen::Pointer(helper,0x20) ||
       m.Read(model)!=std::optional<U32>{0x4ebd60} || m.Read(helper)!=std::optional<U32>{1} ||
       m.Read(model+4)!=mesh)return {};
    const U32 src=m.Read(helper+0xc).value_or(0),offset=m.Read(helper+0x1c).value_or(0);
    if(!TitleScreen::Pointer(src,48) || !TitleScreen::Pointer(offset,8))return {};
    std::array<float,12> v,matrix;
    std::array<float,2> shift;
    if(!TitleScreen::ReadFloats(m,src,v) || !TitleScreen::ReadFloats(m,offset,shift) ||
       !TitleScreen::ReadFloats(m,model+0xc,matrix))return {};
    float left=1e6f,top=1e6f,right=-1e6f,bottom=-1e6f;
    std::array<float,8> xy;
    for(unsigned i=0;i<12;i+=3) {
        const float x=v[i]+shift[0],y=v[i+1]+shift[1],z=v[i+2];
        const float sx=matrix[0]*x+matrix[1]*y+matrix[2]*z+matrix[3];
        const float sy=matrix[4]*x+matrix[5]*y+matrix[6]*z+matrix[7];
        xy[i/3*2]=sx;xy[i/3*2+1]=sy;
        left=std::min(left,sx);right=std::max(right,sx);
        top=std::min(top,sy);bottom=std::max(bottom,sy);
    }
    if(!std::isfinite(right) || !std::isfinite(bottom) || right<=left || bottom<=top ||
       right-left>512 || bottom-top>512 || left>=400 || top>=240 || right<=0 || bottom<=0)return {};
    std::array<float,8> uv;
    const U32 coords=m.Read(helper+0x14).value_or(0);
    if(TitleScreen::Pointer(coords,32) && TitleScreen::ReadFloats(m,coords,uv))
        if(const auto edges=TextureEdges(m,*mesh,uv))
            if(const auto visible=VisibleCorner(xy,uv,*edges))return visible;
    return std::array{right,bottom};
}
struct Placement {float x,y,scale,ax,ay;};
template<class M> Placement Place(M& m,const HUD::Pose& p,bool lock_corner) {
    // Preserve the existing pivot when unlocked or while no map is loaded.
    if(lock_corner)if(const auto corner=NativeCorner(m))
        return {128+p.x-(*corner)[0],232+p.y-(*corner)[1],p.scale,(*corner)[0],(*corner)[1]};
    return {p.x,p.y,p.scale,40,204};
}
// Dungeon room changes destroy the old map (2f233c), load the replacement
// asynchronously (441a88), then fade its helper colors in (42c0a4).
// Arrows live in the A-button atlas, so they can outlive that map. Follow the
// map's own readiness and alpha instead of relocating those orphaned arrows.
template<class M> float DungeonMarkerOpacity(M& m) {
    if(m.Read(Owner+0x50)!=std::optional<U32>{1})return 1;
    if(m.Read(Owner+0x60).value_or(1) || m.Read(Owner+0x58).value_or(1))return 0;
    const U32 model=m.Read(Owner+8).value_or(0),mesh=m.Read(Owner+4).value_or(0);
    const U32 helper=m.Read(Owner+0x18).value_or(0);
    if(!TitleScreen::Pointer(model,0x1e4) || !TitleScreen::Pointer(mesh,0x1b8) ||
       !TitleScreen::Pointer(helper,0x20) || m.Read(model)!=std::optional<U32>{0x4ebd60} ||
       m.Read(model+4)!=std::optional<U32>{mesh} || m.Read(helper)!=std::optional<U32>{1})return 0;
    const U32 colors=m.Read(helper+0x18).value_or(0);
    if(!TitleScreen::Pointer(colors,64))return 0;
    const auto word=m.Read(model+0xfc);if(!word)return 0;
    float alpha=std::bit_cast<float>(*word);
    if(!std::isfinite(alpha) || alpha<0 || alpha>1)return 0;
    for(unsigned v=0;v<4;++v) {
        const auto value=m.Read(colors+v*16+12);if(!value)return 0;
        const float a=std::bit_cast<float>(*value);
        if(!std::isfinite(a) || a<0 || a>1)return 0;
        alpha=std::min(alpha,a);
    }
    return alpha;
}
// Each model's fresh stack material preserves native visibility and fades.
template<class M> void Material(M& m,U32 model,U32 alpha,float opacity) {
    if(!IsModel(m,model))return;
    const float map_alpha=m.Read(Owner+8)==std::optional<U32>{model}?1: DungeonMarkerOpacity(m);
    if(opacity==1 && map_alpha==1)return;
    const auto word=m.Read(alpha);if(!word)return;
    const float native=std::bit_cast<float>(*word);
    if(std::isfinite(native) && native>=0 && native<=1)
        m.Write(alpha,std::bit_cast<U32>(std::min(native,map_alpha)*opacity));
}
// Player/entrance arrows share the A-button atlas. Modify their fresh upload,
// never its source colors or the other six quads (A and localized labels).
template<class M> bool CopyArrows(M& m,U32 mesh,U32 dst,U32 src,U32 bytes,float opacity) {
    if(bytes!=8*64 || !TitleScreen::Pointer(src,bytes) ||
       !TitleScreen::Pointer(dst,bytes) || (src<dst+bytes && dst<src+bytes))return false;
    const U32 model=m.Read(Action+0x20).value_or(0),helper=m.Read(Action+0x18).value_or(0);
    if(!TitleScreen::Pointer(model,0x1e4) || !TitleScreen::Pointer(helper,0x20) ||
       m.Read(model)!=std::optional<U32>{0x4ebd60} ||
       m.Read(model+4)!=std::optional<U32>{mesh} ||
       m.Read(helper)!=std::optional<U32>{8} || m.Read(helper+0x18)!=std::optional<U32>{src})return false;
    opacity*=UserHUD::GetLayout().minimap_opacity;
    const float map_alpha=DungeonMarkerOpacity(m);
    if(opacity==1 && map_alpha==1)return false;
    std::array<U32,8*16> colors{};
    for(unsigned i=0;i<colors.size();++i) {
        const auto word=m.Read(src+4*i);
        if(!word || !m.Read(dst+4*i))return false;
        colors[i]=*word;
    }
    for(unsigned q:{2u,3u})for(unsigned v=0;v<4;++v) {
        const unsigned i=q*16+v*4+3;
        const float native=std::bit_cast<float>(colors[i]);
        if(!std::isfinite(native) || native<0 || native>1)return false;
        colors[i]=std::bit_cast<U32>(std::min(native,map_alpha)*opacity);
    }
    for(unsigned i=0;i<colors.size();++i)m.Write(dst+4*i,colors[i]);
    return true;
}
}
