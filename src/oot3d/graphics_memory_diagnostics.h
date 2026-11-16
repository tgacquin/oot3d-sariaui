// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <mutex>
#include "oot3d/graphics_memory.h"

namespace OOT3D::GraphicsMemory {
// The editor reads a host-side copy, never guest memory or the allocator list.
// Publish only at the existing sample cadence, independently of log throttling.
class LiveDiagnostics {
public:
    using Clock=std::chrono::steady_clock;
    struct Snapshot {
        Monitor::Sample sample{};
        Monitor::Time guest_time_us=0;
        Clock::time_point captured_at{};
        std::array<float,120> occupied_mib{},largest_kib{};
        unsigned count=0,next=0;
        bool observed=false;
    };
    void Publish(const Monitor::Sample& sample,Monitor::Time now) {
        std::scoped_lock lock{mutex};
        if(snapshot.observed && (now<snapshot.guest_time_us ||
           (sample.free.valid && snapshot.sample.free.valid &&
            (sample.free.capacity!=snapshot.sample.free.capacity ||
             sample.free.start!=snapshot.sample.free.start))))snapshot={};
        snapshot.sample=sample;snapshot.guest_time_us=now;
        snapshot.captured_at=Clock::now();snapshot.observed=true;
        if(sample.free.valid) {
            snapshot.occupied_mib[snapshot.next]=float(sample.occupied)/MiB;
            snapshot.largest_kib[snapshot.next]=float(sample.free.largest)/1024;
            snapshot.next=(snapshot.next+1)%snapshot.occupied_mib.size();
            snapshot.count=std::min<unsigned>(snapshot.count+1,snapshot.occupied_mib.size());
        } else {
            // Do not connect graphs across an unreadable/recreated heap.
            snapshot.count=snapshot.next=0;
        }
    }
    Snapshot Read() const {
        std::scoped_lock lock{mutex};return snapshot;
    }
    void Reset() {
        std::scoped_lock lock{mutex};snapshot={};
    }
private:
    mutable std::mutex mutex;
    Snapshot snapshot;
};
inline LiveDiagnostics live_diagnostics;
}
