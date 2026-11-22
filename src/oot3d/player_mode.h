// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string_view>

namespace OOT3D {
// A build property, never a saved preference or an editor toggle.
bool IsPlayerBuild();
const char* PlayerDefaultsDirectory();
// Returns whether a settings key was consumed. F3 authors defaults in developer builds.
bool HandleSettingsFunctionKey(unsigned function_key, bool repeat);

inline bool PlayerShortcutAllowed(std::string_view name) {
    return name == "Fullscreen" || name == "Exit Fullscreen" ||
           name == "Toggle Custom Textures" || name == "Exit Azahar" ||
           name == "First-Person View" || name == "Toggle Minimap" ||
           name == "Audio Mute/Unmute" || name == "Audio Volume Down" ||
           name == "Audio Volume Up";
}
} // namespace OOT3D
