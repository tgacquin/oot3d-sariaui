// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace OOT3D::StoneFocus {
struct Settings {bool enabled;float seconds;};
inline float Scale(float eased) {return 1.f+.05f*std::clamp(eased,0.f,1.f);}

template<std::size_t N>
std::array<float,N> Advance(std::array<float,N>& amount,unsigned selected,Settings settings,float dt) {
    const float duration=std::isfinite(settings.seconds)?std::clamp(settings.seconds,0.f,.3f):.1f;
    const float step=duration>0?dt/duration:1.f;
    std::array<float,N> eased{};
    for(unsigned i=0;i<N;++i) {
        const float t=settings.enabled?std::clamp(amount[i]+(selected==i?step:-step),0.f,1.f):0.f;
        amount[i]=t;
        eased[i]=t*t*(3-2*t);
    }
    return eased;
}
}
