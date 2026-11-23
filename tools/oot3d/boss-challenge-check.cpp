// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/boss_challenge.h"
#include "oot3d/death_menu.h"
#include "oot3d/pause_input.h"
#include "oot3d/control_swap.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
namespace B=OOT3D::BossChallenge;
using B::U32;
struct Env : Dynarmic::A32::UserCallbacks {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    B::Hook hook;
    OOT3D::DeathMenu::Hook death_hook;
    bool patched=true;
    std::uint64_t remaining=0;
    static constexpr U32 Stop=0x900000;
    template<class T> T Get(U32 a) {if(a+sizeof(T)>bytes.size()) {std::cerr<<"invalid read "<<std::hex<<a<<" pc "<<(cpu?cpu->Regs()[15]:0)<<"\n";std::abort();}T v;std::memcpy(&v,bytes.data()+a,sizeof(v));return v;}
    template<class T> void Put(U32 a,T v) {assert(a+sizeof(T)<=bytes.size());std::memcpy(bytes.data()+a,&v,sizeof(v));}
    std::optional<U32> Read(U32 a) {return Get<U32>(a);}
    void Write(U32 a,U32 v) {Put(a,v);}
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(a==Stop)return 0xef000000;
        // Run the real selection/state/branch code; mock only rendering,
        // sound, control animation and the deterministic RNG result.
        for(U32 call:{0x307840u,0x2fd274u,0x2fd360u,0x2f2f74u,0x344670u,
                     0x2f87ecu,0x2f4f64u,0x2e7248u,0x37547cu,0x3759d0u,0x339384u,0x3655d0u})
            if(a==call)return 0xef000001;
        const auto word=Get<U32>(a);
        if(patched && OOT3D::DeathMenu::Instruction(a,word)!=word)return OOT3D::DeathMenu::Instruction(a,word);
        return patched?B::Instruction(a,word):word;
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
        auto& r=cpu->Regs();
        if(svc==1) {
            const auto pc=r[15]-4;
            if(pc==0x3759d0)cpu->ExtRegs()[0]=std::bit_cast<U32>(.5f);
            if(pc==0x339384)r[1]=3;
            r[15]=r[14];return;
        }
        if(svc>=OOT3D::DeathMenu::FirstSVC+7 && svc<OOT3D::DeathMenu::FirstSVC+OOT3D::DeathMenu::Sites.size()) {
            death_hook.Input(*this,r,svc-OOT3D::DeathMenu::FirstSVC,true);return;
        }
        assert(svc>=B::FirstSVC && svc<B::FirstSVC+B::Sites.size());
        hook.Input(*this,r,svc-B::FirstSVC,true);
    }
    void Select(U32 index) {
        cpu->ClearCache();auto& r=cpu->Regs();r[0]=B::Owner;r[1]=index;
        r[13]=0x980000;r[14]=Stop;r[15]=0x429600;remaining=100000;
        cpu->SetCpsr(0x10);cpu->Run();
        assert(r[15]==Stop+4 && r[13]==0x980000 && !hook.selection_return);
    }
    void Init() {
        std::fill(bytes.begin()+B::Owner,bytes.begin()+B::Owner+0x1940,0);
        Write(B::Owner+8,0x10103);Write(B::NativeLayout+4,0x880000);
        Write(B::Owner+0x3e8,0xffffffff);Write(B::Owner+0x3ec,0xffffffff);
        Write(B::Owner+0x18ac,9);
        for(unsigned screen=0;screen<2;++screen)for(unsigned i=0;i<256;++i) {
            const U32 pane=0x800000+(screen*256+i)*0x100;
            Write(B::NativeLayout+0x418+screen*0x400+i*4,pane);
            Write(pane+4,B::NativeLayout+0x18);
        }
        for(unsigned i=0;i<12;++i) {
            const U32 control=B::Owner+0x1758+i*0x1c;
            Write(control+4,B::Owner);Write(control+8,i);
            Write(control+0xc,i<9?10+5*i:i==9?55:i==10?60:65);
        }
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Env e;std::ifstream f(argv[1],std::ios::binary);assert(f);
    f.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),e.bytes.size()-0x100000);
    assert(B::CodeMatches(e));
    for(const auto& site:B::Sites) {
        e.Write(site.pc,site.instruction^1);assert(!B::CodeMatches(e));e.Write(site.pc,site.instruction);
        assert((B::Instruction(site.pc,site.instruction)&0xf0000000)==(site.instruction&0xf0000000));
    }
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu{config};e.cpu=&cpu;
    for(U32 boss=0;boss<9;++boss) {
        e.Init();e.patched=false;e.Select(boss);assert(e.Get<std::uint8_t>(B::Owner+8)==5);
        e.Select(10);assert(e.Get<std::uint8_t>(B::Owner+8)==8);
        const std::vector<unsigned char> expected(e.bytes.begin()+B::Owner,e.bytes.begin()+B::Owner+0x1940);
        e.Init();e.patched=true;e.Select(boss);assert(e.Get<std::uint8_t>(B::Owner+8)==8);
        assert(std::equal(expected.begin(),expected.end(),e.bytes.begin()+B::Owner));
        assert(e.Read(B::Owner+0x3ec)==(boss==8?0:boss));
    }
    e.Init();e.Select(9);assert(e.Get<std::uint8_t>(B::Owner+8)==4); // B/Quit unchanged.
    e.Init();e.Write(B::Owner+0x3e8,2);e.Select(11);
    assert(e.Get<std::uint8_t>(B::Owner+8)==6); // Native Cancel remains Cancel.
    for(unsigned arrangement=0;arrangement<3;++arrangement) {
        const bool two=arrangement==1,horizontal=arrangement==2;
        B::Layout layout;layout.two_columns=two;layout.horizontal=horizontal;
        layout[B::Stones]={-20,10,.4f};layout.spacing_x=9;layout.spacing_y=3;
        B::SetLayout(layout,false);
        {std::scoped_lock lock{B::mutex};B::touch_layout=layout;}
        for(unsigned i=0;i<9;++i) {
            const auto n=B::NativeStone(i),c=B::StoneCenter(i,layout);
            if(horizontal) {
                assert(c[1]==120+layout[B::Stones].y);
                if(i)assert(c[0]>B::StoneCenter(i-1,layout)[0]);
                assert(B::Neighbor(i,2,two,true)==0xffffffff && B::Neighbor(i,3,two,true)==0xffffffff);
                assert(B::Neighbor(i,0,two,true)==((i+1)%9));
                assert(B::Neighbor(i,1,two,true)==((i+8)%9));
            }
            const auto t=B::StoneTransform(i,layout);
            assert(std::abs(n[0]+t.x-(c[0]+40))<1e-5 && std::abs(n[1]+t.y-c[1])<1e-5);
            for(float width:{400.f,533.333f}) {
                const auto touch=B::MapPointer((c[0]-160)/width+.5f,c[1]/240,width);assert(touch);
                assert(std::abs((*touch)[0]*320-n[0])<1e-3 && std::abs((*touch)[1]*240-n[1])<1e-3);
            }
            const U32 down=B::Neighbor(i,3,two,horizontal),right=B::Neighbor(i,0,two,horizontal);
            if(down<9)assert(B::Neighbor(down,2,two,horizontal)==i);
            if(right<9)assert(B::Neighbor(right,1,two,horizontal)==i);
            for(unsigned direction=0;direction<4;++direction) {
                U32 current=i;
                for(unsigned count=0;current<9;++count) {
                    assert(count<9);current=B::Neighbor(current,direction,two,horizontal);
                    if(direction<2 && current==i) {assert(count==8);break;}
                }
            }
        }
    }
    e.Init();B::Hook hook;std::array<U32,16> r{};r[0]=B::Owner;r[14]=0x123456;
    const U32 background=*e.Read(B::NativeLayout+0x818+4);
    e.Write(background+0x64,0x870000);e.Write(background+0xb0,3);
    e.Write(0x870000,0x4ebd60);
    e.Write(0x870000+0x1dc,0x873000); // A ready donor owns a valid material.
    e.Write(background+0x6c,0x01010000); // Native background is hidden.
    OOT3D::FileBackground::pixels.store(std::shared_ptr<const OOT3D::FileBackground::Pixels>(
            std::make_shared<OOT3D::FileBackground::Pixels>()));
    // Full 320x240 artwork, not the challenge pane's 160x120 fragment.
    auto panel_layout=B::GetLayout();panel_layout[B::Background]={17,-9,.5f};
    panel_layout.background_darkness={.25f,.75f};
    const auto panel=B::BrownBackground(panel_layout);
    assert(panel.count==2);
    const auto full=B::BrownBackground(B::GetLayout());
    assert(full.count==2);
    for(unsigned i=0;i<4;++i) {
        assert(panel.positions[i*3]>=137 && panel.positions[i*3]<=297);
        assert(panel.positions[i*3+1]>=51 && panel.positions[i*3+1]<=171);
        assert(panel.uvs[i*2]==.5f/512 || panel.uvs[i*2]==319.5f/512);
        assert(panel.uvs[i*2+1]==255.5f/256 || panel.uvs[i*2+1]==16.5f/256);
        assert(panel.colors[i*4]==.75f && panel.colors[16+i*4]==.25f);
        assert(panel.colors[i*4+3]==1 && panel.colors[16+i*4+3]==1);
        for(unsigned axis=0;axis<3;++axis)
            assert(panel.positions[12+i*3+axis]==full.positions[12+i*3+axis]);
    }
    panel_layout[B::Preview]={20,-10,.5f};panel_layout.shadow_opacity=.6f;panel_layout.shadow_softness=10;
    const auto shadow=B::PreviewShadow(panel_layout);
    assert(shadow.count==9);
    // The inner shadow meets the posed preview exactly, and fades to zero
    // five top-screen pixels outside it (ten native pixels at half scale).
    assert(shadow.positions[0]==115 && shadow.positions[3]==120);
    assert(shadow.colors[3]==0 && shadow.colors[7]==.6f);
    assert(shadow.positions[8*12]==120 && shadow.positions[8*12+1]==50);
    assert(shadow.positions[8*12+9]==320 && shadow.positions[8*12+10]==180);
    panel_layout.border_bottom=false;assert(B::PreviewShadow(panel_layout).positions[8*12+10]==170);
    panel_layout.shadow_opacity=0;assert(B::PreviewShadow(panel_layout).count==0);
    // Use a native single-sprite donor, not the all-zero descriptor used by
    // older fixtures. Independent panels/shadow quads must never inherit its
    // triangle strip topology or disabled vertex color/UV streams.
    const auto check_mesh=[&](auto& mesh_hook,const auto& geometry) {
        using H=std::remove_reference_t<decltype(mesh_hook)>;
        constexpr U32 source=0x870000,mesh=0x871000,desc=0x872000,block=0x880000;
        e.Write(source+4,mesh);e.Write(mesh,desc);e.Write(desc+0x1c,0x98);
        mesh_hook.source=source;mesh_hook.block=block;mesh_hook.geometry=geometry;
        assert(mesh_hook.Prepare(e));
        assert(e.Read(block+H::Descriptor+0x1c)==0x1a0);
        assert(e.Read(desc+0x1c)==0x98); // Donor remains untouched.
        for(unsigned q=0;q<H::Quads;++q) {
            const auto index=[&](unsigned i) {
                return (*e.Read(block+H::Indices+q*12+(i/2)*4)>>(16*(i%2)))&65535;
            };
            for(unsigned i=0;i<6;++i)assert(index(i)/4==q);
            for(unsigned triangle=0;triangle<2;++triangle) {
                const auto a=index(triangle*3),b=index(triangle*3+1),c=index(triangle*3+2);
                const auto& v=geometry.positions;
                const float area=(v[b*3]-v[a*3])*(v[c*3+1]-v[a*3+1])-
                    (v[b*3+1]-v[a*3+1])*(v[c*3]-v[a*3]);
                assert(area<0);
            }
        }
    };
    OOT3D::GuestUI::Hook<B::BackgroundGeometry,OOT3D::FileBackground::Width,OOT3D::FileBackground::Height,OOT3D::FileBackground::Format> background_mesh;check_mesh(background_mesh,panel);
    decltype(hook.shadow) shadow_mesh;check_mesh(shadow_mesh,shadow);
    auto moved_layout=panel_layout;moved_layout[B::Background2]={-130,75,.8f};
    const auto moved=B::BrownBackground(moved_layout);
    check_mesh(background_mesh,moved);
    assert(std::equal(panel.positions.begin(),panel.positions.begin()+12,moved.positions.begin()));
    assert(std::equal(panel.uvs.begin(),panel.uvs.begin()+8,moved.uvs.begin()));
    const U32 native_preview=*e.Read(B::NativeLayout+0x418+4);
    e.Write(native_preview+0x6c,0x100);e.Write(native_preview+0xb0,3);
    const auto before=e.bytes;
    bool saw_background=false,saw_shadow=false,saw_timer=false,saw_fade=false;
    do {
        hook.Top(e,r,true);
        if(hook.stage==B::Hook::Stage::Background) {
            assert(!saw_background && !saw_timer && !saw_fade);
            saw_background=true;
            assert(hook.background.Pending() && hook.background.geometry.positions==OOT3D::FileSelect::Backgrounds(OOT3D::FileSelect::GetLayout()).positions);
            assert(e.Read(background+0x6c)==0x01010000);
            const auto t=hook.ModelTransform();
            assert(t.x==0 && t.y==0 && t.sx==1 && t.sy==1);
            // A failed guest allocation must still allow every native control
            // and the final fade to render, without changing pane visibility.
            r[0]=0;
        } else assert(e.Read(background+0x6c)==0x01010000);
        if(hook.stage==B::Hook::Stage::Vignette || hook.stage==B::Hook::Stage::Particles)r[0]=0;
        if(hook.stage==B::Hook::Stage::Shadow) {
            assert(saw_background && !saw_shadow && !saw_timer && !saw_fade);
            saw_shadow=true;assert(hook.shadow.Pending());
            assert(hook.shadow.geometry.positions==B::PreviewShadow(B::GetLayout()).positions);
            r[0]=0; // Failure must also leave native draw order and state intact.
        }
        if(hook.stage==B::Hook::Stage::Native && hook.pane==210) {
            saw_timer=true;assert(hook.Drawing() && hook.ModelTransform().y==B::GetLayout()[B::Timer].y);
        }
        if(hook.stage==B::Hook::Stage::Fade) {
            saw_fade=true;assert(hook.pane==250 && hook.ModelTransform().sx==1 && hook.submitted>0);
        }
    } while(hook.stage!=B::Hook::Stage::Idle);
    assert(saw_background && saw_shadow && saw_timer && saw_fade);
    assert(before==e.bytes && B::on_top && r[15]==B::Sites[0].pc+4);
    r[5]=B::NativeLayout;r[4]=10;r[0]=*e.Read(B::NativeLayout+0x818+40);
    assert(hook.SkipBottom(e,r));r[4]=55;r[0]=*e.Read(B::NativeLayout+0x818+220);
    assert(!hook.SkipBottom(e,r)); // B is never routed or suppressed.
    for(unsigned i=0;i<256;++i)if(B::RoutedPane(i))assert(i!=1 && i!=250 && (i<55 || i>58));
    e.Write(B::Owner+0x3ec,0);e.Write(native_preview+0x6c,0);assert(B::InBattle(e));
    r={};r[0]=B::Owner;
    do {
        hook.Top(e,r,true);
        assert(hook.stage!=B::Hook::Stage::Background && hook.stage!=B::Hook::Stage::Shadow);
    } while(hook.stage!=B::Hook::Stage::Idle);
    assert(!B::on_top && e.Read(background+0x6c)==0x01010000);
    {
        B::Hook tint;tint.active=true;tint.presenting=true;tint.stage=B::Hook::Stage::Native;tint.pane=1;
        tint.frame_layout.panel_darkness=.5f;constexpr U32 rgb=0x880000;
        for(unsigned c=0;c<4;++c)e.Write(rgb+4*c,std::bit_cast<U32>(.8f));
        tint.PreviewMaterial(e,rgb);
        for(unsigned c=0;c<3;++c)assert(std::bit_cast<float>(*e.Read(rgb+4*c))==.4f);
        assert(std::bit_cast<float>(*e.Read(rgb+12))==.8f);
        for(unsigned pane:{7u,210u,250u}) {tint.pane=pane;tint.PreviewMaterial(e,rgb);}
        tint.pane=1;tint.battle=true;tint.presenting=false;tint.PreviewMaterial(e,rgb);
        assert(std::bit_cast<float>(*e.Read(rgb))==.4f);
        tint.battle=false;tint.presenting=true;tint.stage=B::Hook::Stage::Border;
        tint.border.stage=decltype(tint.border)::Stage::Draw;
        tint.frame_layout.border_darkness=.5f;
        tint.PreviewMaterial(e,rgb);
        for(unsigned c=0;c<3;++c)assert(std::bit_cast<float>(*e.Read(rgb+4*c))==.25f);
        assert(std::bit_cast<float>(*e.Read(rgb+12))==.8f);

    }
    // Real Challenge Failed layout: top pane 7, centered at 200,102.
    {
        B::Hook failed;failed.stage=B::Hook::Stage::Native;failed.pane=7;
        failed.frame_layout[B::FailedTitle]={17,-9,.5f};
        const auto t=failed.ModelTransform();
        assert(t.x==17 && t.y==-9 && t.sx==.5f && t.sy==.5f && t.ax==200 && t.ay==102);
        constexpr U32 p=0x880000,model=0x881000,mesh=0x882000;
        e.Write(p+0x64,model);e.Write(p+0x68,0);e.Write(model,0x4ebd60);e.Write(model+4,mesh);
        e.Write(mesh+0x13c,0);e.Write(mesh+0x140,0);
        for(unsigned pane:{6u,7u,8u,12u}) {
            failed.pane=pane;failed.FilterResultTitle(e,p);assert(e.Read(mesh+0x13c)==1 && e.Read(mesh+0x140)==3);
            e.Write(mesh+0x13c,0);failed.FilterTitleAtDraw(e,model);assert(e.Read(mesh+0x13c)==1);
            failed.RestoreTitleFilter(e);assert(e.Read(mesh+0x13c)==0 && e.Read(mesh+0x140)==0);
        }
        failed.pane=6;failed.battle=true;assert(failed.ModelTransform().sx==1);
    }
    // Filter the renderer's effective sampler before command submission,
    // including native code rebinding a different mesh from the pane's model.
    {
        B::Hook draw;draw.active=true;draw.stage=B::Hook::Stage::Native;
        constexpr U32 p=0x880000,model=0x881000,shadow=0x881400,mesh=0x882000;
        constexpr U32 renderer=0x883000,alpha=0x884000;
        e.Write(renderer+0x354,model);e.Write(renderer+0x350,mesh);
        e.Write(p+0x64,model);e.Write(p+0x68,shadow);
        for(unsigned pane:{6u,7u,8u,12u,210u,217u,220u}) {
            draw.pane=pane;draw.timer_opacity=.35f;
            e.Write(B::NativeLayout+0x418+4*pane,p);
            e.Write(mesh+0x13c,0);e.Write(mesh+0x140,0);
            e.Write(alpha,std::bit_cast<U32>(.8f));
            draw.DrawMaterial(e,renderer,alpha);
            assert(e.Read(mesh+0x13c)==1 && e.Read(mesh+0x140)==3);
            assert(std::bit_cast<float>(*e.Read(alpha))==(pane>=210 && pane<=217?.8f*.35f:.8f));
            draw.RestoreTitleFilter(e);
            assert(e.Read(mesh+0x13c)==0 && e.Read(mesh+0x140)==0);
        }
        draw.pane=210;
        e.Write(alpha,std::bit_cast<U32>(0.f));draw.DrawMaterial(e,renderer,alpha);
        assert(std::bit_cast<float>(*e.Read(alpha))==0); // Hidden native digit stays hidden.
        draw.timer_opacity=0;
        draw.DrawMaterial(e,renderer,alpha);assert(std::bit_cast<float>(*e.Read(alpha))==0);
        draw.RestoreTitleFilter(e);
        draw.frame_layout[B::Timer]={30,150,.8f};
        const std::array<float,12> identity{1,0,0,14,0,1,0,18,0,0,1,0};
        for(unsigned i=0;i<12;++i)e.Write(model+0xc+4*i,std::bit_cast<U32>(identity[i]));
        e.Write(p+0x80,std::bit_cast<U32>(14.f));e.Write(p+0x84,std::bit_cast<U32>(18.f));
        for(float y:{-30.f,0.f,18.f,40.f}) {
            e.Write(model+0x28,std::bit_cast<U32>(y));
            const auto t=draw.DrawTransform(e,model),sh=draw.DrawTransform(e,shadow);
            const float rendered=y*t.sy+t.y+t.ay*(1-t.sy);
            assert(std::abs(rendered-168.f)<.001f);
            assert(t.y==sh.y); // Preserve shadow displacement.
        }
        // Shared HD timer pose composes after boss placement/slide removal.
        auto hd=OOT3D::HUD::GetLayout();hd.hd_timer_text={3,4,1.5f};hd.hd_timer_spacing=2;
        OOT3D::HUD::SetLayout(hd,false);
        auto ht=draw.DrawTransform(e,model);
        assert(std::abs(14*ht.sx+ht.x+ht.ax*(1-ht.sx)-47)<.001f);
        assert(std::abs(40*ht.sy+ht.y+ht.ay*(1-ht.sy)-172)<.001f);
        hd.hd_numbers=false;OOT3D::HUD::SetLayout(hd,false);
        ht=draw.DrawTransform(e,model);
        assert(std::abs(40*ht.sy+ht.y+ht.ay*(1-ht.sy)-168)<.001f);
        hd.hd_numbers=true;OOT3D::HUD::SetLayout(hd,false);draw.results=true;
        ht=draw.DrawTransform(e,model);
        assert(std::abs(14*ht.sx+ht.x-17)<.001f && std::abs(40*ht.sy+ht.y-44)<.001f);
        draw.results=false;OOT3D::HUD::SetLayout({},false);
        draw.pane=15;e.Write(mesh+0x13c,0);e.Write(alpha,std::bit_cast<U32>(.8f));
        draw.DrawMaterial(e,renderer,alpha);
        assert(e.Read(mesh+0x13c)==0 && std::bit_cast<float>(*e.Read(alpha))==.8f);
        // Entering battle must not reset a still-visible preview to full size.
        draw.pane=1;draw.battle=true;draw.frame_layout[B::Preview]={-20,5,.6f};
        assert(draw.ModelTransform().sx==.6f && draw.ModelTransform().x==-20);
    }
    {
        // Warp requests precede pooled fade covers and can leave HUD alpha1.
        namespace H=OOT3D::HUDFade;constexpr U32 play=0xa00000;
        const auto saved=e.bytes;
        e.Write(H::UI+0xc,play);e.Write(play+0x100,0x0203);
        e.Write(play+0x7f24,0x100);e.Write(H::Main+0x34,2);
        e.Write(play+0x5c2c,0);e.Write(play+0x7f10,0);
        e.Write(B::Owner+0xc,1);assert(B::Hook::GameplayTimerOpacity(e,true,1)==0);
        e.Write(B::Owner+0xc,0x101);assert(B::Hook::GameplayTimerOpacity(e,true,.6f)==.6f);
        assert(B::Hook::GameplayTimerOpacity(e,false,1)==0);
        e.Write(play+0x5c2c,0x100);assert(B::Hook::GameplayTimerOpacity(e,true,1)==0);
        e.Write(play+0x5c2c,0);e.Write(play+0x7f10,0x10000);
        assert(B::Hook::GameplayTimerOpacity(e,true,1)==0);
        e.bytes=saved;
    }
    {
        B::Hook result;result.active=true;result.stage=B::Hook::Stage::Native;
        constexpr U32 pane=0x885000,model=0x885100,renderer=0x885400,alpha=0x885800;
        e.Write(B::NativeLayout+0x418+6*4,pane);e.Write(B::NativeLayout+0x418+8*4,0);
        e.Write(pane+4,B::NativeLayout+0x18);e.Write(pane+0xb0,3);
        e.Write(pane+0x6c,0x100);
        for(unsigned state:{0u,8u,9u}) {
            e.Write(B::Owner+8,0x10100|state);assert(!B::Hook::ResultsVisible(e));
        }
        e.Write(B::Owner+8,0x1010a);assert(B::Hook::ResultsVisible(e));
        e.Write(pane+0x6c,1);assert(!B::Hook::ResultsVisible(e)); // Requested, not drawn yet.
        e.Write(pane+0x6c,0x100);result.results=B::Hook::ResultsVisible(e);
        result.frame_layout[B::Timer]={180,210,2};result.timer_opacity=0;
        result.pane=210;e.Write(B::NativeLayout+0x418+210*4,pane);
        e.Write(pane+0x64,model);e.Write(renderer+0x354,model);e.Write(renderer+0x350,0);
        constexpr U32 label=0x887000,label_model=0x887400;
        e.Write(B::NativeLayout+0x418+220*4,label);e.Write(label+4,B::NativeLayout+0x18);
        e.Write(label+0xb0,3);e.Write(label+0x6c,0x100);e.Write(label+0x78,256);
        e.Write(label+0x64,label_model);e.Write(label_model+0xfc,std::bit_cast<U32>(1.f));
        // Finished title layers must not send result digits back to HUD mode.
        e.Write(pane+0x6c,0);
        assert(B::Hook::ResultsVisible(e));
        for(unsigned state:{8u,9u}) {
            e.Write(B::Owner+8,0x10100|state);assert(!B::Hook::ResultsVisible(e));
        }
        e.Write(B::Owner+8,0x1010a);e.Write(pane+0x6c,0x100);
        e.Write(pane+0xc,0x886000);
        e.Write(label+0x84,std::bit_cast<U32>(146.f));
        e.Write(model+0x28,std::bit_cast<U32>(146.f));
        for(float frame:{-1.f,0.f}) {
            e.Write(pane+0x18,std::bit_cast<U32>(frame));e.Write(alpha,std::bit_cast<U32>(1.f));
            result.DrawMaterial(e,renderer,alpha);assert(std::bit_cast<float>(*e.Read(alpha))==0);
        }
        e.Write(pane+0x18,std::bit_cast<U32>(1.f));
        // The first sampled result draw was fully opaque at the old anchor.
        for(U32 anim:{0u,0x886000u}) {
            e.Write(pane+0xc,anim);e.Write(model+0x28,std::bit_cast<U32>(18.f));
            e.Write(alpha,std::bit_cast<U32>(1.f));result.DrawMaterial(e,renderer,alpha);
            assert(std::bit_cast<float>(*e.Read(alpha))==0);
        }
        e.Write(model+0x28,std::bit_cast<U32>(146.f));
        e.Write(alpha,std::bit_cast<U32>(.65f));result.DrawMaterial(e,renderer,alpha);
        assert(std::bit_cast<float>(*e.Read(alpha))==.65f); // HUD hidden: result still visible.
        for(float label_alpha:{0.f,.25f,1.f}) {
            e.Write(label_model+0xfc,std::bit_cast<U32>(label_alpha));
            e.Write(alpha,std::bit_cast<U32>(.65f));result.DrawMaterial(e,renderer,alpha);
            assert(std::bit_cast<float>(*e.Read(alpha))==.65f);
        }
        // The digits still retain their own exit fade / hidden native state.
        for(float digit_alpha:{0.f,.25f,1.f}) {
            e.Write(alpha,std::bit_cast<U32>(digit_alpha));result.DrawMaterial(e,renderer,alpha);
            assert(std::bit_cast<float>(*e.Read(alpha))==digit_alpha);
        }
        e.Write(label+0x6c,0);e.Write(alpha,std::bit_cast<U32>(1.f));
        result.DrawMaterial(e,renderer,alpha);assert(std::bit_cast<float>(*e.Read(alpha))==0);
        e.Write(label+0x6c,0x100);

        const auto native=result.DrawTransform(e,model);
        assert(native.x==0 && native.y==0 && native.sx==1 && native.sy==1);
        result.frame_layout[B::ClearedTitle]={12,-9,.7f};
        for(unsigned layer:{6u,8u}) {
            result.pane=layer;const auto t=result.ModelTransform();
            assert(t.x==12 && t.y==-9 && t.sx==.7f && t.ax==200 && t.ay==102);
        }
        result.frame_layout[B::NewRecord]={-8,10,1.5f};result.pane=12;
        const auto stamp=result.ModelTransform();
        assert(stamp.x==-8 && stamp.y==10 && stamp.sx==1.5f && stamp.ax==284 && stamp.ay==156);
    }
    // Selection exit keeps the last menu pose/visibility and restores native
    // state after each draw, without changing input or gameplay state.
    {
        B::Hook exit;exit.active=exit.presenting=true;exit.stage=B::Hook::Stage::Native;exit.pane=20;
        constexpr U32 p=0x886000,model=0x887000,alpha=0x888000;
        e.Write(p+0x6c,0x101);e.Write(p+0xb0,5);exit.SelectionPane(e,p,20);
        for(unsigned i=0;i<12;++i)e.Write(model+0xc+i*4,std::bit_cast<U32>(float(i)));
        e.Write(model+0xfc,std::bit_cast<U32>(.75f));exit.SelectionModel(e,model);
        exit.battle=true;e.Write(p+0x6c,0);e.Write(p+0xb0,2);
        e.Write(model+0x28,std::bit_cast<U32>(99.f));e.Write(model+0xfc,0);
        exit.SelectionPane(e,p,20);exit.SelectionModel(e,model);exit.SelectionAlpha(e,model,alpha);
        assert(e.Read(p+0x6c)==0x101 && e.Read(p+0xb0)==5);
        assert(std::bit_cast<float>(*e.Read(model+0x28))==7.f);
        assert(std::bit_cast<float>(*e.Read(alpha))==.75f);
        assert(exit.ModelTransform().sx==exit.frame_layout[B::Preview].scale);
        exit.RestoreSelection(e);
        assert(e.Read(p+0x6c)==0 && e.Read(p+0xb0)==2 && e.Read(model+0xfc)==0);
        assert(std::bit_cast<float>(*e.Read(model+0x28))==99.f);
        exit.stage=B::Hook::Stage::Fade;e.Write(alpha,std::bit_cast<U32>(.4f));
        exit.SelectionAlpha(e,model,alpha);assert(std::bit_cast<float>(*e.Read(alpha))==.4f);
    }
    {
        B::Hook prompt;prompt.active=prompt.presenting=true;prompt.stage=B::Hook::Stage::Panes;
        prompt.pane=3;prompt.prompt_alpha=.45f;
        constexpr U32 p=0x889000,model=0x889100,shadow=0x889400,alpha=0x889800;
        prompt.routed[3]=p;e.Write(p+0x64,model);e.Write(p+0x68,shadow);
        const std::array<float,12> growing{.2f,0,0,160,0,.3f,0,10,0,0,1,0};
        for(unsigned i=0;i<12;++i) {
            e.Write(model+0xc+i*4,std::bit_cast<U32>(growing[i]));
            e.Write(shadow+0xc+i*4,std::bit_cast<U32>(growing[i]+(i==3 || i==7?1.f:0.f)));
        }
        e.Write(model+0xfc,std::bit_cast<U32>(1.f));e.Write(shadow+0xfc,std::bit_cast<U32>(.5f));
        prompt.PromptModel(e,model);prompt.PromptModel(e,shadow);
        assert(std::bit_cast<float>(*e.Read(model+0xc))==1);
        assert(std::bit_cast<float>(*e.Read(model+0x20))==1);
        assert(std::bit_cast<float>(*e.Read(model+0x28))==24);
        assert(std::bit_cast<float>(*e.Read(shadow+0x28))==25);
        prompt.SelectionAlpha(e,model,alpha);assert(std::bit_cast<float>(*e.Read(alpha))==.45f);
        prompt.pane=100;prompt.SelectionAlpha(e,model,alpha);
        assert(std::bit_cast<float>(*e.Read(alpha))==.45f); // Text and box match.
        prompt.RestoreSelection(e);
        assert(std::bit_cast<float>(*e.Read(model+0xc))==.2f);
        assert(std::bit_cast<float>(*e.Read(model+0x28))==10);
        namespace W=OOT3D::Widescreen;
        W::enabled=true;W::supported_layout=true;prompt.stage=B::Hook::Stage::Fade;prompt.pane=250;
        const auto fade=prompt.ModelTransform();
        const float left=fade.ax*(1-fade.sx),right=400*fade.sx+left;
        assert(std::abs((right-left)-240*W::Aspect)<.001f && left<0 && right>400);
        prompt.stage=B::Hook::Stage::Native;assert(prompt.ModelTransform().sx==fade.sx);
        W::enabled=false;assert(prompt.ModelTransform().sx==1);
        W::supported_layout=false;
        // A mode reset with a hidden native preview cannot revive custom UI.
        e.Write(p+4,B::NativeLayout+0x18);e.Write(p+0xb0,3);e.Write(p+0x6c,0);
        assert(!B::Hook::VisiblePane(e,p));e.Write(p+0x6c,0x100);assert(B::Hook::VisiblePane(e,p));
    }
    // Quit/Continue uses the native Game Over controller's separate states.
    // Its poses must not leak into the ordinary death/save menu.
    namespace D=OOT3D::DeathMenu;
    constexpr U32 death=0x890000,play=0x891800;
    e.Write(D::ControllerPointer,death);e.Write(death+D::LayoutOffset+4,0x890100);
    for(U32 state:{4u,10u})for(U32 choice:{0u,1u,3u}) {
        if(state==4 && choice==3)continue;
        e.Write(death+8,0x10000|state);e.Write(death+0x24,0);
        for(unsigned i=0;i<4;++i) {
            const U32 control=death+0x1120+i*0x1c;
            e.Write(control+4,death);e.Write(control+8,i);e.Write(control+0xc,25+i*5);
            e.Write(control+0x14,0x100048e);e.Write(control+0x18,0x10100);
        }
        e.Write(death+0xc60,0x892000);e.Write(death+0xee0,0x892100);
        r={};r[0]=death+0x1120+choice*0x1c;r[13]=0x980000;r[14]=Env::Stop;
        r[15]=state==4?0x427000:0x427414;
        for(unsigned i=4;i<12;++i)r[i]=0xcafe0000+i;
        cpu.ClearCache();cpu.Regs()=r;cpu.SetCpsr(0x10);e.remaining=100000;cpu.Run();r=cpu.Regs();
        assert(r[15]==Env::Stop+4 && r[13]==0x980000 && e.death_hook.input_stage==0);
        assert((e.Read(death+8).value()&255)==(state==4?5:11));
        assert(e.Read(death+0x24)==0 && e.Read(death+0x20)==0xffffffff); // No pending touch.
        for(unsigned i=4;i<12;++i)assert(r[i]==0xcafe0000+i);
    }

    // B on Game Over's save question chooses Don't Save immediately.
    e.Write(death+8,0x10104);e.Write(death+4,play);e.Write(death+0x10,0);
    e.Write(death+0x14,0);e.Write(death+0x24,0);e.Write(play+0x18,2);
    for(unsigned i=0;i<4;++i)e.Write(death+0x1120+i*0x1c+0x18,0x10100);
    r={};r[0]=death;r[13]=0x980000;r[14]=Env::Stop;r[15]=0x426e48;
    cpu.ClearCache();cpu.Regs()=r;cpu.SetCpsr(0x10);e.remaining=100000;cpu.Run();r=cpu.Regs();
    assert(r[15]==Env::Stop+4 && r[13]==0x980000 && (e.Read(death+8).value()&255)==5);
    assert(e.Read(death+0x10)==1 && e.Read(death+0x24)==0);
    // A one-frame direction on either half of the render pair must reach
    // each native stone handler, including immediate reversals.
    e.Write(death+4,play);e.Write(death+0x10,1);
    e.Write(death+0x14,0);e.Write(death+0x24,0);D::half_frame=true;
    struct Dialog {U32 state,handler;};
    for(const auto dialog:{Dialog{4,0x426e48},Dialog{10,0x4272fc},Dialog{14,0x427424}})
    for(bool first_extra:{false,true}) {
        e.Write(death+8,0x10100|dialog.state);
        D::extra_frame=first_extra;
        for(U32 pressed:{0x10u,0x20u,0x10000000u,0x20000000u}) {
            e.Write(play+0x18,pressed);e.Write(play+0x14,pressed);
            r={};r[1]=play;r[4]=death;r[15]=D::Sites[6].pc+4;
            e.death_hook.ControlUpdate(e,r);assert(r[15]==D::Sites[6].pc+4);
            r[0]=death;r[13]=0x980000;r[14]=Env::Stop;r[15]=dialog.handler;
            cpu.ClearCache();cpu.Regs()=r;cpu.SetCpsr(0x10);e.remaining=100000;cpu.Run();
            assert(cpu.Regs()[15]==Env::Stop+4 && e.Read(death+8)==(0x10100|dialog.state));
            assert(e.Read(death+0x10)==((pressed==0x10 || pressed==0x10000000)?0u:1u));
            D::extra_frame=!D::extra_frame;
        }
    }
    D::extra_frame=true;
    for(U32 pending:{0u,3u}) {
        e.Write(play+0x18,pending?0x10:0);e.Write(play+0x14,0x10);
        e.Write(death+0x24,pending);
        r={};r[1]=play;r[4]=death;r[15]=D::Sites[6].pc+4;
        e.death_hook.ControlUpdate(e,r);assert(r[15]==0x41ce0c && e.Read(death+0x24)==pending);
    }
    D::extra_frame=D::half_frame=false;
    // Run the complete Boss Challenge input handler: A on either choice,
    // B resumes, and logical Start leaves the dialog and focus untouched.
    for(U32 button:{1u,2u,8u})for(U32 choice:{0u,1u}) {
        e.Write(death+0x20,0x1234);
        e.Write(death+8,0x1010e);e.Write(death+4,play);e.Write(death+0x10,choice);
        e.Write(death+0x14,0);e.Write(death+0x24,0);e.Write(play+0x18,button);e.Write(play+0x14,button);
        for(unsigned i=0;i<4;++i) {
            const U32 control=death+0x1120+i*0x1c;
            e.Write(control+4,death);e.Write(control+8,i);e.Write(control+0xc,25+i*5);
            e.Write(control+0x14,0x100048e);e.Write(control+0x18,0x10100);
        }
        r={};r[0]=death;r[13]=0x980000;r[14]=Env::Stop;r[15]=0x427424;
        for(unsigned i=4;i<12;++i)r[i]=0xcafe0000+i;
        cpu.ClearCache();cpu.Regs()=r;cpu.SetCpsr(0x10);e.remaining=100000;cpu.Run();r=cpu.Regs();
        assert(r[15]==Env::Stop+4 && r[13]==0x980000 && e.death_hook.input_stage==0);
        for(unsigned i=4;i<12;++i)assert(r[i]==0xcafe0000+i);
        if(button==8) {
            assert(e.Read(death+8)==0x1010e && e.Read(death+0x10)==choice);
            assert(e.Read(death+0x24)==0 && e.Read(death+0x20)==0x1234);
            continue;
        }
        assert((e.Read(death+8).value()&255)==15);
        assert(e.Read(death+0x24)==0 && e.Read(death+0x20)==0xffffffff);
        assert(((e.Read(death+8).value()>>8)&255)==(button==1 && choice==1?2:1));
    }
    assert(D::Instruction(0x42ec04,0xebfc3278)==0xebfc3278); // Gameplay input remains native.
    // Run the ROM's Items tab hit tests for a press and release with the
    // coordinates produced by the mapped shortcut. Stop at the HUD loop tail.
    {
        namespace P=OOT3D::PauseMenu;
        constexpr U32 main=0x50af34,touch=0x50bb38,stack=0x980000,tail=0x42f270;
        const auto saved=e.bytes;
        e.Write(tail,0xef000000);
        e.Write(P::UI+0xc,play);e.Write(P::UI+0x18,0);e.Write(P::UI+0x20,0);
        e.Write(play+0x100,0x203);e.Write(play+0x20ac,0x910000);e.Write(stack+0x40,play);
        e.Write(death+8,0);e.Write(B::Owner+8,0x10108);e.Write(B::Owner+0x3ec,0);
        e.Write(main+0x68,0);
        for(bool swap:{false,true}) {
            OOT3D::ControlSwap::enabled=swap;
            OOT3D::ControlSwap::Input mapping;P::MenuInput shortcut;
            shortcut.last_page=P::MenuInput::Target::Map;
            U32 keys=0;mapping.Filter(keys,true);
            e.Write(main+0x34,2);e.Write(P::UI+0x14,0);
            keys=swap?shortcut.Start:shortcut.Select;mapping.Filter(keys,true);
            assert(shortcut.Filter(e,keys,true) && keys==0);
            e.Put<std::uint16_t>(touch+2,shortcut.touch_x);e.Put<std::uint16_t>(touch+4,220);
            for(bool release:{false,true}) {
                e.Put<std::uint16_t>(touch+8,!release);e.Put<std::uint16_t>(touch+10,release);
                r={};r[5]=main;r[13]=stack;r[15]=release?0x42edf4:0x42e7f4;
                cpu.ClearCache();cpu.Regs()=r;cpu.SetCpsr(0x10);e.remaining=100000;cpu.Run();
                assert(cpu.Regs()[15]==tail+4 && e.Read(main+0x34)==(release?12u:7u));
                assert(e.Read(P::UI+0x14)==unsigned(release));
            }
        }
        OOT3D::ControlSwap::enabled=false;e.bytes=saved;cpu.ClearCache();
    }
    {
        namespace W=OOT3D::Widescreen;
        D::Hook fade;fade.stage=D::Hook::Stage::Native;fade.native_top_draw=true;fade.route=true;fade.owner=death;
        e.Write(death+8,0x10010);
        constexpr U32 pane=0x895000,model=0x895200;
        e.Write(death+D::LayoutOffset+0x418+250*4,pane);
        e.Write(pane+4,death+D::PaneResourcesOffset);e.Write(pane+0x64,model);
        W::enabled=true;W::supported_layout=true;
        const auto t=fade.NativeFadeTransform(e,model);assert(t);
        assert(std::abs(t->sx*400-240*W::Aspect)<.001f && t->sy==1);
        e.Write(death+8,0x10000);
        fade.route=false;assert(fade.NativeFadeTransform(e,model));
        assert(!fade.NativeFadeTransform(e,model+4));
        W::enabled=false;assert(!fade.NativeFadeTransform(e,model));
        W::supported_layout=false;
    }

    auto layout=B::GetLayout();layout[B::QuitPrompt]={12,13,.7f};
    layout[B::Quit]={-20,18,.6f};layout[B::Continue]={30,21,.8f};B::SetLayout(layout,false);
    for(unsigned state:{1u,12u,13u,14u,15u,16u,17u}) {
        e.Write(death+8,0x10000|state);e.Write(play+0x100,0x203);
        D::Hook quit;r={};r[0]=death;r[1]=play;quit.Top(e,r,true);
        if(state>=12) {
            assert(quit.frame_layout[D::Prompt].x==12);
            assert(std::abs(quit.frame_layout[D::DontSave].x-(-20+12.8f))<.001f);
            assert(std::abs(quit.frame_layout[D::Save].x-(30-12.8f))<.001f);
            assert(std::abs(quit.frame_layout[D::Save].scale-.8f*.8f)<.001f);
            assert(quit.shared_style);
        } else assert(quit.frame_layout[D::Prompt].x==D::SharedLayout(OOT3D::SaveMenu::GetLayout())[D::Prompt].x);
    }
    e.Write(B::Owner+0x3ec,0xffffffff);assert(!B::InBattle(e));
    {
        const auto previous_bytes=e.bytes;
        const auto original_hd=OOT3D::HUD::GetLayout();
        B::Hook draw;draw.active=true;draw.stage=B::Hook::Stage::Native;draw.pane=210;draw.timer_opacity=.5f;
        constexpr U32 p=0x8c0000,foreground=p+0x100,shadow=p+0x500,renderer=0x8d0000,alpha=0x8d0400;
        e.Write(B::NativeLayout+0x418+4*210,p);e.Write(p+0x64,foreground);e.Write(p+0x68,shadow);
        for(bool enabled:{false,true})for(bool timer:{false,true}) {
            auto hd=original_hd;hd.hd_numbers=enabled;hd.hd_timer_numbers=timer;
            OOT3D::HUD::SetLayout(hd,false);
            for(auto model:{foreground,shadow}) {
                e.Write(renderer+0x354,model);e.Write(alpha,std::bit_cast<U32>(.8f));
                draw.DrawMaterial(e,renderer,alpha);
                assert(std::bit_cast<float>(*e.Read(alpha))==(enabled && timer && model==shadow?0.f:.4f));
            }
        }
        OOT3D::HUD::SetLayout(original_hd,false);e.bytes=previous_bytes;
    }
    std::cout<<"Death Save/Continue: immediate native callbacks, sound continuation and ABI passed. Boss Challenge: native selection equals selection+Battle for all nine choices; Cancel/B, ABI, code guards, routing, both grids and touch mapping passed\n";
}
