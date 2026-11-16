// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"

namespace OOT3D::Textboxes {
struct IndicatorAnimation {
    bool visible=true,spring=true;
    float small=.72f,big=1.18f,speed=1.f;
};
struct Layout {
    float x = 0, y = 0, width = 1;
    HUD::Pose indicator{};
    float scale = 1;
    float sign_y = 0;
    float opening_speed = 1;
    bool mm_close = false;
    IndicatorAnimation indicator_animation{};
    bool smooth_reveal=true;
};
inline float OpeningSpeed(float value) {
    return std::isfinite(value)?std::clamp(value,.25f,3.f):1.f;
}
inline std::mutex layout_mutex;
inline Layout layout{};
inline std::atomic<bool> dirty{false};
inline Layout GetLayout() {
    std::scoped_lock lock{layout_mutex};
    return layout;
}
inline void SetLayout(const Layout& value, bool save = true) {
    std::scoped_lock lock{layout_mutex};
    layout = value;
    if (save) dirty = true;
}
} // namespace OOT3D::Textboxes
