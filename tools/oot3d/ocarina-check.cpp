// SPDX-License-Identifier: GPL-2.0-or-later
// Executes the native ocarina dispatcher; resource allocation/drawing are stubbed.
#include "oot3d/ocarina.h"
#include "oot3d/pause_prompt_assets.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
using namespace OOT3D;
using namespace OOT3D::Ocarina;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 Play=0x800000,SP=0xe00000,Stop=0xf00000,Helper=0xa00000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    Input input;
    unsigned sounds=0,cleanups=0;
    bool fast_list=true,details=false,native_geometry=false;
    bool audit_message=false;
    U32 stop_at=0;
    bool camera_supported=true;
    U32 requested_message=0;
    unsigned titles=0,note_batches=0,stone_batches=0;
    std::uint64_t remaining=0;
    const std::set<U32> stubs{0x2f9484,0x2f6944,0x3525d4,0x2f7af4,0x37547c,
        0x2fcdec,0x2f55d4,0x2f8b80,0x2f9430,0x43bdfc,0x313ce0,0x2f57f0,
        0x2f6f6c,0x43c104,0x2f79b4,0x2f67d4,0x2f663c,0x2f6e10,0x2f650c,0x2f5cd4,0x2f5cc4,
        0x2fc534,0x2fc40c,0x2e18c4,0x3051cc,0x305980,0x305950};
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop || (stop_at && a==stop_at))return 0xef000000;
        if(audit_message) {
            if(a==0x2d0320)return 0xef000002;
            if(a==0x2d0258 || a==0x33f2d8)return 0xef000001;
        }
        if(stubs.contains(a) && !(details && (a==0x43c104 || a==0x2f67d4 || a==0x2f663c)) &&
           !(native_geometry && (a==0x2fc534 || a==0x2fc40c)))
            return 0xef000001;
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
    void AddTicks(std::uint64_t n) override {remaining=n>remaining?0:remaining-n;}
    std::uint64_t GetTicksRemaining() override {return remaining;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();
        if(!svc){cpu->HaltExecution();return;}
        if(svc==2 && audit_message) {requested_message=r[1];cpu->HaltExecution();return;}
        if(svc==1) {
            const auto pc=r[15]-4;
            if(audit_message && pc==0x2d0258)r[0]=0xb40000;
            else if(audit_message && pc==0x33f2d8)r[0]=0;
            else if(pc==0x2f9484) {Put<std::uint16_t>(r[0],0);Put<std::uint16_t>(r[1],0);Put<std::uint8_t>(r[2],0);}
            else if(pc==0x37547c)++sounds;
            else if(pc==0x2f6944)++cleanups;
            else if(pc==0x2f55d4 || pc==0x43bdfc)r[0]=1;
            else if(pc==0x313ce0 || pc==0x2f57f0) {
                if(pc==0x2f57f0 && details)++titles;
                r[0]=0xb00000;
            }
            else if(pc==0x2fc534 && details) {
                const unsigned first=Get<U32>(r[13]),count=r[3];
                if(first==39)++note_batches;
                else if(first>=18 && first<=29)++stone_batches;
                const U32 positions=Get<U32>(r[0]+0x10);
                for(unsigned q=0;q<count;++q) {
                    const float x=Get<float>(r[1]+q*8),y=Get<float>(r[1]+q*8+4);
                    const float w=Get<float>(r[2]+q*8),h=Get<float>(r[2]+q*8+4);
                    for(unsigned v=0;v<4;++v) {
                        Put(positions+(first+q)*48+v*12,x+(v%2)*w);
                        Put(positions+(first+q)*48+v*12+4,y+(v/2)*h);
                    }
                }
            }
            else if(pc==0x2f650c || pc==0x2f5cc4)r[0]=0;
            r[15]=r[14];return;
        }
        if(svc==FirstSVC)input.Update(*this,r);
        else if(svc==FirstSVC+1)input.Navigate(*this,r,true);
        else if(svc==FirstSVC+2)input.AfterSound(*this,r);
        else if(svc==FirstSVC+5)input.BeginList(*this,r,fast_list);
        else if(svc==FirstSVC+6)input.FinishList(*this,r,fast_list);
        else if(svc==FirstSVC+7)DrawMessage(*this,r,true);
        else if(svc==FirstSVC+8)StartRecognition(*this,r,true);
        else if(svc==FirstSVC+9)NativeCameraLens(*this,r,cpu->ExtRegs(),camera_supported);
        else std::abort();
    }
    void Setup(unsigned state) {
        cpu->Reset();cpu->SetCpsr(0x10);input={};sounds=cleanups=0;
        back_title=0;back_title_visible=false;on_top=false;
        Write(0x5043d4+0xc,Play);Write(Play+0x100,0x203);
        Write(Play+0x5c2c,0);Write(Play+0x7f10,0);Write(Play+0x6014,0xaabbccdd);
        Write(Owner+0x14,state);Write(Owner+0x3c,1);Write(Owner+0x40,1);Write(Owner+0x44,1);
        Write(Owner+0x18,0);Write(Owner+0x1c,0);Write(Owner+0x38,0);Write(Owner+0x28,state==12);
        Write(Owner+0x10,0xb00000);Write(Owner+8,Helper);Write(Owner+0xc,0xb10000);
        Write(Play+0x20ac,0xb20000);Write(0xb20284,0);
        Write(0x50af0c+0x24,0);Write(0x50af0c+0x20,0);
    }
    U32 Pad(U32 b) {input.Filter(*this,b,true);return b;}
    void Update() {
        auto& r=cpu->Regs();r[0]=Play;r[13]=SP;r[14]=Stop;r[15]=0x425ab4;remaining=1000000;
        cpu->Run();assert(r[15]==Stop+4 && r[13]==SP);
    }
    void Call(U32 pc) {
        auto& r=cpu->Regs();r[13]=SP;r[14]=Stop;r[15]=pc;remaining=1000000;
        cpu->Run();assert(r[15]==Stop+4 && r[13]==SP);
    }
};
std::vector<std::uint8_t> File(const std::string& path) {
    std::ifstream in(path,std::ios::binary);assert(in);return {std::istreambuf_iterator<char>(in),{}};
}
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
int main(int argc,char** argv) {
    {
        PromptVignette v;
        const auto full=PromptCorner(v,1,400),half=PromptCorner(v,.5f,400);
        assert(full.count==144 && half.count==144 && full.positions==half.positions);
        assert(full.positions[0]==272 && full.positions[1]==168);
        assert(full.colors[3]==0 && full.colors[(144-1)*16+15]==v.strength);
        for(unsigned i=3;i<full.count*16;i+=4)assert(half.colors[i]==full.colors[i]*.5f);
        assert(!PromptCorner(v,0,400).count);
        const auto wide=PromptCorner(v,1,533.3333f);
        for(unsigned i=0;i<full.count*12;i+=3)
            assert(std::abs(wide.positions[i]-full.positions[i]-66.66665f)<.0001f);
        v.enabled=false;assert(!PromptCorner(v,1,400).count);
        v.enabled=true;v.x=-10;v.y=-20;
        const auto moved=PromptCorner(v,1,400);
        assert(moved.positions[0]==262 && moved.positions[1]==148);
        v.strength=NAN;assert(!PromptCorner(v,1,400).count);
    }

    assert(argc==3);Env e;const auto code=File(argv[1]);
    std::memcpy(e.bytes.data()+0x100000,code.data(),code.size());assert(CodeMatches(e));
    for(const auto& s:Sites) {const auto w=*e.Read(s.pc);e.Write(s.pc,w^1);assert(!CodeMatches(e));e.Write(s.pc,w);}
    Dynarmic::A32::UserConfig config;config.callbacks=&e;Dynarmic::A32::Jit cpu{config};e.cpu=&cpu;
    // Both shoulder pairs feed the same native input, including simultaneous
    // presses and releases. Outside the instrument, extra triggers stay native.
    {
        const auto saved=e.bytes;
        for(unsigned state:{0u,4u,12u})for(unsigned triggers=0;triggers<4;++triggers)
            for(U32 shoulders:{0u,0x100u,0x200u,0x300u}) {
                e.Setup(state);
                U32 buttons=shoulders|1u;
                input.Filter(e,buttons,true,triggers);
                const U32 aliases=state?((triggers&1?0x200u:0u)|(triggers&2?0x100u:0u)):0u;
                assert(buttons==(shoulders|1u|aliases));
                buttons=0;input.Filter(e,buttons,true,0);assert(buttons==0);
                buttons=shoulders;input.Filter(e,buttons,false,triggers);assert(buttons==shoulders);
            }
        e.bytes=saved;input={};
    }
    // Execute the actual lens-load, approach and store instructions from the
    // captured ocarina handler. The final degree must keep easing; unrelated
    // item cameras, subcameras and unsupported presentations stay native.
    {
        const auto saved=e.bytes;
        constexpr U32 cam=Env::Play+0x364,player=0x900000;
        e.Write(0x5043e0,Env::Play);e.Write(cam+0xd4,Env::Play);e.Write(cam+0xd8,player);
        e.Put(cam+0x11c,.05f);e.Put(cam+0x18,45.f);
        e.stop_at=0x238670;
        const auto step=[&](float fov) {
            cpu.Reset();cpu.SetCpsr(0x10);cpu.Regs()[4]=cam;cpu.Regs()[13]=Env::SP;
            cpu.Regs()[15]=0x23865c;cpu.ExtRegs()[3]=std::bit_cast<U32>(1.f);
            e.Put(cam+0x144,fov);const auto before=e.bytes;e.remaining=10000;
            cpu.Run();assert(cpu.Regs()[15]==e.stop_at+4 && cpu.Regs()[4]==cam && cpu.Regs()[13]==Env::SP);
            const float result=e.Get<float>(cam+0x144);
            e.Put(cam+0x144,fov);assert(e.bytes==before);e.Put(cam+0x144,result);
            return result;
        };
        for(unsigned variant=0;variant<6;++variant) {
            e.Write(cam+0x188,7|((variant==1?1u:0x38u)<<16));
            e.Write(cam+0x18c,variant==2?3:0);e.Write(Env::Play+0xa64,variant==3?1:0);
            e.Write(player+0x1714,variant==4?0:0x08000000);
            e.camera_supported=variant!=5;
            const float result=step(45.996773f);
            assert(Near(result,variant==0?45.946934f:45.f));
        }
        e.camera_supported=true;
        for(float target:{45.f,55.f}) {
            e.Put(cam+0x18,target);
            float current=target+(target==45?7.f:-7.f);
            for(unsigned frame=0;frame<150;++frame) {
                const float next=step(current);
                assert(std::abs(next-current)<=.351f);
                assert(std::abs(next-target)<=std::abs(current-target));current=next;
            }
            assert(current==target);
        }
        e.stop_at=0;cpu.ClearCache();e.bytes=saved;
        std::cout<<"Native ocarina lens: captured final-degree snap removed, smooth convergence, camera guards and unchanged guest state passed\n";
    }
    // A recognized song's result message reuses the live staff until native
    // playback preparation clears it. Suppress only its eight old note panes,
    // preserving both model variants, the staff, title and every other mode.
    {
        const auto saved=e.bytes;
        e.Setup(12);e.Write(Message,0x4ed0a0);e.Write(Message+0xf40,0x86f);
        constexpr U32 panes=0xc00000,models=0xc10000;
        for(unsigned i=0;i<9;++i) {
            const U32 pane=panes+i*0x100;
            e.Write(Message+0x7c0+4*i,pane);e.Write(pane,0x4ed234);
            e.Write(pane+4,Message+0x168);e.Put<std::uint8_t>(pane+0x6c,1);
            for(unsigned variant=0;variant<2;++variant) {
                const U32 model=models+i*0x400+variant*0x200;
                e.Write(pane+0x64+4*variant,model);e.Write(model,0x4ebd60);
            }
            e.Write(Message+0x998+8*i,i%5); // Previously played note.
        }
        for(U32 action=0;action<=0x31;++action)for(U32 phase=9;phase<=31;++phase) {
            e.Write(Env::Play+0x2b80,action);e.Write(Env::Play+0x2a90,phase);
            const bool expected=(action<=1 || (action>=0x22 && action<=0x29) || action>=0x30) &&
                (phase==17 || phase==18);
            for(unsigned i=0;i<9;++i)for(unsigned variant=0;variant<2;++variant)
                assert(HidePlaybackNote(e,models+i*0x400+variant*0x200,true)==(expected && i<8));
        }
        e.Write(Env::Play+0x2b80,0x24);e.Write(Env::Play+0x2a90,17);
        const auto unchanged=e.bytes;
        assert(HidePlaybackNote(e,models,true) && !HidePlaybackNote(e,models,false));
        assert(e.bytes==unchanged); // No visibility, recognition or timing writes.
        for(U32 id:{0x86eu,0x86du,0x893u,0x950u}) {
            e.Write(Message+0xf40,id);assert(!HidePlaybackNote(e,models,true));
        }
        e.Write(Message+0xf40,0x86f);
        e.Write(panes+4,0);assert(!HidePlaybackNote(e,models,true));
        e.Write(panes+4,Message+0x168);
        // Run the actual cartridge staff reset used by 474c2c, then its note
        // insertion routine. Only animation-resource calls are stubbed.
        cpu.Regs()[0]=Message;e.Call(0x2c0ca4);
        for(unsigned i=0;i<8;++i) {
            assert(e.Get<std::uint8_t>(panes+i*0x100+0x6c)==0);
            assert(e.Read(Message+0x998+8*i)==255);
        }
        e.Write(Env::Play+0x2a90,19);
        for(unsigned i=0;i<8;++i) {
            cpu.Regs()[0]=Message;cpu.Regs()[1]=i;cpu.Regs()[2]=i%5;e.Call(0x494dc0);
            for(unsigned n=0;n<8;++n) {
                assert(e.Get<std::uint8_t>(panes+n*0x100+0x6c)==(n<=i));
                assert(!HidePlaybackNote(e,models+n*0x400,true));
            }
        }
        e.bytes=saved;
    }
    // Hide the free-play prompt at its draw entry, leaving all message/music
    // state intact. Lessons, recording, song results and warp choices survive.
    e.Setup(12);e.Write(Env::Play+0x2b80,0);e.Write(Env::Play+0x2a90,9);
    e.Write(Message,0x4ed0a0);e.Write(Message+0xc,0x01010101);e.Write(Message+0xf40,0x86e);
    assert(FreePlay(e,Env::Play));
    const auto before_prompt=e.bytes;
    cpu.Regs()[0]=Message;e.Call(Sites[7].pc);
    assert(e.bytes==before_prompt);
    for(U32 id:{0x86eu,0x87cu,0x893u,0x88du,0x950u}) {
        e.Write(Message+0xf40,id);
        auto r=cpu.Regs();r[0]=Message;r[14]=Env::Stop;r[15]=Sites[7].pc+4;
        DrawMessage(e,r,true);
        assert(r[1]==1 && r[15]==(id==0x86e?Env::Stop:Sites[7].pc+4));
    }
    e.Write(Message+0xf40,0x86e);
    for(U32 action=0;action<=0x31;++action) {
        const bool continuous=action<=1 || (action>=0x22 && action<=0x29) || action>=0x30;
        e.Write(Env::Play+0x2b80,action);
        auto r=cpu.Regs();r[0]=0x12a5;r[5]=Env::Play;
        StartRecognition(e,r,true);
        assert(r[0]==(continuous?0x52a5u:0x12a5u) && r[15]==0x33704c && r[14]==Sites[8].pc+4);
        r[0]=Message;r[14]=Env::Stop;r[15]=Sites[7].pc+4;
        DrawMessage(e,r,true);assert((r[15]==Env::Stop)==continuous);
    }
    // Replay the live fountain capture: packed action 00ff0024, message 086e,
    // and no bottom-screen list during startup. Drawing must not alter the
    // native action, phase, buffers or actor state at any point.
    for(U32 phase:{5u,9u,12u})for(U32 list:{0u,16u,12u}) {
        e.Setup(list);e.Write(Env::Play+0x2b80,0x00ff0024);
        e.Write(Env::Play+0x2a90,phase);
        const auto captured=e.bytes;
        cpu.Regs()[0]=Message;e.Call(Sites[7].pc);
        assert(e.bytes==captured);
    }
    e.Setup(12);e.Write(Env::Play+0x2a90,9);
    e.Write(Env::Play+0x2b80,0);
    for(bool supported:{false,true})for(U32 allowed:{0u,1u}) {
        e.Write(Owner+0x40,allowed);
        auto r=cpu.Regs();r[0]=0x125;r[5]=Env::Play;
        StartRecognition(e,r,supported);assert(r[0]==(supported?0x4125u:0x125u));
        r[0]=Message;r[14]=Env::Stop;r[15]=Sites[7].pc+4;
        DrawMessage(e,r,supported);assert((r[15]==Env::Stop)==supported);
    }
    // Free-play audio/message handling is independent of the bottom song
    // list, even before that UI has entered its playable state. The old guard
    // silently left both the staff and the strict recognizer active here.
    e.Setup(0);e.Write(Owner+0x40,0);e.Write(Env::Play+0x2a90,9);
    auto independent=cpu.Regs();independent[0]=0x125;independent[5]=Env::Play;
    StartRecognition(e,independent,true);assert(independent[0]==0x4125);
    independent[0]=Message;independent[14]=Env::Stop;independent[15]=Sites[7].pc+4;
    DrawMessage(e,independent,true);assert(independent[15]==Env::Stop);
    for(U32 context:{0u,3u,0x103u}) {
        e.Write(Env::Play+0x100,context);assert(!FreePlay(e,Env::Play));
    }
    e.Setup(12);
    e.Write(Owner+0x40,1);
    for(U32 phase:{9u,12u,13u,14u,15u,16u,17u,23u,24u}) {
        e.Write(Env::Play+0x2a90,phase);
        assert(SongCompleted(e)==(phase==13 || (phase>=17 && phase<=23)));
    }
    // Execute the cartridge's real recognizer, not a mirrored host matcher.
    // Wrong notes and an overlong prefix must still permit every learned song;
    // holding a note must not count twice, and unlearned songs cannot complete.
    constexpr U32 audio=0x54ac48;
    const U32 pitches=*e.Read(0x47b11c),songs=*e.Read(0x47b120);
    const auto note=[&](std::uint8_t pitch) {
        e.Put<std::uint8_t>(audio+0x16,pitch);e.Put<std::uint8_t>(audio+0x17,255);
        e.Call(0x47af90);
        const auto count=e.Get<std::uint8_t>(audio+0x3e);
        e.Put<std::uint8_t>(audio+0x17,pitch);e.Call(0x47af90);
        assert(e.Get<std::uint8_t>(audio+0x3e)==count);
    };
    for(U32 action:{0u,1u,0x22u,0x23u,0x24u,0x25u,0x26u,0x27u,0x28u,0x29u,0x30u,0x31u})
    for(unsigned song=0;song<12;++song)for(bool learned:{false,true}) {
        e.Setup(0);e.Write(Owner+0x40,0);
        e.Write(Env::Play+0x2b80,action);e.Write(Env::Play+0x2a90,9);
        auto r=cpu.Regs();r[0]=learned?1u<<song:0;r[5]=Env::Play;
        StartRecognition(e,r,true);cpu.Regs()[0]=r[0];e.Call(r[15]);
        e.Write(audio+0xdc,0);e.Put<std::uint8_t>(audio+0x1a,0);
        assert(e.Read(audio+0x94)==(0x80004000u|(learned?1u<<song:0)));
        const auto wrong=e.Get<std::uint8_t>(pitches);
        for(unsigned n=0;n<19;++n)note(wrong);
        assert(e.Get<std::uint8_t>(audio+0x2b)==0 && *e.Read(audio+0x94)!=0);
        const unsigned length=e.Get<std::uint8_t>(songs+song*9);assert(length>0 && length<=8);
        for(unsigned n=0;n<length;++n)
            note(e.Get<std::uint8_t>(pitches+e.Get<std::uint8_t>(songs+song*9+n+1)));
        assert(e.Get<std::uint8_t>(audio+0x2b)==(learned?song+1:0));
        assert((*e.Read(audio+0x94)==0)==learned);
    }
    e.Setup(12);assert(e.Pad(8)==0);e.Update();
    assert(e.Read(Owner+0x14)==13 && e.sounds==1 && e.cleanups==1);
    e.Update();assert(e.Read(Owner+0x14)==4 && e.Read(Owner+0x38)==0 && e.Read(Owner+0x28)==0);
    assert(e.Pad(8)==0 && e.input.pending==Input::Action::None); // Held Start does not reopen.
    e.Pad(0);assert(e.Pad(2)==0);e.Update();assert(e.Read(Owner+0x14)==10 && e.sounds==2);
    for(unsigned i=0;i<6;++i)e.Update();
    assert(e.Read(Owner+0x14)==11);
    e.Update();assert(e.Read(Owner+0x14)==12);
    assert(e.Pad(2)==0);e.Update();assert(e.Read(Env::Play+0x6014)==0xaabbccdd);
    e.Pad(0);assert(e.Pad(2)==0);e.Update();assert(e.Read(Env::Play+0x6014)==0xaa01ccdd);
    // Back preserves the original title owner until the presentation has
    // finished. Run the cartridge exit and replacement cleanup, repeatedly,
    // including a presentation exit longer than the native six-frame slide.
    for(unsigned repeat=0;repeat<3;++repeat) {
        e.Setup(4);on_top=true;const auto title=*e.Read(Owner+0x10);
        e.Pad(2);e.Update();
        assert(e.Read(Owner+0x14)==10 && BackTitle(e) && back_title_visible);
        assert(e.Read(Owner+0x10)==title && e.cleanups==0 && e.sounds==1);
        for(unsigned i=0;i<10;++i)e.Update();
        assert(e.Read(Owner+0x14)==11 && e.Read(Owner+0x10)==title && e.cleanups==0);
        back_title_visible=false;e.Update();
        assert(e.Read(Owner+0x14)==12 && e.cleanups==1 && !BackTitle(e));
        e.Update();assert(e.cleanups==1 && back_title==0);
    }
    // Unsupported presentation keeps the complete native entrance path.
    e.fast_list=false;e.Setup(12);e.Pad(8);e.Update();e.Update();
    assert(e.Read(Owner+0x14)==14);
    for(unsigned i=0;i<6;++i)e.Update();
    assert(e.Read(Owner+0x14)==4);e.fast_list=true;
    // Exclusions retain Start/save outside ocarina and reject scripted lessons.
    e.Setup(0);assert(e.Pad(8)==8 && e.Pad(2)==2);
    e.Setup(12);e.Write(Owner+0x44,0);assert(e.Pad(8)==0);e.Update();assert(e.Read(Owner+0x14)==12);
    e.Setup(12);e.Write(Owner+0x40,0);e.Pad(8);e.Update();assert(e.Read(Owner+0x14)==12);
    // Every direction from every stone runs through the native dispatcher and
    // its movement sound; the stored old coordinates still select the right song.
    for(bool two:{false,true})for(unsigned i=0;i<12;++i)for(U32 dir:{0x10u,0x20u,0x40u,0x80u}) {
        Layout nav;nav.two_columns=two;SetLayout(nav,false);
        const unsigned columns=nav.Columns(),rows=12/columns;
        e.Setup(4);e.Write(Owner+0x18,i%4);e.Write(Owner+0x1c,i/4);e.Write(0x50af0c+0x20,dir);
        e.Update();unsigned x=i%columns,y=i/columns;
        if(dir==0x10)x=(x+1)%columns;
        else if(dir==0x20)x=(x+columns-1)%columns;
        else if(dir==0x40)y=(y+rows-1)%rows;
        else y=(y+1)%rows;
        assert(*e.Read(Owner+0x18)+4**e.Read(Owner+0x1c)==x+y*columns && e.sounds==1);
    }
    SetLayout(Layout{},false);
    // Build the actual native atlas rectangles from cartridge data, then verify
    // layout, hidden-quad clipping and read-only source arrays.
    e.Setup(4);constexpr U32 model=0x900000,mesh=0x910000,pos=0xa10000,uv=0xa20000,color=0xa30000;
    e.Write(Owner,mesh);e.Write(Owner+4,model);e.Write(model,0x4ebd60);e.Write(model+4,mesh);
    // Generated UI now validates its native donor before allocating.
    e.Write(mesh,0x920000);e.Write(model+0x1dc,0x930000);
    e.Write(Env::Helper,QuadCount);e.Write(Env::Helper+0x10,pos);e.Write(Env::Helper+0x14,uv);e.Write(Env::Helper+0x18,color);
    for(unsigned q=0;q<QuadCount;++q) {
        float x=e.Get<float>(Owner+0x68+q*8),y=e.Get<float>(Owner+0x68+q*8+4);
        float w=e.Get<float>(Owner+0x3c8+q*8),h=e.Get<float>(Owner+0x3c8+q*8+4);
        if(q==39){x=82;y=12;w=h=16;}
        const std::array<float,12> v{x,y,0,x+w,y,0,x,y+h,0,x+w,y+h,0};
        for(unsigned n=0;n<12;++n)e.Put(pos+q*48+n*4,v[n]);
        for(unsigned n=0;n<16;++n)e.Put(color+q*64+n*4,1.f);
        const float ux=e.Get<float>(Owner+0xa88+q*8),uy=e.Get<float>(Owner+0xa88+q*8+4);
        const float uw=e.Get<float>(Owner+0x728+q*8),uh=e.Get<float>(Owner+0x728+q*8+4);
        const std::array<float,8> tex{ux/512,1-uy/256,(ux+uw)/512,1-uy/256,
            ux/512,1-(uy+uh)/256,(ux+uw)/512,1-(uy+uh)/256};
        for(unsigned n=0;n<8;++n)e.Put(uv+q*32+n*4,tex[n]);
    }
    Layout l;auto g=Atlas(e,l);assert(g.count==108);
    for(unsigned q=0;q<QuadCount;++q)for(unsigned n=0;n<8;++n) {
        const float original=e.Get<float>(uv+q*32+n*4);
        float expected=original;
        if(n%2==0) {
            if(q==0 && original==56.f/512)expected=56.5f/512;
            if(q==2 && original==136.f/512)expected=135.5f/512;
            if(q==1 && (original==103.f/512 || original==104.f/512))expected=103.5f/512;
            if((q==35 || q==37) && original==224.f/512)expected=223.5f/512;
            if(q==36 && (original==222.f/512 || original==223.f/512))expected=222.5f/512;
        } else if(q<=2) {
            if(original==1-192.f/256)expected=1-192.5f/256;
            if(original==0)expected=.5f/256;
        }
        if(q>=39 && q<=46) {
            const unsigned axis=n%2;float lo=1,hi=0;
            for(unsigned v=0;v<4;++v) {const float f=e.Get<float>(uv+q*32+(v*2+axis)*4);lo=std::min(lo,f);hi=std::max(hi,f);}
            const float half=.5f/(axis?256:512);
            if(lo>=0 && hi<=1 && hi-lo>=2*half)expected=std::clamp(original,lo+half,hi-half);
        }
        assert(g.uvs[q*8+n]==expected);
    }
    const auto clean_uv=g.uvs;PromptUVs(g);assert(g.uvs==clean_uv);
    for(unsigned q=18;q<=29;++q)for(unsigned v=0;v<4;++v) {
        assert(g.colors[(q-15)*16+v*4+3]==0);
        assert(g.colors[q*16+v*4+3]==1);
        assert(e.Get<float>(color+(q-15)*64+v*16+12)==1); // Native backing untouched.
    }
    // An unlearned/hidden icon retains its placeholder. Test both native
    // hiding forms without affecting the other learned songs or their layout.
    for(unsigned v=0;v<4;++v)e.Put(color+18*64+v*16+12,0.f);
    auto missing=Atlas(e,l);
    assert(missing.colors[3*16+3]==1 && missing.colors[4*16+3]==0);
    for(unsigned v=0;v<4;++v)e.Put(color+18*64+v*16+12,1.f);
    std::array<float,8> saved_uv;
    for(unsigned n=0;n<8;++n){saved_uv[n]=e.Get<float>(uv+18*32+n*4);e.Put(uv+18*32+n*4,0.f);}
    missing=Atlas(e,l);
    assert(missing.colors[3*16+3]==1 && missing.colors[4*16+3]==0);
    for(unsigned n=0;n<8;++n)e.Put(uv+18*32+n*4,saved_uv[n]);
    assert(missing.positions==g.positions);
    for(unsigned q=3;q<=14;++q) {
        const auto i=StoneIndex(q);assert(i<12);
        const float x=200+(int(i%3)-1)*54,y=134+(float(i/3)-1.5f)*33;
        assert(Near((g.positions[q*12]+g.positions[q*12+3])/2,x));
        assert(Near((g.positions[q*12+1]+g.positions[q*12+7])/2,y));
        for(float width:{400.f,240.f*16/9}) {
            auto p=MapPointer((x+(width-400)/2)/width,y/240,width,l);assert(p);
            assert(Near((*p)[0]*320,52+72*(i%4)) && Near((*p)[1]*240,90+44*(i/4)));
        }
    }
    for(unsigned q:{32u,38u,51u,62u,107u})assert(g.colors[q*16+3]==0);
    // Both grids preserve native song IDs, and independent stone size leaves
    // the centers untouched. Touch selection must use the same scale/spacing.
    for(bool two:{false,true}) {
        Layout custom;custom.two_columns=two;custom.row_spacing=7;custom.column_spacing=13;
        custom[Stones]={-12,5,.7f};
        for(unsigned i=0;i<12;++i) {
            const auto base=Transform(custom,Stones,i);
            custom.stone_size=.6f;const auto small=Transform(custom,Stones,i);
            assert(base.x==small.x && base.y==small.y && Near(small.sx,.42f));
            const float x=small.ax+small.x,y=small.ay+small.y;
            const auto hit=MapPointer(x/400,y/240,400,custom);assert(hit);
            assert(Near((*hit)[0]*320,small.ax) && Near((*hit)[1]*240,small.ay));
            custom.stone_size=1;
        }
        const auto a=Transform(custom,Stones,0),b=Transform(custom,Stones,1);
        const auto c=Transform(custom,Stones,custom.Columns());
        assert(Near((b.ax+b.x)-(a.ax+a.x),85*.7f));
        assert(Near((c.ay+c.y)-(a.ay+a.y),51*.7f));
    }
    // Midpoint slide history changes only translation, with strict resets for
    // editor changes, missed frames, savestates and interpolation-mode changes.
    OcarinaSlide::History slide_mid;
    const auto layout_key=OcarinaSlide::LayoutKey(l);
    assert(slide_mid.Observe(1,0,1,layout_key,-320)==0);
    assert(slide_mid.Observe(2,33333,1,layout_key,-200)==-60);
    assert(slide_mid.Observe(2,33333,1,layout_key,-200)==-60);
    assert(slide_mid.Observe(3,66666,1,layout_key,-100)==-50);
    assert(slide_mid.Observe(4,99999,1,layout_key,0)==-50);
    assert(slide_mid.Observe(5,133332,1,layout_key,0)==0);
    assert(slide_mid.Observe(6,166665,1,layout_key,-30)==15); // Closing.
    assert(slide_mid.Observe(8,233331,1,layout_key,-200)==0); // Missing frame.
    assert(slide_mid.Observe(9,266664,2,layout_key,-250)==0); // New epoch.
    Layout edit=l;edit.row_spacing=2;
    assert(slide_mid.Observe(10,299997,2,OcarinaSlide::LayoutKey(edit),-280)==0);
    assert(slide_mid.Observe(1,0,2,layout_key,-320)==0);
    // Tag the exact native draw and convert the slide through a rotated view.
    constexpr U32 desc=0xd00000,view=0xd10000;
    e.Write(mesh,desc);e.Write(desc,pos);e.Write(desc+4,uv);e.Write(desc+8,color);
    e.Write(desc+12,QuadCount*4);e.Write(model+0x174,QuadCount*6);
    const std::array<float,12> rotated{0,1,0,0,-1,0,0,0,0,0,1,0};
    for(unsigned i=0;i<12;++i)e.Put(view+i*4,rotated[i]);
    auto signature=HUD::HashStart;
    for(const auto& words:std::array<std::array<U32,2>,3>{{{pos,QuadCount*48},{uv,QuadCount*32},{color,QuadCount*64}}})
        for(unsigned i=0;i<words[1];i+=4)signature=HUD::HashWord(signature,*e.Read(words[0]+i));
    const bool enabled=Interpolation::enabled.exchange(true),present=Interpolation::present_60.exchange(true);
    OcarinaSlide::Capture(e,model,view,-60);
    const auto tag_frame=Interpolation::guest_frames.load();const auto epoch=Interpolation::generation.load();
    const auto shift=OcarinaSlide::Find(signature,QuadCount*4,tag_frame,epoch);
    assert(shift && (*shift)[0]==0 && (*shift)[1]==60 && (*shift)[2]==0);
    assert(!OcarinaSlide::Find(signature^1,QuadCount*4,tag_frame,epoch));
    assert(OcarinaSlide::Active(tag_frame+1,epoch));
    assert(OcarinaSlide::Find(signature,QuadCount*4,tag_frame+1,epoch)==shift);
    assert(!OcarinaSlide::Active(tag_frame+2,epoch));
    assert(!OcarinaSlide::Find(signature,QuadCount*4,tag_frame+2,epoch));
    assert(!OcarinaSlide::Find(signature,QuadCount*4,tag_frame,epoch+1));
    // Replay the measured one-frame queue delay, then a stationary identical
    // submission; stale movement must not win over the stationary submission.
    OcarinaSlide::Publish({signature,tag_frame+1,QuadCount*4,epoch,{}});
    assert(!OcarinaSlide::Find(signature,QuadCount*4,tag_frame+1,epoch));
    assert(!OcarinaSlide::Find(signature,QuadCount*4,tag_frame+2,epoch));
    OcarinaSlide::Publish({signature,tag_frame+3,QuadCount*4,epoch,{0,20,0}});
    assert(!OcarinaSlide::Find(signature,QuadCount*4,tag_frame+2,epoch)); // No future sample.
    OcarinaSlide::ArmDiagnostics();assert(OcarinaSlide::DiagnosticActive());
    Interpolation::guest_frames=tag_frame+360;assert(!OcarinaSlide::DiagnosticActive());
    Interpolation::guest_frames=tag_frame;OcarinaSlide::diagnostic_end=0;
    // Exercise the actual generated panel, including all sixty shadow quads.
    // The former 440-vertex/660-index limits rejected this whole draw before
    // either CPU or GPU replay could insert a midpoint.
    {
        const auto saved_mesh=e.bytes;
        const auto full=WithPanel(g,l,-60,-40);
        const unsigned vertices=full.count*4,indices=full.count*6;
        assert(full.count==DrawQuadCount && vertices>440);
        assert(OcarinaSlide::ValidDrawCount(indices));
        assert(!OcarinaSlide::ValidDrawCount(0));
        assert(!OcarinaSlide::ValidDrawCount(indices-1));
        assert(!OcarinaSlide::ValidDrawCount(indices+6));
        e.Write(desc+12,vertices);e.Write(model+0x174,indices);
        for(unsigned i=0;i<full.positions.size();++i)e.Put(pos+i*4,full.positions[i]);
        for(unsigned i=0;i<full.uvs.size();++i)e.Put(uv+i*4,full.uvs[i]);
        for(unsigned i=0;i<full.colors.size();++i)e.Put(color+i*4,full.colors[i]);
        auto full_hash=HUD::HashStart;
        for(const auto& words:std::array<std::array<U32,2>,3>{{{pos,full.count*48},{uv,full.count*32},{color,full.count*64}}})
            for(unsigned i=0;i<words[1];i+=4)full_hash=HUD::HashWord(full_hash,*e.Read(words[0]+i));
        for(float delta:{-60.f,15.f,0.f}) {
            OcarinaSlide::Capture(e,model,view,delta);
            const auto route=OcarinaSlide::Find(full_hash,vertices,tag_frame+1,epoch);
            if(delta==0)assert(!route);
            else assert(route && (*route)[0]==0 && (*route)[1]==-delta && (*route)[2]==0);
        }
        const auto published=OcarinaSlide::capture_published.load();
        e.Write(desc+12,vertices+4);e.Write(model+0x174,indices+6);
        OcarinaSlide::Capture(e,model,view,-60);
        assert(OcarinaSlide::capture_published.load()==published);
        e.bytes=saved_mesh;
    }
    Interpolation::enabled=enabled;Interpolation::present_60=present;
    // The backdrop borrows only the black interior and right edge of the staff.
    // Independent bottom entrance keeps the staff/title aligned and all
    // stone/shadow vertices on their original horizontal path.
    const auto original=WithPanel(g,l,-60,-40);
    const auto horizontal=SplitPanel(g,l,-60,55,-40,false);
    const auto vertical=SplitPanel(g,l,-60,55,-40,true);
    for(unsigned q=0;q<original.count;++q)for(unsigned v=0;v<4;++v) {
        const auto part=q>=AtlasDrawOffset?QuadPart(q-AtlasDrawOffset):Count;
        const bool bottom=part==Notes || part==Name;
        const auto pos=q*12+v*3,alpha=q*16+v*4+3;
        assert((bottom?horizontal:vertical).colors[alpha]==0);
        if(bottom) {
            assert(std::abs(vertical.positions[pos]-original.positions[pos]-60)<.001f);
            assert(std::abs(vertical.positions[pos+1]-original.positions[pos+1]-55)<.001f);
        } else assert(horizontal.positions[pos]==original.positions[pos]);
    }
    const auto panel=WithPanel(g,l,-60,-40);
    assert(panel.count==QuadCount+AtlasDrawOffset && Near(panel.positions[0],-100));
    assert(Near(panel.uvs[0],124.5f/512) && Near(panel.uvs[1],1-216.5f/256));
    assert(Near(panel.uvs[8],130.5f/512) && Near(panel.uvs[10],135.5f/512));
    for(unsigned q=0;q<QuadCount;++q)for(unsigned v=0;v<4;++v) {
        assert(Near(panel.positions[(q+AtlasDrawOffset)*12+v*3],g.positions[q*12+v*3]-60));
        assert(panel.colors[(q+AtlasDrawOffset)*16+v*4+3]==g.colors[q*16+v*4+3]);
    }
    // Shadows follow the learned artwork's exact UV silhouette, stay below
    // it at every scale/slide offset, and never shadow an unlearned placeholder.
    for(float size:{.5f,1.f,2.f})for(float shift:{-300.f,0.f}) {
        auto custom=l;custom.stone_size=size;custom[Stones].x=17;custom[Stones].y=-9;
        const auto art=Atlas(e,custom);
        const auto draw=WithPanel(art,custom,shift,-40);
        const float scale=Transform(custom,Stones).sx;
        for(unsigned q=18;q<=29;++q)for(unsigned v=0;v<4;++v) {
            const unsigned shadow=2+(q-18)*ShadowLayers;
            assert(Near(draw.positions[shadow*12+v*3],art.positions[q*12+v*3]+shift));
            assert(Near(draw.positions[shadow*12+v*3+1],art.positions[q*12+v*3+1]+3*scale));
            assert(draw.uvs[shadow*8+v*2]==art.uvs[q*8+v*2]);
            assert(draw.uvs[shadow*8+v*2+1]==art.uvs[q*8+v*2+1]);
            assert(draw.colors[shadow*16+v*4]==0);
            assert(Near(draw.colors[shadow*16+v*4+3],.18f));
        }
    }
    const auto missing_draw=WithPanel(missing,l,0,-40);
    for(unsigned q=2;q<2+ShadowLayers;++q)for(unsigned v=0;v<4;++v)
        assert(missing_draw.colors[q*16+v*4+3]==0);
    const auto hidden=WithPanel(g,l,-Travel(g,l,-40),-40);
    for(unsigned q=0;q<hidden.count;++q)for(unsigned v=0;v<4;++v)
        if(hidden.colors[q*16+v*4+3]>0)assert(hidden.positions[q*12+v*3]<-40);
    slide_x=-60;
    auto touch=MapPointer((146.f-60)/400,84.5f/240,400,l);assert(touch);
    assert(Near((*touch)[0]*320,52) && Near((*touch)[1]*240,90));slide_x=0;
    // One clock, reversals without jumps, and reset on emulated-time rewind.
    Slide motion;assert(motion.Advance(0,false)==0);
    const float a=motion.Advance(90000,true);assert(a>0 && a<1);
    assert(Near(motion.Advance(90000,false),a));
    assert(motion.Advance(180000,false)==0);
    assert(motion.Advance(360000,true)==1);
    assert(motion.Advance(540000,false)==0 && motion.Advance(1,true)==0);
    // Actual native entrance parks stones outside the source viewport. Our
    // normalization must restore them before clipping, without editing the game.
    const auto before=e.bytes;
    e.Write(Owner+0x14,14);
    for(unsigned q=3;q<=14;++q)for(unsigned v=0;v<4;++v)
        e.Put(pos+q*48+v*12,e.Get<float>(pos+q*48+v*12)+300);
    const auto normal=Atlas(e,l,true);
    for(unsigned q=3;q<=14;++q)for(unsigned v=0;v<4;++v) {
        assert(normal.positions[q*12+v*3]==g.positions[q*12+v*3]);
        assert(normal.colors[q*16+v*4+3]==g.colors[q*16+v*4+3]);
    }
    // Test the real presentation hook through Start, open, B, native teardown,
    // invalid state, and time rewind. Guest rendering is intentionally not run.
    e.bytes=before;Hook hook;std::array<U32,16> regs{};
    const auto frame=[&](U32 state,std::int64_t time) {
        e.Write(Owner+0x14,state);hook.stage=Hook::Stage::Idle;
        hook.atlas.stage=decltype(hook.atlas)::Stage::Idle;
        hook.Begin(e,regs,0x1234,true,time);
    };
    frame(12,0);frame(12,150000);assert(hook.fade.alpha==1);
    frame(13,166667);assert(hook.slide.progress==0 && hook.last_list.count==0 && hook.fade.alpha==1);
    const float first=hook.offset;
    frame(4,250000);assert(hook.offset>first && hook.offset<0 && hook.fade.alpha==1);
    frame(4,400000);assert(hook.offset==0 && hook.fade.alpha==1);
    const auto stable=hook.last_list.positions;
    back_title=e.Read(Owner+0x10).value_or(0);
    assert(back_title!=0);
    frame(10,416667);assert(hook.offset<0 && hook.last_list.positions==stable && hook.fade.alpha==1);
    assert(hook.draw_native && back_title_visible);
    frame(11,450000);assert(hook.Pending() && hook.last_list.positions==stable && hook.fade.alpha==1);
    assert(hook.draw_native && back_title_visible);
    frame(11,600000);assert(!hook.draw_native && !back_title_visible);
    frame(12,650000);assert(hook.slide.progress==0 && hook.fade.alpha==1);
    frame(0,670000);assert(hook.last_list.count==0 && slide_x==0);
    frame(4,700000);frame(4,900000);assert(hook.slide.progress==1);
    frame(4,1000);assert(hook.slide.progress==0);
    // A recognized song closes the reference list while native effects take
    // over. It must not reappear through the original bottom-screen draw.
    e.Write(Env::Play+0x2b80,0);e.Write(Env::Play+0x2a90,12);
    frame(4,201000);assert(hook.slide.progress==1);
    e.Write(Env::Play+0x2a90,13);e.Write(Owner+0x44,0);
    frame(4,217667);assert(hook.song_completed && !hook.draw_native && hook.slide.progress<1);
    frame(4,417667);assert(hook.slide.progress==0);
    on_top=false;regs[15]=0;hook.Bottom(regs);assert(regs[15]==Sites[3].pc+4);
    e.Write(Env::Play+0x2a90,12);e.Write(Owner+0x44,1);
    frame(12,450000);assert(!hook.song_completed);
    // Execute the actual cartridge title, notation and learned-song-artwork
    // routines during the accelerated entrance, not just their call stubs.
    e.bytes=before;e.Setup(12);e.details=true;cpu.ClearCache();
    e.Write(0x587958+0xbc,0xffffffff);e.Write(Owner+0x20,0);
    e.Pad(8);e.Update();assert(e.Read(Owner+0x14)==13);
    e.Update();assert(e.Read(Owner+0x14)==4);
    assert(e.titles==1 && e.note_batches==1 && e.stone_batches==24);
    assert(e.Read(Owner+0x10)==0xb00000 && e.Read(Owner+0x20)==0);
    // In the real learned-song refresh, the colored artwork's height is 36,
    // not the initial static rectangle. Every slide frame must retain it.
    const auto ready_art=Atlas(e,Layout{});
    const float final_height=ready_art.positions[18*12+7]-ready_art.positions[18*12+1];
    assert(Near(final_height,36*.75f));
    for(float shift:{-300.f,-100.f,-10.f,0.f}) {
        const auto draw=WithPanel(ready_art,Layout{},shift,0);
        assert(Near(draw.positions[(18+AtlasDrawOffset)*12+7]-draw.positions[(18+AtlasDrawOffset)*12+1],final_height));
    }
    // Native notation leaves unused position/UV stack entries unwritten and
    // emits zero-size quads from them. Exercise the real rectangle writers with
    // dirty stack memory, including the unlearned -> learned selection change.
    e.native_geometry=true;cpu.ClearCache();
    e.Write(Env::Helper+0xc,pos); // Alias native staging/output positions here.
    e.Put(Env::Helper+4,512.f);e.Put(Env::Helper+8,256.f);
    for(U32 poison:{0x7fc00000u,0x7f000000u,0xff800000u})
    for(unsigned song=0;song<12;++song)for(bool learned:{false,true,false,true}) {
        e.Write(Owner+0x18,song%4);e.Write(Owner+0x1c,song/4);
        e.Write(0x587958+0xbc,learned?0xffffffff:0);
        for(U32 at=Env::SP-0x1000;at<Env::SP;at+=4)e.Write(at,poison);
        e.Call(0x43c104);e.Call(0x2f67d4);
        const unsigned notes=learned?e.Get<U32>(0x4d53c8+song*4):0;
        assert(notes<=8);
        if(notes<8) {
            const float unused=e.Get<float>(uv+(39+notes)*32);
            assert(!std::isfinite(unused) || std::abs(unused)>100000);
        }
        const auto refreshed=Atlas(e,Layout{});
        assert(refreshed.count==QuadCount);
        for(unsigned q=0;q<QuadCount;++q)if(q<39 || q>46)
            for(unsigned v=0;v<4;++v)
                assert(refreshed.positions[q*12+v*3]==ready_art.positions[q*12+v*3]);
        for(unsigned n=0;n<8;++n) {
            const auto q=39+n;
            for(unsigned v=0;v<4;++v)
                assert(refreshed.colors[q*16+v*4+3]==(n<notes?1.f:0.f));
        }
        const auto combined=WithPanel(refreshed,Layout{},0,0);
        for(float x:combined.positions)assert(std::isfinite(x));
        for(float x:combined.uvs)assert(std::isfinite(x));
    }
    e.native_geometry=false;e.details=false;cpu.ClearCache();
    // A parked native cursor must never become visible after HUD scaling.
    e.bytes=before;hook.stage=Hook::Stage::Native;hook.frame_layout=Layout{};
    constexpr U32 cursor=0xb10000,cm=0xb30000,ch=0xb40000,cp=0xb50000;
    e.Write(cursor+4,cm);e.Write(cursor+8,ch);e.Write(ch+0x10,cp);
    e.Write(Owner+0x18,0);e.Write(Owner+0x1c,0);e.Write(Owner+0x28,0);
    e.Put(cp,400.f);e.Put(cp+4,400.f);
    assert(hook.ModelTransform(e,cm).sx==0);
    e.Put(cp,16.f);e.Put(cp+4,68.f);
    assert(Near(hook.ModelTransform(e,cm).sx,.75f));
    hook.frame_layout.stone_size=.5f;
    assert(Near(hook.ModelTransform(e,cm).sx,.375f));
    e.bytes=before;slide_x=0;
    l[Notes]={17,9,1.5f};auto moved=Atlas(e,l);assert(moved.positions[39*12]!=g.positions[39*12]);
    assert(moved.positions[3*12]==g.positions[3*12]);
    e.Put(pos+3*48,400.f);e.Put(pos+3*48+6*4,400.f);e.Put(pos+3*48+3*4,468.f);e.Put(pos+3*48+9*4,468.f);
    l[Stones]={-200,0,.1f};assert(Atlas(e,l).colors[3*16+3]==0);
    const auto art=PausePrompts::MakeAssets(File(std::string(argv[2])+"/message/parts.ctxb"),File(std::string(argv[2])+"/message/us/ltn16.qbf"));assert(art);
    const auto free=Prompts(Layout{},*art,false,.5f),list=Prompts(Layout{},*art,true,1);
    assert(free.icons.count==2 && list.icons.count==2 && free.text.count==11 && list.text.count==9);
    assert(free.icons.uvs[0]==224.5f/256 && free.icons.uvs[1]==95.5f/128 && free.icons.uvs[8]==144.5f/256);
    assert(free.icons.colors[3]==.5f && free.text.colors[3]==.5f);
    assert(free.icons.positions==list.icons.positions && free.icons.uvs==list.icons.uvs);
    assert(Prompts(Layout{},*art,true,1,false).icons.count==1);
    // Result-note filtering is confined to verified message panes, and the
    // original material sampler is restored after submission.
    PlaybackNoteFilter filter;
    constexpr U32 renderer=0xb10000,note_pane=0xb11000,note_model=0xb12000,note_mesh=0xb13000;
    e.Write(Message,0x4ed0a0);e.Write(Message+0x7c0,note_pane);
    e.Write(note_pane,0x4ed234);e.Write(note_pane+4,Message+0x168);e.Write(note_pane+0x64,note_model);
    e.Write(note_model,0x4ebd60);e.Write(renderer+0x354,note_model);e.Write(renderer+0x350,note_mesh);
    e.Write(note_mesh+0x13c,0);e.Write(note_mesh+0x140,2);
    filter.Material(e,renderer);assert(e.Read(note_mesh+0x13c)==1 && e.Read(note_mesh+0x140)==3);
    filter.Restore(e);assert(e.Read(note_mesh+0x13c)==0 && e.Read(note_mesh+0x140)==2);
    e.Write(note_pane+4,Message+0x16c);filter.Material(e,renderer);
    assert(e.Read(note_mesh+0x13c)==0 && e.Read(note_mesh+0x140)==2 && !filter.mesh);
    Fade fade;assert(fade.Advance(0,true)==0);assert(Near(fade.Advance(75000,true),.5f));
    assert(fade.Advance(150000,true)==1);assert(Near(fade.Advance(225000,false),.5f));assert(fade.Advance(300000,false)==0);
    assert(fade.Advance(100,true)==0);
    // Audit every native action's real instruction-message selection, rather
    // than assigning 086e to all the synthetic scenarios. Stop at the native
    // message factory; only audio setup is stubbed, not the action dispatcher.
    e.audit_message=true;cpu.ClearCache();
    for(U32 action=0;action<=0x31;++action) {
        e.Setup(0);e.Write(0x55a21c,1);e.requested_message=0;
        auto& r=cpu.Regs();r[0]=Env::Play;r[1]=action;
        r[13]=Env::SP;r[14]=Env::Stop;r[15]=0x3438a4;e.remaining=1000000;
        cpu.Run();assert(r[15]==0x2d0324);
        const U32 expected=action==0 || action==1 || (action>=0x22 && action!=0x2b && action!=0x2d && action!=0x2e)
            ?0x86e:action<0xf?0x86e + action:action==0x2b || action==0x2d?0x8a0:0x86d;
        if(e.requested_message!=expected)
            std::cerr<<"native action "<<std::hex<<action<<" message "<<e.requested_message<<" expected "<<expected<<'\n';
        assert(e.requested_message==expected);
        e.Write(Env::Play+0x2b80,0x00ff0000|action);
        e.Write(Message,0x4ed0a0);e.Write(Message+0xf40,e.requested_message);
        const bool hide=action<=1 || (action>=0x22 && action<=0x29) || action>=0x30;
        for(U32 phase:{5u,9u,12u}) {
            e.Write(Env::Play+0x2a90,phase);
            auto draw=r;draw[0]=Message;draw[14]=Env::Stop;draw[15]=Sites[7].pc+4;
            DrawMessage(e,draw,true);assert((draw[15]==Env::Stop)==hide);
        }
    }
    std::cout<<"Ocarina: all 50 native action/message paths audited, live fountain prompt regression, rolling recognition for all 12 learned songs after wrong notes, unlearned-song rejection, lessons/recording/minigame exclusions, completion slide, native Start/list/back/cancel paths, held-key isolation, all 96 cursor moves, both stone grids, independent size/spacing, parked cursor rejection, persistent prompts, native title/notation/artwork preparation before sliding, full shadow-mesh midpoint slide routing and reset guards, note panel, hidden sprites, touch mapping, native prompt assets, shared panel/stone transitions, exit caching, right-border UVs and fades passed\n";
}
