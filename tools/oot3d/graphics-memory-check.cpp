// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/graphics_memory.h"
#include "oot3d/frame_lights.h"
#include "oot3d/graphics_memory_diagnostics.h"
#include "oot3d/graphics_allocation_diagnostics.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

namespace G=OOT3D::GraphicsMemory;
using U32=std::uint32_t;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Stop=0xf00000,Heap=0x5b4f7c,LinearBase=0x14000000;
    std::vector<unsigned char> code=std::vector<unsigned char>(0x1000000);
    std::vector<unsigned char> linear=std::vector<unsigned char>(G::Linear);
    Dynarmic::A32::Jit* cpu=nullptr;
    std::uint64_t ticks=0;
    U32 app_budget=96*G::MiB,linear_requested=0,heap_requested=0;
    bool frame_lights=false;
    bool patched=false,boot=false,diagnostics=false;
    OOT3D::GraphicsAllocations::Tracker tracker;
    std::int64_t now=0;
    unsigned char* Ptr(U32 at,unsigned size) {
        if(at==0x1ff80040 && size==4)return reinterpret_cast<unsigned char*>(&app_budget);
        if(at>=LinearBase && std::uint64_t(at)+size<=LinearBase+linear.size())return linear.data()+at-LinearBase;
        assert(std::uint64_t(at)+size<=code.size());return code.data()+at;
    }
    void Write(U32 at,U32 value){Put(at,value);}
    template<class T> T Get(U32 at) {T v;std::memcpy(&v,Ptr(at,sizeof v),sizeof v);return v;}
    template<class T> void Put(U32 at,T v) {std::memcpy(Ptr(at,sizeof v),&v,sizeof v);}
    std::optional<U32> Read(U32 at) {return Get<U32>(at);}
    std::optional<U32> MemoryReadCode(U32 at) override {
        if(at==Stop || (boot && at==0x416128))return 0xef000000;
        if(boot && (at==0x416554 || at==0x416564 || at==0x302058 || at==0x301fc4))return 0xef000001;
        if(frame_lights && at==OOT3D::FrameLights::ResetPC+4)return 0xef000000;
        const U32 word=Get<U32>(at);
        if(frame_lights && OOT3D::FrameLights::Instruction(at,word)!=word)
            return OOT3D::FrameLights::Instruction(at,word);
        if(diagnostics && OOT3D::GraphicsAllocations::CodeMatches(*this) &&
           OOT3D::GraphicsAllocations::Instruction(at,word)!=word)
            return OOT3D::GraphicsAllocations::Instruction(at,word);
        return patched && G::Supported(*this)?G::Instruction(at,word):word;
    }
    std::uint8_t MemoryRead8(U32 a) override {return Get<std::uint8_t>(a);}
    std::uint16_t MemoryRead16(U32 a) override {return Get<std::uint16_t>(a);}
    U32 MemoryRead32(U32 a) override {return Get<U32>(a);}
    std::uint64_t MemoryRead64(U32 a) override {return Get<std::uint64_t>(a);}
    void MemoryWrite8(U32 a,std::uint8_t v) override {Put(a,v);}
    void MemoryWrite16(U32 a,std::uint16_t v) override {Put(a,v);}
    void MemoryWrite32(U32 a,U32 v) override {Put(a,v);}
    void MemoryWrite64(U32 a,std::uint64_t v) override {Put(a,v);}
    void InterpreterFallback(U32 pc,std::size_t) override {std::cerr<<"fallback "<<std::hex<<pc<<'\n';std::abort();}
    void ExceptionRaised(U32 pc,Dynarmic::A32::Exception) override {std::cerr<<"exception "<<std::hex<<pc<<'\n';std::abort();}
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();
        if(svc>=OOT3D::GraphicsAllocations::FirstSVC && svc<OOT3D::GraphicsAllocations::FirstSVC+2) {
            tracker.Handle(*this,r,svc-OOT3D::GraphicsAllocations::FirstSVC,now);return;
        }
        if(svc==OOT3D::FrameLights::FirstSVC || svc==OOT3D::FrameLights::FirstSVC+1) {
            OOT3D::FrameLights::Handle(*this,r,svc-OOT3D::FrameLights::FirstSVC);
            return;
        }
        if(!svc){cpu->HaltExecution();return;}
        assert(svc==1 && boot);
        switch(r[15]-4) {
        case 0x416554:r[0]=app_budget;break;
        case 0x416564:r[0]=9*G::MiB;break;
        case 0x302058:linear_requested=r[0];break;
        case 0x301fc4:heap_requested=r[0];break;
        default:std::abort();
        }
        r[15]=r[14];
    }
    U32 Run(U32 pc,U32 a=0,U32 b=0,U32 c=0) {
        cpu->Reset();cpu->ClearCache();cpu->ClearHalt();cpu->SetCpsr(0x10);
        auto& r=cpu->Regs();r[0]=a;r[1]=b;r[2]=c;r[13]=0xe00000;r[14]=Stop;r[15]=pc;
        ticks=1000000;cpu->Run();assert(ticks);return r[0];
    }
};
std::vector<unsigned char> Load(const char* path) {
    std::ifstream f(path,std::ios::binary);assert(f);
    return {(std::istreambuf_iterator<char>(f)),{}};
}
void CheckDiagnostics(Env& e) {
    struct ResourceMemory {
        std::map<U32,U32> words;
        std::optional<U32> Read(U32 at) {
            const auto it=words.find(at);
            return it==words.end()?std::nullopt:std::optional<U32>{it->second};
        }
    } resource;
    constexpr U32 texture=0x08001000;
    assert(!G::FailedFontAllocation(resource,0));
    assert(!G::FailedFontAllocation(resource,texture)); // Unreadable is not null.
    resource.words[texture+0x24]=32768;
    assert(!G::FailedFontAllocation(resource,texture));
    resource.words[texture+0x4c]=0;
    assert(G::FailedFontAllocation(resource,texture)==32768);
    resource.words[texture+0x24]=0;
    assert(!G::FailedFontAllocation(resource,texture)); // No allocation requested.
    resource.words[texture+0x24]=32768;resource.words[texture+0x4c]=0x14010000;
    assert(!G::FailedFontAllocation(resource,texture)); // Successful allocation.
    assert(!G::FailedFontAllocation(resource,0xfffffff0)); // No address wraparound.
    constexpr U32 start=Env::LinearBase+16,block=start+128,capacity=G::NativeTextures;
    e.Put<U32>(Env::Heap,0x45585048);
    e.Put<U32>(Env::Heap+0x18,start);e.Put<U32>(Env::Heap+0x1c,start+capacity);
    e.Put<U32>(Env::Heap+0x24,block);e.Put<U32>(Env::Heap+0x28,block);
    e.Put<U32>(block,0x4652);e.Put<U32>(block+4,2*G::MiB);
    e.Put<U32>(block+8,0);e.Put<U32>(block+12,0);
    const auto code=e.code,linear=e.linear;
    G::Monitor monitor;
    auto s=monitor.Poll(e,0);
    assert(s && s->free.valid && s->occupied==capacity-2*G::MiB && !s->pressure);
    assert(!monitor.Poll(e,999999) && monitor.samples==1);
    assert(!monitor.Poll(e,1000000) && monitor.samples==2);
    assert(monitor.latest.samples==2); // Live UI advances even when logs are throttled.
    // Monitoring must not write any guest memory.
    assert(e.code==code && e.linear==linear);
    e.Put<U32>(block+4,32768);
    s=monitor.Poll(e,5000000);
    assert(s && s->pressure && s->peak_occupied==capacity-32768 && s->min_largest==32768);
    e.Put<U32>(block+4,2*G::MiB);
    s=monitor.Poll(e,10000000);
    assert(s && !s->pressure && s->peak_occupied==capacity-32768 && s->min_largest==32768);
    // No short-session report cap: reports continue well past the old cap.
    for(int i=1;i<=120;++i)assert(monitor.Poll(e,10000000LL+i*30000000LL));
    assert(monitor.AllocationFailed(4000000000LL));
    for(int i=0;i<100;++i)assert(!monitor.AllocationFailed(4000000001LL+i));
    assert(monitor.AllocationFailed(4005000000LL) && monitor.failures==102);
    // Time rollback resets the monitoring window, including old peak/failures.
    s=monitor.Poll(e,0);
    assert(s && s->samples==1 && s->failures==0 && s->peak_occupied==capacity-2*G::MiB);
    e.Put<U32>(Env::Heap+0x1c,start+G::Textures);
    s=monitor.Poll(e,1000000);
    assert(s && s->samples==1 && s->free.capacity==G::Textures);
    e.Put<U32>(block+12,block);assert(!G::TextureFree(e).valid); // Cycle.
    e.Put<U32>(block+12,0);
    e.Put<U32>(Env::Heap+0x28,0);assert(!G::TextureFree(e).valid); // Wrong tail.
    e.Put<U32>(Env::Heap+0x28,block);
    e.Put<U32>(block+4,G::Textures);assert(!G::TextureFree(e).valid); // Out of bounds.
    e.Put<U32>(block+4,2*G::MiB);
    e.Put<U32>(block+8,block);assert(!G::TextureFree(e).valid); // Wrong predecessor.
    e.Put<U32>(block+8,0);
    e.Put<U32>(Env::Heap+0x24,0);e.Put<U32>(Env::Heap+0x28,0);
    const auto full=G::TextureFree(e);
    assert(full.valid && full.bytes==0 && full.largest==0); // Exhaustion is valid metadata.
    e.Put<U32>(Env::Heap,0);
    G::Monitor invalid;
    s=invalid.Poll(e,0);assert(s && !s->free.valid);
    assert(!invalid.Poll(e,1000000));
    s=invalid.Poll(e,30000000);assert(s && !s->free.valid);
    std::cout<<"Graphics diagnostics: confirmed font failure detection, read-only sampling, persistent reports, peak tracking, failure throttling, reset and invalid-list guards passed\n";
}
void CheckLiveDiagnostics() {
    G::LiveDiagnostics live;
    assert(!live.Read().observed);
    G::Monitor::Sample sample{};
    sample.free={2*G::MiB,G::MiB,3,G::Textures,Env::LinearBase+16,true};
    sample.occupied=G::Textures-sample.free.bytes;sample.peak_occupied=sample.occupied;
    sample.min_largest=sample.free.largest;sample.samples=1;
    live.Publish(sample,0);
    const auto first=live.Read();
    assert(first.observed && first.count==1 && first.next==1);
    assert(first.sample.free.bytes==2*G::MiB && first.occupied_mib[0]==float(G::Textures/G::MiB-2));
    for(unsigned i=1;i<=125;++i) {
        sample.occupied=i*1024;sample.samples=i+1;
        live.Publish(sample,i*1000000LL);
    }
    auto current=live.Read();
    assert(current.count==120 && current.next==6 && current.sample.samples==126);
    assert(current.occupied_mib[current.next]==6.f/1024); // Oldest retained sample.
    assert(first.sample.samples==1 && first.occupied_mib[0]==float(G::Textures/G::MiB-2)); // Stable copy.
    live.Publish(sample,0);current=live.Read();
    assert(current.count==1 && current.guest_time_us==0); // State/time rollback.
    sample.free.capacity=G::NativeTextures;
    live.Publish(sample,1000000);assert(live.Read().count==1); // New heap.
    sample.free.valid=false;
    live.Publish(sample,2000000);current=live.Read();
    assert(!current.sample.free.valid && current.count==0);
    sample.free.valid=true;sample.failures=1;sample.pressure=true;
    live.Publish(sample,3000000);current=live.Read();
    assert(current.count==1 && current.sample.failures==1 && current.sample.pressure);
    live.Reset();current=live.Read();
    assert(!current.observed && current.count==0 && current.sample.failures==0);
    std::cout<<"Live graphics diagnostics: coherent copies, bounded history, invalid samples, rollback and session reset passed\n";
}
void CheckAllocationLedger() {
    namespace A=OOT3D::GraphicsAllocations;
    struct Memory {
        std::map<U32,U32> words;
        std::optional<U32> Read(U32 at) {
            const auto i=words.find(at);return i==words.end()?std::nullopt:std::optional<U32>{i->second};
        }
    } m;
    constexpr U32 start=0x14000010,end=start+G::Textures;
    m.words[A::Heap]=0x45585048;m.words[A::Heap+0x18]=start;m.words[A::Heap+0x1c]=end;
    m.words[A::Heap+0x24]=m.words[A::Heap+0x28]=0;
    A::Tracker t;assert(t.Check(m,0));A::Key key;key.caller=0x123450;
    const auto original=m.words;
    t.Allocate(key,1024,start+128,0);assert(t.live_bytes==1024 && t.live.size()==1);
    unsigned reports=0,details=0;
    const auto report=[&](std::int64_t now) {
        unsigned rows=0;
        t.Report(m,now,[&](const auto&,const auto&,const auto&,const char*){++reports;},
            [&](const auto& c){++details;++rows;assert(c.live_bytes>0 || c.Delta()>0);});
        assert(rows<=12);
    };
    report(0);report(100);assert(reports==1 && details==1);
    // First failure bypasses throttling, so a hang immediately afterwards
    // still leaves the failing size/caller in the log. Repeats are bounded.
    t.Allocate(key,4096,0,101);report(101);assert(reports==2 && t.failures==1);
    t.Allocate(key,4096,0,102);report(102);assert(reports==2);
    report(2000000);assert(reports==3);
    t.Release(start+128);assert(t.live.empty() && t.live_bytes==0 && t.frees==1);
    t.Release(start+128);assert(t.unknown_frees==1);
    t.Allocate(key,1024,start+128,2000000);t.Allocate(key,2048,start+128,2000000);
    assert(t.live_bytes==2048 && t.reused==1 && t.callers[0].live_count==1);
    assert(t.Check(m,0) && t.generation==2 && t.live.empty() && t.failures==0);
    for(unsigned i=0;i<=A::Tracker::MaxLive;++i)t.Allocate(key,4,start+4*i,0);
    assert(t.live.size()==A::Tracker::MaxLive && t.dropped==1);
    m.words[A::Heap+0x1c]=end-4096;assert(t.Check(m,1) && t.live.empty() && t.generation==3);
    for(unsigned i=0;i<=A::Tracker::MaxCallers;++i){key.caller=0x100000+i*4;t.Allocate(key,4,start+4*i,1);}
    assert(t.callers.size()==A::Tracker::MaxCallers && t.dropped==1);
    report(1);assert(reports==4);
    m.words[A::Heap+0x1c]=end;assert(m.words==original); // Ledger never writes guest memory.
    std::cout<<"Graphics ledger: retained/released bytes, reuse visibility, bounded records/reports, failure throttling, rollback and heap reset passed\n";
}
// Run the native record initializer and point-light writer against the queue
// address from the Hyrule Field capture. Preserve actor and shadow lifetimes.
void CheckFrameLights(Env& e) {
    namespace L=OOT3D::FrameLights;
    assert(L::CodeMatches(e));
    constexpr U32 gfx=0xd00000, tint=gfx+0x100, light=gfx+0x200, position=gfx+0x300;
    constexpr U32 scratch=0x5656a0, queue_capacity=0x565530, init=0x35bfb4;
    const auto setup=[&](U32 tail) {
        e.Put<U32>(gfx,0x4000);e.Put(gfx+4,scratch);e.Put(gfx+8,scratch);e.Put(gfx+12,tail);
        e.Put<U32>(queue_capacity,16);
    };
    e.Put<U32>(tint+12,0x66554433);e.Put<U32>(tint+16,0x77);
    e.Put<U32>(light+12,0xabcdef);e.Put<U32>(light+16,100);
    setup(0x5655a0);
    assert(e.Run(init,tint,gfx)==0x565520);
    e.Run(0x213d80,0x565520,light,position);
    assert(e.Get<U32>(queue_capacity)!=16); // Reproduce the original write.
    setup(scratch+0x4000);
    e.frame_lights=true;
    e.Put<U32>(L::Base,99);e.Run(L::ResetPC);assert(e.Get<U32>(L::Base)==0);
    for(U32 i=0;i<L::Records;++i) {
        const auto at=e.Run(init,tint,gfx);
        assert(at==L::Slots+i*L::RecordBytes && e.Get<U32>(gfx+12)==scratch+0x4000);
        assert(e.Get<std::uint8_t>(at)==0 && e.Get<std::uint8_t>(at+8)==0x55 && e.Get<std::uint8_t>(at+9)==0x66 && e.Get<std::uint8_t>(at+10)==0x77);
        e.Run(0x213d80,at,light,position);
    }
    assert(e.Get<std::uint8_t>(L::Slots)==1);
    for(unsigned i=1;i<=128;++i)assert(e.Run(init,tint,gfx)==scratch+0x4000-i*128);
    const auto emergency=e.Run(init,tint,gfx);
    assert(emergency>=L::Base+128 && emergency+128<=L::Slots);
    e.Run(0x213d80,emergency,light,position);
    assert(e.Get<U32>(queue_capacity)==16 && e.Get<U32>(gfx+12)==scratch);
    setup(scratch+0x4000);e.Put<U32>(gfx+8,scratch+0x4000-64);
    assert(e.Run(init,tint,gfx)==emergency && e.Get<U32>(gfx+12)==scratch+0x4000);
    e.Run(L::ResetPC);assert(e.Run(init,tint,gfx)==L::Slots);
    e.frame_lights=false;
    std::cout<<"Frame lights: reproduced the Hyrule Field queue overwrite; distinct records, native light colors, frame reset and exhaustion guards passed\n";
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    Env e;const auto native=Load(argv[1]);assert(native.size()+0x100000<e.code.size());
    std::copy(native.begin(),native.end(),e.code.begin()+0x100000);
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu(config);e.cpu=&cpu;
    assert(G::CodeMatches(e) && G::Supported(e));
    CheckFrameLights(e);
    // Run the original boot partition arithmetic and allocation calls with and
    // without translation patches, including all surrounding ARM instructions.
    e.boot=true;e.Run(0x4160ec);
    const auto old_heap=e.heap_requested;
    assert(e.linear_requested==G::NativeLinear);
    e.patched=true;e.Run(0x4160ec);
    assert(e.linear_requested==G::Linear && e.heap_requested==old_heap-G::Extra);
    assert(e.linear_requested+e.heap_requested+9*G::MiB==e.app_budget);
    assert(e.heap_requested>old_heap-32*G::MiB); // Still above stock 64 MiB setup.
    e.app_budget=64*G::MiB;e.Run(0x4160ec);
    assert(e.linear_requested==G::NativeLinear); // Explicit low-memory override.
    e.app_budget=96*G::MiB;e.boot=false;
    // Decode/execute both subheap-size replacements as ARM, preserving the
    // eight MiB reserved for other SDK buffers in the parent linear heap.
    for(unsigned i=2;i<4;++i) {
        e.Put<U32>(0x700000,G::Instruction(G::Sites[i].pc,G::Sites[i].instruction));
        e.Put<U32>(0x700004,0xe12fff1e);e.Run(0x700000);
        assert(cpu.Regs()[i-1]==G::Textures);
    }
    for(auto site:G::Sites) {
        e.Put(site.pc,site.instruction^1);assert(!G::CodeMatches(e));e.Put(site.pc,site.instruction);
    }
    std::cout<<"Graphics budget: native boot arithmetic, matched parent/subheap sizes, low-memory and code guards passed\n";
    CheckDiagnostics(e);
    CheckLiveDiagnostics();
    CheckAllocationLedger();
    if(argc==3) {
        // Optional local fixture: 0x60 bytes of native EXPH metadata followed
        // by its original 19 MiB allocation (including the parent header tail).
        const auto fixture=Load(argv[2]);assert(fixture.size()==0x60+G::NativeTextures+16);
        std::memcpy(e.Ptr(Env::Heap,0x60),fixture.data(),0x60);
        std::memcpy(e.linear.data(),fixture.data()+0x60,fixture.size()-0x60);
        // Execute actual allocator/free code with and without diagnostic SVCs.
        // Registers, flags and guest memory must match byte-for-byte, including
        // allocation failure. Diagnostics only change the host-side ledger.
        assert(OOT3D::GraphicsAllocations::CodeMatches(e));
        const auto original_code=e.code,original_linear=e.linear;
        for(U32 size:{4096u,32*G::MiB}) {
            e.diagnostics=false;e.code=original_code;e.linear=original_linear;
            const U32 allocation=e.Run(0x3222dc,Env::Heap-0x18,size,128);
            assert((allocation!=0)==(size==4096));
            const auto memory=e.code,linear=e.linear;
            const auto regs=cpu.Regs();const auto fp=cpu.ExtRegs();const auto flags=cpu.Cpsr();
            e.diagnostics=true;e.code=original_code;e.linear=original_linear;e.tracker={};
            assert(e.Run(0x3222dc,Env::Heap-0x18,size,128)==allocation);
            assert(e.code==memory && e.linear==linear && cpu.Regs()==regs && cpu.ExtRegs()==fp && cpu.Cpsr()==flags);
            assert(e.tracker.allocations==1 && e.tracker.failures==(allocation==0));
            if(allocation) {
                assert(e.tracker.live_bytes==size && e.tracker.live.size()==1);
                e.diagnostics=false;e.Run(0x47d99c,Env::Heap,allocation);
                const auto freed_code=e.code,freed_linear=e.linear;
                const auto freed_regs=cpu.Regs();const auto freed_fp=cpu.ExtRegs();const auto freed_flags=cpu.Cpsr();
                e.code=memory;e.linear=linear;e.diagnostics=true;e.Run(0x47d99c,Env::Heap,allocation);
                assert(e.code==freed_code && e.linear==freed_linear && cpu.Regs()==freed_regs &&
                    cpu.ExtRegs()==freed_fp && cpu.Cpsr()==freed_flags);
                assert(e.tracker.live_bytes==0 && e.tracker.live.empty() && e.tracker.frees==1);
            }
        }
        e.diagnostics=false;e.code=original_code;e.linear=original_linear;
        std::cout<<"Graphics ledger native hooks: success/failure/free preserve all registers, flags and guest memory; live bytes return to zero after release\n";
        const auto free=G::TextureFree(e);assert(free.valid && free.bytes==47848 && free.largest==23808);
        // Execute the ACTUAL native allocator against the broken-session heap.
        assert(e.Run(0x4360c4,Env::Heap,32768,128)==0);
        // Model the extra capacity as the same heap initialized G::Extra bytes
        // larger at boot; this is only a test, never a live heap alteration.
        const U32 last=e.Get<U32>(Env::Heap+0x28);
        e.Put<U32>(Env::Heap+0x1c,e.Get<U32>(Env::Heap+0x1c)+G::Extra);
        e.Put<U32>(last+4,e.Get<U32>(last+4)+G::Extra);
        const auto expanded=G::TextureFree(e);assert(expanded.valid && expanded.bytes==free.bytes+G::Extra);
        for(unsigned i=0;i<128;++i) {
            const U32 allocation=e.Run(0x4360c4,Env::Heap,32768,128);
            assert(allocation>=Env::LinearBase && allocation+32768<=Env::LinearBase+G::Textures+16 && !(allocation&127));
        }
        assert(G::TextureFree(e).valid);
        // Exhaust the native allocator while guarding the neighboring SDK
        // region. Allocation failure must not spill beyond the graphics heap.
        const U32 limit=e.Get<U32>(Env::Heap+0x1c);
        const U32 parent_end=Env::LinearBase+G::Linear;
        assert(limit<parent_end);
        std::fill(e.Ptr(limit,parent_end-limit),e.Ptr(parent_end,0),0xa5);
        unsigned more=0;
        while(const U32 allocation=e.Run(0x4360c4,Env::Heap,32768,128)) {
            assert(allocation>=Env::LinearBase && std::uint64_t(allocation)+32768<=limit);
            std::memset(e.Ptr(allocation,32768),0x5a,32768);
            assert(++more<2048);
        }
        assert(G::TextureFree(e).valid);
        for(U32 at=limit;at<parent_end;++at)assert(e.Get<unsigned char>(at)==0xa5);
        std::cout<<"Graphics exhaustion: allocation returns null, adjacent SDK guard bytes intact\n";
        std::cout<<"Captured failure reproduced: native 32 KiB allocation fails in original heap; 128 consecutive text allocations pass with added capacity\n";
    }
}
