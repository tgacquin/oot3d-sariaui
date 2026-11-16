// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/dungeon_collection_data.h"
#include "oot3d/dungeon_location_assets.h"
#include "oot3d/file_particle_settings.h"
#include "oot3d/title_hooks.h"
#include <bit>
#include <chrono>
#include <cmath>

namespace OOT3D::DungeonCollection {
constexpr std::uint32_t Save=0x587958;
struct Progress {
    unsigned dungeon=0,collected=0,total=0;
    bool mq=false;
    float Fraction() const {return total?float(collected)/total:0;}
};
template<class M> std::optional<Progress> Read(M& m,std::uint32_t play) {
    if(!TitleScreen::Pointer(play,0x2248))return {};
    const auto scene_word=m.Read(play+0x104),mode_word=m.Read(Save+0xc);
    if(!scene_word || !mode_word)return {};
    const unsigned scene=*scene_word&0xffff;
    const auto dungeon=DungeonLocation::TitleIndex(scene);
    if(!dungeon)return {};
    Progress p{.dungeon=*dungeon,.mq=((*mode_word>>16)&255)!=0};
    for(const auto& mask:Locations[p.dungeon*2+unsigned(p.mq)]) {
        if(!mask.bits)continue;
        std::optional<std::uint32_t> flags;
        if(mask.kind==Kind::Skulltula) {
            const auto at=Save+0xeb4+mask.scene;
            if(const auto word=m.Read(at&~3u))flags=(*word>>((at&3)*8))&255;
        } else {
            const bool chest=mask.kind==Kind::Chest;
            // The active scene's flags have not necessarily reached the save.
            const auto at=scene==mask.scene?play+(chest?0x2238:0x2244):
                Save+0xec+mask.scene*0x1c+(chest?0:0xc);
            flags=m.Read(at);
        }
        if(!flags)return {}; // Do not report false completion from missing memory.
        p.total+=std::popcount(mask.bits);
        p.collected+=std::popcount(*flags&mask.bits);
    }
    return p.total?std::optional{p}:std::nullopt;
}
inline float Density(float progress,const FileParticles::Progression& endpoints={}) {
    const float start=std::clamp(endpoints.density_start,0,int(FileParticles::DungeonMaxParticles));
    const float end=std::clamp(endpoints.density_end,0,int(FileParticles::DungeonMaxParticles));
    return std::lerp(start,end,std::clamp(progress,0.f,1.f));
}
inline float DriftSpeed(float progress,const FileParticles::Progression& endpoints={}) {
    const float start=std::isfinite(endpoints.drift_start)?std::clamp(endpoints.drift_start,0.f,6.f):1.f;
    const float end=std::isfinite(endpoints.drift_end)?std::clamp(endpoints.drift_end,0.f,6.f):2.f;
    return std::lerp(start,end,std::clamp(progress,0.f,1.f));
}
struct Transition {
    using Clock=std::chrono::steady_clock;
    Clock::time_point last{};
    float fraction=0;
    unsigned dungeon=~0u;
    bool mq=false;
    void Reset() {*this={};}
    float Update(const Progress& p,Clock::time_point now=Clock::now()) {
        const double dt=std::chrono::duration<double>(now-last).count();
        // On a newly opened menu/other dungeon show its existing progress.
        if(dungeon!=p.dungeon || mq!=p.mq || dt<0 || dt>.25)fraction=p.Fraction();
        else fraction=std::lerp(fraction,p.Fraction(),float(1-std::exp(-dt/.3)));
        if(std::abs(fraction-p.Fraction())<.00001f)fraction=p.Fraction();
        dungeon=p.dungeon;mq=p.mq;last=now;
        return fraction;
    }
};
}
