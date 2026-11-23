// SPDX-License-Identifier: GPL-2.0-or-later
// Run the real ARM pacing loop and animation/repeat instructions in Dynarmic.
// Only the VBlank wait is stubbed; this is not a GPU/performance benchmark.
#include "oot3d/pause_framerate.h"
#include "oot3d/pause_description.h"
#include "oot3d/pause_header.h"
#include "oot3d/menu_cursor_draw.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
using namespace OOT3D;
using namespace OOT3D::PauseFramerate;
struct Env : Dynarmic::A32::UserCallbacks {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    Hook hook;
    MasterQuest::Hook quest;
    std::vector<U32> menu_sounds;
    U32 stop=0;
    bool patched=true,capture_cards=false,file_geometry_fixture=false,capture_save=false;
    unsigned prompt_allocations=0,prompt_creations=0,prompt_frees=0;
    std::map<unsigned,std::array<float,2>> save_offsets;
    std::map<unsigned,float> card_offsets;
    unsigned waits=0;
    bool capture_equip=false;
    std::int64_t now=0;
    struct Pulse {float alpha=0;int x=0,y=0,width=0,height=0;};
    std::map<U32,Pulse> pulses;
    std::uint64_t remaining=0;
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==stop)return 0xef000000;
        if(capture_equip && (a==0x2f8ce0 || a==0x2eb304 || a==0x2f8d40 || a==0x2eb3d8))
            return 0xef000006;
        if(capture_save && (a==0x2f9484 || a==0x2f9430 || a==0x313ce0 || a==0x2f57f0 ||
           a==0x2e9768 || a==0x2f6944 || a==0x3525d4 || a==0x2fd84c || a==0x2e9920 || a==0x2f87ec))
            return 0xef000005;
        if(file_geometry_fixture && a==0x2eee84)return 0xef000004;
        if(a==0x37547c)return 0xef000002; // Observe the native menu sound call.
        if(capture_cards && a==0x2f9430)return 0xef000003;
        if(a==0x3101dc)return 0xef000001; // Hardware VBlank boundary.
        return patched?MasterQuest::Instruction(a,FileSelectTiming::Instruction(a,Instruction(a,Get<U32>(a)))):Get<U32>(a);
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
        if(!svc) {cpu->HaltExecution();return;}
        if(svc==6) {
            auto& r=cpu->Regs();const U32 pc=r[15]-4;
            if(pc==0x2f8ce0)pulses[r[0]].alpha=std::bit_cast<float>(cpu->ExtRegs()[0]);
            if(pc==0x2eb304) {
                Put<float>(r[1],267);Put<float>(r[2],7+48*(r[0]/6));
            }
            if(pc==0x2f8d40) {
                auto& p=pulses[r[0]];p.x=int(r[2]);p.y=int(r[3]);
                p.width=int(*Read(r[13]));p.height=int(*Read(r[13]+4));
            }
            r[15]=r[14];return;
        }
        if(svc==5) {
            auto& r=cpu->Regs();const auto pc=r[15]-4;
            if(pc==0x2f9484)Put<std::uint8_t>(r[2],0);
            if(pc==0x313ce0) {++prompt_allocations;r[0]=0xa80000;}
            if(pc==0x2f57f0) {++prompt_creations;assert(r[0]==0xa80000);}
            if(pc==0x2f6944)++prompt_frees;
            if(pc==0x2f9430)save_offsets[r[3]]={Get<float>(r[1]),Get<float>(r[1]+4)};
            if(pc==0x2e9920)Write(SaveMenu::Owner+0x28,0);
            r[15]=r[14];return;
        }
        if(svc==1) {++waits;Write(0x54cc3c,*Read(0x54cc3c)+1);cpu->Regs()[15]=cpu->Regs()[14];return;}
        if(svc==4) {auto& r=cpu->Regs();r[0]=0xb10000+r[0]*0x2000;r[15]=r[14];return;}
        if(svc==3) {
            auto& r=cpu->Regs();card_offsets[r[3]]=Get<float>(r[1]);r[15]=r[14];return;
        }
        if(svc==2) {
            const auto& r=cpu->Regs();menu_sounds.push_back(r[0]);
            assert(r[1]==0 && r[2]==4 && r[3]==Read(0x43045c));
            assert(Read(r[13])==Read(0x43045c) && Read(r[13]+4)==Read(0x430458));
            cpu->Regs()[15]=r[14];return;
        }
        if(svc>=MasterQuest::FirstSVC && svc<MasterQuest::FirstSVC+MasterQuest::Sites.size()) {
            quest.Handle(*this,cpu->Regs(),svc-MasterQuest::FirstSVC);return;
        }
        U32 cpsr=cpu->Cpsr();
        if(svc>=FileSelectTiming::FirstSVC && svc<FileSelectTiming::FirstSVC+FileSelectTiming::Sites.size())
            hook.file.Handle(*this,cpu->Regs(),cpu->ExtRegs(),svc-FileSelectTiming::FirstSVC,&cpsr);
        else if(svc>=SaveFirstSVC && svc<SaveFirstSVC+SaveSites.size())
            hook.HandleSave(*this,cpu->Regs(),cpu->ExtRegs(),cpsr,svc-SaveFirstSVC);
        else {
            assert(svc>=FirstSVC && svc<FirstSVC+Sites.size());
            hook.Handle(*this,cpu->Regs(),cpu->ExtRegs(),cpsr,svc-FirstSVC,now);
        }
        cpu->SetCpsr(cpsr);
    }
    void Run(U32 pc,U32 end) {
        stop=end;cpu->ClearCache();cpu->Regs()[15]=pc;remaining=100000;
        cpu->Run();assert(cpu->Regs()[15]==end+4);
    }
    void Pause(bool on) {
        Write(PauseMenu::UI+0xc,0x800000);Write(PauseMenu::UI+0x14,1);
        Write(PauseMenu::UI+0x20,on?0:1);Write(PauseMenu::UI+0x24,1);Write(0x800100,3);
        Write(SaveMenu::Owner+0x28,0);
    }
    void Begin() {cpu->Regs()[1]=0x54cc34;cpu->Regs()[5]=Renderer;Run(0x41af70,0x41af7c);}
    void End() {cpu->Regs()[4]=Renderer;cpu->Regs()[5]=0x54cc34;Run(0x3fd3a0,0x3fd3d0);}
};
int main(int argc,char** argv) {
    assert(argc==2 || (argc==3 && (std::string_view(argv[2])=="--overwrite-only" || std::string_view(argv[2])=="--save-only" || std::string_view(argv[2])=="--boss-only" || std::string_view(argv[2])=="--equip-only")));Env e;
    std::ifstream in(argv[1],std::ios::binary);assert(in);
    in.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),e.bytes.size()-0x100000);
    assert(CodeMatches(e) && PauseDescription::CodeMatches(e));
    for(const auto& site:Sites) {
        const auto old=*e.Read(site.pc+4);e.Write(site.pc+4,0);assert(!CodeMatches(e));e.Write(site.pc+4,old);
    }
    for(const auto& site:SaveSites) {
        const auto old=*e.Read(site.code.pc+4);e.Write(site.code.pc+4,0);
        assert(!CodeMatches(e));e.Write(site.code.pc+4,old);
    }
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu{config};e.cpu=&cpu;cpu.SetCpsr(0x10);
    auto& r=cpu.Regs();auto& f=cpu.ExtRegs();
    // Execute the complete original landing-effect state, stubbing only its
    // graphics helpers. Compare real 30 Hz samples against 60 Hz interpolation,
    // including both swapped copies and the actual native cleanup branch.
    std::vector<std::map<U32,Env::Pulse>> original;
    for(bool fast:{false,true}) {
        e.hook={};e.patched=fast;e.capture_equip=true;e.Pause(true);
        constexpr U32 root=0x5066f8;
        e.Write(root+0x34,10);e.Write(root+0xb0,0xa10000);
        e.Write(root+0x44,0xa20000);e.Write(root+0x48,0xa30000);
        e.Write(root+0xb8,5);e.Write(root+0xbc,11);
        e.now=1000000;e.Begin();r[4]=root;r[6]=0;
        e.Run(0x4346d4,0x4346d8);e.Write(root+0x34,11);
        const unsigned hz=fast?60:30,frames=fast?12:6;
        for(unsigned tick=1;tick<=frames;++tick) {
            e.now=1000000+(1000000LL*tick+hz/2)/hz;e.Begin();
            const unsigned repeats=fast && tick==5?3:1;
            for(unsigned repeat=0;repeat<repeats;++repeat) {
                r[4]=root;r[13]=0xbf0000;e.pulses.clear();
                e.Run(0x434878,0x434b40);
                assert(e.Read(root+0x34)==(tick==frames?2u:11u));
                if(!fast)original.push_back(e.pulses);
                for(U32 owner:{0xa20000u,0xa30000u}) {
                    const auto& p=e.pulses.at(owner);
                    if(fast && tick%2==0) {
                        const auto& native=original[tick/2-1].at(owner);
                        assert(std::abs(p.alpha-native.alpha)<.0001f);
                        if(p.x!=native.x || p.y!=native.y)
                            std::cerr<<"Landing tick "<<tick<<": "<<p.x<<','<<p.y<<" vs "<<native.x<<','<<native.y
                                     <<" phase="<<e.hook.LandingFrame()<<'\n';
                        assert(p.x==native.x && p.y==native.y);
                        assert(p.width==native.width && p.height==native.height);
                    }
                    if(fast && tick>2 && tick<frames && tick%2) {
                        const auto& before=original[tick/2-1].at(owner);
                        const auto& after=original[tick/2].at(owner);
                        assert(std::abs(p.alpha-(before.alpha+after.alpha)*.5f)<.0001f);
                        // The final native frame hides/clears the rectangle;
                        // interpolate the expansion, never shrink toward zero.
                        assert(p.width==42+3*int(tick-2));
                    }
                }
            }
        }
        assert(e.Read(root+0xb8)==0xffffffff && e.Read(root+0xbc)==0xffffffff);
    }
    std::cout<<"Native landing pulse: original 0.2s duration, both swapped copies, 60 Hz intermediate size/fade, duplicate-update protection and cleanup passed\n";
    if(argc==3 && std::string_view(argv[2])=="--equip-only")return 0;
    e.hook={};e.patched=true;e.capture_equip=false;e.now=0;
    // Execute the real accepted-Overwrite branch after native prompt cleanup.
    // Stop before any save I/O; slot identities and unrelated registers remain
    // native. The disabled path must still select the original second prompt.
    assert(FileSelectTiming::CodeMatches(e));
    for(bool enabled:{false,true}) {
        e.hook.file={};e.hook.file.active=enabled;
        e.Write(FileSelect::Owner+0x10,17);e.Write(FileSelect::Owner+0x20,1);
        e.Write(FileSelect::Owner+0x14,2);e.Write(FileSelect::Owner+0x1c,0);
        r[6]=FileSelect::Owner;r[13]=0xbf0000;r[9]=0x12345678;
        e.Run(0x431afc,0x431b20);
        assert(e.Read(FileSelect::Owner+0x10)==(enabled?24u:20u));
        assert(e.Read(FileSelect::Owner+0x20)==(enabled?0u:1u));
        assert(e.Read(FileSelect::Owner+0x14)==2 && e.Read(FileSelect::Owner+0x1c)==0);
        assert(r[9]==0x12345678 && r[13]==0xbf0000);
    }
    e.hook.file.active=true;e.Write(FileSelect::Owner+0x10,17);
    r[6]=FileSelect::Owner;r[3]=18;
    e.Run(0x431ce0,0x431ce4);
    assert(e.Read(FileSelect::Owner+0x10)==18 && FileSelect::exchange==FileSelect::Exchange::BackOverwrite);
    e.Write(FileSelect::Owner+0x10,14);r[0]=16;
    e.Run(0x4321c8,0x4321cc);
    assert(e.Read(FileSelect::Owner+0x10)==16 && FileSelect::exchange==FileSelect::Exchange::Overwrite);
    assert(FileSelect::exchange_frames==1);
    for(unsigned frame=1;frame<10;++frame) {
        e.hook.file.Frame(true);assert(FileSelect::exchange_frames==frame+1);
    }
    // Real native prompt factory deduplicates an early translated message;
    // its later stock call must not allocate a second controller.
    e.capture_save=true;
    for(unsigned destination=0;destination<3;++destination) {
        const U32 slot=FileSelect::TransitionPromptSlot(16,false,destination);
        assert(slot==10+destination);e.Write(0x55b548+4*slot,0);
        const auto before=e.prompt_allocations;
        r[0]=slot;r[13]=0xbf0000;r[14]=0xb00000;e.Run(0x2ee864,0xb00000);
        assert(e.prompt_allocations==before+1);
        r[0]=slot;r[13]=0xbf0000;r[14]=0xb00000;e.Run(0x2ee864,0xb00000);
        assert(e.prompt_allocations==before+1);
    }
    e.capture_save=false;
    std::cout<<"Native overwrite: first-choice bypass, original/Cancel guards, 60 Hz clock and early prompt reuse passed\n";
    if(argc==3 && std::string_view(argv[2])=="--overwrite-only")return 0;
    e.hook.file={};FileSelect::exchange=FileSelect::Exchange::None;
    // The first native Erase choice still selects state 39 for either A or
    // touch input. After its prompt cleanup, skip the second confirmation and
    // enter the original accepted-erase path. Never perform disk I/O here.
    for(unsigned touch:{0u,1u}) {
        r[6]=FileSelect::Owner;r[4]=touch;
        e.Write(FileSelect::Owner+0x10,37);e.Write(FileSelect::Owner+0x24,1);
        e.Run(0x432bcc,0x432bf4);
        assert(e.Read(FileSelect::Owner+0x10)==39);
        e.hook.file.active=true;
        e.Run(0x432d9c,0x4336f8);
        assert(e.Read(FileSelect::Owner+0x10)==42);
        assert(e.Read(FileSelect::Owner+0x24)==0 && e.Read(FileSelect::Owner+0x2c)==0);
        assert(e.Read(FileSelect::Owner+0x54)==0 && e.Read(0x55b538)==0);
    }
    e.hook.file.active=false;e.Write(FileSelect::Owner+0x10,39);
    e.Run(0x432d9c,0x432da0);assert(r[0]==1 && e.Read(FileSelect::Owner+0x10)==39);
    e.hook.file.active=true;e.Write(FileSelect::Owner+0x10,38); // Cancel path cannot become Erase.
    e.Run(0x432d9c,0x432da0);assert(e.Read(FileSelect::Owner+0x10)==38);
    e.hook.file.active=false;
    std::cout<<"Native first-Erase A/touch routing, redundant-confirmation bypass and disabled/cancel guards passed\n";
    // Execute the native confirmation translation loop for every 60 Hz frame.
    // Only mesh submission is mocked; it records the actual offsets for all
    // normal faces/labels and their separately parked pressed variants.
    e.capture_cards=true;
    for(U32 state:{36u,38u,39u}) {
        e.hook.file={};e.Write(FileSelect::Owner+0x10,state);
        e.Write(0x55b538,state==36?5:0);float last=state==36?501.f:0.f;
        for(unsigned frame=0;frame<10;++frame) {
            e.hook.file.Frame(true);r[6]=FileSelect::Owner;
            e.Run(0x42f9d0,0x42f9d4); // No alternate frame may be skipped.
            if(state==36)e.Run(0x432968,0x432978); // Real native decrement/store.
            else {r[4]=0x55b524;e.Run(0x432a10,0x432a20);}
            if(state!=36)assert(bool(cpu.Cpsr()&0x40000000)==(frame==9));
            r[13]=0xbf0000;r[14]=0xb00000;r[0]=state==39?U32(-1):1;
            e.card_offsets.clear();e.Run(0x2ed908,0xb00000);
            const float x=e.card_offsets.at(134);
            assert(e.card_offsets.at(137)==x && e.card_offsets.at(141)==x && e.card_offsets.at(144)==x);
            assert(e.card_offsets.at(138)==500 && e.card_offsets.at(145)==500);
            assert(state==38?x>last:x<last);last=x;
        }
        assert(std::abs(last-(state==36?0:state==38?500:-500))<.0001f);
    }
    // Cleanup after bypassing the second confirmation keeps these stones left
    // of the viewport instead of replaying the departure from the center.
    e.Write(FileSelect::Owner+0x10,42);e.Write(0x55b538,1);
    r[13]=0xbf0000;r[14]=0xb00000;r[0]=U32(-1);e.Run(0x2ed908,0xb00000);
    assert(e.card_offsets.at(134)==-500 && e.card_offsets.at(141)==-500);
    e.capture_cards=false;e.hook.file={};
    std::cout<<"Native Cancel/Erase slides: ten distinct eased 60 Hz steps, correct directions, parked variants and no cleanup re-entry passed\n";
    // Run the real accepted-state stores. A and touch converge here; unrelated
    // writes at the shared sites must retain their native behavior.
    using Exchange=FileSelect::Exchange;
    struct ExchangeCase {unsigned site;U32 old,next;Exchange mode;};
    for(const auto c:{ExchangeCase{32,8,10,Exchange::Start},{32,8,12,Exchange::Copy},
                     {32,8,34,Exchange::Erase},{33,37,38,Exchange::BackErase},
                     {34,14,15,Exchange::BackCopy},{32,8,9,Exchange::None},
                     {34,17,15,Exchange::None}}) {
        e.hook.file={};e.hook.file.active=true;FileSelect::exchange=Exchange::None;
        e.Write(FileSelect::Owner+0x10,c.old);e.Write(FileSelect::Owner+0x54,4);
        r[6]=FileSelect::Owner;r[0]=c.next;
        const auto pc=FileSelectTiming::Sites[c.site].pc;e.Run(pc,pc+4);
        assert(e.Read(FileSelect::Owner+0x10)==c.next && FileSelect::exchange==c.mode);
        if(c.mode==Exchange::None) {assert(e.Read(FileSelect::Owner+0x54)==4);continue;}
        assert(FileSelect::exchange_frames==1 && e.Read(FileSelect::Owner+0x54)==0);
        for(unsigned frame=2;frame<=10;++frame) {
            e.hook.file.Frame(true);assert(FileSelect::exchange_frames==frame);
        }
        e.hook.file.Frame(true);assert(FileSelect::exchange_frames==10);
    }
    // After the shared slide, native prompt setup runs once without another
    // entrance. Verify the actual ARM counter branches and settled file data.
    for(auto mode:{Exchange::Erase,Exchange::Copy,Exchange::BackErase,Exchange::BackCopy}) {
        e.hook.file={};e.hook.file.active=true;FileSelect::BeginExchange(mode);
        FileSelect::exchange_frames=10;r[6]=FileSelect::Owner;
        const U32 state=mode==Exchange::Erase?36:mode==Exchange::Copy?13:7;
        e.Write(FileSelect::Owner+0x10,state);e.Write(FileSelect::Owner+0x14,1);
        e.Write(FileSelect::Owner+0x28,0);e.Write(0x55b534,5);e.Write(0x55b538,5);
        for(unsigned i=0;i<3;++i) {e.Put<float>(0x55b5f8+4*i,416);e.Put<float>(0x55b610+4*i,-80);}
        e.Run(0x42f9d0,0x42f9d4);
        if(mode==Exchange::Erase) {
            e.Run(0x432968,0x432978);assert(e.Read(0x55b538)==0 && (cpu.Cpsr()&0x40000000));
        } else if(mode==Exchange::Copy) {
            e.Run(0x431184,0x431194);assert(e.Read(FileSelect::Owner+0x28)==5 && (cpu.Cpsr()&0x40000000));
            for(unsigned i:{0u,2u}) {
                assert(e.Get<float>(0x55b5f8+4*i)==16 && e.Get<float>(0x55b604+4*i)==(i==0?56:116));
                assert(e.Read(0x55b610+4*i)==0 && e.Read(0x55b61c+4*i)==0);
            }
        } else {
            r[5]=0x55b524;e.Run(0x430500,0x43050c);
            assert(e.Read(0x55b534)==0 && (cpu.Cpsr()&0x40000000));
            if(mode==Exchange::BackCopy)assert(e.Get<float>(0x55b5fc)==16 && e.Get<float>(0x55b608)==56 && e.Read(0x55b614)==0);
        }
        e.Write(FileSelect::Owner+0x10,mode==Exchange::Erase?37:mode==Exchange::Copy?14:8);
        e.Run(0x42f9d0,0x42f9d4);assert(FileSelect::exchange==Exchange::None);
    }
    e.hook.file={};FileSelect::exchange=Exchange::None;FileSelect::exchange_frames=0;
    std::cout<<"Native shared exchanges: accepted input, ten-frame clock, B directions, prompt handoff and unrelated-store guards passed\n";
    // Early Erase/Back text uses the native prompt slots. The later native
    // constructor must reuse them rather than allocate or replace them.
    assert(e.Read(0x2eed68)==0x957); // Native "Delete this file?" message.
    for(U32 slot:{7u,19u}) {
        constexpr U32 controller=0x900000;
        e.Write(0x55b548+slot*4,controller);
        r[0]=slot;r[13]=0xbf0000;r[14]=0xb00000;
        e.Run(0x2ee864,0xb00000);
        assert(e.Read(0x55b548+slot*4)==controller && r[13]==0xbf0000);
        e.Write(0x55b548+slot*4,0);
    }
    // Each native swap must consume one VBlank when paused and two otherwise.
    // Start/finish accounting uses the same latched duration even if UI closes.
    for(bool pause:{false,true}) {
        e.Pause(pause);e.Write(0x54cc3c,0);e.Write(Renderer+0x88,0);e.waits=0;
        const unsigned frames=pause?60:30;
        for(unsigned n=0;n<frames;++n) {e.Begin();e.End();}
        assert(e.waits==60 && *e.Read(0x54cc3c)==60 && *e.Read(Renderer+0x88)==60);
    }
    {
    const auto saved_bytes=e.bytes;const auto saved_regs=r;const auto saved_ext=f;
    const auto saved_cpsr=cpu.Cpsr();const auto saved_hook=e.hook;
    // Boss selector runs at 60; gameplay and unloaded layouts retain 30.
    e.Pause(false);
    e.Write(BossChallenge::NativeLayout+4,0x900000);
    for(U32 state:{0u,1u,4u,7u,8u,10u,13u}) {
        e.Write(BossChallenge::Owner+8,0x10100|state);
        e.Write(BossChallenge::Owner+0x3ec,0xffffffff);
        e.Write(0x54cc3c,0);e.Write(Renderer+0x88,0);e.waits=0;
        const bool selector=(state>=1 && state<=7) || state==13;
        for(unsigned n=0;n<60;++n){e.Begin();assert(e.hook.boss_fast==selector);e.End();}
        assert(e.waits==(selector?60:120));
    }
    e.Write(BossChallenge::Owner+8,0x10104);
    e.Write(BossChallenge::Owner+0x3ec,0);e.Begin();assert(!e.hook.boss_fast);
    e.Write(BossChallenge::Owner+8,0);e.Write(BossChallenge::NativeLayout+4,0);
    // Run the actual counter loop: double updates retain the same repeat rate.
    for(bool fast:{false,true}) {
        e.hook.boss_fast=fast;
        unsigned repeats=0;
        for(unsigned n=0;n<(fast?120:60);++n) {
            r[5]=BossChallenge::Owner;r[0]=0x900000;r[1]=BossChallenge::Owner+0x1800;
            e.Write(0x900014,1);e.Write(0x900018,n==0?1:0);
            e.Run(0x41d518,0x41d56c);
            if(*e.Read(BossChallenge::Owner+0x18b4)==0)++repeats;
        }
        assert(repeats==12);
    }
    std::cout<<"Boss selector: 60 Hz native pacing, 30 Hz battle guard, exit fade and unchanged held-input repeat timing passed\n";
    if(argc==3 && std::string_view(argv[2])=="--boss-only")return 0;
    e.bytes=saved_bytes;r=saved_regs;f=saved_ext;cpu.SetCpsr(saved_cpsr);e.hook=saved_hook;
    }
    // Run the native cursor placement routine, then match the same selection
    // through either backing buffer used to submit its corners.
    {
        using namespace Interpolation;
        constexpr U32 item_root=0x5066f8,owner=0xc00000,model=0xc01000,mesh=0xc02000;
        constexpr U32 helper=0xc03000,origins=0xc04000,live=0xc06000,alternate=0xc07000;
        e.Write(owner,mesh);e.Write(owner+4,model);e.Write(model+4,mesh);e.Write(model+0x174,48);
        e.Write(owner+8,helper);e.Write(owner+12,8);e.Write(helper+0x1c,origins);
        e.Write(mesh+0x1a0,alternate);e.Write(mesh+0x1a4,live);
        const auto resolve=[&](U32 at){return e.bytes.data()+at;};
        for(const U32 offset:{0x30u,0x28u,0x2cu,0xb0u,0xb4u}) {
            const auto saved=*e.Read(item_root+offset);
            e.Write(item_root+offset,owner);
            const auto x=std::bit_cast<U32>(float(offset)),y=std::bit_cast<U32>(48.f);
            f[0]=x;f[1]=y;r[0]=owner;r[13]=0xbf0000;r[14]=0x700000;
            e.Run(0x2f7af4,0x700000);
            const std::array<U32,3> key{owner,x,y};
            for(U32 bank:{0u,1u}) {
                e.Write(mesh+0x124,bank);
                MenuCursor::Capture(e,MenuPage::Items,model,model,resolve);
                const auto buffer=bank?live:alternate;
                assert(MenuCursor::Selection(resolve(buffer),48,guest_frames,generation)==key);
                assert(MenuCursor::Selection(resolve(buffer),48,guest_frames+1,generation)==key);
                assert(!MenuCursor::Selection(resolve(buffer),48,guest_frames+2,generation)[0]);
                assert(!MenuCursor::Selection(resolve(buffer),48,guest_frames,generation+1)[0]);
                assert(!MenuCursor::Selection(resolve(buffer),42,guest_frames,generation)[0]);
            }
            e.Write(item_root+offset,saved);
        }
        // Change only the native focus, while keeping the same model, UVs and
        // DMA pointer. A selected floor can also be submitted on a copied mesh.
        const auto saved_bytes=e.bytes;
        constexpr U32 copy=0xc08000,copy_mesh=0xc09000,copy_live=0xc0a000;
        e.Write(copy+4,copy_mesh);e.Write(copy+0x174,300);e.Write(copy_mesh+0x124,0);
        e.Write(copy_mesh+0x1a0,copy_live);
        const auto check=[&](MenuPage page,U32 slot,U32 selection,U32 draw,U32 stream,U32 indices) {
            e.Write(slot,owner);
            std::array<U32,3> previous{};
            for(U32 selected:{1u,2u}) {
                e.Write(selection,selected);
                MenuCursor::Capture(e,page,draw,model,resolve);
                const auto key=MenuCursor::Selection(resolve(stream),indices,guest_frames,generation);
                assert(key[0]==owner && key!=previous);
                previous=key;
            }
            e.bytes=saved_bytes;
        };
        check(MenuPage::Map,0x506cb0+0x1c,0x506cb0+0x30,copy,copy_live,300);
        check(MenuPage::Map,0x506cb0+0x4c,0x506cb0+0x48,model,live,48);
        check(MenuPage::Gear,0x50446c+0x14,0x50446c+0x20,model,live,48);
        e.Write(0x504fa0+0x10,3);
        check(MenuPage::FileSelect,0x504fa0+0x34,0x504fa0+0x14,model,live,48);
        e.Write(0x5077f0+8,0);e.Write(0x5077f0+0x14,0);
        check(MenuPage::NameEntry,0x5077f0,0x5077f0+0x10,model,live,48);
    }
    // Identify both sides of a tab switch even when they share a source rate.
    e.Pause(true);e.Write(0x5066f8+0x34,0);e.Write(0x50446c+0x18,2);e.Begin();
    assert(Interpolation::menu_page==Interpolation::MenuPage::Gear);
    e.Write(0x50446c+0x18,0);
    e.Pause(true);e.Write(0x5066f8+0x34,1);e.Begin();assert(Interpolation::menu_page==Interpolation::MenuPage::Items);
    e.Write(SaveMenu::Owner+0x28,1);e.Begin();assert(Interpolation::menu_page==Interpolation::MenuPage::Save);
    e.Write(SaveMenu::Owner+0x28,0);e.Write(0x5066f8+0x34,0);e.Begin();
    assert(Interpolation::menu_page==Interpolation::MenuPage::Map);
    for(const auto page:{Interpolation::MenuPage::Gear,Interpolation::MenuPage::Items,Interpolation::MenuPage::Map}) {
        e.Write(0x5066f8+0x34,page==Interpolation::MenuPage::Items?2:0);
        e.Write(0x50446c+0x18,page==Interpolation::MenuPage::Gear?2:0);
        for(const int target:{60,120}) {
            Interpolation::target_fps=target;e.waits=0;
            for(unsigned tick=0;tick<60;++tick) {
                e.Begin();assert(Interpolation::menu_page==page && e.hook.fast);e.End();
            }
            assert(e.waits==60);
        }
    }
    Interpolation::target_fps=60;
    e.Pause(true);e.Begin();e.Pause(false);e.waits=0;e.End();assert(e.waits==1);
    e.Begin();assert(!e.hook.fast);
    for(U32 save:{1u,2u,3u}) {
        e.Pause(true);e.Write(SaveMenu::Owner+0x28,save);
        e.Write(0x54cc3c,0);e.Write(Renderer+0x88,0);e.waits=0;
        for(int rate:{60,90,120}) {
            Interpolation::target_fps=rate;
            Interpolation::RateClock clock;unsigned presented=0;
            for(unsigned n=0;n<60;++n) {
                e.Begin();assert(e.hook.fast && Interpolation::menu_page==Interpolation::MenuPage::Save);
                presented+=clock.Next(n,rate,Interpolation::SourceFPS(Interpolation::menu_page)).count;
                e.End();
            }
            assert(presented==unsigned(rate));
        }
        Interpolation::target_fps=60;
        assert(e.waits==180 && *e.Read(Renderer+0x88)==180);
        e.Pause(false);e.Write(SaveMenu::Owner+0x28,save);e.Begin();assert(!e.hook.fast);
    }
    e.Pause(true);e.Write(0x800100,2);e.Begin();assert(!e.hook.fast);
    e.Pause(true);e.Begin();
    // Game Over draws transitions and selectable stones on the menu clock.
    constexpr U32 death=0x890000;
    e.Write(DeathMenu::ControllerPointer,death);e.Write(death+DeathMenu::LayoutOffset+4,0x891000);
    for(U32 state:{0u,1u,3u,4u,5u,7u,9u,10u,11u,12u,13u,14u,15u,16u,17u}) {
        e.Pause(false);e.Write(death+8,0x10000|state);
        e.Write(0x54cc3c,0);e.Write(Renderer+0x88,0);e.waits=0;
        const bool animated=DeathMenu::Moving(state) || state==4 || state==10 || state==14;
        unsigned extras=0,presented=0;
        Interpolation::RateClock clock;
        for(unsigned n=0;n<60;++n) {
            e.Begin();assert(e.hook.death_fast==animated);
            assert(Interpolation::menu_page==(animated?Interpolation::MenuPage::Death:Interpolation::MenuPage::None));
            if(animated)presented+=clock.Next(n,120,Interpolation::SourceFPS(Interpolation::menu_page)).count;
            extras+=DeathMenu::extra_frame;e.End();
        }
        assert(e.waits==(animated?60:120));
        assert(extras==(animated?30:0));
        assert(presented==(animated?120:0));
    }
    e.Write(death+8,0);e.Pause(true);e.Begin();
    // Sixty distinct native matrix angles cover exactly the old 30-degree turn.
    constexpr U32 preview=0x900000;
    e.Put<std::uint16_t>(preview+0x118,0);
    for(unsigned n=0;n<60;++n) {
        r[4]=preview;r[9]=preview+0x100;r[0]=e.Get<std::uint16_t>(preview+0x118);
        e.Run(0x4256fc,0x425708);assert(std::bit_cast<float>(f[0])==float(n)*.5f);
        e.Run(0x4258c4,0x4258d0);
    }
    assert(e.Get<std::uint16_t>(preview+0x118)==30);
    // Slider scales native angular increments, including fractional speeds.
    const auto rotation_layout=PauseMenu::GetLayout();
    for(float speed:{.25f,1.3f,2.f,5.f}) {
        auto layout=rotation_layout;layout.preview_rotation_speed=speed;
        PauseMenu::SetLayout(layout,false);
        e.hook.preview_owner=0;e.hook.preview_fraction=0;
        e.Put<std::uint16_t>(preview+0x118,0);
        for(unsigned n=0;n<120;++n) {
            r[4]=preview;r[9]=preview+0x100;r[0]=e.Get<std::uint16_t>(preview+0x118);
            e.Run(0x4256fc,0x425708);
            assert(std::abs(std::bit_cast<float>(f[0])-float(n)*.5f*speed)<.001f);
            e.Run(0x4258c4,0x4258d0);
        }
    }
    // Updating the slider preserves the angle already accumulated.
    const float before=e.Get<std::uint16_t>(preview+0x118)+e.hook.preview_fraction;
    PauseMenu::SetLayout(rotation_layout,false);
    r[4]=preview;r[9]=preview+0x100;r[0]=e.Get<std::uint16_t>(preview+0x118);
    e.Run(0x4256fc,0x425708);assert(std::bit_cast<float>(f[0])==before);
    assert(PauseMenu::PreviewRotationSpeed(0)==.25f);
    assert(PauseMenu::PreviewRotationSpeed(100)==5.f);
    assert(PauseMenu::PreviewRotationSpeed(std::bit_cast<float>(0x7fc00000u))==1.f);
    e.Pause(false);e.Begin();assert(e.hook.preview_fraction==0 && e.hook.preview_owner==0);
    e.Pause(true);e.Begin();
    // All flight axes and both directions reach the same easing envelope in
    // twelve native steps that the old flight covered in six. No held frames.
    for(unsigned site=4;site<=7;++site)for(int delta:{-240,-50,50,240}) {
        e.hook.remainder={};int pos=260,target=260+delta,old=pos;
        unsigned changes=0;
        for(unsigned n=0;n<12;++n) {
            r[0]=pos;r[1]=U32(target-pos);e.Run(Sites[site].pc,Sites[site].pc+8);
            changes+=int(r[0])!=pos;pos=int(r[0]);
            if(n%2==1) {old+=(target-old)/2;assert(std::abs(pos-old)<=2);}
        }
        assert(changes>=8 && std::abs(pos-target)<=5);
    }
    // Real conditional branches/repeat stores: twice as many updates, same time.
    for(bool fast:{false,true})for(bool pad:{false,true}) {
        e.hook.fast=fast;constexpr U32 input=0x50af0c;
        e.Write(input+(pad?0x10:0x18),0);e.Write(input+(pad?0x14:0x1c),0);
        unsigned first=0,previous=0;
        for(unsigned n=1;n<=60;++n) {
            r[0]=0;r[4]=r[5]=0x80;r[6]=input;r[7]=0;
            e.Run(pad?0x42dc2c:0x42dc7c,pad?0x42dc58:0x42dca8);
            if(r[0]) {
                if(!first)first=n;
                if(previous)assert(n-previous==(fast?6u:3u));
                previous=n;
            }
        }
        assert(first==(fast?30u:15u));
    }
    // Native effect conversions use half-frame values, for opacity AND both
    // dimensions/offsets; completion still occurs after 0.2 seconds.
    for(unsigned i=14;i<28;++i) {
        const U32 word=Sites[i].instruction;
        const unsigned dst=((word>>12)&15)*2+((word>>22)&1),src=(word&15)*2+((word>>5)&1);
        for(bool fast:{false,true}) {
            e.hook.fast=fast;f[src]=9;e.Run(Sites[i].pc,Sites[i].pc+4);
            assert(std::bit_cast<float>(f[dst])==(fast?4.5f:9.f));
        }
    }
    for(bool fast:{false,true}) {
        e.hook.fast=fast;r[4]=0x5066f8;e.Write(r[4]+0x80,0);
        unsigned n=0;
        do {e.Run(0x434a04,0x434a14);++n;}while(!(cpu.Cpsr()&0x40000000) && n<20);
        assert(n==(fast?12u:6u));
        e.Write(r[4]+0x5c,0);n=0;
        do {
            e.Run(0x4344a0,0x4344b4);++n;assert(n<=12);
        } while((cpu.Cpsr()&0x40000000) || ((cpu.Cpsr()>>31)&1)!=((cpu.Cpsr()>>28)&1));
        assert(r[0]==10 && n==(fast?12u:6u)); // Native MOVGT selects equip-commit.
    }
    // Every non-pacing hook replays the original instruction exactly when
    // inactive, including NZCV and untouched registers.
    for(unsigned i=2;i<Sites.size();++i) {
        std::array<U32,16> regs{};std::array<U32,64> ext{};
        for(unsigned j=0;j<regs.size();++j)regs[j]=j+37;
        for(unsigned j=0;j<ext.size();++j)ext[j]=j+11;
        // These unconditional Save menu redirects have their own destinations.
        if(i==38 || i==39) {
            r=regs;r[4]=SaveMenu::Owner;e.patched=true;e.Write(r[4]+0x38,1);
            e.Run(Sites[i].pc,i==38?0x4395e8:0x439720);
            if(i==38)assert(*e.Read(r[4]+0x38)==0);
            else assert(r[3]==0);
            continue;
        }
        regs[15]=Sites[i].pc;const U32 flags=0x90000010;
        r=regs;f=ext;cpu.SetCpsr(flags);e.patched=false;
        e.Run(Sites[i].pc,Sites[i].pc+4);const auto native_r=r;const auto native_f=f;const auto native_c=cpu.Cpsr();
        r=regs;f=ext;cpu.SetCpsr(flags);e.patched=true;e.hook.fast=false;
        e.Run(Sites[i].pc,Sites[i].pc+4);assert(r==native_r && f==native_f && cpu.Cpsr()==native_c);
    }
    // Shared pane delta preserves the previous double-speed description open.
    PauseDescription::Hook description;
    f[17]=std::bit_cast<U32>(1.f);description.Handle(e,r,f,5,true,true);
    assert(std::bit_cast<float>(f[0])==.5f);
    // Execute every actual Save/Options position calculation from its timer
    // load to float conversion. Even frames must equal native 30 Hz positions;
    // odd frames must evaluate the intervening half step, including offsets.
    constexpr std::array<U32,22> save_starts{
        0x43933c,0x439378,0x4393bc,0x4397b0,0x4397e8,0x439828,
        0x43a0d0,0x43a108,0x43a148,0x43a204,0x43a244,0x43a2b8,
        0x43a2f8,0x43a4ac,0x43a4ec,0x43ab78,0x43abb8,0x43abf0,
        0x43af38,0x43af7c,0x43afe0,0x43b020};
    e.Pause(true);e.Write(SaveMenu::Owner+0x28,3);e.Begin();
    unsigned expression=0;
    for(const auto& site:SaveSites) {
        if(site.code.instruction!=0xeeb80ac0) {
            const U32 limit=site.code.instruction&255;
            for(U32 n=0;n<=limit*2;++n) {
                r[0]=n;e.Run(site.code.pc,site.code.pc+4);
                assert(bool(cpu.Cpsr()&0x20000000)==(n>=limit*2));
                assert(bool(cpu.Cpsr()&0x40000000)==(n==limit*2));
            }
            continue;
        }
        const auto start=save_starts[expression++];
        const auto position=[&](unsigned frame,bool patched) {
            e.patched=patched;r[5]=SaveMenu::Owner;r[6]=U32(-168);r[13]=0x9f0000;
            e.Write(SaveMenu::Owner+0x2c,frame);
            e.Run(start,site.code.pc+4);return std::bit_cast<float>(f[0]);
        };
        for(unsigned n=0;n<=8;++n) {
            const float a=position(n/2,false),b=position(n/2+1,false);
            const float actual=position(n,true);
            assert(actual==(n%2?(a+b)*.5f:a));
        }
    }
    assert(expression==save_starts.size());
    // Run the real Save entrance/exit dispatcher, stubbing allocation/render
    // services only. The localized prompt is created once on frame zero and
    // the input state remains locked until the 133 ms entrance is complete.
    e.prompt_allocations=e.prompt_creations=e.prompt_frees=0; // Independent Save fixture.
    e.capture_save=true;e.patched=true;e.Write(SaveMenu::Owner+0x28,2);
    e.Write(SaveMenu::Owner+0x24,0);e.Write(SaveMenu::Owner+0x2c,0);
    e.Write(SaveMenu::Owner+0x1c,0);e.Write(SaveMenu::Owner+0x20,0);
    float last_alpha=-1,last_y=19;
    for(unsigned n=0;n<=8;++n) {
        r[0]=0x800000;r[13]=0x9f0000;r[14]=0xf00000;
        e.Run(0x4392a8,0xf00000);
        assert(e.prompt_allocations==1 && e.prompt_creations==1 && e.Read(SaveMenu::Owner+0x1c)==0xa80000);
        assert(e.Read(SaveMenu::Owner+0x24)==(n<8?0u:1u));
        for(unsigned q=0;q<14;++q)
            assert((e.save_offsets.at(q)==std::array{q<6?0.f:320.f-40*n,0.f}));
        const auto motion=SaveMenu::CurrentAnimation(e);
        assert(motion.buttons>=last_alpha && motion.rise<=last_y && motion.prompt==motion.buttons);
        last_alpha=motion.buttons;last_y=motion.rise;
    }
    assert(last_alpha==1 && last_y==0);
    e.Write(SaveMenu::Owner+0x24,2);e.Write(SaveMenu::Owner+0x2c,0);
    last_alpha=1;last_y=0;
    for(unsigned n=0;n<=8;++n) {
        r[0]=0x800000;r[13]=0x9f0000;r[14]=0xf00000;
        e.Run(0x4392a8,0xf00000);
        for(unsigned q=0;q<14;++q)
            assert((e.save_offsets.at(q)==std::array{q<6?0.f:-40.f*n,0.f}));
        if(n<8) {
            const auto motion=SaveMenu::CurrentAnimation(e);
            assert(motion.buttons<=last_alpha && motion.rise>=last_y);
            last_alpha=motion.buttons;last_y=motion.rise;
        }
    }
    assert(e.prompt_frees==1 && e.Read(SaveMenu::Owner+0x28)==0);
    // A menu restored partway through its entrance still gets its prompt.
    e.Write(SaveMenu::Owner+0x28,2);e.Write(SaveMenu::Owner+0x24,0);
    e.Write(SaveMenu::Owner+0x2c,3);e.Write(SaveMenu::Owner+0x1c,0);
    r[0]=0x800000;r[13]=0x9f0000;r[14]=0xf00000;
    e.Run(0x4392a8,0xf00000);
    assert(e.prompt_creations==2 && e.Read(SaveMenu::Owner+0x24)==0);
    e.capture_save=false;
    std::cout<<"Native Save entrance/exit: early prompt created once, native cleanup, stationary prompt, fade clock and delayed-entry recovery passed\n";
    // Outside the save page every new site preserves the original instruction.
    e.Write(SaveMenu::Owner+0x28,0);
    for(const auto& site:SaveSites) {
        r[0]=3;f[0]=U32(-123);cpu.SetCpsr(0x90000010);e.patched=false;
        e.Run(site.code.pc,site.code.pc+4);const auto native_f=f;const auto native_c=cpu.Cpsr();
        r[0]=3;f[0]=U32(-123);cpu.SetCpsr(0x90000010);e.patched=true;
        e.Run(site.code.pc,site.code.pc+4);assert(f==native_f && cpu.Cpsr()==native_c);
    }
    // Execute the actual Continue/Quit input tail. B always continues;
    // A still respects either selection, and no press leaves state11 alone.
    for(unsigned selected:{0u,1u})for(unsigned pressed:{0u,1u,2u,3u}) {
        e.Write(SaveMenu::Owner+0x28,2);e.Write(SaveMenu::Owner+0x24,11);
        e.Write(SaveMenu::Owner+0x34,selected);e.Write(SaveMenu::Owner+0x2c,0);
        r={};r[0]=pressed;r[4]=SaveMenu::Owner;r[5]=0;r[13]=0xbf0000;
        e.Run(0x43a65c,0x43ac70);
        const unsigned expected=(pressed&2)?14:(pressed&1)?(selected?14:16):11;
        assert(e.Read(SaveMenu::Owner+0x24)==expected);
    }
    for(unsigned state:{1u,8u,9u,10u,12u,13u,14u}) {
        e.Write(SaveMenu::Owner+0x24,state);e.Write(SaveMenu::Owner+0x28,2);
        r={};r[0]=2;r[4]=SaveMenu::Owner;r[15]=0x43a660;
        U32 status=0xb0000010;
        e.hook.Handle(e,r,e.cpu->ExtRegs(),status,29);
        assert(r[15]==0x43a660 && r[0]==2 && status==0x70000010);
    }
    std::cout<<"Post-save B continues from either selection; A/Quit and no-input behavior retained\n";
    SaveMenu::on_top=true;e.Write(SaveMenu::Owner+0x28,3);
    for(U32 row=0;row<3;++row) {
        e.Write(SaveMenu::Owner+0x30,row);e.Write(SaveMenu::Owner+0x3c,2);
        e.Write(SaveMenu::Owner+0x2c,9);
        r={};r[5]=SaveMenu::Owner;r[13]=0xbf0000;
        e.Run(0x43b574,0x43b5ec);
        assert(e.Read(SaveMenu::Owner+0x30)==row && e.Read(SaveMenu::Owner+0x3c)==3 &&
               e.Read(SaveMenu::Owner+0x2c)==0); // A closes via native OK, without changing selection.
    }
    r={};r[0]=2;r[5]=SaveMenu::Owner;r[13]=0xbf0000;
    e.Write(SaveMenu::Owner+0x30,2);e.Run(0x43b408,0x43bbe4);
    assert(e.Read(SaveMenu::Owner+0x30)==2); // Down cannot reach Cancel/OK.
    // Native B cancellation still restores all three original option values.
    constexpr U32 snapshot=0xa00000,options=0xa10000,save=0x587958;
    for(U32 i=0;i<3;++i) {e.Write(snapshot+i*4,i&1);e.Write(options+i*4,1-(i&1));}
    r={};r[4]=snapshot;r[5]=SaveMenu::Owner;r[6]=save+0x1000;r[8]=save;r[9]=options;
    e.Run(0x43b634,0x43b5ec);
    for(U32 i=0;i<3;++i)assert(e.Read(options+i*4)==(i&1));
    r[5]=SaveMenu::Owner; // Next native update reloads the controller base.
    e.Run(0x43b0b4,0x43b0bc);
    assert(e.Read(SaveMenu::Owner+0x28)==1 && SaveMenu::returning_from_options);
    SaveMenu::on_top=false;SaveMenu::returning_from_options=false;
    std::cout<<"Native Options: A accepts from each row, Down stops at final row, B restores values, return retains backdrop\n";
    if(argc==3 && std::string_view(argv[2])=="--save-only")return 0;
    // File-select pacing and its actual native stone updater, including all
    // four signed motion paths, clamping, and exact wall-clock duration.
    assert(FileSelectTiming::CodeMatches(e));
    e.Pause(false);e.patched=true;
    e.Write(FileSelect::Owner+0x10,2);
    e.Write(FileSelect::Owner+8,0xb00000);e.Write(FileSelect::Owner+4,0xb01000);
    e.Write(0xb00000,0x4ebd60);e.Write(0xb00004,0xb01000);
    e.Write(FileSelect::Owner+0x30,0xb02000);e.Write(0xb02000,184);
    e.Write(FileSelect::Owner+0x34,0xb03000);
    e.Write(0x54cc3c,0);e.Write(Renderer+0x88,0);e.waits=0;
    for(unsigned n=0;n<60;++n) {
        e.Begin();assert(e.hook.file.active && !e.hook.fast);
        assert(Interpolation::menu_page==Interpolation::MenuPage::FileSelect);e.End();
    }
    assert(e.waits==60 && *e.Read(Renderer+0x88)==60);
    {
        const auto state=*e.Read(FileSelect::NameOwner+4);
        const auto atlas=*e.Read(FileSelect::NameOwner+0x64),keys=*e.Read(FileSelect::NameOwner+0x68);
        e.Write(FileSelect::NameOwner+4,4);
        e.Write(FileSelect::NameOwner+0x64,0xc08000);e.Write(FileSelect::NameOwner+0x68,0xc09000);
        for(U32 model:{0xc08000u,0xc09000u}) {e.Write(model,0x4ebd60);e.Write(model+4,model+0x400);}
        e.Begin();assert(Interpolation::menu_page==Interpolation::MenuPage::NameEntry);
        e.Write(FileSelect::NameOwner+4,state);
        e.Write(FileSelect::NameOwner+0x64,atlas);e.Write(FileSelect::NameOwner+0x68,keys);
        e.Begin();assert(Interpolation::menu_page==Interpolation::MenuPage::FileSelect);
    }
    for(float velocity:{-80.f,80.f}) {
        const float initial=velocity<0?336:16,target=velocity<0?16:336;
        for(unsigned i=0;i<3;++i) {
            e.Put(0x55b5e0+i*4,target);e.Put(0x55b5ec+i*4,target);
            e.Put(0x55b5f8+i*4,initial);e.Put(0x55b604+i*4,initial);
            e.Put(0x55b610+i*4,velocity);e.Put(0x55b61c+i*4,velocity);
            e.Write(0x55b548+i*4,0);e.Write(0x55b554+i*4,0);
        }
        for(unsigned n=1;n<=8;++n) {
            e.Begin();r[13]=0xbf0000;r[14]=0x700000;
            e.Run(0x2ee5e4,0x700000);
            for(unsigned i=0;i<3;++i) {
                assert(e.Get<float>(0x55b5f8+i*4)==initial+n*velocity*.5f);
                assert(e.Get<float>(0x55b604+i*4)==initial+n*velocity*.5f);
            }
        }
        assert(e.Get<float>(0x55b610)==0 && e.Get<float>(0x55b61c)==0);
    }
    // Staggered entrance: eight 60 Hz movement steps per stone, six apart.
    for(unsigned i=0;i<3;++i) {
        e.Put(0x55b5e0+i*4,16.f);e.Put(0x55b5f8+i*4,336.f+240*i);
        e.Put(0x55b610+i*4,-80.f);
    }
    for(unsigned n=1;n<=20;++n) {
        e.Begin();r[13]=0xbf0000;r[14]=0x700000;e.Run(0x2ee5e4,0x700000);
        for(unsigned i=0;i<3;++i)
            assert(e.Get<float>(0x55b5f8+i*4)==std::max(16.f,336.f+240*i-40*n));
    }
    // Copy destinations retain two-row coordinates on the right after Back.
    // Returning to the main list must match Erase: fixed-height horizontal
    // entrances from the left, with only the selected file moving vertically.
    {
        const auto memory=e.bytes;const auto pacing=e.hook.file;
        const auto saved_registers=r;const auto saved_floats=f;const auto saved_cpsr=cpu.Cpsr();
        for(unsigned selected=0;selected<3;++selected)for(bool from_copy:{false,true}) {
            e.hook.file={};e.hook.file.active=true;
            e.Write(FileSelect::Owner+0x10,9);e.Write(FileSelect::Owner+0x14,selected);
            unsigned row=0;
            for(unsigned i=0;i<3;++i) {
                e.Put<float>(0x55b5f8+4*i,i==selected?16:from_copy?416:-384);
                e.Put<float>(0x55b604+4*i,i==selected?56:from_copy?56+60*row++:48+52*i);
                e.Write(0x55b548+4*i,0);e.Write(0x55b554+4*i,0);
            }
            r[0]=0x55b61c+4*selected;f[0]=std::bit_cast<U32>(0.f);
            e.Run(0x430b64,0x430b68);
            for(unsigned frame=0;frame<=10;++frame) {
                for(unsigned i=0;i<3;++i) {
                    const float x=e.Get<float>(0x55b5f8+4*i),y=e.Get<float>(0x55b604+4*i);
                    assert(std::abs(x-(i==selected?16.f:-384.f+40*frame))<.001f);
                    const float expected=i==selected?56.f+(48.f+52*i-56.f)*frame/10:48.f+52*i;
                    assert(std::abs(y-expected)<.001f);
                    if(i!=selected)assert(e.Get<float>(0x55b61c+4*i)==0);
                }
                if(frame<10) {r[13]=0xbf0000;r[14]=0x700000;e.Run(0x2ee5e4,0x700000);}
            }
        }
        e.bytes=memory;e.hook.file=pacing;
        r=saved_registers;f=saved_floats;cpu.SetCpsr(saved_cpsr);
        std::cout<<"Copy/Erase return: all three source slots, fixed row heights, left entrance and selected vertical return at 60 Hz passed\n";
    }
    // Full native selection movement + atlas construction + top layout. The
    // previous tests only advanced the scalar coordinates, missing interactions
    // with the real card bounds and native geometry during departure.
    {
        const auto memory=e.bytes;const auto pacing=e.hook.file;
        e.file_geometry_fixture=true;
        constexpr U32 helper=0xb02000,raw=0xc00000,pos=0xc10000,uv=0xc20000,color=0xc30000,offset=0xc40000;
        e.Write(helper+0xc,raw);e.Write(helper+0x10,pos);e.Write(helper+0x14,uv);
        e.Write(helper+0x18,color);e.Write(helper+0x1c,offset);
        for(unsigned i=0;i<3;++i) {
            e.Write(0xb10000+i*0x2000+0x40,0x00300000); // Three filled hearts.
            e.Write(MasterQuest::Exists+4*i,1);
            e.Write(0x55b548+4*i,0);e.Write(0x55b554+4*i,0);e.Write(0x55b5b4+4*i,0);
        }
        for(unsigned selected=0;selected<3;++selected) {
            e.hook.file={};e.hook.file.active=true;e.hook.file.half=true;
            e.Write(FileSelect::Owner+0x10,4);e.Write(FileSelect::Owner+0x14,selected);
            e.Write(FileSelect::Owner+0x28,0);e.Write(FileSelect::Owner+0x54,0);
            // The fixture enters the dispatcher before drawing its first sample.
            FileSelect::action_opening=true;FileSelect::action_frames=0;
            e.Write(0x55b564,0x900000); // Existing prompt; constructor need not allocate.
            for(unsigned i=0;i<3;++i) {
                e.Put<float>(0x55b5f8+4*i,16);e.Put<float>(0x55b604+4*i,48+52*i);
                e.Put<float>(0x55b5e0+4*i,16);e.Put<float>(0x55b5ec+4*i,48+52*i);
                e.Write(0x55b610+4*i,0);e.Write(0x55b61c+4*i,0);
            }
            FileSelect::Layout layout;layout.stones={0,9,.78f};layout.stone_spacing=1.22f;
            std::array<float,3> previous{},step{};
            for(unsigned frame=1;frame<=16;++frame) {
                e.hook.file.Frame(true);r[6]=FileSelect::Owner;r[13]=0xbf0000;
                // Reproduce the dispatcher's local file-order table.
                const U32 table=*e.Read(0x42fdf0);
                for(unsigned j=0;j<6;++j)e.Write(r[13]+0x3c+4*j,*e.Read(table+4*j));
                if(e.Read(FileSelect::Owner+0x10)==7) {
                    e.Run(0x42f9d0,0x430f14);
                    r[14]=0x700000;e.Run(0x2ee5e4,0x700000);
                } else if(e.hook.file.half)e.Run(0x42f9d0,0x4336f8);
                else {e.Run(0x42f9d0,0x42f9d4);e.Run(0x430360,0x430f1c);}
                r[13]=0xbf0000;r[14]=0x700000;e.Run(0x2ed224,0x700000);
                r[0]=helper;r[14]=0x700000;e.Run(0x2f9a1c,0x700000);
                // Texture dimensions are absent in this position-only fixture.
                for(unsigned j=0;j<184*8;++j)e.Write(uv+4*j,0);
                const auto atlas=FileSelect::Atlas(e,false,layout);
                assert(atlas.count==184);
                float paired_right=0;bool first=true;
                for(unsigned i=0;i<3;++i)if(i!=selected) {
                    const auto b=FileSelect::Bounds(atlas,6+37*i,8+37*i);
                    if(e.Read(FileSelect::Owner+0x10)==8) {assert(!b.Valid());continue;}
                    assert(b.Valid());
                    if(frame>1 && frame<10) {
                        const float delta=previous[i]-b.right;
                        assert(delta>0);
                        if(frame>2)assert(std::abs(delta-step[i])<.001f);
                        step[i]=delta;
                    }
                    // Both stones leave together even though the native state
                    // hasn't started moving the second stone for four frames.
                    if(!first)assert(std::abs(b.right-paired_right)<.001f);
                    paired_right=b.right;first=false;previous[i]=b.right;
                    if(frame>=9)assert(b.right+40<=-23.99f);
                }
            }
        }
        e.bytes=memory;e.hook.file=pacing;e.file_geometry_fixture=false;FileSelect::action_opening=false;
        std::cout<<"Native atlas selection exits: simultaneous constant-speed 60 Hz motion through the edge and native state handoff passed\n";
    }
    // All interactive dispatches remain live on the half frame; transition
    // states branch to motion + geometry, bypassing only their 30 Hz timers.
    for(U32 state:{2u,3u,7u,8u,9u,10u,14u,17u,22u,37u,41u}) {
        e.Write(FileSelect::Owner+0x10,state);e.hook.file.half=true;r[6]=FileSelect::Owner;
        r[15]=0x42f9d4;e.hook.file.Handle(e,r,f,0);
        assert(r[0]==state && r[15]==((state==2 || state==10)?0x2ee5e4u:0x42f9d4u));
    }
    // Entering a save is a timed state. Execute the actual fade arithmetic,
    // stopping before cleanup/save loading, and compare equal wall-clock time.
    // The native 32-update gate must take 64 ticks at 60 Hz, not 32.
    for(bool fast:{false,true}) {
        e.hook.file={};e.Write(FileSelect::Owner+0x10,10);e.Write(FileSelect::Owner+0x28,0);
        const unsigned frames=fast?64:32;
        for(unsigned tick=0;tick<frames;++tick) {
            e.hook.file.Frame(fast);r[6]=FileSelect::Owner;r[15]=0x42f9d4;
            e.hook.file.Handle(e,r,f,0);
            if(r[15]!=0x2ee5e4)e.Run(0x431008,0x43103c);
            const unsigned updates=fast?(tick+2)/2:tick+1;
            assert(e.Read(FileSelect::Owner+0x28)==updates);
            const float native_scale=e.Get<float>(0x431330);
            assert(std::abs(e.Get<float>(FileSelect::Owner+0x44)-
                std::min(1.f,(updates-1)*native_scale))<1e-6f);
        }
        assert(e.Read(FileSelect::Owner+0x28)==32);
    }
    e.hook.file.active=true;
    // Accepting a file clears only its input delay and schedules the movement
    // initialization for the very next frame, regardless of input-frame parity.
    for(bool half:{false,true}) {
        e.hook.file.half=half;r[0]=4;r[6]=FileSelect::Owner;
        e.Run(0x4300ac,0x4300b0);
        assert(*e.Read(FileSelect::Owner+0x54)==0 && r[0]==4);
        e.Write(FileSelect::Owner+0x10,4);e.Begin();
        r[15]=0x42f9d4;e.hook.file.Handle(e,r,f,0);
        assert(r[15]==0x42f9d4);
    }
    // B has no input delay on either frame parity. All three native file
    // positions return together, while the action cards ease out to the right.
    for(unsigned selected=0;selected<3;++selected) {
        for(bool half:{false,true}) {
            e.hook.file.half=half;r[0]=4;r[6]=FileSelect::Owner;
            e.Run(0x430900,0x430904);
            assert(*e.Read(FileSelect::Owner+0x54)==0 && r[0]==4);
        }
        e.Write(FileSelect::Owner+0x10,9);e.Write(FileSelect::Owner+0x14,selected);
        for(unsigned i=0;i<3;++i) {
            e.Put(0x55b5f8+i*4,i==selected?16.f:-384.f);
            e.Put(0x55b604+i*4,i==selected?56.f:48.f+52*i);
        }
        e.Write(0x55b534,0);e.Write(0x55b564,0);
        r[6]=FileSelect::Owner;e.Run(0x430aa4,0x430b68);
        float previous=0,previous_step=INFINITY;
        for(unsigned n=1;n<=10;++n) {
            e.Begin();assert(e.hook.file.active);
            r[5]=0x55b524;e.Run(0x430b68,n<10?0x430c6c:0x430c64);
            assert(*e.Read(0x55b534)==n);
            if(n==10) {r[6]=FileSelect::Owner;e.Run(0x430c64,0x430c6c);}
            f[0]=std::bit_cast<U32>(std::int32_t(80*n));e.Run(0x430c98,0x430c9c);
            const float offset=std::bit_cast<float>(f[0]),step=offset-previous;
            assert(step>0 && step<=previous_step+.001f);
            previous=offset;previous_step=step;
            r[13]=0xbf0000;r[14]=0x700000;e.Run(0x2ee5e4,0x700000);
            for(unsigned i=0;i<3;++i) {
                assert(std::abs(e.Get<float>(0x55b5f8+i*4)-(i==selected?16.f:-384.f+40*n))<.001f);
                const float expected=i==selected?56.f+(48.f+52*i-56)*n/10:48.f+52*i;
                assert(std::abs(e.Get<float>(0x55b604+i*4)-expected)<.001f);
            }
        }
        assert(previous==400 && *e.Read(FileSelect::Owner+0x10)==2);
    }
    // Five native ticks become ten distinct 60 Hz action-stone positions,
    // easing in from either direction with unchanged endpoints/duration.
    for(bool left:{false,true}) {
        e.hook.file.action_clock=false;e.Write(0x55b534,5);
        float previous=400,previous_step=INFINITY;
        for(unsigned n=1;n<=10;++n) {
            r[5]=0x55b524;e.Run(0x430500,0x43050c);
            assert(*e.Read(0x55b534)==10-n);
            f[0]=std::bit_cast<U32>(std::int32_t((left?-80:80)*int(10-n)));
            const U32 pc=left?0x430550:0x430568;e.Run(pc,pc+4);
            const float value=std::abs(std::bit_cast<float>(f[0]));
            const float step=previous-value;
            assert(step>0 && step<=previous_step+.001f);
            previous=value;previous_step=step;
        }
        assert(previous==0);
    }
    // An occupied file starts the action entrance immediately, in parallel
    // with state 4. Entering native state 7 must not replay that entrance.
    e.Write(FileSelect::Owner+0x14,1);e.Write(MasterQuest::Exists+4,1);
    r[0]=4;r[6]=FileSelect::Owner;e.Run(0x4300ac,0x4300b0);
    assert(FileSelect::action_opening && FileSelect::action_frames==1);
    for(unsigned frame=1;frame<=16;++frame) {
        e.Write(FileSelect::Owner+0x10,4);r[6]=FileSelect::Owner;
        e.hook.file.Handle(e,r,f,0);assert(FileSelect::action_frames==frame+1);
    }
    e.Write(FileSelect::Owner+0x10,7);e.Write(0x55b534,5);
    r[5]=0x55b524;e.Run(0x430500,0x43050c);
    assert(*e.Read(0x55b534)==0);
    e.Write(FileSelect::Owner+0x10,8);r[6]=FileSelect::Owner;e.hook.file.Handle(e,r,f,0);
    assert(!FileSelect::action_opening);
    // Cursor pulse advances thirty native ticks, with sixty distinct visual
    // phases. Execute the original arithmetic, comparisons and stores.
    e.Write(0xb03010,0);std::vector<float> phases;
    e.hook.file.half=true;
    for(unsigned n=0;n<60;++n) {
        e.Begin();r[4]=0xb03000;r[13]=0xbf0000;
        e.Run(0x2f94cc,0x2f9524);
        phases.push_back(e.Get<float>(r[13]+0x70));
    }
    assert(*e.Read(0xb03010)==30);
    for(unsigned n=1;n<59;++n)assert(phases[n]>phases[n-1]);
    // World scope supplies a 1/60 timestep, then restores the native value
    // without touching its neighboring halfword. Integer sky motion carries
    // the odd half-step rather than rounding it into double-speed motion.
    e.Write(0x51b2f4,0xb10000);e.Write(0xb10110,0x12340002);r[8]=0x51b2f4;r[4]=0xb20000;
    e.Run(0x45d030,0x45d034);assert(*e.Read(0xb10110)==0x12340001);
    assert(e.hook.file.sky_materials==0xb20568);
    // Execute the native sky material loop, including its conditional stores.
    // Running controllers use half steps only in this file-scene scope; paused
    // controllers keep zero and unrelated skies retain their original speed.
    for(bool match:{false,true})for(bool frozen:{false,true}) {
        r[4]=match?0xb20568:0xb30000;r[3]=0;r[12]=frozen?1:0;
        for(unsigned slot=0;slot<8;++slot) {
            const U32 material=0xb40000+slot*0x100,controller=material+0x40;
            e.Write(r[4]+0x44+slot*4,material);e.Write(material+0xc,controller);
            e.Put<float>(controller+0xc,7.f);
        }
        f[17]=std::bit_cast<U32>(1.f);f[18]=0;
        e.Run(0x2d54d0,0x2d5510);
        for(unsigned slot=0;slot<8;++slot)
            assert(e.Get<float>(0xb4004c+slot*0x100)==(frozen?0.f:match?.5f:1.f));
        assert(f[17]==std::bit_cast<U32>(1.f) && f[18]==0);
    }
    for(unsigned site:{15u,16u})for(int step:{-3,-1,1,3}) {
        e.hook.file.sky_remainder={};int position=0;
        for(unsigned n=0;n<60;++n) {
            r[0]=U32(step);r[2]=U32(position);
            const auto pc=FileSelectTiming::Sites[site].pc;e.Run(pc,pc+4);
            position=int(r[0]);
        }
        assert(position==step*30);
    }
    r[13]=0xbf0000;e.Run(0x45dbd4,0x45dbd8);
    assert(*e.Read(0xb10110)==0x12340002 && r[13]==0xbf0124);
    assert(e.hook.file.sky_materials==0);
    e.Write(FileSelect::Owner+0x10,0);e.Begin();assert(!e.hook.file.active);
    // Every file-select hook must reproduce its displaced instruction outside
    // file select; include signed loads, both VADD encodings and the epilogue.
    for(const auto& site:FileSelectTiming::Sites) {
        std::array<U32,16> regs{};std::array<U32,64> ext{};
        for(unsigned j=0;j<regs.size();++j)regs[j]=0xc00000+j*0x100;
        for(unsigned j=0;j<ext.size();++j)ext[j]=std::bit_cast<U32>(float(j+1));
        regs[0]=0x100;regs[15]=site.pc;const U32 flags=0x90000010;
        e.Write(regs[8],0xc10000);e.Write(regs[6]+0x10,3);
        e.Write(regs[3]+0x100,0xffff);e.Write(0x110,0xfffd);
        r=regs;f=ext;cpu.SetCpsr(flags);e.patched=false;
        e.Run(site.pc,site.pc+4);const auto expected_r=r;const auto expected_f=f;const auto expected_c=cpu.Cpsr();
        r=regs;f=ext;cpu.SetCpsr(flags);e.patched=true;
        e.Run(site.pc,site.pc+4);
        assert(r==expected_r && f==expected_f && cpu.Cpsr()==expected_c);
    }
    // Execute the actual file-menu sound blocks and their new return hooks.
    // The audio engine alone is stubbed, allowing us to inspect exact IDs and ABI.
    e.menu_sounds.clear();
    e.patched=true;e.Write(MasterQuest::Owner+0x10,3);e.Write(MasterQuest::Owner+0x14,1);
    e.Write(MasterQuest::Exists+4,0);e.Write(0x55b53c+4,0);e.Write(0x55b548+0x18,0);
    for(U32 at:{0x50af14u,0x50af18u,0x50af2cu,0x50af30u})e.Write(at,0);
    r[6]=MasterQuest::Owner;r[13]=0xe00000;
    e.Run(MasterQuest::Sites[2].pc,0x4336f8);
    assert(e.quest.modal && e.menu_sounds==std::vector<U32>{0x010004c0});
    assert(e.Read(MasterQuest::Owner+0x10)==3);
    std::int64_t now=MasterQuest::SlideUs+2*MasterQuest::StaggerUs;
    e.quest.Dispatch(e,r,now);assert(!MasterQuest::departing_files && !MasterQuest::ready);
    now+=MasterQuest::SlideUs+MasterQuest::StaggerUs;
    e.quest.Dispatch(e,r,now);assert(MasterQuest::ready);
    e.Write(0x50af30,0x10);e.quest.Dispatch(e,r,now);e.Run(r[15],0x4336f8);
    assert(e.menu_sounds.back()==0x010004be && e.Read(MasterQuest::Owner+0x14)==1);
    e.Write(0x50af30,0);e.Write(0x50af18,1);e.quest.Dispatch(e,r,now);
    e.Run(r[15],0x4336f8);assert(e.menu_sounds.size()==3 && e.menu_sounds.back()==0x010004c0);
    now+=MasterQuest::SlideUs+MasterQuest::StaggerUs;e.quest.Dispatch(e,r,now);
    assert(r[15]==MasterQuest::Sites[4].pc);
    e.Run(r[15],0x4321c8);assert(r[0]==5 && e.menu_sounds.size()==3);
    // After the mode exit, native name entry completes its final departure tick.
    e.Write(MasterQuest::Owner+0x10,5);e.quest.Dispatch(e,r,now);
    e.Run(0x430220,0x43e200);assert(e.Read(MasterQuest::Owner+0x28)==6);
    // B sounds once on the press, while the chooser stays alive for its exit.
    // Returning file stones must finish before held B can reach the native menu.
    e.Write(MasterQuest::Owner+0x10,3);e.Write(0x50af18,0);
    r[6]=MasterQuest::Owner;r[13]=0xe00000;
    e.Run(MasterQuest::Sites[2].pc,0x4336f8);
    now=MasterQuest::SlideUs+2*MasterQuest::StaggerUs;
    e.quest.Dispatch(e,r,now);assert(!MasterQuest::departing_files);
    now+=MasterQuest::SlideUs+MasterQuest::StaggerUs;
    e.quest.Dispatch(e,r,now);e.Write(0x50af18,2);e.quest.Dispatch(e,r,now);
    e.Run(r[15],0x4336f8);
    assert(e.menu_sounds.size()==5 && e.menu_sounds.back()==0x010004c1);
    assert(MasterQuest::choosing && MasterQuest::cancelling && e.Read(MasterQuest::Owner+0x10)==3);
    now+=MasterQuest::SlideUs+MasterQuest::StaggerUs;e.quest.Dispatch(e,r,now);
    assert(MasterQuest::returning && MasterQuest::choosing);
    now+=MasterQuest::StaggerUs;
    assert(e.quest.Dispatch(e,r,now) && e.quest.block_cancel && e.menu_sounds.size()==5);
    e.Write(0x50af18,0);assert(!e.quest.Dispatch(e,r,now));
    std::cout<<"Native MQ Select/cursor sound IDs, call arguments, stack, no duplicate confirmation and name-entry handoff passed\n";
    // Execute the actual dungeon entrance helper. Its final step zeros both
    // axes for all 50 quads and clears the timer that hides separate icons.
    assert(PauseHeader::CodeMatches(e));
    e.Pause(true);e.Write(0x5066f8+0x34,0);e.Write(0x50446c+0x18,0);
    e.Write(0x4fda6c+0x50,1);e.Write(0x506cb0+0x38,7);
    e.capture_save=true;
    PauseHeader::Hook header;
    for(bool supported:{false,true}) {
        e.Write(0x506cb0+0x44,4);e.save_offsets.clear();
        header.EnterDungeon(e,supported);
        assert(e.Read(0x506cb0+0x44)==(supported?0u:4u));
        r[13]=0xe00000;r[14]=0xb00000;
        e.Run(0x443410,0xb00000);
        assert(e.save_offsets.size()==50 && r[0]==(supported?1u:0u));
        if(supported) {
            assert(e.Read(0x506cb0+0x44)==0);
            for(const auto& [quad,offset]:e.save_offsets)
                assert((offset==std::array<float,2>{0,0}));
        } else {
            assert(e.save_offsets[1][0]==-72 && e.save_offsets[21][0]==72);
            assert(e.save_offsets[16][1]==-40);
        }
    }
    std::cout<<"Native dungeon entrance: immediate resting positions for all 50 quads, icon timer cleared, unsupported fallback preserved\n";
    // The real frame limiter must deliver each banner position on consecutive
    // VBlanks during BOTH dungeon opening and closing, not alternate frames.
    PauseMenu::Layout map_layout;map_layout.transition_seconds=.118f;
    PauseMenu::SetLayout(map_layout,false);
    e.capture_save=false;header={};header.last_time=1000000;
    std::int64_t map_time=1000000;
    for(bool opening:{true,false}) {
        e.Write(0x506cb0+0x38,opening?7:8);
        e.Write(0x54ac4c,opening?0x01000000:0);
        e.Write(0x54cc3c,0);e.Write(Renderer+0x88,0);e.waits=0;
        float previous=opening?-PauseHeader::Travel(map_layout):0;
        for(unsigned frame=0;frame<8;++frame) {
            e.Begin();assert(e.hook.fast);
            map_time+=16667;header.Update(e,map_time,true);
            assert(opening?header.offset>previous:header.offset<previous);
            const auto vertices=PauseHeader::Vertices(header.offset,false);
            assert(vertices[4]==header.offset);
            assert(PauseMenu::motion_y==header.offset && PauseMenu::motion_x==0);
            previous=header.offset;e.End();
        }
        assert(e.waits==8 && e.Read(Renderer+0x88)==8);
        assert(header.slide.progress==(opening?1.f:0.f));
    }
    std::cout<<"Dungeon green banner: eight distinct positions on eight consecutive VBlanks entering and leaving; map contents stationary\n";
    std::cout<<"Native ARM checks passed: file-select/pause/save/options 60 Hz, native stone motion/stagger, sky/cursor timing, gameplay 30 Hz, close latch, all 22 save slide expressions, duration gates, model half-steps, flight easing, input repeat, equip effect and fallback ABI\n";
}
