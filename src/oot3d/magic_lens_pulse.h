// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>

namespace OOT3D::MagicMeter {
inline std::atomic<std::int64_t> lens_elapsed_us{-1};

struct LensPulse {
    std::int64_t start = -1, last = -1;
    unsigned generation{};

    std::int64_t Sample(bool active, std::int64_t now, unsigned epoch) {
        if (!active) start = -1;
        else if (start < 0 || now < last || epoch != generation) start = now;
        last = now;
        generation = epoch;
        return start < 0 ? -1 : now - start;
    }
};

// OoT's Interface_UpdateMagicBar targets gray, gray, white, white. At 20 Hz
// YREG(40..43) give two ticks to change color and one tick to hold each end.
inline float LensBrightness(std::int64_t elapsed) {
    constexpr float gray = 150.f / 255.f;
    const auto phase = std::max<std::int64_t>(0, elapsed) % 300000;
    if (phase < 100000) return 1.f - (1.f-gray) * (float(phase)/100000.f);
    if (phase < 150000) return gray;
    if (phase < 250000) return gray + (1.f-gray) * (float(phase-150000)/100000.f);
    return 1.f;
}
} // namespace OOT3D::MagicMeter
