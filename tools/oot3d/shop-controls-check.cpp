// SPDX-License-Identifier: GPL-2.0-or-later
// Execute the native shop selection/pan and prompt draws, stubbing only
// rendering, audio and message services. No game window or save is needed.
#include "oot3d/dpad_input.h"
#include "oot3d/prices.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace OOT3D::Shops;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 ActorAddress=0x800000,Play=0x900000,Item=0xa00000,SP=0xe00000,Stop=0x700000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu{};
    UpdateHook hook;
    bool patched=true;
    bool cursor_test=false;
    bool real_position=false;
    bool input_test=false;
    U32 cursor_return=0;
    struct CursorDraw {U32 visible,flags,fpscr;float scale;std::array<float,3> position;};
    std::vector<CursorDraw> cursors;
    unsigned sounds=0,messages=0,queries=0;
    struct Draw {std::array<float,4> rect;std::array<U32,4> rgba;std::array<U32,12> matrix;U32 model,depth;};
    std::vector<Draw> draws;
    std::uint64_t ticks{};
    template<class T> T Get(U32 a) {assert(a+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[a],sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(&bytes[a],&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {if(a+4>bytes.size())return {};return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop || a==0x103fc8)return 0xef000000;
        if(input_test && a==0x103d8c)return 0xef000000;
        if(cursor_test && a==cursor_return)return 0xef000000;
        if(cursor_test && a==0x35e050)return 0xef000008;
        if(a==0x37547c)return 0xef000001;
        if(a==0x36be34)return 0xef000002;
        if(a==0x34e32c)return 0xef000003;
        if(a==0x3695f8)return 0xef000004; // Allow the area-title draw.
        if(a==0x3769d8)return 0xef000004;
        if(a==0x2d04c0)return 0xef000005;
        if(a==0x34e27c && !cursor_test && !real_position)return 0xef000006;
        if(a==0x34e418)return 0xef000007; // Item presentation, independent of navigation.
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
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();auto& fp=cpu->ExtRegs();
        if(!svc) {cpu->HaltExecution();return;}
        // Production dispatch checks purchase hooks before shop controls.
        // Exercise that order: isolated shop-only callbacks hid an ID collision.
        if(svc>=OOT3D::Prices::FirstSVC && svc<OOT3D::Prices::FirstSVC+OOT3D::Prices::Sites.size()) {
            OOT3D::Prices::Handle(*this,r,svc-OOT3D::Prices::FirstSVC);return;
        }
        if(svc>=FirstSVC && svc<FirstSVC+Sites.size()) {
            auto flags=cpu->Cpsr(),fpscr=cpu->Fpscr();
            hook.Handle(*this,r,fp,flags,fpscr,svc-FirstSVC);
            cpu->SetCpsr(flags);cpu->SetFpscr(fpscr);return;
        }
        if(svc==1) {assert(r[0]==0x0100048e);++sounds;}
        if(svc==2) {
            ++messages;Write(0x5c27b8,r[1]);
            // Exercise full volatile register/flag restoration on an injected
            // message call, not an accidentally well-behaved test stub.
            for(unsigned i=0;i<16;++i)fp[i]=0xdeadbeef;
            cpu->SetCpsr(cpu->Cpsr()^0xf0000000);cpu->SetFpscr(0);
        }
        if(svc==3)Put(r[0]+0x37c,fp[0]);
        if(svc==4)++queries;
        if(svc==5) {
            Draw d{};
            for(unsigned i=0;i<4;++i) {d.rect[i]=Get<float>(r[3]+i*4);d.rgba[i]=Get<U32>(r[2]+i*4);}
            for(unsigned i=0;i<12;++i)d.matrix[i]=Get<U32>(Get<U32>(r[13]+8)+i*4);
            d.model=Get<U32>(r[13]);d.depth=fp[0];draws.push_back(d);
        }
        if(svc==8) {
            assert(r[0]==Play && r[1]==ActorAddress && r[2]==ActorAddress+0x2d4);
            assert(r[14]==cursor_return);
            CursorDraw draw{r[3],cpu->Cpsr(),cpu->Fpscr(),std::bit_cast<float>(fp[0]),{}};
            for(unsigned i=0;i<3;++i)draw.position[i]=Get<float>(r[2]+4*i);
            for(unsigned i=1;i<fp.size();++i)assert(fp[i]==std::bit_cast<U32>(float(i)+.125f));
            cursors.push_back(draw);
        }
        assert(svc<=8);
        // Native callees may clobber caller-saved registers. Verify the hook
        // does not depend on accidental preservation by a host compiler.
        r[0]=svc==4?0:0xdeadbeef;r[1]=r[2]=r[3]=r[12]=0xdeadbeef;
        r[15]=r[14];
    }
    void ResetActor() {
        hook={};navigation_input={};OOT3D::DPad::input={};Write(0x5c1878,0x4ed0a0);Write(0x5c1884,1);Write(0x5c27b8,0);
        Put<std::uint8_t>(0x587958+0xe,0);
        std::fill(bytes.begin()+ActorAddress,bytes.begin()+ActorAddress+0x584,0);
        Put<std::uint16_t>(ActorAddress,0x3d);Write(ActorAddress+0x228,0x103c70);
        Write(0x5043e0,Play);Write(Play+0x14,0);Write(Play+0x18,0);
        Put(Play+0x3c,0.f);Put(Play+0x40,0.f);
        Write(0x51b2f4,Play);Put<std::uint16_t>(Play+0x110,2); // Native 30 Hz step.
        Write(0x55a210,1);Write(0x55a21c,1); // Native sprite helpers initialized.
        for(unsigned i=0;i<12;++i)Put(0x5be29c+4*i,i%5==0?1.f:0.f);
        Write(0x5c18bc+0x528,Play);Put(Play+0x88,0.f);
        for(unsigned i=0;i<8;++i) {
            Write(ActorAddress+0x2a4+i*4,Item+i*0x200);Put<std::uint16_t>(Item+i*0x200,4);
            Put<std::uint16_t>(Item+i*0x200+0x116,0x200+i);
        }
        sounds=messages=queries=0;draws.clear();
    }
    void PollShop(U32 raw,bool live=true) {
        navigation_input.Observe(*this,ActorAddress,Play);
        OOT3D::DPad::input.Filter(*this,raw,live,1000000);
        assert(OOT3D::DPad::input.pending_item==OOT3D::DPad::ElementCount);
        // Native input has no directions; the production HID path routes them.
        Write(Play+0x14,raw);
    }
    void UpdateInput() {
        input_test=true;cpu->ClearCache();cpu->Reset();auto& r=cpu->Regs();
        r[4]=ActorAddress;r[6]=Play;r[13]=SP;r[14]=Stop;r[15]=Sites[17].pc;
        ticks=200000;cpu->Run();assert(ticks && r[13]==SP);
        input_test=false;cpu->ClearCache();
    }
    CursorDraw DrawCursor(U32 caller) {
        cursor_test=true;cpu->ClearCache();cpu->Reset();
        cursor_return=caller+4;
        auto& r=cpu->Regs();auto& fp=cpu->ExtRegs();
        for(unsigned i=0;i<r.size();++i)r[i]=0x100+i;
        for(unsigned i=0;i<fp.size();++i)fp[i]=std::bit_cast<U32>(float(i)+.125f);
        r[4]=ActorAddress;r[5]=Play;r[13]=SP;r[14]=Stop;r[15]=caller-20;
        cpu->SetCpsr(0xa0000010);cpu->SetFpscr(0x01000000);
        const auto count=cursors.size();ticks=200000;cpu->Run();
        assert(ticks && r[13]==SP && cursors.size()==count+1 && !hook.cursor.pending);
        assert(cursors.back().flags==(cursors.back().visible?0x20000010u:0x60000010u) &&
            cursors.back().fpscr==0x01000000);
        cursor_test=false;cpu->ClearCache();return cursors.back();
    }
    void Run(U32 pc,bool draw=false) {
        cpu->Reset();auto& r=cpu->Regs();auto& fp=cpu->ExtRegs();
        for(unsigned i=0;i<13;++i)r[i]=0x100+i;
        for(unsigned i=0;i<fp.size();++i)fp[i]=std::bit_cast<U32>(float(i)+.125f);
        r[0]=draw?Play:ActorAddress;r[1]=draw?ActorAddress:Play;
        if(pc==0x4792a4)r[0]=0; // Area title index.
        if(!draw) {r[4]=ActorAddress;r[6]=Play;}
        r[3]=Get<U32>(0x52d9e0+Get<std::uint16_t>(ActorAddress+0x2a0)*4);
        r[13]=SP;r[14]=Stop;r[15]=pc;
        const auto original=r;const auto original_fp=fp;
        ticks=200000;cpu->Run();assert(ticks && r[13]==SP);
        for(unsigned i=4;i<=11;++i)assert(r[i]==original[i]);
        for(unsigned i=16;i<32;++i)assert(fp[i]==original_fp[i]);
    }
};
int main(int argc,char** argv) {
    for(const auto& site:Sites) {
        const auto svc=Instruction(site.pc,site.instruction)&0xffffff;
        assert(!(svc>=OOT3D::Prices::FirstSVC && svc<OOT3D::Prices::FirstSVC+OOT3D::Prices::Sites.size()));
    }
    assert(argc==2);Env e;std::ifstream f(argv[1],std::ios::binary|std::ios::ate);assert(f);
    const auto size=f.tellg();f.seekg(0);assert(size>0 && size<0x700000);
    f.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),size);assert(f && CodeMatches(e));
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    // Run the game's title-card draw for the lower overworld and centered dungeon
    // variants. Compare its submitted geometry and material with the unpatched draw.
    for(unsigned y:{120u,192u})for(unsigned phase:{48u,255u}) {
        e.ResetActor();e.Write(e.ActorAddress,e.Item);
        e.Write(e.ActorAddress+4,(y<<16)|200);e.Write(e.ActorAddress+8,(36u<<16)|168);
        e.Put(e.ActorAddress+12,1.f);e.Write(e.ActorAddress+16,phase<<16);
        e.Put<std::uint16_t>(e.ActorAddress+20,phase);
        e.Write(0x50c8e8+0x54,4);e.Put(0x50c8e8+0x60,.2f);
        e.patched=false;cpu.ClearCache();e.Run(0x4792a4,true);
        assert(e.draws.size()==1);const auto native=e.draws.back();
        e.patched=true;cpu.ClearCache();
        for(float scale:{.25f,1.f,3.f})for(bool reposition:{false,true}) {
            OOT3D::HUD::Layout layout;layout.area_title_scale=scale;
            layout.reposition_area_titles=reposition;layout.area_title_y=-240;
            OOT3D::HUD::SetLayout(layout,false);e.draws.clear();e.Run(0x4792a4,true);
            assert(e.draws.size()==1);const auto& draw=e.draws.back();
            for(unsigned edge=0;edge<4;++edge) {
                const float center=(native.rect[edge]+native.rect[edge^1])*.5f;
                const float dy=(edge<2 && reposition && y==192)?-240.f:0.f;
                assert(draw.rect[edge]==center+dy+(native.rect[edge]-center)*scale);
            }
            assert(draw.rgba==native.rgba && draw.matrix==native.matrix &&
                   draw.depth==native.depth && draw.model==native.model);
        }
    }
    OOT3D::HUD::SetLayout({},false);
    // Shop entry/idle, greeting, purchases and all other actions must dispatch
    // exactly like the native BLX, including its return address and flags.
    // Step the real JIT substitution through the production price-before-shop
    // callback order instead of calling Handle directly.
    for(unsigned state=0;state<27;++state) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
        e.Put(e.Play+0x3c,0.f);cpu.Reset();
        auto& r=cpu.Regs();
        for(unsigned i=0;i<r.size();++i)r[i]=0x100+i;
        r[0]=e.ActorAddress;r[1]=e.Play;r[3]=e.Get<U32>(0x52d9e0+state*4);
        r[13]=e.SP;r[14]=e.Stop;r[15]=Sites[0].pc;
        auto expected=r;expected[14]=Sites[0].pc+4;expected[15]=r[3];
        const auto flags=cpu.Cpsr();const auto fp=cpu.ExtRegs();
        e.ticks=100;cpu.Step();
        assert(r==expected && cpu.Cpsr()==flags && cpu.ExtRegs()==fp);
    }
    // Compare actual native ARM trajectories: each modified update advances
    // the camera by three original updates (1.5x the previous two-step pan),
    // for both sides and the keeper.
    for(unsigned step:{1u,2u,3u})for(unsigned state:{4u,5u,8u})for(float start:{-30.f,30.f}) {
        const float target=state==4?30.f:state==5?-30.f:0.f;
        if(start==target)continue;
        const auto trajectory=[&](bool patched) {
            e.ResetActor();e.patched=patched;cpu.ClearCache();
            e.Put<std::uint16_t>(e.Play+0x110,step);
            e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
            e.Put<std::uint8_t>(e.ActorAddress+0x2fa,state==4?4:0);
            e.Put(e.ActorAddress+0x37c,start);
            std::vector<float> values;
            for(unsigned i=0;i<60 && e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==state;++i) {
                e.Run(Sites[0].pc);values.push_back(e.Get<float>(e.ActorAddress+0x37c));
            }
            assert(!values.empty() && values.back()==target && e.messages==1);
            return values;
        };
        const auto original=trajectory(false),faster=trajectory(true);
        assert(faster.size()==(original.size()+2)/3);
        for(unsigned i=0;i<faster.size();++i)
            assert(std::abs(faster[i]-original[std::min(3*i+2,unsigned(original.size()-1))])<.001f);
    }
    e.patched=true;cpu.ClearCache();
    {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,6);
        OOT3D::DPad::input.pending_item=OOT3D::DPad::Ocarina;
        e.PollShop(0x10);assert(e.Get<U32>(e.Play+0x14)==0 && navigation_input.held==0x10);
        e.PollShop(0x10,false);assert(navigation_input.held==0);
        e.PollShop(0x10);e.Write(0x5043e0,e.Play+0x10000);
        assert(!navigation_input.Poll(e,0x10,true) && navigation_input.held==0);
        e.Write(0x5043e0,e.Play);e.Put<std::uint16_t>(e.ActorAddress+0x2a0,0);
        assert(!navigation_input.Poll(e,0x10,true));
        e.Put<std::uint16_t>(e.ActorAddress+0x2a0,6);
        e.Write(OOT3D::DPad::UI+0x14,1);e.Write(OOT3D::DPad::UI+0x24,1);e.Write(e.Play+0x100,0x203);
        e.PollShop(0x10);assert(navigation_input.held==0);
        e.Write(OOT3D::DPad::UI+0x14,0);e.Write(OOT3D::DPad::UI+0x24,0);
    }
    // Press and release between two 30 Hz updates. The original shelf handler
    // sees one press and repeat does not convert the tap into a second move.
    for(bool pad:{false,true})for(bool mirrored:{false,true}) {
        e.ResetActor();
        e.Put<std::uint16_t>(e.ActorAddress+0x2a0,7);
        e.Put<std::uint8_t>(0x587958+0xe,mirrored);
        navigation_input.Observe(e,e.ActorAddress,e.Play);
        const U32 right=mirrored?0x20:0x10;
        navigation_input.Poll(e,pad?right:0,true,pad?0:right,1008333);
        navigation_input.Poll(e,0,true,0,1012666);
        e.UpdateInput();e.Run(Sites[0].pc);
        assert(navigation_input.tick_press_us==1008333);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==2);
        const auto sounds=e.sounds;
        e.UpdateInput();e.Run(Sites[0].pc);
        assert(!navigation_input.tick_press_us && e.sounds==sounds &&
               e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==2);
    }
    // Compare real ARM repeat accumulators/press flags for a D-pad sequence
    // versus the equivalent stick sequence, including release and reversal.
    for(bool mirrored:{false,true})
    for(unsigned state:{2u,4u,5u,6u,7u,8u})for(unsigned axis=0;axis<2;++axis) {
        const auto sequence=[&](bool dpad) {
            e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
            e.Put<std::uint8_t>(0x587958+0xe,mirrored);
            std::vector<std::array<U32,3>> result;
            for(int direction:{1,1,1,1,1,1,1,1,0,-1,-1,0,1}) {
                const U32 mask=direction>0?(axis?0x40:0x10):direction<0?(axis?0x80:0x20):0;
                e.PollShop(dpad?mask:0);
                const float stick=direction*(mirrored && !axis?-80.f:80.f);
                e.Put(e.Play+0x3c+axis*4,dpad?0.f:stick);
                e.UpdateInput();
                result.push_back({e.Get<U32>(e.ActorAddress+0x2c8),e.Get<U32>(e.ActorAddress+0x2cc),
                    e.Get<U32>(e.ActorAddress+0x2d0)});
                assert(e.Get<float>(e.Play+0x3c+axis*4)==(dpad?0.f:stick));
            }
            return result;
        };
        assert(sequence(true)==sequence(false));
    }
    // Actual shelf/keeper navigation consumes the same D-pad-derived input.
    for(bool mirrored:{false,true})
    for(unsigned state:{2u,4u,5u,6u,7u,8u})for(unsigned axis=0;axis<2;++axis)for(int direction:{-1,1}) {
        const auto navigate=[&](bool dpad) {
            e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
            e.Put<std::uint8_t>(0x587958+0xe,mirrored);
            e.Put<std::uint8_t>(e.ActorAddress+0x2fa,(state==4 || state==6)?4:0);
            const U32 mask=direction>0?(axis?0x40:0x10):(axis?0x80:0x20);
            e.PollShop(dpad?mask:0);e.Write(e.Play+0x18,dpad?mask:0);
            e.Put(e.Play+0x3c+axis*4,dpad?0.f:direction*(mirrored && !axis?-80.f:80.f));
            e.UpdateInput();e.Run(Sites[0].pc);
            return std::array{unsigned(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)),
                unsigned(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)),e.sounds,e.messages};
        };
        assert(navigate(true)==navigate(false));
    }
    for(unsigned state=0;state<27;++state) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
        e.Put(e.Play+0x3c,-45.f);e.Put(e.Play+0x40,37.f);e.PollShop(0x10);
        const bool browsing=state==2 || (state>=4 && state<=8);
        assert(std::bit_cast<float>(NavigationAxis(e,e.ActorAddress,e.Play,0))==(browsing?80.f:-45.f));
        assert(std::bit_cast<float>(NavigationAxis(e,e.ActorAddress,e.Play,1))==37.f);
        e.PollShop(0xf0);
        assert(std::bit_cast<float>(NavigationAxis(e,e.ActorAddress,e.Play,0))==(browsing?0.f:-45.f));
        e.Put<std::uint16_t>(e.ActorAddress,0x42);
        assert(std::bit_cast<float>(NavigationAxis(e,e.ActorAddress,e.Play,0))==-45.f);
    }
    // A small intentional deflection now reaches the native fresh-press path;
    // neutral noise stays ignored, and larger deflections keep repeat timing.
    for(unsigned state:{2u,4u,5u,6u,7u,8u})for(unsigned axis=0;axis<2;++axis)
    for(float stick:{-80.f,-31.f,-30.f,-16.f,-15.f,-1.f,0.f,1.f,15.f,16.f,30.f,31.f,80.f}) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
        e.Put(e.Play+0x3c+axis*4,stick);e.UpdateInput();
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2d0+axis)==unsigned(std::abs(stick)>15));
        if(std::abs(stick)>30)assert(e.Get<std::int32_t>(e.ActorAddress+0x2c8+axis*4)==int(stick));
        assert(e.Get<float>(e.Play+0x3c+axis*4)==stick);
    }
    for(unsigned state:{6u,7u})for(unsigned axis=0;axis<2;++axis)for(int direction:{-1,1}) {
        const auto move=[&](float magnitude) {
            e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
            e.Put<std::uint8_t>(e.ActorAddress+0x2fa,state==6?4:0);
            e.Put(e.Play+0x3c+axis*4,direction*magnitude);
            e.UpdateInput();e.Run(Sites[0].pc);
            return std::array{unsigned(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)),
                unsigned(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)),e.sounds,e.messages};
        };
        assert(move(16)==move(80));
    }
    e.ResetActor();e.Put(e.Play+0x3c,20.f);
    for(unsigned state=0;state<27;++state) {
        e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
        const bool browsing=state==2 || (state>=4 && state<=8);
        assert(std::bit_cast<float>(NavigationAxis(e,e.ActorAddress,e.Play,0))==(browsing?31.f:20.f));
    }
    // Cursor starts on the selected world-space item on the first pan draw,
    // even if it was still hidden/positioned at the other shelf. Execute the
    // real position routine; mock only its final draw submission.
    constexpr std::array cursor_callers{0x131084u,0x1c2820u,0x1c2898u,0x1e1294u,
        0x212258u,0x29326cu,0x35dd9cu,0x3b4068u};
    // All eight native shop renderers feed the same cursor, not just 131028.
    for(U32 caller:cursor_callers)
    for(unsigned state=0;state<27;++state)for(unsigned selected:{0u,2u,4u,6u,8u})for(unsigned visible:{0u,255u}) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
        e.Put<std::uint8_t>(e.ActorAddress+0x2fa,selected);
        e.Put<std::uint8_t>(e.ActorAddress+0x2f9,visible);
        e.Put(e.ActorAddress+0x2e0,.75f);
        for(unsigned i=0;i<3;++i)e.Put(e.ActorAddress+0x2d4+4*i,-100.f-float(i));
        if(selected<8) {
            const U32 item=e.Item+selected*0x200;
            e.Put(item+0x3c,10.f+selected);e.Put(item+0x40,20.f);e.Put(item+0x44,30.f);
            e.Put(e.Play+0x1b8,13.f+selected);e.Put(e.Play+0x1bc,24.f);e.Put(e.Play+0x1c0,30.f);
        }
        const auto before=std::vector(e.bytes.begin()+e.ActorAddress,e.bytes.begin()+e.ActorAddress+0x584);
        const auto draw=e.DrawCursor(caller);
        const bool early=(state==4 || state==5) && selected<8 && (state==4)==(selected>=4);
        const bool browsing=(state==6 || state==7) && selected<8 && (state==6)==(selected>=4) && visible;
        const bool refreshed=early || browsing;
        assert(draw.visible==(early?1:visible) && draw.scale==.75f);
        assert(draw.position==(refreshed?std::array{16.f+selected,28.f,30.f}:std::array{-100.f,-101.f,-102.f}));
        for(unsigned i=0;i<before.size();++i)
            if(!refreshed || i<0x2d4 || i>=0x2e0)assert(e.bytes[e.ActorAddress+i]==before[i]);
        if(refreshed) {
            // An empty shelf slot must keep native cursor visibility.
            e.Write(e.ActorAddress+0x2a4+selected*4,0);
            const auto missing=e.DrawCursor(caller);assert(missing.visible==visible && missing.position==draw.position);
        }
    }
    // Execute the actual shelf update and cursor-position code, not a host
    // replacement. Selection changes after the native position calculation;
    // the very next draw must refresh it without another game update.
    e.real_position=true;cpu.ClearCache();
    const auto cursor_position=[&] {
        return std::array{e.Get<float>(e.ActorAddress+0x2d4),e.Get<float>(e.ActorAddress+0x2d8),
                          e.Get<float>(e.ActorAddress+0x2dc)};
    };
    for(U32 caller:cursor_callers)for(unsigned state:{6u,7u}) {
        e.ResetActor();const unsigned old=state==6?4:0,next=old+2;
        const float stick=state==6?-80.f:80.f;
        e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
        e.Put<std::uint8_t>(e.ActorAddress+0x2fa,old);
        for(unsigned i=0;i<8;++i) {
            e.Put(e.Item+i*0x200+0x3c,10.f+i*30);
            e.Put(e.Item+i*0x200+0x40,20.f);e.Put(e.Item+i*0x200+0x44,30.f);
        }
        e.Put(e.Play+0x1b8,0.f);e.Put(e.Play+0x1bc,50.f);e.Put(e.Play+0x1c0,80.f);
        e.Run(0x34e27c,true);const auto old_position=cursor_position();
        e.Put<std::uint8_t>(e.ActorAddress+0x2fa,next);
        e.Run(0x34e27c,true);const auto new_position=cursor_position();
        assert(old_position!=new_position);
        e.Put<std::uint8_t>(e.ActorAddress+0x2fa,old);
        e.Put(e.Play+0x3c,stick);e.Put<std::int32_t>(e.ActorAddress+0x2c8,int(stick));
        e.Write(e.ActorAddress+0x2d0,1);e.Run(Sites[0].pc);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==next && cursor_position()==old_position);
        const auto sounds=e.sounds,messages=e.messages;
        for(unsigned repeat=0;repeat<2;++repeat) {
            const auto draw=e.DrawCursor(caller);
            assert(draw.position==new_position && draw.visible==255);
            assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==next);
            assert(e.Get<std::uint8_t>(e.ActorAddress+0x2f9)==255);
            assert(e.sounds==sounds && e.messages==messages);
        }
    }
    e.real_position=false;cpu.ClearCache();
    // Draw every frame through the actual pan-to-browsing state change. The
    // native cursor visibility byte is set on the NEXT browsing update, so
    // checking isolated pan states misses a one-frame disappearance here.
    for(U32 caller:cursor_callers)for(unsigned side:{4u,5u}) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,side);
        const unsigned selected=side==4?4:0;
        e.Put<std::uint8_t>(e.ActorAddress+0x2fa,selected);
        const U32 item=e.Item+selected*0x200;
        e.Put(item+0x3c,10.f);e.Put(item+0x40,20.f);e.Put(item+0x44,30.f);
        e.Put(e.Play+0x1b8,13.f);e.Put(e.Play+0x1bc,24.f);e.Put(e.Play+0x1c0,30.f);
        e.Put(e.ActorAddress+0x2e0,.1f);
        assert(e.DrawCursor(caller).visible);
        unsigned frames=0;
        while(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==side) {
            assert(++frames<30);e.Run(Sites[0].pc);
            assert(e.DrawCursor(caller).visible);
        }
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==side+2);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2f9)==0);
        // Multiple draws before the next update must remain visible too.
        assert(e.DrawCursor(caller).visible);
        e.Run(Sites[0].pc);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2f9)==255 && e.DrawCursor(caller).visible);
        // Once native browsing takes over, its later hides remain effective.
        e.Put<std::uint8_t>(e.ActorAddress+0x2f9,0);
        assert(!e.DrawCursor(caller).visible);
    }
    // Selecting the keeper replaces the item description on that very input
    // update, before its camera has moved. The settle must not reopen it.
    for(bool left:{false,true})for(unsigned masks:{0u,0xf00u})for(bool mask_shop:{false,true}) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x1c,mask_shop?10:0);
        e.Put<std::uint16_t>(0x588866,masks);
        e.Put<std::uint16_t>(e.ActorAddress+0x2a0,left?6:7);
        e.Put<std::uint8_t>(e.ActorAddress+0x2fa,left?4:0);
        const float angle=left?30.f:-30.f,stick=left?80.f:-80.f;
        e.Put(e.ActorAddress+0x37c,angle);e.Put(e.Play+0x3c,stick);
        e.Put<std::int32_t>(e.ActorAddress+0x2c8,int(stick));e.Write(e.ActorAddress+0x2d0,1);
        e.Write(0x5c27b8,0x200+(left?4:0));e.Run(Sites[0].pc);
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==8);
        assert(e.Get<float>(e.ActorAddress+0x37c)==angle);
        assert(e.messages==1 && e.Read(0x5c27b8)==(mask_shop?(masks?0x70ad:0x70a2):0x83));
        e.Put(e.Play+0x3c,0.f);e.Write(e.ActorAddress+0x2c8,0);e.Write(e.ActorAddress+0x2d0,0);
        for(unsigned i=0;i<30 && e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==8;++i)e.Run(Sites[0].pc);
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==2 && e.messages==1);
    }
    for(float direction:{-1.f,1.f})for(bool fresh:{false,true})for(unsigned row:{0u,1u}) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,8);
        e.Put<std::uint8_t>(e.ActorAddress+0x2fa,row);
        const float angle=direction*(fresh?20.f:8.f);
        e.Put(e.ActorAddress+0x37c,angle);e.Put(e.Play+0x3c,direction*80);
        e.Put<std::int32_t>(e.ActorAddress+0x2c8,int(direction*(fresh?80:800)));
        e.Write(e.ActorAddress+0x2d0,fresh?1:0);
        e.Write(e.Play+0x18,fresh?(direction<0?0x20000000u:0x10000000u):0);
        e.Run(Sites[0].pc);
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==(direction<0?4:5));
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==(direction<0?4:0)+row);
        assert(e.Get<float>(e.ActorAddress+0x37c)==angle && e.sounds==1 && e.queries==0);
        assert(e.messages==1 && e.Read(0x5c27b8)==0x200+(direction<0?4:0)+row);
        // The original side-pan handler continues from the current angle,
        // crosses the keeper, and ends with the native item description.
        e.Put(e.Play+0x3c,0.f);e.Write(e.Play+0x18,0);e.Write(e.ActorAddress+0x2c8,0);e.Write(e.ActorAddress+0x2d0,0);
        for(unsigned i=0;i<20 && e.Get<std::uint16_t>(e.ActorAddress+0x2a0)<6;++i)e.Run(Sites[0].pc);
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==(direction<0?6:7));
        assert(e.Get<float>(e.ActorAddress+0x37c)==-direction*30 && e.messages==1);
    }
    // Release/re-press at the keeper, then again while the destination shelf
    // is still panning. Include HID's direction-pressed bits, not just analog X.
    for(float direction:{-1.f,1.f}) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,8);
        e.Put(e.ActorAddress+0x37c,direction*30);
        e.Run(Sites[0].pc); // Released: native camera starts returning to center.
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==8);
        const auto flick=[&](float x) {
            e.Put(e.Play+0x3c,x*80);e.Write(e.Play+0x18,x<0?0x20000000u:0x10000000u);
            e.Put<std::int32_t>(e.ActorAddress+0x2c8,int(x*80));e.Write(e.ActorAddress+0x2d0,1);
        };
        flick(direction);e.Run(Sites[0].pc);
        const unsigned pan_state=direction<0?4:5,inner=direction<0?4:0;
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==pan_state);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==inner);
        // Release one frame, then press again before the shelf camera settles.
        e.Put(e.Play+0x3c,0.f);e.Write(e.Play+0x18,0);e.Write(e.ActorAddress+0x2c8,0);e.Write(e.ActorAddress+0x2d0,0);
        e.Run(Sites[0].pc);flick(direction);e.Run(Sites[0].pc);
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==pan_state);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==inner+2 && e.queries==0);
        assert(std::abs(e.Get<float>(e.ActorAddress+0x37c))<30);
        // Reverse immediately, including returning to the keeper mid-pan.
        flick(-direction);e.Run(Sites[0].pc);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==inner);
        flick(-direction);e.Run(Sites[0].pc);
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==8);
        // A settled shelf also accepts movement while its description opens.
        e.Put<std::uint16_t>(e.ActorAddress+0x2a0,pan_state+2);
        flick(direction);e.Run(Sites[0].pc);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==inner+2 && e.queries==0);
        // Real action buttons must still take the native readiness path.
        for(U32 button:{1u,2u}) {
            flick(-direction);e.Write(e.Play+0x18,e.Get<U32>(e.Play+0x18)|button);
            const auto queries=e.queries;e.Run(Sites[0].pc);
            assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==inner+2 && e.queries==queries+1);
        }
        // Holding must keep its native repeat timing while the camera pans;
        // it must neither move on every early frame nor reset the accumulator.
        e.Put<std::uint16_t>(e.ActorAddress+0x2a0,pan_state);
        e.Put<std::uint8_t>(e.ActorAddress+0x2fa,inner);
        e.Put(e.ActorAddress+0x37c,direction*20);
        flick(direction);e.Write(e.Play+0x18,0);e.Write(e.ActorAddress+0x2d0,0);
        e.Put<std::int32_t>(e.ActorAddress+0x2c8,int(direction*400));
        e.Run(Sites[0].pc);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==inner);
        assert(e.Get<std::int32_t>(e.ActorAddress+0x2c8)==int(direction*400));
        e.Put<std::int32_t>(e.ActorAddress+0x2c8,int(direction*560));e.Run(Sites[0].pc);
        assert(e.Get<std::uint8_t>(e.ActorAddress+0x2fa)==inner+2);
    }
    // Releasing the stick always returns to the actual keeper menu.
    for(float direction:{-1.f,1.f}) {
        e.ResetActor();e.Put<std::uint16_t>(e.ActorAddress+0x2a0,8);
        e.Put(e.ActorAddress+0x37c,direction*30);e.Put(e.Play+0x3c,0.f);
        for(unsigned i=0;i<20 && e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==8;++i)e.Run(Sites[0].pc);
        assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==2 && e.Get<float>(e.ActorAddress+0x37c)==0);
        assert(e.messages==1 && e.sounds==0);
    }
    // No interception of purchases/talk/quit, empty shelves or pending buttons.
    e.ResetActor();e.Put(e.ActorAddress+0x37c,8.f);e.Put(e.Play+0x3c,80.f);
    e.Write(e.ActorAddress+0x2c8,800);e.Write(e.ActorAddress+0x2d0,1);
    for(unsigned state=0;state<27;++state) {
        e.Put<std::uint16_t>(e.ActorAddress+0x2a0,state);
        assert(ContinueBrowsing(e,e.ActorAddress,e.Play)==(state==2 || state==8));
    }
    // The center menu's text animation must not create a second lockout.
    e.Put<std::uint16_t>(e.ActorAddress+0x2a0,2);e.Put(e.ActorAddress+0x37c,0.f);
    e.Run(Sites[0].pc);assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==5 && e.queries==0);
    e.Put<std::uint16_t>(e.ActorAddress+0x2a0,2);e.Put(e.Play+0x3c,0.f);
    e.Run(Sites[0].pc);assert(e.Get<std::uint16_t>(e.ActorAddress+0x2a0)==2 && e.queries==1);
    e.Put(e.Play+0x3c,80.f);
    e.Put<std::uint16_t>(e.ActorAddress+0x2a0,8);e.Write(e.Play+0x18,1);
    assert(!ContinueBrowsing(e,e.ActorAddress,e.Play));e.Write(e.Play+0x18,0);
    e.Write(e.ActorAddress+0x2d0,0);e.Put(e.ActorAddress+0x37c,20.f);
    assert(!ContinueBrowsing(e,e.ActorAddress,e.Play)); // First held step still approaches keeper.
    e.Write(e.ActorAddress+0x2d0,1);
    e.Put(e.Play+0x3c,0.f);assert(!ContinueBrowsing(e,e.ActorAddress,e.Play));
    e.Put(e.Play+0x3c,80.f);
    for(unsigned i=0;i<4;++i)e.Write(e.ActorAddress+0x2a4+i*4,0);
    assert(!ContinueBrowsing(e,e.ActorAddress,e.Play));
    // Real draw routine: all eight mirrored/normal paths retain visibility,
    // color, scale and model arguments, and never change the native positions.
    for(bool mirror:{false,true})for(const auto offsets:{
        std::array<std::array<float,2>,2>{{{0,0},{0,0}}},
        std::array<std::array<float,2>,2>{{{-100.5f,-100.5f},{100.25f,100.25f}}},
        std::array<std::array<float,2>,2>{{{400,240},{-400,-240}}}}) {
        e.ResetActor();e.Put<std::uint8_t>(0x587966,mirror);
        for(auto [at,x]:{std::pair{0x30cu,32.f},{0x324u,12.f},{0x344u,368.f},{0x35cu,388.f}}) {
            e.Put(e.ActorAddress+at,x);e.Put(e.ActorAddress+at+4,96.f);
        }
        e.Put(e.ActorAddress+0x32c,1.f);e.Put(e.ActorAddress+0x364,1.f);
        for(U32 at:{0x2fcu,0x300u,0x304u,0x308u,0x314u,0x318u,0x31cu,0x320u,
                   0x334u,0x338u,0x33cu,0x340u,0x34cu,0x350u,0x354u,0x358u})e.Write(e.ActorAddress+at,255);
        e.Write(e.ActorAddress+0x3a0,e.Item);e.Write(e.ActorAddress+0x3a4,e.Item+0x200);
        e.Write(e.ActorAddress+0x330,1);e.Write(e.ActorAddress+0x368,1);
        const auto actor=std::vector(e.bytes.begin()+e.ActorAddress,e.bytes.begin()+e.ActorAddress+0x584);
        OOT3D::HUD::SetLayout({},false);e.patched=false;cpu.ClearCache();
        e.Run(0x35ddb4,true);const auto native=e.draws;
        assert(native.size()==4);e.draws.clear();
        e.patched=true;cpu.ClearCache();
        OOT3D::HUD::Layout layout;layout.shop_prompt_offsets=offsets;
        OOT3D::HUD::SetLayout(layout,false);e.Run(0x35ddb4,true);assert(e.draws.size()==4);
        for(unsigned i=0;i<4;++i) {
            const auto offset=layout.shop_prompt_offsets[(i%2)^unsigned(mirror)];
            for(unsigned edge=0;edge<4;++edge)
                assert(e.draws[i].rect[edge]==native[i].rect[edge]+offset[edge<2?1:0]);
            assert(e.draws[i].rgba==native[i].rgba && e.draws[i].matrix==native[i].matrix &&
                   e.draws[i].model==native[i].model && e.draws[i].depth==native[i].depth);
        }
        assert(std::equal(actor.begin(),actor.end(),e.bytes.begin()+e.ActorAddress));
        const auto moved=e.draws;e.draws.clear();e.Run(0x35ddb4,true);assert(e.draws.size()==4);
        for(unsigned i=0;i<4;++i)assert(e.draws[i].rect==moved[i].rect); // No accumulation.
        e.Put<std::uint16_t>(e.ActorAddress,0x42); // Shared drawing helper, not a shop actor.
        e.draws.clear();e.Run(0x35ddb4,true);assert(e.draws.size()==4);
        for(unsigned i=0;i<4;++i)assert(e.draws[i].rect==native[i].rect);
        e.Put<std::uint16_t>(e.ActorAddress,0x3d);
        e.Write(e.ActorAddress+0x330,0);e.Write(e.ActorAddress+0x368,0);
        e.draws.clear();e.Run(0x35ddb4,true);assert(e.draws.empty());
    }
    std::cout<<"Native shop checks passed: same-update cursor selection on both shelves/all eight shop draws, 27-action dispatch, release/re-press with HID direction flags, navigation during/after shelf pans, held repeat, action-button guards, ABI, negative/fractional/extreme prompt rectangles and normal/MQ isolation\n";
}
