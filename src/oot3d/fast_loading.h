// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/qol.h"

namespace OOT3D::FastLoading {
constexpr std::uint64_t ProgramId=0x0004000000033500ULL;
constexpr std::uint64_t ReadDelayCapNs=1000000; // Keep a small guest scheduling interval.
inline std::uint64_t ReadDelay(std::uint64_t program_id,std::uint64_t native_delay) {
    if(program_id!=ProgramId || !QOL::fast_loading.load(std::memory_order_relaxed))
        return native_delay;
    return native_delay>ReadDelayCapNs?ReadDelayCapNs:native_delay;
}
}
