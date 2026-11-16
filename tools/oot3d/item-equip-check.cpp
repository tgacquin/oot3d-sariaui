// SPDX-License-Identifier: GPL-2.0-or-later
// Executes the original quick-equip/target-confirm code up to the native
// restriction check, using the installed game binary and built Dynarmic.
#include "oot3d/item_equip_input.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace OOT3D;
using namespace OOT3D::ItemEquip;
struct Env : Dynarmic::A32::UserCallbacks {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;Input input;
    std::uint64_t ticks=0;std::int64_t now=1000000;
    U32 native_pressed=0;bool checked=false;
    constexpr static U32 Play=0x900000,Stop=0xf00000,SP=0xe00000;
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop)return 0xef000000;
        if(a==0x33b5ec)return 0xef000001;
        if(a==0x2eb210)return 0xef000002;
        return Instruction(a,Get<U32>(a));
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
        if(svc==0 || svc==2) {checked=svc==2;cpu->HaltExecution();return;}
        if(svc==1) {r[0]=native_pressed;r[15]=r[14];return;}
        input.Handle(*this,r,svc-FirstSVC,now);
    }
    void Setup(unsigned state=2,unsigned age=0) {
        cpu->Reset();input={};checked=false;native_pressed=0;now=1000000;
        Write(PauseMenu::UI+0xc,Play);Write(PauseMenu::UI+0x14,1);
        Write(PauseMenu::UI+0x20,0);Write(PauseMenu::UI+0x24,1);Write(Play+0x100,0x0203);
        Write(0x54ac4c,1u<<24);Write(0x50446c+0x18,0);
        Write(Items+0x34,state);Write(Items+0x78,2);Write(Items+0x7c,1);
        Write(Items+0x50,8);Write(Items+0x54,0xffffffff);Write(Items+8,0);
        Write(0x587958+4,age);
        Put<std::uint8_t>(0x587958+(age?0x138a:0x13a2)+8,9);
        assert(Ready(*this));
    }
    void Poll(unsigned buttons=0,bool live=true,U32 pad=0) {input.Poll(*this,buttons,pad,live,now);}
    void Run() {
        auto& r=cpu->Regs();r[13]=SP;r[14]=Stop;r[15]=0x2ec3e4;
        cpu->SetCpsr(0x10);cpu->ClearHalt();ticks=10000;cpu->Run();assert(ticks);
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream file(argv[1],std::ios::binary|std::ios::ate);assert(file);
    const auto size=file.tellg();assert(size>0 && size<0xb00000);file.seekg(0);
    file.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);assert(file && CodeMatches(e));
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    for(unsigned state:{2u,8u})for(unsigned age:{0u,1u})for(unsigned trigger:{1u,2u}) {
        e.Setup(state,age);e.Poll(trigger);e.Poll(0); // Short taps survive until the game update.
        e.Run();assert(e.checked);
        assert(e.Read(Items+0x50)==8 && e.Read(Items+0x54)==(trigger==1?5:23));
        assert(e.Get<std::uint16_t>(Items+8)==271 && e.Get<std::uint16_t>(Items+10)==(trigger==1?7:191));
        assert(!e.input.pending && !e.input.assigning);
        // Game still receives the native restriction call, with the selected
        // source and target. No inventory/save bytes are changed by the patch.
        assert(e.Get<std::uint8_t>(0x587958+(age?0x138a:0x13a2)+8)==9);
    }
    for(auto [button,destination]:std::array<std::pair<U32,U32>,2>{{{0x400,11},{0x800,17}}}) {
        e.Setup();e.native_pressed=button;e.Run();assert(e.checked && e.Read(Items+0x54)==destination);
    }
    e.Setup();e.Poll(1);e.Poll(1);e.Run();assert(e.checked);e.Poll(1);assert(!e.input.pending);
    e.Setup();e.Poll(3);assert(!e.input.pending); // No ambiguous simultaneous assignment.
    e.Setup();e.Poll(1,false);e.Poll(1,true);assert(!e.input.pending); // Editor/disabled input cannot queue.
    e.Setup();e.Poll(1,true,2);assert(!e.input.pending); // B wins over equip.
    e.Setup();e.Poll(1);e.Write(Items+0x34,0);e.Poll(0);assert(!e.input.pending);
    e.Setup();e.Poll(1);e.now+=150001;
    std::array<U32,16> r{};r[5]=Items;e.input.Handle(e,r,0,e.now);
    assert(r[15]==0x33b5ec && !e.input.assigning);
    e.Setup();e.Poll(2);e.Write(PauseMenu::UI+0xc,Env::Play+0x10000);
    e.input.Handle(e,r,0,e.now);assert(r[15]==0x33b5ec && !e.input.assigning);
    e.Setup();e.Poll(1);e.Put<std::uint8_t>(0x587958+0x13a2+8,255);e.Run();
    assert(!e.checked && e.Read(Items+0x54)==0xffffffff); // Native empty-item rejection.
    std::cout<<"Native ZL/ZR equip: I/II targets in browsing and Choose modes, child/adult sources, normal X/Y, restrictions, short taps, hold suppression and stale-input rejection passed\n";
}
