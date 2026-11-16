// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include <span>

namespace OOT3D::HeartPulse {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff210;
constexpr TitleScreen::Site Site{0x4444dc,0xebfa8afc,0xe3500001};
inline U32 Instruction(U32 pc,U32 word) {
    return pc==Site.pc && word==Site.instruction?0xef000000|FirstSVC:word;
}
template<class M> bool CodeMatches(M& m) {
    return m.Read(Site.pc)==Site.instruction && m.Read(Site.pc+4)==Site.following &&
        m.Read(0x444540)==0x50af34 && m.Read(0x44451c)==std::bit_cast<U32>(12.f);
}
// Native expansion keys, smoothed between their two-tick steps. The final
// three ticks are the original resting portion of the 17-tick heartbeat.
inline float Expansion(float phase) {
    constexpr std::array<float,10> keys{1,2,3,4,3,2,1,0,0,1};
    phase=std::clamp(phase,0.f,17.f);
    const unsigned key=std::min(unsigned(phase/2),8u);
    const float t=(phase-key*2)/(key==8?1.f:2.f);
    return keys[key]+(keys[key+1]-keys[key])*t;
}
struct Clock {
    U32 owner=0,tick=0;
    std::int64_t time=0;
    float phase=0;
    bool seen=false;
    float Observe(U32 next_owner,U32 next_tick,std::int64_t now) {
        const U32 delta=(next_tick+17-tick)%17;
        if(!seen || owner!=next_owner || now<time || now-time>100000 || delta>1)
            phase=float(next_tick);
        else phase=std::fmod(phase+delta*.5f,17.f);
        owner=next_owner;tick=next_tick;time=now;seen=true;
        return Expansion(phase);
    }
};
struct Hook {
    Clock clock;
    template<class M,class R> void Handle(M& m,R& r,std::int64_t now) {
        const U32 rect=r[0];
        r[14]=Site.pc+4;r[15]=0x2e70d4; // Original pause query/counter update.
        const auto tick=m.Read(0x50af34+0x58),health=m.Read(0x587958+0x44);
        if(!tick || *tick>16 || !health || r[7]!=0x50af34 || r[4]>4)return;
        // Reduce only the expansion; retain the resting heart size and timing.
        const float pulse=.5f*clock.Observe(m.Read(0x50af34).value_or(0),*tick,now);
        if((*health&0xffff)==0)return;
        std::array<float,12> v;
        if(!TitleScreen::ReadFloats(m,rect,v))return;
        const float native=float(r[4]);
        if(v[3]-v[0]!=12+2*native || v[7]-v[1]!=12+2*native)return;
        // This is only the active heart's freshly built rectangle. Never
        // change health, texture selection, the sound clock or saved layout.
        const float change=pulse-native;
        for(unsigned i=0;i<4;++i) {
            v[i*3]+=(i%2?change:-change);
            v[i*3+1]+=(i>=2?change:-change);
        }
        for(unsigned i=0;i<12;++i)m.Write(rect+i*4,std::bit_cast<U32>(v[i]));
    }
};
// Only blend pulse size, never a health/texture change or an editor move.
// The caller supplies the twenty verified heart quads after native projection.
template<class Vertex> struct History {
    std::array<Vertex,120> previous{};
    std::uint64_t frame=0;
    std::int64_t time=0;
    unsigned epoch=0;
    bool seen=false;
    void Reset() {seen=false;}
    bool Apply(std::span<Vertex> hearts,std::uint64_t next,std::int64_t now,unsigned generation) {
        if(hearts.size()!=120) {Reset();return false;}
        std::array<Vertex,120> current;
        std::copy(hearts.begin(),hearts.end(),current.begin());
        const bool adjacent=seen && next==frame+1 && now-time>=20000 &&
            now-time<=50000 && generation==epoch;
        bool changed=false;
        if(adjacent)for(unsigned q=0;q<20;++q) {
            const unsigned at=q*6;
            bool same=true;
            // Native triangle order: TL, BL, TR, TR, BL, BR.
            for(unsigned axis=0;axis<4;++axis) {
                const float old_center=(previous[at].position[axis]+previous[at+5].position[axis])*.5f;
                const float center=(current[at].position[axis]+current[at+5].position[axis])*.5f;
                same &= std::isfinite(center) && std::isfinite(old_center) && std::abs(center-old_center)<.00001f;
            }
            for(unsigned j=0;j<6;++j) {
                const auto& a=previous[at+j];const auto& b=current[at+j];
                same &= a.tex_coord0==b.tex_coord0 && a.color==b.color &&
                    a.position[2]==b.position[2] && a.position[3]==b.position[3];
                for(unsigned axis=0;axis<2;++axis)
                    same &= std::isfinite(a.position[axis]) && std::isfinite(b.position[axis]) &&
                        std::abs(a.position[axis]-b.position[axis])<.025f;
            }
            if(same)for(unsigned j=0;j<6;++j) {
                changed |= previous[at+j].position!=current[at+j].position;
                hearts[at+j].position=(previous[at+j].position+current[at+j].position)*.5f;
            }
        }
        previous=std::move(current);frame=next;time=now;epoch=generation;seen=true;
        return changed;
    }
};
}
