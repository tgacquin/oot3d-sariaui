// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/heart_pulse.h"
#include "common/vector_math.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::HeartPulse;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {auto i=words.find(at);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Write(U32 at,U32 value) {assert(words.contains(at));words[at]=value;}
};
struct Vertex {Common::Vec4f position,color;Common::Vec2f tex_coord0;};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;
    std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
    for(U32 at=0x100000;file.read(reinterpret_cast<char*>(&word),4);at+=4)m.words[at]=word;
    assert(CodeMatches(m));assert(Instruction(Site.pc,Site.instruction)==(0xef000000|FirstSVC));
    Clock clock;assert(clock.Observe(1,0,100000)==1);
    for(unsigned tick=1;tick<=34;++tick) {
        const auto value=clock.Observe(1,tick%17,100000+tick*33333);
        assert(std::abs(value-Expansion(std::fmod(tick*.5f,17.f)))<.00001f);
        assert(clock.Observe(1,tick%17,100000+tick*33333)==value); // No double advance.
    }
    assert(clock.phase==0); // One pulse in twice the native 17-tick period.
    assert(clock.Observe(2,4,1300000)==Expansion(4)); // Scene/reset cuts history.
    constexpr U32 rect=0x8000000;
    m.words[0x50af34]=0x8100000;m.words[0x50af34+0x58]=6;m.words[0x587958+0x44]=48;
    std::array<float,12> native{78,-2,0,98,-2,0,78,18,0,98,18,0};
    for(unsigned i=0;i<12;++i)m.words[rect+i*4]=std::bit_cast<U32>(native[i]);
    auto before=m.words;Hook hook;std::array<U32,16> r{};r[0]=rect;r[4]=4;r[7]=0x50af34;
    hook.Handle(m,r,100000);assert(r[15]==0x2e70d4 && r[14]==Site.pc+4);
    assert(std::bit_cast<float>(*m.Read(rect))==80.f);
    assert(std::bit_cast<float>(*m.Read(rect+12))==96.f);
    // The native routine rebuilds the rectangle before each invocation.
    for(unsigned i=0;i<12;++i)m.words[rect+i*4]=std::bit_cast<U32>(native[i]);
    m.words[0x50af34+0x58]=7;r[0]=rect;hook.Handle(m,r,133333);
    assert(std::bit_cast<float>(*m.Read(rect))==80.125f);
    assert(std::bit_cast<float>(*m.Read(rect+12))==95.875f);
    for(const auto& [at,value]:before)if(at<rect || at>=rect+48)
        if(at!=0x50af34+0x58)assert(m.Read(at)==value);
    m.words[0x587958+0x44]=0;before=m.words;r[0]=rect;hook.Handle(m,r,166666);assert(m.words==before);
    std::array<Vertex,120> hearts{};
    auto set=[&](float radius) {
        for(unsigned q=0;q<20;++q)for(unsigned i=0;i<6;++i) {
            const unsigned v=std::array{0,2,1,1,2,3}[i];
            hearts[q*6+i].position={q*.04f+(v%2?radius:-radius),v>=2?radius:-radius,0,1};
            hearts[q*6+i].color={1,1,1,1};hearts[q*6+i].tex_coord0={float(v%2),float(v/2)};
        }
    };
    History<Vertex> history;set(.01f);assert(!history.Apply(hearts,1,33333,0));
    set(.012f);assert(history.Apply(hearts,2,66666,0));assert(std::abs(hearts[0].position[0]+.011f)<1e-6);
    set(.014f);hearts[0].tex_coord0[0]=.5f;
    history.Apply(hearts,3,99999,0);assert(hearts[0].position[0]==-.014f); // Health changes stay instant.
    set(.016f);for(auto& v:hearts)v.position[0]+=.1f;
    assert(!history.Apply(hearts,4,133332,0)); // HUD editor motion stays instant.
    set(.018f);assert(!history.Apply(hearts,6,199998,0)); // Missing frames.
    set(.02f);assert(!history.Apply(hearts,7,233331,1)); // Interpolation reset.
    std::cout<<"Heart half-speed period, native isolation, smooth midpoint and health/layout/reset guards passed\n";
}
