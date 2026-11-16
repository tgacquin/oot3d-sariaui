// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_menu.h"
#include "oot3d/pause_input.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    unsigned flag_pointer_reads=0;
    std::optional<U32> Read(U32 a) {if((a>=0x04000000 && a<0x04000020) || (a>=0x06000000 && a<0x06000020))++flag_pointer_reads;auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    bool Mapped(U32 at,unsigned bytes) {
        for(U32 page=at&~0xfffu;page<=((at+bytes-1)&~0xfffu);page+=0x1000) {
            const auto first=words.lower_bound(page);
            if(first==words.end() || first->first>=page+0x1000)return false;
        }
        return true;
    }
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
int main(int argc,char**argv) {
    Memory m;
    {
        Memory scan;
        scan.words[0x5066f8+0x40]=0x04000000;
        scan.words[0x5066f8+0x44]=0x06000000;
        assert(!Owned(scan,0x08001000) && scan.flag_pointer_reads==0);
        // Preserve discovery through a real owner and model, even when the
        // same root contains non-pointer state words.
        scan.words[0x5066f8+0x48]=0x08002000;
        scan.words[0x08002004]=0x08003000;
        scan.words[0x08003000]=0x4ebd60;scan.words[0x08003004]=0x08001000;
        assert(Owned(scan,0x08001000) && scan.flag_pointer_reads==0);
    }
    if(argc>1) {
        std::ifstream f(argv[1],std::ios::binary);assert(f);U32 w;
        for(U32 a=0x100000;f.read(reinterpret_cast<char*>(&w),4);a+=4)m.words[a]=w;
        assert(CodeMatches(m));
        // Unmodified layouts keep the original Nintendo navigation path.
        m.words[UI+0xc]=0x8000000;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;
        m.words[0x8000100]=3;m.words[0x50446c+0x18]=2;
        Hook hook;std::array<U32,16> regs{};
        for(U32 selected=0;selected<27;++selected) {
            SetLayout({},false);m.words[0x50446c+0x20]=selected;
            regs[0]=0x80;regs[15]=Sites[8].pc+4;hook.Handle(m,regs,8);
            assert(regs[15]==Sites[8].pc+4 && regs[4]==0x504a74);
        }
        SetLayout({},false);
        // Each substituted dispatch selects the verified native resting branch.
        for(U32 state=4;state<=6;++state)
            assert(m.Read(0x4453fc+state*4)!=m.Read(0x4453fc+2*4));
        assert(m.Read(0x4453fc+2*4)==0x445418);
        assert(m.Read(0x438c6c+2*4)==0x438c90);
        m.words[Sites[3].pc]^=1;assert(!CodeMatches(m));m.words.clear();
    }
    constexpr U32 play=0x8000000,model=0x8100000,mesh=0x8200000,src=0x8300000,dst=0x8400000;
    m.words[UI+0xc]=play;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;
    m.words[play+0x100]=0x203;m.words[0x5066f8+0x20]=model;m.words[0x5066f8+0x34]=2;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;
    {
        Hook hook;std::array<U32,16> regs{};
        for(unsigned site:{17u,18u}) {
            const bool items=site==17;
            regs[5]=items?0x5066f8:0x50446c;
            const U32 at=regs[5]+(items?0x34:0x18),first=items?4:5;
            const auto original=m.words;
            for(U32 state=0;state<15;++state) {
                m.words[at]=state;
                const auto before=m.words;
                hook.Handle(m,regs,site);
                assert(regs[1]==(state>=first && state<first+3?2:state));
                assert(m.words==before); // Native input, timers and cleanup unchanged.
                hook.Handle(m,regs,site,false);assert(regs[1]==state);
                m.words[UI+0x14]=0;
                hook.Handle(m,regs,site);assert(regs[1]==state);
                m.words[UI+0x14]=1;
            }
            m.words=original;
            const auto s=Sites[site];
            assert(Instruction(s.pc,s.instruction)==(0xef000000|FirstSVC+site));
            assert(Instruction(s.pc,s.instruction^1)==(s.instruction^1));
        }
    }
    {
        const auto original=m.words;
        Hook hook;std::array<U32,16> r{};
        m.words[0x5066f8+0x34]=12;m.words[0x5066f8+0x38]=1;
        m.words[0x5066f8+0x24]=0x8500000;
        r[8]=0x5066f8;r[0]=0x8500000;r[1]=37;r[2]=0;r[15]=Sites[20].pc+4;
        const auto before=m.words;
        hook.Handle(m,r,20);assert(r[15]==Sites[20].pc+4 && m.words==before);
        hook.Handle(m,r,20,false);assert(r[15]==0x2f8b80 && m.words==before);
        r[8]=0;hook.Handle(m,r,20);assert(r[15]==0x2f8b80);
        m.words=original;
    }
    // Bottom-row arrow chooser corners belong to Items, even below Y=198.
    // Test cursor, icons and popup background, including native parked quads.
    {
        const auto original=m.words;
        Layout layout;layout[Items]={17,-13,1.2f};layout[GearTab]={-80,40,.4f};
        SetLayout(layout,false);
        constexpr U32 controller=0x8500000;
        for(auto [offset,slot]:std::array<std::array<U32,2>,3>{{{0x28,4},{0x40,8},{0x4c,4}}}) {
            m.words=original;
            m.words[0x5066f8+offset]=controller;m.words[controller+slot]=model;
            for(float y:{140.f,184.f,224.f,240.f}) {
                const std::array<float,12> v{60,y,-3,70,y,-3,60,y+10,-3,70,y+10,-3};
                for(unsigned i=0;i<12;++i){m.words[src+i*4]=std::bit_cast<U32>(v[i]);m.words[dst+i*4]=0;}
                assert(CopyPositions(m,mesh,dst,src,48));
                for(unsigned i=0;i<12;i+=3) {
                    const auto p=y==240?std::array{60.f,240.f}:Move(v[i],v[i+1],Items,layout[Items]);
                    assert(m.F(dst+i*4)==p[0] && m.F(dst+i*4+4)==p[1]);
                }
            }
        }
        m.words=original;SetLayout({},false);
    }
    // Both item preview models stop drawing on accepted close, before the
    // native pause state clears. World models and the live queue stay intact.
    {
        constexpr U32 owner=0x8500000,first=0x8510000,second=0x8520000,entry=0x8530000;
        m.words[0x4fc648+0x34]=owner;
        m.words[owner+0xfc]=first;m.words[owner+0x100]=second;
        m.words[first]=m.words[second]=0x4ec018;
        Hook hook;std::array<U32,16> regs{};regs[0]=entry;
        for(unsigned site:{15u,16u})for(U32 candidate:{first,second,model}) {
            m.words[entry]=candidate;
            m.words[0x54ac4c]=0x01000000;
            hook.Handle(m,regs,site);assert(regs[5]==candidate);
            m.words[0x54ac4c]=0;
            const auto before=m.words;
            hook.Handle(m,regs,site);assert(regs[5]==(candidate==model?model:0));
            assert(m.words==before);
            hook.Handle(m,regs,site,false);assert(regs[5]==candidate);
            m.words[UI+0x14]=0;
            hook.Handle(m,regs,site);assert(regs[5]==candidate);
            m.words[UI+0x14]=1;m.words[0x54ac4c]=0x01000000;
            hook.Handle(m,regs,site);assert(regs[5]==candidate);
        }
        m.words.erase(0x54ac4c);
        hook.Handle(m,regs,15);assert(regs[5]==model);
        for(unsigned site:{15u,16u}) {
            const auto s=Sites[site];
            assert(Instruction(s.pc,s.instruction)==(0xef000000|FirstSVC+site));
            assert(Instruction(s.pc,s.instruction^1)==(s.instruction^1));
        }
    }
    const std::array<std::array<float,4>,4> bounds{{{4,0,128,198},{64,202,128,240},{264,0,320,240},{320,240,376,280}}};
    for(unsigned q=0;q<4;++q) {
        const auto b=bounds[q];const std::array<float,12> v{b[0],b[1],-3,b[2],b[1],-3,b[0],b[3],-3,b[2],b[3],-3};
        for(unsigned i=0;i<12;++i){m.words[src+q*48+i*4]=std::bit_cast<U32>(v[i]);m.words[dst+q*48+i*4]=0;}
    }
    const auto original=m.words;
    assert(CopyPositions(m,mesh,dst,src,192));
    for(unsigned q=0;q<3;++q)for(unsigned i=0;i<12;++i)assert(m.words[dst+q*48+i*4]==m.words[src+q*48+i*4]);
    for(unsigned i=0;i<12;i+=3){assert(m.F(dst+144+i*4)==320);assert(m.F(dst+148+i*4)==240);}
    Layout poses{};poses[Items]={15,-10,1.2f};poses[Tabs]={-20,-30,.7f};poses[Slots]={60,5,1.5f};SetLayout(poses,false);
    assert(CopyPositions(m,mesh,dst,src,192));
    for(unsigned q=0;q<3;++q) {
        Part part=q==0?Items:q==1?Tabs:Slots;
        const auto p=Move(bounds[q][0],bounds[q][1],part,poses[part]);
        assert(m.F(dst+q*48)==p[0] && m.F(dst+q*48+4)==p[1] && m.F(dst+q*48+8)==-3);
    }
    const auto transformed=m.words;assert(CopyPositions(m,mesh,dst,src,192));assert(m.words==transformed);
    for(U32 i=0;i<192;i+=4)assert(m.words[src+i]==original.at(src+i));
    assert(!CopyPositions(m,mesh+4,dst,src,192));
    m.words[UI+0x14]=0;assert(!CopyPositions(m,mesh,dst,src,192));m.words[UI+0x14]=1;
    for(Part part:{Tabs,Items,Slots}) {
        // Test each independent pose without overlap, including native and widescreen mapping.
        Layout p{};p[part]={10,-5,.8f};const auto a=Anchors[part];const auto point=Move(a[0],a[1],part,p[part]);
        for(float width:{400.f,426.666667f}) {
            const float u=.5f+(point[0]-160)*Scale/width;
            const float v=.5f+((point[1]-120)*Scale+OffsetY)/240;
            auto mapped=MapPointer(u,v,width,p);assert(mapped);
            assert(std::abs((*mapped)[0]*320-a[0])<.001 && std::abs((*mapped)[1]*240-a[1])<.001);
        }
    }
    Layout map{};map[Map]={-20,10,.8f};map[Items]={90,-50,2};
    const auto map_point=Move(280,100,Map,map[Map]);
    const auto map_hit=MapPointer(.5f+(map_point[0]-160)*Scale/400,
        .5f+((map_point[1]-120)*Scale+OffsetY)/240,400,map,false);
    assert(map_hit && std::abs((*map_hit)[0]*320-280)<.001);
    // The same native panel upload uses the active page's own pose. Item and
    // border edits must not affect the map, including its shared pause slide.
    m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=0;
    SetLayout(map,false);assert(CopyPositions(m,mesh,dst,src,48));
    const auto map_corner=Move(4,0,Map,map[Map]);
    assert(m.F(dst)==map_corner[0] && m.F(dst+4)==map_corner[1]);
    const auto map_draw=m.words;
    map[Items]={-150,90,.3f};map.map_frame.cut_x=900;map.map_frame.cut_scale=4;
    SetLayout(map,false);assert(CopyPositions(m,mesh,dst,src,48));assert(m.words==map_draw);
    Hook map_hook;
    assert(CopyPositions(m,mesh,dst,src,48,&map_hook.pending));
    motion_x=-60;map_hook.Animate(m);
    assert(std::abs(m.F(dst)-(map_corner[0]-60/Scale))<.001f);
    const auto moving_point=AnimatedMove(100,100,Map,map[Map]);
    const auto moving_hit=MapPointer(.5f+(moving_point[0]-160)*Scale/400,
        .5f+((moving_point[1]-120)*Scale+OffsetY)/240,400,map,false);
    assert(moving_hit && std::abs((*moving_hit)[0]*320-100)<.001f &&
        std::abs((*moving_hit)[1]*240-100)<.001f);
    motion_x=0;m.words[0x5066f8+0x34]=2;
    assert(CopyPositions(m,mesh,dst,src,48));
    const auto item_corner=Move(4,0,Items,map[Items]);
    assert(m.F(dst)==item_corner[0] && m.F(dst+4)==item_corner[1]);
    const auto item_draw=m.words;map[Map]={200,-120,2.5f};SetLayout(map,false);
    assert(CopyPositions(m,mesh,dst,src,48));assert(m.words==item_draw);
    assert(!MapPointer(0,0,400,{}));
    // Each card and each gear quadrant maps back independently after movement/scale.
    for(Part part:{Back,GearTab,ItemsTab,GearLeft,GearRight,GearBottomLeft,GearBottomRight}) {
        Layout p{};p[part]={12,-8,.6f};const auto a=Anchors[part];
        const auto moved=Move(a[0],a[1],part,p[part]);
        const auto hit=MapPointer(.5f+(moved[0]-160)*Scale/400,
            .5f+((moved[1]-120)*Scale+OffsetY)/240,400,p,part<GearLeft,part>=GearLeft);
        assert(hit && std::abs((*hit)[0]*320-a[0])<.001 && std::abs((*hit)[1]*240-a[1])<.001);
    }
    // Horizontal frames/icons translate without rotation; only the backing rail rotates.
    Layout horizontal{};horizontal.horizontal=true;horizontal[Slots]={-70,-70,.8f};SetLayout(horizontal,false);
    for(unsigned slot=0;slot<4;++slot) {
        const auto c=SlotCenters[slot];
        const std::array<float,12> v{c[0]-20,c[1]-20,0,c[0]+20,c[1]-20,0,
            c[0]-20,c[1]+20,0,c[0]+20,c[1]+20,0};
        for(unsigned i=0;i<12;++i)m.words[src+i*4]=std::bit_cast<U32>(v[i]);
        assert(CopyPositions(m,mesh,dst,src,48));
        assert(m.F(dst+12)>m.F(dst) && m.F(dst+16)==m.F(dst+4));
        assert(m.F(dst+24)==m.F(dst) && m.F(dst+28)>m.F(dst+4));
        const auto center=Move(288+c[1]-120,120,Slots,horizontal[Slots]);
        const auto hit=MapPointer(.5f+(center[0]-160)*Scale/400,
            .5f+((center[1]-120)*Scale+OffsetY)/240,400,horizontal,true);
        assert(hit && std::abs((*hit)[0]*320-c[0])<.001 && std::abs((*hit)[1]*240-c[1])<.001);
    }
    // Either lower Gear strip can turn vertical independently. Icons stay upright;
    // the backing rotates, and touch follows the moved/scaled icon groups.
    m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=2;
    for(Part part:{GearBottomLeft,GearBottomRight}) {
        Layout p{};p.gear_vertical[part-GearBottomLeft]=true;p[part]={0,-80,.7f};
        SetLayout(p,false);
        const auto a=Anchors[part];
        const auto upload=[&](float x,float y,float w,float h) {
            const std::array<float,12> v{x,y,0,x+w,y,0,x,y+h,0,x+w,y+h,0};
            for(unsigned i=0;i<12;++i)m.words[src+i*4]=std::bit_cast<U32>(v[i]);
            assert(CopyPositions(m,mesh,dst,src,48));
        };
        upload(a[0]-79,154,158,40);
        assert(std::abs(m.F(dst+12)-m.F(dst))<.001);
        assert(std::abs(m.F(dst+16)-m.F(dst+4)-158*.7f)<.001);
        assert(std::abs(m.F(dst+24)-m.F(dst)+40*.7f)<.001);
        for(unsigned slot=0;slot<4;++slot) {
            const float cx=GearSlotX(part,slot);
            upload(cx-16,158,32,32);
            assert(std::abs(m.F(dst+12)-m.F(dst)-32*.7f)<.001);
            assert(m.F(dst+16)==m.F(dst+4));
            const auto center=Move(a[0],174+cx-a[0],part,p[part]);
            const auto hit=MapPointer(.5f+(center[0]-160)*Scale/400,
                .5f+((center[1]-120)*Scale+OffsetY)/240,400,p,false,true);
            assert(hit && std::abs((*hit)[0]*320-cx)<.001 && std::abs((*hit)[1]*240-174)<.001);
            const auto transformed=m.words;assert(CopyPositions(m,mesh,dst,src,48));assert(m.words==transformed);
        }
        // Cursor corner geometry rotates with its frame, including the wide count selection.
        constexpr U32 cursor=0x8700000;
        m.words[0x50446c+0x14]=cursor;m.words[cursor]=mesh;
        upload(GearSlotX(part,0)-18,156,16,16);
        const auto corner=Move(a[0]+18,174+GearSlotX(part,0)-18-a[0],part,p[part]);
        assert(std::abs(m.F(dst)-corner[0])<.001 && std::abs(m.F(dst+4)-corner[1])<.001);
        assert(m.F(dst+12)==m.F(dst) && m.F(dst+16)>m.F(dst+4));
        m.words.erase(0x50446c+0x14);
        const auto other=part==GearBottomLeft?GearBottomRight:GearBottomLeft;
        upload(GearSlotX(other,0)-16,158,32,32);
        for(unsigned i=0;i<12;++i)assert(m.words.at(dst+i*4)==m.words.at(src+i*4));
        SetLayout({},false);upload(a[0]-79,154,158,40);
        for(unsigned i=0;i<12;++i)assert(m.words.at(dst+i*4)==m.words.at(src+i*4));
    }
    m.words[0x5066f8+0x34]=2;m.words[0x50446c+0x18]=0;
    // The model hook changes a fresh matrix, never the live controller's position or scale.
    constexpr U32 owner=0x8500000,matrix=0x8600000;
    m.words[0x4fc648+0x34]=owner;m.words[owner+0xfc]=model;
    const std::array<float,12> native{1,0,0,0,0,1,0,2.5f,0,0,1,-36};
    for(unsigned i=0;i<12;++i)m.words[matrix+i*4]=std::bit_cast<U32>(native[i]);
    assert(PreviewMatrix(m,owner,model,matrix,{20,10,2}));
    assert(m.F(matrix)==2 && m.F(matrix+20)==2 && m.F(matrix+40)==2);
    assert(m.F(matrix+12)>0 && m.F(matrix+28)<2.5f && m.F(matrix+44)==-36);
    const float normal_x=m.F(matrix+12),normal_y=m.F(matrix+28);
    m.words[0x587958+0xc]=1u<<16;
    for(unsigned i=0;i<12;++i)m.words[matrix+i*4]=std::bit_cast<U32>(native[i]);
    assert(PreviewMatrix(m,owner,model,matrix,{20,10,2}));
    assert(m.F(matrix+12)==-normal_x && m.F(matrix+28)==normal_y);
    assert(m.F(matrix)==2 && m.F(matrix+44)==-36 && m.words[0x587958+0xc]==(1u<<16));
    m.words[0x587958+0xc]=0;
    assert(!PreviewMatrix(m,owner+4,model,matrix,{20,10,2}));
    for(const auto& site:Sites){m.words[site.pc]=site.instruction;m.words[site.pc+4]=site.following;}
    m.words[0x41ec2c]=0xe92d40f0;m.words[0x41eec8]=UI;
    m.words[UI+0x14]=0;m.words[0x50af34+0x34]=2;
    MenuInput input;U32 buttons=4;assert(input.Filter(m,buttons,true) && buttons==0);
    for(int i=0;i<20;++i){buttons=4;input.Filter(m,buttons,true);assert(buttons==0);}
    assert(input.ticks==0);buttons=0;input.Filter(m,buttons,true);
    buttons=8;assert(!input.Filter(m,buttons,true) && buttons==8);
    buttons=4;assert(!input.Filter(m,buttons,false) && buttons==4);
    m.words[UI+0x14]=1;
    Hook h;std::array<U32,16> r{};r[5]=0x3fff;r[15]=Sites[0].pc+4;
    h.Handle(m,r,0);assert(h.drawing && r[15]==0x41ec2c && r[14]==Sites[1].pc);
    // The optional second background draws once and returns to the native queue.
    m.words[UI+0x10]=model;
    Layout stacked{};SetLayout(stacked,false);
    r[15]=Sites[5].pc+4;h.Handle(m,r,5);
    assert(!h.background_copy && r[15]==Sites[5].pc+4);
    stacked.stack_background=true;SetLayout(stacked,false);
    h.Handle(m,r,5);
    assert(h.background_copy && r[0]==model && r[15]==0x3f9680 && r[14]==Sites[5].pc);
    r[15]=Sites[5].pc+4;h.Handle(m,r,5);
    assert(!h.background_copy && r[15]==Sites[5].pc+4);
    h.Handle(m,r,5,false);assert(!h.background_copy && r[15]==Sites[5].pc+4);
    h.Handle(m,r,1);assert(!h.drawing && r[0]==0x3fff);
    h.Handle(m,r,5);assert(!h.background_copy && r[15]==Sites[5].pc+4);
    r[15]=Sites[2].pc+4;h.Handle(m,r,2);assert(r[15]==Sites[2].pc+4 && !h.routed);
    h.Handle(m,r,0,false);assert(!h.drawing && !on_top.load());
    r[15]=Sites[2].pc+4;h.Handle(m,r,2);assert(r[15]==0x41ec2c);
    // Native gameplay UI is never drawn as part of the routed pause screen,
    // even when it becomes visible before the game's pause state finishes closing.
    h.drawing=true;r[15]=Sites[11].pc+4;h.Handle(m,r,11);
    assert(r[15]==Sites[11].pc+4 && r[14]==Sites[11].pc+4);
    h.drawing=false;h.Handle(m,r,11);assert(r[15]==0x42f584);
    h.drawing=true;r[15]=Sites[11].pc+4;h.Handle(m,r,11,false);assert(r[15]==0x42f584);
    // Once the custom close finishes, suppress both menu passes until native
    // cleanup finishes; never route the returning gameplay HUD onto the top.
    h.show_contents=false;r[15]=Sites[0].pc+4;h.Handle(m,r,0);
    assert(h.routed && !h.drawing && r[15]==Sites[0].pc+4);
    r[15]=Sites[2].pc+4;h.Handle(m,r,2);
    assert(r[15]==Sites[2].pc+4 && !h.routed);
    m.words[UI+0x14]=0;r[15]=Sites[2].pc+4;h.Handle(m,r,2);
    assert(r[15]==0x41ec2c);m.words[UI+0x14]=1;
    // Both browsing and equip-target navigation use the horizontal topology.
    for(unsigned site:{6u,7u}) {
        m.words[0x5066f8+0x34]=site==6?2:8;
        Layout p{};p.horizontal=true;SetLayout(p,false);
        const auto step=[&](U32 x,U32 y,U32 direction,U32 nx,U32 ny) {
            m.words[0x5066f8+0x78]=x;m.words[0x5066f8+0x7c]=y;
            r[0]=direction;r[4]=0;r[5]=0x5066f8;r[15]=Sites[site].pc+4;
            h.Handle(m,r,site);
            assert(m.words.at(0x5066f8+0x78)==nx && m.words.at(0x5066f8+0x7c)==ny);
            assert(r[15]==0x2ed1cc && r[4]==U32(nx!=x || ny!=y));
        };
        for(U32 x=0;x<5;++x) {
            step(x,0,0x40,5,(x*3+2)/4); // Up leaves the grid through its top edge.
            step(x,2,0x40,x,1); // Normal grid movement remains intact.
        }
        for(U32 slot=0;slot<4;++slot) {
            step(5,slot,0x80,(slot*4+1)/3,0); // Down returns to the grid.
            step(5,slot,0x10,5,(slot+1)%4);
            step(5,slot,0x20,5,(slot+3)%4);
            step(5,slot,0x40,5,slot); // Up cannot accidentally advance along the bar.
        }
        step(4,1,0x10,0,1);step(0,1,0x20,4,1); // Side edges stay within the grid.
        step(2,1,0,2,1);
        for(unsigned fallback=0;fallback<4;++fallback) {
            p.horizontal=fallback!=0;SetLayout(p,false);
            m.words[0x5066f8+0x34]=fallback==1?9:site==6?2:8;
            m.words[UI+0x14]=fallback==2?0:1;
            r[0]=0x40;r[15]=Sites[site].pc+4;
            const auto before=m.words;
            h.Handle(m,r,site,fallback!=3);
            assert(m.words==before && r[15]==Sites[site].pc+4);
        }
    }
    // Enter/exit animations can bring every pressed-border piece back from
    // its parked position. Suppress them by ownership, even when they overlap
    // the normal card and the user has moved/scaled the tabs.
    for(bool items:{true,false}) {
        Memory tabs;
        constexpr U32 helper=0x8500000;
        const U32 root=items?0x5066f8:0x50446c,count=items?37:41,first=items?28:32;
        tabs.words[UI+0xc]=play;tabs.words[UI+0x14]=1;tabs.words[UI+0x20]=0;
        tabs.words[UI+0x24]=1;tabs.words[play+0x100]=3;
        tabs.words[root+(items?0x1c:0)]=mesh;
        tabs.words[root+(items?0x20:4)]=model;tabs.words[model]=0x4ebd60;tabs.words[model+4]=mesh;
        tabs.words[root+(items?0x24:0xc)]=helper;tabs.words[helper]=count;
        Layout poses{};poses[GearTab]={-12,-25,.7f};poses[ItemsTab]={20,-10,1.2f};
        SetLayout(poses,false);
        for(U32 state:{1u,2u,4u,6u,9u,13u,14u})for(float offset:{0.f,2.f,10.f,20.f}) {
            tabs.words[root+(items?0x34:0x18)]=state;
            for(unsigned q=0;q<count;++q) {
                const float x=q>=first?68.f+16.f*((q-first)%3):80.f;
                const std::array<float,12> quad{x,206+offset,0,x+16,206+offset,0,
                    x,240+offset,0,x+16,240+offset,0};
                for(unsigned i=0;i<12;++i) {
                    tabs.words[src+48*q+4*i]=std::bit_cast<U32>(quad[i]);
                    tabs.words[dst+48*q+4*i]=0;
                }
            }
            const auto before=tabs.words;
            assert(CopyPositions(tabs,mesh,dst,src,count*48));
            for(unsigned q=first;q<count;++q)for(unsigned i=3;i<12;i+=3) {
                assert(tabs.Read(dst+48*q+4*i)==tabs.Read(dst+48*q));
                assert(tabs.Read(dst+48*q+4*(i+1))==tabs.Read(dst+48*q+4));
            }
            const unsigned normal=items?9:21;
            assert(tabs.Read(dst+48*normal)!=tabs.Read(dst+48*normal+12));
            for(U32 off=0;off<count*48;off+=4)assert(tabs.words.at(src+off)==before.at(src+off));
        }
        assert(!PressedTabQuad(tabs,mesh+4,first,count));
        assert(!PressedTabQuad(tabs,mesh,first,count-1));
        tabs.words[model]=0;assert(!PressedTabQuad(tabs,mesh,first,count));
    }
    SetLayout({},false);
    std::cout<<"Pause signatures, routing, poses, transition border suppression, fresh copies and touch mapping passed\n";
}
