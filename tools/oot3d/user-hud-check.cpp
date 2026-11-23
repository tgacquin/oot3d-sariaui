// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hud_defaults.h"
#include "oot3d/input.h"
#include "oot3d/user_hud_preset.h"
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
        for(unsigned i=0;i<baseline.size();++i)if(i!=HUD::Keys)assert(Same(hud[i],baseline[i]));
        const auto keys=hud[HUD::Keys],rupees=hud[HUD::Rupees];
        assert(keys.x==rupees.x && keys.scale==rupees.scale);
        assert(Near(20+rupees.y-keys.y,baseline.counter_distance*rupees.scale));
        assert(Same(hud.outside_minimap,baseline.outside_minimap));
        assert(hud.heart_spacing==baseline.heart_spacing && hud.magic_height==baseline.magic_height);
        assert(hud.magic_border==baseline.magic_border && hud.magic_background_opacity==baseline.magic_background_opacity);
        assert(hud.magic_contour.enabled==baseline.magic_contour.enabled &&
               hud.magic_contour.width==baseline.magic_contour.width && hud.magic_contour.height==baseline.magic_contour.height);
        const auto b=HUDButtons::GetEffectiveLayout();
        for(unsigned i=0;i<HUDButtons::Count;++i)
            assert(Same(*HUDButtons::ButtonPose(b,i),*HUDButtons::ButtonPose(buttons,i)));
        const auto d=DPad::GetEffectiveLayout().group;
        assert(d.x==dpad.group.x && d.y==dpad.group.y &&
               d.scale_x==dpad.group.scale_x && d.scale_y==dpad.group.scale_y);
    };
    unchanged();
    // Epona race layout: the HD digits rise above the clock and the carrot
    // shadow extends below the 24px sprites. Check both edges after scaling.
    {
        auto race=baseline;
        race[HUD::Timer]={11.7f,-5.8f,.86f};
        race[HUD::Carrots]={0,182,.71f};
        race.hd_timer_text={-1.2f,-1.4f,1.22f};
        race.carrot_shadow={true,1.5f,1.4f,.8f};
        race.padding={-2,-3,0,0};
        for(bool hd:{false,true})for(float authored:{1.f,1.5f,2.f}) {
            race.hd_numbers=hd;race.overall_scale=authored;HUD::SetLayout(race,false);
            UserHUD::Layout user;user.overall_scale=2;UserHUD::SetLayout(user,false);
            const auto h=HUD::GetEffectiveLayout();
            const float row_bottom=20+h[HUD::Carrots].y+12*h[HUD::Carrots].scale+1.4f;
            const float timer_top=216+h[HUD::Timer].y+(hd?-2.4f:-1.f)*h[HUD::Timer].scale;
            assert(timer_top-row_bottom>=1.999f && h[HUD::Carrots].x==0);
        }
        race.overall_scale=1;HUD::SetLayout(race,false);UserHUD::SetLayout({},false);
        assert(HUD::GetEffectiveLayout()[HUD::Carrots].y==180);
        UserHUD::Layout apart;apart.overall_scale=2;apart[UserHUD::Carrots].x=-300;
        UserHUD::SetLayout(apart,false);assert(HUD::GetEffectiveLayout()[HUD::Carrots].y==180);
        HUD::SetLayout(baseline,false);UserHUD::SetLayout({},false);
    }
    // F1 moves the authored edges on top of the player's existing offsets.
    // Composing either layout must never bake F1 padding into a player preset.
    UserHUD::Layout personal_padding{};
    personal_padding.top=2;personal_padding.bottom=3;personal_padding.left=4;personal_padding.right=5;
    UserHUD::SetLayout(personal_padding,false);
    const auto preset_before=UserHUD::HUDSettings();
    const auto before=HUD::GetEffectiveLayout();
    const auto buttons_before=HUDButtons::GetEffectiveLayout();
    const auto dpad_before=DPad::GetEffectiveLayout();
    auto padded=baseline;
    padded.padding={7,8,9,10};HUD::SetLayout(padded,false);
    const auto after=HUD::GetEffectiveLayout();
    for(auto row:{HUD::Rupees,HUD::Keys}) {
        assert(Near(after[row].x,before[row].x+9) && Near(after[row].y,before[row].y-8));
    }
    assert(Near(after[HUD::Hearts].y,before[HUD::Hearts].y+7));
    assert(Near(after[HUD::Minimap].x,before[HUD::Minimap].x-10));
    assert(Near(after[HUD::AButton].x,before[HUD::AButton].x-10));
    assert(Near(HUDButtons::GetEffectiveLayout().item_x,buttons_before.item_x-10));
    assert(Near(DPad::GetEffectiveLayout().group.x,dpad_before.group.x+9));
    assert(UserHUD::HUDSettings()==preset_before);
    HUD::SetLayout(baseline,false);UserHUD::SetLayout({},false);unchanged();
    // The topmost D-pad slot and left boot must grow about the same corner
    // as the heart rows and magic bar, including when both HUD scalers apply.
    auto stack=baseline;
    stack[HUD::Hearts]={-8.6f,2.9f,.87f};
    stack[HUD::Magic]={-7.8f,2.f,.67f};
    stack.magic_height=1.86f;stack.magic_border=.62f;
    auto pad=dpad;
    pad.group={-21.1f,-22.9f,.6f,.6f};
    DPad::SetLayout(pad,false);
    HUD::SetLayout(stack,false);
    const auto clearance=[] {
        const auto h=HUD::GetEffectiveLayout();
        const auto p=DPad::GetEffectiveLayout().group;
        const float bar_bottom=36+h[HUD::Magic].y+4*(h.magic_height+h.magic_border)*h[HUD::Magic].scale;
        return std::array{DPad::CenterY+p.y-30*p.scale_y-bar_bottom,
                          DPad::CenterX+p.x-30*p.scale_x-(12+h[HUD::Hearts].x)};
    };
    const auto normal=clearance();
    assert(normal[0]>0);
    for(float authored:{.824f,1.f,1.5f,2.f})for(float user:{.5f,1.f,2.f}) {
        stack.overall_scale=authored;stack.padding={3,0,4,0};
        HUD::SetLayout(stack,false);
        UserHUD::Layout settings{};settings.overall_scale=user;settings.top=2;settings.left=5;
        UserHUD::SetLayout(settings,false);
        const auto scaled=clearance();
        assert(Near(scaled[0],normal[0]*authored*user));
        assert(Near(scaled[1],normal[1]*authored*user));
    }
    DPad::SetLayout(dpad,false);HUD::SetLayout(baseline,false);UserHUD::SetLayout({},false);
    // Overall scale reaches every HUD group once, composes with local scales,
    // and preserves padding and the authored layout across repeated frames.
    for(float factor:{.5f,1.f,1.375f,2.f}) {
        UserHUD::Layout all;all.overall_scale=factor;all.left=7;all.top=4;
        all.button_scale=10;all[UserHUD::XButton].scale=20;
        UserHUD::SetLayout(all,false);
        for(unsigned frame=0;frame<3;++frame) {
            const auto hud=HUD::GetEffectiveLayout();
            for(unsigned i=0;i<hud.size();++i) {
                const float expected=baseline[i==HUD::Keys?HUD::Rupees:i].scale*factor*(i==HUD::AButton?1.1f:1.f);
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
            // The entire right-hand cluster must keep proportional spacing,
            // including the gap between the action and item buttons.
            const auto action=b.buttons[HUDButtons::B].pose;
            const auto old_action=buttons.buttons[HUDButtons::B].pose;
            assert(Near(x.x-action.x,(old_x.x-old_action.x)*factor*1.1f));
            assert(Near(x.y-action.y,(old_x.y-old_action.y)*factor*1.1f));
            // Native routed anchors are (12,10) for hearts and (12,36)
            // for magic. Their separation must grow with their geometry.
            assert(Near(hud[HUD::Magic].x-hud[HUD::Hearts].x,
                (baseline[HUD::Magic].x-baseline[HUD::Hearts].x)*factor));
            assert(Near(26+hud[HUD::Magic].y-hud[HUD::Hearts].y,
                (26+baseline[HUD::Magic].y-baseline[HUD::Hearts].y)*factor));
            assert(Near(HUD::ActionAnchorX+hud[HUD::AButton].x-b.buttons[HUDButtons::B].pose.x,
                (HUD::ActionAnchorX+baseline[HUD::AButton].x-buttons.buttons[HUDButtons::B].pose.x)*factor*1.1f));
            assert(Near(DPad::GetEffectiveLayout().group.scale_x,std::clamp(dpad.group.scale_x*factor,.1f,4.f)));
            assert(UserHUD::GetLayout().button_scale==10);
        }
    }
    // Compare rendered bubble edges, including individual offsets and sizes.
    const auto corner=[] {
        const auto a=HUD::GetEffectiveLayout()[HUD::AButton];
        std::array edges{HUD::ActionAnchorX+a.x+19*a.scale,HUD::ActionAnchorY+a.y-19*a.scale};
        const auto geometry=HUDButtons::MakeGeometry(HUDButtons::GetEffectiveLayout(),{},1,false);
        assert(geometry.circles.count==9);
        for(unsigned i=0;i<geometry.circles.count*12;i+=3) {
            edges[0]=std::max(edges[0],geometry.circles.positions[i]);
            edges[1]=std::min(edges[1],geometry.circles.positions[i+1]);
        }
        return edges;
    };
    UserHUD::Layout player;
    player.top=4;player.right=7;player.bottom=3;player.left=11;
    player.button_scale=20;player[UserHUD::XButton]={7,-5,35};
    player[UserHUD::Rupees].scale=24;
    UserHUD::SetLayout(player,false);
    const auto reference=corner();
    const auto wallet=HUD::GetEffectiveLayout()[HUD::Rupees];
    for(float authored:{.5f,1.f,1.4f,2.f})for(float personal:{.5f,.546f,1.f,2.f}) {
        auto defaults=baseline;defaults.overall_scale=authored;HUD::SetLayout(defaults,false);
        player.overall_scale=personal;UserHUD::SetLayout(player,false);
        const auto edges=corner();const auto actual=HUD::GetEffectiveLayout();
        assert(Near(edges[0],reference[0]) && Near(edges[1],reference[1]));
        assert(Near(actual[HUD::Rupees].x,wallet.x));
        assert(Near(actual[HUD::Rupees].y+18*actual[HUD::Rupees].scale,wallet.y+18*wallet.scale));
        assert(Near(actual[HUD::Hearts].scale,std::clamp(baseline[HUD::Hearts].scale*authored*personal,.1f,3.f)));
        assert(Near(DPad::GetEffectiveLayout().group.scale_x,std::clamp(dpad.group.scale_x*authored*personal,.1f,4.f)));
        assert(HUD::GetLayout().overall_scale==authored && UserHUD::GetLayout().overall_scale==personal);
    }
    // Reproduce the saved counter positions and the HD timer and defense outline.
    auto tuned=baseline;
    tuned[HUD::Rupees]={-14.5f,-.8f,.95f};
    tuned[HUD::Keys]={-15.7f,9.8f,.95f};
    tuned[HUD::Timer]={11.7f,1.4f,.86f};
    tuned.hd_timer_text={-1.2f,-1.4f,1.22f};tuned.hd_timer_spacing=-2.6f;
    tuned.heart_contours.double_defense={.enabled=true,.width=.74f,.height=.74f};
    tuned.heart_contours.shadow.x=-1.25f;
    UserHUD::Layout counters;
    counters[UserHUD::Rupees]={-1.6f,6,-4.8f};
    tuned.counter_distance=25;
    counters.counter_distance_offset=3;
    counters.left=3;counters.bottom=4;
    HUD::SetLayout(tuned,false);UserHUD::SetLayout(counters,false);
    const auto reference_hud=HUD::GetEffectiveLayout();
    for(float authored:{.841644f,1.f})for(float user:{.5f,1.f,2.f})for(float group:{.75f,1.f,1.5f}) {
        tuned.overall_scale=authored;HUD::SetLayout(tuned,false);
        counters.overall_scale=user;counters[UserHUD::CounterGroup]={7,-5,100*(group-1)};
        UserHUD::SetLayout(counters,false);
        const auto h=HUD::GetEffectiveLayout();
        const auto k=h[HUD::Keys],r=h[HUD::Rupees],t=h[HUD::Timer];
        const auto k0=reference_hud[HUD::Keys],r0=reference_hud[HUD::Rupees],t0=reference_hud[HUD::Timer];
        const float factor=authored*user*group;
        assert(Near(r.x,r0.x+7) && Near(r.y+18*r.scale,r0.y+18*r0.scale-5));
        assert(k.x==r.x && k.scale==r.scale);
        assert(Near(20+r.y-k.y,28*r.scale));
        assert(Near(20+r.y-k.y,(20+r0.y-k0.y)*factor));
        assert(Near(k.scale,k0.scale*factor) && Near(r.scale,r0.scale*factor));
        // Compare the actual bounding edges of the clock and the styled five-cell field.
        const float right=182-1.2f+(51-4*2.6f)*1.22f;
        const float bottom=215-1.4f+21*1.22f;
        const float center_offset=(right-159)/2;
        assert(Near(t.x+center_offset*t.scale,t0.x+center_offset*t0.scale));
        assert(Near(t.y+(bottom-216)*t.scale,t0.y+(bottom-216)*t0.scale));
        const float hearts=h[HUD::Hearts].scale/tuned[HUD::Hearts].scale;
        assert(Near(h.heart_contours.double_defense.width,.74f*hearts));
        assert(Near(h.heart_contours.double_defense.height,.74f*hearts));
        assert(Near(h.heart_contours.outline.width,tuned.heart_contours.outline.width*hearts));
        assert(Near(h.heart_contours.shadow.x,-1.25f*hearts));
    }
    HUD::SetLayout(baseline,false);
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
    // Shared button scale includes A, B and every item, independently of the D-pad. Individual
    // screen-space changes must not move another button or shift its group pivot.
    offsets={};offsets.button_scale=50;UserHUD::SetLayout(offsets,false);
    const auto grouped=HUDButtons::GetEffectiveLayout();
    assert(Near(HUD::GetEffectiveLayout()[HUD::AButton].scale,baseline[HUD::AButton].scale*1.5f));
    assert(Near(grouped.buttons[HUDButtons::B].pose.scale,buttons.buttons[HUDButtons::B].pose.scale*1.5f));
    assert(Near(DPad::GetEffectiveLayout().group.scale_x,dpad.group.scale_x));
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
    // All six right-hand buttons scale around their shared authored center.
    // Their storage conventions differ: A is relative, B is absolute.
    const auto old_a=baseline[HUD::AButton];const auto old_b=buttons.buttons[HUDButtons::B].pose;
    const float ax=HUD::ActionAnchorX+old_a.x,ay=HUD::ActionAnchorY+old_a.y;
    float center_x=ax+old_b.x,center_y=ay+old_b.y;
    for(unsigned i=HUDButtons::X;i<HUDButtons::Count;++i) {
        const auto p=*HUDButtons::ButtonPose(buttons,i);center_x+=p.x;center_y+=p.y;
    }
    center_x/=6;center_y/=6;
    for(float percent:{-50.f,0.f,50.f,100.f}) {
        UserHUD::Layout pair;pair.button_scale=percent;pair.top=8;pair.right=12;
        UserHUD::SetLayout(pair,false);
        for(unsigned frame=0;frame<10;++frame) {
            const auto a=HUD::GetEffectiveLayout()[HUD::AButton];
            const auto b=HUDButtons::GetEffectiveLayout().buttons[HUDButtons::B].pose;
            const float x=HUD::ActionAnchorX+a.x,y=HUD::ActionAnchorY+a.y,factor=1+percent/100;
            assert(Near(x-b.x,(ax-old_b.x)*factor));
            assert(Near(y-b.y,(ay-old_b.y)*factor));
            float sum_x=x+b.x,sum_y=y+b.y;
            const auto effective=HUDButtons::GetEffectiveLayout();
            for(unsigned i=HUDButtons::X;i<HUDButtons::Count;++i) {
                const auto p=*HUDButtons::ButtonPose(effective,i);
                const auto old=*HUDButtons::ButtonPose(buttons,i);
                assert(Near(p.x-x,(old.x-ax)*factor));
                assert(Near(p.y-y,(old.y-ay)*factor));
                sum_x+=p.x;sum_y+=p.y;
            }
            assert(Near(sum_x/6,center_x-12));
            assert(Near(sum_y/6,center_y+8));
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
    // Group translation moves every button once, after scaling, without moving
    // unrelated HUD elements or overwriting individual adjustments.
    for(float scale:{.5f,1.f,1.5f}) {
        UserHUD::Layout group;group.overall_scale=scale;group.button_scale=20;
        group[UserHUD::AButton]={3,4,0};group[UserHUD::XButton]={-5,6,10};
        UserHUD::SetLayout(group,false);
        const auto hud=HUD::GetEffectiveLayout();const auto b=HUDButtons::GetEffectiveLayout();
        const auto d=DPad::GetEffectiveLayout();
        group.button_x=12.5f;group.button_y=-7.25f;UserHUD::SetLayout(group,false);
        for(unsigned frame=0;frame<3;++frame) {
            const auto moved=HUD::GetEffectiveLayout();const auto buttons_moved=HUDButtons::GetEffectiveLayout();
            for(unsigned i=0;i<hud.size();++i) {
                if(i==HUD::AButton) {
                    assert(Near(moved[i].x,hud[i].x+12.5f) && Near(moved[i].y,hud[i].y-7.25f));
                    assert(moved[i].scale==hud[i].scale);
                } else assert(Same(moved[i],hud[i]));
            }
            for(unsigned i=0;i<HUDButtons::Count;++i) {
                const auto old=*HUDButtons::ButtonPose(b,i),now=*HUDButtons::ButtonPose(buttons_moved,i);
                assert(Near(now.x,old.x+12.5f) && Near(now.y,old.y-7.25f) && Near(now.scale,old.scale));
            }
            const auto now=DPad::GetEffectiveLayout().group;
            assert(Near(now.x,d.group.x) && Near(now.y,d.group.y));
            assert(now.scale_x==d.group.scale_x && now.scale_y==d.group.scale_y);
        }
    }
    UserHUD::SetLayout({},false);unchanged();
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
    offsets.magic_outer_thickness=6.5f;offsets.magic_height=4;offsets.magic_width=1.5f;
    offsets[UserHUD::XButton]={7,-9,20};UserHUD::SetLayout(offsets,false);
    assert(HUD::GetEffectiveLayout().magic_height==4 && HUD::GetEffectiveLayout().magic_border==baseline.magic_border);
    const auto outer=HUD::GetEffectiveLayout().magic_contour;
    assert(outer.enabled && outer.width==6.5f && outer.height==6.5f);
    assert(HUD::GetLayout().magic_contour.width==baseline.magic_contour.width);
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
    assert(authored.reposition_area_titles && authored.area_title_y==-48 && authored.area_title_scale==1);
    auto scaled=UserHUD::GetLayout();scaled.overall_opacity=.5f;scaled.minimap_opacity=.25f;scaled.overall_scale=1.375f;scaled.button_x=12.5f;scaled.button_y=-7.25f;UserHUD::SetLayout(scaled,false);
    scaled.counter_distance_offset=7.5f;
    scaled[UserHUD::CounterGroup]={12,-4,20};UserHUD::SetLayout(scaled,false);
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
    assert(restored.overall_opacity==.5f && restored.minimap_opacity==.25f);
    assert(restored.heart_spacing==3 && restored.counter_distance_offset==7.5f);
    assert(!config.contains("HUD/hearts/spacing"));
    assert(restored.button_scale==50 && restored[UserHUD::XButton].x==7);
    assert(Same(restored[UserHUD::CounterGroup],scaled[UserHUD::CounterGroup]));
    assert(restored.button_x==12.5f && restored.button_y==-7.25f);
    assert(restored.magic_height==4 && restored.magic_outer_thickness==6.5f && restored.magic_width==1.5f);
    assert(UserHUD::hd_numbers_override==1);
    const auto preset_path=temporary.filePath("hud.json");
    const auto exported=UserHUD::HUDSettings();
    assert(UserHUD::ExportPreset(preset_path).isEmpty());
    UserHUD::SetLayout({},false);
    UserHUD::hd_numbers_override=-1;
    UserHUD::classic_file_select=true;
    UserHUD::dirty=false;
    assert(UserHUD::ImportPreset(preset_path).isEmpty());
    assert(UserHUD::HUDSettings()==exported && UserHUD::dirty && UserHUD::classic_file_select);
    QFile damaged(preset_path);
    assert(damaged.open(QIODevice::ReadOnly));
    auto json=QJsonDocument::fromJson(damaged.readAll()).object();
    damaged.close();
    auto incomplete=json.value("settings").toObject();
    incomplete.remove("UserHUD/hd_numbers");
    json.insert("settings",incomplete);
    assert(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate));
    assert(damaged.write(QJsonDocument(json).toJson())>0);
    damaged.close();
    UserHUD::dirty=false;
    assert(!UserHUD::ImportPreset(preset_path).isEmpty());
    assert(UserHUD::HUDSettings()==exported && !UserHUD::dirty);
    // Obsolete personal overrides cannot defeat the authored repositioning.
    config.setValue("UserHUD/area_titles/enabled_override",true);
    config.setValue("UserHUD/area_titles/enabled",false);
    config.setValue("UserHUD/area_titles/y_override",true);
    config.setValue("UserHUD/area_titles/y",50);
    // F1 remains the sole owner of area-title placement.
    authored.area_title_y=-60;authored.area_title_scale=1.75f;
    UserHUD::SaveAuthoredTitles(config,authored);authored={};
    UserHUD::LoadAuthoredTitles(config,authored);UserHUD::LoadConfig(config);
    assert(authored.reposition_area_titles && authored.area_title_y==-60 && authored.area_title_scale==1.75f);
    HUD::SetLayout(authored,false);
    assert(HUD::GetEffectiveLayout().reposition_area_titles && HUD::GetEffectiveLayout().area_title_y==-60);
    HUD::SetLayout(baseline,false);
    UserHUD::SetLayout({},false);UserHUD::hd_numbers_override=-1;
    UserHUD::SaveConfig(config);
    config.setValue("UserHUD/magic/border",3); // obsolete native-frame multiplier
    UserHUD::LoadConfig(config);unchanged();
    assert(UserHUD::hd_numbers_override==-1);
    // Old outdoor-only adjustments migrate once to the unified minimap.
    QSettings legacy(temporary.filePath("legacy.ini"),QSettings::IniFormat);
    legacy.setValue("UserHUD/outside_minimap/x",-23);UserHUD::LoadConfig(legacy);
    assert(UserHUD::EffectiveOffsets(UserHUD::GetLayout(),1)[UserHUD::Minimap].x==-23);
    assert(UserHUD::EffectiveOffsets(UserHUD::GetLayout(),1)[UserHUD::OutsideMinimap].x==-23);
    UserHUD::SetLayout({},false);UserHUD::SaveConfig(legacy);UserHUD::LoadConfig(legacy);
    assert(UserHUD::EffectiveOffsets(UserHUD::GetLayout(),1)[UserHUD::Minimap].x==0);unchanged();
    assert(!HUD::dirty && !HUDButtons::dirty && !DPad::dirty);
    // Each hotkey closes its own view; the other switches without closing.
    available=false;visible=false;Toggle(SettingsView::Player);assert(!visible);
    available=true;Toggle(SettingsView::Player);assert(visible && settings_view==SettingsView::Player);
    Toggle();assert(visible && settings_view==SettingsView::Editor);
    Toggle();assert(!visible);
    Toggle();Toggle(SettingsView::Player);assert(visible && settings_view==SettingsView::Player);
    Toggle(SettingsView::Player);assert(!visible);
    std::cout<<"User HUD identity, reset, composition, isolation and F1/F2 checks passed\n";
}
