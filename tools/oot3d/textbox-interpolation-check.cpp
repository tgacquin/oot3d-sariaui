// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/textbox_interpolation_hooks.h"
#include <cassert>
#include <map>
#include <iostream>
#include <fstream>
using namespace OOT3D;
namespace T=TextboxInterpolation;
struct Memory {
    std::map<std::uint32_t,std::uint32_t> words;
    std::optional<std::uint32_t> Read(std::uint32_t a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void F(std::uint32_t a,float f) {words[a]=std::bit_cast<std::uint32_t>(f);}
};
struct Vertex {
    std::array<float,4> position{0,0,0,1},color{1,1,1,1};
    std::array<float,2> tex_coord0{},tex_coord1{},tex_coord2{};
};
int main(int argc,char** argv) {
    // Check the native uniform-upload entry point, not a synthetic matrix alone.
    assert(argc==2);
    std::ifstream code(argv[1],std::ios::binary);
    code.seekg(0x3135fc-0x100000);
    std::uint32_t instruction{};
    code.read(reinterpret_cast<char*>(&instruction),sizeof(instruction));
    assert(code && instruction==(0xe3a05000u|T::ModelViewRow)); // mov r5,#20

    assert(T::MeshVertices(4,true)==4);
    assert(T::MeshVertices(184,true)==124);
    assert(T::MeshVertices(718,true)==480);
    assert(T::MeshVertices(586,true)==392);
    assert(!T::MeshVertices(4,false) && !T::MeshVertices(6,true));
    assert(T::MeshVertices(6,false)==4);
    Memory m;constexpr unsigned model=0x8100000,mesh=0x8200000,desc=0x8300000,pane=0x8400000,view=0x8500000;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[mesh]=desc;m.words[desc+12]=4;m.words[model+0x174]=4;
    m.words[desc+0x1c]=0x80;m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=0x8600000;
    m.words[ScreenModels::Message]=0x4ed0a0;m.words[ScreenModels::Message+0x56c]=pane;
    m.words[pane]=0x4ed234;m.words[pane+4]=ScreenModels::Message+0x168;m.words[pane+0x64]=model;
    m.words[ScreenModels::Message+0x978]=42;
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c) {
        m.F(model+12+r*16+c*4,r==c?1.f:c==3?float(r*10):0.f);
        m.F(view+r*16+c*4,r==c?1.f:0.f);
    }
    for(unsigned slot=0;slot<3;++slot) {
        unsigned base=0x8600000+4*(slot==0?0:slot==1?12:20);
        m.words[desc+slot*4]=0xdead0000; // Expired source; only the live upload is valid.
        unsigned stride=slot==0?12:slot==1?8:16;
        for(unsigned i=0;i<stride*4;i+=4)m.F(base+i,float(i));
    }
    Interpolation::enabled=true;Interpolation::present_60=true;Interpolation::guest_frames=10;Interpolation::generation=7;
    const auto original=m.words;T::Capture(m,model,view);assert(m.words==original && T::routes.size()==1);
    auto route=T::routes[0];assert(route.matrix[1][3]==10 && route.source[2]==42);
    assert(T::Find(route.hash,4,route.matrix,11,7));assert(!T::Find(route.hash,4,route.matrix,12,7));
    assert(!T::Find(route.hash,4,route.matrix,11,8));assert(!T::Find(route.hash+1,4,route.matrix,11,7));
    auto wrong=route.matrix;wrong[0][3]=1;assert(!T::Find(route.hash,4,wrong,11,7));
    auto other=route;other.source[0]++;T::Publish(other);assert(!T::Find(route.hash,4,route.matrix,11,7));
    // The captured 480-vertex text is a 718-index strip, expanded by
    // the CPU assembler to 2148 vertices including degenerate connectors.
    // Preserve these connectors, including at the supported maximum size.
    std::array<std::array<float,4>,4> projection{};
    const auto vertices=[](float x,float alpha) {std::vector<Vertex> v(6);for(auto& p:v){p.position[0]=x;p.color[3]=alpha;}return v;};
    {
        T::History<Vertex> strip_history;
        auto previous=vertices(0,.5f);
        previous.resize((T::MaxVertices/4*6-4)*3,previous[0]);
        assert(!strip_history.Apply(previous,route,20,1000000,1,2,projection));
        auto next=previous;for(auto& v:next)v.position[0]=.2f;
        assert(strip_history.Apply(next,route,21,1033333,1,2,projection));
        assert(std::abs(next.back().position[0]-.1f)<.00001f);
    }
    // Recorded sign opening, frames 5670/5671/5672. Native Z changes
    // from -2 to -1.83333 to -1.66667 while the box grows around (200,196).
    // Append captured overshoot/rest poses (5532/5552) to exercise shrink-back
    // at adjacent-frame cadence too. Project with the captured orthographic matrix.
    for(bool depth_free:{false,true}) {
        T::History<Vertex> opening;
        const std::array<float,5> sx{.084f,.3663333654f,.5973333716f,1.008f,.84f};
        const std::array<float,5> sy{.0800000057f,.3488889039f,.5688889623f,.96f,.8f};
        const std::array<float,5> z{-2,-1.8333332539f,-1.6666666269f,-1,-1.01f};
        std::vector<Vertex> prior;
        for(unsigned frame=0;frame<sx.size();++frame) {
            std::vector<Vertex> native(6);
            for(unsigned i=0;i<6;++i) {
                const float x=(i%2?128.f:-128.f)*sx[frame]+200;
                const float y=(i<3?-32.f:32.f)*sy[frame]+196;
                native[i].position={1-y/120.f,1-x/200.f,z[frame]/3.f-1.f/3.f,1};
            }
            auto midpoint=native;
            const bool applied=opening.Apply(midpoint,route,5670+frame,
                187300292+33470*frame,1,2,projection,depth_free);
            assert(applied==(frame>0 && depth_free));
            if(applied)for(unsigned i=0;i<6;++i)for(unsigned c=0;c<3;++c)
                assert(std::abs(midpoint[i].position[c]-(native[i].position[c]+prior[i].position[c])*.5f)<.00001f);
            prior=native;
        }
    }
    for(unsigned pane_id:{0u,1u,2u,256u}) {
        route.source[3]=pane_id;T::History<Vertex> history;
        auto first=vertices(0,.4f);assert(!history.Apply(first,route,20,1000000,1,2,projection));
        auto next=vertices(.2f,.8f);assert(history.Apply(next,route,21,1033333,1,2,projection));
        assert(std::abs(next[0].position[0]-.1f)<.00001 && std::abs(next[0].color[3]-.6f)<.00001);
        next=vertices(.4f,.8f);next[0].tex_coord0[0]=1;
        assert(!history.Apply(next,route,22,1066666,1,2,projection)); // New text/page.
        next=vertices(.4f,.8f);next.push_back(Vertex{});
        assert(!history.Apply(next,route,23,1099999,1,2,projection)); // Reveal adds a glyph.
        assert(!history.Apply(next,route,24,1116666,1,2,projection)); // Native 60 Hz menus.
        route.layout++;assert(!history.Apply(next,route,25,1149999,1,2,projection));
        assert(!history.Apply(next,route,28,1249999,1,2,projection)); // Closed/reopened.
        assert(!history.Apply(next,route,28,1249999,1,2,projection)); // Ambiguous duplicate.
        assert(!history.Apply(next,route,29,1283332,1,2,projection));
        route.generation++;assert(!history.Apply(next,route,30,1316665,1,2,projection));
        route.source[2]++;assert(!history.Apply(next,route,31,1349998,1,2,projection));
    }
    // Choice navigation is a discontinuous row change. Fade colors still blend
    // at every replay phase, but position always equals the newly selected row.
    assert(T::FilteredArrow(2) && T::FilteredArrow(3) && !T::FilteredArrow(1) && !T::FilteredArrow(256));
    for(float phase:{.25f,.5f,.75f})for(float row:{-.2f,.2f}) {
        OOT3D::Interpolation::sample_fraction=phase;
        T::History<Vertex> choice;route.source[3]=3;
        auto first=vertices(0,.4f);assert(!choice.Apply(first,route,60,3000000,1,2,projection));
        auto next=vertices(row,.8f);assert(choice.Apply(next,route,61,3033333,1,2,projection));
        assert(next[0].position[0]==row);
        assert(std::abs(next[0].color[3]-std::lerp(.4f,.8f,phase))<.00001f);
    }
    OOT3D::Interpolation::sample_fraction=.5f;
    std::cout<<"Textbox ownership, read-only capture, midpoint motion/fade, glyph/page changes, native 60 Hz, resets and ambiguity passed\n";
}
