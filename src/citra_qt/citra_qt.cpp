#include "citra_qt/configuration/configure_input.h"
#include <QDialogButtonBox>
#ifdef ENABLE_MM3D_GAME_FOLDER
#include "mm3d/game_folder.h"
#include "mm3d/game_setup.h"
#include "input_common/controller_autoconfig.h"
#endif
#ifdef ENABLE_MM3D_PRESENTATION
#include "mm3d/player_settings.h"
#include "mm3d/player_settings_bridge.h"
#include "citra_qt/configuration/configure_cheats.h"
#include <QScopedValueRollback>
#include <QDesktopServices>
#endif
// Copyright 2024-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <clocale>
#ifdef ENABLE_OOT3D_PRESENTATION
#include "oot3d/game_folder.h"
#include "oot3d/player_mode.h"
#include "input_common/controller_autoconfig.h"
#include "oot3d/release_defaults.h"
#include "oot3d/interpolation_state.h"
#include "oot3d/interpolation_timing.h"
#include "oot3d/settings.h"
#include "oot3d/player_graphics.h"
#include "oot3d/boss_challenge_layout.h"
#include "oot3d/hd_font.h"
#include "oot3d/actor_draw_distance.h"
#include "oot3d/audio_settings.h"
#include "oot3d/qol.h"
#include "oot3d/difficulty.h"
#include "oot3d/rumble.h"
#include "oot3d/randomizer/service.h"
#include "oot3d/randomizer/presets.h"
#include "oot3d/randomizer/activity.h"
#include "audio_core/hle/accuracy.h"
#include "oot3d/minimap_control.h"
#include "oot3d/control_swap.h"
#include "oot3d/remote_names_layout.h"
#include "oot3d/input.h"
#include "oot3d/dpad_input.h"
#include "oot3d/pause_prompt_layout.h"
#include "oot3d/file_select.h"
#include "oot3d/ocarina_layout.h"
#include "oot3d/hud_buttons.h"
#include "oot3d/map_frame_asset.h"
#include "oot3d/widescreen.h"
#include "oot3d/camera.h"
#include "oot3d/orbit_camera.h"
#include "oot3d/dialogue_camera.h"
#include "oot3d/pause_menu.h"
#include "oot3d/pause_color_settings.h"
#include "oot3d/title_layout.h"
#include "oot3d/hd_numbers_config.h"
#include "oot3d/hud_layout.h"
#include "oot3d/user_hud_config.h"
#include "oot3d/user_camera_config.h"
#include "oot3d/hud_defaults.h"
#include "oot3d/textbox_layout.h"
#include "oot3d/inspection_clock.h"
#include <QSettings>
#include <cmath>
#endif
#ifdef ENABLE_MM3D_PRESENTATION
#include "mm3d/pause_control.h"
#include "mm3d/diagnostics.h"
#include <imgui.h>
#endif
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QSysInfo>
#include <QtConcurrent/QtConcurrentMap>
#include <QtConcurrent/QtConcurrentRun>
#include <QtGui>
#include <QtWidgets>
#include <boost/algorithm/string/replace.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#ifdef __APPLE__
#include <unistd.h> // for chdir
#endif
#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <getopt.h>
#endif
#ifdef __unix__
#include <QVariant>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QtDBus>
#include "common/linux/gamemode.h"
#endif
#include "citra_meta/common_strings.h"
#include "citra_qt/aboutdialog.h"
#include "citra_qt/applets/mii_selector.h"
#include "citra_qt/applets/swkbd.h"
#include "citra_qt/bootmanager.h"
#include "citra_qt/camera/qt_multimedia_camera.h"
#include "citra_qt/camera/still_image_camera.h"
#include "citra_qt/citra_qt.h"
#include "citra_qt/compatibility_list.h"
#include "citra_qt/configuration/config.h"
#include "citra_qt/configuration/configure_dialog.h"
#include "citra_qt/configuration/configure_per_game.h"
#include "citra_qt/debugger/console.h"
#include "citra_qt/debugger/graphics/graphics.h"
#include "citra_qt/debugger/graphics/graphics_breakpoints.h"
#include "citra_qt/debugger/graphics/graphics_cmdlists.h"
#include "citra_qt/debugger/graphics/graphics_surface.h"
#include "citra_qt/debugger/graphics/graphics_tracing.h"
#include "citra_qt/debugger/graphics/graphics_vertex_shader.h"
#include "citra_qt/debugger/ipc/recorder.h"
#include "citra_qt/debugger/lle_service_modules.h"
#if MICROPROFILE_ENABLED
#include "citra_qt/debugger/profiler.h"
#endif
#include "citra_qt/debugger/registers.h"
#include "citra_qt/debugger/wait_tree.h"
#ifdef ENABLE_DISCORD_RPC
#include "citra_qt/discord.h"
#endif
#include "citra_qt/dumping/dumping_dialog.h"
#include "citra_qt/game_list.h"
#include "citra_qt/hotkeys.h"
#include "citra_qt/fullscreen_shortcut.h"
#include "citra_qt/loading_screen.h"
#include "citra_qt/movie/movie_play_dialog.h"
#include "citra_qt/movie/movie_record_dialog.h"
#include "citra_qt/multiplayer/state.h"
#include "citra_qt/qt_image_interface.h"
#include "citra_qt/qt_swizzle.h"
#include "citra_qt/uisettings.h"
#include "common/play_time_manager.h"
#ifdef ENABLE_QT_UPDATE_CHECKER
#include "citra_qt/update_checker.h"
#endif
#include "citra_qt/util/clickable_label.h"
#include "citra_qt/util/graphics_device_info.h"
#include "citra_qt/util/util.h"
#include "common/arch.h"
#include "common/common_paths.h"
#include "common/dynamic_library/dynamic_library.h"
#include "common/file_util.h"
#include "common/literals.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/memory_detect.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#if CITRA_ARCH(x86_64)
#include "common/x64/cpu_detect.h"
#endif
#include "common/settings.h"
#include "common/string_util.h"
#include "common/zstd_compression.h"
#include "core/arm/exception_handler.h"
#include "core/core.h"
#include "core/dumping/backend.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/frontend/applets/default_applets.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/savestate.h"
#include "core/system_titles.h"
#include "input_common/main.h"
#include "input_common/keyboard.h"
#include "ui_main.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"

#ifdef __APPLE__
#include "common/apple_authorization.h"
#include "common/apple_utils.h"
Q_IMPORT_PLUGIN(QDarwinCameraPermissionPlugin);
#endif

#ifdef ENABLE_DISCORD_RPC
#include "citra_qt/discord_impl.h"
#endif

#ifdef QT_STATICPLUGIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

#ifdef HAVE_SDL2
#include <SDL.h>
#endif

constexpr int default_mouse_timeout = 2500;

/**
 * "Callouts" are one-time instructional messages shown to the user. In the config settings, there
 * is a bitfield "callout_flags" options, used to track if a message has already been shown to the
 * user. This is 32-bits - if we have more than 32 callouts, we should retire and recycle old ones.
 */

const int GMainWindow::max_recent_files_item;

// There is a bug in the QT implementation on MSYS2 builds
// that cause corners to appear when the app is switched to
// fullscreen. The following code aims to fix that issue
// until it is addressed upstream. It works by manually
// disabling corners through the DWM API.
// TODO(PabloMK7): Remove once the upstream bug is solved.
#if defined(_WIN32) && !defined(_MSC_VER)
#define NEEDS_ROUND_CORNERS_FIX
#endif

#ifdef NEEDS_ROUND_CORNERS_FIX
#include <dwmapi.h>
class WindowCornerManager {
public:
    static WindowCornerManager& instance() {
        static WindowCornerManager inst;
        return inst;
    }

    void blockRoundedCorners(QWidget* widget, bool block) {
        HWND hwnd = reinterpret_cast<HWND>(widget->winId());
        DWORD pref;

        if (block) {
            pref = DWMWCP_DEFAULT;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref,
                                                sizeof(pref)))) {
                original_prefs[hwnd] = pref;
            } else {
                original_prefs[hwnd] = DWMWCP_DEFAULT;
            }

            pref = DWMWCP_DONOTROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        } else {
            auto it = original_prefs.find(hwnd);
            if (it == original_prefs.end())
                return;

            pref = it->second;

            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));

            original_prefs.erase(it);
        }
    }

private:
    WindowCornerManager() = default;
    ~WindowCornerManager() = default;

    std::unordered_map<HWND, DWORD> original_prefs;
};
#endif

static QString PrettyProductName() {
#ifdef _WIN32
    // After Windows 10 Version 2004, Microsoft decided to switch to a different notation: 20H2
    // With that notation change they changed the registry key used to denote the current version
    QSettings windows_registry(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
        QSettings::NativeFormat);
    const QString release_id = windows_registry.value(QStringLiteral("ReleaseId")).toString();
    if (release_id == QStringLiteral("2009")) {
        const u32 current_build = windows_registry.value(QStringLiteral("CurrentBuild")).toUInt();
        const QString display_version =
            windows_registry.value(QStringLiteral("DisplayVersion")).toString();
        const u32 ubr = windows_registry.value(QStringLiteral("UBR")).toUInt();
        const u32 version = current_build >= 22000 ? 11 : 10;
        return QStringLiteral("Windows %1 Version %2 (Build %3.%4)")
            .arg(QString::number(version), display_version, QString::number(current_build),
                 QString::number(ubr));
    }
#endif
    return QSysInfo::prettyProductName();
}

void GMainWindow::ShowCommandOutput(std::string title, std::string message) {
#ifdef _WIN32
    boost::replace_all(message, " ", "\u00a0"); // Non-breaking space
    boost::replace_all(message, "-", "\u2011"); // Non-breaking hyphen
    QMessageBox::information(this, QString::fromStdString(title), QString::fromStdString(message));
#else
    std::cout << message << std::endl;
#endif
}

bool IsPrereleaseBuild() {
    return ((strstr(Common::g_build_fullname, "alpha") != NULL) ||
            (strstr(Common::g_build_fullname, "beta") != NULL) ||
            (strstr(Common::g_build_fullname, "rc") != NULL) ||
            (strstr(Common::g_build_fullname, "test") != NULL));
}

#ifdef ENABLE_QT_UPDATE_CHECKER
static bool ShouldCheckForPrereleaseUpdates() {
    const bool update_channel = UISettings::values.update_check_channel.GetValue();
    const bool using_prerelease_channel =
        (update_channel == UISettings::UpdateCheckChannels::PRERELEASE);
    return (IsPrereleaseBuild() || using_prerelease_channel);
}

static int GetMajorVersion(const std::string& version) {
    size_t dot = version.find('.');
    try {
        return std::stoi(version.substr(0, dot));
    } catch (...) {
        return 0;
    }
}
#endif

#if defined(ENABLE_OOT3D_PRESENTATION) || defined(ENABLE_MM3D_GAME_FOLDER)
static void AutoConfigureController(Core::System& system, QtConfig& config) {
    if (QApplication::activeModalWidget() || Settings::values.use_artic_base_controller.GetValue()) return;
    std::array<std::string,Settings::NativeButton::NumButtons> buttons;
    std::array<std::string,Settings::NativeAnalog::NumAnalogs> analogs;
    for(unsigned i=0;i<buttons.size();++i)buttons[i]=InputCommon::GenerateKeyboardParam(QtConfig::default_buttons[i]);
    for(unsigned i=0;i<analogs.size();++i) {
        const auto& keys=QtConfig::default_analogs[i];
        analogs[i]=InputCommon::GenerateAnalogParamFromKeys(keys[0],keys[1],keys[2],keys[3],keys[4],.5f);
    }
    using namespace InputCommon::ControllerAutoConfig;
    const auto& current=Settings::values.current_input_profile;
    auto mapped=Select(current,InputCommon::GetConnectedControllers(),buttons,analogs,
        InputCommon::GetControllerButtonBinds,InputCommon::GetControllerAnalogBinds);
    if(mapped) {
        int index=Settings::values.current_input_profile_index;
        if(Managed(current,InputCommon::GetControllerButtonBinds,InputCommon::GetControllerAnalogBinds)&&
           index>=0&&index<int(Settings::values.input_profiles.size())) {
            Settings::values.input_profiles[index]=std::move(*mapped);
        } else {
            // Retain the keyboard profile and all unrelated profiles.
            if(index>=0&&index<int(Settings::values.input_profiles.size()))Settings::SaveProfile(index);
            index=int(Settings::values.input_profiles.size());
            Settings::values.input_profiles.push_back(std::move(*mapped));
        }
        Settings::LoadProfile(index);system.ApplySettings();config.Save();
        LOG_INFO(Frontend,"Automatically configured controller in profile {}",index);
    }
}
#endif

GMainWindow::GMainWindow(Core::System& system_)
    : ui{std::make_unique<Ui::MainWindow>()}, system{system_}, movie{system.Movie()},
      user_data_migrator{this}, config{std::make_unique<QtConfig>()}, emu_thread{nullptr} {
    Common::Log::Initialize();
    Common::Log::Start();

    Debugger::ToggleConsole();

#ifdef ENABLE_OOT3D_PRESENTATION
    UISettings::values.single_window_mode = true;
    UISettings::values.show_status_bar = false;
    if (UISettings::values.first_start) UISettings::values.fullscreen = true;
#endif
    QStringList args = QApplication::arguments();
#ifdef ENABLE_MM3D_GAME_FOLDER
    args = QStringList{args.first()};
#endif
    QString game_path;
#ifdef ENABLE_OOT3D_PRESENTATION
    game_path = OoT3D::GameFolder::Rom();
#endif
    std::optional<bool> fullscreen_override;
    for (int i = 1; i < args.size(); ++i) {
        // Preserves drag/drop functionality
        if (args.size() == 2 && !args[1].startsWith(QChar::fromLatin1('-'))) {
            game_path = args[1];
            break;
        }

        // Dump video
        if (args[i] == QStringLiteral("--dump-video") || args[i] == QStringLiteral("-d")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            if (!DynamicLibrary::FFmpeg::LoadFFmpeg()) {
                ShowFFmpegErrorMessage();
                continue;
            }
            video_dumping_path = args[++i];
            video_dumping_on_start = true;
            continue;
        }

        // Launch game in fullscreen mode
        if (args[i] == QStringLiteral("--fullscreen") || args[i] == QStringLiteral("-f")) {
            fullscreen_override = true;
            continue;
        }

        // Enable GDB stub
        if (args[i] == QStringLiteral("--gdbport") || args[i] == QStringLiteral("-g")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            gdbport_from_arg = strtoul(args[++i].toLatin1(), NULL, 0);
            continue;
        }

        if (args[i] == QStringLiteral("--help") || args[i] == QStringLiteral("-h")) {
            ShowCommandOutput("Help", fmt::format(Common::help_string, args[0].toStdString()));
            exit(0);
        }

        if (args[i] == QStringLiteral("--install") || args[i] == QStringLiteral("-i")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            Service::AM::InstallStatus result = Service::AM::InstallCIA(args[++i].toStdString());
            if (result != Service::AM::InstallStatus::Success) {
                std::string failure_reason;

                if (result == Service::AM::InstallStatus::ErrorFailedToOpenFile)
                    failure_reason = "Unable to open file.";

                if (result == Service::AM::InstallStatus::ErrorFileNotFound)
                    failure_reason = "File not found.";

                if (result == Service::AM::InstallStatus::ErrorAborted)
                    failure_reason = "Install was aborted.";

                if (result == Service::AM::InstallStatus::ErrorInvalid)
                    failure_reason = "CIA is invalid.";

                if (result == Service::AM::InstallStatus::ErrorEncrypted)
                    failure_reason = "CIA is encrypted.";

                std::string failure_string = "Failed to install CIA: " + failure_reason;
                ShowCommandOutput("Failure", failure_string);
                exit((int)result +
                     2); // 2 is added here to avoid stepping on the toes of
                         // exit codes 1 and 2 which have pre-established conventional meanings
            }
            ShowCommandOutput("Success", "Installed CIA successfully.");
            exit(0);
        }

        if (args[i] == QStringLiteral("--movie-play") || args[i] == QStringLiteral("-p")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_playback_path = args[++i];
            movie_playback_on_start = true;
            continue;
        }

        if (args[i] == QStringLiteral("--movie-record") || args[i] == QStringLiteral("-r")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_record_path = args[++i];
            movie_record_on_start = true;
            continue;
        }

        if (args[i] == QStringLiteral("--movie-record-author") || args[i] == QStringLiteral("-a")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_record_author = args[++i];
            continue;
        }

        if (args[i] == QStringLiteral("--multiplayer") || args[i] == QStringLiteral("-m")) {
            std::cout << "Warning: The --multiplayer option is not yet implemented for the Qt "
                         "frontend; Ignoring."
                      << std::endl;
            if (i < args.size() - 1 && !args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                i++;
            }
            continue;
        }

        if (args[i] == QStringLiteral("--version") || args[i] == QStringLiteral("-v")) {
            const std::string version_string = std::string("Azahar ") + Common::g_build_fullname;
            ShowCommandOutput("Version", version_string);
            exit(0);
        }

        // Launch game in windowed mode
        if (args[i] == QStringLiteral("--windowed") || args[i] == QStringLiteral("-w")) {
            fullscreen_override = false;
            continue;
        }

        // Launch game at path
        if (i == args.size() - 1 && !args[i].startsWith(QChar::fromLatin1('-'))) {
            game_path = args[i];
            continue;
        }
    }

#ifdef __unix__
    SetGamemodeEnabled(Settings::values.enable_gamemode.GetValue());
#endif

    // register types to use in slots and signals
    qRegisterMetaType<std::size_t>("std::size_t");
    qRegisterMetaType<Service::AM::InstallStatus>("Service::AM::InstallStatus");

    // Register CameraFactory
    qt_cameras = std::make_shared<Camera::QtMultimediaCameraHandlerFactory>();
    Camera::RegisterFactory("image", std::make_unique<Camera::StillImageCameraFactory>());
    Camera::RegisterFactory("qt", std::make_unique<Camera::QtMultimediaCameraFactory>(qt_cameras));

    system.RegisterInfoLEDColorChanged([this]() { emit InfoLEDColorChanged(); });

    LoadTranslation();

    if (Settings::values.pica_debugging) {
        Pica::g_debug_context = Pica::DebugContext::Construct();
    } else {
        Pica::g_debug_context.reset();
    }
    setAcceptDrops(true);
    ui->setupUi(this);
    statusBar()->hide();

    default_theme_paths = QIcon::themeSearchPaths();
    UpdateUITheme();

#ifdef ENABLE_DISCORD_RPC
    SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    discord_rpc->Update(false);
#endif

    play_time_manager = std::make_unique<PlayTime::PlayTimeManager>();

    Network::Init();

    movie.SetPlaybackCompletionCallback([this] {
        QMetaObject::invokeMethod(this, "OnMoviePlaybackCompleted", Qt::BlockingQueuedConnection);
    });

    InitializeWidgets();
    InitializeDebugWidgets();
    InitializeRecentFileMenuActions();
    InitializeSaveStateMenuActions();
    InitializeHotkeys();

    SetDefaultUIGeometry();
    RestoreUIState();

    ui->action_Dump_Video->setChecked(video_dumping_on_start);
    if (fullscreen_override) {
        ui->action_Fullscreen->setChecked(*fullscreen_override);
    }
    ui->action_Close_Movie->setEnabled(movie_playback_on_start || movie_record_on_start);

    ConnectAppEvents();
    ConnectMenuEvents();
    ConnectWidgetEvents();

    LOG_INFO(Frontend, "Azahar Version: {} | {}-{}", Common::g_build_fullname, Common::g_scm_branch,
             Common::g_scm_desc);
#if CITRA_ARCH(x86_64)
    const auto& caps = Common::GetCPUCaps();
    std::string cpu_string = caps.cpu_string;
    if (caps.avx || caps.avx2 || caps.avx512) {
        cpu_string += " | AVX";
        if (caps.avx512) {
            cpu_string += "512";
        } else if (caps.avx2) {
            cpu_string += '2';
        }
        if (caps.fma) {
            cpu_string += " | FMA";
        }
    }
    LOG_INFO(Frontend, "Host CPU: {}", cpu_string);
#endif
    LOG_INFO(Frontend, "Host OS: {}", PrettyProductName().toStdString());
    const auto& mem_info = Common::GetMemInfo();
    using namespace Common::Literals;
    LOG_INFO(Frontend, "Host RAM: {:.2f} GiB", mem_info.total_physical_memory / f64{1_GiB});
    LOG_INFO(Frontend, "Host Swap: {:.2f} GiB", mem_info.total_swap_memory / f64{1_GiB});
    UpdateWindowTitle();

    QIcon azahar_icon = QIcon(QString::fromStdString(":/icons/default/256x256/azahar.png"));
    render_window->setWindowIcon(azahar_icon);
    secondary_window->setWindowIcon(azahar_icon);

#ifdef ENABLE_MM3D_GAME_FOLDER
#ifdef __APPLE__
    // Qt promotes the menu bar to macOS's global application menu. Hiding the
    // widget alone leaves Azahar's Preferences and emulator actions available.
    ui->menubar->setNativeMenuBar(false);
#endif
    ui->menubar->clear();
    ui->menubar->hide();
    setWindowIcon(QIcon());render_window->setWindowIcon(QIcon());
    ui->action_Show_Status_Bar->setChecked(false);
    statusBar()->hide();
    for(auto* menu:ui->menubar->findChildren<QMenu*>())
        for(auto* action:menu->actions())action->setShortcut(QKeySequence());
    game_list->hide();
    game_list_placeholder->hide();
    game_path = MM3D::GameFolder::Rom();
#endif
#ifdef ENABLE_OOT3D_PRESENTATION
#ifdef __APPLE__
    // Keep emulator menus in the macOS menu bar, outside the game window.
    ui->menubar->setNativeMenuBar(true);
    ui->menubar->show();
#else
    ui->menubar->setNativeMenuBar(false);
    ui->menubar->hide();
#endif
    if (OOT3D::IsPlayerBuild()) {
        ui->menubar->setNativeMenuBar(false);
        for (auto* action : ui->menubar->findChildren<QAction*>())
            action->setMenuRole(QAction::NoRole);
        ui->menubar->clear();
        ui->menubar->hide();
    }
    ui->action_Show_Status_Bar->setChecked(false);
    statusBar()->hide();
    game_list->hide();
    game_list_placeholder->hide();
#endif
    show();

#if defined(__APPLE__) && !defined(ENABLE_OOT3D_PRESENTATION)
    if (AppleUtils::IsRunningFromTerminal()) {
        QMessageBox::warning(
            this, tr("Warning"),
            tr("The `azahar` executable is being run directly rather than via the Azahar.app "
               "bundle.\n\n"
               "When run this way, the app may be missing certain functionality such as camera "
               "emulation.\n\n"
               "It is recommended to instead run Azahar using the `open` command, e.g.:\n"
               "`open ./Azahar.app`"));
    }
#endif

#ifdef ENABLE_QT_UPDATE_CHECKER
    if (UISettings::values.check_for_update_on_start) {
        update_future = QtConcurrent::run([]() -> QString {
            const std::optional<std::string> latest_release_tag =
                UpdateChecker::GetLatestRelease(ShouldCheckForPrereleaseUpdates());

            if (latest_release_tag && latest_release_tag.value() != Common::g_build_fullname) {
                const int latest_major_version = GetMajorVersion(latest_release_tag.value());
                const int current_major_version = GetMajorVersion(Common::g_build_fullname);
                if (current_major_version <= latest_major_version) {
                    return QString::fromStdString(latest_release_tag.value());
                }
            }
            return QString{};
        });
        QObject::connect(&update_watcher, &QFutureWatcher<QString>::finished, this,
                         &GMainWindow::OnEmulatorUpdateAvailable);
        update_watcher.setFuture(update_future);
    }
#endif

    mouse_hide_timer.setInterval(default_mouse_timeout);
    connect(&mouse_hide_timer, &QTimer::timeout, this, &GMainWindow::HideMouseCursor);
    connect(ui->menubar, &QMenuBar::hovered, this, &GMainWindow::OnMouseActivity);

#ifdef ENABLE_OPENGL
    gl_renderer = GetOpenGLRenderer();
#endif

#ifdef ENABLE_VULKAN
    physical_devices = GetVulkanPhysicalDevices();
#endif

    if (!game_path.isEmpty()) {
        BootGame(game_path);
    }
}

GMainWindow::~GMainWindow() {
    // Will get automatically deleted otherwise
    if (!render_window->parent()) {
        delete render_window;
    }

    Pica::g_debug_context.reset();
    // Stop in-game discovery and any pending join before shutting ENet down.
    delete multiplayer_state;
    multiplayer_state = nullptr;
    Network::Shutdown();
}

void GMainWindow::InitializeWidgets() {
    render_window = new GRenderWindow(this, emu_thread.get(), system, false);
    secondary_window = new GRenderWindow(this, emu_thread.get(), system, true);
    render_window->hide();
    secondary_window->hide();
    secondary_window->setParent(nullptr);

    game_list = new GameList(*play_time_manager, this);
    ui->horizontalLayout->addWidget(game_list);

    game_list_placeholder = new GameListPlaceholder(this);
    ui->horizontalLayout->addWidget(game_list_placeholder);
    game_list_placeholder->setVisible(false);

    loading_screen = new LoadingScreen(this);
    loading_screen->hide();
    ui->horizontalLayout->addWidget(loading_screen);
    connect(loading_screen, &LoadingScreen::Hidden, this, [&] {
        loading_screen->Clear();
        if (emulation_running) {
            render_window->show();
            render_window->setFocus();
            render_window->activateWindow();
#ifdef ENABLE_MM3D_PRESENTATION
            MM3D::PlayerSettings::startup_hint_requested.store(true);
#endif
#ifdef ENABLE_OOT3D_PRESENTATION
            OOT3D::StartupHint::requested.store(true);
#endif
        }
    });

    InputCommon::Init();
    multiplayer_state = new MultiplayerState(system, this, game_list->GetModel(),
                                             ui->action_Leave_Room, ui->action_Show_Room);
    multiplayer_state->setVisible(false);

    UpdateBootHomeMenuState();

    // Create status bar
    message_label = new QLabel();
    // Configured separately for left alignment
    message_label->setFrameStyle(QFrame::NoFrame);
    message_label->setContentsMargins(4, 0, 4, 0);
    message_label->setAlignment(Qt::AlignLeft);
    statusBar()->addPermanentWidget(message_label, 1);

    progress_bar = new QProgressBar();
    progress_bar->hide();
    statusBar()->addPermanentWidget(progress_bar);

    loading_shaders_label = new QLabel();

    artic_traffic_label = new QLabel();
    artic_traffic_label->setToolTip(
        tr("Current Artic traffic speed. Higher values indicate bigger transfer loads."));

    emu_speed_label = new QLabel();
    emu_speed_label->setToolTip(tr("Current emulation speed. Values higher or lower than 100% "
                                   "indicate emulation is running faster or slower than a 3DS."));
    game_fps_label = new QLabel();
    game_fps_label->setToolTip(tr("How many frames per second the app is currently displaying. "
                                  "This will vary from app to app and scene to scene."));
    emu_frametime_label = new QLabel();
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a 3DS frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    for (auto& label : {loading_shaders_label, artic_traffic_label, emu_speed_label, game_fps_label,
                        emu_frametime_label}) {
        label->setVisible(false);
        label->setFrameStyle(QFrame::NoFrame);
        label->setContentsMargins(4, 0, 4, 0);
        statusBar()->addPermanentWidget(label);
    }

    // Setup Graphics API button
    graphics_api_button = new QPushButton();
    graphics_api_button->setObjectName(QStringLiteral("GraphicsAPIStatusBarButton"));
    graphics_api_button->setFocusPolicy(Qt::NoFocus);
    UpdateAPIIndicator();

    connect(graphics_api_button, &QPushButton::clicked, this, [this] { UpdateAPIIndicator(true); });

    statusBar()->insertPermanentWidget(0, graphics_api_button);

    volume_popup = new QWidget(this);
    volume_popup->setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Popup);
    volume_popup->setLayout(new QVBoxLayout());
    volume_popup->setMinimumWidth(200);

    volume_slider = new QSlider(Qt::Horizontal);
    volume_slider->setObjectName(QStringLiteral("volume_slider"));
    volume_slider->setMaximum(100);
    volume_slider->setPageStep(5);
    connect(volume_slider, &QSlider::valueChanged, this, [this](int percentage) {
        Settings::values.audio_muted = false;
        const auto value = static_cast<float>(percentage) / volume_slider->maximum();
        Settings::values.volume.SetValue(value);
        UpdateVolumeUI();
    });
    volume_popup->layout()->addWidget(volume_slider);

    volume_button = new QPushButton();
    volume_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    volume_button->setFocusPolicy(Qt::NoFocus);
    volume_button->setCheckable(true);
    UpdateVolumeUI();
    connect(volume_button, &QPushButton::clicked, this, [&] {
        UpdateVolumeUI();
        volume_popup->setVisible(!volume_popup->isVisible());
        QRect rect = volume_button->geometry();
        QPoint bottomLeft = statusBar()->mapToGlobal(rect.topLeft());
        bottomLeft.setY(bottomLeft.y() - volume_popup->geometry().height());
        volume_popup->setGeometry(QRect(bottomLeft, QSize(rect.width(), rect.height())));
    });
    statusBar()->insertPermanentWidget(1, volume_button);

    statusBar()->addPermanentWidget(multiplayer_state->GetStatusText());
    statusBar()->addPermanentWidget(multiplayer_state->GetStatusIcon());

    QFrame* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setFixedHeight(16);
    statusBar()->addPermanentWidget(sep);

    notification_led = new LedWidget();
    notification_led->setToolTip(tr("Emulated notification LED"));
    statusBar()->addPermanentWidget(notification_led);
    connect(this, &GMainWindow::InfoLEDColorChanged, this, [this] {
        auto led_color = system.GetInfoLEDColor();
        notification_led->setColor(QColor(led_color.r(), led_color.g(), led_color.b()));
    });

    statusBar()->setVisible(true);

    // Removes an ugly inner border from the status bar widgets under Linux
    setStyleSheet(QStringLiteral("QStatusBar::item{border: none;}"));

    QActionGroup* actionGroup_ScreenLayouts = new QActionGroup(this);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Default);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Single_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Large_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Side_by_Side);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Separate_Windows);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Hybrid_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Custom_Layout);

    QActionGroup* actionGroup_SmallPositions = new QActionGroup(this);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_TopRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_MiddleRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_BottomRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_TopLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_MiddleLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_BottomLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_Above);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_Below);
}

void GMainWindow::InitializeDebugWidgets() {
    if (Pica::g_debug_context) {
        connect(ui->action_Create_Pica_Surface_Viewer, &QAction::triggered, this,
                &GMainWindow::OnCreateGraphicsSurfaceViewer);
    } else {
        ui->action_Create_Pica_Surface_Viewer->setEnabled(false);
    }

    QMenu* debug_menu = ui->menu_View_Debugging;

#if MICROPROFILE_ENABLED
    microProfileDialog = new MicroProfileDialog(this);
    microProfileDialog->hide();
    debug_menu->addAction(microProfileDialog->toggleViewAction());
#endif

    registersWidget = new RegistersWidget(system, this);
    addDockWidget(Qt::RightDockWidgetArea, registersWidget);
    registersWidget->hide();
    debug_menu->addAction(registersWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, registersWidget,
            &RegistersWidget::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, registersWidget,
            &RegistersWidget::OnEmulationStopping);

    if (Pica::g_debug_context) {
        graphicsWidget = new GPUCommandStreamWidget(system, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsWidget);
        graphicsWidget->hide();
        debug_menu->addAction(graphicsWidget->toggleViewAction());

        graphicsCommandsWidget = new GPUCommandListWidget(system, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsCommandsWidget);
        graphicsCommandsWidget->hide();
        debug_menu->addAction(graphicsCommandsWidget->toggleViewAction());

        graphicsBreakpointsWidget = new GraphicsBreakPointsWidget(Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsBreakpointsWidget);
        graphicsBreakpointsWidget->hide();
        debug_menu->addAction(graphicsBreakpointsWidget->toggleViewAction());

        graphicsVertexShaderWidget =
            new GraphicsVertexShaderWidget(system, Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsVertexShaderWidget);
        graphicsVertexShaderWidget->hide();
        debug_menu->addAction(graphicsVertexShaderWidget->toggleViewAction());

        graphicsTracingWidget = new GraphicsTracingWidget(system, Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsTracingWidget);
        graphicsTracingWidget->hide();
        debug_menu->addAction(graphicsTracingWidget->toggleViewAction());
        connect(this, &GMainWindow::EmulationStarting, graphicsTracingWidget,
                &GraphicsTracingWidget::OnEmulationStarting);
        connect(this, &GMainWindow::EmulationStopping, graphicsTracingWidget,
                &GraphicsTracingWidget::OnEmulationStopping);
    }

    waitTreeWidget = new WaitTreeWidget(system, this);
    addDockWidget(Qt::LeftDockWidgetArea, waitTreeWidget);
    waitTreeWidget->hide();
    debug_menu->addAction(waitTreeWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStopping);

    lleServiceModulesWidget = new LLEServiceModulesWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, lleServiceModulesWidget);
    lleServiceModulesWidget->hide();
    debug_menu->addAction(lleServiceModulesWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting,
            [this] { lleServiceModulesWidget->setDisabled(true); });
    connect(this, &GMainWindow::EmulationStopping, waitTreeWidget,
            [this] { lleServiceModulesWidget->setDisabled(false); });

    ipcRecorderWidget = new IPCRecorderWidget(system, this);
    addDockWidget(Qt::RightDockWidgetArea, ipcRecorderWidget);
    ipcRecorderWidget->hide();
    debug_menu->addAction(ipcRecorderWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, ipcRecorderWidget,
            &IPCRecorderWidget::OnEmulationStarting);
}

void GMainWindow::InitializeRecentFileMenuActions() {
    for (int i = 0; i < max_recent_files_item; ++i) {
        actions_recent_files[i] = new QAction(this);
        actions_recent_files[i]->setVisible(false);
        connect(actions_recent_files[i], &QAction::triggered, this, &GMainWindow::OnMenuRecentFile);

        ui->menu_recent_files->addAction(actions_recent_files[i]);
    }
    ui->menu_recent_files->addSeparator();
    QAction* action_clear_recent_files = new QAction(this);
    action_clear_recent_files->setText(tr("Clear Recent Files"));
    connect(action_clear_recent_files, &QAction::triggered, this, [this] {
        UISettings::values.recent_files.clear();
        UpdateRecentFiles();
    });
    ui->menu_recent_files->addAction(action_clear_recent_files);

    UpdateRecentFiles();
}

void GMainWindow::InitializeSaveStateMenuActions() {
    for (u32 i = 0; i < Core::SaveStateSlotCount; ++i) {
        actions_load_state[i] = new QAction(this);
        actions_load_state[i]->setData(i);
        connect(actions_load_state[i], &QAction::triggered, this, &GMainWindow::OnLoadState);
        if (i > 0)
            ui->menu_Load_State->addAction(actions_load_state[i]);
        actions_save_state[i] = new QAction(this);
        actions_save_state[i]->setData(i);
        connect(actions_save_state[i], &QAction::triggered, this, &GMainWindow::OnSaveState);
        if (i > 0)
            ui->menu_Save_State->addAction(actions_save_state[i]);
    }

    connect(ui->action_Load_from_Newest_Slot, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        if (newest_slot != 0) {
            actions_load_state[newest_slot]->trigger();
        }
    });
    connect(ui->action_Save_to_Oldest_Slot, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_save_state[oldest_slot]->trigger();
    });

    // Quick save / load uses slot
    connect(ui->action_Quick_Save, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_save_state[0]->trigger();
    });
    connect(ui->action_Quick_Load, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_load_state[0]->trigger();
    });

    connect(ui->menu_Load_State->menuAction(), &QAction::hovered, this,
            &GMainWindow::UpdateSaveStates);
    connect(ui->menu_Save_State->menuAction(), &QAction::hovered, this,
            &GMainWindow::UpdateSaveStates);

    UpdateSaveStates();
}

void GMainWindow::OnConfigurePresentationShortcut(bool texture_binding) {
    const QString group=QStringLiteral("Main Window"), action=texture_binding?QStringLiteral("Toggle Custom Textures"):QStringLiteral("Fullscreen");
    const QKeySequence default_sequence=texture_binding?QKeySequence{}:DefaultFullscreenShortcut();
    QDialog dialog(this);dialog.setWindowTitle(texture_binding?tr("Custom textures shortcut"):tr("Fullscreen shortcut"));
    auto* layout=new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(texture_binding?tr("Choose a shortcut to toggle custom textures. Leave empty for no shortcut."):tr("Press a key or key combination to toggle fullscreen."),&dialog));
    auto* edit=new QKeySequenceEdit(hotkey_registry.GetKeySequence(group,action),&dialog);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    edit->setMaximumSequenceLength(1);
#endif
    layout->addWidget(edit);
    auto* message=new QLabel(&dialog);message->setWordWrap(true);layout->addWidget(message);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::RestoreDefaults,&dialog);
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(texture_binding?tr("Clear shortcut"):tr("Reset to %1").arg(default_sequence.toString(QKeySequence::NativeText)));
    layout->addWidget(buttons);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),&QPushButton::clicked,&dialog,[edit,default_sequence] {
        edit->setKeySequence(default_sequence);
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&,edit,message] {
        const auto sequence=edit->keySequence();
        if(texture_binding&&sequence.isEmpty()) {
            hotkey_registry.SetKeySequence(group,action,{});
            hotkey_registry.SaveHotkeys();config->Save();dialog.accept();return;
        }
        if(sequence.count()!=1) {message->setText(tr("Choose one key or key combination."));return;}
        const auto key=sequence[0].key();
        if(key==Qt::Key_F1||key==Qt::Key_F2||key==Qt::Key_Escape) {
            message->setText(tr("F1, F2 and Escape are reserved. Choose another key."));return;
        }
        for(const auto& other:UISettings::values.shortcuts) {
            if(other.group==group&&other.name==action)continue;
            const auto used=QKeySequence::fromString(other.shortcut.keyseq,QKeySequence::NativeText);
            if(!used.isEmpty() && (sequence.matches(used)!=QKeySequence::NoMatch || used.matches(sequence)!=QKeySequence::NoMatch)) {
                message->setText(tr("That shortcut is already assigned to %1.").arg(other.name));return;
            }
        }
        const auto bound=[key](const Common::ParamPackage& p) {
            return p.Get("engine", "")=="keyboard" && p.Get("code",0)==int(key);
        };
        bool gameplay_key=false;
        for(const auto& b:Settings::values.current_input_profile.buttons)
            gameplay_key|=bound(Common::ParamPackage(b));
        for(const auto& a:Settings::values.current_input_profile.analogs) {
            const Common::ParamPackage analog(a);
            if(analog.Get("engine", "")!="analog_from_button")continue;
            for(const auto* direction:{"up","down","left","right","modifier"})
                gameplay_key|=bound(Common::ParamPackage(analog.Get(direction,"")));
        }
        if(gameplay_key && sequence[0].keyboardModifiers()==Qt::NoModifier) {message->setText(tr("That key is assigned to a game control. Choose another key."));return;}
        hotkey_registry.SetKeySequence(group,action,sequence);
        hotkey_registry.SaveHotkeys();config->Save();dialog.accept();
    });
    edit->setFocus();dialog.exec();render_window->setFocus();
}

void GMainWindow::InitializeHotkeys() {
    hotkey_registry.LoadHotkeys();
#ifdef ENABLE_MM3D_GAME_FOLDER
    return; // F1 and fullscreen are handled by the presentation event filter.
#endif
    hotkey_registry.buttonMonitor.start(16);
    LOG_DEBUG(Frontend, "Initializing hotkeys");
    const QString main_window = QStringLiteral("Main Window");
    const QString fullscreen = QStringLiteral("Fullscreen");

    // QAction Hotkeys
    const auto link_action_shortcut = [&](QAction* action, const QString& action_name,
                                          const bool primary_only = false,
                                          const bool auto_repeat = false) {
#ifdef ENABLE_OOT3D_PRESENTATION
        if (OOT3D::IsPlayerBuild() && !OOT3D::PlayerShortcutAllowed(action_name.toStdString())) return;
#endif
        static const QString main_window = QStringLiteral("Main Window");
        auto context = hotkey_registry.GetShortcutContext(main_window, action_name);
        auto shortcut = hotkey_registry.GetKeySequence(main_window, action_name);
        action->setShortcut(shortcut);
        action->setShortcutContext(context);
        action->setAutoRepeat(auto_repeat);
        this->addAction(action);
        // handle the shortcuts that are different per-screen
        if (context == Qt::WidgetShortcut) {
            render_window->addAction(action);
            if (!primary_only) {
                secondary_window->addAction(action);
            }
        }
        hotkey_registry.SetAction(main_window, action_name, action);
    };

    link_action_shortcut(ui->action_Load_File, QStringLiteral("Load File"));
    link_action_shortcut(ui->action_Load_Amiibo, QStringLiteral("Load Amiibo"));
    link_action_shortcut(ui->action_Remove_Amiibo, QStringLiteral("Remove Amiibo"));
    link_action_shortcut(ui->action_Exit, QStringLiteral("Exit Azahar"));
    link_action_shortcut(ui->action_Restart, QStringLiteral("Restart Emulation"));
    link_action_shortcut(ui->action_Pause, QStringLiteral("Continue/Pause Emulation"));
    link_action_shortcut(ui->action_Stop, QStringLiteral("Stop Emulation"));
    link_action_shortcut(ui->action_Show_Filter_Bar, QStringLiteral("Toggle Filter Bar"));
    link_action_shortcut(ui->action_Show_Status_Bar, QStringLiteral("Toggle Status Bar"));
    link_action_shortcut(ui->action_Fullscreen, fullscreen);
    link_action_shortcut(ui->action_Capture_Screenshot, QStringLiteral("Capture Screenshot"));
    link_action_shortcut(ui->action_Debug_Pause, QStringLiteral("Debug Pause"));
    link_action_shortcut(ui->action_Debug_Resume, QStringLiteral("Debug Resume"));
    link_action_shortcut(ui->action_Debug_Step, QStringLiteral("Debug Step"), false, true);
    link_action_shortcut(ui->action_Debug_Unschedule_All, QStringLiteral("Debug Unschedule All"));
    link_action_shortcut(ui->action_Debug_Schedule_All, QStringLiteral("Debug Schedule All"));
    link_action_shortcut(ui->action_Screen_Layout_Swap_Screens, QStringLiteral("Swap Screens"));
    link_action_shortcut(ui->action_Screen_Layout_Upright_Screens,
                         QStringLiteral("Rotate Screens Upright"));
    link_action_shortcut(ui->action_Advance_Frame, QStringLiteral("Advance Frame"));
    link_action_shortcut(ui->action_Load_from_Newest_Slot,
                         QStringLiteral("Load from Newest Non-Quicksave Slot"));
    link_action_shortcut(ui->action_Save_to_Oldest_Slot,
                         QStringLiteral("Save to Oldest Non-Quicksave Slot"));
    link_action_shortcut(ui->action_Quick_Save, QStringLiteral("Quick Save"));
    link_action_shortcut(ui->action_Quick_Load, QStringLiteral("Quick Load"));
    link_action_shortcut(ui->action_View_Lobby, QStringLiteral("Multiplayer Browse Public Rooms"));
    link_action_shortcut(ui->action_Start_Room, QStringLiteral("Multiplayer Create Room"));
    link_action_shortcut(ui->action_Connect_To_Room,
                         QStringLiteral("Multiplayer Direct Connect to Room"));
    link_action_shortcut(ui->action_Show_Room, QStringLiteral("Multiplayer Show Current Room"));
    link_action_shortcut(ui->action_Leave_Room, QStringLiteral("Multiplayer Leave Room"));

    // QShortcut Hotkeys
    const auto connect_shortcut = [&](const QString& action_name, const auto& function) {
#ifdef ENABLE_OOT3D_PRESENTATION
        if (OOT3D::IsPlayerBuild() && !OOT3D::PlayerShortcutAllowed(action_name.toStdString())) return;
#endif
        auto* hotkey = hotkey_registry.GetHotkey(main_window, action_name, this);
        if(action_name==QStringLiteral("Toggle Custom Textures"))hotkey->setAutoRepeat(false);
        connect(hotkey, &QShortcut::activated, this, function);
    };

    connect_shortcut(QStringLiteral("Toggle Screen Layout"), &GMainWindow::ToggleScreenLayout);
    connect_shortcut(QStringLiteral("Exit Fullscreen"), &GMainWindow::ExitFullscreen);
#ifdef ENABLE_OOT3D_PRESENTATION
    auto* minimap_hotkey=hotkey_registry.GetHotkey(main_window,QStringLiteral("Toggle Minimap"),this);
    minimap_hotkey->setAutoRepeat(false);
    connect(minimap_hotkey,&QShortcut::activated,this,[this] {
        if(emulation_running && emu_thread && emu_thread->IsRunning() &&
           QGuiApplication::applicationState()==Qt::ApplicationActive &&
           !QApplication::activeModalWidget() && !OOT3D::visible.load())
            OOT3D::MinimapControl::RequestToggle();
    });
    auto* look_hotkey=hotkey_registry.GetHotkey(main_window,QStringLiteral("First-Person View"),this);
    look_hotkey->setAutoRepeat(false);
    connect(look_hotkey,&QShortcut::activated,this,[this] {
        if(emulation_running && emu_thread && emu_thread->IsRunning() &&
           QGuiApplication::applicationState()==Qt::ApplicationActive &&
           !QApplication::activeModalWidget() && !OOT3D::visible.load())
            OOT3D::DPad::RequestLook();
    });
#endif

    connect_shortcut(QStringLiteral("Toggle Per-Application Speed"), [&] {
        if (!hotkey_registry
                 .GetKeySequence(QStringLiteral("Main Window"), QStringLiteral("Toggle Turbo Mode"))
                 .isEmpty()) {
            return;
        }
        Settings::values.frame_limit.SetGlobal(!Settings::values.frame_limit.UsingGlobal());
        UpdateStatusBar();
    });
    connect_shortcut(QStringLiteral("Toggle Texture Dumping"),
                     [&] { Settings::values.dump_textures = !Settings::values.dump_textures; });
    connect_shortcut(QStringLiteral("Toggle Custom Textures"), [&] {
#ifdef ENABLE_OOT3D_PRESENTATION
        const int pending=OOT3D::requested_custom_textures.load();
        OOT3D::requested_custom_textures=!(pending<0?Settings::values.custom_textures.GetValue():pending!=0);
#elif defined(ENABLE_MM3D_PRESENTATION)
        MM3D::PlayerSettings::set_textures.store(!Settings::values.custom_textures.GetValue());
#else
        Settings::values.custom_textures = !Settings::values.custom_textures.GetValue();
        system.ApplySettings();
#endif
    });

    connect_shortcut(QStringLiteral("Toggle Turbo Mode"),
                     [&] { GMainWindow::SetTurboEnabled(!GMainWindow::IsTurboEnabled()); });

    connect_shortcut(QStringLiteral("Increase Speed Limit"), [&] { AdjustSpeedLimit(true); });
    connect_shortcut(QStringLiteral("Decrease Speed Limit"), [&] { AdjustSpeedLimit(false); });

    connect_shortcut(QStringLiteral("Audio Mute/Unmute"), &GMainWindow::OnMute);
    connect_shortcut(QStringLiteral("Audio Volume Down"), &GMainWindow::OnDecreaseVolume);
    connect_shortcut(QStringLiteral("Audio Volume Up"), &GMainWindow::OnIncreaseVolume);

    // We use "static" here in order to avoid capturing by lambda due to a MSVC bug, which makes the
    // variable hold a garbage value after this function exits
    static constexpr u16 FACTOR_3D_STEP = 5;
    connect_shortcut(QStringLiteral("Decrease 3D Factor"), [this] {
        const auto factor_3d = Settings::values.factor_3d.GetValue();
        if (factor_3d > 0) {
            if (factor_3d % FACTOR_3D_STEP != 0) {
                Settings::values.factor_3d = factor_3d - (factor_3d % FACTOR_3D_STEP);
            } else {
                Settings::values.factor_3d = factor_3d - FACTOR_3D_STEP;
            }
            UpdateStatusBar();
        }
    });
    connect_shortcut(QStringLiteral("Increase 3D Factor"), [this] {
        const auto factor_3d = Settings::values.factor_3d.GetValue();
        if (factor_3d < 255) {
            if (factor_3d % FACTOR_3D_STEP != 0) {
                Settings::values.factor_3d =
                    factor_3d + FACTOR_3D_STEP - (factor_3d % FACTOR_3D_STEP);
            } else {
                Settings::values.factor_3d = factor_3d + FACTOR_3D_STEP;
            }
            UpdateStatusBar();
        }
    });
}

void GMainWindow::SetDefaultUIGeometry() {
#ifdef ENABLE_MM3D_GAME_FOLDER
    const QRect available = screen()->availableGeometry();
    const int unit = std::max(1, std::min(available.width() * 4 / 5 / 16,
                                         available.height() * 4 / 5 / 9));
    const QSize size(16 * unit, 9 * unit);
    setGeometry(QRect(available.center() - QPoint(size.width() / 2, size.height() / 2),
                      size));
#else
    // geometry: 55% of the window contents are in the upper screen half, 45% in the lower half
    const QRect screenRect = screen()->geometry();

    const int w = screenRect.width() * 2 / 3;
    const int h = screenRect.height() / 2;
    const int x = (screenRect.x() + screenRect.width()) / 2 - w / 2;
    const int y = (screenRect.y() + screenRect.height()) / 2 - h * 55 / 100;

    setGeometry(x, y, w, h);
#endif
}

void GMainWindow::RestoreUIState() {
#ifndef ENABLE_MM3D_GAME_FOLDER
    restoreGeometry(UISettings::values.geometry);
#endif
    restoreState(UISettings::values.state);
    render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
    secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
#if MICROPROFILE_ENABLED
    microProfileDialog->restoreGeometry(UISettings::values.microprofile_geometry);
    microProfileDialog->setVisible(UISettings::values.microprofile_visible.GetValue());
#endif

    game_list->LoadInterfaceLayout();

#ifdef ENABLE_MM3D_GAME_FOLDER
    UISettings::values.single_window_mode=true;
    UISettings::values.show_status_bar=false;
    // Player builds use the stable audio path; do not inherit Azahar's
    // audio-stretching default from an existing shared configuration.
    Settings::values.enable_audio_stretching=false;
#endif
    ui->action_Single_Window_Mode->setChecked(UISettings::values.single_window_mode.GetValue());
    ToggleWindowMode();

    ui->action_Fullscreen->setChecked(UISettings::values.fullscreen.GetValue());
    SyncMenuUISettings();

    ui->action_Display_Dock_Widget_Headers->setChecked(
        UISettings::values.display_titlebar.GetValue());
    OnDisplayTitleBars(ui->action_Display_Dock_Widget_Headers->isChecked());

    ui->action_Show_Filter_Bar->setChecked(UISettings::values.show_filter_bar.GetValue());
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());

    ui->action_Show_Status_Bar->setChecked(UISettings::values.show_status_bar.GetValue());
    statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
}

void GMainWindow::OnAppFocusStateChanged(Qt::ApplicationState state) {
    if (state != Qt::ApplicationHidden && state != Qt::ApplicationInactive &&
        state != Qt::ApplicationActive) {
        LOG_DEBUG(Frontend, "ApplicationState unusual flag: {} ", state);
    }
    if (!emulation_running) {
        return;
    }
    if (UISettings::values.pause_when_in_background) {
        if (emu_thread->IsRunning() &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            auto_paused = true;
            OnPauseGame();
        } else if (!emu_thread->IsRunning() && auto_paused && state == Qt::ApplicationActive) {
            auto_paused = false;
            OnResumeGame(false);
        }
    }
    if (UISettings::values.mute_when_in_background) {
        if (!Settings::values.audio_muted &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            Settings::values.audio_muted = true;
            auto_muted = true;
        } else if (auto_muted && state == Qt::ApplicationActive) {
            Settings::values.audio_muted = false;
            auto_muted = false;
        }
        UpdateVolumeUI();
    }
}

bool GApplicationEventFilter::eventFilter(QObject* object, QEvent* event) {
#ifdef ENABLE_OOT3D_PRESENTATION
    // Name typing belongs to the focused game window, before keyboard-to-pad
    // bindings or QAction shortcuts can turn letters into unrelated actions.
    static QSet<int> name_typing_keys;
    if(event->type()==QEvent::KeyPress || event->type()==QEvent::KeyRelease ||
       event->type()==QEvent::ShortcutOverride) {
        auto* key=static_cast<QKeyEvent*>(event);
        if(event->type()==QEvent::KeyRelease && name_typing_keys.contains(key->key())) {
            if(!key->isAutoRepeat())name_typing_keys.remove(key->key());
            InputCommon::GetKeyboard()->ReleaseKey(key->key());
            event->accept();return true;
        }
        bool game_focused=false;
        for(auto* widget=qobject_cast<QWidget*>(object);widget;widget=widget->parentWidget())
            if(qobject_cast<GRenderWindow*>(widget)){game_focused=true;break;}
        if(game_focused && OOT3D::available.load() && !OOT3D::visible.load() &&
           OOT3D::NameTyping::available.load() && !QApplication::activeModalWidget() &&
           !(key->modifiers()&(Qt::ControlModifier|Qt::MetaModifier))) {
            const bool backspace=key->key()==Qt::Key_Backspace;
            const bool confirm=key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter;
            const bool text=!key->text().isEmpty() && key->text().front().isPrint();
            if(backspace || confirm || text) {
                if(event->type()==QEvent::KeyPress) {
                    name_typing_keys.insert(key->key());
                    InputCommon::GetKeyboard()->ReleaseKey(key->key());
                    if(backspace)OOT3D::NameTyping::Push(OOT3D::NameTyping::Backspace);
                    else if(confirm) {
                        if(!key->isAutoRepeat())OOT3D::NameTyping::Push(OOT3D::NameTyping::Confirm);
                    } else for(auto code:key->text().normalized(QString::NormalizationForm_C).toUcs4())
                        OOT3D::NameTyping::Push(code);
                }
                event->accept();return true;
            }
        }
    }
    // Reserve F2 before QAction handles Load Amiibo; keep typing in ImGui local.
    if (OOT3D::available.load() && !QApplication::activeModalWidget() &&
        event->type()==QEvent::ShortcutOverride) {
        const auto* key_event=static_cast<QKeyEvent*>(event);
        const auto key=key_event->key();
        // These player shortcuts also work with F1/F2 open, except while typing.
        // Let Qt dispatch the existing QAction/QShortcut exactly once.
        if(OOT3D::visible.load() && !OOT3D::text_input.load() &&
           key!=Qt::Key_F1 && key!=Qt::Key_F2 && key!=Qt::Key_Escape) {
            for(const auto& shortcut:UISettings::values.shortcuts) {
                if(shortcut.group!=QStringLiteral("Main Window") ||
                   (shortcut.name!=QStringLiteral("Fullscreen") &&
                    shortcut.name!=QStringLiteral("Toggle Custom Textures")))continue;
                const auto sequence=QKeySequence::fromString(shortcut.shortcut.keyseq,QKeySequence::NativeText);
                if(sequence.count()==1 && sequence[0]==key_event->keyCombination())return false;
            }
        }
        if (key==Qt::Key_F1 || key==Qt::Key_F2 || OOT3D::visible.load()) {
            event->accept();return true;
        }
    }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
#ifndef ENABLE_MM3D_GAME_FOLDER
    // F2 is normally Azahar's Load Amiibo shortcut. Reserve it only while
    // the developer presentation overlay is available.
    if(event->type()==QEvent::ShortcutOverride && MM3D::presentation_available.load() &&
       static_cast<QKeyEvent*>(event)->key()==Qt::Key_F2 && !QApplication::activeModalWidget()) {
        event->accept();return true;
    }
#endif
    static QSet<int> editing_keys;
    if (!MM3D::overlay_visible.load()) editing_keys.clear();
    const bool keyboard_event = event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease;
    const bool editor_focused = MM3D::text_input_active.load() && !QApplication::activeModalWidget();
    if (keyboard_event && (static_cast<QKeyEvent*>(event)->key() == Qt::Key_F1
#ifndef ENABLE_MM3D_GAME_FOLDER
        || static_cast<QKeyEvent*>(event)->key() == Qt::Key_F2
#endif
        ))
        return false;
    if (MM3D::overlay_visible.load() && editor_focused &&
        event->type() == QEvent::ShortcutOverride) {
        event->accept(); // Prevent Space/C and application shortcuts while editing.
        return true;
    }
    if (MM3D::overlay_visible.load() && keyboard_event &&
        (editor_focused || (event->type() == QEvent::KeyRelease &&
                           editing_keys.contains(static_cast<QKeyEvent*>(event)->key())))) {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool down = event->type() == QEvent::KeyPress;
        if (!key->isAutoRepeat()) {
            if (down) editing_keys.insert(key->key());
            else editing_keys.remove(key->key());
        }
        ImGuiKey mapped = ImGuiKey_None;
        switch (key->key()) {
        case Qt::Key_Backspace: mapped = ImGuiKey_Backspace; break;
        case Qt::Key_Delete: mapped = ImGuiKey_Delete; break;
        case Qt::Key_Left: mapped = ImGuiKey_LeftArrow; break;
        case Qt::Key_Right: mapped = ImGuiKey_RightArrow; break;
        case Qt::Key_Home: mapped = ImGuiKey_Home; break;
        case Qt::Key_End: mapped = ImGuiKey_End; break;
        case Qt::Key_Return: case Qt::Key_Enter: mapped = ImGuiKey_Enter; break;
        case Qt::Key_Escape: mapped = ImGuiKey_Escape; break;
        case Qt::Key_Tab: mapped = ImGuiKey_Tab; break;
        default:
            if (key->key() >= Qt::Key_A && key->key() <= Qt::Key_Z)
                mapped = ImGuiKey(ImGuiKey_A + key->key() - Qt::Key_A);
            break;
        }
        MM3D::KeyInput(ImGuiMod_Shift, key->modifiers().testFlag(Qt::ShiftModifier));
#ifdef Q_OS_MAC
        MM3D::KeyInput(ImGuiMod_Super, key->modifiers().testFlag(Qt::ControlModifier));
        MM3D::KeyInput(ImGuiMod_Ctrl, key->modifiers().testFlag(Qt::MetaModifier));
#else
        MM3D::KeyInput(ImGuiMod_Ctrl, key->modifiers().testFlag(Qt::ControlModifier));
        MM3D::KeyInput(ImGuiMod_Super, key->modifiers().testFlag(Qt::MetaModifier));
#endif
        MM3D::KeyInput(ImGuiMod_Alt, key->modifiers().testFlag(Qt::AltModifier));
        if (mapped != ImGuiKey_None && !key->isAutoRepeat()) MM3D::KeyInput(mapped, down);
        if (down && !key->text().isEmpty() && key->text().at(0).isPrint() &&
            !(key->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) {
            const auto text = key->text().toUtf8();
            MM3D::TextInput(text.constData());
        }
        event->accept();
        return true;
    }
    // Handle the saved fullscreen binding in both desktop and game-folder
    // builds. Consume ShortcutOverride too, avoiding duplicate QAction dispatch.
    if (MM3D::presentation_available.load() && !QApplication::activeModalWidget() &&
        (keyboard_event || event->type() == QEvent::ShortcutOverride)) {
        auto* key = static_cast<QKeyEvent*>(event);
        auto* widget = qobject_cast<QWidget*>(object);
        auto* window = widget ? widget->window() : nullptr;
        if (qobject_cast<GMainWindow*>(window) || qobject_cast<GRenderWindow*>(window)) {
            for (const auto& shortcut : UISettings::values.shortcuts) {
                if (shortcut.group != QStringLiteral("Main Window") ||
                    shortcut.name != QStringLiteral("Fullscreen")) continue;
                const auto sequence=QKeySequence::fromString(shortcut.shortcut.keyseq,QKeySequence::NativeText);
                if (sequence.count()==1 && sequence[0]==key->keyCombination()) {
                    if(event->type()==QEvent::KeyPress && !key->isAutoRepeat())
                        MM3D::PlayerSettings::fullscreen.store(true);
                    event->accept();return true;
                }
            }
        }
    }
    // Reserve Escape for leaving the game window's fullscreen mode. Catch it
    // before native window handling or a saved application shortcut can quit.
    // Dialogs and active ImGui text editing retain their normal Escape behavior.
    if (MM3D::presentation_available.load() && !QApplication::activeModalWidget() &&
        (keyboard_event || event->type() == QEvent::ShortcutOverride)) {
        auto* key = static_cast<QKeyEvent*>(event);
        auto* widget = qobject_cast<QWidget*>(object);
        auto* window = widget ? widget->window() : nullptr;
        if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier &&
            (qobject_cast<GMainWindow*>(window) || qobject_cast<GRenderWindow*>(window))) {
            if (event->type() == QEvent::KeyPress && !key->isAutoRepeat())
                emit ExitFullscreen();
            event->accept();
            return true;
        }
    }
#endif
    if (event->type() == QEvent::FileOpen) {
        emit FileOpen(static_cast<QFileOpenEvent*>(event));
        return true;
    }
    return false;
}

void GMainWindow::ConnectAppEvents() {
    const auto filter = new GApplicationEventFilter();
    QGuiApplication::instance()->installEventFilter(filter);

    connect(filter, &GApplicationEventFilter::FileOpen, this, &GMainWindow::OnFileOpen);
    connect(filter, &GApplicationEventFilter::ExitFullscreen, this, &GMainWindow::ExitFullscreen);
}

void GMainWindow::ConnectWidgetEvents() {
    connect(game_list, &GameList::GameChosen, this, &GMainWindow::OnGameListLoadFile);
    connect(game_list, &GameList::OpenDirectory, this, &GMainWindow::OnGameListOpenDirectory);
    connect(game_list, &GameList::OpenFolderRequested, this, &GMainWindow::OnGameListOpenFolder);
    connect(game_list, &GameList::RemovePlayTimeRequested, this,
            &GMainWindow::OnGameListRemovePlayTimeData);
    connect(game_list, &GameList::CreateShortcut, this, &GMainWindow::OnGameListCreateShortcut);
    connect(game_list, &GameList::DumpRomFSRequested, this, &GMainWindow::OnGameListDumpRomFS);
    connect(game_list, &GameList::AddDirectory, this, &GMainWindow::OnGameListAddDirectory);
    connect(game_list_placeholder, &GameListPlaceholder::AddDirectory, this,
            &GMainWindow::OnGameListAddDirectory);
    connect(game_list, &GameList::ShowList, this, &GMainWindow::OnGameListShowList);
    connect(game_list, &GameList::PopulatingCompleted, this,
            [this] { multiplayer_state->UpdateGameList(game_list->GetModel()); });
#ifdef ENABLE_DEVELOPER_OPTIONS
    connect(game_list, &GameList::StartingLaunchStressTest, this,
            &GMainWindow::StartLaunchStressTest);
#endif

    connect(game_list, &GameList::OpenPerGameGeneralRequested, this,
            &GMainWindow::OnGameListOpenPerGameProperties);

    connect(this, &GMainWindow::EmulationStarting, render_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, render_window,
            &GRenderWindow::OnEmulationStopping);
    connect(this, &GMainWindow::EmulationStarting, secondary_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, secondary_window,
            &GRenderWindow::OnEmulationStopping);

    connect(&status_bar_update_timer, &QTimer::timeout, this, &GMainWindow::UpdateStatusBar);
#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::resolution.store(Settings::values.resolution_factor.GetValue());
    OOT3D::custom_textures=Settings::values.custom_textures.GetValue();
    OOT3D::PlayerGraphics::ApplyPending(Settings::values, [] {}, [] {});
    const auto editor_path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir)) +
                             QStringLiteral("oot3d-editor.ini");
    auto* oot_editor_config = new QSettings(editor_path, QSettings::IniFormat, this);
    OOT3D::Randomizer::Initialize(*oot_editor_config,
        FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "oot3d-randomizer");
    OOT3D::HDFont::SetMode(oot_editor_config->value(QStringLiteral("Graphics/font_mode"),
        oot_editor_config->value(QStringLiteral("Graphics/hd_font"),false).toBool()?
            OOT3D::HDFont::Heavy:OOT3D::HDFont::Original).toInt());
    OOT3D::ActorDrawDistance::multiplier.store(OOT3D::ActorDrawDistance::Clamp(
        oot_editor_config->value(QStringLiteral("Graphics/actor_draw_distance"),1.f).toFloat()));
    OOT3D::AudioSettings::mute_music.store(
        oot_editor_config->value(QStringLiteral("Audio/mute_music"),false).toBool());
    AudioCore::HLE::accurate_audio.store(
        oot_editor_config->value(QStringLiteral("Audio/accurate_hle_audio"),true).toBool());
    OOT3D::AudioSettings::volume.store(std::clamp(
        int(Settings::values.volume.GetValue()*100),0,100));
    OOT3D::Difficulty::damage_multiplier.store(OOT3D::Difficulty::Multiplier(
        oot_editor_config->value(QStringLiteral("Difficulty/damage_multiplier"),1.f).toFloat()));
    OOT3D::Difficulty::disable_hearts.store(oot_editor_config->value(QStringLiteral("Difficulty/disable_hearts"),false).toBool());
    OOT3D::Difficulty::disable_fairies.store(oot_editor_config->value(QStringLiteral("Difficulty/disable_fairies"),false).toBool());
    OOT3D::Difficulty::disable_rupees.store(oot_editor_config->value(QStringLiteral("Difficulty/disable_rupees"),false).toBool());
    OOT3D::Difficulty::weaker_defense.store(oot_editor_config->value(QStringLiteral("Difficulty/weaker_defense"),false).toBool());
    OOT3D::Difficulty::disable_magic.store(oot_editor_config->value(QStringLiteral("Difficulty/disable_magic"),false).toBool());
    OOT3D::Difficulty::disable_ammo.store(oot_editor_config->value(QStringLiteral("Difficulty/disable_ammo"),false).toBool());
    OOT3D::Difficulty::disable_fairy_drops.store(oot_editor_config->value(QStringLiteral("Difficulty/disable_fairy_drops"),false).toBool());
    OOT3D::Difficulty::lose_rupees_on_death.store(oot_editor_config->value(QStringLiteral("Difficulty/lose_rupees_on_death"),false).toBool());
    OOT3D::Difficulty::increase_prices.store(oot_editor_config->value(QStringLiteral("Difficulty/increase_prices"),false).toBool());
    OOT3D::Difficulty::red_potion_hearts.store(std::clamp(oot_editor_config->value(QStringLiteral("Difficulty/red_potion_hearts"),20).toInt(),1,20));
    OOT3D::Difficulty::blue_potion_hearts.store(std::clamp(oot_editor_config->value(QStringLiteral("Difficulty/blue_potion_hearts"),20).toInt(),1,20));
    OOT3D::Difficulty::bottled_fairy_hearts.store(std::clamp(oot_editor_config->value(QStringLiteral("Difficulty/bottled_fairy_hearts"),20).toInt(),1,20));
    OOT3D::Difficulty::price_multiplier.store(std::clamp(oot_editor_config->value(QStringLiteral("Difficulty/price_multiplier"),2).toInt(),2,3));
    OOT3D::Difficulty::InitializePrices();
    OOT3D::QOL::disable_navi.store(oot_editor_config->value(QStringLiteral("QOL/disable_navi"),false).toBool());
    OOT3D::QOL::disable_heart_beep.store(oot_editor_config->value(QStringLiteral("QOL/disable_heart_beep"),false).toBool());
    OOT3D::QOL::fast_ladder.store(oot_editor_config->value(QStringLiteral("QOL/fast_ladder"),false).toBool());
    OOT3D::QOL::fast_loading.store(oot_editor_config->value(QStringLiteral("QOL/fast_loading"),true).toBool());
    // Fixed player behavior; F1 can temporarily reveal the stones for editing.
    OOT3D::QOL::hide_sheikah_stones.store(true);
    OOT3D::QOL::no_bombchu_first_person.store(oot_editor_config->value(QStringLiteral("QOL/no_bombchu_first_person"),false).toBool());
    OOT3D::Widescreen::enabled.store(
        oot_editor_config->value(QStringLiteral("Graphics/widescreen"), false).toBool());
    OOT3D::Camera::desired_fov.store(OOT3D::Camera::Sanitize(
        oot_editor_config->value(QStringLiteral("Graphics/field_of_view"),
                                OOT3D::Camera::NativeFov).toFloat()));
    OOT3D::OrbitCamera::enabled.store(oot_editor_config->value(QStringLiteral("Camera/orbit"),true).toBool());
    OOT3D::OrbitCamera::smooth_collision_return.store(oot_editor_config->value(QStringLiteral("Camera/smooth_collision_return"),true).toBool());
    OOT3D::DialogueCamera::mm_style.store(oot_editor_config->value(QStringLiteral("Camera/mm_dialogue_camera"),true).toBool());
    OOT3D::DialogueCamera::ease_in_time.store(OOT3D::DialogueCamera::Sanitize(
        oot_editor_config->value(QStringLiteral("Camera/dialogue_ease_in_time"),OOT3D::DialogueCamera::DefaultEaseIn).toFloat(),
        OOT3D::DialogueCamera::DefaultEaseIn));
    OOT3D::DialogueCamera::ease_out_time.store(OOT3D::DialogueCamera::Sanitize(
        oot_editor_config->value(QStringLiteral("Camera/dialogue_ease_out_time"),OOT3D::DialogueCamera::DefaultEaseOut).toFloat(),
        OOT3D::DialogueCamera::DefaultEaseOut));
    OOT3D::OrbitCamera::distance_scale.store(OOT3D::OrbitCamera::SanitizeDistance(
        oot_editor_config->value(QStringLiteral("Camera/distance_scale"),OOT3D::OrbitCamera::DefaultDistance).toFloat()));
    OOT3D::OrbitCamera::invert_x.store(oot_editor_config->value(QStringLiteral("Camera/invert_x"),false).toBool());
    OOT3D::OrbitCamera::invert_y.store(oot_editor_config->value(QStringLiteral("Camera/invert_y"),false).toBool());
    OOT3D::OrbitCamera::sensitivity.store(OOT3D::OrbitCamera::Sanitize(
        oot_editor_config->value(QStringLiteral("Camera/sensitivity"),1.f).toFloat()));
    OOT3D::OrbitCamera::acceleration_time.store(OOT3D::OrbitCamera::SanitizeTime(
        oot_editor_config->value(QStringLiteral("Camera/acceleration_time"),
            OOT3D::OrbitCamera::DefaultAccelerationTime).toFloat(), OOT3D::OrbitCamera::DefaultAccelerationTime));
    OOT3D::OrbitCamera::deceleration_time.store(OOT3D::OrbitCamera::SanitizeTime(
        oot_editor_config->value(QStringLiteral("Camera/deceleration_time"),
            OOT3D::OrbitCamera::DefaultDecelerationTime).toFloat(), OOT3D::OrbitCamera::DefaultDecelerationTime));
    OOT3D::UserCamera::LoadConfig(*oot_editor_config);
    const auto& pause_names=OOT3D::PauseMenu::Names;
    OOT3D::PauseMenu::Layout pause_layout{};
    for(unsigned i=0;i<pause_layout.size();++i) {
        oot_editor_config->beginGroup(QStringLiteral("PauseScreen/")+QString::fromLatin1(pause_names[i]));
        const auto read=[&](const char* key,float fallback,float low,float high) {
            bool ok=false;float v=oot_editor_config->value(QString::fromLatin1(key),fallback).toFloat(&ok);
            return ok && std::isfinite(v)?std::clamp(v,low,high):fallback;
        };
        pause_layout[i]={read("x",0,-400,400),read("y",0,-240,240),read("scale",1,.1f,3)};
        if(OOT3D::PauseMenu::IsLowerGear(static_cast<OOT3D::PauseMenu::Part>(i)))
            pause_layout.gear_vertical[i-OOT3D::PauseMenu::GearBottomLeft]=
                oot_editor_config->value(QStringLiteral("vertical"),false).toBool();
        oot_editor_config->endGroup();
    }
    // Start the dungeon parchment/title at the user's existing overworld banner pose.
    for(auto part:{OOT3D::PauseMenu::DungeonLocationPaper,OOT3D::PauseMenu::DungeonLocationTitle}) {
        const QString prefix=QStringLiteral("PauseScreen/")+QString::fromLatin1(pause_names[part])+QStringLiteral("/");
        if(!oot_editor_config->contains(prefix+QStringLiteral("x"))) {
            const auto p=pause_layout[OOT3D::PauseMenu::LocationBanner];pause_layout[part]=p;
            oot_editor_config->setValue(prefix+QStringLiteral("x"),p.x);
            oot_editor_config->setValue(prefix+QStringLiteral("y"),p.y);
            oot_editor_config->setValue(prefix+QStringLiteral("scale"),p.scale);
        }
    }
    // Preserve existing sidebar placements once while separating their contents.
    for(auto [part,stone]:std::array{
            std::pair{OOT3D::PauseMenu::DungeonItems,OOT3D::PauseMenu::DungeonLeft},
            std::pair{OOT3D::PauseMenu::DungeonFloors,OOT3D::PauseMenu::DungeonRight}}) {
        const QString prefix=QStringLiteral("PauseScreen/")+QString::fromLatin1(pause_names[part])+QStringLiteral("/");
        if(!oot_editor_config->contains(prefix+QStringLiteral("x"))) {
            const auto p=pause_layout[stone];pause_layout[part]=p;
            oot_editor_config->setValue(prefix+QStringLiteral("x"),p.x);
            oot_editor_config->setValue(prefix+QStringLiteral("y"),p.y);
            oot_editor_config->setValue(prefix+QStringLiteral("scale"),p.scale);
        }
    }
    // Seed the new map controls from the former shared item/map pose once.
    // Persist immediately so later item edits cannot change the map on restart.
    if(!oot_editor_config->contains(QStringLiteral("PauseScreen/map/x"))) {
        const auto pose=pause_layout[OOT3D::PauseMenu::Items];
        pause_layout[OOT3D::PauseMenu::Map]=pose;
        oot_editor_config->setValue(QStringLiteral("PauseScreen/map/x"),pose.x);
        oot_editor_config->setValue(QStringLiteral("PauseScreen/map/y"),pose.y);
        oot_editor_config->setValue(QStringLiteral("PauseScreen/map/scale"),pose.scale);
    }
    pause_layout.horizontal=oot_editor_config->value(QStringLiteral("PauseScreen/horizontal"),false).toBool();
    pause_layout.stack_background=oot_editor_config->value(QStringLiteral("PauseScreen/stack_background"),false).toBool();
    const float header_y=oot_editor_config->value(QStringLiteral("PauseScreen/header_y"),0).toFloat();
    pause_layout.header_y=std::isfinite(header_y)?std::clamp(header_y,-240.f,240.f):0;
    const auto read_pause_float=[&](const QString& key,float fallback,float lo,float hi) {
        bool ok=false;const float v=oot_editor_config->value(key,fallback).toFloat(&ok);
        return ok && std::isfinite(v)?std::clamp(v,lo,hi):fallback;
    };
    pause_layout.tab_shadow.enabled=oot_editor_config->value(QStringLiteral("PauseScreen/tab_shadow/enabled"),true).toBool();
    pause_layout.tab_shadow.softness=read_pause_float(QStringLiteral("PauseScreen/tab_shadow/softness"),3,.5f,12);
    pause_layout.tab_shadow.strength=read_pause_float(QStringLiteral("PauseScreen/tab_shadow/strength"),.5f,0,1);
    pause_layout.tab_shadow.x=read_pause_float(QStringLiteral("PauseScreen/tab_shadow/x"),0,-12,12);
    pause_layout.tab_shadow.y=read_pause_float(QStringLiteral("PauseScreen/tab_shadow/y"),1,-12,12);
    pause_layout.map_darkness=read_pause_float(QStringLiteral("PauseScreen/map/darkness"),0,0,1);
    pause_layout.world_skulltula_x=read_pause_float(QStringLiteral("PauseScreen/map/skulltula_x"),0,-200,200);
    pause_layout.world_skulltula_y=read_pause_float(QStringLiteral("PauseScreen/map/skulltula_y"),0,-120,120);
    pause_layout.world_skulltula_scale=read_pause_float(QStringLiteral("PauseScreen/map/skulltula_scale"),1,.25f,3);
    pause_layout.world_map_time_tint=oot_editor_config->value(QStringLiteral("PauseScreen/map/time_of_day_tint"),false).toBool();
    pause_layout.hide_dungeon_floor_stones=oot_editor_config->value(QStringLiteral("PauseScreen/dungeon_floors/hide_stones"),false).toBool();
    pause_layout.dungeon_floor_growth.enabled=oot_editor_config->value(QStringLiteral("PauseScreen/dungeon_floors/grow_selected"),false).toBool();
    pause_layout.dungeon_floor_growth.size=read_pause_float(QStringLiteral("PauseScreen/dungeon_floors/selected_size"),1.25f,1,2);
    pause_layout.dungeon_floor_growth.speed=read_pause_float(QStringLiteral("PauseScreen/dungeon_floors/growth_speed"),1,.1f,5);
    pause_layout.dungeon_cursor_scale=read_pause_float(QStringLiteral("PauseScreen/dungeon_floors/cursor_scale"),1,.25f,3);
    pause_layout.dungeon_brightness=read_pause_float(QStringLiteral("PauseScreen/dungeon_map/brightness"),1,0,1);
    pause_layout.dungeon_spacing={read_pause_float(QStringLiteral("PauseScreen/dungeon_items/spacing"),1,.2f,3),
        read_pause_float(QStringLiteral("PauseScreen/dungeon_floors/spacing"),1,.2f,3)};
    pause_layout.background_darkness=read_pause_float(QStringLiteral("PauseScreen/background_darkness"),0,0,1);
    pause_layout.selection_grow_seconds=read_pause_float(QStringLiteral("PauseScreen/selection_grow_seconds"),0.f,0.f,1.f);
    pause_layout.transition_seconds=read_pause_float(QStringLiteral("PauseScreen/transition_seconds"),OOT3D::PauseMenu::DefaultTransitionSeconds,.02f,2.f);
    pause_layout.slot_content_scale=read_pause_float(QStringLiteral("PauseScreen/slot_content_scale"),1.f,.25f,2.f);
    pause_layout.slot_spacing=read_pause_float(QStringLiteral("PauseScreen/slot_spacing"),1.f,.25f,2.f);
    pause_layout.slot_content_x=read_pause_float(QStringLiteral("PauseScreen/slot_content_x"),0.f,-240.f,240.f);
    pause_layout.preview_rotation_speed=read_pause_float(QStringLiteral("PauseScreen/preview_rotation_speed"),1.f,.25f,5.f);
    OOT3D::BossChallenge::Layout boss_layout;
    for(unsigned i=0;i<OOT3D::BossChallenge::Count;++i) {
        const auto group=QStringLiteral("BossChallenge/")+QString::fromLatin1(OOT3D::BossChallenge::Names[i])+QStringLiteral("/");
        auto& p=boss_layout[i];
        p={read_pause_float(group+QStringLiteral("x"),p.x,-400,400),
            read_pause_float(group+QStringLiteral("y"),p.y,-240,240),
            read_pause_float(group+QStringLiteral("scale"),p.scale,.1f,3)};
        if(i==OOT3D::BossChallenge::Background || i==OOT3D::BossChallenge::Background2) {
            auto& darkness=boss_layout.background_darkness[i-OOT3D::BossChallenge::Background];
            darkness=read_pause_float(group+QStringLiteral("darkness"),darkness,0,1);
        }
    }
    boss_layout.corner_radius=read_pause_float(QStringLiteral("BossChallenge/preview/corner_radius"),0,0,32);
    boss_layout.panel_darkness=read_pause_float(QStringLiteral("BossChallenge/preview/darkness"),0,0,1);
    boss_layout.inner_shadow_width=read_pause_float(QStringLiteral("BossChallenge/border/inner_shadow_width"),4,0,24);
    boss_layout.inner_shadow_opacity=read_pause_float(QStringLiteral("BossChallenge/border/inner_shadow_opacity"),.35f,0,1);
    boss_layout.border_darkness=read_pause_float(QStringLiteral("BossChallenge/border/darkness"),0,0,1);
    boss_layout.border_enabled=oot_editor_config->value(QStringLiteral("BossChallenge/border/enabled"),true).toBool();
    boss_layout.border_bottom=oot_editor_config->value(QStringLiteral("BossChallenge/border/bottom"),true).toBool();
    boss_layout.shadow_opacity=read_pause_float(QStringLiteral("BossChallenge/preview/shadow_opacity"),boss_layout.shadow_opacity,0,1);
    boss_layout.shadow_softness=read_pause_float(QStringLiteral("BossChallenge/preview/shadow_softness"),boss_layout.shadow_softness,0,40);
    boss_layout.two_columns=oot_editor_config->value(QStringLiteral("BossChallenge/two_columns"),true).toBool();
    boss_layout.horizontal=oot_editor_config->value(QStringLiteral("BossChallenge/horizontal"),false).toBool();
    boss_layout.spacing_x=read_pause_float(QStringLiteral("BossChallenge/spacing_x"),4,-50,200);
    boss_layout.spacing_y=read_pause_float(QStringLiteral("BossChallenge/spacing_y"),2,-25,120);
    OOT3D::BossChallenge::SetLayout(boss_layout,false);
    OOT3D::DeathMenu::Layout death_layout{};
    for(unsigned i=0;i<OOT3D::DeathMenu::Count;++i) {
        const auto group=QStringLiteral("DeathMenu/")+QString::fromLatin1(OOT3D::DeathMenu::Names[i])+QStringLiteral("/");
        death_layout[i]={read_pause_float(group+QStringLiteral("x"),0,-400,400),
            read_pause_float(group+QStringLiteral("y"),0,-240,240),
            read_pause_float(group+QStringLiteral("scale"),1,.1f,3)};
    }
    OOT3D::DeathMenu::SetLayout(death_layout,false);
    OOT3D::SaveMenu::Layout save_layout;
    save_layout.darkness=read_pause_float(QStringLiteral("SaveMenu/darkness"),.45f,0,1);
    save_layout.hide_header=oot_editor_config->value(QStringLiteral("SaveMenu/hide_header"),false).toBool();
    for(unsigned i=0;i<OOT3D::SaveMenu::Count;++i) {
        const auto group=QStringLiteral("SaveMenu/")+QString::fromLatin1(OOT3D::SaveMenu::Names[i])+QStringLiteral("/");
        save_layout[i]={read_pause_float(group+QStringLiteral("x"),0,-400,400),
            read_pause_float(group+QStringLiteral("y"),0,-240,240),
            read_pause_float(group+QStringLiteral("scale"),1,.1f,3)};
    }
    for(unsigned i=0;i<2;++i) {
        const auto group=QStringLiteral("SaveMenu/prompts/%1/").arg(i);
        save_layout.prompts[i]={read_pause_float(group+QStringLiteral("x"),0,-400,400),
            read_pause_float(group+QStringLiteral("y"),0,-240,240),
            read_pause_float(group+QStringLiteral("scale"),1,.1f,3)};
    }
    OOT3D::SaveMenu::SetLayout(save_layout,false);
    OOT3D::FileSelect::Layout file_layout;
    file_layout.backgrounds_enabled=oot_editor_config->value(QStringLiteral("FileSelect/backgrounds/enabled"),true).toBool();
    for(unsigned i=0;i<file_layout.backgrounds.size();++i) {
        const auto group=QStringLiteral("FileSelect/backgrounds/")+QString::number(i)+QStringLiteral("/");
        auto& b=file_layout.backgrounds[i];
        b.pose={read_pause_float(group+QStringLiteral("x"),b.pose.x,-400,400),
            read_pause_float(group+QStringLiteral("y"),b.pose.y,-240,240),
            read_pause_float(group+QStringLiteral("scale"),b.pose.scale,.1f,3)};
        b.darkness=read_pause_float(group+QStringLiteral("darkness"),0,0,1);
    }
    file_layout.stones={read_pause_float(QStringLiteral("FileSelect/stones/x"),0,-400,400),
        read_pause_float(QStringLiteral("FileSelect/stones/y"),0,-240,240),
        read_pause_float(QStringLiteral("FileSelect/stones/scale"),1,.1f,4)};
    file_layout.stone_shadow_opacity=read_pause_float(QStringLiteral("FileSelect/stones/shadow_opacity"),.55f,0,1);
    file_layout.stone_spacing=read_pause_float(QStringLiteral("FileSelect/stones/spacing"),1,0,4);
    file_layout.stone_ease_out=read_pause_float(QStringLiteral("FileSelect/stones/ease_out"),2,0,4);
    file_layout.stone_focus_grow=oot_editor_config->value(QStringLiteral("FileSelect/stones/focus_grow"),false).toBool();
    file_layout.stone_focus_seconds=read_pause_float(QStringLiteral("FileSelect/stones/focus_seconds"),.1f,0,.3f);
    file_layout.seed_text={read_pause_float(QStringLiteral("FileSelect/seed_text/x"),0,-400,400),
        read_pause_float(QStringLiteral("FileSelect/seed_text/y"),0,-240,240),
        read_pause_float(QStringLiteral("FileSelect/seed_text/scale"),1,.1f,4)};
    file_layout.room_id_gap=read_pause_float(QStringLiteral("FileSelect/seed_text/room_id_gap"),3,0,60);
    file_layout.randomizer_badge={read_pause_float(QStringLiteral("FileSelect/randomizer_badge/x"),0,-400,400),
        read_pause_float(QStringLiteral("FileSelect/randomizer_badge/y"),0,-240,240),
        read_pause_float(QStringLiteral("FileSelect/randomizer_badge/scale"),1,.1f,4)};
    file_layout.randomizer_badge_color={
        read_pause_float(QStringLiteral("FileSelect/randomizer_badge/color_r"),1,0,1),
        read_pause_float(QStringLiteral("FileSelect/randomizer_badge/color_g"),.18f,0,1),
        read_pause_float(QStringLiteral("FileSelect/randomizer_badge/color_b"),.18f,0,1)};
    {
        const auto group=QStringLiteral("FileSelect/seed_text/shadow/");
        auto& s=file_layout.seed_shadow;
        s.enabled=oot_editor_config->value(group+QStringLiteral("enabled"),s.enabled).toBool();
        s.x=read_pause_float(group+QStringLiteral("x"),s.x,-400,400);
        s.y=read_pause_float(group+QStringLiteral("y"),s.y,-240,240);
        s.width=read_pause_float(group+QStringLiteral("width"),s.width,.1f,4);
        s.height=read_pause_float(group+QStringLiteral("height"),s.height,.1f,4);
        s.radius=read_pause_float(group+QStringLiteral("radius"),s.radius,0,80);
        s.softness=read_pause_float(group+QStringLiteral("softness"),s.softness,0,40);
        s.opacity=read_pause_float(group+QStringLiteral("opacity"),s.opacity,0,1);
    }
    for(unsigned i=0;i<8;++i) {
        const auto group=QStringLiteral("FileSelect/quest/")+QString::number(i)+QStringLiteral("/");
        auto& p=i<2?file_layout.quest_stones[i]:i==2?file_layout.quest_description:i==3?file_layout.randomizer_prompt:i==4?file_layout.join_prompt:i==5?file_layout.profile_join_prompt:i==6?file_layout.host_prompt:file_layout.player_count;
        // First use aligns Join above the user's existing Randomizer prompt;
        // subsequently its own saved pose remains independent.
        if(i==4)p=file_layout.randomizer_prompt;
        p={read_pause_float(group+QStringLiteral("x"),p.x,-400,400),
           read_pause_float(group+QStringLiteral("y"),p.y,-240,240),
           read_pause_float(group+QStringLiteral("scale"),p.scale,.1f,4)};
    }
    for(unsigned i=0;i<OOT3D::FileSelect::Count;++i) {
        const auto group=QStringLiteral("FileSelect/parts/")+QString::number(i)+QStringLiteral("/");
        file_layout.parts[i]={read_pause_float(group+QStringLiteral("x"),0,-800,800),
            read_pause_float(group+QStringLiteral("y"),0,-480,480),
            read_pause_float(group+QStringLiteral("scale"),1,.1f,4)};
    }
    for(unsigned i=0;i<file_layout.prompts.size();++i) {
        const auto group=QStringLiteral("FileSelect/")+QString::number(i)+QStringLiteral("/");
        auto& p=file_layout.prompts[i];
        p.x=read_pause_float(group+QStringLiteral("x"),p.x,-400,400);
        p.y=read_pause_float(group+QStringLiteral("y"),p.y,-240,240);
        p.scale=read_pause_float(group+QStringLiteral("scale"),p.scale,.1f,4);
    }
    // New relative anchors start aligned to the saved Select/Back prompts.
    // Old offsets were measured from a different fixed screen location.
    for(unsigned i=0;i<file_layout.name_prompts.size();++i) {
        const auto group=QStringLiteral("FileSelect/name_entry/prompt_offsets/")+QString::number(i)+QStringLiteral("/");
        file_layout.name_prompts[i]={read_pause_float(group+QStringLiteral("x"),0,-400,400),
            read_pause_float(group+QStringLiteral("y"),0,-240,240),
            read_pause_float(group+QStringLiteral("scale"),1,.1f,4)};
    }
    for(unsigned i=0;i<OOT3D::FileSelect::NameCount;++i) {
        const auto group=QStringLiteral("FileSelect/name_entry/")+QString::number(i)+QStringLiteral("/");
        file_layout.name_parts[i]={read_pause_float(group+QStringLiteral("x"),0,-400,400),
            read_pause_float(group+QStringLiteral("y"),0,-240,240),
            read_pause_float(group+QStringLiteral("scale"),1,.1f,4)};
    }
    {
        file_layout.name_box_edge_fade=read_pause_float(QStringLiteral("FileSelect/name_entry/edge_fade"),48,0,148);
        const auto group=QStringLiteral("FileSelect/name_entry/shadow/");
        auto& s=file_layout.keyboard_shadow;
        s.enabled=oot_editor_config->value(group+QStringLiteral("enabled"),s.enabled).toBool();
        s.opacity=read_pause_float(group+QStringLiteral("opacity"),s.opacity,0,1);
        s.softness=read_pause_float(group+QStringLiteral("softness"),s.softness,0,40);
        s.spread=read_pause_float(group+QStringLiteral("spread"),s.spread,-20,40);
        s.padding_top=read_pause_float(group+QStringLiteral("padding_top"),s.padding_top,-240,400);
        s.padding_left=read_pause_float(group+QStringLiteral("padding_left"),s.padding_left,-240,400);
        s.padding_bottom=read_pause_float(group+QStringLiteral("padding_bottom"),s.padding_bottom,-240,400);
        s.padding_right=read_pause_float(group+QStringLiteral("padding_right"),s.padding_right,-240,400);
        s.x=read_pause_float(group+QStringLiteral("x"),s.x,-80,80);
        s.y=read_pause_float(group+QStringLiteral("y"),s.y,-80,80);
    }
    {
        const auto group=QStringLiteral("FileSelect/name_entry/custom/");
        auto& k=file_layout.keyboard;
        k.enabled=oot_editor_config->value(group+QStringLiteral("enabled"),k.enabled).toBool();
        k.spacing_x=read_pause_float(group+QStringLiteral("spacing_x"),k.spacing_x,-10,24);
        k.spacing_y=read_pause_float(group+QStringLiteral("spacing_y"),k.spacing_y,-10,24);
        k.font_scale=read_pause_float(group+QStringLiteral("font_scale"),k.font_scale,.4f,2);
        k.selected_grow=oot_editor_config->value(group+QStringLiteral("selected_grow"),k.selected_grow).toBool();
        k.selected_growth=read_pause_float(group+QStringLiteral("selected_growth"),k.selected_growth,0,1);
        k.selected_growth_seconds=read_pause_float(group+QStringLiteral("selected_growth_seconds"),k.selected_growth_seconds,0,.5f);
        k.cursor_x=read_pause_float(group+QStringLiteral("cursor_x"),k.cursor_x,-32,32);
        k.cursor_y=read_pause_float(group+QStringLiteral("cursor_y"),k.cursor_y,-32,32);
    }
    {
        const auto group=QStringLiteral("FileSelect/vignette/");
        auto& v=file_layout.vignette;
        v.enabled=oot_editor_config->value(group+QStringLiteral("enabled"),v.enabled).toBool();
        v.strength=read_pause_float(group+QStringLiteral("strength"),v.strength,0,1);
        v.bottom_right_boost=read_pause_float(group+QStringLiteral("bottom_right_boost"),v.bottom_right_boost,0,4);
        v.width=read_pause_float(group+QStringLiteral("width"),v.width,.05f,1);
        v.height=read_pause_float(group+QStringLiteral("height"),v.height,.05f,1);
        v.softness=read_pause_float(group+QStringLiteral("softness"),v.softness,.05f,1);
    }
    {
        const auto group=QStringLiteral("FileSelect/particles/");
        auto& p=file_layout.particles;
        p.enabled=oot_editor_config->value(group+QStringLiteral("enabled"),p.enabled).toBool();
        p.density=std::clamp(oot_editor_config->value(group+QStringLiteral("density"),p.density).toInt(),0,64);
        p.glow=read_pause_float(group+QStringLiteral("glow"),p.glow,0,2);
        p.size=read_pause_float(group+QStringLiteral("size"),p.size,.25f,3);
        p.speed=read_pause_float(group+QStringLiteral("speed"),p.speed,0,3);
        p.light_strength=read_pause_float(group+QStringLiteral("light_strength"),p.light_strength,0,1);
        p.light_radius=read_pause_float(group+QStringLiteral("light_radius"),p.light_radius,.25f,3);
        p.spin=read_pause_float(group+QStringLiteral("spin"),p.spin,0,3);
        p.variations=oot_editor_config->value(group+QStringLiteral("variations"),p.variations).toBool();
    }
    OOT3D::FileSelect::SetLayout(file_layout,false);
    OOT3D::Ocarina::Layout ocarina_layout;
    for(unsigned i=0;i<OOT3D::Ocarina::Count;++i) {
        const auto group=QStringLiteral("Ocarina/")+QString::fromLatin1(OOT3D::Ocarina::Names[i])+QStringLiteral("/");
        auto& p=ocarina_layout[i];
        p.x=read_pause_float(group+QStringLiteral("x"),p.x,-400,400);
        p.y=read_pause_float(group+QStringLiteral("y"),p.y,-240,240);
        p.scale=read_pause_float(group+QStringLiteral("scale"),p.scale,.1f,4);
    }
    ocarina_layout.panel.right=read_pause_float(QStringLiteral("Ocarina/panel/right"),320,0,800);
    ocarina_layout.panel.opacity=read_pause_float(QStringLiteral("Ocarina/panel/opacity"),1,0,1);
    ocarina_layout.panel.border=read_pause_float(QStringLiteral("Ocarina/panel/border"),5,0,32);
    ocarina_layout.bottom_panels=oot_editor_config->value(QStringLiteral("Ocarina/bottom_panels"),false).toBool();
    ocarina_layout.two_columns=oot_editor_config->value(QStringLiteral("Ocarina/stones/two_columns"),false).toBool();
    ocarina_layout.row_spacing=read_pause_float(QStringLiteral("Ocarina/stones/row_spacing"),0,-40,200);
    ocarina_layout.column_spacing=read_pause_float(QStringLiteral("Ocarina/stones/column_spacing"),0,-68,200);
    ocarina_layout.stone_size=read_pause_float(QStringLiteral("Ocarina/stones/size"),1,.1f,4);
    ocarina_layout.prompt_vignette.enabled=oot_editor_config->value(QStringLiteral("Ocarina/prompt_vignette/enabled"),true).toBool();
    ocarina_layout.prompt_vignette.strength=read_pause_float(QStringLiteral("Ocarina/prompt_vignette/strength"),.55f,0,1);
    ocarina_layout.prompt_vignette.width=read_pause_float(QStringLiteral("Ocarina/prompt_vignette/width"),128,8,600);
    ocarina_layout.prompt_vignette.height=read_pause_float(QStringLiteral("Ocarina/prompt_vignette/height"),72,8,240);
    ocarina_layout.prompt_vignette.softness=read_pause_float(QStringLiteral("Ocarina/prompt_vignette/softness"),.9f,.05f,1);
    ocarina_layout.prompt_vignette.x=read_pause_float(QStringLiteral("Ocarina/prompt_vignette/x"),0,-400,400);
    ocarina_layout.prompt_vignette.y=read_pause_float(QStringLiteral("Ocarina/prompt_vignette/y"),0,-240,240);
    OOT3D::Ocarina::SetLayout(ocarina_layout,false);
    OOT3D::PausePrompts::Layout prompt_layout;
    prompt_layout.enabled=oot_editor_config->value(QStringLiteral("PausePrompts/enabled"),true).toBool();
    prompt_layout.scale=read_pause_float(QStringLiteral("PausePrompts/scale"),1,.1f,4);
    prompt_layout.icon_scale=read_pause_float(QStringLiteral("PausePrompts/icon_scale"),1,.1f,4);
    prompt_layout.letter_spacing=read_pause_float(QStringLiteral("PausePrompts/letter_spacing"),1,0,8);
    prompt_layout.navigation_distance=read_pause_float(QStringLiteral("PausePrompts/navigation_distance"),4,0,30);
    prompt_layout.navigation_speed=read_pause_float(QStringLiteral("PausePrompts/navigation_speed"),1,0,5);
    {
        auto& shadow=prompt_layout.map_navigation_shadow;
        const auto prefix=QStringLiteral("PausePrompts/map_navigation_shadow/");
        shadow.enabled=oot_editor_config->value(prefix+QStringLiteral("enabled"),true).toBool();
        shadow.x=read_pause_float(prefix+QStringLiteral("x"),0,-200,200);
        shadow.y=read_pause_float(prefix+QStringLiteral("y"),0,-240,240);
        shadow.width=read_pause_float(prefix+QStringLiteral("width"),64,8,200);
        shadow.height=read_pause_float(prefix+QStringLiteral("height"),100,8,240);
        shadow.opacity=read_pause_float(prefix+QStringLiteral("opacity"),.45f,0,1);
        shadow.softness=read_pause_float(prefix+QStringLiteral("softness"),.7f,.01f,1);
    }
    prompt_layout.lower.pose={read_pause_float(QStringLiteral("PausePrompts/lower/x"),0,-400,400),
        read_pause_float(QStringLiteral("PausePrompts/lower/y"),0,-240,240),
        read_pause_float(QStringLiteral("PausePrompts/lower/scale"),1,.1f,4)};
    prompt_layout.lower.spacing=read_pause_float(QStringLiteral("PausePrompts/lower/spacing"),8,-20,100);
    for(unsigned i=0;i<prompt_layout.prompts.size();++i) {
        const auto prefix=QStringLiteral("PausePrompts/")+
            QString::fromLatin1(OOT3D::PausePrompts::Names[i])+QStringLiteral("/");
        const auto read_pose=[&](const QString& group,OOT3D::PausePrompts::Pose& pose) {
            pose={read_pause_float(group+QStringLiteral("x"),0,-400,400),
                read_pause_float(group+QStringLiteral("y"),0,-240,240),
                read_pause_float(group+QStringLiteral("scale"),1,.1f,4)};
        };
        read_pose(prefix,prompt_layout.prompts[i].group);
        read_pose(prefix+QStringLiteral("button/"),prompt_layout.prompts[i].button);
    }
    OOT3D::PausePrompts::SetLayout(prompt_layout,false);
    pause_layout.map_frame={
        oot_editor_config->value(QStringLiteral("PauseScreen/map_frame/enabled"),true).toBool(),
        read_pause_float(QStringLiteral("PauseScreen/map_frame/x"),0,-2000,2000),
        read_pause_float(QStringLiteral("PauseScreen/map_frame/y"),0,-2000,2000),
        read_pause_float(QStringLiteral("PauseScreen/map_frame/width"),1,.25f,10),
        read_pause_float(QStringLiteral("PauseScreen/map_frame/height"),1,.25f,10),
        read_pause_float(QStringLiteral("PauseScreen/map_frame/left"),0,-2000,2000),
        read_pause_float(QStringLiteral("PauseScreen/map_frame/right"),0,-2000,2000),
        read_pause_float(QStringLiteral("PauseScreen/map_frame/top"),0,-2000,2000),
        read_pause_float(QStringLiteral("PauseScreen/map_frame/bottom"),0,-2000,2000)};
    for(unsigned i=0;i<pause_layout.map_frame.pieces.size();++i) {
        const QString prefix=QStringLiteral("PauseScreen/map_frame/piece_%1/").arg(i);
        pause_layout.map_frame.pieces[i]={
            read_pause_float(prefix+QStringLiteral("x"),0,-2000,2000),
            read_pause_float(prefix+QStringLiteral("y"),0,-2000,2000),
            read_pause_float(prefix+QStringLiteral("scale"),1,.05f,10)};
    }
    pause_layout.map_frame.cut_x=read_pause_float(QStringLiteral("PauseScreen/map_frame/cut_x"),0,-2000,2000);
    pause_layout.map_frame.cut_y=read_pause_float(QStringLiteral("PauseScreen/map_frame/cut_y"),0,-2000,2000);
    pause_layout.map_frame.cut_scale=read_pause_float(QStringLiteral("PauseScreen/map_frame/cut_scale"),1,.05f,10);
    pause_layout.map_frame.darkness=read_pause_float(QStringLiteral("PauseScreen/map_frame/darkness"),0,0,1);
    pause_layout.map_extension={
        oot_editor_config->value(QStringLiteral("PauseScreen/map/extension_enabled"),false).toBool(),
        read_pause_float(QStringLiteral("PauseScreen/map/extension_width"),80,0,400),
        read_pause_float(QStringLiteral("PauseScreen/map/extension_blur"),18,0,40),
        read_pause_float(QStringLiteral("PauseScreen/map/extension_darkness"),0,0,1)};
    pause_layout.map_side_shadows=oot_editor_config->value(QStringLiteral("PauseScreen/map/side_shadows"),false).toBool();
    pause_layout.map_shadow={read_pause_float(QStringLiteral("PauseScreen/map/shadow_width"),1,.1f,4),
        read_pause_float(QStringLiteral("PauseScreen/map/shadow_opacity"),.45f,0,1)};
    for(unsigned i=0;i<pause_layout.map_backgrounds.size();++i) {
        const QString prefix=QStringLiteral("PauseScreen/map_background_%1/").arg(i);
        auto& b=pause_layout.map_backgrounds[i];
        b.pose={read_pause_float(prefix+QStringLiteral("x"),b.pose.x,-400,400),
            read_pause_float(prefix+QStringLiteral("y"),0,-240,240),
            read_pause_float(prefix+QStringLiteral("scale"),1,.1f,3)};
        b.darkness=read_pause_float(prefix+QStringLiteral("darkness"),0,0,1);
    }
    pause_layout.world_parchment_blend=read_pause_float(QStringLiteral("PauseScreen/world_parchments/blend"),18,0,80);
    pause_layout.world_parchment_darkness=read_pause_float(QStringLiteral("PauseScreen/world_parchments/darkness"),0,0,1);
    pause_layout.world_map_outline=oot_editor_config->value(QStringLiteral("PauseScreen/world_parchments/outline"),true).toBool();
    pause_layout.world_map_shadow_opacity=read_pause_float(QStringLiteral("PauseScreen/world_parchments/shadow_opacity"),.35f,0,1);
    pause_layout.world_map_shadow_width=read_pause_float(QStringLiteral("PauseScreen/world_parchments/shadow_width"),8,0,48);
    pause_layout.world_map_top_fade.enabled=oot_editor_config->value(QStringLiteral("PauseScreen/world_parchments/top_fade/enabled"),false).toBool();
    pause_layout.world_map_top_fade.opacity=read_pause_float(QStringLiteral("PauseScreen/world_parchments/top_fade/opacity"),.65f,0,1);
    pause_layout.world_map_top_fade.height=read_pause_float(QStringLiteral("PauseScreen/world_parchments/top_fade/height"),48,1,240);
    pause_layout.world_map_top_fade.y=read_pause_float(QStringLiteral("PauseScreen/world_parchments/top_fade/y"),0,-240,240);
    pause_layout.world_map_top_fade.softness=read_pause_float(QStringLiteral("PauseScreen/world_parchments/top_fade/softness"),1,.2f,4);
    for(unsigned c=0;c<3;++c)
        pause_layout.world_map_top_fade.color[c]=read_pause_float(QStringLiteral("PauseScreen/world_parchments/top_fade/color_%1").arg(c),0,0,1);
    for(unsigned i=0;i<pause_layout.world_parchments.size();++i) {
        const QString prefix=QStringLiteral("PauseScreen/world_parchments/%1/").arg(i);
        auto& stack=pause_layout.world_parchments[i];
        stack.enabled=oot_editor_config->value(prefix+QStringLiteral("enabled"),true).toBool();
        stack.pose={read_pause_float(prefix+QStringLiteral("x"),stack.pose.x,-400,400),
            read_pause_float(prefix+QStringLiteral("y"),0,-240,240),
            read_pause_float(prefix+QStringLiteral("scale"),1,.1f,3)};
        stack.spacing=read_pause_float(prefix+QStringLiteral("spacing"),88,0,240);
    }
    pause_layout.parchment_blend=read_pause_float(QStringLiteral("PauseScreen/parchments/blend"),18,0,80);
    for(unsigned i=0;i<3;++i) {
        const QString prefix=QStringLiteral("PauseScreen/parchments/%1/").arg(i+2);
        auto& paper=pause_layout.dungeon_parchments[i];
        paper.enabled=oot_editor_config->value(prefix+QStringLiteral("enabled"),false).toBool();
        paper.pose={read_pause_float(prefix+QStringLiteral("x"),0,-400,400),
            read_pause_float(prefix+QStringLiteral("y"),0,-240,240),
            read_pause_float(prefix+QStringLiteral("scale"),1,.1f,3)};
    }
    {
        const QString prefix=QStringLiteral("PauseScreen/dungeon_floor_line/");
        auto& line=pause_layout.dungeon_floor_line;
        line.enabled=oot_editor_config->value(prefix+QStringLiteral("enabled"),true).toBool();
        line.pose={read_pause_float(prefix+QStringLiteral("x"),0,-400,400),
            read_pause_float(prefix+QStringLiteral("y"),0,-240,240),
            read_pause_float(prefix+QStringLiteral("scale"),1,.1f,3)};
        line.width=read_pause_float(prefix+QStringLiteral("width"),4,0,80);
        line.height=read_pause_float(prefix+QStringLiteral("height"),184,0,600);
        line.opacity=read_pause_float(prefix+QStringLiteral("opacity"),.8f,0,1);
        line.roundness=read_pause_float(prefix+QStringLiteral("roundness"),1,0,1);
    }
    pause_layout.dungeon_edge_shadows=oot_editor_config->value(QStringLiteral("PauseScreen/dungeon_map/edge_shadows"),true).toBool();
    {
        const auto prefix=QStringLiteral("PauseScreen/dungeon_map/top_shadow/");
        auto& s=pause_layout.dungeon_top_shadow;
        s.enabled=oot_editor_config->value(prefix+QStringLiteral("enabled"),true).toBool();
        s.height=read_pause_float(prefix+QStringLiteral("height"),32,0,240);
        s.opacity=read_pause_float(prefix+QStringLiteral("opacity"),.65f,0,1);
        s.y=read_pause_float(prefix+QStringLiteral("y"),0,-240,240);
    }
    for(unsigned side=0;side<2;++side) {
        const QString prefix=QStringLiteral("PauseScreen/dungeon_shadow_%1/").arg(side);
        auto& b=pause_layout.dungeon_blocks[side];
        b.enabled=oot_editor_config->value(prefix+QStringLiteral("enabled"),true).toBool();
        b.x=read_pause_float(prefix+QStringLiteral("x"),0,-600,600);
        b.y=read_pause_float(prefix+QStringLiteral("y"),0,-600,600);
        b.width=read_pause_float(prefix+QStringLiteral("width"),72,0,600);
        b.height=read_pause_float(prefix+QStringLiteral("height"),240,0,600);
        b.fade=read_pause_float(prefix+QStringLiteral("fade"),40,0,600);
        b.opacity=read_pause_float(prefix+QStringLiteral("opacity"),.65f,0,1);
        pause_layout.dungeon_background_darkness[side]=read_pause_float(
            QStringLiteral("PauseScreen/dungeon_map/background_darkness_%1").arg(side),pause_layout.map_backgrounds[side].darkness,0,1);
    }
    {
        auto& p=pause_layout.dungeon_particles;
        const QString key=QStringLiteral("PauseScreen/dungeon_map/particles/");
        pause_layout.dungeon_particles_progress=oot_editor_config->value(key+QStringLiteral("progress"),true).toBool();
        p.enabled=oot_editor_config->value(key+QStringLiteral("enabled"),false).toBool();
        p.density=std::clamp(oot_editor_config->value(key+QStringLiteral("density"),24).toInt(),0,int(OOT3D::FileParticles::DungeonMaxParticles));
        p.glow=read_pause_float(key+QStringLiteral("glow"),.7f,0,2);
        p.size=read_pause_float(key+QStringLiteral("size"),1,.25f,3);
        p.speed=read_pause_float(key+QStringLiteral("speed"),1,0,3);
        auto& endpoints=pause_layout.dungeon_particle_endpoints;
        endpoints.density_start=std::clamp(oot_editor_config->value(key+QStringLiteral("density_start"),32).toInt(),0,int(OOT3D::FileParticles::DungeonMaxParticles));
        endpoints.density_end=std::clamp(oot_editor_config->value(key+QStringLiteral("density_end"),192).toInt(),0,int(OOT3D::FileParticles::DungeonMaxParticles));
        endpoints.drift_start=read_pause_float(key+QStringLiteral("drift_start"),p.speed,0,6);
        endpoints.drift_end=read_pause_float(key+QStringLiteral("drift_end"),2*p.speed,0,6);
        p.spin=read_pause_float(key+QStringLiteral("spin"),1,0,3);
        p.light_strength=read_pause_float(key+QStringLiteral("light_strength"),.35f,0,1);
        p.light_radius=read_pause_float(key+QStringLiteral("light_radius"),1,.25f,3);
        p.variations=oot_editor_config->value(key+QStringLiteral("variations"),true).toBool();
    }
    {
        auto& fade=pause_layout.dungeon_map_top_fade;
        const QString key=QStringLiteral("PauseScreen/dungeon_map/top_fade/");
        fade.enabled=oot_editor_config->value(key+QStringLiteral("enabled"),false).toBool();
        fade.opacity=read_pause_float(key+QStringLiteral("opacity"),.65f,0,1);
        fade.height=read_pause_float(key+QStringLiteral("height"),48,1,240);
        fade.y=read_pause_float(key+QStringLiteral("y"),0,-240,240);
        fade.softness=read_pause_float(key+QStringLiteral("softness"),1,.2f,4);
        for(unsigned c=0;c<3;++c)
            fade.color[c]=read_pause_float(key+QStringLiteral("color_%1").arg(c),0,0,1);
    }
    {
        auto& v=pause_layout.dungeon_prompt_vignette;
        const QString key=QStringLiteral("PauseScreen/dungeon_map/prompt_vignette/");
        v.enabled=oot_editor_config->value(key+QStringLiteral("enabled"),true).toBool();
        v.strength=read_pause_float(key+QStringLiteral("strength"),.55f,0,1);
        v.width=read_pause_float(key+QStringLiteral("width"),128,8,600);
        v.height=read_pause_float(key+QStringLiteral("height"),72,8,240);
        v.softness=read_pause_float(key+QStringLiteral("softness"),.9f,.05f,1);
        v.x=read_pause_float(key+QStringLiteral("x"),0,-400,400);
        v.y=read_pause_float(key+QStringLiteral("y"),0,-240,240);
    }
    {
        auto& v=pause_layout.world_prompt_vignette;
        const QString key=QStringLiteral("PauseScreen/map/prompt_vignette/");
        v.enabled=oot_editor_config->value(key+QStringLiteral("enabled"),false).toBool();
        v.strength=read_pause_float(key+QStringLiteral("strength"),.55f,0,1);
        v.width=read_pause_float(key+QStringLiteral("width"),128,8,600);
        v.height=read_pause_float(key+QStringLiteral("height"),72,8,240);
        v.softness=read_pause_float(key+QStringLiteral("softness"),.9f,.05f,1);
        v.x=read_pause_float(key+QStringLiteral("x"),0,-400,400);
        v.y=read_pause_float(key+QStringLiteral("y"),0,-240,240);
        for(unsigned c=0;c<3;++c)
            v.color[c]=read_pause_float(key+QStringLiteral("color_%1").arg(c),0,0,1);
    }
    pause_layout.gear_panel={
        read_pause_float(QStringLiteral("PauseScreen/gear_panel/x"),0,-400,400),
        read_pause_float(QStringLiteral("PauseScreen/gear_panel/y"),0,-240,240),
        read_pause_float(QStringLiteral("PauseScreen/gear_panel/width"),1,.1f,3),
        read_pause_float(QStringLiteral("PauseScreen/gear_panel/height"),1,.05f,3)};
    if(oot_editor_config->contains(QStringLiteral("PauseScreen/gear_panel_lower/width"))) {
        pause_layout.gear_panel_lower={
            read_pause_float(QStringLiteral("PauseScreen/gear_panel_lower/x"),0,-400,400),
            read_pause_float(QStringLiteral("PauseScreen/gear_panel_lower/y"),37.6f,-240,240),
            read_pause_float(QStringLiteral("PauseScreen/gear_panel_lower/width"),1,.1f,3),
            read_pause_float(QStringLiteral("PauseScreen/gear_panel_lower/height"),.5f,.05f,3)};
    } else {
        // Preserve the edited panel's outer bounds when splitting it in two.
        const auto panels=OOT3D::PauseMenu::SplitGearPanel(pause_layout.gear_panel);
        pause_layout.gear_panel=panels[0];pause_layout.gear_panel_lower=panels[1];
    }
    pause_layout.arrow_shadow.enabled=oot_editor_config->value(QStringLiteral("PauseScreen/arrow_shadow/enabled"),true).toBool();
    pause_layout.arrow_shadow.softness=read_pause_float(QStringLiteral("PauseScreen/arrow_shadow/softness"),8,1,24);
    pause_layout.arrow_shadow.x=read_pause_float(QStringLiteral("PauseScreen/arrow_shadow/x"),0,-12,12);
    pause_layout.arrow_shadow.y=read_pause_float(QStringLiteral("PauseScreen/arrow_shadow/y"),2,-12,12);
    pause_layout.arrow_shadow.strength=read_pause_float(QStringLiteral("PauseScreen/arrow_shadow/strength"),.45f,0,1);
    for(auto [name,shadow]:std::array{
            std::pair{"header_shadow",&pause_layout.header_shadow},
            std::pair{"slots_shadow",&pause_layout.slots_shadow},
            std::pair{"background_shadow",&pause_layout.background_shadow},
            std::pair{"items_shadow",&pause_layout.items_shadow},
            std::pair{"gear_upper_shadow",&pause_layout.gear_shadows[0]},
            std::pair{"gear_lower_shadow",&pause_layout.gear_shadows[1]},
            std::pair{"dungeon_edge_shadow",&pause_layout.dungeon_edge_shadow},
            std::pair{"dungeon_left_shadow",&pause_layout.dungeon_left_shadow},
            std::pair{"dungeon_right_shadow",&pause_layout.dungeon_right_shadow}}) {
        const QString prefix=QStringLiteral("PauseScreen/")+QString::fromLatin1(name)+QStringLiteral("/");
        shadow->width=read_pause_float(prefix+QStringLiteral("width"),1,.1f,4);
        shadow->opacity=read_pause_float(prefix+QStringLiteral("opacity"),.45f,0,1);
    }
    for(auto [name,overlay]:std::array{
            std::pair{"header_overlay",&pause_layout.header_overlay},
            std::pair{"slots_overlay",&pause_layout.slots_overlay},
            std::pair{"dungeon_left_overlay",&pause_layout.dungeon_left_overlay},
            std::pair{"dungeon_right_overlay",&pause_layout.dungeon_right_overlay}}) {
        const QString prefix=QStringLiteral("PauseScreen/")+QString::fromLatin1(name)+QStringLiteral("/");
        constexpr std::array channels{"red","green","blue"};
        for(unsigned c=0;c<3;++c)
            overlay->color[c]=read_pause_float(prefix+QString::fromLatin1(channels[c]),1,0,1);
        overlay->opacity=read_pause_float(prefix+QStringLiteral("opacity"),0,0,1);
    }
    OOT3D::PauseMenu::LoadColorLayouts(*oot_editor_config,pause_layout);
    OOT3D::PauseMenu::SetLayout(pause_layout,false);
    OOT3D::settings_size.store(std::clamp(
        oot_editor_config->value(QStringLiteral("General/settings_size"), 0).toInt(), -5, 10));
    OOT3D::StartupHint::enabled=oot_editor_config->value(
        QStringLiteral("General/show_startup_settings_hint"),true).toBool();
    constexpr std::array pose_names{"logo", "copyright", "press_start", "press_start_extra"};
    auto title_layout = OOT3D::TitleScreen::DefaultLayout;
    for (unsigned i = 0; i < title_layout.size(); ++i) {
        oot_editor_config->beginGroup(QStringLiteral("TitleScreen/") + QString::fromLatin1(pose_names[i]));
        const auto read = [&](const char* key, float fallback, float low, float high) {
            bool ok = false;
            const float value = oot_editor_config->value(QString::fromLatin1(key), fallback).toFloat(&ok);
            return ok && std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        title_layout[i] = {read("x", title_layout[i].x, -400, 400),
                           read("y", title_layout[i].y, -240, 240),
                           read("scale", title_layout[i].scale, .1f, 3)};
        if(i==3) {
            title_layout.text_color={read("color_r",1,0,1),read("color_g",1,0,1),read("color_b",1,0,1)};
            title_layout.letter_spacing=read("letter_spacing",2,-4,12);
        }
        oot_editor_config->endGroup();
    }
    OOT3D::TitleScreen::SetLayout(title_layout, false);
    auto hud_layout=OOT3D::HUDDefaults::Basic();
    constexpr std::array hud_names{"hearts", "rupees", "magic", "a_button", "loading_icon", "minimap", "keys", "timer", "minigame", "shard_agony", "target_indicator", "carrots"};
    for (unsigned i = 0; i < hud_layout.size(); ++i) {
        oot_editor_config->beginGroup(QStringLiteral("HUD/") + QString::fromLatin1(hud_names[i]));
        const auto read = [&](const char* key, float fallback, float low, float high) {
            bool ok = false;
            const float value = oot_editor_config->value(QString::fromLatin1(key), fallback).toFloat(&ok);
            return ok && std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        hud_layout[i] = {read("x", hud_layout[i].x, -400, 400), read("y", hud_layout[i].y, -240, 240), read("scale", hud_layout[i].scale, .1f, 3)};
        if (i == OOT3D::HUD::Minimap) hud_layout.minimap_lock_corner=oot_editor_config->value(QStringLiteral("lock_corner"),true).toBool();
        if (i == OOT3D::HUD::Rupees) {
            hud_layout.hd_rupee_numbers=oot_editor_config->value(QStringLiteral("hd_numbers"),true).toBool();
            hud_layout.rupee_number_gap=read("number_gap",0,-100,100);
            hud_layout.hd_rupee_text={read("hd_text_x",0,-100,100),read("hd_text_y",0,-100,100),read("hd_text_scale",1,.1f,3)};
            hud_layout.hd_rupee_spacing=read("hd_digit_spacing",0,-8,24);
            hud_layout.hd_rupee_shadow={oot_editor_config->value(QStringLiteral("hd_shadow_enabled"),true).toBool(),
                read("hd_shadow_x",.75f,-3,3),read("hd_shadow_y",.75f,-3,3),
                read("hd_shadow_blur",0,0,2),read("hd_shadow_strength",.8f,0,1)};
        }
        if (i == OOT3D::HUD::Hearts) hud_layout.heart_spacing = read("spacing", hud_layout.heart_spacing, -8, 20);
        if (i == OOT3D::HUD::Timer) {
            hud_layout.hd_timer_numbers=oot_editor_config->value(QStringLiteral("hd_numbers"),true).toBool();
            hud_layout.hd_timer_text={read("hd_text_x",0,-100,100),read("hd_text_y",0,-100,100),read("hd_text_scale",1,.1f,3)};
        }
        if (i == OOT3D::HUD::Carrots) {
            hud_layout.carrot_opacity = read("opacity", 1, 0, 1);
            hud_layout.carrot_spacing = read("spacing", 0, -20, 40);
            hud_layout.carrot_animation_delay = read("animation_delay", 0, 0, 2);
            hud_layout.carrot_shadow={oot_editor_config->value(QStringLiteral("shadow_enabled"),false).toBool(),
                read("shadow_x",1.5f,-12,12),read("shadow_y",2,-12,12),
                read("shadow_blur",1,0,4),read("shadow_strength",.6f,0,1)};
        }
        if (i == OOT3D::HUD::Magic) {
            hud_layout.magic_height=read("height",hud_layout.magic_height,1,8);
            hud_layout.magic_border=read("border",hud_layout.magic_border,1,4);
            hud_layout.magic_background_opacity=read("background_opacity",hud_layout.magic_background_opacity,0,1);
        }
        oot_editor_config->endGroup();
    }
    OOT3D::HUD::LoadHDNumbers(*oot_editor_config,hud_layout);
    const bool upgrade_minimap=oot_editor_config->contains(QStringLiteral("HUD/minimap/x")) && oot_editor_config->value(QStringLiteral("HUD/minimap/anchor_version"),0).toInt()<2;
    if(upgrade_minimap)
        OOT3D::HUD::RecoverLegacyMinimap(hud_layout[OOT3D::HUD::Minimap],hud_layout.minimap_lock_corner);
    const auto map_fallback=oot_editor_config->contains(QStringLiteral("HUD/minimap/x"))?
        hud_layout[OOT3D::HUD::Minimap]:hud_layout.outside_minimap;
    hud_layout.outside_minimap={
        read_pause_float(QStringLiteral("HUD/minimap/outside/x"),map_fallback.x,-400,400),
        read_pause_float(QStringLiteral("HUD/minimap/outside/y"),map_fallback.y,-240,240),
        read_pause_float(QStringLiteral("HUD/minimap/outside/scale"),map_fallback.scale,.1f,3)};
    for(unsigned side=0;side<2;++side) {
        const QString group=QStringLiteral("HUD/shop_prompts/")+(side?QStringLiteral("right/"):QStringLiteral("left/"));
        hud_layout.shop_prompt_offsets[side]={read_pause_float(group+QStringLiteral("x"),0,-400,400),
            read_pause_float(group+QStringLiteral("y"),0,-240,240)};
    }
    hud_layout.navi_text={read_pause_float(QStringLiteral("HUD/navi_text/x"),0,-400,400),
        read_pause_float(QStringLiteral("HUD/navi_text/y"),0,-240,240),1};
    OOT3D::UserHUD::LoadAuthoredTitles(*oot_editor_config,hud_layout);
    OOT3D::HUD::SetLayout(hud_layout, upgrade_minimap);
    auto button_layout=OOT3D::HUDDefaults::Buttons();
    button_layout.minigame_b_x=read_pause_float(QStringLiteral("HUD/buttons/b/minigame_x"),0,-100,100);
    button_layout.b_ammo={read_pause_float(QStringLiteral("HUD/buttons/b/ammo/x"),0,-100,100),
        read_pause_float(QStringLiteral("HUD/buttons/b/ammo/y"),0,-100,100),
        read_pause_float(QStringLiteral("HUD/buttons/b/ammo/scale"),1,.1f,3)};
    button_layout.show_items=oot_editor_config->value(QStringLiteral("HUD/buttons/show_items"),true).toBool();
    button_layout.item_x=read_pause_float(QStringLiteral("HUD/buttons/item_x"),button_layout.item_x,-400,400);
    button_layout.item_y=read_pause_float(QStringLiteral("HUD/buttons/item_y"),button_layout.item_y,-240,240);
    button_layout.item_scale=read_pause_float(QStringLiteral("HUD/buttons/item_scale"),button_layout.item_scale,.1f,4);
    button_layout.diamond_rotation=read_pause_float(QStringLiteral("HUD/buttons/diamond/rotation"),button_layout.diamond_rotation,-180,180);
    button_layout.diamond_spacing=read_pause_float(QStringLiteral("HUD/buttons/diamond/spacing"),button_layout.diamond_spacing,0,150);
    button_layout.item_labels.x=read_pause_float(QStringLiteral("HUD/buttons/labels/x"),button_layout.item_labels.x,-400,400);
    button_layout.item_labels.y=read_pause_float(QStringLiteral("HUD/buttons/labels/y"),button_layout.item_labels.y,-240,240);
    button_layout.item_labels.scale=read_pause_float(QStringLiteral("HUD/buttons/labels/scale"),button_layout.item_labels.scale,.1f,4);
    button_layout.item_icons.x=read_pause_float(QStringLiteral("HUD/buttons/items/x"),button_layout.item_icons.x,-400,400);
    button_layout.item_icons.y=read_pause_float(QStringLiteral("HUD/buttons/items/y"),button_layout.item_icons.y,-240,240);
    button_layout.item_icons.scale=read_pause_float(QStringLiteral("HUD/buttons/items/scale"),button_layout.item_icons.scale,.1f,4);
    button_layout.counter_scale=read_pause_float(QStringLiteral("HUD/buttons/counts/scale"),button_layout.counter_scale,.1f,4);
    button_layout.counter_x=read_pause_float(QStringLiteral("HUD/buttons/counts/x"),0,-400,400);
    button_layout.counter_y=read_pause_float(QStringLiteral("HUD/buttons/counts/y"),0,-240,240);
    for(unsigned i=0;i<OOT3D::HUDButtons::Count;++i) {
        const auto group=QStringLiteral("HUD/buttons/")+QString::fromLatin1(OOT3D::HUDButtons::Names[i])+QStringLiteral("/");
        auto& b=button_layout.buttons[i];
        b.enabled=oot_editor_config->value(group+QStringLiteral("enabled"),true).toBool();
        b.pose.x=read_pause_float(group+QStringLiteral("x"),b.pose.x,-400,400);
        b.pose.y=read_pause_float(group+QStringLiteral("y"),b.pose.y,-240,240);
        b.pose.scale=read_pause_float(group+QStringLiteral("scale"),b.pose.scale,.1f,4);
    }
    // Convert the previous row once, centered on the user's current buttons.
    // Subsequent loads preserve any individual positioning adjustments.
    const bool arrange_buttons=oot_editor_config->contains(QStringLiteral("HUD/buttons/x/x")) && oot_editor_config->value(QStringLiteral("HUD/buttons/diamond/version"),0).toUInt()<1;
    if(arrange_buttons)OOT3D::HUDButtons::ArrangeDiamond(button_layout);
    OOT3D::HUDButtons::SetLayout(button_layout,arrange_buttons);
    auto dpad_layout=OOT3D::HUDDefaults::DPadLayout();
    dpad_layout.enabled=oot_editor_config->value(QStringLiteral("HUD/dpad/enabled"),true).toBool();
    dpad_layout.navi_pulse_speed=oot_editor_config->value(QStringLiteral("HUD/dpad/navi_pulse_speed"),1.f).toFloat();
    const auto read_dpad_pose=[&](const QString& name,OOT3D::DPad::Pose& pose) {
        oot_editor_config->beginGroup(QStringLiteral("HUD/dpad/")+name);
        const auto read=[&](const char* key,float fallback,float low,float high) {
            bool ok=false;const float v=oot_editor_config->value(QString::fromLatin1(key),fallback).toFloat(&ok);
            return ok && std::isfinite(v)?std::clamp(v,low,high):fallback;
        };
        pose={read("x",pose.x,-400,400),read("y",pose.y,-240,240),
              read("scale_x",pose.scale_x,.1f,4),read("scale_y",pose.scale_y,.1f,4)};
        oot_editor_config->endGroup();
    };
    read_dpad_pose(QStringLiteral("group"),dpad_layout.group);
    for(unsigned i=0;i<OOT3D::DPad::ElementCount;++i)
        read_dpad_pose(QString::fromLatin1(OOT3D::DPad::Names[i]),dpad_layout.icons[i]);
    OOT3D::DPad::SetLayout(dpad_layout,false);
    OOT3D::UserHUD::LoadConfig(*oot_editor_config);
    const bool interpolation_enabled=oot_editor_config->value(QStringLiteral("Interpolation/enabled"),true).toBool();
    OOT3D::Interpolation::cpu_vertices=oot_editor_config->value(QStringLiteral("Interpolation/cpu_vertices"),OOT3D::IsPlayerBuild()).toBool();
    OOT3D::PlayerGraphics::disable_fog=oot_editor_config->value(QStringLiteral("Graphics/disable_fog"),false).toBool();
    OOT3D::Interpolation::present_60=interpolation_enabled;
    OOT3D::Interpolation::enabled=interpolation_enabled;
    if(interpolation_enabled) {
        OOT3D::PlayerGraphics::set_hardware=!OOT3D::Interpolation::cpu_vertices.load();
        OOT3D::PlayerGraphics::ApplyPending(Settings::values, [] {}, [&] { config->Save(); });
    }
    OOT3D::Interpolation::target_fps=std::clamp(oot_editor_config->value(QStringLiteral("Interpolation/target_fps"),60).toInt(),30,120);
    OOT3D::Interpolation::match_refresh_rate=oot_editor_config->value(QStringLiteral("Interpolation/match_refresh_rate"),false).toBool();
    oot_editor_config->beginGroup(QStringLiteral("Textboxes"));
    const auto read_textbox = [&](const char* key, float fallback, float low, float high) {
        bool ok = false;
        const float value = oot_editor_config->value(QString::fromLatin1(key), fallback).toFloat(&ok);
        return ok && std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    };
    OOT3D::Textboxes::SetLayout({read_textbox("x", 0, -400, 400),
        read_textbox("y", 0, -240, 240), read_textbox("width", 1, 1, 3),
        {read_textbox("indicator_x", 0, -400, 400), read_textbox("indicator_y", 0, -240, 240),
         read_textbox("indicator_scale", 1, .1f, 3)}, read_textbox("scale", 1, .1f, 3), read_textbox("sign_y", 0, -240, 240),
        read_textbox("opening_speed", 1, .25f, 3),
        oot_editor_config->value(QStringLiteral("mm_close"),false).toBool(),
        {oot_editor_config->value(QStringLiteral("indicator_visible"),true).toBool(),
         oot_editor_config->value(QStringLiteral("indicator_spring"),true).toBool(),
         read_textbox("indicator_small",.72f,.2f,1),read_textbox("indicator_big",1.18f,1,1.8f),
         read_textbox("indicator_speed",1,.25f,3)},
        oot_editor_config->value(QStringLiteral("smooth_reveal"),true).toBool()}, false);
    oot_editor_config->endGroup();
    OOT3D::Rumble::enabled = oot_editor_config->value(QStringLiteral("Controller/rumble_enabled"), false).toBool();
    OOT3D::ControlSwap::enabled=oot_editor_config->value(QStringLiteral("Controller/swap_start_select"),false).toBool();
    OOT3D::ControlSwap::shoulders=oot_editor_config->value(QStringLiteral("Controller/swap_shoulders"),false).toBool();
    OOT3D::ControlSwap::cycle_arrows=false; // Removed option; ignore older saved enables.
    OOT3D::Randomizer::Activity::enabled=oot_editor_config->value(QStringLiteral("Multiplayer/activity_log"),true).toBool();
    {
        OOT3D::RemoteNames::Layout p;
        const QString key=QStringLiteral("Multiplayer/player_names/");
        p.enabled=oot_editor_config->value(key+QStringLiteral("enabled"),p.enabled).toBool();
        p.shadow=oot_editor_config->value(key+QStringLiteral("shadow"),p.shadow).toBool();
        p.backdrop=oot_editor_config->value(key+QStringLiteral("backdrop/enabled"),p.backdrop).toBool();
        for(auto [name,value]:std::array{
            std::pair{"x",&p.x},std::pair{"y",&p.y},std::pair{"scale",&p.scale},std::pair{"opacity",&p.opacity},
            std::pair{"shadow_x",&p.shadow_x},std::pair{"shadow_y",&p.shadow_y},
            std::pair{"shadow_strength",&p.shadow_strength},std::pair{"shadow_softness",&p.shadow_softness},
            std::pair{"backdrop/x",&p.backdrop_x},std::pair{"backdrop/y",&p.backdrop_y},
            std::pair{"backdrop/width",&p.backdrop_width},std::pair{"backdrop/height",&p.backdrop_height},
            std::pair{"backdrop/opacity",&p.backdrop_opacity},std::pair{"backdrop/softness",&p.backdrop_softness}})
            *value=oot_editor_config->value(key+QLatin1String(name),*value).toFloat();
        OOT3D::RemoteNames::SetLayout(p,false);
    }
    OOT3D::Rumble::strength = std::clamp(oot_editor_config->value(QStringLiteral("Controller/rumble_strength"), 100).toInt(), 0, 100);
    auto* oot_rumble_timer = new QTimer(this);
    connect(oot_rumble_timer, &QTimer::timeout, this, [this, was_active = false]() mutable {
        const bool running = game_title_id == OOT3D::TitleScreen::ProgramId &&
            emu_thread && emu_thread->IsRunning() && !system.frame_limiter.IsFrameAdvancing();
        OOT3D::Rumble::running = running;
        OOT3D::Rumble::sync_impacts_to_present =
            Settings::GetWorkingGraphicsAPI() == Settings::GraphicsAPI::Vulkan;
        const auto amplitude = OOT3D::Rumble::Sample();
        if (amplitude || was_active) {
            const bool supported = InputCommon::SetControllerRumble(amplitude, amplitude, amplitude ? 80 : 0);
            if (amplitude) OOT3D::Rumble::output_status = supported ? 1 : -1;
        }
        was_active = amplitude != 0;
    });
    oot_rumble_timer->start(16);
    auto* oot_settings_timer = new QTimer(this);
    if (OOT3D::IsPlayerBuild()) AutoConfigureController(system, *config);
    connect(oot_settings_timer, &QTimer::timeout, this, [this, oot_editor_config, controller_ticks=0u]() mutable {
        if (OOT3D::IsPlayerBuild() && controller_ticks++ % 15 == 0)
            AutoConfigureController(system, *config);
        OOT3D::Interpolation::Timing::display_request_fps =
            game_title_id == OOT3D::TitleScreen::ProgramId && emu_thread &&
            emu_thread->IsRunning() && OOT3D::Interpolation::Replaying()
                ? OOT3D::Interpolation::TargetFPS() : 0;
        if(const auto* display=render_window->screen()) {
            const float refresh=float(display->refreshRate());
            if(std::isfinite(refresh) && refresh>=30 && refresh<=1000 &&
               std::abs(OOT3D::Interpolation::display_refresh_rate.exchange(refresh)-refresh)>.1f)
                ++OOT3D::Interpolation::generation;
        }
        if(multiplayer_state) {
            const bool oot=game_title_id==OOT3D::TitleScreen::ProgramId && system.IsPoweredOn();
            if(multiplayer_state->UpdateOotSettings(oot))config->Save();
            for(auto* action:{ui->action_View_Lobby,ui->action_Start_Room,ui->action_Connect_To_Room}) {
                action->setEnabled(!oot);
                action->setToolTip(oot?tr("Use Host room or Join room on file select."):QString{});
            }
        }
        if(!QApplication::activeModalWidget() && OOT3D::configure_controls.exchange(false))
            OnConfigureControls();
        if(!QApplication::activeModalWidget()) {
            if(OOT3D::configure_textures_shortcut.exchange(false))
                OnConfigurePresentationShortcut(true);
            else if(OOT3D::configure_fullscreen_shortcut.exchange(false))
                OnConfigurePresentationShortcut(false);
            if(OOT3D::fullscreen_requested.exchange(false)) {
                ui->action_Fullscreen->setChecked(!ui->action_Fullscreen->isChecked());
                ToggleFullscreen();
                UpdateUISettings();config->Save();
            }
        }
        OOT3D::PlayerGraphics::ApplyPending(Settings::values,
            [&] { system.ApplySettings(); }, [&] { config->Save(); });
        if(OOT3D::PlayerGraphics::fog_dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Graphics/disable_fog"),OOT3D::PlayerGraphics::disable_fog.load());
            oot_editor_config->sync();
        }
        const int textures=OOT3D::requested_custom_textures.exchange(-1);
        if(textures>=0) {
            Settings::values.custom_textures.SetGlobal(true);
            Settings::values.custom_textures=textures!=0;
            system.ApplySettings();config->Save();
        }
        OOT3D::custom_textures=Settings::values.custom_textures.GetValue();
        if(OOT3D::open_texture_folder.exchange(false)) {
            const auto path=QString::fromStdString(fmt::format("{}textures/{:016X}",
                FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),OOT3D::TitleScreen::ProgramId));
            OOT3D::texture_folder_failed=!(QDir().mkpath(path) &&
                QDesktopServices::openUrl(QUrl::fromLocalFile(path)));
        }
        if(OOT3D::Randomizer::Activity::settings_dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Multiplayer/activity_log"),OOT3D::Randomizer::Activity::enabled.load());
            oot_editor_config->sync();
        }
        if (OOT3D::Randomizer::dirty.exchange(false)) OOT3D::Randomizer::SaveConfig(*oot_editor_config);
        if (OOT3D::Randomizer::Presets::dirty.exchange(false)) OOT3D::Randomizer::Presets::Save(*oot_editor_config);
        if (OOT3D::Rumble::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Controller/rumble_enabled"), OOT3D::Rumble::enabled.load());
            oot_editor_config->setValue(QStringLiteral("Controller/rumble_strength"), OOT3D::Rumble::strength.load());
            oot_editor_config->sync();
        }
        if(OOT3D::ControlSwap::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Controller/swap_start_select"),OOT3D::ControlSwap::enabled.load());
            oot_editor_config->setValue(QStringLiteral("Controller/swap_shoulders"),OOT3D::ControlSwap::shoulders.load());
            oot_editor_config->sync();
        }
        if(OOT3D::RemoteNames::dirty.exchange(false)) {
            const auto p=OOT3D::RemoteNames::GetLayout();
            const QString key=QStringLiteral("Multiplayer/player_names/");
            oot_editor_config->setValue(key+QStringLiteral("enabled"),p.enabled);
            oot_editor_config->setValue(key+QStringLiteral("shadow"),p.shadow);
            oot_editor_config->setValue(key+QStringLiteral("backdrop/enabled"),p.backdrop);
            for(auto [name,value]:std::array{
                std::pair{"x",p.x},std::pair{"y",p.y},std::pair{"scale",p.scale},std::pair{"opacity",p.opacity},
                std::pair{"shadow_x",p.shadow_x},std::pair{"shadow_y",p.shadow_y},
                std::pair{"shadow_strength",p.shadow_strength},std::pair{"shadow_softness",p.shadow_softness},
                std::pair{"backdrop/x",p.backdrop_x},std::pair{"backdrop/y",p.backdrop_y},
                std::pair{"backdrop/width",p.backdrop_width},std::pair{"backdrop/height",p.backdrop_height},
                std::pair{"backdrop/opacity",p.backdrop_opacity},std::pair{"backdrop/softness",p.backdrop_softness}})
                oot_editor_config->setValue(key+QLatin1String(name),value);
            oot_editor_config->sync();
        }
        if(OOT3D::AudioSettings::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Audio/mute_music"),
                OOT3D::AudioSettings::mute_music.load());
            oot_editor_config->setValue(QStringLiteral("Audio/accurate_hle_audio"),
                AudioCore::HLE::accurate_audio.load());
            oot_editor_config->sync();
        }
        if(!OOT3D::Difficulty::file_rules.load() && OOT3D::Difficulty::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Difficulty/damage_multiplier"),OOT3D::Difficulty::damage_multiplier.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/disable_hearts"),OOT3D::Difficulty::disable_hearts.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/disable_fairies"),OOT3D::Difficulty::disable_fairies.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/disable_rupees"),OOT3D::Difficulty::disable_rupees.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/weaker_defense"),OOT3D::Difficulty::weaker_defense.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/disable_magic"),OOT3D::Difficulty::disable_magic.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/disable_ammo"),OOT3D::Difficulty::disable_ammo.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/disable_fairy_drops"),OOT3D::Difficulty::disable_fairy_drops.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/lose_rupees_on_death"),OOT3D::Difficulty::lose_rupees_on_death.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/increase_prices"),OOT3D::Difficulty::increase_prices.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/red_potion_hearts"),OOT3D::Difficulty::red_potion_hearts.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/blue_potion_hearts"),OOT3D::Difficulty::blue_potion_hearts.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/bottled_fairy_hearts"),OOT3D::Difficulty::bottled_fairy_hearts.load());
            oot_editor_config->setValue(QStringLiteral("Difficulty/price_multiplier"),OOT3D::Difficulty::price_multiplier.load());
            oot_editor_config->sync();
        }
        if(OOT3D::QOL::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("QOL/disable_navi"),OOT3D::QOL::disable_navi.load());
            oot_editor_config->setValue(QStringLiteral("QOL/disable_heart_beep"),OOT3D::QOL::disable_heart_beep.load());
            oot_editor_config->setValue(QStringLiteral("QOL/fast_ladder"),OOT3D::QOL::fast_ladder.load());
            oot_editor_config->setValue(QStringLiteral("QOL/fast_loading"),OOT3D::QOL::fast_loading.load());
            oot_editor_config->setValue(QStringLiteral("QOL/no_bombchu_first_person"),OOT3D::QOL::no_bombchu_first_person.load());
            oot_editor_config->sync();
        }
        const int volume_request=OOT3D::AudioSettings::requested_volume.exchange(-1);
        if(volume_request>=0) {
            Settings::values.volume.SetGlobal(true);
            Settings::values.volume=std::clamp(volume_request,0,100)/100.f;
            Settings::values.audio_muted=false;
            system.ApplySettings();config->Save();UpdateVolumeUI();
        }
        OOT3D::AudioSettings::volume.store(std::clamp(
            int(Settings::values.volume.GetValue()*100),0,100));
        OOT3D::SetInspectionPaused(!emu_thread || !emu_thread->IsRunning() ||
                                   system.frame_limiter.IsFrameAdvancing());
        if(OOT3D::Ocarina::dirty.exchange(false)) {
            const auto layout=OOT3D::Ocarina::GetLayout();
            for(unsigned i=0;i<OOT3D::Ocarina::Count;++i) {
                const auto group=QStringLiteral("Ocarina/")+QString::fromLatin1(OOT3D::Ocarina::Names[i])+QStringLiteral("/");
                oot_editor_config->setValue(group+QStringLiteral("x"),layout[i].x);
                oot_editor_config->setValue(group+QStringLiteral("y"),layout[i].y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),layout[i].scale);
            }
            oot_editor_config->setValue(QStringLiteral("Ocarina/bottom_panels"),layout.bottom_panels);
            oot_editor_config->setValue(QStringLiteral("Ocarina/stones/two_columns"),layout.two_columns);
            oot_editor_config->setValue(QStringLiteral("Ocarina/stones/row_spacing"),layout.row_spacing);
            oot_editor_config->setValue(QStringLiteral("Ocarina/stones/column_spacing"),layout.column_spacing);
            oot_editor_config->setValue(QStringLiteral("Ocarina/stones/size"),layout.stone_size);
            oot_editor_config->setValue(QStringLiteral("Ocarina/prompt_vignette/enabled"),layout.prompt_vignette.enabled);
            oot_editor_config->setValue(QStringLiteral("Ocarina/prompt_vignette/strength"),layout.prompt_vignette.strength);
            oot_editor_config->setValue(QStringLiteral("Ocarina/prompt_vignette/width"),layout.prompt_vignette.width);
            oot_editor_config->setValue(QStringLiteral("Ocarina/prompt_vignette/height"),layout.prompt_vignette.height);
            oot_editor_config->setValue(QStringLiteral("Ocarina/prompt_vignette/softness"),layout.prompt_vignette.softness);
            oot_editor_config->setValue(QStringLiteral("Ocarina/prompt_vignette/x"),layout.prompt_vignette.x);
            oot_editor_config->setValue(QStringLiteral("Ocarina/prompt_vignette/y"),layout.prompt_vignette.y);
            oot_editor_config->setValue(QStringLiteral("Ocarina/panel/right"),layout.panel.right);
            oot_editor_config->setValue(QStringLiteral("Ocarina/panel/opacity"),layout.panel.opacity);
            oot_editor_config->setValue(QStringLiteral("Ocarina/panel/border"),layout.panel.border);
            oot_editor_config->sync();
        }
        if(OOT3D::FileSelect::dirty.exchange(false)) {
            const auto layout=OOT3D::FileSelect::GetLayout();
            {
                const auto group=QStringLiteral("FileSelect/particles/");
                const auto& p=layout.particles;
                oot_editor_config->setValue(group+QStringLiteral("enabled"),p.enabled);
                oot_editor_config->setValue(group+QStringLiteral("density"),p.density);
                oot_editor_config->setValue(group+QStringLiteral("glow"),p.glow);
                oot_editor_config->setValue(group+QStringLiteral("size"),p.size);
                oot_editor_config->setValue(group+QStringLiteral("speed"),p.speed);
                oot_editor_config->setValue(group+QStringLiteral("light_strength"),p.light_strength);
                oot_editor_config->setValue(group+QStringLiteral("light_radius"),p.light_radius);
                oot_editor_config->setValue(group+QStringLiteral("spin"),p.spin);
                oot_editor_config->setValue(group+QStringLiteral("variations"),p.variations);
            }
            {
                const auto group=QStringLiteral("FileSelect/vignette/");
                const auto& v=layout.vignette;
                oot_editor_config->setValue(group+QStringLiteral("enabled"),v.enabled);
                oot_editor_config->setValue(group+QStringLiteral("strength"),v.strength);
                oot_editor_config->setValue(group+QStringLiteral("bottom_right_boost"),v.bottom_right_boost);
                oot_editor_config->setValue(group+QStringLiteral("width"),v.width);
                oot_editor_config->setValue(group+QStringLiteral("height"),v.height);
                oot_editor_config->setValue(group+QStringLiteral("softness"),v.softness);
            }
            oot_editor_config->setValue(QStringLiteral("FileSelect/backgrounds/enabled"),layout.backgrounds_enabled);
            for(unsigned i=0;i<layout.backgrounds.size();++i) {
                const auto group=QStringLiteral("FileSelect/backgrounds/")+QString::number(i)+QStringLiteral("/");
                const auto& b=layout.backgrounds[i];
                oot_editor_config->setValue(group+QStringLiteral("x"),b.pose.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),b.pose.y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),b.pose.scale);
                oot_editor_config->setValue(group+QStringLiteral("darkness"),b.darkness);
            }
            oot_editor_config->setValue(QStringLiteral("FileSelect/stones/x"),layout.stones.x);
            oot_editor_config->setValue(QStringLiteral("FileSelect/stones/y"),layout.stones.y);
            oot_editor_config->setValue(QStringLiteral("FileSelect/stones/scale"),layout.stones.scale);
            oot_editor_config->setValue(QStringLiteral("FileSelect/stones/spacing"),layout.stone_spacing);
            oot_editor_config->setValue(QStringLiteral("FileSelect/stones/shadow_opacity"),layout.stone_shadow_opacity);
            oot_editor_config->setValue(QStringLiteral("FileSelect/stones/ease_out"),layout.stone_ease_out);
            oot_editor_config->setValue(QStringLiteral("FileSelect/stones/focus_grow"),layout.stone_focus_grow);
            oot_editor_config->setValue(QStringLiteral("FileSelect/stones/focus_seconds"),layout.stone_focus_seconds);
            oot_editor_config->setValue(QStringLiteral("FileSelect/seed_text/x"),layout.seed_text.x);
            oot_editor_config->setValue(QStringLiteral("FileSelect/seed_text/y"),layout.seed_text.y);
            oot_editor_config->setValue(QStringLiteral("FileSelect/seed_text/scale"),layout.seed_text.scale);
            oot_editor_config->setValue(QStringLiteral("FileSelect/seed_text/room_id_gap"),layout.room_id_gap);
            oot_editor_config->setValue(QStringLiteral("FileSelect/randomizer_badge/x"),layout.randomizer_badge.x);
            oot_editor_config->setValue(QStringLiteral("FileSelect/randomizer_badge/y"),layout.randomizer_badge.y);
            oot_editor_config->setValue(QStringLiteral("FileSelect/randomizer_badge/scale"),layout.randomizer_badge.scale);
            oot_editor_config->setValue(QStringLiteral("FileSelect/randomizer_badge/color_r"),layout.randomizer_badge_color[0]);
            oot_editor_config->setValue(QStringLiteral("FileSelect/randomizer_badge/color_g"),layout.randomizer_badge_color[1]);
            oot_editor_config->setValue(QStringLiteral("FileSelect/randomizer_badge/color_b"),layout.randomizer_badge_color[2]);
            {
                const auto group=QStringLiteral("FileSelect/seed_text/shadow/");
                const auto& s=layout.seed_shadow;
                oot_editor_config->setValue(group+QStringLiteral("enabled"),s.enabled);
                oot_editor_config->setValue(group+QStringLiteral("x"),s.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),s.y);
                oot_editor_config->setValue(group+QStringLiteral("width"),s.width);
                oot_editor_config->setValue(group+QStringLiteral("height"),s.height);
                oot_editor_config->setValue(group+QStringLiteral("radius"),s.radius);
                oot_editor_config->setValue(group+QStringLiteral("softness"),s.softness);
                oot_editor_config->setValue(group+QStringLiteral("opacity"),s.opacity);
            }
            for(unsigned i=0;i<8;++i) {
                const auto group=QStringLiteral("FileSelect/quest/")+QString::number(i)+QStringLiteral("/");
                const auto p=i<2?layout.quest_stones[i]:i==2?layout.quest_description:i==3?layout.randomizer_prompt:i==4?layout.join_prompt:i==5?layout.profile_join_prompt:i==6?layout.host_prompt:layout.player_count;
                oot_editor_config->setValue(group+QStringLiteral("x"),p.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),p.y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),p.scale);
            }
            for(unsigned i=0;i<OOT3D::FileSelect::Count;++i) {
                const auto group=QStringLiteral("FileSelect/parts/")+QString::number(i)+QStringLiteral("/");
                const auto p=layout.parts[i];
                oot_editor_config->setValue(group+QStringLiteral("x"),p.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),p.y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),p.scale);
            }
            for(unsigned i=0;i<layout.name_prompts.size();++i) {
                const auto group=QStringLiteral("FileSelect/name_entry/prompt_offsets/")+QString::number(i)+QStringLiteral("/");
                const auto p=layout.name_prompts[i];
                oot_editor_config->setValue(group+QStringLiteral("x"),p.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),p.y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),p.scale);
            }
            for(unsigned i=0;i<OOT3D::FileSelect::NameCount;++i) {
                const auto group=QStringLiteral("FileSelect/name_entry/")+QString::number(i)+QStringLiteral("/");
                const auto p=layout.name_parts[i];
                oot_editor_config->setValue(group+QStringLiteral("x"),p.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),p.y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),p.scale);
            }
            for(unsigned i=0;i<layout.prompts.size();++i) {
                const auto group=QStringLiteral("FileSelect/")+QString::number(i)+QStringLiteral("/");
                const auto p=layout.prompts[i];
                oot_editor_config->setValue(group+QStringLiteral("x"),p.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),p.y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),p.scale);
            }
            {
                oot_editor_config->setValue(QStringLiteral("FileSelect/name_entry/edge_fade"),layout.name_box_edge_fade);
                const auto group=QStringLiteral("FileSelect/name_entry/shadow/");
                const auto s=layout.keyboard_shadow;
                oot_editor_config->setValue(group+QStringLiteral("enabled"),s.enabled);
                oot_editor_config->setValue(group+QStringLiteral("opacity"),s.opacity);
                oot_editor_config->setValue(group+QStringLiteral("softness"),s.softness);
                oot_editor_config->setValue(group+QStringLiteral("spread"),s.spread);
                oot_editor_config->setValue(group+QStringLiteral("padding_top"),s.padding_top);
                oot_editor_config->setValue(group+QStringLiteral("padding_left"),s.padding_left);
                oot_editor_config->setValue(group+QStringLiteral("padding_bottom"),s.padding_bottom);
                oot_editor_config->setValue(group+QStringLiteral("padding_right"),s.padding_right);
                oot_editor_config->setValue(group+QStringLiteral("x"),s.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),s.y);
            }
            {
                const auto group=QStringLiteral("FileSelect/name_entry/custom/");
                const auto k=layout.keyboard;
                oot_editor_config->setValue(group+QStringLiteral("enabled"),k.enabled);
                oot_editor_config->setValue(group+QStringLiteral("spacing_x"),k.spacing_x);
                oot_editor_config->setValue(group+QStringLiteral("spacing_y"),k.spacing_y);
                oot_editor_config->setValue(group+QStringLiteral("font_scale"),k.font_scale);
                oot_editor_config->setValue(group+QStringLiteral("selected_grow"),k.selected_grow);
                oot_editor_config->setValue(group+QStringLiteral("selected_growth"),k.selected_growth);
                oot_editor_config->setValue(group+QStringLiteral("selected_growth_seconds"),k.selected_growth_seconds);
                oot_editor_config->setValue(group+QStringLiteral("cursor_x"),k.cursor_x);
                oot_editor_config->setValue(group+QStringLiteral("cursor_y"),k.cursor_y);
            }
            oot_editor_config->sync();
        }
        if(OOT3D::PausePrompts::dirty.exchange(false)) {
            const auto prompts=OOT3D::PausePrompts::GetLayout();
            oot_editor_config->setValue(QStringLiteral("PausePrompts/enabled"),prompts.enabled);
            oot_editor_config->setValue(QStringLiteral("PausePrompts/scale"),prompts.scale);
            oot_editor_config->setValue(QStringLiteral("PausePrompts/icon_scale"),prompts.icon_scale);
            oot_editor_config->setValue(QStringLiteral("PausePrompts/letter_spacing"),prompts.letter_spacing);
            oot_editor_config->setValue(QStringLiteral("PausePrompts/navigation_distance"),prompts.navigation_distance);
            oot_editor_config->setValue(QStringLiteral("PausePrompts/navigation_speed"),prompts.navigation_speed);
            {
                const auto& shadow=prompts.map_navigation_shadow;
                const auto prefix=QStringLiteral("PausePrompts/map_navigation_shadow/");
                oot_editor_config->setValue(prefix+QStringLiteral("enabled"),shadow.enabled);
                oot_editor_config->setValue(prefix+QStringLiteral("x"),shadow.x);
                oot_editor_config->setValue(prefix+QStringLiteral("y"),shadow.y);
                oot_editor_config->setValue(prefix+QStringLiteral("width"),shadow.width);
                oot_editor_config->setValue(prefix+QStringLiteral("height"),shadow.height);
                oot_editor_config->setValue(prefix+QStringLiteral("opacity"),shadow.opacity);
                oot_editor_config->setValue(prefix+QStringLiteral("softness"),shadow.softness);
            }
            oot_editor_config->setValue(QStringLiteral("PausePrompts/lower/x"),prompts.lower.pose.x);
            oot_editor_config->setValue(QStringLiteral("PausePrompts/lower/y"),prompts.lower.pose.y);
            oot_editor_config->setValue(QStringLiteral("PausePrompts/lower/scale"),prompts.lower.pose.scale);
            oot_editor_config->setValue(QStringLiteral("PausePrompts/lower/spacing"),prompts.lower.spacing);
            for(unsigned i=0;i<prompts.prompts.size();++i) {
                const auto prefix=QStringLiteral("PausePrompts/")+
                    QString::fromLatin1(OOT3D::PausePrompts::Names[i])+QStringLiteral("/");
                const auto save_pose=[&](const QString& group,const OOT3D::PausePrompts::Pose& pose) {
                    oot_editor_config->setValue(group+QStringLiteral("x"),pose.x);
                    oot_editor_config->setValue(group+QStringLiteral("y"),pose.y);
                    oot_editor_config->setValue(group+QStringLiteral("scale"),pose.scale);
                };
                save_pose(prefix,prompts.prompts[i].group);
                save_pose(prefix+QStringLiteral("button/"),prompts.prompts[i].button);
            }
            oot_editor_config->sync();
        }
        if(OOT3D::BossChallenge::dirty.exchange(false)) {
            const auto layout=OOT3D::BossChallenge::GetLayout();
            for(unsigned i=0;i<OOT3D::BossChallenge::Count;++i) {
                const auto group=QStringLiteral("BossChallenge/")+QString::fromLatin1(OOT3D::BossChallenge::Names[i])+QStringLiteral("/");
                oot_editor_config->setValue(group+QStringLiteral("x"),layout[i].x);
                oot_editor_config->setValue(group+QStringLiteral("y"),layout[i].y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),layout[i].scale);
            }
            oot_editor_config->setValue(QStringLiteral("BossChallenge/two_columns"),layout.two_columns);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/horizontal"),layout.horizontal);
            for(unsigned i=0;i<2;++i) {
                const auto group=QStringLiteral("BossChallenge/")+QString::fromLatin1(OOT3D::BossChallenge::Names[OOT3D::BossChallenge::Background+i]);
                oot_editor_config->setValue(group+QStringLiteral("/darkness"),layout.background_darkness[i]);
            }
            oot_editor_config->setValue(QStringLiteral("BossChallenge/preview/corner_radius"),layout.corner_radius);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/preview/darkness"),layout.panel_darkness);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/border/inner_shadow_width"),layout.inner_shadow_width);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/border/inner_shadow_opacity"),layout.inner_shadow_opacity);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/border/darkness"),layout.border_darkness);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/border/enabled"),layout.border_enabled);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/border/bottom"),layout.border_bottom);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/preview/shadow_opacity"),layout.shadow_opacity);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/preview/shadow_softness"),layout.shadow_softness);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/spacing_x"),layout.spacing_x);
            oot_editor_config->setValue(QStringLiteral("BossChallenge/spacing_y"),layout.spacing_y);
            oot_editor_config->sync();
        }
        if(OOT3D::DeathMenu::dirty.exchange(false)) {
            const auto layout=OOT3D::DeathMenu::GetLayout();
            for(unsigned i=0;i<OOT3D::DeathMenu::Count;++i) {
                const auto group=QStringLiteral("DeathMenu/")+QString::fromLatin1(OOT3D::DeathMenu::Names[i])+QStringLiteral("/");
                oot_editor_config->setValue(group+QStringLiteral("x"),layout[i].x);
                oot_editor_config->setValue(group+QStringLiteral("y"),layout[i].y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),layout[i].scale);
            }
            oot_editor_config->sync();
        }
        if(OOT3D::SaveMenu::dirty.exchange(false)) {
            const auto layout=OOT3D::SaveMenu::GetLayout();
            for(unsigned i=0;i<2;++i) {
                const auto group=QStringLiteral("SaveMenu/prompts/%1/").arg(i);
                const auto p=layout.prompts[i];
                oot_editor_config->setValue(group+QStringLiteral("x"),p.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),p.y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),p.scale);
            }
            oot_editor_config->setValue(QStringLiteral("SaveMenu/darkness"),layout.darkness);
            oot_editor_config->setValue(QStringLiteral("SaveMenu/hide_header"),layout.hide_header);
            for(unsigned i=0;i<OOT3D::SaveMenu::Count;++i) {
                const auto group=QStringLiteral("SaveMenu/")+QString::fromLatin1(OOT3D::SaveMenu::Names[i])+QStringLiteral("/");
                oot_editor_config->setValue(group+QStringLiteral("x"),layout[i].x);
                oot_editor_config->setValue(group+QStringLiteral("y"),layout[i].y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),layout[i].scale);
            }
            oot_editor_config->sync();
        }
        if(OOT3D::PauseMenu::dirty.exchange(false)) {
            const auto layout=OOT3D::PauseMenu::GetLayout();
            OOT3D::PauseMenu::SaveColorLayouts(*oot_editor_config,layout);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/tab_shadow/enabled"),layout.tab_shadow.enabled);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/tab_shadow/softness"),layout.tab_shadow.softness);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/tab_shadow/strength"),layout.tab_shadow.strength);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/tab_shadow/x"),layout.tab_shadow.x);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/tab_shadow/y"),layout.tab_shadow.y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/blend"),layout.world_parchment_blend);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/darkness"),layout.world_parchment_darkness);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/outline"),layout.world_map_outline);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/shadow_opacity"),layout.world_map_shadow_opacity);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/shadow_width"),layout.world_map_shadow_width);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/top_fade/enabled"),layout.world_map_top_fade.enabled);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/top_fade/opacity"),layout.world_map_top_fade.opacity);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/top_fade/height"),layout.world_map_top_fade.height);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/top_fade/y"),layout.world_map_top_fade.y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/top_fade/softness"),layout.world_map_top_fade.softness);
            for(unsigned c=0;c<3;++c)
                oot_editor_config->setValue(QStringLiteral("PauseScreen/world_parchments/top_fade/color_%1").arg(c),layout.world_map_top_fade.color[c]);
            for(unsigned i=0;i<layout.world_parchments.size();++i) {
                const QString prefix=QStringLiteral("PauseScreen/world_parchments/%1/").arg(i);
                const auto& stack=layout.world_parchments[i];
                oot_editor_config->setValue(prefix+QStringLiteral("enabled"),stack.enabled);
                oot_editor_config->setValue(prefix+QStringLiteral("x"),stack.pose.x);
                oot_editor_config->setValue(prefix+QStringLiteral("y"),stack.pose.y);
                oot_editor_config->setValue(prefix+QStringLiteral("scale"),stack.pose.scale);
                oot_editor_config->setValue(prefix+QStringLiteral("spacing"),stack.spacing);
            }
            oot_editor_config->setValue(QStringLiteral("PauseScreen/parchments/blend"),layout.parchment_blend);
            for(unsigned i=0;i<3;++i) {
                const QString prefix=QStringLiteral("PauseScreen/parchments/%1/").arg(i+2);
                const auto& paper=layout.dungeon_parchments[i];
                oot_editor_config->setValue(prefix+QStringLiteral("enabled"),paper.enabled);
                oot_editor_config->setValue(prefix+QStringLiteral("x"),paper.pose.x);
                oot_editor_config->setValue(prefix+QStringLiteral("y"),paper.pose.y);
                oot_editor_config->setValue(prefix+QStringLiteral("scale"),paper.pose.scale);
            }
            {
                const QString prefix=QStringLiteral("PauseScreen/dungeon_floor_line/");
                const auto& line=layout.dungeon_floor_line;
                oot_editor_config->setValue(prefix+QStringLiteral("enabled"),line.enabled);
                oot_editor_config->setValue(prefix+QStringLiteral("x"),line.pose.x);
                oot_editor_config->setValue(prefix+QStringLiteral("y"),line.pose.y);
                oot_editor_config->setValue(prefix+QStringLiteral("scale"),line.pose.scale);
                oot_editor_config->setValue(prefix+QStringLiteral("width"),line.width);
                oot_editor_config->setValue(prefix+QStringLiteral("height"),line.height);
                oot_editor_config->setValue(prefix+QStringLiteral("opacity"),line.opacity);
                oot_editor_config->setValue(prefix+QStringLiteral("roundness"),line.roundness);
            }
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_map/edge_shadows"),layout.dungeon_edge_shadows);
            {
                const auto prefix=QStringLiteral("PauseScreen/dungeon_map/top_shadow/");
                const auto& s=layout.dungeon_top_shadow;
                oot_editor_config->setValue(prefix+QStringLiteral("enabled"),s.enabled);
                oot_editor_config->setValue(prefix+QStringLiteral("height"),s.height);
                oot_editor_config->setValue(prefix+QStringLiteral("opacity"),s.opacity);
                oot_editor_config->setValue(prefix+QStringLiteral("y"),s.y);
            }
            for(unsigned side=0;side<2;++side) {
                const QString prefix=QStringLiteral("PauseScreen/dungeon_shadow_%1/").arg(side);
                const auto& b=layout.dungeon_blocks[side];
                oot_editor_config->setValue(prefix+QStringLiteral("enabled"),b.enabled);
                oot_editor_config->setValue(prefix+QStringLiteral("x"),b.x);
                oot_editor_config->setValue(prefix+QStringLiteral("y"),b.y);
                oot_editor_config->setValue(prefix+QStringLiteral("width"),b.width);
                oot_editor_config->setValue(prefix+QStringLiteral("height"),b.height);
                oot_editor_config->setValue(prefix+QStringLiteral("fade"),b.fade);
                oot_editor_config->setValue(prefix+QStringLiteral("opacity"),b.opacity);
                oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_map/background_darkness_%1").arg(side),layout.dungeon_background_darkness[side]);
            }
            {
                const auto& p=layout.dungeon_particles;
                const QString key=QStringLiteral("PauseScreen/dungeon_map/particles/");
                oot_editor_config->setValue(key+QStringLiteral("progress"),layout.dungeon_particles_progress);
                oot_editor_config->setValue(key+QStringLiteral("density_start"),layout.dungeon_particle_endpoints.density_start);
                oot_editor_config->setValue(key+QStringLiteral("density_end"),layout.dungeon_particle_endpoints.density_end);
                oot_editor_config->setValue(key+QStringLiteral("drift_start"),layout.dungeon_particle_endpoints.drift_start);
                oot_editor_config->setValue(key+QStringLiteral("drift_end"),layout.dungeon_particle_endpoints.drift_end);
                oot_editor_config->setValue(key+QStringLiteral("enabled"),p.enabled);
                oot_editor_config->setValue(key+QStringLiteral("density"),p.density);
                oot_editor_config->setValue(key+QStringLiteral("glow"),p.glow);
                oot_editor_config->setValue(key+QStringLiteral("size"),p.size);
                oot_editor_config->setValue(key+QStringLiteral("speed"),p.speed);
                oot_editor_config->setValue(key+QStringLiteral("spin"),p.spin);
                oot_editor_config->setValue(key+QStringLiteral("light_strength"),p.light_strength);
                oot_editor_config->setValue(key+QStringLiteral("light_radius"),p.light_radius);
                oot_editor_config->setValue(key+QStringLiteral("variations"),p.variations);
            }
            {
                const auto& fade=layout.dungeon_map_top_fade;
                const QString key=QStringLiteral("PauseScreen/dungeon_map/top_fade/");
                oot_editor_config->setValue(key+QStringLiteral("enabled"),fade.enabled);
                oot_editor_config->setValue(key+QStringLiteral("opacity"),fade.opacity);
                oot_editor_config->setValue(key+QStringLiteral("height"),fade.height);
                oot_editor_config->setValue(key+QStringLiteral("y"),fade.y);
                oot_editor_config->setValue(key+QStringLiteral("softness"),fade.softness);
                for(unsigned c=0;c<3;++c)
                    oot_editor_config->setValue(key+QStringLiteral("color_%1").arg(c),fade.color[c]);
            }
            {
                const auto& v=layout.dungeon_prompt_vignette;
                const QString key=QStringLiteral("PauseScreen/dungeon_map/prompt_vignette/");
                oot_editor_config->setValue(key+QStringLiteral("enabled"),v.enabled);
                oot_editor_config->setValue(key+QStringLiteral("strength"),v.strength);
                oot_editor_config->setValue(key+QStringLiteral("width"),v.width);
                oot_editor_config->setValue(key+QStringLiteral("height"),v.height);
                oot_editor_config->setValue(key+QStringLiteral("softness"),v.softness);
                oot_editor_config->setValue(key+QStringLiteral("x"),v.x);
                oot_editor_config->setValue(key+QStringLiteral("y"),v.y);
            }
            {
                const auto& v=layout.world_prompt_vignette;
                const QString key=QStringLiteral("PauseScreen/map/prompt_vignette/");
                oot_editor_config->setValue(key+QStringLiteral("enabled"),v.enabled);
                oot_editor_config->setValue(key+QStringLiteral("strength"),v.strength);
                oot_editor_config->setValue(key+QStringLiteral("width"),v.width);
                oot_editor_config->setValue(key+QStringLiteral("height"),v.height);
                oot_editor_config->setValue(key+QStringLiteral("softness"),v.softness);
                oot_editor_config->setValue(key+QStringLiteral("x"),v.x);
                oot_editor_config->setValue(key+QStringLiteral("y"),v.y);
                for(unsigned c=0;c<3;++c)
                    oot_editor_config->setValue(key+QStringLiteral("color_%1").arg(c),v.color[c]);
            }
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_floors/hide_stones"),layout.hide_dungeon_floor_stones);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_floors/grow_selected"),layout.dungeon_floor_growth.enabled);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_floors/selected_size"),layout.dungeon_floor_growth.size);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_floors/growth_speed"),layout.dungeon_floor_growth.speed);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_floors/cursor_scale"),layout.dungeon_cursor_scale);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_map/brightness"),layout.dungeon_brightness);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/darkness"),layout.map_darkness);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/skulltula_x"),layout.world_skulltula_x);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/skulltula_y"),layout.world_skulltula_y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/skulltula_scale"),layout.world_skulltula_scale);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/time_of_day_tint"),layout.world_map_time_tint);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_items/spacing"),layout.dungeon_spacing[0]);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/dungeon_floors/spacing"),layout.dungeon_spacing[1]);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/horizontal"),layout.horizontal);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/stack_background"),layout.stack_background);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/header_y"),layout.header_y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/selection_grow_seconds"),layout.selection_grow_seconds);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/transition_seconds"),layout.transition_seconds);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/slot_content_scale"),layout.slot_content_scale);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/slot_spacing"),layout.slot_spacing);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/slot_content_x"),layout.slot_content_x);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/preview_rotation_speed"),layout.preview_rotation_speed);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/background_darkness"),layout.background_darkness);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/enabled"),layout.map_frame.enabled);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/cut_x"),layout.map_frame.cut_x);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/cut_y"),layout.map_frame.cut_y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/cut_scale"),layout.map_frame.cut_scale);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/darkness"),layout.map_frame.darkness);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/extension_enabled"),layout.map_extension.enabled);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/extension_width"),layout.map_extension.width);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/extension_blur"),layout.map_extension.blur);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/extension_darkness"),layout.map_extension.darkness);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/side_shadows"),layout.map_side_shadows);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/shadow_width"),layout.map_shadow.width);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map/shadow_opacity"),layout.map_shadow.opacity);
            for(unsigned i=0;i<layout.map_backgrounds.size();++i) {
                const QString prefix=QStringLiteral("PauseScreen/map_background_%1/").arg(i);
                const auto& b=layout.map_backgrounds[i];
                oot_editor_config->setValue(prefix+QStringLiteral("x"),b.pose.x);
                oot_editor_config->setValue(prefix+QStringLiteral("y"),b.pose.y);
                oot_editor_config->setValue(prefix+QStringLiteral("scale"),b.pose.scale);
                oot_editor_config->setValue(prefix+QStringLiteral("darkness"),b.darkness);
            }
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/x"),layout.map_frame.x);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/y"),layout.map_frame.y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/width"),layout.map_frame.width);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/height"),layout.map_frame.height);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/left"),layout.map_frame.left);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/right"),layout.map_frame.right);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/top"),layout.map_frame.top);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/map_frame/bottom"),layout.map_frame.bottom);
            for(unsigned i=0;i<layout.map_frame.pieces.size();++i) {
                const QString prefix=QStringLiteral("PauseScreen/map_frame/piece_%1/").arg(i);
                oot_editor_config->setValue(prefix+QStringLiteral("x"),layout.map_frame.pieces[i].x);
                oot_editor_config->setValue(prefix+QStringLiteral("y"),layout.map_frame.pieces[i].y);
                oot_editor_config->setValue(prefix+QStringLiteral("scale"),layout.map_frame.pieces[i].scale);
            }
            oot_editor_config->setValue(QStringLiteral("PauseScreen/gear_panel/x"),layout.gear_panel.x);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/gear_panel/y"),layout.gear_panel.y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/gear_panel/width"),layout.gear_panel.width);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/gear_panel/height"),layout.gear_panel.height);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/gear_panel_lower/x"),layout.gear_panel_lower.x);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/gear_panel_lower/y"),layout.gear_panel_lower.y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/gear_panel_lower/width"),layout.gear_panel_lower.width);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/gear_panel_lower/height"),layout.gear_panel_lower.height);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/arrow_shadow/enabled"),layout.arrow_shadow.enabled);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/arrow_shadow/softness"),layout.arrow_shadow.softness);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/arrow_shadow/x"),layout.arrow_shadow.x);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/arrow_shadow/y"),layout.arrow_shadow.y);
            oot_editor_config->setValue(QStringLiteral("PauseScreen/arrow_shadow/strength"),layout.arrow_shadow.strength);
            for(auto [name,shadow]:std::array{
                    std::pair{"header_shadow",layout.header_shadow},
                    std::pair{"slots_shadow",layout.slots_shadow},
                    std::pair{"background_shadow",layout.background_shadow},
                    std::pair{"items_shadow",layout.items_shadow},
                    std::pair{"gear_upper_shadow",layout.gear_shadows[0]},
                    std::pair{"gear_lower_shadow",layout.gear_shadows[1]},
                    std::pair{"dungeon_edge_shadow",layout.dungeon_edge_shadow},
                    std::pair{"dungeon_left_shadow",layout.dungeon_left_shadow},
                    std::pair{"dungeon_right_shadow",layout.dungeon_right_shadow}}) {
                const QString prefix=QStringLiteral("PauseScreen/")+QString::fromLatin1(name)+QStringLiteral("/");
                oot_editor_config->setValue(prefix+QStringLiteral("width"),shadow.width);
                oot_editor_config->setValue(prefix+QStringLiteral("opacity"),shadow.opacity);
            }
            for(auto [name,overlay]:std::array{
                    std::pair{"header_overlay",layout.header_overlay},
                    std::pair{"slots_overlay",layout.slots_overlay},
                    std::pair{"dungeon_left_overlay",layout.dungeon_left_overlay},
                    std::pair{"dungeon_right_overlay",layout.dungeon_right_overlay}}) {
                const QString prefix=QStringLiteral("PauseScreen/")+QString::fromLatin1(name)+QStringLiteral("/");
                constexpr std::array channels{"red","green","blue"};
                for(unsigned c=0;c<3;++c)
                    oot_editor_config->setValue(prefix+QString::fromLatin1(channels[c]),overlay.color[c]);
                oot_editor_config->setValue(prefix+QStringLiteral("opacity"),overlay.opacity);
            }
            const auto& names=OOT3D::PauseMenu::Names;
            for(unsigned i=0;i<layout.size();++i) {
                oot_editor_config->beginGroup(QStringLiteral("PauseScreen/")+QString::fromLatin1(names[i]));
                oot_editor_config->setValue(QStringLiteral("x"),layout[i].x);
                oot_editor_config->setValue(QStringLiteral("y"),layout[i].y);
                oot_editor_config->setValue(QStringLiteral("scale"),layout[i].scale);
                if(OOT3D::PauseMenu::IsLowerGear(static_cast<OOT3D::PauseMenu::Part>(i)))
                    oot_editor_config->setValue(QStringLiteral("vertical"),
                        layout.gear_vertical[i-OOT3D::PauseMenu::GearBottomLeft]);
                oot_editor_config->endGroup();
            }
            oot_editor_config->sync();
        }
        if (OOT3D::UserCamera::dirty.exchange(false)) {
            OOT3D::UserCamera::SaveConfig(*oot_editor_config);
            oot_editor_config->sync();
        }
        if (OOT3D::OrbitCamera::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Camera/orbit"),OOT3D::OrbitCamera::enabled.load());
            oot_editor_config->setValue(QStringLiteral("Camera/smooth_collision_return"),OOT3D::OrbitCamera::smooth_collision_return.load());
            oot_editor_config->setValue(QStringLiteral("Camera/distance_scale"),OOT3D::OrbitCamera::distance_scale.load());
            oot_editor_config->setValue(QStringLiteral("Camera/invert_x"),OOT3D::OrbitCamera::invert_x.load());
            oot_editor_config->setValue(QStringLiteral("Camera/invert_y"),OOT3D::OrbitCamera::invert_y.load());
            oot_editor_config->setValue(QStringLiteral("Camera/sensitivity"),OOT3D::OrbitCamera::sensitivity.load());
            oot_editor_config->setValue(QStringLiteral("Camera/acceleration_time"),OOT3D::OrbitCamera::acceleration_time.load());
            oot_editor_config->setValue(QStringLiteral("Camera/deceleration_time"),OOT3D::OrbitCamera::deceleration_time.load());
            oot_editor_config->sync();
        }
        if(OOT3D::DialogueCamera::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Camera/mm_dialogue_camera"),OOT3D::DialogueCamera::mm_style.load());
            oot_editor_config->setValue(QStringLiteral("Camera/dialogue_ease_in_time"),OOT3D::DialogueCamera::ease_in_time.load());
            oot_editor_config->setValue(QStringLiteral("Camera/dialogue_ease_out_time"),OOT3D::DialogueCamera::ease_out_time.load());
            oot_editor_config->remove(QStringLiteral("Camera/dialogue_enter_time"));
            oot_editor_config->remove(QStringLiteral("Camera/dialogue_leave_time"));
            oot_editor_config->sync();
        }
        if (OOT3D::Camera::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Graphics/field_of_view"), OOT3D::Camera::desired_fov.load());
            oot_editor_config->sync();
        }
        if (OOT3D::Widescreen::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Graphics/widescreen"), OOT3D::Widescreen::enabled.load());
            oot_editor_config->sync();
        }
        if (OOT3D::HDFont::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Graphics/font_mode"), OOT3D::HDFont::CurrentMode());
            oot_editor_config->setValue(QStringLiteral("Graphics/hd_font"), OOT3D::HDFont::enabled.load());
            oot_editor_config->sync();
        }
        if (OOT3D::ActorDrawDistance::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Graphics/actor_draw_distance"),
                OOT3D::ActorDrawDistance::multiplier.load());
            oot_editor_config->sync();
        }
        if (OOT3D::StartupHint::dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("General/show_startup_settings_hint"),
                OOT3D::StartupHint::enabled.load());
            oot_editor_config->sync();
        }
        if (OOT3D::settings_size_dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("General/settings_size"), OOT3D::settings_size.load());
            oot_editor_config->sync();
        }
        const int requested = OOT3D::requested_resolution.exchange(-1);
        if (requested >= 0) {
            Settings::values.resolution_factor.SetGlobal(true);
            Settings::values.resolution_factor = static_cast<u32>(std::clamp(requested, 1, 9));
            system.ApplySettings();
            config->Save();
        }
        OOT3D::resolution.store(Settings::values.resolution_factor.GetValue());
        if (OOT3D::Textboxes::dirty.exchange(false)) {
            const auto layout = OOT3D::Textboxes::GetLayout();
            oot_editor_config->beginGroup(QStringLiteral("Textboxes"));
            oot_editor_config->setValue(QStringLiteral("x"), layout.x);
            oot_editor_config->setValue(QStringLiteral("y"), layout.y);
            oot_editor_config->setValue(QStringLiteral("width"), layout.width);
            oot_editor_config->setValue(QStringLiteral("scale"), layout.scale);
            oot_editor_config->setValue(QStringLiteral("sign_y"), layout.sign_y);
            oot_editor_config->setValue(QStringLiteral("opening_speed"), layout.opening_speed);
            oot_editor_config->setValue(QStringLiteral("smooth_reveal"), layout.smooth_reveal);
            oot_editor_config->setValue(QStringLiteral("mm_close"), layout.mm_close);
            oot_editor_config->setValue(QStringLiteral("indicator_x"), layout.indicator.x);
            oot_editor_config->setValue(QStringLiteral("indicator_y"), layout.indicator.y);
            oot_editor_config->setValue(QStringLiteral("indicator_scale"), layout.indicator.scale);
            oot_editor_config->setValue(QStringLiteral("indicator_visible"),layout.indicator_animation.visible);
            oot_editor_config->setValue(QStringLiteral("indicator_spring"),layout.indicator_animation.spring);
            oot_editor_config->setValue(QStringLiteral("indicator_small"),layout.indicator_animation.small);
            oot_editor_config->setValue(QStringLiteral("indicator_big"),layout.indicator_animation.big);
            oot_editor_config->setValue(QStringLiteral("indicator_speed"),layout.indicator_animation.speed);
            oot_editor_config->endGroup();
            oot_editor_config->sync();
        }
        if(OOT3D::DPad::dirty.exchange(false)) {
            const auto layout=OOT3D::DPad::GetLayout();
            oot_editor_config->setValue(QStringLiteral("HUD/dpad/enabled"),layout.enabled);
            oot_editor_config->setValue(QStringLiteral("HUD/dpad/navi_pulse_speed"),layout.navi_pulse_speed);
            const auto save_pose=[&](const QString& name,const OOT3D::DPad::Pose& p) {
                oot_editor_config->beginGroup(QStringLiteral("HUD/dpad/")+name);
                oot_editor_config->setValue(QStringLiteral("x"),p.x);
                oot_editor_config->setValue(QStringLiteral("y"),p.y);
                oot_editor_config->setValue(QStringLiteral("scale_x"),p.scale_x);
                oot_editor_config->setValue(QStringLiteral("scale_y"),p.scale_y);
                oot_editor_config->endGroup();
            };
            save_pose(QStringLiteral("group"),layout.group);
            for(unsigned i=0;i<OOT3D::DPad::ElementCount;++i)
                save_pose(QString::fromLatin1(OOT3D::DPad::Names[i]),layout.icons[i]);
            oot_editor_config->sync();
        }
        if(OOT3D::HUDButtons::dirty.exchange(false)) {
            const auto layout=OOT3D::HUDButtons::GetLayout();
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/b/minigame_x"),layout.minigame_b_x);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/b/ammo/x"),layout.b_ammo.x);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/b/ammo/y"),layout.b_ammo.y);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/b/ammo/scale"),layout.b_ammo.scale);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/show_items"),layout.show_items);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/item_x"),layout.item_x);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/item_y"),layout.item_y);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/item_scale"),layout.item_scale);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/diamond/rotation"),layout.diamond_rotation);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/diamond/spacing"),layout.diamond_spacing);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/diamond/version"),1);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/labels/x"),layout.item_labels.x);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/labels/y"),layout.item_labels.y);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/labels/scale"),layout.item_labels.scale);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/items/x"),layout.item_icons.x);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/items/y"),layout.item_icons.y);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/items/scale"),layout.item_icons.scale);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/counts/scale"),layout.counter_scale);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/counts/x"),layout.counter_x);
            oot_editor_config->setValue(QStringLiteral("HUD/buttons/counts/y"),layout.counter_y);
            for(unsigned i=0;i<OOT3D::HUDButtons::Count;++i) {
                const auto group=QStringLiteral("HUD/buttons/")+QString::fromLatin1(OOT3D::HUDButtons::Names[i])+QStringLiteral("/");
                const auto b=layout.buttons[i];
                oot_editor_config->setValue(group+QStringLiteral("enabled"),b.enabled);
                oot_editor_config->setValue(group+QStringLiteral("x"),b.pose.x);
                oot_editor_config->setValue(group+QStringLiteral("y"),b.pose.y);
                oot_editor_config->setValue(group+QStringLiteral("scale"),b.pose.scale);
            }
            oot_editor_config->sync();
        }
        if(OOT3D::Interpolation::settings_dirty.exchange(false)) {
            oot_editor_config->setValue(QStringLiteral("Interpolation/cpu_vertices"),OOT3D::Interpolation::cpu_vertices.load());
            oot_editor_config->setValue(QStringLiteral("Interpolation/enabled"),OOT3D::Interpolation::present_60.load());
            oot_editor_config->setValue(QStringLiteral("Interpolation/target_fps"),OOT3D::Interpolation::target_fps.load());
            oot_editor_config->setValue(QStringLiteral("Interpolation/match_refresh_rate"),OOT3D::Interpolation::match_refresh_rate.load());
            oot_editor_config->sync();
        }
        if (OOT3D::UserHUD::dirty.exchange(false)) {
            OOT3D::UserHUD::SaveConfig(*oot_editor_config);
            oot_editor_config->sync();
        }
        if (OOT3D::HUD::dirty.exchange(false)) {
            const auto layout = OOT3D::HUD::GetLayout();
            OOT3D::UserHUD::SaveAuthoredTitles(*oot_editor_config,layout);
            constexpr std::array names{"hearts", "rupees", "magic", "a_button", "loading_icon", "minimap", "keys", "timer", "minigame", "shard_agony", "target_indicator", "carrots"};
            for (unsigned i = 0; i < layout.size(); ++i) {
                oot_editor_config->beginGroup(QStringLiteral("HUD/") + QString::fromLatin1(names[i]));
                oot_editor_config->setValue(QStringLiteral("x"), layout[i].x);
                oot_editor_config->setValue(QStringLiteral("y"), layout[i].y);
                oot_editor_config->setValue(QStringLiteral("scale"), layout[i].scale);
                if (i == OOT3D::HUD::Minimap)
                    oot_editor_config->setValue(QStringLiteral("lock_corner"),layout.minimap_lock_corner);
                if (i == OOT3D::HUD::Hearts)
                    oot_editor_config->setValue(QStringLiteral("spacing"), layout.heart_spacing);
                if (i == OOT3D::HUD::Rupees)
                    oot_editor_config->setValue(QStringLiteral("number_gap"),layout.rupee_number_gap);
                if (i == OOT3D::HUD::Carrots) {
                    oot_editor_config->setValue(QStringLiteral("opacity"),layout.carrot_opacity);
                    oot_editor_config->setValue(QStringLiteral("spacing"),layout.carrot_spacing);
                    oot_editor_config->setValue(QStringLiteral("animation_delay"),layout.carrot_animation_delay);
                    oot_editor_config->setValue(QStringLiteral("shadow_enabled"),layout.carrot_shadow.enabled);
                    oot_editor_config->setValue(QStringLiteral("shadow_x"),layout.carrot_shadow.x);
                    oot_editor_config->setValue(QStringLiteral("shadow_y"),layout.carrot_shadow.y);
                    oot_editor_config->setValue(QStringLiteral("shadow_blur"),layout.carrot_shadow.blur);
                    oot_editor_config->setValue(QStringLiteral("shadow_strength"),layout.carrot_shadow.strength);
                }
                if (i == OOT3D::HUD::Magic) {
                    oot_editor_config->setValue(QStringLiteral("height"),layout.magic_height);
                    oot_editor_config->setValue(QStringLiteral("border"),layout.magic_border);
                    oot_editor_config->setValue(QStringLiteral("background_opacity"),layout.magic_background_opacity);
                }
                oot_editor_config->endGroup();
            }
            OOT3D::HUD::SaveHDNumbers(*oot_editor_config,layout);
            oot_editor_config->setValue(QStringLiteral("HUD/navi_text/x"),layout.navi_text.x);
            oot_editor_config->setValue(QStringLiteral("HUD/navi_text/y"),layout.navi_text.y);
            oot_editor_config->setValue(QStringLiteral("HUD/minimap/outside/x"),layout.outside_minimap.x);
            oot_editor_config->setValue(QStringLiteral("HUD/minimap/outside/y"),layout.outside_minimap.y);
            oot_editor_config->setValue(QStringLiteral("HUD/minimap/outside/scale"),layout.outside_minimap.scale);
            oot_editor_config->setValue(QStringLiteral("HUD/minimap/anchor_version"),2);
            for(unsigned side=0;side<2;++side) {
                const QString group=QStringLiteral("HUD/shop_prompts/")+(side?QStringLiteral("right/"):QStringLiteral("left/"));
                oot_editor_config->setValue(group+QStringLiteral("x"),layout.shop_prompt_offsets[side][0]);
                oot_editor_config->setValue(group+QStringLiteral("y"),layout.shop_prompt_offsets[side][1]);
            }
            oot_editor_config->sync();
        }
        if (OOT3D::TitleScreen::dirty.exchange(false)) {
            const auto layout = OOT3D::TitleScreen::GetLayout();
            constexpr std::array names{"logo", "copyright", "press_start", "press_start_extra"};
            for (unsigned i = 0; i < layout.size(); ++i) {
                oot_editor_config->beginGroup(QStringLiteral("TitleScreen/") + QString::fromLatin1(names[i]));
                oot_editor_config->setValue(QStringLiteral("x"), layout[i].x);
                oot_editor_config->setValue(QStringLiteral("y"), layout[i].y);
                oot_editor_config->setValue(QStringLiteral("scale"), layout[i].scale);
                if(i==3) {
                    oot_editor_config->setValue(QStringLiteral("color_r"),layout.text_color[0]);
                    oot_editor_config->setValue(QStringLiteral("color_g"),layout.text_color[1]);
                    oot_editor_config->setValue(QStringLiteral("color_b"),layout.text_color[2]);
                    oot_editor_config->setValue(QStringLiteral("letter_spacing"),layout.letter_spacing);
                }
                oot_editor_config->endGroup();
            }
            oot_editor_config->sync();
        }
        // Use the normal Qt shutdown path after pending editor settings save.
        // Presentation builds already close without a confirmation dialog.
        if(OOT3D::quit_requested.exchange(false))close();
    });
    oot_settings_timer->start(50);
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    auto* inspection_timer = new QTimer(this);
    connect(inspection_timer, &QTimer::timeout, this, [this, controller_ticks=0u]() mutable {
#ifdef ENABLE_MM3D_GAME_FOLDER
        // Retry hotplug and saved automatic profiles; preserve custom bindings.
        if(controller_ticks++%15==0 && !QApplication::activeModalWidget() &&
           !Settings::values.use_artic_base_controller.GetValue()) {
            AutoConfigureController(system, *config);
        }
#else
        (void)controller_ticks;
#endif
        using namespace MM3D::PlayerSettings;
        ApplyPending(Settings::values, [&] { system.ApplySettings(); },
                     [&] { config->Save(); });
        if(open_textures.exchange(false)) {
            const auto path=QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::LoadDir))+
                QStringLiteral("textures/0004000000125500");
            if(!QDir().mkpath(path) || !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
                QMessageBox::warning(this,tr("Texture folder"),
                    tr("Could not open the texture folder. You can find it at:\n%1").arg(path));
        }
        if(open_cheats.exchange(false)) {
            u64 title_id{};
            if(system.IsPoweredOn() &&
               system.GetAppLoader().ReadProgramId(title_id)==Loader::ResultStatus::Success && title_id) {
                DekuiCheatsDialog dialog(system.CheatEngine(),title_id,this);
                dialog.exec();
                render_window->setFocus();
            } else {
                QMessageBox::information(this,tr("Cheats"),tr("Start the game before opening cheats."));
            }
        }
        if(controls.exchange(false))OnConfigureControls();
        const bool texture_binding=textures_shortcut.exchange(false);
        if(fullscreen_shortcut.exchange(false)||texture_binding)
            OnConfigurePresentationShortcut(texture_binding);
        if(fullscreen.exchange(false)) {ui->action_Fullscreen->setChecked(!ui->action_Fullscreen->isChecked());ToggleFullscreen();}
        if(about.exchange(false)) {
#ifdef ENABLE_MM3D_GAME_FOLDER
            QDesktopServices::openUrl(QUrl::fromLocalFile(MM3D::GameFolder::Root()+QStringLiteral("/licenses")));
#else
            OnMenuAboutCitra();
#endif
        }
        if(quit.exchange(false)) {
            // The ImGui Quit button already confirmed this specific close.
            const QScopedValueRollback<bool> confirmed{close_already_confirmed,true};
            close();
        }
        if (MM3D::step_requested.exchange(false) && system.IsPoweredOn() &&
            system.frame_limiter.IsFrameAdvancing()) {
            for (unsigned i = 0; i < MM3D::inspection_step_count.load(); ++i)
                system.frame_limiter.AdvanceFrame();
        }
    });
    inspection_timer->start(16);
#endif

    connect(this, &GMainWindow::UpdateProgress, this, &GMainWindow::OnUpdateProgress);
    connect(this, &GMainWindow::CIAInstallReport, this, &GMainWindow::OnCIAInstallReport);
    connect(this, &GMainWindow::CIAInstallFinished, this, &GMainWindow::OnCIAInstallFinished);
    connect(this, &GMainWindow::CompressFinished, this, &GMainWindow::OnCompressFinished);
    connect(this, &GMainWindow::UpdateThemedIcons, multiplayer_state,
            &MultiplayerState::UpdateThemedIcons);
}

void GMainWindow::ConnectMenuEvents() {
    const auto connect_menu = [&](QAction* action, const auto& event_fn,
                                  QAction::MenuRole role = QAction::NoRole) {
        action->setMenuRole(role);
        connect(action, &QAction::triggered, this, event_fn);
        // Add actions to this window so that hiding menus in fullscreen won't disable them
        addAction(action);
        // Add actions to the render window so that they work outside of single window mode
        render_window->addAction(action);
    };

    // File
    connect_menu(ui->action_Load_File, &GMainWindow::OnMenuLoadFile);
    connect_menu(ui->action_Install_CIA, &GMainWindow::OnMenuInstallCIA);
    connect_menu(ui->action_Connect_Artic, &GMainWindow::OnMenuConnectArticBase);
    connect_menu(ui->action_Setup_System_Files, &GMainWindow::OnMenuSetUpSystemFiles);
    for (u32 region = 0; region < Core::NUM_SYSTEM_TITLE_REGIONS; region++) {
        connect_menu(ui->menu_Boot_Home_Menu->actions().at(region),
                     [this, region] { OnMenuBootHomeMenu(region); });
    }
    connect_menu(ui->action_Exit, &QMainWindow::close, QAction::QuitRole);
    connect_menu(ui->action_Load_Amiibo, &GMainWindow::OnLoadAmiibo);
    connect_menu(ui->action_Remove_Amiibo, &GMainWindow::OnRemoveAmiibo);
    connect_menu(ui->action_Open_Citra_Folder, &GMainWindow::OnOpenCitraFolder);
    connect_menu(ui->action_Open_NAND_Folder, &GMainWindow::OnOpenNANDFolder);
    connect_menu(ui->action_Open_SDMC_Folder, &GMainWindow::OnOpenSDMCFolder);

    // Emulation
    connect_menu(ui->action_Pause, &GMainWindow::OnPauseContinueGame);
    connect_menu(ui->action_Stop, &GMainWindow::OnStopGame);
    connect_menu(ui->action_Restart, [this] { BootGame(QString(game_path)); });
    connect_menu(ui->action_Report_Compatibility, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/azahar-emu/compatibility-list/blob/master/CONTRIBUTING.md")));
    });
    connect_menu(ui->action_Configure, &GMainWindow::OnConfigure, QAction::PreferencesRole);
    connect_menu(ui->action_Configure_Current_Game, &GMainWindow::OnConfigurePerGame);

    // View
    connect_menu(ui->action_Single_Window_Mode, &GMainWindow::ToggleWindowMode);
    connect_menu(ui->action_Display_Dock_Widget_Headers, &GMainWindow::OnDisplayTitleBars);
    connect_menu(ui->action_Show_Filter_Bar, &GMainWindow::OnToggleFilterBar);
    connect(ui->action_Show_Status_Bar, &QAction::triggered, statusBar(), &QStatusBar::setVisible);

    // Multiplayer
    connect(ui->action_View_Lobby, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnViewLobby);
    connect(ui->action_Start_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCreateRoom);
    connect(ui->action_Leave_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCloseRoom);
    connect(ui->action_Connect_To_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnDirectConnectToRoom);
    connect(ui->action_Show_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnOpenNetworkRoom);

    connect_menu(ui->action_Fullscreen, &GMainWindow::ToggleFullscreen);
    connect_menu(ui->action_Screen_Layout_Default, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Single_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Large_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Hybrid_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Side_by_Side, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Separate_Windows, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Custom_Layout, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Swap_Screens, &GMainWindow::OnSwapScreens);
    connect_menu(ui->action_Screen_Layout_Upright_Screens, &GMainWindow::OnRotateScreens);
    connect_menu(ui->action_Small_Screen_TopRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_MiddleRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_BottomRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_TopLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_MiddleLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_BottomLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_Above, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_Below, &GMainWindow::ChangeSmallScreenPosition);

    // Movie
    connect_menu(ui->action_Record_Movie, &GMainWindow::OnRecordMovie);
    connect_menu(ui->action_Play_Movie, &GMainWindow::OnPlayMovie);
    connect_menu(ui->action_Close_Movie, &GMainWindow::OnCloseMovie);
    connect_menu(ui->action_Save_Movie, &GMainWindow::OnSaveMovie);
    connect_menu(ui->action_Movie_Read_Only_Mode,
                 [this](bool checked) { movie.SetReadOnly(checked); });
    connect_menu(ui->action_Advance_Frame, [this] {
        if (emulation_running && system.frame_limiter.IsFrameAdvancing()) {
            system.frame_limiter.AdvanceFrame();
        }
    });
    connect_menu(ui->action_Capture_Screenshot, &GMainWindow::OnCaptureScreenshot);
    connect_menu(ui->action_Dump_Video, &GMainWindow::OnDumpVideo);

    // Tools debug
    connect_menu(ui->action_Debug_Pause, [this] {
        if (emu_thread) {
            emu_thread->SetRunning(false);
        }
    });
    connect_menu(ui->action_Debug_Resume, [this] {
        if (emu_thread) {
            emu_thread->SetRunning(true);
        }
    });
    connect_menu(ui->action_Debug_Step, [this] {
        if (emu_thread) {
            emu_thread->ExecStep();
        }
    });
    connect_menu(ui->action_Debug_Unschedule_All,
                 [this] { system.DebugUnscheduleAllThreadsFromFrontend(true); });
    connect_menu(ui->action_Debug_Schedule_All,
                 [this] { system.DebugUnscheduleAllThreadsFromFrontend(false); });

    // Tools
    connect_menu(ui->action_Compress_ROM_File, &GMainWindow::OnCompressFile);
    connect_menu(ui->action_Decompress_ROM_File, &GMainWindow::OnDecompressFile);

    // Help
    connect_menu(ui->action_Open_Log_Folder, []() {
        QString path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::LogDir));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect_menu(ui->action_FAQ, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://azahar-emu.org/pages/faq/")));
    });
    connect_menu(ui->action_About, &GMainWindow::OnMenuAboutCitra, QAction::AboutRole);
}

void GMainWindow::UpdateMenuState() {
    const bool is_paused =
        !emu_thread || !emu_thread->IsRunning() || system.frame_limiter.IsFrameAdvancing();

    const std::array running_actions{
        ui->action_Stop,
        ui->action_Restart,
        ui->action_Configure_Current_Game,
        ui->action_Report_Compatibility,
        ui->action_Load_Amiibo,
        ui->action_Remove_Amiibo,
        ui->action_Pause,
        ui->action_Advance_Frame,
    };

    for (QAction* action : running_actions) {
        action->setEnabled(emulation_running);
    }

    ui->action_Capture_Screenshot->setEnabled(emulation_running);
    ui->action_Advance_Frame->setEnabled(emulation_running && is_paused);

    if (emulation_running && is_paused) {
        ui->action_Pause->setText(tr("Continue"));
    } else {
        ui->action_Pause->setText(tr("Pause"));
    }
}

void GMainWindow::OnDisplayTitleBars(bool show) {
    QList<QDockWidget*> widgets = findChildren<QDockWidget*>();

    if (show) {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(nullptr);
            if (old) {
                delete old;
            }
        }
    } else {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(new QWidget());
            if (old) {
                delete old;
            }
        }
    }
}

#if defined(HAVE_SDL2) && defined(__unix__) && !defined(__APPLE__)
static std::optional<QDBusObjectPath> HoldWakeLockLinux(u32 window_id = 0) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        return {};
    }
    // reference: https://flatpak.github.io/xdg-desktop-portal/#gdbus-org.freedesktop.portal.Inhibit
    QDBusInterface xdp(QStringLiteral("org.freedesktop.portal.Desktop"),
                       QStringLiteral("/org/freedesktop/portal/desktop"),
                       QStringLiteral("org.freedesktop.portal.Inhibit"));
    if (!xdp.isValid()) {
        LOG_WARNING(Frontend, "Couldn't connect to XDP D-Bus endpoint");
        return {};
    }
    QVariantMap options = {};
    //: TRANSLATORS: This string is shown to the user to explain why Citra needs to prevent the
    //: computer from sleeping
    options.insert(QString::fromLatin1("reason"),
                   QCoreApplication::translate("GMainWindow", "Azahar is running an application"));
    // 0x4: Suspend lock; 0x8: Idle lock
    QDBusReply<QDBusObjectPath> reply =
        xdp.call(QString::fromLatin1("Inhibit"),
                 QString::fromLatin1("x11:") + QString::number(window_id, 16), 12U, options);

    if (reply.isValid()) {
        return reply.value();
    }
    LOG_WARNING(Frontend, "Couldn't read Inhibit reply from XDP: {}",
                reply.error().message().toStdString());
    return {};
}

static void ReleaseWakeLockLinux(const QDBusObjectPath& lock) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        return;
    }
    QDBusInterface unlocker(QString::fromLatin1("org.freedesktop.portal.Desktop"), lock.path(),
                            QString::fromLatin1("org.freedesktop.portal.Request"));
    unlocker.call(QString::fromLatin1("Close"));
}
#endif // __unix__

void GMainWindow::PreventOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#elif defined(HAVE_SDL2)
    SDL_DisableScreenSaver();
#if defined(__unix__) && !defined(__APPLE__)
    auto reply = HoldWakeLockLinux(winId());
    if (reply) {
        wake_lock = std::move(reply.value());
    }
#endif // defined(__unix__) && !defined(__APPLE__)
#endif // _WIN32
}

void GMainWindow::AllowOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#elif defined(HAVE_SDL2)
    SDL_EnableScreenSaver();
#if defined(__unix__) && !defined(__APPLE__)
    if (!wake_lock.path().isEmpty()) {
        ReleaseWakeLockLinux(wake_lock);
    }
#endif // defined(__unix__) && !defined(__APPLE__)
#endif // _WIN32
}

bool GMainWindow::LoadROM(const QString& filename) {
    // Shutdown previous session if the emu thread is still active...
    if (emu_thread) {
        ShutdownGame();
    }
#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::MinimapControl::Reset();
#endif

    if (!render_window->InitRenderTarget() || !secondary_window->InitRenderTarget()) {
        LOG_CRITICAL(Frontend, "Failed to initialize render targets!");
        return false;
    }

    const auto scope = render_window->Acquire();

    if (!UISettings::values.inserted_cartridge.GetValue().empty()) {
        system.InsertCartridge(UISettings::values.inserted_cartridge.GetValue());
    }

    const Core::System::ResultStatus result{
        system.Load(*render_window, filename.toStdString(), secondary_window)};

    if (result != Core::System::ResultStatus::Success) {
        QString invalid_format = tr("Invalid application format");
        QString invalid_format_description =
            tr("The application file format not supported.<br>Please make sure you are using one "
               "of the compatible file formats:<ul><li>Cartridge images: "
               "<b>.cci/.zcci/.3ds</b></li><li>Installable archives: "
               "<b>.cia/.zcia</b></li><li>Homebrew titles: <b>.3dsx/.z3dsx</b></li><li>NCCH "
               "containers: <b>.cxi/.zcxi/.app</b></li><li>ELF files: <b>.elf/.axf</b></li></ul>");

        switch (result) {
        case Core::System::ResultStatus::ErrorGetLoader:
            LOG_CRITICAL(Frontend, "Failed to obtain loader for {}", filename.toStdString());
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorSystemMode:
            LOG_CRITICAL(Frontend, "Failed to load application!");
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted: {
            QMessageBox::critical(this, tr("Encrypted application"),
                                  tr("Encrypted applications are not supported.<br/>"
                                     "<a "
                                     "href='https://azahar-emu.org/blog/game-loading-changes/'>"
                                     "Please check our blog for more info.</a>"));
            break;
        }
        case Core::System::ResultStatus::ErrorLoader_ErrorInvalidFormat:
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorLoader_ErrorGbaTitle:
            QMessageBox::critical(this, tr("Unsupported application"),
                                  tr("GBA Virtual Console is not supported by Azahar."));
            break;

        case Core::System::ResultStatus::ErrorArticDisconnected:
            QMessageBox::critical(
                this, tr("Artic Server"),
                tr(fmt::format(
                       "An error has occurred whilst communicating with the Artic Server.\n{}",
                       system.GetStatusDetails())
                       .c_str()));
            break;
        case Core::System::ResultStatus::ErrorN3DSApplication:
            QMessageBox::critical(this, tr("Invalid system mode"),
                                  tr("New 3DS exclusive applications cannot be loaded without "
                                     "enabling the New 3DS mode."));
            break;
        case Core::System::ResultStatus::ErrorLoader:
            QMessageBox::critical(this, tr("Generic load error"),
                                  tr("A generic load error occurred while loading the "
                                     "application.<br/>Please check the log for more details."));
            break;
        case Core::System::ResultStatus::ErrorLoader_ErrorPatches:
            QMessageBox::critical(this, tr("Error applying patches"),
                                  tr("A generic error occurred while applying a patch to the "
                                     "application.<br/>Please check the log for more details."));
            break;
        case Core::System::ResultStatus::ErrorLoader_ErrorPatchesInvalidTitle:
            QMessageBox::critical(
                this, tr("Error applying patches"),
                tr("Failed to apply a patch because it is designed for a different "
                   "application.<br/>Please make sure you are using the patches for "
                   "the right application, region and version."));
            break;
        default:
            QMessageBox::critical(
                this, tr("Error while loading application"),
                tr("An unknown error occurred.<br/>Please see the log for more details."));
            break;
        }
        return false;
    }

    std::string title;
    system.GetAppLoader().ReadTitle(title);
    game_title = QString::fromStdString(title);
    UpdateWindowTitle();

    u64 title_id;
    system.GetAppLoader().ReadProgramId(title_id);

    game_path = filename;
    game_title_id = title_id;

    return true;
}

void GMainWindow::BootGame(const QString& filename) {
#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::StartupHint::requested=false;
    if (OOT3D::IsPlayerBuild() && QFileInfo(filename).canonicalFilePath() !=
                                    QFileInfo(OoT3D::GameFolder::Rom()).canonicalFilePath()) return;
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    MM3D::PlayerSettings::startup_hint_requested.store(false);
#endif
#ifdef ENABLE_MM3D_GAME_FOLDER
    const auto expected=QFileInfo(MM3D::GameFolder::Rom()).canonicalFilePath();
    if (expected.isEmpty() || QFileInfo(filename).canonicalFilePath()!=expected) {
        QMessageBox::critical(this, tr("MM3D setup"), tr("Only the supported game in the Dekui folder can be opened in this build."));
        return;
    }
    if (const auto error=MM3D::GameFolder::Validate(); !error.isEmpty()) {
        QMessageBox::critical(this, tr("MM3D setup"), error);
        return;
    }
#endif
    if (emu_thread) {
        ShutdownGame();
    }

    const bool is_artic = filename.startsWith(QString::fromStdString("articbase:/")) ||
                          filename.startsWith(QString::fromStdString("articinio:/")) ||
                          filename.startsWith(QString::fromStdString("articinin:/"));

    if (!is_artic && filename.endsWith(QStringLiteral(".cia"))) {
        const auto answer = QMessageBox::question(
            this, tr("CIA must be installed before usage"),
            tr("Before using this CIA, you must install it. Do you want to install it now?"),
            QMessageBox::Yes | QMessageBox::No);

        if (answer == QMessageBox::Yes)
            InstallCIA(QStringList(filename));

        return;
    }

    show_artic_label = is_artic;

    LOG_INFO(Frontend, "Azahar starting...");
    if (!is_artic) {
        StoreRecentFile(filename); // Put the filename on top of the list
    }

    if (movie_record_on_start) {
        movie.PrepareForRecording();
    }
    if (movie_playback_on_start) {
        movie.PrepareForPlayback(movie_playback_path.toStdString());
    }

    const std::string path = filename.toStdString();
    auto loader = Loader::GetLoader(path);

    u64 title_id{0};
    Loader::ResultStatus res =
        loader ? loader->ReadProgramId(title_id) : Loader::ResultStatus::Error;

    if (Loader::ResultStatus::Success == res) {
        // Load per game settings
        const std::string name{is_artic ? "" : FileUtil::GetFilename(filename.toStdString())};
        const std::string config_file_name =
            title_id == 0 ? name : fmt::format("{:016X}", title_id);
        LOG_INFO(Frontend, "Loading per application config file for title {}", config_file_name);
        QtConfig per_game_config(config_file_name, QtConfig::ConfigType::PerGameConfig);
    }

    // Artic Server cannot accept a client multiple times, so multiple loaders are not
    // possible. Instead register the app loader early and do not create it again on system load.
    if (loader && !loader->SupportsMultipleInstancesForSameFile()) {
        system.RegisterAppLoaderEarly(loader);
    }

    // Override GDB settings if emulator was launched with
    // GDB port option.
    if (gdbport_from_arg != -1) {
        system.SetGDBPortOverride(gdbport_from_arg);
        system.SetDebugNextProcessFlag();
    }

#ifdef ENABLE_OOT3D_PRESENTATION
    Settings::values.graphics_api.SetGlobal(true);
    Settings::values.graphics_api = Settings::GraphicsAPI::Vulkan;
    Settings::values.custom_textures.SetGlobal(true);
    Settings::values.layout_option.SetGlobal(true);
    Settings::values.layout_option = Settings::LayoutOption::SingleScreen;
    Settings::values.swap_screen.SetGlobal(true);
    Settings::values.swap_screen = false;
    Settings::values.screen_top_stretch.SetGlobal(true);
    Settings::values.screen_top_stretch = false;
    Settings::values.screen_bottom_stretch.SetGlobal(true);
    Settings::values.screen_bottom_stretch = false;
    Settings::values.layouts_to_cycle.SetGlobal(true);
    Settings::values.layouts_to_cycle = std::vector<Settings::LayoutOption>{
        Settings::LayoutOption::SingleScreen, Settings::LayoutOption::Default,
        Settings::LayoutOption::SideScreen};
#endif
#ifdef ENABLE_MM3D_GAME_FOLDER
    Settings::values.resolution_factor.SetGlobal(true);
    Settings::values.volume.SetGlobal(true);
    Settings::values.use_vsync.SetGlobal(true);
    Settings::values.use_disk_shader_cache.SetGlobal(true);
    Settings::values.custom_textures.SetGlobal(true);
    Settings::values.swap_screen.SetGlobal(true);
    Settings::values.swap_screen=false;
    Settings::values.layout_option.SetGlobal(true);
    Settings::values.layout_option=Settings::LayoutOption::SingleScreen;
    Settings::values.graphics_api.SetGlobal(true);
    Settings::values.graphics_api=Settings::GraphicsAPI::Vulkan;
#endif
    system.ApplySettings();

    Settings::LogSettings();

    // Save configurations
    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    config->Save();

    if (!LoadROM(filename)) {
        render_window->ReleaseRenderTarget();
        secondary_window->ReleaseRenderTarget();
        return;
    }

#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::MapFrame::Load(system);
    OOT3D::DPad::Load(system);
    OOT3D::PausePrompts::Load(system);
#endif

    // Set everything up
    if (movie_record_on_start) {
        movie.StartRecording(movie_record_path.toStdString(), movie_record_author.toStdString());
        movie_record_on_start = false;
        movie_record_path.clear();
        movie_record_author.clear();
    }
    if (movie_playback_on_start) {
        movie.StartPlayback(movie_playback_path.toStdString());
        movie_playback_on_start = false;
        movie_playback_path.clear();
    }

    ui->action_Advance_Frame->setEnabled(false);

    if (video_dumping_on_start) {
        StartVideoDumping(video_dumping_path);
        video_dumping_on_start = false;
        video_dumping_path.clear();
    }

    // Register debug widgets
    if (graphicsWidget && graphicsWidget->isVisible()) {
        graphicsWidget->Register();
    }

    // Create and start the emulation thread
    emu_thread = std::make_unique<EmuThread>(system, *render_window);
    emit EmulationStarting(emu_thread.get());
    emu_thread->start();

    connect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    connect(render_window, &GRenderWindow::MouseActivity, this, &GMainWindow::OnMouseActivity);
    connect(secondary_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    connect(secondary_window, &GRenderWindow::MouseActivity, this, &GMainWindow::OnMouseActivity);

    // BlockingQueuedConnection is important here, it makes sure we've finished refreshing our views
    // before the CPU continues
    connect(emu_thread.get(), &EmuThread::DebugModeEntered, registersWidget,
            &RegistersWidget::OnDebugModeEntered, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeEntered, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeEntered, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeLeft, registersWidget,
            &RegistersWidget::OnDebugModeLeft, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeLeft, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeLeft, Qt::BlockingQueuedConnection);

    connect(emu_thread.get(), &EmuThread::LoadProgress, loading_screen,
            &LoadingScreen::OnLoadProgress, Qt::QueuedConnection);
    connect(emu_thread.get(), &EmuThread::SwitchDiskResources, this,
            &GMainWindow::OnSwitchDiskResources, Qt::QueuedConnection);
    connect(emu_thread.get(), &EmuThread::HideLoadingScreen, loading_screen,
            &LoadingScreen::OnLoadComplete);

    // Update the GUI
    registersWidget->OnDebugModeEntered();
    if (ui->action_Single_Window_Mode->isChecked()) {
        game_list->hide();
        game_list_placeholder->hide();
    }
    status_bar_update_timer.start(1000);

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
        setMouseTracking(true);
    }

    loading_screen->Prepare(system.GetAppLoader());
    loading_screen->show();

    emulation_running = true;
    if (ui->action_Fullscreen->isChecked()) {
        ShowFullscreen();
    }

    OnResumeGame(true);
}

void GMainWindow::ShutdownGame() {
    OOT3D::Rumble::running = false;
    OOT3D::Rumble::Clear();
    InputCommon::SetControllerRumble(0, 0, 0);
    if (!emulation_running) {
        return;
    }

    if (ui->action_Fullscreen->isChecked()) {
        HideFullscreen();
    }

    auto video_dumper = system.GetVideoDumper();
    if (video_dumper && video_dumper->IsDumping()) {
        game_shutdown_delayed = true;
        OnStopVideoDumping();
        return;
    }

    AllowOSSleep();

#ifdef ENABLE_DISCORD_RPC
    discord_rpc->Pause();
#endif

    emu_thread->RequestStop();

    // Release emu threads from any breakpoints
    // This belongs after RequestStop() and before wait() because if emulation stops on a GPU
    // breakpoint after (or before) RequestStop() is called, the emulation would never be able
    // to continue out to the main loop and terminate. Thus wait() would hang forever.
    // TODO(bunnei): This function is not thread safe, but it's being used as if it were
    if (Pica::g_debug_context) {
        Pica::g_debug_context->ClearBreakpoints();
    }

    // Unregister debug widgets
    if (graphicsWidget && graphicsWidget->isVisible()) {
        graphicsWidget->Unregister();
    }

    // Frame advancing must be cancelled in order to release the emu thread from waiting
    system.frame_limiter.SetFrameAdvancing(false);

    emit EmulationStopping();

    // Wait for emulation thread to complete and delete it
    emu_thread->wait();
    emu_thread = nullptr;
#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::PauseMenu::on_top.store(false);
    OOT3D::SaveMenu::on_top.store(false);
    OOT3D::DeathMenu::on_top.store(false);
    OOT3D::BossChallenge::on_top.store(false);
    OOT3D::FileSelect::on_top.store(false);
    OOT3D::PauseMenu::item_page.store(false);
    OOT3D::PauseMenu::gear_page.store(false);
#endif

    system.EjectCartridge();

    OnCloseMovie();

#ifdef ENABLE_DISCORD_RPC
    discord_rpc->Update(false);
#endif
#ifdef __unix__
    Common::Linux::StopGamemode();
#endif

    // The emulation is stopped, so closing the window or not does not matter anymore
    disconnect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    disconnect(secondary_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);

    render_window->hide();
    secondary_window->hide();
    loading_screen->hide();
    loading_screen->Clear();

#ifdef ENABLE_OOT3D_PRESENTATION
    if (OOT3D::IsPlayerBuild()) {
        game_list->hide(); game_list_placeholder->hide();
    } else
#endif
    if (game_list->IsEmpty()) {
        game_list_placeholder->show();
    } else {
        game_list->show();
    }
    game_list->SetFilterFocus();

    setMouseTracking(false);

    // Disable status bar updates
    status_bar_update_timer.stop();
    message_label_used_for_movie = false;
    show_artic_label = false;
    loading_shaders_label->setVisible(false);
    artic_traffic_label->setVisible(false);
    emu_speed_label->setVisible(false);
    game_fps_label->setVisible(false);
    emu_frametime_label->setVisible(false);
    notification_led->setColor(QColor(0, 0, 0));

    UpdateSaveStates();

    emulation_running = false;

    game_title.clear();
    UpdateWindowTitle();

    game_path.clear();
    game_title_id = 0;

    // Update the GUI
    UpdateMenuState();

    // When closing the game, destroy the GLWindow to clear the context after the game is closed
    render_window->ReleaseRenderTarget();
    secondary_window->ReleaseRenderTarget();
}

#ifdef ENABLE_DEVELOPER_OPTIONS
void GMainWindow::StartLaunchStressTest(const QString& game_path) {
    QThreadPool::globalInstance()->start([this, game_path] {
        do {
            ui->action_Stop->trigger();
            emit game_list->GameChosen(game_path);
            QThread::sleep(2);
        } while (emulation_running);
    });
}
#endif

void GMainWindow::StoreRecentFile(const QString& filename) {
    UISettings::values.recent_files.prepend(filename);
    UISettings::values.recent_files.removeDuplicates();
    while (UISettings::values.recent_files.size() > max_recent_files_item) {
        UISettings::values.recent_files.removeLast();
    }

    UpdateRecentFiles();
}

void GMainWindow::UpdateRecentFiles() {
    const int num_recent_files =
        std::min(static_cast<int>(UISettings::values.recent_files.size()), max_recent_files_item);

    for (int i = 0; i < num_recent_files; i++) {
        const QString text = QStringLiteral("&%1. %2").arg(i + 1).arg(
            QFileInfo(UISettings::values.recent_files[i]).fileName());
        actions_recent_files[i]->setText(text);
        actions_recent_files[i]->setData(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setToolTip(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setVisible(true);
    }

    for (int j = num_recent_files; j < max_recent_files_item; ++j) {
        actions_recent_files[j]->setVisible(false);
    }

    // Enable the recent files menu if the list isn't empty
    ui->menu_recent_files->setEnabled(num_recent_files != 0);
}

void GMainWindow::UpdateSaveStates() {
    if (!system.IsPoweredOn()) {
        ui->menu_Load_State->setEnabled(false);
        ui->menu_Save_State->setEnabled(false);
        return;
    }

    ui->menu_Load_State->setEnabled(true);
    ui->menu_Save_State->setEnabled(true);
    ui->action_Load_from_Newest_Slot->setEnabled(false);

    oldest_slot = newest_slot = 1;
    oldest_slot_time = std::numeric_limits<u64>::max();
    newest_slot_time = 0;

    u64 title_id;
    if (system.GetAppLoader().ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        return;
    }
    auto savestates = Core::ListSaveStates(title_id, movie.GetCurrentMovieID());
    for (u32 i = 0; i < Core::SaveStateSlotCount; ++i) {
        actions_load_state[i]->setEnabled(false);
        if (i == 0) {
            actions_load_state[i]->setText(tr("Quick Load"));
            actions_save_state[i]->setText(tr("Quick Save"));
        } else {
            actions_load_state[i]->setText(tr("Slot %1").arg(i));
            actions_save_state[i]->setText(tr("Slot %1").arg(i));
        }
    }
    for (const auto& savestate : savestates) {
        if (savestate.slot >= Core::SaveStateSlotCount) {
            continue;
        }
        actions_load_state[savestate.slot]->setEnabled(true);
        if (savestate.slot == 0) {
            const auto text = QStringLiteral("%2")
                                  .arg(QDateTime::fromSecsSinceEpoch(savestate.time)
                                           .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")))
                                  .trimmed();
            ui->action_Quick_Save->setText(tr("Quick Save - %1").arg(text).trimmed());
            ui->action_Quick_Load->setText(tr("Quick Load - %1").arg(text).trimmed());
            continue;
        }
        const auto text = tr("Slot %1 - %2")
                              .arg(savestate.slot)
                              .arg(QDateTime::fromSecsSinceEpoch(savestate.time)
                                       .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")))
                              .trimmed();

        actions_load_state[savestate.slot]->setText(text);
        actions_save_state[savestate.slot]->setText(text);

        ui->action_Load_from_Newest_Slot->setEnabled(true);
        if (savestate.time > newest_slot_time) {
            newest_slot = savestate.slot;
            newest_slot_time = savestate.time;
        }
        if (savestate.time < oldest_slot_time) {
            oldest_slot = savestate.slot;
            oldest_slot_time = savestate.time;
        }
    }
    // Value as 1 because quicksave slot is not used for this calculation
    for (u32 i = 1; i < Core::SaveStateSlotCount; ++i) {
        if (!actions_load_state[i]->isEnabled()) {
            // Prefer empty slot
            oldest_slot = i;
            oldest_slot_time = 0;
            break;
        }
    }
}

void GMainWindow::OnGameListLoadFile(QString game_path) {
    if (ConfirmChangeGame()) {
        BootGame(game_path);
    }
}

void GMainWindow::OnGameListOpenFolder(u64 data_id, GameListOpenTarget target) {
    std::string path;
    std::string open_target;

    switch (target) {
    case GameListOpenTarget::SAVE_DATA: {
        open_target = "Save Data";
        std::string sdmc_dir = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
        path = FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(sdmc_dir, data_id);
        break;
    }
    case GameListOpenTarget::EXT_DATA: {
        open_target = "Extra Data";
        std::string sdmc_dir = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
        path = FileSys::GetExtDataPathFromId(sdmc_dir, data_id);
        break;
    }
    case GameListOpenTarget::APPLICATION: {
        open_target = "Application";
        auto media_type = Service::AM::GetTitleMediaType(data_id);
        path = Service::AM::GetTitlePath(media_type, data_id) + "content/";
        break;
    }
    case GameListOpenTarget::UPDATE_DATA: {
        open_target = "Update Data";
        path = Service::AM::GetTitlePath(Service::FS::MediaType::SDMC, data_id + 0xe00000000) +
               "content/";
        break;
    }
    case GameListOpenTarget::TEXTURE_DUMP: {
        open_target = "Dumped Textures";
        path = fmt::format("{}textures/{:016X}/",
                           FileUtil::GetUserPath(FileUtil::UserPath::DumpDir), data_id);
        break;
    }
    case GameListOpenTarget::TEXTURE_LOAD: {
        open_target = "Custom Textures";
        path = fmt::format("{}textures/{:016X}/",
                           FileUtil::GetUserPath(FileUtil::UserPath::LoadDir), data_id);
        break;
    }
    case GameListOpenTarget::MODS: {
        open_target = "Mods";
        path = fmt::format("{}mods/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                           data_id);
        break;
    }
    case GameListOpenTarget::DLC_DATA: {
        open_target = "DLC Data";
        path = fmt::format("{}Nintendo 3DS/00000000000000000000000000000000/"
                           "00000000000000000000000000000000/title/0004008c/{:08x}/content/",
                           FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), data_id);
        break;
    }
    case GameListOpenTarget::SHADER_CACHE: {
        open_target = "Shader Cache";
        path = FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir);
        break;
    }
    default:
        LOG_ERROR(Frontend, "Unexpected target {}", static_cast<int>(target));
        return;
    }

    QString qpath = QString::fromStdString(path);

    QDir dir(qpath);
    if (!dir.exists()) {
        QMessageBox::critical(
            this, tr("Error Opening %1 Folder").arg(QString::fromStdString(open_target)),
            tr("Folder does not exist!"));
        return;
    }

    LOG_INFO(Frontend, "Opening {} path for data_id={:016x}", open_target, data_id);

    QDesktopServices::openUrl(QUrl::fromLocalFile(qpath));
}

void GMainWindow::OnGameListRemovePlayTimeData(u64 program_id) {
    if (QMessageBox::question(this, tr("Remove Play Time Data"), tr("Reset play time?"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    play_time_manager->ResetProgramPlayTime(program_id);
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

bool GMainWindow::CreateShortcutLink(const std::filesystem::path& shortcut_path,
                                     const std::string& comment,
                                     const std::filesystem::path& icon_path,
                                     const std::string& command, const std::string& arguments,
                                     const std::string& categories, const std::string& keywords,
                                     const std::string& name, const bool& skip_tryexec) try {
#if defined(__linux__) || defined(__FreeBSD__) // Linux and FreeBSD
    std::filesystem::path shortcut_path_full = shortcut_path / (name + ".desktop");
    std::ofstream shortcut_stream(shortcut_path_full, std::ios::binary | std::ios::trunc);
    if (!shortcut_stream.is_open()) {
        LOG_ERROR(Frontend, "Failed to create shortcut");
        return false;
    }
    // TODO: Migrate fmt::print to std::print in futures STD C++ 23.
    fmt::print(shortcut_stream, "[Desktop Entry]\n");
    fmt::print(shortcut_stream, "Type=Application\n");
    fmt::print(shortcut_stream, "Version=1.0\n");
    fmt::print(shortcut_stream, "Name={}\n", name);
    if (!comment.empty()) {
        fmt::print(shortcut_stream, "Comment={}\n", comment);
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        fmt::print(shortcut_stream, "Icon={}\n", icon_path.string());
    }
    if (!skip_tryexec) {
        fmt::print(shortcut_stream, "TryExec={}\n", command);
    }
    fmt::print(shortcut_stream, "Exec={} {}\n", command, arguments);
    if (!categories.empty()) {
        fmt::print(shortcut_stream, "Categories={}\n", categories);
    }
    if (!keywords.empty()) {
        fmt::print(shortcut_stream, "Keywords={}\n", keywords);
    }
    return true;
#elif defined(_WIN32) // Windows
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(Frontend, "CoInitialize failed");
        return false;
    }
    SCOPE_EXIT({ CoUninitialize(); });
    IShellLinkW* ps1 = nullptr;
    IPersistFile* persist_file = nullptr;
    SCOPE_EXIT({
        if (persist_file != nullptr) {
            persist_file->Release();
        }
        if (ps1 != nullptr) {
            ps1->Release();
        }
    });
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                    reinterpret_cast<void**>(&ps1));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to create IShellLinkW instance");
        return false;
    }
    hres = ps1->SetPath(Common::UTF8ToUTF16W(command).data());
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to set path");
        return false;
    }
    if (!arguments.empty()) {
        hres = ps1->SetArguments(Common::UTF8ToUTF16W(arguments).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set arguments");
            return false;
        }
    }
    if (!comment.empty()) {
        hres = ps1->SetDescription(Common::UTF8ToUTF16W(comment).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set description");
            return false;
        }
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        hres = ps1->SetIconLocation(icon_path.c_str(), 0);
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set icon location");
            return false;
        }
    }
    hres = ps1->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist_file));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to get IPersistFile interface");
        return false;
    }
    hres = persist_file->Save(
        std::filesystem::path{shortcut_path / (Common::UTF8ToUTF16W(name) + L".lnk")}.c_str(),
        TRUE);
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to save shortcut");
        return false;
    }
    return true;
#else                 // Unsupported platform
    return false;
#endif
} catch (const std::exception& e) {
    LOG_ERROR(Frontend, "Failed to create shortcut: {}", e.what());
    return false;
}

// Messages in pre-defined message boxes for less code spaghetti
bool GMainWindow::CreateShortcutMessagesGUI(QWidget* parent, int message,
                                            const QString& game_title) {
    int result = 0;
    QMessageBox::StandardButtons buttons;
    switch (message) {
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_FULLSCREEN_PROMPT:
        buttons = QMessageBox::Yes | QMessageBox::No;
        result = QMessageBox::information(
            parent, tr("Create Shortcut"),
            tr("Do you want to launch the application in fullscreen?"), buttons);
        return result == QMessageBox::Yes;
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_SUCCESS:
        QMessageBox::information(parent, tr("Create Shortcut"),
                                 tr("Successfully created a shortcut to %1").arg(game_title));
        return false;
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_APPIMAGE_VOLATILE_WARNING:
        buttons = QMessageBox::StandardButton::Ok | QMessageBox::StandardButton::Cancel;
        result =
            QMessageBox::warning(this, tr("Create Shortcut"),
                                 tr("This will create a shortcut to the current AppImage. This may "
                                    "not work well if you update. Continue?"),
                                 buttons);
        return result == QMessageBox::Ok;
    default:
        buttons = QMessageBox::Ok;
        QMessageBox::critical(parent, tr("Create Shortcut"),
                              tr("Failed to create a shortcut to %1").arg(game_title), buttons);
        return false;
    }
}

bool GMainWindow::MakeShortcutIcoPath(const u64 program_id, const std::string_view game_file_name,
                                      std::filesystem::path& out_icon_path) {
    // Get path to Citra icons directory & icon extension
    std::string ico_extension = "png";
#if defined(_WIN32)
    out_icon_path = FileUtil::GetUserPath(FileUtil::UserPath::IconsDir);
    ico_extension = "ico";
#elif defined(__linux__) || defined(__FreeBSD__)
    out_icon_path = FileUtil::GetUserDirectory("XDG_DATA_HOME") + "/icons/hicolor/256x256/";
#endif
    // Create icons directory if it doesn't exist
    if (!FileUtil::CreateFullPath(out_icon_path.string())) {
        QMessageBox::critical(
            this, tr("Create Icon"),
            tr("Cannot create icon file. Path \"%1\" does not exist and cannot be created.")
                .arg(QString::fromStdString(out_icon_path.string())),
            QMessageBox::StandardButton::Ok);
        out_icon_path.clear();
        return false;
    }

    // Create icon file path
    out_icon_path /= (program_id == 0 ? fmt::format("citra-{}.{}", game_file_name, ico_extension)
                                      : fmt::format("citra-{:016X}.{}", program_id, ico_extension));
    return true;
}

void GMainWindow::OnGameListCreateShortcut(u64 program_id, const std::string& game_path,
                                           GameListShortcutTarget target) {
    std::string citra_command{};
    bool skip_tryexec = false;
    const char* env_flatpak_id = getenv("FLATPAK_ID");
    if (env_flatpak_id) {
        citra_command = fmt::format("flatpak run {}", env_flatpak_id);
        skip_tryexec = true;
    } else {
        // Get path to Citra executable
        const QStringList args = QApplication::arguments();
        citra_command = args[0].toStdString();
        // If relative path, make it an absolute path
        if (citra_command.c_str()[0] == '.') {
            citra_command = FileUtil::GetCurrentDir().value_or("") + DIR_SEP + citra_command;
        }
    }

    // Shortcut path
    std::filesystem::path shortcut_path{};
    if (target == GameListShortcutTarget::Desktop) {
        shortcut_path =
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).toStdString();
    } else if (target == GameListShortcutTarget::Applications) {
        shortcut_path = GetApplicationsDirectory();
    }

    // Icon path and title
    if (!std::filesystem::exists(shortcut_path)) {
        CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_ERROR, {});
        LOG_ERROR(Frontend, "Invalid shortcut target");
        return;
    }

    // Get title from game file
    const auto loader = Loader::GetLoader(game_path);
    std::string game_title = fmt::format("{:016X}", program_id);
    if (loader->ReadTitle(game_title) != Loader::ResultStatus::Success) {
        game_title = fmt::format("{:016x}", program_id);
    }

    // Delete illegal characters from title
    const std::string illegal_chars = "<>:\"/\\|?*.";
    for (auto it = game_title.rbegin(); it != game_title.rend(); ++it) {
        if (illegal_chars.find(*it) != std::string::npos) {
            game_title.erase(it.base() - 1);
        }
    }

    // Get icon from game file
    std::vector<u8> icon_image_file;
    if (loader->ReadIcon(icon_image_file) != Loader::ResultStatus::Success) {
        LOG_WARNING(Frontend, "Could not read icon from {:s}", game_path);
    }

    const QPixmap pixmap = GetQPixmapFromSMDH(icon_image_file);
    const QImage icon_data = pixmap.toImage();
    std::filesystem::path out_icon_path;
    if (MakeShortcutIcoPath(program_id, game_title, out_icon_path)) {
        if (!SaveIconToFile(out_icon_path, icon_data)) {
            LOG_ERROR(Frontend, "Could not write icon to file");
        }
    }

    const auto qt_game_title = QString::fromStdString(game_title);
#if defined(__linux__)
    // Special case for AppImages
    // Warn once if we are making a shortcut to a volatile AppImage
    const std::string appimage_ending =
        std::string(Common::g_scm_rev).substr(0, 9).append(".AppImage");
    if (citra_command.ends_with(appimage_ending) && !UISettings::values.shortcut_already_warned) {
        if (CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_APPIMAGE_VOLATILE_WARNING,
                                      qt_game_title)) {
            return;
        }
        UISettings::values.shortcut_already_warned = true;
    }
#endif // __linux__
    // Create shortcut
    std::string arguments = fmt::format("\"{:s}\"", game_path);
    if (CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_FULLSCREEN_PROMPT, qt_game_title)) {
        arguments = "-f " + arguments;
    }
    const std::string comment = fmt::format("Start {:s} with the Azahar Emulator", game_title);
    const std::string categories = "Game;Emulator;Qt;";
    const std::string keywords = "3ds;Nintendo;";

    if (CreateShortcutLink(shortcut_path, comment, out_icon_path, citra_command, arguments,
                           categories, keywords, game_title, skip_tryexec)) {
        CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_SUCCESS, qt_game_title);
        return;
    }
    CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_ERROR, qt_game_title);
}

void GMainWindow::OnGameListDumpRomFS(QString game_path, u64 program_id) {
    auto* dialog = new QProgressDialog(tr("Dumping..."), tr("Cancel"), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowFlags(dialog->windowFlags() &
                           ~(Qt::WindowCloseButtonHint | Qt::WindowContextHelpButtonHint));
    dialog->setCancelButton(nullptr);
    dialog->setMinimumDuration(0);
    dialog->setValue(0);

    const auto base_path = fmt::format(
        "{}romfs/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir), program_id);
    const auto update_path =
        fmt::format("{}romfs/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                    program_id | 0x0004000e00000000);
    using FutureWatcher = QFutureWatcher<std::pair<Loader::ResultStatus, Loader::ResultStatus>>;
    auto* future_watcher = new FutureWatcher(this);
    connect(future_watcher, &FutureWatcher::finished, this,
            [this, dialog, base_path, update_path, future_watcher] {
                dialog->hide();
                const auto& [base, update] = future_watcher->result();
                if (base != Loader::ResultStatus::Success) {
                    QMessageBox::critical(
                        this, QStringLiteral("Azahar"),
                        tr("Could not dump base RomFS.\nRefer to the log for details."));
                    return;
                }
                QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(base_path)));
                if (update == Loader::ResultStatus::Success) {
                    QDesktopServices::openUrl(
                        QUrl::fromLocalFile(QString::fromStdString(update_path)));
                }
            });

    auto future = QtConcurrent::run([game_path, base_path, update_path] {
        std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(game_path.toStdString());
        return std::make_pair(loader->DumpRomFS(base_path), loader->DumpUpdateRomFS(update_path));
    });
    future_watcher->setFuture(future);
}

void GMainWindow::OnGameListOpenDirectory(const QString& directory) {
    QString path;
    if (directory == QStringLiteral("INSTALLED")) {
        path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) +
                                      "Nintendo "
                                      "3DS/00000000000000000000000000000000/"
                                      "00000000000000000000000000000000/title/00040000");
    } else if (directory == QStringLiteral("SYSTEM")) {
        path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir) +
                                      "00000000000000000000000000000000/title/00040010");
    } else {
        path = directory;
    }
    if (!QFileInfo::exists(path)) {
        QMessageBox::critical(this, tr("Error Opening %1").arg(path), tr("Folder does not exist!"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void GMainWindow::OnGameListAddDirectory() {
    const QString dir_path = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
    if (dir_path.isEmpty())
        return;
    UISettings::GameDir game_dir{dir_path, false, true};
    if (!UISettings::values.game_dirs.contains(game_dir)) {
        UISettings::values.game_dirs.append(game_dir);
        game_list->PopulateAsync(UISettings::values.game_dirs);
    } else {
        LOG_WARNING(Frontend, "Selected directory is already in the application list");
    }
}

void GMainWindow::OnGameListShowList(bool show) {
#ifdef ENABLE_OOT3D_PRESENTATION
    if (OOT3D::IsPlayerBuild()) {
        game_list->hide(); game_list_placeholder->hide(); return;
    }
#endif
#ifdef ENABLE_MM3D_GAME_FOLDER
    // Async game-list population must never reveal the emulator launcher.
    game_list->hide();
    game_list_placeholder->hide();
    return;
#endif
    if (emulation_running && ui->action_Single_Window_Mode->isChecked())
        return;
    game_list->setVisible(show);
    game_list_placeholder->setVisible(!show);
};

void GMainWindow::OnGameListOpenPerGameProperties(const QString& file) {
    const auto loader = Loader::GetLoader(file.toStdString());

    u64 title_id{};
    if (!loader || loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        QMessageBox::information(this, tr("Properties"),
                                 tr("The application properties could not be loaded."));
        return;
    }

    OpenPerGameConfiguration(title_id, file);
}

void GMainWindow::OnMenuLoadFile() {
    const QString extensions = QStringLiteral("*.").append(
        GameList::supported_file_extensions.join(QStringLiteral(" *.")));
    const QString file_filter = tr("3DS Executable") + QStringLiteral(" (%1)").arg(extensions) +
                                QStringLiteral(";;") + tr("All Files") + QStringLiteral(" (*.*)");
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load File"), UISettings::values.roms_path, file_filter);

    if (filename.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filename).path();
    BootGame(filename);
}

void GMainWindow::OnMenuSetUpSystemFiles() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Set Up System Files"));

    QVBoxLayout layout(&dialog);

    QLabel label_description(
        tr("<p>Azahar needs console unique data and firmware files from a real console to be "
           "able to use some of its features.<br>Such files and data can be set up with the <a "
           "href=https://github.com/azahar-emu/ArticSetupTool>Azahar "
           "Artic Setup Tool</a><br>Notes:<ul><li><b>This operation will install console unique "
           "data to Azahar, do not share your user or nand folders<br>after performing the setup "
           "process!</b></li><li>While doing the setup process, Azahar will link to the console "
           "running the setup tool. You can unlink the<br>console later from the System tab in the "
           "emulator configuration menu.</li><li>Do not go online with both Azahar and your 3DS "
           "console at the same time after setting up system files,<br>as it could cause "
           "issues.</li><li>Old 3DS setup is needed for the New 3DS setup to work (doing both "
           "setup modes is recommended).</li><li>Both setup modes will work regardless of the "
           "model of the console running the setup tool.</li></ul><hr></p>"),
        &dialog);
    label_description.setOpenExternalLinks(true);
    layout.addWidget(&label_description);

    QHBoxLayout layout_h(&dialog);
    layout.addLayout(&layout_h);

    QLabel label_enter(tr("Enter Azahar Artic Setup Tool address:"), &dialog);

    layout_h.addWidget(&label_enter);

    QLineEdit textInput(UISettings::values.last_artic_base_addr, &dialog);
    layout_h.addWidget(&textInput);

    QLabel label_select(QStringLiteral("<br>") + tr("Choose setup mode:"), &dialog);
    layout.addWidget(&label_select);

    std::pair<bool, bool> install_state = Core::AreSystemTitlesInstalled();

    QRadioButton radio1(&dialog);
    QRadioButton radio2(&dialog);
    QString new3dsSetupString = tr("New 3DS setup");
    QString old3dsSetupString = tr("Old 3DS setup");
    QString availableIcon = QStringLiteral("(\u2139\uFE0F) ");
    QString unavailableIcon = QStringLiteral("(\u26A0) ");
    QString installedIcon = QStringLiteral("(\u2705) ");
    if (!install_state.first) {
        radio1.setChecked(true);

        radio1.setText(availableIcon + old3dsSetupString);
        radio1.setToolTip(tr("Setup is possible."));

        radio2.setText(unavailableIcon + new3dsSetupString);
        radio2.setToolTip(tr("Old 3DS setup is required first."));
        radio2.setEnabled(false);
    } else {
        radio1.setText(installedIcon + old3dsSetupString);
        radio1.setToolTip(tr("Setup completed."));

        if (!install_state.second) {
            radio2.setChecked(true);

            radio2.setText(availableIcon + new3dsSetupString);
            radio2.setToolTip(tr("Setup is possible."));
        } else {
            radio1.setChecked(true);

            radio2.setText(installedIcon + new3dsSetupString);
            radio2.setToolTip(tr("Setup completed."));
        }
    }
    layout.addWidget(&radio1);
    layout.addWidget(&radio2);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout.addWidget(&buttonBox);

    int res = dialog.exec();
    if (res == QDialog::Accepted) {
        bool is_o3ds = radio1.isChecked();
        if ((is_o3ds && install_state.first) || (!is_o3ds && install_state.second)) {
            QMessageBox::StandardButton answer =
                QMessageBox::question(this, tr("Set Up System Files"),
                                      tr("The system files for the selected mode are already set "
                                         "up.\nReinstall the files anyway?"),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
        Core::UninstallSystemFiles(is_o3ds ? Core::SystemTitleSet::Old3ds
                                           : Core::SystemTitleSet::New3ds);
        QString addr = textInput.text();
        UISettings::values.last_artic_base_addr = addr;
        BootGame(QString::fromStdString(is_o3ds ? "articinio://" : "articinin://").append(addr));
    }
}

void GMainWindow::OnMenuInstallCIA() {
    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load Files"), UISettings::values.roms_path,
        tr("3DS Installation File") + QStringLiteral(" (*.cia *.zcia);;") + tr("All Files") +
            QStringLiteral(" (*.*)"));

    if (filepaths.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filepaths[0]).path();
    InstallCIA(filepaths);
}

void GMainWindow::OnMenuConnectArticBase() {
    bool ok = false;
    auto res = QInputDialog::getText(this, tr("Connect to Artic Base"),
                                     tr("Enter Artic Base server address:"), QLineEdit::Normal,
                                     UISettings::values.last_artic_base_addr, &ok);
    if (ok) {
        UISettings::values.last_artic_base_addr = res;
        BootGame(QString::fromStdString("articbase://").append(res));
    }
}

void GMainWindow::OnMenuBootHomeMenu(u32 region) {
    BootGame(QString::fromStdString(Core::GetHomeMenuNcchPath(region)));
}

void GMainWindow::InstallCIA(QStringList filepaths) {
    ui->action_Install_CIA->setEnabled(false);
    game_list->SetDirectoryWatcherEnabled(false);

    emit UpdateProgress(0, 0);

    (void)QtConcurrent::run([&, filepaths] {
        Service::AM::InstallStatus status;
        const auto cia_progress = [&](std::size_t written, std::size_t total) {
            emit UpdateProgress(written, total);
        };
        for (const auto& current_path : filepaths) {
            status = Service::AM::InstallCIA(current_path.toStdString(), cia_progress);
            emit CIAInstallReport(status, current_path);
        }
        emit CIAInstallFinished();
    });
}

void GMainWindow::OnUpdateProgress(std::size_t written, std::size_t total) {
    if (written == 0 and total == 0) {
        progress_bar->show();
        progress_bar->setValue(0);
        progress_bar->setMaximum(INT_MAX);
    }
    progress_bar->setValue(
        static_cast<int>(INT_MAX * (static_cast<double>(written) / static_cast<double>(total))));
}

void GMainWindow::OnCIAInstallReport(Service::AM::InstallStatus status, QString filepath) {
    QString filename = QFileInfo(filepath).fileName();
    switch (status) {
    case Service::AM::InstallStatus::Success:
        this->statusBar()->showMessage(tr("%1 has been installed successfully.").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorFailedToOpenFile:
        QMessageBox::critical(this, tr("Unable to open File"),
                              tr("Could not open %1").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorAborted:
        QMessageBox::critical(
            this, tr("Installation aborted"),
            tr("The installation of %1 was aborted. Please see the log for more details")
                .arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorInvalid:
        QMessageBox::critical(this, tr("Invalid File"), tr("%1 is not a valid CIA").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorEncrypted:
        QMessageBox::critical(this, tr("CIA Encrypted"),
                              tr("Your CIA file is encrypted.<br/>"
                                 "<a "
                                 "href='https://azahar-emu.org/blog/game-loading-changes/'>"
                                 "Please check our blog for more info.</a>"));
        break;
    case Service::AM::InstallStatus::ErrorFileNotFound:
        QMessageBox::critical(this, tr("Unable to find File"),
                              tr("Could not find %1").arg(filename));
        break;
    }
}

void GMainWindow::OnCompressFinished(bool is_compress, bool success) {
    progress_bar->hide();
    progress_bar->setValue(0);

    if (!success) {
        if (is_compress) {
            QMessageBox::critical(this, tr("Z3DS Compression"),
                                  tr("Failed to compress some files, check log for details."));
        } else {
            QMessageBox::critical(this, tr("Z3DS Compression"),
                                  tr("Failed to decompress some files, check log for details."));
        }
    } else {
        if (is_compress) {
            QMessageBox::information(this, tr("Z3DS Compression"),
                                     tr("All files have been compressed successfully."));
        } else {
            QMessageBox::information(this, tr("Z3DS Compression"),
                                     tr("All files have been decompressed successfully."));
        }
    }
}

void GMainWindow::OnCIAInstallFinished() {
    progress_bar->hide();
    progress_bar->setValue(0);
    game_list->SetDirectoryWatcherEnabled(true);
    ui->action_Install_CIA->setEnabled(true);
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void GMainWindow::UninstallTitles(
    const std::vector<std::tuple<Service::FS::MediaType, u64, QString>>& titles) {
    if (titles.empty()) {
        return;
    }

    // Select the first title in the list as representative.
    const auto first_name = std::get<QString>(titles[0]);

    QProgressDialog progress(tr("Uninstalling '%1'...").arg(first_name), tr("Cancel"), 0,
                             static_cast<int>(titles.size()), this);
    progress.setWindowModality(Qt::WindowModal);

    QFutureWatcher<void> future_watcher;
    QObject::connect(&future_watcher, &QFutureWatcher<void>::finished, &progress,
                     &QProgressDialog::reset);
    QObject::connect(&progress, &QProgressDialog::canceled, &future_watcher,
                     &QFutureWatcher<void>::cancel);
    QObject::connect(&future_watcher, &QFutureWatcher<void>::progressValueChanged, &progress,
                     &QProgressDialog::setValue);

    auto failed = false;
    QString failed_name;

    const auto uninstall_title = [&future_watcher, &failed, &failed_name](const auto& title) {
        const auto name = std::get<QString>(title);
        const auto media_type = std::get<Service::FS::MediaType>(title);
        const auto program_id = std::get<u64>(title);

        const auto result = Service::AM::UninstallProgram(media_type, program_id);
        if (result.IsError()) {
            LOG_ERROR(Frontend, "Failed to uninstall '{}': 0x{:08X}", name.toStdString(),
                      result.raw);
            failed = true;
            failed_name = name;
            future_watcher.cancel();
        }
    };

    future_watcher.setFuture(QtConcurrent::map(titles, uninstall_title));
    progress.exec();
    future_watcher.waitForFinished();

    if (failed) {
        QMessageBox::critical(this, QStringLiteral("Azahar"),
                              tr("Failed to uninstall '%1'.").arg(failed_name));
    } else if (!future_watcher.isCanceled()) {
        QMessageBox::information(this, QStringLiteral("Azahar"),
                                 tr("Successfully uninstalled '%1'.").arg(first_name));
        emit InstalledTitlesChanged();
    }
}

void GMainWindow::OnMenuRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    const QString filename = action->data().toString();
    if (QFileInfo::exists(filename)) {
        BootGame(filename);
    } else {
        // Display an error message and remove the file from the list.
        QMessageBox::information(this, tr("File not found"),
                                 tr("File \"%1\" not found").arg(filename));

        UISettings::values.recent_files.removeOne(filename);
        UpdateRecentFiles();
    }
}

void GMainWindow::OnResumeGame(bool first_start) {
    qt_cameras->ResumeCameras();

    PreventOSSleep();

    emu_thread->SetRunning(true);
    system.frame_limiter.SetFrameAdvancing(false);
    graphics_api_button->setEnabled(false);
    qRegisterMetaType<Core::System::ResultStatus>("Core::System::ResultStatus");
    qRegisterMetaType<std::string>("std::string");
    connect(emu_thread.get(), &EmuThread::ErrorThrown, this, &GMainWindow::OnCoreError);

    UpdateMenuState();

    play_time_manager->SetProgramId(game_title_id);
    play_time_manager->Start();

    if (first_start) {
#ifdef ENABLE_DISCORD_RPC
        discord_rpc->Update(true);
#endif
    }

#ifdef __unix__
    Common::Linux::StartGamemode();
#endif

    UpdateSaveStates();
    UpdateStatusButtons();
}

void GMainWindow::OnRestartGame() {
    if (!system.IsPoweredOn()) {
        return;
    }
    // Make a copy since BootGame edits game_path
    BootGame(QString(game_path));
}

void GMainWindow::OnPauseGame() {
    OOT3D::Rumble::running = false;
    OOT3D::Rumble::Clear();
    InputCommon::SetControllerRumble(0, 0, 0);
    system.frame_limiter.SetFrameAdvancing(true);
    qt_cameras->PauseCameras();

    play_time_manager->Stop();

    UpdateMenuState();
    AllowOSSleep();

#ifdef __unix__
    Common::Linux::StopGamemode();
#endif
}

void GMainWindow::OnPauseContinueGame() {
    if (emulation_running) {
        if (emu_thread->IsRunning() && !system.frame_limiter.IsFrameAdvancing()) {
            OnPauseGame();
        } else {
            OnResumeGame(false);
        }
    }
}

void GMainWindow::OnStopGame() {
    SetTurboEnabled(false);

    play_time_manager->Stop();
    // Update game list to show new play time
    game_list->PopulateAsync(UISettings::values.game_dirs);

    ShutdownGame();
    graphics_api_button->setEnabled(true);
    Settings::RestoreGlobalState(false);
    UpdateStatusButtons();
}

void GMainWindow::OnLoadComplete() {
    loading_screen->OnLoadComplete();
    UpdateSecondaryWindowVisibility();
}

void GMainWindow::ExitFullscreen() {
    if (!emulation_running) return;
    if (secondary_window->isActiveWindow()) {
        if (secondary_window->isFullScreen()) secondary_window->showNormal();
        return;
    }
    if (ui->action_Fullscreen->isChecked() || isFullScreen() || render_window->isFullScreen()) {
        ui->action_Fullscreen->setChecked(false);
        HideFullscreen();
    }
}

void GMainWindow::ToggleFullscreen() {
    if (!emulation_running) {
        return;
    }
    if (secondary_window->isVisible() && secondary_window->isActiveWindow()) {
        // undo the action and fullscreen secondary manually
        ui->action_Fullscreen->toggle();
        ToggleSecondaryFullscreen();
    } else {
        if (ui->action_Fullscreen->isChecked()) {
            ShowFullscreen();
        } else {
            HideFullscreen();
        }
    }
}

void GMainWindow::ToggleSecondaryFullscreen() {
    if (!emulation_running) {
        return;
    }
    if (secondary_window->isFullScreen()) {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(secondary_window, false);
#endif
        secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
        secondary_window->showNormal();
    } else {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(secondary_window, true);
#endif
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
        LOG_INFO(Frontend, "Attempting to fullscreen secondary window");
        secondary_window->showFullScreen();
    }
}

void GMainWindow::ShowFullscreen() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        UISettings::values.geometry = saveGeometry();
#if !defined(ENABLE_OOT3D_PRESENTATION) || !defined(__APPLE__)
        ui->menubar->hide();
#endif
        statusBar()->hide();
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(this, true);
#endif
        showFullScreen();
    } else {
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(render_window, true);
#endif
        render_window->showFullScreen();
    }
}

void GMainWindow::HideFullscreen() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
#ifdef ENABLE_OOT3D_PRESENTATION
        // Player builds clear the emulator menu and hide its non-native
        // widget. Showing that empty widget reserves a blank strip above
        // the game. Only the developer's native macOS menu is restored.
        ui->menubar->setVisible(!OOT3D::IsPlayerBuild() && ui->menubar->isNativeMenuBar());
#elif !defined(ENABLE_MM3D_GAME_FOLDER)
        ui->menubar->show();
#endif
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(this, false);
#endif
        showNormal();
        restoreGeometry(UISettings::values.geometry);
    } else {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(render_window, false);
#endif
        render_window->showNormal();
        render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
    }
}

void GMainWindow::ToggleWindowMode() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        // Render in the main window...
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
        ui->horizontalLayout->addWidget(render_window);
        render_window->setFocusPolicy(Qt::StrongFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->setFocus();
            game_list->hide();
        }

    } else {
        // Render in a separate window...
        ui->horizontalLayout->removeWidget(render_window);
        render_window->setParent(nullptr);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
            game_list->show();
        }
    }
}

void GMainWindow::UpdateSecondaryWindowVisibility() {
    if (!emulation_running) {
        return;
    }
    if (Settings::values.layout_option.GetValue() == Settings::LayoutOption::SeparateWindows) {
        secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
        secondary_window->show();
    } else {
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
        secondary_window->hide();
    }
    // make sure focus is on primary window whenever this changes
    if (UISettings::values.single_window_mode.GetValue()) {
        QApplication::setActiveWindow(this);
    } else {
        QApplication::setActiveWindow(render_window);
    }
}

void GMainWindow::ChangeScreenLayout() {
    Settings::LayoutOption new_layout = Settings::LayoutOption::Default;
    if (ui->action_Screen_Layout_Default->isChecked()) {
        new_layout = Settings::LayoutOption::Default;
    } else if (ui->action_Screen_Layout_Single_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::SingleScreen;
    } else if (ui->action_Screen_Layout_Large_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::LargeScreen;
        ui->menu_Small_Screen_Position->setEnabled(true);
    } else if (ui->action_Screen_Layout_Hybrid_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::HybridScreen;
    } else if (ui->action_Screen_Layout_Side_by_Side->isChecked()) {
        new_layout = Settings::LayoutOption::SideScreen;
    } else if (ui->action_Screen_Layout_Separate_Windows->isChecked()) {
        new_layout = Settings::LayoutOption::SeparateWindows;
    } else if (ui->action_Screen_Layout_Custom_Layout->isChecked()) {
        new_layout = Settings::LayoutOption::CustomLayout;
    }

    Settings::values.layout_option = new_layout;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

void GMainWindow::ChangeSmallScreenPosition() {
    Settings::SmallScreenPosition new_position = Settings::SmallScreenPosition::BottomRight;

    if (ui->action_Small_Screen_TopRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::TopRight;
    } else if (ui->action_Small_Screen_MiddleRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::MiddleRight;
    } else if (ui->action_Small_Screen_BottomRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::BottomRight;
    } else if (ui->action_Small_Screen_TopLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::TopLeft;
    } else if (ui->action_Small_Screen_MiddleLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::MiddleLeft;
    } else if (ui->action_Small_Screen_BottomLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::BottomLeft;
    } else if (ui->action_Small_Screen_Above->isChecked()) {
        new_position = Settings::SmallScreenPosition::AboveLarge;
    } else if (ui->action_Small_Screen_Below->isChecked()) {
        new_position = Settings::SmallScreenPosition::BelowLarge;
    }

    Settings::values.small_screen_position = new_position;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

bool GMainWindow::IsTurboEnabled() {
    return turbo_mode_active;
}

void GMainWindow::SetTurboEnabled(bool state) {
    turbo_mode_active = state;
    GMainWindow::ReloadTurbo();
}

void GMainWindow::ReloadTurbo() {
    if (IsTurboEnabled()) {
        Settings::temporary_frame_limit = Settings::values.turbo_limit.GetValue();
        Settings::is_temporary_frame_limit = true;
    } else {
        Settings::is_temporary_frame_limit = false;
    }

    UpdateStatusBar();
}

// TODO: This should probably take in something more descriptive than a bool. -OS
void GMainWindow::AdjustSpeedLimit(bool increase) {
    const int SPEED_LIMIT_STEP = 5;
    auto active_limit =
        IsTurboEnabled() ? &Settings::values.turbo_limit : &Settings::values.frame_limit;
    const auto active_limit_value = active_limit->GetValue();

    if (increase) {
        if (active_limit_value < 995) {
            active_limit->SetValue(active_limit_value + SPEED_LIMIT_STEP);
        }
    } else {
        if (active_limit_value > SPEED_LIMIT_STEP) {
            active_limit->SetValue(active_limit_value - SPEED_LIMIT_STEP);
        }
    }

    if (IsTurboEnabled()) {
        ReloadTurbo();
    }

    UpdateStatusBar();
}

void GMainWindow::ToggleScreenLayout() {
    const Settings::LayoutOption new_layout = []() {
        const Settings::LayoutOption current_layout = Settings::values.layout_option.GetValue();
        std::vector<Settings::LayoutOption> layouts_to_cycle =
            Settings::values.layouts_to_cycle.GetValue();
        const auto current_pos =
            distance(layouts_to_cycle.begin(),
                     std::find(layouts_to_cycle.begin(), layouts_to_cycle.end(), current_layout));
        // if the layouts_to_cycle setting has somehow been
        // cleared out, add just default back in
        if (layouts_to_cycle.size() == 0) {
            layouts_to_cycle.push_back(Settings::LayoutOption::Default);
        }
        if (current_pos >= layouts_to_cycle.size() - 1) {
            // either this layout wasn't found or it was last so move to the beginning
            return layouts_to_cycle[0];
        } else {
            return layouts_to_cycle[current_pos + 1];
        }
    }();

    Settings::values.layout_option = new_layout;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

void GMainWindow::OnSwapScreens() {
    Settings::values.swap_screen = ui->action_Screen_Layout_Swap_Screens->isChecked();
    system.ApplySettings();
}

void GMainWindow::OnRotateScreens() {
    Settings::values.upright_screen = ui->action_Screen_Layout_Upright_Screens->isChecked();
    system.ApplySettings();
}

void GMainWindow::TriggerSwapScreens() {
    ui->action_Screen_Layout_Swap_Screens->trigger();
}

void GMainWindow::TriggerRotateScreens() {
    ui->action_Screen_Layout_Upright_Screens->trigger();
}

void GMainWindow::OnSaveState() {
    if (!system.IsPoweredOn()) {
        return;
    }

    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    system.SendSignal(Core::System::Signal::Save, action->data().toUInt());
    system.frame_limiter.AdvanceFrame();
    newest_slot = action->data().toUInt();
}

void GMainWindow::OnLoadState() {
    if (!system.IsPoweredOn()) {
        return;
    }

    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    if (UISettings::values.save_state_warning) {
        QMessageBox::warning(
            this, tr("Savestates"),
            tr("Warning: Savestates are NOT a replacement for in-application saves, "
               "and are not meant to be reliable.\n\nUse at your own risk!"));
        UISettings::values.save_state_warning = false;
        config->Save();
    }

    system.SendSignal(Core::System::Signal::Load, action->data().toUInt());
    system.frame_limiter.AdvanceFrame();
}

void GMainWindow::OnConfigureControls() {
    const auto old_profiles=Settings::values.input_profiles;
    const auto old_current=Settings::values.current_input_profile;
    const auto old_touch_maps=Settings::values.touch_from_button_maps;
    const int old_profile=Settings::values.current_input_profile_index;
    QDialog dialog(this);
#ifdef ENABLE_MM3D_PRESENTATION
    dialog.setWindowTitle(tr("Dekui controls"));
#else
    dialog.setWindowTitle(tr("Controller / keyboard"));
#endif
    auto* layout=new QVBoxLayout(&dialog);
    auto* input=new ConfigureInput(system,&dialog);layout->addWidget(input);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted) {
        input->ApplyConfiguration();input->ApplyProfile();system.ApplySettings();config->Save();
    } else {
        Settings::values.input_profiles=old_profiles;
        Settings::values.touch_from_button_maps=old_touch_maps;
        Settings::LoadProfile(old_profile);
        Settings::values.current_input_profile=old_current;
    }
    render_window->setFocus();
}

void GMainWindow::OnConfigure() {
#ifdef ENABLE_OOT3D_PRESENTATION
    if (OOT3D::IsPlayerBuild()) { OnConfigureControls(); return; }
#endif
    game_list->SetDirectoryWatcherEnabled(false);
    Settings::SetConfiguringGlobal(true);
    ConfigureDialog configureDialog(this, hotkey_registry, system, gl_renderer, physical_devices,
                                    !multiplayer_state->IsHostingPublicRoom());
    connect(&configureDialog, &ConfigureDialog::LanguageChanged, this,
            &GMainWindow::OnLanguageChanged);
    auto old_theme = UISettings::values.theme;
    const int old_input_profile_index = Settings::values.current_input_profile_index;
    const auto old_input_profiles = Settings::values.input_profiles;
    const auto old_touch_from_button_maps = Settings::values.touch_from_button_maps;
#ifdef ENABLE_DISCORD_RPC
    const bool old_discord_presence = UISettings::values.enable_discord_presence.GetValue();
#endif
#ifdef __unix__
    const bool old_gamemode = Settings::values.enable_gamemode.GetValue();
#endif
    auto result = configureDialog.exec();
    game_list->SetDirectoryWatcherEnabled(true);
    if (result == QDialog::Accepted) {
        configureDialog.ApplyConfiguration();
        InitializeHotkeys();
        if (UISettings::values.theme != old_theme) {
            UpdateUITheme();
        }
#ifdef ENABLE_DISCORD_RPC
        if (UISettings::values.enable_discord_presence.GetValue() != old_discord_presence) {
            SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
            discord_rpc->Update(system.IsPoweredOn());
        }
#endif
#ifdef __unix__
        if (Settings::values.enable_gamemode.GetValue() != old_gamemode) {
            SetGamemodeEnabled(Settings::values.enable_gamemode.GetValue());
        }
#endif
        if (!multiplayer_state->IsHostingPublicRoom())
            multiplayer_state->UpdateCredentials();
        emit UpdateThemedIcons();
        SyncMenuUISettings();
        game_list->RefreshGameDirectory();
        config->Save();
        if (UISettings::values.hide_mouse && emulation_running) {
            setMouseTracking(true);
            mouse_hide_timer.start();
        } else {
            setMouseTracking(false);
        }
        ReloadTurbo();
        UpdateSecondaryWindowVisibility();
        UpdateBootHomeMenuState();
        UpdateStatusButtons();
    } else {
        Settings::values.input_profiles = old_input_profiles;
        Settings::values.touch_from_button_maps = old_touch_from_button_maps;
        Settings::LoadProfile(old_input_profile_index);
    }
}

void GMainWindow::OnLoadAmiibo() {
    if (!emu_thread || !emu_thread->IsRunning()) [[unlikely]] {
        return;
    }

    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc == nullptr) {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    if (nfc->IsTagActive()) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("A tag is already in use."));
        return;
    }

    if (!nfc->IsSearchingForAmiibos()) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("Application is not looking for amiibos."));
        return;
    }

    const QString extensions{QStringLiteral("*.bin")};
    const QString file_filter = tr("Amiibo File") + QStringLiteral(" (%1);;").arg(extensions) +
                                tr("All Files") + QStringLiteral(" (*.*)");
    const QString filename = QFileDialog::getOpenFileName(this, tr("Load Amiibo"), {}, file_filter);

    if (filename.isEmpty()) {
        return;
    }

    LoadAmiibo(filename);
}

void GMainWindow::LoadAmiibo(const QString& filename) {
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (!nfc) [[unlikely]] {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    if (!nfc->LoadAmiibo(filename.toStdString())) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("Unable to open amiibo file \"%1\" for reading.").arg(filename));
        return;
    }

    ui->action_Remove_Amiibo->setEnabled(true);
}

void GMainWindow::OnRemoveAmiibo() {
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (!nfc) [[unlikely]] {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    nfc->RemoveAmiibo();
    ui->action_Remove_Amiibo->setEnabled(false);
}

void GMainWindow::OnOpenCitraFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::UserDir))));
}

void GMainWindow::OnOpenNANDFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir))));
}

void GMainWindow::OnOpenSDMCFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir))));
}

void GMainWindow::OnToggleFilterBar() {
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());
    if (ui->action_Show_Filter_Bar->isChecked()) {
        game_list->SetFilterFocus();
    } else {
        game_list->ClearFilter();
    }
}

void GMainWindow::OnCreateGraphicsSurfaceViewer() {
    auto graphicsSurfaceViewerWidget =
        new GraphicsSurfaceWidget(system, Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsSurfaceViewerWidget);
    // TODO: Maybe graphicsSurfaceViewerWidget->setFloating(true);
    graphicsSurfaceViewerWidget->show();
}

void GMainWindow::OnRecordMovie() {
    MovieRecordDialog dialog(this, system);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    movie_record_on_start = true;
    movie_record_path = dialog.GetPath();
    movie_record_author = dialog.GetAuthor();

    if (emulation_running) { // Restart game
        BootGame(QString(game_path));
    }
    ui->action_Close_Movie->setEnabled(true);
    ui->action_Save_Movie->setEnabled(true);
}

void GMainWindow::OnPlayMovie() {
    MoviePlayDialog dialog(this, game_list, system);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    movie_playback_on_start = true;
    movie_playback_path = dialog.GetMoviePath();
    BootGame(dialog.GetGamePath());

    ui->action_Close_Movie->setEnabled(true);
    ui->action_Save_Movie->setEnabled(false);
}

void GMainWindow::OnCloseMovie() {
    if (movie_record_on_start) {
        QMessageBox::information(this, tr("Record Movie"), tr("Movie recording cancelled."));
        movie_record_on_start = false;
        movie_record_path.clear();
        movie_record_author.clear();
    } else {
        const bool was_running = emu_thread && emu_thread->IsRunning();
        if (was_running) {
            OnPauseGame();
        }

        const bool was_recording = movie.GetPlayMode() == Core::Movie::PlayMode::Recording;
        movie.Shutdown();
        if (was_recording) {
            QMessageBox::information(this, tr("Movie Saved"),
                                     tr("The movie is successfully saved."));
        }

        if (was_running) {
            OnResumeGame(false);
        }
    }

    ui->action_Close_Movie->setEnabled(false);
    ui->action_Save_Movie->setEnabled(false);
}

void GMainWindow::OnSaveMovie() {
    const bool was_running = emu_thread && emu_thread->IsRunning();
    if (was_running) {
        OnPauseGame();
    }

    if (movie.GetPlayMode() == Core::Movie::PlayMode::Recording) {
        movie.SaveMovie();
        QMessageBox::information(this, tr("Movie Saved"), tr("The movie is successfully saved."));
    } else {
        LOG_ERROR(Frontend, "Tried to save movie while movie is not being recorded");
    }

    if (was_running) {
        OnResumeGame(false);
    }
}

void GMainWindow::OnCaptureScreenshot() {
    if (!emu_thread) [[unlikely]] {
        return;
    }

    const bool was_running = emu_thread->IsRunning();

    if (was_running || (QMessageBox::question(this, tr("Application will unpause"),
                                              tr("The application will be unpaused, and the next "
                                                 "frame will be captured. Is this okay?"),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No) == QMessageBox::Yes)) {
        if (was_running) {
            OnPauseGame();
        }
        std::string path = UISettings::values.screenshot_path.GetValue();
        if (!FileUtil::IsDirectory(path)) {
            if (!FileUtil::CreateFullPath(path)) {
                QMessageBox::information(
                    this, tr("Invalid Screenshot Directory"),
                    tr("Cannot create specified screenshot directory. Screenshot "
                       "path is set back to its default value."));
                path = FileUtil::GetUserPath(FileUtil::UserPath::UserDir);
                path.append("screenshots/");
                UISettings::values.screenshot_path = path;
            };
        }

        static QRegularExpression expr(QStringLiteral("[\\/:?\"<>|]"));
        const std::string filename = game_title.remove(expr).toStdString();
        const std::string timestamp = QDateTime::currentDateTime()
                                          .toString(QStringLiteral("dd.MM.yy_hh.mm.ss.z"))
                                          .toStdString();
        path.append(fmt::format("/{}_{}.png", filename, timestamp));
        auto* const screenshot_window =
            secondary_window->HasFocus() ? secondary_window : render_window;
        screenshot_window->CaptureScreenshot(
            UISettings::values.screenshot_resolution_factor.GetValue(),
            QString::fromStdString(path));
        OnResumeGame(false);
    }
}

void GMainWindow::ShowFFmpegErrorMessage() {
    QMessageBox message_box;
    message_box.setWindowTitle(tr("Could not load video dumper"));
    message_box.setText(
        tr("FFmpeg could not be loaded. Make sure you have a compatible version installed."
#ifdef _WIN32
           "\n\nTo install FFmpeg to Azahar, press Open and select your FFmpeg directory."
#endif
           "\n\nTo view a guide on how to install FFmpeg, press Help."));
    message_box.setStandardButtons(QMessageBox::Ok | QMessageBox::Help
#ifdef _WIN32
                                   | QMessageBox::Open
#endif
    );
    auto result = message_box.exec();
    if (result == QMessageBox::Help) {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/azahar-emu/azahar/wiki/Installing-FFmpeg")));
#ifdef _WIN32
    } else if (result == QMessageBox::Open) {
        OnOpenFFmpeg();
#endif
    }
}

void GMainWindow::OnDumpVideo() {
    if (DynamicLibrary::FFmpeg::LoadFFmpeg()) {
        if (ui->action_Dump_Video->isChecked()) {
            OnStartVideoDumping();
        } else {
            OnStopVideoDumping();
        }
    } else {
        ui->action_Dump_Video->setChecked(false);
        ShowFFmpegErrorMessage();
    }
}

void GMainWindow::OnCompressFile() {
    // NOTE: Encrypted files SHOULD NEVER be compressed, otherwise the resulting
    // compressed file will have very poor compression ratios, due to the high
    // entropy caused by encryption. This may cause confusion to the user as they
    // will see the files do not compress well and blame the emulator.
    //
    // This is enforced using the loaders as they already return an error on encryption.

    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load 3DS ROM Files"), UISettings::values.roms_path,
        tr("3DS ROM Files") + QStringLiteral(" (*.cia *.cci *.3dsx *.cxi *.3ds);;") +
            tr("All Files") + QStringLiteral(" (*.*)"));

    QString out_path;

    if (filepaths.isEmpty()) {
        return;
    }

    bool single_file = filepaths.size() == 1;
    if (single_file) {
        // If it's a single file, ask the user for the output file.
        auto compress_info = Loader::GetCompressFileInfo(filepaths[0].toStdString(), true);
        if (!compress_info.has_value()) {
            emit CompressFinished(true, false);
            return;
        }

        QFileInfo fileinfo(filepaths[0]);
        QString final_path =
            fileinfo.path() + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
            QStringLiteral(".") +
            QString::fromStdString(compress_info.value().first.recommended_compressed_extension);

        QString out_filter = tr("3DS Compressed ROM File") +
                             QStringLiteral(" (*.%1)").arg(QString::fromStdString(
                                 compress_info.value().first.recommended_compressed_extension));
        out_path = QFileDialog::getSaveFileName(this, tr("Save 3DS Compressed ROM File"),
                                                final_path, out_filter);
        if (out_path.isEmpty()) {
            return;
        }
    } else {
        // Otherwise, ask the user the directory to output the files.
        out_path = QFileDialog::getExistingDirectory(
            this, tr("Select Output 3DS Compressed ROM Folder"), UISettings::values.roms_path,
            QFileDialog::ShowDirsOnly);
        if (out_path.isEmpty()) {
            return;
        }
    }

    (void)QtConcurrent::run([&, filepaths, out_path] {
        bool single_file = filepaths.size() == 1;
        QString out_filepath;
        bool total_success = true;

        for (const QString& filepath : filepaths) {

            std::string in_path = filepath.toStdString();

            // Identify file type
            auto compress_info = Loader::GetCompressFileInfo(filepath.toStdString(), true);
            if (!compress_info.has_value()) {
                total_success = false;
                continue;
            }

            if (single_file) {
                out_filepath = out_path;
            } else {
                QFileInfo fileinfo(filepath);
                out_filepath = out_path + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
                               QStringLiteral(".") +
                               QString::fromStdString(
                                   compress_info.value().first.recommended_compressed_extension);
            }

            std::string out_path = out_filepath.toStdString();

            emit UpdateProgress(0, 0);

            const auto progress = [&](std::size_t written, std::size_t total) {
                emit UpdateProgress(written, total);
            };
            bool success = FileUtil::CompressZ3DSFile(in_path, out_path,
                                                      compress_info.value().first.underlying_magic,
                                                      compress_info.value().second, progress,
                                                      compress_info.value().first.default_metadata);
            if (!success) {
                total_success = false;
                FileUtil::Delete(out_path);
            }
        }

        emit CompressFinished(true, total_success);
    });
}

void GMainWindow::OnDecompressFile() {

    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load 3DS Compressed ROM Files"), UISettings::values.roms_path,
        tr("3DS Compressed ROM Files") + QStringLiteral(" (*.zcia *zcci *z3dsx *zcxi)") +
            QStringLiteral(";;") + tr("All Files") + QStringLiteral(" (*.*)"));

    QString out_path;

    if (filepaths.isEmpty()) {
        return;
    }

    bool single_file = filepaths.size() == 1;
    if (single_file) {
        // If it's a single file, ask the user for the output file.
        auto compress_info = Loader::GetCompressFileInfo(filepaths[0].toStdString(), false);
        if (!compress_info.has_value()) {
            emit CompressFinished(false, false);
            return;
        }

        QFileInfo fileinfo(filepaths[0]);
        QString final_path =
            fileinfo.path() + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
            QStringLiteral(".") +
            QString::fromStdString(compress_info.value().first.recommended_uncompressed_extension);

        QString out_filter = tr("3DS ROM File") +
                             QStringLiteral(" (*.%1)").arg(QString::fromStdString(
                                 compress_info.value().first.recommended_uncompressed_extension));
        out_path =
            QFileDialog::getSaveFileName(this, tr("Save 3DS ROM File"), final_path, out_filter);
        if (out_path.isEmpty()) {
            return;
        }
    } else {
        // Otherwise, ask the user the directory to output the files.
        out_path = QFileDialog::getExistingDirectory(this, tr("Select Output 3DS ROM Folder"),
                                                     UISettings::values.roms_path,
                                                     QFileDialog::ShowDirsOnly);
        if (out_path.isEmpty()) {
            return;
        }
    }

    (void)QtConcurrent::run([&, filepaths, out_path] {
        bool single_file = filepaths.size() == 1;
        QString out_filepath;
        bool total_success = true;

        for (const QString& filepath : filepaths) {

            std::string in_path = filepath.toStdString();

            // Identify file type
            auto compress_info = Loader::GetCompressFileInfo(filepath.toStdString(), false);
            if (!compress_info.has_value()) {
                total_success = false;
                continue;
            }

            if (single_file) {
                out_filepath = out_path;
            } else {
                QFileInfo fileinfo(filepath);
                out_filepath = out_path + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
                               QStringLiteral(".") +
                               QString::fromStdString(
                                   compress_info.value().first.recommended_uncompressed_extension);
            }

            std::string out_path = out_filepath.toStdString();

            emit UpdateProgress(0, 0);

            const auto progress = [&](std::size_t written, std::size_t total) {
                emit UpdateProgress(written, total);
            };

            // TODO(PabloMK7): What should we do with the metadata?
            bool success = FileUtil::DeCompressZ3DSFile(in_path, out_path, progress);
            if (!success) {
                total_success = false;
                FileUtil::Delete(out_path);
            }
        }

        emit CompressFinished(false, total_success);
    });
}

#ifdef _WIN32
void GMainWindow::OnOpenFFmpeg() {
    auto filename =
        QFileDialog::getExistingDirectory(this, tr("Select FFmpeg Directory")).toStdString();
    if (filename.empty()) {
        return;
    }
    // Check for a bin directory if they chose the FFmpeg root directory.
    auto bin_dir = filename + DIR_SEP + "bin";
    if (!FileUtil::Exists(bin_dir)) {
        // Otherwise, assume the user directly selected the directory containing the DLLs.
        bin_dir = filename;
    }

    static const std::array library_names = {
        Common::DynamicLibrary::GetLibraryName("avcodec", LIBAVCODEC_VERSION_MAJOR),
        Common::DynamicLibrary::GetLibraryName("avfilter", LIBAVFILTER_VERSION_MAJOR),
        Common::DynamicLibrary::GetLibraryName("avformat", LIBAVFORMAT_VERSION_MAJOR),
        Common::DynamicLibrary::GetLibraryName("avutil", LIBAVUTIL_VERSION_MAJOR),
        Common::DynamicLibrary::GetLibraryName("swresample", LIBSWRESAMPLE_VERSION_MAJOR),
    };

    for (auto& library_name : library_names) {
        if (!FileUtil::Exists(bin_dir + DIR_SEP + library_name)) {
            QMessageBox::critical(this, QStringLiteral("Azahar"),
                                  tr("The provided FFmpeg directory is missing %1. Please make "
                                     "sure the correct directory was selected.")
                                      .arg(QString::fromStdString(library_name)));
            return;
        }
    }

    std::atomic<bool> success(true);
    auto process_file = [&success](u64* num_entries_out, const std::string& directory,
                                   const std::string& virtual_name) -> bool {
        auto file_path = directory + DIR_SEP + virtual_name;
        if (file_path.ends_with(".dll")) {
            auto destination_path = FileUtil::GetExeDirectory() + DIR_SEP + virtual_name;
            if (!FileUtil::Copy(file_path, destination_path)) {
                success.store(false);
                return false;
            }
        }
        return true;
    };
    FileUtil::ForeachDirectoryEntry(nullptr, bin_dir, process_file);

    if (success.load()) {
        QMessageBox::information(this, QStringLiteral("Azahar"),
                                 tr("FFmpeg has been sucessfully installed."));
    } else {
        QMessageBox::critical(this, QStringLiteral("Azahar"),
                              tr("Installation of FFmpeg failed. Check the log file for details."));
    }
}
#endif

void GMainWindow::OnStartVideoDumping() {
    DumpingDialog dialog(this, system);
    if (dialog.exec() != QDialog::DialogCode::Accepted) {
        ui->action_Dump_Video->setChecked(false);
        return;
    }
    const auto path = dialog.GetFilePath();
    if (emulation_running) {
        StartVideoDumping(path);
    } else {
        video_dumping_on_start = true;
        video_dumping_path = path;
    }
}

void GMainWindow::StartVideoDumping(const QString& path) {
    auto& renderer = system.GPU().Renderer();
    const auto layout{Layout::FrameLayoutFromResolutionScale(renderer.GetResolutionScaleFactor())};

    auto dumper = std::make_shared<VideoDumper::FFmpegBackend>(renderer);
    if (dumper->StartDumping(path.toStdString(), layout)) {
        system.RegisterVideoDumper(dumper);
    } else {
        QMessageBox::critical(
            this, QStringLiteral("Azahar"),
            tr("Could not start video dumping.<br>Please ensure that the video encoder is "
               "configured correctly.<br>Refer to the log for details."));
        ui->action_Dump_Video->setChecked(false);
    }
}

void GMainWindow::OnStopVideoDumping() {
    ui->action_Dump_Video->setChecked(false);

    if (video_dumping_on_start) {
        video_dumping_on_start = false;
        video_dumping_path.clear();
    } else {
        auto dumper = system.GetVideoDumper();
        if (!dumper || !dumper->IsDumping()) {
            return;
        }

        game_paused_for_dumping = emu_thread->IsRunning();
        OnPauseGame();

        auto future = QtConcurrent::run([dumper] { dumper->StopDumping(); });
        auto* future_watcher = new QFutureWatcher<void>(this);
        connect(future_watcher, &QFutureWatcher<void>::finished, this, [this] {
            if (game_shutdown_delayed) {
                game_shutdown_delayed = false;
                ShutdownGame();
            } else if (game_paused_for_dumping) {
                game_paused_for_dumping = false;
                OnResumeGame(false);
            }
        });
        future_watcher->setFuture(future);
    }
}

void GMainWindow::UpdateStatusBar() {
    if (!emu_thread) [[unlikely]] {
        status_bar_update_timer.stop();
        return;
    }

    // Update movie status
    const u64 current = movie.GetCurrentInputIndex();
    const u64 total = movie.GetTotalInputCount();
    const auto play_mode = movie.GetPlayMode();
    if (play_mode == Core::Movie::PlayMode::Recording) {
        message_label->setText(tr("Recording %1").arg(current));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(true);
    } else if (play_mode == Core::Movie::PlayMode::Playing) {
        message_label->setText(tr("Playing %1 / %2").arg(current).arg(total));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(false);
    } else if (play_mode == Core::Movie::PlayMode::MovieFinished) {
        message_label->setText(tr("Movie Finished"));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(false);
    } else if (message_label_used_for_movie) { // Clear the label if movie was just closed
        message_label->setText(QString{});
        message_label_used_for_movie = false;
        ui->action_Save_Movie->setEnabled(false);
    }

    auto results = system.GetAndResetPerfStats();

    if (show_artic_label) {
        const bool do_mb = results.artic_transmitted >= (1000.0 * 1000.0);
        const double value = do_mb ? (results.artic_transmitted / (1000.0 * 1000.0))
                                   : (results.artic_transmitted / 1000.0);
        static const std::array<std::pair<Core::PerfStats::PerfArticEventBits, QString>, 5>
            perf_events = {
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SHARED_EXT_DATA,
                               tr("(Accessing SharedExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SYSTEM_SAVE_DATA,
                               tr("(Accessing SystemSaveData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_BOSS_EXT_DATA,
                               tr("(Accessing BossExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_EXT_DATA,
                               tr("(Accessing ExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SAVE_DATA,
                               tr("(Accessing SaveData)")),
            };

        const QString unit = do_mb ? QStringLiteral("MB/s") : QStringLiteral("KB/s");
        QString event{};
        for (auto p : perf_events) {
            if (results.artic_events.Get(p.first)) {
                event = QString::fromStdString(" ") + p.second;
                break;
            }
        }

        static const std::array label_color = {QStringLiteral(""), QStringLiteral("#eed202"),
                                               QStringLiteral("#ff3333")};

        int style_index;

        if (value > 200.0) {
            style_index = 2;
        } else if (value > 125.0) {
            style_index = 1;
        } else {
            style_index = 0;
        }

        QString style_sheet;
        if (!label_color[style_index].isEmpty()) {
            style_sheet = QStringLiteral("QLabel { color: %0; }").arg(label_color[style_index]);
        }

        artic_traffic_label->setText(
            tr("Artic Traffic: %1 %2%3").arg(value, 0, 'f', 0).arg(unit).arg(event));
        artic_traffic_label->setStyleSheet(style_sheet);
    }

    if (Settings::GetFrameLimit() == 0) {
        emu_speed_label->setText(tr("Speed: %1%").arg(results.emulation_speed * 100.0, 0, 'f', 0));
    } else {
        emu_speed_label->setText(tr("Speed: %1% / %2%")
                                     .arg(results.emulation_speed * 100.0, 0, 'f', 0)
                                     .arg(Settings::GetFrameLimit()));
    }
    game_fps_label->setText(tr("App: %1 FPS").arg(results.game_fps, 0, 'f', 0));
    if (UISettings::values.show_advanced_frametime_info) {
        emu_frametime_label->setText(
            tr("Frame: %1 ms (GPU: [CMD: %2 ms, SWP: %3 ms], IPC: %4 ms, SVC: %5 ms, Rem: %6 ms)")
                .arg(results.time_vblank_interval * 1000.0, 2, 'f', 2)
                .arg(results.time_gpu * 1000.0, 2, 'f', 2)
                .arg(results.time_swap * 1000.0, 2, 'f', 2)
                .arg(results.time_hle_ipc * 1000.0, 2, 'f', 2)
                .arg(results.time_hle_svc * 1000.0, 2, 'f', 2)
                .arg(results.time_remaining * 1000.0, 2, 'f', 2));
    } else {
        emu_frametime_label->setText(
            tr("Frame: %1 ms").arg(results.time_vblank_interval * 1000.0, 2, 'f', 2));
    }

    if (show_artic_label) {
        artic_traffic_label->setVisible(true);
    }
    emu_speed_label->setVisible(true);
    game_fps_label->setVisible(true);
    emu_frametime_label->setVisible(true);
}

void GMainWindow::UpdateBootHomeMenuState() {
    const auto current_region = Settings::values.region_value.GetValue();
    for (u32 region = 0; region < Core::NUM_SYSTEM_TITLE_REGIONS; region++) {
        const auto path = Core::GetHomeMenuNcchPath(region);
        ui->menu_Boot_Home_Menu->actions().at(region)->setEnabled(
            (current_region == Settings::REGION_VALUE_AUTO_SELECT ||
             current_region == static_cast<int>(region)) &&
            !path.empty() && FileUtil::Exists(path));
    }
}

void GMainWindow::HideMouseCursor() {
    if (!emu_thread || !UISettings::values.hide_mouse.GetValue()) {
        mouse_hide_timer.stop();
        ShowMouseCursor();
        return;
    }
    render_window->setCursor(QCursor(Qt::BlankCursor));
    secondary_window->setCursor(QCursor(Qt::BlankCursor));
    if (UISettings::values.single_window_mode.GetValue()) {
        setCursor(QCursor(Qt::BlankCursor));
    }
}

void GMainWindow::ShowMouseCursor() {
    unsetCursor();
    render_window->unsetCursor();
    secondary_window->unsetCursor();
    if (emu_thread && UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }
}

void GMainWindow::OnMute() {
    Settings::values.audio_muted = !Settings::values.audio_muted;
    UpdateVolumeUI();
}

void GMainWindow::OnDecreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume =
        static_cast<s32>(Settings::values.volume.GetValue() * volume_slider->maximum());
    int step = 5;
    if (current_volume <= 30) {
        step = 2;
    }
    if (current_volume <= 6) {
        step = 1;
    }
    const auto value =
        static_cast<float>(std::max(current_volume - step, 0)) / volume_slider->maximum();
    Settings::values.volume.SetValue(value);
    UpdateVolumeUI();
}

void GMainWindow::OnIncreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume =
        static_cast<s32>(Settings::values.volume.GetValue() * volume_slider->maximum());
    int step = 5;
    if (current_volume < 30) {
        step = 2;
    }
    if (current_volume < 6) {
        step = 1;
    }
    const auto value = static_cast<float>(current_volume + step) / volume_slider->maximum();
    Settings::values.volume.SetValue(value);
    UpdateVolumeUI();
}

void GMainWindow::UpdateVolumeUI() {
    const auto volume_value =
        static_cast<int>(Settings::values.volume.GetValue() * volume_slider->maximum());
    volume_slider->setValue(volume_value);
    if (Settings::values.audio_muted) {
        volume_button->setChecked(false);
        volume_button->setText(tr("VOLUME: MUTE"));
    } else {
        volume_button->setChecked(true);
        volume_button->setText(tr("VOLUME: %1%", "Volume percentage (e.g. 50%)").arg(volume_value));
    }
}

void GMainWindow::UpdateAPIIndicator(bool update) {
    static std::array graphics_apis = {QStringLiteral("SOFTWARE"), QStringLiteral("OPENGL"),
                                       QStringLiteral("VULKAN")};

    static std::array graphics_api_colors = {QStringLiteral("#3ae400"), QStringLiteral("#00ccdd"),
                                             QStringLiteral("#91242a")};

    u32 api_index = static_cast<u32>(Settings::values.graphics_api.GetValue());
    if (update) {
        api_index = (api_index + 1) % graphics_apis.size();
        // Skip past any disabled renderers.
#ifndef ENABLE_SOFTWARE_RENDERER
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::Software)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#endif
#ifndef ENABLE_OPENGL
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::OpenGL)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#endif
#ifndef ENABLE_VULKAN
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::Vulkan)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#else
        if (physical_devices.empty()) {
            if (api_index == static_cast<u32>(Settings::GraphicsAPI::Vulkan)) {
                api_index = (api_index + 1) % graphics_apis.size();
            }
        }
#endif
        Settings::values.graphics_api = static_cast<Settings::GraphicsAPI>(api_index);
    }

    const QString style_sheet = QStringLiteral("QPushButton { font-weight: bold; color: %0; }")
                                    .arg(graphics_api_colors[api_index]);

    graphics_api_button->setText(graphics_apis[api_index]);
    graphics_api_button->setStyleSheet(style_sheet);
}

void GMainWindow::UpdateStatusButtons() {
    UpdateAPIIndicator();
    UpdateVolumeUI();
}

void GMainWindow::OnMouseActivity() {
    ShowMouseCursor();
}

void GMainWindow::mouseMoveEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::mousePressEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::showEvent([[maybe_unused]] QShowEvent* event) {
    game_list->LoadCompatibilityList();
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

bool GMainWindow::ShowExceptionDialog(Core::System::ResultStatus result,
                                      const std::string& details) {
    QDialog dialog(this);
    dialog.setWindowTitle(result == Core::System::ResultStatus::ErrorCoreExceptionRaised
                              ? tr("An exception occurred")
                              : tr("An invalid memory access occurred"));

    dialog.setMinimumSize(600, 500);

    auto* layout = new QVBoxLayout(&dialog);

    auto* label = new QLabel(
        result == Core::System::ResultStatus::ErrorCoreExceptionRaised
            ? tr("An exception occurred while executing the emulated application.")
            : tr("An invalid memory access occurred while executing the emulated application."));

    layout->addWidget(label);

    auto* textEdit = new QPlainTextEdit();
    textEdit->setPlainText(QString::fromStdString(details));
    textEdit->setReadOnly(true);
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(10);
    textEdit->setFont(monoFont);
    textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(textEdit);

    auto* buttonLayout = new QHBoxLayout();

    auto* ignoreButton = new QPushButton(tr("Ignore for this Session"));

    QObject::connect(ignoreButton, &QPushButton::clicked,
                     []() { Core::SetIgnoreExceptionsForSession(true); });

    buttonLayout->addWidget(ignoreButton);
    buttonLayout->addStretch();

    auto* continueButton = new QPushButton(tr("Continue"));
    QObject::connect(continueButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonLayout->addWidget(continueButton);

    auto* stopButton = new QPushButton(tr("Stop Emulation"));
    QObject::connect(stopButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(stopButton);

    layout->addLayout(buttonLayout);

    return dialog.exec() == QDialog::Accepted;
}

void GMainWindow::OnCoreError(Core::System::ResultStatus result, std::string details) {
    QString status_message;

    if (result == Core::System::ResultStatus::ErrorGraphicsAllocation ||
        result == Core::System::ResultStatus::ErrorInvalidResource) {
        if (emu_thread) {
            ShutdownGame();
        }
        QMessageBox::critical(this, result == Core::System::ResultStatus::ErrorInvalidResource
                                  ? tr("Invalid Game Resource") : tr("Graphics Memory Allocation Failed"),
                              QString::fromStdString(details));
        return;
    }

    // Handle exception dialogs separately
    if (result == Core::System::ResultStatus::ErrorCoreExceptionRaised) {
        if (ShowExceptionDialog(result, details)) {
            if (emu_thread) {
                ShutdownGame();
                return;
            }
        }

        if (emu_thread) {
            emu_thread->SetRunning(true);
            message_label->setText(status_message);
            message_label_used_for_movie = false;
        }
        return;
    }

    QString title, message;
    QMessageBox::Icon error_severity_icon;
    bool can_continue = true;
    if (result == Core::System::ResultStatus::ErrorSystemFiles) {
        const QString common_message =
            tr("%1 is missing. Please <a "
               "href='https://github.com/azahar-emu/azahar/wiki/Dumping-System-Files'>dump your "
               "system archives</a>.<br/>Continuing emulation may result in crashes and bugs.");

        if (!details.empty()) {
            message = common_message.arg(QString::fromStdString(details));
        } else {
            message = common_message.arg(tr("A system archive"));
        }

        title = tr("System Archive Not Found");
        status_message = tr("System Archive Missing");
        error_severity_icon = QMessageBox::Icon::Critical;
    } else if (result == Core::System::ResultStatus::ErrorSavestate) {
        title = tr("Save/load Error");
        message = QString::fromStdString(details);
        error_severity_icon = QMessageBox::Icon::Warning;
    } else if (result == Core::System::ResultStatus::ErrorArticDisconnected) {
        title = tr("Artic Server");
        message =
            tr(fmt::format("A communication error has occurred. The game will quit.\n{}", details)
                   .c_str());
        error_severity_icon = QMessageBox::Icon::Critical;
        can_continue = false;
    } else if (result == Core::System::ResultStatus::ErrorSavestateBuildMismatch) {
        title = tr("Savestate version mismatch");
        message = tr("Could not load savestate because it was created on a different Azahar "
                     "version:<br/>"
                     "<b>Azahar %1</b>.<br/><br/>Please read our blog entry <a "
                     "href='https://azahar-emu.org/blog/understanding-save-states/'>understanding "
                     "savestates</a> for more information.<br/><br/>To recover your progress, "
                     "downgrade to <b>Azahar %1</b>, load this savestate and use the application's "
                     "built-in save functionality.")
                      .arg(QString::fromStdString(details));
        error_severity_icon = QMessageBox::Icon::Critical;
    } else {
        title = tr("Fatal Error");
        message = tr("A fatal error occurred. "
                     "Check the log for details."
                     "<br/>Continuing emulation may result in crashes and bugs.");
        status_message = tr("Fatal Error encountered");
        error_severity_icon = QMessageBox::Icon::Critical;
    }

    QMessageBox message_box;
    message_box.setWindowTitle(title);
    message_box.setText(message);
    message_box.setIcon(error_severity_icon);
    if (error_severity_icon == QMessageBox::Icon::Critical) {
        if (can_continue) {
            message_box.addButton(tr("Continue"), QMessageBox::RejectRole);
        }
        QPushButton* abort_button =
            message_box.addButton(tr("Quit Application"), QMessageBox::AcceptRole);
        if (result != Core::System::ResultStatus::ShutdownRequested)
            message_box.exec();

        if (!can_continue || result == Core::System::ResultStatus::ShutdownRequested ||
            message_box.clickedButton() == abort_button) {
            if (emu_thread) {
                ShutdownGame();
                return;
            }
        }
    } else {
        // This block should run when the error isn't too big of a deal
        // e.g. when a save state can't be saved or loaded
        message_box.addButton(tr("OK"), QMessageBox::RejectRole);
        message_box.exec();
    }

    // Only show the message if the game is still running.
    if (emu_thread) {
        emu_thread->SetRunning(true);
        message_label->setText(status_message);
        message_label_used_for_movie = false;
    }
}

void GMainWindow::OnMenuAboutCitra() {
    AboutDialog about{this};
    about.exec();
}

bool GMainWindow::ConfirmClose() {
#if defined(ENABLE_OOT3D_PRESENTATION) || defined(ENABLE_MM3D_PRESENTATION)
    // Closing the game window is already an explicit exit request. Keep the
    // normal shutdown path, without the inherited emulator confirmation.
    return true;
#else
    if (close_already_confirmed || !emu_thread || !UISettings::values.confirm_before_closing) {
        return true;
    }

    QMessageBox::StandardButton answer =
        QMessageBox::question(this, QStringLiteral("Azahar"), tr("Would you like to exit now?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
#endif
}

void GMainWindow::closeEvent(QCloseEvent* event) {
    if (!ConfirmClose()) {
        event->ignore();
        return;
    }

    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    hotkey_registry.SaveHotkeys();

    // Shutdown session if the emu thread is active...
    if (emu_thread) {
        ShutdownGame();
    }

    // Save settings in case they were changed from outside the configuration menu.
    config->Save();

    render_window->close();
    secondary_window->close();
    multiplayer_state->Close();
    InputCommon::Shutdown();
    QWidget::closeEvent(event);
}

static bool IsSingleFileDropEvent(const QMimeData* mime) {
    return mime->hasUrls() && mime->urls().length() == 1;
}

static const std::array<std::string, 11> AcceptedExtensions = {
    "cci", "cxi", "bin", "3dsx", "app", "elf", "axf", "zcci", "zcxi", "z3dsx", "3ds"};

static bool IsCorrectFileExtension(const QMimeData* mime) {
    const QString& filename = mime->urls().at(0).toLocalFile();
    return std::find(AcceptedExtensions.begin(), AcceptedExtensions.end(),
                     QFileInfo(filename).suffix().toStdString()) != AcceptedExtensions.end();
}

static bool IsAcceptableDropEvent(QDropEvent* event) {
    return IsSingleFileDropEvent(event->mimeData()) && IsCorrectFileExtension(event->mimeData());
}

void GMainWindow::AcceptDropEvent(QDropEvent* event) {
    if (IsAcceptableDropEvent(event)) {
        event->setDropAction(Qt::DropAction::LinkAction);
        event->accept();
    }
}

bool GMainWindow::DropAction(QDropEvent* event) {
    if (!IsAcceptableDropEvent(event)) {
        return false;
    }

    const QMimeData* mime_data = event->mimeData();
    const QString& filename = mime_data->urls().at(0).toLocalFile();

    if (emulation_running && QFileInfo(filename).suffix() == QStringLiteral("bin")) {
        // Amiibo
        LoadAmiibo(filename);
    } else {
        // Game
        if (ConfirmChangeGame()) {
            BootGame(filename);
        }
    }
    return true;
}

void GMainWindow::OnFileOpen(const QFileOpenEvent* event) {
    BootGame(event->file());
}

void GMainWindow::dropEvent(QDropEvent* event) {
    DropAction(event);
}

void GMainWindow::dragEnterEvent(QDragEnterEvent* event) {
    AcceptDropEvent(event);
}

void GMainWindow::dragMoveEvent(QDragMoveEvent* event) {
    AcceptDropEvent(event);
}

bool GMainWindow::ConfirmChangeGame() {
    if (!emu_thread) [[unlikely]] {
        return true;
    }

    auto answer = QMessageBox::question(
        this, QStringLiteral("Azahar"),
        tr("The application is still running. Would you like to stop emulation?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
}

void GMainWindow::filterBarSetChecked(bool state) {
    ui->action_Show_Filter_Bar->setChecked(state);
    emit(OnToggleFilterBar());
}

inline bool isDarkMode() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Use colorScheme for Qt 6.5 and later
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark;
#else
    // Fallback for Qt 6.4: Check the window palette
    QPalette palette = QGuiApplication::palette();
    return palette.color(QPalette::Window).lightness() < 128; // Rough check for dark mode
#endif
}

void GMainWindow::UpdateUITheme() {
    const QString icons_base_path = QStringLiteral(":/icons/");
    QString default_theme;
    if (!isDarkMode()) {
        default_theme = QStringLiteral("default");
    } else {
        default_theme = QStringLiteral("default_with_light_icons");
    }

    const QString default_theme_path = icons_base_path + default_theme;

    const QString& current_theme = UISettings::values.theme;
    const bool is_default_theme = current_theme == QString::fromUtf8(UISettings::themes[0].second);
    QStringList theme_paths(default_theme_paths);

    if (is_default_theme || current_theme.isEmpty()) {
        const QString theme_uri(QStringLiteral(":default/style.qss"));
        QFile f(theme_uri);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
            setStyleSheet(ts.readAll());
        } else {
            LOG_ERROR(Frontend,
                      "Unable to open default stylesheet, falling back to empty stylesheet");
            qApp->setStyleSheet({});
            setStyleSheet({});
        }
        theme_paths.append(default_theme_path);
        QIcon::setThemeName(default_theme);
    } else {
        const QString theme_uri(QLatin1Char{':'} + current_theme + QStringLiteral("/style.qss"));
        QFile f(theme_uri);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
            setStyleSheet(ts.readAll());
        } else {
            LOG_ERROR(Frontend, "Unable to set style, stylesheet file not found");
        }

        const QString current_theme_path = icons_base_path + current_theme;
        theme_paths.append({default_theme_path, current_theme_path});
        QIcon::setThemeName(current_theme);
    }

    QIcon::setThemeSearchPaths(theme_paths);
}

void GMainWindow::LoadTranslation() {
    bool loaded = false;

    const QString lang_en = QStringLiteral("en");
    const QString languages_dir = QStringLiteral(":/languages/");

    // Workaround for incorrect Qt system language detection
    // TODO: Allow the "<System>" option to actually be selected rather than overriding the
    //       selected language option? Current behaviour is better than the issue it fixes,
    //       but not ideal.
    if (UISettings::values.language.isEmpty()) {
        QStringList languages;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        languages = QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore);
#else
        languages = QLocale::system().uiLanguages();
        for (auto& lang : languages)
            lang.replace(u'-', u'_');
#endif
        for (const auto& lang : languages) {
            // If the first language found is English, no need to install any translation
            if (lang == lang_en) {
                UISettings::values.language = lang_en;
                return;
            }
            loaded = citraTranslator.load(lang, languages_dir);
            if (loaded) {
                UISettings::values.language = lang;
                break;
            }
        }
    }

    // If the selected language is English, no need to install any translation
    if (UISettings::values.language == lang_en) {
        return;
    }

    const QString qtbase_prefix = QStringLiteral("qtbase_");
    if (UISettings::values.language.isEmpty() && !loaded) {
        // Use the system's default locale
        qtTranslator.load(qtbase_prefix + QLocale::system().name(), {}, {},
                          QStringLiteral(":/languages/"));
        loaded = citraTranslator.load(QLocale::system(), {}, {}, QStringLiteral(":/languages/"));
    } else {
        // Otherwise load from the specified file
        qtTranslator.load(qtbase_prefix + UISettings::values.language,
                          QStringLiteral(":/languages/"));
        loaded = citraTranslator.load(UISettings::values.language, QStringLiteral(":/languages/"));
    }

    if (loaded) {
        qApp->installTranslator(&qtTranslator);
        qApp->installTranslator(&citraTranslator);
    } else {
        UISettings::values.language = lang_en;
    }
}

void GMainWindow::OnLanguageChanged(const QString& locale) {
    if (UISettings::values.language != QStringLiteral("en")) {
        qApp->removeTranslator(&qtTranslator);
        qApp->removeTranslator(&citraTranslator);
    }

    UISettings::values.language = locale;
    LoadTranslation();
    ui->retranslateUi(this);
    RetranslateStatusBar();
    UpdateWindowTitle();
}

void GMainWindow::OnConfigurePerGame() {
#ifdef ENABLE_OOT3D_PRESENTATION
    if (OOT3D::IsPlayerBuild()) return;
#endif
    u64 title_id{};
    system.GetAppLoader().ReadProgramId(title_id);
    OpenPerGameConfiguration(title_id, game_path);
}

void GMainWindow::OpenPerGameConfiguration(u64 title_id, const QString& file_name) {
    Settings::SetConfiguringGlobal(false);
    ConfigurePerGame dialog(this, title_id, file_name, gl_renderer, physical_devices, system);
    const auto result = dialog.exec();

    if (result != QDialog::Accepted) {
        Settings::RestoreGlobalState(system.IsPoweredOn());
        return;
    } else if (result == QDialog::Accepted) {
        dialog.ApplyConfiguration();
    }

    // Do not cause the global config to write local settings into the config file
    const bool is_powered_on = system.IsPoweredOn();
    Settings::RestoreGlobalState(system.IsPoweredOn());

    if (!is_powered_on) {
        config->Save();
    }

    UpdateStatusButtons();
}

void GMainWindow::OnMoviePlaybackCompleted() {
    OnPauseGame();
    QMessageBox::information(this, tr("Playback Completed"), tr("Movie playback completed."));
}

#ifdef ENABLE_QT_UPDATE_CHECKER
void GMainWindow::OnEmulatorUpdateAvailable() {
    QString version_string = update_future.result();
    if (version_string.isEmpty())
        return;

    QMessageBox update_prompt(this);
    update_prompt.setWindowTitle(tr("Update Available"));
    update_prompt.setIcon(QMessageBox::Information);
    update_prompt.addButton(QMessageBox::Yes);
    update_prompt.addButton(QMessageBox::Ignore);
    update_prompt.setText(tr("Update %1 for Azahar is available.\nWould you like to download it?")
                              .arg(version_string));
    update_prompt.exec();
    if (update_prompt.button(QMessageBox::Yes) == update_prompt.clickedButton()) {
        std::string update_page_url;
        if (ShouldCheckForPrereleaseUpdates()) {
            update_page_url = "https://github.com/azahar-emu/azahar/releases";
        } else {
            update_page_url = "https://azahar-emu.org/pages/download/";
        }
        QDesktopServices::openUrl(QUrl(QString::fromStdString(update_page_url)));
    }
}
#endif

void GMainWindow::OnSwitchDiskResources(VideoCore::LoadCallbackStage stage, std::size_t value,
                                        std::size_t total, const std::string& object) {
    if (stage == VideoCore::LoadCallbackStage::Prepare) {
        loading_shaders_label->setText(QString());
        loading_shaders_label->setVisible(true);
    } else if (stage == VideoCore::LoadCallbackStage::Complete) {
        loading_shaders_label->setVisible(false);
    } else {
        loading_shaders_label->setText(
            loading_screen->GetStageTranslation(stage, value, total, object));
    }
}

void GMainWindow::UpdateWindowTitle() {
#ifdef ENABLE_OOT3D_PRESENTATION
    setWindowTitle(QStringLiteral("SariaUI"));
    if (render_window) render_window->setWindowTitle(QStringLiteral("SariaUI"));
    if (secondary_window) secondary_window->setWindowTitle(QStringLiteral("SariaUI"));
    return;
#endif
#ifdef ENABLE_MM3D_GAME_FOLDER
    setWindowTitle(QStringLiteral("Dekui"));
    if(render_window)render_window->setWindowTitle(QStringLiteral("Dekui"));
    return;
#endif
    const QString full_name = QString::fromUtf8(Common::g_build_fullname);

    if (game_title.isEmpty()) {
        setWindowTitle(QStringLiteral("Azahar %1").arg(full_name));
    } else {
        setWindowTitle(QStringLiteral("Azahar %1 | %2").arg(full_name, game_title));
        render_window->setWindowTitle(
            QStringLiteral("Azahar %1 | %2 | %3").arg(full_name, game_title, tr("Primary Window")));
        secondary_window->setWindowTitle(QStringLiteral("Azahar %1 | %2 | %3")
                                             .arg(full_name, game_title, tr("Secondary Window")));
    }
}

void GMainWindow::UpdateUISettings() {
    if (!ui->action_Fullscreen->isChecked()) {
        UISettings::values.geometry = saveGeometry();
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
    }
    if (!secondary_window->isFullScreen()) {
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
    }
    UISettings::values.state = saveState();
#if MICROPROFILE_ENABLED
    UISettings::values.microprofile_geometry = microProfileDialog->saveGeometry();
    UISettings::values.microprofile_visible = microProfileDialog->isVisible();
#endif
    UISettings::values.single_window_mode = ui->action_Single_Window_Mode->isChecked();
    UISettings::values.fullscreen = ui->action_Fullscreen->isChecked();
    UISettings::values.display_titlebar = ui->action_Display_Dock_Widget_Headers->isChecked();
    UISettings::values.show_filter_bar = ui->action_Show_Filter_Bar->isChecked();
    UISettings::values.show_status_bar = ui->action_Show_Status_Bar->isChecked();
    UISettings::values.first_start = false;
}

void GMainWindow::SyncMenuUISettings() {
    ui->action_Screen_Layout_Default->setChecked(Settings::values.layout_option.GetValue() ==
                                                 Settings::LayoutOption::Default);
    ui->action_Screen_Layout_Single_Screen->setChecked(Settings::values.layout_option.GetValue() ==
                                                       Settings::LayoutOption::SingleScreen);
    ui->action_Screen_Layout_Large_Screen->setChecked(Settings::values.layout_option.GetValue() ==
                                                      Settings::LayoutOption::LargeScreen);
    ui->action_Screen_Layout_Hybrid_Screen->setChecked(Settings::values.layout_option.GetValue() ==
                                                       Settings::LayoutOption::HybridScreen);
    ui->action_Screen_Layout_Side_by_Side->setChecked(Settings::values.layout_option.GetValue() ==
                                                      Settings::LayoutOption::SideScreen);
    ui->action_Screen_Layout_Separate_Windows->setChecked(
        Settings::values.layout_option.GetValue() == Settings::LayoutOption::SeparateWindows);
    ui->action_Screen_Layout_Custom_Layout->setChecked(Settings::values.layout_option.GetValue() ==
                                                       Settings::LayoutOption::CustomLayout);
    ui->action_Screen_Layout_Swap_Screens->setChecked(Settings::values.swap_screen.GetValue());
    ui->action_Screen_Layout_Upright_Screens->setChecked(
        Settings::values.upright_screen.GetValue());

    ui->menu_Small_Screen_Position->setEnabled(Settings::values.layout_option.GetValue() ==
                                               Settings::LayoutOption::LargeScreen);

    ui->action_Small_Screen_TopRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::TopRight);
    ui->action_Small_Screen_MiddleRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::MiddleRight);
    ui->action_Small_Screen_BottomRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::BottomRight);
    ui->action_Small_Screen_TopLeft->setChecked(Settings::values.small_screen_position.GetValue() ==
                                                Settings::SmallScreenPosition::TopLeft);
    ui->action_Small_Screen_MiddleLeft->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::MiddleLeft);
    ui->action_Small_Screen_BottomLeft->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::BottomLeft);
    ui->action_Small_Screen_Above->setChecked(Settings::values.small_screen_position.GetValue() ==
                                              Settings::SmallScreenPosition::AboveLarge);
    ui->action_Small_Screen_Below->setChecked(Settings::values.small_screen_position.GetValue() ==
                                              Settings::SmallScreenPosition::BelowLarge);
}

void GMainWindow::RetranslateStatusBar() {
    if (emu_thread)
        UpdateStatusBar();

    emu_speed_label->setToolTip(tr("Current emulation speed. Values higher or lower than 100% "
                                   "indicate emulation is running faster or slower than a 3DS."));
    game_fps_label->setToolTip(tr("How many frames per second the app is currently displaying. "
                                  "This will vary from app to app and scene to scene."));
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a 3DS frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    multiplayer_state->retranslateUi();
}

#ifdef ENABLE_DISCORD_RPC
void GMainWindow::SetDiscordEnabled([[maybe_unused]] bool state) {
    if (state) {
        discord_rpc = std::make_unique<DiscordRPC::DiscordImpl>(system);
    } else {
        discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
    }
}
#endif

#ifdef __unix__
void GMainWindow::SetGamemodeEnabled(bool state) {
    if (emulation_running) {
        Common::Linux::SetGamemodeState(state);
    }
}
#endif

#ifdef main
#undef main
#endif

static Qt::HighDpiScaleFactorRoundingPolicy GetHighDpiRoundingPolicy() {
#ifdef _WIN32
    // For Windows, we want to avoid scaling artifacts on fractional scaling ratios.
    // This is done by setting the optimal scaling policy for the primary screen.

    // Create a temporary QApplication.
    int temp_argc = 0;
    char** temp_argv = nullptr;
    QApplication temp{temp_argc, temp_argv};

    // Get the current screen geometry.
    const QScreen* primary_screen = QGuiApplication::primaryScreen();
    if (!primary_screen) {
        return Qt::HighDpiScaleFactorRoundingPolicy::PassThrough;
    }

    const QRect screen_rect = primary_screen->geometry();
    const qreal real_ratio = primary_screen->devicePixelRatio();
    const qreal real_width = std::trunc(screen_rect.width() * real_ratio);
    const qreal real_height = std::trunc(screen_rect.height() * real_ratio);

    // Recommended minimum width and height for proper window fit.
    // Any screen with a lower resolution than this will still have a scale of 1.
    constexpr qreal minimum_width = 1350.0;
    constexpr qreal minimum_height = 900.0;

    const qreal width_ratio = std::max(1.0, real_width / minimum_width);
    const qreal height_ratio = std::max(1.0, real_height / minimum_height);

    // Get the lower of the 2 ratios and truncate, this is the maximum integer scale.
    const qreal max_ratio = std::trunc(std::min(width_ratio, height_ratio));

    if (max_ratio > real_ratio) {
        return Qt::HighDpiScaleFactorRoundingPolicy::Round;
    } else {
        return Qt::HighDpiScaleFactorRoundingPolicy::Floor;
    }
#else
    // Other OSes should be better than Windows at fractional scaling.
    return Qt::HighDpiScaleFactorRoundingPolicy::PassThrough;
#endif
}

int LaunchQtFrontend(int argc, char* argv[]) {
#ifdef __APPLE__
    // Ensure that the linker doesn't optimize qt_swizzle.mm out of existence.
    QtSwizzle::Dummy();
#endif

#if MICROPROFILE_ENABLED
    MicroProfileOnThreadCreate("Frontend");
    SCOPE_EXIT({ MicroProfileShutdown(); });
#endif

    // Init settings params
    QCoreApplication::setOrganizationName(QStringLiteral("Azahar Developers"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("azahar_emu.org"));
    QCoreApplication::setApplicationName(QStringLiteral("Azahar"));
#ifdef ENABLE_MM3D_GAME_FOLDER
    QCoreApplication::setApplicationName(QStringLiteral("Dekui"));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.tgacquin.dekui"));
#else
    QGuiApplication::setDesktopFileName(QStringLiteral("org.azahar_emu.Azahar"));
#endif

    auto rounding_policy = GetHighDpiRoundingPolicy();
    QApplication::setHighDpiScaleFactorRoundingPolicy(rounding_policy);

#ifdef __APPLE__
    auto bundle_dir = FileUtil::GetBundleDirectory();
    if (bundle_dir) {
        FileUtil::SetCurrentDir(bundle_dir.value() + "..");
    }
#endif

#ifdef ENABLE_OPENGL
    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

#ifdef ENABLE_OOT3D_PRESENTATION
    QCoreApplication::setApplicationName(QStringLiteral("SariaUI"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("SariaUI"));
    if (OOT3D::IsPlayerBuild()) {
        QGuiApplication::setDesktopFileName(QStringLiteral("io.github.tgacquin.sariaui"));
    }
#endif
    QApplication app(argc, argv);
    // Loader checks must not create configuration or require a game.
    if (app.arguments().contains(QStringLiteral("--version")) ||
        app.arguments().contains(QStringLiteral("-v"))) {
        std::cout << QCoreApplication::applicationName().toStdString() << " "
                  << Common::g_build_fullname
#ifdef ENABLE_OOT3D_PRESENTATION
                  << (OOT3D::IsPlayerBuild() ? " [player]" : " [developer]")
#endif
                  << std::endl;
        return 0;
    }
#ifdef ENABLE_OOT3D_PRESENTATION
    // Exit before constructing any window, configuration, logger or user folder.
    if (OoT3D::GameFolder::Rom().isEmpty()) return 0;
    // Set before QtConfig/logging is constructed; never inherit the MM3D user directory.
    const QString oot_user = OoT3D::GameFolder::User();
    if (!QDir().mkpath(oot_user)) {
        QMessageBox::critical(nullptr, QStringLiteral("SariaUI"), QStringLiteral("Cannot create the user folder."));
        return 1;
    }
    const QString oot_user_absolute = QDir(oot_user).absolutePath() + QLatin1Char('/');
    FileUtil::SetUserPath(oot_user_absolute.toStdString());
    Q_INIT_RESOURCE(release_defaults);
    if (!OOT3D::ReleaseDefaults::Prepare(
            QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir)))) {
        QMessageBox::critical(nullptr, QStringLiteral("SariaUI"),
                              QStringLiteral("Cannot prepare default settings."));
        return 1;
    }
#endif
#ifdef ENABLE_MM3D_GAME_FOLDER
    if (MM3D::GameFolder::Rom().isEmpty()) {
        QMessageBox::information(nullptr, QStringLiteral("Game required"), MM3D::GameFolder::MissingGameMessage());
        return 1;
    }
    if (const auto error=MM3D::GameFolder::Prepare(); !error.isEmpty()) {
        QMessageBox::critical(nullptr, QStringLiteral("Dekui setup"), error);
        return 1;
    }
    if (const auto error=MM3D::GameFolder::Validate(); !error.isEmpty()) {
        QMessageBox::critical(nullptr, QStringLiteral("Dekui setup"), error);
        return 1;
    }
    FileUtil::SetUserPath(MM3D::GameFolder::User().toStdString());
#endif

    // Required when using .qrc resources from within a static library.
    // See https://doc.qt.io/qt-5/resources.html#using-resources-in-a-library
    Q_INIT_RESOURCE(compatibility_list);
    Q_INIT_RESOURCE(theme_colorful);
    Q_INIT_RESOURCE(theme_colorful_dark);
    Q_INIT_RESOURCE(theme_colorful_midnight_blue);
    Q_INIT_RESOURCE(theme_default);
    Q_INIT_RESOURCE(theme_qdarkstyle);
    Q_INIT_RESOURCE(theme_qdarkstyle_midnight_blue);
#ifdef ENABLE_QT_TRANSLATION
    Q_INIT_RESOURCE(languages);
#endif

    // Qt changes the locale and causes issues in float conversion using std::to_string() when
    // generating shaders
    setlocale(LC_ALL, "C");

    auto& system{Core::System::GetInstance()};

    // Register Qt image interface
    system.RegisterImageInterface(std::make_shared<QtImageInterface>());

    GMainWindow main_window(system);

    // Register frontend applets
    Frontend::RegisterDefaultApplets(system);

    system.RegisterMiiSelector(std::make_shared<QtMiiSelector>(main_window));
    system.RegisterSoftwareKeyboard(std::make_shared<QtKeyboard>(main_window));

#ifdef __APPLE__
    // Register microphone permission check.
    system.RegisterMicPermissionCheck(&AppleAuthorization::CheckAuthorizationForMicrophone);
#endif

    main_window.show();

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &main_window,
                     &GMainWindow::OnAppFocusStateChanged);

    // Process any pending events before executing the app (prevents freeze-on–boot on macOS)
    app.processEvents();

    int result = app.exec();
    return result;
}
