// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"

namespace OOT3D::GraphicsMemory {
using TitleScreen::U32;
constexpr U32 MiB=1024*1024, Extra=12*MiB;
constexpr U32 NativeLinear=27*MiB, NativeTextures=19*MiB;
constexpr U32 Linear=NativeLinear+Extra, Textures=NativeTextures+Extra;
// The supported loader already reports a 96 MiB application budget. Partition
// twelve of those MiB for graphics, in addition to the stock pool: 31 MiB
// inside a 39 MiB linear reservation, retaining 8 MiB for other SDK buffers.
// Change BOTH the reservation/subtraction and the subheap allocation/size:
// changing only the texture limit would let it overwrite adjacent GPU buffers.
constexpr std::array<TitleScreen::Site,4> Sites{{
    {0x416100,0xe240061b,0xe1a04620}, // SUB available RAM,27 MiB.
    {0x416114,0xe3a0061b,0xe1a05605}, // Reserve 27 MiB linear RAM.
    {0x4166e0,0xe3a01613,0xe58d3000}, // Allocate 19 MiB from linear parent.
    {0x4166f8,0xe3a02613,0xe1a00004}, // Construct graphics heap with same size.
}};
constexpr std::array<U32,4> Replacements{
    0xe2400627,0xe3a00627,0xe3a0161f,0xe3a0261f};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return Replacements[i];
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& site:Sites)
        if(m.Read(site.pc)!=site.instruction || m.Read(site.pc+4)!=site.following)return false;
    return m.Read(0x41611c)==0xebfbafcd && m.Read(0x416120)==0xe1a00004 &&
        m.Read(0x416124)==0xebfbafa6 && m.Read(0x4166ec)==0xebfc2efa &&
        m.Read(0x416700)==0xeb0002ce;
}
template<class M> bool Supported(M& m) {
    // Honor explicit lower-memory configurations; never undercut stock game's
    // heap on those configurations or silently change their process limit.
    return m.Read(0x1ff80040).value_or(0)>=96*MiB && CodeMatches(m);
}
struct FreeStats {U32 bytes=0,largest=0,blocks=0,capacity=0,start=0;bool valid=false;};
// Only call at the verified native font-upload completion site. An empty or
// unreadable resource is not proof of allocation failure.
template<class M> std::optional<U32> FailedFontAllocation(M& m,U32 resource) {
    if(!resource || resource>0xffffffafu || (resource&3))return {};
    const auto requested=m.Read(resource+0x24);
    if(requested && *requested && m.Read(resource+0x4c)==0)return requested;
    return {};
}
template<class M> FreeStats TextureFree(M& m) {
    constexpr U32 heap=0x5b4f7c;
    FreeStats stats;
    if(m.Read(heap)!=0x45585048)return stats; // Native EXPH header.
    const U32 start=m.Read(heap+0x18).value_or(0),end=m.Read(heap+0x1c).value_or(0);
    if(start<0x14000000 || end<=start || end>0x1c000000)return stats;
    stats.capacity=end-start;stats.start=start;
    U32 block=m.Read(heap+0x24).value_or(0),previous=0;
    while(block) {
        if(block<start || block>end-16 || (block&3) || ++stats.blocks>16384 ||
           m.Read(block)!=0x4652 || m.Read(block+8)!=previous)return {};
        const U32 size=m.Read(block+4).value_or(end);
        if(size>end-block-16 || size>end-start-stats.bytes)return {};
        stats.bytes+=size;stats.largest=std::max(stats.largest,size);
        previous=block;block=m.Read(block+12).value_or(end);
    }
    if(m.Read(heap+0x28)!=previous)return {};
    stats.valid=true;return stats;
}
// Read-only, one sample per emulated second at the frame boundary. Peaks are
// sampled, not exact allocation peaks. Keep reporting for the whole session;
// initial retained resources are not by themselves evidence of a leak.
struct Monitor {
    using Time=std::int64_t;
    Time sample_at=-1,report_at=-1,failure_at=-1;
    U32 capacity=0,start=0,peak_occupied=0,min_largest=0;
    std::uint64_t failures=0,samples=0;
    bool pressure=false;
    struct Sample {
        FreeStats free;
        U32 occupied,peak_occupied,min_largest;
        std::uint64_t samples,failures;
        bool pressure;
    };
    Sample latest{};
    template<class M> std::optional<Sample> Poll(M& m,Time now) {
        if(sample_at>=0 && now<sample_at)*this={}; // Save-state/time rollback.
        if(sample_at>=0 && now-sample_at<1000000)return {};
        sample_at=now;
        const auto free=TextureFree(m);
        if(!free.valid) {
            latest={free,0,peak_occupied,min_largest,samples,failures,false};
            if(report_at>=0 && now-report_at<30000000)return {};
            report_at=now;
            return latest;
        }
        if(capacity!=free.capacity || start!=free.start) {
            capacity=free.capacity;start=free.start;
            peak_occupied=0;min_largest=free.largest;samples=0;report_at=-1;
        }
        ++samples;
        const U32 occupied=capacity-free.bytes; // Includes allocator overhead.
        peak_occupied=std::max(peak_occupied,occupied);
        min_largest=std::min(min_largest,free.largest);
        const bool next_pressure=free.bytes<MiB || free.largest<64*1024;
        const bool changed=pressure!=next_pressure;
        pressure=next_pressure;
        latest={free,occupied,peak_occupied,min_largest,samples,failures,pressure};
        // At most one report per 5s during pressure changes, 30s otherwise.
        if(report_at>=0 && now-report_at<30000000 &&
           !(changed && now-report_at>=5000000))return {};
        report_at=now;
        return latest;
    }
    bool AllocationFailed(Time now) {
        ++failures;
        if(failure_at>=0 && now>=failure_at && now-failure_at<5000000)return false;
        failure_at=now;return true;
    }
};
static_assert(Linear-Textures==NativeLinear-NativeTextures);
}
