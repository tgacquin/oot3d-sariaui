// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_input.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {
        auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};
    }
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
};
int main() {
    Memory m;
    for(const auto& s:Sites){m.words[s.pc]=s.instruction;m.words[s.pc+4]=s.following;}
    m.words[0x41ec2c]=0xe92d40f0;m.words[0x41eec8]=UI;
    constexpr U32 play=0x8000000,model=0x8100000,mesh=0x8200000,src=0x8300000,dst=0x8400000;
    m.words[UI+0xc]=play;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;
    m.words[play+0x100]=3;m.words[0x50af34+0x34]=2;
    m.words[0x5066f8+0x20]=model;m.words[model]=0x4ebd60;m.words[model+4]=mesh;
    const auto page=[&](U32 items,U32 gear) {m.words[0x5066f8+0x34]=items;m.words[0x50446c+0x18]=gear;};
    MenuInput input;
    const auto poll=[&](U32 raw,bool touch,U32 filtered,unsigned x=220,bool live=true) {
        U32 buttons=raw;assert(input.Filter(m,buttons,live)==touch);assert(buttons==filtered);
        if(touch)assert(input.touch_x==x);
    };
    page(2,0);
    poll(MenuInput::R,true,0,170); // R invokes the native Map card.
    assert(input.HiddenTouch(170,220) && !input.HiddenTouch(220,220));
    page(0,0);poll(MenuInput::R,false,0); // Accepted transition ends touch immediately.
    assert(input.HiddenTouch(100,220) && input.HiddenTouch(220,220));
    assert(!input.HiddenTouch(170,100)); // Map content remains interactive.
    poll(0,false,0);poll(MenuInput::L,true,0); // L invokes Items.
    page(1,0);poll(MenuInput::L,false,0); // Stop on entrance, not just stable browsing.
    poll(0,false,0);page(2,0);
    for(unsigned i=0;i<25;++i)poll(MenuInput::R,i<10,0,170); // Holding never repeats.
    poll(0,false,0);poll(MenuInput::R,true,0,170);
    poll(MenuInput::R|MenuInput::B,false,MenuInput::B); // B cancels a pending switch.
    poll(0,false,0);poll(MenuInput::R|MenuInput::Start,false,MenuInput::R|MenuInput::Start);
    page(8,0);poll(0,false,0);poll(MenuInput::R,false,MenuInput::R); // Equip modal unchanged.
    page(2,0);poll(0,false,0);poll(MenuInput::L,true,0,100); // Items -> Gear.
    poll(MenuInput::L,true,0,100); // Touch lasts until the native switch is accepted.
    page(0,1);poll(MenuInput::L,false,0);
    page(0,2);poll(0,false,0);poll(MenuInput::L,false,0); // No Gear -> Map wrap.
    poll(MenuInput::L,false,0);
    poll(0,false,0);poll(MenuInput::R,true,0); // Gear -> Items.
    poll(MenuInput::R,true,0); // The old Gear page must not cancel this request.
    page(1,0);poll(MenuInput::R,false,0);
    page(2,0);poll(MenuInput::R,false,0); // Holding R cannot skip through to Map.
    poll(0,false,0);poll(MenuInput::R,true,0,170);
    page(0,0);poll(MenuInput::R,false,0);
    poll(0,false,0);poll(MenuInput::R,false,0); // No Map -> Gear wrap.
    page(0,0);poll(0,false,0);poll(MenuInput::L|MenuInput::R,false,MenuInput::L|MenuInput::R);
    m.words[UI+0x14]=0;poll(0,false,0);poll(MenuInput::R,false,MenuInput::R); // Gameplay unchanged.
    poll(0,false,0);poll(MenuInput::Select,true,0); // No live map controller: retain Items.
    m.words[UI+0x14]=1;page(1,0);poll(MenuInput::Select,false,0);
    poll(0,false,0);page(2,0);poll(MenuInput::R,false,MenuInput::R,220,false); // Movie input untouched.
    poll(0,false,0);m.words[Sites[0].pc]^=1;poll(MenuInput::R,false,MenuInput::R);
    assert(!input.HiddenTouch(170,220));m.words[Sites[0].pc]^=1;

    // Reopen each native page directly, without an intermediate Items page.
    for(const auto remembered:{MenuInput::Target::Items,MenuInput::Target::Gear,MenuInput::Target::Map}) {
        input={};m.words[UI+0x14]=1;m.words[0x54ac4c]=0x01000000;
        page(remembered==MenuInput::Target::Items?2:0,remembered==MenuInput::Target::Gear?2:0);
        m.words[0x506cb0+0x38]=remembered==MenuInput::Target::Map?2:0;
        poll(0,false,0);assert(input.last_page==remembered);
        poll(MenuInput::Select,true,0,30); // Native Back touch.
        poll(MenuInput::Select,true,0,30);
        m.words[0x54ac4c]=0;page(0,0); // Closing controllers are not a new Map page.
        poll(MenuInput::Select,false,0);assert(input.last_page==remembered);
        m.words[UI+0x14]=0;m.words[0x506cb0+0x38]=0;
        for(unsigned i=0;i<20;++i)poll(MenuInput::Select,false,0); // Held Select cannot reopen.
        poll(0,false,0);
        const unsigned x=remembered==MenuInput::Target::Map?170:remembered==MenuInput::Target::Gear?100:220;
        poll(MenuInput::Select,true,0,x);
        poll(MenuInput::Select,true,0,x);
        m.words[UI+0x14]=1;m.words[0x54ac4c]=0x01000000;
        page(remembered==MenuInput::Target::Items?1:0,remembered==MenuInput::Target::Gear?1:0);
        m.words[0x506cb0+0x38]=remembered==MenuInput::Target::Map?1:0;
        poll(MenuInput::Select,false,0); // Stop touching as soon as opening is accepted.
        for(unsigned i=0;i<20;++i)poll(MenuInput::Select,false,0); // Held opening key cannot close.
        assert(input.last_page==remembered);
        poll(0,false,0);
        m.words[OOT3D::SaveMenu::Owner+0x28]=2;
        poll(MenuInput::Select,false,0);assert(input.last_page==remembered);
        m.words[OOT3D::SaveMenu::Owner+0x28]=0;
    }

    // Hidden cards are collapsed in fresh geometry and absent from touch mapping.
    constexpr std::array<Part,4> cards{Back,GearTab,MapTab,ItemsTab};
    constexpr std::array<float,5> xs{0,64,140,198,256};
    for(unsigned q=0;q<4;++q) {
        const std::array<float,12> v{xs[q],202,0,xs[q+1],202,0,xs[q],240,0,xs[q+1],240,0};
        for(unsigned i=0;i<12;++i){m.words[src+48*q+4*i]=std::bit_cast<U32>(v[i]);m.words[dst+48*q+4*i]=0;}
    }
    SetLayout({},false);motion_x=motion_slots=motion_y=0;
    for(unsigned mode=0;mode<3;++mode) {
        const bool items=mode==0,gear=mode==1;page(items?2:0,gear?2:0);
        assert(CopyPositions(m,mesh,dst,src,192));
        for(unsigned q=0;q<4;++q) {
            const bool visible=q!=2 && (items || gear);
            assert(TabVisible(cards[q],items,gear)==visible);
            for(unsigned i=0;i<12;i+=3) {
                const U32 expected=visible?m.words.at(src+48*q+4*i):m.words.at(src+48*q);
                assert(m.words.at(dst+48*q+4*i)==expected);
            }
            const float x=(xs[q]+xs[q+1])/2,y=220;
            const auto hit=MapPointer(.5f+(x-160)*Scale/400,
                .5f+((y-120)*Scale+OffsetY)/240,400,{},items,gear);
            assert(bool(hit)==visible);
        }
    }
    std::cout<<"Pause page shortcuts, remembered tabs, Select close/reopen, held buttons, cancellations, fallback, hidden cards and touch targets passed\n";
}
