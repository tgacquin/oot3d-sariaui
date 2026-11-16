// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include "oot3d/message_patch.h"
#include "oot3d/rumble.h"
#include <memory>
namespace OOT3D::ShardText {
struct Patch {std::uint32_t id,slot;std::uint64_t fingerprint;unsigned size;std::span<const Messages::Edit> edits;};
}
#include "oot3d/messages/rumble_patches.h"
namespace OOT3D::ShardText {
struct Variants {std::array<std::uint32_t,2> entries{};std::uint32_t signature=0;};
inline Common::AtomicSharedPtr<const Variants> variants;
inline void Reset() {variants.store(std::shared_ptr<const Variants>{});}
inline bool Prepare(std::vector<std::uint8_t>& bytes) {
    Reset();using Messages::Read32;using Messages::Write32;
    if(bytes.size()<16 || bytes.size()>8*1024*1024 || Read32(bytes,0)!=0x4d51 || Read32(bytes,4)!=4)return false;
    const auto count=Read32(bytes,8);
    if(!count || count>(bytes.size()-16)/96)return false;
    auto result=bytes;auto next=std::make_shared<Variants>();
    unsigned found=0;
    for(unsigned i=0;i<count;++i) {
        const auto entry=16+i*96,id=Read32(bytes,entry);
        if(id!=0x68 && id!=0x76f)continue;
        const unsigned index=id==0x68?0:1;if(next->entries[index])return false;
        std::vector<std::uint8_t> copy(bytes.begin()+entry,bytes.begin()+entry+96);
        for(const auto& patch:Patches)if(patch.id==id) {
            const auto at=16+patch.slot*8,off=Read32(copy,at),size=Read32(copy,at+4);
            if(size!=patch.size || off<16+count*96 || off>bytes.size() || size>bytes.size()-off ||
               Messages::Fingerprint(std::span(bytes).subspan(off,size))!=patch.fingerprint)return false;
            while(result.size()%4)result.push_back(0);
            const auto start=result.size();unsigned cursor=0;
            for(const auto& edit:patch.edits) {
                if(edit.offset<cursor || edit.offset>size || edit.remove>size-edit.offset)return false;
                result.insert(result.end(),bytes.begin()+off+cursor,bytes.begin()+off+edit.offset);
                result.insert(result.end(),edit.insert.begin(),edit.insert.end());cursor=edit.offset+edit.remove;
            }
            result.insert(result.end(),bytes.begin()+off+cursor,bytes.begin()+off+size);
            Write32(copy,at,start);Write32(copy,at+4,result.size()-start);++found;
        }
        while(result.size()%4)result.push_back(0);
        next->entries[index]=result.size();result.insert(result.end(),copy.begin(),copy.end());
    }
    if(found!=6)return false;
    next->signature=result.size();for(auto word:{0x53484152u,0x4452554du})
        for(unsigned i=0;i<4;++i)result.push_back(word>>(i*8));
    bytes.swap(result);variants.store(std::shared_ptr<const Variants>(next));return true;
}
template<class M,class R> bool Select(M& m,R& r) {
    if(!Rumble::enabled.load() || (r[1]!=0x68 && r[1]!=0x76f))return false;
    const auto v=variants.load();const auto base=m.Read(r[0]+4).value_or(0);
    if(!v || !base || m.Read(base)!=0x4d51 || m.Read(base+v->signature)!=0x53484152 ||
       m.Read(base+v->signature+4)!=0x4452554d)return false;
    r[0]=base+v->entries[r[1]==0x68?0:1];r[15]=r[14];return true;
}
}
