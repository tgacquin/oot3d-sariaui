// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

namespace OOT3D::FrameLights {
using U32 = std::uint32_t;
// A separate page-aligned extension after the randomizer's entire reservation.
// The cursor belongs to the guest so save/load and core switches preserve it.
constexpr U32 Base = 0xa00000, RecordBytes = 128, Records = 1024;
constexpr U32 Slots = Base + 0x1000, End = Slots + Records * RecordBytes;
constexpr U32 ResetPC = 0x4162f8, AllocatePC = 0x35bfc0, FirstSVC = 0xfff650;
constexpr U32 ResetWord = 0xe58d7010, AllocateWord = 0xe591000c;

inline U32 Instruction(U32 pc, U32 word) {
    if (pc == ResetPC && word == ResetWord) return 0xef000000 | FirstSVC;
    if (pc == AllocatePC && word == AllocateWord) return 0xef000000 | (FirstSVC + 1);
    return word;
}

template<class M> bool CodeMatches(M& m) {
    return m.Read(ResetPC) == ResetWord && m.Read(0x4162dc) == 0xe3a01901 &&
           m.Read(0x4162ec) == 0xe58d000c && m.Read(AllocatePC) == AllocateWord &&
           m.Read(AllocatePC + 4) == 0xe2400080 && m.Read(AllocatePC + 8) == 0xe581000c &&
           m.Read(AllocatePC + 12) == 0xe5c0300c && m.Read(0x35bfec) == 0xe12fff1e;
}

template<class M, class R> void Handle(M& m, R& r, U32 site) {
    if (site == 0) {
        m.Write(r[13] + 16, r[7]); // Displaced store of the native arena's end.
        m.Write(Base, 0);
        return;
    }
    // All eight native callers require a writable record (one returns it to
    // its caller, another passes it on to a shadow callback). Give records
    // frame lifetime and let the original routine initialize the lighting.
    const U32 count = m.Read(Base).value_or(Records);
    if (count < Records) {
        r[0] = Slots + count * RecordBytes;
        m.Write(Base, count + 1);
    } else {
        // Keep using distinct native records if both ends of its arena allow.
        const U32 head = m.Read(r[1] + 8).value_or(0);
        const U32 tail = m.Read(r[1] + 12).value_or(0);
        const U32 start = m.Read(r[1] + 4).value_or(0);
        const U32 size = m.Read(r[1]).value_or(0);
        if (start && head >= start && tail >= head && tail - start <= size &&
            tail - head >= RecordBytes) {
            r[0] = tail - RecordBytes;
            m.Write(r[1] + 12, r[0]);
        } else {
            // Isolate last-resort records from retained normal records. The
            // eight native return sites occupy distinct entries in this table.
            constexpr U32 callers[] = {0x1bf928,0x1c0354,0x2d5f98,0x2e2bf4,
                                       0x33e2c8,0x35c054,0x3b52b8,0x4638d4};
            unsigned slot = 0;
            while (slot < 8 && callers[slot] != r[14]) ++slot;
            r[0] = Base + (1 + slot) * RecordBytes;
        }
    }
    r[15] = AllocatePC + 12;
}
}
