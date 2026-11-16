// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"

namespace OOT3D::PauseMenu {
// Use short native tab touches for Select -> last page and the non-wrapping
// Gear <-> Items <-> Map order. Native transitions own setup, sound and cleanup.
// Coordinates are native, independent of the editor's positions and scaling.
struct MenuInput {
    enum class Target { None, OpenLast, Close, Map, Items, Gear };
    static constexpr U32 Select=4, Start=8, B=2, R=0x100, L=0x200;
    U32 previous=0,consumed=0;
    unsigned ticks=0;
    unsigned touch_x=220;
    Target target=Target::None;
    Target last_page=Target::Items;
    bool pause_tabs=false,map_page=false;
    void Cancel() {ticks=0;target=Target::None;}
    bool HiddenTouch(unsigned x,unsigned y) const {
        return pause_tabs && x<256 && y>=198 && y<240 &&
            (map_page || (x>=140 && x<198));
    }
    template<class M> bool Filter(M& m,U32& buttons,bool live) {
        const U32 raw=buttons,edges=raw&~previous;previous=raw;
        pause_tabs=map_page=false;
        if(!live || !CodeMatches(m)) {Cancel();consumed=0;return false;}
        consumed&=raw;buttons&=~consumed;
        const auto play=m.Read(UI+0xc).value_or(0);
        if(!TitleScreen::Pointer(play,0x104) || (m.Read(play+0x100).value_or(0)&255)!=3 ||
           m.Read(UI+0x20).value_or(1)!=0) {Cancel();return false;}
        buttons&=~Select;
        if(SaveMenu::Active(m)) {Cancel();return false;}
        pause_tabs=Active(m);
        const auto items=m.Read(0x5066f8+0x34).value_or(0);
        const auto gear=m.Read(0x50446c+0x18).value_or(0);
        map_page=pause_tabs && items==0 && gear==0;
        const bool closing=Closing(m);
        // Remember real pages only. During cleanup both tab controllers can
        // be zero without the map being open.
        if(pause_tabs && !closing) {
            if(items==2 && gear==0)last_page=Target::Items;
            else if(gear==2 && items==0)last_page=Target::Gear;
            else if(map_page && m.Read(0x506cb0+0x38).value_or(0)!=0)last_page=Target::Map;
        }
        if(raw&(B|Start)) {Cancel();return false;}
        if((target==Target::OpenLast && pause_tabs) ||
           (target==Target::Close && (!pause_tabs || closing)) ||
           (target==Target::Map && (!pause_tabs || map_page || gear!=0)) ||
           (target==Target::Gear && (!pause_tabs || gear!=0 || map_page)) ||
           (target==Target::Items && (!pause_tabs || items!=0)))Cancel();
        const auto shoulder=raw&(L|R);
        // Swallow outward presses so native handlers cannot wrap at the ends.
        if((pause_tabs && gear==2 && items==0 && shoulder==L) ||
           (map_page && shoulder==R))consumed|=shoulder;
        if(!ticks) {
            if((edges&Select) && pause_tabs && !closing)
                target=Target::Close;
            else if((edges&Select) && !pause_tabs && m.Read(0x50af34+0x34).value_or(0)==2)
                target=Target::OpenLast;
            else if(pause_tabs && items==2 && gear==0 && (raw&(L|R))==R && (edges&R)) {
                target=Target::Map;consumed|=R;
            } else if(pause_tabs && items==2 && gear==0 && shoulder==L && (edges&L)) {
                target=Target::Gear;consumed|=L;
            } else if(pause_tabs && gear==2 && items==0 && shoulder==R && (edges&R)) {
                target=Target::Items;consumed|=R;
            } else if(map_page && (raw&(L|R))==L && (edges&L)) {
                target=Target::Items;consumed|=L;
            }
            if(target!=Target::None)ticks=10;
        }
        buttons&=~consumed;
        const auto touch_target=target==Target::OpenLast?last_page:target;
        touch_x=touch_target==Target::Close?30:touch_target==Target::Map?170:
            touch_target==Target::Gear?100:220;
        if(ticks) {if(--ticks==0)target=Target::None;return true;}
        return false;
    }
};
inline MenuInput menu_input;
} // namespace OOT3D::PauseMenu
