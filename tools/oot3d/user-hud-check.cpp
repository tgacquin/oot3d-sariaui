// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hud_defaults.h"
#include "oot3d/input.h"
#include "oot3d/user_hud_config.h"
#include <QTemporaryDir>
#include <cassert>
#include <iostream>
#include <limits>

using namespace OOT3D;
template<class P> bool Same(const P& a,const P& b) {
    return a.x==b.x && a.y==b.y && a.scale==b.scale;
}
bool Near(float a,float b) {return std::abs(a-b)<.0001f;}
int main() {
    const auto baseline=HUDDefaults::Basic();
    const auto buttons=HUDDefaults::Buttons();
    const auto dpad=HUDDefaults::DPadLayout();
    HUD::SetLayout(baseline,false);HUDButtons::SetLayout(buttons,false);
    DPad::SetLayout(dpad,false);UserHUD::SetLayout({},false);
    const auto unchanged=[&] {
        const auto hud=HUD::GetEffectiveLayout();
        for(unsigned i=0;i<baseline.size();++i)assert(Same(hud[i],baseline[i]));
        assert(Same(hud.outside_minimap,baseline.outside_minimap));
        assert(hud.heart_spacing==baseline.heart_spacing && hud.magic_height==baseline.magic_height);
        assert(hud.magic_border==baseline.magic_border && hud.magic_background_opacity==baseline.magic_background_opacity);
        const auto b=HUDButtons::GetEffectiveLayout();
        for(unsigned i=0;i<HUDButtons::Count;++i)
            assert(Same(*HUDButtons::ButtonPose(b,i),*HUDButtons::ButtonPose(buttons,i)));
        const auto d=DPad::GetEffectiveLayout().group;
        assert(d.x==dpad.group.x && d.y==dpad.group.y &&
               d.scale_x==dpad.group.scale_x && d.scale_y==dpad.group.scale_y);
    };
    unchanged();
    // Overall scale reaches every HUD group once, composes with local scales,
    // and preserves padding and the authored layout across repeated frames.
    for(float factor:{.5f,1.f,1.375f,2.f}) {
        UserHUD::Layout all;all.overall_scale=factor;all.left=7;all.top=4;
        all.button_scale=10;all[UserHUD::XButton].scale=20;
        UserHUD::SetLayout(all,false);
        for(unsigned frame=0;frame<3;++frame) {
            const auto hud=HUD::GetEffectiveLayout();
            for(unsigned i=0;i<hud.size();++i) {
                const float expected=baseline[i].scale*factor*(i==HUD::AButton?1.1f:1.f);
                assert(Near(hud[i].scale,std::clamp(expected,.1f,3.f)));
                assert(Same(HUD::GetLayout()[i],baseline[i]));
            }
            assert(Near(hud[HUD::Hearts].x,baseline[HUD::Hearts].x+7));
            assert(Near(hud[HUD::Hearts].y,baseline[HUD::Hearts].y+4));
            assert(Near(hud.outside_minimap.scale,std::clamp(baseline.outside_minimap.scale*factor,.1f,3.f)));
            const auto b=HUDButtons::GetEffectiveLayout();
            const auto old_x=*HUDButtons::ButtonPose(buttons,HUDButtons::X);
            const auto old_i=*HUDButtons::ButtonPose(buttons,HUDButtons::I);
            const auto x=*HUDButtons::ButtonPose(b,HUDButtons::X);
            const auto i=*HUDButtons::ButtonPose(b,HUDButtons::I);
            assert(Near(x.scale,old_x.scale*factor*1.1f*1.2f));
            assert(Near(i.scale,old_i.scale*factor*1.1f));
            assert(Near(i.y-x.y,(old_i.y-old_x.y)*factor*1.1f));
            assert(Near(HUD::ActionAnchorX+hud[HUD::AButton].x-b.buttons[HUDButtons::B].pose.x,
                (HUD::ActionAnchorX+baseline[HUD::AButton].x-buttons.buttons[HUDButtons::B].pose.x)*factor*1.1f));
            assert(Near(DPad::GetEffectiveLayout().group.scale_x,std::clamp(dpad.group.scale_x*factor*1.1f,.1f,4.f)));
            assert(UserHUD::GetLayout().button_scale==10);
        }
    }
    UserHUD::Layout invalid;invalid.overall_scale=std::numeric_limits<float>::quiet_NaN();
    UserHUD::SetLayout(invalid,false);assert(UserHUD::GetLayout().overall_scale==1);
    invalid.overall_scale=100;UserHUD::SetLayout(invalid,false);assert(UserHUD::GetLayout().overall_scale==2);
    invalid.overall_scale=-1;UserHUD::SetLayout(invalid,false);assert(UserHUD::GetLayout().overall_scale==.5f);
    UserHUD::SetLayout({},false);unchanged();
    UserHUD::Layout offsets{};
    offsets[UserHUD::Rupees]={12,-7,50};
    offsets[UserHUD::ItemButtons]={20,10,-50};
    offsets[UserHUD::BButton]={-10,5,25};
    offsets[UserHUD::Minimap]={-40,-20,10};
    offsets[UserHUD::DPad]={3,4,50};
    UserHUD::SetLayout(offsets);
    assert(UserHUD::dirty.exchange(false));
    for(unsigned frame=0;frame<100;++frame) {
        const auto hud=HUD::GetEffectiveLayout();
        assert(Near(hud[HUD::Rupees].x,baseline[HUD::Rupees].x+12));
        assert(Near(hud[HUD::Rupees].y,baseline[HUD::Rupees].y-7));
        assert(Near(hud[HUD::Rupees].scale,baseline[HUD::Rupees].scale*1.5f));
        assert(Near(hud[HUD::Minimap].x,baseline[HUD::Minimap].x-40));
        assert(Near(hud.outside_minimap.x,baseline.outside_minimap.x-40));
        const auto b=HUDButtons::GetEffectiveLayout();
        const auto old_x=*HUDButtons::ButtonPose(buttons,HUDButtons::X);
        const auto old_i=*HUDButtons::ButtonPose(buttons,HUDButtons::I);
        const auto x=*HUDButtons::ButtonPose(b,HUDButtons::X);
        const auto i=*HUDButtons::ButtonPose(b,HUDButtons::I);
        assert(Near(x.scale,old_x.scale*.5f));
        assert(Near(i.y-x.y,(old_i.y-old_x.y)*.5f));
        assert(Near((x.y+i.y)/2,(old_x.y+old_i.y)/2+10));
        assert(Near(x.x,old_x.x+20));
        assert(Near(b.buttons[0].pose.x,buttons.buttons[0].pose.x-10));
        assert(Near(b.buttons[0].pose.scale,buttons.buttons[0].pose.scale*1.25f));
        assert(Near(DPad::GetEffectiveLayout().group.scale_x,dpad.group.scale_x*1.5f));
        assert(Near(DPad::GetEffectiveLayout().group.x,dpad.group.x+3));
        // Saving the developer layout must never bake in user offsets.
        assert(Same(HUD::GetLayout()[HUD::Rupees],baseline[HUD::Rupees]));
        assert(HUDButtons::GetLayout().item_scale==buttons.item_scale);
        assert(DPad::GetLayout().group.x==dpad.group.x);
    }
    assert(!HUD::dirty && !HUDButtons::dirty && !DPad::dirty);
    UserHUD::SetLayout({});unchanged();
    // Padding composes with individual offsets without scaling the inset,
    // accumulating each frame, or changing the authored layouts.
    offsets.top=8;offsets.bottom=12;offsets.left=16;offsets.right=20;
    UserHUD::SetLayout(offsets,false);
    for(unsigned frame=0;frame<100;++frame) {
        const auto hud=HUD::GetEffectiveLayout();
        assert(Near(hud[HUD::Hearts].x,baseline[HUD::Hearts].x+16));
        assert(Near(hud[HUD::Hearts].y,baseline[HUD::Hearts].y+8));
        assert(Near(hud[HUD::Rupees].x,baseline[HUD::Rupees].x+12+16));
        assert(Near(hud[HUD::Rupees].y,baseline[HUD::Rupees].y-7-12));
        assert(Near(hud[HUD::Minimap].x,baseline[HUD::Minimap].x-40-20));
        assert(Near(hud.outside_minimap.y,baseline.outside_minimap.y-20-12));
        assert(Near(hud[HUD::Timer].x,baseline[HUD::Timer].x));
        assert(Near(hud[HUD::Timer].y,baseline[HUD::Timer].y-12));
        assert(Near(hud[HUD::Carrots].x,baseline[HUD::Carrots].x));
        assert(Near(hud[HUD::Carrots].y,baseline[HUD::Carrots].y+8));
        const auto b=HUDButtons::GetEffectiveLayout();
        assert(Near(b.item_x,buttons.item_x+20-20));
        assert(Near(b.item_y,buttons.item_y+10+8));
        assert(Near(b.buttons[0].pose.x,buttons.buttons[0].pose.x-10-20));
        assert(Near(DPad::GetEffectiveLayout().group.x,dpad.group.x+3+16));
        assert(Near(DPad::GetEffectiveLayout().group.y,dpad.group.y+4+8));
        assert(UserHUD::GetLayout()[UserHUD::Rupees].x==12);
        assert(Same(HUD::GetLayout()[HUD::Rupees],baseline[HUD::Rupees]));
    }
    offsets={};offsets.top=-8;offsets.bottom=-12;offsets.left=-16;offsets.right=-20;
    UserHUD::SetLayout(offsets,false);
    assert(Near(HUD::GetEffectiveLayout()[HUD::Hearts].x,baseline[HUD::Hearts].x-16));
    assert(Near(HUD::GetEffectiveLayout()[HUD::Hearts].y,baseline[HUD::Hearts].y-8));
    assert(Near(HUD::GetEffectiveLayout()[HUD::Minimap].x,baseline[HUD::Minimap].x+20));
    assert(Near(HUD::GetEffectiveLayout()[HUD::Minimap].y,baseline[HUD::Minimap].y+12));
    offsets.top=999;offsets.bottom=-999;offsets.left=999;offsets.right=-999;
    UserHUD::SetLayout(offsets,false);
    assert(UserHUD::GetLayout().top==40 && UserHUD::GetLayout().bottom==-40);
    assert(UserHUD::GetLayout().left==100 && UserHUD::GetLayout().right==-100);
    UserHUD::SetLayout({},false);unchanged();
    // Corrupt saved values cannot reach render math.
    offsets[0]={std::numeric_limits<float>::quiet_NaN(),9999,-999};
    UserHUD::SetLayout(offsets,false);
    assert(UserHUD::GetLayout()[0].x==0 && UserHUD::GetLayout()[0].y==240 &&
           UserHUD::GetLayout()[0].scale==-90);
    UserHUD::SetLayout({},false);
    // Shared button scale includes A, B, every item, and the D-pad. Individual
    // screen-space changes must not move another button or shift its group pivot.
    offsets={};offsets.button_scale=50;UserHUD::SetLayout(offsets,false);
    const auto grouped=HUDButtons::GetEffectiveLayout();
    assert(Near(HUD::GetEffectiveLayout()[HUD::AButton].scale,baseline[HUD::AButton].scale*1.5f));
    assert(Near(grouped.buttons[HUDButtons::B].pose.scale,buttons.buttons[HUDButtons::B].pose.scale*1.5f));
    assert(Near(DPad::GetEffectiveLayout().group.scale_x,dpad.group.scale_x*1.5f));
    for(unsigned i=HUDButtons::X;i<HUDButtons::Count;++i) {
        const auto before=*HUDButtons::ButtonPose(grouped,i);
        assert(Near(before.scale,HUDButtons::ButtonPose(buttons,i)->scale*1.5f));
        auto individual=offsets;individual[UserHUD::XButton+i-HUDButtons::X]={7,-9,20};
        UserHUD::SetLayout(individual,false);
        const auto effective=HUDButtons::GetEffectiveLayout();
        for(unsigned j=HUDButtons::X;j<HUDButtons::Count;++j) {
            const auto actual=*HUDButtons::ButtonPose(effective,j);
            const auto expected=*HUDButtons::ButtonPose(grouped,j);
            if(i==j)assert(Near(actual.x,expected.x+7) && Near(actual.y,expected.y-9) && Near(actual.scale,expected.scale*1.2f));
            else assert(Same(actual,expected));
        }
    }
    // A/B must scale their separation around the same authored midpoint.
    // Their storage conventions differ: A is relative, B is absolute.
    const auto old_a=baseline[HUD::AButton];const auto old_b=buttons.buttons[HUDButtons::B].pose;
    const float ax=HUD::ActionAnchorX+old_a.x,ay=HUD::ActionAnchorY+old_a.y;
    for(float percent:{-50.f,0.f,50.f,100.f}) {
        UserHUD::Layout pair;pair.button_scale=percent;pair.top=8;pair.right=12;
        UserHUD::SetLayout(pair,false);
        for(unsigned frame=0;frame<10;++frame) {
            const auto a=HUD::GetEffectiveLayout()[HUD::AButton];
            const auto b=HUDButtons::GetEffectiveLayout().buttons[HUDButtons::B].pose;
            const float x=HUD::ActionAnchorX+a.x,y=HUD::ActionAnchorY+a.y,factor=1+percent/100;
            assert(Near(x-b.x,(ax-old_b.x)*factor));
            assert(Near(y-b.y,(ay-old_b.y)*factor));
            assert(Near((x+b.x)/2,(ax+old_b.x)/2-12));
            assert(Near((y+b.y)/2,(ay+old_b.y)/2+8));
            assert(Same(HUD::GetLayout()[HUD::AButton],old_a));
            assert(Same(HUDButtons::GetLayout().buttons[HUDButtons::B].pose,old_b));
        }
        const auto before_a=HUD::GetEffectiveLayout()[HUD::AButton];
        const auto before_b=HUDButtons::GetEffectiveLayout().buttons[HUDButtons::B].pose;
        pair[UserHUD::AButton]={7,-9,10};UserHUD::SetLayout(pair,false);
        const auto after_a=HUD::GetEffectiveLayout()[HUD::AButton];
        assert(Near(after_a.x,before_a.x+7) && Near(after_a.y,before_a.y-9));
        assert(Same(HUDButtons::GetEffectiveLayout().buttons[HUDButtons::B].pose,before_b));
    }
    // Heart spacing composes with F1, resets to it, and never modifies it.
    auto spaced=baseline;spaced.heart_spacing=4;HUD::SetLayout(spaced,false);
    UserHUD::Layout hearts;hearts.heart_spacing=3;UserHUD::SetLayout(hearts,false);
    assert(HUD::GetEffectiveLayout().heart_spacing==7 && HUD::GetLayout().heart_spacing==4);
    hearts.heart_spacing=28;UserHUD::SetLayout(hearts,false);
    assert(HUD::GetEffectiveLayout().heart_spacing==20);
    hearts.heart_spacing=-28;UserHUD::SetLayout(hearts,false);
    assert(HUD::GetEffectiveLayout().heart_spacing==-8);
    UserHUD::SetLayout({},false);assert(HUD::GetEffectiveLayout().heart_spacing==4);
    HUD::SetLayout(baseline,false);
    offsets.heart_spacing=3;
    offsets.magic_border=3;offsets.magic_height=4;offsets.magic_width=1.5f;
    offsets[UserHUD::XButton]={7,-9,20};UserHUD::SetLayout(offsets,false);
    assert(HUD::GetEffectiveLayout().magic_height==4 && HUD::GetEffectiveLayout().magic_border==3);
    assert(HUD::GetEffectiveLayout().magic_width==1.5f);
    assert(HUD::GetLayout().magic_height==baseline.magic_height);
    assert(HUD::GetLayout().magic_border==baseline.magic_border);
    UserHUD::hd_numbers_override=0;assert(!HUD::GetEffectiveLayout().hd_numbers);
    assert(HUD::GetLayout().hd_numbers==baseline.hd_numbers);
    UserHUD::hd_numbers_override=1;assert(HUD::GetEffectiveLayout().hd_numbers);
    assert(HUD::GetEffectiveLayout().reposition_area_titles);
    assert(HUD::GetEffectiveLayout().area_title_y==baseline.area_title_y);
    // All persistence checks are isolated from the user's installed settings.
    QTemporaryDir temporary;assert(temporary.isValid());
    QSettings config(temporary.filePath("hud.ini"),QSettings::IniFormat);
    config.setValue("Restorations/area_titles",false);config.setValue("UserHUD/area_title_y",-48);
    config.setValue("HUD/magic/height",baseline.magic_height);
    config.setValue("HUD/hd_numbers",true);
    auto authored=baseline;UserHUD::LoadAuthoredTitles(config,authored);
    assert(authored.reposition_area_titles && authored.area_title_y==-48);
    auto scaled=UserHUD::GetLayout();scaled.overall_scale=1.375f;UserHUD::SetLayout(scaled,false);
    UserHUD::SaveConfig(config);config.sync();
    assert(!config.value("Restorations/area_titles").toBool());
    assert(config.value("UserHUD/area_title_y").toFloat()==-48);
    assert(config.value("HUD/magic/height").toFloat()==baseline.magic_height);
    assert(config.value("HUD/hd_numbers").toBool());
    assert(!config.contains("HUD/area_titles/y"));
    UserHUD::SetLayout({},false);UserHUD::hd_numbers_override=-1;
    UserHUD::LoadConfig(config);
    const auto restored=UserHUD::GetLayout();
    assert(restored.overall_scale==1.375f && !config.contains("HUD/overall_scale"));
    assert(restored.heart_spacing==3);
    assert(!config.contains("HUD/hearts/spacing"));
    assert(restored.button_scale==50 && restored[UserHUD::XButton].x==7);
    assert(restored.magic_height==4 && restored.magic_border==3 && restored.magic_width==1.5f);
    assert(UserHUD::hd_numbers_override==1);
    // Obsolete personal overrides cannot defeat the authored repositioning.
    config.setValue("UserHUD/area_titles/enabled_override",true);
    config.setValue("UserHUD/area_titles/enabled",false);
    config.setValue("UserHUD/area_titles/y_override",true);
    config.setValue("UserHUD/area_titles/y",50);
    // F1 remains the sole owner of area-title placement.
    authored.area_title_y=-60;UserHUD::SaveAuthoredTitles(config,authored);
    UserHUD::LoadAuthoredTitles(config,authored);UserHUD::LoadConfig(config);
    assert(authored.reposition_area_titles && authored.area_title_y==-60);
    HUD::SetLayout(authored,false);
    assert(HUD::GetEffectiveLayout().reposition_area_titles && HUD::GetEffectiveLayout().area_title_y==-60);
    HUD::SetLayout(baseline,false);
    UserHUD::SetLayout({},false);UserHUD::hd_numbers_override=-1;
    UserHUD::SaveConfig(config);UserHUD::LoadConfig(config);unchanged();
    assert(UserHUD::hd_numbers_override==-1);
    // Old outdoor-only adjustments migrate once to the unified minimap.
    QSettings legacy(temporary.filePath("legacy.ini"),QSettings::IniFormat);
    legacy.setValue("UserHUD/outside_minimap/x",-23);UserHUD::LoadConfig(legacy);
    assert(UserHUD::GetEffectiveOffsets()[UserHUD::Minimap].x==-23);
    assert(UserHUD::GetEffectiveOffsets()[UserHUD::OutsideMinimap].x==-23);
    UserHUD::SetLayout({},false);UserHUD::SaveConfig(legacy);UserHUD::LoadConfig(legacy);
    assert(UserHUD::GetEffectiveOffsets()[UserHUD::Minimap].x==0);unchanged();
    assert(!HUD::dirty && !HUDButtons::dirty && !DPad::dirty);
    // Each hotkey closes its own view; the other switches without closing.
    available=false;visible=false;Toggle(true);assert(!visible);
    available=true;Toggle(true);assert(visible && player_view);
    Toggle();assert(visible && !player_view);
    Toggle();assert(!visible);
    Toggle();Toggle(true);assert(visible && player_view);
    Toggle(true);assert(!visible);
    std::cout<<"User HUD identity, reset, composition, isolation and F1/F2 checks passed\n";
}
