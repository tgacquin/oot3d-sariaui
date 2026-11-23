// SPDX-License-Identifier: GPL-2.0-or-later
// Execute the actual OoT TALK handler and its camera math in Dynarmic.
// Actor focus/height and an open collision scene are fixtures, not a game run.
#include "oot3d/dialogue_camera.h"
#include "dynarmic/interface/A32/a32.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace OOT3D::DialogueCamera;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Play=0x800000, Cam=Play+0x364, Player=0x900000, Target=0x901000;
    static constexpr U32 Stop=0x700000, Stack=0xfff000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu{};
    Hook hook;
    std::uint64_t remaining{},frame{};
    float height=44;
    bool obstructed=false, fixed_zoom=true;
    static constexpr U32 SceneCamera=0x903000;
    unsigned collisions=0;
    float native_radius=0,requested_radius=0;
    template<class T> T Get(U32 a) {
        assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;
    }
    template<class T> void Put(U32 a,T v) {
        assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));
    }
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    Vec Vector(U32 a) {return {Get<float>(a),Get<float>(a+4),Get<float>(a+8)};}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==0x338c5c)return 0xef000006; // Fixed scene camera fixture.
        if(a==Stop)return 0xef000000;
        if(a==0x367ef0)return 0xef000001; // Actor height.
        if(a==0x338790)return 0xef000002; // Actor focus and rotation.
        if(a==0x3317ac)return 0xef000003; // Actor obstruction query.
        if(a==OOT3D::OrbitCamera::Collision)return 0xef000004;
        if(a==0x3375bc)return 0xef000005; // Unrelated camera interpolation bookkeeping.
        if(a==Sites[7].pc && !fixed_zoom)return Get<U32>(a);
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
    void InterpreterFallback(U32 pc,std::size_t) override {
        std::cerr<<"fallback "<<std::hex<<pc<<'\n';std::abort();
    }
    void ExceptionRaised(U32 pc,Dynarmic::A32::Exception) override {
        std::cerr<<"exception "<<std::hex<<pc<<'\n';std::abort();
    }
    void AddTicks(std::uint64_t n) override {remaining=n>remaining?0:remaining-n;}
    std::uint64_t GetTicksRemaining() override {return remaining;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();auto& fp=cpu->ExtRegs();
        if(!svc) {cpu->HaltExecution();return;}
        if(svc==FirstSVC+7) {hook.FixedIndoorZoom(*this,r,fp);return;}
        if(svc==FirstSVC+2) {hook.NativeEntry(*this,r,frame);return;}
        if(svc==FirstSVC+3) {hook.NativeParameters(*this,r,fp);return;}
        if(svc==FirstSVC+4) {hook.NativeLens(*this,r,fp);return;}
        if(svc==FirstSVC+5) {hook.NativeSide(*this,r,fp);return;}
        if(svc==FirstSVC+6) {
            native_radius=std::bit_cast<float>(fp[0]);
            hook.NativeRadius(*this,r,fp);
            requested_radius=Get<float>(r[13]+0x54);
            return;
        }
        if(svc==1)fp[0]=std::bit_cast<U32>(height);
        else if(svc==2) {
            const Vec focus{0,height,r[1]==Player?0.f:100.f};
            OOT3D::OrbitCamera::Put(*this,r[0],focus);
            Write(r[0]+12,0);Write(r[0]+16,0);
        } else if(svc==3)r[0]=0;
        else if(svc==4) {
            ++collisions;
            if(obstructed) {
                auto at=Vector(r[1]),eye=Vector(r[2]);
                for(unsigned i=0;i<3;++i)eye[i]=at[i]+.95f*(eye[i]-at[i]);
                OOT3D::OrbitCamera::Put(*this,r[2],eye);
            }
            r[0]=0;
        } else if(svc==6)r[0]=SceneCamera;
        else assert(svc==5);
        r[15]=r[14];
    }
    void Setup(bool modern,float side,float scale,float actor_height,unsigned setting=1,unsigned scene=1) {
        mm_style=modern;hook={};height=actor_height;frame=0;collisions=0;
        for(U32 a=Play;a<Play+0x1000;a+=4)Write(a,0);
        Write(0x5043e0,Play);Write(0x588e3c,0);Write(0x51b2f4,0x902000);
        Write(Play+0x104,scene);Put<std::uint8_t>(Play+0x361,1);
        Write(Cam+0xd4,Play);Write(Cam+0xd8,Player);Write(Cam+0xf0,Target);
        Write(Target+0x13c,1);Write(Cam+0x188,7|(setting<<16));Write(Cam+0x18c,0);
        Write(Player+0x1710,setting==6?0x818000:0);Write(Player+0x1714,0);
        if(setting==6)Write(Cam+0x18c,2); // Captured mounted lock-on before TALK.
        const Pose pose{{side*150*scale,height,-100*scale},{0,height,0},{0,1,0},52};
        OOT3D::OrbitCamera::Put(*this,Cam+0x80,pose.at);
        OOT3D::OrbitCamera::Put(*this,Cam+0x8c,pose.eye);
        OOT3D::OrbitCamera::Put(*this,Cam+0xa4,pose.eye);
        const U32 view=Play+0x188;
        Write(view,0);Write(view+4,400);Write(view+8,0);Write(view+12,240);
        Put<float>(view+0x10,25); // Undisplayed native lens during gameplay.
        OOT3D::OrbitCamera::Put(*this,view+0x30,pose.eye);
        OOT3D::OrbitCamera::Put(*this,view+0x3c,pose.at);
        OOT3D::OrbitCamera::Put(*this,view+0x48,pose.up);
        auto& r=cpu->Regs();r.fill(0);r[4]=view;r[13]=Stack;
        hook.Projection(*this,r,cpu->ExtRegs(),0,100000);
        Write(Cam+0x18c,3);Put<float>(Cam+0x144,25);
    }
    void Tick(U32 handler=0x23708c) {
        ++frame;auto& r=cpu->Regs();r[0]=Cam;r[13]=Stack;r[14]=Stop;r[15]=handler;
        cpu->SetCpsr(0x10);remaining=1000000;cpu->Run();
        assert(r[15]==Stop+4 && r[13]==Stack);
    }
};
float Yaw(const Vec& eye,const Vec& at) {return std::atan2(eye[0]-at[0],eye[2]-at[2])*180/3.14159265358979323846f;}
float Difference(float a,float b) {return std::remainder(a-b,360.f);}
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream f(argv[1],std::ios::binary);assert(f);
    f.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),0x600000);assert(f.gcount()>0);
    assert(CodeMatches(e));
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu(config);e.cpu=&cpu;
    for(unsigned setting:{1u,4u,6u,59u})
      for(float height:{44.f,68.f})for(float scale:{1.f,2.f})for(float side:{-1.f,1.f}) {
        e.Setup(true,side,scale,height,setting,setting==6?99:setting==1?1:73);
        const auto before=e.Vector(Env::Cam+0x8c);e.Tick();
        assert(e.hook.native_active && e.Get<std::uint16_t>(Env::Cam+0x4c)==30);
        assert(e.Vector(Env::Cam+0x8c)==before && e.Get<float>(Env::Cam+0x144)==52);
        std::vector<Vec> eyes{e.Vector(Env::Cam+0xa4)},aims{e.Vector(Env::Cam+0x80)};
        for(unsigned n=1;n<=30;++n) {
            e.Tick();eyes.push_back(e.Vector(Env::Cam+0xa4));aims.push_back(e.Vector(Env::Cam+0x80));
            assert(e.Get<std::uint16_t>(Env::Cam+0x4c)==30-n);
            // Lens and angular pan settle together, rather than halving the
            // lens residual each tick and finishing before the pan gets going.
            const float progress=float(n*(61-n))/930;
            assert(std::abs(e.Get<float>(Env::Cam+0x144)-
                (52+(NativeDialogueFov()-52)*progress))<.001f);
        }
        const float start=Yaw(eyes.front(),aims.front());
        const float turn=Difference(Yaw(eyes.back(),aims.back()),start);
        assert(turn*side>0); // Same target, opposite starting side: opposite turn.
        for(unsigned n=0;n<=30;++n) {
            const float progress=float(n*(61-n))/930;
            assert(std::abs(Difference(Yaw(eyes[n],aims[n]),start)-turn*progress)<.15f);
            for(unsigned axis=0;axis<3;++axis)
                assert(std::abs(aims[n][axis]-(aims[0][axis]+(aims[30][axis]-aims[0][axis])*n/30))<.001f);
        }
        assert(e.collisions>=31 && std::abs(e.Get<float>(Env::Cam+0x144)-NativeDialogueFov())<.001f);
        // Native destination includes MM's whole-radius 1.5 multiplier, not
        // just its table base of 80. Fixture actors are 100 units apart.
        const auto settled_eye=e.Vector(Env::Cam+0x8c),settled_at=e.Vector(Env::Cam+0x80);
        const float settled_radius=std::hypot(settled_eye[0]-settled_at[0],
            settled_eye[1]-settled_at[1],settled_eye[2]-settled_at[2]);
        assert(std::abs(e.requested_radius-e.native_radius*1.5f)<.001f);
        // KeepOn3 adds one unit each update in addition to the integrated
        // radius delta. Native spherical conversion has small rounding error.
        assert(std::abs(settled_radius-(e.requested_radius+30.f))<1.f);
        const auto stable=e.Vector(Env::Cam+0x8c);e.Tick();assert(e.Vector(Env::Cam+0x8c)==stable);
        // A supported TALK session publishes the endpoint used by the orbit's
        // existing smooth return. The capture had no dialogue_release in either
        // Fishing Hole profile because native_active never became true.
        OOT3D::OrbitCamera::Hook orbit;
        const U32 view=Env::Play+0x188;
        OOT3D::OrbitCamera::Put(e,view+0x30,stable);
        OOT3D::OrbitCamera::Put(e,view+0x3c,e.Vector(Env::Cam+0x80));
        e.Put<float>(view+0x10,e.Get<float>(Env::Cam+0x144));
        cpu.Regs()[4]=view;cpu.Regs()[13]=Env::Stack;
        e.hook.Projection(e,cpu.Regs(),cpu.ExtRegs(),e.frame,100000+e.frame*33333,&orbit);
        assert(orbit.dialogue.valid && orbit.dialogue.eye==stable && !e.hook.apply);
        e.Write(Env::Cam+0x18c,0);
        assert(orbit.DialogueMatches(e,Env::Cam,e.frame+1));
    }
    // The indoor table is shared elsewhere. Only the captured scene is added;
    // mutated tables, alternate handlers and subcameras still fail closed.
    for(unsigned setting:{4u,59u}) {
        e.Setup(true,1,1,44,setting,1);assert(!CommonContext(e,Env::Cam));
        e.Setup(true,1,1,44,setting,73);assert(CommonContext(e,Env::Cam));
        const U32 modes=e.Get<U32>(0x516ff0+setting*8+4),parameters=e.Get<U32>(modes+28);
        for(unsigned i=0;i<13;++i) {
            const U32 value=e.Get<U32>(parameters+4*i);
            e.Write(parameters+4*i,value^1);assert(!CommonContext(e,Env::Cam));
            e.Write(parameters+4*i,value);
        }
        const U32 handler=e.Get<U32>(modes+24);
        e.Write(modes+24,handler^1);assert(!CommonContext(e,Env::Cam));e.Write(modes+24,handler);
        e.Put<std::uint16_t>(Env::Play+0xa64,1);assert(!CommonContext(e,Env::Cam));
        e.Setup(false,1,1,44,setting,73);e.Tick();
        assert(!e.hook.native_active && e.Get<std::uint16_t>(Env::Cam+0x4c)==10);
        assert(e.Get<float>(Env::Cam+4)==70 && e.Get<float>(Env::Cam+0x20)==45);
    }
    e.Setup(true,1,1,68,6,99);
    e.Write(Env::Player+0x1710,0);assert(!NativeContext(e,Env::Cam));
    e.Setup(false,1,1,68,6,99);e.Tick();
    assert(!e.hook.native_active && e.Get<std::uint16_t>(Env::Cam+0x4c)==10);
    // Disabled mode retains OoT's original timer and parameters.
    e.Setup(false,1,1,44);e.Tick();
    assert(!e.hook.native_active && e.Get<std::uint16_t>(Env::Cam+0x4c)==10);
    assert(e.Get<float>(Env::Cam+4)==50 && e.Get<float>(Env::Cam+0x20)==45);
    assert(e.requested_radius==e.native_radius);
    for(unsigned i=0;i<10;++i)e.Tick();
    assert(e.Get<std::uint16_t>(Env::Cam+0x4c)==0);
    // Execute the fixed indoor TALK handler, including its original camera math.
    for(bool modern:{false,true})for(bool patched:{false,true})for(int lens:{4500,6000,7000,-1}) {
        e.fixed_zoom=patched;cpu.ClearCache();
        e.Setup(modern,1,1,44,26,38);
        for(unsigned i=0;i<20;i+=2)e.Put<std::int16_t>(Env::SceneCamera+i,0);
        e.Put<std::int16_t>(Env::SceneCamera,130);
        e.Put<std::int16_t>(Env::SceneCamera+2,32);
        e.Put<std::int16_t>(Env::SceneCamera+12,lens);
        const float base=lens==-1?60.f:lens*.01f;
        for(unsigned tick=0;tick<16;++tick) {
            e.Tick(0x236168);
            assert(std::abs(e.Get<float>(Env::Cam+12)-base*(patched?1.f:.7f))<.001f);
            assert((e.Vector(Env::Cam+0x8c)==Vec{130,32,0}));
            if(patched)assert(std::abs(e.Get<float>(Env::Cam+0x144)-base)<.001f);
        }
        assert(std::abs(e.Get<float>(Env::Cam+0x144)-base*(patched?1.f:.7f))<.01f);
    }
    // Wrong camera ownership, mode, setting, status and modified tables replay VLDR.
    e.fixed_zoom=true;cpu.ClearCache();
    for(unsigned guard=0;guard<10;++guard) {
        e.Setup(true,1,1,44,26,38);e.Put<float>(Env::Cam,.3f);
        const U32 address=guard<4?0x5188c4+guard*4:
            guard==4?0x519fe4+24:guard==5?0x519fe4+28:
            guard==6?Env::Cam+0x18c:guard==7?Env::Cam+0x188:
            guard==8?Env::Play+0xa64:0x516ff0+26*8+4;
        const auto saved=e.Get<U32>(address);e.Write(address,saved^1);
        cpu.Regs()[4]=Env::Cam;e.hook.FixedIndoorZoom(e,cpu.Regs(),cpu.ExtRegs());
        assert(std::bit_cast<float>(cpu.ExtRegs()[2])==.3f);e.Write(address,saved);
    }
    std::cout<<"Fixed indoor TALK: native ARM 60->42 baseline, patched scene lens retained, 45/60/70/default FOV, fixed eye, both dialogue settings and guard fallbacks passed\n";
    mm_style=true;
    std::cout<<"Native TALK: common, mounted lock-on and both Fishing Hole profiles, actual ARM handler, 30-update angular ease-out, both sides/ages, 1x/2x start continuity, lens handoff, collision calls, smooth-return endpoint, scene/table/handler/subcamera guards and legacy fallback passed\n";
}
