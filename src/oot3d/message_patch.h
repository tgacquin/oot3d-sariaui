// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace OOT3D::Messages {
constexpr std::uint64_t ProgramId=0x0004000000033500ULL;
enum class Language : unsigned {English=1,French=5,Spanish=7};
enum class Dependency {None,NaviUp,SongListStart};
struct Edit {unsigned offset,remove;std::string_view insert;};
struct Message {
    std::uint32_t id;
    std::uint64_t fingerprint;
    unsigned original_size;
    std::span<const Edit> edits;
    Dependency dependency;
};
// Enable staged tutorials only alongside their verified input implementation.
constexpr bool Ready(Dependency dependency) {
    return dependency==Dependency::None || dependency==Dependency::NaviUp ||
           dependency==Dependency::SongListStart;
}
inline std::uint32_t Read32(std::span<const std::uint8_t> b,std::size_t at) {
    return std::uint32_t(b[at])|(std::uint32_t(b[at+1])<<8)|
        (std::uint32_t(b[at+2])<<16)|(std::uint32_t(b[at+3])<<24);
}
inline void Write32(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)b[at+i]=value>>(i*8);
}
inline std::uint64_t Fingerprint(std::span<const std::uint8_t> b) {
    std::uint64_t hash=14695981039346656037ULL;
    for(auto v:b)hash=(hash^v)*1099511628211ULL;
    return hash;
}
}
#include "oot3d/messages/english_patches.h"
#include "oot3d/messages/french_patches.h"
#include "oot3d/messages/spanish_patches.h"

namespace OOT3D::Messages {
inline std::span<const Message> Catalog(Language language) {
    switch(language) {
    case Language::English:return English;
    case Language::French:return French;
    case Language::Spanish:return Spanish;
    }
    return {};
}
// Rebuild only the selected language's table pointers. Original payloads,
// other locales and entry metadata remain byte-for-byte intact. No ROM file
// is written; LayeredFS supplies the new bytes to the running game.
inline unsigned Apply(std::vector<std::uint8_t>& bytes,Language language=Language::English) {
    const auto catalog=Catalog(language);
    if(catalog.empty() || bytes.size()<16 || bytes.size()>4*1024*1024 ||
       Read32(bytes,0)!=0x4d51 || Read32(bytes,4)!=4)return 0;
    const auto count=Read32(bytes,8);
    if(!count || count>4096 || count>(bytes.size()-16)/96)return 0;
    const std::size_t table_end=16+count*96;
    std::vector<std::pair<std::size_t,const Message*>> targets;
    for(unsigned i=0;i<count;++i) {
        const std::size_t entry=16+i*96;
        for(unsigned slot=0;slot<10;++slot) {
            const auto off=Read32(bytes,entry+16+slot*8),size=Read32(bytes,entry+20+slot*8);
            if(size && (off<table_end || off>bytes.size() || size>bytes.size()-off))return 0;
        }
        const auto id=Read32(bytes,entry);
        const auto message=std::find_if(catalog.begin(),catalog.end(),
            [id](const auto& m){return m.id==id;});
        if(message==catalog.end() || !Ready(message->dependency))continue;
        for(const auto& [at,previous]:targets)if(previous==&*message)return 0;
        const auto at=entry+16+unsigned(language)*8;
        const auto off=Read32(bytes,at),size=Read32(bytes,at+4);
        if(size!=message->original_size ||
           Fingerprint(std::span(bytes).subspan(off,size))!=message->fingerprint)return 0;
        targets.emplace_back(at,&*message);
    }
    // Fail closed for a different revision or custom dialogue pack. Never
    // partly rewrite a tutorial when its source no longer matches the audit.
    const auto expected=std::count_if(catalog.begin(),catalog.end(),
        [](const auto& m){return Ready(m.dependency);});
    if(targets.size()!=expected)return 0;
    auto patched=bytes;
    for(const auto& [at,message]:targets) {
        const auto off=Read32(bytes,at),size=Read32(bytes,at+4);
        const auto original=std::span(bytes).subspan(off,size);
        while(patched.size()%4)patched.push_back(0);
        const auto start=patched.size();unsigned cursor=0;
        for(const auto& edit:message->edits) {
            if(edit.offset<cursor || edit.offset>size || edit.remove>size-edit.offset)return 0;
            patched.insert(patched.end(),original.begin()+cursor,original.begin()+edit.offset);
            patched.insert(patched.end(),edit.insert.begin(),edit.insert.end());
            cursor=edit.offset+edit.remove;
        }
        patched.insert(patched.end(),original.begin()+cursor,original.end());
        Write32(patched,at,start);Write32(patched,at+4,patched.size()-start);
    }
    bytes.swap(patched);return targets.size();
}
// Install the three localized catalogs as one transaction. Language switching
// then uses the appropriate native slot without touching another locale's text.
inline unsigned ApplyAll(std::vector<std::uint8_t>& bytes) {
    auto patched=bytes;unsigned total=0;
    for(auto language:{Language::English,Language::French,Language::Spanish}) {
        const auto count=Apply(patched,language);
        if(!count)return 0;
        total+=count;
    }
    bytes.swap(patched);return total;
}
}
