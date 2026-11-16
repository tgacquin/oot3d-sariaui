// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_particles.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

using namespace OOT3D::FileParticles;

template<unsigned Capacity>
void CheckGeometry(const OOT3D::PausePrompts::QuadGeometry<Capacity>& g,float width) {
    assert(g.count<=Capacity);
    for(unsigned q=0;q<g.count;++q)for(unsigned v=0;v<4;++v) {
        const auto p=q*12+v*3;
        assert(std::isfinite(g.positions[p]) && g.positions[p]>=(400-width)/2-.001f);
        assert(g.positions[p]<=(400+width)/2+.001f);
        assert(std::isfinite(g.positions[p+1]) && g.positions[p+1]>=-.001f && g.positions[p+1]<=240.001f);
        for(unsigned c=0;c<4;++c) {
            const float value=g.colors[q*16+v*4+c];
            assert(std::isfinite(value) && value>=0 && value<=1);
        }
        for(unsigned c=0;c<2;++c) {
            const float value=g.uvs[q*8+v*2+c];
            assert(std::isfinite(value) && value>=.5f/(c?Height:Width)-.00001f && value<=1-.5f/(c?Height:Width)+.00001f);
        }
    }
}

int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    std::ifstream input(argv[1],std::ios::binary);assert(input);
    std::vector<std::uint8_t> archive{std::istreambuf_iterator<char>(input),{}};
    const auto sprite=MakePixels(archive);assert(sprite);
    unsigned transparent=0,visible=0;
    for(auto pixel:*sprite) {
        assert((pixel&0xffffff00)==0xffffff00);
        transparent+=(pixel&255)==0;visible+=(pixel&255)>128;
    }
    assert(transparent>0 && visible>0);
    assert(!MakePixels({}));
    for(unsigned n=0;n<24;++n)assert(!MakePixels(std::span{archive}.first(n)));
    auto broken=archive;broken[0]=0;assert(!MakePixels(broken));
    for(unsigned field:{16,20}) {
        broken=archive;std::fill_n(broken.begin()+field,4,255);
        assert(!MakePixels(broken));
    }
    const auto files=OOT3D::MapFrame::LE32(archive,16),offsets=OOT3D::MapFrame::LE32(archive,20);
    for(unsigned i=0;i<(archive[10]|unsigned(archive[11])<<8);++i) {
        const auto at=OOT3D::MapFrame::LE32(archive,files+i*8+4);
        if(std::string_view(reinterpret_cast<const char*>(archive.data()+at))!="tex/acto_huyuchu1.ctxb")continue;
        const auto payload=OOT3D::MapFrame::LE32(archive,offsets+i*4);
        broken=archive;broken[payload+48]=0;assert(!MakePixels(broken));
        assert(!MakePixels(std::span{archive}.first(payload+72+SpriteSize*SpriteSize-1)));
    }
    Settings s;
    assert(Draw(s,10,0,400).count==0);
    for(float width:{400.f,426.6667f,560.f})for(int density:{0,1,24,64,1000}) {
        s.density=density;s.size=3;s.glow=2;
        for(double time:{0.,.016,10.,35.,100.,3599.999,3600.001,1e6})
            CheckGeometry(Draw(s,time,1,width),width);
    }
    // Dungeon maps have three times the capacity; other menus retain 64.
    s={};s.density=DungeonMaxParticles;
    const auto dense=Draw<DungeonMaxParticles>(s,10,1,400);
    const auto ordinary=Draw(s,10,1,400);
    assert(dense.count>ordinary.count);
    s.density=1000;
    const auto capped=Draw<DungeonMaxParticles>(s,10,1,400);
    assert(capped.count==dense.count && capped.positions==dense.positions);
    for(float width:{400.f,426.6667f,560.f}) {
        s.size=3;s.glow=2;s.light_radius=3;
        for(double time:{0.,.016,10.,35.,100.,3600.001})
            CheckGeometry(Draw<DungeonMaxParticles>(s,time,1,width),width);
    }
    Animation map_animation;
    CheckGeometry(map_animation.Frame<DungeonMaxParticles>(s,400),400);
    s={};assert(Draw(s,10,1,400).count>0);
    s.enabled=false;assert(!Draw(s,10,1,400).count);
    s={};s.glow=0;s.light_strength=0;
    const auto core=Draw(s,10,1,400);assert(core.count>0 && core.count<=unsigned(s.density)*3);
    s.size=std::numeric_limits<float>::quiet_NaN();assert(!Draw(s,10,1,400).count);
    s={};assert(!Draw(s,std::numeric_limits<double>::infinity(),1,400).count);

    // Presentation time, not native update counts, controls motion.
    const Animation::Clock::time_point start{};
    std::array<Animation,3> animations;
    unsigned index=0;
    for(int hz:{30,60,120}) {
        auto& a=animations[index++];
        a.Frame(s,400,start);
        for(int frame=1;frame<=hz*2;++frame)
            a.Frame(s,400,start+std::chrono::nanoseconds(std::int64_t(1e9*frame/hz)));
        assert(std::abs(a.seconds-2)<1e-6 && std::abs(a.rotation_seconds-2)<1e-6 && a.entrance==1);
    }
    // The spin changes on every 60 Hz presentation, even with drift frozen.
    s={};s.density=1;s.glow=0;s.light_strength=0;s.variations=false;
    double sample_time=0;
    Geometry previous=Draw(s,sample_time,1,400,0);
    while(!previous.count && sample_time<60)previous=Draw(s,sample_time+=.1,1,400,0);
    assert(previous.count);
    for(unsigned frame=1;frame<=60;++frame) {
        const auto next=Draw(s,sample_time,1,400,frame/60.);
        assert(next.count==previous.count && next.positions!=previous.positions);
        previous=next;
    }
    // Rotated sprites at every edge retain interpolated UVs, never reach an
    // adjacent atlas cell, and stay inside the viewport after polygon clipping.
    for(float x:{-20.f,0.f,200.f,400.f,420.f})for(float y:{0.f,120.f,240.f})
        for(float angle:{0.f,.3f,.8f,1.5f}) {
            Geometry g;AddSprite(g,x,y,80,.65f,angle,1,{1,1,1},0,400);
            CheckGeometry(g,400);assert(g.count<=3);
            for(unsigned j=0;j<g.count*8;j+=2)assert(g.uvs[j]<=63.5f/Width+.00001f);
        }
    for(unsigned i=0;i<MaxParticles;++i) {
        bool invisible=false,visible=false;float last=Visibility(0,i);
        for(unsigned frame=1;frame<=720;++frame) {
            const float value=Visibility(frame/60.,i);
            invisible|=value==0;visible|=value>.99f;
            assert(std::abs(value-last)<.03f);last=value;
        }
        assert(invisible && visible);
    }
    // The light has no square edge, and lifts a dark background at its center.
    const float center_alpha=float((*sprite)[Tiled(96,32)]&255)/255;
    assert(center_alpha>.98f && ((*sprite)[Tiled(64,0)]&255)==0);
    assert(std::lerp(.08f,.9f,center_alpha*.35f)>.3f);
    if(argc==3) {
        std::ofstream preview(argv[2],std::ios::binary);assert(preview);
        s={};const auto g=Draw(s,10,1,426.6667f);
        preview.write(reinterpret_cast<const char*>(sprite->data()),sizeof(Pixels));
        preview.write(reinterpret_cast<const char*>(&g.count),sizeof(g.count));
        preview.write(reinterpret_cast<const char*>(g.positions.data()),g.count*12*sizeof(float));
        preview.write(reinterpret_cast<const char*>(g.uvs.data()),g.count*8*sizeof(float));
        preview.write(reinterpret_cast<const char*>(g.colors.data()),g.count*16*sizeof(float));
    }
    s={};auto& a=animations[0];s.speed=0;
    const auto frozen=a.seconds;a.Frame(s,400,start+std::chrono::seconds(3));
    assert(a.seconds==frozen);
    s.enabled=false;a.Frame(s,400);assert(!a.started && a.seconds==0);
    s.enabled=true;const auto initial=a.Frame(s,400);
    const auto established=Draw(s,0,1,400,0);
    assert(initial.count>0 && initial.count==established.count && a.entrance==1);
    assert(initial.positions==established.positions && initial.colors==established.colors);
    std::cout<<"Native sprite extraction, malformed assets, viewport clipping, particle limits, rotated-edge clipping, independent fades, background light and 30/60/120 Hz timing passed\n";
}
