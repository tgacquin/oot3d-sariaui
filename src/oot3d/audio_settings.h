// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>

namespace OOT3D::AudioSettings {
inline std::atomic<bool> mute_music{false},dirty{false};
// The render thread requests volume changes; Qt applies/saves the setting.
inline std::atomic<int> volume{100},requested_volume{-1};
}
