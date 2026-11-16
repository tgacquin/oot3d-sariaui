// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QSettings>
#include "oot3d/pause_menu.h"

namespace OOT3D::PauseMenu {
// Legacy colors are loaded first. Missing layouts inherit those exact values;
// merely opening the editor must never reset an existing user's palette.
inline void LoadColorLayouts(QSettings& settings,Layout& value) {
    auto& layouts=value.color_layouts;
    layouts={};layouts.Initialize(value.header_overlay,value.slots_overlay);
    constexpr std::array channels{"red","green","blue","opacity"};
    for(unsigned slot=0;slot<layouts.saved.size();++slot)
        for(unsigned part=0;part<ColorNames.size();++part) {
            auto& color=layouts.saved[slot][part];
            const QString prefix=QStringLiteral("PauseScreen/color_layouts/layout_")+
                QString::number(slot+1)+QStringLiteral("/")+QString::fromLatin1(ColorNames[part])+QStringLiteral("/");
            for(unsigned c=0;c<channels.size();++c) {
                float& component=c==3?color.opacity:color.color[c];
                bool ok=false;
                const float v=settings.value(prefix+QString::fromLatin1(channels[c]),component).toFloat(&ok);
                if(ok && std::isfinite(v))component=std::clamp(v,0.f,1.f);
            }
        }
    const int active=settings.value(QStringLiteral("PauseScreen/color_layouts/active"),0).toInt();
    layouts.active=unsigned(std::clamp(active,0,1));
    layouts.working=layouts.saved[layouts.active];
}
inline void SaveColorLayouts(QSettings& settings,const Layout& value) {
    auto layouts=value.color_layouts;
    layouts.Initialize(value.header_overlay,value.slots_overlay);
    settings.setValue(QStringLiteral("PauseScreen/color_layouts/active"),layouts.active);
    constexpr std::array channels{"red","green","blue"};
    for(unsigned slot=0;slot<layouts.saved.size();++slot)
        for(unsigned part=0;part<ColorNames.size();++part) {
            const auto& color=layouts.saved[slot][part];
            const QString prefix=QStringLiteral("PauseScreen/color_layouts/layout_")+
                QString::number(slot+1)+QStringLiteral("/")+QString::fromLatin1(ColorNames[part])+QStringLiteral("/");
            for(unsigned c=0;c<channels.size();++c)
                settings.setValue(prefix+QString::fromLatin1(channels[c]),color.color[c]);
            settings.setValue(prefix+QStringLiteral("opacity"),color.opacity);
        }
    // Deliberately save the stored palettes, never the live unsaved preview.
}
} // namespace OOT3D::PauseMenu
