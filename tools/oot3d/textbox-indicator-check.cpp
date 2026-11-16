// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/textbox_indicator.h"
#include "oot3d/pause_description.h"
#include "oot3d/textbox_interpolation_hooks.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::TextboxIndicator;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a){auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Write(U32 a,U32 v){words[a]=v;}
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);
    U32 word;for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(PauseDescription::CodeMatches(m));
    auto settings=Textboxes::IndicatorAnimation{};
    assert(Spring(0,settings)[0]==1 && Spring(45000,settings)[0]==settings.small);
    assert(Spring(117000,settings)[1]==settings.big && Spring(180000,settings)[1]==1);
    settings.speed=2;assert(Spring(22500,settings)[0]==settings.small);settings.speed=1;
    Animation anim;anim.Show(true,1000000,settings);assert(anim.Alpha(1000000)==0 && anim.Alpha(1080000)==1);
    anim.Show(false,1080000,settings);assert(anim.Alpha(1120000)==.5f);
    anim.Show(true,1120000,settings);assert(anim.Alpha(1120000)==.5f && anim.Alpha(1200000)==1);
    anim.Press(1200000,settings);anim.Show(false,1210000,settings);assert(anim.visible);
    anim.Show(false,1380000,settings);assert(!anim.visible && anim.Alpha(1460000)==0);
    anim.Show(true,100,settings);assert(anim.Alpha(100)==0); // Clock rewind.
    constexpr U32 pane=0x8000000,model=0x8010000,view=0x8020000,play=0x8030000,
        mesh=0x8040000,desc=0x8050000,buffer=0x8060000,alpha=0x8070000;
    m.Write(Message,0x4ed0a0);m.Write(Message+0x570,pane);m.Write(Message+8,play);m.Write(Message+0xf38,6);
    m.Write(HUDFade::UI+0xc,play);m.Write(play+0x104,0x55);m.Write(pane,0x4ed234);
    m.Write(pane+4,Message+0x168);m.Write(pane+0x64,model);m.Write(model,0x4ebd60);m.Write(model+4,mesh);
    m.Write(mesh,desc);m.Write(desc+12,4);m.Write(desc+0x1c,0x80);m.Write(mesh+0x124,0);m.Write(mesh+0x1a0,buffer);
    m.Write(model+0x174,4);
    for(unsigned off:{0x158u,0x15cu,0x164u})m.Write(mesh+off,0x12340000+off);
    for(unsigned i=0;i<36;++i)m.Write(buffer+4*i,std::bit_cast<U32>(float(i)));
    for(unsigned i=0;i<12;++i) {
        m.Write(model+12+4*i,std::bit_cast<U32>(i==3?200.f:i==7?196.f:float(i==0||i==5||i==10)));
        m.Write(view+4*i,std::bit_cast<U32>(float(i==0||i==5||i==10)));
    }
    const auto initial=m.words;Textboxes::SetLayout({},false);
    State state;std::array<U32,16> r{};r[0]=model;r[1]=view;
    m.Write(alpha,std::bit_cast<U32>(1.f));state.Material(m,model,alpha,1000000,true);
    assert(*m.Read(alpha)==0);assert(state.Transform(m,r,1000000,true));
    assert(state.ready && state.seen && r[2]==model+12);
    // Native shader sampling identity is published even with interpolation off.
    Interpolation::enabled=false;TextboxInterpolation::Capture(m,model,view);
    assert(!TextboxInterpolation::routes.empty() && TextboxInterpolation::routes.back().source[3]==2);
    Interpolation::enabled=true;
    m.Write(alpha,std::bit_cast<U32>(1.f));state.Material(m,model,alpha,1080000,true);
    state.Transform(m,r,1080000,true);assert(*m.Read(alpha)==std::bit_cast<U32>(1.f));
    std::array<U32,64> fp{};for(unsigned i=0;i<64;++i)fp[i]=0x12340000+i;
    U32 flags=0xa0000010,fpscr=0x1000000;
    state.Present(m,r,fp,flags,fpscr,1080000,true);assert(!state.pending); // Never draw twice.
    m.Write(play+0x18,0);state.Press(m,1080000);assert(!state.animation.pressed); // Held A / no edge.
    m.Write(play+0x18,2);state.Press(m,1080000);assert(state.animation.pressed); // Accepted B.
    state.animation.pressed=false;
    m.Write(play+0x18,1);state.Press(m,1080000);assert(state.animation.pressed);
    const auto captured=state;
    for(unsigned type:{0u,1u})for(unsigned input:{1u,2u})for(bool ready:{false,true}) {
        auto waiting=state;waiting.animation.pressed=false;waiting.ready=ready;
        waiting.captured=700000; // No draw within the old 100 ms input cutoff.
        m.Write(Message+0xf48,type);m.Write(play+0x18,input);
        waiting.Press(m,1080000);assert(waiting.animation.pressed);
        assert(waiting.animation.Scale(1125000,settings)[0]==settings.small);
    }
    m.Write(Message+0xf48,0);
    const auto savedr=r;const auto savedfp=fp;
    state.Present(m,r,fp,flags,fpscr,1125000,true);assert(state.pending);
    const auto before=m.words;
    r[0]=model;r[1]=view;
    // Native pane may move after disappearing. RetainedView cancels its motion.
    m.Write(model+12+3*4,std::bit_cast<U32>(-300.f));
    assert(state.Transform(m,r,1170000,true));
    std::array<float,12> retained{};assert(TitleScreen::ReadFloats(m,view,retained));
    assert(std::abs(retained[0]-settings.small)<1e-6f);
    assert(std::abs(retained[0]*-300+retained[3]-200)<.001f);
    m.Write(alpha,std::bit_cast<U32>(0.f));state.Material(m,model,alpha,1170000,true);
    assert(*m.Read(alpha)==std::bit_cast<U32>(1.f)); // Retained sample time/alpha.
    for(const auto& [a,v]:before)if(a!=model+24 && a!=alpha && (a<view||a>=view+48))assert(m.Read(a)==v);
    r.fill(0);fp.fill(0);flags=fpscr=0;state.Resume(r,fp,flags,fpscr);
    assert(r==savedr && fp==savedfp && flags==0xa0000010 && fpscr==0x1000000);
    // A close cuts off the spring's visibility hold at every speed, including
    // a native indicator draw still queued on the closing frame.
    for(float speed:{.25f,1.f,3.f})for(unsigned closing:{0u,14u,15u}) {
        m.words=before;state=captured;
        auto layout=Textboxes::GetLayout();layout.indicator_animation.speed=speed;
        Textboxes::SetLayout(layout,false);m.Write(Message+0xf38,closing);
        constexpr std::int64_t close_time=1090000;
        state.Present(m,r,fp,flags,fpscr,close_time,true);
        assert(state.pending && !state.animation.visible && state.animation.Alpha(close_time)==1);
        state.Resume(r,fp,flags,fpscr);
        state.Present(m,r,fp,flags,fpscr,close_time+Fade/2,true);assert(state.pending);
        m.Write(alpha,std::bit_cast<U32>(1.f));
        state.Material(m,model,alpha,close_time+Fade/2+1000,true);
        assert(*m.Read(alpha)==std::bit_cast<U32>(.5f));
        state.Resume(r,fp,flags,fpscr);
        state.Present(m,r,fp,flags,fpscr,close_time+Fade,true);
        assert(!state.pending && state.animation.Alpha(close_time+Fade)==0);
        state=captured;state.seen=true;
        m.Write(alpha,std::bit_cast<U32>(1.f));state.Material(m,model,alpha,close_time,true);
        state.Present(m,r,fp,flags,fpscr,close_time,true);
        assert(!state.pending && !state.animation.visible);
        m.Write(alpha,std::bit_cast<U32>(1.f));state.Material(m,model,alpha,close_time+Fade/2,true);
        assert(*m.Read(alpha)==std::bit_cast<U32>(.5f));
        m.Write(alpha,std::bit_cast<U32>(1.f));state.Material(m,model,alpha,close_time+Fade,true);
        assert(*m.Read(alpha)==0); // No spring can bring the closing icon back.
    }
    // Follow the actual plate's close transform with independent indicator
    // placement, a shifted/scaled sign box, and a delayed retained draw.
    for(std::int64_t elapsed:{0LL,33333LL,66666LL}) {
        m.words=before;state=captured;
        constexpr U32 plate=0x8080000,plate_pane=0x8090000,plate_view=0x80a0000;
        auto layout=Textboxes::GetLayout();layout.mm_close=true;Textboxes::SetLayout(layout,false);
        m.Write(Message+0xc,1);m.Write(Message+0xf38,6);m.Write(Message+0xf40,0x312);
        m.Write(Message+0x56c,plate_pane);m.Write(plate_pane,0x4ed234);
        m.Write(plate_pane+4,Message+0x168);m.Write(plate_pane+0x64,plate);
        m.Write(plate,0x4ebd60);m.Write(plate+4,0x80b0000);
        const std::array<float,12> plate_pose{1,0,0,200,0,1,0,180,0,0,1,0};
        const std::array<float,12> plate_camera{1.3f,0,0,-20,0,1.3f,0,-30,0,0,1,0};
        for(unsigned i=0;i<12;++i) {
            m.Write(plate+12+4*i,std::bit_cast<U32>(plate_pose[i]));
            m.Write(plate_view+4*i,std::bit_cast<U32>(plate_camera[i]));
        }
        TextboxClose::State box;box.Capture(m,plate,plate_view,1080000);assert(box.ready);
        box.closing=true;box.started=1090000;m.Write(Message+0xf38,14);
        state.view={1.7f,0,0,13,0,1.4f,0,-22,0,0,1,0};
        state.Present(m,r,fp,flags,fpscr,box.started,true);assert(state.pending);
        state.Resume(r,fp,flags,fpscr);
        const auto sample=box.started+elapsed;
        state.Present(m,r,fp,flags,fpscr,sample,true);assert(state.pending);
        const auto original=m.words;
        r[0]=model;r[1]=view;assert(state.Transform(m,r,sample+5000,true,&box));
        std::array<float,12> out{};assert(TitleScreen::ReadFloats(m,view,out));
        const float scale=TextboxClose::Scale(elapsed);
        for(unsigned row=0;row<2;++row) {
            const unsigned i=4*row;
            const float center=plate_camera[i+row]*plate_pose[i+3]+plate_camera[i+3];
            const float icon=state.view[i+row]*state.matrix[i+3]+state.view[i+3];
            const float actual=out[i+row]*state.matrix[i+3]+out[i+3];
            assert(std::abs(actual-(center+(icon-center)*scale))<.002f);
            assert(std::abs(out[i+row]-state.view[i+row]*state.animation.Scale(sample,layout.indicator_animation)[row]*scale)<.0001f);
        }
        for(const auto& [a,v]:original)if(a<view || a>=view+48)assert(m.Read(a)==v);
        // The plate itself uses the same scale and stays about the same center.
        r[0]=plate;r[1]=plate_view;assert(box.Transform(m,r,sample));
        assert(std::abs(std::bit_cast<float>(*m.Read(plate_view))-plate_camera[0]*scale)<.0001f);
        state.Resume(r,fp,flags,fpscr);
    }
    Textboxes::SetLayout({},false);
    m.words=before;state=captured;m.Write(buffer,0xdeadbeef);
    state.Present(m,r,fp,flags,fpscr,1125000,true);assert(!state.pending && !state.ready);
    m.words=before;state=captured;++Interpolation::generation;
    state.Present(m,r,fp,flags,fpscr,1125000,true);assert(!state.pending && !state.ready);--Interpolation::generation;
    m.words=before;state=captured;m.Write(Message+0xf38,2);
    state.Present(m,r,fp,flags,fpscr,1125000,true);assert(!state.pending && !state.ready);
    m.words=before;state=captured;auto layout=Textboxes::GetLayout();layout.indicator_animation.visible=false;
    Textboxes::SetLayout(layout,false);state.Present(m,r,fp,flags,fpscr,1125000,true);assert(!state.pending);
    state.Material(m,model,alpha,1125000,true);assert(*m.Read(alpha)==0);
    for(unsigned n:{1u,2u,3u,5u}) {m.Write(desc+12,n);assert(!State::Geometry(m,mesh));}
    Textboxes::SetLayout({},false);
    std::cout<<"Indicator curves, immediate close fades, parent closing transform, accepted A/B with stale or absent draws, retained draw, native signatures and resource/register isolation passed\n";
}
