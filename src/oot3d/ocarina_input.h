// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_state.h"
#include "oot3d/title_hooks.h"
#include "oot3d/input.h"
#include "oot3d/ocarina_layout.h"
namespace OOT3D::Ocarina {
using TitleScreen::U32;
constexpr U32 Owner=0x5093e4,FirstSVC=0xfff150;
constexpr U32 Message=0x5c1878;
// The title remains owned by Owner+0x10 throughout the Back slide. The
// native state-11 cleanup releases it before creating the playable prompt.
inline std::atomic<U32> back_title{0};
inline std::atomic<bool> back_title_visible{false};
template<class M> bool BackTitle(M& m) {
    const auto title=back_title.load(),state=m.Read(Owner+0x14).value_or(0);
    return title && (state==10 || state==11) && m.Read(Owner+0x10)==title;
}
constexpr std::array<TitleScreen::Site,10> Sites{{
    {0x425b08,0xe5950014,0xe3500011}, // Native ocarina update after touch sampling.
    {0x425ca4,0xe320f000,0xebfc5648}, // Song navigation, before native key repeat.
    {0x425cd8,0xe5950018,0xe2500001}, // After native cursor movement sound.
    {0x41ee74,0xeb001e33,0xe320f000}, // Bottom-screen ocarina draw.
    {0x426768,0xe12fff31,0xe5940038}, // Atlas draw, before cursor and song name.
    {0x42661c,0xe3a00005,0xe5850038}, // Playable-notes cleanup finished; start list entrance.
    {0x426640,0xe3a00004,0xe5850014}, // Native list entrance finished.
    {0x42cba8,0xe5d0100c,0xe3510000}, // Message draw, before any pane/text submission.
    {0x474118,0xebfb0bcb,0xea000007}, // Initialize the native song recognizer.
    {0x238664,0xed940a06,0xeb047444}, // Item camera's lens target, before native approach.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return m.Read(0x355780)==std::optional<U32>{0xee302a60} &&
           m.Read(0x2386c0)==std::optional<U32>{0x3f800000} &&
           m.Read(0x425ef0)==std::optional<U32>{Owner} &&
           m.Read(0x42679c)==std::optional<U32>{Owner} &&
           m.Read(0x4263c8)==std::optional<U32>{0x6016} &&
           m.Read(0x426408)==std::optional<U32>{0xe5950010} &&
           m.Read(0x426190)==std::optional<U32>{0xe5950010} &&
           m.Read(0x4261b4)==std::optional<U32>{0xe595000c} &&
           m.Read(0x42659c)==std::optional<U32>{0xe5950010};
}
template<class M,class R,class F> void NativeCameraLens(M& m,R& r,F& fp,bool supported) {
    const U32 cam=r[4];
    fp[0]=m.Read(cam+0x18).value_or(0); // Displaced VLDR: retain the native target.
    if(!supported || !TitleScreen::Pointer(cam,0x1bc))return;
    const U32 play=m.Read(cam+0xd4).value_or(0),player=m.Read(cam+0xd8).value_or(0);
    // The item camera is shared by other scripted views. Only change the
    // main ocarina view captured at setting 0x38, normal mode, status 7.
    if(!TitleScreen::Pointer(play,0xa68) || cam!=play+0x364 ||
       m.Read(0x5043e0)!=play || (m.Read(play+0xa64).value_or(~0u)&0xffff)!=0 ||
       m.Read(cam+0x188)!=std::optional<U32>{7u|(0x38u<<16)} ||
       (m.Read(cam+0x18c).value_or(~0u)&0xffff)!=0 ||
       !TitleScreen::Pointer(player,0x1718) ||
       !(m.Read(player+0x1714).value_or(0)&0x08000000u))return;
    // 355780 snaps to the target when the remaining error is below s3.
    // The stock one-degree cutoff caused the captured 45.996773 -> 45 cut.
    // Keep the native rate/target and continue easing to a subpixel cutoff.
    if(fp[3]==std::bit_cast<U32>(1.f))fp[3]=std::bit_cast<U32>(.01f);
}
template<class M> U32 State(M& m) {
    const U32 play=m.Read(0x5043d4+0xc).value_or(0);
    if(!TitleScreen::Pointer(play,0x7f28) ||
       (m.Read(play+0x100).value_or(0)&0xffff)!=0x0203 ||
       (m.Read(play+0x5c2c).value_or(0)&0xff00) ||
       (m.Read(play+0x7f10).value_or(0)&0xff0000))return 0;
    const auto state=m.Read(Owner+0x14).value_or(0);
    return state<=16?state:0;
}
inline bool List(U32 state) {
    return state==1 || state==2 || state==3 || state==4 || state==6 || state==7 || state==10 || state==14;
}
template<class M> bool FreePlay(M& m,U32 play) {
    // Action 0 is learned-song recognition; action 1 is native free play.
    // Both use instruction message 086e (3438a4). Lessons, recordings,
    // frog games and scripted song checks use other actions.
    // The bottom-screen song-list state and its availability flag are not
    // the instrument's mode. They can be inactive during native free play
    // (including its startup), and must not gate the audio recognizer or
    // the top-screen message. Validate the gameplay owner and action itself.
    return TitleScreen::Pointer(play,0x2b84) &&
        m.Read(0x5043d4+0xc)==std::optional<U32>{play} &&
        (m.Read(play+0x100).value_or(0)&0xffff)==0x0203 &&
        (m.Read(play+0x2b80).value_or(~U32{0})&0xffff)<=1;
}
template<class M> bool ContinuousPlaying(M& m,U32 play) {
    if(FreePlay(m,play))return true;
    if(!TitleScreen::Pointer(play,0x2b84) ||
       m.Read(0x5043d4+0xc)!=std::optional<U32>{play} ||
       (m.Read(play+0x100).value_or(0)&0xffff)!=0x0203)return false;
    const U32 action=m.Read(play+0x2b80).value_or(~U32{0})&0xffff;
    // Native location checks for the six non-warp songs and Scarecrow,
    // free-play completion, and non-warp-only playing/completion (30/31).
    // These share instruction 086e (343b6c / 4740fc)
    // and the learned-song recognizer with free play. In particular 0024 is
    // the Lullaby check used at the fairy fountain. Retain the native action
    // so the actor still decides whether the recognized song is correct.
    // Lessons, playback, recording, memory/frog games stay native.
    return (action>=0x22 && action<=0x29) || action==0x30 || action==0x31;
}
template<class M> bool SongCompleted(M& m) {
    const U32 play=m.Read(0x5043d4+0xc).value_or(0);
    const U32 phase=m.Read(play+0x2a90).value_or(0)&255;
    return ContinuousPlaying(m,play) && (phase==0xd || (phase>=0x11 && phase<=0x17));
}
// Result/playback notes are native message panes, separate from the song list.
// Scope the sampler change to their synchronous draw; restore it at the model
// hook after sampler submission, before any other pane can reuse the material.
struct PlaybackNoteFilter {
    U32 mesh=0,mag=0,min=0;
    template<class M> void Restore(M& m) {
        if(mesh) {m.Write(mesh+0x13c,mag);m.Write(mesh+0x140,min);mesh=0;}
    }
    template<class M> void Material(M& m,U32 renderer) {
        Restore(m);
        if(m.Read(Message)!=0x4ed0a0)return;
        const U32 model=m.Read(renderer+0x354).value_or(0);
        if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=0x4ebd60)return;
        bool note=false;
        for(unsigned i=0;i<8;++i) {
            const U32 pane=m.Read(Message+0x7c0+4*i).value_or(0);
            if(TitleScreen::Pointer(pane,0xb4) && m.Read(pane)==0x4ed234 &&
               m.Read(pane+4)==Message+0x168 &&
               (m.Read(pane+0x64)==model || m.Read(pane+0x68)==model)) {note=true;break;}
        }
        if(!note)return;
        const U32 candidate=m.Read(renderer+0x350).value_or(0);
        if(!TitleScreen::Pointer(candidate,0x19c))return;
        const auto a=m.Read(candidate+0x13c),b=m.Read(candidate+0x140);
        if(!a || !b || *a>1 || *b>5)return;
        mesh=candidate;mag=*a;min=*b;
        m.Write(mesh+0x13c,1);m.Write(mesh+0x140,3);
    }
};
template<class M> bool HidePlaybackNote(M& m,U32 model,bool supported) {
    if(!supported || m.Read(Message)!=std::optional<U32>{0x4ed0a0} ||
       m.Read(Message+0xf40)!=std::optional<U32>{0x86f})return false;
    const U32 play=m.Read(0x5043d4+0xc).value_or(0);
    if(!ContinuousPlaying(m,play))return false;
    const U32 phase=m.Read(play+0x2a90).value_or(0)&255;
    // Recognition switches to result message 086f before clearing the played
    // notes. They otherwise flash white during phases 17/18. Native 474c2c
    // clears the staff, and phase 19 reveals playback notes one at a time.
    // Hide only those eight note panes; keep the staff and message visible.
    if(phase!=17 && phase!=18)return false;
    if(!TitleScreen::Pointer(model,0x1e4) ||
       m.Read(model)!=std::optional<U32>{0x4ebd60})return false;
    for(unsigned i=0;i<8;++i) {
        const U32 pane=m.Read(Message+0x7c0+4*i).value_or(0);
        if(!TitleScreen::Pointer(pane,0xb4) ||
           m.Read(pane)!=std::optional<U32>{0x4ed234} ||
           m.Read(pane+4)!=std::optional<U32>{Message+0x168})continue;
        if(m.Read(pane+0x64)==model || m.Read(pane+0x68)==model)return true;
    }
    return false;
}
template<class M,class R> void DrawMessage(M& m,R& r,bool supported) {
    r[1]=m.Read(r[0]+0xc).value_or(0)&255; // Displaced LDRB.
    const U32 play=m.Read(0x5043d4+0xc).value_or(0);
    // Suppress only the playing instructions/live staff. Keep the message
    // object updating: recognition, song results, warp choices and B cleanup
    // still belong to native code. Stable IDs work with every language.
    if(supported && r[0]==Message && ContinuousPlaying(m,play) &&
       m.Read(Message)==std::optional<U32>{0x4ed0a0} && m.Read(Message+0xf40)==0x86e)
        r[15]=r[14];
}
template<class M,class R> void StartRecognition(M& m,R& r,bool supported) {
    // Audio update 460858 chooses 47af90 when bit 0x4000 is set: a rolling
    // eight-note suffix matcher. Other bits remain the native learned-song
    // mask, including Scarecrow availability. No host note buffer is needed.
    if(supported && ContinuousPlaying(m,r[5]) && (m.Read(r[5]+0x2a90).value_or(0)&255)==9)
        r[0]|=0x4000;
    r[14]=Sites[8].pc+4;r[15]=0x33704c; // Displaced BL, unchanged arguments otherwise.
}
template<class M> bool Ready(M& m) {
    const auto state=State(m);
    return (state==4 || state==12) && m.Read(Owner+0x44)==std::optional<U32>{1};
}
struct Input {
    enum class Action {None,Songs,Back,Cancel};
    U32 previous=0,consumed=0;
    Action pending=Action::None;
    bool navigation_sound=false;
    struct Move {unsigned from{},to{};std::int64_t time{};};
    struct Edge {U32 directions{};std::int64_t time{};};
    static constexpr unsigned MaxEdges=8; // HID samples at 234 Hz, native navigation at 30.
    std::array<Move,MaxEdges> moves{};
    std::array<Edge,MaxEdges> edges{};
    unsigned move_count{},edge_count{};
    U32 held_directions{},stick_directions{};
    std::int64_t stick_edge_time{};
    bool sample_directions=false;
    void PollDirections(U32 directions,U32 stick,std::int64_t now) {
        const U32 pressed=directions & ~held_directions;
        held_directions=directions;
        const bool stick_pressed=(stick & ~stick_directions)!=0;
        stick_directions=stick;
        if(!sample_directions || !now) {
            edge_count=move_count=0;stick_edge_time=0;return;
        }
        // HID and the native circle-pad menu use different thresholds. Use HID
        // only to date the snap. The game chooses the direction (including MQ)
        // and owns both the initial stick step and every held-stick repeat.
        if(stick_pressed)stick_edge_time=now;
        if(pressed && edge_count<edges.size()) {
            edges[edge_count++]={pressed,now};
        }
    }

