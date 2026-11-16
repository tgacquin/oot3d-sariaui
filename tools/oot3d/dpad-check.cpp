// SPDX-License-Identifier: GPL-2.0-or-later
// Executes the native boots item handler and equipment refresh in Dynarmic.
// Only model rebuilding and sound playback are stubbed at their boundaries.
#include "oot3d/dpad_hud.h"
#include "oot3d/dpad_assets.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace OOT3D;
using namespace OOT3D::DPad;
// Test-only input token; the hotkey queues a request and never emits a guest bit.
constexpr U32 ViewButton=0x10000000;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Play=0x900000,Link=0x910000,SP=0xe00000,Stop=0xf00000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    BootsHook hook;
    std::uint64_t ticks=0;
    std::int64_t now=1000000;
    unsigned refresh=0,sound=0;
    bool restricted=false,native_update=false,poll_during_confirmation=false,view_down=false;
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {if(a+4>bytes.size())return {};return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop)return 0xef000000;
        if(a==0x42ea4c)return 0xef000000;
        if(a==0x42f270)return 0xef000000;
        if(a==UpdateSite.pc+4 && !native_update)return 0xef000000;
        if(a==0x374be8 && !native_update)return 0xef000002; // Scene restriction fixture.
        if(a==0x32c2c0 || a==0x3589dc || a==0x36f59c)return 0xef000001;
        // Stop after the hook's displaced ADD; the original caller's VFP/
        // register epilogue is outside this small synthetic entry frame.
        if(a==Site.pc+4)return 0xef000000;
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
        if(svc==0){cpu->HaltExecution();return;}
        if(svc==2){r[0]=restricted;r[15]=r[14];return;}
        if(svc==1) {
            if(r[15]-4==0x32c2c0){assert(r[0]==Link);++refresh;}
            else if(r[15]-4==0x3589dc){assert(r[0]==Play && r[1]==Link);++refresh;}
            else {assert(r[0]==Link);++sound;}
            r[15]=r[14];return;
        }
        if(svc==AcceptedSVC) {
            AcceptAction(*this,r);
            // HID may observe state 10/11 while the same native update is
            // completing the accepted action. It must not cancel confirmation.
            if(poll_during_confirmation)Poll(0,0);
        }
        else if(svc==UpdateSVC)BeginUpdate(*this,r);
        else if(svc==FirstSVC)hook.Handle(*this,r,now);
        else {
            assert(svc>FirstSVC && svc<=FirstSVC+ActionSites.size());
            HandleAction(*this,r,svc-FirstSVC-1,now);
        }
    }
    void Setup() {
        input={};look_requested=false;view_down=false;SetLayout({},false);now=1000000;
        Write(UI+0x18,0);Write(0x50bb38,0);Write(0x50bb3c,0);Write(0x50bb40,0);
        Write(UI+0xc,Play);Write(UI+0x14,0);Write(UI+0x20,0);Write(UI+0x24,0);
        Write(Play+0x100,0x0203);Write(Play+0x7f24,0x100);
        Write(Play+0x5c2c,0);Write(Play+0x7f10,0);Write(Play+0x43c,Link);
        Write(Play+0x2dd4,0);
        Write(Main+0x34,2);Write(Main+0x38,0);Write(Main+0x60,0);Write(Main+0x68,0);
        Write(Main+0x70,0);Put<std::uint8_t>(Save+0x1575,0);restricted=false;
        Write(Link+0x1710,0);Write(Link+0x1714,0);Write(Link+0x1748,0);
        Write(Link+0x1728,0);Write(Link+0x2224,0);
        Write(Save+4,0);Put<std::uint16_t>(Save+0x8a,0x1121);
        Put<std::uint16_t>(Save+0xb6,0x7000);Put<std::uint8_t>(Save+0x93,8);
        Put<std::uint8_t>(Save+0xa4,0x45);Put<std::uint8_t>(Save+0xa5,0x46);
        refresh=sound=0;
    }
    void Poll(U32 raw,U32 expected,bool live=true,std::int64_t time=1000000) {
        now=time;
        if((raw&ViewButton) && !view_down)RequestLook();
        view_down=bool(raw&ViewButton);raw&=~ViewButton;expected&=~ViewButton;
        U32 buttons=raw;input.Filter(*this,buttons,live,now);
        assert(buttons==expected);
    }
    void Boots(U32 button,unsigned equip) {
        Poll(0,0);Poll(button,0);
        assert(input.pending_item==(button==Left?Iron:Hover));
        auto& r=cpu->Regs();for(unsigned i=0;i<16;++i)r[i]=0xcafe0000+i;
        r[13]=SP;r[14]=Stop;r[15]=Site.pc;cpu->SetCpsr(0x10);
        cpu->ClearHalt();ticks=10000;cpu->Run();assert(ticks);
        assert(r[13]==SP+0x38 && r[15]==Site.pc+8 && r[14]==Stop);
        for(unsigned i=0;i<13;++i)assert(r[i]==0xcafe0000+i);
        assert(Get<std::uint16_t>(Save+0x8a)==(equip<<12|0x121));
        assert(Get<std::uint8_t>(Link+0x1a7)==equip-1);
        assert(input.pending_item==ElementCount);
        Poll(button,0);assert(input.pending_item==ElementCount);
    }
    void Touch(unsigned x,unsigned y,unsigned expected,bool pressed=true) {
        StartUpdate();
        // Feed the native touch structure and execute OoT's actual ocarina/
        // View branches, including hit testing, availability and state change.
        Put<std::uint16_t>(0x50bb3a,x);Put<std::uint16_t>(0x50bb3c,y);
        Put<std::uint16_t>(0x50bb40,pressed);
        Write(SP+0x28,Link+0x1000);Write(SP+0x40,Play);
        auto& r=cpu->Regs();r.fill(0);r[5]=Main;r[6]=1;r[8]=Play+0x2ba4;
        r[13]=SP;r[15]=0x42e91c;
        cpu->SetCpsr(0x10);cpu->ClearHalt();ticks=10000;cpu->Run();
        assert(ticks && Get<U32>(Main+0x34)==expected && r[13]==SP);
    }
    void Action(U32 button,unsigned x=0,unsigned y=0) {
        Write(Main+0x38,0);Write(Main+0x60,0);
        Touch(x,y,1,false);
        // States 10/11 merely select the touchscreen button. These fields
        // publish the actual View/ocarina requests to the player update.
        assert(Get<U32>(Main+0x38)==(button==ViewButton?1:0));
        assert(Get<U32>(Main+0x60)==(button==Right?1:0));
        assert(input.confirming==ElementCount);
    }
    void Release(unsigned state,bool released) {
        StartUpdate();
        Write(SP+0x28,Link+0x1000);Write(SP+0x40,Play);
        Put<std::uint16_t>(0x50bb40,0);Put<std::uint16_t>(0x50bb42,released);
        auto& r=cpu->Regs();r.fill(0);r[5]=Main;r[6]=1;r[8]=Play+0x2ba4;
        r[13]=SP;r[15]=state==10?0x42f150:0x42f0cc;
        cpu->SetCpsr(0x10);cpu->ClearHalt();ticks=10000;cpu->Run();
        assert(ticks && Get<U32>(Main+0x34)==1 && r[13]==SP);
    }
    void StartUpdate() {
        auto& r=cpu->Regs();r[15]=UpdateSite.pc;
        cpu->SetCpsr(0x10);cpu->ClearHalt();ticks=10000;cpu->Run();
        assert(ticks && r[5]==Main && input.update_sequence==input.sequence);
    }
    void FinishUpdate(unsigned pending=ElementCount) {
        auto& r=cpu->Regs();r[13]=SP;r[15]=Site.pc;
        cpu->ClearHalt();ticks=10000;cpu->Run();assert(ticks);
        assert(input.pending_item==pending);
    }
};
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);Env e;std::ifstream f(argv[1],std::ios::binary|std::ios::ate);assert(f);
    auto size=f.tellg();assert(size>0 && size<0xb00000);f.seekg(0);
    f.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);assert(f && CodeMatches(e));
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu(config);e.cpu=&cpu;e.Setup();
    e.Setup();e.Touch(26,26,11); // Native first-person View request.
    assert(e.Get<U32>(Main+0x38)==0); // Highlighting is not the action.
    e.Release(11,false);assert(e.Get<U32>(Main+0x38)==0);
    e.Setup();e.Touch(26,26,11);e.Release(11,true);
    assert(e.Get<U32>(Main+0x38)==1); // A genuine native release commits it.
    e.Setup();e.Touch(26,212,10); // Native ocarina request.
    assert(e.Get<U32>(Main+0x60)==0);
    e.Release(10,false);assert(e.Get<U32>(Main+0x60)==0);
    e.Setup();e.Touch(26,212,10);e.Release(10,true);
    assert(e.Get<U32>(Main+0x60)==1);
    e.Setup();e.restricted=true;e.Touch(26,212,2);
    e.Setup();e.Put<std::uint8_t>(Save+0x93,0xff);e.Touch(26,212,2);
    e.Setup();e.Write(Main+0x68,0x10000000);e.Touch(26,26,2);
    // HID runs at 234 Hz, independently of the native game update. Even a
    // press AND release entirely between updates must reach exactly one action.
    for(auto button:{ViewButton,Right})for(unsigned polls:{0u,1u,6u,12u,25u}) {
        e.Setup();e.Poll(button,0);
        for(unsigned i=0;i<polls;++i)e.Poll(0,0,true,1000000+4274*(i+1));
        e.Action(button);
        assert(input.pending_item==ElementCount && e.Get<U32>(0x50bb40)==0);
        e.Write(Main+0x34,2);e.Touch(0,0,2,false); // Never repeat on another update.
    }
    // Navi now has its own Up slot, available even without an ocarina.
    for(unsigned polls:{0u,1u,6u,12u,25u}) {
        e.Setup();e.Write(Env::Play+0x2dd4,1);e.Put<std::uint8_t>(Save+0x93,0xff);
        assert(NaviCalling(e) && TextureVariant(e)==0);
        e.Poll(Up,0);assert(input.pending_item==Navi);
        for(unsigned i=0;i<polls;++i)e.Poll(0,0,true,1000000+4274*(i+1));
        e.poll_during_confirmation=true;e.Touch(0,0,1,false);e.poll_during_confirmation=false;
        assert(e.Get<U32>(Main+0x38)==2 && e.Get<U32>(Main+0x60)==0);
        assert(input.pending_item==ElementCount && input.confirming==ElementCount);
        assert(e.Get<U32>(0x50bb40)==0);
        e.Write(Main+0x34,2);e.Write(Main+0x38,0);e.Touch(0,0,2,false);
        assert(e.Get<U32>(Main+0x38)==0);
    }
    e.Setup();e.Poll(Up,0);assert(input.pending_item==ElementCount); // No call: no View fallback.
    e.Setup();e.Write(Env::Play+0x2dd4,1);e.Poll(ViewButton,0);e.Action(ViewButton);
    assert(e.Get<U32>(Main+0x38)==1); // Stick click never answers Navi.
    e.Setup();e.Write(Env::Play+0x2dd4,1);e.Poll(Right,0);e.Action(Right);
    assert(e.Get<U32>(Main+0x60)==1); // Ocarina is independent of Navi.
    e.Setup();e.Write(Env::Play+0x2dd4,1);e.Poll(Up,0);e.Touch(0,0,1,false);
    e.Write(Main+0x34,2);e.Write(Main+0x38,0);e.Poll(Up,0);e.Touch(0,0,2,false);
    assert(e.Get<U32>(Main+0x38)==0); // Holding Up cannot retrigger.
    e.Setup();e.Write(Env::Play+0x2dd4,1);e.Poll(Up,0);e.Write(Env::Play+0x2dd4,0);
    e.Touch(0,0,2,false);assert(e.Get<U32>(Main+0x38)==0 && e.Get<U32>(Main+0x60)==0);
    e.Setup();e.Poll(Right,0);e.Write(Env::Play+0x2dd4,1);e.Action(Right);
    e.Setup();e.Write(Env::Play+0x2dd4,1);e.Write(Main+0x68,0x10000000);e.Poll(Up,0);
    e.Touch(0,0,2,false);e.FinishUpdate();assert(e.Get<U32>(Main+0x38)==0);
    // Native touches retain their original behavior while Navi is calling.
    e.Setup();e.Write(Env::Play+0x2dd4,1);e.Touch(26,26,11);e.Release(11,true);
    assert(e.Get<U32>(Main+0x38)==2);
    e.Setup();e.Write(Env::Play+0x2dd4,1);e.Touch(0,0,2,false);e.Poll(Up,0);
    e.FinishUpdate(Navi);e.Touch(0,0,1,false);assert(e.Get<U32>(Main+0x38)==2);
    for(auto button:{ViewButton,Right}) {
        e.Setup();e.Poll(button,0);e.poll_during_confirmation=true;e.Action(button);
        e.poll_during_confirmation=false;
        // A poll arriving after the game's input checks belongs to the next
        // update. The current update's epilogue must not discard it.
        e.Setup();e.Touch(0,0,2,false);e.Poll(button,0);
        e.FinishUpdate(button==ViewButton?Look:DPad::Ocarina);
        e.Action(button);e.FinishUpdate();
        e.Setup();e.Poll(button,0);e.Action(button);
        e.Write(Main+0x34,2);
        e.Poll(button,0);e.Touch(0,0,2,false); // Holding does not retrigger.
        e.Poll(0,0);e.Poll(button,0);e.Action(button);
        // A genuine touch being held elsewhere cannot swallow the shortcut.
        e.Setup();e.Poll(button,0);
        e.Put<std::uint16_t>(0x50bb3e,1);
        e.Action(button,180,120);
        assert(e.Get<std::uint16_t>(0x50bb3e)==1);
        assert(e.Get<std::uint16_t>(0x50bb3a)==180);
        // Native touch lock is honored, and the rejected request is discarded.
        e.Setup();e.Poll(button,0);e.Write(UI+0x18,1);
        e.Touch(0,0,2,false);e.FinishUpdate();
        e.Write(UI+0x18,0);e.Touch(0,0,2,false);
        // Expiry and state changes are checked even without another HID poll.
        e.Setup();e.Poll(button,0);e.now+=150001;
        e.Touch(0,0,2,false);assert(input.pending_item==ElementCount);
        e.Setup();e.Poll(button,0);e.now-=1;
        e.Touch(0,0,2,false);assert(input.pending_item==ElementCount);
        e.Setup();e.Poll(button,0);e.Write(UI+0xc,Env::Play+0x10000);
        e.Touch(0,0,2,false);assert(input.pending_item==ElementCount);
        e.Setup();e.Poll(button,0);e.Write(Env::Play+0x5c2c,0x100);
        e.Touch(0,0,2,false);assert(input.pending_item==ElementCount);
        e.Setup();e.Poll(button,0);e.Poll(8,8);
        e.Touch(0,0,2,false);assert(input.pending_item==ElementCount);
        e.Setup();e.Poll(button,0);e.Poll(0,0,false);
        e.Touch(0,0,2,false);assert(input.pending_item==ElementCount);
        e.Setup();e.Poll(button,0);auto disabled=GetLayout();disabled.enabled=false;
        SetLayout(disabled,false);
        if(button==ViewButton)e.Action(button);else e.Touch(0,0,2,false);
        assert(input.pending_item==ElementCount);
    }
    e.Setup();e.Poll(Right,0);e.restricted=true;
    e.Touch(0,0,2,false);assert(input.pending_item==ElementCount);
    e.restricted=false;e.Touch(0,0,2,false);
    e.Setup();e.Poll(Right,0);e.Put<std::uint8_t>(Save+0x93,0xff);
    e.Touch(0,0,2,false);assert(input.pending_item==ElementCount);
    e.Setup();e.Poll(ViewButton,0);e.Write(Main+0x68,0x10000000);
    e.Touch(0,0,2,false);e.FinishUpdate();e.Write(Main+0x68,0);e.Touch(0,0,2,false);
    e.Setup();e.Poll(ViewButton,0);e.Put<std::uint8_t>(Env::Link+0x1749,1);
    e.Action(ViewButton); // Alternate native View path.
    e.Setup();e.Poll(ViewButton,0);e.Put<std::uint8_t>(Env::Link+0x1749,2);
    e.Touch(0,0,2,false);e.FinishUpdate();
    // A real ocarina touch has native priority over View in the same update.
    e.Setup();e.Poll(ViewButton,0);e.Touch(26,212,10);e.FinishUpdate();
    e.Setup();e.Boots(Left,2);e.Boots(Left,1);e.Boots(Down,3);e.Boots(Left,2);
    assert(e.refresh==8 && e.sound==4);
    e.Setup();e.Write(Save+4,1);e.Poll(Left,0);assert(input.pending_item==ElementCount);
    e.Setup();e.Put<std::uint16_t>(Save+0xb6,0x1000);e.Poll(Down,0);
    assert(input.pending_item==ElementCount);
    e.Setup();e.Put<std::uint8_t>(Save+0x93,0xff);e.Poll(Right,0);
    e.Setup();e.Write(UI+0x14,1);e.Write(UI+0x24,1);
    e.Poll(Up,Up);assert(input.pending_item==ElementCount); // Pause navigation.
    e.Poll(ViewButton,0);assert(input.pending_item==ElementCount);
    e.Setup();e.Write(Main+0x34,14);e.Poll(Left,Left); // Save/menu input.
    e.Setup();e.Write(Env::Play+0x5c2c,0x100);e.Poll(ViewButton,ViewButton); // Loading.
    e.Setup();e.Poll(ViewButton,ViewButton,false);e.Poll(ViewButton,0); // No buffered edge.
    e.Setup();e.Poll(Left|Down,0);assert(input.pending_item==ElementCount);
    e.Setup();e.Write(Env::Link+0x1710,0x400);e.Poll(Left,0); // Item-get animation.
    assert(input.pending_item==ElementCount);
    e.Setup();e.Poll(Left,0);e.Poll(0,0,true,1200000);assert(input.pending_item==ElementCount);
    e.Setup();auto g=MakeGeometry(e,{},1);assert(g.count==5);
    assert(MakeGeometry(e,{},0).count==0);
    e.Write(Save+4,1);g=MakeGeometry(e,{},1);
    assert(g.count==5 && g.colors[3*16+3]==DisabledOpacity && g.colors[4*16+3]==DisabledOpacity);
    assert(g.colors[1*16+3]==0 && g.colors[2*16+3]==1); // Navi is hidden without a call; ocarina stays bright.
    e.Setup();e.Put<std::uint16_t>(Save+0xb6,0x1000);
    g=MakeGeometry(e,{},.5f);
    assert(g.count==5 && g.colors[3*16+3]==0 &&
           g.colors[4*16+3]==0); // Missing boots are invisible, not disabled icons.
    e.Put<std::uint8_t>(Save+0x93,0xff);g=MakeGeometry(e,{},1);
    assert(g.colors[2*16+3]==0); // Neither ocarina collected.
    for(auto ocarina:{7u,8u}) {
        e.Put<std::uint8_t>(Save+0x93,ocarina);g=MakeGeometry(e,{},.5f);
        assert(g.colors[2*16+3]==.5f);
    }
    e.Setup();e.Write(Env::Link+0x1710,0x400);
    g=MakeGeometry(e,{},1);
    assert(g.count==5 && g.colors[3*16+3]==DisabledOpacity); // Owned but temporarily blocked.
    e.Setup();Layout l;l.group={10,-4,2,3};l.icons[Iron]={7,-5,.5f,2};
    g=MakeGeometry(e,l,.5f);
    // Iron center: (36+10+(-20+7)*2, 88-4-5*3), size (20,120).
    assert(g.positions[3*12]==10 && g.positions[3*12+6]==30);
    assert(g.positions[3*12+4]==9 && g.positions[3*12+1]==129);
    assert(g.colors[3]==.5f && g.colors[3*16+3]==.5f);
    assert(g.uvs[3*8]==128.5f/256 && g.uvs[3*8+1]==1-64.5f/128);
    std::array<unsigned,2> samples{};
    auto texture=MakeTexture(false,[&](bool menu,unsigned x,unsigned y) {
        ++samples[menu];return (x<<16)|y;
    });
    assert(samples[0]==42*42*3 && samples[1]==48*48);
    // Native sprite cells are upside down in the decoded atlas. Check both
    // ends of each icon: screen top must use the cell's last source row.
    assert(texture[Tiled(128,47)]==((176u<<16)|255));
    assert(texture[Tiled(128,0)]==((176u<<16)|208));
    assert(texture[Tiled(192,41)]==((294u<<16)|511));
    assert(texture[Tiled(192,0)]==((294u<<16)|470));
    assert(texture[Tiled(128,105)]==((378u<<16)|301));
    assert(texture[Tiled(128,64)]==((378u<<16)|260));
    assert(texture[Tiled(192,105)]==((420u<<16)|301));
    assert(texture[Tiled(192,64)]==((420u<<16)|260));
    assert(texture[Tiled(0,103)]==Assets::RestorationDpad[0]);
    const auto navi_texture=MakeTexture(false,[](bool menu,unsigned x,unsigned y) {
        return (unsigned(menu)<<24)|(x<<16)|y;
    });
    assert(navi_texture[Tiled(128,0)]==((1u<<24)|(176u<<16)|208));
    assert(navi_texture[Tiled(175,47)]==((1u<<24)|(223u<<16)|255));
    e.Setup();const auto before_navi=MakeGeometry(e,{},.6f);
    e.Write(Env::Play+0x2dd4,1);e.Put<std::uint8_t>(Save+0x93,0xff);
    const auto navi_geometry=MakeGeometry(e,{},.6f);
    assert(navi_geometry.positions==before_navi.positions); // Same top-slot pose and scale.
    assert(navi_geometry.colors[19]==.6f && before_navi.colors[19]==0);
    assert(navi_geometry.colors[35]==0); // Missing ocarina is invisible.
    assert(navi_geometry.uvs==before_navi.uvs);
    e.Write(Env::Play+0x2dd4,0);
    const auto expired_navi=MakeGeometry(e,{},.6f);
    assert(expired_navi.colors[19]==0 && expired_navi.positions==navi_geometry.positions);
    // Native update clears the hint before drawing; retain an existing call
    // across the swing anyway, without ever granting conversation input.
    NaviDisplay display;
    std::int64_t navi_time=1000000;
    unsigned generation=1;
    const auto draw_navi=[&] {
        navi_time+=33333;
        return MakeGeometry(e,{},.6f,navi_time,&display,generation).colors[19];
    };
    e.Write(Env::Play+0x2dd4,1);assert(draw_navi()>0);
    e.Write(Env::Play+0x2dd4,0);
    e.Put<std::uint16_t>(Env::Link+0x1728,0);
    e.Put<std::uint8_t>(Env::Link+0x2227,1);
    for(unsigned i=0;i<12;++i)assert(draw_navi()>0);
    assert(!NaviCalling(e));
    e.Poll(Up,0);assert(input.pending_item==ElementCount);
    e.Touch(0,0,2,false);assert(e.Get<U32>(Main+0x38)==0);
    e.Put<std::uint8_t>(Env::Link+0x2227,0);assert(draw_navi()==0);
    e.Put<std::uint8_t>(Env::Link+0x2227,1);assert(draw_navi()==0); // No invented call.
    e.Write(Env::Play+0x2dd4,1);assert(draw_navi()>0);
    e.Write(Env::Play+0x2dd4,0);++generation;assert(draw_navi()==0);
    e.Write(Env::Play+0x2dd4,1);assert(draw_navi()>0);
    e.Write(Env::Play+0x2dd4,0);navi_time+=200000;assert(draw_navi()==0);
    e.Write(Env::Play+0x2dd4,1);assert(draw_navi()>0);
    e.Write(UI+0xc,0);assert(draw_navi()==0);
    e.Write(UI+0xc,Env::Play);e.Write(Env::Play+0x2dd4,0);assert(draw_navi()==0);
    assert(navi_geometry.uvs[12]==175.5f/256 && navi_geometry.uvs[11]==1-47.5f/128);
    // Follow the native UI UV convention through to PICA rows. Checking the
    // atlas alone missed the opposite-row sampling seen in the game: the eye
    // and ocarina quads displayed clipped boots from the other atlas row.
    e.Setup();g=MakeGeometry(e,{},1);
    constexpr std::array<unsigned,5> sx{0,128,192,128,192};
    constexpr std::array<unsigned,5> sy{0,0,0,64,64},extent{104,48,42,42,42};
    for(unsigned q=0;q<5;++q)for(unsigned vertex=0;vertex<4;++vertex) {
        const auto u=g.uvs[q*8+vertex*2],v=g.uvs[q*8+vertex*2+1];
        const unsigned x=unsigned(u*TextureWidth),y=unsigned((1-v)*TextureHeight);
        assert(x==sx[q]+(vertex>=2?extent[q]-1:0));
        assert(y==sy[q]+(vertex%2?extent[q]-1:0));
    }
    // Optional developer fixture: a flat virtual-memory image recovered from
    // a gameplay savestate. Run the actual HUD update and scene restrictions,
    // stopping after it publishes the action to the player's update.
    if(argc==3)for(auto action:{Look,DPad::Ocarina,Navi}) {
        e.bytes.resize(0x10000000);
        std::ifstream snapshot(argv[2],std::ios::binary);
        snapshot.read(reinterpret_cast<char*>(e.bytes.data()),e.bytes.size());assert(snapshot);
        input={};look_requested=false;e.view_down=false;SetLayout({},false);e.native_update=true;cpu.ClearCache();
        assert(CodeMatches(e) && Gameplay(e));
        e.Put<std::uint8_t>(Save+0x93,8); // Give this fixture an ocarina.
        const U32 play=e.Get<U32>(UI+0xc);
        const U32 button=action==Look?ViewButton:action==Navi?Up:Right;
        e.Put<std::int16_t>(play+0x2dd4,action==DPad::Ocarina?0:1);
        e.Poll(button,0);
        auto& r=cpu.Regs();r.fill(0);r[0]=play;r[13]=Env::SP;r[14]=Env::Stop;r[15]=0x42df18;
        cpu.SetCpsr(0x10);cpu.ClearHalt();e.ticks=1000000;cpu.Run();assert(e.ticks);
        assert(e.Get<U32>(Main+0x34)==1 && input.confirming==ElementCount);
        assert(e.Get<U32>(Main+0x60)==(action==DPad::Ocarina?1:0));
        assert(e.Get<U32>(Main+0x38)==(action==Look?1:action==Navi?2:0));
        std::cout<<"Full native HUD update published "<<(action==Look?"View":action==Navi?"Navi":"ocarina")<<" action\n";
    }
    std::cout<<"Stick-click View and D-pad Up Navi/Right ocarina actions, touchscreen release, boots toggles, edge handling, menu/ownership gates, icon swaps and orientation passed\n";
}
