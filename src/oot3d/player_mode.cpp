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

bool OOT3D::HandleSettingsFunctionKey(unsigned function_key, bool repeat) {
    if (!available.load() || (function_key != 1 && function_key != 2)) return false;
    if (repeat || (IsPlayerBuild() && function_key == 2)) return true;
    Toggle(IsPlayerBuild() || function_key == 2);
    return true;
}