    template<class M> void Filter(M& m,U32& buttons,bool live,unsigned triggers=0) {
        constexpr U32 Start=8,B=2;
        const U32 raw=buttons,edges=raw&~previous;previous=raw;
        consumed&=raw;
        sample_directions=false;
        if(!live || !CodeMatches(m)) {pending=Action::None;consumed=0;return;}
        const auto state=State(m);
        sample_directions=state==4 && on_top.load() && Interpolation::Replaying() &&
            Interpolation::TargetFPS()>30 && m.Read(Owner+0x44)==1;
        if(state) {
            // Extra HID reports ZL/ZR separately. Alias them to native L/R
            // while the instrument owns input; retain physical L/R as well.
            if(triggers&1)buttons|=0x200; // ZL -> L.
            if(triggers&2)buttons|=0x100; // ZR -> R.
            consumed|=raw&Start; // Never let ocarina Start reach Save.
            if((edges&Start) && state==12 && Ready(m) && m.Read(Owner+0x40)==std::optional<U32>{1})
                pending=Action::Songs;
            if(List(state)) {
                consumed|=raw&B; // Retain through the return transition, until release.
                if((edges&B) && state==4 && Ready(m) && m.Read(Owner+0x40)==std::optional<U32>{1})
                    pending=Action::Back;
            } else if(state==12 && Ready(m) && m.Read(Owner+0x3c)==std::optional<U32>{1}) {
                consumed|=raw&B;
                if(edges&B)pending=Action::Cancel;
            }
        } else pending=Action::None;
        buttons&=~consumed;
    }
    template<class M,class R> void Update(M& m,R& r) {
        r[0]=m.Read(r[5]+0x14).value_or(0); // Displaced LDR.
        navigation_sound=false;
        const auto action=pending;pending=Action::None;
        if(r[5]==Owner && back_title.load()) {
            if(!BackTitle(m)) {back_title=0;back_title_visible=false;}
            else if(r[0]==11 && back_title_visible.load()) {
                r[15]=0x425b80;return; // Finish the visible slide before cleanup.
            }
        }
        if(r[5]!=Owner || !Ready(m))return;
        if(action==Action::Cancel && r[0]==12 && m.Read(Owner+0x3c)==std::optional<U32>{1}) {
            // The native Quit acceptance at 4262f0 sets play+6016. The player
            // consumes this request and owns instrument/audio/gameplay cleanup.
            if(m.Read(0x5043d4+0xc)!=std::optional<U32>{r[4]})return;
            const U32 at=r[4]+0x6014;
            if(const auto word=m.Read(at))m.Write(at,(*word&~0x00ff0000u)|0x00010000u);
            return;
        }
        if(m.Read(Owner+0x40)!=std::optional<U32>{1})return;
        // Enter the ACCEPTED native toggle branches directly. They own text
        // cleanup, sound, pane transitions and restoration of playable notes.
        if(action==Action::Songs && r[0]==12)r[15]=0x426408;
        if(action==Action::Back && r[0]==4) {
            r[15]=0x426190;
            if(on_top.load() && m.Read(Owner+0x10).value_or(0)) {
                back_title=m.Read(Owner+0x10).value_or(0);back_title_visible=true;
                r[15]=0x4261b4; // Keep title ownership; run native sound/cursor/exit setup.
            }
        }
    }
    template<class M,class R> void BeginList(M& m,R& r,bool enabled) {
        r[0]=5; // Displaced MOV.
        if(!enabled || r[5]!=Owner || State(m)!=13 ||
           m.Read(Owner+0x40)!=std::optional<U32>{1})return;
        // The native resource/notes cleanup has completed. Evaluate the final
        // native entrance pose once; our presentation owns the visible slide.
        m.Write(Owner+0x38,0);r[15]=0x42662c;
    }
    template<class M,class R> void FinishList(M& m,R& r,bool enabled) {
        r[0]=4; // Displaced MOV.
        const auto state=State(m);
        if(!enabled || r[5]!=Owner || (state!=13 && state!=14) ||
           m.Read(Owner+0x40)!=std::optional<U32>{1})return;
        m.Write(Owner+0x14,4);m.Write(Owner+0x28,0);
        m.Write(Owner+0x20,~U32{0}); // Recreate the selected song's native title.
        // The artwork routine also finalizes a shared size template while
        // visiting the stones. Finish that pass before the full list refresh
        // so even the first stones use their settled dimensions on frame one.
        // Then refresh title/cursor/notation without polling input or playing.
        r[14]=0x425e40;r[15]=0x2f663c;
    }
    template<class M,class R> void Navigate(M& m,R& r,bool enabled,std::int64_t now=0) {
        if(!enabled || r[5]!=Owner || State(m)!=4)return; // Displaced NOP.
        auto x=m.Read(Owner+0x18).value_or(4),y=m.Read(Owner+0x1c).value_or(3);
        if(x>=4 || y>=3)return;
        const U32 pad=m.Read(0x50af0c+0x24).value_or(0);
        const U32 keys=(pad?pad:m.Read(0x50af0c+0x20).value_or(0))&0xf0;
        const unsigned columns=GetLayout().Columns(),rows=12/columns;
        const unsigned old=x+4*y;
        unsigned next=old;
        move_count=0;
        const auto advance=[&](U32 direction,std::int64_t time) {
            unsigned col=next%columns,row=next/columns;
            if(direction&0x20)col=(col+columns-1)%columns;
            else if(direction&0x10)col=(col+1)%columns;
            else if(direction&0x40)row=(row+rows-1)%rows;
            else if(direction&0x80)row=(row+1)%rows;
            else return;
            const unsigned stone=row*columns+col;
            moves[move_count++]={next,stone,time};
            next=stone;
        };
        if(sample_directions && now) {
            for(unsigned i=0;i<edge_count;++i)
                if(edges[i].time<=now && now-edges[i].time<=50000)
                    advance(edges[i].directions,edges[i].time);
            edge_count=0;
            if(!move_count) {
                if(pad) {
                    // +0x0c holds new button presses. Those already came from
                    // HID, while repeat pulses appear only in +0x24.
                    advance(keys & ~m.Read(0x50af0c+0x0c).value_or(0),now);
                } else if(keys) {
                    const auto time=stick_edge_time>0 && stick_edge_time<=now &&
                        now-stick_edge_time<=50000?stick_edge_time:now;
                    advance(keys,time);
                    stick_edge_time=0;
                }
            }
        } else advance(keys,now);
        m.Write(Owner+0x18,next%4);m.Write(Owner+0x1c,next/4);
        navigation_sound=move_count!=0;
        r[15]=navigation_sound?0x425cb8:0x425dcc;
    }
    template<class M,class R> void AfterSound(M& m,R& r) {
        r[0]=m.Read(r[5]+0x18).value_or(0);
        if(navigation_sound) {navigation_sound=false;r[15]=0x425dcc;}
    }
};
inline Input input;
}
