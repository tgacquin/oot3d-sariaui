// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/control_swap.h"
#include "oot3d/message_patch.h"
#include <array>
#include <memory>
#include <optional>
#include <unordered_map>
namespace OOT3D::ControlSwap {
struct Variants {
    std::uint32_t signature=0;
    std::unordered_map<std::uint32_t,std::uint32_t> entries,shoulder_entries,both_entries;
};
inline std::shared_ptr<const Variants> variants;
inline void ResetText() {std::atomic_store(&variants,std::shared_ptr<const Variants>{});}
// QM uses UTF-8 text interleaved with binary commands. Never search command
// arguments, which can contain arbitrary bytes including apparent words.
inline std::optional<std::vector<std::uint8_t>> SwapText(std::span<const std::uint8_t> text,
        bool start_select=true,bool shoulder_swap=false) {
    constexpr std::array<int,44> args{0,0,1,2,0,0,1,0,1,-1,1,0,0,-1,5,1,1,3,0,0,0,0,0,0,1,0,4,6,0,1,0,-1,-1,-1,-1,1,1,3,1,0,0,0,0,0};
    std::vector<std::uint8_t> out;
    std::size_t text_begin=0;
    const auto letter=[](unsigned char c){return (c>='A' && c<='Z') || (c>='a' && c<='z') || c=='_';};
    for(std::size_t at=0;at<text.size();) {
        if(text[at]==127) {
            if(at+1>=text.size() || text[at+1]>=args.size() || args[text[at+1]]<0)return {};
            const unsigned size=2+args[text[at+1]];
            if(size>text.size()-at)return {};
            if(shoulder_swap && text[at+1]==0x24 && (text[at+2]==0x0a || text[at+2]==0x0b)) {
                out.push_back('Z');out.push_back(text[at+2]==0x0a?'L':'R');
                at+=size;text_begin=at;continue;
            }
            out.insert(out.end(),text.begin()+at,text.begin()+at+size);
            if(!text[at+1]) {out.insert(out.end(),text.begin()+at+size,text.end());break;}
            at+=size;text_begin=at;continue;
        }
        bool replaced=false;
        for(const std::string_view word:{"START","SELECT","ZL","ZR","L","R"}) {
            if((word=="START" || word=="SELECT")?!start_select:!shoulder_swap)continue;
            if(word.size()>text.size()-at || (at>text_begin && letter(text[at-1])) ||
               (at+word.size()<text.size() && letter(text[at+word.size()])) ||
               !std::equal(word.begin(),word.end(),text.begin()+at))continue;
            // French elisions such as L'objet are words, not button labels.
            if(word=="L" && at+1<text.size() && (text[at+1]=='\'' || text[at+1]>=128))continue;
            const std::string_view other=word=="START"?"SELECT":word=="SELECT"?"START":
                word=="ZL"?"L":word=="ZR"?"R":word=="L"?"ZL":"ZR";
            out.insert(out.end(),other.begin(),other.end());at+=word.size();replaced=true;break;
        }
        if(!replaced)out.push_back(text[at++]);
    }
    return out;
}
inline bool PrepareText(std::vector<std::uint8_t>& bytes) {
    ResetText();using Messages::Read32;using Messages::Write32;
    if(bytes.size()<16 || bytes.size()>16*1024*1024 || Read32(bytes,0)!=0x4d51 || Read32(bytes,4)!=4)return false;
    const auto count=Read32(bytes,8);
    if(!count || count>(bytes.size()-16)/96)return false;
    auto result=bytes;auto next=std::make_shared<Variants>();
    for(unsigned mode=1;mode<=3;++mode)for(unsigned i=0;i<count;++i) {
        const auto entry=16+i*96,id=Read32(bytes,entry);
        std::vector<std::uint8_t> copy(bytes.begin()+entry,bytes.begin()+entry+96);bool changed=false;
        for(unsigned slot=0;slot<10;++slot) {
            const auto at=16+slot*8,off=Read32(copy,at),size=Read32(copy,at+4);
            if(!size)continue;
            if(off<16+count*96 || off>bytes.size() || size>bytes.size()-off)return false;
            const auto source=std::span(bytes).subspan(off,size);
            const auto swapped=SwapText(source,(mode&1)!=0,(mode&2)!=0);if(!swapped)return false;
            if(std::equal(swapped->begin(),swapped->end(),source.begin(),source.end()))continue;
            while(result.size()%4)result.push_back(0);
            Write32(copy,at,result.size());Write32(copy,at+4,swapped->size());
            result.insert(result.end(),swapped->begin(),swapped->end());changed=true;
        }
        if(!changed)continue;
        while(result.size()%4)result.push_back(0);
        auto& entries=mode==1?next->entries:mode==2?next->shoulder_entries:next->both_entries;
        if(!entries.emplace(id,result.size()).second)return false;
        result.insert(result.end(),copy.begin(),copy.end());
    }
    next->signature=result.size();
    for(auto word:{0x53574150u,0x53544152u})for(unsigned i=0;i<4;++i)result.push_back(word>>(8*i));
    bytes.swap(result);std::atomic_store(&variants,std::shared_ptr<const Variants>(next));return true;
}
template<class M,class R> bool SelectText(M& m,R& r) {
    const unsigned mode=(enabled.load()?1:0)|(shoulders.load()?2:0);
    if(!mode)return false;
    const auto v=std::atomic_load(&variants);if(!v)return false;
    const auto& entries=mode==1?v->entries:mode==2?v->shoulder_entries:v->both_entries;
    const auto it=entries.find(r[1]);if(it==entries.end())return false;
    const auto base=m.Read(r[0]+4).value_or(0);
    if(!base || m.Read(base)!=0x4d51 || m.Read(base+v->signature)!=0x53574150 ||
       m.Read(base+v->signature+4)!=0x53544152)return false;
    r[0]=base+it->second;r[15]=r[14];return true;
}
}
