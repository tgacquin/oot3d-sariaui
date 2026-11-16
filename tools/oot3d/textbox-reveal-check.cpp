// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/textbox_interpolation_hooks.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
namespace T=TextboxInterpolation;
struct Memory {
    std::map<unsigned,unsigned> words;
    std::optional<unsigned> Read(unsigned at) {auto i=words.find(at);return i==words.end()?std::nullopt:std::optional{i->second};}
    void F(unsigned at,float f){words[at]=std::bit_cast<unsigned>(f);}
};
struct Vertex {
    std::array<float,4> position{},color{1,1,1,1};
    std::array<float,2> tex_coord0{},tex_coord1{},tex_coord2{};
    bool operator==(const Vertex&) const=default;
};
// Expand complete quads and their native degenerate strip connectors. The
// assembler keeps the two alternating buffered vertices, including degenerates.
std::vector<Vertex> Glyphs(unsigned count,bool strip) {
    std::vector<Vertex> quads,out;std::vector<unsigned> indices;
    for(unsigned q=0;q<count;++q) {
        for(unsigned corner=0;corner<4;++corner) {
            Vertex v;v.position={float(q)*.03f+float(corner/2)*.02f,float(corner%2)*.02f,0,1};
            v.tex_coord0={float(q*2+corner/2)/64,float(corner%2)/16};quads.push_back(v);
        }
        if(strip) {
            if(q){indices.push_back(q*4-1);indices.push_back(q*4);}
            for(unsigned i=0;i<4;++i)indices.push_back(q*4+i);
        } else for(unsigned i:{0u,1u,2u,2u,1u,3u})indices.push_back(q*4+i);
    }
    if(strip) {
        std::array<unsigned,2> buffered{};unsigned slot=0;bool ready=false;
        for(unsigned i:indices) {
            if(ready){out.push_back(quads[buffered[0]]);out.push_back(quads[buffered[1]]);out.push_back(quads[i]);}
            buffered[slot]=i;ready|=slot==1;slot=!slot;
        }
    } else for(unsigned i:indices)out.push_back(quads[i]);
    return out;
}
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream code(argv[1],std::ios::binary);assert(code);
    const auto word=[&](unsigned address){unsigned v{};code.seekg(address-0x100000);code.read(reinterpret_cast<char*>(&v),4);assert(code);return v;};
    // Real native reveal -> index count -> pane255, both front/shadow models.
    assert(word(0x488434)==0x15900024 && word(0x488448)==0xe0800080 && word(0x488450)==0xe2400002);
    assert(word(0x487d08)==0xe5900920 && word(0x480b58)==0xe2840044);
    assert(word(0x48ba50)==0xe5940068 && word(0x3332ac)==0xa5801174);
    assert(word(0x487bd4)==0xed840a0a); // Native cumulative fractional character budget.
    constexpr unsigned message=ScreenModels::Message,model=0x8100000,mesh=0x8200000,
        desc=0x8300000,pane=0x8400000,view=0x8500000,live=0x8600000,play=0x8700000;
    m.words[message]=0x4ed0a0;m.words[message+0x964]=pane;m.words[message+8]=play;
    m.words[message+0xf38]=4;m.words[message+0xeb4]=2;m.words[message+0xf40]=0x123;
    m.words[pane]=0x4ed234;m.words[pane+4]=message+0x168;m.words[pane+0x64]=model;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[model+0x174]=4;
    m.words[mesh]=desc;m.words[desc+12]=12;m.words[desc+0x1c]=0xc4;
    m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=live;m.F(message+0xed4,1);
    for(unsigned i=0;i<12;++i){m.F(model+12+i*4,i%5==0?1:0);m.F(view+i*4,i%5==0?1:0);}
    for(unsigned i=0;i<12*36;i+=4)m.F(live+i,float(i)/1024);
    Interpolation::enabled=true;Interpolation::present_60=true;Interpolation::generation=7;Interpolation::guest_frames=10;
    const auto before=m.words;T::routes.clear();T::Capture(m,model,view);
    assert(m.words==before && T::routes.size()==1);
    auto route=T::routes.back();assert(route.vertices==4 && route.typewriter && route.strip && route.source[3]==255);
    auto expected_hash=HUD::HashStart;
    for(unsigned slot=0;slot<3;++slot) {
        const unsigned base=live+12*(slot==0?0:slot==1?12:20),stride=slot==0?12:slot==1?8:16;
        for(unsigned i=0;i<4*stride;i+=4)expected_hash=HUD::HashWord(expected_hash,m.words.at(base+i));
    }
    assert(route.hash==expected_hash); // Hash visible prefixes using full-capacity attribute offsets.
    assert(T::Find(route.hash,4,route.matrix,10,7));
    for(unsigned mode=0;mode<5;++mode) {
        m.words=before;auto layout=Textboxes::GetLayout();
        if(mode==0)layout.smooth_reveal=false;
        if(mode==1)m.words[play+0x18]=2; // Skip edge.
        if(mode==2)m.words[message+0xec4]=0x10000; // Script instant mode.
        if(mode==3)m.words[message+0xf38]=5; // Waiting for input.
        if(mode==4)m.words[message+0xeb4]=1; // Preparing.
        Textboxes::SetLayout(layout,false);T::routes.clear();T::Capture(m,model,view);
        assert(T::routes.size()==1 && !T::routes.back().typewriter);
        layout.smooth_reveal=true;Textboxes::SetLayout(layout,false);
    }
    // Other panes must still reject a partial mesh; never truncate plates/arrows/counters.
    m.words=before;m.words[message+0x964]=0;m.words[message+0x56c]=pane;
    T::routes.clear();T::Capture(m,model,view);assert(T::routes.empty());
    std::array<std::array<float,4>,4> projection{};
    route.source[3]=255;route.typewriter=true;
    for(bool strip:{false,true})for(unsigned added:{1u,2u,3u,4u,6u})for(float phase:{.25f,.5f,.75f,1.f}) {
        T::History<Vertex> history;route.strip=strip;route.vertices=4*3;route.reveal_clock=3;
        auto old=Glyphs(3,strip);assert(!history.Apply(old,route,20,1000000,1,2,projection));
        route.vertices=4*(3+added);route.reveal_clock=3+added;
        const auto native=Glyphs(3+added,strip);auto sample=native;Interpolation::sample_fraction=phase;
        assert(history.Apply(sample,route,21,1033333,1,2,projection));
        const auto expected=Glyphs(3+unsigned(std::ceil(added*phase)),strip);
        assert(sample==expected && history.entries[0].vertices==native);
        // The next interval starts from the complete native endpoint, not the
        // trimmed replay. No missing/reappearing glyphs or accumulated slowdown.
        route.vertices+=8;route.reveal_clock+=2;sample=Glyphs(5+added,strip);
        assert(history.Apply(sample,route,22,1066666,1,2,projection));
        assert(sample==Glyphs(3+added+unsigned(std::ceil(2*phase)),strip));
    }
    // First glyph/page is immediate; discontinuities and native 60 Hz menus
    // retain the native draw. None may truncate or mutate a rejected draw.
    for(unsigned mode=0;mode<12;++mode) {
        T::History<Vertex> history;route.strip=true;route.typewriter=true;route.vertices=8;route.reveal_clock=2;
        auto old=Glyphs(2,true);assert(!history.Apply(old,route,30,2000000,1,2,projection));
        auto next=Glyphs(4,true);route.vertices=16;route.reveal_clock=4;
        auto frame=31u;auto time=2033333ll;auto texture=1u;
        if(mode==0)route.typewriter=false;
        if(mode==1)route.message++;
        if(mode==2)route.generation++;
        if(mode==3)route.layout++;
        if(mode==4)route.reveal_clock=2; // Script wait/no budget.
        if(mode==5)route.reveal_clock=2.25f; // Instant burst beyond normal budget.
        if(mode==6)time=2016666; // Native 60 Hz update; don't halve it again.
        if(mode==7)frame=33;
        if(mode==8)texture=3;
        if(mode==9)next[0].tex_coord0[0]+=.25f;
        if(mode==10){next=Glyphs(1,true);route.vertices=4;route.reveal_clock=1;}
        if(mode==11){auto duplicate=old;history.Apply(duplicate,route,30,2000000,1,2,projection);}
        const auto native=next;
        assert(!history.Apply(next,route,frame,time,texture,2,projection) && next==native);
    }
    // Closing/reopening or a first draw has no history to delay the first letter.
    T::History<Vertex> fresh;route.vertices=4;auto first=Glyphs(1,true);const auto native=first;
    assert(!fresh.Apply(first,route,50,3000000,1,2,projection) && first==native);
    Interpolation::sample_fraction=.5f;
    std::cout<<"Native glyph-prefix signatures, partial-buffer routing, 60/120 Hz reveal order/endpoints, first letter, skip/instant/wait/page/cadence guards and read-only guest buffers passed\n";
}
