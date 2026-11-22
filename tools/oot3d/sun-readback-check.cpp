// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/sun_readback_hooks.h"
#include "oot3d/interpolation_rate.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace OOT3D::SunReadbackProbe;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Stop=0xf00000, SP=0xe00000, Depth=0xc00000, DepthEnd=Depth+400*480*4;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu{};
    Hook hook;
    bool hooked{}, forbid_depth{}, optimized{};
    I64 now{};
    unsigned loads{}, tails{};
    std::uint64_t ticks{};
    template<class T> T Get(U32 a) {assert(std::uint64_t(a)+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof v);return v;}
    template<class T> void Put(U32 a,T v) {assert(std::uint64_t(a)+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof v);}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    U32 ReadDepth(U32 a) {assert(sun_query);return MemoryRead32(a);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop)return 0xef000000;
        if(a==0x47c944)return 0xef000001;
        const auto w=Get<U32>(a);
        return hooked && CodeMatches(*this)?Instruction(a,w):w;
    }
    std::uint8_t MemoryRead8(U32 a) override {return Get<std::uint8_t>(a);}
    std::uint16_t MemoryRead16(U32 a) override {return Get<std::uint16_t>(a);}
    U32 MemoryRead32(U32 a) override {
        if(a >= 0x1f000000 && a < 0x1f000000 + DepthEnd - Depth) a = a - 0x1f000000 + Depth;
        if(a>=Depth && a<DepthEnd) {assert(!forbid_depth);++loads;}
        return Get<U32>(a);
    }
    std::uint64_t MemoryRead64(U32 a) override {return Get<std::uint64_t>(a);}
    void MemoryWrite8(U32 a,std::uint8_t v) override {Put(a,v);}
    void MemoryWrite16(U32 a,std::uint16_t v) override {Put(a,v);}
    void MemoryWrite32(U32 a,U32 v) override {Put(a,v);}
    void MemoryWrite64(U32 a,std::uint64_t v) override {Put(a,v);}
    void InterpreterFallback(U32,std::size_t) override {std::abort();}
    void ExceptionRaised(U32,Dynarmic::A32::Exception) override {std::abort();}
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();
        if(!svc) {cpu->HaltExecution();return;}
        if(svc==1) {++tails;assert(r[0]==0x812345);r[15]=r[14];return;}
        assert(svc>=FirstSVC && svc<FirstSVC+2);
        hook.Handle(*this,r,svc-FirstSVC,now,optimized,42,7);
    }
    void Run() {
        cpu->Reset();cpu->ClearCache();cpu->ClearHalt();cpu->SetCpsr(0xa0000010);
        auto& r=cpu->Regs();for(unsigned i=0;i<13;++i)r[i]=0x100+i;
        r[1]=0x812345;r[13]=SP;r[14]=Stop;r[15]=Entry;
        for(unsigned i=0;i<cpu->ExtRegs().size();++i)cpu->ExtRegs()[i]=0x3f800000+i;
        loads=tails=0;ticks=100000;cpu->Run();assert(ticks);
        assert(tails==1 && r[13]==SP && r[14]==Stop && r[15]==Stop+4);
        for(unsigned i=4;i<12;++i)assert(r[i]==0x100+i);
        for(unsigned i=0;i<cpu->ExtRegs().size();++i)assert(cpu->ExtRegs()[i]==0x3f800000+i);
    }
};
struct CachedDepth : OOT3D::SunVisibility::Backend {
    Env& env;
    bool pending{}, unsupported{};
    unsigned calls{}, resets{};
    explicit CachedDepth(Env& e) : env(e) {}
    std::optional<OOT3D::SunVisibility::Sample> ReadSunDepth(U32 address,
        const OOT3D::SunVisibility::Context& context) override {
        assert(context.frame==42 && context.generation==7 && context.owner==0xb00000 && context.scene==0x55);
        assert(address>=0x18000000 && address<0x18000000+Env::DepthEnd-Env::Depth);
        ++calls;
        if(unsupported)return {};
        return OOT3D::SunVisibility::Sample{
            pending?0:env.Get<U32>(Env::Depth+address-0x18000000),pending};
    }
    void ResetSunDepth() override {++resets;}
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);assert(file);
    const auto size=file.tellg();file.seekg(0);file.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);
    assert(file && CodeMatches(e));
    // Every word is checked, not just a prologue that another revision might share.
    for(U32 a=Entry;a<0x4592d0;a+=4) {
        const U32 word=*e.Read(a);e.Write(a,word^1);assert(!CodeMatches(e));e.Write(a,word);
    }
    assert(Instruction(Entry,EntryWord^1)==(EntryWord^1));
    assert(Instruction(Load,LoadWord^1)==(LoadWord^1));
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    // Run the whole native callback, including its real tiling table/memcpy,
    // in both framebuffer layouts and at corners/outside the screen.
    for(bool stereo:{false,true})for(int x:{-20,0,200,399,420})
        for(int y:{-20,0,120,239,260})for(bool clear:{false,true}) {
        e.Write(0x55a21c,1);e.Put<std::uint8_t>(0x5be5b8+0x75,stereo?0:1);
        e.Put<std::int16_t>(0x531ec8,x);e.Put<std::int16_t>(0x531eca,y);
        e.Write(0x531ed4,Env::Depth);
        for(U32 p=Env::Depth;p<Env::DepthEnd;p+=4)e.Write(p,clear?0xffffffffu:0xff000001u);
        current.store({});e.hooked=false;e.forbid_depth=false;e.Run();
        const U32 result=*e.Read(0x531ed8);const unsigned reads=e.loads;
        const auto registers=cpu.Regs();const auto flags=cpu.Cpsr();
        auto session=Start(0);e.hooked=true;
        for(int phase:{-1,0,1,2,3}) {
            e.now=phase<0?0:session->begins+phase*Duration;
            e.forbid_depth=phase==1;e.Run();
            assert(*e.Read(0x531ed8)==(phase==1?0:result));
            assert(e.loads==(phase==1?0:reads));
            if(phase!=1) {assert(cpu.Regs()==registers);assert(cpu.Cpsr()==flags);}
            assert(!sun_query);
        }
        assert(session->phases[1].queries==1 && session->phases[1].bypasses==1 && session->phases[1].reads==0);
        assert(session->phases[0].reads==reads && session->phases[2].reads==reads);
        session->cancelled=true;e.now=session->begins+Duration;e.forbid_depth=false;e.Run();
        assert(e.loads==reads && *e.Read(0x531ed8)==result);
    }
    // Reporting cannot label an uninstalled or unexecuted bypass as verified.
    auto session=Start(0);installed=3;
    assert(Finish(session->begins).empty());
    assert(Finish(session->begins+3*Duration).find("bypass_verified=0")!=std::string::npos);
    session=Start(0);session->phases[1].queries=1;session->phases[1].bypasses=1;
    assert(Finish(session->begins+3*Duration).find("bypass_verified=1")!=std::string::npos);
    assert(Finish(session->begins+3*Duration).empty());
    // Each diagnostic phase is reported once without adjusting the FPS target.
    session=Start(0);
    for(int phase=0;phase<3;++phase) {
        const auto start=session->begins+phase*Duration;
        assert(BeginPhase(start)==phase);
        assert(BeginPhase(start)==-1);
    }
    assert(BeginPhase(session->begins+3*Duration)==-1);
    // A GPU read from the last native query can cross into the bypass interval.
    // Keep it assigned to its original query instead of falsely failing bypass.
    session=Start(0);
    {
        ReadScope origin{Ticket{session,0}};
        {
            ReadScope nested{Ticket{session,2}};
            DownloadScope download{true,session->begins+Duration};
        }
        DownloadScope download{true,session->begins+Duration};
    }
    assert(session->phases[0].sun_downloads==1 && session->phases[1].sun_downloads==0);
    assert(session->phases[2].sun_downloads==1 && !sun_query);
    // Execute the unchanged native visibility logic with asynchronous samples.
    // All nine taps must produce the same result and registers as native reads;
    // pending snapshots must never touch guest depth, even on the cold start.
    current.store({});
    CachedDepth cache(e);OOT3D::SunVisibility::backend=&cache;
    e.Write(0x5043e0,0xb00000);e.Write(0xb00104,0x55);
    e.Write(0xb00188,0);e.Write(0xb0018c,400);e.Write(0xb00190,0);e.Write(0xb00194,240);
    e.Write(0x531ed4,0x1f000000);
    for(bool stereo:{false,true})for(int x:{-20,0,200,399,420})
        for(int y:{-20,0,120,239,260})for(bool clear:{false,true}) {
        e.Put<std::uint8_t>(0x5be5b8+0x75,stereo?0:1);
        e.Put<std::int16_t>(0x531ec8,x);e.Put<std::int16_t>(0x531eca,y);
        for(U32 p=Env::Depth;p<Env::DepthEnd;p+=4)e.Write(p,clear?0xffffffffu:0xff000001u);
        e.optimized=false;e.forbid_depth=false;e.Run();
        const auto result=*e.Read(0x531ed8);const auto registers=cpu.Regs();const auto flags=cpu.Cpsr();
        const auto reads=e.loads;
        cache.calls=0;cache.pending=false;e.optimized=true;e.forbid_depth=true;e.Run();
        assert(!e.loads && cache.calls==reads && *e.Read(0x531ed8)==result);
        assert(cpu.Regs()==registers && cpu.Cpsr()==flags);
        cache.pending=true;e.Run();assert(!e.loads && *e.Read(0x531ed8)==0);
        cache.pending=false;cache.unsupported=true;e.forbid_depth=false;e.Run();
        assert(e.loads==reads && *e.Read(0x531ed8)==result);
        cache.unsupported=false;
    }
    // Invalid live owner must disable the backend, never reuse another scene.
    const auto calls=cache.calls;
    e.Write(0x5043e0,0);e.Run();assert(cache.calls==calls && !e.hook.reader.active);
    OOT3D::SunVisibility::backend=nullptr;
    assert(!MM3D::VisibilityDepth::Physical(0xffffffff,1));
    std::cout<<"Async sun hook: real native routine equivalence, both layouts/bounds, pending without guest depth reads, unsupported fallback and invalid owner passed\n";
    std::cout<<"Sun readback: full native routine equivalence, both layouts, bounds, no depth access during bypass, tail ABI, expiry/cancel restoration and reporting checks passed\n";
    std::cout<<"Comparison: phase reporting without FPS changes and boundary attribution passed\n";
}
