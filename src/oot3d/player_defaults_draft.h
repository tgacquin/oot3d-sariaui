// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QFileInfo>
#include <QSettings>
#include <QVariantMap>

namespace OOT3D {
// Each editor or frontend dialog owns its instance. Reads start from the release baseline, even
// when the developer has a different controller, HUD layout or active seed.
class PlayerDefaultsDraft {
public:
    explicit PlayerDefaultsDraft(const QString& directory)
        : choices(directory + QStringLiteral("/player.ini"), QSettings::IniFormat) {
        valid = QFileInfo::exists(choices.fileName()) && choices.isWritable();
        for (const auto& group : {QStringLiteral("presentation"), QStringLiteral("runtime")}) {
            QSettings base(directory + QStringLiteral("/") + group + QStringLiteral(".ini"), QSettings::IniFormat);
            valid &= !base.allKeys().isEmpty() && base.status() == QSettings::NoError;
            for (const auto& key : base.allKeys()) values.insert(group + QStringLiteral("/") + key, base.value(key));
        }
        for (const auto& key : choices.allKeys()) values.insert(key, choices.value(key));
        valid &= choices.status() == QSettings::NoError;
    }
    // Explicit runtime keys live in the other INI in a player's user folder.
    static QString Path(const QString& key) {
        if (key.startsWith(QStringLiteral("Renderer/")) || key.startsWith(QStringLiteral("Utility/")) ||
            key.startsWith(QStringLiteral("UI/")) || key == QStringLiteral("Audio/volume"))
            return QStringLiteral("runtime/") + key;
        return QStringLiteral("presentation/") + key;
    }
    QVariant value(const QString& key, const QVariant& fallback = {}) const {
        return values.value(Path(key), fallback);
    }
    bool contains(const QString& key) const { return values.contains(Path(key)); }
    void setValue(const QString& key, const QVariant& value) {
        const auto path = Path(key);
        if (values.contains(path) && values.value(path) == value) return;
        values.insert(path, value);
        choices.setValue(path, value);
        pending = true;
    }
    void Flush() {
        if (pending) {
            choices.sync();
            pending = false;
        }
    }
    bool Writable() const { return valid && choices.status() == QSettings::NoError; }
private:
    QSettings choices;
    QVariantMap values;
    bool valid = false;
    bool pending = false;
};
} // namespace OOT3D
