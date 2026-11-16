// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>

namespace OOT3D::Randomizer::ResourceDiagnostics {
// Validate object archives before ZAR_Init follows their offsets or allocates
// texture handles. A damaged CMAB texture count otherwise turns into an
// unbounded allocation/write loop (the 2026-11-06 game-entry crash).
struct ArchiveCheck {
    const char* error=nullptr;
    std::uint32_t offset=0,index=0;
    explicit operator bool() const {return error==nullptr;}
};
template<class M> ArchiveCheck CheckArchive(M& m,std::uint32_t base,std::uint32_t size) {
    using U32=std::uint32_t;
    const auto fits=[&](std::uint64_t at,std::uint64_t bytes) {
        return at<=size && bytes<=size-at;
    };
    if(base<0x100000 || (base&3) || size<32 || std::uint64_t(base)+size>0x40000000)
        return {"invalid archive buffer",0,0};
    const auto word=[&](U32 at)->std::optional<U32> {
        if(!fits(at,4))return {};
        const auto lo=m.Read(base+(at&~3u));if(!lo)return {};
        if(!(at&3))return lo;
        const auto hi=m.Read(base+(at&~3u)+4);if(!hi)return {};
        const unsigned shift=(at&3)*8;return (*lo>>shift)|(*hi<<(32-shift));
    };
    if(word(0)!=0x0152415a)return {"invalid ZAR header",0,0};
    const auto declared=word(4),counts=word(8),types=word(12),files=word(16),data=word(20);
    if(!declared || *declared<32 || *declared>size || !counts || !types || !files || !data)
        return {"truncated ZAR header",4,0};
    const U32 nt=*counts&0xffff,nf=*counts>>16;
    if(!fits(*types,std::uint64_t(nt)*16) || !fits(*files,std::uint64_t(nf)*8) ||
       !fits(*data,std::uint64_t(nf)*4))return {"ZAR tables outside buffer",12,0};
    for(U32 i=0;i<nt;++i) {
        const auto at=*types+i*16;
        const auto n=word(at),list=word(at+4),name=word(at+8);
        if(!n || !list || !name || *n>nf || (*n && !fits(*list,std::uint64_t(*n)*4)) || !fits(*name,1))
            return {"invalid ZAR file-type table",at,i};
        for(U32 j=0;j<*n;++j) {
            const auto index=word(*list+j*4);
            if(!index || *index>=nf)return {"invalid ZAR file index",*list+j*4,i};
        }
    }
    for(U32 i=0;i<nf;++i) {
        const auto length=word(*files+i*8),name=word(*files+i*8+4),at=word(*data+i*4);
        if(!length || !name || !at || !fits(*at,*length) || !fits(*name,1))
            return {"ZAR member outside buffer",*files+i*8,i};
        if(*length<4 || word(*at)!=0x62616d63)continue; // cmab
        const auto inside=[&](std::uint64_t off,std::uint64_t bytes) {
            return off<=*length && bytes<=*length-off;
        };
        if(*length<32)return {"truncated CMAB header",*at,i};
        const auto n=word(*at+16),animations=word(*at+20),textures=word(*at+28);
        if(!n || !animations || !textures)return {"unreadable CMAB header",*at,i};
        if(!*n)continue;
        if(!inside(*animations,20))return {"CMAB animation table outside member",*at+20,i};
        const auto relative=word(*at+*animations+16);
        if(!relative)return {"unreadable CMAB texture table",*at+*animations+16,i};
        if(!*relative)continue;
        const std::uint64_t table=std::uint64_t(*animations)+*relative;
        if(!inside(table,8))return {"CMAB texture table outside member",*at+*animations+16,i};
        const auto count=word(*at+U32(table)+4);
        if(!count || !inside(table+8,std::uint64_t(*count)*24))
            return {"CMAB texture count exceeds member",*at+U32(table)+4,i};
        for(U32 t=0;t<*count;++t) {
            const U32 entry=*at+U32(table)+8+t*24;
            const auto bytes=word(entry),offset=word(entry+16);
            if(!bytes || !offset || !inside(std::uint64_t(*textures)+*offset,*bytes))
                return {"CMAB texture data outside member",entry,i};
        }
    }
    return {};
}
// The native async request stores a 128-code-unit UTF-16 filename, not ASCII.
template<class M> std::u16string Path(M& m, std::uint32_t address) {
    std::u16string result;
    for (unsigned i = 0; i < 128; ++i) {
        const auto at = address + 2 * i;
        const auto word = m.Read(at & ~3u);
        if (!word) break;
        const auto character = char16_t(*word >> ((at & 3) * 8));
        if (!character) break;
        result.push_back(character);
    }
    return result;
}

struct Arena {
    std::uint64_t used = 0, free = 0;
    std::uint32_t largest_free = 0, blocks = 0, stopped_at = 0;
    bool complete = false;
};

// Native SystemArena uses 16-byte linked headers: magic/free, size, next, prev.
// Inspect only; never repair a damaged list or follow it without bounds.
template<class M> Arena InspectArena(M& m, std::uint32_t arena = 0x565500) {
    Arena result;
    const auto head = m.Read(arena);
    if (!head) return result;
    std::unordered_set<std::uint32_t> visited;
    auto node = *head;
    std::uint32_t previous = 0;
    while (node && result.blocks < 16384) {
        result.stopped_at = node;
        if ((node & 15) || node > 0xfffffff0u || !visited.insert(node).second) return result;
        const auto flags = m.Read(node), size = m.Read(node + 4);
        const auto next = m.Read(node + 8), prev = m.Read(node + 12);
        if (!flags || !size || !next || !prev || (*flags & 0xffff) != 0x7373 ||
            (*flags >> 16) > 1 || *prev != previous) return result;
        ++result.blocks;
        if (*flags >> 16) {
            result.free += *size;
            result.largest_free = std::max(result.largest_free, *size);
        } else {
            result.used += *size;
        }
        previous = node;
        node = *next;
    }
    result.stopped_at = node;
    result.complete = node == 0;
    return result;
}
} // namespace OOT3D::Randomizer::ResourceDiagnostics
