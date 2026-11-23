// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include "oot3d/randomizer/activity.h"
#include "oot3d/screen_model_hooks.h"
#include "oot3d/boss_challenge_layout.h"
#include "oot3d/save_menu.h"
#include "oot3d/atlas_seams.h"
#include "oot3d/guest_ui_mesh.h"
#include "oot3d/widescreen.h"
#include <atomic>
#include <mutex>

namespace OOT3D::DeathMenu {
using TitleScreen::U32;
enum Part : unsigned {Prompt,Save,DontSave,Count};
inline constexpr std::array Names{"prompt","save","dont_save"};
constexpr std::array Labels{"Prompt box and text","Save stone","Don't save stone"};
constexpr std::array<std::array<float,2>,Count> Anchors{{{160,24},{224,120},{96,120}}};
struct Pose {float x=0,y=0,scale=1;};
using Layout=std::array<Pose,Count>;
inline Layout layout{},touch_layout{};
inline std::mutex layout_mutex;
inline std::atomic<bool> dirty{false};
inline Layout GetLayout() {std::scoped_lock lock{layout_mutex};return layout;}
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{layout_mutex};layout=value;if(save)dirty=true;
}
inline bool Valid(const Pose& p) {
    return std::isfinite(p.x) && std::abs(p.x)<=400 && std::isfinite(p.y) &&
        std::abs(p.y)<=240 && std::isfinite(p.scale) && p.scale>=.1f && p.scale<=3;
}
constexpr U32 ControllerPointer=0x5c588c, LayoutOffset=0x2e0, PanesOffset=0xaf8;
constexpr U32 PaneResourcesOffset=LayoutOffset+0x18;
constexpr U32 FirstSVC=0xfff220;
constexpr std::array<TitleScreen::Site,13> Sites{{
    {0x300544,0xeb049d33,0xe2840dcb}, // Native top game-over draw.
    {0x2fde88,0x1bffccfc,0xe2844001}, // Bottom layout's conditional pane draw.
    {0x41c960,0xe3a03012,0xe3a02001}, // Title entrance finished: choose its next animation.
    {0x41c98c,0xeb0027b7,0xe320f000}, // Start the save-stone entrance.
    {0x494ed4,0xe59d0058,0xe3100001}, // Fresh copy of evaluated QAN channel flags.
    {0x41ce1c,0xebfb82b1,0xe3a00000}, // Before death layout evaluation.
    {0x41c784,0xe5d10100,0xe1a05001}, // After prologue, before death control update.
    {0x427000,0xeafb37ad,0x0100048e}, // Accepted Save-menu controller A.
    {0x427414,0xeafb36a8,0x0100048e}, // Accepted Continue-menu controller A/B.
    {0x42755c,0xebfb3656,0xe5940004}, // Accepted Boss Challenge A/B.
    {0x4275a8,0xeafb3643,0xe28dd008}, // Boss Challenge Start/back tail call.
    {0x426fe0,0x0affffcb,0xe5940010}, // Save question: no A, allow B to decline saving.
    {0x426ac8,0xe2865a01,0xe3500000}, // Save question setup, before testing Visions availability.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)if(pc==Sites[i].pc && word==Sites[i].instruction)
        return (word&0xf0000000)|0x0f000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return m.Read(0x2e71ac)==std::optional<U32>{0x5c55b8} &&
        m.Read(0x2e4fd8)==std::optional<U32>{0xe92d4ff0} &&
        m.Read(0x2d2a08)==std::optional<U32>{0xe2871018}; // Pane resources = layout + 0x18.
}
template<class M> U32 Controller(M& m) {
    const U32 owner=m.Read(ControllerPointer).value_or(0);
    return TitleScreen::Pointer(owner,0x1180)?owner:0;
}
template<class M> bool Active(M& m) {
    const U32 owner=Controller(m);
    if(!owner)return false;
    const U32 flags=m.Read(owner+8).value_or(0),state=flags&255;
    // Loaded does not mean open: the controller is resident during gameplay.
    return state>=1 && state<=17 && ((flags>>16)&255)!=0;
}
template<class M> bool Drawable(M& m,U32 owner) {
    return owner && owner==Controller(m) && Active(m) &&
        (m.Read(owner+8).value_or(0)>>24)==0 &&
        m.Read(owner+LayoutOffset+4).value_or(0)!=0;
}
// Native transition curves and selectable stone focus have separate lifetimes.
constexpr bool Moving(U32 state) {return state==3 || state==5 || state==9 || state==11 || state==13 || state==15;}
constexpr bool Selectable(U32 state) {return state==4 || state==10 || state==14;}
template<class M> bool FastPresentation(M& m) {
    const U32 owner=Controller(m);
    if(!Drawable(m,owner))return false;
    const auto state=m.Read(owner+8).value_or(0)&255;
    return Moving(state) || Selectable(state);
}
inline bool half_frame=false,extra_frame=false;
inline std::array<float,2> focus_scale{1,1}; // Indexed by the shared save-layout parts.
// Native QLY pane IDs, shared by the English/French/Spanish layouts.
// Selection corners follow their selected control, leaving Visions on bottom.
constexpr bool CursorPane(unsigned pane) {return (pane>=60 && pane<=63) || (pane>=65 && pane<=68);}
constexpr bool RoutedPane(unsigned pane,U32 selection) {
    return (pane>=5 && pane<=13) || (pane>=15 && pane<=23) ||
        (pane>=25 && pane<=33) || (pane>=45 && pane<=47) || pane==90 || pane==91 ||
        (selection<2 && CursorPane(pane));
}
constexpr Part PanePart(unsigned pane,U32 selection) {
    if((pane>=5 && pane<=13) || (pane>=15 && pane<=23) || pane==90 || pane==91)return Prompt;
    if(pane>=25 && pane<=29)return Save;
    if(pane>=30 && pane<=33)return DontSave;
    if((pane>=45 && pane<=47) || (pane>=60 && pane<=63) || (pane>=65 && pane<=68))
        return selection==0?Save:selection==1?DontSave:Count;
    return Count;
}
inline bool ValidRenderedPose(const Pose& p) {
    // Shared warning controls compose two valid scales/offsets, then the .8
    // base scale. Their rendered range is wider than any individual slider.
    return std::isfinite(p.x) && std::abs(p.x)<=1000 && std::isfinite(p.y) &&
        std::abs(p.y)<=1000 && std::isfinite(p.scale) && p.scale>=.008f && p.scale<=8;
}
inline ScreenModels::Transform PaneTransform(unsigned pane,U32 selection,const Layout& poses) {
    const auto part=PanePart(pane,selection);
    if(part==Count || !ValidRenderedPose(poses[part]))return {40,0};
    const auto p=poses[part];auto a=Anchors[part];
    if(part==Prompt && ((pane>=15 && pane<=23) || pane==91))a[1]=204;
    return {40+p.x,p.y,p.scale,p.scale,a[0],a[1],true};
}
// Save is projected from the bottom viewport at 80%, centered at (200,138)
// on top. Death's QLY draws directly in top-screen pixels instead. Compose
// that base transform with the SAME saved editor poses, including the warning.
inline Pose SharedPose(SaveMenu::Part part,const SaveMenu::Layout& poses) {
    const auto p=SaveMenu::Valid(poses[part]) &&
        (part!=SaveMenu::CardWarning || SaveMenu::Valid(poses[SaveMenu::Prompt]))?
        SaveMenu::ControlPose(part,poses):SaveMenu::Pose{};
    const auto a=SaveMenu::Anchors[part];
    return {p.x+.2f*(160-a[0]),18+p.y+.2f*(120-a[1]),.8f*p.scale};
}
inline Layout SharedLayout(const SaveMenu::Layout& poses) {
    return {SharedPose(SaveMenu::Prompt,poses),SharedPose(SaveMenu::Save,poses),
        SharedPose(SaveMenu::DontSave,poses)};
}
constexpr bool UpperPrompt(unsigned pane) {return (pane>=5 && pane<=13) || pane==90;}
constexpr bool Stone(unsigned pane) {return pane>=25 && pane<=33;}
constexpr bool PressedStone(unsigned pane) {return pane>=45 && pane<=47;}
constexpr U32 PromptResource(U32 state,bool saving,U32 current) {
    if(state==3 || state==4)return 200; // Save your progress?
    if(state>=5 && state<=7)return saving?201:204; // Saving... / Continue playing?
    if(state==8)return 201; // Keep Saving... until Continue/Quit start together.
    if(state==9)return saving?203:204;
    return current; // Continue/Quit retains the native saved/not-saved wording.
}
inline ScreenModels::Transform SharedTransform(unsigned pane,U32 selection,
                                               const SaveMenu::Layout& poses) {
    auto layout=SharedLayout(poses);
    const bool warning=(pane>=15 && pane<=23) || pane==91;
    if(warning)layout[Prompt]=SharedPose(SaveMenu::CardWarning,poses);
    return PaneTransform(pane,selection,layout);
}
// QLY's initial resource IDs are NOT the runtime selection: 426870 binds
// short resources 5..13; 2f4fac later binds tall resources 15..23. Assemble
// those same atlas slices as a complete box, so changing messages changes
// height immediately instead of animating the middle/bottom rows separately.
struct PromptGeometry {
    std::array<float,9*12> positions{};
    std::array<float,9*8> uvs{};
    std::array<float,9*16> colors{};
    unsigned count=0;
};
inline PromptGeometry MakePrompt(bool tall) {
    PromptGeometry g;
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col) {
        if(!tall && row==1)continue;
        const float x=col==0?-148.f:col==1?-124.f:124.f;
        const float y=row==0?(tall?-32.f:-20.f):row==1?-12.f:(tall?12.f:0.f);
        const float w=col==1?248.f:24.f,h=row==1?24.f:20.f;
        const float u=col==0?96.f:col==1?(row==1?111.f:118.f):120.f;
        const float du=col==0?24.f:col==1?(row==1?8.f:1.f):-24.f;
        const float v=row==0?0.f:row==1?(col==1?16.f:19.f):20.f;
        const float dv=row==1?(col==1?8.f:1.f):20.f;
        const unsigned q=g.count++;
        const std::array<float,12> p{x,y,0,x+w,y,0,x,y+h,0,x+w,y+h,0};
        std::array<float,8> uv{u/256,1-v/256,(u+du)/256,1-v/256,
            u/256,1-(v+dv)/256,(u+du)/256,1-(v+dv)/256};
        for(unsigned c=0;c<8;++c) {
            uv[c]=std::bit_cast<float>(AtlasSeams::SavePromptWord(22+row*3+col,c,std::bit_cast<U32>(uv[c])));
            // Vertical middle caps are a one-texel strip, just like the
            // horizontal middle of the top/bottom row.
            if(row==1 && col!=1 && c%2)uv[c]=1-19.5f/256;
        }
        std::copy(p.begin(),p.end(),g.positions.begin()+q*12);
        std::copy(uv.begin(),uv.end(),g.uvs.begin()+q*8);
        std::fill_n(g.colors.begin()+q*16,16,1.f);
    }
    return g;
}
inline SaveMenu::Animation Animation(U32 state,float ticks) {
    const float t=std::clamp(ticks/4.f,0.f,1.f); // Same 133ms as Save's eight 60Hz ticks.
    const float ease=1-(1-t)*(1-t)*(1-t);
    if(state==12)return {0,0,18}; // Boss Challenge prepares the prompt before entrance.
    if(state==3 || state==13)return {ease,ease,18*(1-ease)};
    if(state==9)return {ease,1,18*(1-ease)};
    if(state==5)return {1-ease,1,18*ease};
    if(state==11 || state==15)return {1-ease,1-ease,18*ease};
    if(state>=6 && state<=8)return {0,1,18};
    if(state==16 || state==17)return {0,0,18};
    return {};
}
inline std::atomic<bool> on_top{false};
inline std::optional<std::array<float,2>> MapPointer(float u,float v,float width,const Layout& poses) {
    if(!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(width) || width<=0)return {};
    const float x=(u-.5f)*width+160,y=v*240;
    for(const auto part:{DontSave,Save}) {
        const auto p=poses[part];if(!ValidRenderedPose(p))continue;
        const auto a=Anchors[part];
        const float nx=a[0]+(x-a[0]-p.x)/p.scale,ny=a[1]+(y-a[1]-p.y)/p.scale;
        if(nx>=a[0]-60 && nx<a[0]+60 && ny>=96 && ny<144)return std::array{nx/320,ny/240};
    }
    return {};
}
inline std::optional<std::array<float,2>> MapPointer(float u,float v,float width) {
    std::scoped_lock lock{layout_mutex};return MapPointer(u,v,width,touch_layout);
}
struct Hook {
    enum class Stage {Idle,Native,Panes};
    Stage stage=Stage::Idle;
    std::array<U32,16> saved{};
    std::array<U32,256> routed_panes{};
    Layout frame_layout{};
    SaveMenu::Layout save_layout{};
    bool shared_style=true;
    std::array<GuestUI::Hook<PromptGeometry,1,1,0,true>,2> prompts;
    std::array<bool,2> replaced{},attempted{};
    SaveMenu::Animation animation{};
    U32 state=0;
    struct SamplerRestore {U32 mesh=0,mag=0,min=0;};
    std::array<SamplerRestore,2> samplers{};
    U32 visible_pane=0,visible_flags=0;
    U32 owner=0,selection=0xffffffff;
    unsigned next=0,submitted=0,current_pane=0;
    bool route=false,native_top_draw=false;
    U32 fading_owner=0;
    std::array<U32,16> update_saved{};
    U32 update_owner=0,update_message=0;
    unsigned update_next=0;
    bool updating=false;
    U32 animation_owner=0,animation_state=0;
    float animation_ticks=0;
    template<class M,class R> void ControlUpdate(M& m,R& r) {
        // Only actual game-over with zero health, not Boss Rush success UI.
        Randomizer::Activity::ObserveDeath(Active(m) && (m.Read(0x587958+0x44).value_or(1)&0xffff)==0);
        r[0]=m.Read(r[1]+0x100).value_or(0)&255; // Displaced LDRB.
        if(half_frame && extra_frame && r[4]==Controller(m) && Drawable(m,r[4])) {
            // The selectable stone dialogs read newly pressed buttons.
            // Skipping its frame loses the edge even if the player keeps holding.
            if(Selectable(m.Read(r[4]+8).value_or(0)&255) && m.Read(r[4]+0x24)==0 &&
               m.Read(r[1]+0x18).value_or(0)!=0)return;
            // Pending touch callbacks and transition clocks advance on normal frames.
            m.Write(r[4]+4,r[1]);r[15]=0x41ce0c;
        }
    }

