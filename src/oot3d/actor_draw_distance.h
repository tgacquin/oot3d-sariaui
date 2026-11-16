// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>

namespace OOT3D::ActorDrawDistance {
using U32=std::uint32_t;
constexpr float Minimum=1.f,Maximum=5.f,Default=1.f;
inline std::atomic<float> multiplier{Default};
inline std::atomic<bool> dirty{false};
constexpr U32 FirstSVC=0xfff600;
struct Site {U32 pc,word,actor_register,offset;bool squared;unsigned fp_register=2;};
constexpr std::array<Site,4> Sites{{
    // Actor_DrawAll's inlined visibility test: S2 = actor->uncullZoneForward.
    // All 12 categories pass here, including NPCs, signs, gates and bushes.
    {0x4619f8,0xed941a3f,4,0xfc,false},
    // Obj_Mure2 spawns/despawns grass and rock groups using XZ distance squared.
    // Scale only the loaded comparison factor, retaining native hysteresis.
    {0x3b30d4,0xed951a77,5,0x1dc,true},
    {0x3aefe8,0xed951a77,5,0x1dc,true},
    // Fishing's pooled reeds/lily pads have their own forward distance and
    // opacity ramp. Extend its comparison, leaving fade and interaction alone.
    {0x3b5ea8,0xedd40a0e,4,0x38,false,1},
}};
// Verified against the USA executable. Do not hook camera placement/targeting
// visibility tests: those use similar code but are not rendering decisions.
constexpr std::array<U32,44> Visibility{
    0xed941a3f,0xed940a40,0xedd40a3e,0xe5d41120,0xee711a00,0xed941a3d,
    0xeef41ac1,0xeef1fa10,0x95c4b120,0x9a00001f,0xeef11a40,0xeef41ac1,
    0xeef1fa10,0x2a00001a,0xee100a90,0xe35005fe,0xbef00a48,0xba000000,
    0xeec80a20,0xed941a3b,0xeeb01ac1,0xee311a40,0xee211a20,0xee110a10,
    0xe35005fe,0xaa00000e,0xed941a3c,0xedd41a41,0xe59f2274,0xee711a21,
    0xee611aa0,0xee110a90,0xe1500002,0x2a000006,0xee310a40,0xee200a20,
    0xee100a10,0xe35005fe,0xb1a00008,0xb5c46120,0xba000001,0xe5c48120,
    0xe1a00006,0xe3500000};
constexpr std::array<U32,14> Spawn{
    0xedd00a3b,0xed900a3d,0xe1d001fc,0xee600aa0,0xe59f2138,0xe2001003,
    0xed951a77,0xe0821101,0xee400a00,0xed910a00,0xee200a01,0xeef40ac0,
    0xeef1fa10,0x2a000043};
constexpr std::array<U32,14> Despawn{
    0xedd50a3b,0xe1d501bc,0xe59f10d0,0xee600aa0,0xed950a3d,0xe2000003,
    0xed951a77,0xe0811100,0xee400a00,0xed910a00,0xee200a01,0xeef40ac0,
    0xeef1fa10,0xba000027};
constexpr std::array<U32,12> FishingVisibility{
    0xed940a08,0xedd40a0e,0xeeb40ae0,0xeef1fa10,0x2a000052,0xedd40a06,
    0xed9f1a1f,0xeef00ae0,0xee301a01,0xeef40ac1,0xeef1fa10,0xea00001b};
template<class M,std::size_t N> bool WordsMatch(M& m,U32 at,const std::array<U32,N>& words) {
    for(unsigned i=0;i<N;++i)if(m.Read(at+4*i)!=words[i])return false;
    return true;
}
template<class M> bool CodeMatches(M& m) {
    return WordsMatch(m,0x4619f8,Visibility) && WordsMatch(m,0x3b30bc,Spawn) &&
        WordsMatch(m,0x3aefd0,Despawn) && m.Read(0x461bc4)==0xe350000c &&
        m.Read(0x3b320c)==0x535544 && m.Read(0x3af0b0)==0x535550 &&
        WordsMatch(m,0x3b5ea4,FishingVisibility) && m.Read(0x3b5f14)==0x59df84 &&
        m.Read(0x3b5f40)==0x42c80000;
}
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].word)return 0xef000000|(FirstSVC+i);
    return word;
}
inline float Clamp(float value) {
    return std::isfinite(value)?std::clamp(value,Minimum,Maximum):Default;
}
inline U32 Scale(U32 word,float factor,bool squared=false) {
    factor=Clamp(factor);
    if(factor==Default)return word; // Exact native bits, including signed zero.
    const float value=std::bit_cast<float>(word);
    if(!std::isfinite(value) || value<=0)return word;
    if(squared)factor*=factor;
    const float extended=value*factor;
    return std::isfinite(extended)?std::bit_cast<U32>(extended):word;
}
template<class M,class R,class F> void Handle(M& m,R& r,F& f,unsigned index) {
    if(index>=Sites.size())return;
    const auto& site=Sites[index];
    // Replay the displaced VLDR. No actor fields, matrices, room rules,
    // clipping planes, fade counters or persistent guest allocations change.
    const auto original=m.Read(r[site.actor_register]+site.offset);
    if(original)f[site.fp_register]=Scale(*original,multiplier.load(),site.squared);
}
}
