// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_state.h"
#include "oot3d/title_hooks.h"
#include "oot3d/save_menu.h"

namespace OOT3D::Interpolation::MenuCursor {
using TitleScreen::U32;
struct Draw {
    const void* positions{};
    U32 indices{},owner{},focus{},detail{},epoch{};
    std::uint64_t frame{};
};
inline std::mutex mutex;
inline std::array<Draw,12> draws{};
inline unsigned next_draw{};

// Match the submitted float3 stream, including the replacement mesh used
// to draw the dungeon cursor above the floor cards.
template<class M,class Resolve>
void Record(M& m,U32 model,U32 owner,U32 focus,U32 detail,Resolve resolve) {
    const U32 mesh=m.Read(model+4).value_or(0);
    if(!TitleScreen::Pointer(mesh,0x1a8))return;
    const auto bank=m.Read(mesh+0x124).value_or(2);
    const U32 live=bank<2?m.Read(mesh+0x1a0+4*bank).value_or(0):0;
    if(!TitleScreen::Pointer(live,12))return;
    Draw draw{resolve(live),m.Read(model+0x174).value_or(0),owner,focus,detail,
              generation.load(),guest_frames.load()};
    if(!draw.positions || !draw.indices)return;
    std::scoped_lock lock{mutex};
    draws[next_draw++%draws.size()]=draw;
}

template<class M,class Resolve>
void Capture(M& m,MenuPage page,U32 model,U32 source,Resolve resolve) {
    constexpr U32 items=0x5066f8,gear=0x50446c,map=0x506cb0,file=0x504fa0,name=0x5077f0;
    if(!TitleScreen::Pointer(source,8) || !TitleScreen::Pointer(model,0x178))return;
    const auto match=[&](U32 slot) -> U32 {
        const U32 owner=m.Read(slot).value_or(0);
        if(!TitleScreen::Pointer(owner,12) || m.Read(owner+4)!=source)return 0;
        const auto mesh=m.Read(source+4);
        return mesh && TitleScreen::Pointer(*mesh,0x1a8) && m.Read(owner)==mesh ? owner : 0;
    };
    if(page==MenuPage::Save) {
        if(const auto cursor=match(SaveMenu::Owner+0x18)) {
            const auto page_state=m.Read(SaveMenu::Owner+0x28).value_or(0);
            const auto row=m.Read(SaveMenu::Owner+(page_state==3?0x30:0x38)).value_or(0);
            const auto choice=m.Read(SaveMenu::Owner+0x34).value_or(0);
            Record(m,model,cursor,(page_state<<16)|row,choice,resolve);
        }
    } else if(page==MenuPage::Gear) {
        if(const auto owner=match(gear+0x14))
            Record(m,model,owner,m.Read(gear+0x20).value_or(27),0,resolve);
    } else if(page==MenuPage::Map) {
        // The two calls to 2fb934 in the native map draw use different
        // controllers. The blue frame and region markers have their own draws.
        if(const auto dungeon=match(map+0x1c))
            Record(m,model,dungeon,m.Read(map+0x30).value_or(~0u),0,resolve);
        else if(const auto world=match(map+0x4c))
            Record(m,model,world,m.Read(map+0x48).value_or(~0u),0,resolve);
    } else if(page==MenuPage::NameEntry) {
        if(const auto owner=match(name)) {
            const auto mode=m.Read(name+8).value_or(0),row=m.Read(name+0x14).value_or(0);
            const auto column=m.Read(name+(mode==4?0x2c:row==5?0x28:0x10)).value_or(0);
            Record(m,model,owner,(mode<<16)|row,column,resolve);
        }
    } else if(page==MenuPage::Items || page==MenuPage::FileSelect) {
        // 2f7af4 sets the logical origin before the corner and panel transforms.
        const auto origin=[&](U32 owner) {
            const auto helper=m.Read(owner+8).value_or(0);
            if(!TitleScreen::Pointer(helper,0x20))return;
            const auto at=m.Read(helper+0x1c).value_or(0);
            if(!TitleScreen::Pointer(at,8))return;
            const auto x=m.Read(at),y=m.Read(at+4);
            if(x && y)Record(m,model,owner,*x,*y,resolve);
        };
        if(page==MenuPage::FileSelect) {
            if(const auto owner=match(file+0x34)) {
                // Different file actions can reuse the same native origin and
                // move the cursor with the transform of a different stone.
                const auto state=m.Read(file+0x10).value_or(0);
                U32 selection_at{};
                switch(state) {
                case 3: selection_at=file+0x14; break;
                case 8: case 10: selection_at=file+0x18; break;
                case 14: selection_at=file+0x1c; break;
                case 17: selection_at=file+0x20; break;
                case 37: selection_at=file+0x24; break;
                default: origin(owner); return;
                }
                Record(m,model,owner,state,m.Read(selection_at).value_or(~0u),resolve);
            }
        } else {
            for(U32 slot:{items+0x30,items+0x28,items+0x2c,items+0xb0,items+0xb4})
                if(const auto owner=match(slot)){origin(owner);return;}
        }
    }
}
inline std::array<U32,3> Selection(const void* positions,U32 count,std::uint64_t frame,U32 epoch) {
    if(!positions)return {};
    std::scoped_lock lock{mutex};
    for(unsigned i=0;i<draws.size();++i) {
        const auto& d=draws[(next_draw%draws.size()+draws.size()-1-i)%draws.size()];
        if(d.indices==count && d.epoch==epoch && frame>=d.frame && frame-d.frame<=1 &&
           positions==d.positions)return {d.owner,d.focus,d.detail};
    }
    return {};
}
}
