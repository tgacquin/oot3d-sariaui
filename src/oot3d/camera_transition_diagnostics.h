// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/orbit_camera.h"
#include <chrono>
#include <sstream>

namespace OOT3D::CameraTransitionDiagnostics {
using TitleScreen::U32;
inline std::atomic<std::int64_t> begins{0},ends{0};
inline std::atomic<unsigned> capture{0},samples{0};
inline std::int64_t Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void Arm(std::int64_t now=Now()) {
    ends=0;samples=0;begins=now+3000;++capture;ends=now+33000;
}
inline bool Pending(std::int64_t now=Now()) {return now<ends.load();}
inline bool Recording(std::int64_t now=Now()) {
    return Pending(now) && now>=begins.load();
}
inline void Stop() {ends=0;}
enum Stage {Begin,NativeReturn,Finish,Projection,LookAt,Lens,Count};
constexpr std::array Names{"begin","native-return","finish","projection","lookat","lens"};
struct Trace {
    unsigned session=0,total=0;
    struct Seen {std::int64_t us=-1;std::array<U32,4> cameras{};};
    std::array<Seen,Count> seen{};
    template<class M,class Sink> bool Observe(M& m,Stage stage,U32 cam,std::uint64_t frame,
            std::int64_t us,const OrbitCamera::Hook& orbit,float lens,U32 eye_address,
            U32 at_address,U32 dispatch,Sink&& sink,std::int64_t wall=Now()) {
        // Read-only, bounded, and independent of the interpolation toggle.
        if(!Recording(wall) || stage>=Count)return false;
        const auto id=capture.load();
        if(session!=id) {session=id;total=0;seen={};}
        if(total>=12000 || !TitleScreen::Pointer(cam,0x1bc))return false;
        const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
        if(!TitleScreen::Pointer(play,0x7f14) || m.Read(0x5043e0)!=play ||
           !TitleScreen::Pointer(player,0x1718))return false;
        // Include subcameras during doors/cutscenes. Deduplicate repeated eyes
        // and collision returns by emulated time, not interpolation frame IDs.
        auto& s=seen[stage];
        if(s.us!=us) {s={};s.us=us;}
        if(std::find(s.cameras.begin(),s.cameras.end(),cam)!=s.cameras.end())return false;
        auto free=std::find(s.cameras.begin(),s.cameras.end(),0u);
        if(free==s.cameras.end())return false;
        OrbitCamera::Vec eye{},at{},pos{};
        if(!TitleScreen::ReadFloats(m,eye_address?eye_address:cam+0x8c,eye) ||
           !TitleScreen::ReadFloats(m,at_address?at_address:cam+0x80,at) ||
           !TitleScreen::ReadFloats(m,player+0x28,pos))return false;
        *free=cam;++total;++samples;
        const auto read=[&](U32 address){return m.Read(address).value_or(0);};
        const auto half=[&](U32 address){return OrbitCamera::Half(m,address);};
        const auto f=[&](U32 address){return std::bit_cast<float>(read(address));};
        float radius=0;for(unsigned i=0;i<3;++i)radius+=(eye[i]-at[i])*(eye[i]-at[i]);
        std::ostringstream out;out.precision(8);
        out<<"capture="<<id<<" stage="<<Names[stage]<<" frame="<<frame<<" us="<<us
           <<" elapsed_ms="<<wall-begins.load()<<" cam="<<cam<<" play="<<play<<" player="<<player
           <<" main="<<(cam==play+0x364)<<" scene="<<half(play+0x104)<<" active_camera="<<half(play+0xa64)
           <<" status="<<half(cam+0x188)<<" setting="<<half(cam+0x18a)<<" mode="<<half(cam+0x18c)
           <<" phase="<<half(cam+0x1a6)<<" timer="<<half(cam+0x4c)<<" dispatch="<<dispatch
           <<" flags="<<read(player+0x1710)<<'/'<<read(player+0x1714)
           <<" request="<<read(play+0x5c2c)<<" fade="<<read(play+0x7f10)
           <<" cutscene_state="<<(read(play+0x22a0)&255)
           <<" room_environment="<<((read(play+0x4c30)>>16)&255)
           <<" room_hot="<<(read(play+0x4c34)>>24)
           <<" script="<<read(play+0x4c30)<<'/'<<read(play+0x4c34)
           <<" enabled="<<OrbitCamera::enabled.load()<<" input="<<OrbitCamera::input_allowed.load()
           <<" allowed="<<OrbitCamera::Allowed(m,cam)<<" targeting="<<OrbitCamera::Targeting(m,cam)
           <<" entrance="<<OrbitCamera::NativeEntrance(m,cam)<<" entry_view="<<OrbitCamera::EntryView(m,cam)
           <<" entry_armed="<<OrbitCamera::entry_state.armed<<" orbit="<<orbit.active<<" owner="<<orbit.owner
           <<" cached_entry="<<orbit.native_entry.valid<<" cached_cam="<<orbit.native_entry.camera
           <<" cached_frame="<<orbit.native_entry.frame<<" entry_match="<<orbit.EntryMatches(m,cam,frame)
           <<" entry_release="<<orbit.entry_release<<" dialogue_release="<<orbit.dialogue_release
           <<" door_start_lens="<<orbit.DoorStartLens(m,play+0x188)
           <<" release_frame="<<orbit.release_frame<<" epoch="<<OrbitCamera::reset_epoch.load()
           <<" distance_scale="<<OrbitCamera::ConfiguredDistance()<<" wanted_radius="<<OrbitCamera::ConfiguredRadius(m)
           <<" orbit_radius="<<orbit.radius<<" radius="<<std::sqrt(radius)
           <<" configured_fov="<<Camera::ConfiguredFov()<<" native_fov="<<f(cam+0x144)
           <<" view_fov="<<f(play+0x198)<<" effective_fov="<<lens
           <<" gameplay_lens="<<OrbitCamera::GameplayLens(m,play+0x188)
           <<" orbit_fov="<<orbit.GameplayFov(m,play+0x188)
           <<" eye=["<<eye[0]<<','<<eye[1]<<','<<eye[2]<<"] at=["<<at[0]<<','<<at[1]<<','<<at[2]
           <<"] player_pos=["<<pos[0]<<','<<pos[1]<<','<<pos[2]<<']';
        sink(out.str());return true;
    }
};
}
