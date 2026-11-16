// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <mutex>

namespace OOT3D::TitleScreen {
struct Pose { float x = 0, y = 0, scale = 1; };
struct Layout {
    std::array<Pose,4> poses{{{}, {}, {}, {0,28,1}}};
    std::array<float,3> text_color{1,1,1};
    float letter_spacing=2;
    constexpr auto size() const {return poses.size();}
    constexpr Pose& operator[](unsigned i) {return poses[i];}
    constexpr const Pose& operator[](unsigned i) const {return poses[i];}
};
inline constexpr Layout DefaultLayout{};
inline std::mutex layout_mutex;
inline Layout layout = DefaultLayout;
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
} // namespace OOT3D::TitleScreen
