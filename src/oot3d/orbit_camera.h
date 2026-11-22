// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include "oot3d/camera.h"
#include "oot3d/save_menu.h"
#include "oot3d/camera_clearance.h"
#include "oot3d/camera_collision_recovery.h"
#include "oot3d/widescreen.h"
#include <atomic>

// Camera field offsets and special-setting IDs cross-checked against
// Roberto-Nessy/OoT3D_Standalone_Free_Cam (47d2cf74185438ae0360ec13ab1f7c00f26388ce).
// This host hook retains the original update tail and calls native collision.
namespace OOT3D::OrbitCamera {
using TitleScreen::U32;
using TitleScreen::Pointer;
inline std::atomic<bool> enabled{true}, invert_x{false}, invert_y{false}, dirty{false};
constexpr float DefaultAccelerationTime = .20f, DefaultDecelerationTime = .20f;
constexpr float DefaultDistance = 1.f, MinimumDistance = .5f, MaximumDistance = 2.f;
inline std::atomic<float> sensitivity{1.f}, stick_x{0}, stick_y{0};
inline std::atomic<float> distance_scale{DefaultDistance};
inline std::atomic<bool> smooth_collision_return{true};
inline std::atomic<float> acceleration_time{DefaultAccelerationTime};
inline std::atomic<float> deceleration_time{DefaultDecelerationTime};
inline std::atomic<unsigned> reset_epoch{0};
inline std::atomic<bool> input_allowed{true};
inline float Sanitize(float s) { return std::isfinite(s) ? std::clamp(s,.25f,3.f) : 1.f; }
inline float SanitizeDistance(float scale) {
    return std::isfinite(scale) ? std::clamp(scale,MinimumDistance,MaximumDistance) : DefaultDistance;
}
inline float SanitizeTime(float seconds, float fallback) {
    return std::isfinite(seconds) ? std::clamp(seconds, 0.f, 1.f) : fallback;
}
// MM capture: remaining velocity error halves per native update, including
// reversals. Times represent six half-lives (~98% settled), not a linear ramp.
// Keep user-selected times; default .20 seconds gives alpha=.5 at 30 Hz.
inline float ConfiguredDistance() {
    return UserCamera::Apply(UserCamera::Distance,SanitizeDistance(distance_scale.load()),MinimumDistance,MaximumDistance);
}
inline float ConfiguredSensitivity() {
    return UserCamera::Apply(UserCamera::Sensitivity,Sanitize(sensitivity.load()),.25f,3.f);
}
inline float ConfiguredAcceleration() {
    return UserCamera::Apply(UserCamera::Acceleration,SanitizeTime(acceleration_time.load(),DefaultAccelerationTime),0,1);
}
inline float ConfiguredDeceleration() {
    return UserCamera::Apply(UserCamera::Deceleration,SanitizeTime(deceleration_time.load(),DefaultDecelerationTime),0,1);
}
inline bool ConfiguredInvertX() {return UserCamera::Apply(UserCamera::InvertX,invert_x.load());}
inline bool ConfiguredInvertY() {return UserCamera::Apply(UserCamera::InvertY,invert_y.load());}
inline bool ConfiguredSmoothPullback() {return UserCamera::Apply(UserCamera::SmoothPullback,smooth_collision_return.load());}
inline float AdvanceVelocity(float current, float target, float acceleration,
                             float deceleration, float dt = 1.f / 30.f) {
    const float time = current*target<0 || std::abs(target)>std::abs(current) ? acceleration : deceleration;
    if (time == 0) return target;
    const float next=target+(current-target)*std::exp2(-6.f*dt/time);
    return std::abs(next-target)<.001f ? target : next;
}
constexpr float PitchRadians = 3.14159265358979323846f / 180.f;
constexpr float MinimumPitch = -35.f * PitchRadians, MaximumPitch = 30.f * PitchRadians;
// Brake toward either end over the last eight degrees. Apply the cap to the
// actual velocity, so holding the stick at an endpoint cannot store momentum.
// Motion back into the range keeps the usual stick response immediately.
inline float AdvancePitch(float pitch,float& velocity,float step) {
    if(velocity==0)return pitch;
    const float limit=velocity<0?std::min(pitch,MinimumPitch):std::max(pitch,MaximumPitch);
    const float remaining=std::abs(limit-pitch);
    if(remaining<=.02f*PitchRadians) {velocity=0;return limit;}
    velocity=std::copysign(std::min(std::abs(velocity),remaining/(8.f*PitchRadians)),velocity);
    const float next=pitch+velocity*step;
    const float result=std::clamp(next,std::min(pitch,MinimumPitch),std::max(pitch,MaximumPitch));
    if(result!=next)velocity=0;
    return result;
}
// Remove the deadzone without a speed jump at its boundary; retain stick direction.
inline std::array<float,2> Stick(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return {};
    x = std::clamp(x,-1.f,1.f); y = std::clamp(y,-1.f,1.f);
    const float length = std::hypot(x,y);
    if (length <= .2f) return {};
    const float scale = (std::min(length,1.f)-.2f) / (.8f*length);
    return {x*scale,y*scale};
}
constexpr U32 FirstSVC = 0xfff060, Collision = 0x3553fc, Resume = 0x2d891c;
// Wrap the mode dispatch, retaining its state/interface updates before applying
// the orbit. Native floor/water and quake/view/input-direction tails stay intact.
constexpr std::array<TitleScreen::Site,2> Sites{{
    {0x2d88e4,0xe12fff31,0xea00000b}, // BLX r1
    {0x2d88e8,0xea00000b,0xe59400d8}, // B native continuation
}};
// Player_Update already has a focus actor and the enemy-lock bit on this
// branch. Keep its battle camera (mode 2) when the boomerang leaves the hand.
constexpr U32 LockedBoomerangMode=0x252ebc;
inline U32 Instruction(U32 pc,U32 word) {
    if(pc==LockedBoomerangMode && word==0x13a08014)return 0x13a08002; // MOVNE r8,#2
    for (unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction) return 0xef000000|FirstSVC+i;
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following}) return false;
    return m.Read(LockedBoomerangMode-8)==0xe3100402 && // TST boomerang-in-flight flag.
           m.Read(LockedBoomerangMode-4)==0x03a08002 &&
           m.Read(LockedBoomerangMode)==0x13a08014 &&
           m.Read(Collision)==std::optional<U32>{0xe92d4070} &&
           m.Read(Collision+4)==std::optional<U32>{0xe24dd040} &&
           m.Read(0x2d84c4)==std::optional<U32>{0xe92d4ff0} &&
           m.Read(0x354dac)==std::optional<U32>{0xe3e00000} && // Quick chest: -1.
           m.Read(0x354db0)==std::optional<U32>{0xe18100b7} && // chest+0x38c.
           m.Read(0x4bc22c)==std::optional<U32>{0xe92d47f0}; // Item-opening action.
}
template<class M> U32 Half(M& m,U32 a) {
    return (m.Read(a&~3u).value_or(0) >> ((a&2)*8))&0xffff;
}
// Player_Update sets the Z-press bit at 250dd4..250de4, before selecting
// actor targeting (0x10000) or parallel targeting (0x20000). All three
// belong to targeting, not to the states that surrender camera ownership.
constexpr U32 TargetingFlags=0x38000;
constexpr U32 MountedFlag=0x800000; // Native Epona riding state; not camera ownership.
template<class M> bool OrdinaryChestOpening(M& m,U32 cam) {
    const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
    if(!Pointer(play,0x22a4) || !Pointer(player,0x2488))return false;
    const auto cutscene=m.Read(play+0x22a0);
    if(!cutscene || (*cutscene&255))return false;
    const auto flags=m.Read(player+0x1710).value_or(0);
    if((flags&0x20000c00)!=0x20000c00)return false;
    // 354b98 queues 4bc91c through 4a1ef0; the callback then installs 4bc22c.
    // Retain orbit through that one-update handoff, not through other item gifts.
    const U32 action=m.Read(player+0x1708).value_or(0);
    if(action!=0x4bc22c && !(action==0x4a1ef0 && m.Read(player+0x2484)==0x4bc91c))return false;
    const U32 chest=m.Read(player+0x12b0).value_or(0);
    // Native 354d98 selects the quick opening and writes -1 at chest+38c.
    // The dramatic branch writes +1 and starts camera sequence 0x28. Test the
    // chosen animation, so randomized contents/appearance remain independent.
    return Pointer(chest,0x398) && Half(m,chest)==0x0a && Half(m,chest+0x38c)==0xffff;
}
inline bool TargetingMode(U32 mode) {
    // Player_Update: parallel Z-target (1), battle (2), friendly target (4).
    // TALK (3), first person and scripted cameras retain their own framing.
    return mode==1 || mode==2 || mode==4;
}
template<class M> bool TargetingState(M& m,U32 cam) {
    if(TargetingMode(Half(m,cam+0x18c)))return true;
    // Player_Update selects targeting from these flags (252ea8, 252f3c).
    // Flags and Camera.mode need not switch on the same update. Keep distance
    // and lens coverage during that handoff; native mode dispatch still runs.
    const U32 player=m.Read(cam+0xd8).value_or(0);
    return Pointer(player,0x1718) && Half(m,cam+0x18c)==0 &&
        (m.Read(player+0x1710).value_or(0)&TargetingFlags)!=0;
}
template<class M> bool GameplayAllowed(M& m,U32 cam) {
    if(!enabled.load() || !input_allowed.load() || !Pointer(cam,0x1bc)) return false;
    const U32 play=m.Read(cam+0xd4).value_or(0), player=m.Read(cam+0xd8).value_or(0);
    if(!Pointer(play,0x5f14) || !Pointer(player,0x1718) || cam!=play+0x364 ||
       m.Read(0x5043e0)!=std::optional<U32>{play} || Half(m,play+0xa64)!=0 ||
       Half(m,play+0x104)==0x45 || Half(m,cam+0x188)!=7)
        return false;
    // Camera.mode selects an action's camera parameters, not camera ownership.
    // Player_Update (0x252e..) changes it during ordinary movement and attacks.
    // Requiring NORMAL here surrendered an active orbit to those native zooms.
    // Keep ownership based on the main camera, player state and scene setting;
    // targeting/first-person/scripted states still yield through these checks.
    // TALK owns its actor framing even on the boundary before player flags
    // reflect the conversation. The dialogue presentation hook eases its view.
    if(Half(m,cam+0x18c)==3)return false;
    // Player_Update requests TALK when an interaction actor is present and
    // Actor.flags has 0x100 (0x252e88..0x252ea4). Yield during that request too:
    // native camera mode changes can wait for their entry handshake, so mode
    // alone is too late to protect the start of sign/conversation framing.
    if((m.Read(player+4).value_or(0)&0x100) &&
       m.Read(player+0x16f8).value_or(0)!=0)return false;
    const auto flags1=m.Read(player+0x1710), flags2=m.Read(player+0x1714);
    // Parallel/battle/friendly targeting shares the gameplay distance
    // policy, but retains native aim instead of accepting stick rotation.
    // Native ranged-weapon setup (33603c) sets 0x200 when drawing/charging.
    // It is not a camera takeover while locked on: the capture keeps mode 2
    // and toggles 18008 <-> 18208 on each pull. Retain distance AND lens through
    // that flag, but only in targeting modes/the normal-mode flag handoff.
    // Free aiming still yields here; the separate first-person flag 0x100000
    // and every other ownership exclusion remain authoritative.
    const U32 targeting_draw=TargetingState(m,cam)?0x200u:0u;
    const U32 rejected=0x20938230&~(0x20000000u|TargetingFlags|MountedFlag|targeting_draw);
    if(!flags1 || !flags2 || (*flags1&rejected) || (*flags2&0x40000)) return false;
    const U32 game=m.Read(0x588e3c).value_or(~0u); // SaveContext.gameMode
    if(game!=0 && game!=3) return false;
    for(U32 setting:{0x14u,0x15u,0x19u,0x1au,0x1bu,0x1du,0x23u,0x40u,0x46u})
        if(Half(m,cam+0x18a)==setting) return false;
    return true;
}
template<class M> bool Targeting(M& m,U32 cam) {
    return GameplayAllowed(m,cam) && TargetingState(m,cam);
}
template<class M> bool Allowed(M& m,U32 cam) {
    if(!GameplayAllowed(m,cam) || TargetingState(m,cam))return false;
    const U32 player=m.Read(cam+0xd8).value_or(0);
    const auto flags=m.Read(player+0x1710).value_or(0);
    return !(flags&TargetingFlags) && (!(flags&0x20000000u) || OrdinaryChestOpening(m,cam));
}
// Scene fades and automatic entrance walks temporarily disable orbit input.
// Keep the distance/lens policy through that handoff without accepting stick
// rotation or replacing genuine cutscene/dialogue cameras.
struct EntryState {
    U32 camera{},player{},scene{};
    unsigned epoch{};
    bool armed{};
};
inline EntryState entry_state;
template<class M> bool EntryContext(M& m,U32 cam) {
    if(!GameplayAllowed(m,cam) || TargetingMode(Half(m,cam+0x18c)) || OrdinaryChestOpening(m,cam))return false;
    const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
    if(!Pointer(play,0x7f14) || (m.Read(player+0x1710).value_or(~0u)&TargetingFlags))return false;
    // These are ROOM fields, not cutscene state: +4c32 environment type
    // and +4c37 visuallyHot. Leave those rooms' entrance framing native.
    const auto room=m.Read(play+0x4c30),room_flags=m.Read(play+0x4c34),cutscene=m.Read(play+0x22a0);
    return room && room_flags && cutscene && !(*cutscene&255) &&
        ((*room>>16)&255)!=3 && (*room_flags>>24)==0;
}
template<class M> void ObserveEntry(M& m,U32 cam) {
    if(!EntryContext(m,cam)) {entry_state={};return;}
    const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
    const U32 scene=Half(m,play+0x104);
    const auto epoch=reset_epoch.load();
    if(entry_state.camera!=cam || entry_state.player!=player || entry_state.scene!=scene || entry_state.epoch!=epoch)
        entry_state={cam,player,scene,epoch,false};
    const auto request=m.Read(play+0x5c2c),fade=m.Read(play+0x7f10);
    // 2e2e60 / HUDFade::GameplayReady: scene request at +5c2d and fade at +7f12.
    const bool transitioning=request && fade && ((*request&0xff00) || (*fade&0xff0000));
    const bool walking=(m.Read(player+0x1710).value_or(0)&0x20000000)!=0;
    if(transitioning)entry_state.armed=true;
    else if(!walking)entry_state.armed=false;
}
template<class M> bool EntryView(M& m,U32 cam) {
    if(!entry_state.armed || entry_state.camera!=cam || entry_state.epoch!=reset_epoch.load() ||
       !EntryContext(m,cam))return false;
    const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
    return entry_state.player==player && entry_state.scene==Half(m,play+0x104) &&
        (m.Read(player+0x1710).value_or(0)&0x20000000)!=0;
}
// Observe native entrance views even when their camera setting/player state
// excludes orbit control. This grants no camera ownership: the native handler
// keeps running until the ordinary gameplay checks allow the handoff.
template<class M> bool NativeEntranceContext(M& m,U32 cam) {
    if(!enabled.load() || !input_allowed.load() || !Pointer(cam,0x1bc))return false;
    const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
    if(!Pointer(play,0x7f14) || !Pointer(player,0x1718) || cam!=play+0x364 ||
       m.Read(0x5043e0)!=play || Half(m,play+0xa64)!=0 || Half(m,play+0x104)==0x45 ||
       Half(m,cam+0x188)==0 || Half(m,cam+0x18c)==3 || TargetingState(m,cam))return false;
    // CutsceneContext is at +2298; its state byte is +8. The old checks
    // used Room.environmentType / visuallyHot, rejecting ordinary entrances
    // in hot rooms (including Dodongo's Cavern) as if they were cutscenes.
    const auto game=m.Read(0x588e3c),cutscene=m.Read(play+0x22a0);
    if(!game || (*game!=0 && *game!=3) || !cutscene || (*cutscene&255)!=0 ||
       ((m.Read(player+4).value_or(0)&0x100) && m.Read(player+0x16f8).value_or(0)))return false;
    return true;
}
template<class M> bool NativeEntrance(M& m,U32 cam) {
    if(!NativeEntranceContext(m,cam) || OrdinaryChestOpening(m,cam))return false;
    const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
    const auto flags=m.Read(player+0x1710),request=m.Read(play+0x5c2c),fade=m.Read(play+0x7f10);
    return flags && request && fade &&
        ((*flags&0x20000000) || (*request&0xff00) || (*fade&0xff0000));
}
template<class M> bool GameplayLens(M& m,U32 view) {
    const U32 play=m.Read(0x5043e0).value_or(0);
    return Pointer(play,0x5f14) && view==play+0x188 &&
           (Allowed(m,play+0x364) || Targeting(m,play+0x364) || EntryView(m,play+0x364));
}
template<class M> float ConfiguredRadius(M& m) {
    return (m.Read(0x58795c).value_or(1)!=0?150.f:200.f)*ConfiguredDistance();
}
using Vec = std::array<float,3>;
template<class M> void Put(M& m,U32 a,const Vec& v) {
    for(unsigned i=0;i<3;++i) m.Write(a+4*i,std::bit_cast<U32>(v[i]));
}
struct Hook {
    bool active=false;
    CameraClearance::Recovery collision_recovery;
    static constexpr unsigned ReleaseFrames=30; // Bounded exponential recovery, independent of stick input.
    bool target_view=false;
    unsigned release_frame=ReleaseFrames+1;
    Vec release_at_offset{};
    float release_radius=0;
    // Previous TALK/entrance views. Never reuse them across a camera, player,
    // scene, savestate epoch or missed update.
    struct DialogueView {
        Vec eye{},at{};
        float fov=Camera::NativeFov;
        U32 camera{},player{},scene{};
        unsigned epoch{};
        std::uint64_t frame{};
        bool valid{};
        bool z_latched{};
    } dialogue, native_entry, door_start, save_view;
    U32 door_source_setting{};
    bool door_native_started{};
    std::uint64_t current_frame{};
    bool entry_release=false;
    bool dialogue_release=false;
    bool release_z_latched=false;
    static constexpr unsigned DialogueReleaseFrames=180;
    float dialogue_fov=Camera::NativeFov,release_fov=Camera::NativeFov;
    float dialogue_remaining=1,dialogue_rate=.01685f;
    U32 owner=0, player_owner=0, scene=0;
    unsigned epoch=0;
    float yaw=0,pitch=0,radius=0;
    float yaw_velocity=0,pitch_velocity=0;
    bool pitch_recovery=false;
    float pitch_recovery_rate=.04f;
    void ResetMotion() {
        collision_recovery.Reset();
        active=target_view=false; yaw_velocity=pitch_velocity=0;release_frame=ReleaseFrames+1;
        dialogue_release=false;release_z_latched=false;dialogue_fov=Camera::NativeFov;
        entry_release=false;
        pitch_recovery=false;pitch_recovery_rate=.04f;
    }
    template<class M> bool EntryMatches(M& m,U32 cam,std::uint64_t frame) const {
        const U32 play=m.Read(cam+0xd4).value_or(0);
        return native_entry.valid && native_entry.camera==cam &&
            native_entry.player==m.Read(cam+0xd8).value_or(0) &&
            native_entry.scene==Half(m,play+0x104) && native_entry.epoch==reset_epoch.load() &&
            frame>=native_entry.frame && frame-native_entry.frame<=1;
    }
    template<class M> void RememberEntry(M& m,U32 cam) {
        const bool handoff=EntryMatches(m,cam,current_frame) && NativeEntranceContext(m,cam) && Allowed(m,cam);
        native_entry.valid=false;
        if(!handoff && !NativeEntrance(m,cam))return;
        Vec eye,at;
        const float fov=std::bit_cast<float>(m.Read(cam+0x144).value_or(0));
        if(!std::isfinite(fov) || fov<=0 || fov>=170 ||
           !TitleScreen::ReadFloats(m,cam+0x8c,eye) || !TitleScreen::ReadFloats(m,cam+0x80,at))return;
        const U32 play=m.Read(cam+0xd4).value_or(0);
        native_entry={eye,at,fov,cam,m.Read(cam+0xd8).value_or(0),Half(m,play+0x104),
                      reset_epoch.load(),current_frame,true};
    }
    template<class M> void RememberDialogue(M& m,U32 cam,const Vec& eye,const Vec& at,
                                           float fov,std::uint64_t frame) {
        if(!std::isfinite(fov) || fov<=0 || fov>=170)return;
        for(const auto& v:{eye,at})for(float x:v)if(!std::isfinite(x))return;
        const U32 play=m.Read(cam+0xd4).value_or(0);
        dialogue={eye,at,fov,cam,m.Read(cam+0xd8).value_or(0),Half(m,play+0x104),
                  reset_epoch.load(),frame,true};
        dialogue.z_latched=(m.Read(dialogue.player+0x1710).value_or(0)&0x8000)!=0;
    }
    template<class M> bool DialogueMatches(M& m,U32 cam,std::uint64_t frame) const {
        const U32 play=m.Read(cam+0xd4).value_or(0);
        return dialogue.valid && dialogue.camera==cam &&
            dialogue.player==m.Read(cam+0xd8).value_or(0) &&
            dialogue.scene==Half(m,play+0x104) && dialogue.epoch==reset_epoch.load() &&
            frame>=dialogue.frame && frame-dialogue.frame<=1;
    }
    template<class M> bool SaveQuitLens(M& m,U32 view) const {
        const U32 play=m.Read(0x5043e0).value_or(0),cam=play+0x364;
        return save_view.valid && Camera::GameplayView(m,view) &&
            save_view.camera==cam && save_view.player==m.Read(cam+0xd8).value_or(0) &&
            save_view.scene==Half(m,play+0x104) && save_view.epoch==reset_epoch.load() &&
            SaveMenu::Quitting(m);
    }
    template<class M> void RememberSaveView(M& m,U32 view,float lens) {
        if(!Camera::GameplayView(m,view) || SaveQuitLens(m,view))return;
        save_view.valid=false;
        if(!SaveMenu::Active(m) || SaveMenu::Quitting(m) ||
           !std::isfinite(lens) || lens<=0 || lens>=170)return;
        const U32 play=m.Read(0x5043e0).value_or(0),cam=play+0x364;
        Vec eye,at;
        if(!TitleScreen::ReadFloats(m,cam+0x8c,eye) || !TitleScreen::ReadFloats(m,cam+0x80,at))return;
        save_view={eye,at,lens,cam,m.Read(cam+0xd8).value_or(0),Half(m,play+0x104),
                   reset_epoch.load(),current_frame,true};
    }
    template<class M> float GameplayFov(M& m,U32 view) const {
        const U32 play=m.Read(0x5043e0).value_or(0);
        if(SaveQuitLens(m,view))return save_view.fov;
        // The door holds this gameplay view while opening. Changing its lens
        // during that hold introduced a visible 62 -> 65.345 degree zoom-out.
        // Keep the inherited lens until the native door changes camera phase.
        if(DoorStartLens(m,view))return door_start.fov;
        if(view==play+0x188 && EntryMatches(m,play+0x364,current_frame) && EntryView(m,play+0x364))
            return native_entry.fov;
        return (dialogue_release || entry_release) && active && owner==play+0x364 && view==play+0x188 &&
            player_owner==m.Read(owner+0xd8).value_or(0) && scene==Half(m,play+0x104) &&
            epoch==reset_epoch.load() && (Allowed(m,owner) || ReturningDialogue(m,owner))?
            dialogue_fov:Camera::NativeFov;
    }
    template<class M> bool DoorPending(M& m,U32 cam) const {
        const U32 play=m.Read(cam+0xd4).value_or(0);
        // Jabu's openings retain NORMAL for two updates after auto-walk starts.
        // Bridge only that bounded gap; native DOOR0 movement owns the view.
        return door_start.valid && !door_native_started && cam==door_start.camera &&
            current_frame>=door_start.frame && current_frame-door_start.frame<=1 &&
            Half(m,cam+0x18a)==door_source_setting && Half(m,cam+0x18c)==0 &&
            EntryContext(m,cam) && !EntryView(m,cam) &&
            (m.Read(door_start.player+0x1710).value_or(0)&0x20000000) &&
            !(m.Read(play+0x5c2c).value_or(~0u)&0xff00) &&
            !(m.Read(play+0x7f10).value_or(~0u)&0xff0000);
    }
    template<class M> bool DoorStartLens(M& m,U32 view) const {
        const U32 play=m.Read(0x5043e0).value_or(0),cam=play+0x364;
        if(!door_start.valid || view!=play+0x188 || door_start.camera!=cam ||
           door_start.player!=m.Read(cam+0xd8).value_or(0) ||
           door_start.scene!=Half(m,play+0x104) || door_start.epoch!=reset_epoch.load() ||
           current_frame<door_start.frame ||
           !NativeEntrance(m,cam) || Half(m,cam+0x18c)!=0)return false;
        return DoorPending(m,cam) ||
            (Half(m,cam+0x18a)==29 && Half(m,cam+0x1a6)<=1);
    }
    template<class M> bool ReturningDialogue(M& m,U32 cam) const {
        // Live trace: TALK -> NORMAL retains 0x10000, or 0x18000 when the
        // conversation began locked on, after the interaction actor clears.
        // The latter's Z bit was already set during TALK, not newly pressed.
        // Restrict the exception to a recent TALK handoff, NORMAL, no target
        // actor and no new Z press; genuine targeting keeps its native owner.
        if(!GameplayAllowed(m,cam))return false;
        const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
        const U32 flags=m.Read(player+0x1710).value_or(0);
        const bool inherited_z=(dialogue.valid && dialogue.z_latched) ||
            (dialogue_release && release_z_latched);
        if(Half(m,cam+0x18c)!=0 ||
           m.Read(player+0x16f8).value_or(0)!=0 ||
           (flags&(0x20000u|0x20000000u)) || ((flags&0x8000u) && !inherited_z))return false;
        const auto current_epoch=reset_epoch.load();
        return (dialogue.valid && dialogue.camera==cam && dialogue.player==player &&
                dialogue.scene==Half(m,play+0x104) && dialogue.epoch==current_epoch) ||
               (dialogue_release && active && owner==cam && player_owner==player &&
                scene==Half(m,play+0x104) && epoch==current_epoch);
    }
    Vec last_eye{};
    // Scratch lives on the guest stack, so an interrupted collision call is
    // self-contained even in a savestate. No host pointer crosses guest calls.
    // Keep the unmodified collision request above the native 0x28-byte query.
    // Version cookies distinguish older 0x40-byte pending savestate frames.
    static constexpr U32 ScratchSize=0x50, Cookie=0x4f524255, NativeCookie=0x4f52424f;
    static constexpr U32 LegacyCookie=0x4f524254, LegacyNativeCookie=0x4f52424e;
    template<class M,class R> void Begin(M& m,R& r,std::uint64_t frame=0) {
        const U32 cam=r[0], target=r[1];
        const auto previous_frame=current_frame;
        current_frame=frame;
        const U32 play=m.Read(cam+0xd4).value_or(0);
        if(frame<previous_frame || frame-previous_frame>1 || !DoorStartLens(m,play+0x188))
            door_start.valid=false;
        if(door_start.valid && Half(m,cam+0x18a)==29)door_native_started=true;
        const bool door_pending=door_start.valid && DoorPending(m,cam);
        const bool save_hold=save_view.camera==cam && SaveQuitLens(m,play+0x188);
        if(!EntryMatches(m,cam,frame) || !NativeEntranceContext(m,cam) ||
           (!NativeEntrance(m,cam) && !Allowed(m,cam)))native_entry.valid=false;
        // Every mode handler must run, including NORMAL after targeting ends:
        // it publishes the interface flags at 0x516eb0 and advances mode state.
        r[14]=Sites[1].pc; r[15]=target&~1u;
        ObserveEntry(m,cam);
        // Once the inherited bit clears, a subsequent Z press is new input.
        const U32 player=m.Read(cam+0xd8).value_or(0);
        if(!Pointer(player,0x1718) || !(m.Read(player+0x1710).value_or(0)&0x8000))release_z_latched=false;
        if(!DialogueMatches(m,cam,frame))dialogue.valid=false;
        // A pressed at a sliding door sets auto-walk one or two updates before
        // setting 29 starts. Running the ordinary camera without the orbit
        // correction during that gap pulls the displayed eye inward. Use the
        // shared gameplay eligibility in EntryContext, not a setting whitelist:
        // one captured door uses setting 4 on one side and 62 on the other.
        // Preserve the last collision-tested view across the bounded handoff;
        // the door handler then owns its hold, side change and return normally.
        const bool door_handoff=active && owner==cam && player_owner==player &&
            scene==Half(m,play+0x104) && epoch==reset_epoch.load() &&
            frame==previous_frame+1 && EntryContext(m,cam) && !EntryView(m,cam) &&
            Half(m,cam+0x18c)==0 &&
            (m.Read(player+0x1710).value_or(0)&0x20000000) &&
            !(m.Read(play+0x5c2c).value_or(~0u)&0xff00) &&
            !(m.Read(play+0x7f10).value_or(~0u)&0xff0000);
        if(!save_hold && !door_handoff && !door_pending && !Allowed(m,cam) && !Targeting(m,cam) && !EntryView(m,cam)) {
            if(Half(m,cam+0x18c)!=3)dialogue.valid=false;
            ResetMotion();return;
        }
        if(!Allowed(m,cam) && !ReturningDialogue(m,cam))dialogue.valid=false;
        Vec at,eye;
        if(!TitleScreen::ReadFloats(m,cam+0x80,at) || !TitleScreen::ReadFloats(m,cam+0x8c,eye) ||
           r[13]<ScratchSize || !Pointer(r[13]-ScratchSize,ScratchSize)) {ResetMotion();return;}
        const U32 sp=r[13]-ScratchSize;
        for(U32 i=0;i<ScratchSize;i+=4)if(!m.Read(sp+i)) {ResetMotion();return;}
        if(door_handoff) {
            door_start={last_eye,at,(dialogue_release || entry_release)?dialogue_fov:Camera::NativeFov,
                        cam,player,scene,epoch,frame,true};
            door_source_setting=Half(m,cam+0x18a);
            door_native_started=false;
            eye=last_eye;
            ResetMotion();
        }
        // Native mode changes can restore an old eye before dispatch. Use the
        // last view actually displayed, not that restored pre-conversation eye.
        if(save_hold) {eye=save_view.eye;at=save_view.at;ResetMotion();}
        else if(door_handoff || door_pending) {eye=door_start.eye;at=door_start.at;}
        else if(dialogue.valid) {eye=dialogue.eye;at=dialogue.at;}
        else if(native_entry.valid) {eye=native_entry.eye;at=native_entry.at;}
        Put(m,sp,eye);Put(m,sp+0x28,at);
        m.Write(sp+0x34,cam);m.Write(sp+0x38,NativeCookie);m.Write(sp+0x3c,sp^NativeCookie);
        r[13]=sp;
    }
    template<class M,class R> void Apply(M& m,R& r,U32 cam,Vec at,const Vec& eye) {
        // Called after the native mode has returned, using the preceding view
        // to keep native camera drift out of stick motion. Failed validation
        // leaves the native handler's output intact.
        if(!Allowed(m,cam) && !ReturningDialogue(m,cam)) {ResetMotion();return;}
        Vec pos;
        const U32 player=m.Read(cam+0xd8).value_or(0), play=m.Read(cam+0xd4).value_or(0);
        if(!TitleScreen::ReadFloats(m,player+0x28,pos)) {ResetMotion();return;}
        const U32 new_scene=Half(m,play+0x104);
        const unsigned new_epoch=reset_epoch.load();
        const bool from_target=target_view && owner==cam && player_owner==player &&
            scene==new_scene && epoch==new_epoch;
        const bool from_dialogue=dialogue.valid && dialogue.camera==cam && dialogue.player==player &&
            dialogue.scene==new_scene && dialogue.epoch==new_epoch;
        const bool from_entry=!from_dialogue && !from_target && EntryMatches(m,cam,current_frame);
        const float previous_fov=from_entry?native_entry.fov:dialogue.fov;
        native_entry.valid=false;
        dialogue.valid=false;
        if(from_entry || from_dialogue || owner!=cam || player_owner!=player || scene!=new_scene || epoch!=new_epoch || eye!=last_eye)
            ResetMotion();
        owner=cam;player_owner=player;scene=new_scene;epoch=new_epoch;
        const auto stick = Allowed(m,cam)?Stick(stick_x.load(),stick_y.load()):std::array<float,2>{};
        const bool child=m.Read(0x58795c).value_or(1)!=0;
        const float target_radius=ConfiguredRadius(m);
        pos[1]+=(child?38.f:50.f)*((m.Read(player+0x1710).value_or(0)&0x2000)?.5f:1.f);
        if(!active) {
            // Reacquire without waiting for stick input. Targeting, TALK and
            // native entrances preserve their view on the first gameplay update.
            yaw_velocity=pitch_velocity=0;
            // Targeting aims between Link and the target. Reconstruct the
            // orbit around Link from the existing eye, not that shifted aim
            // point, or changing pivots moves the camera on the release frame.
            const Vec pivot=(from_target || from_dialogue || from_entry)?pos:at;
            const float dx=pivot[0]-eye[0],dy=pivot[1]-eye[1],dz=pivot[2]-eye[2];
            radius=target_radius;
            yaw=std::atan2(dx,dz);pitch=std::atan2(dy,std::hypot(dx,dz));
            // First person can leave an angle outside the orbit range. Keep
            // that angle on the handoff frame, then gently recover without
            // requiring stick input. Scripted/target handoffs retain their
            // existing framing and release behavior.
            pitch_recovery=!(from_target || from_dialogue || from_entry) &&
                (pitch<MinimumPitch || pitch>MaximumPitch);
            if(from_target || from_dialogue || from_entry) {
                for(unsigned i=0;i<3;++i)release_at_offset[i]=at[i]-pos[i];
                radius=release_radius=std::hypot(dx,dy,dz);
                release_frame=0;
                dialogue_release=from_dialogue;
                entry_release=from_entry;
                release_z_latched=from_dialogue && dialogue.z_latched &&
                    (m.Read(player+0x1710).value_or(0)&0x8000)!=0;
                dialogue_fov=release_fov=previous_fov;
                dialogue_remaining=1;dialogue_rate=.01685f;
            }
            target_view=false;
        } else if(pitch_recovery) {
            const float limit=std::clamp(pitch,MinimumPitch,MaximumPitch);
            const float error=limit-pitch;
            if(std::abs(error)<=.02f*PitchRadians) {
                pitch=limit;pitch_recovery=false;
            } else {
                pitch+=error*pitch_recovery_rate;
                pitch_recovery_rate+=(.18f-pitch_recovery_rate)*.25f;
            }
        }
        constexpr float Pi=3.14159265358979323846f;
        const bool mirror=(m.Read(0x587964).value_or(0)&0xff0000)!=0;
        const float acceleration=ConfiguredAcceleration();
        const float deceleration=ConfiguredDeceleration();
        const float target_x=-stick[0]*((ConfiguredInvertX()^mirror)?-1:1);
        const float target_y=stick[1]*(ConfiguredInvertY()?-1:1);
        yaw_velocity=AdvanceVelocity(yaw_velocity,target_x,acceleration,deceleration);
        pitch_velocity=AdvanceVelocity(pitch_velocity,target_y,acceleration,deceleration);
        // This hook runs once per native 30 Hz camera update, independent of display refresh.
        // MM measurements: ~120 degrees/s horizontal, ~60 vertical at full tilt.
        const float speed=(4.f*Pi/180.f)*ConfiguredSensitivity();
        yaw=std::remainder(yaw+yaw_velocity*speed,2*Pi);
        // Preserve a steep native targeting angle on handoff. Input can move
        // it back within the orbit limits without a one-frame angle clamp.
        // Our pitch sign is opposite eye elevation: MM measured -30..+35.
        pitch=AdvancePitch(pitch,pitch_velocity,speed*.5f);
        Vec pivot;
        if(dialogue_release || release_frame<=ReleaseFrames) {
            const unsigned frame=release_frame++;
            float weight;
            if(dialogue_release) {
                // MM's captured exit starts gently, then approaches ~5% of
                // the remaining lens error per update. Preserve the first
                // frame exactly and bound the tail to avoid residual drift.
                if(frame) {
                    dialogue_remaining*=1-dialogue_rate;
                    dialogue_rate+=(.05f-dialogue_rate)*.18f;
                }
                weight=frame>=DialogueReleaseFrames?1.f:1-dialogue_remaining;
                dialogue_fov=release_fov+(Camera::NativeFov-release_fov)*weight;
                if(frame>=DialogueReleaseFrames)dialogue_release=false;
            } else {
                if(entry_release) {
                    // The child Deku door ends at ~60 units. The old 20%
                    // first step jumped ~25 units before collision held it
                    // against the doorway. Ease both ends of the same bounded
                    // recovery instead of front-loading the distance/FOV change.
                    const float t=std::min(1.f,float(frame)/ReleaseFrames);
                    weight=t*t*(3.f-2.f*t);
                } else weight=frame==ReleaseFrames?1.f:1.f-std::pow(.8f,float(frame));
                if(entry_release) {
                    dialogue_fov=release_fov+(Camera::NativeFov-release_fov)*weight;
                    if(frame==ReleaseFrames)entry_release=false;
                }
            }
            for(unsigned i=0;i<3;++i)at[i]=pos[i]+release_at_offset[i]*(1-weight);
            radius=release_radius+(target_radius-release_radius)*weight;
            pivot=pos;
        } else {
            for(unsigned i=0;i<3;++i) at[i]+=(pos[i]-at[i])*.3f;
            radius+=(target_radius-radius)*.5f;
            pivot=at;
        }
        // Rotation keeps a constant intended distance, including vertical
        // movement. A target handoff or explicit distance/age change can ease
        // the radius; native collision still takes precedence over either.
        Vec next{pivot[0]-radius*std::cos(pitch)*std::sin(yaw),pivot[1]-radius*std::sin(pitch),
                 pivot[2]-radius*std::cos(pitch)*std::cos(yaw)};
        if(ConfiguredSmoothPullback() && !dialogue_release && !entry_release && release_frame>ReleaseFrames)
            next=collision_recovery.Request(at,next,current_frame);
        else collision_recovery.Reset();
        active=BeginCollision(m,r,cam,at,next);
        if(!active)ResetMotion();
    }
    template<class M,class R> void ApplyEntry(M& m,R& r,U32 cam) {
        ResetMotion();
        Vec at,eye;
        if(!TitleScreen::ReadFloats(m,cam+0x80,at) || !TitleScreen::ReadFloats(m,cam+0x8c,eye))return;
        Vec delta;for(unsigned i=0;i<3;++i)delta[i]=eye[i]-at[i];
        const float length=std::hypot(delta[0],delta[1],delta[2]);
        if(!std::isfinite(length) || length<.01f)return;
        const float scale=ConfiguredRadius(m)/length;
        for(unsigned i=0;i<3;++i)eye[i]=at[i]+delta[i]*scale;
        // Native entrance direction, aim, timers and collision still run.
        // Once controls return, orbit inherits this already-correct radius.
        BeginCollision(m,r,cam,at,eye);
    }
    template<class M,class R> void ApplyTargeting(M& m,R& r,U32 cam) {
        ResetMotion();
        Vec at,eye,head;
        const U32 player=m.Read(cam+0xd8).value_or(0),play=m.Read(cam+0xd4).value_or(0);
        if(!TitleScreen::ReadFloats(m,cam+0x80,at) ||
           !TitleScreen::ReadFloats(m,cam+0x8c,eye) ||
           !TitleScreen::ReadFloats(m,player+0x28,head))return;
        owner=cam;player_owner=player;scene=Half(m,play+0x104);epoch=reset_epoch.load();
        target_view=true;
        const bool child=m.Read(0x58795c).value_or(1)!=0;
        head[1]+=(child?38.f:50.f)*((m.Read(player+0x1710).value_or(0)&0x2000)?.5f:1.f);
        Vec offset;for(unsigned i=0;i<3;++i)offset[i]=eye[i]-at[i];
        const float length=std::hypot(offset[0],offset[1],offset[2]);
        const float minimum=ConfiguredRadius(m);
        if(!std::isfinite(length) || length<.01f)return;
        Vec relative;for(unsigned i=0;i<3;++i)relative[i]=eye[i]-head[i];
        // Perspective size depends on depth along the viewing direction, not
        // Euclidean radius from Link. A sideways aim offset could satisfy the
        // old radius check while still bringing Link dramatically closer in
        // projection. Preserve native aim/ray, enforcing the configured depth.
        float depth=0;for(unsigned i=0;i<3;++i) {offset[i]/=length;depth+=relative[i]*offset[i];}
        if(!std::isfinite(depth) || depth>=minimum)return;
        const float retreat=minimum-depth;
        for(unsigned i=0;i<3;++i)eye[i]+=offset[i]*retreat;
        BeginCollision(m,r,cam,at,eye);
    }
    template<class M,class R> bool BeginCollision(M& m,R& r,U32 cam,const Vec& at,const Vec& next) {
        if(r[13]<ScratchSize || !Pointer(r[13]-ScratchSize,ScratchSize))return false;
        const U32 sp=r[13]-ScratchSize;
        for(U32 i=0;i<ScratchSize;i+=4) if(!m.Read(sp+i))return false;
        for(U32 i=0;i<ScratchSize;i+=4) m.Write(sp+i,0);
        Put(m,sp,next);Put(m,sp+0x28,at);Put(m,sp+0x40,next);
        m.Write(sp+0x34,cam);m.Write(sp+0x38,Cookie);m.Write(sp+0x3c,sp^Cookie);
        r[13]=sp;r[0]=cam;r[1]=sp+0x28;r[2]=sp;r[15]=Collision;
        return true;
    }
    template<class M,class R> void Finish(M& m,R& r,std::array<U32,64>* fp=nullptr,U32* flags=nullptr,U32* fpscr=nullptr) {
        if(fp && flags && fpscr && CameraClearance::Resume(m,r,*fp,*flags,*fpscr,reset_epoch.load(),Sites[1].pc))return;
        const U32 sp=r[13],cam=r[4];
        const U32 cookie=Pointer(sp,0x40)?m.Read(sp+0x38).value_or(0):0;
        const U32 scratch=(cookie==LegacyCookie || cookie==LegacyNativeCookie)?0x40:ScratchSize;
        r[15]=Resume;
        if(Pointer(sp,scratch) && (cookie==NativeCookie || cookie==LegacyNativeCookie) &&
           m.Read(sp+0x3c)==std::optional<U32>{sp^cookie} && m.Read(sp+0x34)==std::optional<U32>{cam}) {
            Vec at,eye;
            const bool valid=TitleScreen::ReadFloats(m,sp,eye) &&
                TitleScreen::ReadFloats(m,sp+0x28,at);
            m.Write(sp+0x38,0);r[13]+=scratch;
            if(valid && save_view.camera==cam && SaveQuitLens(m,m.Read(cam+0xd4).value_or(0)+0x188)) {
                // The world is exiting: run native mode bookkeeping, but keep
                // the last paused view while its black transition completes.
                Put(m,cam+0x80,at);Put(m,cam+0x8c,eye);Put(m,cam+0xa4,eye);last_eye=eye;
            } else if(valid && DoorPending(m,cam) &&
               DoorStartLens(m,m.Read(cam+0xd4).value_or(0)+0x188)) {
                r[14]=Sites[1].pc;
                BeginCollision(m,r,cam,at,eye);
            } else if(valid && EntryView(m,cam)) {
                // A restricted entrance can become a regular auto-walk before
                // controls return. Keep that native view until the handoff;
                // switching to ApplyEntry here would reintroduce the snap.
                if(native_entry.valid)RememberEntry(m,cam);
                else {
                    r[14]=Sites[1].pc;
                    ApplyEntry(m,r,cam);
                }
            } else if(valid && ReturningDialogue(m,cam)) {
                r[14]=Sites[1].pc;
                Apply(m,r,cam,at,eye);
            } else if(valid && Targeting(m,cam)) {
                r[14]=Sites[1].pc;
                ApplyTargeting(m,r,cam);
            } else if(valid && Allowed(m,cam)) {
                // Collision returns through this same site, using its own
                // cookie. Both stages can resume from a guest savestate.
                r[14]=Sites[1].pc;
                Apply(m,r,cam,at,eye);
            } else ResetMotion();
            return;
        }
        if(Pointer(sp,scratch) && (cookie==Cookie || cookie==LegacyCookie) &&
           m.Read(sp+0x3c)==std::optional<U32>{sp^cookie} && m.Read(sp+0x34)==std::optional<U32>{cam}) {
            if(fp && flags && fpscr && m.Read(sp+0x1c)!=CameraClearance::Done && GameplayAllowed(m,cam)) {
                const U32 view=m.Read(cam+0xd4).value_or(0)+0x188;
                const float near=std::bit_cast<float>(m.Read(view+0x14).value_or(0));
                const float fov=Camera::EffectiveFov(m,view,GameplayFov(m,view),Camera::ConfiguredFov());
                if(CameraClearance::Begin(m,r,*fp,*flags,*fpscr,cam,near,fov,
                    Widescreen::Active()?Widescreen::Aspect:Widescreen::NativeAspect,reset_epoch.load(),Sites[1].pc,
                    cookie==Cookie?sp+0x40:0))return;
            }
            Vec at,eye;
            if(Pointer(cam,0x1bc) && TitleScreen::ReadFloats(m,sp,eye) &&
               TitleScreen::ReadFloats(m,sp+0x28,at)) {
                if(collision_recovery.pending)
                    collision_recovery.Observe(at,eye,current_frame);
                Put(m,cam+0x80,at);Put(m,cam+0x8c,eye);Put(m,cam+0xa4,eye);
                last_eye=eye;
                if(DoorPending(m,cam))RememberEntry(m,cam);
                // Native tail computes movement directions from the collision-corrected eye.
            }
            m.Write(sp+0x38,0);r[13]+=scratch;
        } else if(native_entry.valid || (!Allowed(m,cam) && !Targeting(m,cam)))RememberEntry(m,cam);
        r[15]=Resume; // Displaced B, leaves flags intact.
    }
};
} // namespace OOT3D::OrbitCamera
