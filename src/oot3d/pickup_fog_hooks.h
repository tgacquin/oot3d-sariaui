// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pickup_fog.h"
#include "oot3d/camera.h"
#include "oot3d/title_hooks.h"
#include <string_view>
namespace OOT3D::PickupFog {
template<class M,class R> void Capture(M& m,const R& r,std::int64_t now) {
    // The verified skeletal submission hook runs even at original frame rate.
    // Read-only tagging: no native material, camera or resource edits.
    const auto renderer=r[6];
    if(m.Read(renderer)!=0x4ebd98)return;
    const auto skeleton=m.Read(renderer+8).value_or(0);
    const auto wrapper=m.Read(skeleton+4).value_or(0);
    const auto resource=m.Read(wrapper).value_or(0);
    if(resource<0x44 || m.Read(resource)!=0x206c6b73)return;
    const auto cmb=resource-0x44;
    if(m.Read(cmb)!=0x20626d63 || m.Read(cmb+0x24)!=0x44)return;
    // Verified zelda_keep freestanding collectible family. Do not include
    // get-item presentations (zelda_gi_*), HUD sprites or particle effects.
    std::array<char,17> name{};
    for(unsigned i=0;i<4;++i) {
        const auto word=m.Read(cmb+0x10+4*i);if(!word)return;
        for(unsigned j=0;j<4;++j)name[4*i+j]=char(*word>>(8*j));
    }
    constexpr std::array<std::string_view,13> names{"drop_gi_rupy","drop_gi_heart",
        "drop_gi_bomb_1","drop_gi_hearts_","drop_gi_arrow_0","drop_gi_arrow_1",
        "drop_gi_arrow_2","drop_gi_nuts","drop_gi_stick","drop_gi_seed","drop_gi_key",
        "drop_gi_magicpo","drop_gi_ocarina"};
    const std::string_view model{name.data()};
    const bool torch=model=="torch4_modelT" || model=="torch4_modelT_1";
    if(!torch && std::find(names.begin(),names.end(),model)==names.end())return;
    const auto primitive=r[11];
    if(m.Read(primitive)!=0x736d7270 || m.Read(primitive+8)!=1)return;
    const auto prm=primitive+m.Read(primitive+20).value_or(0);
    if(m.Read(prm)!=0x206d7270)return;
    const auto view=m.Read(0x5be5b8+0xfc).value_or(0);
    if(!Camera::GameplayView(m,view))return;
    Draw draw;draw.scene=m.Read(view-0x188+0x104).value_or(~0u);
    draw.count=m.Read(primitive+12).value_or(0)>>16;
    draw.indices=m.Read(prm+20).value_or(0)&65535;
    if(!draw.count || draw.count>10 || !draw.indices)return;
    if(torch) {
        if(draw.count!=1 || draw.indices!=6)return;
        draw.mode=Torch;
    }
    for(unsigned i=0;i<draw.count;++i)
        if(!TitleScreen::ReadFloats(m,renderer+0x90+48*i,draw.palette[i]))return;
    draw.frame=Interpolation::guest_frames.load();draw.epoch=Interpolation::generation.load();draw.time=now;
    Publish(draw);
}
}
