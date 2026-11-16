// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <mutex>
namespace OOT3D::LoadingIcon {
struct Anchor {float x=380,y=220,half=8;};
inline std::mutex anchor_mutex;
inline Anchor top_anchor;
inline void ObserveTop(Anchor value) {
    std::lock_guard lock{anchor_mutex};top_anchor=value;
}
inline Anchor TopAnchor() {std::lock_guard lock{anchor_mutex};return top_anchor;}
}
