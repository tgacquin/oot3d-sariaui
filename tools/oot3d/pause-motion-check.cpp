// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_shadow.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void F(U32 a,float f) {words[a]=std::bit_cast<U32>(f);}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
int main() {
    Memory m;
    constexpr U32 play=0x8000000,model=0x8100000,mesh=0x8200000,src=0x8300000,dst=0x8400000;
    constexpr U32 helper=0x8500000,offsets=0x8600000,icon_model=0x8700000,icon_mesh=0x8800000;
    m.words[UI+0xc]=play;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;m.words[play+0x100]=3;
    m.words[0x5066f8+0x20]=model;m.words[0x5066f8+0x1c]=mesh;m.words[0x5066f8+0x24]=helper;m.words[0x5066f8+0x34]=13;
    m.words[model]=m.words[icon_model]=0x4ebd60;m.words[model+4]=mesh;m.words[icon_model+4]=icon_mesh;
    m.words[0x5066f8+0x28]=icon_model;m.words[helper]=37;m.words[helper+0x1c]=offsets;
    const auto quad=[&](U32 address,float x,float y,float w,float h) {
        const std::array<float,12> v{x,y,-3,x+w,y,-3,x,y+h,-3,x+w,y+h,-3};
        for(unsigned i=0;i<12;++i)m.F(address+4*i,v[i]);
    };
    for(unsigned q=0;q<37;++q) {quad(src+48*q,400,240,16,16);quad(dst+48*q,0,0,0,0);}
    quad(src,4-160,0,124,200);quad(src+48,128-160,0,124,200);
    quad(src+12*48,256+40,0,64,240);quad(src+17*48,266+40,2,52,52);
    m.F(offsets,-160);m.F(offsets+4,0);m.F(offsets+12*8,40);m.F(offsets+12*8+4,0);
    const auto native=m.words;
    for(bool horizontal:{false,true})for(float scale:{.5f,1.f,2.f}) {
        m.words=native;
        Layout layout{};layout.horizontal=horizontal;layout[Items]={-70,45,scale};layout[Slots]={-80,-70,scale};
        SetLayout(layout,false);motion_x=motion_slots=motion_y=0;
        Hook hook;
        assert(CopyPositions(m,mesh,dst,src,37*48,&hook.pending));
        const auto resting=m.words;
        // Even the first plate half, originally fully offscreen, is recovered.
        assert(Near(m.F(dst),Move(4,0,Items,layout[Items])[0]));
        const auto slot=horizontal?Move(168,152,Slots,layout[Slots]):Move(256,0,Slots,layout[Slots]);
        assert(Near(m.F(dst+12*48),slot[0]) && Near(m.F(dst+12*48+4),slot[1]));
        motion_x=-140;motion_y=motion_slots=-35;hook.Animate(m);
        for(unsigned q:{0u,1u,12u,17u})for(unsigned i=0;i<12;i+=3) {
            const auto at=dst+q*48+i*4;
            assert(Near(m.F(at),std::bit_cast<float>(resting.at(at))+(q<2?-140/Scale:0)));
            assert(Near(m.F(at+4),std::bit_cast<float>(resting.at(at+4))+(q>=12?-35/Scale:0)));
            assert(m.F(at+8)==-3);
        }
        const auto moved=m.words;hook.Animate(m);assert(m.words==moved && hook.pending.empty());
        for(U32 i=0;i<37*48;i+=4)assert(m.words.at(src+i)==native.at(src+i));
        // The slots' separate icon upload has no native slide to subtract.
        quad(src,271,7,42,42);
        assert(CopyPositions(m,icon_mesh,dst,src,48,&hook.pending));
        const float ix=m.F(dst),iy=m.F(dst+4);hook.Animate(m);
        assert(Near(m.F(dst),ix) && Near(m.F(dst+4),iy-35/Scale));
        assert(Near(m.F(dst+12)-m.F(dst),42*scale));
        // Reused native destinations must not be overwritten by stale entries.
        assert(CopyPositions(m,icon_mesh,dst,src,48,&hook.pending));
        m.F(dst,123);const auto replaced=m.words;hook.Animate(m);assert(m.words==replaced);
        // Touch and shadows use the same final pixel displacement.
        const auto c=SlotCenters[0];
        const auto point=AnimatedMove(horizontal?288+c[1]-120:c[0],horizontal?120:c[1],Slots,layout[Slots]);
        const auto hit=MapPointer(.5f+(point[0]-160)*Scale/400,.5f+((point[1]-120)*Scale+OffsetY)/240,400,layout);
        assert(hit && Near((*hit)[0]*320,c[0]) && Near((*hit)[1]*240,c[1]));
        // At fully closed, moved/scaled panels and their shadows clear the screen.
        layout[Background]={80,10,scale};layout.stack_background=true;
        const float travel=PauseHeader::PanelTravel(layout,true);
        assert(200+128*scale+80+8*scale-travel < (400-240*Widescreen::Aspect)/2);
        assert(PauseHeader::SlotsTravel(layout)>=138+layout[Slots].y+Scale*scale*(horizontal?32:120));
    }
    // Resize/re-space the upright stones without resizing the rail, and map
    // touches back to their native slot after all group transforms.
    for(bool horizontal:{false,true})for(float size:{.6f,1.4f}) {
        m.words=native;motion_x=motion_y=motion_slots=motion_bottom=0;
        Layout l;l.horizontal=horizontal;l[Slots]={-100,-60,.75f};SetLayout(l,false);
        assert(CopyPositions(m,mesh,dst,src,37*48));
        std::array<U32,12> rail{};
        for(unsigned k=0;k<12;++k)rail[k]=m.words[dst+12*48+k*4];
        l.slot_content_scale=size;l.slot_spacing=.8f;l.slot_content_x=13;l.slot_content_y=9;SetLayout(l,false);
        assert(CopyPositions(m,mesh,dst,src,37*48));
        for(unsigned k=0;k<12;++k)assert(rail[k]==m.words[dst+12*48+k*4]);
        const float cx=horizontal?288+(28-120)*.8f+13:292+13;
        const float cy=horizontal?129:129+(28-120)*.8f;
        const auto center=Move(cx,cy,Slots,l[Slots]);
        const auto at=dst+17*48;
        assert(Near((m.F(at)+m.F(at+12))/2,center[0]));
        assert(Near((m.F(at+4)+m.F(at+28))/2,center[1]));
        assert(Near(m.F(at+12)-m.F(at),52*.75f*size));
        const float u=.5f+(center[0]-160)*Scale/400;
        const float v=.5f+((center[1]-120)*Scale+OffsetY)/240;
        const auto pointer=MapPointer(u,v,400,l);assert(pointer);
        assert(Near((*pointer)[0]*320,292) && Near((*pointer)[1]*240,28));
    }
    // Ammunition digits are nested below the item sprite controller and are
    // regenerated during draw, after Animate. Match only these owned counters.
    constexpr U32 owner=0x8900000,counter=0x8a00000,number_model=0x8b00000,number_mesh=0x8c00000;
    m.words[0x5066f8+0x3c]=owner;m.words[owner+8]=icon_model;m.words[owner+0x224]=24;
    m.words[owner+0xc+5*4]=counter;m.words[counter+0x10]=number_model;
    m.words[number_model]=0x4ebd60;m.words[number_model+4]=number_mesh;
    assert(OwnedItemCounter(m,number_mesh) && !OwnedItemCounter(m,number_mesh+4));
    for(bool horizontal:{false,true})for(unsigned slot=0;slot<4;++slot) {
        Layout layout{};layout.horizontal=horizontal;layout[Slots]={-100,-60,.75f};layout.slot_content_y=9;SetLayout(layout,false);
        const auto c=SlotCenters[slot];const float x=c[0]+8,y=c[1]+10;
        quad(src,x,y,8,12);quad(src+48,x+8,y,8,12);quad(src+96,400,240,8,12);
        const auto before=m.words;
        Hook hook;hook.drawing=true;std::array<U32,16> r{};
        r[4]=number_mesh;r[0]=dst;r[6]=src;r[5]=144;r[15]=Sites[3].pc+4;
        motion_y=motion_slots=-20;motion_x=-90;hook.Handle(m,r,3);
        assert(r[15]==0x3675f4 && hook.pending.empty());
        const auto expected=AnimatedMove(horizontal?x+288+c[1]-120-c[0]:x,
            (horizontal?y+120-c[1]:y)+9,Slots,layout[Slots]);
        assert(Near(m.F(dst),expected[0]) && Near(m.F(dst+4),expected[1]));
        assert(Near(m.F(dst+12)-m.F(dst),6) && Near(m.F(dst+48)-m.F(dst),6));
        for(U32 i=0;i<144;i+=4)assert(m.words.at(src+i)==before.at(src+i));
        assert(m.F(dst+96)==m.F(dst+108)); // Hidden spare digit stays collapsed.
        const auto frame=m.words;hook.Handle(m,r,3);assert(m.words==frame);
    }
    // The shared slide owns visibility while native opening/closing counters
    // still run. Never change gameplay, unsupported builds, or equip states.
    for(U32 state:{0u,1u,2u,9u,10u,11u,13u,14u})
    for(bool supported:{false,true})for(bool drawing:{false,true}) {
        Hook hook;hook.drawing=drawing;
        m.words[0x5066f8+0x34]=state;
        for(unsigned site:{12u,13u}) {
            const U32 field=0x5066f8+(site==12?0x60:0x84);
            m.words[field]=site==12?3:state==13?0xffffffff:1;
            const U32 original=m.words[field];
            std::array<U32,16> r{};r[6]=0x5066f8;r[15]=Sites[site].pc+4;
            hook.Handle(m,r,site,supported);
            const bool shared=supported && drawing &&
                (state==13 || state==14 || (state==1 && site==13));
            assert(r[0]==(shared?0:original) && r[15]==Sites[site].pc+4);
            assert(m.words[field]==original-(shared && site==13 && original==1?1:0));
        }
    }
    // The final delay is consumed once; normal browsing must not flash empty.
    Hook hook;hook.drawing=true;std::array<U32,16> r{};r[6]=0x5066f8;
    m.words[0x5066f8+0x34]=1;m.words[0x5066f8+0x84]=1;
    hook.Handle(m,r,13);assert(r[0]==0);
    m.words[0x5066f8+0x34]=2;hook.Handle(m,r,13);assert(r[0]==0);
    // Gear overlays and bottom cards use the custom duration too. Native
    // entrance offsets must not cause a second motion or hide a panel early.
    {
        Memory gear;
        gear.words[0x50446c]=mesh;gear.words[0x50446c+0x18]=9;
        gear.words[0x50446c+0xc]=helper;gear.words[helper]=41;gear.words[helper+0x1c]=offsets;
        gear.F(offsets+8, -160);gear.F(offsets+12,0);
        std::array<float,12> q{-140,20,0,-40,20,0,-140,120,0,-40,120,0};
        RestingQuad(gear,mesh,1,q);assert(q[0]==20 && q[3]==120);
        gear.words[0x50446c+0x18]=5;const auto before=q;
        RestingQuad(gear,mesh,1,q);assert(q==before); // A page-card press is not an entrance.
        motion_x=-75;motion_bottom=42;
        const auto overlay=AnimatedMove(79,78,GearLeft,{});
        assert(Near(overlay[0],79-75/Scale) && overlay[1]==78);
        const auto card=AnimatedMove(100,220,GearTab,{});
        assert(card[0]==100 && Near(card[1],220+42/Scale));
        Layout l;l[GearTab]={0,-150,2};l[Tabs]={0,-10,1.2f};
        const auto top=Move(100,198,GearTab,l[GearTab]);
        const auto group=Move(top[0],top[1],Tabs,l[Tabs]);
        assert(OffsetY+Scale*group[1]+PauseHeader::BottomTravel(l)>240);
    }
    motion_x=motion_slots=motion_y=motion_bottom=0;SetLayout({},false);
    std::cout<<"Pause shared motion, native offset removal, icons, hidden geometry, fresh buffers, touch and exit bounds passed\n";
}
