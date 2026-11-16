// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/message_patch.h"
#include "oot3d/prices.h"
#include <string>

namespace OOT3D::Prices {
struct PriceEdit {unsigned offset,remove,cost;};
struct PriceMessage {
    U32 id,slot;
    std::uint64_t fingerprint;
    unsigned size;
    std::span<const PriceEdit> edits;
};
}
#include "oot3d/messages/price_patches.h"
namespace OOT3D::Prices {
inline unsigned ApplyText(std::vector<std::uint8_t>& bytes,unsigned factor) {
    using Messages::Read32;
    using Messages::Write32;
    if(factor<2 || factor>3 || bytes.size()<16 || bytes.size()>8*1024*1024 ||
       Read32(bytes,0)!=0x4d51 || Read32(bytes,4)!=4)return 0;
    const auto count=Read32(bytes,8);
    if(!count || count>4096 || count>(bytes.size()-16)/96)return 0;
    const auto table_end=16+count*96;
    std::vector<std::pair<std::size_t,const PriceMessage*>> targets;
    for(unsigned i=0;i<count;++i) {
        const auto entry=16+i*96,id=Read32(bytes,entry);
        for(unsigned slot:{1u,5u,7u}) {
            const auto at=entry+16+slot*8,off=Read32(bytes,at),size=Read32(bytes,at+4);
            if(size && (off<table_end || off>bytes.size() || size>bytes.size()-off))return 0;
            const auto found=std::find_if(std::begin(PriceMessages),std::end(PriceMessages),
                [&](const auto& m) {return m.id==id && m.slot==slot && m.size==size &&
                    Messages::Fingerprint(std::span(bytes).subspan(off,size))==m.fingerprint;});
            if(found==std::end(PriceMessages))continue;
            for(const auto& [previous,message]:targets)
                if(message->id==id && message->slot==slot)return 0;
            targets.emplace_back(at,found);
        }
    }
    // All languages must agree with gameplay. Unknown translations/revisions
    // leave the whole feature off, rather than charging an undisclosed price.
    if(targets.size()!=PriceMessageCount)return 0;
    auto patched=bytes;
    for(const auto& [at,message]:targets) {
        const auto off=Read32(bytes,at),size=Read32(bytes,at+4);
        const auto original=std::span(bytes).subspan(off,size);
        while(patched.size()%4)patched.push_back(0);
        const auto start=patched.size();unsigned cursor=0;
        for(const auto& edit:message->edits) {
            if(edit.offset<cursor || edit.offset>size || edit.remove>size-edit.offset)return 0;
            patched.insert(patched.end(),original.begin()+cursor,original.begin()+edit.offset);
            const auto price=std::to_string(Cost(edit.cost,factor));
            patched.insert(patched.end(),price.begin(),price.end());
            cursor=edit.offset+edit.remove;
        }
        patched.insert(patched.end(),original.begin()+cursor,original.end());
        Write32(patched,at,start);Write32(patched,at+4,patched.size()-start);
    }
    bytes.swap(patched);return targets.size();
}
}
