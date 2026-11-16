// SPDX-License-Identifier: GPL-2.0-or-later
// Runs native Camera_BGCheckInfo; only the scene intersection/floor queries
// are replaced with deterministic planes. No game window or save is needed.
#include "oot3d/orbit_camera.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace OOT3D::CameraClearance;
struct Plane {Vec normal;float d;};
struct Env: Dynarmic::A32::UserCallbacks {
    static constexpr U32 Play=0x810000,Cam=Play+0x364,Player=0x820000,Stack=0xe00000,Stop=0xf00000,Polys=0x900000;
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x1000000);
    std::vector<Plane> planes;
    Vec last_native_eye{};
    Dynarmic::A32::Jit* cpu{};std::uint64_t ticks{};unsigned queries{};
    template<class T> T Get(U32 at) {assert(std::uint64_t(at)+sizeof(T)<=bytes.size());T v;std::memcpy(&v,bytes.data()+at,sizeof(T));return v;}
    template<class T> void Set(U32 at,T v) {assert(std::uint64_t(at)+sizeof(T)<=bytes.size());std::memcpy(bytes.data()+at,&v,sizeof(T));}
    std::optional<U32> Read(U32 at) {if(std::uint64_t(at)+4>bytes.size())return {};return Get<U32>(at);}
    void Write(U32 at,U32 value) {Set(at,value);}
    Vec V(U32 at) {return {Get<float>(at),Get<float>(at+4),Get<float>(at+8)};}
    std::optional<U32> MemoryReadCode(U32 at) override {
        if(at==Stop || at==OOT3D::OrbitCamera::Sites[1].pc)return 0xef000000;
        if(at==0x3723c0)return 0xef000001;
        if(at==0x372300)return 0xef000002;
        return Get<U32>(at);
    }
    std::uint8_t MemoryRead8(U32 a) override{return Get<std::uint8_t>(a);}
    std::uint16_t MemoryRead16(U32 a) override{return Get<std::uint16_t>(a);}
    U32 MemoryRead32(U32 a) override{return Get<U32>(a);}
    std::uint64_t MemoryRead64(U32 a) override{return Get<std::uint64_t>(a);}
    void MemoryWrite8(U32 a,std::uint8_t v) override{Set(a,v);}
    void MemoryWrite16(U32 a,std::uint16_t v) override{Set(a,v);}
    void MemoryWrite32(U32 a,U32 v) override{Set(a,v);}
    void MemoryWrite64(U32 a,std::uint64_t v) override{Set(a,v);}
    void InterpreterFallback(U32 at,std::size_t) override{std::cerr<<"fallback "<<std::hex<<at<<'\n';std::abort();}
    void ExceptionRaised(U32 at,Dynarmic::A32::Exception) override{std::cerr<<"exception "<<std::hex<<at<<'\n';std::abort();}
    void AddTicks(std::uint64_t n) override{ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override{return ticks;}
    void CallSVC(U32 svc) override {
        if(!svc){cpu->HaltExecution();return;}
        auto& r=cpu->Regs();const U32 return_to=r[14];
        if(svc==1) {
            ++queries;const auto from=V(r[1]),to=V(r[2]),delta=Add(to,Mul(from,-1));
            float closest=2;int hit=-1;
            for(unsigned i=0;i<planes.size();++i) {
                const auto& p=planes[i];const float start=Dot(p.normal,from)-p.d,slope=Dot(p.normal,delta);
                if(start<0 || slope>=-.00001f)continue;
                const float t=-start/slope;
                if(t>=0 && t<=1 && t<closest){closest=t;hit=int(i);}
            }
            if(hit>=0) {
                Put(*this,r[3],Add(from,Mul(delta,closest)));
                Write(Get<U32>(r[13]),Polys+hit*16);Write(Get<U32>(r[13]+20),0);
            }
            r[0]=hit>=0;
        } else {
            assert(svc==2);const Vec pos=V(r[3]);float floor=-32000;int hit=-1;
            for(unsigned i=0;i<planes.size();++i) {
                const auto& p=planes[i];if(p.normal[1]<=.01f)continue;
                const float y=(p.d-p.normal[0]*pos[0]-p.normal[2]*pos[2])/p.normal[1];
                if(y<=pos[1] && y>floor){floor=y;hit=int(i);}
            }
            if(hit>=0){Write(r[1],Polys+hit*16);Write(r[2],0);}
            cpu->ExtRegs()[0]=std::bit_cast<U32>(floor);
        }
        r[15]=return_to;
    }
    void Run() {ticks=100000;cpu->ClearHalt();cpu->Run();assert(ticks);}
    void Integrated(bool restore) {
        namespace Orbit=OOT3D::OrbitCamera;
        planes={{{0,1,0},0}};
        Set<std::int16_t>(Polys+10,0);Set<std::int16_t>(Polys+12,32767);Set<std::int16_t>(Polys+14,0);
        Write(Cam+0xd4,Play);Write(Cam+0xd8,Player);Write(0x5043e0,Play);
        Write(Play+0xa64,0);Write(Play+0x104,0x55);Write(Cam+0x188,7|(1<<16));
        Write(Cam+0x18c,0);Write(Player+0x1710,0);Write(Player+0x1714,0);
        Write(0x588e3c,0);Write(0x58795c,1);
        Put(*this,Player+0x28,{0,12,0});Put(*this,Cam+0x80,{0,50,0});Put(*this,Cam+0x8c,{0,-25,-150});
        Set<float>(Play+0x188+0x14,10);
        Orbit::enabled=true;Orbit::input_allowed=true;Orbit::stick_x=.3f;Orbit::stick_y=0;
        Orbit::Hook hook;
        cpu->Reset();auto& r=cpu->Regs();r.fill(0);r[0]=r[4]=Cam;r[1]=0x123400;r[13]=Stack;
        U32 flags=0x60000010,fpscr=0x03000000;
        hook.Begin(*this,r);
        assert(Read(r[13]+0x38)==Orbit::Hook::NativeCookie);
        // The mode dispatcher returns here; the actual native collision
        // routines then execute for the center and all four corner rays.
        r[0]=1;hook.Finish(*this,r,&cpu->ExtRegs(),&flags,&fpscr);
        assert(r[15]==NativeCollision && hook.active);
        const float pitch=hook.pitch,yaw=hook.yaw,radius=hook.radius,velocity=hook.yaw_velocity;
        cpu->SetCpsr(flags);cpu->SetFpscr(fpscr);Run();
        const auto before=r;const auto before_fp=cpu->ExtRegs();
        flags=cpu->Cpsr();fpscr=cpu->Fpscr();const U32 before_flags=flags,before_fpscr=fpscr;
        const unsigned start=queries;
        hook.Finish(*this,r,&cpu->ExtRegs(),&flags,&fpscr);
        assert(r[15]==NativeCollision);
        Orbit::Hook restored;
        auto& finisher=restore?restored:hook;
        do {
            cpu->SetCpsr(flags);cpu->SetFpscr(fpscr);Run();
            flags=cpu->Cpsr();fpscr=cpu->Fpscr();
            finisher.Finish(*this,r,&cpu->ExtRegs(),&flags,&fpscr);
        } while(r[15]==NativeCollision);
        assert(queries-start==4 && r[13]==Stack && r[15]==Orbit::Resume);
        for(unsigned i=0;i<16;++i)if(i!=13 && i!=15)assert(r[i]==before[i]);
        assert(cpu->ExtRegs()==before_fp && flags==before_flags && fpscr==before_fpscr);
        assert(hook.pitch==pitch && hook.yaw==yaw && hook.radius==radius && hook.yaw_velocity==velocity);
        const auto fitted=Frustum::Make(V(Cam+0x80),V(Cam+0x8c),10,
            OOT3D::Camera::Widen(OOT3D::Camera::NativeFov,OOT3D::Camera::desired_fov.load()),
            OOT3D::Widescreen::NativeAspect);
        assert(fitted && fitted->radius<radius && finisher.last_eye==V(Cam+0x8c));
        for(unsigned i=0;i<4;++i)assert(fitted->Corner(i)[1]>=.73f);
        assert(Read(Stack-Orbit::Hook::ScratchSize+0x38)==0);
        Orbit::stick_x=0;
    }
    Vec Check(Vec at,Vec desired,float fov=77,float aspect=16.f/9,bool stale=false,bool requested=true) {
        for(unsigned i=0;i<planes.size();++i)for(unsigned j=0;j<3;++j)
            Set<std::int16_t>(Polys+i*16+10+j*2,std::int16_t(std::round(planes[i].normal[j]*32767)));
        Write(Cam+0xd4,Play);for(unsigned i=0;i<0x40;i+=4)Write(Stack+i,0);
        Put(*this,Stack,desired);Put(*this,Stack+0x28,at);Put(*this,Stack+0x40,desired);
        cpu->Reset();auto& r=cpu->Regs();r.fill(0);r[0]=r[4]=Cam;r[1]=Stack+0x28;r[2]=Stack;
        r[13]=Stack;r[14]=Stop;r[15]=NativeCollision;cpu->SetCpsr(0x10);cpu->SetFpscr(0);
        Run();assert(r[13]==Stack);
        const Vec native_eye=V(Stack);last_native_eye=native_eye;
        const auto before_r=r;const auto before_fp=cpu->ExtRegs();
        U32 flags=cpu->Cpsr(),fpscr=cpu->Fpscr();const U32 before_flags=flags,before_fpscr=fpscr;
        assert(Begin(*this,r,cpu->ExtRegs(),flags,fpscr,Cam,10,fov,aspect,5,Stop,requested?Stack+0x40:0));
        const unsigned start=queries;
        do {Run();flags=cpu->Cpsr();fpscr=cpu->Fpscr();}
        while(Resume(*this,r,cpu->ExtRegs(),flags,fpscr,stale?6:5,Stop));
        assert(queries-start==(stale?1u:4u));
        assert(r==before_r && cpu->ExtRegs()==before_fp && flags==before_flags && fpscr==before_fpscr);
        const Vec result=V(Stack);
        if(stale || planes.empty())assert(result==native_eye);
        else {
            const auto f=Frustum::Make(at,result,10,fov,aspect);assert(f);
            for(const auto& p:planes)for(unsigned corner=0;corner<4;++corner) {
                const float clearance=Dot(p.normal,f->Corner(corner))-p.d;
                if(clearance<.73f)std::cerr<<"corner clearance "<<clearance<<" eye "<<result[0]<<','<<result[1]<<','<<result[2]<<'\n';
                assert(clearance>=.73f);
            }
        }
        return result;
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env env;std::ifstream file(argv[1],std::ios::binary);assert(file);
    file.read(reinterpret_cast<char*>(env.bytes.data()+0x100000),0x4c7000);
    assert(env.Read(NativeCollision)==0xe92d4070);
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&env;Dynarmic::A32::Jit cpu(cfg);env.cpu=&cpu;
    {
        // The captured downhill follow is approximately a 1:3 slope with
        // near-horizontal orbit elevation. Native floor probes snap the eye
        // down on contact, then the radius hold restores its angle next tick.
        // Execute that same native routine on a deterministic ramp, including
        // every recovery request, rather than smoothing a fabricated final eye.
        const auto ramp=[&](bool requested,float slope,float elevation,float direction,float fov,float aspect) {
            const float n=1/std::sqrt(1+slope*slope);
            env.planes={{{0,n,slope*n},0}};
            Recovery recovery;Vec previous{};float largest=0;
            for(unsigned frame=0;frame<90;++frame) {
                const float z=direction*float(frame)*5;
                const Vec at{0,42-slope*z,z};
                const float angle=elevation*3.14159265358979323846f/180;
                const Vec intended=Add(at,{0,165*std::sin(angle),-165*std::cos(angle)});
                const auto request=recovery.Request(at,intended,frame);
                const auto result=env.Check(at,request,fov,aspect,false,requested);
                recovery.Observe(at,result,frame);
                const auto relative=Add(result,Mul(at,-1));
                if(frame)largest=std::max(largest,std::abs(relative[1]-previous[1]));
                if(requested) {
                    const float actual=std::atan2(relative[1],-relative[2]);
                    assert(std::abs(actual-angle)<.00001f);
                    // Native floor probes report floor+1, while line hits
                    // report the surface itself. Preserve the more conservative
                    // clearance; this can shift radius slightly but must not
                    // recreate the multi-unit angle bounce in the capture.
                    if(frame && std::abs(relative[1]-previous[1])>=.2f)
                        std::cerr<<"slope="<<slope<<" elevation="<<elevation<<" direction="<<direction
                            <<" fov="<<fov<<" aspect="<<aspect<<" frame="<<frame
                            <<" relativeY="<<previous[1]<<" -> "<<relative[1]<<'\n';
                    if(frame)assert(std::abs(relative[1]-previous[1])<.2f);
                }
                previous=relative;
            }
            return largest;
        };
        const float old_bounce=ramp(false,1.f/3,2.107f,1,64,400.f/240);
        assert(old_bounce>1.f); // Reproduce the old floor/contact feedback.
        float largest=0;
        for(float slope:{.25f,1.f/3,.5f})for(float angle:{2.107f,-2.258f})
            for(float direction:{-1.f,1.f})for(float fov:{52.f,77.f})for(float aspect:{400.f/240,16.f/9})
                largest=std::max(largest,ramp(true,slope,angle,direction,fov,aspect));
        std::cout<<"Native slope replay: prior vertical bounce "<<old_bounce
            <<", requested-ray residual "<<largest<<" units; all corners retain clearance\n";
        env.planes.clear();
    }
    {
        Recovery recovery;
        const Vec at{0,50,0},desired{0,50,-187.5f};
        const auto length=[&](Vec eye) {const auto v=Add(eye,Mul(at,-1));return std::sqrt(Dot(v,v));};
        assert(recovery.Request(at,desired,1)==desired);
        recovery.Observe(at,{0,50,-149.94168f},1);
        // Captured rebound: 149.94 -> 169.59 -> 178.14. Keep the request
        // close for two updates; a renewed obstruction still wins instantly.
        for(unsigned f=2;f<=3;++f) {
            const auto request=recovery.Request(at,desired,f);
            assert(std::abs(length(request)-150.94168f)<.001f);
            recovery.Observe(at,Add(request,{0,0,1}),f);
        }
        auto request=recovery.Request(at,desired,4);
        assert(std::abs(length(request)-153.44168f)<.001f);
        recovery.Observe(at,{0,50,-120},4);
        assert(length(recovery.Request(at,desired,5))==121);
        recovery.Observe(at,{0,50,-120},5);
        const Vec turn{187.5f*.6f,50,-187.5f*.8f};
        const auto turning=recovery.Request(at,turn,6);
        assert(turning[0]>70 && std::abs(turning[0]/-turning[2]-.75f)<.0001f);
        recovery.Observe(at,turning,6); // Radius hold must not hold yaw.
        // Missed updates, teleport and explicit mode/epoch resets discard history.
        assert(recovery.Request(at,desired,8)==desired);
        recovery.Observe(at,{0,50,-100},8);
        assert(recovery.Request({100,50,0},{100,50,-187.5f},9)==Vec({100,50,-187.5f}));
        recovery.Reset();assert(recovery.Request(at,desired,10)==desired);
        recovery.Observe(at,desired,10);
        assert(recovery.Request(at,desired,11)==desired); // Open space unchanged.
    }
    {
        // Run every shortened request through the actual native center and
        // frustum collision routines. A wall approaching during recovery must
        // still correct inward; removing it must eventually restore distance.
        Recovery recovery;
        const Vec at{0,50,0},desired{0,50,-187.5f};
        float previous=187.5f;
        for(unsigned f=1;f<=100;++f) {
            env.planes.clear();
            if(f<10)env.planes.push_back({{0,0,1},f<5?-150.f:-120.f});
            const auto request=recovery.Request(at,desired,f);
            const auto result=env.Check(at,request);
            const auto delta=Add(result,Mul(at,-1));
            const float radius=std::sqrt(Dot(delta,delta));
            if(f>1)assert(radius<=previous+2.6f);
            if(f==5)assert(radius<previous-20);
            recovery.Observe(at,result,f);previous=radius;
        }
        assert(std::abs(previous-186.5f)<.1f); // Native clear-ray offset.
    }
    env.Check({0,50,0},{0,60,-150});
    for(float fov:{52.f,77.f})for(float aspect:{400.f/240,16.f/9}) {
        env.planes={{{0,1,0},0}};env.Check({0,50,0},{0,-25,-150},fov,aspect);
        env.planes={{{1,0,0},0}};env.Check({50,50,0},{-5,50,-150},fov,aspect);
        env.planes={{{0,-1,0},-80}};env.Check({0,50,0},{0,90,-150},fov,aspect);
        env.planes={{{1,0,0},0},{{0,1,0},0}};env.Check({50,50,0},{-5,-5,-150},fov,aspect);
        env.planes={{{.6f,.8f,0},0}};env.Check({30,40,0},{-15,-20,-150},fov,aspect);
    }
    env.planes={{{0,1,0},0}};env.Check({0,50,0},{0,-25,-150},77,16.f/9,true);
    // Small continuous stick-equivalent yaw steps must keep moving along
    // contact. The caller retains its intended orbit, never the shortened arm.
    Vec previous{};
    for(unsigned i=0;i<100;++i) {
        const float yaw=.001f*i;
        const Vec desired{150*std::sin(yaw),-25,-150*std::cos(yaw)};
        const Vec result=env.Check({0,50,0},desired);
        if(i)assert(result[0]>previous[0]+.01f);
        previous=result;
    }
    // Radial fitting cannot satisfy a wall parallel to the arm when Link's
    // pivot is too close. Report infeasible rather than pushing sideways.
    const auto parallel=Frustum::Make({1,50,0},{1,50,-150},10,77,16.f/9);
    assert(parallel && parallel->Limit({0,0,0},{1,0,0},150)<0);
    // Opposite-facing constraints must form a feasible interval. A wall
    // discovered later must not invalidate clearance from an earlier wall.
    float minimum=11;
    const auto corridor=Frustum::Make({0,50,0},{0,50,-150},10,77,16.f/9);
    assert(corridor && corridor->Limit({0,0,-30},{0,0,-1},150,&minimum)==150);
    const float upper=corridor->Limit({0,0,-35},{0,0,1},150,&minimum);
    assert(minimum>upper);
    env.planes={{{0,1,0},0}};
    Vec previous_native{};unsigned moved=0;
    for(unsigned i=0;i<100;++i) {
        // Reverse tiny pitch changes at contact without a dead region.
        // Native collision quantizes its spherical angles: require movement
        // whenever native collision itself moves beyond float roundoff.
        const float y=-25+.01f*(i<50?i:100-i);
        const auto result=env.Check({0,50,0},{0,y,-150});
        if(i && std::abs(env.last_native_eye[2]-previous_native[2])>.0001f) {
            assert(result!=previous);++moved;
        }
        previous=result;previous_native=env.last_native_eye;
    }
    assert(moved>20);
    env.Integrated(false);env.Integrated(true);
    std::cout<<"Native camera collision + frustum fit: outward recovery, immediate inward correction, clear-ray offset, floor, wall, ceiling, corner, FOV/aspect extremes, tiny tangential motion, CPU preservation and restored/reset queries passed\n";
}
