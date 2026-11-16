// SPDX-License-Identifier: GPL-2.0-or-later
// Execute native enqueue AND queue/model matrix preparation before drawing.
#include "oot3d/save_quit_backdrop.h"
#include "oot3d/pause_shadow.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace OOT3D;
using TitleScreen::U32;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Play=0x800000,SP=0xe00000,Model=0xa00000,Mesh=0xa01000,Identity=0xa02000,Stop=0xf00000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    SaveMenu::QuitBackdrop hook;
    bool supported=true,flushed=false;
    std::uint64_t remaining=0;
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return a+4<=bytes.size()?std::optional<U32>{Get<U32>(a)}:std::nullopt;}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        // Only GPU buffer uploads are stubbed; matrix building runs natively.
        if(a==Stop)return 0xef000000;
        if(a==0x3fb5dc)return 0xef000001;
        return PauseMenu::Instruction(a,Get<U32>(a));
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
    void AddTicks(std::uint64_t n) override {remaining=n>remaining?0:remaining-n;}
    std::uint64_t GetTicksRemaining() override {return remaining;}
    void CallSVC(U32 svc) override {
        if(svc==0) {flushed=true;cpu->HaltExecution();return;}
        if(svc==1) {cpu->Regs()[15]=cpu->Regs()[14];return;}
        assert(svc==PauseMenu::FirstSVC+21);
        U32 flags=cpu->Cpsr(),fpscr=cpu->Fpscr();
        const bool returning=hook.pending;
        const auto expected=hook.saved;const auto expected_fp=hook.saved_fp;
        const U32 expected_flags=hook.saved_flags,expected_fpscr=hook.saved_fpscr;
        hook.Handle(*this,cpu->Regs(),cpu->ExtRegs(),flags,fpscr,PauseMenu::Sites[21].pc,supported);
        cpu->SetCpsr(flags);cpu->SetFpscr(fpscr);
        if(returning)assert(cpu->Regs()==expected && cpu->ExtRegs()==expected_fp && flags==expected_flags && fpscr==expected_fpscr);
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;
    std::ifstream in(argv[1],std::ios::binary);assert(in);
    std::vector<char> code{std::istreambuf_iterator<char>(in),{}};
    std::memcpy(e.bytes.data()+0x100000,code.data(),code.size());
    assert(PauseMenu::CodeMatches(e) && HUDFade::CodeMatches(e));
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu{config};e.cpu=&cpu;
    const auto original=e.bytes;
    constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
    const auto matrix=[&](U32 at,const auto& values) {
        for(unsigned i=0;i<values.size();++i)e.Put(at+i*4,values[i]);
    };
    const auto run=[&] {
        cpu.Reset();cpu.SetCpsr(0xa0000010);cpu.SetFpscr(0x01000000);
        for(unsigned i=0;i<16;++i)cpu.Regs()[i]=0x12340000+i;
        for(unsigned i=0;i<64;++i)cpu.ExtRegs()[i]=std::bit_cast<U32>(float(i)+.25f);
        cpu.Regs()[0]=HUDFade::Queue;cpu.Regs()[1]=cpu.Regs()[2]=Env::Identity;
        cpu.Regs()[13]=Env::SP;cpu.Regs()[14]=Env::Stop;cpu.Regs()[15]=0x300620;
        const auto expected=cpu.Regs();e.flushed=false;e.remaining=100000;
        cpu.Run();assert(e.flushed && !e.hook.pending);
        assert(cpu.Regs()[15]==Env::Stop+4 && cpu.Regs()[13]==Env::SP);
        for(unsigned i=4;i<=11;++i)assert(cpu.Regs()[i]==expected[i]);
    };
    for(unsigned variant=0;variant<10;++variant)for(bool clock:{false,true})
    for(float native_alpha:{-1.f,0.f,.1f,.5f,.9f,1.f}) {
        e.bytes=original;e.hook={};e.supported=variant!=6;SaveMenu::animate=clock;
        e.Write(0x5043e0,Env::Play);e.Write(Env::Play+0x100,variant==5?0:0x203);
        e.Write(SaveMenu::Owner+0x28,variant==3?2:0);
        e.Write(SaveMenu::Owner+0x24,variant==4?14:16);
        e.Write(0x588e3c,variant==7?0:1);
        e.Write(Env::Play+0x5c2c,variant==8?0:20u<<8);
        e.Write(Env::Play+0x5c74,2u<<16);
        e.Write(Env::Play+0x7f10,(variant%3)<<16);
        e.Write(PauseMenu::UI+0x24,0);e.Write(PauseHeader::Items+0x20,0);
        assert(!PauseMenu::Active(e));
        // Start with the cached 320px bottom-screen matrix that caused the
        // visible hard edge. Changing rect/scale alone must not pass this test.
        matrix(Env::Model+0xc,std::array<float,12>{320,0,0,160,0,240,0,120,0,0,1,0});
        matrix(Env::Model+0x54,identity);matrix(Env::Identity,identity);
        e.Write(Env::Model,0x4ebd60);e.Write(Env::Model+4,Env::Mesh);
        e.Write(Env::Mesh,Env::Mesh+0x200);e.Write(Env::Mesh+0x128,1);
        for(unsigned pass=0;pass<7;++pass) {
            e.Write(HUDFade::Queue+0x14+4*pass,0);e.Write(HUDFade::Queue+0x754+4*pass,0);
        }
        const bool existing=native_alpha>=0;
        const U32 used=variant==9?24:unsigned(existing);
        e.Write(HUDFade::Queue+8,used);e.Write(HUDFade::Queue+0xf4,Env::Model);
        if(existing) {
            e.Write(HUDFade::Queue+0x24,1);e.Write(HUDFade::Queue+0x214+4*0x60,Env::Model);
            matrix(Env::Model+0x3c,std::array<float,6>{200,120,0,400,240,1});
            matrix(Env::Model+0xf0,std::array<float,4>{0,0,0,native_alpha});
        }
        run();
        const bool active=variant<3 || variant==9;
        const bool queued=!existing && variant<3;
        assert(e.Read(HUDFade::Queue+8)==used+unsigned(queued));
        assert(e.Read(HUDFade::Queue+0x20)==0); // No separate pause tint in pass 3.
        assert(e.Read(HUDFade::Queue+0x24)==unsigned(existing || queued));
        if(existing || queued) {
            assert(e.Read(HUDFade::Queue+0x214+4*0x60)==Env::Model);
            std::array<float,12> drawn{};assert(TitleScreen::ReadFloats(e,Env::Model+0xc,drawn));
            assert((drawn==std::array<float,12>{400,0,0,200,0,240,0,120,0,0,1,0}));
            // The matrix actually consumed by drawing covers both edges; it
            // also matches the renderer's existing widescreen fade path.
            std::array<Widescreen::Row,3> projected{};
            for(unsigned i=0;i<12;++i)projected[i/4][i%4]=drawn[i];
            projected[2][3]-=1; // Native top UI view's depth translation.
            constexpr std::array<Widescreen::Row,4> projection{{
                {0,-2.f/240,0,1},{-2.f/400,0,0,1},{0,0,1.f/3,-1.f/3},{0,0,0,1}}};
            constexpr std::array<Widescreen::Row,4> vertices{{
                {-.5f,-.5f,0,1},{.5f,-.5f,0,1},{-.5f,.5f,0,1},{.5f,.5f,0,1}}};
            assert(Widescreen::FadeGeometry(projection,projected,vertices));
            const float expected=active?.835f+.165f*std::max(0.f,native_alpha):native_alpha;
            assert(std::abs(e.Get<float>(Env::Model+0xfc)-expected)<.00001f);
            const float alpha=e.Get<float>(Env::Model+0xfc);
            run();assert(e.Get<float>(Env::Model+0xfc)==alpha); // No repeated darkening.
            if(active)assert(PauseShadow::Panels(e,PauseMenu::GetLayout()).count==0);
        }
    }
    std::cout<<"Native quit fade: stale 320px matrix replaced with full-width draw matrix, single pass-4 blackout, continuous alpha, repeat preparation, CPU and scope/capacity checks passed\n";
}
