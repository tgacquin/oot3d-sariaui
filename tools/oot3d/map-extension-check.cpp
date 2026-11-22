// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_extension.h"
#include "video_core/texture/etc1.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <pthread.h>
using namespace OOT3D;
namespace E=OOT3D::MapExtension;
// A main-thread test has a much larger stack than the macOS emulator thread.
// Exercise observation (including early-outs and rebuilds) with a 512 KiB limit.
template<class F> void OnEmulatorStack(F&& run) {
    pthread_attr_t attributes;
    assert(pthread_attr_init(&attributes)==0);
    assert(pthread_attr_setstacksize(&attributes,512*1024)==0);
    pthread_t thread;
    assert(pthread_create(&thread,&attributes,[](void* context)->void* {
        // Leave room for the caller frames (JIT, RunLoop and CallSVC) that
        // are present in the app but absent from this isolated hook test.
        volatile unsigned char caller_frames[64*1024];
        for(unsigned i=0;i<sizeof(caller_frames);++i)caller_frames[i]=i%251;
        (*static_cast<std::remove_reference_t<F>*>(context))();
        for(unsigned i=0;i<sizeof(caller_frames);++i)assert(caller_frames[i]==i%251);
        return nullptr;
    },&run)==0);
    assert(pthread_attr_destroy(&attributes)==0);
    assert(pthread_join(thread,nullptr)==0);
}
struct Memory {
    std::map<std::uint32_t,std::uint32_t> words;
    std::optional<std::uint32_t> Read(std::uint32_t a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Write(std::uint32_t a,std::uint32_t v) {assert(words.contains(a));words[a]=v;}
    template<class T> void Floats(std::uint32_t a,const T& v) {for(unsigned i=0;i<v.size();++i)words[a+4*i]=std::bit_cast<std::uint32_t>(v[i]);}
};
int main(int argc,char** argv) {
    const auto pixel=[](const std::shared_ptr<E::Pixels>& p,unsigned x,unsigned y){return (*p)[MapFrame::TiledOffset(x,255-y)];};
    std::vector<std::uint32_t> source(320*240,0x806040ff);
    auto flat=E::Extend(source,320,240,40,0);
    for(auto p:*flat)assert(p==0x806040ff); // Constant parchment stays constant at every blur radius.
    auto dim=E::Extend(source,320,240,18,1);
    assert(pixel(dim,0,0)==0x806040ff && pixel(dim,256,255)==0x806040ff);
    assert(pixel(dim,255,0)==255 && pixel(dim,511,255)==255); // Darkness grows outward only.
    for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x)source[y*320+x]=
        ((x<64?((y%2)?255u:0u):x>295?128u:64u)<<24)|0x2040ff;
    auto sharp=E::Extend(source,320,240,0,0),blurred=E::Extend(source,320,240,40,0);
    assert(pixel(sharp,0,0)==pixel(blurred,0,0));
    assert(pixel(sharp,256,255)==pixel(blurred,256,255));
    double sharp_variation=0,blur_variation=0;
    for(unsigned y=1;y<256;++y) {
        sharp_variation+=std::abs(int(pixel(sharp,64,y)>>24)-int(pixel(sharp,64,y-1)>>24));
        blur_variation+=std::abs(int(pixel(blurred,64,y)>>24)-int(pixel(blurred,64,y-1)>>24));
    }
    assert(blur_variation<sharp_variation*.2);
    // Dark terrain near either edge and a brown bottom border must not become
    // muddy side bands. Preserve their seam pixels, but continue the pale paper.
    source.assign(320*240,0xf4e4acff);
    for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x)
        if((y>=40 && y<90 && (x<28 || x>=292)) || y>=215)source[y*320+x]=0x403020ff;
    const auto clean=E::Extend(source,320,240,18,0);
    for(unsigned side=0;side<2;++side) {
        assert(pixel(clean,side*256,64)==0x403020ff);
        assert(pixel(clean,side*256,255)==0x403020ff);
        for(unsigned y=0;y<256;++y) {
            assert(pixel(clean,side*256+255,y)==0xf4e4acff);
            assert((pixel(clean,side*256+128,y)>>24)>=243);
        }
    }
    source.assign(320*240,0xff000000);assert(pixel(E::Extend(source,320,240,20,0),0,0)==0);
    PauseMenu::Layout layout;layout.map_extension={true,80,18,0};PauseMenu::SetLayout(layout,false);
    const auto geometry=E::Make({50,30,250,180},layout.map_extension,.6f);
    assert(geometry.count==2 && geometry.positions[0]==-30 && geometry.positions[3]==50);
    assert(geometry.positions[12]==250 && geometry.positions[15]==330);
    assert(geometry.uvs[0]>geometry.uvs[2] && geometry.uvs[8]<geometry.uvs[10]);
    assert(geometry.colors[3]==.6f);
    auto off=layout.map_extension;off.enabled=false;assert(E::Make({0,0,320,240},off).count==0);

    Memory m;constexpr std::uint32_t model=0x8100000,mesh=0x8110000,helper=0x8120000,desc=0x8130000,live=0x14900000,texture=0x14a00000;
    m.words[0x5066f8+0x34]=m.words[0x50446c+0x18]=m.words[0x4fda6c+0x50]=0;
    m.words[E::Hook::World+4]=model;m.words[E::Hook::World]=mesh;m.words[E::Hook::World+0x10]=helper;
    m.words[E::Hook::World+0x38]=2;m.words[helper]=E::Hook::Quads;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[mesh]=desc;
    m.words[mesh+0x15c]=512|(256u<<16);m.words[mesh+0x158]=texture;
    m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=live;m.words[desc+12]=120;
    m.Floats(model+12,std::array<float,12>{1,0,0,0,0,1,0,0,0,0,1,0});
    for(unsigned i=0;i<120*36;i+=4)m.words[live+i]=0;
    m.Floats(live,std::array<float,12>{0,0,0,320,0,0,0,240,0,320,240,0});
    m.Floats(live+120*12,std::array<float,8>{0,1,320.f/512,1,0,16.f/256,320.f/512,16.f/256});
    std::array<float,16> color;color.fill(1);m.Floats(live+120*20,color);
    std::vector<std::uint8_t> atlas(131072,255);
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary);assert(file);file.seekg(72);
        file.read(reinterpret_cast<char*>(atlas.data()),atlas.size());assert(file.gcount()==atlas.size());
    }
    for(unsigned i=0;i<atlas.size();i+=4)m.words[texture+i]=MapFrame::LE32(atlas,i);
    unsigned decodes=0;
    const auto decode=[&](std::uint64_t bits,unsigned x,unsigned y) {
        ++decodes;const auto rgb=Pica::Texture::SampleETC1Subtile(bits,x,y);
        return std::array{rgb.r(),rgb.g(),rgb.b()};
    };
    E::Hook hook;
    const auto observe=[&] {OnEmulatorStack([&] {hook.Observe(m,model,decode);});};
    observe();
    assert(hook.bounds && hook.extension && decodes==320*240);
    assert(hook.cached_crop==E::Crop{});
    const auto image=hook.extension;assert(hook.dirty);hook.ResetFrame();observe();
    assert(hook.extension==image && decodes==320*240); // Pending guest upload cannot trigger another CPU composition.
    m.Floats(live,std::array<float,12>{50,20,0,210,20,0,50,140,0,210,140,0});
    hook.ResetFrame();observe();assert(hook.extension==image);
    assert(std::abs(hook.bounds->right-hook.bounds->left-128)<.001f);
    layout.map_extension.blur=30;PauseMenu::SetLayout(layout,false);
    observe();assert(hook.extension!=image && decodes==320*240);
    m.words[texture+8]^=1;hook.dirty=false;observe();assert(decodes==2*320*240);
    hook.ResetFrame();m.words[0x4fda6c+0x50]=1;observe();assert(!hook.bounds);
    m.words[0x4fda6c+0x50]=0;m.words[0x5066f8+0x34]=2;observe();assert(!hook.bounds);
    if(argc>2) {
        // Local preview of the exact generated texture, never bundled with the mod.
        std::ofstream out(argv[2],std::ios::binary);out<<"P6\n480 240\n255\n";
        for(unsigned y=0;y<240;++y)for(unsigned x=0;x<480;++x) {
            std::uint32_t p;
            if(x>=80 && x<400)p=hook.decoded[y*320+x-80];
            else {unsigned xx=x<80?(79-x)*255/79:256+(x-400)*255/79;p=pixel(image,xx,y*255/239);}
            const char rgb[]{char(p>>24),char(p>>16),char(p>>8)};out.write(rgb,3);
        }
    }
    PauseMenu::SetLayout({},false);
    std::cout<<"Map extension seams, progressive blur, darkness, geometry, native atlas selection, cache and page isolation passed on a limited emulator thread stack\n";
}
