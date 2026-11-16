// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDir>
#include <QSettings>
#include <QStringList>

namespace OOT3D::ReleaseDefaults {
inline const QString VersionKey = QStringLiteral("SariaUI/defaults_version");
inline constexpr int Version = 2;

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

inline bool Apply(QSettings& settings, const QString& resource, bool runtime) {
    const int previous_version = settings.value(VersionKey, 0).toInt();
    if (previous_version >= Version) return true;
    QSettings defaults(resource, QSettings::IniFormat);
    if (defaults.status() != QSettings::NoError || defaults.allKeys().isEmpty()) return false;
    const bool replace_placeholder = !runtime && IsUnconfiguredEditor(settings);
    for (const auto& key : defaults.allKeys()) {
        if (replace_placeholder || !settings.contains(key) ||
            (runtime && previous_version == 0 &&
             settings.value(key + QStringLiteral("/default"), false).toBool())) {
            settings.setValue(key, defaults.value(key));
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
    settings.setValue(VersionKey, Version);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

inline bool Prepare(const QString& config_dir) {
    if (!QDir().mkpath(config_dir)) return false;
    QSettings runtime(config_dir + QStringLiteral("/qt-config.ini"), QSettings::IniFormat);
    QSettings presentation(config_dir + QStringLiteral("/oot3d-editor.ini"), QSettings::IniFormat);
    return Apply(runtime, QStringLiteral(":/oot3d/defaults/runtime.ini"), true) &&
           Apply(presentation, QStringLiteral(":/oot3d/defaults/presentation.ini"), false);
}
} // namespace OOT3D::ReleaseDefaults
