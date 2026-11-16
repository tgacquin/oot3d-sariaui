// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the game's ARM heap sizing and allocator with the recorded failure
// workload. No emulator window, user configuration or save is opened.
#include "oot3d/randomizer/resource_diagnostics.h"
#include "oot3d/graphics_memory.h"
#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/exclusive_monitor.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using U32=std::uint32_t;
namespace D=OOT3D::Randomizer::ResourceDiagnostics;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Stop=0x700000, Stack=0x7ff000, ArenaBase=0x800000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x5000000);
    Dynarmic::A32::Jit* cpu{};
    U32 allowance=0, committed=0x904000, stop_at=Stop;
    std::uint64_t ticks=0;
    bool graphics_expanded=false;
    template<class T> T Get(U32 at) {
        assert(std::uint64_t(at)+sizeof(T)<=bytes.size());
        T value;std::memcpy(&value,bytes.data()+at,sizeof value);return value;
    }
    template<class T> void Put(U32 at,T value) {
        assert(std::uint64_t(at)+sizeof(T)<=bytes.size());
        std::memcpy(bytes.data()+at,&value,sizeof value);
    }
    std::optional<U32> Read(U32 at) {
        if(std::uint64_t(at)+4>bytes.size())return {};
        return Get<U32>(at);
    }
    std::optional<U32> MemoryReadCode(U32 at) override {
        if(at==Stop || at==stop_at)return 0xef000000;
        if(at==0x416554)return 0xef000001; // Kernel's reported application allowance.
        if(at==0x416564)return 0xef000002; // Process's current committed bytes.
        if(at==0x313ce0)return 0xef000003; // Small mutex storage, outside SystemArena.
        if(at==0x2ff560 || at==0x2ff508)return 0xef000004; // Single-threaded test locks.
        const auto word=Get<U32>(at);
        return graphics_expanded?OOT3D::GraphicsMemory::Instruction(at,word):word;
    }
    std::uint8_t MemoryRead8(U32 at) override {return Get<std::uint8_t>(at);}
    std::uint16_t MemoryRead16(U32 at) override {return Get<std::uint16_t>(at);}
    U32 MemoryRead32(U32 at) override {return Get<U32>(at);}
    std::uint64_t MemoryRead64(U32 at) override {return Get<std::uint64_t>(at);}
    void MemoryWrite8(U32 at,std::uint8_t v) override {Put(at,v);}
    void MemoryWrite16(U32 at,std::uint16_t v) override {Put(at,v);}
    void MemoryWrite32(U32 at,U32 v) override {Put(at,v);}
    void MemoryWrite64(U32 at,std::uint64_t v) override {Put(at,v);}
    bool MemoryWriteExclusive32(U32 at,U32 value,U32 expected) override {
        if(Get<U32>(at)!=expected)return false;
        Put(at,value);return true;
    }
    void InterpreterFallback(U32 pc,std::size_t) override {
        std::cerr<<"fallback "<<std::hex<<pc<<'\n';std::abort();
    }
    void ExceptionRaised(U32 pc,Dynarmic::A32::Exception) override {
        std::cerr<<"exception "<<std::hex<<pc<<'\n';std::abort();
    }
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();
        if(!svc) {cpu->HaltExecution();return;}
        if(svc==1)r[0]=allowance;
        else if(svc==2)r[0]=committed;
        else if(svc==3) {assert(r[0]==12);r[0]=0x710000;}
        else assert(svc==4);
        r[15]=r[14];
    }
    U32 Run(U32 pc,U32 a=0,U32 b=0) {
        cpu->Reset();cpu->SetCpsr(0x10);
        auto& r=cpu->Regs();r[0]=a;r[1]=b;r[13]=Stack;r[14]=Stop;r[15]=pc;
        ticks=10000000;cpu->Run();
        assert(ticks && (r[15]==Stop+4 || r[15]==stop_at+4));
        return r[0];
    }
    U32 HeapSize(U32 budget) {
        allowance=budget;stop_at=0x41611c;cpu->ClearCache();
        Run(0x4160ec); // Execute native subtraction/alignment up to the first allocation.
        const auto size=cpu->Regs()[5];
        stop_at=Stop;cpu->ClearCache();return size;
    }
    U32 Alloc(U32 size) {return Run(0x35010c,size);}
    void Free(U32 address) {Run(0x34fc6c,address);}
};

int main(int argc,char** argv) {
    assert(argc==2);Env e;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);assert(file);
    const auto length=file.tellg();assert(length>0 && length<0x600000);file.seekg(0);
    file.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),length);assert(file);
    Dynarmic::ExclusiveMonitor monitor(1);
    Dynarmic::A32::UserConfig config;config.callbacks=&e;config.global_monitor=&monitor;
    Dynarmic::A32::Jit cpu(config);e.cpu=&cpu;
    constexpr U32 MiB=1024*1024, FailedResource=1376728, LargestFree=1358816;
    const auto old_size=e.HeapSize(64*MiB), new_size=e.HeapSize(96*MiB);
    assert(old_size==0x151c000 && new_size-old_size==32*MiB);
    e.graphics_expanded=true;
    const auto graphics_size=e.HeapSize(96*MiB);
    assert(graphics_size==new_size-OOT3D::GraphicsMemory::Extra);
    // Match the failure's largest free block; the tiny other free fragments
    // cannot satisfy this request. The real native allocator must fail at 64 MiB.
    for(const auto size : {old_size,new_size,graphics_size}) {
        e.Run(0x416cc4,Env::ArenaBase,size); // Native SystemArena initializer.
        const auto initial=D::InspectArena(e);assert(initial.complete && initial.blocks==1);
        const auto busy=e.Alloc(old_size-LargestFree-32);assert(busy);
        auto resource=e.Alloc(FailedResource);
        if(size==old_size) {
            assert(!resource && D::InspectArena(e).largest_free==LargestFree);
        } else {
            assert(resource);
            // Model several additional large enemy resources, free in mixed
            // order, and confirm the native allocator coalesces/reuses them.
            auto second=e.Alloc(8*MiB), third=e.Alloc(4*MiB);assert(second && third);
            e.Free(second);e.Free(resource);e.Free(third);
            resource=e.Alloc(12*MiB);assert(resource);e.Free(resource);
        }
        e.Free(busy);
        const auto final=D::InspectArena(e);
        assert(final.complete && final.blocks==1 && final.free==initial.free && final.used==0);
    }
    std::cout<<"Native OoT heap: recorded failure reproduced at 64 MiB; allocations and cleanup pass at 96 MiB, including the expanded graphics reservation.\n";
}
