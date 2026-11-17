// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace OOT3D::SaveWaitDiagnostics {
// Observe only. Never cancel, retry, reap, or mark a native save job complete.
// Capture entry and four later snapshots, then stop until the menu leaves Saving.
struct Monitor {
    using Time = std::int64_t;
    static constexpr std::array<Time, 5> Delays{0, 2000000, 5000000, 15000000, 30000000};
    Time started = -1, last = -1;
    unsigned next = 0;
    struct Sample { Time elapsed; bool exited; };
    std::optional<Sample> Poll(bool saving, Time now) {
        if (last >= 0 && now < last) *this = {};
        last = now;
        if (!saving) {
            if (started < 0) return {};
            const auto elapsed = now - started;
            started = -1;
            next = 0;
            return Sample{elapsed, true};
        }
        if (started < 0) started = now;
        const auto elapsed = now - started;
        if (next == Delays.size() || elapsed < Delays[next]) return {};
        // Coalesce missed samples after a long frame instead of logging a burst.
        do { ++next; } while (next < Delays.size() && elapsed >= Delays[next]);
        return Sample{elapsed, false};
    }
};
}
