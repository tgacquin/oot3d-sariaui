// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/graphics_memory.h"
#include <map>
#include <unordered_map>
#include <vector>

namespace OOT3D::GraphicsAllocations {
using TitleScreen::U32;
using Time=std::int64_t;
constexpr U32 Heap=0x5b4f7c,FirstSVC=0xfff7e1;
// Observe the common allocator after its result/accounting, and the SDK free
// entry. No allocations, stack redirects or extra buffers in guest memory.
constexpr std::array<TitleScreen::Site,2> Sites{{
    {0x322338,0xe8bd81f0,0xe92d41f0}, // POP {r4-r8,pc}.
    {0x47d99c,0xe2411010,0xe52de004}, // SUB r1,r1,#16.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|FirstSVC+i;
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    return m.Read(0x3222dc)==0xe92d41f0 && m.Read(0x322324)==0xeb044f66 &&
        m.Read(0x322328)==0xe3500000 && m.Read(0x47d9f0)==0xeb001e93;
}
// Raw native state values are deliberately retained rather than guessing a
// UI label. Scene 0xffff means no live gameplay owner (e.g. file selection).
using Context=std::array<U32,7>; // scene, file, save page/state, items, gear, map.
template<class M> Context ReadContext(M& m) {
    U32 scene=0xffff;
    const U32 play=m.Read(0x5043e0).value_or(0);
    if(TitleScreen::Pointer(play,0x108) && (m.Read(play+0x100).value_or(0)&0xffff)==0x203)
        scene=m.Read(play+0x104).value_or(0xffff)&0xffff;
    return {scene,m.Read(0x504fb0).value_or(0),m.Read(0x50a530).value_or(0),
        m.Read(0x50a52c).value_or(0),m.Read(0x50672c).value_or(0),
        m.Read(0x504484).value_or(0),m.Read(0x506ce8).value_or(0)};
}
struct Key {
    U32 caller=0;
    // Valid-looking return addresses on the stack, NOT an unwound backtrace.
    std::array<U32,3> hints{};
    Context origin{};
    auto operator<=>(const Key&) const=default;
};
struct Caller {
    Key key{};
    std::uint64_t allocations=0,frees=0,failures=0,live_bytes=0,peak_bytes=0,previous_bytes=0;
    U32 live_count=0;
    Time first=0;
    std::int64_t Delta() const {return std::int64_t(live_bytes)-std::int64_t(previous_bytes);}
};
struct Live {U32 size;unsigned caller;};
struct Tracker {
    static constexpr unsigned MaxLive=16384,MaxCallers=512;
    std::unordered_map<U32,Live> live;
    std::map<Key,unsigned> indices;
    std::vector<Caller> callers;
    Time last_time=-1,report_at=-1;
    U32 start=0,end=0;
    unsigned generation=0;
    std::uint64_t allocations=0,frees=0,failures=0,live_bytes=0,dropped=0,unknown_frees=0,reused=0;
    std::uint64_t reported_failures=0;
    Context reported_context{};
    Key failed_key{};
    U32 failed_size=0;
    template<class M> bool Check(M& m,Time now) {
        if(m.Read(Heap)!=0x45585048)return false;
        const U32 a=m.Read(Heap+0x18).value_or(0),b=m.Read(Heap+0x1c).value_or(0);
        if(a<0x14000000 || b<=a || b>0x1c000000)return false;
        if(a!=start || b!=end || (last_time>=0 && now<last_time)) {
            const unsigned next=generation+1;*this={};generation=next;start=a;end=b;
        }
        last_time=now;return true;
    }
    template<class M> Key Origin(M& m,U32 sp) {
        Key key;key.caller=m.Read(sp+20).value_or(0);key.origin=ReadContext(m);
        unsigned count=0;
        for(unsigned i=24;i<152 && count<key.hints.size();i+=4) {
            const U32 pc=m.Read(sp+i).value_or(0);
            if(pc<0x100004 || pc>=0x500000 || (pc&3) || pc==key.caller ||
               std::find(key.hints.begin(),key.hints.end(),pc)!=key.hints.end())continue;
            const auto instruction=m.Read(pc-4);if(!instruction)continue;
            if((*instruction&0x0f000000)==0x0b000000 || (*instruction&0x0ffffff0)==0x012fff30)
                key.hints[count++]=pc;
        }
        return key;
    }
    unsigned CallerIndex(const Key& key,Time now) {
        if(auto i=indices.find(key);i!=indices.end())return i->second;
        if(callers.size()==MaxCallers)return MaxCallers;
        const unsigned index=callers.size();indices.emplace(key,index);
        callers.push_back({key});callers.back().first=now;return index;
    }
    void Release(U32 address,bool actual=true) {
        const auto found=live.find(address);
        if(found==live.end()){if(actual)++unknown_frees;return;}
        const auto allocation=found->second;auto& c=callers[allocation.caller];
        c.live_bytes-=allocation.size;--c.live_count;live_bytes-=allocation.size;
        if(actual){++c.frees;++frees;}else ++reused;
        live.erase(found);
    }
    void Allocate(const Key& key,U32 size,U32 address,Time now) {
        ++allocations;
        const unsigned index=CallerIndex(key,now);
        if(!address){++failures;failed_key=key;failed_size=size;}
        if(address && live.contains(address))Release(address,false);
        if(index==MaxCallers){++dropped;return;}
        auto& c=callers[index];++c.allocations;
        if(!address){++c.failures;return;}
        // Unexpected reuse is explicitly counted, never disguised as a free.
        if(address<start || std::uint64_t(address)+size>end || live.size()==MaxLive){++dropped;return;}
        live.emplace(address,Live{size,index});live_bytes+=size;c.live_bytes+=size;++c.live_count;
        c.peak_bytes=std::max(c.peak_bytes,c.live_bytes);
    }
    template<class M,class R> bool Handle(M& m,R& r,unsigned site,Time now) {
        bool failed=false;
        if(site==0) {
            if(r[4]==Heap-0x18 && Check(m,now)) {
                const auto key=Origin(m,r[13]);
                Allocate(key,r[7],r[0],now);failed=r[0]==0;
            }
            const U32 sp=r[13];
            for(unsigned i=0;i<5;++i)r[4+i]=m.Read(sp+i*4).value_or(0);
            r[15]=m.Read(sp+20).value_or(0);r[13]=sp+24; // Original POP, flags unchanged.
        } else {
            if(r[0]==Heap && Check(m,now))Release(r[1]);
            r[1]-=16; // Original SUB, flags unchanged; native free still runs.
        }
        return failed;
    }
    template<class M,class Summary,class Detail>
    void Report(M& m,Time now,Summary summary,Detail detail) {
        if(!Check(m,now))return;
        const auto context=ReadContext(m);
        const bool failed=failures!=reported_failures,changed=context!=reported_context;
        if(report_at>=0 && !(failed && reported_failures==0) && (now-report_at<1000000 ||
           (!failed && !changed && now-report_at<10000000)))return;
        const auto pool=GraphicsMemory::TextureFree(m);
        summary(*this,pool,context,failed?"allocation-failure":report_at<0?"reset":changed?"context":"periodic");
        // Largest retained callers plus fastest growing callers. Never emit
        // a line per allocation; total output is at most twelve caller rows.
        std::vector<unsigned> order;order.reserve(callers.size());
        for(unsigned i=0;i<callers.size();++i)order.push_back(i);
        std::vector<unsigned> selected;
        const auto choose=[&](bool growth) {
            std::sort(order.begin(),order.end(),[&](unsigned a,unsigned b) {
                return growth?callers[a].Delta()>callers[b].Delta():callers[a].live_bytes>callers[b].live_bytes;
            });
            unsigned count=0;
            for(auto i:order) {
                if(growth?callers[i].Delta()<=0:callers[i].live_bytes==0)break;
                if(std::find(selected.begin(),selected.end(),i)==selected.end())selected.push_back(i);
                if(++count==6)break;
            }
        };
        choose(false);choose(true);
        for(auto i:selected)detail(callers[i]);
        for(auto& c:callers)c.previous_bytes=c.live_bytes;
        reported_context=context;reported_failures=failures;report_at=now;
    }
};
// All emulated CPU cores share one ledger: allocation and release may execute
// on different cores. Records are bounded host memory, never guest graphics RAM.
inline std::mutex mutex;
inline Tracker tracker;
inline void Reset(){std::scoped_lock lock{mutex};tracker={};}
}
