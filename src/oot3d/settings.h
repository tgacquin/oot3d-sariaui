// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>

namespace OOT3D {
// The presentation thread queues frontend changes for Qt to apply and save.
inline std::atomic<bool> configure_controls{false},quit_requested{false};
inline std::atomic<bool> custom_textures{false},fullscreen_requested{false};
inline std::atomic<int> requested_custom_textures{-1};
inline std::atomic<bool> configure_textures_shortcut{false},configure_fullscreen_shortcut{false};
inline std::atomic<bool> open_texture_folder{false},texture_folder_failed{false};
enum class DefaultsDialog { None, ImportHUD, ExportHUD, FullscreenShortcut, TexturesShortcut };
inline std::atomic<DefaultsDialog> defaults_dialog{DefaultsDialog::None};
inline std::atomic<unsigned> defaults_revision{0};
enum class HUDPresetAction { None, Import, Export };
inline std::atomic<HUDPresetAction> hud_preset_request{HUDPresetAction::None};
inline std::atomic<int> resolution{1};
inline std::atomic<int> requested_resolution{-1};
inline std::atomic<int> settings_size{0}; // -5..10, each step changes scale by 10%.
inline std::atomic<bool> settings_size_dirty{false};
namespace StartupHint {
inline std::atomic<bool> enabled{true},dirty{false},requested{false};
}
namespace GameLanguage {
// The emulation thread saves these through the existing CFG service.
// CFG system-language IDs, not OoT message-table slots (1, 5, 7).
inline constexpr std::array<int,3> Languages{1,2,5};
inline std::atomic<int> game_language{1},set_game_language{-1};
inline std::atomic<int> game_language_status{0}; // Idle, pending, saved, failed.
inline bool SupportedLanguage(int value) {
    for(const auto language:Languages)if(value==language)return true;
    return false;
}
} // namespace GameLanguage
} // namespace OOT3D
