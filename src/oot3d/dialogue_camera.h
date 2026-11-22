// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/camera.h"
#include "oot3d/orbit_camera.h"
#include "oot3d/interpolation_math.h"

namespace OOT3D::DialogueCamera {
using TitleScreen::U32;
using OrbitCamera::Vec;
namespace Math=Interpolation::Math;
constexpr float DefaultEaseIn=.2f, DefaultEaseOut=.3f, MaximumTime=2.f;
inline std::atomic<float> ease_in_time{DefaultEaseIn}, ease_out_time{DefaultEaseOut};
inline std::atomic<bool> mm_style{true};
inline std::atomic<bool> dirty{false};
inline float Sanitize(float value,float fallback) {
    return std::isfinite(value)?std::clamp(value,0.f,MaximumTime):fallback;
}
// Integrate a smooth velocity ramp up, followed by a smooth ramp down.
// Each phase has its own duration; their combined area is normalized to one.
// Zero skips that phase, while zero for both bypasses the transition entirely.
inline double Progress(double seconds,double ramp_in,double ramp_out) {
    const double total=ramp_in+ramp_out;
    if(total<=0 || seconds>=total)return 1;
    if(seconds<=0)return 0;
    if(seconds<ramp_in) {
        const double u=seconds/ramp_in;
        return ramp_in*(u*u*u-.5*u*u*u*u)/(.5*total);
    }
    const double u=(seconds-ramp_in)/ramp_out;
    return (.5*ramp_in+ramp_out*(u-u*u*u+.5*u*u*u*u))/(.5*total);
}
struct Pose {
    Vec eye{},at{},up{0,1,0};
    float fov=52;
};
inline Vec Cross(const Vec& a,const Vec& b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
inline float Length(const Vec& v) {return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
inline std::optional<Math::Matrix> Camera(const Pose& p) {
    for(const auto& v:{p.eye,p.at,p.up})for(float x:v)if(!std::isfinite(x))return {};
    if(!std::isfinite(p.fov) || p.fov<=0 || p.fov>=170)return {};
    Vec back;for(unsigned i=0;i<3;++i)back[i]=p.eye[i]-p.at[i];
    const float distance=Length(back);if(distance<.01f)return {};
    for(float& x:back)x/=distance;
    auto right=Cross(p.up,back);const float width=Length(right);if(width<.001f)return {};
    for(float& x:right)x/=width;
    const auto up=Cross(back,right);
    auto matrix=Math::Identity();
    for(unsigned i=0;i<3;++i)matrix[i]={right[i],up[i],back[i],p.eye[i]};
    return matrix;
}
inline Pose Blend(const Pose& a,const Pose& b,double weight) {
    const auto camera=Math::Blend(*Camera(a),*Camera(b),weight);
    Vec a_offset,b_offset;for(unsigned i=0;i<3;++i) {
        a_offset[i]=a.eye[i]-a.at[i];b_offset[i]=b.eye[i]-b.at[i];
    }
    const auto distance=Length(a_offset)+(Length(b_offset)-Length(a_offset))*weight;
    Pose result;
    for(unsigned i=0;i<3;++i) {
        result.eye[i]=float(camera[i][3]);result.up[i]=float(camera[i][1]);
        result.at[i]=float(camera[i][3]-camera[i][2]*distance);
    }
    result.fov=float(a.fov+(b.fov-a.fov)*weight);return result;
}
// Presentation follows the native destination with a finite, smooth-ended
// blend. Native camera timers and actor/message state never receive the eased
// values as feedback. Ease-in and ease-out are the acceleration and braking
// phases of entering dialogue. Closing dialogue uses native camera movement.
struct Transition {
    Pose output{},start{};
    U32 owner{},scene{};
    unsigned epoch{};
    std::uint64_t frame{};
    std::int64_t time{},began{};
    float ramp_in{},ramp_out{};
    bool seen{},talking{},blending{};
    void Reset() {*this={};}
    bool Observe(const Pose& native,U32 next_owner,U32 next_scene,unsigned next_epoch,
                 std::uint64_t next_frame,std::int64_t now,bool talk) {
        if(!Camera(native)) {Reset();return false;}
        const bool continuous=seen && owner==next_owner && scene==next_scene && epoch==next_epoch &&
            next_frame>=frame && next_frame<=frame+1 && now>=time && now-time<=100000;
        if(continuous && next_frame==frame)return blending;
        if(!continuous) {
            Reset();output=native;talking=talk;
        } else if(talk!=talking) {
            start=output;began=now;talking=talk;
            ramp_in=Sanitize(ease_in_time.load(),DefaultEaseIn);
            ramp_out=Sanitize(ease_out_time.load(),DefaultEaseOut);
            blending=talk && ramp_in+ramp_out>0;
        }
        owner=next_owner;scene=next_scene;epoch=next_epoch;frame=next_frame;time=now;seen=true;
        // Setting both phases to zero restores native behavior immediately.
        if(Sanitize(ease_in_time.load(),DefaultEaseIn)+Sanitize(ease_out_time.load(),DefaultEaseOut)==0)
            blending=false;
        if(blending) {
            const double t=Progress(double(now-began)/1000000.,ramp_in,ramp_out);
            output=Blend(start,native,t);
            if(t>=1) {output=native;blending=false;}
        } else output=native;
        return blending;
    }
};

constexpr U32 FirstSVC=0xfff1c0;
constexpr std::array<TitleScreen::Site,8> Sites{{
    {0x478e98,0xed948a04,0xeeb48a68}, // VLDR s16,[r4,#0x10], before projection.
    {0x479188,0xe1a01005,0xebf98335}, // MOV r1,r5, temporary look-at vectors.
    {0x237090,0xe1a06000,0xe280808c}, // MOV r6,r0, native TALK handler entry.
    {0x2372b4,0xedc60a0a,0xeb04ec6d}, // VSTR s1,[r6,#0x28], decoded TALK parameters.
    {0x2377f8,0xedd60a51,0xed960a08}, // VLDR s1,[r6,#0x144], native lens approach.
    {0x23747c,0xeddf0af8,0xed9f0af6}, // VLDR s1,[pc,#0x3e0], before destination/collision search.
    {0x2374f0,0xed8d0a15,0xeb04ebd3}, // VSTR s0,[sp,#0x54], destination radius before collision.
    {0x236350,0xed941a00,0xeef00a40}, // Fixed indoor TALK lens reduction, s2 <- camera+0.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000 | (FirstSVC + i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return OOT3D::Camera::CodeMatches(m) && OrbitCamera::CodeMatches(m);
}
template<class M> bool MainCamera(M& m,U32 view) {
    if(!OOT3D::Camera::GameplayView(m,view))return false;
    const U32 play=view-0x188,cam=play+0x364;
    const auto game=m.Read(0x588e3c);
    return (game==0 || game==3) && OrbitCamera::Half(m,play+0x104)!=0x45 &&
        m.Read(cam+0xd4)==play && TitleScreen::Pointer(m.Read(cam+0xd8).value_or(0),0x1718) &&
        m.Read(play+0xa64) && OrbitCamera::Half(m,play+0xa64)==0 &&
        m.Read(cam+0x188) && OrbitCamera::Half(m,cam+0x188)==7;
}
// The common OoT KeepOn3 profile. Preserve bespoke boss/scene profiles and
// their native actor framing. These tagged words also guard the table lookup.
constexpr U32 CommonParameters=0x517a60;
constexpr std::array<U32,13> CommonWords{
    0x0000ffe2,0x00010032,0x00020064,0x000d003c,0x000e001e,0x000f0000,
    0x00100005,0x00110046,0x0007002d,0x00080032,0x0014fed4,0x0004000a,0x00093500};
// Fishing Hole's sign/desk capture uses main-camera TALK with settings 4 and
// 59. Both dispatch the same KeepOn3 handler, but use different native tables.
// Admit these verified tables only in scene 73; scripted fishing subcameras
// and other scenes using the indoor profile retain their existing behavior.
constexpr std::array<U32,13> FishingIndoorWords{
    0x0000ffec,0x00010046,0x000200c8,0x000d0028,0x000e000a,0x000f0000,
    0x00100005,0x00110046,0x0007002d,0x00080032,0x0014fed4,0x0004000a,0x00093500};
constexpr std::array<U32,13> FishingPondWords{
    0x0000ffe2,0x00010046,0x000200c8,0x000d0028,0x000e000a,0x000f0000,
    0x00100005,0x00110046,0x0007002d,0x00080032,0x0014ff38,0x0004000a,0x00093f20};
template<class M> bool CommonContext(M& m,U32 cam) {
    const U32 play=m.Read(cam+0xd4).value_or(0);
    if(!MainCamera(m,play+0x188) || cam!=play+0x364)
        return false;
    const U32 setting=OrbitCamera::Half(m,cam+0x18a);
    if(setting>0x46)return false;
    const U32 modes=m.Read(0x516ff0+setting*8+4).value_or(0);
    if(!TitleScreen::Pointer(modes,32))return false;
    const auto parameters=m.Read(modes+3*8+4).value_or(0);
    const auto* words=&CommonWords;
    if(parameters!=CommonParameters) {
        if(OrbitCamera::Half(m,play+0x104)!=73 || m.Read(modes+3*8)!=0x000c000e)return false;
        if(setting==4 && parameters==0x518114)words=&FishingIndoorWords;
        else if(setting==59 && parameters==0x518ea8)words=&FishingPondWords;
        else return false;
    }
    for(unsigned i=0;i<CommonWords.size();++i)
        if(m.Read(parameters+4*i)!=(*words)[i])return false;
    return true;
}
template<class M> bool NativeContext(M& m,U32 cam) {
    return OrbitCamera::Half(m,cam+0x18c)==3 && CommonContext(m,cam);
}
// Preserve MM's optical zoom ratio, expressed against OoT's 52-degree lens.
// Camera::Handle subsequently applies the user's usual relative FOV widening.
inline float NativeDialogueFov() {
    constexpr float radians=3.14159265358979323846f/360.f;
    return std::atan(std::tan(OOT3D::Camera::NativeFov*radians)*
                     std::tan(35.f*radians)/std::tan(45.f*radians))/radians;
}
inline int SignedAngle(int value) {
    const auto wrapped=static_cast<unsigned>(value)&0xffffu;
    return wrapped<0x8000u?int(wrapped):int(wrapped)-0x10000;
}
// MM KeepOn3's ordinary-actor side choice (0x4415f0..0x441620).
// In the far hemisphere MM compares against the *flipped* target axis;
// OoT instead reuses the unflipped difference with a different sign rule.
inline int DialogueYaw(int target,int current,int offset,bool force_opposite=false) {
    const int difference=SignedAngle(target-current);
    const bool near=!force_opposite && difference>=-0x3fff && difference<=0x3fff;
    const int base=SignedAngle(target-(near?0:0x8000));
    const bool negate=near?difference>0:SignedAngle(base-current)<0;
    return SignedAngle(base+(negate?-offset:offset));
}
struct Hook {
    template<class M,class R,class F> void FixedIndoorZoom(M& m,R& r,F& fp) {
        const U32 cam=r[4];
        fp[2]=m.Read(cam).value_or(0); // Displaced VLDR.
        const U32 play=m.Read(cam+0xd4).value_or(0);
        // Setting 26 dispatches the fixed indoor TALK handler. Its 30% lens
        // reduction turns a 60-degree room camera into 42 degrees; leave its
        // scene camera, aim, timer and message handling intact.
        if(!MainCamera(m,play+0x188) || cam!=play+0x364 ||
           OrbitCamera::Half(m,cam+0x18a)!=26 || OrbitCamera::Half(m,cam+0x18c)!=3 ||
           m.Read(0x516ff0+26*8+4)!=0x519fe4 ||
           m.Read(0x519fe4+3*8)!=0x0004000b || m.Read(0x519fe4+3*8+4)!=0x5188c4)return;
        constexpr std::array<U32,4> words{0x0018001e,0x00190000,0x00040004,0x00093500};
        for(unsigned i=0;i<words.size();++i)if(m.Read(0x5188c4+4*i)!=words[i])return;
        fp[2]=std::bit_cast<U32>(0.f);
    }
    Transition transition;
    Pose displayed{};
    U32 view{};
    unsigned epoch{};
    std::uint64_t frame{};
    bool apply{};
    U32 native_camera{},native_scene{};
    unsigned native_epoch{};
    bool native_latched{},native_active{},native_seeded{};
    Pose gameplay_pose{};
    U32 gameplay_owner{},gameplay_player{},gameplay_scene{};
    unsigned gameplay_epoch{};
    std::uint64_t gameplay_frame{};
    bool gameplay_seen{};
    void ResetNative() {
        native_camera=0;native_latched=native_active=native_seeded=false;
    }
    template<class M,class R> void NativeEntry(M& m,R& r,std::uint64_t next_frame) {
        r[6]=r[0]; // Displaced MOV; native prologue and mode dispatch still run.
        const U32 cam=r[0];
        if(!NativeContext(m,cam)) {ResetNative();return;}
        const U32 play=m.Read(cam+0xd4).value_or(0),scene=OrbitCamera::Half(m,play+0x104);
        const auto current_epoch=OrbitCamera::reset_epoch.load();
        if(native_camera!=cam || native_scene!=scene || native_epoch!=current_epoch)ResetNative();
        const U32 phase=OrbitCamera::Half(m,cam+0x1a6);
        const bool initializing=phase==0 || phase==10 || phase==20;
        if(!native_latched) {
            native_camera=cam;native_scene=scene;native_epoch=current_epoch;native_latched=true;
            // A host recreated midway through a saved native call can recognize
            // the profile already stored in guest memory; don't restart its timer.
            const bool restored=OrbitCamera::Half(m,cam+0x2c)==30 &&
                m.Read(cam+4)==std::bit_cast<U32>(80.f) &&
                m.Read(cam+8)==std::bit_cast<U32>(150.f) &&
                m.Read(cam+0x20)==std::bit_cast<U32>(NativeDialogueFov());
            native_active=initializing?mm_style.load():restored;
        }
        if(!native_active || !initializing || native_seeded)return;
        native_seeded=true;
        // Start from the view the player actually saw, including the orbit's
        // distance and lens policy. The native lens can otherwise still hold
        // an undisplayed targeting zoom when TALK starts.
        if(gameplay_seen && gameplay_owner==play && gameplay_scene==scene &&
           gameplay_player==m.Read(cam+0xd8).value_or(0) && gameplay_epoch==current_epoch &&
           next_frame>=gameplay_frame && next_frame-gameplay_frame<=1) {
            OrbitCamera::Put(m,cam+0x80,gameplay_pose.at);
            OrbitCamera::Put(m,cam+0x8c,gameplay_pose.eye);
            OrbitCamera::Put(m,cam+0xa4,gameplay_pose.eye);
            m.Write(cam+0x144,std::bit_cast<U32>(gameplay_pose.fov));
        }
    }
    template<class M,class R,class F> void NativeParameters(M& m,R& r,F& fp) {
        const U32 cam=r[6];
        m.Write(cam+0x28,fp[1]); // Displaced VSTR, including the legacy path.
        if(!native_active || native_camera!=cam || native_epoch!=OrbitCamera::reset_epoch.load() ||
           !NativeContext(m,cam))return;
        const float old_offset=std::bit_cast<float>(m.Read(cam).value_or(0));
        const float height=std::bit_cast<float>(fp[19])-old_offset;
        if(!std::isfinite(height) || height<1 || height>1000)return;
        // MM's shared native profile, not the captured Deku-specific output.
        // Keep OoT's live actor height, target focus and collision search.
        // NativeSide supplies MM's ordinary-actor angular side choice.
        const float offset=-.2f*height*(.8f+13.6f/height);
        const std::array<float,11> values{
            offset,80.f,150.f,50.f,5.f,20.f,5.f,.4f,NativeDialogueFov(),.5f,0.f};
        for(unsigned i=0;i<values.size();++i)m.Write(cam+4*i,std::bit_cast<U32>(values[i]));
        fp[19]=std::bit_cast<U32>(height+offset);
        // The native handler already blends its aim linearly and its angles
        // using sum(1..N). MM uses N=30: w(n)=n(61-n)/930, verified by capture.
        // Preserve native interface flags in the other half of this word.
        m.Write(cam+0x2c,(m.Read(cam+0x2c).value_or(0)&0xffff0000u)|30u);
    }
    template<class M,class R,class F> void NativeLens(M& m,R& r,F& fp) {
        fp[1]=m.Read(r[6]+0x144).value_or(0); // Displaced VLDR.
        if(native_active && native_camera==r[6] && native_epoch==OrbitCamera::reset_epoch.load()) {
            // Match the native pan's integrated ease-out: with R updates
            // remaining, this update consumes R / sum(1..R) of the residual.
            // The old .5 rate finished the lens zoom in ~0.2 s while the pan
            // took a second, which looked like a cut into a close-up.
            const auto remaining=OrbitCamera::Half(m,r[6]+0x4c);
            if(remaining>=1 && remaining<=30) {
                fp[2]=std::bit_cast<U32>(2.f/float(remaining+1));
                fp[3]=std::bit_cast<U32>(0.f);
            }
        }
    }
    template<class M,class R,class F> void NativeSide(M& m,R& r,F& fp) {
        fp[1]=m.Read(0x237864).value_or(0); // Displaced VLDR: native aim coefficient .4.
        if(!native_active || native_camera!=r[6] || native_epoch!=OrbitCamera::reset_epoch.load())return;
        const float distance=std::bit_cast<float>(m.Read(r[13]+0x5c).value_or(0));
        if(!std::isfinite(distance) || distance<0)return;
        const float ratio=std::min(distance/150.f,1.f);
        const int offset=int(.5f+(50.f+(5.f-50.f)*ratio)*182.0416717529297f);
        const int target=SignedAngle(OrbitCamera::Half(m,r[13]+0x62));
        const int current=SignedAngle(OrbitCamera::Half(m,r[13]+0x4a));
        r[0]=static_cast<U32>(DialogueYaw(target,current,offset));
    }
    template<class M,class R,class F> void NativeRadius(M& m,R& r,F& fp) {
        // MM KeepOn3 multiplies the whole destination radius by 1.5
        // (constant 0x4415e8), after base + actor separation * .5. Matching
        // its decoded table alone misses this step and frames actors too close.
        if(native_active && native_camera==r[6] && native_epoch==OrbitCamera::reset_epoch.load() &&
           NativeContext(m,r[6])) {
            const float radius=std::bit_cast<float>(fp[0]);
            if(std::isfinite(radius) && radius>0)fp[0]=std::bit_cast<U32>(radius*1.5f);
        }
        m.Write(r[13]+0x54,fp[0]); // Displaced VSTR, including the legacy path.
    }
    // Same native collision query as orbit. All call state lives on the guest
    // stack, including volatile registers, so a restored call can finish safely.
    static constexpr U32 ScratchSize=0xb0,Cookie=0x4449414c;
    static constexpr std::array<unsigned,6> SavedRegisters{0,1,2,3,12,14};
    template<class M,class R,class F> void Projection(M& m,R& r,F& fp,
                                  std::uint64_t next_frame,std::int64_t now,
                                  OrbitCamera::Hook* orbit=nullptr) {
        const U32 sp=r[13];
        if(TitleScreen::Pointer(sp,ScratchSize) && m.Read(sp+0x38)==Cookie &&
           m.Read(sp+0x3c)==(sp^Cookie) && m.Read(sp+0x34)==r[4]) {
            Pose clipped;
            const bool valid=TitleScreen::ReadFloats(m,sp,clipped.eye) &&
                TitleScreen::ReadFloats(m,sp+0x28,clipped.at) &&
                TitleScreen::ReadFloats(m,sp+0xa0,clipped.up);
            clipped.fov=std::bit_cast<float>(m.Read(sp+0x58).value_or(0));
            const auto saved_epoch=m.Read(sp+0x5c);
            for(unsigned i=0;i<SavedRegisters.size();++i)r[SavedRegisters[i]]=m.Read(sp+0x40+4*i).value_or(0);
            for(unsigned i=0;i<16;++i)fp[i]=m.Read(sp+0x60+4*i).value_or(0);
            m.Write(sp+0x38,0);r[13]+=ScratchSize;r[15]=Sites[0].pc+4;
            apply=valid && Camera(clipped).has_value() && saved_epoch==OrbitCamera::reset_epoch.load();
            view=r[4];frame=next_frame;epoch=OrbitCamera::reset_epoch.load();displayed=clipped;
            if(apply)transition.output=clipped;
            fp[16]=apply?std::bit_cast<U32>(clipped.fov):m.Read(r[4]+0x10).value_or(0);
            return;
        }
        fp[16]=m.Read(r[4]+0x10).value_or(0); // Displaced VLDR.
        if(!OOT3D::Camera::GameplayView(m,r[4]))return;
        if(!MainCamera(m,r[4])) {transition.Reset();ResetNative();gameplay_seen=false;apply=false;return;}
        const U32 play=r[4]-0x188;
        Pose native;
        native.fov=std::bit_cast<float>(fp[16]);
        if(!TitleScreen::ReadFloats(m,r[4]+0x30,native.eye) ||
           !TitleScreen::ReadFloats(m,r[4]+0x3c,native.at) ||
           !TitleScreen::ReadFloats(m,r[4]+0x48,native.up)) {transition.Reset();apply=false;return;}
        const auto current_epoch=OrbitCamera::reset_epoch.load();
        // Camera mode 3 is native TALK (KeepOn3), selected for NPCs and signs
        // by Player_Update. Other modes do not start a dialogue transition.
        const auto mode=OrbitCamera::Half(m,play+0x364+0x18c);
        const auto player=m.Read(play+0x364+0xd8).value_or(0);
        // An NPC can request TALK one update before Camera.mode changes.
        // Native NORMAL already moves the eye on that update after orbit
        // yields. Keep the last displayed gameplay view through that request,
        // and let NativeEntry seed the actual TALK path from it next update.
        if(mode==0 && mm_style.load() && gameplay_seen && gameplay_owner==play &&
           gameplay_player==player && gameplay_scene==OrbitCamera::Half(m,play+0x104) &&
           gameplay_epoch==current_epoch && next_frame>=gameplay_frame &&
           next_frame-gameplay_frame<=1 && (m.Read(player+4).value_or(0)&0x100) &&
           m.Read(player+0x16f8).value_or(0)!=0 && CommonContext(m,play+0x364)) {
            displayed=gameplay_pose;view=r[4];frame=next_frame;epoch=current_epoch;
            apply=true;gameplay_frame=next_frame;
            fp[16]=std::bit_cast<U32>(displayed.fov);
            return;
        }
        if(mode!=3)ResetNative();
        gameplay_seen=mode!=3 && OrbitCamera::GameplayLens(m,r[4]);
        if(gameplay_seen) {
            gameplay_pose=native;
            gameplay_pose.fov=orbit?orbit->GameplayFov(m,r[4]):OOT3D::Camera::NativeFov;
            gameplay_owner=play;gameplay_scene=OrbitCamera::Half(m,play+0x104);
            gameplay_player=m.Read(play+0x364+0xd8).value_or(0);
            gameplay_epoch=current_epoch;gameplay_frame=next_frame;
        }
        if(native_active && native_camera==play+0x364 && native_epoch==current_epoch) {
            // The native handler now supplies the complete angular transition.
            // Layering the old transform blend over it distorts that path.
            if(orbit && Camera(native))
                orbit->RememberDialogue(m,play+0x364,native.eye,native.at,native.fov,next_frame);
            transition.Reset();apply=false;return;
        }
        const bool duplicate=apply && view==r[4] && frame==next_frame && epoch==current_epoch;
        if(!transition.Observe(native,play,OrbitCamera::Half(m,play+0x104),current_epoch,next_frame,now,mode==3)) {
            apply=false;return;
        }
        if(duplicate) {fp[16]=std::bit_cast<U32>(displayed.fov);return;}
        apply=false;
        if(sp<ScratchSize || !TitleScreen::Pointer(sp-ScratchSize,ScratchSize))return;
        const U32 scratch=sp-ScratchSize;
        for(U32 i=0;i<ScratchSize;i+=4)if(!m.Read(scratch+i))return;
        for(U32 i=0;i<ScratchSize;i+=4)m.Write(scratch+i,0);
        const auto& pose=transition.output;
        OrbitCamera::Put(m,scratch,pose.eye);OrbitCamera::Put(m,scratch+0x28,pose.at);
        OrbitCamera::Put(m,scratch+0xa0,pose.up);
        m.Write(scratch+0x34,r[4]);m.Write(scratch+0x38,Cookie);m.Write(scratch+0x3c,scratch^Cookie);
        for(unsigned i=0;i<SavedRegisters.size();++i)m.Write(scratch+0x40+4*i,r[SavedRegisters[i]]);
        m.Write(scratch+0x58,std::bit_cast<U32>(pose.fov));m.Write(scratch+0x5c,current_epoch);
        for(unsigned i=0;i<16;++i)m.Write(scratch+0x60+4*i,fp[i]);
        r[13]=scratch;r[0]=play+0x364;r[1]=scratch+0x28;r[2]=scratch;
        r[14]=Sites[0].pc;r[15]=OrbitCamera::Collision;
    }
    template<class M,class R> void LookAt(M& m,R& r,std::uint64_t next_frame) {
        r[1]=r[5]; // Displaced MOV; replace only local draw vectors.
        if(!apply || view!=r[4] || frame!=next_frame || epoch!=OrbitCamera::reset_epoch.load())return;
        OrbitCamera::Put(m,r[5],displayed.eye);OrbitCamera::Put(m,r[6],displayed.up);
        OrbitCamera::Put(m,r[7],displayed.at);
    }
};
} // namespace OOT3D::DialogueCamera
