// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_state.h"
#include <chrono>
#include <sstream>

namespace OOT3D::Interpolation::FirstPersonDiagnostics {
inline std::atomic<std::int64_t> until{0};
inline std::atomic<unsigned> capture{0};
inline std::int64_t Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void Arm() {++capture;until=Now()+20000;}
inline bool Active() {const auto end=until.load();return end && Now()<end;}
// Each call site owns its quota; no locks, allocations or logging when idle.
struct Gate {
    unsigned session=0,total=0,in_frame=0;
    std::uint64_t frame=~std::uint64_t{};
    bool Take(std::uint64_t next,unsigned per_frame=1) {
        if(!Active())return false;
        const auto id=capture.load();
        if(session!=id){session=id;total=0;frame=~std::uint64_t{};}
        if(frame!=next){frame=next;in_frame=0;}
        if(total>=2400 || in_frame>=per_frame)return false;
        ++total;++in_frame;return true;
    }
};
inline std::string MatrixText(const Matrix& matrix) {
    std::ostringstream out;out.precision(7);
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
        if(r || c)out<<',';
        out<<matrix[r][c];
    }
    return out.str();
}
} // namespace OOT3D::Interpolation::FirstPersonDiagnostics
