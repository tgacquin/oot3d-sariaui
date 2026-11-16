// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace OOT3D::FileParticles {
inline constexpr unsigned DungeonMaxParticles=192;
struct Progression {
    int density_start=32,density_end=DungeonMaxParticles;
    float drift_start=1,drift_end=2;
};
struct Settings {
    bool enabled=true;
    int density=24;
    float glow=.7f,size=1,speed=1;
    float light_strength=.35f,light_radius=1,spin=1;
    bool variations=true;
};
}
