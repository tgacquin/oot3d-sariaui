// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/camera_transition_diagnostics.h"
#include <cassert>
#include <iostream>
#include <map>
namespace C=OOT3D::CameraTransitionDiagnostics;
using C::U32;
struct ReadOnlyMemory {
    std::map<U32,U32> words;
    unsigned reads=0;
    std::optional<U32> Read(U32 a) {
        ++reads;const auto i=words.find(a);
        return i==words.end()?std::nullopt:std::optional{i->second};
    }
};
int main() {
    ReadOnlyMemory m;C::Trace trace;OOT3D::OrbitCamera::Hook orbit;
    constexpr U32 play=0x08000000,cam=play+0x364,player=0x09000000,sub=play+0x600;
    const auto put=[&](U32 at,std::array<float,3> values) {
        for(unsigned i=0;i<3;++i)m.words[at+4*i]=std::bit_cast<U32>(values[i]);
    };
    m.words[0x5043e0]=play;
    for(U32 camera:{cam,sub}) {
        m.words[camera+0xd4]=play;m.words[camera+0xd8]=player;
        put(camera+0x80,{0,38,0});put(camera+0x8c,{0,38,-150});
    }
    put(player+0x28,{0,0,0});m.words[play+0x104]=7;
    std::vector<std::string> lines;
    const auto observe=[&](C::Stage stage,U32 camera,std::int64_t us,std::int64_t wall) {
        return trace.Observe(m,stage,camera,12,us,orbit,0,0,0,0,
            [&](const std::string& line){lines.push_back(line);},wall);
    };
    C::Stop();assert(!observe(C::Begin,cam,0,1000) && m.reads==0);
    C::Arm(1000);assert(C::Pending(1000) && !C::Recording(3999));
    assert(!observe(C::Begin,cam,0,3999) && m.reads==0);
    const auto before=m.words;
    assert(observe(C::Begin,cam,0,4000));
    assert(!observe(C::Begin,cam,0,4001)); // Replayed render is not another sample.
    assert(observe(C::Finish,cam,0,4001));
    assert(observe(C::Begin,sub,0,4001)); // Native door subcamera is captured too.
    assert(lines.back().find("main=0")!=std::string::npos);
    assert(lines.back().find("radius=150")!=std::string::npos);
    assert(m.words==before); // No guest writes exist in this adapter.
    m.words[play+0x104]=8;
    assert(observe(C::Begin,cam,33333,4033));
    assert(lines.back().find("scene=8")!=std::string::npos);
    const auto reads=m.reads;
    assert(!observe(C::Begin,cam,66666,34000) && m.reads==reads);
    C::Arm(40000);assert(observe(C::Begin,cam,33333,43000));
    assert(C::samples==1 && trace.total==1); // Rearming resets dedup and quota.
    trace.total=12000;assert(!observe(C::Begin,cam,66666,43033));
    C::Stop();assert(!C::Pending(43034));
    std::cout<<"Entrance capture: read-only, countdown, replay dedup, subcamera, scene change, expiry and quota passed\n";
}
