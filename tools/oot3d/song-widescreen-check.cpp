// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/interpolation_pose.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    using namespace OOT3D;
    using namespace Interpolation;
    assert(argc==2);std::ifstream in(argv[1],std::ios::binary);assert(in);
    const std::string bytes{std::istreambuf_iterator<char>(in),{}};
    for(const char* name:{"efc_ocarina_1_m","efc_ocarina_2_m","efc_ocarina_3_m",
         "efc_ocarina_5_m","efc_ocarina_6_m","efc_ocarina_41_","efc_ocarina_42_"}) {
        assert(bytes.find(name)!=std::string::npos && SongWidescreen::Model(name));
    }
    assert(!SongWidescreen::Model("link_v2") && !SongWidescreen::Model("efc_ocarina_other"));
    auto lens=Math::Identity();lens[0]={0,2.,0,0};lens[1]={-1.2,0,0,0};lens[3]={0,0,-1,0};
    const float native=std::atan(.5)*360/3.141592653589793;
    assert(std::abs(SongWidescreen::Zoom(native,lens)-1)<1e-6);
    lens[0][1]/=1.25;assert(std::abs(SongWidescreen::Zoom(native,lens)-1.25)<1e-6);
    assert(SongWidescreen::Zoom(NAN,lens)==1);
    assert(SongWidescreen::Zoom(native,Math::Identity())==1);
    Pose::Reset();Pose::Sample s;s.key={1,2,3,4,1};s.model="efc_ocarina_3_m";
    s.frame=1;s.time=100000;s.native[0]=Math::Identity();s.world[0]=s.native[0];
    s.camera.valid=true;s.camera.owner=1;s.camera.scene=2;s.camera.epoch=3;
    Pose::Publish(s);Pose::Replay replay;Pose::Palette p{};p[0]=s.native[0];
    assert(replay.SongEffect(p,s.camera,1,100000));
    assert(replay.SongEffect(p,s.camera,2,130000)); // Deferred native GPU submission.
    assert(!replay.SongEffect(p,s.camera,3,133333));
    assert(!replay.SongEffect(p,s.camera,2,150001));
    p[0][0][3]=10;assert(!replay.SongEffect(p,s.camera,1,100000));
    p[0]=s.native[0];s.key.renderer=2;s.model="link_v2";Pose::Publish(s);
    assert(!replay.SongEffect(p,s.camera,1,100000)); // Ambiguous owner stays native.
    std::cout<<"Song backdrops: original model names, native/FOV lens coverage, ownership and stale/ambiguous guards passed\n";
}
