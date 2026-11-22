// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include <string>

namespace OOT3D::AreaLoadTrace {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff620;
enum Site { FadeExit, TimedExit, AlternateExit, Initialize, Dispatch, Returned, OutgoingFade };
constexpr std::array<TitleScreen::Site,7> Sites{{
    {0x2e3758,0xe59f043c,0xe585000c}, // All three exits load the new state constructor.
    {0x2e38e8,0xe59f02ac,0xe3a06000},
    {0x2e39f4,0xe59f01a0,0xe3a06000},
    {0x4352f8,0xe59f02dc,0xe5901000}, // After the randomizer PlayInit call sets r5.
    {0x454a30,0xe5911008,0xe12fff31}, // Before the native BLX, even if it never returns.
    {0x454a38,0xe3500001,0x0a000007},
    {0x2e3060,0xe3a06000,0xe3a00001}, // Set up outgoing fade once, before the cover is drawn.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& site:Sites)
        if(m.Read(site.pc)!=site.instruction || m.Read(site.pc+4)!=site.following)return false;
    return m.Read(0x2e3b9c)==0x4352f0 && m.Read(0x4355dc)==0x587958 &&
           m.Read(0x454a24)==0xe5941058 && m.Read(0x454a28)==0xe5940000;
}
inline const char* StageName(U32 callback) {
    switch(callback) {
    case 0x448f0c:return "request scene info";
    case 0x448fb4:return "wait for scene info";
    case 0x449048:return "wait for scene archive";
    case 0x4490d8:return "initialize scene";
    case 0x4489f8:return "set up entrance";
    case 0x448c68:return "wait for room";
    case 0x448c88:return "set up camera";
    case 0x448e38:return "wait for loader worker";
    case 0x448ec8:return "wait for actors";
    case 0x449420:return "enter gameplay";
    default:return "scene setup";
    }
}
// 301300 copies the game asset path to the job's inline UTF-16 buffer at
// +0x14. Only report its filename, bounded by the next field at +0x114.
template<class ReadField> std::string AssetName(U32 job,ReadField read) {
    if(!TitleScreen::Pointer(job,0x118))return {};
    std::string name;
    for(unsigned offset=0x14;offset<0x114;offset+=4) {
        const U32 word=read(job,offset);
        if(word==~0u)return {};
        for(unsigned shift:{0u,16u}) {
            const U32 ch=(word>>shift)&0xffff;
            if(ch==0)return name;
            if(ch=='/' || ch=='\\')name.clear();
            else name.push_back(ch>=32 && ch<=126?static_cast<char>(ch):'?');
        }
    }
    return {}; // Incomplete or invalid job.
}
struct Trace {
    U32 owner=0,stage=~0u,callback=0,previous_result=~0u;
    std::int64_t started=0,last_report=0;
    bool Enter(U32 play,U32 index,U32 target,std::int64_t now) {
        if(play==owner && index==stage && target==callback && now>=last_report)return false;
        owner=play;stage=index;callback=target;previous_result=~0u;
        started=last_report=now;
        return true;
    }
    bool Result(U32 result,std::int64_t now) {
        // Coalesce a pending callback retried every game update.
        if(result==previous_result && now>=last_report && now-last_report<5000000)return false;
        previous_result=result;last_report=now;
        return true;
    }
};
}
