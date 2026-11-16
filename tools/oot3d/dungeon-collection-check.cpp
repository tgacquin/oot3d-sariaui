// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/dungeon_collection.h"
#include "oot3d/file_particles.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace DungeonCollection;
using U32=std::uint32_t;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {
        const auto it=words.find(at);
        return it==words.end()?std::nullopt:std::optional{it->second};
    }
    void Byte(U32 at,unsigned v) {
        auto& word=words[at&~3u];const auto shift=(at&3)*8;
        word=(word&~(255u<<shift))|((v&255)<<shift);
    }
};
constexpr U32 Play=0x10000000;
constexpr std::array<unsigned,12> Scenes{0,1,2,3,4,5,6,7,8,9,11,13};
void Init(Memory& m,unsigned dungeon,bool mq,unsigned scene) {
    m.words.clear();
    for(U32 at=Save;at<Save+0x1000;at+=4)m.words[at]=0;
    m.words[Play+0x104]=scene;m.words[Play+0x2238]=0;m.words[Play+0x2244]=0;
    m.Byte(Save+0xe,mq);
}
void Collect(Memory& m,const Mask& mask,unsigned scene,U32 bits) {
    if(mask.kind==Kind::Skulltula) {m.Byte(Save+0xeb4+mask.scene,bits);return;}
    const bool chest=mask.kind==Kind::Chest;
    const auto at=mask.scene==scene?Play+(chest?0x2238:0x2244):Save+0xec+mask.scene*0x1c+(chest?0:0xc);
    m.words[at]=bits;
}
bool Near(float a,float b) {return std::abs(a-b)<.0001f;}
int main() {
    Memory m;
    // Every normal/MQ dungeon, including outdoor Spirit chests and boss rooms.
    constexpr std::array<unsigned,24> Totals{10,11,11,12,7,15,18,17,19,17,15,11,24,27,22,25,17,8,6,6,22,17,16,14};
    for(unsigned d=0;d<12;++d)for(bool mq:{false,true}) {
        Init(m,d,mq,Scenes[d]);
        auto p=Read(m,Play);assert(p && p->dungeon==d && p->mq==mq);
        assert(p->collected==0 && p->total==Totals[d*2+mq] && p->Fraction()==0);
        for(const auto& mask:Locations[d*2+mq])if(mask.bits)Collect(m,mask,Scenes[d],mask.bits);
        p=Read(m,Play);assert(p && p->collected==p->total && p->Fraction()==1);
        // A spent small key doesn't undo the location collection flags.
        m.Byte(Save+0xd4+Scenes[d],0);
        assert(Read(m,Play)->Fraction()==1);
    }
    Init(m,5,false,5);
    // Unknown scene bits must not inflate progress. Active chest flags win
    // over stale saved ones; map/compass ownership isn't counted twice.
    m.words[Save+0xec+5*0x1c]=~0u;
    m.words[Play+0x2238]=1u<<31;
    m.Byte(Save+0xc0+5,7);
    assert(Read(m,Play)->collected==0);
    m.words[Play+0x2238]|=1;
    assert(Read(m,Play)->collected==1);
    m.words.erase(Play+0x2238);assert(!Read(m,Play));
    assert(!Read(m,0));
    Init(m,0,false,0x55);assert(!Read(m,Play));
    // Boss-room scene uses live flags there and saved dungeon flags elsewhere.
    Init(m,1,false,18);
    for(const auto& mask:Locations[2])if(mask.bits)Collect(m,mask,18,mask.bits);
    assert(Read(m,Play)->dungeon==1 && Read(m,Play)->Fraction()==1);
    assert(Density(0)==32 && Density(.5f)==112 && Density(1)==192);
    assert(DriftSpeed(0)==1 && DriftSpeed(.5f)==1.5f && DriftSpeed(1)==2);
    FileParticles::Progression custom{.density_start=12,.density_end=80,.drift_start=.5f,.drift_end=4};
    assert(Density(0,custom)==12 && Density(.5f,custom)==46 && Density(1,custom)==80);
    assert(DriftSpeed(0,custom)==.5f && DriftSpeed(.5f,custom)==2.25f && DriftSpeed(1,custom)==4);
    custom={.density_start=192,.density_end=0,.drift_start=6,.drift_end=0};
    assert(Density(.5f,custom)==96 && DriftSpeed(.5f,custom)==3);
    custom={.density_start=-10,.density_end=999,.drift_start=-1,.drift_end=99};
    assert(Density(0,custom)==0 && Density(1,custom)==192);
    assert(DriftSpeed(0,custom)==0 && DriftSpeed(1,custom)==6);
    Transition t;const Transition::Clock::time_point start{};
    Progress p{.dungeon=5,.collected=0,.total=10};
    assert(t.Update(p,start)==0);p.collected=10;
    const float blend=t.Update(p,start+std::chrono::milliseconds(16));
    assert(blend>0 && blend<1);
    for(unsigned frame=2;frame<300;++frame)t.Update(p,start+std::chrono::milliseconds(16*frame));
    assert(t.fraction==1);
    p.dungeon=4;p.collected=5;
    assert(t.Update(p,start+std::chrono::seconds(5))==.5f);
    using namespace FileParticles;
    Settings s;s.density=192;s.speed=3;
    Animation a;
    a.Frame<DungeonMaxParticles>(s,400,start,192,6);
    for(unsigned frame=1;frame<=120;++frame)
        a.Frame<DungeonMaxParticles>(s,400,start+std::chrono::nanoseconds(std::int64_t(1e9*frame/120)),192,6);
    assert(std::abs(a.seconds-6)<.00001 && std::abs(a.rotation_seconds-1)<.00001);
    s={};s.glow=0;s.light_strength=0;
    const auto sum=[](const auto& g){float total=0;for(unsigned i=0;i<g.count*4;++i)total+=g.colors[i*4+3];return total;};
    bool checked=false;
    for(double seconds=0;seconds<60 && !checked;seconds+=.1) {
        const auto base=Draw<DungeonMaxParticles>(s,seconds,1,400,0,32);
        const auto full=Draw<DungeonMaxParticles>(s,seconds,1,400,0,33);
        if(sum(full)-sum(base)<.01f)continue;
        const auto half=Draw<DungeonMaxParticles>(s,seconds,1,400,0,32.5f);
        assert(Near(sum(half),std::lerp(sum(base),sum(full),.5f)));checked=true;
    }
    assert(checked);
    std::cout<<"24 dungeon layouts, live/saved flags, spent keys, missing memory, progress smoothing, 32-192 density and doubled drift passed\n";
}
