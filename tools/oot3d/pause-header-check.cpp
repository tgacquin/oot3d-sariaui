// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdio>
#include <cstdlib>
#include "oot3d/pause_header.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::PauseHeader;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {
        const auto i=words.find(at);return i==words.end()?std::nullopt:std::optional<U32>{i->second};
    }
    void Write(U32 at,U32 word) {assert(words.contains(at));words[at]=word;}
};
int main(int argc,char** argv) {
    assert(!DiagnosticActive());ArmDiagnostics();assert(DiagnosticActive());
    assert(DiagnosticLine() && diagnostic_lines==1);
    diagnostic_lines=2400;assert(!DiagnosticLine());
    diagnostic_end_ms=DiagnosticTime()-1;assert(!DiagnosticActive() && !DiagnosticLine());
    Memory m;
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
        for(U32 at=0x100000;file.read(reinterpret_cast<char*>(&word),4);at+=4)m.words[at]=word;
        assert(CodeMatches(m));
        for(unsigned i=0;i<FloorSites.size();++i) {
            const auto site=FloorSites[i];
            assert(Instruction(site.pc,site.instruction)==(0xef000000|(FloorFirstSVC+i)));
            assert(Instruction(site.pc,site.instruction^1)==(site.instruction^1));
            m.words[site.pc+4]^=1;assert(!CodeMatches(m));m.words[site.pc+4]^=1;
        }
        for(unsigned i=0;i<CarrySites.size();++i) {
            const auto site=CarrySites[i];
            assert(Instruction(site.pc,site.instruction)==(0xef000000|(CarryFirstSVC+i)));
            m.words[site.pc+4]^=1;assert(!CodeMatches(m));m.words[site.pc+4]^=1;
        }
        m.words[Entry+4]^=1;assert(!CodeMatches(m));m.words.clear();
    }
    Slide slide;
    for(int i=0;i<2;++i)slide.Advance(true,1.f/30);
    assert(slide.progress==1 && slide.Y()==0);
    slide.Advance(false,Duration/2);
    assert(std::abs(slide.Y()+Height/2)<.001);
    const float midway=slide.Y();slide.Advance(true,1.f/30);assert(slide.Y()>midway);
    for(int i=0;i<10;++i)slide.Advance(false,1.f/30);
    assert(slide.progress==0 && slide.Y()==-Height);
    for(float duration:{.02f,.25f,1.f,2.f})for(float fps:{30.f,60.f}) {
        Slide timed;
        const float step=1/fps;
        for(unsigned i=0;i<unsigned(std::ceil(duration*fps))+1;++i)timed.Advance(true,step,duration);
        assert(timed.progress==1);
        timed.Advance(false,step,duration);
        if(duration>step)assert(timed.progress>0 && timed.progress<1);
        const float halfway=timed.progress;
        timed.Advance(true,step,duration);assert(timed.progress>=halfway);
        for(unsigned i=0;i<unsigned(std::ceil(duration*fps))+1;++i)timed.Advance(false,step,duration);
        assert(timed.progress==0);
    }
    assert(PauseMenu::TransitionSeconds(NAN)==Duration);
    assert(PauseMenu::TransitionSeconds(0)==.02f && PauseMenu::TransitionSeconds(100)==2.f);
    for(bool wide:{false,true}) {
        const auto v=Vertices(0,wide);
        const float width=wide?240*Widescreen::Aspect:400;
        assert(std::abs(v[30]-v[0]-width)<.001 && v[1]==Height && v[4]==0);
        assert(v[0]+v[30]==400 && v[3]==v[0] && v[9]==v[6]);
    }
    const std::array<float,8> atlas{.75f,1,1,1,.75f,.0625f,1,.0625f};
    for(bool wide:{false,true}) {
        const auto v=Vertices(0,wide);const auto uv=TileUVs(atlas,wide);
        for(unsigned q=0;q<Quads;++q) {
            const float width=v[q*12+6]-v[q*12];
            // Bilinear footprints stay inside the native 64x240 crop.
            for(unsigned i=0;i<4;++i) {
                const float u=uv[q*8+i*2]*256-.5f;
                const float t=uv[q*8+i*2+1]*256-.5f;
                assert(u>=192 && u<=255 && t>=16 && t<=255);
            }
            assert(uv[q*8+1]==255.5f/256);
            if(std::abs(width-TileWidth)<.001f)assert(uv[q*8+5]==16.5f/256);
            else assert(std::abs(uv[q*8+5]-(1-(1.f-.0625f)*width/TileWidth))<.00001f);
            if(q)assert(v[q*12]==v[(q-1)*12+6]); // Adjacent tiles, no gaps/overlaps.
        }
    }
    PauseMenu::Layout layout{};layout.header_y=60;layout[PauseMenu::Titles]={37,85,1.5f};
    assert(std::abs(SlotsTravel(layout)-240.4f)<.001f);PauseMenu::SetLayout(layout,false);
    assert(Travel(layout)==133.f); // Header/title bounds no longer include slots.
    {
        PauseMenu::Layout captured;captured.horizontal=true;captured.header_y=-7;
        captured[PauseMenu::Titles]={-165.1f,-7,.82f};captured[PauseMenu::Slots]={-241,-84.9f,.79f};
        captured.transition_seconds=.101f;
        assert(std::abs(SlotsTravel(captured)-78.38f)<.001f && Travel(captured)==41.f);
        for(bool opening:{true,false}) {
            Slide motion;motion.progress=opening?0:1;
            unsigned old_visible=0,new_visible=0;
            for(unsigned frame=0;frame<6;++frame) {
                motion.Advance(opening,1.f/60,captured.transition_seconds);
                old_visible+=motion.Y(SlotsTravel(captured))+captured.header_y+Height>0;
                new_visible+=motion.Y(Travel(captured))+captured.header_y+Height>0;
            }
            assert(new_visible>old_visible);
        }
    }
    constexpr U32 source=0x8000000,mesh=0x8010000,helper=0x8020000,uv=0x8030000;
    constexpr U32 material=0x8040000,play=0x8050000,block=0x8060000,model=0x8070000,node=0x8080000;
    for(U32 i=0;i<0x1e4;i+=4)m.words[source+i]=0;
    for(U32 i=0;i<0x1b8;i+=4)m.words[mesh+i]=0;
    for(U32 i=0;i<0x1e4;i+=4)m.words[model+i]=0;
    for(U32 i=0;i<AllocationSize;i+=4)m.words[block+i]=0;
    m.words[source+0x174]=222;m.words[source]=0x4ebd60;m.words[source+4]=mesh;m.words[source+8]=0x8090000;m.words[source+0x1dc]=material;
    m.words[mesh+0x158]=0x8091000;m.words[mesh]=mesh+4;m.words[Items+0x20]=source;m.words[Items+0x24]=helper;
    m.words[helper]=37;m.words[helper+0x14]=uv;
    for(U32 i=0;i<8;++i)m.words[uv+12*32+i*4]=std::bit_cast<U32>(atlas[i]);
    m.words[0x54ac4c]=0x01000000;m.words[PauseMenu::UI+0xc]=play;m.words[play+0x100]=3;
    m.words[PauseMenu::UI+0x14]=1;m.words[PauseMenu::UI+0x20]=0;m.words[PauseMenu::UI+0x24]=1;
    Hook hook;
    // The map's backdrop must not precede its contents. Native preparation
    // has pause active with the open byte still zero, so Closing alone cannot
    // distinguish these opening frames from a real exit.
    {
        const auto original=m.words;
        m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=0;
        m.words[0x4fda6c+0x50]=0;m.words[0x54ac4c]=0;
        assert(Instruction(BackdropSite.pc,BackdropSite.instruction)==(0xef000000|BackdropSVC));
        for(U32 dungeon:{0u,1u})for(U32 controller:{0u,12u,16u,20u,21u})for(U32 map_state:{0u,1u,2u,7u,8u,9u,10u}) {
            m.words[0x4fda6c+0x50]=dungeon;
            m.words[0x50af34+0x34]=controller;m.words[0x506cb0+0x38]=map_state;
            std::array<U32,16> r{};r.fill(123);r[15]=BackdropSite.pc+4;
            auto expected=r;expected[14]=BackdropSite.pc+4;
            if(controller!=20 || map_state!=0)expected[15]=0x3339e8;
            hook.Backdrop(m,r,true);assert(r==expected);
        }
        for(unsigned exclusion=0;exclusion<4;++exclusion) {
            m.words[0x50af34+0x34]=20;m.words[0x506cb0+0x38]=0;
            m.words[0x5066f8+0x34]=exclusion==1?2:0;
            m.words[0x50446c+0x18]=exclusion==2?2:0;
            m.words[0x4fda6c+0x50]=0;
            m.words[PauseMenu::UI+0x14]=exclusion==3?0:1;
            std::array<U32,16> r{};r[15]=BackdropSite.pc+4;
            hook.Backdrop(m,r,exclusion!=0);
            assert(r[14]==BackdropSite.pc+4 && r[15]==0x3339e8);
        }
        m.words=original;
    }
    {
        const auto original=m.words;const bool was_animated=SaveMenu::animate;
        SaveMenu::animate=true;
        for(unsigned page:{1u,2u,3u})for(bool supported:{false,true}) {
            m.words[SaveMenu::Owner+0x28]=page;
            std::array<U32,16> r{};r[15]=BackdropSite.pc+4;
            hook.Backdrop(m,r,supported);
            assert(r[14]==BackdropSite.pc+4);
            assert(r[15]==(supported?BackdropSite.pc+4:0x3339e8));
        }
        SaveMenu::animate=false;
        std::array<U32,16> r{};r[15]=BackdropSite.pc+4;hook.Backdrop(m,r,true);
        assert(r[15]==0x3339e8); // Native-rate fallback keeps its own tint.
        m.words[SaveMenu::Owner+0x28]=1;
        r[15]=BackdropSite.pc+4;hook.Backdrop(m,r,true);
        assert(r[15]==BackdropSite.pc+4); // Preparation precedes the fast-clock latch.
        // Closing Save turns its clock off before the return-to-title fade.
        // The retained composite backdrop still replaces the native tint.
        constexpr U32 quit_play=0x8000000;
        m.words[0x5043e0]=quit_play;m.words[quit_play+0x100]=0x203;
        m.words[SaveMenu::Owner+0x28]=0;m.words[SaveMenu::Owner+0x24]=16;
        m.words[0x588e3c]=1;m.words[quit_play+0x5c2c]=20u<<8;
        m.words[quit_play+0x5c74]=2u<<16;
        assert(SaveMenu::QuitTransition(m));
        r[15]=BackdropSite.pc+4;hook.Backdrop(m,r,true);assert(r[15]==BackdropSite.pc+4);
        r[15]=BackdropSite.pc+4;hook.Backdrop(m,r,false);assert(r[15]==0x3339e8);
        m.words=original;SaveMenu::animate=was_animated;
    }
    // Captured Save flash: pause is already active for four draws before
    // Save+28 becomes 2. Main state14 is waiting to call native Save setup.
    {
        const auto original=m.words;const bool was_animated=SaveMenu::animate;
        m.words[0x54ac4c]=0;m.words[SaveMenu::Owner+0x28]=0;
        m.words[SaveMenu::Owner+0x24]=2;m.words[SaveMenu::Owner+0x2c]=9;
        m.words[0x50af34+0x34]=14;
        for(unsigned frame=0;frame<4;++frame) {
            SaveMenu::animate=frame!=0;
            std::array<U32,16> r{};r[15]=BackdropSite.pc+4;
            hook.Backdrop(m,r,true);assert(r[15]==BackdropSite.pc+4);
            assert(!SaveMenu::BackdropActive(m)); // No custom opacity yet either.
        }
        m.words[SaveMenu::Owner+0x28]=2;m.words[SaveMenu::Owner+0x24]=0;
        m.words[SaveMenu::Owner+0x2c]=0;m.words[0x50af34+0x34]=0;
        float previous=0;
        for(unsigned tick=0;tick<=9;++tick) {
            m.words[SaveMenu::Owner+0x2c]=tick;
            std::array<U32,16> r{};r[15]=BackdropSite.pc+4;
            hook.Backdrop(m,r,true);assert(r[15]==BackdropSite.pc+4);
            const float alpha=SaveMenu::BackdropOpacity(m);
            assert(alpha>=previous);previous=alpha;
        }
        m.words[SaveMenu::Owner+0x28]=0;
        for(unsigned controller:{12u,13u,15u,16u,17u,21u}) {
            m.words[0x50af34+0x34]=controller;
            std::array<U32,16> r{};r[15]=BackdropSite.pc+4;
            hook.Backdrop(m,r,true);assert(r[15]==0x3339e8);
        }
        m.words[0x50af34+0x34]=14;
        std::array<U32,16> r{};r[15]=BackdropSite.pc+4;
        hook.Backdrop(m,r,false);assert(r[15]==0x3339e8);
        m.words[PauseMenu::UI+0x14]=0;r[15]=BackdropSite.pc+4;
        hook.Backdrop(m,r,true);assert(r[15]==0x3339e8);
        m.words=original;SaveMenu::animate=was_animated;
    }
    // Captured failure: state 9 consumes the entire entrance before state 1
    // creates the banner. Prepare native models on the first state-9 update.
    {
        const auto original=m.words;
        m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=0;
        m.words[0x4fda6c+0x50]=0;m.words[0x506cb0+0x38]=9;
        m.words[MapLocation::Owner+0x38]=0;m.words[play+0x104]=0x51;
        m.words[0x42d658]=std::bit_cast<U32>(0.f);
        assert(Instruction(LocationSetupSite.pc,LocationSetupSite.instruction)==(0xef000000|LocationSetupSVC));
        std::array<U32,16> r{};std::array<U32,64> f{};
        r.fill(123);f.fill(456);r[6]=play;r[15]=LocationSetupSite.pc+4;
        const auto before=r;auto expected_f=f;expected_f[16]=0;
        Hook opening;
        opening.PrepareLocation(m,r,f,true);
        assert(opening.location_setup_pending && r[15]==0x441668 && r[0]==play && r[14]==LocationSetupSite.pc);
        // Emulate native setup returning, with caller-saved registers changed.
        m.words[MapLocation::Owner+0x38]=0x8092000;r.fill(789);f.fill(987);
        opening.PrepareLocation(m,r,f,true);
        assert(!opening.location_setup_pending && r==before && f==expected_f);
        opening.PrepareLocation(m,r,f,true);assert(r==before && !opening.location_setup_pending);
        for(unsigned exclusion=0;exclusion<6;++exclusion) {
            m.words[MapLocation::Owner+0x38]=0;m.words[play+0x104]=0x51;
            m.words[0x4fda6c+0x50]=0;m.words[0x506cb0+0x38]=9;
            m.words[0x54ac4c]=0x01000000;m.words[0x5066f8+0x34]=0;
            if(exclusion==1)m.words[play+0x104]=0x50;
            if(exclusion==2)m.words[0x4fda6c+0x50]=1;
            if(exclusion==3)m.words[0x506cb0+0x38]=1;
            if(exclusion==4)m.words[0x54ac4c]=0;
            if(exclusion==5)m.words[0x5066f8+0x34]=2;
            r=before;f.fill(456);opening.PrepareLocation(m,r,f,exclusion!=0);
            assert(!opening.location_setup_pending && r==before && f==expected_f);
        }
        m.words=original;
    }
    // Every page keeps submitting its models throughout a slow close, then
    // takes its own native cleanup path. Unrelated calls keep their original flow.
    {
        Hook closing;closing.slide.progress=.5f;
        m.words[0x54ac4c]=0;m.words[0x42d658]=0;
        std::array<U32,16> r{};std::array<U32,64> f{};
        for(unsigned site=0;site<CloseSites.size();++site) {
            r.fill(123);f.fill(456);r[15]=CloseSites[site].pc+4;
            const auto before=r;
            closing.CloseStep(m,r,f,site,true);
            assert(r[15]==(site==3?0x42d678:CloseSites[site].pc+8));
            if(site!=3)assert(r[0]==0);else assert(f[16]==0);
            for(unsigned i=1;i<15;++i)assert(r[i]==before[i]);
            closing.slide.progress=0;
            closing.CloseStep(m,r,f,site,true);
            if(site!=3)assert(r[0]==1);else assert(r[15]==0x42d5b8);
            closing.slide.progress=.5f;
            r=before;closing.CloseStep(m,r,f,site,false);assert(r==before);
            m.words[0x54ac4c]=0x01000000;
            closing.CloseStep(m,r,f,site,true);assert(r==before);
            m.words[0x54ac4c]=0;
        }
        m.words[0x54ac4c]=0x01000000;
    }
    hook.Update(m,100000,true);assert(hook.slide.progress>0);
    hook.Update(m,133334,true);hook.Update(m,166668,true);assert(hook.slide.progress==1);
    m.words[0x54ac4c]=0;hook.Update(m,200000,true);
    assert(hook.offset<0 && PauseMenu::Active(m)); // Close starts while other widgets remain active.
    hook.Update(m,250000,true);hook.Update(m,266670,true);assert(hook.slide.progress==0);
    m.words[0x54ac4c]=0x01000000;hook.Update(m,300000,true);assert(hook.slide.progress>0);
    hook.Update(m,350000,false);assert(!hook.presenting && hook.slide.progress==0);
    PauseMenu::SetLayout({},false);hook.Update(m,400000,true);
    std::array<U32,16> regs{};
    for(U32 i=0;i<regs.size();++i)regs[i]=i*100;
    regs[15]=Entry+4;const auto saved=regs;const auto original=m.words;
    hook.Handle(m,regs,true);
    assert(regs[15]==0x313ce0 && regs[0]==AllocationSize && regs[14]==Entry);
    regs[0]=block;hook.Handle(m,regs,true);
    assert(regs[15]==0x348f34 && regs[0]==block && regs[1]==block+Descriptor);
    assert(m.words.at(block+Descriptor+0xc)==Quads*4 && m.words.at(block+Descriptor+0x14)==Quads*6);
    const auto safe_uv=TileUVs(atlas,Widescreen::Active());
    for(U32 i=0;i<safe_uv.size();++i)
        assert(m.words.at(block+UVs+i*4)==std::bit_cast<U32>(safe_uv[i]));
    m.words[block+0x128]=2;
    hook.Handle(m,regs,true);
    assert(regs[15]==0x34897c && regs[1]==block && regs[2]==material && regs[3]==0);
    assert(m.words.at(block+0x158)==m.words.at(mesh+0x158));
    m.words[model]=0x4ebd60;m.words[model+8]=node;m.words[node+0x358]=material;regs[0]=model;hook.Handle(m,regs,true);
    assert(regs[15]==0x36759c && regs[0]==block && regs[1]==PositionBytes && regs[2]==block+Positions);
    assert(hook.Owned(m,source) && m.words.at(model+8)==node && m.words.at(model+0x174)==Quads*6);
    hook.Handle(m,regs,true);assert(regs[15]==0x317d1c && regs[1]==UVBytes);
    hook.Handle(m,regs,true);assert(regs[15]==0x2f9934 && regs[1]==Quads*64);
    hook.Handle(m,regs,true);assert(regs[15]==0x3f9680 && regs[0]==model && m.words.at(block+0x124)==1);
    hook.Handle(m,regs,true);assert(regs==saved);
    for(const auto& [at,word]:original)
        if((at<block || at>=block+AllocationSize) && (at<model || at>=model+0x1e4))assert(m.words.at(at)==word);
    auto tinted=PauseMenu::GetLayout();tinted.header_overlay={{.1f,.8f,.3f},.75f};
    PauseMenu::SetLayout(tinted,false);
    hook.SetVertices(m);
    const auto expected=PauseMenu::OverlayTint(tinted.header_overlay);
    for(unsigned i=0;i<Quads*16;++i)
        assert(std::bit_cast<float>(m.words.at(block+Colors+4*i))==(i%4==3?1.f:expected[i%4]));
    auto split=tinted;
    split.color_layouts.Initialize(tinted.header_overlay,tinted.slots_overlay);
    split.color_layouts.working[0]={{1,0,0},1};
    split.color_layouts.working[1]={{0,1,0},.5f};
    split.color_layouts.working[2]={{0,0,1},.75f};
    const auto old_items=m.Read(Items+0x34),old_gear=m.Read(0x50446c+0x18);
    PauseMenu::SetLayout(split,false);
    for(unsigned page=0;page<3;++page) {
        m.words[Items+0x34]=page==1?2:0;m.words[0x50446c+0x18]=page==2?2:0;
        hook.SetVertices(m);
        const auto tint=PauseMenu::OverlayTint(split.color_layouts.working[page]);
        for(unsigned i=0;i<Quads*16;++i)
            assert(std::bit_cast<float>(m.words.at(block+Colors+4*i))==(i%4==3?1.f:tint[i%4]));
    }
    if(old_items)m.words[Items+0x34]=*old_items;else m.words.erase(Items+0x34);
    if(old_gear)m.words[0x50446c+0x18]=*old_gear;else m.words.erase(0x50446c+0x18);
    PauseMenu::SetLayout(tinted,false);
    // Reuse the independent mesh, without allocating on every frame or touching the source.
    hook.Handle(m,regs,true);assert(regs[15]==0x36759c);
    hook.Handle(m,regs,true);hook.Handle(m,regs,true);hook.Handle(m,regs,true);
    m.words[BannerOwner+0x28]=source;hook.Handle(m,regs,true);
    assert(hook.stage==Hook::Stage::Banner && regs[0]==source && regs[15]==0x3f9680);
    hook.NativeDraw(m,regs,true);assert(regs[15]==0x3f9680); // Our late banner draw is allowed.
    hook.Handle(m,regs,true);assert(regs==saved && m.words.at(block+0x124)==0);
    regs[0]=source;regs[14]=0x123456;hook.NativeDraw(m,regs,true);assert(regs[15]==0x123456);
    regs=saved;
    // Actual location owners: preview+4, parchment+0x10, localized name+4.
    // They must be deferred after the custom rail AND the Gear/Map/Items title.
    constexpr U32 preview=0x8100000,location=0x8110000;
    constexpr U32 preview_model=0x8120000,paper_model=0x8130000,name_model=0x8140000;
    m.words[MapLocation::Owner+0x3c]=preview;m.words[preview+4]=preview_model;
    m.words[MapLocation::Owner+0x38]=location;m.words[location+0x10]=paper_model;
    m.words[location+4]=name_model;
    for(U32 model:{preview_model,paper_model,name_model})m.words[model]=0x4ebd60;
    m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=0;
    assert(MapLocation::Select(m,preview_model)==PauseMenu::LocationMap);
    assert(MapLocation::Select(m,paper_model)==PauseMenu::LocationBanner);
    assert(MapLocation::Select(m,name_model)==PauseMenu::LocationBanner);
    assert(!MapLocation::Select(m,source));
    for(U32 model:{preview_model,paper_model,name_model}) {
        regs[0]=model;regs[14]=0x123456;regs[15]=0x3f9684;
        hook.NativeDraw(m,regs,true);assert(regs[15]==regs[14]);
    }
    assert(hook.foreground_count==3);
    regs=saved;hook.Handle(m,regs,true);
    for(int i=0;i<4;++i)hook.Handle(m,regs,true);
    assert(hook.stage==Hook::Stage::Banner);
    for(U32 model:{preview_model,paper_model,name_model}) {
        hook.Handle(m,regs,true);
        assert(hook.stage==Hook::Stage::Foreground && regs[0]==model && regs[15]==0x3f9680);
        hook.NativeDraw(m,regs,true);assert(regs[15]==0x3f9680);
    }
    hook.Handle(m,regs,true);
    assert(regs==saved && hook.stage==Hook::Stage::Idle && hook.foreground_count==0);
    // Uniform scale in displayed pixels, despite the routed 320-wide projection.
    PauseMenu::Layout locations;
    locations[PauseMenu::LocationMap]={40,-20,1.5f};
    locations[PauseMenu::LocationBanner]={-32,28,.75f};
    locations[PauseMenu::Map]={300,200,3};
    for(auto part:{PauseMenu::LocationMap,PauseMenu::LocationBanner})for(bool bottom:{false,true}) {
        const auto t=MapLocation::Transform(part,locations,bottom);
        const auto p=locations[part];const auto a=PauseMenu::Anchors[part];
        const float aspect=bottom?.8f:1;
        assert(t.sx==p.scale*aspect && t.sy==p.scale);
        assert(std::abs((a[0]+t.x)/aspect-(a[0]+p.x))<.0001f);
        assert(a[1]+t.y==a[1]+p.y);
    }
    // Opening directly from gameplay reveals overworld art at rest from its
    // first frame, while the header still animates. Dungeon contents also
    // stay still; only the page title and temple-name banner move upward.
    {
        const auto original=m.words;
        m.words[0x54ac4c]=0x01000000;m.words[0x506cb0+0x38]=1;
        for(unsigned page=0;page<4;++page) {
            m.words[0x4fda6c+0x50]=page==1?1:0;
            m.words[0x5066f8+0x34]=page==2?13:0;
            m.words[0x50446c+0x18]=page==3?9:0;
            Hook opening;opening.last_time=1000000;
            for(unsigned frame=1;frame<=2;++frame) {
                opening.Update(m,1000000+frame*16667,true);
                assert(opening.ShowContents() && opening.slide.progress>0 && opening.slide.progress<1);
                assert(PauseMenu::motion_y<0 && !PauseMenu::world_exit_hold);
                if(page==0) {
                    assert(PauseMenu::motion_x==0 && PauseMenu::world_location_alpha<1);
                    assert(PauseMenu::world_location_y<0);
                    assert(PauseMenu::world_panel_y>0);
                    assert(PauseMenu::world_location_alpha>0);
                    assert(std::abs(PauseMenu::world_location_y-opening.slide.Y(16))<.00001f);
                    assert(PauseMenu::Motion(PauseMenu::Map)==(std::array<float,2>{0,0}));
                    assert(PauseMenu::Motion(PauseMenu::Background)==(std::array<float,2>{0,0}));
                } else if(page==1) {
                    assert(PauseMenu::motion_x==0 && PauseMenu::motion_bottom==0);
                    for(auto part:{PauseMenu::DungeonMap,PauseMenu::DungeonContents,
                        PauseMenu::DungeonLeft,PauseMenu::DungeonRight,PauseMenu::DungeonItems,
                        PauseMenu::DungeonFloors,PauseMenu::Background})
                        assert(PauseMenu::Motion(part)==(std::array<float,2>{0,0}));
                    assert(PauseMenu::Motion(PauseMenu::DungeonBanner)[1]==PauseMenu::motion_y);
                } else assert(PauseMenu::motion_x<0);
            }
        }
        m.words=original;
    }
    // Location widgets share the green header's clock, including when their
    // models arrive late. Tab changes never start a new entrance, even during
    // a slow pause opening; closing after a tab change still animates.
    {
        const auto original=m.words;
        m.words[0x54ac4c]=0x01000000;m.words[0x506cb0+0x38]=1;m.words[0x4fda6c+0x50]=0;
        auto slow=locations;slow.transition_seconds=.25f;PauseMenu::SetLayout(slow,false);
        Hook opening;opening.last_time=1000000;
        m.words[location+4]=0;
        float previous_alpha=0;
        constexpr U32 material_alpha=0x8320000;
        for(unsigned frame=1;frame<=15;++frame) {
            if(frame==2)m.words[location+4]=name_model;
            opening.Update(m,1000000+frame*16667,true);
            const float alpha=PauseMenu::world_location_alpha,y=PauseMenu::world_location_y;
            assert(alpha>previous_alpha && alpha<=1);
            assert(std::abs(y-opening.slide.Y(16))<.00001f);
            assert(PauseMenu::world_panel_y==-y);
            assert(PauseMenu::motion_x==0);
            assert(MapLocation::Transform(PauseMenu::LocationMap,locations,false).y==-20);
            if(frame>=2)for(U32 model:{paper_model,name_model}) {
                m.words[material_alpha]=std::bit_cast<U32>(.6f);
                assert(MapLocation::Material(m,model,material_alpha)==(alpha!=1));
                assert(std::abs(std::bit_cast<float>(m.words[material_alpha])-.6f*alpha)<.00001f);
            }
            assert(!MapLocation::Material(m,preview_model,material_alpha));
            previous_alpha=alpha;
        }
        assert(PauseMenu::world_location_alpha==1 && PauseMenu::world_location_y==0);
        assert(PauseMenu::world_panel_y==0);
        // Clock tint reaches both current-location title models while their
        // material alpha still follows the banner fade. Preview stays native.
        {
            const auto before_tint=m.words;
            auto tinted=slow;tinted.world_map_time_tint=true;PauseMenu::SetLayout(tinted,false);
            m.words[0x4596a0]=0x587958;m.words[0x4594e4]=0xe1d600bc;
            for(unsigned time:{0u,0x8000u,0xb555u})for(float fade:{1.f,.25f}) {
                m.words[0x587964]=time;PauseMenu::world_location_alpha=fade;
                const auto tint=MapTimeTint::At(time);
                for(U32 model:{paper_model,name_model,preview_model}) {
                    for(unsigned c=0;c<3;++c)m.words[material_alpha-12+c*4]=std::bit_cast<U32>(.8f);
                    m.words[material_alpha]=std::bit_cast<U32>(.6f);
                    const bool banner=model!=preview_model;
                    assert(MapLocation::Material(m,model,material_alpha)==
                        (banner && (fade!=1 || tint!=MapTimeTint::Neutral)));
                    for(unsigned c=0;c<3;++c)
                        assert(std::abs(std::bit_cast<float>(m.words[material_alpha-12+c*4])-
                            .8f*(banner?std::lerp(1.f,tint[c],.5f):1.f))<.00001f);
                    assert(std::abs(std::bit_cast<float>(m.words[material_alpha])-
                        .6f*(banner?fade:1.f))<.00001f);
                }
            }
            m.words=before_tint;PauseMenu::world_location_alpha=1;PauseMenu::SetLayout(slow,false);
        }
        // Leave and return after settling: no second entrance.
        m.words[0x5066f8+0x34]=13;opening.Update(m,1266667,true);
        m.words[0x5066f8+0x34]=0;opening.Update(m,1283334,true);
        assert(!opening.map_open_entrance && PauseMenu::world_location_alpha==1);
        assert(PauseMenu::world_location_y==0 && PauseMenu::world_panel_y==0);
        m.words[0x54ac4c]=0;opening.Update(m,1300001,true);
        assert(PauseMenu::world_location_alpha>0 && PauseMenu::world_location_alpha<1);
        assert(PauseMenu::world_location_y<0 && PauseMenu::world_panel_y<0);
        // Also skip the entrance when changing tabs before the header settles.
        m.words[0x54ac4c]=0x01000000;m.words[0x5066f8+0x34]=13;
        Hook tabs;tabs.last_time=2000000;tabs.Update(m,2016667,true);
        m.words[0x5066f8+0x34]=0;tabs.Update(m,2033334,true);
        assert(tabs.slide.progress<1 && !tabs.map_open_entrance);
        assert(PauseMenu::world_location_alpha==1 && PauseMenu::world_location_y==0 && PauseMenu::world_panel_y==0);
        // A later pause session on the map animates again.
        m.words[PauseMenu::UI+0x14]=0;tabs.Update(m,2050001,true);
        m.words[PauseMenu::UI+0x14]=1;tabs.Update(m,2066668,true);
        assert(tabs.map_open_entrance && PauseMenu::world_location_alpha>0 && PauseMenu::world_location_alpha<1);
        const float partial=PauseMenu::world_location_alpha;
        m.words[0x54ac4c]=0;tabs.Update(m,2083335,true);
        assert(PauseMenu::world_location_alpha<=partial);
        PauseMenu::SetLayout(locations,false);m.words=original;
    }
    // Overworld exit holds map/parchments still, raises/fades only location
    // banners, and hands control back on the unchanged shared close deadline.
    {
        const auto original=m.words;
        Hook close;close.slide.progress=1;close.last_time=1000000;
        m.words[0x54ac4c]=0;m.words[0x506cb0+0x38]=1;m.words[0x4fda6c+0x50]=0;
        close.Update(m,1016667,true);
        assert(close.slide.progress>0 && close.slide.progress<1);
        assert(PauseMenu::motion_x==0 && PauseMenu::motion_y<0);
        assert(PauseMenu::world_location_y<0 && PauseMenu::world_location_y>-16);
        const float alpha=PauseMenu::world_location_alpha;
        assert(alpha>0 && alpha<1);
        assert(PauseMenu::Motion(PauseMenu::Map)==(std::array<float,2>{0,0}));
        assert(close.ForegroundModel(m,preview_model) && close.ForegroundModel(m,paper_model));
        assert(MapLocation::Transform(PauseMenu::LocationMap,locations,false).y==-20);
        assert(MapLocation::Transform(PauseMenu::LocationBanner,locations,false).y<28);
        constexpr U32 material_alpha=0x8320000;
        const auto fresh_alpha=std::bit_cast<U32>(.6f);
        for(unsigned frame=0;frame<2;++frame) {
            m.words[material_alpha]=fresh_alpha;
            assert(MapLocation::Material(m,paper_model,material_alpha));
            assert(std::abs(std::bit_cast<float>(m.words[material_alpha])-.6f*alpha)<.00001f);
        }
        assert(!MapLocation::Material(m,preview_model,material_alpha));
        assert(!MapLocation::Material(m,source,material_alpha));
        close.Update(m,1200000,true);
        assert(close.slide.progress==0 && !close.ForegroundModel(m,paper_model));
        assert(PauseMenu::motion_x==0 && PauseMenu::world_location_alpha==0);
        assert(close.ShowContents() && PauseMenu::world_exit_hold);
        // Native cleanup hides the map controller before the unpause phase
        // finishes. Keep uploading/drawing its art without changing that state.
        m.words[0x506cb0+0x38]=0;m.words[0x50af34+0x34]=21;
        close.Update(m,1210000,true);
        assert(close.ShowContents() && PauseMenu::motion_x==0);
        std::array<U32,16> carry{};carry[4]=0x506cb0;
        const auto before_carry=m.words;
        close.CarryMap(m,carry,true);assert(carry[0]==1 && m.words==before_carry);
        close.CarryMap(m,carry,false);assert(carry[0]==0 && m.words==before_carry);
        m.words[PauseMenu::UI+0x14]=0;close.Update(m,1212000,true);
        assert(!close.ShowContents() && !PauseMenu::world_exit_hold);
        close.CarryMap(m,carry,true);assert(carry[0]==0);
        m.words[PauseMenu::UI+0x14]=1;m.words[0x506cb0+0x38]=1;
        close.slide.progress=1;close.last_time=1200000;m.words[0x4fda6c+0x50]=1;
        close.Update(m,1216667,true);
        assert(PauseMenu::motion_x==0 && PauseMenu::world_location_y<0 &&
               PauseMenu::world_location_alpha>0 && PauseMenu::world_location_alpha<1);
        assert(!close.ForegroundModel(m,paper_model) && !PauseMenu::world_exit_hold);
        assert(PauseMenu::dungeon_exit_hold && PauseMenu::DungeonPage(m));
        close.Update(m,1266667,true);assert(close.slide.progress==0);
        // Keep routing the dungeon art through native cleanup, so there is
        // never a gray-only gap before gameplay actually resumes.
        m.words[0x506cb0+0x38]=0;close.Update(m,1283334,true);
        assert(!PauseMenu::world_exit_hold && PauseMenu::dungeon_exit_hold);
        assert(close.ShowContents() && PauseMenu::DungeonPage(m));
        const auto dungeon_before=m.words;
        close.CarryMap(m,carry,true);assert(carry[0]==1 && m.words==dungeon_before);
        // Cleanup and both dungeon update branches try to restore Link's
        // floor while the final pause image is still being carried on-screen.
        std::array<U32,16> floor_regs{};floor_regs[1]=floor_regs[6]=0x4fda6c;
        for(U32 selected:{0u,2u,7u})for(unsigned site=0;site<FloorSites.size();++site) {
            m.words[0x4fda6c+0x38]=selected;floor_regs[7]=floor_regs[0]=(selected+1)%8;
            const auto held=m.words;const auto saved_regs=floor_regs;
            close.FloorStep(m,floor_regs,site,true);
            assert(m.words==held && floor_regs==saved_regs);
            PauseMenu::dungeon_exit_hold=false;
            close.FloorStep(m,floor_regs,site,true);
            assert(m.words==held && floor_regs==saved_regs);
            PauseMenu::dungeon_exit_hold=true;
            close.FloorStep(m,floor_regs,site,false);
            assert(m.words[0x4fda6c+0x38]==floor_regs[0]);
            m.words[0x4fda6c+0x38]=selected;
            const auto closed=m.words[0x54ac4c];m.words[0x54ac4c]=0x01000000;
            close.FloorStep(m,floor_regs,site,true);
            assert(m.words[0x4fda6c+0x38]==floor_regs[0]); // Normal floor selection remains live.
            m.words[0x54ac4c]=closed;
        }
        // Real 42c0a4 order: clear map type, write actual floor, restore
        // dungeon type. The old test kept +50=1 and missed both writes.
        for(unsigned site:{1u,2u})for(U32 selected:{0u,2u,7u}) {
            m.words[0x4fda6c+0x38]=selected;
            m.words[0x4fda6c+0x50]=0;
            floor_regs[0]=(selected+1)%8;
            close.FloorStep(m,floor_regs,site,true);
            assert(m.words[0x4fda6c+0x38]==selected);
            m.words[0x4fda6c+0x50]=1;
        }
        // This does not freeze user navigation or an ordinary outdoor update.
        m.words[0x4fda6c+0x50]=0;m.words[0x4fda6c+0x38]=2;floor_regs[7]=0;
        close.FloorStep(m,floor_regs,3,true);assert(m.words[0x4fda6c+0x38]==0);
        m.words[0x4fda6c+0x50]=1;
        m.words[PauseMenu::UI+0x14]=0;close.Update(m,1300001,true);
        for(unsigned site=0;site<FloorSites.size();++site) {
            m.words[0x4fda6c+0x38]=2;floor_regs[7]=floor_regs[0]=0;
            close.FloorStep(m,floor_regs,site,true);
            assert(m.words[0x4fda6c+0x38]==0); // Gameplay uses the actual floor immediately.
        }
        assert(!close.ShowContents() && !PauseMenu::dungeon_exit_hold && !PauseMenu::DungeonPage(m));
        assert(!close.presenting && PauseMenu::world_location_y==0 && PauseMenu::world_location_alpha==1);
        m.words=original;
    }
    // Neither a closing map nor a different pause page may fall back to an
    // untransformed native draw, even after our slide has finished.
    const auto before_map_close=m.words;
    m.words[0x54ac4c]=0;
    hook.Update(m,433334,true);hook.Update(m,483334,true);
    assert(hook.slide.progress==0 && PauseMenu::Active(m));
    for(U32 model:{preview_model,paper_model,name_model}) {
        regs[0]=model;regs[14]=0x123456;regs[15]=0x3f9684;
        hook.NativeDraw(m,regs,true);assert(regs[15]==regs[14]);
        assert(!hook.ForegroundModel(m,model) && hook.foreground_count==0);
        regs[15]=0x3f9684;
        hook.NativeDraw(m,regs,false);assert(regs[15]==0x3f9684);
        m.words[PauseMenu::UI+0x14]=0;
        hook.NativeDraw(m,regs,true);assert(regs[15]==0x3f9684);
        m.words[PauseMenu::UI+0x14]=1;
    }
    m.words=before_map_close;hook.Update(m,516668,true);
    assert(hook.ForegroundModel(m,preview_model));
    m.words[0x5066f8+0x34]=1;regs[0]=preview_model;regs[15]=0x3f9680;
    hook.NativeDraw(m,regs,true);assert(regs[15]==regs[14] && hook.foreground_count==0);
    m.words[0x5066f8+0x34]=0;
    regs=saved;
    // Options help (plate and lettering) is submitted before the routed pause
    // pass, but must be replayed after its dark overlay and controls even
    // though the Options header is now suppressed.
    constexpr U32 help_plate=0x8150000,help_text=0x8160000,pane=0x8170000,text=0x8180000;
    m.words[SaveMenu::Owner+0x28]=3;
    m.words[ScreenModels::Message]=0x4ed0a0;m.words[ScreenModels::Message+0xf38]=5;
    m.words[ScreenModels::Message+0x56c]=pane;
    m.words[pane]=0x4ed234;m.words[pane+4]=ScreenModels::Message+0x168;
    m.words[pane+0x64]=help_plate;m.words[help_plate]=0x4ebd60;
    m.words[ScreenModels::Message+0x974]=text;m.words[ScreenModels::Message+0x978]=0;
    m.words[text+8]=help_text;m.words[help_text]=0x4ebd60;
    assert(ScreenModels::MessageElement(m,help_plate)==1);
    assert(ScreenModels::MessageElement(m,help_text)==256);
    {
        const auto original=m.words;
        m.words[SaveMenu::Owner+0x28]=0;
        m.words[0x5066f8+0x34]=m.words[0x50446c+0x18]=0;
        m.words[0x506cb0+0x38]=2;m.words[0x4fda6c+0x50]=1;
        m.words[0x54ac4c]=0x01000000;
        for(U32 message:{0x774u,0x775u,0x776u}) {
            m.words[ScreenModels::Message+0xf40]=message;
            assert(hook.ForegroundModel(m,help_plate) && hook.ForegroundModel(m,help_text));
        }
        // The deferred draw must use the dungeon controls as well as correcting
        // the pause pass's horizontal projection, for both plate and lettering.
        const auto saved_layout=PauseMenu::GetLayout();auto layout=saved_layout;
        layout[PauseMenu::DungeonDescription]={30,-12,.5f};PauseMenu::SetLayout(layout,false);
        constexpr U32 view=0x8190000;
        const std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
        for(U32 model:{help_plate,help_text})for(unsigned i=0;i<12;++i)
            m.words[model+0xc+i*4]=std::bit_cast<U32>(identity[i]);
        m.words[help_plate+0x18]=std::bit_cast<U32>(200.f);
        m.words[help_plate+0x28]=std::bit_cast<U32>(180.f);
        const auto saved_projection=hook.bottom_projection;hook.bottom_projection=true;
        for(U32 model:{help_plate,help_text}) {
            for(unsigned i=0;i<12;++i)m.words[view+i*4]=std::bit_cast<U32>(identity[i]);
            assert(hook.ApplyForegroundDescription(m,view,model));
            assert(std::abs(std::bit_cast<float>(m.words[view])-.4f)<.0001f);
            assert(std::abs(std::bit_cast<float>(m.words[view+20])-.5f)<.0001f);
            assert(std::abs(std::bit_cast<float>(m.words[view+12])-104.f)<.0001f);
            assert(std::abs(std::bit_cast<float>(m.words[view+28])-78.f)<.0001f);
        }
        hook.bottom_projection=saved_projection;PauseMenu::SetLayout(saved_layout,false);
        m.words[ScreenModels::Message+0xf40]=0x700;
        assert(!hook.ForegroundModel(m,help_text));
        m.words[ScreenModels::Message+0xf40]=0x774;m.words[0x54ac4c]=0;
        assert(!hook.ForegroundModel(m,help_plate));
        m.words=original;
    }
    for(U32 model:{help_plate,help_text}) {
        regs[0]=model;regs[14]=0x123456;regs[15]=0x3f9684;
        hook.NativeDraw(m,regs,true);assert(regs[15]==regs[14]);
    }
    regs=saved;
    for(U32 model:{help_plate,help_text}) {
        hook.Handle(m,regs,true);
        assert(hook.stage==Hook::Stage::Foreground && regs[0]==model && regs[15]==0x3f9680);
        hook.NativeDraw(m,regs,true);assert(regs[15]==0x3f9680);
    }
    hook.Handle(m,regs,true);assert(regs==saved && hook.foreground_count==0);
    m.words[SaveMenu::Owner+0x28]=0;
    assert(!hook.ForegroundModel(m,help_plate)); // Normal dialogue is unaffected.
    // Accepted close hides both plate and text immediately, even while native
    // message teardown and the remaining pause slide are still in progress.
    const auto before_close=m.words;
    m.words[0x54ac4c]=0;
    for(U32 state:{5u,14u,15u})for(U32 model:{help_plate,help_text}) {
        m.words[ScreenModels::Message+0xf38]=state;
        regs[0]=model;regs[14]=0x123456;regs[15]=0x3f9684;
        hook.NativeDraw(m,regs,true);assert(regs[15]==regs[14]);
        regs[15]=0x3f9684;
        hook.NativeDraw(m,regs,false);assert(regs[15]==0x3f9684);
    }
    regs[0]=model;regs[15]=0x3f9684;
    hook.NativeDraw(m,regs,true);assert(regs[15]==0x3f9684);
    m.words[PauseMenu::UI+0x14]=0;regs[0]=help_plate;
    hook.NativeDraw(m,regs,true);assert(regs[15]==0x3f9684);
    m.words=before_close;regs[0]=help_plate;
    hook.NativeDraw(m,regs,true);assert(regs[15]==0x3f9684);
    regs=saved;
    hook.Update(m,450000,false);assert(regs==saved && hook.slide.progress==0);
    // Options always hides its header; the Save toggle remains independent.
    auto save_layout=SaveMenu::GetLayout();save_layout.hide_header=true;
    SaveMenu::SetLayout(save_layout,false);
    m.words[SaveMenu::Owner+0x28]=2;
    hook.presenting=true;hook.slide.progress=1;hook.stage=Hook::Stage::Idle;
    regs=saved;hook.Handle(m,regs,true);
    assert(regs==saved && hook.stage==Hook::Stage::Idle);
    constexpr U32 save_banner=0x8c00000;
    m.words[BannerOwner+0x28]=save_banner;
    regs[0]=save_banner;regs[14]=0x123456;regs[15]=0x3f9684;
    hook.NativeDraw(m,regs,true);assert(regs[15]==regs[14]);
    for(U32 state:{0u,3u}) {
        m.words[SaveMenu::Owner+0x28]=state;
        assert(SaveMenu::HideHeader(m)==(state==3));
    }
    m.words[SaveMenu::Owner+0x28]=0;
    save_layout.hide_header=false;SaveMenu::SetLayout(save_layout,false);regs=saved;
    // Loading a state without our allocation invalidates it before any reuse.
    m.words[block+Cookie]=0;hook.Update(m,500000,true);hook.Handle(m,regs,true);assert(regs[15]==0x313ce0);
    regs[0]=0;hook.Handle(m,regs,true);assert(regs==saved && hook.stage==Hook::Stage::Idle);
    std::cout<<"Pause header tiling, immediate close, shared motion, draw ordering, native buffers and state fallback passed\n";
}
