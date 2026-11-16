// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/file_particles.h"
#include "oot3d/graphics_memory.h"
#include "oot3d/dungeon_collection.h"
namespace OOT3D::MapParticles {
// Submitted after the brown backgrounds/darkness and before native map paper,
// stones, banners and their shadows. Foreground naturally occludes the motes.
using Geometry=FileParticles::GeometryFor<FileParticles::DungeonMaxParticles>;
struct Hook : GuestUI::Hook<Geometry,FileParticles::Width,FileParticles::Height> {
    FileParticles::Animation animation;
    DungeonCollection::Transition progress;
    template<class M,class R> void Begin(M& m,R& r,TitleScreen::U32 entry) {
        if(!PauseMenu::DungeonPage(m) || !PauseMenu::Active(m) || SaveMenu::BackdropActive(m))return;
        const auto layout=PauseMenu::GetLayout();
        const auto& settings=layout.dungeon_particles;
        if(!settings.enabled) {animation.Reset();progress.Reset();return;}
        const auto image=FileParticles::GetPixels();if(!image)return;
        if(!Owned(m)) {
            const auto free=GraphicsMemory::TextureFree(m);
            if(free.valid && (free.bytes<1024*1024 || free.largest<TextureBytes+65536))return;
        }
        const auto now=FileParticles::Animation::Clock::now();
        float density=-1,drift=-1;
        if(layout.dungeon_particles_progress) {
            float fraction=0;
            if(const auto collected=DungeonCollection::Read(m,m.Read(PauseMenu::UI+0xc).value_or(0)))
                fraction=progress.Update(*collected,now);
            else progress.Reset();
            density=DungeonCollection::Density(fraction,layout.dungeon_particle_endpoints);
            drift=DungeonCollection::DriftSpeed(fraction,layout.dungeon_particle_endpoints);
        } else progress.Reset();
        auto geometry=animation.Frame<FileParticles::DungeonMaxParticles>(settings,
            Widescreen::Active()?240*Widescreen::Aspect:400,now,density,drift);
        const float offset=PauseMenu::Scale*PauseMenu::motion_x.load();
        for(unsigned i=0;i<geometry.count*4;++i)geometry.positions[i*3]+=offset;
        GuestUI::Hook<Geometry,FileParticles::Width,FileParticles::Height>::Begin(
            m,r,entry,geometry,*image,m.Read(PauseMenu::UI+0x10).value_or(0));
    }
};
}
