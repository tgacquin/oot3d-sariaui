// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_state.h"
#include "oot3d/camera.h"
#include "oot3d/qol.h"
#include "oot3d/game_control_options.h"
#include "oot3d/widescreen.h"
#include "oot3d/hud_fade.h"
#include "oot3d/multiplayer_startup.h"
#include "oot3d/pause_menu.h"

namespace OOT3D::Interpolation {
using TitleScreen::U32;
using Math::Matrix;
// Camera capture feeds the emulator-side replay; legacy preview stays isolated. 
enum class Status {Off,Waiting,Ready,Cut,Gap,Invalid};
inline std::atomic<Status> status{Status::Off};
constexpr U32 FirstSVC=0xfff198;
constexpr TitleScreen::Site Site{0x479290,0xe58040fc,0xe28dd074};
constexpr U32 RefreshSVC=0xfff19a;
// First-person camera modes refresh the View after the main draw setup via
// 4592d0. This path rebuilds world/attached views but retains all three lenses.
// Capture its publication too, before the next command buffer uses that View.
constexpr TitleScreen::Site RefreshSite{0x45940c,0xe58040fc,0xe28dd024};
inline U32 Instruction(U32 pc,U32 word) {
    if(pc==Site.pc && word==Site.instruction)return 0xef000000|FirstSVC;
    if(pc==RefreshSite.pc && word==RefreshSite.instruction)return 0xef000000|RefreshSVC;
    return word;
}
template<class M> bool CodeMatches(M& m) {
    return Camera::CodeMatches(m) && m.Read(Site.pc)==Site.instruction && m.Read(Site.pc+4)==Site.following &&
        m.Read(0x47918c)==0xebf98335 && // Native look-at into View+114.
        m.Read(RefreshSite.pc)==RefreshSite.instruction && m.Read(RefreshSite.pc+4)==RefreshSite.following &&
        m.Read(0x459344)==0xebfa02c7 && m.Read(0x459398)==0xebfa02b2; // World/attached look-at.
}
inline double Setting(float v,double low,double high,double fallback) {
    return std::isfinite(v)?std::clamp(double(v),low,high):fallback;
}
struct History {
    Matrix previous{},current{};
    std::int64_t time=0;
    U32 owner=0,scene=0;
    bool seen=false,ready=false;
    void Reset() {seen=ready=false;time=0;owner=scene=0;}
    Status Observe(const Matrix& view,U32 next_owner,U32 next_scene,std::int64_t now,double distance,double angle) {
        const auto inverse=Math::InverseCamera(view);
        if(!inverse) {Reset();return Status::Invalid;}
        const auto& camera=*inverse;
        // Repeated draws of a native frame must not advance the history.
        if(seen && owner==next_owner && scene==next_scene && now>=time && now-time<10000 &&
           Math::Near(camera,current,.00001))return ready?Status::Ready:Status::Waiting;
        const bool adjacent=seen && owner==next_owner && scene==next_scene && now>time && now-time<=100000;
        Status result=seen?Status::Gap:Status::Waiting;ready=false;
        if(adjacent) {
            double displacement=0,trace=0;
            for(unsigned i=0;i<3;++i) {
                displacement+=(camera[i][3]-current[i][3])*(camera[i][3]-current[i][3]);
                for(unsigned j=0;j<3;++j)trace+=current[i][j]*camera[i][j];
            }
            ready=displacement<=distance*distance && (trace-1)/2>=std::cos(angle*3.141592653589793/180.);
            result=ready?Status::Ready:Status::Cut;
        }
        previous=current;current=camera;time=now;owner=next_owner;scene=next_scene;seen=true;
        return result;
    }
    Matrix View(double fraction) const {return Math::InverseRigid(Math::Blend(previous,current,fraction));}
};
struct Hook {
    History history;
    Matrix source{};
    unsigned epoch=0;
    bool Sync() {
        const auto next=generation.load();
        const auto session=MultiplayerStartup::GetSession();
        const bool needed=enabled.load() || Widescreen::Active() || (session && session->role);
        if(next!=epoch || !needed) {
            history.Reset();Publish({});epoch=next;status=enabled.load()?Status::Waiting:Status::Off;
        }
        return needed;
    }
    template<class M> bool Gameplay(M& m) const {
        const U32 play=m.Read(PauseMenu::UI+0xc).value_or(0);
        return HUDFade::GameplayReady(m,play) && !PauseMenu::Active(m);
    }
    template<class M> bool WorldScene(M& m) const {
        const U32 play=m.Read(PauseMenu::UI+0xc).value_or(0);
        if(!TitleScreen::Pointer(play,0x2cc) || PauseMenu::Active(m) || SaveMenu::Active(m))return false;
        const auto context=m.Read(play+0x100);
        // Transition flags hide the HUD before the world stops rendering.
        // Read-only replay can keep capturing that world's real View through
        // fades and scripted movement, even when player input/update is held.
        // View ownership is checked below; scene identity and cuts still break
        // the camera/pose pairs. Title, file select and menus stay excluded.
        return context && (*context&0xffff)==0x0203;
    }
    template<class M,class R> void Capture(M& m,R& r,std::int64_t now,bool supported) {
        m.Write(r[0]+0xfc,r[4]); // Displaced native STR; never replace the live camera.
        if(!supported)native_scene=~0u;
        else if(Camera::GameplayView(m,r[4])) {
            const U32 play=m.Read(PauseMenu::UI+0xc).value_or(0);
            native_scene=TitleScreen::Pointer(play,0x108)?m.Read(play+0x104).value_or(~0u):~0u;
        }
        if(!Sync()) {capture_result=CaptureResult::Off;return;}
        if(!supported || !(Replaying()?WorldScene(m):Gameplay(m))) {
            capture_result=CaptureResult::WorldInactive;
            history.Reset();Publish({});status=Status::Waiting;return;
        }
        if(!Camera::GameplayView(m,r[4])) {capture_result=CaptureResult::OtherView;return;}
        std::array<float,12> values;
        if(!TitleScreen::ReadFloats(m,r[4]+0x114,values)) {
            capture_result=CaptureResult::ReadFailure;
            history.Reset();Publish({});status=Status::Invalid;return;
        }
        source=Math::Identity();
        for(unsigned i=0;i<12;++i)source[i/4][i%4]=values[i];
        const U32 play=m.Read(PauseMenu::UI+0xc).value_or(0),scene=m.Read(play+0x104).value_or(~0u);
        const auto result=history.Observe(source,play,scene,now,
            Setting(distance_limit.load(),1,1000,250),Setting(angle_limit.load(),1,90,45));
        capture_result=result==Status::Invalid?CaptureResult::InvalidTransform:CaptureResult::Ready;
        CameraSample sample{source,play,scene,epoch,now,result!=Status::Invalid};
        sample.native_fov=std::bit_cast<float>(m.Read(r[4]+0x10).value_or(0));
        // Native SHOP (0x19) swaps fixed views without a large enough
        // displacement/angle to trip the generic cut detector. Mode changes
        // within that setting are not cuts. Never alter the native camera.
        const auto active=m.Read(play+0xa64);
        const auto setting=m.Read(play+0x364+0x188);
        sample.cut_group=active && (*active&0xffff)==0 && setting &&
            (*setting>>16)==0x19 ? 0x19 : 0;
        const auto mode=m.Read(play+0x364+0x18c);
        sample.first_person=active && (*active&0xffff)==0 && mode &&
            FirstPersonMode(*mode&0xffff);
        if(sample.first_person && GameControlOptions::motion_controls.load())sample.aim_mode=*mode&0xffff;
        sample.projections_valid=true;
        for(unsigned lens=0;lens<3;++lens) {
            std::array<float,16> projection;
            if(!TitleScreen::ReadFloats(m,r[4]+0x54+lens*0x40,projection)) {
                sample.projections_valid=false;break;
            }
            // Native View matrices precede the CMB renderer's 90-degree
            // screen rotation. Capture all coefficients, including heat warp.
            auto& p=sample.projections[lens];
            for(unsigned col=0;col<4;++col) {
                p[0][col]=projection[4+col];p[1][col]=-projection[col];
                p[2][col]=projection[8+col];p[3][col]=projection[12+col];
            }
        }
        if(TitleScreen::ReadFloats(m,r[4]+0x144,values)) {
            sample.attached_view=Math::Identity();
            for(unsigned i=0;i<12;++i)sample.attached_view[i/4][i%4]=values[i];
            sample.attached_valid=Math::Rigid(sample.attached_view);
        }
        Publish(sample);
        ++samples;status=result;
        if(result==Status::Ready)++pairs;
        else if(result==Status::Cut || result==Status::Gap || result==Status::Invalid)++rejected;
    }
    template<class M> bool Apply(M& m,U32 temporary_view,std::int64_t now,bool supported) {
        if(!Sync() || present_60.load() || !preview.load() || !supported || !history.ready ||
           now<history.time || now-history.time>100000 || !Gameplay(m) ||
           history.owner!=m.Read(PauseMenu::UI+0xc).value_or(0) ||
           m.Read(0x5be5b8+0xfc)!=history.owner+0x188 ||
           history.scene!=m.Read(history.owner+0x104).value_or(~0u))return false;
        std::array<float,12> native;
        if(!TitleScreen::ReadFloats(m,temporary_view,native))return false;
        // Match the exact freshly captured world view. HUD, menus, shadow
        // cameras and model previews keep their own transforms.
        for(unsigned i=0;i<12;++i)if(native[i]!=float(source[i/4][i%4]))return false;
        const auto mixed=history.View(Setting(phase.load(),0,1,.5));
        for(unsigned i=0;i<12;++i)m.Write(temporary_view+4*i,std::bit_cast<U32>(float(mixed[i/4][i%4])));
        ++applied;return true;
    }
};
}
