// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_routing.h"
#include "oot3d/interpolation_state.h"
#include "oot3d/ocarina_layout.h"
#include "oot3d/title_hooks.h"
#include "oot3d/textbox_interpolation.h"

namespace OOT3D::OcarinaSlide {
inline bool ValidDrawCount(unsigned indices) {
    return indices && indices%6==0 && indices<=Ocarina::DrawQuadCount*6;
}
inline std::uint64_t LayoutKey(const Ocarina::Layout& l) {
    auto key=HUD::HashStart;
    const auto add=[&](float f){key=HUD::HashWord(key,std::bit_cast<std::uint32_t>(f));};
    for(const auto& p:l){add(p.x);add(p.y);add(p.scale);}
    for(float f:{l.panel.right,l.panel.opacity,l.panel.border,l.row_spacing,l.column_spacing,l.stone_size})add(f);
    return HUD::HashWord(HUD::HashWord(key,l.two_columns),l.bottom_panels);
}
struct History {
    std::uint64_t frame=0,key=0;
    std::int64_t time=0;
    unsigned epoch=0;
    float offset=0,shift=0;
    bool seen=false;
    float Observe(std::uint64_t next,std::int64_t now,unsigned generation,std::uint64_t layout,float x) {
        if(seen && next==frame && generation==epoch && now==time && key==layout && x==offset)return shift;
        const bool adjacent=seen && next==frame+1 && generation==epoch && layout==key &&
            now-time>=20000 && now-time<=50000;
        shift=adjacent?(offset-x)*.5f:0;
        seen=true;frame=next;time=now;epoch=generation;key=layout;offset=x;
        return shift;
    }
};
// Bounded diagnostics for a reported native-rate slide; no per-vertex logging.
inline std::atomic<unsigned> capture_attempts{0},capture_published{0},capture_vertices{0},capture_indices{0};
inline std::atomic<std::uint32_t> capture_desc{0};
inline std::atomic<unsigned> capture_logs{0},renderer_logs{0};
inline std::atomic<std::uint64_t> diagnostic_start{0},diagnostic_end{0};
inline void ArmDiagnostics() {
    capture_logs=renderer_logs=0;capture_attempts=capture_published=0;
    diagnostic_start=Interpolation::guest_frames.load();
    diagnostic_end=diagnostic_start.load()+360;
}
inline bool DiagnosticActive() {
    const auto frame=Interpolation::guest_frames.load(),end=diagnostic_end.load();
    return end!=0 && frame>=diagnostic_start.load() && frame<end;
}
inline bool Recent(std::uint64_t captured,std::uint64_t rendered) {
    // Native model submissions are consumed after EndGameFrame increments the
    // guest counter. Match the uploaded bytes from this or the preceding frame.
    return rendered>=captured && rendered-captured<=1;
}
struct Interval {
    float start{},end{}; // Unclamped clock endpoints of this portion of the 180 ms slide.
    static float Remaining(float progress) {
        const float t=1-std::clamp(progress,0.f,1.f);
        return t*t*t;
    }
    float PreviousWeight(float phase) const {
        const float a=Remaining(start),b=Remaining(end);
        if(a==b)return 1-phase;
        return (Remaining(std::lerp(start,end,phase))-b)/(a-b);
    }
};
struct CursorStep {float x{},y{},phase{};};
struct CursorPath {
    std::array<CursorStep,8> steps{};
    unsigned count{};
};
struct Route {
    std::uint64_t hash=0,frame=0;
    unsigned vertices=0,epoch=0;
    std::array<float,3> translation{};
    Interval interval{};
    struct Stone {std::array<float,3> delta{};float phase{};};
    std::array<Stone,8> stones{};
    unsigned stone_count{};
    bool Moving() const {return translation!=std::array<float,3>{} || stone_count>0;}
    std::array<float,3> At(float phase) const {
        auto delta=translation;
        const float weight=2*interval.PreviousWeight(phase);
        for(auto& value:delta)value*=weight;
        for(unsigned s=0;s<stone_count;++s)if(phase<stones[s].phase) {
            for(unsigned i=0;i<3;++i)delta[i]+=stones[s].delta[i];
            break;
        }
        return delta;
    }
};
inline std::mutex mutex;
inline std::array<Route,64> routes{};
inline unsigned cursor=0;
inline void Publish(Route r) {
    std::scoped_lock lock{mutex};routes[cursor++%routes.size()]=r;
}
inline bool Active(std::uint64_t frame,unsigned epoch) {
    std::scoped_lock lock{mutex};
    for(const auto& r:routes)if(r.vertices && Recent(r.frame,frame) && r.epoch==epoch && r.Moving())return true;
    return false;
}
inline std::optional<std::array<float,3>> Find(std::uint64_t hash,unsigned vertices,
                                             std::uint64_t frame,unsigned epoch,float phase=.5f) {
    std::scoped_lock lock{mutex};
    for(unsigned i=0;i<routes.size();++i) {
        const auto& r=routes[(cursor+routes.size()-1-i)%routes.size()];
        if(r.vertices==vertices && r.hash==hash && Recent(r.frame,frame) && r.epoch==epoch) {
            // A newer stationary submission cancels an identical older moving
            // draw, preventing a final-frame wobble when the slide settles.
            if(!r.Moving())return {};
            return r.At(phase);
        }
    }
    return {};
}
// Tag only models actually drawn by the song-list hook. Hash the exact native
// position/UV/color upload, rather than matching any shared texture or shader.
template<class M> void Capture(M& m,std::uint32_t model,std::uint32_t view,float shift,float shift_y=0,
                                     Interval interval={},const CursorPath& path={},bool title=false) {
    if(!Interpolation::Replaying())return;
    ++capture_attempts;
    const auto mesh=m.Read(model+4).value_or(0),desc=m.Read(mesh).value_or(0);
    const unsigned capacity=m.Read(desc+12).value_or(0);
    const unsigned indices=m.Read(model+0x174).value_or(0);
    capture_desc=desc;capture_vertices=capacity;capture_indices=indices;
    unsigned vertices=capacity;
    std::array<std::uint32_t,3> buffers{};
    if(title) {
        // Native lettering submits the active prefix of a double-buffered glyph mesh.
        // Its descriptor's source arrays can be older than the selected GPU upload.
        const auto flags=m.Read(desc+0x1c).value_or(0),bank=m.Read(mesh+0x124).value_or(2);
        if(!(flags&0x80) || (flags&0x18) || bank>=2)return;
        vertices=TextboxInterpolation::MeshVertices(indices,!(flags&0x20));
        const auto allocated=capacity?capacity:4;
        if(!vertices || allocated%4 || allocated>TextboxInterpolation::MaxVertices || vertices>allocated ||
           vertices>Ocarina::DrawQuadCount*4)return;
        const auto live=m.Read(mesh+0x1a0+4*bank).value_or(0);
        if(!TitleScreen::Pointer(live,allocated*36))return;
        buffers={live,live+allocated*12,live+allocated*20};
    } else {
        if(vertices%4 || vertices>Ocarina::DrawQuadCount*4 ||
           !ValidDrawCount(indices) || indices!=vertices/4*6)return;
        for(unsigned slot=0;slot<3;++slot)buffers[slot]=m.Read(desc+slot*4).value_or(0);
    }
    auto hash=HUD::HashStart;
    for(unsigned slot=0;slot<3;++slot) {
        const unsigned stride=slot==0?12:slot==1?8:16;
        const auto base=buffers[slot];
        if(!TitleScreen::Pointer(base,vertices*stride))return;
        for(unsigned i=0;i<vertices*stride;i+=4) {
            const auto word=m.Read(base+i);if(!word)return;
            hash=HUD::HashWord(hash,*word);
        }
    }
    // The unmodified view's X basis converts a logical-screen slide into the
    // model-view translation, including the rotated 3DS screen orientation.
    std::array<float,12> matrix{};if(!TitleScreen::ReadFloats(m,view,matrix))return;
    Route r{hash,Interpolation::guest_frames.load(),vertices,Interpolation::generation.load(),{}};
    for(unsigned row=0;row<3;++row)r.translation[row]=matrix[row*4]*shift+matrix[row*4+1]*shift_y;
    r.interval=interval;r.stone_count=path.count;
    for(unsigned i=0;i<path.count;++i) {
        const auto& step=path.steps[i];auto& stone=r.stones[i];
        stone.phase=step.phase;
        for(unsigned row=0;row<3;++row)stone.delta[row]=matrix[row*4]*step.x+matrix[row*4+1]*step.y;
    }
    Publish(r);++capture_published;
}
}
