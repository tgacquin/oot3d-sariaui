// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/death_menu.h"
#include "oot3d/pause_header.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <vector>
using OOT3D::TitleScreen::U32;
namespace D=OOT3D::DeathMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {
        auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};
    }
    void Write(U32 a,U32 value) {assert(words.contains(a));words[a]=value;}
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;
    std::ifstream f(argv[1],std::ios::binary);assert(f);U32 w;
    for(U32 at=0x100000;f.read(reinterpret_cast<char*>(&w),4);at+=4)m.words[at]=w;
    assert(D::CodeMatches(m));
    for(unsigned i=0;i<D::Sites.size();++i) {
        const auto site=D::Sites[i];
        assert((D::Instruction(site.pc,site.instruction)&0xf0000000)==(site.instruction&0xf0000000));
        assert((D::Instruction(site.pc,site.instruction)&0xffffff)==D::FirstSVC+i);
        m.words[site.pc]^=1;assert(!D::CodeMatches(m));m.words[site.pc]^=1;
    }
    constexpr U32 owner=0x8000000,play=0x8100000,panes=0x8200000;
    m.words[D::ControllerPointer]=owner;m.words[owner+8]=0x10000;
    m.words[owner+D::LayoutOffset+4]=0x8300000;
    m.words[OOT3D::PauseMenu::UI+0xc]=play;m.words[OOT3D::PauseMenu::UI+0x14]=1;
    m.words[OOT3D::PauseMenu::UI+0x20]=0;m.words[OOT3D::PauseMenu::UI+0x24]=1;
    m.words[play+0x100]=0x203;
    assert(!D::Active(m) && OOT3D::PauseMenu::Active(m));
    OOT3D::PauseHeader::Hook header;
    for(unsigned state=1;state<=17;++state) {
        m.words[owner+8]=0x10000|state;
        assert(D::Active(m) && !OOT3D::PauseMenu::Active(m));
        header.slide.progress=1;header.presenting=true;header.Update(m,100000,true);
        assert(!header.presenting && header.slide.progress==0);
    }
    m.words[owner+8]=0x10004;
    for(unsigned i=0;i<256;++i) {
        m.words[owner+D::PanesOffset+i*4]=panes+i*0x100;
        m.words[panes+i*0x100+4]=owner+0x2f8; // Native constructor: layout + 0x18.
    }
    for(U32 selection:{0u,1u,2u,0xffffffffu}) {
        m.words[owner+0x10]=selection;const auto before=m.words;
        D::Hook hook;std::array<U32,16> r{};r[0]=owner;r[1]=play;
        hook.Top(m,r,true);assert(r[15]==0x427a18 && !D::on_top.load());
        r[0]=123;r[15]=D::Sites[0].pc+4;
        auto expected=r;expected[14]=expected[15];
        std::vector<unsigned> draws;
        for(unsigned step=0;step<257;++step) {
            hook.Top(m,r,true);
            if(hook.stage==D::Hook::Stage::Idle)break;
            assert(hook.Drawing() && r[15]==0x2f1280);
            draws.push_back((r[0]-panes)/0x100);
        }
        assert(D::on_top.load() && r==expected && !draws.empty());
        for(unsigned i=0;i<256;++i) {
            const bool selected=std::find(draws.begin(),draws.end(),i)!=draws.end();
            assert(selected==(D::RoutedPane(i,selection) && !D::PressedStone(i)));
            r[0]=panes+i*0x100;r[4]=i;r[5]=owner+D::LayoutOffset;
            hook.BottomPane(m,r);assert((r[15]==D::Sites[1].pc+4)==D::RoutedPane(i,selection));
            r[5]+=4;hook.BottomPane(m,r);assert(r[15]==0x2f1280); // Other native layouts untouched.
        }
        for(unsigned i:{35u,36u,37u,38u,50u,51u,52u,250u})assert(!D::RoutedPane(i,selection));
        assert(m.words==before); // No pane, animation, or save-state edits.
        r[0]=owner;r[1]=play;hook.Top(m,r,false);hook.Top(m,r,false);
        assert(!D::on_top.load() && hook.stage==D::Hook::Stage::Idle);
    }
    // A rejected pane stays on bottom, even if other panes were routed.
    for(bool reject_all:{false,true}) {
        for(unsigned i=0;i<256;++i)
            m.words[panes+i*0x100+4]=owner+((reject_all || i==25)?0x2e0:0x2f8);
        D::Hook hook;std::array<U32,16> r{};r[0]=owner;r[1]=play;
        hook.Top(m,r,true);
        do {hook.Top(m,r,true);}while(hook.stage!=D::Hook::Stage::Idle);
        assert(D::on_top.load()!=reject_all);
        r[0]=panes+25*0x100;r[4]=25;r[5]=owner+D::LayoutOffset;
        hook.BottomPane(m,r);assert(r[15]==0x2f1280);
        if(!reject_all) {
            r[0]=panes+30*0x100;r[4]=30;hook.BottomPane(m,r);
            assert(r[15]==D::Sites[1].pc+4);
            r[0]+=4;hook.BottomPane(m,r);assert(r[15]==0x2f1280);
        }
    }
    m.words[owner+8]=0x1010004;assert(!D::Drawable(m,owner));
    m.words[owner+8]=0x10000;assert(!D::Active(m) && OOT3D::PauseMenu::Active(m));
    for(float width:{400.f,480.f}) {
        const auto p=D::MapPointer(.5f+64*.8f/width,138.f/240,width);
        assert(p && std::abs((*p)[0]-.7f)<.0001f && (*p)[1]==.5f);
        assert(!D::MapPointer(.5f,220.f/240,width)); // No invisible Visions hitbox on top.
    }
    // Each pane group (including lettering, pressed borders and cursor corners)
    // shares its control's pose. Other groups retain the existing layout.
    D::Layout edited{};edited[D::Prompt]={-15,20,1.4f};
    edited[D::Save]={120,70,.5f};edited[D::DontSave]={-30,-18,1.2f};
    for(unsigned pane:{5u,13u,15u,23u,90u,91u}) {
        const auto t=D::PaneTransform(pane,0,edited);
        assert(t.x==25 && t.y==20 && t.sx==1.4f && t.sy==1.4f && t.ax==160);
        assert(t.ay==((pane==91 || (pane>=15 && pane<=23))?204:24));
    }
    for(unsigned pane:{25u,26u,27u,28u,45u,46u,47u,60u,63u,65u,68u}) {
        const auto t=D::PaneTransform(pane,0,edited);
        assert(t.x==160 && t.y==70 && t.sx==.5f && t.sy==.5f && t.ax==224 && t.ay==120);
    }
    for(unsigned pane:{30u,31u,32u,33u,45u,46u,47u,60u,63u,65u,68u}) {
        const auto t=D::PaneTransform(pane,1,edited);
        assert(t.x==10 && t.y==-18 && t.sx==1.2f && t.sy==1.2f && t.ax==96 && t.ay==120);
    }
    assert(D::PanePart(35,2)==D::Count); // Visions is unchanged.
    for(float width:{400.f,480.f}) {
        for(auto part:{D::Save,D::DontSave}) {
            const auto p=edited[part];const auto a=D::Anchors[part];
            for(float offset:{-20.f,0.f,20.f}) {
                const float x=a[0]+p.x+offset*p.scale,y=a[1]+p.y+10*p.scale;
                const auto mapped=D::MapPointer(.5f+(x-160)/width,y/240,width,edited);
                assert(mapped && std::abs((*mapped)[0]-(a[0]+offset)/320)<.0001f);
                assert(std::abs((*mapped)[1]-130.f/240)<.0001f);
            }
        }
        assert(!D::MapPointer(.5f+64/width,.5f,width,edited)); // Old Save hitbox is gone.
    }
    edited[D::Save].scale=0;
    assert(D::PaneTransform(25,0,edited).sx==1); // Invalid configuration falls back safely.
    // The death QLY and regular Save atlas must land on the same pixels,
    // including extreme but legal editor scales and the separate card warning.
    namespace S=OOT3D::SaveMenu;
    const auto near=[](float a,float b){return std::abs(a-b)<.001f;};
    for(float scale:{.1f,1.f,3.f}) {
        S::Layout shared;shared[S::Prompt]={40,-20,scale};
        shared[S::Save]={-35,50,scale};shared[S::DontSave]={70,-60,scale};
        shared[S::CardWarning]={-400,240,scale};
        for(auto [pane,part]:std::array<std::pair<unsigned,S::Part>,4>{{
                {90,S::Prompt},{25,S::Save},{30,S::DontSave},{91,S::CardWarning}}}) {
            const auto t=D::SharedTransform(pane,0,shared);
            for(auto delta:std::array<std::array<float,2>,3>{{{0,0},{-40,-16},{40,16}}}) {
                const auto a=S::Anchors[part];const float x=a[0]+delta[0],y=a[1]+delta[1];
                const auto expected=S::MoveControl(x,y,part,shared);
                assert(near(t.ax+(x-t.ax)*t.sx+t.x,200+.8f*(expected[0]-160)));
                assert(near(t.ay+(y-t.ay)*t.sy+t.y,138+.8f*(expected[1]-120)));
            }
        }
        for(auto part:{S::Save,S::DontSave}) {
            const auto a=S::Anchors[part],p=S::MoveControl(a[0],a[1],part,shared);
            const auto hit=D::MapPointer(.5f+.8f*(p[0]-160)/480,
                (138+.8f*(p[1]-120))/240,480,D::SharedLayout(shared));
            assert(hit && near((*hit)[0],a[0]/320) && near((*hit)[1],a[1]/240));
        }
        const auto top=D::SharedTransform(5,0,shared),bottom=D::SharedTransform(11,0,shared);
        const auto center=D::SharedTransform(90,0,shared);
        assert(near(top.y,center.y) && near(bottom.y,center.y));
        assert(near(D::SharedTransform(8,0,shared).sx,.8f*scale));
    }
    // Validate every native texture slice, including mirrored right caps,
    // against all three localized QSPs. Short/tall share their top edge at y=4.
    for(const auto language:{"english","french","spanish"})for(bool tall:{false,true}) {
        const auto path=std::filesystem::path(argv[1]).parent_path().parent_path()/
            "romfs"/"misc"/"us"/language/"gameover.qsp";
        std::ifstream qsp(path,std::ios::binary);assert(qsp);
        const auto g=D::MakePrompt(tall);assert(g.count==(tall?9:6));
        unsigned q=0;
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col) {
            if(!tall && row==1)continue;
            std::array<float,8> native;
            qsp.seekg((unsigned(tall?15:5)+row*3+col)*40+4);
            qsp.read(reinterpret_cast<char*>(native.data()),32);assert(qsp);
            assert(g.positions[q*12]==-native[0] && g.positions[q*12+1]==-native[1]);
            assert(g.positions[q*12+3]-g.positions[q*12]==native[2]);
            assert(g.positions[q*12+7]-g.positions[q*12+1]==native[3]);
            for(unsigned v=0;v<4;++v) {
                const float u=(native[4]+(v%2?native[6]:0))/256;
                const float t=1-(native[5]+(v>=2?native[7]:0))/256;
                assert(std::abs(g.uvs[q*8+v*2]-u)<=.5f/256);
                assert(std::abs(g.uvs[q*8+v*2+1]-t)<=.5f/256);
            }
            ++q;
        }
        assert((tall?36:24)+g.positions[1]==4);
    }
    for(U32 state:{3u,9u,5u,11u,13u,15u}) {
        const auto first=D::Animation(state,0),middle=D::Animation(state,2),last=D::Animation(state,4);
        assert(middle.buttons>0 && middle.buttons<1);
        assert(first.rise==(state==3 || state==9 || state==13?18:0));
        assert(last.rise==(state==3 || state==9 || state==13?0:18));
        assert(D::Animation(state,40).rise==last.rise);
    }
    // Native QAN evaluation has already copied its channels onto the stack.
    // Strip presentation motion there, retaining the native clocks/resources
    // and unrelated colors, and never touch a different menu or boss quit.
    {
        D::Hook h;std::array<U32,16> regs{};
        constexpr U32 stack=0x0ffff000;
        regs[13]=stack;regs[4]=panes+5*0x100;
        m.words[regs[4]+4]=owner+D::PaneResourcesOffset;
        for(U32 state:{3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,14u})for(bool supported:{false,true}) {
            m.words[owner+8]=0x10000|state;m.words[stack+0x58]=0xfffff;
            const auto before=m.words;
            h.PaneAnimation(m,regs,supported);
            const U32 expected=supported && state<=15?0xffc00:0xfffff;
            assert(regs[0]==expected && m.Read(stack+0x58)==expected);
            auto restored=m.words;restored[stack+0x58]=0xfffff;assert(restored==before);
        }
        m.words[owner+8]=0x10003;m.words[stack+0x58]=0xfffff;
        regs[4]=panes+250*0x100; // Scene fade must retain all native channels.
        h.PaneAnimation(m,regs,true);assert(regs[0]==0xfffff);
    }
    // Pressed RGB must be stripped for both labels and stone backgrounds,
    // while alpha and native shadow color keep their own channels.
    {
        D::Hook h;std::array<U32,16> r{};r[13]=0x0ffff000;
        m.words[owner+8]=0x10004;
        for(unsigned i:{25u,28u,30u,33u}) {
            r[4]=panes+i*0x100;m.words[r[4]+4]=owner+D::PaneResourcesOffset;
            m.words[r[13]+0x58]=0xfffff;h.PaneAnimation(m,r,true);
            assert(r[0]==0xfe000);
        }
        // Interleaved frames only run the presentation tail, with context
        // supplied exactly as the normal prologue would have done.
        r[4]=owner;r[1]=play;r[15]=D::Sites[6].pc+4;
        m.words[owner+4]=0;D::half_frame=D::extra_frame=true;
        h.ControlUpdate(m,r);assert(r[15]==0x41ce0c && m.Read(owner+4)==play);
        D::extra_frame=false;r[15]=D::Sites[6].pc+4;h.ControlUpdate(m,r);
        assert(r[15]==D::Sites[6].pc+4 && r[0]==3);D::half_frame=false;
    }
    // Resource changes happen before the real layout update. Exercise the
    // native rebind continuations, including the initial hidden text and the
    // short -> saving -> tall transition with no blank state in between.
    {
        D::Hook h;std::array<U32,16> regs{};
        constexpr U32 assets=0x8800000,models=0x8900000;
        for(unsigned res=0;res<206;++res) {
            const U32 asset=assets+res*0x400;
            m.words[owner+D::PaneResourcesOffset+4*res]=asset;
            m.words[asset+4]=0x9000000;m.words[asset+0x1bc]=0x9000100;
        }
        for(unsigned i=0;i<10;++i) {
            const U32 pane=panes+(i==9?90:5+i)*0x100;
            m.words[pane+4]=owner+D::PaneResourcesOffset;
            m.words[pane+0x6c]=0;m.words[pane+0x84]=std::bit_cast<U32>(24.f);
            m.words[pane+0x78]=i==9?200:5+i;
            for(unsigned j=0;j<2;++j) {
                const U32 model=models+(i*2+j)*0x200;
                m.words[pane+0x64+j*4]=model;m.words[model]=0x4ebd60;
                m.words[model+8]=0x9100000;
            }
        }
        const U32 text=panes+90*0x100;
        // Boss challenge keeps its localized resource 205, shows its tall
        // box/text immediately, and advances presentation in half-ticks.
        for(U32 state:{13u,14u,15u}) {
            m.words[owner+8]=0x10000|state;m.words[text+0x78]=205;
            m.words[text+0x6c]=0;regs[0]=owner+D::LayoutOffset;
            do {
                h.Update(m,regs,true);
                if(h.updating)m.Write(regs[0]+0x78,regs[1]);
            }while(h.updating);
            assert(m.Read(text+0x78)==205 && (m.Read(text+0x6c).value()&255)==1);
            assert(m.Read(panes+5*0x100+0x78)==15);
            assert(m.Read(panes+5*0x100+0x84)==std::bit_cast<U32>(36.f));
        }
        for(bool saving:{true,false})for(U32 state:{3u,4u,5u,6u,7u,8u,9u,10u,11u}) {
            if(!saving && (state==6 || state==7 || state==8))continue;
            m.words[owner+8]=0x10000|state;m.words[owner+0xc]=saving;
            m.words[text+0x6c]=0; // Native save flow hides it before this hook.
            regs.fill(0x12345678);regs[0]=owner+D::LayoutOffset;
            const auto saved=regs;
            unsigned calls=0;
            do {
                h.Update(m,regs,true);
                if(h.updating) {
                    assert(regs[15]==0x344670 && regs[14]==D::Sites[5].pc);
                    m.Write(regs[0]+0x78,regs[1]); // Simulate the native rebind.
                    assert(++calls<=10);
                }
            }while(h.updating);
            assert(regs[15]==0x2fd8e8 && regs[14]==D::Sites[5].pc+4);
            regs[14]=saved[14];regs[15]=saved[15];assert(regs==saved);
            assert((m.Read(text+0x6c).value()&255)==1);
            const U32 expected=state<5?200:state<=8?(saving?201:204):(saving?203:204);
            assert(m.Read(text+0x78)==expected);
            const bool tall=expected>=203;
            assert(m.Read(panes+5*0x100+0x78)==(tall?15:5));
            assert(m.Read(panes+5*0x100+0x84)==std::bit_cast<U32>(tall?36.f:24.f));
        }
        // Unsupported rendering leaves every byte intact, including text visibility.
        regs[0]=owner+D::LayoutOffset;const auto before=m.words;
        h.Update(m,regs,false);assert(m.words==before && !h.updating);
        m.words[owner+8]=0x10004;
        // Remove fixture render models so later routing tests use their native fallback.
        for(unsigned i=0;i<10;++i)for(unsigned j=0;j<2;++j)
            m.words[panes+(i==9?90:5+i)*0x100+0x64+j*4]=0;
    }
    // Native offscreen slide is removed in the fresh view transform; its
    // resting position, editor scale, and native model matrix remain intact.
    {
        D::Hook h;h.current_pane=25;h.state=3;h.stage=D::Hook::Stage::Panes;
        h.animation=D::Animation(3,2);
        constexpr U32 pane=0x8600000,model=0x8610000,alpha=0x8630000;
        h.routed_panes[25]=pane;
        const std::array<float,12> matrix{1,0,0,864,0,1,0,120,0,0,1,0};
        for(unsigned i=0;i<12;++i)m.words[model+0xc+i*4]=std::bit_cast<U32>(matrix[i]);
        m.words[pane+0x64]=model;m.words[pane+0x80]=std::bit_cast<U32>(224.f);
        m.words[pane+0x84]=std::bit_cast<U32>(120.f);m.words[alpha]=0;
        const auto before=m.words;const auto t=h.ModelTransform(m,model);
        assert(near(t.ax+(864-t.ax)*t.sx+t.x,200+64*.8f));
        assert(near(t.ay+(120-t.ay)*t.sy+t.y,138+h.animation.rise*.8f));
        assert(m.words==before);
        h.Material(m,alpha,model);assert(near(std::bit_cast<float>(*m.Read(alpha)),h.animation.buttons));
        m.words[alpha]=0;h.Material(m,alpha,model+4);assert(m.Read(alpha)==0);
    }
    // Prompt stays stationary through entrance/exit, and opacity reaches
    // zero without reviving when Continue advances into the scene fade.
    {
        D::Hook h;h.stage=D::Hook::Stage::Panes;h.current_pane=90;
        constexpr U32 pane=0x8640000,model=0x8650000,alpha=0x8660000;
        h.routed_panes[90]=pane;m.words[pane+0x64]=model;
        m.words[pane+0x80]=std::bit_cast<U32>(160.f);m.words[pane+0x84]=std::bit_cast<U32>(36.f);
        std::array<float,12> matrix{1,0,0,160,0,1,0,36,0,0,1,0};
        for(unsigned i=0;i<12;++i)m.words[model+0xc+i*4]=std::bit_cast<U32>(matrix[i]);
        m.words[alpha]=0;
        for(U32 state:{3u,11u,13u,15u}) {
            h.state=state;const bool entering=state==3 || state==13;float previous=entering?0:1;
            for(float ticks=0;ticks<=4;ticks+=.5f) {
                h.animation=D::Animation(state,ticks);
                const auto t=h.ModelTransform(m,model),rest=D::SharedTransform(90,0,h.save_layout);
                assert(near(t.y,rest.y));h.Material(m,alpha,model);
                const float a=std::bit_cast<float>(*m.Read(alpha));
                assert(entering?a>=previous:a<=previous);previous=a;
            }
            assert(previous==(entering?1:0));
        }
        for(U32 state:{12u,16u,17u}) {
            h.state=state;h.animation=D::Animation(state,0);h.Material(m,alpha,model);
            assert(m.Read(alpha)==0);
            h.shared_style=false;m.words[alpha]=std::bit_cast<U32>(.75f);
            h.Material(m,alpha,model);assert(m.Read(alpha)==std::bit_cast<U32>(.75f));
            h.shared_style=true;
        }
    }
    // If the owned prompt cannot be allocated, draw every original slice,
    // including the first cap, and restore the donor sampler after the call.
    for(bool succeeds:{false,true}) {
        D::Hook h;h.stage=D::Hook::Stage::Panes;h.owner=owner;h.selection=0;
        h.current_pane=5;h.next=6;h.attempted[0]=true;
        h.routed_panes[5]=panes+5*0x100;
        for(unsigned i=0;i<256;++i)m.words[panes+i*0x100+4]=owner+0x2f8;
        auto& prompt=h.prompts[0];
        prompt.stage=succeeds?decltype(prompt.stage)::Draw:decltype(prompt.stage)::Allocate;
        std::array<U32,16> regs{}; // Null allocation result simulates failure.
        h.Top(m,regs,true);
        assert(h.replaced[0]==succeeds);
        assert(regs[0]==panes+(succeeds?15:5)*0x100);
        assert(regs[15]==0x2f1280 && h.Drawing());
    }
    // Both foreground and shadow samplers are linear only during the pane's
    // native draw. The atlas address, material, and all other fields survive.
    {
        constexpr U32 pane=0x8600000,model=0x8610000,mesh=0x8620000;
        m.words[pane+0x64]=model;m.words[pane+0x68]=model+0x200;
        for(unsigned i=0;i<2;++i) {
            m.words[model+i*0x200]=0x4ebd60;m.words[model+i*0x200+4]=mesh+i*0x200;
            for(unsigned j=0;j<0x19c;j+=4)m.words[mesh+i*0x200+j]=0x12345678+j;
            m.words[mesh+i*0x200+0x13c]=0;m.words[mesh+i*0x200+0x140]=0;
        }
        const auto before=m.words;D::Hook hook;hook.FilterPane(m,pane);
        for(unsigned i=0;i<2;++i) {
            assert(m.Read(mesh+i*0x200+0x13c)==1 && m.Read(mesh+i*0x200+0x140)==3);
            assert(m.Read(mesh+i*0x200+0x158)==before.at(mesh+i*0x200+0x158));
        }
        hook.RestoreSamplers(m);assert(m.words==before);
        // Game Over belongs to the native top layout, not the routed save
        // panes. Its original draw gets the same filtering and restores it
        // before returning, even if the controller closes during that draw.
        m.words[owner+D::LayoutOffset+0x41c]=pane;
        for(bool supported:{false,true})for(bool owned:{false,true}) {
            m.words[pane+4]=owner+D::PaneResourcesOffset+(owned?0:4);
            m.words[owner+8]=0x10001;
            const auto original=m.words;
            D::Hook title_hook;std::array<U32,16> regs{};
            regs[0]=owner;regs[1]=play;
            title_hook.Top(m,regs,supported);
            assert(regs[15]==0x427a18 && title_hook.stage==D::Hook::Stage::Native);
            for(unsigned i=0;i<2;++i) {
                assert(m.Read(mesh+i*0x200+0x13c)==(supported && owned?1u:0u));
                assert(m.Read(mesh+i*0x200+0x140)==(supported && owned?3u:0u));
            }
            m.words[owner+8]=0x10000;
            title_hook.Top(m,regs,supported);
            assert(title_hook.stage==D::Hook::Stage::Idle);
            m.words[owner+8]=0x10001;
            assert(m.words==original);
        }
        // The shadow may share the foreground descriptor: restore it once.
        m.words[model+0x204]=mesh;const auto shared=m.words;
        hook.FilterPane(m,pane);hook.RestoreSamplers(m);assert(m.words==shared);
        m.words[model]=0; m.words[model+0x200]=0;const auto invalid=m.words;
        hook.FilterPane(m,pane);hook.RestoreSamplers(m);assert(m.words==invalid);
    }
    // Complete the native Game Over fade before invoking the stone entrance.
    // The title's animation clock keeps advancing in the existing update tail.
    constexpr U32 title=0x8400000;
    m.words[owner+4]=play;m.words[owner+8]=0x10001;
    m.words[owner+D::LayoutOffset+0x41c]=title;
    m.words[title+4]=owner+D::PaneResourcesOffset;
    m.words[title+0xc]=0x8500000;
    m.words[title+0x1c]=std::bit_cast<U32>(15.f);
    D::Hook transition;std::array<U32,16> r{};r[4]=owner;r[0]=owner;
    for(unsigned death=0;death<2;++death) {
        m.words[owner+8]=0x10001;m.words[title+0x6c]=0x12345601;
        r[15]=D::Sites[2].pc+4;transition.Transition(m,r,2,true);
        assert(r[3]==0x13 && r[15]==D::Sites[2].pc+4);
        m.words[owner+8]=0x10002;
        for(float clock:{0.f,.5f,7.5f,14.5f}) {
            m.words[title+0x18]=std::bit_cast<U32>(clock);const auto before=m.words;
            transition.Transition(m,r,3,true);
            assert(r[15]==D::Sites[3].pc+4 && m.words==before);
        }
        m.words[title+0x18]=std::bit_cast<U32>(15.f);
        transition.Transition(m,r,3,true);
        assert(r[15]==0x426870 && r[14]==D::Sites[3].pc+4);
        assert(m.words[title+0x6c]==0x12345600 && transition.fading_owner==0);
    }
    // Other menus and unsupported rendering retain the original loop/call.
    for(bool supported:{false,true})for(U32 state:{0u,3u,7u}) {
        m.words[play+0x100]=state;m.words[title+0x6c]=1;
        transition.Transition(m,r,2,supported);
        assert(r[3]==(supported && state==3?0x13u:0x12u));
        if(!supported || state!=3) {
            transition.Transition(m,r,3,supported);
            assert(r[15]==0x426870 && m.words[title+0x6c]==1);
        }
    }
    // A missing title/animation cannot leave the save menu waiting forever.
    m.words[play+0x100]=3;transition.Transition(m,r,2,true);
    m.words[title+0xc]=0;transition.Transition(m,r,3,true);
    assert(r[15]==0x426870);
    m.words[title+4]=0;transition.Transition(m,r,2,true);
    assert(r[3]==0x12);transition.Transition(m,r,3,true);assert(r[15]==0x426870);
    std::cout<<"Death menu: native signatures, pause isolation, pane routing, Visions isolation, fallback and touch passed\n";
}
