// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/items_interpolation.h"
#include "oot3d/interpolation_replay.h"
#include "oot3d/interpolation_pose.h"
#include "oot3d/waterfall_interpolation.h"
#include "oot3d/fairy_glow_interpolation.h"
#include <cassert>
#include "oot3d/interpolation_timing.h"
#include <iostream>
using namespace OOT3D::Interpolation;
static bool Near(double a,double b,double epsilon=1e-5){return std::abs(a-b)<epsilon;}
int main() {
    {
        Timing::Displayed(1.0+2.0/120);Timing::Displayed(1.0);
        Timing::Displayed(1.0+1.0/120);Timing::Displayed(0);
        const auto report=Timing::TakeDisplayed();
        assert(report.count==3);
        assert(Near(report.minimum,1.0/120) && Near(report.maximum,1.0/120));
        assert(Timing::TakeDisplayed().count==0);
    }
    for(int fps=30;fps<=120;++fps) {
        RateClock clock;unsigned count=0;double previous=0;
        for(unsigned frame=0;frame<300;++frame) {
            const auto plan=clock.Next(frame,fps);
            assert(plan.count>=1 && plan.count<=4 && plan.count<=plan.sample_slots);
            assert(plan.sample_slots<=4 && plan.fps==fps);
            const auto repeated=clock.Next(frame,fps);
            assert(repeated.phases==plan.phases && repeated.count==plan.count);
            for(unsigned i=0;i<plan.count;++i) {
                assert(plan.phases[i]>0 && plan.phases[i]<=1);
                const double timestamp=(frame+double(plan.phases[i]))/30;
                assert(Near(timestamp-previous,1./fps,1e-7));
                previous=timestamp;++count;
            }
        }
        assert(count==unsigned(fps*10));
        const auto restarted=clock.Next(400,fps);RateClock fresh;
        assert(restarted.phases==fresh.Next(400,fps).phases);
        assert(clock.Next(401,120).count==4);
    }
    // The pause tabs and file select keep the guest at 60 Hz. Every interpolated sample must land on
    // the requested display timeline, including non-divisors such as 75 Hz.
    for(int requested=30;requested<=120;++requested) {
        const int fps=std::max(60,requested);
        RateClock clock;
        unsigned output=0;
        for(unsigned frame=0;frame<600;++frame) {
            const auto plan=clock.Next(frame,requested,60);
            assert(plan.source_fps==60 && plan.fps==fps && plan.sample_slots<=2);
            for(unsigned i=0;i<plan.count;++i) {
                ++output;
                assert(Near((frame+double(plan.phases[i]))/60,double(output)/fps,1e-7));
            }
        }
        assert(output==unsigned(10*fps));
        assert(clock.Next(600,120,30).count==4);
        assert(clock.Next(601,120,60).count==2);
    }
    {
        PresentationClock clock;
        assert(clock.Next(0,0,false,0,60)==Presentation::Native);
        assert(clock.Next(1,0,true,17,60)==Presentation::Pair);
        assert(clock.Next(1,0,true,17,60)==Presentation::Skip);
        assert(clock.Next(2,0,true,50,30)==Presentation::Native);
        struct Vertex {std::array<float,4> position{},color{};std::array<float,2> tex_coord0{};};
        MenuGeometry::History<Vertex> history;
        std::vector<Vertex> triangle(3),result;
        for(auto& v:triangle)v.position[3]=1;
        triangle[1].position[1]=.1f;
        triangle[2].position[0]=.1f;
        assert(!history.Apply(0,{},triangle,result));
        for(auto& v:triangle){v.position[1]+=.25f;v.color[3]=1;}
        sample_fraction=.5f;
        assert(history.Apply(1,{},triangle,result));
        assert(result.size()==3 && result.back().position[1]==.125f && result.back().color[3]==.5f);
        result.clear();
        triangle.front().tex_coord0[0]=.25f;
        assert(!history.Apply(2,{},triangle,result) && result.empty());
        assert(!history.Apply(4,{},triangle,result));
        MenuGeometry::Material cursor_key{};
        cursor_key[6]=1;
        assert(!history.Apply(4,cursor_key,triangle,result));
        cursor_key[7]=24;
        for(auto& v:triangle)v.position[0]+=.1f;
        assert(!history.Apply(5,cursor_key,triangle,result) && result.empty());
        for(auto& v:triangle)v.position[0]+=.1f;
        assert(history.Apply(6,cursor_key,triangle,result) && Near(result[0].position[0],.15f));
        result.clear();
        const auto visible=triangle;
        for(auto& v:triangle)v.position={.611f,0,0,1};
        assert(!history.Apply(7,cursor_key,triangle,result) && result.empty());
        assert(!history.Apply(8,cursor_key,visible,result) && result.empty());
        triangle=visible;
        std::swap(triangle[1],triangle[2]);
        assert(!history.Apply(9,cursor_key,triangle,result) && result.empty());

    }
    {
        using enum MenuPage;
        ReplayFrame replay;
        unsigned frame=10;
        for(auto page:{Items,Gear,Map,Items,FileSelect,NameEntry}) {
            menu_page=page;
            assert(!replay.Complete(frame));
            assert(replay.Begin(frame,123));
            replay.menu=true;
            assert(replay.Complete(++frame));
        }
        menu_page=None;
        assert(!replay.Complete(frame));

    }
    target_fps=200;assert(TargetFPS()==120);target_fps=1;assert(TargetFPS()==30);
    target_fps=75;display_refresh_rate=144;match_refresh_rate=true;assert(TargetFPS()==120);
    display_refresh_rate=59.94f;assert(TargetFPS()==60);match_refresh_rate=false;assert(TargetFPS()==75);
    target_fps=60;
    PresentationPacer pacer;
    assert(pacer.Deadline(0,true,120)==0);
    assert(pacer.Deadline(0,true,120)==8333333);
    assert(pacer.Deadline(100000000,true,120)==100000000); // no catch-up burst
    assert(pacer.Deadline(100000001,false,120)==100000001);

    assert(!NeedsSoftwarePacing(true,120,120));
    assert(!NeedsSoftwarePacing(true,60,59.94f));
    assert(NeedsSoftwarePacing(true,60,120));
    assert(NeedsSoftwarePacing(false,120,120));
    assert(!CanBorrowIntermediate(0) && !CanBorrowIntermediate(1));
    assert(CanBorrowIntermediate(2));
    const std::array<bool,5> busy_head{false,true,false,true,true};
    assert(ReadyFrameIndex(busy_head,true)==1);
    assert(ReadyFrameIndex(busy_head,false)==1);
    const std::array<bool,3> last_ready{false,false,true};
    assert(!ReadyFrameIndex(last_ready,true));
    assert(ReadyFrameIndex(last_ready,false)==2);
    assert(!ReadyFrameIndex(std::array<bool,2>{false,false},false));
    // Five usable frames, with one more retained by the overlay. At a 60 Hz
    // sink, discard excess 120 Hz samples but never borrow the native reserve.
    for(unsigned drain:{2u,4u}) {
        unsigned ready=5,queued=0,dropped=0;
        for(unsigned tick=0;tick<300;++tick) {
            const unsigned completed=std::min(drain,queued);
            queued-=completed;ready+=completed;
            for(unsigned sample=0;sample<3;++sample) {
                if(CanBorrowIntermediate(ready)){--ready;++queued;}
                else ++dropped;
            }
            assert(ready>0);--ready;++queued; // required frame never waits
            assert(ready+queued==5);
        }
        assert((drain==4)==(dropped==0));
    }

    // All four gold brackets use this pose path. An accepted shelf move
    // changes endpoints on the first displayed sample at or after the edge.
    for(int phase_index=1;phase_index<=4;++phase_index) {
        Pose::Sample a,b;
        a.key=b.key={1,2,3,4,1};
        a.model=b.model="h_cursor";a.shop_slot=0;b.shop_slot=2;
        a.camera=b.camera={Math::Identity(),1,2,0,100000,true};
        a.time=100000;b.time=133333;b.camera.time=b.time;
        a.frame=1;b.frame=2;
        a.native[0]=a.world[0]=Math::Identity();
        b.world[0]=a.world[0];b.world[0][0][3]=25;b.native=b.world;
        b.shop_press_us=a.time+std::llround((b.time-a.time)*phase_index/4.);
        Pose::History poses;poses.Observe(a);poses.Observe(b);
        for(unsigned tick=1;tick<=4;++tick) {
            sample_fraction=tick/4.f;
            const auto sample=Pose::Resolve(poses.Pairs(),b.native,b.camera,b.frame,b.time);
            assert(sample && sample->palette[0][0][3]==(int(tick)<phase_index?0:25));
        }
        b.shop_press_us=0;
        assert(Pose::BlendPalette(a,b,0,.25)==b.world[0]);
        b.shop_slot=a.shop_slot;
        assert(Near((*Pose::BlendPalette(a,b,0,.5))[0][3],12.5));
    }
    for(float phase:{.25f,.4f,.5f,.75f,1.f}) {
        sample_fraction=phase;
        auto a=Math::Identity(),b=a;b[0][3]=20;
        auto lens=Math::Identity();lens[3]={0,0,-1,0};lens[2][3]=1;
        CameraSample camera{Math::InverseRigid(a),1,2,0,100000,true};generation=0;
        ReplayTrack track;assert(!track.Observe(10,camera,a,lens,100000));
        camera.view=Math::InverseRigid(b);camera.time=133333;
        const auto projection=track.Observe(11,camera,b,lens,133333);assert(projection);
        assert(Near((*projection)[0][3],20*(1-phase)));
        assert(track.Observe(11,camera,b,lens,133333)==projection);
        camera.scene=3;camera.time=166666;assert(!track.Observe(12,camera,b,lens,166666));

        Pose::History poses;Pose::Sample first;
        first.key={1,2,3,4,1};first.camera={Math::Identity(),1,2,0,100000,true};
        first.frame=10;first.time=100000;first.draw_view=Math::Identity();
        first.world[0]=first.native[0]=a;
        auto second=first;second.frame=11;second.time=133333;second.camera.time=133333;
        second.world[0]=second.native[0]=b;
        poses.Observe(first);poses.Observe(second);
        const auto pose=Pose::Resolve(poses.Pairs(),second.native,second.camera,11,133333);
        assert(pose && Near(pose->palette[0][0][3],20*phase));
        auto rotation=b;rotation[0][0]=rotation[1][1]=std::cos(.4);
        rotation[0][1]=-std::sin(.4);rotation[1][0]=std::sin(.4);
        const auto blended=Pose::Half(a,rotation,phase);assert(blended && Math::Rigid(*blended));
        assert(Near(std::atan2((*blended)[1][0],(*blended)[0][0]),.4*phase,1e-3));

        Waterfall::History water;Waterfall::UV old{},current{};Waterfall::Key key{1,2,3,4,0,0,1};
        old[1][3]=.99f;current=old;current[1][3]=.02f;
        assert(!water.Apply(10,key,old));const auto uv=water.Apply(11,key,current);assert(uv);
        assert(Near((*uv)[1][3],.02-.03*(1-phase)));
        assert(!water.Apply(13,key,current));

        FairyGlow::History glow;glow.Context(0,2);
        const FairyGlow::Key glow_key{1,2,3,1};
        for(unsigned i=0;i<3;++i){a[i][i]=10;b[i][i]=12;}
        const FairyGlow::Color c0{1,1,1,.4},c1{1,1,1,.6};
        assert(!glow.Apply(10,glow_key,Math::Identity(),a,c0));
        const auto g=glow.Apply(11,glow_key,Math::Identity(),b,c1);assert(g);
        assert(Near(g->model_view[0][3],20*phase));
        assert(Near(g->model_view[0][0],10+2*phase));
        assert(Near(g->color[3],.4+.2*phase));
    }
    sample_fraction=.5f;
    std::cout<<"30–120 Hz cadence, phase spacing, rate changes, pacing, queue backpressure, camera, pose, UV wrapping and glow checks passed\n";
}
