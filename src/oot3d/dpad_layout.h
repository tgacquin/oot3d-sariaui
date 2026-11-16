// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/user_hud.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <cstdint>

namespace Core { class System; }
namespace OOT3D::DPad {
// Keep the legacy "look" layout key for the top slot, now occupied by Navi.
// Look remains a separate action, requested by the first-person hotkey.
enum Element { Look, Ocarina, Iron, Hover, ElementCount, Navi };
constexpr std::array Names{"look", "ocarina", "iron_boots", "hover_boots"};
constexpr std::array Labels{"Up: Navi", "Right: ocarina",
                            "Left: Iron Boots", "Down: Hover Boots"};
struct Pose { float x=0, y=0, scale_x=1, scale_y=1; };
struct Layout {
    bool enabled=true;
    float navi_pulse_speed=1.f;
    Pose group{};
    std::array<Pose,ElementCount> icons{};
};
inline std::mutex layout_mutex;
inline Layout layout{};
inline std::atomic<bool> dirty{false};
inline Layout GetLayout() {std::scoped_lock lock{layout_mutex};return layout;}
inline Layout GetEffectiveLayout() {
    auto result=GetLayout();const auto a=UserHUD::GetEffectiveOffsets()[UserHUD::DPad];
    auto& p=result.group;
    if(a.x!=0)p.x=std::clamp(p.x+a.x,-400.f,400.f);
    if(a.y!=0)p.y=std::clamp(p.y+a.y,-240.f,240.f);
    if(a.scale!=0) {
        p.scale_x=std::clamp(p.scale_x*UserHUD::Factor(a),.1f,4.f);
        p.scale_y=std::clamp(p.scale_y*UserHUD::Factor(a),.1f,4.f);
    }
    return result;
}
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{layout_mutex};layout=value;
    layout.navi_pulse_speed=std::isfinite(value.navi_pulse_speed)?std::clamp(value.navi_pulse_speed,.25f,4.f):1.f;
    if(save)dirty=true;
}
constexpr unsigned TextureWidth=256,TextureHeight=128;
using Pixels=std::array<std::uint32_t,TextureWidth*TextureHeight>;
// Both ocarinas come from the player's cartridge. Never bundle the game atlas.
constexpr unsigned TextureCount=2;
inline std::array<std::shared_ptr<const Pixels>,TextureCount> textures{};
inline auto GetTexture(unsigned variant) {return std::atomic_load(&textures[variant]);}
void Load(Core::System& system);
}
