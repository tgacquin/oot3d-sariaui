// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <optional>
#include "oot3d/sun_readback_probe.h"
#include "oot3d/sun_visibility.h"

namespace OOT3D::SunReadbackProbe {
constexpr U32 Entry = 0x4590f8, Load = 0x45924c, FirstSVC = 0xfff1a6;
constexpr U32 EntryWord = 0xe92d4ff0, LoadWord = 0xe7910100;
inline U32 Instruction(U32 pc, U32 word) {
    if(pc == Entry && word == EntryWord) return 0xef000000 | FirstSVC;
    if(pc == Load && word == LoadWord) return 0xef000000 | (FirstSVC + 1);
    return word;
}
// USA Rev 1: check the entire routine, its tiled address table pointer,
// visibility result write and mandatory unrelated tail call. Independent of
// all QoL hooks, so their guards cannot silently disable this experiment.
template<class M> bool CodeMatches(M& memory) {
    std::uint64_t hash = 14695981039346656037ULL;
    for(U32 pc = Entry; pc < 0x4592d0; pc += 4) {
        const auto word = memory.Read(pc);
        if(!word) return false;
        hash = (hash ^ *word) * 1099511628211ULL;
    }
    return hash == 0x34aa82cfcef53c72ULL;
}
struct Hook {
    Ticket query;
    SunVisibility::Reader reader;
    template<class M, class R> void Handle(M& memory, R& r, unsigned site, I64 now = Now(),
        bool optimized = false, std::uint64_t frame = 0, U32 generation = 0) {
        if(site == 0) {
            query = Take(now);
            if(auto* stats = query.Get()) {
                ++stats->queries;
                if(query.phase == 1 && !query.session->optimized) {
                    ++stats->bypasses;
                    // Occluded result; retain native flare fade/drawing and the
                    // unrelated callback tail. No depth pointer is even read.
                    memory.Write(0x531ed8, 0);
                    r[0] = r[1];r[15] = 0x47c944;return;
                }
                optimized = query.phase == 1 && query.session->optimized;
            }
            reader.Begin(memory, optimized, frame, generation);
            const U32 sp = r[13] - 36;
            for(unsigned i = 4; i < 12; ++i) memory.Write(sp + 4 * (i - 4), r[i]);
            memory.Write(sp + 32, r[14]);r[13] = sp;
        } else {
            // Native unsigned ARM address wrap, register and flags semantics.
            const U32 address = r[1] + (r[0] << 2);
            auto* stats = query.Get();
            if(const auto sample = reader.Read(r[1], r[0])) {
                r[0] = sample->value;
                if(sample->pending) {++SunVisibility::pending;if(stats) ++stats->pending;}
                else {
                    ++SunVisibility::cached;if(stats) ++stats->cached;
                    if((sample->value & 0xffffff) == 0xffffff) {
                        ++SunVisibility::clear;if(stats) ++stats->clear;
                    }
                }
                return;
            }
            ++SunVisibility::exact;
            const auto start = stats ? Now() : 0;
            ReadScope read_scope{query};
            r[0] = memory.ReadDepth(address);
            if(stats) {++stats->reads;stats->read_us += Now() - start;}
        }
    }
};
}
