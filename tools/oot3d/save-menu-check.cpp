// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_input.h"
#include "oot3d/pause_framerate.h"
#include "oot3d/pause_shadow.h"
#include "oot3d/screen_model_hooks.h"
#include "oot3d/heap_diagnostics.h"
#include "oot3d/save_loading.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
    for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(PauseMenu::CodeMatches(m));
    assert(HeapDiagnostics::CodeMatches(m));
    {
        SaveLoading::Fade fade;
        assert(fade.Advance(1000000,1,true,true)==0);
        const float middle=fade.Advance(1040000,1,true,true);assert(std::abs(middle-.5f)<.001f);
        assert(fade.Advance(1080000,1,true,true)==1);
        assert(fade.Advance(1080000,1,true,true)==1); // Duplicate draws do not speed it up.
        assert(std::abs(fade.Advance(1130000,1,true,false)-.5f)<.001f);
        assert(fade.Advance(1180000,1,true,false)==0);
        fade.Advance(1220000,1,true,true);
        assert(fade.Advance(1230000,2,true,true)==0); // Controller recreation.
        assert(fade.Advance(1200000,2,true,true)==0); // Savestate/time rollback.
        assert(fade.Advance(1600000,2,true,true)==0); // Missed UI updates.
        assert(fade.Advance(1680000,2,true,true)==1);
        assert(fade.Advance(1680001,2,false,true)==0);
        // Read-only busy-state routing: choosing Don't Save must never light
        // the spinner, and leaving the write state starts its fade immediately.
        Memory memory;std::array<U32,16> regs{},original=regs;
        SaveLoading::Hook hook;SaveMenu::on_top=true;
        memory.words[SaveMenu::Owner+0x28]=2;
        memory.words[SaveMenu::Owner+0x24]=7;
        hook.Begin(memory,regs,0x100000,false,true,true,1000000);
        hook.Begin(memory,regs,0x100000,false,true,true,1080000);assert(hook.fade.opacity==0);
        memory.words[SaveMenu::Owner+0x24]=8;
        hook.Begin(memory,regs,0x100000,false,true,true,1160000);assert(hook.fade.opacity==1);
        memory.words[SaveMenu::Owner+0x24]=9;
        hook.Begin(memory,regs,0x100000,false,true,true,1200000);assert(hook.fade.opacity==1);
        memory.words[SaveMenu::Owner+0x24]=10;
        hook.Begin(memory,regs,0x100000,false,true,true,1300000);assert(hook.fade.opacity==0);
        constexpr U32 death=0x08000000;
        memory.words[DeathMenu::ControllerPointer]=death;
        memory.words[death+DeathMenu::LayoutOffset+4]=0x08010000;
        memory.words[death+8]=0x10005;memory.words[death+0xc]=1;DeathMenu::on_top=true;
        hook.Begin(memory,regs,0x100000,true,true,false,1400000);
        hook.Begin(memory,regs,0x100000,true,true,false,1480000);assert(hook.fade.opacity==1);
        memory.words[death+8]=0x10007;
        hook.Begin(memory,regs,0x100000,true,true,false,1500000);assert(hook.fade.opacity==1);
        memory.words[death+8]=0x10008;
        hook.Begin(memory,regs,0x100000,true,true,false,1600000);assert(hook.fade.opacity==0);
        memory.words[death+8]=0x10005;memory.words[death+0xc]=0;
        hook.Begin(memory,regs,0x100000,true,true,false,1680000);assert(hook.fade.opacity==0);
        assert(regs==original && !hook.Pending());
        SaveMenu::on_top=DeathMenu::on_top=false;
    }
    {
        HeapDiagnostics::Hook trace;
        std::array<U32,16> regs{};
        for(unsigned i=0;i<regs.size();++i)regs[i]=0x12340000+i;
        regs[4]=0x8800000;regs[5]=452;regs[13]=0xffff000;
        m.words[regs[13]+20]=0x313ce8;m.words[regs[13]+28]=0x2f5820;
        const auto before=m.words;auto expected=regs;expected[13]+=12;
        unsigned records=0,details=0;
        const auto record=[&](U32 caller,U32 size,U32,const auto&,auto,auto) {
            assert(caller==0x2f5820 && size==452);++records;
        };
        const auto detail=[&](const auto& entry){assert(entry.pc==0x2f5820);++details;};
        trace.Return(m,regs,record,detail);assert(regs==expected && m.words==before);
        for(unsigned i=0;i<10;++i) {
            regs[13]=0xffff000;regs[4]=0;
            trace.Return(m,regs,record,detail);
        }
        assert(records<=5 && details==4 && trace.failures==10);
        assert(trace.callers[0].requests==11 && trace.callers[0].bytes==11*452 && m.words==before);
    }
    assert(m.Read(0x43ae74)==SaveMenu::Owner && m.Read(0x439658)==SaveMenu::Owner);
    constexpr U32 play=0x8000000,mesh=0x8100000,src=0x8200000,dst=0x8300000;
    m.words[PauseMenu::UI+0xc]=play;m.words[PauseMenu::UI+0x14]=1;
    m.words[PauseMenu::UI+0x20]=0;m.words[PauseMenu::UI+0x24]=1;m.words[play+0x100]=0x203;
    m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=0;
    m.words[SaveMenu::Owner+0x28]=2;m.words[SaveMenu::Owner]=mesh;
    m.words[SaveMenu::Owner+0x24]=1;
    assert(SaveMenu::Active(m) && PauseMenu::Active(m) && !PauseMenu::MapPage(m));
    PauseMenu::Layout unrelated;
    unrelated[PauseMenu::Map]={200,150,2};unrelated.map_backgrounds[0].darkness=1;
    PauseMenu::SetLayout(unrelated,false);PauseMenu::motion_x=-300;
    // Native atlas groups: prompt, right button, left button, button lettering.
    for(unsigned q=0;q<78;++q) {
        const float x=q<6?12:q<10?164:36,y=q<6?4:96;
        const std::array<float,12> v{x,y,0,x,y+40,0,x+24,y,0,x+24,y+40,0};
        for(unsigned i=0;i<12;++i) {
            m.words[src+q*48+4*i]=std::bit_cast<U32>(v[i]);m.words[dst+q*48+4*i]=0;
        }
    }
    SaveMenu::SetLayout({},false);
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48));
    for(unsigned i=0;i<43*12;++i)assert(m.words[dst+4*i]==m.words[src+4*i]);
    const auto collapsed=[&](unsigned q) {
        for(unsigned i=3;i<12;i+=3)for(unsigned axis=0;axis<3;++axis)
            if(m.words[dst+48*q+4*(i+axis)]!=m.words[dst+48*q+4*axis])return false;
        return true;
    };
    for(unsigned q=43;q<78;++q)assert(collapsed(q));
    SaveMenu::Layout layout;layout[SaveMenu::DontSave]={-24,32,.5f};
    layout[SaveMenu::Save]={40,-16,1.5f};layout[SaveMenu::Prompt]={8,16,2};
    SaveMenu::SetLayout(layout,false);
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48));
    assert(m.F(dst)==160+(12-160)*2+10 && m.F(dst+4)==24+(4-24)*2+20);
    assert(m.F(dst+6*48)==224+(164-224)*1.5f+50);
    assert(m.F(dst+10*48)==96+(36-96)*.5f-30);
    assert(m.words[dst+15*48]==m.words[src+15*48]); // Native Back card is independent.
    const auto once=m.words;
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48) && m.words==once);
    layout[SaveMenu::Back]={-72,-48,1.5f};layout[SaveMenu::Options]={88,-32,.75f};
    SaveMenu::SetLayout(layout,false);
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48));
    for(unsigned q:{15u,16u,17u,37u,38u,39u,18u,19u,20u,21u,40u,41u,42u}) {
        const auto part=SaveMenu::AtlasPart(q,false);assert(part);
        const auto p=SaveMenu::Move(m.F(src+48*q),m.F(src+48*q+4),*part,layout[*part]);
        assert(m.F(dst+48*q)==p[0] && m.F(dst+48*q+4)==p[1]);
    }
    for(auto part:{SaveMenu::DontSave,SaveMenu::Save,SaveMenu::Back,SaveMenu::Options})
        for(float width:{400.f,240*Widescreen::Aspect}) {
        const auto a=SaveMenu::Anchors[part],p=SaveMenu::Move(a[0],a[1],part,layout[part]);
        const auto hit=SaveMenu::MapPointer(.5f+(p[0]-160)*.8f/width,
            .5f+((p[1]-120)*.8f+18)/240,width,layout);
        assert(hit && std::abs((*hit)[0]-a[0]/320)<.0001f && std::abs((*hit)[1]-a[1]/240)<.0001f);
    }
    assert(!SaveMenu::MapPointer(.5f,.1f,400,layout));
    assert(SaveMenu::MapPointer(.5f,.4f,400,layout,SaveMenu::InputPage::Options));
    assert(!SaveMenu::MapPointer(.5f,.4f,400,layout,SaveMenu::InputPage::Hidden));
    // Hidden primary/pressed labels and the Options footer stay hidden even
    // when a user offset would bring their native parking coordinates onscreen.
    const auto quad=[&](unsigned q,float x,float y,float w,float h) {
        const std::array<float,12> v{x,y,0,x,y+h,0,x+w,y,0,x+w,y+h,0};
        for(unsigned i=0;i<12;++i)m.words[src+q*48+4*i]=std::bit_cast<U32>(v[i]);
    };
    layout[SaveMenu::Save]={-320,0,1};SaveMenu::SetLayout(layout,false);
    quad(34,564,98,16,48);
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48) && collapsed(34));
    m.words[SaveMenu::Owner+0x28]=3;m.words[SaveMenu::Owner+0x3c]=1;
    assert(SaveMenu::CurrentInputPage(m)==SaveMenu::InputPage::Hidden);
    quad(43,330,25,16,48);quad(44,80,25,200,48);quad(64,2,266,8,36);
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48));
    assert(collapsed(43) && !collapsed(44) && collapsed(64) && collapsed(6));
    m.words[SaveMenu::Owner+0x3c]=2;
    assert(SaveMenu::CurrentInputPage(m)==SaveMenu::InputPage::Options);
    quad(43,10,25,16,48);quad(64,2,202,8,36);
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48));
    assert(!collapsed(43) && !collapsed(64));
    layout[SaveMenu::OptionsItems]={-56,-24,.75f};SaveMenu::SetLayout(layout,false);
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48));
    for(unsigned q=43;q<78;++q) {
        const auto part=q<64?SaveMenu::OptionsItems:
            (q<68 || (q>=72 && q<=74))?SaveMenu::Cancel:SaveMenu::Okay;
        assert(SaveMenu::AtlasPart(q,false)==part);
        const auto p=SaveMenu::Move(m.F(src+48*q),m.F(src+48*q+4),
            SaveMenu::OptionsItems,layout[SaveMenu::OptionsItems]);
        assert(m.F(dst+48*q)==p[0] && m.F(dst+48*q+4)==p[1]);
    }
    for(float width:{400.f,240*Widescreen::Aspect})
        for(const auto a:{std::array{20.f,45.f},std::array{290.f,100.f},
                         std::array{52.f,220.f},std::array{268.f,220.f}}) {
            const auto p=SaveMenu::Move(a[0],a[1],SaveMenu::OptionsItems,layout[SaveMenu::OptionsItems]);
            const auto hit=SaveMenu::MapPointer(.5f+(p[0]-160)*.8f/width,
                .5f+((p[1]-120)*.8f+18)/240,width,layout,SaveMenu::InputPage::Options);
            assert(hit && std::abs((*hit)[0]-a[0]/320)<.0001f &&
                std::abs((*hit)[1]-a[1]/240)<.0001f);
        }
    // Independent button movement/scale covers normal and pressed art, while
    // preserving the existing whole-menu pose and every setting row.
    layout[SaveMenu::Cancel]={28,-30,.65f};
    layout[SaveMenu::Okay]={-12,-70,1.3f};
    SaveMenu::SetLayout(layout,false);
    for(unsigned q=64;q<78;++q) {
        const auto part=*SaveMenu::AtlasPart(q,false);
        const auto a=SaveMenu::Anchors[part];
        quad(q,a[0]-40,204,80,32);
    }
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48));
    for(unsigned q=43;q<78;++q) {
        const auto part=*SaveMenu::AtlasPart(q,false);
        const auto p=SaveMenu::MoveControl(m.F(src+48*q),m.F(src+48*q+4),part,layout);
        assert(std::abs(m.F(dst+48*q)-p[0])<.0001f &&
            std::abs(m.F(dst+48*q+4)-p[1])<.0001f);
    }
    for(auto part:{SaveMenu::Cancel,SaveMenu::Okay})for(float width:{400.f,240*Widescreen::Aspect}) {
        const auto a=SaveMenu::Anchors[part];
        const auto p=SaveMenu::MoveControl(a[0],a[1],part,layout);
        const auto hit=SaveMenu::MapPointer(.5f+(p[0]-160)*.8f/width,
            .5f+((p[1]-120)*.8f+18)/240,width,layout,SaveMenu::InputPage::Options);
        assert(hit && std::abs((*hit)[0]*320-a[0])<.0001f &&
            std::abs((*hit)[1]*240-a[1])<.0001f);
        const auto old=SaveMenu::Move(a[0],a[1],SaveMenu::OptionsItems,layout[SaveMenu::OptionsItems]);
        assert(!SaveMenu::MapPointer(.5f+(old[0]-160)*.8f/width,
            .5f+((old[1]-120)*.8f+18)/240,width,layout,SaveMenu::InputPage::Options));
    }
    m.words[SaveMenu::Owner+0x3c]=3;
    quad(43,-310,25,16,48);quad(64,2,266,8,36);
    assert(PauseMenu::CopyPositions(m,mesh,dst,src,78*48) && collapsed(43) && collapsed(64));
    assert(SaveMenu::CurrentInputPage(m)==SaveMenu::InputPage::Hidden);
    // Cursor is suppressed for every transition, follows Options on the save
    // page, and never inherits a Save pose on the actual Options page.
    constexpr U32 cursor=0x8600000,cursor_mesh=0x8700000;
    m.words[SaveMenu::Owner+0x18]=cursor;m.words[cursor]=cursor_mesh;
    for(unsigned q=0;q<8;++q)quad(q,108,200,16,16);
    assert(SaveMenu::CopyPositions(m,cursor_mesh,dst,src,8*48) && collapsed(0));
    m.words[SaveMenu::Owner+0x28]=2;m.words[SaveMenu::Owner+0x38]=1;
    for(U32 sub:{0,2,7,8,9,10,14,15,16}) {
        m.words[SaveMenu::Owner+0x24]=sub;
        assert(SaveMenu::CopyPositions(m,cursor_mesh,dst,src,8*48) && collapsed(0));
    }
    m.words[SaveMenu::Owner+0x24]=1;
    assert(SaveMenu::CopyPositions(m,cursor_mesh,dst,src,8*48));
    const auto option_corner=SaveMenu::Move(108,200,SaveMenu::Options,layout[SaveMenu::Options]);
    assert(m.F(dst)==option_corner[0] && m.F(dst+4)==option_corner[1]);
    m.words[SaveMenu::Owner+0x28]=3;m.words[SaveMenu::Owner+0x3c]=2;
    assert(SaveMenu::CopyPositions(m,cursor_mesh,dst,src,8*48));
    const auto cursor_corner=SaveMenu::Move(108,200,SaveMenu::OptionsItems,layout[SaveMenu::OptionsItems]);
    assert(m.F(dst)==cursor_corner[0] && m.F(dst+4)==cursor_corner[1]);
    for(auto part:{SaveMenu::Cancel,SaveMenu::Okay}) {
        m.words[SaveMenu::Owner+0x30]=part==SaveMenu::Cancel?3:4;
        const auto a=SaveMenu::Anchors[part];
        for(unsigned q=0;q<8;++q)quad(q,a[0]-48,200,8,8);
        assert(SaveMenu::CopyPositions(m,cursor_mesh,dst,src,8*48));
        const auto p=SaveMenu::MoveControl(a[0]-48,200,part,layout);
        assert(std::abs(m.F(dst)-p[0])<.0001f && std::abs(m.F(dst+4)-p[1])<.0001f);
    }
    quad(0,400,0,16,16);
    assert(SaveMenu::CopyPositions(m,cursor_mesh,dst,src,8*48) && collapsed(0));
    m.words[SaveMenu::Owner+0x28]=2;
    PauseMenu::MenuInput keys;U32 buttons=PauseMenu::MenuInput::L;
    assert(!keys.Filter(m,buttons,true) && buttons==PauseMenu::MenuInput::L);
    assert(!keys.HiddenTouch(170,220)); // Save is not the tabless map page.
    // Prompt text uses its own nine-model native text renderer, not the map.
    constexpr U32 text=0x8400000,model=0x8500000;
    m.words[SaveMenu::Owner+0x1c]=text;m.words[text+0x40]=2;
    for(unsigned i=0;i<9;++i)m.words[text+0x10+i*4]=model+i*0x200;
    for(unsigned i=0;i<9;++i)assert(SaveMenu::PromptModel(m,model+i*0x200));
    assert(!SaveMenu::PromptModel(m,model+0x2000));
    // Saving warning frame/text have one additional pose. Existing prompt
    // layouts stay identical at zero, and reused confirmation art stays separate.
    {
        const auto memory=m.words;
        auto warning_layout=layout;
        m.words[SaveMenu::Owner+0x24]=9;
        constexpr U32 warning_text=0x8c00000,warning_model=0x8d00000;
        m.words[SaveMenu::Owner+0x20]=warning_text;m.words[warning_text+0x40]=2;
        for(unsigned i=0;i<9;++i) {
            m.words[warning_text+0x10+4*i]=warning_model+i*0x200;
            assert(SaveMenu::PromptPart(m,warning_model+i*0x200)==SaveMenu::CardWarning);
            assert(SaveMenu::PromptPart(m,model+i*0x200)==SaveMenu::Prompt);
        }
        quad(0,12,4,24,20);quad(22,12,172,24,20);
        const auto old=SaveMenu::Move(12,172,SaveMenu::Prompt,layout[SaveMenu::Prompt]);
        const auto unchanged=SaveMenu::MoveControl(12,172,SaveMenu::CardWarning,warning_layout);
        assert(old==unchanged);
        warning_layout[SaveMenu::CardWarning]={-30,24,.65f};SaveMenu::SetLayout(warning_layout,false);
        assert(SaveMenu::CopyPositions(m,mesh,dst,src,78*48));
        const auto moved=SaveMenu::MoveControl(12,172,SaveMenu::CardWarning,warning_layout);
        assert(m.F(dst+22*48)==moved[0] && m.F(dst+22*48+4)==moved[1]);
        const auto upper=SaveMenu::MoveControl(m.F(src),m.F(src+4),SaveMenu::Prompt,warning_layout);
        assert(m.F(dst)==upper[0] && m.F(dst+4)==upper[1]);
        m.words[SaveMenu::Owner+0x24]=10;
        assert(SaveMenu::PromptPart(m,warning_model)==SaveMenu::Prompt);
        assert(SaveMenu::CopyPositions(m,mesh,dst,src,78*48));
        assert(m.F(dst+22*48)==old[0] && m.F(dst+22*48+4)==old[1]);
        m.words=memory;SaveMenu::SetLayout(layout,false);
    }
    {
        const auto original=m.words;const bool was_animated=SaveMenu::animate;
        SaveMenu::animate=true;
        assert(m.Read(0x41ec14)==std::bit_cast<U32>(SaveMenu::NativeBackdropAlpha));
        const float settled=1-(1-.7f)*(1-layout.darkness);
        m.words[SaveMenu::Owner+0x28]=1;
        assert(SaveMenu::BackdropOpacity(m)==0);
        SaveMenu::animate=false;assert(SaveMenu::BackdropOpacity(m)==0);
        SaveMenu::animate=true;
        m.words[SaveMenu::Owner+0x28]=2;m.words[SaveMenu::Owner+0x24]=0;
        float previous=0;
        for(unsigned tick=1;tick<=9;++tick) {
            m.words[SaveMenu::Owner+0x2c]=tick;
            const float opacity=SaveMenu::BackdropOpacity(m);
            assert(opacity>=previous && opacity<=settled);
            assert(opacity==settled*SaveMenu::CurrentAnimation(m).prompt);
            const auto geometry=PauseShadow::Panels(m,unrelated);
            if(opacity>0)assert(geometry.count==1 && geometry.colors[3]==opacity);
            previous=opacity;
        }
        assert(previous==settled);
        for(unsigned state:{1u,7u,8u,9u,10u,14u,15u,16u}) {
            m.words[SaveMenu::Owner+0x24]=state;m.words[SaveMenu::Owner+0x2c]=1;
            assert(SaveMenu::BackdropOpacity(m)==settled);
        }
        for(unsigned state:{2u}) {
            m.words[SaveMenu::Owner+0x24]=state;
            float last=settled;
            for(unsigned tick=1;tick<=9;++tick) {
                m.words[SaveMenu::Owner+0x2c]=tick;
                const float opacity=SaveMenu::BackdropOpacity(m);
                assert(opacity<=last && opacity==settled*SaveMenu::CurrentAnimation(m).prompt);
                last=opacity;
            }
            assert(last==0);
        }
        // Native state 15 animates the Save widgets out, then switches to
        // Options page 3. The backdrop must stay settled for every tick.
        assert(m.Read(0x43a1cc)==0xe3a00003 && m.Read(0x43a1d0)==0xe5850028);
        m.words[SaveMenu::Owner+0x24]=15;
        for(unsigned tick=0;tick<=9;++tick) {
            m.words[SaveMenu::Owner+0x2c]=tick;
            assert(SaveMenu::BackdropOpacity(m)==settled);
            const auto geometry=PauseShadow::Panels(m,unrelated);
            assert(geometry.count==1 && geometry.colors[3]==settled);
        }
        m.words[SaveMenu::Owner+0x28]=3;assert(SaveMenu::BackdropOpacity(m)==settled);
        m.words=original;SaveMenu::animate=was_animated;
    }
    {
        const auto original=m.words;
        SaveMenu::animate=true;SaveMenu::on_top=true;
        m.words[SaveMenu::Owner+0x28]=3;
        PauseFramerate::Hook clock;
        std::array<U32,16> regs{};std::array<U32,64> fp{};U32 flags=0;
        regs[5]=SaveMenu::Owner;
        for(U32 row=0;row<3;++row) {
            regs[0]=row;regs[15]=0x43b40c;
            clock.Handle(m,regs,fp,flags,31);
            assert(regs[15]==(row==2?0x43bbe4u:0x43b40cu));
            m.words[SaveMenu::Owner+0x30]=row;
            clock.Handle(m,regs,fp,flags,32);assert(regs[0]==4);
        }
        clock.Handle(m,regs,fp,flags,30);assert(regs[0]==0 && regs[15]==0x43b244);
        for(U32 sub:{1u,3u}) {
            m.words[SaveMenu::Owner+0x3c]=sub;
            for(U32 tick=1;tick<=9;++tick) {
                m.words[SaveMenu::Owner+0x2c]=tick;
                const auto motion=SaveMenu::CurrentAnimation(m);
                assert(motion.buttons>=0 && motion.buttons<=1);
                assert(std::abs(motion.rise-18*(1-motion.buttons))<.0001f);
                if(tick==1)assert(motion.buttons==(sub==1?0:1));
                if(tick==9)assert(motion.buttons==(sub==1?1:0));
            }
        }
        const float settled=SaveMenu::BackdropOpacity(m);
        clock.Handle(m,regs,fp,flags,33);
        assert(SaveMenu::returning_from_options && regs[0]==1);
        m.words[SaveMenu::Owner+0x28]=1;assert(SaveMenu::BackdropOpacity(m)==settled);
        m.words[SaveMenu::Owner+0x28]=2;m.words[SaveMenu::Owner+0x24]=0;
        for(U32 tick=0;tick<=9;++tick) {
            m.words[SaveMenu::Owner+0x2c]=tick;assert(SaveMenu::BackdropOpacity(m)==settled);
        }
        SaveMenu::returning_from_options=false;SaveMenu::animate=false;SaveMenu::on_top=false;
        m.words=original;
    }
    // The background is a single fullscreen dark overlay, regardless of map poses.
    auto g=PauseShadow::Panels(m,unrelated);
    assert(g.count==1 && g.positions[4]==0 && g.positions[1]==240);
    assert(g.colors[3]==layout.darkness);
    PauseMenu::Hook hook;hook.drawing=true;hook.background_index=0;
    std::array<U32,16> r{};r[15]=PauseMenu::Sites[5].pc+4;
    hook.Handle(m,r,5);assert(hook.background_index==0 && r[15]==PauseMenu::Sites[5].pc+4);
    {
        const auto before=m.words;
        const bool original_clock=SaveMenu::animate;
        SaveMenu::animate=true;
        m.words[SaveMenu::Owner+0x28]=2;m.words[SaveMenu::Owner+0x24]=11;
        const auto settled=PauseShadow::Panels(m,unrelated);
        // Native Save and Quit closes its controller before the requested
        // scene transition finishes. Preserve the dark backdrop, not the UI.
        m.words[SaveMenu::Owner+0x24]=16;m.words[SaveMenu::Owner+0x28]=0;
        m.words[0x588e3c]=1;m.words[play+0x5c2c]=20u<<8;m.words[play+0x5c74]=2u<<16;
        assert(!SaveMenu::Active(m) && SaveMenu::QuitTransition(m) && SaveMenu::BackdropActive(m));
        assert(!PauseMenu::MapPage(m) && SaveMenu::CurrentInputPage(m)==SaveMenu::InputPage::Hidden);
        auto quitting=PauseShadow::Panels(m,unrelated);
        assert(quitting.count==0 && SaveMenu::BackdropOpacity(m)==settled.colors[3]);
        // The pause clock turns off after the controller closes. This used
        // to drop composite opacity back to configured darkness during Quit.
        const bool clock=SaveMenu::animate;SaveMenu::animate=false;
        quitting=PauseShadow::Panels(m,unrelated);
        assert(quitting.count==0 && SaveMenu::BackdropOpacity(m)==settled.colors[3]);
        SaveMenu::animate=clock;
        hook.background_index=2;hook.background_copy=true;
        hook.Handle(m,r,5);assert(!hook.background_copy && hook.background_index==0);
        // No lingering backdrop when a new scene begins, the quit flag is
        // cleared, or this is a normal Save/Don't Save return to gameplay.
        for(unsigned gate=0;gate<4;++gate) {
            const auto pending=m.words;
            if(gate==0)m.words[play+0x5c2c]=0;
            if(gate==1)m.words[0x588e3c]=0;
            if(gate==2)m.words[SaveMenu::Owner+0x24]=14;
            if(gate==3)m.words[play+0x100]=0;
            assert(!SaveMenu::QuitTransition(m) && !SaveMenu::BackdropActive(m));
            m.words=pending;
        }
        m.words=before;
        SaveMenu::animate=original_clock;
    }

    // Entrance/exit uses fresh uploads, preserving user layout and native RGB.
    SaveMenu::animate=true;m.words[SaveMenu::Owner+0x24]=0;m.words[SaveMenu::Owner+0x2c]=5;
    layout[SaveMenu::Save]={40,-16,1.5f};SaveMenu::SetLayout(layout,false);
    constexpr U32 atlas_helper=0x8a00000,offsets=0x8b00000;
    m.words[SaveMenu::Owner+8]=atlas_helper;m.words[atlas_helper]=78;m.words[atlas_helper+0x1c]=offsets;
    for(unsigned q=0;q<78;++q) {
        m.words[offsets+q*8]=std::bit_cast<U32>(q>=6 && q<14?160.f:0.f);
        m.words[offsets+q*8+4]=0;
    }
    quad(6,324,96,24,40);quad(0,12,4,24,40);
    const auto motion=SaveMenu::CurrentAnimation(m);
    assert(motion.buttons==.875f && motion.prompt==.875f && motion.rise==2.25f);
    assert(SaveMenu::CopyPositions(m,mesh,dst,src,78*48));
    const auto stone=SaveMenu::MoveControl(164,96,SaveMenu::Save,layout);
    const auto prompt=SaveMenu::MoveControl(12,4,SaveMenu::Prompt,layout);
    assert(m.F(dst+6*48)==stone[0] && m.F(dst+6*48+4)==stone[1]+motion.rise);
    assert(m.F(dst)==prompt[0] && m.F(dst+4)==prompt[1]);
    // Options loses its larger native footer slide and shares the stones' rise.
    quad(18,112,236,24,32);m.words[offsets+18*8+4]=std::bit_cast<U32>(32.f);
    assert(SaveMenu::CopyPositions(m,mesh,dst,src,78*48));
    const auto options=SaveMenu::MoveControl(112,204,SaveMenu::Options,layout);
    assert(m.F(dst+18*48)==options[0] && m.F(dst+18*48+4)==options[1]+motion.rise);
    // Options rows and footer stones replace different native slide axes.
    m.words[SaveMenu::Owner+0x28]=3;m.words[SaveMenu::Owner+0x3c]=1;
    quad(43,170,25,40,32);m.words[offsets+43*8]=std::bit_cast<U32>(160.f);
    quad(64,4,236,24,32);m.words[offsets+64*8+4]=std::bit_cast<U32>(32.f);
    assert(SaveMenu::CopyPositions(m,mesh,dst,src,78*48));
    const auto row=SaveMenu::MoveControl(10,25,SaveMenu::OptionsItems,layout);
    const auto cancel=SaveMenu::MoveControl(4,204,SaveMenu::Cancel,layout);
    assert(m.F(dst+43*48)==row[0] && m.F(dst+43*48+4)==row[1]+motion.rise);
    assert(m.F(dst+64*48)==cancel[0] && m.F(dst+64*48+4)==cancel[1]+motion.rise);
    m.words[SaveMenu::Owner+0x28]=2;
    // The original labels stay parked while confirmation buttons replace them.
    m.words[SaveMenu::Owner+0x24]=10;quad(9,-156,96,24,40);
    m.words[offsets+9*8]=std::bit_cast<U32>(-320.f);
    assert(SaveMenu::CopyPositions(m,mesh,dst,src,78*48) && collapsed(9));
    m.words[SaveMenu::Owner+0x24]=0;
    constexpr U32 colors=0x8700000,output=0x8800000,prompt_mesh=0x8900000;
    for(unsigned i=0;i<78*16;++i) {
        m.words[colors+4*i]=std::bit_cast<U32>(i%4==3?.6f:.25f);m.words[output+4*i]=0;
    }
    const auto source_colors=m.words;
    assert(SaveMenu::CopyColors(m,mesh,output,colors,78*64));
    assert(m.F(output)==.25f && m.F(output+12)==.6f*.875f);
    assert(m.F(output+6*64+12)==.6f*.875f && m.F(output+15*64+12)==.6f);
    assert(m.F(output+18*64+12)==.6f*.875f && m.F(output+40*64+12)==.6f*.875f);
    for(unsigned i=0;i<78*16;++i)assert(m.words[colors+4*i]==source_colors.at(colors+4*i));
    const auto faded=m.words;
    assert(SaveMenu::CopyColors(m,mesh,output,colors,78*64) && m.words==faded);
    m.words[model+4]=prompt_mesh;
    assert(SaveMenu::CopyColors(m,prompt_mesh,output,colors,64));
    assert(m.F(output+12)==.6f*.875f); // Prompt glyphs fade with their background.
    m.words[SaveMenu::Owner+0x24]=2;
    assert(SaveMenu::CurrentAnimation(m).buttons==.125f);
    m.words[SaveMenu::Owner+0x24]=9;
    assert(SaveMenu::CurrentAnimation(m).buttons==0 && SaveMenu::CurrentAnimation(m).prompt==1);
    m.words[SaveMenu::Owner+0x28]=3;m.words[SaveMenu::Owner+0x3c]=2;
    assert(!SaveMenu::CopyColors(m,mesh,output,colors,78*64)); // Options remains untouched.
    SaveMenu::animate=false;
    layout.hide_header=true;SaveMenu::SetLayout(layout,false);
    for(U32 state:{0u,1u,2u,3u}) {
        m.words[SaveMenu::Owner+0x28]=state;
        assert(SaveMenu::HideHeader(m)==(state>=1 && state<=3));
    }
    layout.hide_header=false;SaveMenu::SetLayout(layout,false);
    m.words[SaveMenu::Owner+0x28]=3;assert(SaveMenu::HideHeader(m));
    m.words[SaveMenu::Owner+0x28]=0;assert(PauseMenu::MapPage(m));
    std::cout<<"Save menu isolation, native atlas groups, prompt models, touch mapping, dark overlay and map fallback passed\n";
}
