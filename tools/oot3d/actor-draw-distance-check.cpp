// SPDX-License-Identifier: GPL-2.0-or-later
// Execute the original ARM visibility and group-distance checks, patched and
// unpatched, to verify register replay and the bounds that must stay native.
#include "oot3d/actor_draw_distance.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
using namespace OOT3D::ActorDrawDistance;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Actor=0x900000,Stack=0xe00000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu{};
    bool patched{};
    std::uint64_t ticks{};
    std::array<U32,2> stops{};
    template<class T> T Get(U32 a) {
        assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;
    }
    template<class T> void Put(U32 a,T v) {
        assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));
    }
    std::optional<U32> Read(U32 a) {if(a+4>bytes.size())return {};return Get<U32>(a);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==stops[0] || a==stops[1])return 0xef000000;
        return patched?Instruction(a,Get<U32>(a)):Get<U32>(a);
    }
    std::uint8_t MemoryRead8(U32 a) override {return Get<std::uint8_t>(a);}
    std::uint16_t MemoryRead16(U32 a) override {return Get<std::uint16_t>(a);}
    U32 MemoryRead32(U32 a) override {return Get<U32>(a);}
    std::uint64_t MemoryRead64(U32 a) override {return Get<std::uint64_t>(a);}
    void MemoryWrite8(U32 a,std::uint8_t v) override {Put(a,v);}
    void MemoryWrite16(U32 a,std::uint16_t v) override {Put(a,v);}
    void MemoryWrite32(U32 a,U32 v) override {Put(a,v);}
    void MemoryWrite64(U32 a,std::uint64_t v) override {Put(a,v);}
    void InterpreterFallback(U32 pc,std::size_t) override {
        std::cerr<<"fallback "<<std::hex<<pc<<'\n';std::abort();
    }
    void ExceptionRaised(U32 pc,Dynarmic::A32::Exception) override {
        std::cerr<<"exception "<<std::hex<<pc<<'\n';std::abort();
    }
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        if(!svc){cpu->HaltExecution();return;}
        assert(svc>=FirstSVC && svc<FirstSVC+Sites.size());
        const auto before=bytes;const auto regs=cpu->Regs();
        const auto flags=cpu->Cpsr(),fpscr=cpu->Fpscr();const auto fp=cpu->ExtRegs();
        Handle(*this,cpu->Regs(),cpu->ExtRegs(),svc-FirstSVC);
        assert(bytes==before && std::equal(regs.begin(),regs.end(),cpu->Regs().begin()));
        assert(flags==cpu->Cpsr() && fpscr==cpu->Fpscr());
        for(unsigned i=0;i<fp.size();++i)
            if(i!=Sites[svc-FirstSVC].fp_register)assert(fp[i]==cpu->ExtRegs()[i]);
    }
    void Setup(float x,float y,float z,unsigned category=6) {
        std::fill(bytes.begin()+Actor,bytes.begin()+Actor+0x300,0);
        Put<U32>(Actor,0x141|category<<16);
        Put<float>(Actor+0xec,x);Put<float>(Actor+0xf0,y);Put<float>(Actor+0xf4,z);
        Put<float>(Actor+0xf8,std::abs(z));Put<float>(Actor+0xfc,1000);
        Put<float>(Actor+0x100,100);Put<float>(Actor+0x104,100);Put<float>(Actor+0x1dc,1);
    }
    U32 Run(U32 pc,std::array<U32,2> end,bool patch,float factor) {
        patched=patch;stops=end;multiplier=factor;
        auto& r=cpu->Regs();for(unsigned i=0;i<r.size();++i)r[i]=0xcafe0000+i;
        r[0]=Actor;r[4]=r[5]=Actor;r[6]=0;r[8]=1;r[11]=2;r[13]=Stack;r[15]=pc;
        cpu->ExtRegs().fill(0);cpu->ExtRegs()[16]=std::bit_cast<U32>(1.f);
        cpu->SetCpsr(0x10);cpu->SetFpscr(0);cpu->ClearCache();cpu->ClearHalt();
        ticks=10000;cpu->Run();assert(ticks);
        assert(r[13]==Stack && (r[15]==stops[0]+4 || r[15]==stops[1]+4));
        return r[15]-4;
    }
    void SameAtOne(U32 start,std::array<U32,2> end) {
        const auto initial=bytes;Run(start,end,false,1);
        const auto native=bytes;const auto regs=cpu->Regs();const auto fp=cpu->ExtRegs();
        const auto flags=cpu->Cpsr(),fpscr=cpu->Fpscr();
        bytes=initial;Run(start,end,true,1);
        assert(bytes==native && regs==cpu->Regs() && fp==cpu->ExtRegs() &&
            flags==cpu->Cpsr() && fpscr==cpu->Fpscr());
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream in(argv[1],std::ios::binary);assert(in);
    in.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),0x600000);assert(in.gcount()>0);
    assert(CodeMatches(e));
    for(unsigned i=0;i<Sites.size();++i) {
        const auto& s=Sites[i];assert(Instruction(s.pc,s.word)==(0xef000000|(FirstSVC+i)));
        assert(Instruction(s.pc,s.word^1)==(s.word^1));
        e.Put(s.pc,s.word^1);assert(!CodeMatches(e));e.Put(s.pc,s.word);
    }
    assert(Clamp(NAN)==1 && Clamp(INFINITY)==1 && Clamp(-5)==1 && Clamp(50)==5);
    for(const auto word:{0u,0x80000000u,0x7fc00001u,0x7f800000u,0xbf800000u,0x7f7fffffu}) {
        assert(Scale(word,1)==word);assert(Scale(word,5)==word);
    }
    assert(std::bit_cast<float>(Scale(std::bit_cast<U32>(1000.f),2))==2000);
    assert(std::bit_cast<float>(Scale(std::bit_cast<U32>(4.f),3,true))==36);
    Dynarmic::A32::UserConfig config;config.callbacks=&e;Dynarmic::A32::Jit cpu(config);e.cpu=&cpu;
    constexpr std::array<U32,2> visibility_end{0x461b2c,0x461b2c};
    for(unsigned category=0;category<12;++category)for(float z:{500.f,1800.f,3000.f,-200.f}) {
        e.Setup(0,0,z,category);e.SameAtOne(0x4619f8,visibility_end);
        e.Setup(0,0,z,category);e.Run(0x4619f8,visibility_end,true,2);
        assert(bool(e.Get<U32>(Env::Actor+4)&64)==(z==500 || z==1800));
        assert(e.Get<float>(Env::Actor+0xfc)==1000); // Never accumulate the multiplier.
    }
    // Side, top, bottom and behind-camera rejection survive distance changes.
    for(const auto xyz:std::array<std::array<float,3>,4>{{{4000,0,1800},{0,4000,1800},
                                                         {0,-4000,1800},{0,0,-200}}}) {
        e.Setup(xyz[0],xyz[1],xyz[2]);e.Run(0x4619f8,visibility_end,true,5);
        assert(!(e.Get<U32>(Env::Actor+4)&64));
    }
    // The group uses 1600-unit spawn and 1705-unit despawn radii. Scale the
    // squared comparisons together; retain the gap so groups don't oscillate.
    constexpr std::array<U32,2> spawn_end{0x3b30f4,0x3b3204},despawn_end{0x3af008,0x3af0a8};
    for(unsigned kind=0;kind<3;++kind)for(float z:{1000.f,2500.f,3300.f,3500.f}) {
        e.Setup(0,0,z);e.Put<std::uint16_t>(Env::Actor+0x1c,kind);
        e.SameAtOne(0x3b30bc,spawn_end);e.SameAtOne(0x3aefd0,despawn_end);
        assert(e.Run(0x3b30bc,spawn_end,true,2)==(z<3200?spawn_end[0]:spawn_end[1]));
        assert(e.Run(0x3aefd0,despawn_end,true,2)==(z<3410?despawn_end[1]:despawn_end[0]));
        assert(e.Get<float>(Env::Actor+0x1dc)==1);
    }
    // Run Fishing's real visibility branch: reeds (1200) and lily pads (800).
    // Both fade paths and the near interaction threshold remain native.
    constexpr std::array<U32,2> fishing_end{0x3b5f48,0x3b6004};
    for(float radius:{800.f,1200.f})for(float factor:{1.f,2.f,5.f})
        for(float z:{500.f,800.f,1000.f,1600.f,2400.f,3999.f,6000.f})
            for(float x:{0.f,7000.f}) {
                e.Setup(0,0,0);e.Put<float>(Env::Actor+0x18,x);
                e.Put<float>(Env::Actor+0x20,z);e.Put<float>(Env::Actor+0x38,radius);
                e.Put<float>(Env::Actor+0x3c,.6f);
                e.SameAtOne(0x3b5ea4,fishing_end);
                const bool visible=z<radius*factor && std::abs(x)<z+100;
                assert(e.Run(0x3b5ea4,fishing_end,true,factor)==fishing_end[visible?0:1]);
                assert(e.Get<float>(Env::Actor+0x38)==radius);
                assert(e.Get<float>(Env::Actor+0x3c)==.6f);
            }
    std::cout<<"Actor draw distance: native ARM 1x parity, all 12 categories, extended visibility, preserved frustum rejection, grass/rock hysteresis, fishing foliage limits with untouched opacity, no hook memory writes, register preservation and code guards passed\n";
}
