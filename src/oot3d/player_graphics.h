// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_state.h"

namespace OOT3D::PlayerGraphics {
inline std::atomic<bool> vsync{true}, cache{true}, hardware{true}, async_shaders{false};
inline std::atomic<int> set_vsync{-1}, set_cache{-1}, set_hardware{-1}, set_async{-1};
inline std::atomic<bool> disable_fog{false}, fog_dirty{false};
enum class Mode { Original, CPU, GPU };
inline Mode CurrentMode() {
    if (!Interpolation::Replaying()) return Mode::Original;
    const int pending = set_hardware.load();
    const bool accelerated = pending < 0 ? hardware.load() : pending != 0;
    return Interpolation::cpu_vertices.load() || !accelerated ? Mode::CPU : Mode::GPU;
}
inline void SelectMode(Mode mode) {
    const bool enabled = mode != Mode::Original;
    if (enabled) {
        Interpolation::cpu_vertices = mode == Mode::CPU;
        set_hardware = mode == Mode::GPU;
        set_vsync = 1;
    }
    Interpolation::enabled = enabled;
    Interpolation::present_60 = enabled;
    // Keep the chosen rate when changing backend or returning from Original.
    // Fresh profiles already default to 60 FPS with display matching off.
    ++Interpolation::generation;
    Interpolation::settings_dirty = true;
}

// The frontend owns emulator settings and persistence. The rendering thread
// only queues requests; none of these options restart the game.
template<class Values, class ApplyRuntime, class Persist>
void ApplyPending(Values& values, ApplyRuntime apply_runtime, Persist persist) {
    bool changed = false, runtime = false, hardware_changed = false;
    const auto apply = [&](auto& request, auto& setting) {
        const int value = request.exchange(-1);
        if (value < 0) return false;
        setting.SetGlobal(true); setting.SetValue(value != 0);
        changed = true; return true;
    };
    runtime |= apply(set_vsync, values.use_vsync);
    runtime |= apply(set_cache, values.use_disk_shader_cache);
    hardware_changed = apply(set_hardware, values.use_hw_shader);
    apply(set_async, values.async_shader_compilation);
    if (hardware_changed) ++Interpolation::generation;
    if (runtime) apply_runtime();
    if (changed) persist();
    vsync = values.use_vsync.GetValue();
    cache = values.use_disk_shader_cache.GetValue();
    hardware = values.use_hw_shader.GetValue();
    async_shaders = values.async_shader_compilation.GetValue();
}
}
