// SPDX-License-Identifier: GPL-2.0-or-later
// Execute the ROM's minimap toggle in Dynarmic, capturing its audio request.
// Link the built dynarmic, mcl and fmt libraries; argument: decompressed code.
#include "oot3d/minimap_control.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace OOT3D::MinimapControl;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 SP=0xe00000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    std::vector<U32> sounds;
    bool gameplay=true,pause_at_audio=false;
    std::uint64_t ticks=0;
    InstrumentFade fade;
    bool fade_test=false;
    unsigned map_draws=0;
    std::int64_t now=1000000;
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(fade_test && a==0xe10000)return 0xef000002; // Caller return.
        if(fade_test && a==0xe10004)return 0xef000003; // Native model draw.
        if(a==Resume)return 0xef000000;
        if(a==0x37547c)return 0xef000001;
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
        auto& r=cpu->Regs();
        if(svc==0) {cpu->HaltExecution();return;}
        if(svc==1) {
            sounds.push_back(r[0]);
            assert(r[1]==0 && r[2]==4 && r[3]==0x54ac20);
            assert(Get<U32>(r[13])==0x54ac20 && Get<U32>(r[13]+4)==0x54ac24);
            assert(Get<U32>(Action+0x68)==(shown.load()?1:0));
            r[0]=r[1]=r[2]=r[3]=r[12]=0xdeadbeef;r[15]=r[14];
            if(pause_at_audio)cpu->HaltExecution();
            return;
        }
        if(fade_test && svc==2) {cpu->HaltExecution();return;}
        if(fade_test && svc==3) {++map_draws;r[15]=r[14];return;}
        if(svc>FirstSVC && svc<=FirstSVC+3) {
            fade.Handle(*this,r,svc-FirstSVC,false,now);return;
        }
        assert(svc==FirstSVC);
        // Intentionally recreate the hook on every call, like production.
        ToggleHook{}.Handle(*this,r,gameplay);
    }
    std::array<U32,16> Begin() {
        cpu->Reset();auto& r=cpu->Regs();
        for(unsigned i=0;i<13;++i)r[i]=0x100+i;
        r[13]=SP;r[14]=0x123456;r[15]=Site.pc;
        return r;
    }
    void Run() {ticks=10000;cpu->Run();assert(ticks);}
    void Check(const std::array<U32,16>& saved) {
        auto expected=saved;expected[15]=Resume+4;assert(cpu->Regs()==expected);
        assert(Get<U32>(Action+0x68)==1);
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream f(argv[1],std::ios::binary|std::ios::ate);assert(f);
    const auto size=f.tellg();assert(size>0 && size<0xb00000);f.seekg(0);
    f.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);assert(f);
    assert(CodeMatches(e));
    e.Write(Site.pc,*e.Read(Site.pc)^1);assert(!CodeMatches(e));e.Write(Site.pc,Site.instruction);
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    Reset();e.Write(Action+0x68,1);e.Write(Owner+8,0x800000);
    auto saved=e.Begin();RequestToggle();e.Run();e.Check(saved);
    assert(!shown && e.sounds==std::vector<U32>{0x01000499});
    saved=e.Begin();e.Run();e.Check(saved);assert(e.sounds.size()==1); // No repeat.
    saved=e.Begin();RequestToggle();e.Run();e.Check(saved);
    assert(shown && e.sounds==std::vector<U32>({0x01000499,0x01000498}));
    for(bool available:{false,true}) {
        e.gameplay=!available;e.Write(Owner+8,available?0x800000:0);
        saved=e.Begin();RequestToggle();e.Run();e.Check(saved);
        assert(shown && requests.load()==0 && e.sounds.size()==2);
    }
    e.gameplay=true;e.Write(Owner+8,0x800000);
    saved=e.Begin();RequestToggle();RequestToggle();e.Run();e.Check(saved);
    assert(shown && e.sounds.size()==2);
    // Resume with only saved guest memory/registers, even if gameplay changes
    // during the audio call. Return frames must restore SP and every register.
    saved=e.Begin();e.pause_at_audio=true;RequestToggle();e.Run();
    assert(cpu.Regs()[13]<Env::SP && e.sounds.size()==3);
    const auto registers=cpu.Regs();const auto cpsr=cpu.Cpsr();
    cpu.Reset();cpu.Regs()=registers;cpu.SetCpsr(cpsr);e.gameplay=false;
    e.Run();e.Check(saved);assert(e.sounds.size()==3 && !shown);
    Reset();
    // Execute the REAL HUD entry and its early-return branch. Previously
    // tracking at 42bb04 never ran here, so the map vanished without a fade.
    e.fade_test=true;e.pause_at_audio=false;
    constexpr U32 play=0x900000,map=0x800000,vtable=0x810000;
    e.Write(OOT3D::HUDFade::UI+0xc,play);e.Write(OOT3D::HUDFade::UI+0x1c,1);
    e.Write(play+0x100,0x203);e.Write(play+0x104,1);e.Write(play+0x20ac,0x920000);
    e.Write(play+0x5c2c,0);e.Write(play+0x7f10,0);e.Write(play+0x7f24,0);
    e.Write(OOT3D::HUDFade::Main+0x34,0x17);
    e.Write(OOT3D::SaveMenu::Owner+0x28,0);e.Write(OOT3D::Ocarina::Owner+0x14,0);
    e.Write(Owner+8,map);e.Write(Owner+0x60,0);e.Write(Owner+0x20,0);e.Write(Owner+0x28,0);
    e.Write(map,vtable);e.Write(vtable+0xc,0xe10004);
    e.Write(0x588958+0x4e4,0);e.Write(Action+0x58,1);e.Write(Action+0x68,1);
    std::array<U32,16> fixture{};fixture[0]=play;fixture[4]=Owner;
    e.fade.Handle(e,fixture,3,false,e.now);e.fade.Handle(e,fixture,2,false,e.now);
    e.Write(OOT3D::Ocarina::Owner+0x14,12);
    for(unsigned frame=0;frame<12;++frame) {
        e.now+=16667;cpu.Reset();auto& r=cpu.Regs();
        for(unsigned i=0;i<13;++i)r[i]=0x100+i;
        r[0]=play;r[13]=Env::SP;r[14]=0xe10000;r[15]=0x42b9d0;
        const auto before=r;const auto draws=e.map_draws;
        e.Run();
        assert(r[13]==Env::SP && r[15]==0xe10004);
        for(unsigned i=4;i<=10;++i)assert(r[i]==before[i]);
        assert(e.map_draws==draws+(e.fade.opacity>0?1:0));
    }
    assert(e.map_draws>0 && e.fade.opacity==0);
    std::cout<<"Native HUD early-return path preserves map fade, caller frame and callee registers\n";
    Reset();std::cout<<"Native minimap hide/show sounds, gating, no repeat and guest return checks passed\n";
}
