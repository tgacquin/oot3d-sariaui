// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/player_mode.h"
#include "oot3d/input.h"

bool OOT3D::IsPlayerBuild() {
#ifdef ENABLE_OOT3D_PLAYER_MODE
    return true;
#else
    return false;
#endif
}

const char* OOT3D::PlayerDefaultsDirectory() {
#ifdef OOT3D_DEFAULTS_DIR
    return OOT3D_DEFAULTS_DIR;
#else
    return nullptr;
#endif
}

bool OOT3D::HandleSettingsFunctionKey(unsigned function_key, bool repeat) {
    if (!available.load() || (function_key < 1 || function_key > (IsPlayerBuild() ? 2u : 3u))) return false;
    if (repeat || (IsPlayerBuild() && function_key == 2)) return true;
    const auto view = function_key == 3 ? SettingsView::Defaults :
        IsPlayerBuild() || function_key == 2 ? SettingsView::Player : SettingsView::Editor;
    Toggle(view);
    return true;
}
