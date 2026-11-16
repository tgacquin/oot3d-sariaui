// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#define XXH_INLINE_ALL
#include "xxhash.h"
#include "oot3d/interpolation_replay.h"
using namespace OOT3D::Interpolation;
int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream file(argv[1],std::ios::binary);assert(file);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    const auto read=[&](unsigned at) {assert(at+4<=bytes.size());std::uint32_t v;std::memcpy(&v,bytes.data()+at,4);return v;};
    assert(read(0)==0x424c5644 && read(4)==1);
    const auto dvle=read(8),dvlp=12u;
    const auto uniforms=dvle+read(dvle+48),count=read(dvle+52),symbols=dvle+read(dvle+56);
    unsigned found=0;
    for(unsigned i=0;i<count;++i) {
        const std::string name(bytes.data()+symbols+read(uniforms+i*8));
        const auto start=(read(uniforms+i*8+4)&65535)-16;
        if(name=="uInvView") {assert(start==InverseViewRow);++found;}
        if(name=="uProjection") {assert(start==0);++found;}
        if(name=="uMatrixPallet") {assert(start==20);++found;}
    }
    assert(found==3);
    assert(XXH3_64bits(bytes.data()+dvlp+read(dvlp+8),read(dvlp+12)*4)==ModelShader);
    std::vector<std::uint32_t> swizzles;
    for(unsigned i=0;i<read(dvlp+20);++i)swizzles.push_back(read(dvlp+read(dvlp+16)+8*i));
    assert(XXH3_64bits(swizzles.data(),swizzles.size()*4)==ModelSwizzle);

    enabled=true;present_60=true;generation=1;
    Matrix lens{};lens[0][1]=2; lens[1][0]=-1.2; lens[2][2]=1.001; lens[2][3]=5; lens[3][2]=-1;
    auto camera=Math::Identity();
    CameraSample sample{Math::InverseRigid(camera),1,2,1,100000,true};
    ReplayCamera history;
    assert(!history.Observe(0,sample,camera,lens,100000));
    camera[0][3]=20;sample.view=Math::InverseRigid(camera);sample.time=133333;
    const auto half=history.Observe(1,sample,camera,lens,133333);assert(half);
    const auto expected=Math::Multiply(lens,[]{auto m=Math::Identity();m[0][3]=10;return m;}());
    assert(Math::Near(*half,expected,.000001));
    assert(history.Observe(1,sample,camera,lens,133334)==half); // same frame must reuse pair
    auto sky=lens;sky[2][2]=1.00001;sky[2][3]=5.0001;
    const auto sky_half=history.Observe(1,sample,camera,sky,133335);assert(sky_half);
    assert((*sky_half)[2]==sky[2]); // retain this draw's clip plane
    auto other=camera;other[0][3]+=1;assert(!history.Observe(1,sample,other,lens,133336));
    assert(!history.Observe(2,sample,camera,lens,233334)); // stale capture
    sample.time=166666;sample.scene=3;assert(!history.Observe(2,sample,camera,lens,166666));
    sample.time=199999;assert(history.Observe(3,sample,camera,lens,199999));
    sample.time=233332;camera[0][3]=1000;sample.view=Math::InverseRigid(camera);
    assert(!history.Observe(4,sample,camera,lens,233332));
    sample.time=266665;assert(!history.Observe(6,sample,camera,lens,266665)); // missing frame
    sample.time=299998;auto changed=lens;changed[0][1]=4;
    assert(!history.Observe(7,sample,camera,changed,299998));
    // Captured shop handoff: 241.96 units, roughly 21 degrees, FOV 55->50.
    // These are below generic cut limits, but buying-view entry/exit must cut.
    for(bool native:{false,true}) {
        ReplayCamera shop;
        auto first=Math::Identity(),second=Math::Identity();
        const double angle=.367,c=std::cos(angle),s=std::sin(angle);
        second[0]={c,0,s,-100};second[1][3]=12;
        second[2]={-s,0,c,220};
        CameraSample a{Math::InverseRigid(first),1,45,1,100000,true};
        a.projections_valid=native;a.projections.fill(lens);
        assert(!shop.Observe(0,a,first,lens,a.time));
        auto b=a;b.time+=33333;b.view=Math::InverseRigid(second);
        auto zoom=lens;zoom[0][1]*=1.12;zoom[1][0]*=1.12;
        b.projections.fill(zoom);
        // Confirm the original geometric detector accepts this handoff.
        assert(shop.Observe(1,b,second,zoom,b.time));
        shop.Reset();assert(!shop.Observe(0,a,first,lens,a.time));
        b.cut_group=0x19;
        assert(!shop.Observe(1,b,second,zoom,b.time));
        b.time+=33333;assert(shop.Observe(2,b,second,zoom,b.time));
        a.time=b.time+33333;assert(!shop.Observe(3,a,first,lens,a.time));
        a.time+=33333;assert(shop.Observe(4,a,first,lens,a.time));
    }
    // Exit-only first-person cuts, even below geometric cut limits. Exercise
    // both legacy and native-lens tracks, plus the camera-relative item view.
    for(bool native:{false,true})for(bool attached:{false,true}) {
        ReplayCamera aim;
        auto a=camera;a[0][3]=0;
        CameraSample v{Math::InverseRigid(a),1,45,1,100000,true};
        v.projections_valid=native;v.projections.fill(lens);
        v.attached_valid=attached;v.attached_view=v.view;
        if(native && attached)v.view[0][3]+=40; // Select the distinct attached track.
        const auto observe=[&](unsigned frame,bool first_person) {
            v.first_person=first_person;v.time=100000+frame*33333;
            return aim.Observe(frame,v,a,lens,v.time);
        };
        assert(!observe(0,false));
        assert(observe(1,true)); // Entry keeps the existing interpolation.
        assert(observe(2,true)); // Aiming remains smooth.
        assert(!observe(3,false)); // Exit uses the native cut immediately.
        assert(!observe(3,false)); // Every draw in the handoff frame agrees.
        assert(observe(4,false)); // Resume on the next gameplay frame.
        assert(observe(5,true)); // Repeated entry is unaffected.
        assert(!observe(6,false));
    }
    auto nan=camera;nan[1][0]=NAN;assert(!CameraMatches(nan,sample));
    // Native look-at matrices have small basis errors. A transpose creates a
    // position error proportional to scene coordinates, rejecting the whole
    // frame as the player looks/aims around even though this is the same camera.
    {
        ReplayCamera aiming;
        for(unsigned f=0;f<20;++f) {
            const double angle=.015*f,c=std::cos(angle),s=std::sin(angle);
            auto view=Math::Identity();
            view[0]={c*1.0003,0,s*1.0003,-8000};
            view[2]={-s*.9998,0,c*.9998,4500};
            view[1][3]=-120;
            assert(Math::Rigid(view));
            const auto inverse=Math::InverseCamera(view);assert(inverse);
            assert(Math::Near(Math::Multiply(view,*inverse),Math::Identity(),1e-8));
            const auto approximate=Math::InverseRigid(view);
            assert(std::abs(approximate[0][3]-(*inverse)[0][3])>.25);
            CameraSample aiming_sample{view,1,2,generation.load(),100000+33333*f,true};
            assert(CameraMatches(*inverse,aiming_sample));
            const auto midpoint=aiming.Observe(f,aiming_sample,*inverse,lens,aiming_sample.time);
            assert(bool(midpoint)==(f!=0));
            auto wrong=*inverse;wrong[0][3]+=1;
            assert(!CameraMatches(wrong,aiming_sample));
        }
    }
    ++generation;assert(!history.Observe(8,sample,camera,changed,333331));

    // A continuous zoom used to reject every whole-frame pair until it stopped.
    // Exercise zoom-in/out with camera movement, repeated draws and the sky's
    // different depth planes. Image-plane coefficients must be the midpoint;
    // each draw's own depth projection must survive the camera correction.
    history.Reset();camera=Math::Identity();sample={camera,1,2,generation.load(),100000,true};
    auto previous_lens=lens;
    assert(!history.Observe(0,sample,camera,previous_lens,sample.time));
    for(unsigned frame_index=1;frame_index<=12;++frame_index) {
        auto zoom=lens;
        const double scale=1.+.03*(frame_index<=6?frame_index:12-frame_index);
        zoom[0][1]*=scale;zoom[1][0]*=scale;
        camera[0][3]+=2;camera[2][3]+=4;
        sample.view=Math::InverseRigid(camera);sample.time+=33333;
        const auto result=history.Observe(frame_index,sample,camera,zoom,sample.time);assert(result);
        auto mid=zoom;
        for(unsigned row=0;row<2;++row)for(unsigned col=0;col<4;++col)
            mid[row][col]=(previous_lens[row][col]+zoom[row][col])*.5;
        auto correction=Math::Identity();correction[0][3]=1;correction[2][3]=2;
        assert(Math::Near(*result,Math::Multiply(mid,correction),1e-6));
        auto zoom_sky=zoom;zoom_sky[2]=sky[2];mid[2]=sky[2];
        const auto sky_result=history.Observe(frame_index,sample,camera,zoom_sky,sample.time+1);
        assert(sky_result && Math::Near(*sky_result,Math::Multiply(mid,correction),1e-6));
        assert(history.Observe(frame_index,sample,camera,zoom,sample.time+2)==result);
        auto incompatible=zoom;incompatible[0][1]*=1.01;
        assert(!history.Observe(frame_index,sample,camera,incompatible,sample.time+3));
        previous_lens=zoom;
    }
    sample.time+=33333;changed=lens;changed[0][1]*=2;changed[1][0]*=2;
    assert(!history.Observe(13,sample,camera,changed,sample.time)); // abrupt lens cut
    sample.time+=33333;assert(history.Observe(14,sample,camera,changed,sample.time));
    auto flipped=changed;flipped[0][1]*=-1;sample.time+=33333;
    assert(!history.Observe(15,sample,camera,flipped,sample.time));
    auto degenerate=lens;degenerate[0]={};assert(!ContinuousLens(degenerate,degenerate));

    // The three lenses belong to the same native View but must never share
    // their previous lens. First-person zoom and its foreground lens can be
    // very different within one frame. Draw order is not a stable identity.
    for(const bool warp : {false,true}) {
        ReplayCamera multi;
        camera=Math::Identity();
        CameraSample s{camera,1,2,generation.load(),100000,true};
        s.projections_valid=true;s.attached_valid=true;
        s.attached_view=Math::Identity();s.attached_view[2][3]=-120;
        const auto attached=*Math::InverseCamera(s.attached_view);
        std::array<Matrix,3> previous{};
        for(unsigned f=0;f<12;++f) {
            s.time=100000+33333*f;camera[0][3]=double(f)*2;
            s.view=Math::InverseRigid(camera);
            s.projections={lens,sky,lens};
            s.projections[0][0][1]*=1.4+.02*f;
            s.projections[0][1][0]*=1.4+.02*f;
            s.projections[2][0][1]*=.9;s.projections[2][1][0]*=.9;
            // Native heat deformation changes the perspective divide as well
            // as the image rows. It is not the canonical [0,0,-1,0] shape.
            if(warp)for(auto& p:s.projections) {
                p[0][0]=.003*std::sin(f*.1);p[1][1]=-.002*std::sin(f*.1);
                p[3][0]=.004*std::sin(f*.1);p[3][2]=-1-.005*std::cos(f*.1);
            }
            for(unsigned j=0;j<3;++j) {
                const unsigned i=f%2?2-j:j;
                const auto& p=s.projections[i];
                assert(Perspective(p)==!warp && WorldProjection(p,s));
                assert(NativeLensMatches(p,p));
                const auto result=multi.Observe(f,s,camera,p,s.time);
                assert(bool(result)==(f!=0));
                if(result) {
                    Matrix midpoint{};
                    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                        midpoint[r][c]=(previous[i][r][c]+p[r][c])*.5;
                    auto delta=Math::Identity();delta[0][3]=1;
                    assert(Math::Near(*result,Math::Multiply(midpoint,delta),1e-7));
                }
                assert(multi.Observe(f,s,camera,p,s.time+1)==result);
            }
            const auto foreground=multi.Observe(f,s,attached,s.projections[2],s.time+2);
            assert(bool(foreground)==(f!=0));
            if(foreground)for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                assert(std::abs((*foreground)[r][c]-(previous[2][r][c]+s.projections[2][r][c])*.5)<1e-7);
            previous=s.projections;
        }
        auto unrelated=s.projections[0];unrelated[0][1]*=1.2;
        if(warp)assert(!WorldProjection(unrelated,s));
        assert(!multi.Observe(11,s,camera,unrelated,s.time+3));
        auto invalid=s.projections[0];invalid[0][0]=NAN;
        assert(!WorldProjection(invalid,s));
        assert(!WorldProjection(Math::Identity(),s));
        s.scene=8;s.time+=33333;
        assert(!multi.Observe(12,s,camera,s.projections[0],s.time));
        s.time+=33333;assert(multi.Observe(13,s,camera,s.projections[0],s.time));
        // Returning after an absent native projection must seed new history.
        assert(!multi.Observe(13,s,camera,s.projections[1],s.time));
        s.epoch++;s.time+=33333;
        assert(!multi.Observe(14,s,camera,s.projections[0],s.time));
    }

    ReplayFrame frame;assert(frame.Begin(5,0x100));frame.world=true;
    assert(!frame.Complete(5) && frame.Complete(6));
    assert(!frame.Begin(5,0x100));frame.failed=true;
    frame.world=true;assert(!frame.Complete(6));
    assert(frame.Begin(6,0x100));frame.world=true;
    frame.Begin(6,0x200);assert(!frame.Complete(7));
    frame.Begin(7,0x100);frame.world=true;++generation;assert(!frame.Complete(8));
    frame.Begin(8,0x100);frame.world=true;enabled=false;assert(!frame.Complete(9));enabled=true;

    ColorClears clears;
    clears.Observe(1,0x1000,0x1010,0x80402000,4);
    const auto background=clears.Background(1,0x1000,2,2,true);
    assert(background[0]==128.f/255 && background[1]==64.f/255 && background[2]==32.f/255 && background[3]==1);
    assert(clears.Background(2,0x1000,2,2,true)[0]==0); // previous-frame fill
    assert(clears.Background(1,0x1000,2,2,false)[0]==0); // incompatible color format
    clears.Observe(1,0x1008,0x1010,0xffffff,3);
    assert(clears.Background(1,0x1000,2,2,true)[0]==0); // overwritten clear

    PresentationClock clock;
    assert(clock.Next(1,0,true,0)==Presentation::Native);
    assert(clock.Next(2,0,true,33)==Presentation::Native);
    assert(clock.Next(3,0,true,67)==Presentation::Pair);
    assert(clock.Next(3,0,true,67)==Presentation::Skip);
    assert(clock.Next(4,0,true,83)==Presentation::Native); // already native 60 FPS
    assert(clock.Next(5,0,true,116)==Presentation::Native);
    assert(clock.Next(6,0,false,150)==Presentation::Native); // incomplete frame
    assert(clock.Next(7,0,true,183)==Presentation::Pair);
    assert(clock.Next(8,0,true,100)==Presentation::Native); // rewind
    PresentationPacer pacer;
    assert(pacer.Deadline(100000000,true)==100000000);
    assert(pacer.Deadline(101000000,true)==116666667);
    assert(pacer.Deadline(200000000,true)==200000000); // no catch-up bursts
    assert(pacer.Deadline(201000000,false)==201000000);
    std::cout<<"Replay: native shader mapping/hash, camera correction, cuts, gaps, epochs, complete-frame gating and 30/60 Hz pacing passed\n";
}
