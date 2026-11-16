// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"
namespace OOT3D::PauseSelection {
using TitleScreen::U32;
struct State {
    struct Selection {U32 owner=0,index=~0u;std::int64_t start=0;};
    std::array<Selection,3> selections{};
    template<class M> void Track(M& m,std::int64_t now) {
        if(!PauseMenu::Active(m) || PauseMenu::Closing(m)) {selections={};return;}
        for(unsigned page=0;page<2;++page) {
            const U32 root=page?0x50446c:0x5066f8;
            const bool active=m.Read(root+(page?0x18:0x34)).value_or(0)!=0;
            const U32 owner=active?m.Read(root+(page?0x10:0x3c)).value_or(0):0;
            const U32 index=m.Read(root+(page?0x20:0x64)).value_or(~0u);
            auto& s=selections[page];
            if(s.owner!=owner || s.index!=index || now<s.start)s={owner,index,now};
        }
        const U32 root=0x5066f8;
        const U32 owner=PauseMenu::ArrowChooserOpen(m)?m.Read(root+0x40).value_or(0):0;
        const U32 x=m.Read(root+0x9c).value_or(~0u),y=m.Read(root+0xa0).value_or(~0u);
        const U32 index=x<2 && y<2?x+2*y:~0u;
        auto& arrow=selections[2];
        if(arrow.owner!=owner || arrow.index!=index || now<arrow.start)arrow={owner,index,now};
    }
    static float Scale(std::int64_t elapsed,float duration,float enlarged=50.f) {
        if(!std::isfinite(duration) || duration<=0)return 1;
        const float t=std::clamp(float(elapsed)/1000000.f/std::clamp(duration,.001f,1.f),0.f,1.f);
        return (42.f+(enlarged-42.f)*t*t*(3-2*t))/enlarged;
    }
    template<class M,class R> void Apply(M& m,R& r,std::int64_t now) {
        r[0]=m.Read(r[4]+0x10c).value_or(0); // Native LDR at 2eb494.
        Track(m,now);
        const float duration=PauseMenu::GetLayout().selection_grow_seconds;
        if(!std::isfinite(duration) || duration<=0)return;
        for(unsigned page=0;page<selections.size();++page) {
            const auto& s=selections[page];
            if(!s.owner || r[4]!=s.owner || r[7]!=s.index || s.index>=64)continue;
            std::array<float,12> v;
            if(!TitleScreen::ReadFloats(m,r[13]+4,v))return;
            // Native Items: 42 -> 52, offsets -5 (434bcc..434c00).
            // Native Gear: 42 -> 50, offsets -4 (2f7b44).
            // Arrow types: 42 -> 52, offsets -5 (446710..44688c).
            // Excludes cursor corners, equip flights and 3D models.
            const float enlarged=page==1?50.f:52.f;
            if(v[3]-v[0]!=enlarged || v[7]-v[1]!=enlarged || v[4]!=v[1] ||
               v[6]!=v[0] || v[9]!=v[3] || v[10]!=v[7])return;
            const float scale=Scale(now-s.start,duration,enlarged),cx=(v[0]+v[3])*.5f,cy=(v[1]+v[7])*.5f;
            for(unsigned i=0;i<12;i+=3) {
                m.Write(r[13]+4+i*4,std::bit_cast<U32>(cx+(v[i]-cx)*scale));
                m.Write(r[13]+8+i*4,std::bit_cast<U32>(cy+(v[i+1]-cy)*scale));
            }
            return;
        }
    }
};
}
