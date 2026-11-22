// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_menu.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void F(U32 a,float f) {words[a]=std::bit_cast<U32>(f);}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
U32 Pack(unsigned x,unsigned y) {return x|(y<<16);}
int main(int argc,char** argv) {
    Memory m;
    if(argc>1) {
        std::ifstream f(argv[1],std::ios::binary);assert(f);U32 w;
        for(U32 at=0x100000;f.read(reinterpret_cast<char*>(&w),4);at+=4)m.words[at]=w;
        assert(CodeMatches(m));
        assert(m.Read(0x433ae8+9*4)==0x4343a8 && m.Read(0x433ae8+10*4)==0x4344bc);
        assert(m.Read(0x4343a8)==0xe1d411b0); // target X from owner+0x10
        assert(m.Read(0x4343ac)==0xe1d401b4); // current X from owner+0x14
        m.words.clear();
    }
    constexpr U32 root=0x5066f8,play=0x8000000,owner=0x8100000,model=0x8200000,
        mesh=0x8300000,src=0x8400000,dst=0x8500000,glow=0x8600000,
        glow_model=0x8700000,glow_mesh=0x8800000,counter=0x8900000,
        count_model=0x8a00000,count_mesh=0x8b00000;
    m.words[UI+0xc]=play;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;
    m.words[play+0x100]=3;m.words[root+0x3c]=owner;m.words[owner+0x224]=24;
    m.words[owner+8]=model;m.words[model]=0x4ebd60;m.words[model+4]=mesh;
    m.words[root+0x44]=glow;m.words[glow+8]=glow_model;
    m.words[glow_model]=0x4ebd60;m.words[glow_model+4]=glow_mesh;
    m.words[counter+0x10]=count_model;m.words[count_model]=0x4ebd60;m.words[count_model+4]=count_mesh;
    const auto quad=[&](U32 address,float x,float y,float w,float h) {
        const std::array<float,12> v{x,y,-3,x+w,y,-3,x,y+h,-3,x+w,y+h,-3};
        for(unsigned i=0;i<12;++i)m.F(address+4*i,v[i]);
    };
    const auto point=[&](unsigned index) {
        if(index%6==5)return SlotCenters[index/6];
        return std::array{32.f+48*(index%6),28.f+48*(index/6)};
    };
    for(float content_scale:{.6f,1.f,1.4f})for(bool horizontal:{false,true})for(unsigned destination:{5u,11u,17u,23u,2u})
    for(unsigned source:{12u,23u}) {
        if(source==destination)continue;
        Layout poses;poses.horizontal=horizontal;
        poses[Items]={-35,12,1.2f};poses[Slots]={-170,-80,.65f};
        poses.slot_content_scale=content_scale;poses.slot_spacing=.8f;poses.slot_content_x=13;
        SetLayout(poses,false);
        const auto a=point(source),b=point(destination);
        m.words[root+0x50]=source;m.words[root+0x54]=destination;
        m.words[root+0xc]=Pack(unsigned(a[0]-21),unsigned(a[1]-21));
        m.words[root+0x10]=Pack(unsigned(b[0]-21),unsigned(b[1]-21));
        m.words[owner+0xc+source*4]=counter;
        const auto target=[&](unsigned index) {
            auto p=point(index);const bool slot=index%6==5;
            if(slot) {
                p=horizontal?std::array{288+(p[1]-120)*.8f+13,120.f}:
                    std::array{p[0]+13,120+(p[1]-120)*.8f};
            }
            return Move(p[0],p[1],slot?Slots:Items,poses[slot?Slots:Items]);
        };
        const auto ta=target(source),tb=target(destination);
        // Samples include crossing the old slot boundary and reaching either
        // endpoint. Integer rounding matches the game's coordinate storage.
        for(unsigned step=0;step<=16;++step) {
            m.words[root+0x34]=step==16?10:9;
            std::array<std::array<float,2>,2> centers;
            for(unsigned direction=0;direction<2;++direction) {
                const auto start=direction?b:a,end=direction?a:b;
                centers[direction]={float(int(start[0]+(end[0]-start[0])*step/16)),
                                    float(int(start[1]+(end[1]-start[1])*step/16))};
                const auto c=centers[direction];
                m.words[root+0x14+direction*4]=Pack(unsigned(c[0]-21),unsigned(c[1]-21));
            }
            for(unsigned q=0;q<24;++q) {
                const auto c=q==source?centers[0]:q==destination?centers[1]:point(q);
                quad(src+q*48,c[0]-26,c[1]-26,52,52);quad(dst+q*48,0,0,0,0);
            }
            const auto original=m.words;
            motion_x=-40;motion_y=motion_slots=-12;
            Hook hook;
            assert(CopyPositions(m,mesh,dst,src,24*48,&hook.pending));
            for(unsigned direction=0;direction<2;++direction) {
                const unsigned index=direction?destination:source;
                const auto f=Flight(m,mesh,index,poses);assert(f);
                const float t=f->progress;
                const auto begin=direction?tb:ta,end=direction?ta:tb;
                const auto at=dst+index*48;
                assert(Near((m.F(at)+m.F(at+12))/2,begin[0]+t*(end[0]-begin[0])));
                assert(Near((m.F(at+4)+m.F(at+28))/2,begin[1]+t*(end[1]-begin[1])));
                assert(Near(m.F(at+12)-m.F(at),52*f->scale));
            }
            const auto resting=m.words;hook.Animate(m);
            for(unsigned q:{source,destination}) {
                const auto f=Flight(m,mesh,q,poses);assert(f);
                const auto d=QuadMotion({0,{},f->from,f->to,f->progress});
                assert(Near(m.F(dst+q*48),std::bit_cast<float>(resting.at(dst+q*48))+d[0]/Scale));
                assert(Near(m.F(dst+q*48+4),std::bit_cast<float>(resting.at(dst+q*48+4))+d[1]/Scale));
            }
            const auto frame=m.words;hook.Animate(m);assert(m.words==frame);
            for(unsigned i=0;i<24*48;i+=4)assert(m.words.at(src+i)==original.at(src+i));
            const auto outgoing=Flight(m,mesh,source,poses);
            const auto highlight=Flight(m,glow_mesh,0,poses),number=Flight(m,count_mesh,0,poses);
            assert(highlight && number && outgoing);
            assert(highlight->center==outgoing->center && number->center==outgoing->center);
            assert(!Flight(m,mesh+4,source,poses));
        }
        m.words[owner+0xc+source*4]=0;
    }
    // Landing pulses grow across the old X=256 panel boundary. Both native
    // ghost controllers must stay centered on the selected destination slot.
    m.words[root+0x34]=11;
    constexpr U32 glow2=0x8c00000,model2=0x8d00000,mesh2=0x8e00000;
    m.words[root+0x48]=glow2;m.words[glow2+8]=model2;
    m.words[model2]=0x4ebd60;m.words[model2+4]=mesh2;
    for(float content_scale:{.6f,1.f,1.4f})for(bool horizontal:{false,true})for(unsigned index:{5u,11u,17u,23u,2u}) {
        Layout poses;poses.horizontal=horizontal;poses[Slots]={-170,-80,.65f};poses[Items]={25,15,1.2f};
        poses.slot_content_scale=content_scale;poses.slot_spacing=.8f;poses.slot_content_x=13;SetLayout(poses,false);
        const auto center=point(index);
        for(unsigned side=0;side<2;++side) {
            m.words[root+0xb8+4*side]=index;
            for(unsigned tick=0;tick<12;++tick) {
                const U32 id=side?mesh2:glow_mesh;
                const auto f=LandingGlow(m,id,0,poses);assert(f);
                const float width=42+3*tick;
                // Native conversion truncates the half-pixel top-left on the
                // intermediate frames. The uploaded copy must stay centered.
                quad(src,float(int(center[0]-width/2)),float(int(center[1]-width/2)),width,width);
                quad(dst,0,0,0,0);
                std::vector<AnimatedQuad> pending;
                assert(CopyPositions(m,id,dst,src,48,&pending));
                assert(Near((m.F(dst)+m.F(dst+12))/2,f->center[0]));
                assert(Near((m.F(dst+4)+m.F(dst+28))/2,f->center[1]));
                assert(Near(m.F(dst+12)-m.F(dst),width*f->scale));
                assert(pending.size()==1 && pending[0].part==f->from);
            }
            m.words[root+0xb8+4*side]=0xffffffff;
            assert(!LandingGlow(m,side?mesh2:glow_mesh,0,poses));
        }
    }
    m.words[root+0x34]=12;
    m.words[root+0x78]=5;
    m.words[root+0x7c]=3;
    for(bool horizontal:{false,true})for(unsigned index:{5u,11u,17u,23u,8u}) {
        Layout poses;poses.horizontal=horizontal;poses[Items]={-34,13,1.1f};poses[Slots]={-170,-80,.7f};
        poses.slot_spacing=.8f;poses.slot_content_x=9;poses.slot_content_scale=1.25f;
        SetLayout(poses,false);
        const bool equipped=index%6==5;
        const auto dest=equipped?SlotCenter(index/6,poses):point(index);
        const auto end=Move(dest[0],dest[1],equipped?Slots:Items,poses[equipped?Slots:Items]);
        m.words[root+0x50]=index;
        for(unsigned choice=0;choice<4;++choice) {
            m.words[root+0x90]=choice?0x37+choice:3;
            const auto start=Move(151+21+(choice%2?52:7),137+21+(choice>=2?53:7),Items,poses[Items]);
            for(unsigned tick=0;tick<=6;++tick) {
                m.words[root+0x38]=tick==6?4:3;m.words[root+0x80]=tick%6;
                const float t=tick==6?1:1-std::ldexp(1.f,-int(tick));
                quad(src,253,175,tick==6?60:42,tick==6?60:42);quad(dst,0,0,0,0);
                std::vector<AnimatedQuad> pending;
                assert(CopyPositions(m,glow_mesh,dst,src,48,&pending));
                assert(pending.size()==1);
                assert(Near((m.F(dst)+m.F(dst+12))/2,std::lerp(start[0],end[0],t)));
                assert(Near((m.F(dst+4)+m.F(dst+28))/2,std::lerp(start[1],end[1],t)));
                assert(!ArrowFlight(m,mesh2,0,poses));
            }
        }
    }
    m.words[root+0x34]=2;assert(!Flight(m,mesh,12,GetLayout()));
    m.words[root+0x34]=9;m.words[root+0x50]=100;
    assert(!Flight(m,mesh,12,GetLayout()));
    motion_x=motion_slots=motion_y=0;SetLayout({},false);
    std::cout<<"Item flights: all four slots, swaps, horizontal/vertical layouts, scale, counters, glow, motion and native endpoints passed\n";
}
