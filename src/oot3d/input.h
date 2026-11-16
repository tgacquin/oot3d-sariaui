// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

namespace OOT3D {
inline std::atomic<bool> available{false};
inline std::atomic<bool> visible{false};
inline std::atomic<bool> player_view{false};
struct PointerEvent { float x, y; int button; bool down; };
inline std::mutex input_mutex;
inline std::vector<PointerEvent> pointer_events;
struct KeyEvent { int key; bool down,ctrl,shift,alt,super; std::string text; };
inline std::vector<KeyEvent> key_events;
inline std::atomic<bool> text_input{false};
inline void Key(int key,bool down,bool ctrl,bool shift,bool alt,bool super,std::string text={}) {
    std::scoped_lock lock{input_mutex};
    key_events.push_back({key,down,ctrl,shift,alt,super,std::move(text)});
}
inline float wheel_x = 0, wheel_y = 0;
inline bool Pointer(float x, float y, int button = -1, bool down = false) {
    if (!available.load() || !visible.load()) return false;
    std::scoped_lock lock{input_mutex};
    pointer_events.push_back({x, y, button, down});
    return true;
}
inline void Toggle(bool player=false) {
    if (!available.load()) return;
    const bool close=visible.load() && player_view.load()==player;
    player_view.store(player);
    text_input.store(false);
    visible.store(!close);
}
inline bool Scroll(float x, float y, float horizontal, float vertical) {
    if (!available.load() || !visible.load()) return false;
    std::scoped_lock lock{input_mutex};
    pointer_events.push_back({x, y, -1, false});
    wheel_x += horizontal;
    wheel_y += vertical;
    return true;
}
} // namespace OOT3D
