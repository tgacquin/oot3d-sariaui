// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <vector>
namespace OOT3D::Interpolation::Timing {
inline std::atomic<std::int64_t> draw_us{},pool_us{},acquire_us{},present_us{},present_total_us{},queue_lock_us{},submit_us{};
inline std::atomic<unsigned> submissions{};
// Desired macOS display-link cadence and measured callbacks (not scanout).
inline std::atomic<int> display_request_fps{};
inline std::atomic<float> display_callback_fps{};
inline std::atomic<float> displayed_fps{},display_gap_min_ms{},display_gap_max_ms{};
inline std::mutex displayed_mutex;
inline double last_displayed{};
inline std::vector<double> displayed_times;
inline void Displayed(double at) {
    if(!std::isfinite(at) || at<=0)return;
    std::scoped_lock lock(displayed_mutex);
    // Completion handlers may arrive out of order. Count every valid report;
    // rejecting timestamps older than the last callback undercounts frames.
    if(displayed_times.size()<4096)displayed_times.push_back(at);
}
struct DisplayReport {unsigned count{};double minimum{},maximum{};};
inline DisplayReport TakeDisplayed() {
    std::scoped_lock lock(displayed_mutex);
    std::sort(displayed_times.begin(),displayed_times.end());
    DisplayReport result{static_cast<unsigned>(displayed_times.size()),1e9,0};
    for(double at:displayed_times) {
        if(at<last_displayed)continue;
        if(last_displayed && at-last_displayed<1) {
            result.minimum=std::min(result.minimum,at-last_displayed);
            result.maximum=std::max(result.maximum,at-last_displayed);
        }
        last_displayed=at;
    }
    if(result.minimum==1e9)result.minimum=0;
    displayed_times.clear();return result;
}
inline std::atomic<float> submitted_fps{},draw_ms{},pool_ms{},acquire_ms{},present_ms{},present_total_ms{},queue_lock_ms{},submit_ms{};
inline std::int64_t Now() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
class Scope {
    std::atomic<std::int64_t>& counter;
    std::int64_t start=Now();
public:
    explicit Scope(std::atomic<std::int64_t>& value):counter(value){}
    ~Scope(){counter.fetch_add(Now()-start,std::memory_order_relaxed);}
};
class Reporter {
    std::int64_t start{};
    unsigned batches{};
public:
    bool Tick() {
        const auto now=Now();
        if(!start)start=now;
        ++batches;
        if(now-start<1000000)return false;
        submitted_fps=float(submissions.exchange(0))*1000000.f/(now-start);
        const auto shown=TakeDisplayed();
        displayed_fps=float(shown.count)*1000000.f/(now-start);
        display_gap_min_ms=float(shown.minimum*1000);
        display_gap_max_ms=float(shown.maximum*1000);
        draw_ms=float(draw_us.exchange(0))/(1000.f*batches);
        pool_ms=float(pool_us.exchange(0))/(1000.f*batches);
        acquire_ms=float(acquire_us.exchange(0))/(1000.f*batches);
        present_ms=float(present_us.exchange(0))/(1000.f*batches);
        present_total_ms=float(present_total_us.exchange(0))/(1000.f*batches);
        queue_lock_ms=float(queue_lock_us.exchange(0))/(1000.f*batches);
        submit_ms=float(submit_us.exchange(0))/(1000.f*batches);
        start=now;batches=0;return true;
    }
};
}
