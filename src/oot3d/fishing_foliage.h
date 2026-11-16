// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_pose.h"
#include "oot3d/title_hooks.h"

namespace OOT3D::Interpolation::Pose::FishingFoliage {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff610,Props=0x59df84;
constexpr std::array<TitleScreen::Site,2> Sites{{
    {0x1c05f8,0xe59504a0,0xeb06c6f7},
    {0x1c0964,0xe5950520,0xeb06c61c},
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    return m.Read(0x1c06d8)==Props && m.Read(0x3fee28)==0xe5842014;
}
// Owned solely by the emulation thread. Tags live for one native frame and
// identify the prop before its pooled handle's world matrix is submitted.
class Tags {
    struct Entry {
        U32 renderer{},skeleton{},owner{},prop{},play{},type{};
        std::uint64_t frame{};unsigned epoch{};std::int64_t time{};
    };
    std::vector<Entry> entries;
public:
    template<class M,class R> void Handle(M& m,R& r,unsigned index,std::int64_t now) {
        if(index>=Sites.size())return;
        const U32 offset=index?0x520:0x4a0;
        r[0]=m.Read(r[5]+offset).value_or(0); // Displaced LDR; all other CPU state untouched.
        const auto frame=guest_frames.load();const auto epoch=generation.load();
        const U32 renderer=m.Read(r[0]+0x14).value_or(0);
        std::erase_if(entries,[&](const Entry& e) {
            return e.frame!=frame || e.epoch!=epoch || e.renderer==renderer || now<e.time;
        });
        const U32 play=m.Read(0x5043e0).value_or(0),owner=r[10],prop=r[4],slot=r[index?8:7];
        if(!TitleScreen::Pointer(play,0x108) || (m.Read(play+0x104).value_or(0)&65535)!=73 ||
           !TitleScreen::Pointer(owner,0x63c) || (m.Read(owner).value_or(0)&65535)!=0xfe ||
           slot>=(index?71u:32u) || r[5]!=owner+slot*4 ||
           prop<Props || prop>=Props+103*0x40 || (prop-Props)%0x40 ||
           (m.Read(prop+0x30).value_or(0)&255)!=index+1 ||
           !TitleScreen::Pointer(r[0],0xb0) || !TitleScreen::Pointer(renderer,0x480) ||
           m.Read(renderer)!=0x4ebd98)return;
        const auto alpha=std::bit_cast<float>(m.Read(prop+0x3c).value_or(0));
        const auto skeleton=m.Read(renderer+8).value_or(0);
        if(!std::isfinite(alpha) || alpha<=0 || alpha>1 || !TitleScreen::Pointer(skeleton,0x34))return;
        if(entries.size()>=103)entries.erase(entries.begin());
        entries.push_back({renderer,skeleton,owner,prop,play,index+1,frame,epoch,now});
    }
    void Attach(Sample& sample) const {
        if(!FishingPlant(sample))return;
        for(const auto& e:entries) {
            if(e.renderer!=sample.key.renderer || e.skeleton!=sample.key.skeleton ||
               e.play!=sample.camera.owner || e.frame!=sample.frame || e.epoch!=sample.camera.epoch ||
               sample.time<e.time || sample.time-e.time>50000 ||
               e.type!=(sample.model=="turi_kusa_model"?1u:2u))continue;
            sample.instance_owner=e.owner;sample.instance=e.prop;return;
        }
    }
};
inline Tags tags;
}
