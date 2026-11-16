// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise native damage, collectible drop, fairy lifecycle and revival paths.
#include "oot3d/difficulty_hooks.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
using namespace OOT3D;
using namespace OOT3D::Difficulty;
struct Env : Dynarmic::A32::UserCallbacks {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    std::uint64_t ticks=0;std::int64_t now=1000000;
    unsigned spawns=0,callbacks=0;
    constexpr static U32 Play=0x8100000,Player=0x8200000,Stop=0xf00000,SP=0xe00000;
    template<class T> T Get(U32 a) {if(a>=0x8000000)a-=0x7800000;assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {if(a>=0x8000000)a-=0x7800000;assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop || a==0x2523e8 || a==0x4968c8 || a==0x4962b0 || a==0x34af4c || a==0x252418)return 0xef000000;
        for(unsigned i=1;i<=4;++i)if(a==Sites[i].pc+8)return 0xef000000;
        if(a==0x3738d0 || a==0x37547c || a==0xe10000)return 0xef000001;
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
        if(svc==0) {cpu->HaltExecution();return;}
        if(svc==1) {
            const auto pc=r[15]-4;
            if(pc==0x3738d0){++spawns;r[0]=Player;}
            if(pc==0xe10000)++callbacks;
            r[15]=r[14];return;
        }
        Handle(*this,r,svc-FirstSVC);
    }
    void Setup() {
        cpu->Reset();spawns=callbacks=0;
        damage_multiplier=1;disable_hearts=disable_fairies=disable_rupees=false;
        weaker_defense=disable_magic=disable_ammo=disable_fairy_drops=lose_rupees_on_death=false;
        red_potion_hearts=blue_potion_hearts=bottled_fairy_hearts=20;
        Write(0x587958+0x40,320u<<16);Write(0x587958+0x44,160);
        Put<std::uint8_t>(0x587958+0xe,0);Put<std::uint8_t>(0x587958+0x51,0);
    }
    void Run(U32 pc,U32 flags=0x10) {
        cpu->Regs()[13]=SP;cpu->Regs()[14]=Stop;cpu->Regs()[15]=pc;
        cpu->SetCpsr(flags);cpu->ClearHalt();ticks=10000;cpu->Run();assert(ticks);
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream file(argv[1],std::ios::binary|std::ios::ate);assert(file);
    const auto size=file.tellg();file.seekg(0);file.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);
    assert(file && CodeMatches(e));
    for(const auto& site:Sites) {
        e.Write(site.pc,site.instruction^1);assert(!CodeMatches(e));
        assert(Instruction(site.pc,site.instruction^1)==(site.instruction^1));
        e.Write(site.pc,site.instruction);
    }
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    for(int multiplier:{1,2,3,4,16})for(bool mq:{false,true})for(bool defense:{false,true})for(bool weak:{false,true}) {
        e.Setup();damage_multiplier=multiplier;weaker_defense=weak;
        e.Put<std::uint8_t>(0x587958+0xe,mq);e.Put<std::uint8_t>(0x587958+0x51,defense);
        cpu.Regs()[1]=U32(-16);e.Run(0x352dbc);
        assert(e.Get<std::uint8_t>(0x587958+0xe)==mq);
        const int hit=16; // MQ uses the same configurable multiplier as the normal quest.
        const int base=defense?(weak?(hit*2+1)/3:hit/2):hit;
        const int expected=std::max(0,160-int(std::ceil(base*multiplier)));
        assert(e.Get<std::int16_t>(0x587958+0x44)==expected);
        e.Write(0x587958+0x44,160);cpu.Regs()[1]=32;e.Run(0x352dbc);
        assert(e.Get<std::int16_t>(0x587958+0x44)==192); // Healing unchanged.
    }
    e.Setup();damage_multiplier=16;assert(Damage(INT32_MIN)==-32768);
    assert(Multiplier(std::numeric_limits<float>::quiet_NaN())==1);
    assert(Multiplier(.25f)==1 && Multiplier(2.6f)==3 && Multiplier(200)==16);
    damage_multiplier=1;assert(Damage(-1)==-1);
    // Every drop site: category toggles are independent; metadata bits do not
    // hide a heart piece, container, key or ammunition. Native null handling.
    for(unsigned site=1;site<=4;++site)for(unsigned flags=0;flags<16;++flags)
        for(unsigned type=0;type<=0x19;++type) {
            e.Setup();disable_hearts=flags&1;disable_rupees=flags&2;disable_magic=flags&4;disable_ammo=flags&8;
            const bool blocked=(type==3 && (flags&1)) ||
                ((type<=2 || type==0x13 || type==0x14) && (flags&2)) ||
                ((type==0xe || type==0xf) && (flags&4)) ||
                ((type==4 || type==5 || (type>=8 && type<=0xd) || type==0x10) && (flags&8));
            e.Write(Env::SP+8,type|0x8500);cpu.Regs()[2]=0x15;e.Run(Sites[site].pc);
            assert(e.spawns==(blocked?0u:1u));
            assert(cpu.Regs()[4]==(blocked?0u:Env::Player));
        }
    for(unsigned type=0;type<8;++type)for(bool disabled:{false,true})for(bool drops:{false,true}) {
        e.Setup();disable_fairies=disabled;disable_fairy_drops=drops;
        e.Write(Env::Player+0x1c,type);e.Write(Env::Player+0x9cc,0xe10000);
        e.Write(Env::Player+0x13c,0x1d732c);e.Write(Env::Player+0x140,0x1d6ec4);
        e.Write(Env::Player+4,0x31);cpu.Regs()[0]=Env::Player;e.Run(0x1d732c);
        const bool blocked=(disabled && (type==2 || type==6 || type==7)) || (drops && type==2);
        assert(e.callbacks==(blocked?0u:1u));
        assert(e.Read(Env::Player+0x140)==(blocked?0u:0x1d6ec4u));
        assert(e.Read(Env::Player+0x13c)==(blocked?0u:0x1d732cu));
        assert(e.Read(Env::Player+4)==(blocked?0x30u:0x31u));
    }
    for(bool disabled:{false,true}) {
        e.Setup();disable_fairies=disabled;
        e.Write(0x587958+0x80,0xffffffff);e.Write(0x587958+0x84,0xffffffff);
        e.Put<std::uint8_t>(0x587958+0x9e,0x18);
        for(unsigned i=1;i<4;++i)e.Put<std::uint8_t>(0x587958+0x9e + i,0x14);
        const auto before=e.bytes;
        e.Run(0x2523e4);
        assert(cpu.Regs()[0]==(disabled?0u:1u));
        assert(e.Get<std::uint8_t>(0x587958+0x9e)==(disabled?0x18:0x14));
        if(disabled)for(U32 a=0x587958;a<0x587b00;++a)assert(e.bytes[a]==before[a]);
        cpu.Regs()[1]=0x588e58;cpu.Regs()[5]=Env::Play;e.Run(0x4968b4);
        assert(e.Get<std::uint16_t>(0x588e58+0xb2)==(disabled?0:0x140));
    }
    // Potion changes preserve the NE condition and native halfword store.
    for(unsigned action:{0x25u,0x26u,0x27u})for(int hearts:{1,7,20})for(bool applies:{false,true}) {
        e.Setup();red_potion_hearts=hearts;blue_potion_hearts=hearts;
        e.Write(Env::Player+0x1ac,0xaabbcc00|action);
        e.Write(Env::Play+0xb0,0x1234abcd);
        cpu.Regs()[0]=0x99;cpu.Regs()[4]=Env::Player;cpu.Regs()[5]=Env::Play;
        e.Run(0x4962a8,applies?0x10:0x40000010);
        assert(e.Get<std::uint16_t>(Env::Play+0xb0)==0xabcd);
        assert(e.Get<std::uint16_t>(Env::Play+0xb2)==(applies?(action==0x27?320:hearts*16):0x1234));
    }
    for(int hearts:{1,7,20}) {
        e.Setup();bottled_fairy_hearts=hearts;
        e.Run(0x34af48);assert(cpu.Regs()[0]==U32(hearts*16));
        cpu.Regs()[1]=0x588e58;cpu.Regs()[5]=Env::Play;e.Run(0x4968b4);
        assert(e.Get<std::uint16_t>(0x588e58+0xb2)==hearts*16);
    }
    for(bool enabled:{false,true})for(unsigned health:{0u,16u}) {
        e.Setup();lose_rupees_on_death=enabled;
        e.Write(0x58799c,health);e.Write(0x5879a0,0xabcd012c);e.Write(0x588eb4,0xface000a);
        e.Write(Env::Play+0x8c,0x1234ffff);cpu.Regs()[0]=1;cpu.Regs()[1]=Env::Play;
        e.Run(0x252414);
        assert(e.Read(Env::Play+0x8c)==0x12340001);
        assert(e.Read(0x5879a0)==(enabled && !health?0xabcd0000u:0xabcd012cu));
        assert(e.Read(0x588eb4)==(enabled && !health?0xface0000u:0xface000au));
    }
    std::cout<<"Native difficulty checks passed: damage/MQ/defense/healing, all four drop paths, protected items, fairy cleanup, bottled healing and revive without inventory loss\n";
}
