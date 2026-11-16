// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"

namespace OOT3D::HeapDiagnostics {
using TitleScreen::U32;
constexpr U32 SVC=0xfff7e0;
constexpr TitleScreen::Site Site{0x33de5c,0xe28dd00c,0xe1a00004};
inline U32 Instruction(U32 pc,U32 word) {
    return pc==Site.pc && word==Site.instruction?0xef000000|SVC:word;
}
template<class M> bool CodeMatches(M& m) {
    return m.Read(Site.pc)==Site.instruction && m.Read(Site.pc+4)==Site.following &&
        m.Read(0x33de18)==0xe1a05000 && m.Read(0x33de50)==0xe1a04000 &&
        m.Read(0x313ce4)==0xeb00a84a;
}
// Read-only observations at the common heap allocator's return. Totals count
// requests, not live bytes: a high total alone does NOT establish a leak.
// Fixed capacity and bounded logging keep a failure from flooding the log.
struct Hook {
    struct Caller {U32 pc=0;std::uint64_t requests=0,bytes=0,failed=0;};
    std::array<Caller,128> callers{};
    std::array<U32,6> last_page{};
    std::uint64_t requests=0,bytes=0;
    unsigned failures=0,reports=0;
    template<class M,class R,class Log,class Detail>
    void Return(M& m,R& r,Log log,Detail detail) {
        U32 caller=m.Read(r[13]+20).value_or(0);
        if(caller==0x313ce8)caller=m.Read(r[13]+28).value_or(caller);
        const U32 size=r[5],result=r[4];
        r[13]+=12; // Displaced ADD sp,sp,#12. Preserve every other register/flag.
        ++requests;bytes+=size;
        for(auto& entry:callers)if(entry.pc==caller || !entry.pc) {
            entry.pc=caller;++entry.requests;entry.bytes+=size;entry.failed+=result==0 && size!=0;break;
        }
        const std::array<U32,6> page{
            m.Read(0x50a530).value_or(0),m.Read(0x50a52c).value_or(0),
            m.Read(0x50a544).value_or(0),m.Read(0x50672c).value_or(0),
            m.Read(0x504484).value_or(0),m.Read(0x506ce8).value_or(0)};
        const bool failed=result==0 && size!=0;
        if((failed && failures<4) || (page!=last_page && reports<128)) {
            log(caller,size,result,page,requests,bytes);++reports;
            if(failed)for(const auto& entry:callers)if(entry.requests)detail(entry);
        }
        if(failed)++failures;
        last_page=page;
    }
};
}
