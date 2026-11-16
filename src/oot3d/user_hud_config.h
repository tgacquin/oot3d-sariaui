// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"
#include <QSettings>

namespace OOT3D::UserHUD {
inline float ReadFloat(QSettings& config,const QString& key,float fallback,float low,float high) {
    bool ok=false;
    const float value=config.value(key,fallback).toFloat(&ok);
    return ok && std::isfinite(value)?std::clamp(value,low,high):fallback;
}
inline void LoadAuthoredTitles(QSettings& config,HUD::Layout& authored) {
    // Reposition by default. Only the authored F1 setting may override this;
    // obsolete player restoration/override toggles no longer affect titles.
    authored.reposition_area_titles=config.value(QStringLiteral("HUD/area_titles/enabled"),true).toBool();
    authored.area_title_y=ReadFloat(config,QStringLiteral("HUD/area_titles/y"),
        ReadFloat(config,QStringLiteral("UserHUD/area_title_y"),-72,-240,240),-240,240);
}
inline void SaveAuthoredTitles(QSettings& config,const HUD::Layout& authored) {
    config.setValue(QStringLiteral("HUD/area_titles/enabled"),authored.reposition_area_titles);
    config.setValue(QStringLiteral("HUD/area_titles/y"),authored.area_title_y);
}
inline void LoadConfig(QSettings& config) {
    Layout value{};
    for(unsigned i=0;i<value.size();++i) {
        const auto prefix=QStringLiteral("UserHUD/")+QString::fromLatin1(Names[i])+QStringLiteral("/");
        value[i]={ReadFloat(config,prefix+QStringLiteral("x"),0,-400,400),
            ReadFloat(config,prefix+QStringLiteral("y"),0,-240,240),
            ReadFloat(config,prefix+QStringLiteral("scale"),0,-90,200)};
    }
    // Prefer an explicit dungeon setting; otherwise preserve an old outdoor-only adjustment.
    if(!config.contains(QStringLiteral("UserHUD/minimap_unified")) &&
       value[Minimap].x==0 && value[Minimap].y==0 && value[Minimap].scale==0)
        value[Minimap]=value[OutsideMinimap];
    value[OutsideMinimap]=value[Minimap];
    value.top=config.value(QStringLiteral("UserHUD/padding/top"),0).toInt();
    value.bottom=config.value(QStringLiteral("UserHUD/padding/bottom"),0).toInt();
    value.left=config.value(QStringLiteral("UserHUD/padding/left"),0).toInt();
    value.right=config.value(QStringLiteral("UserHUD/padding/right"),0).toInt();
    value.heart_spacing=ReadFloat(config,QStringLiteral("UserHUD/hearts/spacing"),0,-28,28);
    value.button_scale=ReadFloat(config,QStringLiteral("UserHUD/buttons/group_scale"),0,-90,200);
    value.magic_height=ReadFloat(config,QStringLiteral("UserHUD/magic/height"),0,0,8);
    value.magic_border=ReadFloat(config,QStringLiteral("UserHUD/magic/border"),0,0,4);
    value.magic_width=ReadFloat(config,QStringLiteral("UserHUD/magic/width"),1,.5f,4);
    SetLayout(value,false);
    hd_numbers_override=std::clamp(config.value(QStringLiteral("UserHUD/hd_numbers"),-1).toInt(),-1,1);
    classic_file_select=config.value(QStringLiteral("UserHUD/classic_file_select"),false).toBool();
}
inline void SaveConfig(QSettings& config) {
    auto value=GetLayout();value[OutsideMinimap]=value[Minimap];
    for(unsigned i=0;i<value.size();++i) {
        const auto prefix=QStringLiteral("UserHUD/")+QString::fromLatin1(Names[i])+QStringLiteral("/");
        config.setValue(prefix+QStringLiteral("x"),value[i].x);
        config.setValue(prefix+QStringLiteral("y"),value[i].y);
        config.setValue(prefix+QStringLiteral("scale"),value[i].scale);
    }
    config.setValue(QStringLiteral("UserHUD/minimap_unified"),true);
    config.setValue(QStringLiteral("UserHUD/padding/top"),value.top);
    config.setValue(QStringLiteral("UserHUD/padding/bottom"),value.bottom);
    config.setValue(QStringLiteral("UserHUD/padding/left"),value.left);
    config.setValue(QStringLiteral("UserHUD/padding/right"),value.right);
    config.setValue(QStringLiteral("UserHUD/buttons/group_scale"),value.button_scale);
    config.setValue(QStringLiteral("UserHUD/hearts/spacing"),value.heart_spacing);
    config.setValue(QStringLiteral("UserHUD/magic/height"),value.magic_height);
    config.setValue(QStringLiteral("UserHUD/magic/border"),value.magic_border);
    config.setValue(QStringLiteral("UserHUD/magic/width"),value.magic_width);
    config.setValue(QStringLiteral("UserHUD/hd_numbers"),hd_numbers_override.load());
    config.setValue(QStringLiteral("UserHUD/classic_file_select"),classic_file_select.load());
}
} // namespace OOT3D::UserHUD
