// SPDX-License-Identifier: GPL-2.0-or-later
// Uses the local game binary: executes its queue, model draw and material-color
// copy in Dynarmic. Captures the color at the shader-upload boundary; GPU work
// is omitted. Link the built dynarmic, mcl and fmt static libraries.
#include "oot3d/hud_hooks.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace OOT3D;
using HUD::U32;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Model=0x800000, CounterModel=0x801000, Counter=0x802000;
    static constexpr U32 Renderer=0x803000, VTable=0x806000, Cover=0x807000;
    static constexpr U32 Play=0x900000, ActionModel=0x880000, ActionRenderer=0x880400;
    static constexpr U32 ActionHelper=0x881000, ActionColors=0x882000;
    static constexpr U32 Stop=0xf00000, SP=0xe00000, Q=HUDFade::Queue;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    HUD::RoutingState routing;
    std::vector<float> alphas;
    std::uint64_t ticks=0;
    std::int64_t now_us=1000000;
    bool paused=false,action_drawn=true;
    float drawn_action_alpha=0;
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop)return 0xef000000;
        if(a==0x31485c)return 0xef000001;
        // Execute the frame visibility sample and native queued model draws.
        // No source meshes are installed, so routing cannot add extra models.
        for(unsigned i=4;i<HUD::Sites.size();++i)
            if(i!=5 && a==HUD::Sites[i].pc)return HUD::Instruction(a,Get<U32>(a));
        return Get<U32>(a);
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
    HUD::Layout layout{};
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();
        if(svc==0) {cpu->HaltExecution();return;}
        if(svc==1) {
            alphas.push_back(Get<float>(r[1]+12));
            assert(Get<float>(r[1])==.2f && Get<float>(r[1]+4)==.4f && Get<float>(r[1]+8)==.6f);
            // Rendering after this boundary is omitted. Run the real epilogue
            // so queue iteration/return and register preservation are exercised.
            r[15]=0x3fbb9c;return;
        }
        HUD::Handle(*this,r,svc-HUD::FirstSVC,layout,routing,paused,now_us);
    }
    void Setup() {
        cpu->Reset();layout={};routing={};alphas.clear();
        paused=false;action_drawn=true;now_us=1000000;drawn_action_alpha=0;
        Write(Ocarina::Owner+0x14,0);Write(SaveMenu::Owner+0x28,0);
        routing.fade.progress=1; routing.fade.initialized=true;
        routing.fade.context=Play; routing.fade.last_us=now_us;
        Write(HUDFade::UI+0xc,Play);Write(Play+0x100,0x0203);
        Write(Play+0x7f24,0x100);Write(Play+0x5c2c,0);Write(Play+0x7f10,0);
        Write(Play+0x318c,0);Write(0x5c1878+0xf38,0);
        Write(HUD::Main+0x34,2);Write(HUD::Main+0x68,0);
        Write(HUDFade::Action+0x18,ActionHelper);Write(HUDFade::Action+0x20,ActionModel);
        Write(ActionHelper,8);Write(ActionHelper+0x18,ActionColors);
        for(unsigned i=0;i<16;++i)Put<float>(ActionColors+i*4,1);
        Write(ActionModel,0x4ebd60);Write(ActionModel+8,ActionRenderer);Write(ActionModel+0x178,0);
        Write(ActionRenderer,VTable);Write(ActionRenderer+0x354,ActionModel);
        Put<float>(ActionModel+0xf0,.2f);Put<float>(ActionModel+0xf4,.4f);
        Put<float>(ActionModel+0xf8,.6f);Put<float>(ActionModel+0xfc,1);

        for(unsigned pass=0;pass<7;++pass) {
            Write(Q+0x14+pass*4,0);Write(Q+0x754+pass*4,0);
        }
        Write(Q+8,0);Write(HUD::Main+0xc,Model);Write(HUD::Main+0x1c,Counter);
        Write(HUD::Main+0x20,0);Write(Counter+0x10,CounterModel);
        for(U32 model:{Model,CounterModel}) {
            Write(model,0x4ebd60);Write(model+8,Renderer+(model-Model));Write(model+0x178,0);
            Write(Renderer+(model-Model)+0x354,model);
            Write(Renderer+(model-Model),VTable);
            Put<float>(model+0xf0,.2f);Put<float>(model+0xf4,.4f);
            Put<float>(model+0xf8,.6f);Put<float>(model+0xfc,.8f);
        }
        // The model's renderer normally calls 3fb9ac while drawing. Go directly
        // to that native material routine to omit unrelated GPU initialization.
        Write(VTable+8,0x3fb9ac);
    }
    U32 AddCover(unsigned pass,float alpha,float color=0) {
        const U32 used=*Read(Q+8),count=*Read(Q+0x14+pass*4),cover=Cover+used*0x200;
        Write(Q+0xf4+used*4,cover);Write(Q+8,used+1);
        Write(Q+0x214+pass*0x60+count*4,cover);Write(Q+0x14+pass*4,count+1);
        const float width=pass<6?400:320;
        const std::array<float,6> rect{width/2,120,0,width,240,1};
        for(unsigned i=0;i<6;++i)Put<float>(cover+0x3c+i*4,rect[i]);
        for(unsigned i=0;i<3;++i)Put<float>(cover+0xf0+i*4,color);
        Put<float>(cover+0xfc,alpha);return cover;
    }
    void Run(U32 entry,U32 arg0,U32 arg1=0) {
        auto& r=cpu->Regs();r[0]=arg0;r[1]=arg1;r[13]=SP;r[14]=Stop;r[15]=entry;
        for(unsigned i=4;i<=11;++i)r[i]=0xcafe0000+i;
        cpu->SetCpsr(0x10);cpu->ClearHalt();ticks=10000;cpu->Run();
        assert(ticks && r[13]==SP && r[15]==Stop+4 && !routing.drawing_top_model);
        for(unsigned i=4;i<=11;++i)assert(r[i]==0xcafe0000+i);
    }
    void Draw(unsigned pass=5) {
        // Real top ordering: pass 3 resets capture, native A material draws,
        // then pass 5 samples its opacity before drawing the relocated HUD.
        // The synthetic covers are read by policy; their GPU draws are omitted.
        const U32 count=*Read(Q+0x20);Write(Q+0x20,0);Run(0x2fea30,Q,3);Write(Q+0x20,count);
        if(action_drawn) {
            alphas.clear();Run(0x3f9680,ActionModel);
            assert(alphas.size()==1);
            drawn_action_alpha=alphas[0];
            const float factor=routing.hud_return.hidden?0.f:
                routing.hud_return.returning?routing.hud_return.opacity:1.f;
            assert(std::abs(drawn_action_alpha-Get<float>(ActionModel+0xfc)*factor*layout.overall_opacity)<.00001f);
        }
        alphas.clear();Write(Q+0x14+pass*4,2);
        Write(Q+0x214+pass*0x60,Model);Write(Q+0x218+pass*0x60,CounterModel);
        Run(0x2fea30,Q,pass);
        assert(alphas.size()==2);
        assert(Get<float>(Model+0xfc)==.8f && Get<float>(CounterModel+0xfc)==.8f);
    }
    void Expect(float value) {
        Draw();for(float a:alphas)assert(std::abs(a-value)<.00001f);
    }
    void Settle(float value) {
        for(unsigned i=0;i<3;++i) {now_us+=HUDFade::FadeUs/3;Draw();}
        Expect(value);
    }
    void Reveal() {
        Expect(0);now_us+=HUDFade::FadeUs/2;Expect(.5f);
        Expect(.5f);now_us+=HUDFade::FadeUs/2;Expect(1);
    }

};
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream f(argv[1],std::ios::binary|std::ios::ate);assert(f);
    auto size=f.tellg();assert(size>0 && size<0xb00000);f.seekg(0);
    f.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);assert(f);
    assert(HUD::CodeMatches(e) && HUDFade::CodeMatches(e));
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    // A-button visibility drives a quick fade in both directions. The native
    // .8 material must not dim the result, and repeated eyes/draws cannot speed it up.
    e.Setup();e.Expect(1);e.action_drawn=false;e.Expect(1);
    e.now_us+=HUDFade::FadeUs/2;e.Expect(.5f);e.Expect(.5f);
    e.now_us+=HUDFade::FadeUs/2;e.Expect(0);e.Expect(0);
    e.action_drawn=true;e.Reveal();
    e.Setup();e.layout.overall_opacity=.5f;e.Expect(.5f);e.Expect(.5f);
    e.action_drawn=false;e.now_us+=HUDFade::FadeUs/2;e.Expect(.25f);
    e.layout.overall_opacity=0;e.Expect(0);
    e.Setup();
    // Reverse midway without jumping or restarting from an endpoint.
    e.action_drawn=false;e.now_us+=HUDFade::FadeUs/2;e.Expect(.5f);
    e.action_drawn=true;e.Expect(.5f);e.now_us+=HUDFade::FadeUs/4;e.Expect(.75f);
    e.now_us+=HUDFade::FadeUs/4;e.Expect(1);
    // Same duration with 6/12/24 updates; both routed models agree.
    for(unsigned steps:{6u,12u,24u}) {
        e.Setup();e.action_drawn=false;
        for(unsigned i=1;i<=steps;++i) {
            e.now_us=1000000+HUDFade::FadeUs*i/steps;e.Expect(1-float(i)/steps);
        }
        e.action_drawn=true;
        for(unsigned i=1;i<=steps;++i) {
            e.now_us=1000000+HUDFade::FadeUs+HUDFade::FadeUs*i/steps;e.Expect(float(i)/steps);
        }
    }
    // Reset interpolation on first use, time rewind, missing frames or scene replacement.
    e.Setup();e.routing.fade={};e.Reveal();
    e.Setup();--e.now_us;e.Reveal();
    e.Setup();e.now_us+=1000000;e.Reveal();
    e.Setup();e.Write(HUDFade::UI+0xc,Env::Play+0x10000);
    std::memcpy(&e.bytes[Env::Play+0x10000],&e.bytes[Env::Play],0x8000);e.Reveal();
    // Once settled, the exact native A alpha is retained.
    e.Setup();
    for(float alpha:{1.f,.75f,.5f,.25f,0.f,0.f,.25f,.5f,.75f,1.f}) {
        e.Put<float>(Env::ActionModel+0xfc,alpha);e.Settle(alpha);e.Expect(alpha);
    }
    e.Setup();e.Put<float>(Env::ActionModel+0xfc,.5f);
    for(unsigned i:{3u,7u,11u,15u})e.Put<float>(Env::ActionColors+i*4,.5f);
    e.Settle(.25f); // Native vertex and material opacity each applied once.
    e.Setup();e.Expect(1);e.action_drawn=false;e.Settle(0);e.Expect(0);
    e.action_drawn=true;e.Reveal(); // Skipped A draws do not leave stale visibility.
    // Dimmed bottom HUD / active message must not suppress a fully visible A.
    for(auto [at,value]:std::array<std::pair<U32,U32>,3>{{
            {Env::Play+0x318c,1},{0x5c1878+0xf38,1},{HUD::Main+0x68,0xe0400000}}}) {
        e.Setup();e.Write(at,value);e.Expect(1);
        e.Put<float>(Env::ActionModel+0xfc,.5f);e.Settle(.5f);
    }
    for(unsigned pass:{3u,4u,6u})for(float color:{0.f,1.f}) {
        e.Setup();auto cover=e.AddCover(pass,0,color);
        for(float alpha:{0.f,.25f,.5f,.75f,1.f,1.f,.75f,.5f,.25f,0.f}) {
            e.Put<float>(cover+0xfc,alpha);
            assert(std::abs(HUDFade::Visibility(e,pass)-(1-alpha))<.00001f);
            e.Settle(pass==4?1-alpha:1); // A is above pass 3; pass 6 is bottom only.
        }
    }
    e.Setup();e.AddCover(4,.5f);e.AddCover(6,.5f);e.Expect(.5f);
    e.Setup();e.AddCover(4,.5f);e.AddCover(4,.5f);e.Expect(.25f);
    // Covers can disappear while the native scene-transition renderer holds
    // black. Even a stale, fully opaque A model cannot expose the HUD then.
    e.Setup();e.AddCover(4,1);e.Write(Env::Play+0x7f10,3<<16);e.Expect(0);
    e.Write(Env::Q+0x14+4*4,0);e.Write(Env::Q+8,0);
    for(unsigned i=0;i<30;++i)e.Expect(0);
    e.Write(Env::Play+0x7f10,0);e.Write(Env::Play+0x100,3);e.Expect(0);
    e.Write(Env::Play+0x100,0x0203);e.Write(Env::Play+0x7f24,0);e.Expect(0);
    e.Write(Env::Play+0x7f24,0x100);e.Reveal();
    for(auto [at,value]:std::array<std::pair<U32,U32>,6>{{
            {Env::Play+0x5c2c,0xec00},{Env::Play+0x7f10,17<<16},
            {HUD::Main+0x34,0},{HUD::Main+0x34,0x17},
            {Env::Play+0x100,0x0202},{HUDFade::UI+0xc,0}}}) {
        e.Setup();const U32 old=*e.Read(at);e.Write(at,value);e.Expect(0);
        e.Write(at,old);e.Reveal();
    }
    e.Setup();e.paused=true;e.Expect(0);e.paused=false;e.Reveal();
    e.Setup();e.Write(HUDFade::Action+0x18,0);e.Settle(0);
    e.Setup();e.Write(Env::ActionHelper,9);e.Settle(0);
    e.Setup();e.Put<float>(Env::ActionColors+12,std::numeric_limits<float>::quiet_NaN());e.Settle(0);
    e.Setup();e.Put<float>(Env::ActionColors+12,.5f);e.Settle(0); // Unexpected nonuniform alpha.
    e.Setup();e.AddCover(4,1);e.Draw(6);for(float a:e.alphas)assert(a==.8f); // Native bottom copy.
    e.Setup();e.AddCover(4,1);e.Write(HUD::Main+0xc,0);e.Write(Env::Counter+0x10,0);e.Expect(.8f);
    e.Setup();e.AddCover(4,1);e.Write(Env::Q+0x14+4*4,0);e.Expect(1); // Stale pool object.
    e.Setup();e.AddCover(4,1);e.Write(Env::Q+8,0);e.Expect(1); // Nonpooled model.
    e.Setup();auto c=e.AddCover(4,1);e.Put<float>(c+0x48,80);e.Expect(1); // Partial cover.
    e.Setup();c=e.AddCover(4,1);e.Put<float>(c+0xf4,1);e.Expect(1); // Colored effect.
    e.Setup();e.AddCover(4,1);e.Write(Env::Q+8,25);e.Expect(1); // Invalid pool count.
    e.Setup();c=e.AddCover(4,1);e.Put<float>(c+0xfc,std::numeric_limits<float>::quiet_NaN());e.Expect(1);
    for(auto [at,word]:HUDFade::Guards) {
        e.Setup();e.AddCover(4,1);e.Write(at,word^1);assert(!HUDFade::CodeMatches(e));e.Expect(.8f);e.Write(at,word);
    }
    // The key number inherits the same material fade as the routed main
    // atlas, even though it has a different native owner from rupees.
    e.Setup();e.Write(HUD::Main+0x1c,0);e.Write(HUD::Main+0x20,Env::Counter);
    e.action_drawn=false;e.now_us+=HUDFade::FadeUs/2;e.Expect(.5f);
    e.AddCover(4,1);e.Expect(0);
    // Real failure case: A jumps from absent to fully opaque. BOTH native A
    // and the relocated atlas/digits must instead ramp together over 150 ms.
    // Exercise ocarina, Save, Options and pause exits at multiple frame rates.
    for(unsigned menu=0;menu<4;++menu)for(unsigned steps:{6u,12u,24u}) {
        e.Setup();
        if(menu==0)e.Write(Ocarina::Owner+0x14,12);
        if(menu==1 || menu==2)e.Write(SaveMenu::Owner+0x28,menu+1);
        if(menu==3)e.paused=true;
        // Native A can still submit at full alpha throughout the menu and
        // its closing frames. It must be hidden, not just faded after exit.
        e.action_drawn=true;e.Settle(0);assert(e.routing.hud_return.seen);
        assert(e.drawn_action_alpha==0);
        e.Write(Ocarina::Owner+0x14,0);e.Write(SaveMenu::Owner+0x28,0);e.paused=false;
        e.Write(HUD::Main+0x34,13);e.action_drawn=true;
        const auto start=e.now_us;
        for(unsigned i=0;i<=steps;++i) {
            e.now_us=start+HUDFade::FadeUs*i/steps;
            const float expected=float(i)/steps;
            e.Expect(expected);assert(std::abs(e.drawn_action_alpha-expected)<.00001f);
            e.Expect(expected); // Stereo/repeated draws cannot advance the fade.
            assert(e.Get<float>(Env::ActionModel+0xfc)==1); // Material untouched.
        }
        e.Write(HUD::Main+0x34,2);e.Expect(1);assert(!e.routing.hud_return.returning);
        e.action_drawn=false;e.now_us+=HUDFade::FadeUs/2;e.Expect(.5f);
    }
    // Do not consume the fade while the native A draw is still absent.
    e.Setup();e.Write(Ocarina::Owner+0x14,12);e.action_drawn=false;e.Settle(0);
    e.Write(Ocarina::Owner+0x14,0);
    e.now_us+=HUDFade::FadeUs;e.Expect(0);assert(!e.routing.hud_return.started);
    e.action_drawn=true;e.Reveal();assert(e.drawn_action_alpha==1);
    // Reopening a menu halfway through resets the next return; loading cancels it.
    e.Setup();e.paused=true;e.Expect(0);e.paused=false;e.Expect(0);
    e.now_us+=HUDFade::FadeUs/2;e.Expect(.5f);
    e.Write(SaveMenu::Owner+0x28,2);e.Expect(0);
    e.Write(SaveMenu::Owner+0x28,0);e.Reveal();
    e.Write(Env::Play+0x5c2c,0x100);e.Expect(0);assert(!e.routing.hud_return.seen);
    std::cout<<"Native queue/material path: timed fade-in/out, reversal, repeat-draw and frame-rate independence, A-button opacity and skipped draws, full HUD during native dim states, transition hold without covers, scene readiness, both routed models, unchanged native colors, screen isolation and stale-cover rejection passed\n";
}
