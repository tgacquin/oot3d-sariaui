// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/multiplayer_startup.h"
#include "oot3d/loading_icon_assets.h"
#include "oot3d/hud_hooks.h"
#include "oot3d/widescreen.h"
#include "oot3d/file_status_assets.h"
#include "oot3d/randomizer/file_status.h"
#include "oot3d/ui_language.h"
#include <cmath>

namespace OOT3D::MultiplayerLoading {
struct Hook {
    using Geometry=PausePrompts::QuadGeometry<1>;
    GuestUI::Hook<Geometry,8,8> cover;
    GuestUI::Hook<LoadingIcon::Geometry,64,64,4> icon;
    GuestUI::Hook<FileStatusText::Geometry,FileStatusText::Width,FileStatusText::Height> error_text;
    std::shared_ptr<const FileStatusText::Assets> error_font;
    FileStatusText::Geometry error_geometry;
    enum class Stage {Idle,Cover,Icon,Error};
    Stage stage=Stage::Idle;
    bool after_native_fade=false;
    unsigned generation=0;
    float opacity=0;
    std::int64_t last=0,ready_at=0,failed_at=-1;
    std::uint32_t entry=0,source=0;
    LoadingIcon::Geometry icon_geometry;
    bool Pending() const {return stage!=Stage::Idle;}
    bool Drawing() const {return cover.Drawing() || icon.Drawing() || error_text.Drawing();}
    bool Needed() const {
        return MultiplayerStartup::phase!=MultiplayerStartup::Phase::Idle &&
            (generation!=MultiplayerStartup::generation.load() || opacity>0);
    }
    template<class M,class R> void BeginIcon(M& m,R& r) {
        if(const auto pixels=LoadingIcon::GetPixels()) {
            icon.Begin(m,r,entry,icon_geometry,*pixels,source);
            if(icon.Pending()){stage=Stage::Icon;return;}
        }
        BeginError(m,r);
    }
    template<class M,class R> void BeginError(M& m,R& r) {
        if(error_font && error_geometry.count) {
            error_text.Begin(m,r,entry,error_geometry,error_font->pixels,source);
            if(error_text.Pending()) {stage=Stage::Error;return;}
        }
        stage=Stage::Idle;error_font.reset();
    }
    template<class M,class R> void Resume(M& m,R& r) {
        if(stage==Stage::Cover) {
            cover.Resume(m,r);if(cover.Pending())return;
            BeginIcon(m,r);
        } else if(stage==Stage::Icon) {
            icon.Resume(m,r);if(!icon.Pending())BeginError(m,r);
        } else if(stage==Stage::Error) {
            error_text.Resume(m,r);if(!error_text.Pending()) {stage=Stage::Idle;error_font.reset();}
        }
    }
    template<class M,class R> void Begin(M& m,R& r,std::uint32_t pc,std::int64_t now) {
        const auto next=MultiplayerStartup::generation.load();
        if(generation!=next) {generation=next;opacity=1;last=now;ready_at=0;failed_at=-1;}
        const float dt=std::clamp(float(now-last)/1000000.f,0.f,.1f);last=now;
        if(MultiplayerStartup::Busy()) {opacity=1;ready_at=0;}
        else {
            if(!ready_at)ready_at=now;
            // Let the ordinary HUD/scene entrance settle before revealing it.
            if(now-ready_at>=350000)opacity=std::max(0.f,opacity-dt/.25f);
        }
        if(opacity<=0)return;
        source=m.Read(HUD::Action+0x20).value_or(0);entry=pc;
        const float left=Widescreen::Active()?(400-240*Widescreen::Aspect)/2:0;
        Geometry geometry;geometry.Add(left,0,400-2*left,240,0,0,1,1);
        for(unsigned v=0;v<4;++v)geometry.colors[v*4+3]=opacity;
        constexpr auto black=[] {std::array<std::uint32_t,64> p{};p.fill(0x000000ff);return p;}();
        auto pose=HUD::GetEffectiveLayout()[HUD::LoadingIcon];
        error_font.reset();error_geometry={};
        const bool failed=MultiplayerStartup::phase==MultiplayerStartup::Phase::Failed;
        if(failed) {
            if(failed_at<0)failed_at=now;
            const float t=std::clamp(float(now-failed_at)/200000.f,0.f,1.f);
            pose.x+=5.f*std::sin(t*6*3.141592654f)*(1-t);
            if((error_font=FileStatusText::GetAssets())) {
                const auto label=Randomizer::FileStatus::ErrorLabel(Randomizer::FileStatus::Error::Connection,UILanguage::MessageLanguage());
                const float scale=std::min(.8f,360.f/std::max(1.f,FileStatusText::Measure(*error_font,label,1.f)));
                const float width=FileStatusText::Measure(*error_font,label,scale);
                error_geometry=FileStatusText::Text(*error_font,std::string(label),200-width/2,120,width+1,scale);
                for(unsigned i=0;i<error_geometry.count*16;i+=4) {
                    error_geometry.colors[i]=1;error_geometry.colors[i+1]=error_geometry.colors[i+2]=.18f;
                    error_geometry.colors[i+3]=opacity;
                }
            }
        } else failed_at=-1;
        icon_geometry=LoadingIcon::Draw(now,.8f*opacity,pose);
        if(failed)for(unsigned i=0;i<icon_geometry.count*16;i+=4) {
            icon_geometry.colors[i]=1;icon_geometry.colors[i+1]=icon_geometry.colors[i+2]=.18f;
        }
        cover.Begin(m,r,entry,geometry,black,source);
        if(cover.Pending())stage=Stage::Cover;else BeginIcon(m,r);
    }
};
}
