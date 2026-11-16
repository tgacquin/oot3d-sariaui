// SPDX-License-Identifier: GPL-2.0-or-later
// Execute native I/II press, restriction, hold and release branches.
#include "oot3d/item_use_input.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace OOT3D;
using namespace OOT3D::ItemUse;
struct Env : Dynarmic::A32::UserCallbacks {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;Input input;
    std::uint64_t ticks=0;std::int64_t now=1000000;
    U32 native_pressed=0;bool checked=false;
    constexpr static U32 Play=0x8100000,Stop=0xf00000,SP=0xe00000;
    template<class T> T Get(U32 a) {if(a>=0x8000000)a-=0x7800000;assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {if(a>=0x8000000)a-=0x7800000;assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop || a==0x42ec00 || a==0x42f270)return 0xef000000;
        if(a==0x33f428 || a==0x2f43e8)return 0xef000001;
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
    void Setup() {
        cpu->Reset();input={};native_pressed=0;now=1000000;
        Write(PauseMenu::UI+0xc,Play);Write(PauseMenu::UI+0x14,1);
        Write(PauseMenu::UI+0x20,0);Write(PauseMenu::UI+0x24,0);Write(Play+0x100,0x0203);
        Write(Play+0x7f24,0x100);Write(Play+0x5c2c,0);Write(Play+0x7f10,0);
        Write(0x54ac4c,0);Write(Main+0x34,2);Write(Main+0x68,0);
        for(unsigned at=0x3c;at<=0x50;at+=4)Write(Main+at,0);
        Write(0x587958+0x80,0x09ffffff); // I has an item; native X/Y empty.
        Write(0x587958+0x84,9); // II has an item.
        Write(SP+0x1c,0);
        assert(Gameplay(*this));
    }
    void Poll(unsigned triggers=0,bool live=true,U32 pad=0) {input.Poll(*this,triggers,pad,live,now);}
    void Run(U32 pc=0x42eabc) {
        auto& r=cpu->Regs();r[5]=Main;r[6]=1;r[10]=0;r[13]=SP;r[14]=Stop;r[15]=pc;
        cpu->SetCpsr(0x10);cpu->ClearHalt();ticks=10000;cpu->Run();assert(ticks);
    }

};

int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream file(argv[1],std::ios::binary|std::ios::ate);assert(file);
    const auto size=file.tellg();file.seekg(0);file.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);
    assert(file && CodeMatches(e));
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    for(unsigned slot:{1u,2u}) {
        const unsigned bit=1u<<(slot-1),press=slot==1?0x3c:0x40,hold=slot==1?0x44:0x48;
        const U32 hold_pc=slot==1?0x42ec44:0x42ec78;
        e.Setup();e.Poll(bit);e.Run();
        assert(e.Read(Main+0x34)==slot+4 && e.Read(Main+press)==1 && e.Read(Main+hold)==1);
        assert(e.input.active==slot && !e.input.pending);
        // Aiming/charging follows the shoulder button even without a touch.
        e.Poll(bit);e.Run(hold_pc);
        assert(e.Read(Main+0x34)==slot+4 && e.Read(Main+press)==0 && e.Read(Main+hold)==1);
        e.Poll(0);e.Run(hold_pc);
        assert(e.Read(Main+0x34)==2 && e.Read(Main+hold)==0 && !e.input.active);
        // Short taps still publish one press, then release normally.
        e.Setup();e.Poll(bit);e.Poll(0);e.Run();assert(e.Read(Main+press)==1);
        e.Run(hold_pc);assert(e.Read(Main+0x34)==2 && e.Read(Main+hold)==0);
        // Native empty-item and disabled-slot restrictions remain authoritative.
        e.Setup();e.Put<std::uint8_t>(0x587958+(slot==1?0x83:0x84),255);
        e.Poll(bit);e.Run();assert(e.Read(Main+0x34)==2 && !e.input.active);
        e.Setup();e.Write(Main+0x68,slot==1?0x4000000:0x2000000);
        e.Poll(bit);e.Run();assert(e.Read(Main+0x34)==2 && !e.input.active);
        e.Setup();e.Poll(bit,false);e.Poll(bit);e.Run();assert(e.Read(Main+0x34)==2);
        e.Setup();e.Poll(bit,true,8);e.Run();assert(e.Read(Main+0x34)==2);
        e.Setup();e.Poll(bit);e.now+=150001;e.Run();assert(e.Read(Main+0x34)==2);
        e.Setup();e.Poll(bit);e.Write(Env::Play+0x7f10,0x10000);e.Run();assert(e.Read(Main+0x34)==2);
        // An editor/scene transition releases an already active item.
        e.Setup();e.Poll(bit);e.Run();e.Poll(bit,false);e.Run(hold_pc);
        assert(e.Read(Main+0x34)==2 && e.Read(Main+hold)==0);
    }
    // Native touch press/release still works without shoulder input.
    e.Setup();e.native_pressed=1;e.Run();assert(e.Read(Main+0x34)==6 && !e.input.active);
    e.native_pressed=0;e.Run(0x42ec78);assert(e.Read(Main+0x34)==2);
    e.Setup();e.Poll(3);e.Run();assert(e.Read(Main+0x34)==2);
    std::cout<<"Native gameplay I/II: press, hold, release, quick taps, empty/disabled slots, editor and transition gates, and original touch passed\n";
}
