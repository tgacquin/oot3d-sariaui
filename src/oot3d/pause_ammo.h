// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"

namespace OOT3D::PauseMenu {
using TitleScreen::U32;
template<class M> bool CounterCandidate(M& m,U32 address,unsigned bytes) {
    if(!TitleScreen::Pointer(address,bytes))return false;
    if constexpr(requires {m.Mapped(address,bytes);})return m.Mapped(address,bytes);
    return true;
}
// Item sprite controllers own separate number renderers for ammunition.
// Those renderers upload during drawing, one level below the icon models.
template<class M,class F> U32 FindItemCounter(M& m,F match) {
    for(U32 off:{0x3cu,0x40u,0x44u,0x48u}) {
        const U32 owner=m.Read(0x5066f8+off).value_or(0);
        if(!CounterCandidate(m,owner,0x534))continue;
        const U32 icon=m.Read(owner+8).value_or(0),count=m.Read(owner+0x224).value_or(0);
        if(!CounterCandidate(m,icon,0x1e4) || m.Read(icon)!=std::optional<U32>{0x4ebd60} ||
           count==0 || count>64)continue;
        for(U32 i=0;i<count;++i) {
            const U32 counter=m.Read(owner+0xc+4*i).value_or(0);
            if(!CounterCandidate(m,counter,0x14))continue;
            const U32 model=m.Read(counter+0x10).value_or(0);
            if(CounterCandidate(m,model,0x1e4) && m.Read(model)==std::optional<U32>{0x4ebd60} &&
               match(counter,model))return counter;
        }
    }
    return 0;
}
}
