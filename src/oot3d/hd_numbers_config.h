// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"
#include <QSettings>
#include <cmath>

namespace OOT3D::HUD {
// Read after legacy HUD/rupees and HUD/timer so existing editor values survive
// the move. Save only the new section; no configuration is changed on load.
inline void LoadHDNumbers(QSettings& config,Layout& l) {
    config.beginGroup(QStringLiteral("HDNumbers"));
    const auto read=[&](const char* key,float fallback,float low,float high) {
        bool ok=false;const float v=config.value(QString::fromLatin1(key),fallback).toFloat(&ok);
        return ok && std::isfinite(v)?std::clamp(v,low,high):fallback;
    };
    l.hd_numbers=config.value(QStringLiteral("enabled"),l.hd_rupee_numbers || l.hd_timer_numbers).toBool();
    l.hd_numbers_bold=config.value(QStringLiteral("font"),QStringLiteral("thin")).toString()==QStringLiteral("bold");
    const float legacy_spacing=read("spacing",l.hd_rupee_spacing,-8,24);
    auto& s=l.hd_rupee_shadow;
    s.enabled=config.value(QStringLiteral("shadow_enabled"),s.enabled).toBool();
    s.x=read("shadow_x",s.x,-3,3);s.y=read("shadow_y",s.y,-3,3);
    s.blur=read("shadow_blur",s.blur,0,2);s.strength=read("shadow_strength",s.strength,0,1);
    config.beginGroup(QStringLiteral("timers/shadow"));
    auto& ts=l.hd_timer_shadow;
    ts.enabled=config.value(QStringLiteral("enabled"),s.enabled).toBool();
    ts.x=read("x",s.x,-3,3);ts.y=read("y",s.y,-3,3);
    ts.blur=read("blur",s.blur,0,2);ts.strength=read("strength",s.strength,0,1);
    config.endGroup();
    struct Section {const char* name;bool* enabled;Pose* pose;float* spacing;};
    for(const auto section:{Section{"rupees",&l.hd_rupee_numbers,&l.hd_rupee_text,&l.hd_rupee_spacing},
        Section{"timers",&l.hd_timer_numbers,&l.hd_timer_text,&l.hd_timer_spacing},
        Section{"ammo",&l.hd_ammo_numbers,&l.hd_ammo_text,&l.hd_ammo_spacing},
        Section{"scores",&l.hd_score_numbers,&l.hd_score_text,&l.hd_score_spacing},
        Section{"skulltula",&l.hd_skulltula_numbers,&l.hd_skulltula_text,&l.hd_skulltula_spacing}}) {
        config.beginGroup(QString::fromLatin1(section.name));
        *section.enabled=config.value(QStringLiteral("enabled"),*section.enabled).toBool();
        auto& p=*section.pose;p={read("x",p.x,-100,100),read("y",p.y,-100,100),read("scale",p.scale,.1f,3)};
        *section.spacing=read("spacing",section.spacing==&l.hd_skulltula_spacing?0:legacy_spacing,-8,24);
        config.endGroup();
    }
    config.beginGroup(QStringLiteral("ammo/items_screen"));
    // Preserve existing appearance when upgrading from shared placement.
    l.hd_pause_ammo_text={read("x",l.hd_ammo_text.x,-100,100),
        read("y",l.hd_ammo_text.y,-100,100),read("scale",l.hd_ammo_text.scale,.1f,3)};
    config.endGroup();
    for(const auto& section:{std::pair{"ammo/backdrop",&l.hd_ammo_backdrop},
        std::pair{"rupees/backdrop",&l.hd_rupee_backdrop},std::pair{"timers/backdrop",&l.hd_timer_backdrop}}) {
        config.beginGroup(QString::fromLatin1(section.first));
        auto& b=*section.second;
        b.enabled=config.value(QStringLiteral("enabled"),b.enabled).toBool();
        b.x=read("x",b.x,-20,20);b.y=read("y",b.y,-20,20);
        b.width=read("width",b.width,.25f,4);b.height=read("height",b.height,.25f,4);
        b.softness=read("softness",b.softness,0,1);b.opacity=read("opacity",b.opacity,0,1);
        config.endGroup();
    }
    config.endGroup();
}
inline void SaveHDNumbers(QSettings& config,const Layout& l) {
    config.beginGroup(QStringLiteral("HDNumbers"));
    config.setValue(QStringLiteral("enabled"),l.hd_numbers);
    config.setValue(QStringLiteral("font"),l.hd_numbers_bold?QStringLiteral("bold"):QStringLiteral("thin"));
    const auto& s=l.hd_rupee_shadow;
    config.setValue(QStringLiteral("shadow_enabled"),s.enabled);
    config.setValue(QStringLiteral("shadow_x"),s.x);config.setValue(QStringLiteral("shadow_y"),s.y);
    config.setValue(QStringLiteral("shadow_blur"),s.blur);config.setValue(QStringLiteral("shadow_strength"),s.strength);
    config.beginGroup(QStringLiteral("timers/shadow"));
    const auto& ts=l.hd_timer_shadow;
    config.setValue(QStringLiteral("enabled"),ts.enabled);
    config.setValue(QStringLiteral("x"),ts.x);config.setValue(QStringLiteral("y"),ts.y);
    config.setValue(QStringLiteral("blur"),ts.blur);config.setValue(QStringLiteral("strength"),ts.strength);
    config.endGroup();
    struct Section {const char* name;bool enabled;Pose pose;float spacing;};
    for(const auto section:{Section{"rupees",l.hd_rupee_numbers,l.hd_rupee_text,l.hd_rupee_spacing},
        Section{"timers",l.hd_timer_numbers,l.hd_timer_text,l.hd_timer_spacing},
        Section{"ammo",l.hd_ammo_numbers,l.hd_ammo_text,l.hd_ammo_spacing},
        Section{"scores",l.hd_score_numbers,l.hd_score_text,l.hd_score_spacing},
        Section{"skulltula",l.hd_skulltula_numbers,l.hd_skulltula_text,l.hd_skulltula_spacing}}) {
        config.beginGroup(QString::fromLatin1(section.name));
        config.setValue(QStringLiteral("enabled"),section.enabled);
        config.setValue(QStringLiteral("x"),section.pose.x);config.setValue(QStringLiteral("y"),section.pose.y);
        config.setValue(QStringLiteral("scale"),section.pose.scale);
        config.setValue(QStringLiteral("spacing"),section.spacing);config.endGroup();
    }
    for(const auto& section:{std::pair{"ammo/backdrop",&l.hd_ammo_backdrop},
        std::pair{"rupees/backdrop",&l.hd_rupee_backdrop},std::pair{"timers/backdrop",&l.hd_timer_backdrop}}) {
        config.beginGroup(QString::fromLatin1(section.first));
        const auto& b=*section.second;
        config.setValue(QStringLiteral("enabled"),b.enabled);
        config.setValue(QStringLiteral("x"),b.x);config.setValue(QStringLiteral("y"),b.y);
        config.setValue(QStringLiteral("width"),b.width);config.setValue(QStringLiteral("height"),b.height);
        config.setValue(QStringLiteral("softness"),b.softness);config.setValue(QStringLiteral("opacity"),b.opacity);
        config.endGroup();
    }
    config.beginGroup(QStringLiteral("ammo/items_screen"));
    config.setValue(QStringLiteral("x"),l.hd_pause_ammo_text.x);
    config.setValue(QStringLiteral("y"),l.hd_pause_ammo_text.y);
    config.setValue(QStringLiteral("scale"),l.hd_pause_ammo_text.scale);
    config.endGroup();
    config.endGroup();
}
}
