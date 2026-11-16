// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace OOT3D::RemoteNames {
struct Layout {
    bool enabled=true,shadow=true;
    float x=0,y=0,scale=1.5f,opacity=1;
    float shadow_x=2,shadow_y=2,shadow_strength=.85f,shadow_softness=.75f;
    bool backdrop=true;
    float backdrop_x=0,backdrop_y=1,backdrop_width=1.3f,backdrop_height=1.5f;
    float backdrop_opacity=.3f,backdrop_softness=.8f;
};
inline std::mutex layout_mutex;
inline Layout layout;
inline std::atomic<bool> dirty{false};
inline Layout GetLayout() {std::scoped_lock lock(layout_mutex);return layout;}
inline void SetLayout(Layout value,bool save=true) {
    const Layout defaults;
    const auto bounded=[](float v,float fallback,float lo,float hi) {
        return std::isfinite(v)?std::clamp(v,lo,hi):fallback;
    };
    value.x=bounded(value.x,defaults.x,-300,300);value.y=bounded(value.y,defaults.y,-200,200);
    value.scale=bounded(value.scale,defaults.scale,.5f,3);
    value.opacity=bounded(value.opacity,defaults.opacity,0,1);
    value.shadow_x=bounded(value.shadow_x,defaults.shadow_x,-12,12);
    value.shadow_y=bounded(value.shadow_y,defaults.shadow_y,-12,12);
    value.shadow_strength=bounded(value.shadow_strength,defaults.shadow_strength,0,1);
    value.shadow_softness=bounded(value.shadow_softness,defaults.shadow_softness,0,6);
    value.backdrop_x=bounded(value.backdrop_x,defaults.backdrop_x,-20,20);
    value.backdrop_y=bounded(value.backdrop_y,defaults.backdrop_y,-20,20);
    value.backdrop_width=bounded(value.backdrop_width,defaults.backdrop_width,.25f,4);
    value.backdrop_height=bounded(value.backdrop_height,defaults.backdrop_height,.25f,4);
    value.backdrop_opacity=bounded(value.backdrop_opacity,defaults.backdrop_opacity,0,1);
    value.backdrop_softness=bounded(value.backdrop_softness,defaults.backdrop_softness,0,1);
    std::scoped_lock lock(layout_mutex);layout=value;if(save)dirty=true;
}
}
