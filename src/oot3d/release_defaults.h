// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDir>
#include <QSettings>
#include <QKeySequence>
#include <QStringList>

namespace OOT3D::ReleaseDefaults {
inline const QString VersionKey = QStringLiteral("SariaUI/defaults_version");
inline constexpr int Version = 3;

inline bool IsUnconfiguredEditor(const QSettings& settings) {
    // The initial test build wrote only these fifteen migration placeholders.
    // Repair that exact unfinished layout, preserving any actual editor edits.
    const QStringList parts{QStringLiteral("dungeon_floors"), QStringLiteral("dungeon_items"),
        QStringLiteral("dungeon_location_paper"), QStringLiteral("dungeon_location_title"),
        QStringLiteral("map")};
    for (const auto& key : settings.allKeys()) {
        const auto path = key.split(QLatin1Char('/'));
        if (path.size() != 3 || path[0] != QStringLiteral("PauseScreen") ||
            !parts.contains(path[1])) return false;
        if (path[2] != QStringLiteral("x") && path[2] != QStringLiteral("y") &&
            path[2] != QStringLiteral("scale")) return false;
        bool ok = false;
        const double value = settings.value(key).toDouble(&ok);
        if (!ok || value != (path[2] == QStringLiteral("scale") ? 1.0 : 0.0)) return false;
    }
    return true;
}

inline bool Apply(QSettings& settings, const QString& resource, bool runtime, bool player = true) {
    const int previous_version = settings.value(VersionKey, 0).toInt();
    if (previous_version >= Version) return true;
    QSettings defaults(resource, QSettings::IniFormat);
    if (defaults.status() != QSettings::NoError || defaults.allKeys().isEmpty()) return false;
    const bool replace_placeholder = !runtime && IsUnconfiguredEditor(settings);
    QVariantMap values;
    for (const auto& key : defaults.allKeys()) values.insert(key, defaults.value(key));
    if (player) {
        QSettings choices(QStringLiteral(":/oot3d/defaults/player.ini"), QSettings::IniFormat);
        choices.beginGroup(runtime ? QStringLiteral("runtime") : QStringLiteral("presentation"));
        for (const auto& key : choices.allKeys()) {
            // Older F3 drafts can contain personal HUD overrides. Fresh profiles inherit F1.
            if (!runtime && key.startsWith(QStringLiteral("UserHUD/"))) continue;
            auto value=choices.value(key);
            if(runtime && key.startsWith(QStringLiteral("UI/Shortcuts/")) && key.endsWith(QStringLiteral("/KeySeq")))
                value=QKeySequence::fromString(value.toString(),QKeySequence::PortableText).toString(QKeySequence::NativeText);
            values.insert(key,value);
        }
        if (choices.status() != QSettings::NoError) return false;
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const auto& key = it.key();
        if (replace_placeholder || !settings.contains(key) ||
            (runtime && previous_version == 0 &&
             settings.value(key + QStringLiteral("/default"), false).toBool())) {
            settings.setValue(key, it.value());
            // Qt's /default=true means ignore the stored value and use its
            // upstream fallback. Our portable baseline must be read literally.
            if (runtime) settings.setValue(key + QStringLiteral("/default"), false);
        }
    }
    // Correct only the original shipped map column; retain customized poses.
    if (!runtime && previous_version == 1 &&
        settings.value("PausePrompts/map_move/x").toDouble() == 301 &&
        settings.value("PausePrompts/map_return/x").toDouble() == 69.5 &&
        settings.value("PausePrompts/map_move/button/x").toDouble() == 0 &&
        settings.value("PausePrompts/map_return/button/x").toDouble() == 0) {
        settings.setValue("PausePrompts/map_return/x", 69);
    }
    // Adopt the corrected F1 layout only where the previous shipped value is still in use.
    if (player && !runtime && previous_version < 3) {
        const auto update_default = [&](const QString& key, float old_value) {
            if (values.contains(key) && settings.value(key).toFloat() == old_value)
                settings.setValue(key, values.value(key));
        };
        update_default(QStringLiteral("FileSelect/0/x"), 360.6f);
        update_default(QStringLiteral("FileSelect/0/y"), 211.2f);
        update_default(QStringLiteral("FileSelect/1/x"), 360.6f);
        update_default(QStringLiteral("FileSelect/1/y"), 224.4f);
        update_default(QStringLiteral("HUD/overall_scale"), .867184042930603f);
        update_default(QStringLiteral("HUD/magic/height"), 2.02f);
        update_default(QStringLiteral("HUD/magic/contour_width"), 1.09f);
        update_default(QStringLiteral("HUD/magic/contour_height"), 1.09f);
        update_default(QStringLiteral("HUD/carrots/opacity"), .74f);
        update_default(QStringLiteral("HUD/carrots/shadow_opacity"), .8f);
        update_default(QStringLiteral("PauseScreen/dungeon_floors/x"), 42.4f);
    }
    settings.setValue(VersionKey, Version);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

inline bool Prepare(const QString& config_dir, bool player = true) {
    if (!QDir().mkpath(config_dir)) return false;
    QSettings runtime(config_dir + QStringLiteral("/qt-config.ini"), QSettings::IniFormat);
    QSettings presentation(config_dir + QStringLiteral("/oot3d-editor.ini"), QSettings::IniFormat);
    return Apply(runtime, QStringLiteral(":/oot3d/defaults/runtime.ini"), true, player) &&
           Apply(presentation, QStringLiteral(":/oot3d/defaults/presentation.ini"), false, player);
}
} // namespace OOT3D::ReleaseDefaults
