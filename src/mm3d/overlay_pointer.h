// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include "imgui.h"

namespace MM3D {
// Accessed under the overlay input mutex. ImGui's render thread publishes
// regions/popups; the frontend routes clicks without touching ImGui itself.
struct OverlayPointer {
    struct Region {
        ImVec2 position{},size{};
        bool Contains(float x,float y) const {
            return x>=position.x && y>=position.y &&
                x<position.x+size.x && y<position.y+size.y;
        }
    } panel,editor;
    bool popup_open{};
    std::array<bool,2> held{},owned{};
    bool Consume(float x,float y,int button,bool down,bool visible) {
        // Popups can extend beyond both parent windows. Forward their input
        // (including outside clicks for dismissal) to ImGui's own hit testing.
        const bool inside=visible && (popup_open || panel.Contains(x,y) || editor.Contains(x,y));
        bool consumed=inside;
        if(button>=0 && button<int(held.size())) {
            if(down) {held[button]=true;owned[button]=inside;}
            consumed=held[button] && owned[button];
            if(!down)held[button]=false;
        } else {
            for(unsigned i=0;i<held.size();++i)if(held[i])consumed=owned[i];
        }
        return consumed;
    }
};
}