    std::array<U32,16> input_saved{};
    U32 input_control=0,input_site=0,input_stage=0;
    std::array<U32,2> input_stack{};
    template<class M,class R> void Input(M& m,R& r,unsigned site,bool supported) {
        if(site==12) {
            r[5]=r[6]+0x1000; // Displaced ADD. The following CMP tests availability in r0.
            // The native unavailable path hides the stone and clears its navigation flag.
            if(supported && r[6]==Controller(m))r[0]=0;
            return;
        }
        if(site==10) {
            // Boss Challenge tests logical Start after HID remapping. The
            // caller has restored LR and the stack before this tail call.
            r[15]=supported?r[14]:0x2f4ebc;
            return;
        }
        if(site==11) {
            r[15]=0x426f14; // Displaced BEQ: A was not pressed.
            const U32 menu=Controller(m),control=menu+0x113c;
            if(supported && r[4]==menu && Drawable(m,menu) &&
               (m.Read(menu+8).value_or(0)&255)==4 && (r[1]&2) &&
               (m.Read(control+0x18).value_or(0)&0xffff00)==0x10100 &&
               m.Read(menu+0x24).value_or(0)==0) {
                m.Write(menu+0x10,1);r[15]=0x426fe4; // Native Don't Save path.
            }
            return;
        }
        if(input_stage==0) {
            if(site==9)r[14]=Sites[site].pc+4; // Displaced BL, unlike the other tail calls.
            r[15]=0x2f4ebc; // Original call, including all unsupported cases.
            const U32 control=r[0],menu=Controller(m);
            const U32 state=m.Read(menu+8).value_or(0)&255;
            const U32 choice=m.Read(control+8).value_or(~0u);
            if(!supported || !Drawable(m,menu) || (state!=4 && state!=10 && state!=14) ||
               (choice>1 && !(choice==3 && (state==10 || state==14))) ||
               control!=menu+0x1120+choice*0x1c || m.Read(control+4)!=menu ||
               (m.Read(control+0x18).value_or(0)&0xffff00)!=0x10100 ||
               m.Read(menu+0x24).value_or(0)!=0 || r[13]<8 ||
               !m.Read(r[13]-8) || !m.Read(r[13]-4))return;
            input_saved=r;input_control=control;input_site=site;input_stage=1;
            r[0]=menu;r[14]=Sites[site].pc;
            r[15]=state==4?(choice==0?0x43c4a0:0x43c590):(choice==1?0x43c36c:0x43c67c);
            return;
        }
        if(input_stage==1) {
            const U32 sound=m.Read(input_control+0x14).value_or(~0u);
            if(!(sound&0x80000000)) {
                r=input_saved;r[13]-=8;
                input_stack={m.Read(r[13]).value(),m.Read(r[13]+4).value()};
                r[0]=sound;r[1]=0;r[2]=4;r[3]=m.Read(0x427a14).value_or(0);
                m.Write(r[13],r[3]);m.Write(r[13]+4,m.Read(0x427a10).value_or(0));
                r[14]=Sites[input_site].pc;r[15]=0x37547c;input_stage=2;return;
            }
        } else {
            m.Write(input_saved[13]-8,input_stack[0]);m.Write(input_saved[13]-4,input_stack[1]);
        }
        // Match the native touch callback's cleanup without synthesizing a
        // held button or leaving a pending touch target for the next screen.
        const U32 menu=m.Read(input_control+4).value_or(0);
        if(menu==Controller(m) && m.Read(menu+0x20))m.Write(menu+0x20,~0u);
        r=input_saved;r[15]=r[14];input_stage=0;
    }
    // Change presentation before native layout evaluation. Rebind using the
    // game's own helper; never copy a descriptor or retain another pane's
    // material ownership. The helper is integer-only (344670..3446d8), so the
    // caller's s0 layout delta survives every call.
    template<class M,class R> void Update(M& m,R& r,bool supported) {
        if(!updating) {
            update_saved=r;update_owner=r[0]-LayoutOffset;update_next=0;
            const U32 menu_state=m.Read(update_owner+8).value_or(0)&255;
            const U32 text=m.Read(update_owner+PanesOffset+90*4).value_or(0);
            if(!supported || !Drawable(m,update_owner) || menu_state<3 || menu_state==12 || menu_state>15 ||
               !TitleScreen::Pointer(text,0xb4) || m.Read(text+4)!=update_owner+PaneResourcesOffset) {
                r[14]=Sites[5].pc+4;r[15]=0x2fd8e8;return;
            }
            const U32 current=m.Read(text+0x78).value_or(0);
            update_message=PromptResource(menu_state,(m.Read(update_owner+0xc).value_or(0)&255)!=0,current);
            if(update_message<200 || update_message>205 || update_message==202) {
                r[14]=Sites[5].pc+4;r[15]=0x2fd8e8;return;
            }
            if(animation_owner!=update_owner || animation_state!=menu_state) {
                animation_owner=update_owner;animation_state=menu_state;animation_ticks=0;
            } else animation_ticks=std::min(4.f,animation_ticks+(half_frame?.5f:1.f));
            updating=true;
        }
        const bool tall=update_message>=203 && update_message<=205;
        while(update_next<10) {
            const unsigned i=update_next++,index=i==9?90:5+i;
            const U32 pane=m.Read(update_owner+PanesOffset+index*4).value_or(0);
            const U32 resource=i==9?update_message:(tall?15:5)+i;
            if(!TitleScreen::Pointer(pane,0xb4) || m.Read(pane+4)!=update_owner+PaneResourcesOffset)continue;
            const bool visible=i==9 || tall || i<3 || i>5;
            if(const auto flags=m.Read(pane+0x6c))m.Write(pane+0x6c,(*flags&~255u)|U32(visible));
            if(i!=9 && m.Read(pane+0x84))m.Write(pane+0x84,std::bit_cast<U32>(tall?36.f:24.f));
            // Empty short middle resources are deliberately never rebound.
            if(!visible || m.Read(pane+0x78)==resource)continue;
            const U32 asset=m.Read(update_owner+PaneResourcesOffset+4*resource).value_or(0);
            if(!TitleScreen::Pointer(asset,0x394))continue;
            bool valid=true;
            for(unsigned j=0;j<2;++j) {
                const U32 model=m.Read(pane+0x64+4*j).value_or(0);
                const U32 desc=m.Read(asset+(j?0x1bc:4)).value_or(0);
                valid&=TitleScreen::Pointer(model,0x1e4) && m.Read(model)==0x4ebd60 &&
                    TitleScreen::Pointer(m.Read(model+8).value_or(0),0x35c) &&
                    TitleScreen::Pointer(desc,0x18);
            }
            if(!valid)continue;
            r=update_saved;r[0]=pane;r[1]=resource;r[14]=Sites[5].pc;r[15]=0x344670;return;
        }
        r=update_saved;r[14]=Sites[5].pc+4;r[15]=0x2fd8e8;updating=false;
    }
    template<class M,class R> void PaneAnimation(M& m,R& r,bool supported) const {
        const U32 at=r[13]+0x58,pane=r[4],menu=Controller(m);
        U32 flags=m.Read(at).value_or(0);
        const U32 menu_state=m.Read(menu+8).value_or(0)&255;
        if(supported && Drawable(m,menu) && menu_state>=3 && menu_state<=15 &&
           TitleScreen::Pointer(pane,0xb4) && m.Read(pane+4)==menu+PaneResourcesOffset) {
            for(unsigned i:{5u,6u,7u,8u,9u,10u,11u,12u,13u,15u,16u,17u,18u,19u,20u,21u,22u,23u,
                           25u,26u,27u,28u,30u,31u,32u,33u,90u,91u}) {
                if(m.Read(menu+PanesOffset+i*4)!=pane)continue;
                // Remove resource, visibility, position, scale, rotation and
                // UV animation channels from this STACK COPY only. In
                // particular 0x14 scales the opening prompt from zero, while
                // 0x16/0x17 grow/reposition its middle and lower rows.
                flags&=~0x3ffu;
                // The pressed label also has a gray RGB animation; hiding
                // the separate pressed border alone does not remove it.
                if(Stone(i))flags&=~0x1c00u;
                m.Write(at,flags);break;
            }
        }
        r[0]=flags; // Displaced LDR r0,[sp,#0x58].
    }
    // Use the native title exit animation and its completion clock. The menu
    // remains in state 2, updating its layout normally, until the title is gone.
    template<class M,class R> void Transition(M& m,R& r,unsigned site,bool supported) {
        const U32 menu=r[4],play=m.Read(menu+4).value_or(0);
        const bool death=supported && menu==Controller(m) && Active(m) &&
            TitleScreen::Pointer(play,0x104) && (m.Read(play+0x100).value_or(0)&255)==3;
        const U32 title=m.Read(menu+LayoutOffset+0x41c).value_or(0);
        const bool valid_title=TitleScreen::Pointer(title,0xb4) &&
            m.Read(title+4)==menu+PaneResourcesOffset;
        if(site==2) {
            fading_owner=death && valid_title?menu:0;
            r[3]=fading_owner?0x13:0x12;
            return;
        }
        r[14]=Sites[3].pc+4;r[15]=0x426870; // Displaced BL, unchanged outside death.
        if(!death || !valid_title || fading_owner!=menu) {fading_owner=0;return;}
        const auto clock=m.Read(title+0x18),length=m.Read(title+0x1c);
        if(m.Read(title+0xc).value_or(0)!=0 && clock && length) {
            const float time=std::bit_cast<float>(*clock),end=std::bit_cast<float>(*length);
            if(std::isfinite(time) && std::isfinite(end) && time<end) {
                r[15]=Sites[3].pc+4;return;
            }
        }
        // Later native menu exits replay this title's exit animation. Keep the
        // completed title hidden so it cannot flash back over the save stones.
        if(const auto flags=m.Read(title+0x6c))m.Write(title+0x6c,*flags&~255u);
        fading_owner=0;
    }
    bool Drawing() const {return stage==Stage::Panes;}
    template<class M> std::optional<ScreenModels::Transform> NativeFadeTransform(M& m,U32 model) const {
        if(stage!=Stage::Native || !native_top_draw || !Widescreen::Active())return {};
        const U32 pane=m.Read(owner+LayoutOffset+0x418+250*4).value_or(0);
        if(!TitleScreen::Pointer(pane,0xb4) || m.Read(pane+4)!=owner+PaneResourcesOffset ||
           (m.Read(pane+0x64)!=model && m.Read(pane+0x68)!=model))return {};
        // The quit blackout belongs to this shared menu, not the boss selector.
        // Stretch its native quad, preserving the game's alpha and timing.
        return ScreenModels::Transform{0,0,240*Widescreen::Aspect/400,1,200,120,true};
    }
    template<class M> ScreenModels::Transform ModelTransform(M& m,U32 model) const {
        auto t=shared_style?SharedTransform(current_pane,selection,save_layout):
            PaneTransform(current_pane,selection,frame_layout);
        if(!shared_style)return t;
        const U32 pane=routed_panes[current_pane];
        // Cancel only the native entrance/exit translation, retaining the
        // actual short/tall resource geometry and the editor's final pose.
        const bool upper=(current_pane>=5 && current_pane<=13) || current_pane==90;
        const bool stone=(current_pane>=25 && current_pane<=33);
        if((upper || stone) && Moving(state)) {
            std::array<float,12> matrix;std::array<float,2> base;
            if(TitleScreen::ReadFloats(m,model+0xc,matrix) && TitleScreen::ReadFloats(m,pane+0x80,base)) {
                t.x+=(base[0]-matrix[3])*t.sx;
                t.y+=(base[1]-matrix[7])*t.sy;
                if(stone)t.y+=animation.rise*t.sy;
            }
        }
        return t;
    }
    template<class M> void Material(M& m,U32 at,U32 model) const {
        if(!Drawing() || !shared_style)return;
        const U32 pane=routed_panes[current_pane];
        if(m.Read(pane+0x64)!=model && m.Read(pane+0x68)!=model &&
           !prompts[0].Drawing() && !prompts[1].Drawing())return;
        const bool upper=(current_pane>=5 && current_pane<=13) || current_pane==90;
        const bool stone=(current_pane>=25 && current_pane<=33);
        const bool transition=Moving(state);
        if((upper && state>=3 && state<=15) || (stone && (transition || (state>=6 && state<=8))) ||
           ((upper || stone) && (state==16 || state==17))) {
            const float opacity=upper?animation.prompt:animation.buttons;
            // Replace the native slide's alpha in the fresh draw uniform;
            // don't compound two independent fades or alter shared materials.
            m.Write(at,std::bit_cast<U32>(opacity));
        }
    }
    template<class M> void RestoreSamplers(M& m) {
        if(visible_pane) {m.Write(visible_pane+0x6c,visible_flags);visible_pane=0;}
        for(auto& s:samplers)if(s.mesh) {
            m.Write(s.mesh+0x13c,s.mag);m.Write(s.mesh+0x140,s.min);s={};
        }
    }
    template<class M> void FilterPane(M& m,U32 pane) {
        // Native BoardModel sampler enums: linear mag=1, linear min=3.
        // Change only this synchronous pane draw, then restore before the next
        // one. Never substitute texture pointers, materials, or cached owners.
        for(unsigned i=0;i<2;++i) {
            const U32 model=m.Read(pane+0x64+4*i).value_or(0);
            if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=0x4ebd60)continue;
            const U32 mesh=m.Read(model+4).value_or(0);
            if(!TitleScreen::Pointer(mesh,0x19c) || (i && samplers[0].mesh==mesh))continue;
            const auto mag=m.Read(mesh+0x13c),min=m.Read(mesh+0x140);
            if(!mag || !min || *mag>1 || *min>5)continue;
            samplers[i]={mesh,*mag,*min};m.Write(mesh+0x13c,1);m.Write(mesh+0x140,3);
        }
    }
    template<class M,class R> void Top(M& m,R& r,bool supported) {
        for(unsigned i=0;i<prompts.size();++i)if(prompts[i].Pending()) {
            replaced[i]|=prompts[i].Drawing();
            prompts[i].Resume(m,r);
            if(prompts[i].Pending())return;
            if(!replaced[i])next=current_pane; // Failed setup must retain the entire native box.
        }
        RestoreSamplers(m);
        if(stage==Stage::Idle) {
            routed_panes={};submitted=0;replaced={};attempted={};animation={};
            save_layout=SaveMenu::GetLayout();save_layout.focus_scale=focus_scale;
            frame_layout=SharedLayout(save_layout);shared_style=true;
            state=m.Read(Controller(m)+8).value_or(0)&255;
            if(BossChallenge::InBattle(m) && state>=12 && state<=17) {
                const auto boss=BossChallenge::GetLayout();
                // Match death/save placement; retain Boss Challenge controls
                // as additional adjustments rather than losing saved edits.
                const auto adjust=[&](SaveMenu::Part part,BossChallenge::Part source) {
                    const auto p=boss[source];if(!BossChallenge::Valid(p))return;
                    auto& target=save_layout[part];
                    target.x+=p.x;target.y+=p.y;target.scale*=p.scale;
                };
                adjust(SaveMenu::Prompt,BossChallenge::QuitPrompt);
                adjust(SaveMenu::Save,BossChallenge::Continue);
                adjust(SaveMenu::DontSave,BossChallenge::Quit);
                frame_layout=SharedLayout(save_layout);
            }
            on_top.store(false);owner=r[0];const auto play_state=m.Read(r[1]+0x100).value_or(0);
            const bool native_visible=(play_state&255)!=3 || ((play_state>>8)&255)==0 ||
                ((play_state>>8)&255)==2;
            // 427a18 can still submit the top layout after the last menu state.
            // Keep its own fade wide for as long as the native pane draws.
            native_top_draw=supported && native_visible && owner && owner==Controller(m);
            route=native_top_draw && Drawable(m,owner);
            // Game Over is native top-layout pane 1, outside the bottom
            // save panes routed below. Filter its original foreground/shadow
            // for this draw too; the Native return restores both samplers.
            const U32 title=m.Read(owner+LayoutOffset+0x41c).value_or(0);
            if(route && TitleScreen::Pointer(title,0xb4) &&
               m.Read(title+4)==std::optional<U32>{owner+PaneResourcesOffset})
                FilterPane(m,title);
            stage=Stage::Native;
            r[14]=Sites[0].pc;r[15]=0x427a18;return;
        }
        if(stage==Stage::Native) {
            saved=r;saved[14]=saved[15]=Sites[0].pc+4;
            if(!route || !Drawable(m,owner)) {stage=Stage::Idle;r=saved;return;}
            selection=m.Read(owner+0x10).value_or(0xffffffff);
            if(shared_style) {
                const U32 clock_pane=m.Read(owner+PanesOffset+25*4).value_or(0);
                const float clock=std::bit_cast<float>(m.Read(clock_pane+0x18).value_or(0));
                // Native layout increments the clock after evaluating it.
                animation=Animation(state,animation_owner==owner && animation_state==state?
                    animation_ticks:(std::isfinite(clock)?std::max(0.f,clock-(half_frame?.5f:1.f)):0.f));
            }
            next=0;stage=Stage::Panes;
        }
        while(next<256) {
            const unsigned index=next++;
            if(!RoutedPane(index,selection))continue;
            const U32 pane=m.Read(owner+PanesOffset+4*index).value_or(0);
            if(!TitleScreen::Pointer(pane,0xb4) ||
               m.Read(pane+4)!=std::optional<U32>{owner+PaneResourcesOffset})continue;
            // Native draw retains visibility, animation, text and shadow order.
            routed_panes[index]=pane;++submitted;current_pane=index;
            const bool upper=index>=5 && index<=13,warning=index>=15 && index<=23;
            const unsigned group=warning?1:0;
            // State 12 prepares the Boss Challenge dialog. Its text must not
            // be forced visible before the box begins fading in at state 13.
            if(shared_style && state==12 && (upper || index==90))continue;
            if(shared_style && (upper || warning)) {
                if(replaced[group])continue;
                if((index==5 || index==15) && !attempted[group]) {
                    const U32 resource=m.Read(pane+0x78).value_or(0);
                    const U32 flags=m.Read(pane+0x6c).value_or(0);
                    if(((flags>>8)&255) && m.Read(pane+0xb0).value_or(0)>2 && (resource==5 || resource==15)) {
                        FilterPane(m,pane);
                        attempted[group]=true;
                        r=saved;
                        prompts[group].Begin(m,r,Sites[0].pc,MakePrompt(resource==15),{},
                            m.Read(pane+0x64).value_or(0));
                        if(prompts[group].Pending())return;
                        RestoreSamplers(m);
                    }
                }
            }
            if(shared_style && (state==16 || state==17))continue; // Stay gone during scene fade.
            if(shared_style && PressedStone(index))continue; // Keep normal stone artwork on click.
            if(shared_style && index==90 && state>=3 && state<=15) {
                // The newly bound message already updates its model while
                // hidden. Present it alongside the box, not after stone entry.
                if(const auto flags=m.Read(pane+0x6c)) {
                    visible_pane=pane;visible_flags=*flags;
                    m.Write(pane+0x6c,*flags|0x100);
                }
            }
            if(shared_style)FilterPane(m,pane);
            r=saved;r[0]=pane;r[14]=Sites[0].pc;r[15]=0x2f1280;return;
        }
        {std::scoped_lock lock{layout_mutex};touch_layout=frame_layout;}
        on_top.store(submitted!=0);r=saved;stage=Stage::Idle;
    }
    template<class M,class R> void BottomPane(M& m,R& r) const {
        const bool skip=on_top.load() && Drawable(m,owner) && r[5]==owner+LayoutOffset &&
            r[4]<routed_panes.size() && r[0]!=0 && routed_panes[r[4]]==r[0] &&
            RoutedPane(r[4],m.Read(owner+0x10).value_or(0xffffffff));
        r[15]=Sites[1].pc+4;
        if(!skip) {r[14]=Sites[1].pc+4;r[15]=0x2f1280;}
    }
};
}
