// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <span>
#include <optional>
namespace OOT3D::Interpolation {
inline std::atomic<int> target_fps{60};
inline std::atomic<bool> match_refresh_rate{false};
inline std::atomic<float> display_refresh_rate{60};
enum class MenuPage { None, Items, Gear, Map, FileSelect, NameEntry, Save };
inline std::atomic<MenuPage> menu_page{MenuPage::None};
inline constexpr unsigned MaxRateSamples=4;
inline int TargetFPS() {
    const float rate=match_refresh_rate.load()?display_refresh_rate.load():float(target_fps.load());
    return std::isfinite(rate)?std::clamp(int(std::lround(rate)),30,120):60;
}
constexpr int SourceFPS(MenuPage page) { return page==MenuPage::None ? 30 : 60; }
inline int PresentationFPS(MenuPage page, bool vsync) {
    int fps=TargetFPS();
    const float refresh=display_refresh_rate.load();
    if(vsync && std::isfinite(refresh))
        fps=std::min(fps,std::clamp(int(std::lround(refresh)),30,120));
    return std::max(fps,SourceFPS(page));
}
// Scoped to the renderer thread. Native hooks and existing 60 Hz tests retain
// their half-frame default; replay histories sample the requested phase.
inline thread_local float sample_fraction=.5f;
template<class T> inline auto SampleMix(const T& before,const T& after) {
    return before+(after-before)*sample_fraction;
}
struct RatePlan {
    std::array<float,MaxRateSamples> phases{};
    unsigned count{},sample_slots{};
    int fps=60, source_fps=30;
};
class RateClock {
    std::uint64_t last=~std::uint64_t{};
    int rate{};
    unsigned remainder{};
    RatePlan plan;
public:
    void Reset(){last=~std::uint64_t{};rate=0;remainder=0;plan={};}
    const RatePlan& Next(std::uint64_t frame,int requested,int source_fps=30) {
        requested=std::clamp(requested,source_fps,120);
        if(last==frame && rate==requested && plan.source_fps==source_fps)return plan;
        if(plan.source_fps!=source_fps || rate!=requested || frame!=last+1)remainder=0;
        plan={};plan.fps=requested;plan.source_fps=source_fps;
        plan.sample_slots=(requested+source_fps-1)/source_fps;
        // Exact integer phase accumulator avoids drift at 59, 75, 119 Hz.
        for(unsigned tick=source_fps-remainder;tick<=unsigned(requested);tick+=source_fps)
            plan.phases[plan.count++]=float(tick)/requested;
        remainder=(remainder+requested)%source_fps;
        for(unsigned i=plan.count;i<plan.sample_slots;++i)plan.phases[i]=1;
        last=frame;rate=requested;return plan;
    }
};
}

namespace OOT3D::Interpolation {
inline std::atomic<unsigned> skipped_present_samples{};
inline bool CanBorrowIntermediate(unsigned ready_frames) {return ready_frames>=2;}
inline std::optional<unsigned> ReadyFrameIndex(std::span<const bool> ready,bool reserve_native) {
    std::optional<unsigned> first;unsigned count=0;
    for(unsigned i=0;i<ready.size();++i)if(ready[i]) {
        if(!first)first=i;
        ++count;
    }
    return count>=(reserve_native?2u:1u)?first:std::nullopt;
}
inline bool NeedsSoftwarePacing(bool vsync,int fps,float display_hz) {
    // FIFO already clocks a display-matched stream. A second host timer can
    // miss the next vblank and accidentally divide the presentation rate.
    return !vsync || !std::isfinite(display_hz) || fps<int(std::lround(display_hz));
}
}
