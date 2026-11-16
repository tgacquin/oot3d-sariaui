// SPDX-License-Identifier: GPL-2.0-or-later
// Execute the verified native Navi, health-alarm and ladder-speed branches.
#include "oot3d/qol_hooks.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace OOT3D;
using namespace OOT3D::QOL;
struct Env : Dynarmic::A32::UserCallbacks {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    std::uint64_t ticks=0;std::int64_t now=1000000;
    unsigned sounds=0;
    bool bombchu_test=false,stone_test=false;
    std::vector<U32> stone_calls;
    constexpr static std::array<U32,8> StoneHelpers{0x3510b0,0x3532e8,0x3532c0,
        0x353ec8,0x34f6bc,0x358ef8,0x358ea8,0x350be0};
    constexpr static U32 Play=0x8100000,Player=0x8200000,Stop=0xf00000,SP=0xe00000;
    template<class T> T Get(U32 a) {if(a>=0x8000000)a-=0x7800000;assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {if(a>=0x8000000)a-=0x7800000;assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(stone_test && a==StoneKill)return 0xef000002;
        if(stone_test && std::find(StoneHelpers.begin(),StoneHelpers.end(),a)!=StoneHelpers.end())return 0xef000003;
        if(bombchu_test && (a==0x3cb2e0 || a==0x3cb35c || a==0x3cb364))return 0xef000000;
        if(a==0x3738d4 || a==Stop || a==0x4be32c || a==0x267f90 || a==0x267e04 ||
           a==0x267f74 || a==0x471758 || a==0x47177c)return 0xef000000;
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
    void InterpreterFallback(U32 pc,std::size_t) override {std::cerr<<"fallback "<<std::hex<<pc<<'\n';std::abort();}
    void ExceptionRaised(U32 pc,Dynarmic::A32::Exception) override {std::cerr<<"exception "<<std::hex<<pc<<'\n';std::abort();}
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();
        if(svc==0) {cpu->HaltExecution();return;}
        if(svc==1) {++sounds;assert(r[0]==0x010004a0);r[15]=r[14];return;}
        if(svc==2) {stone_calls.push_back(StoneKill);r[1]=0;return;} // Execute the remaining native Actor_Kill instructions.
        if(svc==3) {
            const U32 fn=r[15]-4;stone_calls.push_back(fn);
            if(fn==0x353ec8) {
                // Model the registration so the REAL native destructor must
                // detach it; no allocator or graphics driver is needed here.
                Write(Play+0xaec,r[2]);Write(Play+0x2004,1);
            }
            r[0]=0;r[15]=r[14];return;
        }
        Handle(*this,r,cpu->ExtRegs()[0],svc-FirstSVC,now);
    }
    void Setup() {
        cpu->Reset();taps={};now=1000000;sounds=0;
        disable_navi=disable_heart_beep=fast_ladder=no_bombchu_first_person=false;
        Write(PauseMenu::UI+0xc,Play);Write(PauseMenu::UI+0x14,1);
        Write(PauseMenu::UI+0x20,0);Write(PauseMenu::UI+0x24,0);
        Write(Play+0x100,0x0203);Write(Play+0x7f24,0x100);
        Write(Play+0x5c2c,0);Write(Play+0x7f10,0);Write(HUDFade::Main+0x34,2);
        Write(Play+0x43c,Player);Write(Play+0x20ac,Player);
        Write(Player+0x1708,LadderAction);Write(Player+0x2234,0);Write(Player+0x2238,0);
        Write(Player+0x16f8,0);Write(Player+0x1728,0);
        assert(Gameplay(*this,Play) && Climbing(*this,Player));
    }
    void Pad(bool down,bool live=true) {now+=10000;Poll(*this,down?2:0,live,now);}
    void Charge() {fast_ladder=true;Pad(false);Pad(true);}
    void Run(U32 pc) {
        cpu->Regs()[13]=SP;cpu->Regs()[14]=Stop;cpu->Regs()[15]=pc;
        cpu->SetCpsr(0x10);cpu->ClearHalt();ticks=10000;cpu->Run();assert(ticks);
    }
    float Climb(int stick=40,float previous_speed=1,int horizontal=0) {
        auto& r=cpu->Regs();r[4]=Player;r[6]=Player+0x2200;r[7]=stick;r[8]=horizontal;r[10]=Play;
        Write(Player+0x294,std::bit_cast<U32>(previous_speed));
        Run(0x4be270);
        return std::bit_cast<float>(*Read(Player+0x294));
    }
};

int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream file(argv[1],std::ios::binary|std::ios::ate);assert(file);
    const auto size=file.tellg();file.seekg(0);file.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);
    assert(file && CodeMatches(e));
    for(const auto& site:Sites) {
        e.Write(site.pc,site.instruction^1);assert(!CodeMatches(e));
        assert(Instruction(site.pc,site.instruction^1)==(site.instruction^1));
        e.Write(site.pc,site.instruction);
    }
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    // Verify tutorial actor -> profile -> object archive in the original ROM.
    assert(e.Read(0x50cd84+0x1d9*32+20)==0x526438);
    assert(e.Read(0x526438+8)==0x194);
    assert(std::memcmp(e.bytes.data()+0x543844,"rom:/actor/zelda_hintstone.zar",28)==0);
    for(bool hide:{false,true})for(U32 actor:{0x1d9u,0x1d8u,0x120u,0u}) {
        e.Setup();hide_sheikah_stones=hide;
        auto& r=cpu.Regs();for(unsigned i=0;i<13;++i)r[i]=0x100+i;
        r[2]=actor;const auto before=r;
        e.Run(0x3738d0);
        // The common spawn entry is now untouched for EVERY actor and toggle state.
        assert(r[13]==Env::SP-52 && r[15]==0x3738d4+4 && r[0]==before[0]);
        for(unsigned i=1;i<13;++i)assert(r[i]==before[i]);
        for(unsigned i=0;i<12;++i)assert(e.Read(Env::SP-52+4*i)==before[i]);
        assert(e.Read(Env::SP-4)==Env::Stop);
    }
    // Execute the stone's complete native initializer with resource services
    // stubbed, then its real cleanup. The hook cannot skip initialization or
    // touch the player, and preserves the caller's stack/callee registers.
    constexpr U32 stone=0x8300000;
    for(bool hide:{false,true})for(unsigned age:{0u,1u}) {
        e.Setup();hide_sheikah_stones=hide;e.stone_test=true;e.stone_calls.clear();
        e.Write(stone,0x601d9);e.Write(stone+4,9);e.Write(stone+0x1c,0);
        e.Write(stone+0x138,0x24be24);e.Write(stone+0x13c,0x281834);e.Write(stone+0x140,0x2817cc);
        e.Write(Env::Play+0x3a64,1);e.Put<std::uint8_t>(0x587958+0xe,age);
        const auto player=std::vector<unsigned char>(e.bytes.begin()+Env::Player-0x7800000,
            e.bytes.begin()+Env::Player-0x7800000+0x2a4c);
        auto& r=cpu.Regs();for(unsigned i=4;i<=11;++i)r[i]=0xabcdef00+i;
        const auto before=r;r[0]=stone;r[1]=Env::Play;e.Run(0x24bd08);
        assert(r[13]==Env::SP && r[15]==Env::Stop+4);
        for(unsigned i=4;i<=11;++i)assert(r[i]==before[i]);
        assert(std::equal(player.begin(),player.end(),e.bytes.begin()+Env::Player-0x7800000));
        assert(e.Read(stone+0x1bc)==e.Read(0x24be20)); // Native action initialized.
        assert(e.Read(stone+0x138)==0x24be24); // Destructor survives removal.
        assert(e.stone_calls[0]==0x3510b0 && e.stone_calls[3]==0x353ec8);
        const auto skeleton=std::find(e.stone_calls.begin(),e.stone_calls.end(),0x358ea8);
        assert(skeleton!=e.stone_calls.end());
        if(hide)assert(e.stone_calls.back()==StoneKill && skeleton+1==e.stone_calls.end()-1);
        const bool removed=hide || age!=0;
        assert(e.Read(stone+0x13c)==(removed?0u:0x281834u));
        assert(e.Read(stone+0x140)==(removed?0u:0x2817ccu));
        assert(e.Read(stone+4)==(removed?8u:9u));
        assert(e.Read(Env::Play+0xaec)==stone); // Registered until ordinary cleanup.
        if(removed) {
            r[0]=stone;r[1]=Env::Play;e.Run(*e.Read(stone+0x138));
            assert(e.stone_calls.back()==0x350be0 && e.Read(Env::Play+0xaec)==0);
            assert(e.Read(stone+0x1a4)==0xffffffff && (*e.Read(Env::Play+0x2004)&3)==3);
            assert(r[13]==Env::SP && r[15]==Env::Stop+4);
        }
        e.stone_test=false;
    }
    // Even if given the stone epilogue with a mismatched actor, never remove it.
    for(U32 actor:{0u,0x120u,0x1d8u}) {
        e.Setup();hide_sheikah_stones=true;e.Write(stone,actor);
        auto& r=cpu.Regs();r[4]=stone;r[13]=Env::SP-24;r[15]=Sites[5].pc+4;
        const auto before=r;Handle(e,r,0,5,e.now);
        assert(r[13]==Env::SP && r[15]==before[15] && r[14]==before[14]);
    }
    hide_sheikah_stones=false;
    // Run the native speed calculation, including its min/max clamp and sign.
    for(int stick:{-80,-40,40,80})for(float sign:{-1.f,1.f}) {
        e.Setup();const float native=e.Climb(stick,sign);e.Charge();
        assert(e.Climb(stick,sign)==std::copysign(std::min(std::abs(native),3.f),native)*2.f);
        fast_ladder=false;assert(e.Climb(stick,sign)==native);
    }
    // Vines/walls share this action and select the dominant stick axis.
    for(unsigned mode:{1u,2u})for(int stick:{-80,-40,40,80})
    for(bool horizontal:{false,true})for(float sign:{-1.f,1.f}) {
        e.Setup();e.Write(Env::Player+0x2234,mode<<24);
        const int y=horizontal?0:stick,x=horizontal?stick:0;
        const float normal=e.Climb(y,sign,x);e.Charge();
        assert(e.Climb(y,sign,x)==std::copysign(std::min(std::abs(normal),3.f),normal)*2.f);
        fast_ladder=false;assert(e.Climb(y,sign,x)==normal);
    }
    e.Setup();const float idle=e.Climb(0);e.Charge();assert(e.Climb(0)==idle);
    e.Setup();const float native=e.Climb();e.Charge();
    assert(e.Climb()==native*2.f); // First press works immediately.
    e.Pad(true);assert(e.Climb()==native*2.f); // One following native update.
    e.Pad(true);assert(e.Climb()==native); // Holding cannot retrigger.
    for(int i=0;i<36;++i){e.Pad(true);assert(e.Climb()==native);}
    e.Pad(false);e.Pad(true);assert(e.Climb()==native*2.f); // Slow taps also burst.
    // An edge during the latched update does not extend MM's two-update burst.
    e.Pad(false);e.Pad(true);assert(e.Climb()==native*2.f);
    e.Pad(false);assert(e.Climb()==native);
    e.Setup();e.Charge();Reset();assert(e.Climb()==native);
    // Editor/movie input, pause, transitions and leaving the ladder discard taps.
    for(unsigned gate=0;gate<7;++gate) {
        e.Setup();e.Charge();
        if(gate==0)e.Pad(true,false);
        if(gate==1)e.Write(PauseMenu::UI+0x24,1);
        if(gate==2)e.Write(Env::Play+0x7f10,0x10000);
        if(gate==3)e.Write(Env::Player+0x1708,0);
        if(gate==4)e.Write(Env::Player+0x2238,2); // Not a climbing stride.
        if(gate==5)e.Write(Env::Player+0x2238,0xfffe); // Mounting.
        if(gate==6)e.now-=500000; // State rewind.
        if(gate)e.Pad(true);
        assert(!taps.Boost(Env::Player,e.now));
        e.Write(PauseMenu::UI+0x24,0);e.Write(Env::Play+0x7f10,0);
        e.Write(Env::Player+0x1708,LadderAction);e.Write(Env::Player+0x2234,0);
        e.Write(Env::Player+0x2238,0);
        e.Pad(true);assert(!taps.Boost(Env::Player,e.now));
    }
    // Native Navi branch: bypass only the unsolicited-reminder section.
    for(bool disabled:{false,true}) {
        e.Setup();disable_navi=disabled;
        auto& r=cpu.Regs();r[0]=Env::Player+0x4000;r[1]=Env::Play;
        e.Write(r[0]+0x99c,0x1234012c);e.Run(0x267dc0);
        assert(r[15]==(disabled?0x267f94u:0x267e08u));
        assert(e.Read(Env::Player+0x499c)==(disabled?0x12340000u:0x1234012cu));
        if(!disabled)assert(r[0]==0x587958);
        e.Setup();disable_navi=disabled;e.Put<std::uint16_t>(Env::Player+0x1728,0x123);
        r[0]=Env::Player+0x4000;r[1]=Env::Play;e.Run(0x267dc0);
        assert(r[15]==0x267f78 && e.Get<std::uint16_t>(Env::Player+0x1728)==0x123);
    }
    // Both native heartbeat branches keep their visual counter and suppress
    // only the actual sound call. Disabling the option restores the call.
    for(unsigned count:{0u,7u})for(bool disabled:{false,true}) {
        e.Setup();disable_heart_beep=disabled;
        cpu.Regs()[4]=Env::Play+0x2da4;e.Put<std::uint16_t>(Env::Play+0x2e0a,count);
        e.Run(0x471720);
        assert(e.sounds==(disabled?0u:1u));
        assert(e.Get<std::uint16_t>(Env::Play+0x2e0a)==count+1);
    }
    e.bombchu_test=true;
    for(unsigned actor:{0xdau,0x10u,0x100u})for(bool disabled:{false,true}) {
        e.Setup();no_bombchu_first_person=disabled;
        auto& r=cpu.Regs();r[0]=actor;r[6]=Env::Player+0x1000;
        e.Write(Env::Player+0x1710,0x12345678);
        const auto before=e.bytes;
        const auto fp=std::vector<U32>(cpu.ExtRegs().begin(),cpu.ExtRegs().end());
        e.Run(0x3cb2d4);
        assert(e.bytes==before && std::equal(fp.begin(),fp.end(),cpu.ExtRegs().begin()));
        const auto stop=actor!=0xda?0x3cb364u:disabled?0x3cb35cu:0x3cb2e0u;
        assert(r[15]==stop+4);
        assert(r[0]==(actor==0xda && !disabled?0x12345678u:actor));
        assert(r[6]==Env::Player+0x1000);
    }
    std::cout<<"Native QOL: stone initialization/removal/destructor and untouched common spawn, Navi reminder/scripted-dialogue separation, both heartbeat branches, "
                "MM two-update 2x climbing bursts, base clamp, holding, vines, mount, menu, movie/editor and state resets passed\n";
}
