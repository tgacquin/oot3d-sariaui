// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/death_menu.h"
#include "oot3d/loading_icon_assets.h"

namespace OOT3D::SaveLoading {
using TitleScreen::U32;
struct Fade {
    U32 owner{};
    std::int64_t last{};
    float opacity{};
    float Advance(std::int64_t now,U32 next_owner,bool visible,bool busy) {
        if(!visible || !next_owner) {owner=0;last=now;opacity=0;return 0;}
        if(owner!=next_owner || now<last || now-last>250000) {
            owner=next_owner;last=now;opacity=0;
        }
        const float dt=std::clamp(float(now-last)/1000000.f,0.f,.1f);
        last=now;
        opacity=std::clamp(opacity+(busy?dt/.08f:-dt/.10f),0.f,1.f);
        return opacity*opacity*(3.f-2.f*opacity);
    }
};
struct Hook {
    GuestUI::Hook<LoadingIcon::Geometry,64,64,4> icon;
    Fade fade;
    bool bottom_projection{};
    bool Pending() const {return icon.Pending();}
    bool Drawing() const {return icon.Drawing();}
    template<class M,class R> void Resume(M& m,R& r) {icon.Resume(m,r);}
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc,bool death,bool supported,
                                         bool bottom,std::int64_t now) {
        U32 owner=SaveMenu::Owner,source=0;
        bool visible=false,busy=false;
        if(death) {
            owner=DeathMenu::Controller(m);
            const U32 state=m.Read(owner+8).value_or(0)&255;
            visible=supported && DeathMenu::on_top.load() && DeathMenu::Drawable(m,owner) && state>=3 && state<=11;
            busy=state>=5 && state<=7 && (m.Read(owner+0xc).value_or(0)&255)!=0;
            const U32 pane=m.Read(owner+DeathMenu::PanesOffset+5*4).value_or(0);
            if(TitleScreen::Pointer(pane,0xb4))source=m.Read(pane+0x64).value_or(0);
        } else {
            visible=supported && SaveMenu::on_top.load() && m.Read(owner+0x28)==2;
            const U32 state=m.Read(owner+0x24).value_or(~0u);
            busy=state==8 || state==9; // Save accepted -> write in progress.
            source=m.Read(owner+4).value_or(0);
        }
        const float opacity=fade.Advance(now,owner,visible,busy);
        if(opacity<=0)return;
        if(const auto image=LoadingIcon::GetPixels()) {
            bottom_projection=bottom;
            const auto geometry=LoadingIcon::Draw(now,.8f*opacity,HUD::GetEffectiveLayout()[HUD::LoadingIcon]);
            icon.Begin(m,r,return_pc,geometry,*image,source);
        }
    }
};
}
