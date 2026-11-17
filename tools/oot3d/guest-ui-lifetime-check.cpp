// SPDX-License-Identifier: GPL-2.0-or-later
// Run the native mesh/model constructors, uploads, draw dispatch and destructor.
// Only allocation, GPU storage and the final GPU submission are supplied by the harness.
#include "oot3d/guest_ui_mesh.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>

using U32=std::uint32_t;
struct Geometry {
    std::array<float,12*256> positions{};
    std::array<float,8*256> uvs{};
    std::array<float,16*256> colors{};
    unsigned count=1;
};
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Stop=0xf00000, Resume=Stop+4, Alloc=Stop+8, Free=Stop+12;
    static constexpr U32 Source=0x700000, Mesh=0x701000, Desc=0x702000, Material=0x703000;
    static constexpr U32 Texture=0x14000000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000), texture=std::vector<unsigned char>(0x400000);
    Dynarmic::A32::Jit* cpu=nullptr;
    std::map<U32,std::function<void()>> mocks;
    std::function<void()> resume;
    std::map<U32,U32> buffers, bindings;
    std::set<U32> freed;
    U32 next=0x800000,next_id=1,expected_material=Material;
    unsigned allocations=0,draws=0,texture_uploads=0;
    U32 linear_next=Texture+256,general_bytes=0,linear_bytes=0;
    bool fail_linear=false,fail_texture=false;
    std::uint64_t ticks=0;
    bool stopped=false;
    unsigned char* Ptr(U32 at,unsigned size) {
        if(at>=Texture && std::uint64_t(at)+size<=Texture+texture.size())return texture.data()+at-Texture;
        assert(std::uint64_t(at)+size<=bytes.size());return bytes.data()+at;
    }
    template<class T> T Get(U32 at) {T v;std::memcpy(&v,Ptr(at,sizeof v),sizeof v);return v;}
    template<class T> void Put(U32 at,T v) {std::memcpy(Ptr(at,sizeof v),&v,sizeof v);}
    std::optional<U32> Read(U32 at) {return Get<U32>(at);}
    void Write(U32 at,U32 v) {Put(at,v);}
    bool WriteWords(U32 at,std::span<const U32> words) {
        if(at<Texture || std::uint64_t(at)+words.size_bytes()>Texture+texture.size())return false;
        std::memcpy(Ptr(at,words.size_bytes()),words.data(),words.size_bytes());
        ++texture_uploads;return true;
    }
    U32 Allocate(unsigned size) {U32 p=next;next+=(size+127)&~127u;++allocations;assert(next<0xd00000);return p;}
    std::optional<U32> MemoryReadCode(U32 at) override {
        if(at==Stop)return 0xef000000;
        if(at==Resume)return 0xef000001;
        if(mocks.contains(at))return 0xef000002;
        return Get<U32>(at);
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
        if(svc==0) {stopped=true;cpu->HaltExecution();return;}
        if(svc==1) {resume();return;}
        if(svc==2) {const U32 pc=r[15]-4;r[15]=r[14];mocks.at(pc)();return;}
        std::cerr<<"unexpected svc "<<svc<<'\n';std::abort();
    }
    void Install() {
        auto& r=cpu->Regs();
        mocks[0x313ce0]=[&]{general_bytes+=r[0];r[0]=Allocate(r[0]);};
        mocks[Alloc]=[&]{r[0]=Allocate(r[1]);};
        mocks[Free]=[&]{assert(freed.insert(r[1]).second);};
        mocks[0x2a8bdc]=[&]{
            assert(r[0]==0x10000 && r[1]==0x101);
            if(fail_linear || (fail_texture && r[3]==256)){r[0]=0;return;}
            const U32 size=(r[3]+127)&~127u;
            assert(linear_next+size<=Texture+texture.size());
            r[0]=linear_next;linear_next+=size;linear_bytes+=size;++allocations;
        };
        mocks[0x2c1678]=[&]{for(U32 i=0;i<r[0];++i)Write(r[1]+i*4,next_id++);};
        mocks[0x2d15d8]=[&]{bindings[r[0]]=r[1];};
        mocks[0x2c150c]=[&]{const U32 p=Allocate(r[1]);buffers[bindings.at(r[0])]=p;if(r[2])std::memcpy(Ptr(p,r[1]),Ptr(r[2],r[1]),r[1]);};
        mocks[0x2d14c4]=[&]{std::memcpy(Ptr(buffers.at(bindings.at(r[0]))+r[1],r[2]),Ptr(r[3],r[2]),r[2]);};
        mocks[0x2c14a8]=[&]{Write(r[2],buffers.at(bindings.at(r[0])));};
        mocks[0x30e604]=[]{ };
        mocks[0x32471c]=[]{ }; // Start native GPU material command block.
        mocks[0x3130a4]=[&]{
            // This pointer is loaded by the actual native BoardRenderer draw.
            if(r[0]!=expected_material || freed.contains(r[0])) {
                std::cerr<<"stale material: expected "<<std::hex<<expected_material<<" got "<<r[0]<<'\n';
                std::abort();
            }
            ++draws;r[15]=0x3fbc6c; // Native draw epilogue; omit GPU submission.
        };
        Write(0x710000,0x711000);Write(0x711000+0xc,Alloc);Write(0x711000+0x10,Free);
        for(U32 at:{0x55a19cu,0x55a1a4u,0x55a1a8u})Write(at,0x710000);
        Write(Mesh,Desc);Write(Desc+0xc,4);Write(Desc+0x14,6);Write(Desc+0x1c,0x1c0);
        Write(Mesh+0x158,Texture);Write(Mesh+0x15c,8|(8<<16));
        std::fill(texture.begin(),texture.begin()+256,0xa5);
    }
    void Execute() {ticks=10000000;stopped=false;cpu->Run();assert(stopped);}
    void Run(U32 pc,U32 a,U32 b=0,U32 c=0) {
        cpu->Reset();cpu->SetCpsr(0x10);auto& r=cpu->Regs();
        r[0]=a;r[1]=b;r[2]=c;r[13]=0xe00000;r[14]=Stop;r[15]=pc;Execute();
    }
    void Donor(U32 source,U32 material) {
        Run(0x34897c,0,Mesh,material);
        const U32 native=cpu->Regs()[0];
        std::memcpy(Ptr(source,0x1e4),Ptr(native,0x1e4),0x1e4);
        Write(Get<U32>(source+8)+0x354,source);Write(source+0x1dc,material);
        expected_material=material;
    }
    template<bool Borrow,unsigned Width,unsigned Height>
    void Draw(OOT3D::GuestUI::Hook<Geometry,Width,Height,0,Borrow>& hook,U32 donor,float x,bool success=true) {
        Geometry g;g.positions[0]=x;g.colors.fill(1);
        std::vector<U32> pixels(Width*Height);
        for(unsigned i=0;i<pixels.size();++i)pixels[i]=0x12345600+i;
        cpu->Reset();cpu->SetCpsr(0x10);auto& r=cpu->Regs();
        for(unsigned i=0;i<13;++i)r[i]=0x12340000+i;
        r[13]=0xe00000;r[14]=Stop;r[15]=Stop;const auto saved=r;
        resume=[&]{hook.Resume(*this,r);};
        hook.Begin(*this,r,Resume,g,pixels,donor);Execute();
        r[15]=Stop;assert(r==saved && !hook.Pending() && hook.Owned(*this)==success);
        // Generated mesh writes must not touch the native donor's texture.
        for(unsigned i=0;i<256;++i)assert(texture[i]==0xa5);
        if(!success)return;
        if constexpr(Borrow) {
            const U32 mesh=Get<U32>(donor+4);
            for(unsigned at=0x13c;at<0x19c;at+=4)
                assert(Get<U32>(hook.block+at)==Get<U32>(mesh+at));
        } else {
            assert(hook.texture>=hook.block+hook.AllocationSize);
            for(unsigned i=0;i<pixels.size();++i)assert(Get<U32>(hook.texture+4*i)==pixels[i]);
            assert(Get<U32>(hook.block+0x158)==hook.texture);
        }
        const U32 current=Get<U32>(hook.block+0x124);
        assert(Get<float>(Get<U32>(hook.block+0x1a0+current*4))==x);
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream f(argv[1],std::ios::binary|std::ios::ate);assert(f);
    const auto size=f.tellg();assert(size>0 && size<0x600000);f.seekg(0);
    f.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);assert(f);
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;e.Install();
    OOT3D::GuestUI::Hook<Geometry,8,8> background;
    OOT3D::GuestUI::Hook<Geometry,8,8,0,true> keyboard;
    e.Donor(Env::Source,Env::Material);
    e.Draw(background,Env::Source,20);e.Draw(keyboard,Env::Source,40);
    // Native naming teardown frees its material. Recreated menus can occupy
    // the same model address while their material resides elsewhere.
    e.Run(0x348904,0,Env::Source);assert(e.freed.contains(Env::Material));
    e.Donor(Env::Source,Env::Material+0x400);
    const unsigned allocated=e.allocations;
    e.Draw(background,Env::Source,-12);e.Draw(keyboard,Env::Source,100);
    assert(e.allocations==allocated);
    // Switching file/naming/pause donors must rebind without leaking another
    // texture, mesh, model or renderer on every visit.
    e.Donor(Env::Source+0x400,Env::Material+0x800);
    const unsigned stable=e.allocations;
    for(unsigned i=0;i<100;++i) {
        const U32 donor=Env::Source+(i%2)*0x400;
        e.expected_material=Env::Material+0x400+(i%2)*0x400;
        e.Draw(background,donor,float(i));e.Draw(keyboard,donor,-float(i));
    }
    assert(e.allocations==stable && e.draws==204);
    // Keep more CPU mesh data resident than the game's entire 512 KiB C++
    // heap. Native constructors/uploads/draws must work from linear RAM and
    // none of those arrays may call the small general allocator.
    std::array<OOT3D::GuestUI::Hook<Geometry,8,8,0,true>,16> retained;
    e.expected_material=Env::Material+0x400;
    for(auto& h:retained)e.Draw(h,Env::Source,1);
    assert(e.linear_bytes>0x80000 && e.general_bytes==0);
    const unsigned retained_allocations=e.allocations;
    for(unsigned visit=0;visit<10;++visit)
        for(auto& h:retained)e.Draw(h,Env::Source,float(visit));
    assert(e.allocations==retained_allocations);
    // An allocation failure leaves the native caller's registers untouched.
    // An initial failure can recover; a partial construction cannot repeatedly
    // allocate another mesh when the texture resource pool is exhausted.
    OOT3D::GuestUI::Hook<Geometry,8,8> retry,partial;
    e.fail_linear=true;e.Draw(retry,Env::Source,0,false);
    e.fail_linear=false;e.Draw(retry,Env::Source,0);
    e.fail_texture=true;e.Draw(partial,Env::Source,0,false);
    assert(partial.creation_failed && partial.block);
    const unsigned failed_allocations=e.allocations;
    for(unsigned visit=0;visit<100;++visit)e.Draw(partial,Env::Source,0,false);
    assert(e.allocations==failed_allocations);e.fail_texture=false;
    const U32 retained_block=partial.block;
    e.Draw(partial,Env::Source,7);
    assert(partial.block==retained_block && !partial.creation_failed);
    OOT3D::GuestUI::Hook<Geometry,8,8> early;
    const unsigned before_early=e.allocations;
    e.Write(Env::Source+0x1dc,0);e.Draw(early,Env::Source,0,false);
    assert(e.allocations==before_early && !early.creation_failed);
    e.Write(Env::Source+0x1dc,Env::Material+0x400);e.Draw(early,Env::Source,1);
    // The generated map uses a much larger texture than the small UI gradients.
    // Exercise that size with the same native constructors, sampler and uploads.
    OOT3D::GuestUI::Hook<Geometry,1024,512> map;
    const unsigned before_map_uploads=e.texture_uploads;
    e.Draw(map,Env::Source,2);
    assert(e.texture_uploads==before_map_uploads+1);
    const unsigned map_allocations=e.allocations;
    e.Draw(map,Env::Source,3);assert(e.allocations==map_allocations);
    assert(e.texture_uploads==before_map_uploads+1);
    assert(e.Get<U32>(map.block+0x15c)==(1024|(512u<<16)));
    // Generated models borrow their donor's material; their native destructor
    // must never free it a second time.
    e.Run(0x348904,0,background.model);e.Run(0x348904,0,keyboard.model);
    assert(!e.freed.contains(Env::Material+0x400) && !e.freed.contains(Env::Material+0x800));
    std::cout<<"Native guest UI lifecycle: linear mesh allocation beyond 512 KiB, reuse, allocation failure, material replacement and borrowed-material destruction passed\n";
}
