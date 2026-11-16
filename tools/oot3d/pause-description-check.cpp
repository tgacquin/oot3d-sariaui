// SPDX-License-Identifier: GPL-2.0-or-later
// Executes the local game's ARM message dispatcher and animation clock in
// Dynarmic. Resource allocation/rendering helpers are explicitly stubbed below;
// this checks native control flow and ABI, not in-game visual presentation.
#include "oot3d/pause_description.h"
#include "oot3d/textbox_open.h"
#include "oot3d/shop_controls.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

using namespace OOT3D;
using namespace OOT3D::PauseDescription;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Stop=0xf00000, SP=0xe00000, Pane=0xc00000, Animation=0xc01000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    Hook hook;
    Shops::DescriptionContext shop;
    bool ShopDescription() {return !PauseMenu::Active(*this) && shop.Active(*this);}
    bool patched=true, supported=true, old_text=false, resource_blocked=false, half_frame=false;
    unsigned builds=0, cleanups=0, ticks=0, visible=0, glyphs=0, loads=0, sounds=0;
    std::uint64_t remaining=0;
    bool close_path=false;unsigned close_draws=0;
    std::int64_t close_time=1033333,transform_delay=0;
    float opening_sample=0;
    // The native dispatcher runs unchanged between these resource boundaries.
    const std::map<U32,U32> stubs{{0x2cd744,1}, {0x487a8c,2}, {0x488428,3},
        {0x487d38,4}, {0x487d00,5}, {0x487cb8,6}, {0x2cd710,7},
        {0x2cd6c0,8}, {0x2cd974,9}, {0x305940,10}, {0x2cd030,11},
        {0x37547c,13}, {0x305754,14}, {0x2cd2b4,15}, {0x2c453c,16},
        {0x48c368,17}, {0x48b8a8,18}};
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop || (close_path && a==0x30055c))return 0xef000000;
        if(close_path && a==0x3f9680)return 0xef00000c;
        if(auto i=stubs.find(a);i!=stubs.end())return 0xef000000|i->second;
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
    void InterpreterFallback(U32 pc,std::size_t) override {std::cerr<<"fallback "<<std::hex<<pc<<'\n';std::abort();}
    void ExceptionRaised(U32 pc,Dynarmic::A32::Exception) override {std::cerr<<"exception "<<std::hex<<pc<<'\n';std::abort();}
    void AddTicks(std::uint64_t n) override {remaining=n>remaining?0:remaining-n;}
    std::uint64_t GetTicksRemaining() override {return remaining;}
    void State(unsigned n) {Put<std::uint8_t>(Message+0xf38,n);}
    unsigned State() {return Get<std::uint8_t>(Message+0xf38);}
    void Engine(unsigned n) {Put<std::uint8_t>(Message+0xeb4,n);}
    unsigned Engine() {return Get<std::uint8_t>(Message+0xeb4);}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();auto& f=cpu->ExtRegs();
        if(svc>=FirstSVC && svc<FirstSVC+Sites.size()) {
            if(svc==FirstSVC+6) {
                U32 flags=cpu->Cpsr(),fpscr=cpu->Fpscr();
                hook.close.Present(*this,r,f,flags,fpscr,close_time,supported);
                cpu->SetCpsr(flags);cpu->SetFpscr(fpscr);return;
            }
            hook.Handle(*this,r,f,svc-FirstSVC,supported,half_frame,ShopDescription(),close_time);return;
        }
        if(svc==0) {cpu->HaltExecution();return;}
        switch(svc) {
        case 1: // Resource-dependent glyph preparation, 2cd744.
            ++builds;assert(r[0]==Message);
            if(!old_text && !resource_blocked) {State(4);Engine(1);glyphs=0;}
            break;
        case 2: { // Engine callbacks/resources are not available in this harness.
            ++ticks;assert(r[0]==Message+0xeac);
            if(Engine()==1)Engine(2);
            else if(Engine()==2) {
                // Exercise the displaced reveal-budget instruction with its
                // native argument convention. Native parsing is not stub-tested.
                auto saved=r;r[1]=1;
                if(patched)hook.Handle(*this,r,f,4,supported,half_frame,ShopDescription());else r[6]=r[1];
                glyphs=std::min(80u,glyphs+r[6]);r=saved;
                if(glyphs==80)Engine(3); // Page boundary, do not advance it.
            }
            break;
        }
        case 3:r[0]=glyphs;break;
        case 4: { // Plate/render traversal is resource-dependent; clock tested separately.
            auto saved=r;r[4]=Pane;f[17]=std::bit_cast<U32>(1.f);
            if(patched)hook.Handle(*this,r,f,5,supported,half_frame,ShopDescription());else f[0]=f[17];
            if(State()==2)Put<float>(Pane+0x18,Get<float>(Pane+0x18)+std::bit_cast<float>(f[0]));
            if(State()==2 && opening_sample>0) {
                Write(Pane,0x4ed234);Write(Pane+4,Message+0x168);Write(Pane+0x24,0x60);
                Put<float>(Pane+0x3c,opening_sample);Put<float>(Pane+0x40,opening_sample);
            }
            r=saved;break;
        }
        case 5:visible=r[1];break;
        case 6:assert(r[0]==Message+0x44);++cleanups;old_text=false;break;
        case 7:r[0]=old_text;break;
        case 8:break; // Native dispatcher itself sets the cleanup flags/countdown.
        case 9:State(2);Put<float>(Pane+0x18,0);break;
        case 10:break;
        case 11:State(0);break;
        case 12:
            assert(close_path && r[0]==0xc02000);++close_draws;
            r[1]=0xc03000;
            assert(hook.close.Transform(*this,r,close_time+transform_delay));
            assert(std::abs(Get<float>(r[1])-TextboxClose::Scale(close_time-hook.close.started))<1e-6f);
            r[0]=r[1]=r[2]=r[3]=0xdeadbeef;f[0]=0x1234;
            break;
        case 13:++sounds;break; // Audio is not part of message timing.
        case 14:assert(r[0]==Message+0x44);break; // Restore text renderer position.
        case 15:assert(r[0]==Message);++loads;r[0]=!resource_blocked;break; // Resource lookup.
        case 16:assert(r[0]==Message+0x44);break; // Hide native choice pane.
        case 17:r[0]=1;break; // Choice line metadata.
        case 18:assert(r[0]==Message+0x44);break; // Native choice position update.
        default:std::abort();
        }
        r[15]=r[14];
    }
    void Setup(unsigned state=13) {
        cpu->Reset();hook={};shop={};supported=true;old_text=true;resource_blocked=false;
        opening_sample=0;
        builds=cleanups=ticks=visible=glyphs=loads=sounds=0;
        Write(PauseMenu::UI+0xc,0x800000);Write(PauseMenu::UI+0x14,1);
        Write(PauseMenu::UI+0x20,0);Write(PauseMenu::UI+0x24,1);Write(0x800100,3);
        Write(0x5066f8+0x34,2);Write(0x50446c+0x18,0);Write(Message,0x4ed0a0);Write(Message+0xc,1);
        Write(Message+8,0x810000);Write(0x810018,0);Write(Message+0xf40,0x700);Write(Message+0xfa4,3);
        Write(Message+0xfac,0x123);Write(Message+0xfb0,3);Write(Message+0xfc4,0);
        Write(Message+0xfb4,0x124);Write(Message+0xbd8,3);
        Write(Message+0xfc8,1);Write(Message+0xfcc,2);State(state);Engine(0);
        Write(Message+0x56c,Pane);Write(Pane+0xc,Animation);
        Put<float>(Pane+0x18,0);Put<float>(Pane+0x1c,12);
        Put<std::uint8_t>(Pane+0x14,1);Put<std::uint8_t>(Pane+0x20,0);
        // State 1's native initial delay inspects this gameplay context.
        Write(Get<U32>(0x480ba4),0x820000);Put<std::uint16_t>(0x8204b2,1);
    }
    void Shop(unsigned state=6,U32 id=0x100,bool masks=false) {
        Setup();Write(PauseMenu::UI+0x14,0);
        constexpr U32 actor=0x900000,item=0x910000,play=0x800000;
        Write(0x5043e0,play);Write(actor,0x3d);Write(actor+0x228,0x103c70);
        Write(actor+0x1c,masks?10:0);Write(actor+0x2a0,state);
        for(unsigned i=0;i<8;++i) {
            Write(actor+0x2a4+4*i,item+i*0x200);Write(item+i*0x200,4);
            Put<std::uint16_t>(item+i*0x200+0x116,0x100+i);
        }
        Write(Message+0xf40,id);shop.Observe(*this,actor,play);
    }
    void Run(U32 pc=0x4807f8,U32 arg=Message) {
        auto& r=cpu->Regs();r[0]=arg;r[13]=SP;r[14]=Stop;r[15]=pc;
        for(unsigned i=4;i<=11;++i)r[i]=0xcafe0000+i;
        cpu->SetCpsr(0x10);cpu->ClearHalt();remaining=100000;
        cpu->Run();
        assert(remaining && r[15]==Stop+4 && r[13]==SP);
        for(unsigned i=4;i<=11;++i)assert(r[i]==0xcafe0000+i);
    }
};
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);Env e;std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
    assert(file);const auto size=file.tellg();assert(size>0 && size<0xb00000);
    file.seekg(0);file.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);assert(file);
    assert(CodeMatches(e));
    for(const auto& site:Sites) {
        const auto word=*e.Read(site.pc);e.Write(site.pc,word^1);assert(!CodeMatches(e));e.Write(site.pc,word);
    }
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu(config);e.cpu=&cpu;
    // Compare native timing with the hooks. Replacement has no blank rendered
    // frame, even if the native replacement branch must first schedule cleanup.
    for(bool patched:{false,true}) {
        e.patched=patched;cpu.ClearCache();
        for(bool pending:{false,true}) {
            e.Setup();e.Write(Message+0xfc8,pending);e.Run();
            if(patched)assert(e.visible==80 && e.cleanups==1 && e.State()==4 && e.Engine()==3);
            else assert(e.visible==0 && e.State()==13);
        }
        e.Setup(1);e.old_text=false;e.Write(Message+0xfc8,0);
        unsigned updates=0;
        do {e.Run();++updates;assert(updates<100);}while(e.State()!=4);
        std::cout<<(patched?"Patched":"Native")<<" opening reaches text preparation after "<<updates<<" updates\n";
        assert(updates==(patched?13u:14u));
    }
    // Execute native accepted-input branches: ordinary next page, linked next
    // message, and choice result. Only the first native glyph appears this tick.
    for(bool patched:{false,true})for(unsigned state:{5u,6u,7u})
    for(unsigned input:{1u,2u})for(bool pending:{false,true}) {
        e.patched=patched;cpu.ClearCache();e.Setup(state);e.Write(PauseMenu::UI+0x14,0);
        e.Engine(3);e.Write(Message+0xfa4,0);e.Write(Message+0xfc8,pending);e.Write(0x810018,input);
        e.Run();
        if(state==7 && input==2) {assert(e.State()==7 && e.loads==0);continue;} // B does not select.
        if(patched) {
            assert(e.State()==4 && e.Engine()==2 && e.glyphs==1 && e.visible==1);
            assert(e.cleanups==1 && e.builds>=1 && e.builds<=8 && e.loads==(state==5?0u:1u));
            const auto old_builds=e.builds,old_loads=e.loads;
            e.Write(0x810018,0);e.Run();
            assert(e.glyphs==2 && e.visible==2 && e.builds==old_builds && e.loads==old_loads);
        } else assert(e.visible==0);
    }
    e.patched=true;cpu.ClearCache();
    // Execute the native acceptance branches with an owned indicator that has
    // not drawn recently. Both wooden signs and NPC pages accept A and B;
    // held/no input, typing, choices and blocked final advances must not spring.
    for(unsigned type:{0u,1u})for(unsigned input:{0u,1u,2u})
    for(unsigned state:{4u,5u,6u,7u})for(bool blocked:{false,true}) {
        e.Setup(state);e.Write(PauseMenu::UI+0xc,0x810000);e.Write(PauseMenu::UI+0x14,0);
        e.Write(Message+0xf48,type);e.Write(Message+0xfa4,0);e.Write(Message+0xfc8,0);
        e.Write(0x810018,input);e.Engine(state==4?2:3);
        constexpr U32 pane=0xc10000,model=0xc20000;
        e.Write(Message+0x570,pane);e.Write(pane,0x4ed234);
        e.Write(pane+4,Message+0x168);e.Write(pane+0x64,model);
        e.Write(model,0x4ebd60);e.Write(model+4,0xc30000);
        // Native final-message acceptance checks this cutscene guard first.
        e.Put<std::uint16_t>(0x8204d2,blocked?1:0);
        e.Run();
        const bool accepted=input && (state==5 || (state==6 && !blocked));
        assert(e.hook.indicator.animation.pressed==accepted);
        if(accepted)assert(e.hook.indicator.animation.press_time==e.close_time);
    }
    e.Put<std::uint16_t>(0x8204d2,0);
    for(unsigned state:{5u,6u,7u}) {
        e.Setup(state);e.Write(PauseMenu::UI+0x14,0);e.Engine(3);e.Write(Message+0xfc8,0);
        e.Write(Message+0xfa4,0);e.Run();assert(e.State()==state && !e.builds && !e.loads); // No edge.
    }
    e.Setup(5);e.Write(PauseMenu::UI+0x14,0);e.Engine(3);e.Write(Message+0xfc8,0);
    e.Write(0x810018,1);e.supported=false;e.Run();assert(e.State()==3 && !e.builds && !e.glyphs);
    e.Setup(5);e.Write(PauseMenu::UI+0x14,0);e.Engine(3);e.Write(Message+0xfc8,0);
    e.Write(0x810018,1);e.resource_blocked=true;e.Run();assert(e.builds<=8 && !e.glyphs && !e.hook.advance_candidate);
    for(unsigned state:{6u,7u}) {
        e.Setup(state);e.Write(PauseMenu::UI+0x14,0);e.Engine(3);e.Write(Message+0xfc8,0);
        e.Write(Message+0xfa4,0);e.Write(Message+0xfac,0xffff);e.Write(Message+0xfb4,0xffffffff);
        e.Write(0x810018,1);e.Run();assert(e.State()==14 && !e.builds && !e.loads); // Final close is native.
    }
    // Do not accelerate messages still typing or script-controlled waits.
    for(unsigned state:{4u,13u}) {
        e.Setup(state);e.Write(PauseMenu::UI+0x14,0);e.Write(Message+0xfc8,0);
        e.Write(0x810018,1);e.Engine(state==4?2:3);e.Run();
        assert(e.builds==0 && e.glyphs==(state==4?1u:0u));
    }
    // The carried-over click cannot set the native parser's skip-page argument.
    e.Setup(5);e.Write(PauseMenu::UI+0x14,0);e.Write(0x810018,1);
    auto& reveal_r=cpu.Regs();auto& reveal_f=cpu.ExtRegs();reveal_r[0]=Message;reveal_r[13]=Env::SP;
    e.hook.Handle(e,reveal_r,reveal_f,0);e.State(4);
    reveal_r[0]=Message+0xeac;reveal_r[1]=0;reveal_r[2]=1;
    e.hook.Handle(e,reveal_r,reveal_f,4);assert(reveal_r[6]==1 && reveal_r[2]==0);
    std::cout<<"Native page/linked/choice advance reveals one glyph in the accepted-input update; later typing, close and wait guards passed\n";
    // The 60 Hz pause path keeps native opening duration (12 native clock units)
    // and immediate replacement. Only animation delta changes, never parsing.
    e.half_frame=true;
    e.Setup(1);e.old_text=false;e.Write(Message+0xfc8,0);
    unsigned fast_updates=0;
    do {e.Run();++fast_updates;assert(fast_updates<100);}while(e.State()!=4);
    assert(fast_updates==25); // 24 half-steps plus the initial dispatcher tick.
    e.Setup();e.Run();assert(e.visible==80 && e.cleanups==1 && e.State()==4);
    e.half_frame=false;
    // Shopkeeper <-> item and item <-> item replacements complete within the
    // same native update, including replacements with retired glyphs pending.
    for(unsigned state:{2u,4u,5u,6u,7u,8u})for(U32 id:{0x83u,0x100u,0x107u})for(bool pending:{false,true}) {
        e.Shop(state,id);e.Write(Message+0xfc8,pending);assert(e.ShopDescription());
        e.Run();assert(e.visible==80 && e.cleanups==1 && e.State()==4 && e.Engine()==3);
        e.Run();assert(e.visible==80 && e.Engine()==3); // Do not advance a page/choice boundary.
    }
    for(U32 id:{0x70a2u,0x70adu}) {
        e.Shop(2,id,true);e.Run();assert(e.visible==80 && e.State()==4);
    }
    for(unsigned state=0;state<27;++state) {
        if(state==2 || (state>=4 && state<=8))continue;
        e.Shop(state);assert(!e.ShopDescription());e.Run();
        assert(e.visible==0 && e.State()==13 && e.Read(Message+0xfa4)==2);
    }
    for(unsigned exclusion=0;exclusion<5;++exclusion) {
        e.Shop();
        if(exclusion==0)e.Write(Message+0xf40,0x999); // Unrelated dialogue/receipt.
        if(exclusion==1)e.Write(0x5043e0,0x820000); // Scene/context changed.
        if(exclusion==2)e.Write(0x900000,0x42); // Actor no longer belongs to this shop.
        if(exclusion==3)e.Write(Message+0xc,0); // Closed message controller.
        if(exclusion==4)e.Write(0x900228,0); // Shop actor has been destroyed.
        assert(!e.ShopDescription());
    }
    e.Shop();e.State(3);e.old_text=false;e.resource_blocked=true;e.Write(Message+0xfc8,0);
    e.Run();assert(e.builds==8 && !e.hook.updating);
    // Unsupported modes, NPC IDs, other pause pages and gameplay keep native delay.
    for(unsigned exclusion=0;exclusion<5;++exclusion) {
        e.Setup();
        if(exclusion==0)e.supported=false;
        if(exclusion==1)e.Write(Message+0xf40,0x100);
        if(exclusion==2)e.Write(0x5066f8+0x34,0);
        if(exclusion==3)e.Write(0x800100,0);
        if(exclusion==4)e.Write(0x5066f8+0x34,14);
        e.Run();assert(e.visible==0 && e.State()==13 && e.Read(Message+0xfa4)==2);
    }
    for(U32 page:{1u,2u,8u,13u}) {
        e.Setup();e.Write(0x5066f8+0x34,page);e.Run();assert(e.visible==80);
    }
    for(U32 gear:{1u,2u,3u,9u})for(bool pending:{false,true}) {
        e.Setup();e.Write(0x5066f8+0x34,0);e.Write(0x50446c+0x18,gear);
        e.Write(Message+0xf40,0x755);e.Write(Message+0xfc8,pending);
        e.Run();assert(e.visible==80 && e.cleanups==1 && e.State()==4 && e.Engine()==3);
    }
    for(U32 gear:{0u,4u,5u,6u,7u,8u,10u}) {
        e.Setup();e.Write(0x5066f8+0x34,0);e.Write(0x50446c+0x18,gear);
        e.Run();assert(e.visible==0 && e.State()==13);
    }
    e.Setup(3);e.old_text=false;e.resource_blocked=true;e.Write(Message+0xfc8,0);
    e.Run();assert(e.builds==8 && !e.hook.updating); // Bounded resource failure.
    e.Setup(5);e.Write(Message+0xfc8,0);e.Run();assert(!e.builds && e.ticks==1 && e.State()==5);
    e.Setup(14);e.Write(Message+0xfc8,0);e.Run();assert(!e.builds && e.State()==0);
    e.Setup(4);e.Engine(3);e.Write(Message+0xfc8,0);e.Run();assert(e.Engine()==3 && e.ticks==1);
    // Execute the real native clock for pause, dialogue and signs. The global
    // multiplier affects only the active message's opening plate, never text,
    // other UI panes or the closing animation. Half-rate correction stays independent.
    for(float speed:{.25f,1.f,2.f,3.f})for(bool half:{false,true})
    for(bool pause:{false,true})for(bool opening:{false,true})for(bool owned:{false,true}) {
        auto layout=Textboxes::GetLayout();layout.opening_speed=speed;Textboxes::SetLayout(layout,false);
        e.Setup(opening?2:4);e.hook.updating=false;
        if(!pause)e.Write(PauseMenu::UI+0x14,0);
        auto& r=cpu.Regs();auto& f=cpu.ExtRegs();r[4]=owned?Env::Pane:Env::Pane+0x100;
        f[17]=std::bit_cast<U32>(1.f);e.hook.Handle(e,r,f,5,true,half);
        e.Run(0x498b0c,Env::Pane+8);
        const float expected=(half?.5f:1.f)*(opening && owned?speed:1.f);
        assert(e.Get<float>(Env::Pane+0x18)==expected);
    }
    assert(Textboxes::OpeningSpeed(0)==.25f && Textboxes::OpeningSpeed(10)==3.f);
    assert(Textboxes::OpeningSpeed(std::numeric_limits<float>::quiet_NaN())==1);
    Textboxes::SetLayout({},false);
    if(argc==3) {
        // Evaluate the shipped opening QAN with the real ARM evaluator, then
        // copy the same 60-byte channel block as 494ebc..494ed0. Verify growth,
        // overshoot and settling at quarter-frame intervals, including that
        // the hook changes no native state or channels other than scale.
        std::ifstream qan(argv[2],std::ios::binary);assert(qan);
        U32 offset=0,size=0;qan.seekg(8);qan.read(reinterpret_cast<char*>(&offset),4);
        qan.read(reinterpret_cast<char*>(&size),4);assert(offset==2048 && size==500);
        qan.seekg(offset);std::vector<char> opening(size);qan.read(opening.data(),size);assert(qan);
        for(unsigned exclusion=0;exclusion<10;++exclusion) {
            float previous=0;bool saw_overshoot=false;
            for(unsigned tick=0;tick<=40;++tick) {
                e.Setup(2);std::memcpy(e.bytes.data()+Env::Animation,opening.data(),size);
                e.Write(0x5066f8+0x34,0);e.Write(0x50446c+0x18,0);
                e.Write(Env::Pane,0x4ed234);e.Write(Env::Pane+4,Message+0x168);
                e.Put<float>(Env::Pane+0x18,tick*.25f);cpu.ExtRegs()[0]=0;
                e.Run(0x498b0c,Env::Pane+8);
                std::memcpy(e.bytes.data()+Env::SP+0x58,e.bytes.data()+Env::Pane+0x24,60);
                auto& r=cpu.Regs();r[4]=Env::Pane;r[13]=Env::SP;
                if(exclusion==1)e.Write(0x5066f8+0x34,2); // Items.
                if(exclusion==2)e.State(4);
                if(exclusion==3)e.Write(Message+0x56c,Env::Pane+0x100);
                if(exclusion==4)e.Write(Env::Pane+4,Message+0x200);
                if(exclusion==5)e.Write(Message+0xc,0);
                if(exclusion==6)e.State(14);
                if(exclusion==8)e.Write(PauseMenu::UI+0x14,0); // Gameplay dialogue/sign.
                if(exclusion==9)e.Write(0x50446c+0x18,2); // Gear.
                const float native=e.Get<float>(Env::SP+0x70);
                assert(std::isfinite(native) && native>=.099f && native<=1.201f);
                saw_overshoot|=native>1.01f;
                const auto before=e.bytes;const auto regs=r;
                TextboxOpening::Apply(e,r,exclusion!=7);
                const float actual=e.Get<float>(Env::SP+0x70);
                assert(actual==(exclusion?native:std::min(native,1.f)));
                assert(e.Get<float>(Env::SP+0x74)==actual && r==regs);
                if(!exclusion) {assert(actual+1e-6f>=previous);previous=actual;}
                e.Put<float>(Env::SP+0x70,native);e.Put<float>(Env::SP+0x74,native);
                assert(e.bytes==before);
                // The dispatcher ends the remaining bounce only after the
                // evaluated plate (not its already-advanced clock) is full.
                const auto clock=e.Read(Env::Pane+0x18);
                const bool finished=TextboxOpening::FinishAtFullSize(e);
                const bool expected_finish=(exclusion==0 || exclusion==7) && native>=1;
                assert(finished==expected_finish);
                assert(e.Read(Env::Pane+0x18)==(finished?e.Read(Env::Pane+0x1c):clock));
                e.Write(Env::Pane+0x18,*clock);assert(e.bytes==before);
            }
            assert(saw_overshoot);
            if(!exclusion)assert(previous==1.f);
        }
        std::cout<<"Native opening QAN: capped map growth, other-screen and ownership exclusions passed\n";
    }
    Textboxes::SetLayout({},false);
    // Native dispatcher returns with text already visible on the first full
    // map plate update. Other screens keep their existing opening timing.
    for(unsigned screen=0;screen<4;++screen)for(float size:{.99f,1.f,1.2f}) {
        e.Setup(2);e.old_text=false;e.Write(Message+0xfc8,0);e.opening_sample=size;
        e.Write(0x5066f8+0x34,0);e.Write(0x50446c+0x18,0);
        if(screen==1)e.Write(PauseMenu::UI+0x14,0);
        if(screen==2)e.Write(0x5066f8+0x34,2);
        if(screen==3)e.Write(0x50446c+0x18,2);
        e.Run();
        if(screen==0 && size>=1) {
            assert(e.visible==1 && e.State()==4);
            e.Run();assert(e.visible==2); // No ongoing reveal acceleration.
        } else assert(e.visible==0 && e.State()==2);
    }
    e.Setup(2);e.old_text=false;e.resource_blocked=true;e.opening_sample=1;e.Write(Message+0xfc8,0);
    e.Write(0x5066f8+0x34,0);e.Write(0x50446c+0x18,0);
    e.Run();assert(e.builds>0 && e.builds<8 && e.visible==0);
    Textboxes::SetLayout({},false);
    // Execute the real top-screen scene-local message call and its continuation.
    // Its r0 is play+32c0, not the global dialogue Message; the old hook missed it.
    for(bool toggle:{false,true})for(bool boundary:{false,true}) {
        e.Setup(0);e.close_path=true;e.close_draws=0;cpu.ClearCache();
        auto layout=Textboxes::GetLayout();layout.mm_close=toggle;Textboxes::SetLayout(layout,false);
        constexpr U32 play=0x800000,model=0xc02000,view=0xc03000;
        e.Write(Message+8,play);e.Write(HUDFade::UI+0xc,play);e.Write(play+0x104,1);
        e.Write(Env::Pane,0x4ed234);e.Write(Env::Pane+4,Message+0x168);
        e.Write(Env::Pane+0x64,model);e.Write(model,0x4ebd60);e.Write(model+4,0xc04000);
        for(unsigned i=0;i<12;++i) {
            e.Put<float>(model+12+4*i,float(i==0||i==5||i==10));
            e.Put<float>(view+4*i,float(i==0||i==5||i==10));
        }
        e.State(6);e.hook.close.Capture(e,model,view,1000000);e.State(0);
        e.close_time=1033333;e.transform_delay=0;
        if(boundary && toggle) {
            e.hook.close.closing=true;e.hook.close.started=e.close_time;
            e.close_time+=TextboxClose::Duration-1;e.transform_delay=1000;
        }
        auto& r=cpu.Regs();r[4]=play;r[13]=Env::SP;r[14]=Env::Stop;r[15]=0x300548;
        e.Write(Env::SP+0x40,0x87654321);e.Write(play+0x32c0+0xc,0);
        cpu.ExtRegs()[0]=0x4321;cpu.SetCpsr(0x10);cpu.ClearHalt();e.remaining=100000;cpu.Run();
        assert(e.remaining && r[15]==0x300560 && r[13]==Env::SP && r[4]==play);
        assert(e.close_draws==unsigned(toggle) && r[0]==0x87654321 && cpu.ExtRegs()[0]==0x4321);
        assert(!e.hook.close.pending);
    }
    Textboxes::SetLayout({},false);
    std::cout<<"Native dispatcher, signatures, cleanup ABI, immediate pause/shop replacements, bounded retries, opening clock, purchase/dialogue exclusions and shop lifetime guards passed\n";
}
