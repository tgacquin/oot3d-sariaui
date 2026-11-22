// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/user_hud_config.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace OOT3D::UserHUD {
inline QString ExportPreset(const QString& path, const QVariantMap& values = HUDSettings()) {
    const QJsonObject preset{{QStringLiteral("format"), QStringLiteral("SariaUI-HUD")},
                             {QStringLiteral("version"), 2},
                             {QStringLiteral("settings"), QJsonObject::fromVariantMap(values)}};
    const auto bytes = QJsonDocument(preset).toJson();
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() ||
        !output.commit())
        return output.errorString();
    return {};
}

inline QString ImportPreset(const QString& path, QVariantMap* imported = nullptr) {
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly))
        return input.errorString();
    constexpr qint64 maximum_size = 256 * 1024;
    const auto bytes = input.read(maximum_size + 1);
    if (input.error() != QFileDevice::NoError)
        return input.errorString();
    if (bytes.size() > maximum_size)
        return QStringLiteral("This file is too large for a HUD preset.");
    const auto preset = QJsonDocument::fromJson(bytes).object();
    if (preset.value(QStringLiteral("format")).toString() != QStringLiteral("SariaUI-HUD"))
        return QStringLiteral("Choose a SariaUI HUD preset exported from the HUD tab.");
    const int version = preset.value(QStringLiteral("version")).toInt();
    if (version != 1 && version != 2)
        return QStringLiteral("This HUD preset version is unsupported.");
    auto settings = preset.value(QStringLiteral("settings")).toObject();
    if (version == 1)
        settings.insert(QStringLiteral("UserHUD/counter_group/distance_offset"), 0);
    const auto expected = QJsonObject::fromVariantMap(HUDSettings(Layout{},-1));
    for (auto it = expected.begin(); it != expected.end(); ++it) {
        if (settings.value(it.key()).type() != it.value().type())
            return QStringLiteral("Missing or invalid HUD setting: %1").arg(it.key());
    }
    const auto values=settings.toVariantMap();
    if (imported) {
        const int hd=std::clamp(values.value(QStringLiteral("UserHUD/hd_numbers")).toInt(),-1,1);
        *imported=HUDSettings(ReadHUDLayout(values),hd);
    } else LoadHUDSettings(values, true);
    return {};
}
} // namespace OOT3D::UserHUD
