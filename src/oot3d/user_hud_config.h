// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"
#include <QSettings>

namespace OOT3D::UserHUD {
inline float ReadFloat(const auto& config,const QString& key,float fallback,float low,float high) {
    bool ok=false;
    const float value=config.value(key,fallback).toFloat(&ok);
    return ok && std::isfinite(value)?std::clamp(value,low,high):fallback;
}
inline Padding ReadPadding(const auto& config,const QString& prefix) {
    const auto read=[&](const QString& edge,int limit) {
        return std::clamp(config.value(prefix+edge,0).toInt(),-limit,limit);
    };
    return {read(QStringLiteral("top"),VerticalPaddingLimit),read(QStringLiteral("bottom"),VerticalPaddingLimit),
            read(QStringLiteral("left"),SidePaddingLimit),read(QStringLiteral("right"),SidePaddingLimit)};
}
inline void LoadAuthoredTitles(QSettings& config,HUD::Layout& authored) {
    // Reposition by default. Only the authored F1 setting may override this;
    // obsolete player restoration/override toggles no longer affect titles.
    authored.reposition_area_titles=config.value(QStringLiteral("HUD/area_titles/enabled"),true).toBool();
    authored.area_title_y=ReadFloat(config,QStringLiteral("HUD/area_titles/y"),
        ReadFloat(config,QStringLiteral("UserHUD/area_title_y"),-72,-240,240),-240,240);
    authored.area_title_scale=ReadFloat(config,QStringLiteral("HUD/area_titles/scale"),1,
        HUD::Layout::MinAreaTitleScale,HUD::Layout::MaxAreaTitleScale);
}
inline void SaveAuthoredTitles(QSettings& config,const HUD::Layout& authored) {
    config.setValue(QStringLiteral("HUD/area_titles/enabled"),authored.reposition_area_titles);
    config.setValue(QStringLiteral("HUD/area_titles/y"),authored.area_title_y);
    config.setValue(QStringLiteral("HUD/area_titles/scale"),authored.area_title_scale);
}
inline Layout ReadHUDLayout(const auto& config) {
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
    static_cast<Padding&>(value)=ReadPadding(config,QStringLiteral("UserHUD/padding/"));
    value.heart_spacing=ReadFloat(config,QStringLiteral("UserHUD/hearts/spacing"),0,-28,28);
    value.overall_opacity=ReadFloat(config,QStringLiteral("UserHUD/overall_opacity"),1,0,1);
    value.minimap_opacity=ReadFloat(config,QStringLiteral("UserHUD/minimap/opacity"),1,0,1);
    value.overall_scale=ReadFloat(config,QStringLiteral("UserHUD/overall_scale"),1,.5f,2);
    value.counter_distance_offset=ReadFloat(config,QStringLiteral("UserHUD/counter_group/distance_offset"),0,-Layout::MaxCounterDistance,Layout::MaxCounterDistance);
    value.button_scale=ReadFloat(config,QStringLiteral("UserHUD/buttons/group_scale"),0,-90,200);
    value.button_x=ReadFloat(config,QStringLiteral("UserHUD/buttons/group_x"),0,-400,400);
    value.button_y=ReadFloat(config,QStringLiteral("UserHUD/buttons/group_y"),0,-240,240);
    value.magic_height=ReadFloat(config,QStringLiteral("UserHUD/magic/height"),0,0,8);
    value.magic_outer_thickness=ReadFloat(config,QStringLiteral("UserHUD/magic/outer_thickness"),0,0,8);
    value.magic_width=ReadFloat(config,QStringLiteral("UserHUD/magic/width"),1,.5f,4);
    return value;
}
inline void LoadHUDSettings(const auto& config, bool save) {
    hd_numbers_override=std::clamp(config.value(QStringLiteral("UserHUD/hd_numbers"),-1).toInt(),-1,1);
    SetLayout(ReadHUDLayout(config),save);
}
inline void LoadConfig(QSettings& config) {
    LoadHUDSettings(config,false);
    classic_file_select=config.value(QStringLiteral("UserHUD/classic_file_select"),false).toBool();
}
inline QVariantMap HUDSettings(Layout value, int hd_override) {
    QVariantMap settings;
    value[OutsideMinimap]=value[Minimap];
    for(unsigned i=0;i<value.size();++i) {
        const auto prefix=QStringLiteral("UserHUD/")+QString::fromLatin1(Names[i])+QStringLiteral("/");
        settings.insert(prefix+QStringLiteral("x"),value[i].x);
        settings.insert(prefix+QStringLiteral("y"),value[i].y);
        settings.insert(prefix+QStringLiteral("scale"),value[i].scale);
    }
    settings.insert(QStringLiteral("UserHUD/minimap_unified"),true);
    settings.insert(QStringLiteral("UserHUD/overall_opacity"),value.overall_opacity);
    settings.insert(QStringLiteral("UserHUD/minimap/opacity"),value.minimap_opacity);
    settings.insert(QStringLiteral("UserHUD/overall_scale"),value.overall_scale);
    settings.insert(QStringLiteral("UserHUD/padding/top"),value.top);
    settings.insert(QStringLiteral("UserHUD/padding/bottom"),value.bottom);
    settings.insert(QStringLiteral("UserHUD/padding/left"),value.left);
    settings.insert(QStringLiteral("UserHUD/padding/right"),value.right);
    settings.insert(QStringLiteral("UserHUD/counter_group/distance_offset"),value.counter_distance_offset);
    settings.insert(QStringLiteral("UserHUD/buttons/group_scale"),value.button_scale);
    settings.insert(QStringLiteral("UserHUD/buttons/group_x"),value.button_x);
    settings.insert(QStringLiteral("UserHUD/buttons/group_y"),value.button_y);
    settings.insert(QStringLiteral("UserHUD/hearts/spacing"),value.heart_spacing);
    settings.insert(QStringLiteral("UserHUD/magic/height"),value.magic_height);
    settings.insert(QStringLiteral("UserHUD/magic/outer_thickness"),value.magic_outer_thickness);
    settings.insert(QStringLiteral("UserHUD/magic/width"),value.magic_width);
    settings.insert(QStringLiteral("UserHUD/hd_numbers"),hd_override);
    return settings;
}
inline QVariantMap HUDSettings() { return HUDSettings(GetLayout(),hd_numbers_override.load()); }
inline void SaveConfig(QSettings& config) {
    const auto settings=HUDSettings();
    for (auto it=settings.cbegin(); it!=settings.cend(); ++it)
        config.setValue(it.key(),it.value());
    config.setValue(QStringLiteral("UserHUD/classic_file_select"),classic_file_select.load());
}
} // namespace OOT3D::UserHUD
