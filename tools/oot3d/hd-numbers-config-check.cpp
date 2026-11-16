// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hd_numbers_config.h"
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

int main() {
    using namespace OOT3D::HUD;
    QTemporaryDir dir;assert(dir.isValid());
    QSettings config(dir.filePath("editor.ini"),QSettings::IniFormat);
    Layout legacy;legacy.hd_rupee_text={-1.2f,-3.6f,1.28f};legacy.hd_timer_text={4,5,.8f};
    legacy.hd_rupee_spacing=2;legacy.hd_rupee_shadow={true,-1,2,.5f,.4f};
    legacy.hd_timer_numbers=false;
    LoadHDNumbers(config,legacy);
    assert(!legacy.hd_numbers_bold);
    assert(legacy.hd_rupee_text.scale==1.28f && legacy.hd_timer_text.y==5);
    assert(legacy.hd_rupee_spacing==2 && legacy.hd_rupee_shadow.blur==.5f && !legacy.hd_timer_numbers);
    assert(legacy.hd_timer_spacing==2 && legacy.hd_ammo_spacing==2 && legacy.hd_score_spacing==2);
    assert(config.allKeys().empty()); // Loading does not write migration state.
    assert(legacy.hd_timer_shadow==legacy.hd_rupee_shadow);
    QSettings old_ammo(dir.filePath("ammo.ini"),QSettings::IniFormat);
    old_ammo.setValue("HDNumbers/ammo/x",5);
    old_ammo.setValue("HDNumbers/ammo/scale",1.4f);
    Layout old_layout;LoadHDNumbers(old_ammo,old_layout);
    assert(old_layout.hd_pause_ammo_text.x==5 && old_layout.hd_pause_ammo_text.scale==1.4f);
    QSettings shared(dir.filePath("shared.ini"),QSettings::IniFormat);
    shared.setValue("HDNumbers/spacing",6);
    Layout migrated;LoadHDNumbers(shared,migrated);
    assert(migrated.hd_rupee_spacing==6 && migrated.hd_timer_spacing==6 &&
           migrated.hd_ammo_spacing==6 && migrated.hd_score_spacing==6);
    legacy.hd_numbers=false;legacy.hd_ammo_text={3,-2,2};legacy.hd_score_text={7,8,.6f};
    legacy.hd_timer_spacing=3;legacy.hd_ammo_spacing=-1;legacy.hd_score_spacing=5;
    legacy.hd_ammo_backdrop={true,-3,4,2,1.5f,.6f,.4f};
    legacy.hd_rupee_backdrop={true,2,-1,1.5f,1.1f,.8f,.2f};
    legacy.hd_timer_backdrop={true,3,2,2.5f,1.8f,.9f,.7f};
    legacy.hd_pause_ammo_text={-4,7,.8f};
    legacy.hd_skulltula_text={2,-1,1.2f};legacy.hd_skulltula_spacing=-2;
    legacy.hd_skulltula_numbers=false;
    legacy.hd_numbers_bold=true;
    legacy.hd_timer_shadow={true,2,-2,1.5f,.8f};
    legacy.hd_score_numbers=false;SaveHDNumbers(config,legacy);config.sync();
    Layout restored;LoadHDNumbers(config,restored);
    assert(restored.hd_numbers_bold);
    assert(restored.hd_timer_shadow==legacy.hd_timer_shadow && restored.hd_timer_shadow!=restored.hd_rupee_shadow);
    assert(!restored.hd_numbers && !restored.hd_score_numbers && !restored.hd_timer_numbers);
    assert(restored.hd_ammo_numbers && restored.hd_rupee_numbers);
    assert(restored.hd_ammo_backdrop==legacy.hd_ammo_backdrop);
    assert(restored.hd_rupee_backdrop==legacy.hd_rupee_backdrop);
    assert(restored.hd_timer_backdrop==legacy.hd_timer_backdrop);
    assert(restored.hd_pause_ammo_text.x==-4 && restored.hd_pause_ammo_text.scale==.8f);
    assert(!restored.hd_skulltula_numbers && restored.hd_skulltula_text.scale==1.2f && restored.hd_skulltula_spacing==-2);
    assert(restored.hd_rupee_text.scale==1.28f && restored.hd_timer_text.y==5);
    assert(restored.hd_ammo_text.x==3 && restored.hd_score_text.scale==.6f);
    assert(restored.hd_rupee_spacing==2 && restored.hd_rupee_shadow.strength==.4f);
    assert(restored.hd_timer_spacing==3 && restored.hd_ammo_spacing==-1 && restored.hd_score_spacing==5);
    config.setValue("HDNumbers/ammo/scale","nan");
    config.setValue("HDNumbers/font","invalid");
    config.setValue("HDNumbers/scores/x",10000);
    config.setValue("HDNumbers/ammo/backdrop/opacity",2);
    config.setValue("HDNumbers/ammo/backdrop/width","nan");
    config.setValue("HDNumbers/timers/backdrop/opacity",3);
    config.setValue("HDNumbers/timers/backdrop/height","nan");
    LoadHDNumbers(config,restored);
    assert(restored.hd_timer_backdrop.opacity==1 && restored.hd_timer_backdrop.height==1.8f);
    assert(!restored.hd_numbers_bold);
    assert(restored.hd_ammo_text.scale==2 && restored.hd_score_text.x==100);
    assert(restored.hd_ammo_backdrop.opacity==1 && restored.hd_ammo_backdrop.width==2);
    std::cout<<"HD Numbers settings: legacy values, master/category toggles, persistence and validation passed\n";
}
