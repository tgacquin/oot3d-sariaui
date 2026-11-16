// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/dialogue_camera.h"

namespace OOT3D::DialogueCamera {
// Temporary, bounded diagnosis of the actual update -> render handoff. Log
// only the opening/closing windows, never change camera memory or settings.
struct Trace {
    enum Stage { Begin, Finish, Entry, Projection, LookAt, Lens, Count };
    static constexpr std::array<const char*,Count> Names{
        "begin","finish","entry","projection","lookat","lens"};
    U32 owner{};
    bool interacting{},seen{};
    std::int64_t until{},last_time{};
    std::array<std::int64_t,Count> stage_time{};
    unsigned samples{};
    template<class M,class Sink> void Observe(M& m,Stage stage,U32 cam,std::uint64_t frame,
            std::int64_t now,const OrbitCamera::Hook& orbit,const Hook& dialogue,
            float lens,U32 eye_address,U32 at_address,Sink&& sink) {
        if(samples>=6000 || !TitleScreen::Pointer(cam,0x1bc))return;
        const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
        if(cam!=play+0x364 || m.Read(0x5043e0)!=play || !TitleScreen::Pointer(player,0x1718))return;
        const U32 mode=OrbitCamera::Half(m,cam+0x18c);
        const U32 actor_flags=m.Read(player+4).value_or(0);
        const U32 target=m.Read(player+0x16f8).value_or(0);
        const bool next=mode==3 || ((actor_flags&0x100) && target);
        if(!seen || owner!=cam || now<last_time) {
            owner=cam;interacting=false;until=0;stage_time.fill(-1);
        }
        if(next!=interacting) {until=now+3000000;stage_time.fill(-1);}
        interacting=next;seen=true;last_time=now;
        if(now>=until || (stage_time[stage]>=0 && now-stage_time[stage]<5000))return;
        stage_time[stage]=now;++samples;
        Vec eye{},at{};
        TitleScreen::ReadFloats(m,eye_address,eye);
        TitleScreen::ReadFloats(m,at_address,at);
        const auto read_float=[&](U32 address) {
            return std::bit_cast<float>(m.Read(address).value_or(0));
        };
        const U32 setting=OrbitCamera::Half(m,cam+0x18a);
        const U32 modes=setting<=0x46?m.Read(0x516ff0+setting*8+4).value_or(0):0;
        const U32 profile=TitleScreen::Pointer(modes,32)?m.Read(modes+3*8+4).value_or(0):0;
        sink(Names[stage],frame,now,cam,mode,setting,profile,OrbitCamera::Half(m,cam+0x188),
            OrbitCamera::Half(m,play+0xa64),actor_flags,target,
            m.Read(player+0x1710).value_or(0),m.Read(player+0x1714).value_or(0),
            OrbitCamera::Allowed(m,cam),OrbitCamera::GameplayLens(m,play+0x188),
            orbit.active,orbit.dialogue.valid,orbit.dialogue.frame,orbit.dialogue_release,
            orbit.release_frame,orbit.GameplayFov(m,play+0x188),
            dialogue.native_active,dialogue.native_seeded,dialogue.gameplay_seen,dialogue.gameplay_frame,
            OrbitCamera::Half(m,cam+0x1a6),OrbitCamera::Half(m,cam+0x4c),
            read_float(cam+0x144),read_float(play+0x198),lens,
            eye[0],eye[1],eye[2],at[0],at[1],at[2]);
    }
};
} // namespace OOT3D::DialogueCamera
