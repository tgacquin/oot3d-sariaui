// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/user_camera.h"
#include <QSettings>

namespace OOT3D::UserCamera {
inline void LoadConfig(QSettings& config) {
    Values value;
    // Older shared Camera/ and Graphics/ keys remain the F1 baseline.
    // Loading an old file must neither rewrite it nor invent player overrides.
    for(unsigned i=0;i<ScaleCount;++i) {
        bool ok=false;
        const float v=config.value(QStringLiteral("UserCamera/")+QString::fromLatin1(ScaleNames[i]),1.f).toFloat(&ok);
        value.scales[i]=ok?SanitizeScale(static_cast<Scale>(i),v):1.f;
    }
    for(unsigned i=0;i<ToggleCount;++i) {
        bool ok=false;
        const int v=config.value(QStringLiteral("UserCamera/")+QString::fromLatin1(ToggleNames[i]),-1).toInt(&ok);
        value.toggles[i]=ok?SanitizeToggle(v):-1;
    }
    Set(value,false);
}
inline void SaveConfig(QSettings& config) {
    const auto value=Get();
    for(unsigned i=0;i<ScaleCount;++i)
        config.setValue(QStringLiteral("UserCamera/")+QString::fromLatin1(ScaleNames[i]),value.scales[i]);
    for(unsigned i=0;i<ToggleCount;++i)
        config.setValue(QStringLiteral("UserCamera/")+QString::fromLatin1(ToggleNames[i]),value.toggles[i]);
}
} // namespace OOT3D::UserCamera
