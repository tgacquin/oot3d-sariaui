// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include "oot3d/price_text.h"
#include "oot3d/loading_prompt.h"
#include "oot3d/shard_text.h"
#include "oot3d/control_swap_text.h"
#include <memory>
#include <unordered_map>

namespace OOT3D::Prices {
constexpr U32 TextSVC=0xfff410;
constexpr TitleScreen::Site TextSite{0x2df4c4,0xe52d4004,0xe5902004};
struct TextVariants {
    U32 signature=0;
    std::unordered_map<U32,std::array<U32,3>> entries;
};
inline Common::AtomicSharedPtr<const TextVariants> text_variants;
inline unsigned RequestedFactor() {
    return Difficulty::increase_prices.load()?unsigned(std::clamp(Difficulty::price_multiplier.load(),2,3)):1;
}
// Keep the normal archive table intact. Append both translated variants, so
// every textbox can select its own price version without reloading resources.
inline bool PrepareTextVariants(std::vector<std::uint8_t>& bytes, unsigned ordinary_factor) {
    auto variants=std::make_shared<TextVariants>();
    auto twice=bytes,thrice=bytes;
    if(!ApplyText(twice,2) || !ApplyText(thrice,3)) {
        text_variants.store(std::shared_ptr<const TextVariants>{});return false;
    }
    using Messages::Read32;using Messages::Write32;
    auto result=bytes;
    for(unsigned factor=0;factor<3;++factor) {
        const auto& source=factor==0?bytes:factor==1?twice:thrice;
        const auto count=Read32(source,8);
        for(unsigned i=0;i<count;++i) {
            const auto entry=16+i*96;
            bool changed=false;
            for(unsigned language:{1u,5u,7u})
                changed|=Read32(twice,entry+16+language*8)!=Read32(bytes,entry+16+language*8);
            if(!changed)continue;
            std::vector<std::uint8_t> copy(source.begin()+entry,source.begin()+entry+96);
            for(unsigned language:{1u,5u,7u}) {
                const auto at=16+language*8,offset=Read32(source,entry+at),size=Read32(source,entry+at+4);
                if(offset==Read32(bytes,entry+at))continue;
                while(result.size()%4)result.push_back(0);
                Write32(copy,at,result.size());
                result.insert(result.end(),source.begin()+offset,source.begin()+offset+size);
            }
            while(result.size()%4)result.push_back(0);
            variants->entries[Read32(copy,0)][factor]=result.size();
            result.insert(result.end(),copy.begin(),copy.end());
        }
    }
    // Preserve the ordinary game's startup-latched price table. Randomized
    // saves select appended entries, including the unscaled version.
    if(ordinary_factor>=2 && ordinary_factor<=3) {
        const auto count=Read32(result,8);
        for(unsigned i=0;i<count;++i) {
            const auto entry=16+i*96;
            const auto it=variants->entries.find(Read32(result,entry));
            if(it!=variants->entries.end())
                std::copy_n(result.begin()+it->second[ordinary_factor-1],96,result.begin()+entry);
        }
    }
    variants->signature=result.size();
    for(auto word:{0x50544f4fu,0x32454349u})
        for(unsigned i=0;i<4;++i)result.push_back(word>>(i*8));
    bytes.swap(result);
    text_variants.store(std::shared_ptr<const TextVariants>(std::move(variants)));
    return true;
}
template<class M> bool TextCodeMatches(M& m) {
    return m.Read(TextSite.pc)==TextSite.instruction && m.Read(TextSite.pc+4)==TextSite.following &&
        m.Read(0x2df4b0)==0xe3510000 && m.Read(0x2df4c0)==0xe12fff1e;
}
template<class M,class R> void SelectText(M& m,R& r) {
    if(LoadingPrompt::Select(m,r) || ShardText::Select(m,r) || ControlSwap::SelectText(m,r))return;
    const auto variants=text_variants.load();
    const auto base=m.Read(r[0]+4).value_or(0);
    const bool valid=Difficulty::file_rules.load() && variants && base && m.Read(base)==0x4d51 &&
        m.Read(base+variants->signature)==0x50544f4f &&
        m.Read(base+variants->signature+4)==0x32454349;
    if(valid) {
        const auto factor=RequestedFactor();
        Difficulty::active_price_multiplier=factor;Difficulty::price_text_ready=true;
        {
            const auto it=variants->entries.find(r[1]);
            if(it!=variants->entries.end()) {
                r[0]=base+it->second[factor-1];r[15]=r[14];return;
            }
        }
    }
    r[13]-=4;m.Write(r[13],r[4]); // Original PUSH on the ordinary lookup path.
}
}
