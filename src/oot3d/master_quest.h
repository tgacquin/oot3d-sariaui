// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include "oot3d/control_swap.h"
#include "oot3d/loading_prompt.h"
#include "oot3d/room_join.h"
#include "oot3d/randomizer/file_status.h"
#include <atomic>

namespace OOT3D::MasterQuest {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff200, Owner=0x504fa0, System=0x587934;
constexpr U32 Exists=0x55bea8, Saves=0x55bec0, Stride=0x15c4;
constexpr U32 LiveSave=0x587958, SaveSize=0x14dc, ModeOffset=0xe;
constexpr U32 RandomizerToggleSound=0x01000129; // NA_SE_IT_REFLECTION_WOOD.
constexpr U32 RejectionSound=0x010004c2; // Native keyboard empty/invalid-name cue.
// Refresh native card lettering before the geometry tail, without menu input.
constexpr U32 FilePresentation=0x430f14;
constexpr U32 GuestStart=0x43082c,GuestBack=0x4308d0;
constexpr U32 ProfileBack=0x4308f0; // Native Back transition after its sound.
// Existing immutable 0.75f ROM literal. Audio retains the volume pointer, so
// it must not point into a temporary stack slot or modify the shared 1.0f.
constexpr U32 RandomizerToggleVolume=0x1148f8;
constexpr std::array<TitleScreen::Site,10> Sites{{
    {0x3487ec,0xe5d00000,0xe35000be}, // Title's unlock test.
    {0x2e63c8,0xe5d0101f,0xe35100ef}, // Save-bank query, exclusively used by save I/O/UI.
    {0x43004c,0xe59f3404,0xe59f2404}, // A on a stone, before changing any menu state.
    {0x43108c,0xeb0058b2,0xe28dd06c}, // Start selected save.
    {0x43006c,0xe59f53f0,0xe5950018}, // Native Select sound returned.
    {0x4301c8,0xe5960014,0xe2800001}, // Native cursor sound returned.
    {0x43015c,0xe51f1370,0x03a00004}, // Native Back sound returned.
    {0x2eea14,0xe3a01e95,0xeb001b74}, // File-prompt message ID.
    {0x2eea20,0xe5810018,0xe28dd01c}, // Remember which text the live prompt owns.
    {0x43082c,0xe51f33dc,0xe51f23dc}, // Start action, before sound, fade or menu teardown.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    return m.Read(0x447348)==Saves && m.Read(0x447354)==Stride/4 &&
        m.Read(0x447358)==Exists && m.Read(0x2eeeb8)==System &&
        m.Read(0x2fddf4)==SaveSize && m.Read(0x2fdde8)==LiveSave &&
        m.Read(0x483894)==LiveSave && m.Read(0x352dcc)==0xe5d5000e &&
        m.Read(0x430460)==0x010004c0 && m.Read(0x430474)==0x010004be &&
        m.Read(0x430470)==0x010004c1 && m.Read(0x430068)==0xebfd1503 &&
        m.Read(RandomizerToggleVolume)==0x3f400000 &&
        m.Read(0x4301c4)==0xebfd14ac &&
        m.Read(FilePresentation)==0xe320f000 && m.Read(FilePresentation+4)==0xebfaf5b1 &&
        m.Read(ProfileBack)==0xe3570000 && m.Read(ProfileBack+4)==0xe59f152c &&
        m.Read(0x430298)==0xe3510000 && m.Read(0x43029c)==0x1a000016 &&
        m.Read(0x4300c4)==0xe59f139c && m.Read(0x4300c8)==0xed1f0aac &&
        m.Read(GuestStart)==0xe51f33dc && m.Read(GuestBack)==0xe51f3480;
}
inline std::atomic<bool> choosing{false};
inline std::atomic<unsigned> highlighted{0};
inline std::atomic<bool> ready{false},leaving{false};
inline std::atomic<bool> cancelling{false},returning{false},departing_files{false};
inline std::atomic<bool> loading{false},loading_label{false},generation_return{false};
inline std::atomic<float> loading_alpha{0};
inline std::atomic<bool> mq_error{false};
using FileError=Randomizer::FileStatus::Error;
inline std::atomic<FileError> start_error{FileError::None};
inline std::atomic<FileError> notice_error{FileError::None};
// 0..2 file cards, 3..5 Start/Copy/Erase, 6..7 quest choices.
inline std::atomic<unsigned> notice_target{8};
inline std::atomic<std::int64_t> notice_shake_us{200000};
constexpr std::int64_t ShakeUs=200000;
inline std::atomic<std::int64_t> shake_us{ShakeUs};
inline std::atomic<std::int64_t> start_shake_us{ShakeUs};
inline float ShakeOffset(std::int64_t elapsed) {
    const float t=std::clamp(float(elapsed)/ShakeUs,0.f,1.f);
    return 5.f*std::sin(t*6*3.141592654f)*(1-t);
}
inline float NoticeShake(unsigned target) {
    return notice_error!=FileError::None && notice_target==target?ShakeOffset(notice_shake_us):0;
}
inline float StartShake() {return start_error!=FileError::None?ShakeOffset(start_shake_us):0;}
inline std::atomic<U32> prompt_controller{0},prompt_message{0x950};
inline std::atomic<bool> starting{false};
inline U32 PromptMessage() {
    if(notice_error!=FileError::None && LoadingPrompt::Ready())return LoadingPrompt::ErrorId(notice_error.load());
    if(starting.load() && LoadingPrompt::Ready())return LoadingPrompt::StartingMessageId;
    if(start_error!=FileError::None && LoadingPrompt::Ready())
        return LoadingPrompt::ErrorId(start_error.load());
    if(RoomJoin::StatusPrompt() && LoadingPrompt::Ready())return LoadingPrompt::RoomMessageId();
    if(choosing.load() && !cancelling.load() && mq_error.load() && LoadingPrompt::Ready())
        return LoadingPrompt::ErrorMessageId;
    if(choosing.load() && !cancelling.load())
        return loading_label.load() && LoadingPrompt::Ready()?LoadingPrompt::GenerationMessageId():0x900;
    return 0x950;
}
constexpr std::int64_t SlideUs=266667,StaggerUs=100000;
inline std::atomic<std::int64_t> motion_us{SlideUs+StaggerUs};
constexpr std::int64_t HopUpUs=40000,HopDownUs=93333;
inline std::atomic<std::int64_t> hop_us{HopUpUs+HopDownUs};
inline std::atomic<std::int64_t> cursor_us{0};
inline float CursorGlow() {
    // Native file cursor: 30 ticks rising, 30 falling, alpha 0..180/255.
    // Emulated time supplies the in-between values at 60 Hz.
    constexpr std::int64_t period=2000000;
    const float phase=float(cursor_us.load()%period)/1000000.f;
    return (1-std::abs(phase-1))*(180.f/255.f);
}
inline float RandomizerAlpha() {
    constexpr float fade_us=150000;
    const float t=std::clamp(float(motion_us.load())/fade_us,0.f,1.f);
    const float eased=t*t*(3-2*t);
    return leaving.load()?1-eased:eased;
}
inline float RandomizerHop() {
    const auto elapsed=hop_us.load();
    const float t=std::clamp(float(elapsed<HopUpUs?elapsed:elapsed-HopUpUs)/
        float(elapsed<HopUpUs?HopUpUs:HopDownUs),0.f,1.f);
    const float smooth=t*t*(3-2*t);
    return -3.f*(elapsed<HopUpUs?smooth:1-smooth);
}
inline std::array<float,2> Offsets(float strength) {
    const bool exit=leaving.load();const auto elapsed=motion_us.load();
    const bool cancel_exit=exit && cancelling.load();
    const float power=1+(std::isfinite(strength)?std::clamp(strength,0.f,4.f):2.f);
    std::array<float,2> result{};
    for(unsigned i=0;i<2;++i) {
        const auto delay=cancel_exit?0:i*StaggerUs;
        const float t=std::clamp(float(elapsed-delay)/SlideUs,0.f,1.f);
        const float remaining=std::pow(1-t,power);
        result[i]=exit?(cancelling.load()?480:-480)*(1-remaining):
            (generation_return.load()?-480:480)*remaining;
    }
    if(mq_error.load() && !exit) {
        result[1]+=ShakeOffset(shake_us);
    }
    if(!exit)for(unsigned i=0;i<2;++i)result[i]+=NoticeShake(6+i);
    return result;
}
inline float ReturnOffset(unsigned slot,float strength) {
    if(!returning.load() && !departing_files.load())return 0;
    const float t=std::clamp(float(motion_us.load()-slot*StaggerUs)/SlideUs,0.f,1.f);
    const float power=1+(std::isfinite(strength)?std::clamp(strength,0.f,4.f):2.f);
    const float remaining=std::pow(1-t,power);
    return -480*(departing_files.load()?1-remaining:remaining);
}
inline std::atomic<unsigned> pointer_action{0}; // 1/2: choose mode; 3: cancel.
template<class M> unsigned Byte(M& m,U32 at) {
    return (m.Read(at&~3u).value_or(0)>>((at&3)*8))&255;
}
template<class M> void SetByte(M& m,U32 at,unsigned value) {
    const auto word=m.Read(at&~3u);if(!word)return;
    const unsigned shift=(at&3)*8;
    m.Write(at&~3u,(*word&~(255u<<shift))|((value&255)<<shift));
}
template<class M> void SetMode(M& m,unsigned mode) {
    // These are the original game's unlock and current-mode markers. Gameplay,
    // mirroring, damage, dungeon resources and native saving all see one mode.
    if(mode)SetByte(m,System,0xbe);
    SetByte(m,System+0x1f,mode?0xef:0);
    SetByte(m,LiveSave+ModeOffset,mode?1:0);
}
template<class M> bool Occupied(M& m,unsigned slot) {
    return slot<3 && m.Read(Exists+slot*4).value_or(0)!=0;
}
// The native save already contains its Master Quest flag at +0xe. Gameplay
// reads it for dungeon layouts, mirroring, damage and MQ-specific logic. Native
// CRC, copy, erase and load include this byte without modifying the save format.
// Only the save-bank selector is bypassed: every mode uses save00..save02.
template<class M> unsigned Mode(M& m,unsigned slot) {
    return slot<3 && Byte(m,Saves+slot*Stride+ModeOffset)!=0;
}
template<class M> std::u16string SavedFileName(M& m,unsigned slot) {
    std::u16string name;
    if(slot<3) {
        const auto base=Saves+slot*Stride;
        for(unsigned i=0;i<std::min(8u,Byte(m,base+0x2c));++i) {
            const auto c=Byte(m,base+0x1c+2*i)|(Byte(m,base+0x1d+2*i)<<8);
            if(c>=32 && c!=0xffff)name.push_back(char16_t(c));
        }
    }
    return name.empty()?u"Link":name;
}
// The native file menu can be exercised independently of the randomizer.
struct OrdinaryFiles {
    void ToggleRandomizer() {}
    template<class M> int Prepare(M&, unsigned, bool) { return 1; }
    template<class M> bool Open(M&, unsigned) { return true; }
    template<class M> bool Start(M&, unsigned) { return true; }
    void Cancel() {}
};
struct Hook {
    unsigned slot=3,choice=0;
    U32 constructing_prompt_message=0x950;
    bool generating=false;
    bool modal=false,wait_release=false,confirmed=false,block_cancel=false,awaiting_name=false;
    enum class Sound {None,Select,Move,Back};
    Sound sound_only=Sound::None;
    std::int64_t started_us=0;
    std::int64_t hop_started_us=0;
    std::int64_t shake_started_us=0;
    std::int64_t start_shake_started_us=0;
    unsigned start_error_slot=3;
    std::shared_ptr<const Randomizer::FileStatus::Notice> seen_notice;
    std::int64_t notice_started_us=0;
    unsigned notice_state=0,notice_slot=3,notice_action=0;
    void ShowNotice(FileError error,std::int64_t now,unsigned state,unsigned selected,unsigned action,bool quest) {
        notice_state=state;notice_slot=selected;notice_action=action;
        notice_started_us=now;notice_shake_us=0;
        notice_target=quest?6+choice:state==8 && action<3?3+action:selected;
        notice_error=error;
    }
    void ClearNotice() {notice_error=FileError::None;notice_target=8;}

    bool start_wait_release=false;
    std::int64_t loading_tick_us=0;
    bool toggle_down=false,x_down=false,hop_active=false;
    unsigned profile_buttons=0,pending_profile=0,profile_slot=3;
    Hook() {ClearNotice();seen_notice=Randomizer::FileStatus::LatestNotice();Reset();RoomJoin::Reset();}
    ~Hook() {ClearNotice();Reset();RoomJoin::Reset();}
    void Reset(bool keep_start_error=false,bool keep_starting=false) {
        if(!keep_starting)starting=false;
        choosing=false;pointer_action=0;ready=false;leaving=false;
        cancelling=false;returning=false;departing_files=false;
        loading=false;loading_label=false;generation_return=false;loading_alpha=0;loading_tick_us=0;
        motion_us=SlideUs+StaggerUs;
        hop_us=HopUpUs+HopDownUs;hop_active=false;toggle_down=false;x_down=false;
        cursor_us=0;
        mq_error=false;shake_us=ShakeUs;
        if(!keep_start_error) {start_error=FileError::None;start_shake_us=ShakeUs;start_error_slot=3;start_wait_release=false;}
    }
    template<class M,class R,class Files=OrdinaryFiles> bool Dispatch(M& m,R& r,std::int64_t now_us=0, Files files={}) {
        cursor_us=std::max(std::int64_t{0},now_us);
        if(mq_error.load())shake_us=std::clamp(now_us-shake_started_us,std::int64_t{0},ShakeUs);
        const auto state=m.Read(Owner+0x10).value_or(0);
        if(state!=8 && state!=10 && state!=11)starting=false;
        const auto selected=m.Read(Owner+0x14).value_or(3);
        const auto action=m.Read(Owner+0x18).value_or(0);
        if(notice_error!=FileError::None) {
            if(state!=notice_state || selected!=notice_slot || (state==8 && action!=notice_action))ClearNotice();
            else notice_shake_us=std::clamp(now_us-notice_started_us,std::int64_t{0},ShakeUs);
        }
        if(!generating && (state==3 || state==8)) {
            const auto event=Randomizer::FileStatus::LatestNotice();
            if(event && event!=seen_notice) {
                seen_notice=event;
                // Room joins already own their native status prompt. Consume
                // their event here so cancelling cannot revive it afterward.
                if(!event->Cancelled() && !RoomJoin::Active() && !RoomJoin::GuestActive())
                    ShowNotice(event->error,now_us,state,selected,action,modal);
            }
        }

        if(RoomJoin::phase==RoomJoin::Phase::ProfileLeaving) {
            RoomJoin::opacity=1-RoomJoin::Fade(now_us);
            if(RoomJoin::opacity.load()>0) {r[15]=FilePresentation;return true;}
            RoomJoin::phase=RoomJoin::Phase::ProfileReturn;RoomJoin::profile_visible=false;
            // Reuse native action-menu cleanup, hidden after the fade. The
            // Join press already played Select; do not play Back a second time.
            r[6]=Owner;r[7]=1;r[15]=ProfileBack;return true;
        }
        if(RoomJoin::phase==RoomJoin::Phase::ProfileReturn && state==3)
            RoomJoin::Wait(now_us,((m.Read(0x50af18).value_or(0)|m.Read(0x50af14).value_or(0))&2)!=0);
        RoomJoin::profile_visible=state==8 && !RoomJoin::Active() && !RoomJoin::GuestActive() &&
            bool(RoomJoin::ProfileAt(selected));
        const auto buttons=m.Read(0x50af18).value_or(0)|m.Read(0x50af14).value_or(0);
        if(!(buttons&1))start_wait_release=false;
        if(state!=8 || selected!=start_error_slot || m.Read(Owner+0x18).value_or(0)!=0)
            start_error=FileError::None;
        if(start_error!=FileError::None)
            start_shake_us=std::clamp(now_us-start_shake_started_us,std::int64_t{0},ShakeUs);
        const auto edges=buttons&~profile_buttons;profile_buttons=buttons;
        if(const auto request=std::atomic_load(&RoomJoin::request);request && request->inline_host) {
            const auto status=request->status.load();
            if(status==RoomJoin::Status::Connected && !request->cancelled.load()) {
                request->accepted=true;RoomJoin::host_opening=false;
                std::atomic_store(&RoomJoin::request,std::shared_ptr<const RoomJoin::Request>{});
            } else if(request->cancelled.load() || (status!=RoomJoin::Status::Searching && status!=RoomJoin::Status::Connecting)) {
                RoomJoin::host_opening=false;RoomJoin::host_error=!request->cancelled.load();
                std::atomic_store(&RoomJoin::request,std::shared_ptr<const RoomJoin::Request>{});
            }
        }
        if(RoomJoin::profile_visible.load()) {
            if(pending_profile) {
                RoomJoin::BeginProfile(profile_slot,RoomJoin::Intent::Resume,SavedFileName(m,profile_slot),now_us);
                pending_profile=0;awaiting_name=false;
                r[15]=FilePresentation;return true;
            }
            if(RoomJoin::stopping.load()) {r[15]=FilePresentation;return true;}
            if((edges&0x800) || ((edges&0x400) && !RoomJoin::hosting.load() && !RoomJoin::host_opening.load())) {
                if(!(edges&0x800)) {
                    RoomJoin::host_error=false;pending_profile=1;profile_slot=selected;
                    // Join uses the ordinary Select call and guarded return.
                    sound_only=Sound::Select;r[3]=m.Read(0x430458).value_or(0);
                    r[15]=Sites[2].pc+4;return true;
                }
                RoomJoin::host_hop_started=now_us;
                if((edges&0x800) && (RoomJoin::hosting.load() || RoomJoin::host_opening.load())) {
                    RoomJoin::stopping=true;RoomJoin::leave_complete=false;RoomJoin::leave_requested=true;
                } else if(edges&0x800) {RoomJoin::BeginHost(selected);m.Write(Owner+0x18,0);m.Write(Owner+0xc,~0u);}
                r[0]=RandomizerToggleSound;r[1]=0;r[2]=4;r[3]=m.Read(0x43045c).value_or(0);
                m.Write(r[13],RandomizerToggleVolume);m.Write(r[13]+4,m.Read(0x430458).value_or(0));
                sound_only=Sound::Move;r[15]=0x4301c4;return true;
            }
            if(RoomJoin::hosting.load() || RoomJoin::host_opening.load()) {
                m.Write(Owner+0x18,0);m.Write(Owner+0xc,~0u);
                if(edges&2) {r[6]=Owner;r[7]=1;r[15]=GuestBack;}
                else if((edges&1) && RoomJoin::hosting.load() && !RoomJoin::host_opening.load()) {
                    r[6]=Owner;r[15]=GuestStart;
                } else r[15]=FilePresentation;
                return true; // Copy/Erase cannot be selected by keys, stick or touch.
            }
        } else pending_profile=0;
        if(RoomJoin::GuestActive() && state==8) {
            const auto input=m.Read(0x50af18).value_or(0)|m.Read(0x50af14).value_or(0);
            const bool back=(input&2)!=0;
            if(RoomJoin::guest==RoomJoin::Guest::Waiting) {
                if(back && !RoomJoin::back_held)RoomJoin::Disconnect();
                else if(RoomJoin::remote_started.load()) {
                    RoomJoin::guest=RoomJoin::Guest::Starting;
                    r[6]=Owner;r[15]=GuestStart;return true; // Native Start, only on the host signal.
                }
            }
            RoomJoin::back_held=back;
            if(RoomJoin::guest==RoomJoin::Guest::Disconnecting && RoomJoin::leave_complete.load()) {
                RoomJoin::guest=RoomJoin::Guest::Returning;
                r[6]=Owner;r[7]=1;r[15]=GuestBack;return true; // Native Back after teardown, without input delay.
            }
            r[15]=FilePresentation;return true; // Block A, directions and native touch actions.
        }
        // Returning from session-name entry uses the native keyboard cleanup
        // and file-menu initialization, but never creates or writes a save.
        if(RoomJoin::HideFiles() && state==2) {
            m.Write(Owner+0x28,10); // Finish the hidden file-card entrance.
            if(RoomJoin::phase.load()==RoomJoin::Phase::Reopen) {
                for(unsigned i=0;i<3;++i) {
                    for(U32 base:{0x55b5e0u,0x55b5f8u})m.Write(base+4*i,std::bit_cast<U32>(16.f));
                    for(U32 base:{0x55b5ecu,0x55b604u})m.Write(base+4*i,std::bit_cast<U32>(48.f+52*i));
                    m.Write(0x55b610+4*i,0);m.Write(0x55b61c+4*i,0);
                }
            }
        }
        if(RoomJoin::Active() && state==3 && m.Read(RoomJoin::NameOwner+4).value_or(0)==0) {
            const auto p=RoomJoin::phase.load();
            if(p==RoomJoin::Phase::Reopen) {
                const unsigned selected=RoomJoin::slot;
                if(Occupied(m,selected) && files.Open(m,selected)) {
                    const auto request=std::atomic_load(&RoomJoin::request);
                    const bool is_host=request && request->intent==RoomJoin::Intent::Host;
                    if(request)request->accepted=true;
                    RoomJoin::Reset();Reset();modal=false;slot=3;confirmed=false;awaiting_name=false;
                    m.Write(Owner+0x14,selected);SetMode(m,Mode(m,selected));
                    RoomJoin::entrance_slot=selected;RoomJoin::success_chime=true;
                    RoomJoin::guest=is_host?RoomJoin::Guest::None:RoomJoin::Guest::Waiting;
                    r[4]=1;r[3]=m.Read(0x430458).value_or(0);
                    r[15]=Sites[2].pc+4;return true; // Native selection with one completion chime.
                }
                RoomJoin::phase=RoomJoin::Phase::Waiting;RoomJoin::opacity=1;
                if(const auto request=std::atomic_load(&RoomJoin::request)) {
                    request->committing=false;request->accepted=false;request->status=RoomJoin::Status::SaveFailed;
                }
            }
            U32 input=m.Read(0x50af18).value_or(0)|m.Read(0x50af14).value_or(0);
            if(p==RoomJoin::Phase::ReturnToFiles) {
                slot=RoomJoin::slot;choice=RoomJoin::choice;modal=true;awaiting_name=false;
                choosing=true;departing_files=false;leaving=false;ready=false;
                RoomJoin::Wait(now_us,(input&2)!=0);
            }
            if(RoomJoin::WaitingScreen()) {
                const bool back=(input&2)!=0;
                if(RoomJoin::phase.load()==RoomJoin::Phase::Waiting) {
                    RoomJoin::opacity=RoomJoin::Fade(now_us);
                    if(back && !RoomJoin::back_held && RoomJoin::CanCancel()) {
                        RoomJoin::leaving_status=RoomJoin::CurrentStatus();
                        RoomJoin::exit_opacity=RoomJoin::opacity.load();
                        RoomJoin::phase=RoomJoin::Phase::Leaving;
                        RoomJoin::started_us=now_us;
                        if(const auto request=std::atomic_load(&RoomJoin::request))request->cancelled=true;
                        std::atomic_store(&RoomJoin::request,std::shared_ptr<const RoomJoin::Request>{});
                        sound_only=Sound::Back;r[15]=0x430138;
                        RoomJoin::back_held=back;return true;
                    }
                    if(RoomJoin::CurrentStatus()==RoomJoin::Status::Connected ||
                       RoomJoin::CurrentStatus()==RoomJoin::Status::SavedDisconnected) {
                        RoomJoin::exit_opacity=RoomJoin::opacity.load();
                        RoomJoin::phase=RoomJoin::Phase::Leaving;
                        RoomJoin::started_us=now_us;
                    }
                } else RoomJoin::opacity=RoomJoin::exit_opacity*(1-RoomJoin::Fade(now_us));
                RoomJoin::back_held=back;
                if(RoomJoin::phase.load()!=RoomJoin::Phase::Leaving || RoomJoin::opacity.load()>0) {
                    r[15]=0x4336f8;return true;
                }
            }
            if(p==RoomJoin::Phase::Naming || RoomJoin::phase.load()==RoomJoin::Phase::Leaving) {
                if(const auto request=std::atomic_load(&RoomJoin::request);request && request->saved.load() &&
                   (request->status==RoomJoin::Status::Connected || request->status==RoomJoin::Status::SavedDisconnected)) {
                    // Rebuild native file models from the completed disk write,
                    // then select the imported file without another A press.
                    RoomJoin::phase=RoomJoin::Phase::Reopen;
                    m.Write(Owner+0x54,0);m.Write(Owner+0x10,1);r[15]=0x4336f8;return true;
                }
                // A keyboard cancellation and B on the waiting screen both
                // return to the ordinary file list, with its existing entrance.
                const unsigned selected=RoomJoin::slot;
                RoomJoin::Reset();Reset();
                slot=3;modal=false;awaiting_name=false;confirmed=false;wait_release=false;
                returning=true;block_cancel=true;
                started_us=now_us;motion_us=0;
                m.Write(Owner+0x14,selected);
                // The hidden native entrance was shortened. Settle its actual
                // card positions before applying the shared return animation.
                for(unsigned i=0;i<3;++i) {
                    for(U32 base:{0x55b5e0u,0x55b5f8u})m.Write(base+i*4,std::bit_cast<U32>(16.f));
                    for(U32 base:{0x55b5ecu,0x55b604u})m.Write(base+i*4,std::bit_cast<U32>(48.f+52*i));
                    m.Write(0x55b610+i*4,0);m.Write(0x55b61c+i*4,0);
                }
                // Re-anchor names/dates too; the shared presentation transform
                // then slides every part of each card from these resting poses.
                r[15]=FilePresentation;return true;
            }
        }
        if(state!=3) {
            if(generating) { files.Cancel(); generating=false; }
            if(awaiting_name && state==5) {
                // Our stones have already left. Finish the native six-step
                // departure without flashing the old file cards back onscreen.
                m.Write(Owner+0x54,0);m.Write(Owner+0x28,5);return false;
            }
            modal=false;confirmed=false;block_cancel=false;awaiting_name=false;
            Reset(state==8,state==8 || state==10 || state==11);
            cursor_us=std::max(std::int64_t{0},now_us);return false;
        }
        U32 pressed=m.Read(0x50af18).value_or(0);
        if(!pressed)pressed=m.Read(0x50af14).value_or(0);
        // Read physical shoulders before gameplay remapping. ZL/ZR do not
        // otherwise reach native OoT input when shoulder swap is disabled.
        // Prefer this held state over native repeats to avoid a delayed second
        // toggle after release. Either/both still count once per hold.
        const auto physical_shoulders=ControlSwap::file_menu_shoulders.load();
        const bool shoulder_down=physical_shoulders<0?(pressed&0x300)!=0:physical_shoulders!=0;
        const bool toggle=shoulder_down && !toggle_down;
        toggle_down=shoulder_down;
        const bool join=(pressed&0x400) && !x_down;
        x_down=(pressed&0x400)!=0;
        if(hop_active) {
            if(now_us<hop_started_us)hop_started_us=now_us;
            hop_us=std::min(now_us-hop_started_us,HopUpUs+HopDownUs);
            if(hop_us.load()==HopUpUs+HopDownUs)hop_active=false;
        }
        if(returning.load() && !modal) {
            if(now_us<started_us)started_us=now_us;
            motion_us=now_us-started_us;
            if(motion_us.load()<SlideUs+2*StaggerUs) {r[15]=FilePresentation;return true;}
            returning=false;block_cancel=true;
        }
        if(block_cancel) {
            if(pressed&3) {r[15]=0x4336f8;return true;}
            block_cancel=false;
        }
        if(!modal)return false;
        if(loading_tick_us && now_us>=loading_tick_us) {
            const float step=float(now_us-loading_tick_us)/150000.f;
            loading_alpha=std::clamp(loading_alpha.load()+(loading.load()?step:-step),0.f,1.f);
        }
        loading_tick_us=now_us;
        if(now_us<started_us)started_us=now_us;
        motion_us=now_us-started_us;
        if(departing_files.load()) {
            if(motion_us.load()>=SlideUs+2*StaggerUs) {
                departing_files=false;started_us=now_us;motion_us=0;
            }
            r[15]=0x4336f8;return true;
        }
        if(cancelling.load()) {
            if(motion_us.load()>=SlideUs+2*StaggerUs) {
                modal=false;slot=3;Reset();block_cancel=true;
            }
            r[15]=0x4336f8;return true;
        }
        if(confirmed) {
            if(motion_us.load()>=SlideUs+StaggerUs && loading_alpha.load()<=0) {
                // Select already sounded on the press. Resume AFTER that call,
                // with this frame's live stack, to start native name entry.
                modal=false;confirmed=false;slot=3;awaiting_name=true;loading_label=false;
                r[4]=r[7]=0;r[15]=Sites[4].pc;return true;
            }
            r[15]=0x4336f8;return true;
        }
        ready=!generating && motion_us.load()>=SlideUs+StaggerUs;
        U32 navigation=m.Read(0x50af30).value_or(0);
        if(!navigation)navigation=m.Read(0x50af2c).value_or(0);
        navigation|=pressed;
        const unsigned click=pointer_action.exchange(0);
        if(!(pressed&3))wait_release=false;
        if(generating && ((pressed&2) || click==3)) {
            files.Cancel(); generating=false; wait_release=true;
            loading=false;loading_label=false;leaving=false;generation_return=true;
            ready=false;started_us=now_us;motion_us=0;
            sound_only=Sound::Back;r[15]=0x430138;return true;
        }
        if(generating || (!wait_release && ready.load())) {
            if((pressed&2) || click==3) {
                mq_error=false;ClearNotice();
                files.Cancel();
                cancelling=true;returning=true;leaving=true;ready=false;started_us=now_us;motion_us=0;
                sound_only=Sound::Back;r[15]=0x430138;return true;
            }
            if(!generating && join && Randomizer::FileStatus::enabled.load() && slot<3 && !Occupied(m,slot)) {
                files.Cancel();RoomJoin::Begin(slot,choice);mq_error=false;ClearNotice();
                confirmed=true;ready=false;leaving=true;generation_return=false;
                started_us=now_us;motion_us=0;
                sound_only=Sound::Select;r[3]=m.Read(0x430458).value_or(0);
                r[15]=Sites[2].pc+4;return true;
            }
            if(!generating && toggle && !(pressed&1) && !click) {
                mq_error=false;ClearNotice();
                if constexpr(requires {files.ToggleRandomizer();})files.ToggleRandomizer();
                hop_started_us=now_us;hop_us=0;hop_active=true;
                // Same native audio call/return as the cursor cue, with a wood
                // strike and quieter volume. Preserve pitch and reverb.
                r[0]=RandomizerToggleSound;r[1]=0;r[2]=4;
                r[3]=m.Read(0x43045c).value_or(0);
                m.Write(r[13],RandomizerToggleVolume);
                m.Write(r[13]+4,m.Read(0x430458).value_or(0));
                sound_only=Sound::Move;r[15]=0x4301c4;return true;
            }
            const unsigned previous=choice;
            if(!generating && (navigation&(0x20|0x20000000)))choice=0;
            if(!generating && (navigation&(0x10|0x10000000)))choice=1;
            if(!generating && (click==1 || click==2))choice=click-1;
            highlighted=choice;
            if(choice!=previous) {mq_error=false;ClearNotice();}
            if(generating || (pressed&1) || click==1 || click==2) {
                if(slot<3 && !Occupied(m,slot)) {
                    if(!generating)ClearNotice();
                    if constexpr(requires {files.RejectChoice(choice!=0);}) {
                        if(!generating && files.RejectChoice(choice!=0)) {
                            mq_error=true;shake_started_us=now_us;shake_us=0;wait_release=true;
                            // Reuse the cursor audio call and its guarded return,
                            // preserving the live file-menu stack and registers.
                            r[0]=RejectionSound;r[1]=0;r[2]=4;
                            r[3]=m.Read(0x43045c).value_or(0);
                            m.Write(r[13],r[3]);m.Write(r[13]+4,m.Read(0x430458).value_or(0));
                            sound_only=Sound::Move;r[15]=0x4301c4;return true;
                        }
                    }
                    mq_error=false;
                    const int result=files.Prepare(m,slot,choice!=0);
                    const bool was_generating=generating;
                    generating=result==0;
                    if(result<=0) {
                        if(result==0 && !was_generating) {
                            loading=true;loading_label=true;leaving=true;generation_return=false;
                            ready=false;started_us=now_us;motion_us=0;
                            // Acknowledge the quest press immediately, just as
                            // for an ordinary file. Completion has its own cue.
                            sound_only=Sound::Select;r[3]=m.Read(0x430458).value_or(0);
                            r[15]=Sites[2].pc+4;return true;
                        }
                        if(result<0) {
                            files.Cancel();wait_release=true;loading=false;loading_label=false;
                            // Restore the choices on the failure frame so the
                            // rejection shake is visible, not spent offscreen.
                            leaving=false;generation_return=false;ready=true;loading_alpha=0;
                            started_us=now_us-SlideUs-StaggerUs;motion_us=SlideUs+StaggerUs;
                            const auto event=Randomizer::FileStatus::LatestNotice();
                            const auto error=event && event!=seen_notice?event->error:FileError::Generation;
                            seen_notice=event;
                            ShowNotice(error,now_us,state,selected,action,true);
                            r[0]=RejectionSound;r[1]=0;r[2]=4;r[3]=m.Read(0x43045c).value_or(0);
                            m.Write(r[13],r[3]);m.Write(r[13]+4,m.Read(0x430458).value_or(0));
                            sound_only=Sound::Move;r[15]=0x4301c4;return true;
                        }
                        r[15]=0x4336f8;return true;
                    }
                    SetMode(m,choice);confirmed=true;ready=false;leaving=true;
                    loading=false;generation_return=false;
                    if(!was_generating) {started_us=now_us;motion_us=0;}
                    // Execute the exact native Select sound block, then return
                    // to the geometry tail while the chosen stones slide out.
                    sound_only=Sound::Select;r[3]=m.Read(0x430458).value_or(0);
                    r[15]=Sites[2].pc+4;return true;
                }
            } else if(choice!=previous) {
                sound_only=Sound::Move;r[15]=0x4301a8;return true;
            }
        }
        r[15]=0x4336f8;return true;
    }
    template<class M,class R,class Files=OrdinaryFiles> void Handle(M& m,R& r,unsigned site,std::int64_t now_us=0, Files files={}) {
        if(site==0) {
            r[0]=Byte(m,r[0]);
            if(r[14]==0x348104 || r[14]==0x18cd34 || r[14]==0x1daaf4)r[0]=0;
        } else if(site==1) {
            // Both modes use the same three physical files; gameplay retains
            // the native per-save mode marker and System mode flags.
            r[0]=0;r[15]=r[14];
        } else if(site==2) {
            const unsigned selected=m.Read(Owner+0x14).value_or(3);
            if(selected<3 && !Occupied(m,selected)) {
                slot=selected;choice=0;modal=true;wait_release=true;confirmed=false;
                awaiting_name=false;started_us=now_us;motion_us=0;leaving=false;ready=false;
                cancelling=false;returning=false;departing_files=true;
                highlighted=0;pointer_action=0;choosing=true;sound_only=Sound::Select;
                // Keep the native Select sound that this hook used to skip.
            } else if(selected<3) {
                const bool opened=[&] {
                    if constexpr(requires {files.OpenForManagement(m,selected);})return files.OpenForManagement(m,selected);
                    else return files.Open(m,selected);
                }();
                if(!opened) {r[15]=0x4336f8;return;}
                SetMode(m,Mode(m,selected));
            }
            r[3]=m.Read(0x430458).value_or(0);
        } else if(site==3) {
            if(r[1]<3) {
                if(!files.Start(m,r[1])) {
                    // The BL has not executed. Return through the native caller's
                    // normal epilogue, leaving the occupied file untouched.
                    r[15]=Sites[3].pc+4;return;
                }
                SetMode(m,Mode(m,r[1]));
                RoomJoin::host_started=true;
            }
            r[14]=Sites[3].pc+4;r[15]=0x44735c;
        } else if(site==9) {
            ClearNotice();
            const unsigned selected=m.Read(Owner+0x14).value_or(3);
            if constexpr(requires {files.CheckStart(m,selected);}) {
                if(selected<3) {
                    if(start_wait_release) {r[15]=FilePresentation;return;}
                    const auto error=files.CheckStart(m,selected);
                    if(error!=FileError::None) {
                        start_error=error;start_error_slot=selected;
                        start_shake_started_us=now_us;start_shake_us=0;start_wait_release=true;
                        r[0]=RejectionSound;r[1]=0;r[2]=4;r[3]=m.Read(0x43045c).value_or(0);
                        m.Write(r[13],r[3]);m.Write(r[13]+4,m.Read(0x430458).value_or(0));
                        sound_only=Sound::Move;r[15]=0x4301c4;return;
                    }
                }
            }
            starting=selected<3 && (RoomJoin::hosting.load() || RoomJoin::guest==RoomJoin::Guest::Starting);
            start_error=FileError::None;r[3]=m.Read(0x430458).value_or(0);
        } else if(site==7) {
            r[1]=constructing_prompt_message=PromptMessage();
        } else if(site==8) {
            // The worker may advance while native text construction runs.
            // Remember the message actually built so the next frame can replace it.
            m.Write(r[1]+0x18,r[0]);prompt_controller=r[0];prompt_message=constructing_prompt_message;
        } else {
            const Sound expected=site==4?Sound::Select:site==5?Sound::Move:Sound::Back;
            if(sound_only==expected) {
                sound_only=Sound::None;r[15]=0x4336f8;return;
            }
            // Ordinary native file selection/navigation remains unchanged.
            if(site==4)r[5]=m.Read(0x430464).value_or(0);
            if(site==5)r[0]=m.Read(r[6]+0x14).value_or(0);
            if(site==6)r[1]=m.Read(0x42fdf4).value_or(0);
        }
    }
};
}
