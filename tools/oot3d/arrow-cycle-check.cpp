// SPDX-License-Identifier: GPL-2.0-or-later
// Native epilogue/call-return ABI, with animation/spawn (GPU/resources) stubbed.
#include "oot3d/arrow_cycle.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace OOT3D;
using namespace OOT3D::ArrowCycle;
struct Env : Dynarmic::A32::UserCallbacks {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;Hook hook;
    std::uint64_t ticks=0;std::int64_t now=1000000;
    unsigned nocks=0,raises=0;
    constexpr static U32 Play=0x900000,Player=0xa00000,Arrow=0xb00000,Stop=0xf00000,SP=0xe00000;
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop)return 0xef000000;
        if(a==Nock || a==RaiseBow)return 0xef000001;
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
    void InterpreterFallback(U32,std::size_t) override {std::abort();}
    void ExceptionRaised(U32,Dynarmic::A32::Exception) override {std::abort();}
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        if(!svc) {cpu->HaltExecution();return;}
        auto& r=cpu->Regs();auto& f=cpu->ExtRegs();
        if(svc==1) {
            assert(r[1]==Play && r[14]==Site.pc && r[13]==SP);
            if(r[15]-4==Nock) {assert(r[0]==Player);++nocks;}
            else {assert(r[0]==Player+0x1764 && r[2]==0x1b1);++raises;}
            // Native callees may clobber caller-saved GPRs/FPRs and flags.
            const U32 ret=r[14];r[0]=123;r[1]=456;r[12]=789;f[0]=0x12345678;
            cpu->SetCpsr(0x90000010);cpu->SetFpscr(0);r[15]=ret;return;
        }
        assert(svc==SVC);U32 flags=cpu->Cpsr(),fpflags=cpu->Fpscr();
        hook.Handle(*this,r,f,flags,fpflags,now);cpu->SetCpsr(flags);cpu->SetFpscr(fpflags);
    }
    void Setup(unsigned type=0) {
        hook={};input={};nocks=raises=0;ControlSwap::cycle_arrows=true;
        Write(PauseMenu::UI+0xc,Play);Write(PauseMenu::UI+0x14,0);Write(PauseMenu::UI+0x20,0);
        Write(Play+0x100,0x203);Write(Play+0x7f24,0x100);Write(Play+0x43c,Player);
        Write(Play+0x5c2c,0);Write(Play+0x7f10,0);Write(ItemUse::Main+0x34,2);
        Write(Player,0);Write(Player+0x1710,0);Write(Player+0x1224,0);
        SetByte(*this,Player+0x1a9,type+8);SetByte(*this,Player+0x1aa,type?0x37+type:3);
        SetByte(*this,Player+0x1ac,type+8);SetByte(*this,Save+0x8f,type?0x37+type:3);
        SetByte(*this,Save+0x90,4);SetByte(*this,Save+0x96,12);
        Write(Save+0x1580,0);Write(Save+0x1584,48);Write(Save+0x1588,20);SetByte(*this,Save+0x47,24);
        U32 buttons=0;input.Poll(*this,buttons,true,now);
    }
    void Press(U32 buttons) {input.Poll(*this,buttons,true,now);assert(!(buttons&0x300));}
    void Run() {
        auto& r=cpu->Regs();auto& f=cpu->ExtRegs();r.fill(0);f.fill(0);
        r[4]=Player;r[6]=Play;r[13]=SP;r[15]=Site.pc;r[12]=42;f[0]=0xabcdef;
        Write(SP+0x48,44);Write(SP+0x4c,55);Write(SP+0x50,66);Write(SP+0x54,77);Write(SP+0x58,88);Write(SP+0x5c,Stop);
        cpu->SetCpsr(0x60000010);cpu->SetFpscr(0);ticks=10000;cpu->ClearCache();cpu->Run();
        assert(r[15]==Stop+4 && r[13]==SP+0x60 && r[4]==44 && r[6]==66 && r[12]==42);
        assert(f[0]==0xabcdef && cpu->Cpsr()==0x60000010);
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream file(argv[1],std::ios::binary);assert(file);
    file.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),e.bytes.size()-0x100000);assert(CodeMatches(e));
    Dynarmic::A32::UserConfig config;config.callbacks=&e;Dynarmic::A32::Jit cpu{config};e.cpu=&cpu;
    for(unsigned current=0;current<3;++current)for(int direction:{-1,1}) {
        e.Setup(current);e.Press(direction<0?0x200:0x100);e.Run();
        const unsigned next=(int(current)+3+direction)%3;
        assert(Byte(e,Env::Player+0x1a9)==next+8 && Byte(e,Env::Player+0x1ac)==next+8);
        assert(Byte(e,Save+0x8f)==(next?0x37+next:3));assert(!e.nocks && !e.raises);
        e.Press(direction<0?0x200:0x100);e.Run();assert(Byte(e,Env::Player+0x1a9)==next+8);
    }
    e.Setup();SetByte(e,Save+0x90,255);e.Press(0x100);e.Run();assert(Byte(e,Env::Player+0x1a9)==10);
    e.Setup();SetByte(e,Save+0x90,255);SetByte(e,Save+0x96,255);e.Press(0x100);e.Run();assert(Byte(e,Env::Player+0x1a9)==8);
    for(unsigned type:{0u,1u,2u,3u})for(bool has_child:{false,true}) {
        e.Setup(type);e.Write(Env::Player+0x1224,Env::Arrow);e.Write(Env::Arrow,0x16);
        e.Write(Env::Arrow+0x1c,type+2);e.Write(Env::Arrow+0x128,has_child?0xb10000:0);
        e.Write(Env::Arrow+0x13c,0x1234);e.Write(Env::Arrow+0x140,0x5678);
        e.Write(Save+0x1580,type?1:0);e.Press(0x100);e.Run();
        assert(e.Read(Env::Arrow+0x13c)==0 && e.Read(Env::Arrow+0x140)==0 && e.raises==1 && !e.nocks);
        assert(Byte(e,Save+0x47)==(type==3?28u:24u));
        if(type && has_child) {
            const auto refunded=Byte(e,Save+0x47);e.Press(0);e.Press(0x200);e.Run();
            assert(Byte(e,Save+0x47)==refunded && e.raises==1); // Never refund an already-killed arrow twice.
        }
        e.Write(Save+0x1580,0);e.now+=33333;e.Run();assert(e.nocks==1);e.Run();assert(e.nocks==1);
    }
    e.Setup();e.Press(0x300);e.Run();assert(Byte(e,Env::Player+0x1a9)==8);
    e.Setup();e.Press(0x100);e.now+=150001;e.Run();assert(Byte(e,Env::Player+0x1a9)==8);
    e.Setup();e.Write(Env::Player+0x1710,1u<<23);U32 b=0x200;input.Poll(e,b,true,e.now);assert(b==0x200 && !input.pending);
    e.Setup();e.Press(0x200);SetByte(e,Env::Player+0x1a9,0);b=0x200;input.Poll(e,b,true,e.now);assert(b==0);
    e.Setup();ControlSwap::cycle_arrows=false;b=0x100;input.Poll(e,b,true,e.now);e.Run();assert(Byte(e,Env::Player+0x1a9)==8);
    std::cout<<"Arrow cycling: native call/return ABI and teardown, both directions, ownership, input edges, magic refund, delayed nock and exclusion guards passed\n";
}
