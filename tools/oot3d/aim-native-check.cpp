// SPDX-License-Identifier: GPL-2.0-or-later
// Execute the native view-only refresh, including its look-at math and epilogue.
#include "oot3d/interpolation.h"
#include "oot3d/interpolation_replay.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace OOT3D;
using namespace OOT3D::Interpolation;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Play=0x900000,View=Play+0x188,Stack=0xe00000,Stop=0xf00000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu{};
    Hook hook;
    bool patched=true;
    unsigned captures=0;
    std::uint64_t ticks{};
    std::int64_t now=100000;
    template<class T> T Get(U32 a) {
        assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;
    }
    template<class T> void Put(U32 a,T v) {
        assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));
    }
    std::optional<U32> Read(U32 a) {if(a+4>bytes.size())return {};return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop)return 0xef000000;
        return patched?Instruction(a,Get<U32>(a)):Get<U32>(a);
    }
    std::uint8_t MemoryRead8(U32 a) override {return Get<std::uint8_t>(a);}
    std::uint16_t MemoryRead16(U32 a) override {return Get<std::uint16_t>(a);}
    U32 MemoryRead32(U32 a) override {return Get<U32>(a);}
    std::uint64_t MemoryRead64(U32 a) override {return Get<std::uint64_t>(a);}
    void MemoryWrite8(U32 a,std::uint8_t v) override {Put(a,v);}
    void MemoryWrite16(U32 a,std::uint16_t v) override {Put(a,v);}
    void MemoryWrite32(U32 a,U32 v) override {Put(a,v);}
    void MemoryWrite64(U32 a,std::uint64_t v) override {Put(a,v);}
    void InterpreterFallback(U32 pc,std::size_t) override {
        std::cerr<<"fallback "<<std::hex<<pc<<'\n';std::abort();
    }
    void ExceptionRaised(U32 pc,Dynarmic::A32::Exception) override {
        std::cerr<<"exception "<<std::hex<<pc<<'\n';std::abort();
    }
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        if(!svc){cpu->HaltExecution();return;}
        assert(svc==RefreshSVC);++captures;
        hook.Capture(*this,cpu->Regs(),now,true);
    }
    void Setup(const Matrix& camera,const Matrix& lens) {
        Write(PauseMenu::UI+0xc,Play);Write(PauseMenu::UI+0x24,0);
        Write(Play+0x100,0x0203);Write(Play+0x104,1);Write(SaveMenu::Owner+0x28,0);
        Write(View,0);Write(View+4,400);Write(View+8,0);Write(View+12,240);
        // The renderer singleton is already constructed during gameplay.
        Write(Get<U32>(0x45942c),1);
        assert(Get<U32>(0x459430)==0x5be5b8);
        for(unsigned i=0;i<3;++i) {
            Put<float>(View+0x30+4*i,float(camera[i][3]));
            Put<float>(View+0x3c+4*i,float(camera[i][3]-100*camera[i][2]));
            Put<float>(View+0x48+4*i,float(camera[i][1]));
        }
        for(unsigned l=0;l<3;++l)for(unsigned c=0;c<4;++c) {
            // Undo the renderer's screen rotation for the native View storage.
            Put<float>(View+0x54+64*l+4*c,float(-lens[1][c]));
            Put<float>(View+0x64+64*l+4*c,float(lens[0][c]));
            Put<float>(View+0x74+64*l+4*c,float(lens[2][c]));
            Put<float>(View+0x84+64*l+4*c,float(lens[3][c]));
        }
    }
    void Run() {
        auto& r=cpu->Regs();for(unsigned i=0;i<16;++i)r[i]=0xcafe0000+i;
        r[0]=View;r[13]=Stack;r[14]=Stop;r[15]=0x4592d0;
        cpu->ExtRegs().fill(0);cpu->SetCpsr(0x10);cpu->SetFpscr(0);
        cpu->ClearHalt();ticks=100000;cpu->Run();
        assert(ticks && r[15]==Stop+4 && r[13]==Stack && r[0]==1);
        for(unsigned i=4;i<=11;++i)assert(r[i]==0xcafe0000+i);
    }
};
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);Env e;
    std::ifstream code(argv[1],std::ios::binary);assert(code);
    code.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),0x600000);assert(code.gcount()>0);
    assert(CodeMatches(e));
    assert(Instruction(RefreshSite.pc,RefreshSite.instruction)==(0xef000000|RefreshSVC));
    assert(Instruction(RefreshSite.pc,0)==0);
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu(config);e.cpu=&cpu;
    enabled=true;present_60=true;generation=1;
    Matrix lens{};lens[0][1]=1.7;lens[1][0]=-1.02;lens[2][2]=1.001;lens[2][3]=7;lens[3][2]=-1;
    auto camera=Math::Identity();camera[0][3]=-450;camera[1][3]=90;camera[2][3]=-277;
    e.Setup(camera,lens);
    const auto before=e.bytes;
    e.patched=false;e.Run();
    const auto native=e.bytes;const auto native_regs=cpu.Regs();const auto native_fp=cpu.ExtRegs();
    const auto native_flags=cpu.Cpsr();
    e.bytes=before;e.patched=true;cpu.ClearCache();e.Run();
    assert(e.captures==1 && e.bytes==native && cpu.Regs()==native_regs &&
           cpu.ExtRegs()==native_fp && cpu.Cpsr()==native_flags);
    assert(CameraMatches(camera,CameraSnapshot()));
    assert(CameraSnapshot().projections_valid && CameraSnapshot().attached_valid);
    // A refreshed view must be available before drawing, even when the full
    // view hook last published the previous frame. Retain strict rejection.
    ReplayCamera replay;
    assert(!replay.Observe(0,CameraSnapshot(),camera,lens,e.now));
    for(unsigned frame=1;frame<=30;++frame) {
        const auto previous=CameraSnapshot();camera[0][3]+=2;e.now+=33333;
        assert(!CameraMatches(camera,previous));
        e.Setup(camera,lens);e.Run();
        assert(CameraMatches(camera,CameraSnapshot()));
        assert(replay.Observe(frame,CameraSnapshot(),camera,lens,e.now));
        auto wrong=camera;wrong[0][3]+=1;assert(!CameraMatches(wrong,CameraSnapshot()));
    }
    // Optionally replay an extracted capture fixture: frame, mode, native view,
    // draw camera, draw projection. No ROM or user capture is stored in the repo.
    unsigned checked=0,aiming=0,paired=0,mismatched=0;
    if(argc==3) {
        std::ifstream fixture(argv[2]);assert(fixture);std::uint64_t frame;unsigned mode;
        replay.Reset();++generation;
        while(fixture>>frame>>mode) {
            Matrix view{},draw{},projection{};
            for(auto* matrix:{&view,&draw,&projection})for(auto& row:*matrix)for(auto& value:row)fixture>>value;
            assert(fixture);
            const auto inverse=Math::InverseCamera(view);assert(inverse);
            CameraSample reference;reference.view=view;reference.valid=true;
            const bool expected_match=CameraMatches(draw,reference);
            // A mode-exit frame can still contain the preceding mode's draws.
            // Preserve that mismatch rather than relaxing the camera guard.
            if(mode==6 || mode==11)assert(expected_match);
            e.now=100000+std::int64_t(frame)*33333;e.Setup(*inverse,projection);e.Run();
            const auto sample=CameraSnapshot();
            if(CameraMatches(draw,sample)!=expected_match) {
                std::cerr<<"Unexpected camera match result at capture frame "<<frame<<" mode "<<mode<<'\n';std::abort();
            }
            const auto midpoint=replay.Observe(frame,sample,draw,projection,e.now);
            if(!expected_match) {assert(!midpoint && replay.failure==ReplayFailure::CameraMismatch);++mismatched;}
            else if(midpoint)++paired;
            else assert(replay.failure==ReplayFailure::History || replay.failure==ReplayFailure::CameraCut ||
                        replay.failure==ReplayFailure::LensCut);
            ++checked;if(mode==6 || mode==11)++aiming;
        }
    }
    std::cout<<"Native view refresh: identical ARM registers/flags/memory, retained lenses, current aiming sample, strict mismatch checks passed";
    if(checked)std::cout<<"; capture fixtures "<<checked<<", aiming "<<aiming<<", midpoints "<<paired
                        <<", preserved transition mismatches "<<mismatched;
    std::cout<<'\n';
}
