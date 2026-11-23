// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/release_defaults.h"
#include "oot3d/world_map_assets.h"
#include "oot3d/pause_prompts.h"
#include "common/file_util.h"
#include <QFile>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>

static void Check(bool condition) {
    if (!condition) std::abort();
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    Q_INIT_RESOURCE(release_defaults);
    QTemporaryDir temporary;
    Check(temporary.isValid());
    const auto root = temporary.path();
    using namespace OOT3D::ReleaseDefaults;
    Check(Prepare(root + "/fresh", false));
    QSettings qt(root + "/fresh/qt-config.ini", QSettings::IniFormat);
    QSettings editor(root + "/fresh/oot3d-editor.ini", QSettings::IniFormat);
    Check(qt.value("Renderer/resolution_factor").toInt() == 3);
    Check(!qt.value("Renderer/resolution_factor/default").toBool());
    Check(!qt.value("Audio/enable_audio_stretching").toBool());
    Check(!qt.value("Audio/enable_audio_stretching/default").toBool());
    Check(editor.value("Interpolation/target_fps").toInt() == 60);
    Check(editor.value("Interpolation/cpu_vertices").toBool());
    Check(!qt.value("Renderer/use_hw_shader").toBool());
    Check(qt.value("Renderer/use_disk_shader_cache").toBool());
    Check(qt.value("Renderer/use_shader_jit").toBool());
    Check(!editor.value("Interpolation/match_refresh_rate").toBool());
    Check(editor.value("Graphics/widescreen").toBool());
    Check(editor.value("Graphics/actor_draw_distance").toFloat() == 3.75f);
    Check(editor.value("PauseScreen/dungeon_floor_line/width").toInt() == 42);
    Check(editor.value("PauseScreen/dungeon_floors/grow_selected").toBool());
    Check(editor.value("Graphics/font_mode").toInt() == 1);
    Check(editor.value("TitleScreen/press_start/x").toFloat() == 400);
    Check(editor.value("FileSelect/name_entry/custom/enabled").toBool());
    Check(editor.value("PauseScreen/horizontal").toBool());
    Check(editor.value("HDNumbers/enabled").toBool());
    Check(!editor.contains("HDNumbers/timers/backdrop/enabled"));
    Check(!editor.contains("HDNumbers/timers/shadow/enabled"));
    Check(editor.contains("HUD/magic/one_row_y") && editor.contains("HUD/magic/two_row_y"));
    for (const auto& key : editor.allKeys()) {
        Check(!key.startsWith("Randomizer/") && !key.startsWith("UserHUD/") &&
              !key.startsWith("UserCamera/"));
    }
    for (const auto& key : qt.allKeys()) {
        Check(!key.startsWith("WebService/") && !key.startsWith("Controls/") &&
              !key.startsWith("Data Storage/") && !key.startsWith("UI/"));
    }

    // F3 writes are embedded only for fresh player profiles. Compare the
    // actual selected values so authoring a new release never breaks this test.
    Check(Prepare(root + "/player",true));
    QSettings player_editor(root + "/player/oot3d-editor.ini",QSettings::IniFormat);
    for (const auto& key : player_editor.allKeys()) Check(!key.startsWith("UserHUD/"));
    for (const auto& key : editor.allKeys()) {
        if (key.startsWith("HUD/") || key.startsWith("HDNumbers/"))
            Check(player_editor.value(key)==editor.value(key));
    }
    QSettings player_qt(root + "/player/qt-config.ini",QSettings::IniFormat);
    QSettings choices(":/oot3d/defaults/player.ini",QSettings::IniFormat);
    for (const auto& group : {QStringLiteral("presentation"),QStringLiteral("runtime")}) {
        choices.beginGroup(group);
        auto& destination=group=="presentation" ? player_editor : player_qt;
        for(const auto& key:choices.allKeys()) {
            if(key.startsWith("UI/Shortcuts/") && key.endsWith("/KeySeq"))
                Check(QKeySequence::fromString(destination.value(key).toString(),QKeySequence::NativeText)==
                      QKeySequence::fromString(choices.value(key).toString(),QKeySequence::PortableText));
            else Check(destination.value(key)==choices.value(key));
        }
        choices.endGroup();
    }
    player_editor.setValue("Graphics/bloom",!player_editor.value("Graphics/bloom").toBool());
    const bool selected_bloom=player_editor.value("Graphics/bloom").toBool();
    player_editor.sync();
    Check(Prepare(root + "/player",true));
    player_editor.sync();
    Check(player_editor.value("Graphics/bloom").toBool()==selected_bloom);

    // Actual asset loaders must restore authored geometry without local files.
    FileUtil::UpdateUserPath(FileUtil::UserPath::ConfigDir, (root + "/fresh/").toStdString());
    OOT3D::WorldMap::LoadOutline();
    const auto outline = OOT3D::WorldMap::GetOutline();
    Check(outline && OOT3D::WorldMap::Valid(*outline) && outline->cut.hole.size() == 32);
    const auto boundary = OOT3D::WorldMap::Boundary(*outline);
    unsigned soft = 0, opaque = 0, transparent = 0;
    for (unsigned y = 0; y < 240; ++y) for (unsigned x = 0; x < 320; ++x) {
        const float a = OOT3D::WorldMap::Alpha(boundary, outline->feather, x + .5f, y + .5f);
        soft += a > 0 && a < 1; opaque += a == 1; transparent += a == 0;
    }
    Check(soft > 1000 && opaque > 1000 && transparent > 1000);
    auto pixels = std::make_shared<OOT3D::MapFrame::SourcePixels>(); pixels->fill(~0u);
    OOT3D::MapFrame::source_image.store(std::shared_ptr<const OOT3D::MapFrame::SourcePixels>(pixels));
    OOT3D::MapFrame::LoadCutout();
    Check(OOT3D::MapFrame::GetCutImage() && OOT3D::MapFrame::has_saved_cutout);
    Check(OOT3D::MapFrame::saved_cutout.hole.size() == 20);
    Check(!QFile::exists(root + "/fresh/oot3d-world-map-outline.json"));
    Check(!QFile::exists(root + "/fresh/oot3d-map-cutout.json"));
    auto custom = *outline; custom.feather = 2;
    Check(OOT3D::WorldMap::ApplyOutline(custom));
    OOT3D::WorldMap::LoadOutline();
    Check(OOT3D::WorldMap::GetOutline()->feather == 2); // Local edit wins.

    // Both authored map icons share a column, including viewport fitting with
    // long labels. Changing language must translate the pair together.
    using namespace OOT3D::PausePrompts;
    Layout layout; Assets artwork;
    for (unsigned i : {MapMove, MapReturn}) {
        const QString key = QString("PausePrompts/%1/").arg(Names[i]);
        layout.prompts[i].group = {editor.value(key + "x").toFloat(),
            editor.value(key + "y").toFloat(), editor.value(key + "scale").toFloat()};
        artwork.widths[i] = 200;
        artwork.glyphs[i][0] = {1, i == MapMove ? 198u : 40u, 0};
    }
    for (bool wide : {false, true}) {
        OOT3D::Widescreen::enabled = wide; OOT3D::Widescreen::supported_layout = true;
        const auto g = MakeGeometry(layout, artwork, Page::Map, 0, true, false);
        Check(std::abs(g.icons.positions[0] - g.icons.positions[12]) < .0001f);
        for (unsigned q = 0; q < g.text.count; ++q)
            Check(g.text.positions[q * 12 + 3] <= 400 - SafeLeft() + .001f);
    }

    // Upgrading the first player beta corrects its column without resetting
    // deliberate choices that happen to match upstream renderer defaults.
    qt.setValue(VersionKey, 1); qt.setValue("Renderer/resolution_factor", 1);
    qt.setValue("Renderer/resolution_factor/default", true); qt.sync();
    editor.setValue(VersionKey, 1); editor.setValue("PausePrompts/map_return/x", 69.5); editor.sync();
    Check(Prepare(root + "/fresh", false)); qt.sync(); editor.sync();
    Check(qt.value("Renderer/resolution_factor").toInt() == 1);
    Check(editor.value("PausePrompts/map_return/x").toInt() == 69);

    // Reproduce the broken beta's emitted Qt defaults and migration placeholders.
    QDir().mkpath(root + "/old-beta");
    QSettings old_qt(root + "/old-beta/qt-config.ini", QSettings::IniFormat);
    QSettings old_editor(root + "/old-beta/oot3d-editor.ini", QSettings::IniFormat);
    old_qt.setValue("Renderer/resolution_factor", 1);
    old_qt.setValue("Renderer/resolution_factor/default", true);
    old_qt.setValue("Audio/enable_audio_stretching", true);
    old_qt.setValue("Audio/enable_audio_stretching/default", true);
    old_qt.setValue("Controls/profile", "preserve me");
    old_qt.sync();
    old_editor.setValue("PauseScreen/map/x", 0);
    old_editor.setValue("PauseScreen/map/y", 0);
    old_editor.setValue("PauseScreen/map/scale", 1);
    old_editor.sync();
    Check(Prepare(root + "/old-beta", false));
    old_qt.sync(); old_editor.sync();
    Check(old_qt.value("Renderer/resolution_factor").toInt() == 3);
    Check(!old_qt.value("Audio/enable_audio_stretching").toBool());
    Check(old_qt.value("Controls/profile").toString() == "preserve me");
    Check(old_editor.value("PauseScreen/map/x") == editor.value("PauseScreen/map/x"));

    // Preserve intentional pre-existing settings, including F2 overrides.
    QDir().mkpath(root + "/edited");
    QSettings edited_qt(root + "/edited/qt-config.ini", QSettings::IniFormat);
    QSettings edited(root + "/edited/oot3d-editor.ini", QSettings::IniFormat);
    edited_qt.setValue("Renderer/resolution_factor", 5);
    edited_qt.setValue("Renderer/resolution_factor/default", false);
    edited_qt.sync();
    edited.setValue("PauseScreen/map/x", 0);
    edited.setValue("HUD/hearts/x", 42);
    edited.setValue("UserHUD/hearts/x", 13);
    edited.setValue("Randomizer/seed", "keep existing");
    edited.sync();
    Check(Prepare(root + "/edited", false));
    edited_qt.sync(); edited.sync();
    Check(edited_qt.value("Renderer/resolution_factor").toInt() == 5);
    Check(edited.value("PauseScreen/map/x").toInt() == 0);
    Check(edited.value("HUD/hearts/x").toInt() == 42);
    Check(edited.value("UserHUD/hearts/x").toInt() == 13);
    Check(edited.value("Randomizer/seed").toString() == "keep existing");

    // A later deliberate choice matching an upstream default must survive relaunch.
    qt.setValue("Renderer/resolution_factor", 1);
    qt.setValue("Renderer/resolution_factor/default", true);
    qt.setValue("Audio/enable_audio_stretching", true);
    qt.setValue("Audio/enable_audio_stretching/default", true);
    qt.sync();
    editor.setValue("Interpolation/target_fps", 90);
    editor.sync();
    Check(Prepare(root + "/fresh", false));
    qt.sync(); editor.sync();
    Check(qt.value("Renderer/resolution_factor").toInt() == 1);
    Check(qt.value("Audio/enable_audio_stretching").toBool());
    Check(editor.value("Interpolation/target_fps").toInt() == 90);
    std::cout << "Embedded release-default checks passed\n";
}
