// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/orbit_camera.h"
#include "oot3d/camera.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <limits>
using namespace OOT3D::OrbitCamera;
struct Memory {
    std::map<U32,U32> words;
    unsigned null_player_reads=0;
    std::optional<U32> Read(U32 a) {if(a==0x1710)++null_player_reads;auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {words[a]=v;}
};
int main(int argc,char**argv) {
    const auto near=[](float a,float b) { return std::abs(a-b)<1e-5f; };
    // MM capture: residual halves at the default response time. Direction
    // changes aim directly at the new velocity rather than braking separately.
    float velocity=0;
    for(int i=1;i<=6;++i) {
        velocity=AdvanceVelocity(velocity,1,.2f,.2f);
        assert(near(velocity,1-std::exp2(-float(i))));
    }
    for(int i=0;i<10;++i) velocity=AdvanceVelocity(velocity,0,.2f,.2f);
    assert(velocity==0);
    assert(AdvanceVelocity(0,1,0,.2f)==1);
    assert(AdvanceVelocity(1,0,.2f,0)==0);
    assert(AdvanceVelocity(1,-1,0,0)==-1);
    // Measured reversal crossed neutral for one update, then reversed on the
    // first opposite-input update. Normalized MM sequence: -1, -.5, +.25.
    velocity=AdvanceVelocity(-1,0,.2f,.2f);assert(near(velocity,-.5f));
    velocity=AdvanceVelocity(velocity,1,.2f,.2f);assert(near(velocity,.25f));
    assert(near(AdvanceVelocity(1,-1,.2f,.2f),0));
    for(int i=0;i<20;++i) velocity=AdvanceVelocity(velocity,-1,.2f,.2f);
    assert(velocity==-1);
    assert(AdvanceVelocity(0,1,.5f,.2f)<AdvanceVelocity(0,1,.1f,.2f));
    assert(AdvanceVelocity(1,0,.2f,.5f)>AdvanceVelocity(1,0,.2f,.1f));
    assert(AdvanceVelocity(.5f,.49f,.2f,.2f)>.49f);
    assert(AdvanceVelocity(.5f,.49f,.2f,.2f)<.5f);
    assert(Stick(.2f,0)[0]==0 && Stick(.2001f,0)[0]<.001f);
    assert(near(Stick(1,0)[0],1));
    auto diagonal=Stick(1,1);assert(near(std::hypot(diagonal[0],diagonal[1]),1));
    assert(Stick(std::numeric_limits<float>::quiet_NaN(),0)[0]==0);
    assert(SanitizeTime(-1,.2f)==0 && SanitizeTime(2,.2f)==1);
    assert(SanitizeTime(std::numeric_limits<float>::infinity(),.2f)==.2f);
    assert(SanitizeDistance(-1)==MinimumDistance && SanitizeDistance(100)==MaximumDistance);
    assert(SanitizeDistance(std::numeric_limits<float>::quiet_NaN())==DefaultDistance);
    // Both endpoints brake monotonically, including instant response and the
    // full sensitivity range. Reversing away from a limit remains responsive.
    for(float direction:{-1.f,1.f})for(float multiplier:{.25f,1.f,3.f}) {
        const float limit=direction<0?MinimumPitch:MaximumPitch;
        float p=limit-direction*8.f*PitchRadians,v=direction,previous_step=100;
        for(unsigned frame=0;frame<300;++frame) {
            v=AdvanceVelocity(v,direction,0,0);
            const float next=AdvancePitch(p,v,2.f*PitchRadians*multiplier);
            const float movement=std::abs(next-p);
            assert((movement<=previous_step+1e-6f ||
                (next==limit && movement<=.0201f*PitchRadians)) && direction*(next-p)>=0);
            assert(next>=MinimumPitch && next<=MaximumPitch);
            previous_step=movement;p=next;
        }
        assert(p==limit && v==0);
        v=AdvanceVelocity(v,-direction,.2f,.2f);
        const float returned=AdvancePitch(p,v,2.f*PitchRadians*multiplier);
        assert(near(v,-direction*.5f) && direction*(returned-p)<0);
    }
    // Small stick input is not artificially accelerated near the endpoint.
    float gentle=.01f;
    assert(near(AdvancePitch(MaximumPitch-PitchRadians,gentle,2.f*PitchRadians),
        MaximumPitch-PitchRadians+.02f*PitchRadians) && gentle==.01f);
    Memory m;
    if(argc>1) {
        std::ifstream f(argv[1],std::ios::binary);assert(f);U32 w;
        for(U32 a=0x100000;f.read(reinterpret_cast<char*>(&w),4);a+=4)m.words[a]=w;
        assert(CodeMatches(m));m.words[Collision]^=1;assert(!CodeMatches(m));m.words.clear();
    }
    constexpr U32 play=0x08000000, cam=play+0x364, player=0x09000000, sp=0x0ffff000;
    m.words[cam+0xd4]=play;m.words[cam+0xd8]=player;m.words[0x5043e0]=play;
    m.words[play+0xa64]=0;m.words[play+0x104]=0x55;m.words[cam+0x188]=7|(1<<16);
    m.words[cam+0x18c]=0;m.words[player+0x1710]=0;m.words[player+0x1714]=0;
    m.words[0x588e3c]=0;m.words[0x58795c]=1;
    Put(m,cam+0x80,{0,38,0});Put(m,cam+0x8c,{0,38,-150});Put(m,player+0x28,{0,0,0});
    for(U32 a=sp-Hook::ScratchSize;a<sp;a+=4)m.words[a]=0;
    std::array<U32,16> r{};r[0]=r[4]=cam;r[1]=0x123400;r[13]=sp;
    m.words[play+0x22a0]=0;
    const auto native=m.words; const auto regs=r;
    {
        Memory absent=m;absent.words[cam+0xd8]=0;
        Hook no_player;auto title_regs=regs;
        no_player.Begin(absent,title_regs);
        assert(absent.null_player_reads==0);
        assert(title_regs[15]==regs[1] && title_regs[13]==sp && !no_player.active);
        assert(!no_player.ReturningDialogue(absent,cam));
        assert(!TargetingState(absent,cam) && absent.null_player_reads==0);
    }
    unsigned native_calls=0;
    const auto begin=[&](Hook& hook,std::uint64_t frame=0) {
        const bool eligible=Allowed(m,r[0]);
        hook.Begin(m,r,frame);
        // All frames still dispatch the original mode. The regression skipped
        // this call, leaving the targeting interface flags latched forever.
        assert(r[15]==0x123400);
        if(eligible) {
            assert(m.Read(r[13]+0x38)==Hook::NativeCookie);
            ++native_calls;
            // Simulate native mode-state/interface writes and unwanted drift.
            m.words[0x516eb0]=0;
            m.words[cam+0x1a8]=native_calls;
            Put(m,cam+0x80,{40,100,80});Put(m,cam+0x8c,{40,90,60});
            r[0]=1; // Native camera-handler return value, not a camera pointer.
            hook.Finish(m,r);
            assert(m.words[0x516eb0]==0 && m.words[cam+0x1a8]==native_calls);
        }
    };
    // Quick chest openings keep orbit through the queued action and item
    // action. Their shared auto-walk flag must not start a door handoff.
    {
        constexpr U32 chest=0x09100000;
        Hook opening;
        m.words[play+0x5c2c]=m.words[play+0x7f10]=0;
        m.words[play+0x4c30]=m.words[play+0x4c34]=0;
        stick_x=1;begin(opening,1);opening.Finish(m,r);
        m.words[player+0x1710]=0x20000c00;
        m.words[player+0x1708]=0x4a1ef0;m.words[player+0x2484]=0x4bc91c;
        m.words[player+0x12b0]=chest;m.words[chest]=0x0a;m.words[chest+0x38c]=0xffff;
        // Only the cutscene state byte belongs to this guard.
        m.words[play+0x22a0]=0x12340000;
        const auto quick=m.words;
        assert(OrdinaryChestOpening(m,cam) && Allowed(m,cam));
        assert(!EntryContext(m,cam) && !NativeEntrance(m,cam));
        const float before=opening.yaw;
        r=regs;begin(opening,2);
        assert(opening.active && !opening.door_start.valid && opening.yaw!=before);
        opening.Finish(m,r);
        m.words[player+0x1708]=0x4bc22c;
        for(unsigned frame=3;frame<35;++frame) {
            stick_x=0;r=regs;begin(opening,frame);
            assert(opening.active && !opening.door_start.valid && r[15]==Collision);
            opening.Finish(m,r);
        }
        const float held=opening.yaw;
        r=regs;begin(opening,35);opening.Finish(m,r);assert(opening.yaw==held);
        m.words[player+0x1710]=0;
        r=regs;begin(opening,36);opening.Finish(m,r);assert(opening.active && opening.yaw==held);
        // Dramatic/unopened chests, other actors/actions, dialogue, targeting,
        // cutscenes and subcameras never acquire quick-chest orbit permission.
        for(auto [address,value]:std::array<std::pair<U32,U32>,11>{{
            {chest+0x38c,1},{chest+0x38c,0},{chest,0x0b},
            {player+0x1708,0x123456},{player+0x2484,0},
            {play+0x22a0,1},{play+0xa64,1},{cam+0x18c,3},
            {player+0x1710,0x20000c00|0x8000},{player+0x1710,0x20000000},
            {cam+0x188,7|(0x14<<16)}}}) {
            m.words=quick;m.words[address]=value;assert(!Allowed(m,cam));
        }
        m.words=native;r=regs;stick_x=0;entry_state={};
    }
    Hook h;begin(h);assert(h.active && r[15]==Collision && near(h.radius,150));h.Finish(m,r);
    const float initial_yaw=h.yaw;
    stick_x=.1; r=regs;begin(h);assert(r[15]==Collision && h.yaw==initial_yaw);h.Finish(m,r); // Deadzone
    stick_x=1;r=regs;begin(h);assert(h.active && r[15]==Collision && r[13]==sp-Hook::ScratchSize);
    assert(r[1]==r[13]+0x28 && r[2]==r[13]);
    // Simulate native collision moving the eye out of a wall. Do not smooth back through it.
    const Vec clipped{5,40,-20};Put(m,r[13],clipped);
    h.Finish(m,r);assert(r[15]==Resume && r[13]==sp);
    Vec eye;assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==clipped);
    // Hold long enough to reach full speed before measuring the release ramp.
    for(int i=0;i<12;++i) {r=regs;begin(h);h.Finish(m,r);}
    assert(std::abs(h.yaw_velocity)==1);
    stick_x=0;r=regs;begin(h);assert(h.active && r[15]==Collision);h.Finish(m,r);
    // Actual hook coasts after release, then holds its angle with no residual drift.
    const float released_yaw=h.yaw;
    r=regs;begin(h);h.Finish(m,r);assert(h.yaw!=released_yaw);
    for(int i=0;i<30;++i) {r=regs;begin(h);h.Finish(m,r);}
    assert(h.yaw_velocity==0);
    const float stopped_yaw=h.yaw;
    r=regs;begin(h);h.Finish(m,r);assert(h.yaw==stopped_yaw);
    // Check measured full-tilt rates through the actual native-call/collision
    // hook, before hitting either vertical limit. Sensitivity still scales both.
    for(bool vertical:{false,true})for(float multiplier:{.75f,1.f}) {
        m.words=native;h={};sensitivity=multiplier;
        stick_x=vertical?0:1;stick_y=vertical?1:0;
        for(unsigned frame=0;frame<11;++frame){r=regs;begin(h);h.Finish(m,r);}
        const float before=vertical?h.pitch:h.yaw;
        r=regs;begin(h);h.Finish(m,r);
        const float delta=(vertical?h.pitch:h.yaw)-before;
        assert(near(delta,(vertical?2.f:-4.f)*3.14159265358979323846f/180.f*multiplier));
    }
    sensitivity=1;stick_x=stick_y=0;
    for(unsigned frame=0;frame<30;++frame){r=regs;begin(h);h.Finish(m,r);}
    // Native mode changes are parameters for the same player's camera, not a
    // transfer of ownership. Preserve the orbit with a released stick through
    // climb, jump, hang, freefall, charge, still/attack and push/pull modes.
    m.words[cam+0x144]=std::bit_cast<U32>(52.f);
    const float held_yaw=h.yaw,held_radius=h.radius;
    for(auto [mode,flags]:std::array<std::pair<U32,U32>,8>{{{5,0x200000},{13,0x40000},
         {14,0x2000},{16,0x80000},{17,0x1000},{18,0},{19,0},{0,0}}}) {
        m.words[cam+0x18c]=mode;m.words[player+0x1710]=flags;
        r=regs;begin(h);
        assert(h.active && r[15]==Collision && h.yaw==held_yaw && near(h.radius,held_radius));
        h.Finish(m,r);assert(m.words[cam+0x144]==std::bit_cast<U32>(52.f));
    }
    // C-stick can also engage while already in an action mode.
    m.words[cam+0x18c]=5;m.words[player+0x1710]=0x200000;
    h.ResetMotion();stick_x=1;r=regs;begin(h);assert(h.active && r[15]==Collision);h.Finish(m,r);
    m.words[cam+0x18c]=0;m.words[player+0x1710]=0;
    // Targeting, first-person, dialogue, subcamera, scene restriction and
    // scripted settings still yield native control, regardless of mode.
    for(auto [a,value]:std::array<std::pair<U32,U32>,9>{{{cam+0x18c,3},{player+0x1710,0x100000},
         {player+0x1710,0x40|0x20000000},{player+0x1710,0x10},
         {player+0x1714,0x40000},{play+0xa64,1},{play+0x104,0x45},{cam+0x188,7|(0x14<<16)},{0x588e3c,1}}}) {
        const auto old=m.words[a];m.words[a]=value;stick_x=1;r=regs;begin(h);
        assert(!h.active && h.yaw_velocity==0 && h.pitch_velocity==0 && r[15]==0x123400);m.words[a]=old;
    }
    enabled=false;r=regs;begin(h);assert(!h.active && r[15]==0x123400);enabled=true;
    m.words=native;stick_x=1;r=regs;begin(h);const float normal=h.yaw;h.Finish(m,r);
    m.words=native;h={};invert_x=true;r=regs;begin(h);assert(std::abs(h.yaw+normal)<1e-6);h.Finish(m,r);invert_x=false;
    stick_y=1;stick_x=0;
    for(int i=0;i<100;++i) {r=regs;begin(h);h.Finish(m,r);}
    assert(std::abs(h.pitch-30*3.141592654f/180)<.0001f);
    stick_y=-1;
    for(int i=0;i<100;++i) {r=regs;begin(h);h.Finish(m,r);}
    assert(std::abs(h.pitch+35*3.141592654f/180)<.0001f);
    reset_epoch.fetch_add(1);stick_y=0;r=regs;begin(h);
    assert(h.active && r[15]==Collision && h.yaw_velocity==0 && h.pitch_velocity==0);h.Finish(m,r);
    // A fresh host hook can finish a collision restored from a savestate.
    stick_x=1;r=regs;begin(h);Hook restored;restored.Finish(m,r);assert(r[13]==sp && r[15]==Resume);
    // Distance changes the actual collision query, not the lens, for both
    // ages. It eases toward the requested radius and remains adjustable with
    // the stick released, without dropping orbit ownership.
    stick_y=0;
    for(bool child:{true,false}) {
        m.words=native;h={};m.words[0x58795c]=child?1:0;
        const float base=child?150.f:200.f,height=child?38.f:50.f;
        Put(m,cam+0x80,{0,height,0});Put(m,cam+0x8c,{0,height,-base});
        m.words[cam+0x144]=std::bit_cast<U32>(52.f);
        distance_scale=1.5f;stick_x=1;r=regs;begin(h);
        assert(r[15]==Collision && near(h.radius,base*1.5f));h.Finish(m,r);
        stick_x=0;
        for(int i=0;i<30;++i) {r=regs;begin(h);assert(r[15]==Collision);h.Finish(m,r);}
        assert(std::abs(h.radius-base*1.5f)<.001f);
        const float yaw=h.yaw;
        distance_scale=.75f;
        for(int i=0;i<30;++i) {r=regs;begin(h);assert(r[15]==Collision);h.Finish(m,r);}
        assert(std::abs(h.radius-base*.75f)<.001f && h.yaw==yaw);
        Vec at;assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x80,at));
        assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye));
        float actual=0;for(unsigned i=0;i<3;++i)actual+=(eye[i]-at[i])*(eye[i]-at[i]);
        assert(std::abs(std::sqrt(actual)-base*.75f)<.001f);
        assert(m.words[cam+0x144]==std::bit_cast<U32>(52.f));
    }
    // Untracked native views restore the configured distance without waiting
    // for stick input. Tracked targeting handoffs are tested separately below.
    // Later rotation must not cause another zoom, even at a steep pitch.
    for(bool child:{true,false}) {
        m.words=native;h={};m.words[0x58795c]=child?1:0;
        distance_scale=1.25f;stick_x=stick_y=0;
        const float expected=(child?150.f:200.f)*1.25f;
        r=regs;begin(h);h.Finish(m,r);
        m.words[player+0x1710]=0x20000000;
        r=regs;begin(h);assert(!h.active && r[15]==0x123400);
        const Vec native_at{10,75,20},native_eye{10,45,-20};
        Put(m,cam+0x80,native_at);Put(m,cam+0x8c,native_eye);
        m.words[player+0x1710]=0;m.words[0x516eb0]=0x3200;
        r=regs;begin(h);
        assert(m.words[0x516eb0]==0);
        assert(h.active && r[15]==Collision && near(h.radius,expected));
        assert(near(h.yaw,0) && near(h.pitch,std::atan2(30.f,40.f)));
        h.Finish(m,r);
        for(unsigned frame=0;frame<90;++frame) {
            stick_x=frame>=10 && frame<60?.7f:0;
            stick_y=frame>=10 && frame<35?.6f:frame<60 && frame>=35?-.6f:0;
            r=regs;begin(h);assert(r[15]==Collision && near(h.radius,expected));
            Vec query_at,query_eye;
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13]+0x28,query_at));
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query_eye));
            float length=0;for(unsigned i=0;i<3;++i)length+=std::pow(query_eye[i]-query_at[i],2);
            assert(std::abs(std::sqrt(length)-expected)<.001f);
            h.Finish(m,r);
        }
    }
    // Turning smooth pullback off during recovery requests the full distance
    // immediately, still runs collision, and clears the old recovery history.
    for(bool smoothing:{true,false}) {
        m.words=native;h={};distance_scale=1;stick_x=stick_y=0;
        smooth_collision_return=true;
        r=regs;begin(h,100);h.Finish(m,r);
        h.collision_recovery.distance=40;h.collision_recovery.frame=100;
        assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x80,h.collision_recovery.previous_at));
        h.collision_recovery.valid=true;
        h.collision_recovery.recovering=true;h.collision_recovery.hold=2;
        smooth_collision_return=smoothing;
        r=regs;begin(h,101);
        assert(r[15]==Collision);
        Vec query_at,query_eye;
        assert(OOT3D::TitleScreen::ReadFloats(m,r[13]+0x28,query_at));
        assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query_eye));
        float distance=0;for(unsigned i=0;i<3;++i)distance+=std::pow(query_eye[i]-query_at[i],2);
        assert(std::abs(std::sqrt(distance)-(smoothing?40.f:150.f))<.001f);
        assert(h.collision_recovery.pending==smoothing);
        const Vec contact{0,38,-20};Put(m,r[13],contact);h.Finish(m,r);
        assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==contact);
        if(!smoothing)assert(!h.collision_recovery.valid);
    }
    smooth_collision_return=true;
    // Leaving first person at either extreme preserves the initial angle,
    // then returns inside the range with no stick input. Collision still owns
    // the displayed eye; recovery never writes an untested eye over its result.
    for(float direction:{-1.f,1.f})for(bool reverse:{false,true}) {
        m.words=native;h={};distance_scale=1;stick_x=stick_y=0;
        m.words[player+0x1710]=0x40|0x20000000;
        r=regs;begin(h);assert(!h.active);
        const float inherited=direction*65.f*PitchRadians;
        Put(m,cam+0x80,{0,38,0});
        Put(m,cam+0x8c,{0,38-150*std::sin(inherited),-150*std::cos(inherited)});
        m.words[player+0x1710]=0;
        r=regs;begin(h);assert(near(h.pitch,inherited) && h.pitch_recovery);
        h.Finish(m,r);
        const float limit=direction<0?MinimumPitch:MaximumPitch;
        float previous=h.pitch;
        for(unsigned frame=0;frame<100;++frame) {
            stick_y=reverse && frame<20?-direction:0;
            r=regs;begin(h);assert(r[15]==Collision && h.active);
            assert(direction*(h.pitch-previous)<=1e-6f);
            if(!reverse)assert(direction*(h.pitch-limit)>=-1e-6f);
            if(frame==3) {
                const Vec collision_eye{0,50,-80};Put(m,r[13],collision_eye);
                h.Finish(m,r);
                Vec shown;assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,shown) && shown==collision_eye);
            } else h.Finish(m,r);
            previous=h.pitch;
        }
        assert(!h.pitch_recovery && h.pitch>=MinimumPitch && h.pitch<=MaximumPitch);
        if(!reverse)assert(h.pitch==limit);
    }
    stick_y=0;
    // Native handlers can transfer control during the call. Do not override a
    // newly started dialogue/subcamera or restore the saved pre-call view.
    m.words=native;h={};r=regs;h.Begin(m,r);
    assert(r[15]==0x123400 && m.Read(r[13]+0x38)==Hook::NativeCookie);
    m.words[cam+0x18c]=3;const Vec conversation_eye{100,90,80};
    Put(m,cam+0x8c,conversation_eye);h.Finish(m,r);
    assert(!h.active && r[13]==sp && r[15]==Resume);
    assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==conversation_eye);
    // Riding Epona permits orbit; aiming/targeting, dialogue and scripted
    // cameras still retain native ownership with the mounted bit set.
    m.words=native;m.words[player+0x1710]=MountedFlag;
    assert(Allowed(m,cam));
    m.words[player+0x1714]=0x40000;assert(!Allowed(m,cam));m.words[player+0x1714]=0;
    m.words[cam+0x18c]=3;assert(!Allowed(m,cam));
    m.words[cam+0x18c]=2;assert(!Allowed(m,cam) && Targeting(m,cam));
    m.words[cam+0x18c]=0;m.words[cam+0x188]=7|(0x1d<<16);assert(!Allowed(m,cam));
    // A sign/conversation request owns framing before the mode has reached
    // TALK. Merely having a target or the flag alone must not disable orbit.
    m.words=native;h={};stick_x=stick_y=0;
    m.words[player+4]=0x100;assert(Allowed(m,cam));
    m.words[player+4]=0;m.words[player+0x16f8]=0x09100000;
    assert(Allowed(m,cam));
    r=regs;begin(h);h.Finish(m,r);assert(h.active);
    m.words[player+4]=0x100;r=regs;h.Begin(m,r);
    assert(!h.active && r[15]==0x123400 && r[13]==sp);
    Put(m,cam+0x8c,conversation_eye);h.Finish(m,r);
    assert(r[15]==Resume && r[13]==sp);
    assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==conversation_eye);
    // Release resumes the configured distance without waiting for stick input.
    m.words[player+4]=0;m.words[player+0x16f8]=0;
    r=regs;begin(h);assert(h.active && near(h.radius,150.f*distance_scale.load()));h.Finish(m,r);
    // The same ownership request can arrive inside a native handler.
    r=regs;h.Begin(m,r);m.words[player+4]=0x100;m.words[player+0x16f8]=0x09100000;
    Put(m,cam+0x8c,conversation_eye);h.Finish(m,r);
    assert(!h.active && r[13]==sp && r[15]==Resume);
    assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==conversation_eye);
    // Player flags can change before Camera.mode. Cover both directions of
    // that handoff without letting the native narrow FOV through for a frame.
    for(U32 flags:{0x8000u,0x10000u,0x18000u,0x20000u,0x28000u,0x30000u,0x38000u}) {
        m.words=native;h={};m.words[player+0x1710]=flags;
        distance_scale=1.4f;stick_x=stick_y=1;
        m.words[play+0x188]=0;m.words[play+0x18c]=400;
        m.words[play+0x190]=0;m.words[play+0x194]=240;
        assert(Targeting(m,cam) && !Allowed(m,cam) && GameplayLens(m,play+0x188));
        std::array<U32,64> fp{};
        fp[16]=std::bit_cast<U32>(25.f);fp[23]=0x3c8efa35;
        OOT3D::Camera::Handle(m,play+0x188,fp,65,GameplayLens(m,play+0x188));
        assert(near(std::bit_cast<float>(fp[0]),65*std::bit_cast<float>(fp[23])));
        assert(fp[16]==std::bit_cast<U32>(25.f)); // Native lens state retained.
        r=regs;h.Begin(m,r);assert(r[15]==0x123400 && r[13]==sp-Hook::ScratchSize);
        Put(m,cam+0x80,{0,38,0});Put(m,cam+0x8c,{0,38,-40});
        h.Finish(m,r);assert(r[15]==Collision && h.target_view && !h.active);
        h.Finish(m,r);
        assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye));
        assert(std::abs(eye[2]+210)<.001f);
        // Native targeting ends normally even with stale flags on this tick.
        m.words[player+0x1710]=0;stick_x=stick_y=0;r=regs;begin(h);h.Finish(m,r);
        assert(h.active && !h.target_view && m.words[0x516eb0]==0);
        m.words[cam+0x18c]=3;m.words[player+0x1710]=flags;
        assert(!Targeting(m,cam) && !GameplayLens(m,play+0x188)); // Dialogue still owns lens.
    }
    // Captured non-enemy slingshot sequence: ready 18008 -> draw 18208 ->
    // ready 18008 -> charge 18208. Bit 200 used to drop both distance and
    // lens coverage on each pull, even though the camera stayed in mode 2.
    for(bool child:{true,false})for(U32 mode:{0u,1u,2u,4u}) {
        m.words=native;h={};m.words[0x58795c]=child?1:0;
        m.words[cam+0x18c]=mode;distance_scale=1.1f;stick_x=stick_y=1;
        m.words[play+0x188]=0;m.words[play+0x18c]=400;
        m.words[play+0x190]=0;m.words[play+0x194]=240;
        m.words[player+0x1714]=0x100002;
        const float height=child?38.f:50.f,minimum=(child?150.f:200.f)*1.1f;
        for(U32 flags:{0x18000u,0x18008u,0x18208u,0x18208u,
                       0x18008u,0x18208u,0x18208u,0x18008u}) {
            m.words[player+0x1710]=flags;
            assert(Targeting(m,cam) && !Allowed(m,cam) && GameplayLens(m,play+0x188));
            std::array<U32,64> fp{};
            fp[16]=std::bit_cast<U32>(45.f);fp[23]=0x3c8efa35;
            OOT3D::Camera::Handle(m,play+0x188,fp,64,GameplayLens(m,play+0x188));
            assert(near(std::bit_cast<float>(fp[0]),64*std::bit_cast<float>(fp[23])));
            assert(fp[16]==std::bit_cast<U32>(45.f));
            r=regs;h.Begin(m,r);
            assert(r[15]==0x123400 && r[13]==sp-Hook::ScratchSize);
            const Vec aim{20,height,0},close_eye{20,height,-120};
            Put(m,cam+0x80,aim);Put(m,cam+0x8c,close_eye);
            h.Finish(m,r);
            assert(r[15]==Collision && h.target_view && !h.active);
            Vec query;assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query));
            assert(query[0]==20 && query[1]==height && std::abs(query[2]+minimum)<.001f);
            h.Finish(m,r);
            assert(r[13]==sp && r[15]==Resume);
        }
        // First-person look/bow/slingshot modes, including stale lock flags,
        // must still yield, as must unrelated player-state exclusion bits.
        for(U32 aim_mode:{6u,7u,11u})for(U32 flags:{0x208u,0x18208u,0x100208u}) {
            m.words[cam+0x18c]=aim_mode;m.words[player+0x1710]=flags;
            assert(!Targeting(m,cam) && !Allowed(m,cam) && !GameplayLens(m,play+0x188));
        }
        m.words[cam+0x18c]=mode;
        for(U32 blocked:{0x10u,0x20u,0x100000u}) {
            m.words[player+0x1710]=0x18208|blocked;
            assert(!Targeting(m,cam) && !GameplayLens(m,play+0x188));
        }
        // A conversation or free-aim transition inside the native dispatch
        // takes ownership immediately, including when charge flags persist.
        for(U32 next_mode:{3u,7u,11u}) {
            m.words[cam+0x18c]=mode;m.words[player+0x1710]=0x18208;
            r=regs;h.Begin(m,r);
            m.words[cam+0x18c]=next_mode;Put(m,cam+0x8c,conversation_eye);
            h.Finish(m,r);
            assert(r[13]==sp && r[15]==Resume && !h.target_view && !h.active);
            assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==conversation_eye);
            assert(!GameplayLens(m,play+0x188));
        }
        m.words[cam+0x18c]=0;m.words[player+0x1710]=0x208;
        assert(!Allowed(m,cam) && !Targeting(m,cam)); // Unlocked draw remains native.
    }
    // Regression: radial distance is already large but view depth is tiny.
    // The old minimum-radius guard did nothing here, allowing a large apparent
    // zoom. Check both target sides/ages and preserve the native viewing ray.
    for(bool child:{true,false})for(float side:{-1.f,1.f}) {
        m.words=native;h={};m.words[0x58795c]=child?1:0;
        m.words[cam+0x18c]=2;m.words[player+0x1710]=0x10000;
        distance_scale=1.4f;
        const float height=child?38.f:50.f,minimum=(child?150.f:200.f)*1.4f;
        const Vec at{side*minimum*2,height,0},close_eye{at[0],height,-20};
        assert(std::hypot(close_eye[0],close_eye[2])>minimum);
        r=regs;h.Begin(m,r);Put(m,cam+0x80,at);Put(m,cam+0x8c,close_eye);h.Finish(m,r);
        assert(r[15]==Collision);
        Vec query;assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query));
        assert(query[0]==close_eye[0] && query[1]==height && std::abs(query[2]+minimum)<.001f);
        h.Finish(m,r);
    }
    // Parallel, enemy and friendly targeting use native aim, never C-stick
    // angles, but keep the configured minimum radius for either age.
    // Player_Update sets 0x8000 on the Z press (250dd4..250de4), then
    // combines it with the actor/parallel flags. It must not disable either
    // the distance override or its lens policy, including the first frame.
    for(bool child:{true,false})for(U32 mode:{1u,2u,4u})
    for(U32 pressed:{0u,0x8000u})for(float distance:{1.f,1.5f,2.f}) {
        m.words=native;h={};m.words[0x58795c]=child?1:0;
        m.words[cam+0x18c]=mode;
        m.words[player+0x1710]=(mode==1?0x20000:mode==2?0x10000:0x20000000)|pressed;
        if(mode!=1)m.words[player+0x16f8]=0x09100000;
        distance_scale=distance;stick_x=stick_y=1;
        const float minimum=(child?150.f:200.f)*distance;
        assert(Targeting(m,cam) && !Allowed(m,cam) && GameplayLens(m,play+0x188));
        for(unsigned frame=0;frame<60;++frame) {
            r=regs;h.Begin(m,r);
            assert(r[15]==0x123400 && r[13]==sp-Hook::ScratchSize);
            // A changing native target/angle must be used instead of the saved
            // pre-dispatch view. Its interface writes must also survive.
            const Vec native_at{float(frame),75,20};
            const Vec native_eye{float(frame),45,-20}; // 50 units, 3:4 direction
            Put(m,cam+0x80,native_at);Put(m,cam+0x8c,native_eye);
            m.words[0x516eb0]=0x3200;
            h.Finish(m,r);
            assert(!h.active && h.yaw_velocity==0 && h.pitch_velocity==0);
            assert(r[15]==Collision && r[14]==Sites[1].pc);
            Vec query_at,query_eye;
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13]+0x28,query_at) && query_at==native_at);
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query_eye));
            assert(near(query_eye[0],native_at[0]));
            // Target aim is offset from Link; the minimum protects view depth
            // from Link's head, while the native 3:4 viewing direction is retained.
            const float head_y=child?38.f:50.f;
            Vec ray;float ray_length=0,depth=0;
            for(unsigned axis=0;axis<3;++axis){ray[axis]=query_eye[axis]-query_at[axis];ray_length+=ray[axis]*ray[axis];}
            ray_length=std::sqrt(ray_length);
            for(unsigned axis=0;axis<3;++axis)depth+=(query_eye[axis]-(axis==1?head_y:0))*ray[axis]/ray_length;
            assert(std::abs(depth-minimum)<.001f);
            assert(std::hypot(query_eye[0],query_eye[1]-head_y,query_eye[2])>=minimum-.001f);
            assert(std::abs((query_eye[1]-native_at[1])*4-(query_eye[2]-native_at[2])*3)<.001f);
            // Collision is authoritative even when it needs a closer camera.
            if(frame==59)Put(m,r[13],native_eye);
            h.Finish(m,r);
            assert(r[15]==Resume && r[13]==sp && m.words[0x516eb0]==0x3200);
            assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye));
            assert(eye==(frame==59?native_eye:query_eye));
        }
        // Wide framing is retained, with no multiplying feedback each frame.
        for(unsigned frame=0;frame<10;++frame) {
            r=regs;h.Begin(m,r);
            const Vec wide{0,38,-minimum*2};
            Put(m,cam+0x80,{0,38,0});Put(m,cam+0x8c,wide);
            h.Finish(m,r);assert(r[15]==Resume && r[13]==sp);
            assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==wide);
        }
        // Signs, first person, special settings, subcameras and disabled orbit
        // never get the targeting radius/lens override.
        for(auto [address,value]:std::array<std::pair<U32,U32>,5>{{
             {cam+0x18c,3},{player+0x1710,0x100000},{player+4,0x100},
             {cam+0x188,7|(0x14<<16)},{play+0xa64,1}}}) {
            const U32 old=m.words[address];m.words[address]=value;
            // A sign request includes its actor even in parallel mode.
            m.words[player+0x16f8]=0x09100000;
            assert(!Targeting(m,cam) && !GameplayLens(m,play+0x188));
            m.words[address]=old;
        }
        enabled=false;assert(!Targeting(m,cam) && !GameplayLens(m,play+0x188));enabled=true;
        m.words[cam+0x18c]=0;m.words[player+0x1710]=0;m.words[player+0x16f8]=0;
        stick_x=stick_y=0;
        Vec release_eye,release_at;
        assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,release_eye));
        assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x80,release_at));
        r=regs;begin(h);assert(h.active);
        Vec query_eye,query_at;
        assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query_eye));
        assert(OOT3D::TitleScreen::ReadFloats(m,r[13]+0x28,query_at));
        for(unsigned i=0;i<3;++i) {
            assert(std::abs(query_eye[i]-release_eye[i])<.001f);
            assert(std::abs(query_at[i]-release_at[i])<.001f);
        }
        h.Finish(m,r);
        for(unsigned frame=0;frame<Hook::ReleaseFrames+2;++frame) {
            r=regs;begin(h);h.Finish(m,r);
        }
        assert(near(h.radius,minimum));
    }
    // Releasing an offset target must not translate the eye at all when its
    // distance already matches orbit. Only the look-at point returns to Link.
    // Test both a nearby collision and a free camera, and both target sides.
    for(bool clipped_by_wall:{false,true})for(float side:{-1.f,1.f}) {
        m.words=native;h={};distance_scale=1.25f;stick_x=stick_y=0;
        const float configured=150.f*1.25f, actual=clipped_by_wall?70.f:configured;
        const Vec target_at{side*80,65,50},target_eye{0,38,-actual};
        m.words[cam+0x18c]=2;m.words[player+0x1710]=0x10000;
        r=regs;h.Begin(m,r);
        Put(m,cam+0x80,target_at);Put(m,cam+0x8c,target_eye);h.Finish(m,r);
        if(r[15]==Collision) {Put(m,r[13],target_eye);h.Finish(m,r);}
        m.words[cam+0x18c]=0;m.words[player+0x1710]=0;
        Vec previous_at=target_at;
        float previous_radius=actual;
        for(unsigned frame=0;frame<=Hook::ReleaseFrames+12;++frame) {
            r=regs;begin(h);
            Vec query_at,query_eye;
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13]+0x28,query_at));
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query_eye));
            if(frame==0) {
                for(unsigned i=0;i<3;++i) {
                    assert(std::abs(query_eye[i]-target_eye[i])<.001f);
                    assert(std::abs(query_at[i]-target_at[i])<.001f);
                }
            }
            const float radius_now=std::hypot(query_eye[0],query_eye[1]-38,query_eye[2]);
            assert(radius_now>=previous_radius-.001f && radius_now<=configured+.001f);
            assert(radius_now-previous_radius<configured*.21f);
            if(!clipped_by_wall)for(unsigned i=0;i<3;++i)
                assert(std::abs(query_eye[i]-target_eye[i])<.001f);
            assert(std::abs(query_at[0])<=std::abs(previous_at[0])+.001f);
            if(frame>=Hook::ReleaseFrames) {
                assert(std::abs(radius_now-configured)<.001f);
                assert(std::abs(query_at[0])<.001f && std::abs(query_at[1]-38)<.001f && std::abs(query_at[2])<.001f);
            }
            previous_radius=radius_now;previous_at=query_at;
            // A wall may continue to clip the interpolated request; it must
            // remain authoritative throughout the handoff.
            if(clipped_by_wall)Put(m,r[13],target_eye);
            h.Finish(m,r);
            assert(m.words[0x516eb0]==0);
            if(clipped_by_wall) {
                assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==target_eye);
            }
        }
        // Stick movement after the handoff cannot trigger another pullback.
        stick_x=.7f;
        for(unsigned frame=0;frame<10;++frame) {
            r=regs;begin(h);assert(near(h.radius,configured));h.Finish(m,r);
        }
    }
    // MM-style dialogue hands over the final displayed view, even if native
    // NORMAL has already restored a different pre-dialogue eye. Recover with
    // no stick input, at both ages/scales, on either side and near collision.
    for(bool child:{true,false})for(float scale:{1.f,2.f})
    for(float side:{-1.f,1.f})for(bool clipped:{false,true})for(U32 residual_flags:{0u,0x10000u,0x18000u}) {
        m.words=native;h={};distance_scale=scale;stick_x=stick_y=0;
        m.words[0x58795c]=child?1:0;
        const float height=child?38.f:50.f,goal=(child?150.f:200.f)*scale;
        const Vec talk_eye{side*(clipped?35.f:140.f),height+20,-50};
        const Vec talk_at{0,height,60};
        m.words[cam+0x18c]=3;r=regs;h.Begin(m,r,100);
        assert(!h.active && r[13]==sp); // Native TALK retains ownership.
        // Captured locked-on conversation: 20018040 during TALK, 00018000
        // on the first NORMAL update. This bit is not a new Z press.
        m.words[player+0x1710]=residual_flags|0x20000040;
        h.RememberDialogue(m,cam,talk_eye,talk_at,40.f,100);
        m.words[cam+0x18c]=0;
        Put(m,cam+0x8c,{0,height,-goal});Put(m,cam+0x80,{0,height,0});
        float last_fov=40.f;
        for(unsigned frame=0;frame<=Hook::DialogueReleaseFrames+3;++frame) {
            // Captured sign/NPC exit: NORMAL with target already cleared but
            // Targeting flags still present for one update. Do not reacquire
            // the targeting camera or snap the lens to the gameplay FOV.
            m.words[player+0x1710]=frame==0?residual_flags:0;
            m.words[player+0x1714]=residual_flags && frame==0?0x00100022:0x00100000;
            m.words[player+0x16f8]=0;
            r=regs;h.Begin(m,r,101+frame);
            assert(r[13]==sp-Hook::ScratchSize);
            // Native NORMAL still runs its mode/interface bookkeeping.
            Put(m,cam+0x8c,{0,90,-20});Put(m,cam+0x80,{0,90,0});
            m.words[0x516eb0]=0;h.Finish(m,r);
            assert(r[15]==Collision && h.active);
            Vec request,aim;
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13],request));
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13]+0x28,aim));
            if(frame==0)for(unsigned i=0;i<3;++i) {
                assert(std::abs(request[i]-talk_eye[i])<.001f);
                assert(std::abs(aim[i]-talk_at[i])<.001f);
            }
            // The camera stays on the dialogue side, not the old rear view.
            assert(request[0]*side>0);
            const float fov=h.GameplayFov(m,play+0x188);
            assert(fov>=last_fov && fov<=52.f && fov-last_fov<.5f);
            if(frame==0)assert(fov==40.f);
            if(frame==30)assert(fov>48 && fov<49.5f);
            if(frame>=Hook::DialogueReleaseFrames) {
                assert(fov==52.f && std::abs(h.radius-goal)<.001f);
                for(unsigned i=0;i<3;++i)assert(std::abs(aim[i]-(i==1?height:0))<.001f);
            }
            last_fov=fov;
            // Both primary and inherited secondary projections receive the
            // same lens, widened once; guest FOV is not mutated.
            m.words[play+0x188]=0;m.words[play+0x18c]=400;
            m.words[play+0x190]=0;m.words[play+0x194]=240;
            std::array<U32,64> fp{};
            fp[16]=std::bit_cast<U32>(25.f);fp[23]=std::bit_cast<U32>(1.f);
            for(unsigned projection=0;projection<2;++projection) {
                OOT3D::Camera::Handle(m,play+0x188,fp,70,true,h.GameplayFov(m,play+0x188));
                assert(near(std::bit_cast<float>(fp[0]),OOT3D::Camera::Widen(fov,70)));
                assert(std::bit_cast<float>(fp[16])==25.f);
            }
            if(clipped)Put(m,r[13],talk_eye);
            h.Finish(m,r);
            if(clipped) {
                Vec actual;assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,actual) && actual==talk_eye);
            }
        }
    }
    // After a locked-on conversation's inherited Z flag clears, pressing Z
    // again still interrupts the return and gives targeting back to native code.
    m.words=native;h={};distance_scale=1;stick_x=stick_y=0;
    m.words[player+0x1710]=0x20018040;
    h.RememberDialogue(m,cam,{140,58,-50},{0,38,60},40,100);
    for(unsigned frame=0;frame<3;++frame) {
        m.words[player+0x1710]=frame==1?0:0x18000;
        m.words[player+0x16f8]=0;
        r=regs;h.Begin(m,r,101+frame);
        Put(m,cam+0x80,{0,38,0});Put(m,cam+0x8c,{0,38,-40});
        h.Finish(m,r);
        if(frame<2) {
            assert(h.dialogue_release && h.active);
            assert(h.release_z_latched==(frame==0));
            assert(h.GameplayFov(m,play+0x188)<52);
        } else {
            assert(!h.dialogue_release && h.target_view && !h.active);
            assert(h.GameplayFov(m,play+0x188)==52);
        }
        if(r[15]==Collision)h.Finish(m,r);
        assert(r[13]==sp);
    }
    // Stale TALK views cannot leak into scene changes, player replacement,
    // savestates, first person, targeting, disabled orbit or other cameras.
    for(unsigned reason=0;reason<12;++reason) {
        m.words=native;h={};r=regs;distance_scale=1;stick_x=stick_y=0;
        h.RememberDialogue(m,cam,{100,45,-100},{0,38,30},40,100);
        if(reason==0)m.words[play+0x104]++;
        if(reason==1)m.words[cam+0xd8]=0x09100000;
        if(reason==2)++reset_epoch;
        if(reason==3)m.words[player+0x1710]=0x100000;
        if(reason==4){m.words[cam+0x18c]=1;m.words[player+0x1710]=0x20000;}
        if(reason==5)enabled=false;
        if(reason==6)m.words[play+0xa64]=1;
        if(reason==8)m.words[player+0x1710]=0x18000; // New Z press wins immediately.
        if(reason==9){m.words[player+0x1710]=0x10000;m.words[player+0x16f8]=0x09100000;}
        if(reason==10)m.words[player+0x1710]=0x20000000; // Scripted auto-walk.
        if(reason==11)m.words[player+0x1710]=0x20000; // Real parallel targeting.
        h.Begin(m,r,reason==7?103:101);
        assert(!h.dialogue.valid && h.GameplayFov(m,play+0x188)==52.f);
        enabled=true;
    }
    // The newly inserted native-call stage is self-contained in guest stack
    // memory too: a fresh host hook can resume it after loading a savestate.
    m.words=native;h={};r=regs;h.Begin(m,r);Hook from_native_save;
    from_native_save.Finish(m,r);assert(r[15]==Collision && r[13]==sp-Hook::ScratchSize);
    from_native_save.Finish(m,r);assert(r[15]==Resume && r[13]==sp);
    // Older saves can contain the prior 0x40-byte dispatch/collision frame.
    // Finish its exact allocation, then use the new layout for any next call.
    for(bool collision:{false,true}) {
        m.words=native;r=regs;Hook legacy;
        const U32 saved_sp=sp-0x40;
        Put(m,saved_sp,{0,38,-150});Put(m,saved_sp+0x28,{0,38,0});
        const U32 cookie=collision?Hook::LegacyCookie:Hook::LegacyNativeCookie;
        m.words[saved_sp+0x34]=cam;m.words[saved_sp+0x38]=cookie;
        m.words[saved_sp+0x3c]=saved_sp^cookie;r[13]=saved_sp;
        legacy.Finish(m,r);
        if(!collision) {
            assert(r[15]==Collision && r[13]==sp-Hook::ScratchSize);
            assert(m.Read(r[13]+0x38)==Hook::Cookie);
            legacy.Finish(m,r);
        }
        assert(r[13]==sp && r[15]==Resume);
    }
    assert(native_calls>200);
    distance_scale=DefaultDistance;
    // Native entrance walks keep the gameplay distance/lens through the fade
    // and the remaining auto-walk, but never enable right-stick input.
    for(bool child:{false,true}) {
        m.words=native;h={};entry_state={};distance_scale=1.4f;stick_x=1;stick_y=1;
        m.words[0x58795c]=child?1:0;
        const float desired=(child?150.f:200.f)*1.4f;
        m.words[play+0x4c30]=0;m.words[play+0x4c34]=0;
        m.words[play+0x5c2c]=0;m.words[play+0x7f10]=1u<<16;
        m.words[player+0x1710]=0x20000000;
        for(unsigned frame=0;frame<50;++frame) {
            if(frame==15)m.words[play+0x7f10]=0; // fade ends before auto-walk
            r=regs;h.Begin(m,r);
            assert(r[15]==0x123400 && r[13]==sp-Hook::ScratchSize);
            assert(!Allowed(m,cam) && EntryView(m,cam) && GameplayLens(m,play+0x188));
            const Vec at{float(frame),child?38.f:50.f,0};
            // The native entrance camera tries to change its distance each tick.
            Put(m,cam+0x80,at);Put(m,cam+0x8c,{at[0],at[1],-60.f-float(frame)});
            h.Finish(m,r);assert(r[15]==Collision && !h.active && h.yaw_velocity==0 && h.pitch_velocity==0);
            h.Finish(m,r);assert(r[13]==sp);
            assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye));
            assert(std::abs(eye[2]+desired)<.001f && eye[0]==at[0]);
        }
        // First controllable frame does not change the distance or lens policy.
        stick_x=stick_y=0;m.words[player+0x1710]=0;r=regs;begin(h);
        assert(!EntryView(m,cam) && GameplayLens(m,play+0x188) && std::abs(h.radius-desired)<.001f);
        h.Finish(m,r);
        // A later scripted action without an area transition remains native.
        m.words[player+0x1710]=0x20000000;r=regs;h.Begin(m,r);
        assert(r[13]==sp && !EntryView(m,cam) && !GameplayLens(m,play+0x188));
        // Never apply to an actual cutscene, conversation, inactive/subcamera,
        // scene change, restored state, or disabled mod.
        m.words[play+0x7f10]=1u<<16;r=regs;h.Begin(m,r);assert(EntryView(m,cam));
        for(auto [address,value]:std::array<std::pair<U32,U32>,5>{{
            {play+0x4c30,3u<<16},{play+0x4c34,1u<<24},{cam+0x18c,3},
            {play+0xa64,1},{play+0x104,0x56}}}) {
            const auto old=m.words[address];m.words[address]=value;
            assert(!EntryView(m,cam));m.words[address]=old;
        }
        enabled=false;assert(!EntryView(m,cam));enabled=true;
        reset_epoch.fetch_add(1);assert(!EntryView(m,cam));
    }
    entry_state={};
    // Auto-walk starts before DOOR0. Capture 4635 -> 4636 uses setting 4
    // and cut ~90 units inward; the opposite side uses 62 and was protected.
    // Every ordinary gameplay setting must use the same handoff, preserving
    // collision and holding the lens until the native door side switch.
    for(U32 setting=0;setting<=0x47;++setting)for(bool interrupted:{false,true})for(bool delayed:{false,true}) {
        m.words=native;h={};entry_state={};stick_x=stick_y=0;distance_scale=1.25f;
        m.words[play+0x4c30]=m.words[play+0x4c34]=m.words[play+0x5c2c]=m.words[play+0x7f10]=0;
        m.words[cam+0x188]=7|(setting<<16);m.words[cam+0x144]=std::bit_cast<U32>(55.f);
        const bool eligible=EntryContext(m,cam);
        if(setting==1 || setting==3 || setting==4 || setting==62)assert(eligible);
        const Vec before_eye=setting==4?Vec{-62.242558f,-773.05847f,780.01099f}:
            Vec{-583.30188f,516.00861f,601.92096f};
        const Vec before_at=setting==4?Vec{-69.154236f,-842.f,606.85919f}:
            Vec{-470.63962f,438.01221f,475.40601f};
        Put(m,cam+0x80,before_at);Put(m,cam+0x8c,before_eye);
        h.active=true;h.owner=cam;h.player_owner=player;h.scene=0x55;h.epoch=reset_epoch.load();
        h.current_frame=6861;h.last_eye=before_eye;
        m.words[player+0x1710]=0x20000000;
        r=regs;h.Begin(m,r,6862);assert(r[15]==0x123400 && !h.active);
        if(!eligible) {
            assert(!h.door_start.valid && r[13]==sp);
            h.Finish(m,r);assert(r[13]==sp && r[15]==Resume);
            continue;
        }
        Put(m,cam+0x80,setting==4?Vec{-69.249977f,-847.82318f,605.14368f}:
            Vec{-469.83905f,436.04523f,474.87695f});
        Put(m,cam+0x8c,setting==4?Vec{-65.478409f,-813.02502f,699.47394f}:
            Vec{-566.43213f,501.405f,583.27753f});
        h.Finish(m,r);assert(r[15]==Collision);
        Vec query;assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query) && query==before_eye);
        // Collision remains authoritative even on the preserved handoff frame.
        const Vec corrected{before_eye[0]+1,before_eye[1],before_eye[2]};Put(m,r[13],corrected);
        h.Finish(m,r);assert(r[13]==sp && r[15]==Resume && h.native_entry.valid);
        assert(h.DoorStartLens(m,play+0x188) && h.GameplayFov(m,play+0x188)==52.f);
        if(delayed) {
            // Jabu capture: a second NORMAL update pulls radius 163 -> 85
            // before DOOR0 starts. Preserve eye/aim/lens through that update.
            r=regs;h.Begin(m,r,6863);assert(r[13]==sp-Hook::ScratchSize && r[15]==0x123400);
            Put(m,cam+0x8c,Vec{4.2282267f,-272.14307f,-1019.2103f});
            h.Finish(m,r);assert(r[15]==Collision);
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query) && query==before_eye);
            Put(m,r[13],corrected);h.Finish(m,r);
            assert(r[13]==sp && h.native_entry.frame==6863);
            assert(h.DoorStartLens(m,play+0x188) && h.GameplayFov(m,play+0x188)==52.f);
        }
        m.words[cam+0x188]=7|(29u<<16);m.words[cam+0x1a4]=1u<<16;
        float previous=52;
        for(unsigned tick=1;tick<=18;++tick) {
            if(interrupted && tick==1)reset_epoch.fetch_add(1);
            r=regs;h.Begin(m,r,6862+unsigned(delayed)+tick);assert(r[13]==sp && r[15]==0x123400);
            h.Finish(m,r);assert(r[13]==sp && r[15]==Resume);
            assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,query) && query==corrected);
            if(interrupted)assert(!h.DoorStartLens(m,play+0x188));
            else {
                const float lens=h.GameplayFov(m,play+0x188);
                assert(h.DoorStartLens(m,play+0x188) && lens==52.f);
                previous=lens;
            }
        }
        if(!interrupted)assert(previous==52);
        m.words[cam+0x1a4]=2u<<16;
        assert(!h.DoorStartLens(m,play+0x188));
    }
    // The extra NORMAL update is a bridge, not a camera lock. Reject stale
    // ownership and unrelated transitions, and never reacquire after DOOR0.
    for(unsigned fault=0;fault<11;++fault) {
        m.words=native;h={};entry_state={};
        m.words[play+0x4c30]=m.words[play+0x4c34]=m.words[play+0x5c2c]=m.words[play+0x7f10]=0;
        m.words[cam+0x188]=7|(4u<<16);m.words[player+0x1710]=0x20000000;
        const Vec at{0,38,0},eye{0,60,160};
        Put(m,cam+0x80,at);Put(m,cam+0x8c,eye);
        h.active=true;h.owner=cam;h.player_owner=player;h.scene=0x55;h.epoch=reset_epoch.load();
        h.current_frame=100;h.last_eye=eye;
        r=regs;h.Begin(m,r,101);h.Finish(m,r);assert(r[15]==Collision);h.Finish(m,r);
        assert(h.door_start.valid);
        std::uint64_t next=102;
        switch(fault) {
        case 0: next=103;break; // Pending window expired / skipped update.
        case 1: m.words[cam+0x188]=7|(3u<<16);break;
        case 2: m.words[player+0x1710]=0;break;
        case 3: m.words[play+0x104]=0x56;break;
        case 4: reset_epoch.fetch_add(1);break;
        case 5: m.words[cam+0xd8]=player+0x100;break;
        case 6: m.words[play+0x7f10]=1u<<16;break;
        case 7: m.words[play+0x22a0]=1;break;
        case 8: m.words[player+0x1710]|=0x8000;break;
        case 9: m.words[play+0x5c2c]=1u<<8;break;
        case 10:
            m.words[cam+0x188]=7|(29u<<16);m.words[cam+0x1a4]=1u<<16;
            r=regs;h.Begin(m,r,102);h.Finish(m,r);assert(h.door_native_started);
            m.words[cam+0x188]=7|(4u<<16);break;
        }
        r=regs;h.Begin(m,r,next);
        assert(!h.door_start.valid && !h.DoorStartLens(m,play+0x188));
    }
    // Quit switches gameMode before the scene goes black. Retain the Save
    // screen's last actual eye/aim and lens while native camera code runs.
    for(unsigned fault=0;fault<5;++fault) {
        m.words=native;h={};entry_state={};
        const U32 view=play+0x188;
        m.words[view]=0;m.words[view+4]=400;m.words[view+8]=0;m.words[view+12]=240;
        m.words[play+0x100]=0x203;
        m.words[OOT3D::SaveMenu::Owner+0x28]=2;m.words[OOT3D::SaveMenu::Owner+0x24]=11;
        const Vec eye{10,70,165},at{10,38,0};Put(m,cam+0x8c,eye);Put(m,cam+0x80,at);
        h.RememberSaveView(m,view,52);assert(h.save_view.valid);
        h.RememberSaveView(m,view+0x1000,60);assert(h.save_view.valid); // Other views cannot discard it.
        m.words[OOT3D::SaveMenu::Owner+0x24]=16;assert(h.SaveQuitLens(m,view));
        // Native state16 closes Save and requests the title transition.
        m.words[OOT3D::SaveMenu::Owner+0x28]=0;m.words[0x588e3c]=1;
        m.words[play+0x5c2c]=20u<<8;m.words[play+0x5c74]=2u<<16;
        for(unsigned tick=1;tick<4;++tick) {
            r=regs;h.Begin(m,r,tick);assert(r[15]==0x123400 && r[13]==sp-Hook::ScratchSize);
            Put(m,cam+0x8c,Vec{0,20,80});Put(m,cam+0x80,Vec{0,0,0});
            h.Finish(m,r);assert(r[15]==Resume && r[13]==sp);
            Vec actual;assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,actual) && actual==eye);
            assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x80,actual) && actual==at);
            assert(h.SaveQuitLens(m,view) && h.GameplayFov(m,view)==52);
            h.RememberSaveView(m,view,55);assert(h.save_view.fov==52);
        }
        if(fault==0)m.words[OOT3D::SaveMenu::Owner+0x24]=14; // Continue, not Quit.
        if(fault==1)m.words[play+0x104]=0x56;
        if(fault==2)reset_epoch.fetch_add(1);
        if(fault==3)m.words[cam+0xd8]=player+0x100;
        if(fault==4)m.words[play+0x5c2c]=0;
        assert(!h.SaveQuitLens(m,view));
        h.RememberSaveView(m,view,55);assert(!h.save_view.valid);
    }
    // Some entrances run a restricted native setting/status until after the
    // scene fade. Observe (without overriding) that view, then preserve its
    // eye, aim and lens on the first gameplay frame and ease to the user's
    // distance. Also cover a regular auto-walk between those two states.
    for(bool child:{false,true})for(bool auto_walk:{false,true}) {
        m.words=native;h={};entry_state={};stick_x=stick_y=0;distance_scale=2;
        m.words[0x58795c]=child?1:0;
        m.words[play+0x4c30]=m.words[play+0x4c34]=m.words[play+0x5c2c]=0;
        m.words[play+0x7f10]=1u<<16;
        m.words[player+0x1710]=0x20000000;
        m.words[cam+0x188]=5|(0x23u<<16);
        m.words[cam+0x144]=std::bit_cast<U32>(40.f);
        const Vec entrance_at{10,60,20},entrance_eye{10,70,-80};
        const float height=child?38.f:50.f,goal=(child?150.f:200.f)*2;
        const float start_radius=std::hypot(10.f,70-height,-80.f);
        for(unsigned frame=0;frame<5;++frame) {
            r=regs;h.Begin(m,r,frame);
            assert(!Allowed(m,cam) && !EntryView(m,cam) && r[13]==sp && r[15]==0x123400);
            Put(m,cam+0x80,entrance_at);Put(m,cam+0x8c,entrance_eye);
            h.Finish(m,r);
            assert(h.native_entry.valid && !h.active && r[13]==sp && r[15]==Resume);
            assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==entrance_eye);
        }
        m.words[cam+0x188]=7|(1u<<16);
        if(auto_walk) {
            r=regs;h.Begin(m,r,5);assert(EntryView(m,cam));
            h.Finish(m,r);
            assert(r[13]==sp && r[15]==Resume && h.native_entry.valid);
            assert(near(h.GameplayFov(m,play+0x188),40.f));
            assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==entrance_eye);
        }
        m.words[play+0x7f10]=0;m.words[player+0x1710]=0;
        float previous=start_radius,previous_fov=40.f;
        for(unsigned tick=0;tick<=Hook::ReleaseFrames;++tick) {
            // Native mode changes may restore a different eye before dispatch.
            if(tick==0)Put(m,cam+0x8c,{0,height,-200});
            r=regs;h.Begin(m,r,5+unsigned(auto_walk)+tick);
            Put(m,cam+0x80,{0,height,0});Put(m,cam+0x8c,{0,height,-90});
            m.words[cam+0x144]=std::bit_cast<U32>(52.f);
            h.Finish(m,r);assert(r[15]==Collision && h.active);
            Vec query_eye,query_at;
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13],query_eye));
            assert(OOT3D::TitleScreen::ReadFloats(m,r[13]+0x28,query_at));
            const float lens=h.GameplayFov(m,play+0x188);
            if(!tick) {
                for(unsigned axis=0;axis<3;++axis) {
                    assert(std::abs(query_eye[axis]-entrance_eye[axis])<.001f);
                    assert(std::abs(query_at[axis]-entrance_at[axis])<.001f);
                }
                assert(near(lens,40.f));
            }
            assert(h.radius>=previous-.001f && h.radius<=goal+.001f);
            assert(h.radius-previous<(goal-start_radius)*.21f);
            assert(h.radius-previous<(goal-start_radius)*.051f);
            if(tick==1)assert(h.radius-start_radius<(goal-start_radius)*.004f);
            assert(lens>=previous_fov && lens<=52.f && lens-previous_fov<2.5f);
            previous=h.radius;previous_fov=lens;
            h.Finish(m,r);assert(r[13]==sp && r[15]==Resume);
        }
        assert(near(h.radius,goal) && near(previous_fov,52.f) && !h.entry_release);
    }
    // Captured Fire Temple -> crater auto-walk: script state 3 used to
    // prevent remembering the 162-unit view, causing a one-frame jump to 249.
    {
        m.words=native;h={};entry_state={};stick_x=stick_y=0;distance_scale=1.25f;
        m.words[0x58795c]=0;m.words[play+0x104]=97;
        m.words[cam+0x188]=7|(3u<<16);m.words[cam+0x18c]=0;
        m.words[play+0x4c30]=196608;m.words[play+0x4c34]=65550;
        m.words[play+0x5c2c]=16772097;m.words[play+0x7f10]=196608;
        m.words[player+0x1710]=0x20000000;m.words[player+0x1714]=32;
        const Vec last_at{11.999425f,-284.99091f,-1487.0002f};
        const Vec last_eye{12.029966f,-269.63763f,-1325.6044f};
        Put(m,player+0x28,{11.999425f,-350,-1497});
        Put(m,cam+0x80,last_at);Put(m,cam+0x8c,last_eye);
        m.words[cam+0x144]=std::bit_cast<U32>(57.701801f);
        r=regs;h.Begin(m,r,100);h.Finish(m,r);
        assert(!h.active && h.native_entry.valid && !EntryView(m,cam));
        m.words[player+0x1710]=0;r=regs;h.Begin(m,r,101);
        Put(m,cam+0x8c,{0,-280,-1300});h.Finish(m,r);
        assert(h.entry_release && r[15]==Collision);
        Vec rendered;assert(OOT3D::TitleScreen::ReadFloats(m,r[13],rendered));
        for(unsigned i=0;i<3;++i)assert(std::abs(rendered[i]-last_eye[i])<.001f);
        assert(near(h.GameplayFov(m,play+0x188),57.701801f));
        h.Finish(m,r);
    }
    // Adult death/respawn capture: NORMAL setting 1 retains script 0x30001
    // during the fade. Preserve the final displayed view, then release smoothly.
    {
        m.words=native;h={};entry_state={};stick_x=stick_y=0;distance_scale=1.25f;
        m.words[0x58795c]=0;m.words[play+0x104]=97;
        m.words[cam+0x188]=7|(1u<<16);m.words[cam+0x18c]=0;
        m.words[play+0x4c30]=196609;m.words[play+0x4c34]=65551;
        m.words[play+0x5c2c]=16772097;m.words[play+0x7f10]=196608;
        m.words[player+0x1710]=0x20000000;
        const Vec last_at{-.03175756f,500.7229f,.013076782f};
        const Vec last_eye{.031762425f,531.40039f,165.6633f};
        Put(m,player+0x28,{0,441,0});
        Put(m,cam+0x80,last_at);Put(m,cam+0x8c,last_eye);
        m.words[cam+0x144]=std::bit_cast<U32>(55.018826f);
        r=regs;h.Begin(m,r,100);h.Finish(m,r);
        assert(!h.active && h.native_entry.valid && !EntryView(m,cam));
        m.words[player+0x1710]=0;
        float previous_radius=0,previous_fov=55.018826f;
        for(unsigned tick=0;tick<=Hook::ReleaseFrames;++tick) {
            r=regs;h.Begin(m,r,101+tick);
            Put(m,cam+0x8c,{0,530,245});h.Finish(m,r);
            assert(r[15]==Collision);
            if(!tick) {
                Vec rendered;assert(OOT3D::TitleScreen::ReadFloats(m,r[13],rendered));
                for(unsigned i=0;i<3;++i)assert(std::abs(rendered[i]-last_eye[i])<.001f);
                assert(near(h.GameplayFov(m,play+0x188),55.018826f));
            } else assert(h.radius>=previous_radius && h.radius-previous_radius<17);
            const float lens=h.GameplayFov(m,play+0x188);
            assert(lens<=previous_fov+.0001f && previous_fov-lens<1);
            previous_radius=h.radius;previous_fov=lens;h.Finish(m,r);
        }
        assert(near(h.radius,250) && near(previous_fov,52));
    }
    // Dodongo's Cavern capture: room visuallyHot=1 is not a cutscene.
    // The 55999 -> 56000 handoff must preserve eye and FOV, then recover.
    {
        m.words=native;h={};entry_state={};stick_x=stick_y=0;distance_scale=1.25f;
        m.words[0x58795c]=1;m.words[play+0x104]=1;
        m.words[cam+0x188]=7|(3u<<16);m.words[cam+0x18c]=0;
        m.words[play+0x4c30]=16777216;m.words[play+0x4c34]=16842767;
        m.words[play+0x5c2c]=16772096;m.words[play+0x7f10]=196608;
        m.words[player+0x1710]=0x20000000;
        const Vec last_eye{-2.9781978f,51.387688f,482.91284f},last_at{-3.0109563f,41.116486f,369.00775f};
        Put(m,cam+0x8c,last_eye);Put(m,cam+0x80,last_at);Put(m,player+0x28,{-3,0,369});
        m.words[cam+0x144]=std::bit_cast<U32>(56.886772f);
        r=regs;h.Begin(m,r,100);h.Finish(m,r);
        assert(!h.active && h.native_entry.valid && !EntryView(m,cam));
        m.words[player+0x1710]=0;r=regs;h.Begin(m,r,101);
        Put(m,cam+0x8c,{-3,57,555});h.Finish(m,r);
        assert(h.entry_release && r[15]==Collision);
        Vec rendered;assert(OOT3D::TitleScreen::ReadFloats(m,r[13],rendered));
        for(unsigned i=0;i<3;++i)assert(std::abs(rendered[i]-last_eye[i])<.001f);
        assert(near(h.GameplayFov(m,play+0x188),56.886772f));h.Finish(m,r);
    }
    // No cached entrance may cross a scene/player/camera change, a savestate,
    // a missed update, dialogue, a subcamera or a real scripted cutscene.
    m.words=native;m.words[play+0x4c30]=m.words[play+0x4c34]=0;
    m.words[play+0x5c2c]=0;m.words[play+0x7f10]=1u<<16;
    m.words[player+0x1710]=0x20000000;
    m.words[cam+0x144]=std::bit_cast<U32>(40.f);
    h={};h.RememberEntry(m,cam);assert(h.native_entry.valid);
    assert(!h.EntryMatches(m,cam,2));
    for(auto [address,value]:std::array<std::pair<U32,U32>,7>{{
        {play+0x22a0,1},{play+0x22a0,2},{cam+0x18c,3},
        {play+0xa64,1},{0x588e3c,1},{cam+0xd8,player+0x2000},{play+0x104,0x56}}}) {
        const auto old=m.words[address];m.words[address]=value;
        r=regs;h.Begin(m,r);assert(!h.native_entry.valid);
        m.words[address]=old;h.RememberEntry(m,cam);assert(h.native_entry.valid);
    }
    reset_epoch.fetch_add(1);assert(!h.EntryMatches(m,cam,0));
    enabled=false;h.RememberEntry(m,cam);assert(!h.native_entry.valid);enabled=true;
    // Cutscene fields take precedence even if gameplay flags clear on the
    // same update. Conversely, a native handler ending its entrance midway
    // through dispatch must leave a current snapshot for the next update.
    h.RememberEntry(m,cam);assert(h.native_entry.valid);
    m.words[player+0x1710]=0;m.words[play+0x22a0]=1;
    r=regs;h.Begin(m,r);assert(!h.native_entry.valid);
    m.words[play+0x22a0]=0;m.words[player+0x1710]=0x20000000;
    m.words[cam+0x188]=7|(0x23u<<16);entry_state={};h={};
    r=regs;h.Begin(m,r,10);h.Finish(m,r);assert(h.native_entry.valid);
    r=regs;h.Begin(m,r,11);assert(r[13]==sp);
    m.words[cam+0x188]=7|(1u<<16);m.words[player+0x1710]=0;m.words[play+0x7f10]=0;
    h.Finish(m,r);assert(h.EntryMatches(m,cam,12));
    r=regs;h.Begin(m,r,12);h.Finish(m,r);assert(h.entry_release && r[15]==Collision);
    // Collision remains authoritative during recovery, including its first frame.
    Put(m,r[13],clipped);h.Finish(m,r);
    assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==clipped);
    entry_state={};
    std::cout<<"Orbit native-mode dispatch/cleanup, targeting handoff, constant rotation distance, native fallback, collision and both saved-call stages passed\n";
}
