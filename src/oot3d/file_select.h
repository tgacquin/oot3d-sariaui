// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompts.h"
#include "oot3d/screen_model_hooks.h"
#include "oot3d/file_background_assets.h"
#include "oot3d/file_particles.h"
#include "oot3d/master_quest.h"
#include "oot3d/quest_choice_assets.h"
#include "oot3d/dpad_layout.h"
#include "oot3d/widescreen.h"
#include "oot3d/user_hud.h"
#include "oot3d/file_status_assets.h"
#include "oot3d/name_keyboard.h"
#include "oot3d/stone_focus.h"
#include "oot3d/name_typing.h"
#include "oot3d/loading_icon_assets.h"
#include "oot3d/randomizer/file_status.h"
#include "oot3d/room_player_icon.h"
#include "oot3d/multiplayer_loading.h"

namespace OOT3D::FileSelect {
using TitleScreen::U32;
constexpr U32 Owner=0x504fa0, NameOwner=0x5077f0, NameDraw=0x428924;
constexpr U32 FirstSVC=0xfff0b0, QuadCount=184;
constexpr std::array<TitleScreen::Site,21> Sites{{
    {0x419838,0xeb0016a9,0xe5951444}, // Original bottom file-select draw.
    {0x41f304,0xe12fff31,0xe59f5034}, // Atlas draw before lettering and cursor.
    {0x41ee8c,0xeb0026a4,0xe320f000}, // Complete native name-entry draw on bottom.
    {0x300520,0xeb0488e6,0xe2840901}, // Top insertion BEFORE the native scene fade.
    {0x45da54,0xebf9e63e,0xe59409e0}, // Original file-select top-screen logo.
    {0x45db00,0xebf9e613,0xe320f000}, // Original file-select description panel.
    {0x42894c,0xe12fff31,0xe2844001}, // Name-entry native atlas submission.
    {0x427ff0,0xe3a00006,0xea000083}, // Validated name: keyboard exit -> next save prompt.
    {0x43cc7c,0xe5990014,0xe3a0b00b}, // Western keyboard directional navigation dispatch.
    {0x43cc80,0xe3a0b00b,0xe2800001}, // Return from custom navigation's native repeat-input read.
    {0x43e664,0xe3a0900a,0x0a00002d}, // Accent navigation, after native repeat-input read.
    {0x2f79b8,0xe1a05000,0xe59f0120}, // Cursor corner spacing, after native register save.
    {0x2f7afc,0xe1a05000,0xed8d0a01}, // Cursor origin, after native register save.
    {0x43e59c,0xe59a0024,0xe3500000}, // Accent B-delete before the silent native tail call.
    {0x427cf8,0xe5960004,0xe350000b}, // Name-state dispatch after native input/prologue.
    {0x43c9ac,0xebfbfb0e,0xe59faa8c}, // Western A read.
    {0x43cb38,0xebfbfaab,0xe59fb90c}, // Western B read.
    {0x43e440,0xebfbf469,0xe59f9530}, // Accent A read.
    {0x43e58c,0xebfbf416,0xe3100002}, // Accent B read.
    {0x43c8c0,0xebfaaec9,0xe59f9b68}, // Western touch dispatch.
    {0x43e3c8,0xebfaa807,0xebfbf486}, // Accent touch dispatch.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return 0xef000000 | (FirstSVC + i);
    return word;
}
template<class M,class R> void AcceptName(M& m,R& r) {
    if(RoomJoin::Active()) {r[15]=0x42863c;return;} // Session names never enter save creation.
    // Only reached after native OK validation and the keyboard exit. The save
    // prompt still owns saving/cancellation; skip just the redundant Yes/No.
    const auto length=m.Read(NameOwner+0x24).value_or(0);
    r[0]=m.Read(NameOwner+4)==5 && length>0 && length<=8?10:6;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return m.Read(0x41f340)==std::optional<U32>{Owner} &&
        m.Read(0x41f344)==std::optional<U32>{0x55b548} &&
        m.Read(NameDraw)==std::optional<U32>{0xe92d4070} &&
        m.Read(0x4289ac)==std::optional<U32>{NameOwner};
}
enum Part : unsigned {Stone1,Stone2,Stone3,Prompt,Start,Copy,Erase,LowerPrompt,DeleteCancel,DeleteErase,OverwriteCancel,OverwriteAccept,CopyArrow,Count};
constexpr std::array Names{"File 1 stone","File 2 stone","File 3 stone","Prompt box",
    "Start","Copy","Erase","Game Card warning box","Cancel stone","Erase confirmation stone",
    "Overwrite cancel stone","Overwrite stone","Copy arrow"};
enum NamePart : unsigned {Keyboard,NameBox,Cancel,Okay,KeyboardBackground,ConfirmNo,ConfirmYes,NameCount};
constexpr unsigned QuestFocus=Count,NameFocus=Count+2,FocusCount=NameFocus+NameCount;
inline constexpr std::array NameLabels{"Keyboard","Name box","Cancel","OK","Blue keyboard background","No confirmation stone","Yes confirmation stone"};
struct KeyboardShadow {
    bool enabled=true;
    float opacity=.6f,softness=12,spread=2,x=0,y=3;
    float padding_top=0,padding_left=0,padding_bottom=0,padding_right=0;
};
struct Vignette {
    bool enabled=true;
    float strength=.55f,width=.85f,height=.85f,softness=.85f;
    float bottom_right_boost=0;
};
struct SeedShadow {
    bool enabled=true;
    float x=0,y=0,width=1,height=1,radius=6,softness=6,opacity=.6f;
};
struct Layout {
    std::array<PausePrompts::Pose,Count> parts{};
    std::array<PausePrompts::Pose,NameCount> name_parts{};
    KeyboardShadow keyboard_shadow;
    NameKeyboard::Settings keyboard;
    std::array<PausePrompts::Pose,2> name_prompts{};
    float name_box_edge_fade=48;
    Vignette vignette;
    FileParticles::Settings particles;
    PausePrompts::Pose stones{};
    float stone_spacing=1;
    float stone_shadow_opacity=.55f;
    float stone_ease_out=2;
    bool stone_focus_grow=false;
    float stone_focus_seconds=.1f;
    std::array<float,FocusCount> stone_focus_amount{}; // Frame snapshot only; never saved as editor scale.
    std::array<PausePrompts::Pose,2> quest_stones{{{0,15,.75f},{0,15,.75f}}};
    PausePrompts::Pose quest_description{}; // Retain the existing saved pose for the replacement prompt.
    PausePrompts::Pose randomizer_prompt{}; // Offset from the original prompt position.
    PausePrompts::Pose profile_join_prompt{},host_prompt{};
    PausePrompts::Pose player_count{};
    PausePrompts::Pose join_prompt{}; // Independent offset/scale above Randomizer.
    PausePrompts::Pose seed_text{};
    float room_id_gap=3; // Gap below the seed, before shared layout scaling.
    PausePrompts::Pose randomizer_badge{};
    std::array<float,3> randomizer_badge_color{1.f,.18f,.18f};
    SeedShadow seed_shadow;
    std::array<PausePrompts::Pose,2> prompts{{{120,220,1},{220,220,1}}};
    // Presentation-only saved-file entrance; never persisted in the editor.
    unsigned saved_file_slot=3;
    float saved_file_remaining=0;
    bool backgrounds_enabled=true;
    std::array<PauseMenu::MapBackground,2> backgrounds{{{{-128,0,1},0},{{128,0,1},0}}};
};
inline float FocusScale(const Layout& l,unsigned index) {
    return l.stone_focus_grow && index<FocusCount?
        StoneFocus::Scale(l.stone_focus_amount[index]):1.f;
}
using BackgroundGeometry=PausePrompts::QuadGeometry<4>;
using VignetteGeometry=PausePrompts::QuadGeometry<4*12*12>;
inline VignetteGeometry CornerVignette(const Vignette& s,bool wide) {
    VignetteGeometry g;
    if(!s.enabled || !std::isfinite(s.strength) || !std::isfinite(s.width) ||
       !std::isfinite(s.height) || !std::isfinite(s.softness) || s.strength<=0)return g;
    const float screen=wide?240*Widescreen::Aspect:400;
    const float left=(400-screen)/2;
    const float w=screen*.5f*std::clamp(s.width,.05f,1.f);
    const float h=120*std::clamp(s.height,.05f,1.f);
    const float softness=std::clamp(s.softness,.05f,1.f);
    const auto alpha=[&](float x,float y,unsigned corner) {
        const float t=std::clamp((std::sqrt(x*x+y*y)-(1-softness))/softness,0.f,1.f);
        const float base=std::clamp(s.strength,0.f,1.f)*(1-t*t*(3-2*t));
        const float boost=std::isfinite(s.bottom_right_boost)?std::clamp(s.bottom_right_boost,0.f,4.f):0;
        return corner==3 && boost>0?1-std::pow(1-base,1+boost):base;
    };
    // Four disjoint corner patches. The center stays clear and no overlapping
    // layers compound the opacity. Coordinates follow the actual game viewport.
    for(unsigned corner=0;corner<4;++corner)for(unsigned y=0;y<12;++y)for(unsigned x=0;x<12;++x) {
        const bool right=corner&1,bottom=corner&2;
        const float px=right?left+screen-w+x*w/12:left+x*w/12;
        const float py=bottom?240-h+y*h/12:y*h/12;
        g.Add(px,py,w/12,h/12,.5f,.5f,.5f,.5f);
        for(unsigned v=0;v<4;++v) {
            float nx=(x+(v&1))/12.f,ny=(y+(v>>1))/12.f;
            if(right)nx=1-nx;
            if(bottom)ny=1-ny;
            g.colors[(g.count-1)*16+v*4+3]=alpha(nx,ny,corner);
        }
    }
    return g;
}
using ChoiceGeometry=PausePrompts::QuadGeometry<12>;
inline ChoiceGeometry QuestStones(unsigned selected,std::array<float,2> offsets={},
                                 const Layout& layout={}) {
    ChoiceGeometry g;
    const auto add=[&](float x,float y,float w,float h,float u,float v,float tw,float th) {
        // COmoteUraSelector's atlas rectangles are measured from the bottom.
        // Preserve that native crop; the shared UI material flips V at draw.
        g.Add(x,y,w,h,(u+.5f)/512,(v+th-.5f)/256,
              (u+tw-.5f)/512,(v+.5f)/256);
    };
    for(unsigned i=0;i<2;++i) {
        const float x=62+154.f*i+offsets[i];
        add(x-1.8f,38.2f,127.8f,169.2f,304,60,142,188);
        // Replace the baked shadow with the same adjustable shadow as the files.
        for(unsigned k=3;k<16;k+=4)g.colors[(g.count-1)*16+k]=0;
        add(x,40,124.2f,165.6f,i?160:8,64,138,184);
        const float brightness=i==selected?1.f:.55f;
        for(unsigned k=0;k<16;++k)if(k%4!=3)g.colors[(g.count-1)*16+k]=brightness;
    }
    const float x=62+154.f*(selected&1)+offsets[selected&1];
    // The four original yellow selection corners.
    add(x-3.6f,36.4f,14.4f,14.4f,384,32,16,16);
    add(x-3.6f,196.6f,14.4f,14.4f,416,32,16,16);
    add(x+115.2f,36.4f,14.4f,14.4f,400,32,16,16);
    add(x+115.2f,196.6f,14.4f,14.4f,432,32,16,16);
    // The atlas also contains the four bright highlight corners. Native
    // cursors pulse this second layer over the opaque gold base.
    add(x-3.6f,36.4f,14.4f,14.4f,448,32,16,16);
    add(x-3.6f,196.6f,14.4f,14.4f,480,32,16,16);
    add(x+115.2f,36.4f,14.4f,14.4f,464,32,16,16);
    add(x+115.2f,196.6f,14.4f,14.4f,496,32,16,16);
    const float glow=MasterQuest::CursorGlow();
    for(unsigned q=8;q<12;++q)for(unsigned v=0;v<4;++v)g.colors[q*16+v*4+3]=glow;
    for(unsigned q=0;q<g.count;++q) {
        const unsigned mode=q<4?q/2:selected&1;
        auto p=layout.quest_stones[mode];
        if(!PausePrompts::Valid(p))p={};
        p.scale*=FocusScale(layout,QuestFocus+mode);
        const float cx=124.1f+154.f*mode,cy=122.8f;
        const float native_offset=offsets[mode];
        // Keep the entrance/exit completely offscreen even after enlarging or moving a stone.
        const float travel=native_offset>=0?std::max(480.f,400-(cx+p.x-62.1f*p.scale)+24):
            std::max(480.f,cx+p.x+62.1f*p.scale+24);
        for(unsigned v=0;v<4;++v) {
            auto& x=g.positions[q*12+v*3];auto& y=g.positions[q*12+v*3+1];
            x=cx+p.x+(x-native_offset-cx)*p.scale+native_offset*travel/480;
            y=cy+p.y+(y-cy)*p.scale;
        }
    }
    return g;
}
inline BackgroundGeometry Backgrounds(const Layout& layout) {
    BackgroundGeometry g;
    if(!layout.backgrounds_enabled)return g;
    for(const auto& stack:layout.backgrounds) {
        const auto p=stack.pose;
        if(!PauseMenu::Valid(p) || !std::isfinite(stack.darkness))continue;
        const float w=256*p.scale,h=192*p.scale;
        const float left=200+p.x-w/2,seam=120+p.y;
        const float brightness=1-std::clamp(stack.darkness,0.f,1.f);
        for(unsigned row=0;row<2;++row) {
            g.Add(left,seam+(float(row)-1)*h,w,h,
                .5f/FileBackground::Width,255.5f/FileBackground::Height,
                319.5f/FileBackground::Width,16.5f/FileBackground::Height);
            for(unsigned i=0;i<16;++i)g.colors[(g.count-1)*16+i]=i%4==3?1:brightness;
        }
    }
    return g;
}
struct Rect {
    float left=0,top=0,right=0,bottom=0;
    bool moving=false;
    float slide_x=0,slide_y=0;
    bool Valid() const {return std::isfinite(left) && std::isfinite(top) &&
        std::isfinite(right) && std::isfinite(bottom) && right>left && bottom>top;}
    bool Visible() const {return Valid() && right>0 && left<320 && bottom>0 && top<240;}
    bool Contains(float x,float y) const {return Valid() && x>=left && x<right && y>=top && y<bottom;}
    float X() const {return (left+right)/2;}
    float Y() const {return (top+bottom)/2;}
};
inline std::mutex mutex;
inline Layout layout;
inline std::array<Rect,Count> touch_bounds{};
inline std::array<Rect,2> quest_touch_bounds{};
inline Layout touch_layout;
inline std::atomic<bool> dirty{false},on_top{false},name_on_top{false};
inline std::atomic<bool> action_opening{false};
inline std::atomic<unsigned> action_frames{0};
// The outgoing and incoming pages share a 60 Hz clock. Native menu states
// still own prompts, input, save operations and their completion/error paths.
enum class Exchange {None,Erase,Copy,Start,BackErase,BackCopy,Overwrite,BackOverwrite,AcceptOverwrite};
inline std::atomic<Exchange> exchange{Exchange::None};
inline std::atomic<unsigned> exchange_frames{0};
inline void BeginExchange(Exchange value) {exchange_frames=1;exchange=value;}
inline bool ExchangeState(Exchange value,U32 state) {
    switch(value) {
    case Exchange::Erase:return state==34 || state==36;
    case Exchange::Copy:return state==12 || state==13;
    case Exchange::Start:return state==10;
    case Exchange::BackErase:return state==38 || state==7;
    case Exchange::BackCopy:return state==15 || state==7;
    case Exchange::Overwrite:return state==16 || state==17;
    case Exchange::BackOverwrite:return state==18;
    case Exchange::AcceptOverwrite:return state==24;
    default:return false;
    }
}
inline unsigned TransitionPromptSlot(U32 state,bool erase_cancel=false,unsigned destination=3) {
    if(state==12 || state==13 || state==14)return 9; // Copy destination prompt.
    if((state==16 || state==17) && destination<3)return 10+destination;
    if(state==18)return 9;
    if(state>=24 && state<=28)return 15; // Copying file..., including save I/O.
    if(state==32)return 16; // Copy completed; native save has finished.
    if(state==15)return 7; // Back from Copy: selected file's action prompt.
    if(state==39 || (state==42 && !erase_cancel))return 22; // Deleting file...
    if(state==34 || state==36)return 19;
    if(state==4 || state==7 || state==8 || state==38)return 7;
    if(state==2 || state==3 || state==9)return 6;
    return 0;
}

template<class M> bool DeletingFile(M& m) {
    const auto state=m.Read(Owner+0x10).value_or(0);
    return state==39 || state==44 || (state==42 && m.Read(Owner+0x2c)==std::optional<U32>{0});
}
template<class M> bool FileOperationBusy(M& m) {
    const auto state=m.Read(Owner+0x10).value_or(0);
    return DeletingFile(m) || (state>=24 && state<=28);
}
struct FileOperationPromptTransition {
    float opacity=0;
    std::int64_t clock_us=0;
    void Frame(bool busy) {
        // Crossfade the ordinary prompts and existing loading icon in 100 ms.
        opacity=std::clamp(opacity+(busy?1.f:-1.f)/6,0.f,1.f);
        if(opacity<.00001f)opacity=0;
        if(opacity>.99999f)opacity=1;
        clock_us=opacity>0?clock_us+16667:0;
    }
};
inline Layout GetLayout() {std::scoped_lock lock{mutex};return layout;}
inline StoneFocus::Settings GetStoneFocusSettings() {
    std::scoped_lock lock{mutex};
    return {layout.stone_focus_grow,layout.stone_focus_seconds};
}
inline Layout PresentationLayout(Layout value) {
    if(UserHUD::classic_file_select.load()) {
        value.backgrounds_enabled=false;
        value.vignette.enabled=false;
    }
    return value;
}
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{mutex};layout=value;if(save)dirty=true;
}
// Back's red card, lettering, and underlying shadow. All other pieces belong
// to the prompt, save stones and their details, or file-action/confirmation buttons.
constexpr bool BackQuad(unsigned q) {return (q>=118 && q<=120) || q==173 || q==174;}
constexpr unsigned QuadPart(unsigned q) {
    if(q<6)return Prompt;
    if(q>=6 && q<117)return (q-6)/37;
    if((q>=122 && q<=125) || (q>=175 && q<=177))return Start;
    if((q>=126 && q<=129) || (q>=178 && q<=180))return Copy;
    if((q>=130 && q<=133) || q>=181)return Erase;
    if(q>=134 && q<=140)return DeleteCancel;
    if(q>=141 && q<=147)return DeleteErase;
    if(q>=148 && q<=154)return OverwriteCancel;
    if(q>=155 && q<=161)return OverwriteAccept;
    if(q==171)return CopyArrow;
    if(q>=162 && q<=170)return LowerPrompt;
    return Count;
}
template<class M> bool Active(M& m) {
    const auto state=m.Read(Owner+0x10).value_or(0);
    const U32 model=m.Read(Owner+8).value_or(0),mesh=m.Read(Owner+4).value_or(0);
    const U32 helper=m.Read(Owner+0x30).value_or(0);
    return state>0 && state<48 && TitleScreen::Pointer(model,0x1e4) &&
        m.Read(model)==std::optional<U32>{0x4ebd60} &&
        m.Read(model+4)==std::optional<U32>{mesh} &&
        TitleScreen::Pointer(mesh,0x19c) && m.Read(helper)==std::optional<U32>{QuadCount};
}
template<class M> bool NameActive(M& m) {
    const U32 state=m.Read(NameOwner+4).value_or(0);
    if(state==0 || state>10)return false;
    // Both native keyboard atlases, including the name field and action keys.
    for(U32 off:{0x64u,0x68u}) {
        const U32 model=m.Read(NameOwner+off).value_or(0);
        if(!TitleScreen::Pointer(model,0x1e4) ||
           m.Read(model)!=std::optional<U32>{0x4ebd60} ||
           !TitleScreen::Pointer(m.Read(model+4).value_or(0),0x19c))return false;
    }
    return true;
}
using Geometry=PausePrompts::QuadGeometry<QuadCount>;
template<unsigned Capacity> inline void PromptCapUVs(PausePrompts::QuadGeometry<Capacity>& g,
                                                     unsigned q,float edge) {
    // Both mirrored caps end beside transparent WHITE atlas padding. Linear
    // filtering at the boundary mixes that RGB into the dark panel, producing
    // vertical creases. Sample the last interior texel, preserving all geometry,
    // vertical slicing and outer edges (including texture-pack coordinates).
    const float boundary=edge/512.f,inside=(edge-.5f)/512.f;
    for(unsigned v=0;v<4;++v) {
        auto& u=g.uvs[q*8+v*2];if(u==boundary)u=inside;
    }
}
inline void StoneFaceUVs(Geometry& g,unsigned q) {
    // Full stone faces abut other artwork in the native 512x256 atlas.
    // Sampling their texel boundaries leaks a thin neighboring white stripe
    // into transparent margins when enlarged. Keep positions/padding intact.
    for(unsigned axis=0;axis<2;++axis) {
        float lo=INFINITY,hi=-INFINITY;
        for(unsigned v=0;v<4;++v) {
            const float uv=g.uvs[q*8+v*2+axis];lo=std::min(lo,uv);hi=std::max(hi,uv);
        }
        if(!std::isfinite(lo) || !std::isfinite(hi) || hi<=lo)continue;
        const float inset=std::min((hi-lo)/2,.5f/(axis?256:512));
        for(unsigned v=0;v<4;++v) {
            auto& uv=g.uvs[q*8+v*2+axis];uv=std::clamp(uv,lo+inset,hi-inset);
        }
    }
}
template<unsigned Capacity> inline Rect Bounds(const PausePrompts::QuadGeometry<Capacity>& g,unsigned first,unsigned last) {
    Rect b{INFINITY,INFINITY,-INFINITY,-INFINITY};
    for(unsigned q=first;q<=last;++q)for(unsigned v=0;v<4;++v) {
        const float x=g.positions[q*12+v*3],y=g.positions[q*12+v*3+1];
        if(!std::isfinite(x) || !std::isfinite(y))return {};
        b.left=std::min(b.left,x);b.right=std::max(b.right,x);
        b.top=std::min(b.top,y);b.bottom=std::max(b.bottom,y);
    }
    return b;
}
inline std::array<Rect,Count> PartBounds(const Geometry& g) {
    std::array<Rect,Count> out{Bounds(g,6,8),Bounds(g,43,45),Bounds(g,80,82),Bounds(g,0,5),
        Bounds(g,122,124),Bounds(g,126,128),Bounds(g,130,132),Bounds(g,162,170),Bounds(g,134,136),Bounds(g,141,143),
        Bounds(g,148,150),Bounds(g,155,157),Bounds(g,171,171)};
    for(unsigned part=DeleteCancel;part<=OverwriteAccept;++part) {
        const unsigned first=134+(part-DeleteCancel)*7;
        const auto pressed=Bounds(g,first+4,first+6),label=Bounds(g,first+3,first+3);
        if(pressed.Valid() && label.Valid() &&
           std::abs(pressed.X()-label.X())<std::abs(out[part].X()-label.X()))out[part]=pressed;
    }
    return out;
}
// Measure the actual native glyph mesh, including the model transform. This
// follows translations and runtime message substitutions without guessing from
// UTF-8 byte counts or allocating a second text renderer.
template<class M> Rect MessageBounds(M& m,U32 controller) {
    Rect result{INFINITY,INFINITY,-INFINITY,-INFINITY};
    if(!TitleScreen::Pointer(controller,0x4c))return {};
    for(unsigned layer=0;layer<2;++layer) {
        const auto model=m.Read(controller+0x10+layer*4).value_or(0);
        if(!TitleScreen::Pointer(model,0x1e4))return {};
        const auto mesh=m.Read(model+4).value_or(0),desc=m.Read(mesh).value_or(0);
        const auto positions=m.Read(desc).value_or(0),count=m.Read(desc+12).value_or(0);
        if(!count || count>1024 || !TitleScreen::Pointer(positions,count*12))return {};
        std::array<float,12> matrix;
        if(!TitleScreen::ReadFloats(m,model+0xc,matrix))return {};
        for(unsigned i=0;i<count;++i) {
            std::array<float,3> v;
            if(!TitleScreen::ReadFloats(m,positions+i*12,v))return {};
            const float x=matrix[0]*v[0]+matrix[1]*v[1]+matrix[2]*v[2]+matrix[3];
            const float y=matrix[4]*v[0]+matrix[5]*v[1]+matrix[6]*v[2]+matrix[7];
            if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x)>2000 || std::abs(y)>2000)return {};
            result.left=std::min(result.left,x);result.right=std::max(result.right,x);
            result.top=std::min(result.top,y);result.bottom=std::max(result.bottom,y);
        }
    }
    return result.Valid()?result:Rect{};
}
struct MessageFit {float left=0,right=0,dx=0,scale=1;};
inline MessageFit FitMessage(float box_left,float box_right,float text_left,float text_right) {
    const float lo=PausePrompts::SafeLeft(),hi=400-lo,padding=12;
    const float text_width=text_right-text_left;
    if(text_width<=0 || box_right<=box_left)return {box_left,box_right};
    const float scale=std::min(1.f,(hi-lo-2*padding)/text_width);
    const float width=std::min(hi-lo,std::max(box_right-box_left,text_width*scale+2*padding));
    const float center=std::clamp((box_left+box_right)/2,lo+width/2,hi-width/2);
    // Preserve ordinary native alignment unless the text requires more space.
    const float text_center=(text_left+text_right)/2;
    const float target=std::clamp(text_center,center-width/2+padding+text_width*scale/2,
        center+width/2-padding-text_width*scale/2);
    return {center-width/2,center+width/2,target-text_center,scale};
}
inline float EasePosition(float value,float start,float end,float strength) {
    if(!std::isfinite(value) || start==end)return value;
    const float remaining=(value-end)/(start-end);
    if(remaining<=0 || remaining>=1)return value;
    const float power=1+(std::isfinite(strength)?std::clamp(strength,0.f,4.f):2.f);
    return end+(start-end)*std::pow(remaining,power);
}
inline float EntranceX(float x,float strength) {
    if(!std::isfinite(x) || x<=16 || x>=336)return x;
    const float power=1+(std::isfinite(strength)?std::clamp(strength,0.f,4.f):2.f);
    // Each stone reaches x=336 at a different native time. Preserve that
    // stagger and its arrival time; ease only the final 320-pixel approach.
    return 16+320*std::pow((x-16)/320,power);
}
inline ScreenModels::Transform PartTransform(const Layout& l,unsigned part,
                                             const std::array<Rect,Count>& bounds) {
    // The top projection already has square pixels. Center the original 320px
    // canvas on the 400px screen; widescreen correction is handled by the renderer.
    if(part>=Count)return {40,0};
    const auto b=bounds[part];
    if(!b.Valid() || (!b.Visible() && !b.moving))return {0,0,0,0,0,0,true};
    const auto& saved=l.parts[part];
    const bool valid=std::isfinite(saved.x) && std::abs(saved.x)<=800 &&
        std::isfinite(saved.y) && std::abs(saved.y)<=480 &&
        std::isfinite(saved.scale) && saved.scale>=.1f && saved.scale<=4;
    const auto p=valid?saved:PausePrompts::Pose{};
    const float scale=p.scale*FocusScale(l,part);
    if(part<=Stone3) {
        const auto group=PausePrompts::Valid(l.stones)?l.stones:PausePrompts::Pose{};
        const float spacing=std::isfinite(l.stone_spacing)?std::clamp(l.stone_spacing,0.f,4.f):1.f;
        // Keep existing stone adjustments as the base layout. Scale the whole
        // arrangement about the middle stone's resting center. An animated
        // middle stone must not drag the other stones' scaling pivot with it.
        const auto middle=PausePrompts::Valid(l.parts[Stone2])?l.parts[Stone2]:PausePrompts::Pose{};
        const float cx=160+middle.x,cy=122+middle.y;
        return {
            40+group.x+cx+(b.X()+b.slide_x+p.x-cx)*group.scale-b.X()+MasterQuest::NoticeShake(part),
            group.y+cy+(b.Y()+b.slide_y+p.y-cy)*spacing*group.scale-b.Y(),
            scale*group.scale,scale*group.scale,b.X(),b.Y(),true};
    }
    float slide=0;
    if(b.moving && ((part>=DeleteCancel && part<=OverwriteAccept) ||
       (part>=Start && part<=Erase && exchange.load()!=Exchange::None))) {
        const float native_left=(part==DeleteCancel || part==OverwriteCancel)?34.f:(part==DeleteErase || part==OverwriteAccept)?162.f:
            part==Start?98.f:part==Copy?70.f:166.f;
        const float offset=b.left-native_left,center=b.X()-offset;
        const float half_width=(b.right-b.left)*scale/2;
        const float travel=offset>=0?std::max(500.f,424-(40+p.x+center-half_width)):
            std::max(500.f,40+p.x+center+half_width+24);
        slide=offset*(travel/500-1);
    }
    return {40+p.x+slide+b.slide_x+(part==Start?MasterQuest::StartShake():0)+(part>=Start && part<=Erase?MasterQuest::NoticeShake(3+part-Start):0),p.y+b.slide_y,scale,scale,b.X(),b.Y(),true};
}
inline void Hide(Geometry& g,unsigned q) {std::fill_n(g.positions.begin()+q*12,12,0.f);}
// Unclassified native decoration keeps its original screen limits. Editable
// cards bypass this: their complete geometry is clipped by the top viewport.
inline bool Clip(Geometry& g,unsigned q) {
    const auto b=Bounds(g,q,q);
    if(!b.Visible()){Hide(g,q);return false;}
    if(b.left>=0 && b.right<=320 && b.top>=0 && b.bottom<=240)return true;
    std::array<float,12> positions;
    std::array<float,8> uvs;
    std::array<float,16> colors;
    std::copy_n(g.positions.begin()+q*12,12,positions.begin());
    std::copy_n(g.uvs.begin()+q*8,8,uvs.begin());
    std::copy_n(g.colors.begin()+q*16,16,colors.begin());
    for(unsigned v=0;v<4;++v) {
        const float x=std::clamp(positions[v*3],0.f,320.f);
        const float y=std::clamp(positions[v*3+1],0.f,240.f);
        const float fx=(x-b.left)/(b.right-b.left),fy=(y-b.top)/(b.bottom-b.top);
        g.positions[q*12+v*3]=x;g.positions[q*12+v*3+1]=y;
        for(unsigned c=0;c<6;++c) {
            float value=0;
            for(unsigned k=0;k<4;++k) {
                const float weight=(positions[k*3]==b.left?1-fx:fx)*
                    (positions[k*3+1]==b.top?1-fy:fy);
                value+=weight*(c<2?uvs[k*2+c]:colors[k*4+c-2]);
            }
            if(c<2)g.uvs[q*8+v*2+c]=value;
            else g.colors[q*16+v*4+c-2]=value;
        }
    }
    return true;
}
template<class M> unsigned CopyDestinationSlot(M& m) {
    // Native copy selection is a row (0/1), excluding the source slot.
    const auto source=m.Read(Owner+0x14).value_or(3);
    const auto row=m.Read(Owner+0x1c).value_or(2);
    if(source>=3 || row>=2)return Count;
    return row+(row>=source?1:0);
}
template<class M> Geometry Atlas(M& m,bool bottom,const Layout& l=GetLayout(),
                                std::array<Rect,Count>* native_bounds=nullptr) {
    Geometry out;
    if(!Active(m))return out;
    const U32 helper=m.Read(Owner+0x30).value_or(0);
    if(!TitleScreen::ReadFloats(m,m.Read(helper+0x10).value_or(0),out.positions) ||
       !TitleScreen::ReadFloats(m,m.Read(helper+0x14).value_or(0),out.uvs) ||
       !TitleScreen::ReadFloats(m,m.Read(helper+0x18).value_or(0),out.colors))return {};
    for(unsigned q:{0u,2u,3u,5u,162u,164u,165u,167u,168u,170u})PromptCapUVs(out,q,288);
    for(unsigned q:{125u,129u,133u,137u,144u,151u,158u})StoneFaceUVs(out,q);
    auto bounds=PartBounds(out);
    const auto state=m.Read(Owner+0x10).value_or(0);
    if((state==4 || state==7) && action_opening.load()) {
        const float progress=std::min(action_frames.load()/10.f,1.f);
        const float offset=EasePosition(400*(1-progress),400,0,l.stone_ease_out);
        for(unsigned q=0;q<QuadCount;++q) {
            const auto part=QuadPart(q);if(part<Start || part>Erase)continue;
            // The recessed/pressed variants are parked separately by the game.
            // Never pull them into the entrance along with the normal faces.
            if(q>=175){Hide(out,q);continue;}
            // State 4 updates file-card feedback only. Action faces can still
            // carry the last visit's pressed alpha until state 7 refreshes them.
            if(q==125 || q==129 || q==133)
                for(unsigned v=0;v<4;++v)out.colors[q*16+v*4+3]=1;
            const auto x=m.Read(0x504ff8+q*8),y=m.Read(0x504ff8+q*8+4);
            if(!x || !y)continue;
            const auto raw=Bounds(out,q,q);
            const float dx=std::bit_cast<float>(*x)+offset-raw.left;
            const float dy=std::bit_cast<float>(*y)-raw.top;
            for(unsigned v=0;v<4;++v) {
                out.positions[q*12+v*3]+=dx;out.positions[q*12+v*3+1]+=dy;
            }
        }
        bounds=PartBounds(out);
    }
    const auto page_exchange=exchange.load();
    const bool exchanging=ExchangeState(page_exchange,state);
    const bool returning=page_exchange==Exchange::BackErase || page_exchange==Exchange::BackCopy;
    const float progress=std::min(exchange_frames.load()/10.f,1.f);
    const float incoming=EasePosition(500*(1-progress),500,0,l.stone_ease_out);
    const float outgoing=EasePosition(-500*progress,0,-500,l.stone_ease_out);
    if(exchanging && page_exchange!=Exchange::Overwrite &&
       page_exchange!=Exchange::BackOverwrite && page_exchange!=Exchange::AcceptOverwrite) {
        for(unsigned q=0;q<QuadCount;++q) {
            const auto part=QuadPart(q);
            const bool action=part>=Start && part<=Erase;
            const bool confirmation=(page_exchange==Exchange::Erase || page_exchange==Exchange::BackErase) &&
                (part==DeleteCancel || part==DeleteErase);
            if(!action && !confirmation)continue;
            if(q>=175 || (confirmation && ((q>=138 && q<=140) || (q>=145 && q<=147)))) {
                Hide(out,q);continue;
            }
            const auto x=m.Read(0x504ff8+q*8),y=m.Read(0x504ff8+q*8+4);
            if(!x || !y)continue;
            const auto raw=Bounds(out,q,q);if(!raw.Valid())continue;
            const float offset=action?(returning?-incoming:outgoing):(returning?-outgoing:incoming);
            const float dx=std::bit_cast<float>(*x)+offset-raw.left,dy=std::bit_cast<float>(*y)-raw.top;
            for(unsigned v=0;v<4;++v){out.positions[q*12+v*3]+=dx;out.positions[q*12+v*3+1]+=dy;}
        }
        bounds=PartBounds(out);
        for(unsigned part=Start;part<=Erase;++part)bounds[part].moving=true;
        if(page_exchange==Exchange::Erase || page_exchange==Exchange::BackErase)
            bounds[DeleteCancel].moving=bounds[DeleteErase].moving=true;
    }
    const bool overwrite_exchange=exchanging && (page_exchange==Exchange::Overwrite ||
        page_exchange==Exchange::BackOverwrite || page_exchange==Exchange::AcceptOverwrite);
    if(overwrite_exchange) {
        const float offset=page_exchange==Exchange::Overwrite?incoming:
            page_exchange==Exchange::BackOverwrite?-outgoing:-500*progress;
        for(unsigned q=148;q<=161;++q) {
            const unsigned first=q<155?148:155;
            if(q>=first+4){Hide(out,q);continue;} // Independently parked pressed faces.
            const auto x=m.Read(0x504ff8+q*8),y=m.Read(0x504ff8+q*8+4);
            if(!x || !y)continue;
            const auto b=Bounds(out,q,q);if(!b.Valid())continue;
            for(unsigned v=0;v<4;++v) {
                out.positions[q*12+v*3]+=std::bit_cast<float>(*x)+offset-b.left;
                out.positions[q*12+v*3+1]+=std::bit_cast<float>(*y)-b.top;
            }
        }
        bounds=PartBounds(out);
        bounds[OverwriteCancel].moving=bounds[OverwriteAccept].moving=true;
    }
    for(unsigned i=0;i<3;++i) {
        std::array<float,2> velocity{};
        // Off-screen stones with an active native slide must survive routing.
        // Parked controls still remain hidden, even with editor offsets/scales.
        const auto vx=m.Read(0x55b610+i*4),vy=m.Read(0x55b61c+i*4);
        if(vx && vy) {
            velocity={std::bit_cast<float>(*vx),std::bit_cast<float>(*vy)};
            bounds[i].moving=std::isfinite(velocity[0]) && std::isfinite(velocity[1]) &&
                (velocity[0]!=0 || velocity[1]!=0);
        }
        if(state==2) {
            const auto word=m.Read(0x55b5f8+i*4);
            if(word) {
                const float x=std::bit_cast<float>(*word);
                if(std::isfinite(x))bounds[i].slide_x=EntranceX(x,l.stone_ease_out)-x;
            }
        }
        if(state==4 || state==5) {
            const auto x=m.Read(0x55b5f8+i*4),target=m.Read(0x55b5e0+i*4);
            if(x && target && std::bit_cast<float>(*target)==-384.f) {
                const float current=std::bit_cast<float>(*x);
                // An exit must not ease to a crawl while the last edge is
                // still visible. Keep native 60 Hz travel linear, extending
                // it for the saved scale/position so the whole shadow clears.
                bounds[i].slide_x=16-current;
                const auto pose=PartTransform(l,i,bounds);
                const float right=pose.x+pose.ax+(bounds[i].right-pose.ax)*pose.sx;
                const float scale=PausePrompts::Valid(l.stones)?l.stones.scale:1.f;
                const float travel=std::max(400.f,(right+24)/scale);
                bounds[i].slide_x+=(current-16)*travel/400;
            }
            const auto y=m.Read(0x55b604+i*4),target_y=m.Read(0x55b5ec+i*4);
            if(state==4 && m.Read(Owner+0x14)==i && y && target_y &&
               std::bit_cast<float>(*target_y)==56.f) {
                const float current=std::bit_cast<float>(*y);
                bounds[i].slide_y=EasePosition(current,48.f+52*i,56,l.stone_ease_out)-current;
            }
        }
        if(state==3 && (MasterQuest::returning.load() || MasterQuest::departing_files.load())) {
            bounds[i].moving=true;
            // Remain fully offscreen at the page handoff even with larger or
            // shifted editor layouts. Share this translation with text/shadows.
            const auto pose=PartTransform(l,i,bounds);
            const float right=pose.x+pose.ax+(bounds[i].right-pose.ax)*pose.sx;
            const float group_scale=PausePrompts::Valid(l.stones)?l.stones.scale:1.f;
            const float travel=std::max(480.f,(right+24)/group_scale);
            bounds[i].slide_x=MasterQuest::ReturnOffset(i,l.stone_ease_out)*travel/480;
        }
        if(state==9) {
            const auto x=m.Read(0x55b5f8+i*4),y=m.Read(0x55b604+i*4);
            if(x && velocity[0]>0) {
                const float current=std::bit_cast<float>(*x);
                bounds[i].slide_x=EasePosition(current,-384,16,l.stone_ease_out)-current;
            }
            if(y && m.Read(Owner+0x14)==i) {
                const float current=std::bit_cast<float>(*y);
                bounds[i].slide_y=EasePosition(current,56,48.f+52*i,l.stone_ease_out)-current;
            }
        }
    }
    if(exchanging && page_exchange==Exchange::Overwrite) {
        const auto selected=CopyDestinationSlot(m);
        if(selected<3 && bounds[selected].Valid()) {
            const auto y=m.Read(0x55b604+4*selected);
            if(y && std::isfinite(std::bit_cast<float>(*y))) {
                // Move the chosen destination on the same ten-frame 60 Hz
                // clock as Cancel/Overwrite. Native motion is delayed and
                // linear; rebase every draw so it cannot tug the card after
                // our ease-out has reached its original resting position.
                const float start=56.f+60*m.Read(Owner+0x1c).value_or(0);
                const float desired=56+(start-56)*EasePosition(1-progress,1,0,l.stone_ease_out);
                bounds[selected].slide_y=desired-std::bit_cast<float>(*y);
                bounds[selected].moving=true;
            }
        }
    }
    if((state==4 || state==7) && action_opening.load()) {
        // The native file exits start four frames apart and use a separate
        // timer from the action entrance. Drive both departures from the same
        // presentation clock, including the accepted-input frame. Rebase each
        // live card so native staggering/parking cannot alter its visible speed.
        const float progress=std::min(action_frames.load()/9.f,1.f);
        const unsigned selected=m.Read(Owner+0x14).value_or(3);
        for(unsigned i=0;i<3;++i)if(i!=selected && bounds[i].Valid()) {
            const auto x=m.Read(0x55b5f8+4*i);if(!x)continue;
            const float current=std::bit_cast<float>(*x);if(!std::isfinite(current))continue;
            auto& b=bounds[i];b.moving=true;b.slide_x=16-current;
            const auto pose=PartTransform(l,i,bounds);
            const float right=pose.x+pose.ax+(b.right-pose.ax)*pose.sx;
            const float scale=PausePrompts::Valid(l.stones)?l.stones.scale:1.f;
            b.slide_x-=progress*std::max(400.f,(right+24)/scale);
        }
    }
    if(exchanging && (page_exchange==Exchange::Copy || page_exchange==Exchange::BackCopy)) {
        const unsigned selected=m.Read(Owner+0x14).value_or(3);unsigned row=0;
        for(unsigned i=0;i<3;++i) {
            auto& b=bounds[i];if(!b.Valid())continue;
            const float offset=i==selected?(returning?-incoming:outgoing):(returning?-outgoing:incoming);
            const float y=i==selected?56.f:56.f+60*row++;
            const auto x_word=m.Read(0x55b5f8+4*i),y_word=m.Read(0x55b604+4*i);
            if(!x_word || !y_word)continue;
            const float x=std::bit_cast<float>(*x_word),current_y=std::bit_cast<float>(*y_word);
            if(!std::isfinite(x) || !std::isfinite(current_y))continue;
            // Native face edges sit outside their scalar anchor. Rebasing by
            // the edge instead produces a visible jump at the settled page.
            b.slide_x=16-x;b.slide_y=y-current_y;b.moving=true;
            const auto pose=PartTransform(l,i,bounds);
            const float left=pose.x+pose.ax+(b.left-pose.ax)*pose.sx;
            const float right=pose.x+pose.ax+(b.right-pose.ax)*pose.sx;
            const float scale=PausePrompts::Valid(l.stones)?l.stones.scale:1.f;
            const float travel=std::max(500.f,(offset<0?right+24:424-left)/scale);
            b.slide_x+=offset*travel/500;
        }
    }
    if(state==7 || state==9 || (state==4 && action_opening.load()))
        for(unsigned i=Start;i<=Erase;++i)bounds[i].moving=true;
    if(state>=36 && state<=39) {
        bounds[DeleteCancel].moving=true;bounds[DeleteErase].moving=true;
    }
    // Erase uses the upper "Delete this file?" prompt. Its unused lower
    // message frame can be parked partly inside the wider viewport.
    if(state>=34 && state<=39)bounds[LowerPrompt]={};
    if(l.saved_file_slot<3) {
        // Every element starts from the fully settled native action page. Move
        // the complete selected stone and all three actions by one screen delta.
        for(unsigned i=0;i<3;++i)if(i!=l.saved_file_slot)bounds[i]={};
        float travel=0;
        for(unsigned part:{l.saved_file_slot,unsigned(Start),unsigned(Copy),unsigned(Erase)}) {
            const auto& b=bounds[part];if(!b.Valid())continue;
            const auto t=PartTransform(l,part,bounds);
            const float left=t.x+t.ax+(b.left-t.ax)*t.sx;
            travel=std::max(travel,424-left);
        }
        const float dx=travel*std::clamp(l.saved_file_remaining,0.f,1.f);
        const float group=PausePrompts::Valid(l.stones)?l.stones.scale:1.f;
        for(unsigned part:{l.saved_file_slot,unsigned(Start),unsigned(Copy),unsigned(Erase)}) {
            bounds[part].slide_x+=part<=Stone3?dx/group:dx;
        }
    }
    if(native_bounds)*native_bounds=bounds;
    // Compare complete faces before transforming any of their slices. A
    // parked frame's left cap can be closer than the old per-quad threshold.
    std::array<bool,QuadCount> parked{};
    for(unsigned first:{134u,141u,148u,155u}) {
        const auto part=QuadPart(first);
        for(unsigned face:{first,first+4}) {
            const bool hidden=std::abs(Bounds(out,face,face+2).X()-bounds[part].X())>350;
            for(unsigned q=face;q<face+3;++q)parked[q]=hidden;
        }
    }
    for(unsigned q=0;q<QuadCount;++q) {
        if(BackQuad(q)!=bottom){Hide(out,q);continue;}
        if(bottom)continue;
        if(q==171 && (state<12 || state>23 || state==15)) {
            // Copy's decorative direction arrow is not a selection cursor.
            // It belongs only to copy selection/confirmation. Dismissing the
            // success page switches 32 -> 1 before native atlas cleanup, so
            // hiding only save states lets the old arrow flash on the return.
            // Also exclude Back from copy (15), regardless of editor offsets.
            Hide(out,q);
            for(unsigned v=0;v<4;++v)out.colors[q*16+v*4+3]=0;
            continue;
        }
        const auto part=QuadPart(q);
        // Once the erase page has settled, only its prompt, selected save and
        // two confirmation cards belong here. Native parked cards may otherwise
        // expose a border in the wider viewport after editor transforms.
        if((state==37 || state==39) && part!=Prompt && part!=DeleteCancel && part!=DeleteErase &&
           !(part<=Stone3 && part==m.Read(Owner+0x14).value_or(3))) {
            Hide(out,q);continue;
        }
        if(parked[q] || (state>=34 && state<=39 && q>=148 && q<=161)) {
            Hide(out,q);continue;
        }
        if(part==Count) {if(!Clip(out,q))continue;}
        else if(!bounds[part].Valid() || (!bounds[part].Visible() && !bounds[part].moving)) {
            Hide(out,q);continue;
        }
        if(part>=DeleteCancel && part<=OverwriteAccept) {
            // Native pressed faces are parked independently at +500. Moving a
            // whole editable card must never bring that unused face into view.
            const auto quad=Bounds(out,q,q);
            if(std::abs(quad.X()-bounds[part].X())>350){Hide(out,q);continue;}
        }
        // Submit the complete moving card. The destination viewport clips it
        // after layout, just as it clips the native lettering on that card.
        const auto t=PartTransform(l,part,bounds);
        for(unsigned v=0;v<4;++v) {
            auto& x=out.positions[q*12+v*3];auto& y=out.positions[q*12+v*3+1];
            // Centering is shared with native lettering in the fresh view.
            x=t.x-40+t.ax+(x-t.ax)*t.sx;
            y=t.y+t.ay+(y-t.ay)*t.sy;
        }
    }
    out.count=QuadCount;
    if(RoomJoin::phase==RoomJoin::Phase::ProfileLeaving)
        for(unsigned i=3;i<out.count*16;i+=4)out.colors[i]*=RoomJoin::opacity.load();
    if(RoomJoin::HideFiles())for(unsigned q=0;q<QuadCount;++q) {
        if(!RoomJoin::WaitingScreen() || q>=6)Hide(out,q);
        else for(unsigned v=0;v<4;++v)out.colors[q*16+v*4+3]*=RoomJoin::opacity.load();
    }
    if(RoomJoin::profile_visible.load() && !RoomJoin::stopping.load() &&
       (RoomJoin::hosting.load() || RoomJoin::host_opening.load()))for(unsigned q=0;q<QuadCount;++q) {
        const auto part=QuadPart(q);
        if(part==Copy || part==Erase)for(unsigned v=0;v<4;++v)out.colors[q*16+v*4+3]*=.5f;
    }
    if(RoomJoin::GuestActive())for(unsigned q=0;q<QuadCount;++q) {
        const auto part=QuadPart(q);
        if(part>=Start && part<=Erase)Hide(out,q);
    }
    return out;
}
// Shadows use the final card geometry, so every saved pose, group scale and
// staggered slide applies identically. Draw after the backdrop, before the cards.
using StoneShadowGeometry=PausePrompts::QuadGeometry<90>;
inline void AddStoneShadow(StoneShadowGeometry& out,const Rect& b,float alpha,float opacity,float softness=-1) {
    if(!b.Valid() || !std::isfinite(alpha) || !std::isfinite(opacity))return;
    alpha=std::clamp(alpha,0.f,1.f)*std::clamp(opacity,0.f,1.f);
    if(alpha<=0)return;
    const float w=b.right-b.left,h=b.bottom-b.top,d=softness>=0?softness:std::min(w,h)*.16f;
    const auto add=[&](float x,float y,float width,float height,std::array<float,4> a) {
        out.Add(x,y,width,height,.5f,.5f,.5f,.5f);
        for(unsigned v=0;v<4;++v)out.colors[(out.count-1)*16+v*4+3]=a[v]*alpha;
    };
    add(b.left-d,b.top,d,h,{0,1,0,1});
    add(b.right,b.top,d,h,{1,0,1,0});
    add(b.left,b.top-d,w,d,{0,0,1,1});
    add(b.left,b.bottom,w,d,{1,1,0,0});
    add(b.left-d,b.top-d,d,d,{0,0,0,1});
    add(b.right,b.top-d,d,d,{0,0,1,0});
    add(b.left-d,b.bottom,d,d,{0,1,0,0});
    add(b.right,b.bottom,d,d,{1,0,0,0});
    // Cover transparent chamfered corners as well as the exterior edges.
    add(b.left,b.top,w,h,{1,1,1,1});
}
inline StoneShadowGeometry StoneShadows(const Geometry& atlas,float opacity=.55f) {
    StoneShadowGeometry out;
    // Three save slots and Start/Copy/Erase, all in their final animated poses.
    for(unsigned first:{6u,43u,80u,122u,126u,130u,134u,141u,148u,155u}) {
        float alpha=1;
        for(unsigned q=first;q<first+3;++q)for(unsigned v=0;v<4;++v)
            alpha=std::min(alpha,std::clamp(atlas.colors[q*16+v*4+3],0.f,1.f));
        AddStoneShadow(out,Bounds(atlas,first,first+2),alpha,opacity);
    }
    return out;
}
inline StoneShadowGeometry QuestShadows(const ChoiceGeometry& cards,float opacity) {
    StoneShadowGeometry out;
    for(unsigned q:{1u,3u}) {
        auto b=Bounds(cards,q,q);
        // The shared shadow draw centers a 320px canvas, while quest stones
        // already use top coordinates. Compensate for that common +40 offset.
        b.left-=40;b.right-=40;
        AddStoneShadow(out,b,cards.colors[q*16+3],opacity);
    }
    return out;
}
using BadgeGeometry=PausePrompts::QuadGeometry<3>;
struct RandomizerBadgeState {
    unsigned visible=0;
    template<class M> void Frame(M& m) {
        const auto selected=m.Read(Owner+0x14).value_or(3);
        const bool deleting=DeletingFile(m);
        for(unsigned slot=0;slot<3;++slot) {
            const unsigned bit=1u<<slot;
            const bool live=MasterQuest::Occupied(m,slot) &&
                MasterQuest::Byte(m,MasterQuest::Saves+slot*MasterQuest::Stride+0x2f)==0xa7;
            // The native menu keeps its occupied-card snapshot until erase I/O
            // completes, then refreshes this array and creates "New Game".
            // Keep only the presentation badge alive, never the deleted save.
            const bool old_card=deleting && selected==slot &&
                m.Read(0x55b53c+slot*4).value_or(0)!=0;
            if(live)visible|=bit;
            else if(!old_card)visible&=~bit;
        }
    }
};
template<class M> void RandomizerBadges(M& m,FileStatusText::Geometry& out,
        const FileStatusText::Assets& font,const Geometry& atlas,const Layout& layout,
        const std::array<Rect,Count>& bounds,unsigned retained=0) {
    constexpr std::string_view label="RANDO";
    constexpr float size=.7f,gap=5;
    const auto pose=PausePrompts::Valid(layout.randomizer_badge)?layout.randomizer_badge:PausePrompts::Pose{};
    float width=1;
    for(char c:label) {
        const auto glyph=font.Find(c);if(!glyph || glyph->code!=unsigned(c))return;
        width+=glyph->width+1;
    }
    for(unsigned slot=0;slot<3;++slot) {
        if(!(retained&(1u<<slot)) && (!MasterQuest::Occupied(m,slot) ||
           MasterQuest::Byte(m,MasterQuest::Saves+slot*MasterQuest::Stride+0x2f)!=0xa7))continue;
        const unsigned first=6+slot*37;
        if(!Bounds(atlas,first,first+2).Valid())continue;
        float alpha=1;
        for(unsigned q=first;q<first+3;++q)for(unsigned v=0;v<4;++v)
            alpha=std::min(alpha,std::clamp(atlas.colors[q*16+v*4+3],0.f,1.f));
        // Quads 11..30 are the twenty hearts in each 37-quad file block.
        // Use the first heart's final geometry so the label stays beside it
        // through selection, card slides, group scaling and custom positions.
        const auto heart=Bounds(atlas,first+5,first+5);
        if(!heart.Valid() || alpha<=0)continue;
        const auto t=PartTransform(layout,slot,bounds);
        if(t.sx<=0 || t.sy<=0)continue;
        const float sx=t.sx*size*pose.scale,sy=t.sy*size*pose.scale;
        const float left=heart.left+40+(pose.x-gap)*t.sx-width*sx;
        const float y=heart.Y()+pose.y*t.sy-10*sy;
        float pen=0;
        for(char c:label) {
            const auto glyph=font.Find(c);
            const float x=left+pen*sx;
            // Eight dark outline offsets followed by the colored face, matching the
            // native name's outline. All pieces share the card's pose and fade.
            const auto add=[&](float dx,float dy,bool face) {
                if(out.count+1>out.colors.size()/16)return;
                out.Add(x+dx*sx,y+dy*sy,(glyph->width+2)*sx,20*sy,
                    float(glyph->x)/FileStatusText::Width,(glyph->y+.5f)/FileStatusText::Height,
                    float(glyph->x+glyph->width+2)/FileStatusText::Width,(glyph->y+19.5f)/FileStatusText::Height);
                for(unsigned v=0;v<4;++v) {
                    const unsigned c=(out.count-1)*16+v*4;
                    for(unsigned channel=0;channel<3;++channel)
                        out.colors[c+channel]=face?layout.randomizer_badge_color[channel]:.08f;
                    out.colors[c+3]=alpha;
                }
            };
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(dx || dy)add(dx,dy,false);
            add(0,0,true);
            pen+=glyph->width+1;
        }
    }
}
template<class M> BadgeGeometry QuestBadges(M& m,const Geometry& atlas) {
    BadgeGeometry out;
    for(unsigned slot=0;slot<3;++slot) {
        if(!MasterQuest::Occupied(m,slot) || !MasterQuest::Mode(m,slot))continue;
        const unsigned first=6+slot*37;
        const auto b=Bounds(atlas,first,first+2);
        if(!b.Valid())continue;
        const float sx=(b.right-b.left)/288,sy=(b.bottom-b.top)/44;
        float alpha=1;
        for(unsigned q=first;q<first+3;++q)for(unsigned v=0;v<4;++v)
            alpha=std::min(alpha,std::clamp(atlas.colors[q*16+v*4+3],0.f,1.f));
        if(alpha<=0)continue;
        // Native names start at stone X+28. The 16px badge sits in the
        // adjacent left margin, with the same full card transform and fade.
        // Match the D-pad ocarina's UV orientation and exact 42px atlas cell.
        out.Add(b.left+8*sx,b.top+6*sy,16*sx,16*sy,
            192.5f/DPad::TextureWidth,(DPad::TextureHeight-41.5f)/DPad::TextureHeight,
            233.5f/DPad::TextureWidth,(DPad::TextureHeight-.5f)/DPad::TextureHeight);
        for(unsigned i=3;i<16;i+=4)out.colors[(out.count-1)*16+i]=alpha;
    }
    return out;
}
inline constexpr auto ShadowPixels=[] {std::array<U32,64> a{};a.fill(0x000000ff);return a;}();
using SeedShadowGeometry=PausePrompts::QuadGeometry<324>;
inline SeedShadowGeometry SeedBackdrop(const FileStatusText::Geometry& text,const SeedShadow& s) {
    SeedShadowGeometry out;
    if(!s.enabled || !text.count)return out;
    for(float v:{s.x,s.y,s.width,s.height,s.radius,s.softness,s.opacity})
        if(!std::isfinite(v))return out;
    const auto b=Bounds(text,0,text.count-1);
    if(!b.Valid())return out;
    const float hw=(b.right-b.left+16)*std::clamp(s.width,.1f,4.f)/2;
    const float hh=(b.bottom-b.top+8)*std::clamp(s.height,.1f,4.f)/2;
    const float radius=std::clamp(s.radius,0.f,std::min(hw,hh));
    const float softness=std::clamp(s.softness,0.f,40.f);
    const float opacity=std::clamp(s.opacity,0.f,1.f);
    const float cx=b.X()+s.x,cy=b.Y()+s.y;
    // A filled rounded contour plus disjoint feather rings avoids dark seams
    // from overlapping translucent rectangles, at every aspect ratio.
    constexpr unsigned Points=36,Rings=8;
    const auto point=[&](unsigned i,float spread) {
        i%=Points;
        const unsigned corner=i/9,step=i%9;
        const float angle=(corner+step/8.f)*1.57079632679f;
        return std::array{cx+(corner==0 || corner==3?1:-1)*(hw-radius)+(radius+spread)*std::cos(angle),
            cy+(corner<2?1:-1)*(hh-radius)+(radius+spread)*std::sin(angle)};
    };
    const auto quad=[&](std::array<float,2> a,std::array<float,2> b,
            std::array<float,2> c,std::array<float,2> d,float inner,float outer) {
        out.Add(0,0,1,1,.5f,.5f,.5f,.5f);
        const unsigned q=out.count-1;
        const std::array vertices{a,b,c,d};
        for(unsigned v=0;v<4;++v) {
            out.positions[q*12+v*3]=vertices[v][0];
            out.positions[q*12+v*3+1]=vertices[v][1];
            out.colors[q*16+v*4+3]=(v<2?inner:outer)*opacity;
        }
    };
    for(unsigned i=0;i<Points;++i) {
        quad({cx,cy},{cx,cy},point(i+1,0),point(i,0),1,1);
        for(unsigned ring=0;softness>0 && ring<Rings;++ring) {
            const float t=ring/float(Rings),u=(ring+1)/float(Rings);
            quad(point(i+1,t*softness),point(i,t*softness),
                point(i+1,u*softness),point(i,u*softness),1-t*t*(3-2*t),1-u*u*(3-2*u));
        }
    }
    return out;
}
// X places the left edge of the button icon. Labels extend toward the middle.
inline float VisiblePromptX(float x,float scale) {
    const float margin=PausePrompts::SafeLeft();
    return std::clamp(x,margin,std::min(400.f,400-margin-16*scale));
}
struct ButtonLabel {float button,text,text_scale;};
inline ButtonLabel PlaceButtonLabel(float x,float scale,float width) {
    x=VisiblePromptX(x,scale);
    const float gap=4*scale,margin=PausePrompts::SafeLeft();
    const bool right=x+8*scale>=200;
    const float space=right?x-gap-margin:400-margin-(x+16*scale+gap);
    const float text_scale=width>0?std::min(scale,space/width):scale;
    return {x,right?x-gap-width*text_scale:x+16*scale+gap,text_scale};
}
enum class PromptAnchor { Button, Group };
inline PausePrompts::Pair Prompts(const Layout& l,const PausePrompts::Assets& assets,float alpha,
                                  bool back_only=false,PromptAnchor anchor=PromptAnchor::Button) {
    PausePrompts::Pair out;
    for(unsigned i=0;i<2;++i) {
        if(back_only && i==0)continue;
        const auto p=l.prompts[i];if(!PausePrompts::Valid(p))continue;
        const unsigned ib=out.icons.count,tb=out.text.count;
        const float tx=i?144:128,s=p.scale;
        const unsigned row=i?PausePrompts::FileBackText:PausePrompts::FileSelectText;
        const float width=PausePrompts::LabelWidth(assets,row);
        auto edge=PlaceButtonLabel(p.x,s,width);
        if(anchor==PromptAnchor::Group) {
            const auto group=PausePrompts::EdgePrompt(p.x,s,width,
                PausePrompts::LabelColumn(assets,{PausePrompts::FileSelectText,PausePrompts::FileBackText}));
            edge={group.button,group.text,s};
        }
        const float ts=edge.text_scale;
        out.icons.Add(edge.button,p.y,16*s,16*s,(tx+.5f)/256,95.5f/128,(tx+15.5f)/256,80.5f/128);
        for(unsigned j=0;j<assets.glyphs[row].size();++j) {
            const auto glyph=assets.glyphs[row][j];if(!glyph.width)continue;
            const float start=glyph.x-1;
            const float shift=j*(1.f-assets.native_spacing);
            out.text.Add(edge.text+(start+shift)*.75f*ts,p.y+8*s-7.5f*ts,
                (glyph.width+2)*.75f*ts,15*ts,start/PausePrompts::TextWidth,
                (row*PausePrompts::TextRowHeight+.5f)/PausePrompts::TextHeight,
                (start+glyph.width+2)/PausePrompts::TextWidth,(row*PausePrompts::TextRowHeight+19.5f)/PausePrompts::TextHeight);
        }
        PausePrompts::KeepOnScreen(out,ib,tb);
    }
    const auto fade=[&](auto& g) {for(unsigned i=3;i<g.count*16;i+=4)g.colors[i]=std::clamp(alpha,0.f,1.f);};
    fade(out.icons);fade(out.text);return out;
}
// Keyboard prompts share the file-select anchors, with independent editor offsets.
inline PausePrompts::Pose NamePromptPose(const Layout& l,unsigned i) {
    if(i>=l.prompts.size())return {};
    const auto base=PausePrompts::Valid(l.prompts[i])?l.prompts[i]:Layout{}.prompts[i];
    const auto extra=PausePrompts::Valid(l.name_prompts[i])?l.name_prompts[i]:PausePrompts::Pose{};
    return {base.x+extra.x,base.y+extra.y,base.scale*extra.scale};
}
template<class M> bool StandardPromptsVisible(M& m) {
    if(RoomJoin::HideFiles())return false;
    // Suppress from quest acceptance through keyboard teardown, even before
    // its models exist. Loading alpha falls back to zero before name entry.
    const auto name_state=m.Read(NameOwner+4).value_or(0);
    if(name_state>0 && name_state<=10)return false;
    if(m.Read(Owner+0x10)==5)return false; // Native handoff to name entry.
    return !(MasterQuest::choosing.load() && MasterQuest::leaving.load() &&
        !MasterQuest::cancelling.load() && !MasterQuest::generation_return.load());
}
struct NamePromptTransition {
    float keyboard=0,standard=1;
    void Frame(bool typing,bool file_prompts,float entrance,bool saving=false) {
        constexpr float step=1.f/9; // 150 ms at the file menu's 60 Hz draw rate.
        if(saving) {keyboard=standard=0;return;}
        keyboard=typing?std::min(std::clamp(entrance,0.f,1.f),keyboard+step):
            std::max(0.f,keyboard-step);
        // Never overlap the two labels on the same button. Preserve the
        // existing immediate suppression when first entering the keyboard.
        standard=file_prompts && keyboard==0?std::min(1.f,standard+step):0;
    }
};
inline float ScreenLeft() {return Widescreen::Active()?(400-240*Widescreen::Aspect)/2:0;}
inline PausePrompts::Pair WaitingPrompts(const Layout& layout,const PausePrompts::Assets& assets,float alpha) {
    auto waiting=layout;
    // Keep the usual B size and vertical alignment without changing its saved pose.
    waiting.prompts[1].x=ScreenLeft()+10;
    return Prompts(waiting,assets,alpha,true);
}
inline PausePrompts::Pose FitLabel(PausePrompts::Pose p,std::string_view label) {
    const auto font=FileStatusText::GetAssets();if(!font)return p;
    const float width=21+FileStatusText::Measure(*font,label),lo=PausePrompts::SafeLeft(),hi=400-lo;
    p.scale=std::min({p.scale,(hi-lo)/width,236.f/16});
    p.x=std::clamp(p.x,lo,hi-width*p.scale);
    p.y=std::clamp(p.y,2.f,238-16*p.scale);
    return p;
}
inline std::string_view WidestPrompt(std::initializer_list<std::string_view> labels) {
    const auto font=FileStatusText::GetAssets();std::string_view longest;float width=-1;
    for(const auto label:labels) {
        const float next=font?FileStatusText::Measure(*font,label):float(label.size());
        if(next>width){width=next;longest=label;}
    }
    return longest;
}
inline PausePrompts::EdgePositions StatusPromptEdge(const FileStatusText::Assets& font,
        const PausePrompts::Pose& pose,std::string_view label,std::string_view column) {
    // Text adds one final antialiasing pixel beyond its last glyph advance.
    return PausePrompts::EdgePrompt(pose.x,pose.scale,FileStatusText::Measure(font,label)+.75f,
        FileStatusText::Measure(font,column)+.75f);
}
inline std::string_view RandomizerPromptColumn() {
    return WidestPrompt({UILanguage::Randomizer(false),UILanguage::Randomizer(true)});
}
inline std::string_view NamePromptLabel(unsigned i) {
    return i?UILanguage::Pick("Clear","Effacer","Borrar"):UILanguage::Pick("Choose","Choisir","Elegir");
}
inline PausePrompts::Pose RandomizerPrompt(const Layout& layout) {
    const auto p=PausePrompts::Valid(layout.randomizer_prompt)?layout.randomizer_prompt:PausePrompts::Pose{};
    return FitLabel({400-ScreenLeft()-124+p.x,198+p.y+MasterQuest::RandomizerHop()*p.scale,p.scale},
        RandomizerPromptColumn());
}
inline PausePrompts::Pose JoinPrompt(const Layout& layout) {
    const auto p=PausePrompts::Valid(layout.join_prompt)?layout.join_prompt:PausePrompts::Pose{};
    return FitLabel({400-ScreenLeft()-124+p.x,174+p.y,p.scale},UILanguage::Join());
}
inline PausePrompts::Pose ProfilePrompt(const Layout& layout,bool host) {
    const auto pose=host?layout.host_prompt:layout.profile_join_prompt;
    const auto p=PausePrompts::Valid(pose)?pose:PausePrompts::Pose{};
    float hop=0;
    if(host) {
        const auto elapsed=std::max(std::int64_t{0},MasterQuest::cursor_us.load()-RoomJoin::host_hop_started.load());
        const bool up=elapsed<MasterQuest::HopUpUs;
        const float t=std::clamp(float(up?elapsed:elapsed-MasterQuest::HopUpUs)/
            float(up?MasterQuest::HopUpUs:MasterQuest::HopDownUs),0.f,1.f);
        hop=-3*(up?t*t*(3-2*t):1-t*t*(3-2*t));
    }
    return FitLabel({400-ScreenLeft()-124+p.x,(host?150.f:174.f)+p.y+hop*p.scale,p.scale},
        host?UILanguage::Host((RoomJoin::hosting.load() || RoomJoin::host_opening.load()) && !RoomJoin::stopping.load()):UILanguage::Join());
}
struct JoinPromptFade {
    float amount=0;
    void Frame(bool quest,bool enabled) {
        if(!quest) {amount=0;return;}
        amount=std::clamp(amount+(enabled?1.f:-1.f)/9,0.f,1.f);
    }
    float Alpha() const {return amount*amount*(3-2*amount)*MasterQuest::RandomizerAlpha();}
};
inline FileStatusText::Geometry SeedDetails(const FileStatusText::Assets& font,
        const Layout& layout,const std::string& seed,const std::string& room_id) {
    const auto pose=PausePrompts::Valid(layout.seed_text)?layout.seed_text:PausePrompts::Pose{};
    const float x=ScreenLeft()+10+pose.x,bottom=218+pose.y;
    const float width=(400-2*ScreenLeft()-20)*pose.scale;
    auto out=FileStatusText::Text(font,std::string(UILanguage::Pick("Seed: ","Graine : ","Semilla: "))+seed,x,bottom,width,.75f*pose.scale);
    if(!room_id.empty()) {
        const float gap=std::isfinite(layout.room_id_gap)?std::clamp(layout.room_id_gap,0.f,60.f):3.f;
        const auto line=FileStatusText::Text(font,std::string(UILanguage::Pick("Room ID: ","ID du salon : ","ID de sala: "))+room_id,x,bottom+(15+gap)*pose.scale,width,.75f*pose.scale);
        const unsigned n=out.count;
        if((n+line.count)*12<=out.positions.size()) {
            std::copy_n(line.positions.begin(),line.count*12,out.positions.begin()+n*12);
            std::copy_n(line.uvs.begin(),line.count*8,out.uvs.begin()+n*8);
            std::copy_n(line.colors.begin(),line.count*16,out.colors.begin()+n*16);
            out.count+=line.count;
        }
    }
    return out;
}
template<class M> std::string SelectedSeed(M& m) {
    const auto state=m.Read(Owner+0x10).value_or(0);
    // Native action menu, including its entrance. Exclude copy/erase targets,
    // name entry and ordinary files even if this slot previously held a seed.
    if(state!=4 && state!=7 && state!=8 && state!=10)return {};
    const auto slot=m.Read(Owner+0x14).value_or(3);
    if(!MasterQuest::Occupied(m,slot) ||
       MasterQuest::Byte(m,MasterQuest::Saves+slot*MasterQuest::Stride+0x2f)!=0xa7)return {};
    return Randomizer::FileStatus::Seed(slot);
}
// Name entry is a native keyboard atlas plus an atlas of frames/action keys.
// The blue backing is baked into the keyboard art. Split its key rectangles
// only when the backing has a different pose; the default uses the exact quad.
using NameGeometry=PausePrompts::QuadGeometry<256>;
constexpr std::array<Rect,NameCount> NameBounds{{
    {-2,34,322,200},{12,-2,308,38},{2,202,102,238},{218,202,318,238},{-2,34,322,200},
    {34,94,158,146},{162,94,286,146}}};
inline float name_touch_progress=1;
inline bool name_touch_ready=false;
inline bool name_touch_confirmation=false;
inline std::vector<NameKeyboard::Key> name_touch_keys;
constexpr unsigned NameSlideFrames=12; // 200 ms at the file menu's 60 Hz presentation.
inline float NameEase(float t) {t=1-std::clamp(t,0.f,1.f);return 1-t*t*t;}
inline ScreenModels::Transform NameTransform(const Layout& l,unsigned part,float progress=1) {
    if(part>=NameCount)return {40,0};
    auto p=l.name_parts[part];if(!PausePrompts::Valid(p))p={};
    if(part==Cancel || part==Okay || part==ConfirmNo || part==ConfirmYes)
        p.scale*=FocusScale(l,NameFocus+part);
    const auto b=NameBounds[part];
    const float remaining=1-NameEase(progress);
    // Travel is large enough to clear the viewport after repositioning/scaling.
    // One horizontal offset keeps the name box, keyboard and its shadow together,
    // even when their saved positions/scales differ.
    float travel=400;
    for(unsigned moving:{Keyboard,KeyboardBackground,NameBox}) {
        const auto pose=PausePrompts::Valid(l.name_parts[moving])?l.name_parts[moving]:PausePrompts::Pose{};
        const auto bounds=NameBounds[moving];
        travel=std::max(travel,424-(40+pose.x+bounds.X()-(bounds.right-bounds.left)*pose.scale/2));
    }
    const float down=std::max(48.f,264-(p.y+b.Y()-(b.bottom-b.top)*p.scale/2));
    return {40+p.x+((part==Keyboard || part==KeyboardBackground || part==NameBox)?travel*remaining:0),
        p.y+((part==Cancel || part==Okay)?down*remaining:0),p.scale,p.scale,b.X(),b.Y(),true};
}
template<class M> std::vector<NameKeyboard::Key> CustomKeys(M& m,const Layout& l) {
    return l.keyboard.enabled && FileStatusText::GetAssets()?NameKeyboard::Keys(m,l.keyboard):
        std::vector<NameKeyboard::Key>{};
}
template<class M,class R,class FP> void KeyboardCursor(M& m,R& r,FP& fp,unsigned site,bool supported=true) {
    r[5]=r[0]; // Displaced MOV; native prologue already saved the caller's r5.
    if(!supported || m.Read(NameOwner)!=r[0] || m.Read(NameOwner+4)!=4)return;
    for(const auto& key:CustomKeys(m,GetLayout()))if(key.selected) {
        // Native corners are 16px square. Increase their separation instead
        // of scaling their art. Word controls need additional text clearance.
        const auto [width,height]=NameKeyboard::CursorSize(key);
        fp[0]=std::bit_cast<U32>(site==11?width-16:key.X()-width/2);
        fp[1]=std::bit_cast<U32>(site==11?height-16:key.Y()-height/2);
        break;
    }
}
inline Rect CustomKeyboardBounds(const Layout& l) {
    // Both pages share tabs, twelve columns and the bottom-right OK anchor.
    // Content/control labels must not resize the shadow when switching pages.
    Rect b{INFINITY,INFINITY,-INFINITY,-INFINITY};
    constexpr std::array<std::array<float,3>,4> anchors{{
        {94,50,128},{225,50,130},{160-11*(NameKeyboard::Columns-1),77,22},
        {160+11*(NameKeyboard::Columns-1),181,60}}};
    for(const auto& anchor:anchors) {
        NameKeyboard::Key key;key.display_x=anchor[0];key.display_y=anchor[1];key.display_width=anchor[2];
        const auto c=NameKeyboard::Center(key,l.keyboard);
        b.left=std::min(b.left,c[0]-key.display_width/2);b.right=std::max(b.right,c[0]+key.display_width/2);
        b.top=std::min(b.top,c[1]-13);b.bottom=std::max(b.bottom,c[1]+13);
    }
    return b;
}
template<class M> FileStatusText::Geometry KeyboardText(M& m,const Layout& l,float progress,
        NameKeyboard::GrowthAnimation* animation=nullptr) {
    if(progress<=0) {if(animation)animation->Reset();return {};}
    const auto font=FileStatusText::GetAssets();auto keys=CustomKeys(m,l);
    if(!font || keys.empty()) {if(animation)animation->Reset();return {};}
    if(animation)animation->Apply(keys,l.keyboard);
    const unsigned atlas=m.Read(NameOwner+8)==3?1:0;
    const U32 helper=m.Read(NameOwner+0x6c+atlas*4).value_or(0);
    PausePrompts::QuadGeometry<1> raw;
    if(!TitleScreen::ReadFloats(m,m.Read(helper+0x10).value_or(0),raw.positions))return {};
    auto b=Bounds(raw,0,0);
    const auto state=m.Read(NameOwner+4).value_or(0);
    const bool entering=progress<1 && (state==1 || state==2 || state==4);
    if(entering)b=NameBounds[Keyboard];
    else if(!b.Visible())return {};
    const auto color=m.Read(m.Read(helper+0x18).value_or(0)+12);
    if(!color || !std::isfinite(std::bit_cast<float>(*color)))return {};
    const float a=entering?1.f:std::clamp(std::bit_cast<float>(*color),0.f,1.f);
    auto g=NameKeyboard::Draw(*font,keys,l.keyboard);
    const auto t=NameTransform(l,Keyboard,progress);
    for(unsigned q=0;q<g.count;++q)for(unsigned v=0;v<4;++v) {
        auto& x=g.positions[q*12+v*3];auto& y=g.positions[q*12+v*3+1];
        x=t.x+t.ax+(x+b.left-NameBounds[Keyboard].left-t.ax)*t.sx;
        y=t.y+t.ay+(y+b.top-NameBounds[Keyboard].top-t.ay)*t.sy;
        g.colors[q*16+v*4+3]=a;
    }
    return g;
}
template<class M> StoneShadowGeometry NameShadow(M& m,const Layout& l,float progress) {
    StoneShadowGeometry out;const auto s=l.keyboard_shadow;
    if(progress<=0)return out;
    if(!s.enabled || !std::isfinite(s.opacity) || !std::isfinite(s.softness) ||
       !std::isfinite(s.spread) || !std::isfinite(s.x) || !std::isfinite(s.y))return out;
    for(float padding:{s.padding_top,s.padding_left,s.padding_bottom,s.padding_right})
        if(!std::isfinite(padding))return out;
    const unsigned mode=m.Read(NameOwner+8).value_or(0);
    const bool western=m.Read(NameOwner+0x40).value_or(0)!=0;
    const unsigned keyboard=mode<(western?3u:2u)?0:1;
    const U32 helper=m.Read(NameOwner+0x6c+keyboard*4).value_or(0);
    PausePrompts::QuadGeometry<1> raw;
    if(!TitleScreen::ReadFloats(m,m.Read(helper+0x10).value_or(0),raw.positions))return out;
    auto b=Bounds(raw,0,0);if(!b.Valid())return out;
    const auto state=m.Read(NameOwner+4).value_or(0);
    const bool entering=progress<1 && (state==1 || state==2 || state==4);
    if(entering)b=NameBounds[Keyboard];
    else if(!b.Visible())return out;
    if(const auto keys=CustomKeys(m,l);!keys.empty()) {
        const auto custom=CustomKeyboardBounds(l);
        const float dx=b.left-NameBounds[Keyboard].left,dy=b.top-NameBounds[Keyboard].top;
        b={custom.left+dx,custom.top+dy,custom.right+dx,custom.bottom+dy};
    }
    const U32 colors=m.Read(helper+0x18).value_or(0);
    if(!TitleScreen::Pointer(colors,16))return out;
    const auto alpha=m.Read(colors+12);if(!alpha)return out;
    const float spread=std::clamp(s.spread,-20.f,40.f);
    b.left+=s.x-spread;b.right+=s.x+spread;b.top+=s.y-spread;b.bottom+=s.y+spread;
    b.left-=std::clamp(s.padding_left,-240.f,400.f);
    b.right+=std::clamp(s.padding_right,-240.f,400.f);
    b.top-=std::clamp(s.padding_top,-240.f,400.f);
    b.bottom+=std::clamp(s.padding_bottom,-240.f,400.f);
    AddStoneShadow(out,b,entering?1.f:std::bit_cast<float>(*alpha),s.opacity,std::clamp(s.softness,0.f,40.f));
    const auto t=NameTransform(l,Keyboard,progress);
    for(unsigned q=0;q<out.count;++q)for(unsigned v=0;v<4;++v) {
        auto& x=out.positions[q*12+v*3];auto& y=out.positions[q*12+v*3+1];
        x=t.x-40+t.ax+(x-t.ax)*t.sx;y=t.y+t.ay+(y-t.ay)*t.sy;
    }
    return out;
}
inline unsigned NameQuadPart(unsigned q) {
    if(q==0)return Keyboard;
    if((q>=1 && q<=7) || (q>=16 && q<=25) || q==46 || (q>=48 && q<=56))return NameBox;
    if((q>=8 && q<=11) || (q>=34 && q<=36))return Cancel;
    if((q>=12 && q<=15) || (q>=37 && q<=39))return Okay;
    if((q>=26 && q<=29) || (q>=40 && q<=42))return ConfirmNo;
    if((q>=30 && q<=33) || (q>=43 && q<=45))return ConfirmYes;
    if(q==47)return Keyboard; // Pressed key overlay.
    return NameCount; // Other native help elements retain their own layout.
}
// Native code parks typed glyphs at x=400 between keyboard/confirmation
// states. Shrinking that position can bring them back onscreen. Place each
// glyph in its visible name slot instead; never animate that parked position.
template<class M> ScreenModels::Transform NameGlyphTransform(M& m,const Layout& l,
        U32 controller,U32 model,unsigned index,float progress,const Rect* frozen_field=nullptr) {
    const auto hidden=ScreenModels::Transform{0,0,0,0,0,0,true};
    const U32 helper=m.Read(NameOwner+0x70).value_or(0);
    if(m.Read(helper)!=62)return hidden;
    const U32 positions=m.Read(helper+0x10).value_or(0),colors=m.Read(helper+0x18).value_or(0);
    for(unsigned q:{25u,7u}) {
        if(frozen_field && q!=7)continue;
        PausePrompts::QuadGeometry<1> field;
        if(!TitleScreen::ReadFloats(m,positions+q*48,field.positions))continue;
        auto b=frozen_field?*frozen_field:Bounds(field,0,0);
        const auto alpha=frozen_field?std::optional<U32>{std::bit_cast<U32>(1.f)}:m.Read(colors+q*64+12);
        if(!b.Valid() || !b.Visible() || !alpha || std::bit_cast<float>(*alpha)<=0)continue;
        if(q==7 && progress<1) {
            // Match the frame's canonical entrance position, never its native
            // stepped slide. All letter models follow this same name-field pose.
            const auto x=m.Read(0x507864+q*8),y=m.Read(0x507868+q*8);
            if(!x || !y)return hidden;
            const float dx=std::bit_cast<float>(*x)-b.left,dy=std::bit_cast<float>(*y)-b.top;
            b.left+=dx;b.right+=dx;b.top+=dy;b.bottom+=dy;
        }
        const unsigned kind=m.Read(controller+0x40).value_or(0);
        std::array<float,2> origin;
        if(kind==1) {
            // 0x2e946c writes kind-1 placement into the two glyph meshes,
            // leaving the model translation at zero. Read THIS draw's live
            // buffer; the other model may not have advanced its buffer yet.
            const U32 mesh=m.Read(model+4).value_or(0);
            const U32 current=m.Read(mesh+0x124).value_or(2);
            const U32 live=current<2?m.Read(mesh+0x1a0+current*4).value_or(0):0;
            if(!live || !TitleScreen::ReadFloats(m,live,origin))return hidden;
            // The first mesh is the one-pixel shadow. Preserve that offset.
            if(m.Read(controller+0x10)==model) {origin[0]-=1;origin[1]-=1;}
        } else {
            const U32 reference=m.Read(controller+(kind==2?0x30:0x10)).value_or(0);
            if(!TitleScreen::ReadFloats(m,reference+0x3c,origin))return hidden;
        }
        auto t=NameTransform(l,NameBox,progress);
        t.x+=(b.left+2+22*index-origin[0])*t.sx;
        t.y+=(b.top+2-origin[1])*t.sy;
        return t;
    }
    return hidden;
}
inline void PoseNameQuad(NameGeometry& g,unsigned q,const Layout& l,unsigned part,float progress) {
    const auto t=NameTransform(l,part,progress);
    for(unsigned v=0;v<4;++v) {
        auto& x=g.positions[q*12+v*3];auto& y=g.positions[q*12+v*3+1];
        x=t.x-40+t.ax+(x-t.ax)*t.sx;y=t.y+t.ay+(y-t.ay)*t.sy;
        if(part==NameBox)g.colors[q*16+v*4+3]*=NameEase(progress);
    }
}
// Crop using bilinear coordinates so this works with every native keyboard
// page's UV orientation and tint, including accented letters and symbols.
inline void NameCrop(NameGeometry& out,const NameGeometry& raw,unsigned q,Rect crop,Rect destination) {
    if(out.count>=256)return;
    const auto b=Bounds(raw,q,q);if(!b.Valid())return;
    const unsigned dst=out.count++;
    for(unsigned v=0;v<4;++v) {
        const bool right=v&1,bottom=v&2;
        out.positions[dst*12+v*3]=right?destination.right:destination.left;
        out.positions[dst*12+v*3+1]=bottom?destination.bottom:destination.top;
        const float fx=((right?crop.right:crop.left)-b.left)/(b.right-b.left);
        const float fy=((bottom?crop.bottom:crop.top)-b.top)/(b.bottom-b.top);
        for(unsigned c=0;c<6;++c) {
            float value=0;
            for(unsigned k=0;k<4;++k) {
                const float weight=(raw.positions[q*12+k*3]==b.left?1-fx:fx)*
                    (raw.positions[q*12+k*3+1]==b.top?1-fy:fy);
                value+=weight*(c<2?raw.uvs[q*8+k*2+c]:raw.colors[q*16+k*4+c-2]);
            }
            if(c<2)out.uvs[dst*8+v*2+c]=value;
            else out.colors[dst*16+v*4+c-2]=value;
        }
    }
}
inline bool NameBoxFade(NameGeometry& out,const NameGeometry& raw,unsigned q,const Layout& l,float progress) {
    if(!((q>=1 && q<=6) || (q>=16 && q<=24)) ||
       !std::isfinite(l.name_box_edge_fade) || l.name_box_edge_fade<=0)return false;
    const unsigned first=q<16?1:16,col=(q-first)%3,middle=q-col+1;
    const auto b=Bounds(raw,q,q);
    const float left=b.left-(col==0?0:col==1?24:272),right=left+296;
    const float fade=std::clamp(l.name_box_edge_fade,1.f,148.f);
    const unsigned slices=static_cast<unsigned>(std::ceil((b.right-b.left)/8));
    for(unsigned i=0;i<slices && out.count<256;++i) {
        auto piece=b;piece.left=b.left+(b.right-b.left)*i/slices;
        piece.right=b.left+(b.right-b.left)*(i+1)/slices;
        NameCrop(out,raw,q,piece,piece);
        const unsigned dst=out.count-1;
        for(unsigned v=0;v<4;++v) {
            if(col!=1) {
                // Extend the straight middle strip through the old end caps:
                // no vertical border or rounded corner survives the fade.
                const unsigned row=v&2;
                for(unsigned c=0;c<2;++c)
                    out.uvs[dst*8+v*2+c]=(raw.uvs[middle*8+row*2+c]+raw.uvs[middle*8+(row+1)*2+c])*.5f;
            }
            const float x=out.positions[dst*12+v*3];
            const float a=std::clamp(std::min(x-left,right-x)/fade,0.f,1.f);
            out.colors[dst*16+v*4+3]*=a*a*(3-2*a);
        }
        PoseNameQuad(out,dst,l,NameBox,progress);
    }
    return true;
}
inline void NameKeyboard(NameGeometry& out,const NameGeometry& raw,const Layout& l,
                         float progress,bool accents) {
    const auto b=Bounds(raw,0,0);if(!b.Valid())return;
    const auto keys=l.name_parts[Keyboard],back=l.name_parts[KeyboardBackground];
    if(keys.x==back.x && keys.y==back.y && keys.scale==back.scale) {
        NameCrop(out,raw,0,b,b);PoseNameQuad(out,out.count-1,l,Keyboard,progress);return;
    }
    // Use the clean 24px blue patch above/left of the layout toggle. The old
    // 4px sample at the outer border enlarged the bevel into a blurry grid.
    // Sample texel centers at native density, mirroring adjacent tiles so both
    // sides of every seam use exactly the same texels. Trim the final tiles'
    // UV spans along with their geometry instead of squeezing the full patch.
    for(unsigned row=0;row<7;++row)for(unsigned col=0;col<14;++col) {
        const float x=b.left+24*col,y=b.top+24*row;
        const float w=std::min(24.f,b.right-x),h=std::min(24.f,b.bottom-y);
        if(w<=0 || h<=0)continue;
        const float sx=b.left+(col&1?27.5f:4.5f),sy=b.top+(row&1?27.5f:4.5f);
        const Rect sample{sx,sy,sx+(col&1?-23.f:23.f)*w/24,
            sy+(row&1?-23.f:23.f)*h/24};
        NameCrop(out,raw,0,sample,{x,y,x+w,y+h});
        PoseNameQuad(out,out.count-1,l,KeyboardBackground,progress);
    }
    const auto key=[&](float x,float y,float w,float h) {
        const Rect r{b.left+x,b.top+y,b.left+x+w,b.top+y+h};
        NameCrop(out,raw,0,r,r);PoseNameQuad(out,out.count-1,l,Keyboard,progress);
    };
    key(32,4,128,24);key(162,4,130,24);
    if(accents) {
        for(unsigned row=0;row<5;++row)for(unsigned col=0;col<11;++col)
            key(6+26*col,30+26*row,26,26);
        key(292,56,26,52);key(292,108,26,52);
    } else {
        for(unsigned col=0;col<12;++col)key(6+26*col,30,26,26);
        for(unsigned col=0;col<10;++col)key(18+26*col,56,26,26);
        key(6,82,26,26);
        for(unsigned col=0;col<9;++col)key(32+26*col,82,26,26);
        key(6,108,38,26);
        for(unsigned col=0;col<10;++col)key(44+26*col,108,26,26);
        key(58,134,234,26);key(292,56,26,52);
    }
}
template<class M> NameGeometry NameAtlas(M& m,unsigned atlas,const Layout& l,float progress,bool* valid=nullptr) {
    if(valid)*valid=false;
    if(atlas>1)return {};
    NameGeometry raw,out;
    const U32 helper=m.Read(NameOwner+0x6c+atlas*4).value_or(0);
    const unsigned count=atlas?62:1;
    if(m.Read(helper)!=count)return {};
    const auto read=[&](U32 field,auto& values,unsigned n) {
        const U32 at=m.Read(helper+field).value_or(0);
        if(!TitleScreen::Pointer(at,n*4))return false;
        for(unsigned i=0;i<n;++i) {
            const auto w=m.Read(at+4*i);if(!w)return false;
            values[i]=std::bit_cast<float>(*w);if(!std::isfinite(values[i]))return false;
        }
        return true;
    };
    if(!read(0x10,raw.positions,count*12) || !read(0x14,raw.uvs,count*8) ||
       !read(0x18,raw.colors,count*16))return {};
    if(atlas)for(unsigned q:{1u,3u,4u,6u,16u,18u,19u,21u,22u,24u,48u,50u,51u,53u,54u,56u})
        PromptCapUVs(raw,q,360);
    if(valid)*valid=true;
    if(progress<=0)return {}; // Includes the name box and native glyph backing.
    raw.count=count;
    const unsigned mode=m.Read(NameOwner+8).value_or(0);
    const bool western=m.Read(NameOwner+0x40).value_or(0)!=0;
    const unsigned keyboard=mode<(western?3u:2u)?0:1;
    const unsigned state=m.Read(NameOwner+4).value_or(0);
    const bool entering=progress<1 && (state==1 || state==2 || state==4);
    const bool custom=!CustomKeys(m,l).empty();
    for(unsigned q=0;q<count;++q) {
        // The two name-field backings contain all eight blue slot markers.
        // Leave their native data intact for glyph placement, but draw only
        // the separate active insertion cursor (46), which follows typing/B.
        if(atlas && (q==7 || q==25))continue;
        if(custom && (q==0 || (atlas==1 && q==47)))continue;
        const unsigned part=atlas?NameQuadPart(q):Keyboard;
        if(custom && (part==Cancel || part==Okay))continue;
        auto b=Bounds(raw,q,q);if(!b.Valid())continue;
        if(entering && ((q==0 && atlas==keyboard) || (atlas && q>=1 && q<=15))) {
            const auto x=m.Read(q==0?0x507864:0x507864+q*8);
            const auto y=m.Read(q==0?0x507868:0x507868+q*8);
            if(!x || !y)return {};
            const float dx=std::bit_cast<float>(*x)-b.left,dy=std::bit_cast<float>(*y)-b.top;
            for(unsigned v=0;v<4;++v) {
                raw.positions[q*12+v*3]+=dx;raw.positions[q*12+v*3+1]+=dy;
                if(part==NameBox)raw.colors[q*16+v*4+3]=1;
            }
            b=Bounds(raw,q,q);
        }
        // Hidden alternatives must remain hidden even after large user offsets.
        if(!b.Visible() || (q==0 && entering && atlas!=keyboard))continue;
        if(atlas && NameBoxFade(out,raw,q,l,progress))continue;
        if(q==0 && western)NameKeyboard(out,raw,l,progress,atlas==1);
        else {
            // Keep only the bottom tenth of the active insertion cursor.
            // Cropping retains the native tint/pulsing opacity;
            // glyph placement continues to use the untouched native field.
            if(atlas && q==46)b.top+=.9f*(b.bottom-b.top);
            NameCrop(out,raw,q,b,b);
            PoseNameQuad(out,out.count-1,l,part,progress);
        }
    }
    return out;
}
inline std::optional<std::array<float,2>> MapPointer(float u,float v,float width) {
    if(MasterQuest::choosing.load()) {
        if(MasterQuest::ready.load()) {
            std::scoped_lock lock{mutex};
            for(unsigned i=0;i<2;++i)if(quest_touch_bounds[i].Contains(u*width-(width-400)/2,v*240)) {
                MasterQuest::pointer_action=i+1;break;
            }
        }
        return {};
    }
    const float x=u*width-(width-320)/2,y=v*240;
    if(name_on_top.load()) {
        std::scoped_lock lock{mutex};
        if(name_touch_ready) {
            if(!name_touch_keys.empty()) {
                const auto t=NameTransform(touch_layout,Keyboard,name_touch_progress);
                const float nx=t.ax+(x-(t.x-40)-t.ax)/t.sx;
                const float ny=t.ay+(y-t.y-t.ay)/t.sy;
                for(const auto& key:name_touch_keys) {
                    const auto c=NameKeyboard::Center(key,touch_layout.keyboard);
                    const float w=key.display_width*(1+NameKeyboard::Spacing(touch_layout.keyboard.spacing_x)/26);
                    const float h=26*(1+NameKeyboard::Spacing(touch_layout.keyboard.spacing_y)/26);
                    if(std::abs(nx-c[0])<=w/2 && std::abs(ny-c[1])<=h/2) {
                        if(key.row==-2)return {}; // Current page tab is not an input.
                        return std::array{key.X()/320,key.Y()/240};
                    }
                }
            }
            for(unsigned part:{Cancel,Okay,NameBox,Keyboard,ConfirmNo,ConfirmYes}) {
                if(!name_touch_keys.empty() && (part==Keyboard || part==Cancel || part==Okay))continue;
                if(name_touch_confirmation!=(part==ConfirmNo || part==ConfirmYes))continue;
                const auto t=NameTransform(touch_layout,part,name_touch_progress);
                const float nx=t.ax+(x-(t.x-40)-t.ax)/t.sx;
                const float ny=t.ay+(y-t.y-t.ay)/t.sy;
                if(NameBounds[part].Contains(nx,ny) && nx>=0 && nx<320 && ny>=0 && ny<240)
                    return std::array{nx/320,ny/240};
            }
            return {};
        }
        if(x<0 || x>=320 || y<0 || y>=240)return {};
        return std::array{x/320,y/240};
    }
    std::scoped_lock lock{mutex};
    // Reverse the same pose used to draw each visible part. Touches at the old
    // location must not activate a card that has been moved elsewhere.
    for(unsigned i=0;i<Count;++i) {
        const auto b=touch_bounds[i];if(!b.Visible())continue;
        const auto t=PartTransform(touch_layout,i,touch_bounds);
        if(t.sx<=0 || t.sy<=0)continue;
        const float nx=t.ax+(x-(t.x-40)-t.ax)/t.sx;
        const float ny=t.ay+(y-t.y-t.ay)/t.sy;
        if(b.Contains(nx,ny) && nx>=0 && nx<320 && ny>=0 && ny<240)
            return std::array{nx/320,ny/240};
    }
    for(const auto& b:touch_bounds)if(b.Contains(x,y))return {};
    if(x<0 || x>=320 || y<0 || y>=240 || (x<58 && y>=202))return {};
    return std::array{x/320,y/240};
}
template<class M> unsigned FocusedStone(M& m,bool naming) {
    if(naming) {
        if(m.Read(NameOwner+4)!=4)return FocusCount;
        if(m.Read(NameOwner+8)==4)
            return NameFocus+(m.Read(NameOwner+0x2c)==0?ConfirmNo:ConfirmYes);
        if(!GetLayout().keyboard.enabled && m.Read(NameOwner+0x14)==5)
            return NameFocus+(m.Read(NameOwner+0x28)==0?Cancel:Okay);
        return FocusCount;
    }
    if(MasterQuest::choosing.load() && !MasterQuest::cancelling.load())
        return QuestFocus+(MasterQuest::highlighted.load()&1);
    const auto state=m.Read(Owner+0x10).value_or(0);
    if(state==3 || state==14) {
        const auto i=state==14?CopyDestinationSlot(m):m.Read(Owner+0x14).value_or(3);
        return i<3?i:FocusCount;
    }
    if(state==8 || state==10) {
        const auto i=m.Read(Owner+0x18).value_or(3);
        return i<3?Start+i:FocusCount;
    }
    if(state==37)return m.Read(Owner+0x24)==0?DeleteCancel:DeleteErase;
    if(state==17)return m.Read(Owner+0x20)==0?OverwriteCancel:OverwriteAccept;
    return FocusCount;
}
struct StoneFocusAnimation {
    std::array<float,FocusCount> amount{};
    void Apply(Layout& frame,unsigned selected) {
        frame.stone_focus_amount=StoneFocus::Advance(amount,selected,
            {frame.stone_focus_grow,frame.stone_focus_seconds},1.f/60);
    }
};
// Saving remains entirely native. Track only its presentation and the automatic
// return, and wait for mode 6, which native code sets AFTER joining the save job.
struct NameSaveFlow {
    bool active=false,returning=false,select_pending=false,entering=false;
    bool room_entrance=false;
    unsigned entrance_frames=0;
    static constexpr unsigned EntranceFrames=12;
    unsigned slot=3;
    float opacity=0;
    std::int64_t clock_us=0;
    bool Visible() const {return active || returning || opacity>0;}
    void Frame() {
        if(entering && entrance_frames<EntranceFrames)++entrance_frames;
        opacity=room_entrance?0.f:std::clamp(opacity+((active || returning)?1.f:-1.f)/9,0.f,1.f);
        if(Visible())clock_us+=16667;else clock_us=0;
    }
    template<class M,class R> void NameDispatch(M& m,R& r,bool supported) {
        r[0]=m.Read(r[6]+4).value_or(0); // Displaced LDR.
        if(!supported)return;
        const auto mode=m.Read(NameOwner+8).value_or(0);
        if(r[0]==1)*this={};
        if(r[0]==5 && (mode==2 || mode==3) && !active) {
            const auto length=m.Read(NameOwner+0x24).value_or(0);
            // 0x4302a4 writes the slot through the 0x588958 context, NOT
            // the smaller 0x587934 system-settings structure.
            const auto selected=m.Read(0x588958+0x4dc).value_or(3);
            if(length>0 && length<=8 && selected<3) {
                *this={};active=true;slot=selected;
            }
        }
        if(active && r[0]==4 && mode==6) {
            active=false;returning=true;select_pending=true;
            // Original completion's cleanup/return path, without requiring A/B.
            // The caller's native stack and callee-saved registers are intact.
            r[15]=0x42863c;
        }
    }
    template<class M,class R> bool FileDispatch(M& m,R& r) {
        if(const auto joined=RoomJoin::entrance_slot.exchange(3);joined<3) {
            *this={};slot=joined;returning=true;room_entrance=true;
        }
        const auto state=m.Read(Owner+0x10).value_or(0);
        if(entering) {
            if(state==8 && entrance_frames<EntranceFrames) {
                r[15]=0x4336f8;return true; // Keep input off until the cards arrive.
            }
            entering=false;room_entrance=false;
        }
        if(!returning)return false;
        if(state==2 && select_pending) {
            // Skipping the hidden entrance timer alone leaves the stones at
            // staggered x positions. Settle the actual native motion too.
            for(unsigned i=0;i<3;++i)Settle(m,i,16,48.f+52*i);
            m.Write(Owner+0x28,10);
        }
        if(state==3 && select_pending) {
            select_pending=false;
            const U32 occupied=m.Read(0x42fdfc).value_or(0);
            if(slot>=3 || !occupied || !m.Read(occupied+slot*4).value_or(0)) {
                returning=false;return false; // Native list remains usable on a failed save.
            }
            m.Write(Owner+0x14,slot);
            r[4]=1;r[15]=0x43006c; // Native occupied-file selection, after its input sound.
            return true;
        }
        if(state==7 || state==8) {
            for(unsigned i=0;i<3;++i)Settle(m,i,i==slot?16.f:-384.f,i==slot?56.f:48.f+52*i);
        }
        if(state==8) {
            returning=false;entering=true;entrance_frames=0;
            action_opening=false;action_frames=0;
            r[15]=0x4336f8;return true;
        }
        if(state>8)returning=false;
        return false;
    }
    template<class M> static void Settle(M& m,unsigned i,float x,float y) {
        for(U32 base:{0x55b5e0u,0x55b5f8u})m.Write(base+4*i,std::bit_cast<U32>(x));
        for(U32 base:{0x55b5ecu,0x55b604u})m.Write(base+4*i,std::bit_cast<U32>(y));
        m.Write(0x55b610+4*i,0);m.Write(0x55b61c+4*i,0);
    }
};
struct SavingKeyboard {
    FileStatusText::Geometry text;
    StoneShadowGeometry shadow;
    std::array<NameGeometry,2> atlases;
    Rect field;
    Layout layout;
    template<class M> void Capture(M& m,const Layout& l) {
        layout=l;text=KeyboardText(m,l,1);shadow=NameShadow(m,l,1);
        for(unsigned a=0;a<2;++a)atlases[a]=NameAtlas(m,a,l,1);
        PausePrompts::QuadGeometry<1> g;
        const U32 helper=m.Read(NameOwner+0x70).value_or(0);
        if(TitleScreen::ReadFloats(m,m.Read(helper+0x10).value_or(0)+7*48,g.positions))field=Bounds(g,0,0);
    }
};
// Independent geometry borrows the live atlas binding. No game art is bundled,
// and no native mesh or animation state is overwritten.
struct Hook {
    NameTyping::Hook typing;
    template<class M,class R> void KeyboardTyping(M& m,R& r,unsigned site) {
        if(typing.buttons && m.Read(NameOwner+4)==4) {
            if(site<19)r[0]=*typing.buttons;
            r[15]=Sites[site].pc+4;return;
        }
        r[14]=Sites[site].pc+4;r[15]=site<19?0x33b5ec:0x2e83ec;
    }
    NameSaveFlow name_save;
    FileOperationPromptTransition operation_prompt;
    std::unique_ptr<SavingKeyboard> saving_keyboard;
    bool KeyboardFading() const {
        return name_save.active || RoomJoin::phase.load()==RoomJoin::Phase::NameLeaving ||
            RoomJoin::phase.load()==RoomJoin::Phase::ReturnToFiles;
    }
    float KeyboardOpacity() const {return 1-(RoomJoin::Active()?RoomJoin::opacity.load():name_save.opacity);}
    template<class G> void FadeSavingKeyboard(G& geometry) const {
        if(!KeyboardFading() || !saving_keyboard)return;
        const float opacity=KeyboardOpacity();
        for(unsigned i=3;i<geometry.count*16;i+=4)geometry.colors[i]*=opacity;
    }
    template<class M,class R> void NameDispatch(M& m,R& r,bool supported,std::int64_t now_us=0) {
        typing.Frame(m,supported && !KeyboardFading());
        const bool was_fading=KeyboardFading();
        if(RoomJoin::NameDispatch(m,r,now_us)) {
            name_save={};
            if(KeyboardFading() && !was_fading) {
                saving_keyboard=std::make_unique<SavingKeyboard>();
                saving_keyboard->Capture(m,PresentationLayout(GetLayout()));
            }
            if(r[0]==1)saving_keyboard.reset();
            return;
        }
        const bool was_saving=name_save.active;
        name_save.NameDispatch(m,r,supported);
        if(name_save.active && !was_saving) {
            saving_keyboard=std::make_unique<SavingKeyboard>();
            saving_keyboard->Capture(m,PresentationLayout(GetLayout()));
        }
        if(r[0]==1)saving_keyboard.reset();
    }
    StoneFocusAnimation stone_focus;
    NameKeyboard::GrowthAnimation character_growth;
    GuestUI::Hook<FileStatusText::Geometry,FileStatusText::Width,FileStatusText::Height> keyboard_text;
    // Observe the current frame, including empty geometry. The mesh retains
    // its previous geometry when Begin cannot submit, so inspecting it alone
    // would incorrectly report that a missing keyboard was still being drawn.
    unsigned keyboard_quads=0;
    float keyboard_alpha=0;
    Rect keyboard_bounds{};
    bool keyboard_submitted=false;
    std::array<GuestUI::Hook<NameGeometry,1,1,0,true>,2> name_atlases;
    unsigned name_frames=0;
    float name_progress=1;
    NamePromptTransition prompt_transition;
    bool was_naming=false,name_editable=false;
    bool keyboard_delete_pending=false;
    template<class M,class R> void KeyboardDelete(M& m,R& r,bool supported=true) {
        if(keyboard_delete_pending) {
            keyboard_delete_pending=false;
            // Native deletion has returned with the original local frame intact.
            // Match the normal page's delete sound and use the accent epilogue.
            m.Write(r[13],0x54ac20);m.Write(r[13]+4,0x54ac24);
            r[0]=0x010004bf;r[1]=0;r[2]=4;r[3]=0x54ac20;
            r[14]=0x43e614;r[15]=0x37547c;return;
        }
        r[0]=m.Read(r[10]+0x24).value_or(0); // Displaced LDR.
        if(!supported || !r[0] || m.Read(NameOwner+4)!=4 ||
           CustomKeys(m,GetLayout()).empty())return;
        keyboard_delete_pending=true;
        r[14]=Sites[13].pc;r[15]=0x2e7cf4; // Original deletion, exactly once.
    }
    bool keyboard_navigation_pending=false;
    template<class M,class R> void KeyboardNavigation(M& m,R& r,unsigned site,bool supported=true) {
        const auto keys=supported?CustomKeys(m,GetLayout()):std::vector<NameKeyboard::Key>{};
        if(site==8) {
            r[0]=m.Read(r[9]+0x14).value_or(0); // Displaced LDR.
            keyboard_navigation_pending=!keys.empty() && m.Read(NameOwner+4)==4;
            if(keyboard_navigation_pending){r[14]=Sites[9].pc;r[15]=0x33b5d0;}
            return;
        }
        const bool custom=site==9?keyboard_navigation_pending:!keys.empty() && m.Read(NameOwner+4)==4;
        keyboard_navigation_pending=false;
        if(site==9)r[11]=11;else r[9]=10; // Displaced MOV (flags unchanged).
        if(!custom)return;
        const U32 done=site==9?0x43c938:0x43e434; // Native stack/register epilogue.
        r[15]=done;
        if(NameKeyboard::Navigate(m,keys,r[0])) {
            // Same move sound and native calling convention, with the original
            // twelve-byte local stack frame still in place.
            m.Write(r[13],0x54ac20);m.Write(r[13]+4,0x54ac24);
            r[0]=0x010004be;r[1]=0;r[2]=4;r[3]=0x54ac20;
            r[14]=done;r[15]=0x37547c;
        }
    }
    GuestUI::Hook<BackgroundGeometry,FileBackground::Width,FileBackground::Height,FileBackground::Format> backgrounds;
    GuestUI::Hook<Geometry,1,1,0,true> top,bottom;
    GuestUI::Hook<StoneShadowGeometry,8,8> shadows;
    GuestUI::Hook<VignetteGeometry,8,8> vignette;
    GuestUI::Hook<FileParticles::Geometry,FileParticles::Width,FileParticles::Height> particles;
    FileParticles::Animation particle_animation;
    GuestUI::Hook<ChoiceGeometry,QuestChoice::Width,QuestChoice::Height,4> quest;
    GuestUI::Hook<BadgeGeometry,DPad::TextureWidth,DPad::TextureHeight> badges;
    GuestUI::Hook<PausePrompts::Geometry,256,128,4> icons;
    GuestUI::Hook<PausePrompts::TextGeometry,PausePrompts::TextWidth,PausePrompts::TextHeight> text;
    GuestUI::Hook<FileStatusText::Geometry,FileStatusText::Width,FileStatusText::Height> status_text;
    GuestUI::Hook<SeedShadowGeometry,8,8> seed_shadow;
    GuestUI::Hook<LoadingIcon::Geometry,64,64,4> loading_icon;
    GuestUI::Hook<RoomPlayerIcon::Geometry,RoomPlayerIcon::Width,RoomPlayerIcon::Height> player_icon;
    RoomPlayerIcon::Geometry player_icon_geometry;
    float player_count_alpha=0;
    unsigned displayed_players=0;
    enum class Stage {Idle,Loading,PromptDestroy,PromptFree,PromptInit,Background,Vignette,Particles,Shadows,Atlas,Badges,Native,Name,Quest,Icons,Text,SeedShadow,Status,Keyboard,PlayerCount};
    Stage stage=Stage::Idle;
    std::array<U32,16> saved{};
    std::shared_ptr<const PausePrompts::Assets> assets;
    PausePrompts::TextGeometry lettering;
    std::shared_ptr<const FileStatusText::Assets> status_assets;
    FileStatusText::Geometry status_geometry;
    std::optional<ScreenModels::Transform> fitted_prompt_text;
    SeedShadowGeometry seed_shadow_geometry;
    Geometry atlas_geometry;
    ChoiceGeometry quest_geometry;
    unsigned quest_selection=0;
    BadgeGeometry badge_geometry;
    RandomizerBadgeState randomizer_badges;
    JoinPromptFade join_prompt_fade;
    float profile_prompt_alpha=0;
    MultiplayerLoading::Hook multiplayer_loading;
    U32 entry=0,source=0,prompt_slot=0,prompt_at=0;
    float alpha=1;
    bool quest_frame=false,name_frame=false,return_frame=false;
    Layout frame_layout;
    std::array<Rect,Count> bounds{};
    bool Pending() const {return stage!=Stage::Idle;}
    bool NativeDrawing() const {return stage==Stage::Native;}
    bool Drawing() const {return particles.Drawing() || vignette.Drawing() || shadows.Drawing() || backgrounds.Drawing() || quest.Drawing() || badges.Drawing() || top.Drawing() || NativeDrawing() || stage==Stage::Name;}
    bool PromptDrawing() const {return multiplayer_loading.Drawing() || player_icon.Drawing() || keyboard_text.Drawing() || icons.Drawing() || text.Drawing() || seed_shadow.Drawing() || status_text.Drawing() || loading_icon.Drawing();}
    template<class M> ScreenModels::Transform ModelTransform(M& m,U32 model) const {
        if(particles.Drawing() || vignette.Drawing() || backgrounds.Drawing() || quest.Drawing())return {}; // Already in top-screen coordinates.
        if(PromptDrawing())return {};
        if(stage==Stage::Name && name_progress<=0)return {0,0,0,0,0,0,true};
        if(stage==Stage::Name && name_editable) {
            if(name_atlases[0].Drawing() || name_atlases[1].Drawing())return {40,0};
            for(unsigned i=0;i<12;++i) {
                const U32 controller=m.Read(0x55be60+i*4).value_or(0);
                if(!TitleScreen::Pointer(controller,0x4c))continue;
                const auto kind=m.Read(controller+0x40).value_or(0);
                const unsigned count=kind==2?9:kind==3?1:2;
                for(unsigned j=0;j<count;++j)if(m.Read(controller+0x10+j*4)==model) {
                    if(KeyboardFading() && saving_keyboard) {
                        if(i>=8)return {0,0,0,0,0,0,true}; // Saving/status text is replaced by the spinner.
                        return NameGlyphTransform(m,saving_keyboard->layout,controller,model,i,1,&saving_keyboard->field);
                    }
                    return i<8?NameGlyphTransform(m,frame_layout,controller,model,i,name_progress):
                        NameTransform(frame_layout,NameBox,name_progress);
                }
            }
            const U32 cursor=m.Read(NameOwner).value_or(0);
            if(cursor && m.Read(cursor+4)==model) {
                const auto mode=m.Read(NameOwner+8).value_or(6);
                if(m.Read(NameOwner+4)!=4 || mode>4)return {0,0,0,0,0,0,true};
                const auto keys=CustomKeys(m,frame_layout);
                const unsigned part=mode==4?(m.Read(NameOwner+0x2c)==0?ConfirmNo:ConfirmYes):
                    !keys.empty()?Keyboard:m.Read(NameOwner+0x14)==5?
                    (m.Read(NameOwner+0x28)==0?Cancel:Okay):Keyboard;
                auto t=NameTransform(frame_layout,part,name_progress);
                if(part==Keyboard)for(const auto& key:keys)if(key.selected) {
                    const auto c=NameKeyboard::Center(key,frame_layout.keyboard);
                    const auto& k=frame_layout.keyboard;
                    const float dx=std::isfinite(k.cursor_x)?std::clamp(k.cursor_x,-32.f,32.f):0;
                    const float dy=std::isfinite(k.cursor_y)?std::clamp(k.cursor_y,-32.f,32.f):0;
                    t.x+=(c[0]-key.X()+dx)*t.sx;
                    t.y+=(c[1]-key.Y()+dy)*t.sy;break;
                }
                return t;
            }
        }
        if(!NativeDrawing())return {40,0};
        if(name_save.returning)return {0,0,0,0,0,0,true};
        if(RoomJoin::HideFiles() && !RoomJoin::WaitingScreen())return {0,0,0,0,0,0,true};
        // Native name/date controllers belong to their stone, including all
        // nine outline/shadow models. Other messages use the upper/lower box.
        for(unsigned i=0;i<27;++i) {
            const U32 controller=m.Read(0x55b548+i*4).value_or(0);
            if(!TitleScreen::Pointer(controller,0x4c))continue;
            const unsigned kind=m.Read(controller+0x40).value_or(0);
            const unsigned count=kind==2?9:kind==3?1:2;
            for(unsigned j=0;j<count;++j)if(m.Read(controller+0x10+j*4)==std::optional<U32>{model}) {
                // Room/status errors borrow controller 6 outside its native
                // file-list lifetime. Copy, Erase and Start do not destroy it.
                // Render it only while this exact message is still requested,
                // including states with no replacement prompt (e.g. Start).
                // Leave native ownership/cleanup and ordinary prompt fades alone.
                if(i==6 && controller==MasterQuest::prompt_controller.load()) {
                    const auto message=MasterQuest::prompt_message.load();
                    if((LoadingPrompt::RoomMessage(message) || LoadingPrompt::FileErrorMessage(message) || LoadingPrompt::RetryMessage(message) || message==LoadingPrompt::StartingMessageId) &&
                       (prompt_slot!=6 || message!=MasterQuest::PromptMessage()))return {0,0,0,0,0,0,true};
                }
                if(RoomJoin::WaitingScreen() && (i!=6 || !LoadingPrompt::Ready()))return {0,0,0,0,0,0,true};
                if(MasterQuest::starting.load() && i>=6 && (i!=6 || !LoadingPrompt::Ready()))return {0,0,0,0,0,0,true};
                if(RoomJoin::HostPrompt() && i==7)return {0,0,0,0,0,0,true};
                if((MasterQuest::start_error!=MasterQuest::FileError::None || MasterQuest::notice_error!=MasterQuest::FileError::None) && i>6)return {0,0,0,0,0,0,true};
                if(RoomJoin::GuestPrompt() && i>=6 && (i!=6 || !LoadingPrompt::Ready()))return {0,0,0,0,0,0,true};
                if(quest_frame && MasterQuest::loading_label.load() && !LoadingPrompt::Ready() && i==6)return {0,0,0,0,0,0,true};
                if(quest_frame && !return_frame && i!=6)return {0,0,0,0,0,0,true};
                const auto state=m.Read(Owner+0x10).value_or(0);
                if((prompt_slot==6 && i==7 && (state==2 || state==9)) ||
                   (prompt_slot==7 && (i==6 || i==9 || i==19)) ||
                   (prompt_slot==9 && (i==7 || (i>=10 && i<=12))) ||
                   (prompt_slot>=10 && prompt_slot<=12 && i>=6 && i<=12 && i!=prompt_slot) ||
                   (prompt_slot==15 && i>=6 && i<=18 && i!=15) ||
                   (prompt_slot==16 && i>=6 && i<=23 && i!=16) ||
                   (prompt_slot==22 && (i==7 || i==19 || i==20 || i==21)) ||
                   (prompt_slot==19 && i==7))return {0,0,0,0,0,0,true};
                // Slot 16 is the upper "Copy completed" message (native
                // y=16), not the lower Game Card warning in slot 24 (y=195).
                const unsigned part=i<6?i%3:(i==18 || i==21 || i==24)?LowerPrompt:Prompt;
                if(i==prompt_slot && fitted_prompt_text)return *fitted_prompt_text;
                return PartTransform(frame_layout,part,bounds);
            }
        }
        const U32 cursor=m.Read(Owner+0x34).value_or(0);
        if(cursor && m.Read(cursor+4)==std::optional<U32>{model}) {
            if(RoomJoin::GuestActive())return {0,0,0,0,0,0,true};
            if(RoomJoin::HideFiles())return {0,0,0,0,0,0,true};
            if(quest_frame && !return_frame)return {0,0,0,0,0,0,true};
            const auto state=m.Read(Owner+0x10).value_or(0);
            if(ExchangeState(exchange.load(),state) &&
               !(exchange.load()==Exchange::Overwrite && state==17 && exchange_frames.load()>=10))return {0,0,0,0,0,0,true};
            unsigned part=Count;
            if(state==3 || state==14) {
                const unsigned index=state==14?CopyDestinationSlot(m):m.Read(Owner+0x14).value_or(3);
                if(state==14 && index>=3)return {0,0,0,0,0,0,true};
                if(index<3)part=index;
            } else if(state==8 || state==10) {
                const unsigned index=m.Read(Owner+0x18).value_or(3);
                if(index<3)part=Start+index;
            } else if(state==37) {
                part=m.Read(Owner+0x24)==0?DeleteCancel:DeleteErase;
            } else if(state==17) {
                part=m.Read(Owner+0x20)==0?OverwriteCancel:OverwriteAccept;
            } else if(state!=22 && state!=41)
                return {0,0,0,0,0,0,true}; // Native cursor is parked at x=400.
            return PartTransform(frame_layout,part,bounds);
        }
        return {40,0};
    }
    template<class M,class R> void BeginLoading(M& m,R& r) {
        if(const auto image=LoadingIcon::GetPixels()) {
            const auto pose=HUD::GetEffectiveLayout()[HUD::LoadingIcon];
            const bool saving=name_save.Visible();
            const bool busy=operation_prompt.opacity>0;
            const auto clock=busy?operation_prompt.clock_us:saving?name_save.clock_us:MasterQuest::cursor_us.load();
            const float opacity=RoomJoin::WaitingScreen()?(RoomJoin::Searching()?RoomJoin::opacity.load():0.f):
                busy?operation_prompt.opacity:saving?name_save.opacity:MasterQuest::loading_alpha.load();
            const auto g=LoadingIcon::Draw(clock,.8f*opacity,pose);
            loading_icon.Begin(m,r,entry,g,*image,source);
            if(loading_icon.Pending()) {stage=Stage::Loading;return;}
        }
        stage=Stage::Idle;
    }
    template<class M,class R> void Resume(M& m,R& r) {
        if(stage==Stage::Keyboard) {
            if(keyboard_text.Drawing())keyboard_submitted=true;
            keyboard_text.Resume(m,r);if(keyboard_text.Pending())return;
            BeginNativeName(r);return;
        }
        if(stage==Stage::Loading) {
            loading_icon.Resume(m,r);if(!loading_icon.Pending())stage=Stage::Idle;return;
        }
        if(stage==Stage::Name) {r=saved;BeginNamePrompts(m,r);return;}
        if(stage==Stage::PromptDestroy) {
            stage=Stage::PromptFree;r[14]=entry;r[15]=0x3525d4;return;
        }
        if(stage==Stage::PromptFree) {
            m.Write(prompt_at,0);stage=Stage::PromptInit;
            r[0]=prompt_slot;r[14]=entry;r[15]=0x2ee864;return;
        }
        if(stage==Stage::PromptInit) {r=saved;BeginBackdrop(m,r);return;}
        if(stage==Stage::Background) {
            backgrounds.Resume(m,r);if(backgrounds.Pending())return;
            BeginVignette(m,r);
            return;
        }
        if(stage==Stage::Vignette) {
            vignette.Resume(m,r);if(vignette.Pending())return;
            BeginParticles(m,r);
            return;
        }
        if(stage==Stage::Particles) {
            particles.Resume(m,r);if(particles.Pending())return;
            BeginCards(m,r);
            return;
        }
        if(stage==Stage::Shadows) {
            shadows.Resume(m,r);if(shadows.Pending())return;
            if(name_frame)BeginName(m,r);else BeginAtlas(m,r);return;
        }
        if(stage==Stage::Quest) {
            quest.Resume(m,r);if(quest.Pending())return;
            auto g=atlas_geometry;
            if(!return_frame)for(unsigned q=6;q<QuadCount;++q)Hide(g,q);
            top.Begin(m,r,entry,g,{},source);
            stage=top.Pending()?Stage::Atlas:Stage::Idle;return;
        }
        if(stage==Stage::Atlas) {
            top.Resume(m,r);if(top.Pending())return;
            if(!top.Owned(m)) {stage=Stage::Idle;return;}
            if(const auto image=DPad::GetTexture(1)) {
                badges.Begin(m,r,entry,badge_geometry,*image,source);
                if(badges.Pending()) {stage=Stage::Badges;return;}
            }
            on_top.store(true);saved=r;stage=Stage::Native;
            r[14]=entry;r[15]=0x41f2e4;return;
        }
        if(stage==Stage::Badges) {
            badges.Resume(m,r);if(badges.Pending())return;
            on_top.store(true);saved=r;stage=Stage::Native;
            r[14]=entry;r[15]=0x41f2e4;return;
        }
        if(stage==Stage::Native) {
            if(name_save.returning) {r=saved;BeginLoading(m,r);return;}
            // Finish the outgoing keyboard labels even if native name entry
            // has already torn down its models. Use the current file donor.
            if(prompt_transition.keyboard>0 && !name_save.Visible() && !RoomJoin::HideFiles()) {
                r=saved;BeginNamePrompts(m,r);return;
            }
            r=saved;assets=PausePrompts::GetAssets();
            status_assets=FileStatusText::GetAssets();status_geometry={};seed_shadow_geometry={};
            const bool toggle_prompt=quest_frame && !MasterQuest::loading_label.load();
            const float prompt_alpha=MasterQuest::RandomizerAlpha();
            const auto p=RandomizerPrompt(frame_layout);
            const auto randomizer_edge=status_assets?StatusPromptEdge(*status_assets,p,
                UILanguage::Randomizer(Randomizer::FileStatus::enabled.load()),RandomizerPromptColumn()):
                PausePrompts::EdgePositions{p.x,p.x+20*p.scale};
            const bool profile=RoomJoin::profile_visible.load();
            const auto jp=profile?ProfilePrompt(frame_layout,false):JoinPrompt(frame_layout);
            const auto hp=ProfilePrompt(frame_layout,true);
            const float host_alpha=profile?profile_prompt_alpha:0;
            const float join_alpha=profile?host_alpha*((RoomJoin::hosting.load() || RoomJoin::host_opening.load()) && !RoomJoin::stopping.load()?.5f:1.f):toggle_prompt?join_prompt_fade.Alpha():0;
            if(status_assets) {
                if((MasterQuest::starting.load() || RoomJoin::StatusPrompt() || (quest_frame && MasterQuest::loading_label.load())) && !LoadingPrompt::Ready()) {
                    const auto generation_label=LoadingPrompt::GenerationLabel(UILanguage::MessageLanguage());
                    const std::string_view label=MasterQuest::starting.load()?LoadingPrompt::StartingLabel(UILanguage::MessageLanguage()):
                        RoomJoin::StatusPrompt()?RoomJoin::Label(RoomJoin::CurrentStatus(),UILanguage::MessageLanguage()):std::string_view(generation_label);
                    const auto b=bounds[Prompt];
                    const auto pose=PartTransform(frame_layout,Prompt,bounds);
                    const float scale=pose.sx;
                    const float width=FileStatusText::Measure(*status_assets,label,scale);
                    status_geometry=FileStatusText::Text(*status_assets,label,
                        pose.x+b.X()-width/2,pose.y+b.Y()+10*scale,std::max(width,1.f),scale);
                    for(unsigned i=3;i<status_geometry.count*16;i+=4)
                        status_geometry.colors[i]=LoadingPrompt::PresentationOpacity(MasterQuest::cursor_us.load(),MasterQuest::starting.load(),quest_frame && MasterQuest::loading_label.load())*
                            (RoomJoin::WaitingScreen()?RoomJoin::opacity.load():1.f);
                    if(RoomJoin::GuestPrompt() && RoomJoin::CurrentStatus()==RoomJoin::Status::Disconnecting)
                        for(unsigned i=0;i<status_geometry.count*16;i+=4) {
                            status_geometry.colors[i]=1.f;
                            status_geometry.colors[i+1]=status_geometry.colors[i+2]=.18f;
                        }
                } else if(toggle_prompt) {
                    const bool enabled=Randomizer::FileStatus::enabled.load();
                    status_geometry=FileStatusText::Text(*status_assets,
                    UILanguage::Randomizer(enabled),
                    randomizer_edge.text,p.y+15*p.scale,1000*p.scale,.75f*p.scale);
                    for(unsigned i=0;i<status_geometry.count*16;i+=4) {
                        if(enabled) {
                            status_geometry.colors[i]=1.f;
                            status_geometry.colors[i+1]=status_geometry.colors[i+2]=.18f;
                        }
                        status_geometry.colors[i+3]=prompt_alpha;
                    }
                }
                else if(const auto seed=SelectedSeed(m);!RoomJoin::HideFiles() && !seed.empty()) {
                    status_geometry=SeedDetails(*status_assets,frame_layout,seed,
                        Randomizer::FileStatus::ShortRoomId(m.Read(Owner+0x14).value_or(3)));
                    seed_shadow_geometry=SeedBackdrop(status_geometry,frame_layout.seed_shadow);
                    if(RoomJoin::phase==RoomJoin::Phase::ProfileLeaving) {
                        for(unsigned i=3;i<status_geometry.count*16;i+=4)status_geometry.colors[i]*=RoomJoin::opacity.load();
                        for(unsigned i=3;i<seed_shadow_geometry.count*16;i+=4)seed_shadow_geometry.colors[i]*=RoomJoin::opacity.load();
                    }
                }
                if(join_alpha>0) {
                    const auto label=FileStatusText::Text(*status_assets,UILanguage::Join(),
                        jp.x+20*jp.scale,jp.y+15*jp.scale,1000*jp.scale,.75f*jp.scale);
                    const unsigned n=status_geometry.count;
                    std::copy_n(label.positions.begin(),label.count*12,status_geometry.positions.begin()+n*12);
                    std::copy_n(label.uvs.begin(),label.count*8,status_geometry.uvs.begin()+n*8);
                    std::copy_n(label.colors.begin(),label.count*16,status_geometry.colors.begin()+n*16);
                    status_geometry.count+=label.count;
                    for(unsigned i=n*16+3;i<status_geometry.count*16;i+=4)status_geometry.colors[i]=join_alpha;
                }
                if(host_alpha>0) {
                    const auto label=FileStatusText::Text(*status_assets,UILanguage::Host((RoomJoin::hosting.load() || RoomJoin::host_opening.load()) && !RoomJoin::stopping.load()),
                        hp.x+20*hp.scale,hp.y+15*hp.scale,1000*hp.scale,.75f*hp.scale);
                    const unsigned n=status_geometry.count;
                    if((n+label.count)*12<=status_geometry.positions.size()) {
                        std::copy_n(label.positions.begin(),label.count*12,status_geometry.positions.begin()+n*12);
                        std::copy_n(label.uvs.begin(),label.count*8,status_geometry.uvs.begin()+n*8);
                        std::copy_n(label.colors.begin(),label.count*16,status_geometry.colors.begin()+n*16);
                        status_geometry.count+=label.count;
                        for(unsigned i=n*16+3;i<status_geometry.count*16;i+=4)status_geometry.colors[i]=host_alpha;
                    }
                }
                if(!name_frame && !RoomJoin::HideFiles() && (!quest_frame || return_frame))
                    RandomizerBadges(m,status_geometry,*status_assets,atlas_geometry,frame_layout,bounds,randomizer_badges.visible);
                if(player_count_alpha>0 && RoomPlayerIcon::GetPixels()) {
                    const auto p=PausePrompts::Valid(frame_layout.player_count)?frame_layout.player_count:PausePrompts::Pose{};
                    const float x=ScreenLeft()+16+p.x,y=20+p.y,s=p.scale;
                    const float a=player_count_alpha*alpha;
                    player_icon_geometry.Add(x,y,20*s,20*s,8.f/32,8.f/32,24.f/32,24.f/32);
                    for(unsigned v=0;v<4;++v)player_icon_geometry.colors[v*4+3]=a;
                    const auto label=FileStatusText::Text(*status_assets,std::to_string(displayed_players),x+24*s,y+18*s,60*s,.9f*s);
                    const unsigned n=status_geometry.count;
                    if((n+label.count)*12<=status_geometry.positions.size()) {
                        std::copy_n(label.positions.begin(),label.count*12,status_geometry.positions.begin()+n*12);
                        std::copy_n(label.uvs.begin(),label.count*8,status_geometry.uvs.begin()+n*8);
                        std::copy_n(label.colors.begin(),label.count*16,status_geometry.colors.begin()+n*16);
                        status_geometry.count+=label.count;
                        for(unsigned i=n*16+3;i<status_geometry.count*16;i+=4)status_geometry.colors[i]=a;
                    }
                }
            }
            if(assets) {
                const float buttons_alpha=(quest_frame?1.f:alpha)*(name_save.Visible()?1-name_save.opacity:1.f)*prompt_transition.standard*(1-operation_prompt.opacity);
                auto g=RoomJoin::GuestActive()?Prompts(frame_layout,*assets,RoomJoin::guest==RoomJoin::Guest::Waiting?buttons_alpha:0.f,true):
                    RoomJoin::WaitingScreen()?WaitingPrompts(frame_layout,*assets,RoomJoin::CanCancel()?RoomJoin::opacity.load():0.f):
                    StandardPromptsVisible(m)?Prompts(frame_layout,*assets,buttons_alpha):PausePrompts::Pair{};
                lettering=g.text;
                if(toggle_prompt && status_assets) {
                    g.icons.Add(randomizer_edge.button,p.y,16*p.scale,16*p.scale,208.5f/256,95.5f/128,223.5f/256,80.5f/128);
                    for(unsigned v=0;v<4;++v)g.icons.colors[(g.icons.count-1)*16+v*4+3]=prompt_alpha;
                }
                if(join_alpha>0 && status_assets) {
                    g.icons.Add(jp.x,jp.y,16*jp.scale,16*jp.scale,160.5f/256,95.5f/128,175.5f/256,80.5f/128);
                    for(unsigned v=0;v<4;++v)g.icons.colors[(g.icons.count-1)*16+v*4+3]=join_alpha;
                }
                if(host_alpha>0 && status_assets) {
                    g.icons.Add(hp.x,hp.y,16*hp.scale,16*hp.scale,176.5f/256,95.5f/128,191.5f/256,80.5f/128);
                    for(unsigned v=0;v<4;++v)g.icons.colors[(g.icons.count-1)*16+v*4+3]=host_alpha;
                }
                icons.Begin(m,r,entry,g.icons,assets->icons,source);
                if(icons.Pending()){stage=Stage::Icons;return;}
            }
        } else if(stage==Stage::Icons) {
            icons.Resume(m,r);if(icons.Pending())return;
            text.Begin(m,r,entry,lettering,assets->text,source);
            if(text.Pending()){stage=Stage::Text;return;}
        } else if(stage==Stage::Text) {text.Resume(m,r);if(text.Pending())return;}
        else if(stage==Stage::SeedShadow) {
            seed_shadow.Resume(m,r);if(seed_shadow.Pending())return;
            BeginStatus(m,r);return;
        }
        else if(stage==Stage::Status) {
            status_text.Resume(m,r);if(status_text.Pending())return;
            FinishStatus(m,r);return;
        }
        else if(stage==Stage::PlayerCount) {
            player_icon.Resume(m,r);if(player_icon.Pending())return;
            if(name_frame && !name_save.Visible())stage=Stage::Idle;else BeginLoading(m,r);
            return;
        }
        if(seed_shadow_geometry.count) {
            seed_shadow.Begin(m,r,entry,seed_shadow_geometry,ShadowPixels,source);
            if(seed_shadow.Pending()) {stage=Stage::SeedShadow;return;}
        }
        BeginStatus(m,r);
    }
    template<class M,class R> void BeginStatus(M& m,R& r) {
        if(status_assets && status_geometry.count) {
            status_text.Begin(m,r,entry,status_geometry,status_assets->pixels,source);
            if(status_text.Pending()) {stage=Stage::Status;return;}
        }
        FinishStatus(m,r);
    }
    template<class M,class R> void FinishStatus(M& m,R& r) {
        status_assets.reset();assets.reset();
        if(player_icon_geometry.count)if(const auto pixels=RoomPlayerIcon::GetPixels()) {
            player_icon.Begin(m,r,entry,player_icon_geometry,*pixels,source);
            if(player_icon.Pending()){stage=Stage::PlayerCount;return;}
        }
        if(name_frame && !name_save.Visible())stage=Stage::Idle;else BeginLoading(m,r);
    }
    template<class M,class R> void BeginNamePrompts(M& m,R& r) {
        stage=Stage::Idle;
        const bool saving=name_save.active;
        if(!saving && prompt_transition.keyboard<=0)return;
        if(saving && name_save.opacity>=1) {BeginLoading(m,r);return;}
        assets=PausePrompts::GetAssets();status_assets=FileStatusText::GetAssets();
        if(!assets || !status_assets) {if(saving)BeginLoading(m,r);return;}
        const float fade=saving?1-name_save.opacity:prompt_transition.keyboard;
        PausePrompts::Geometry buttons;lettering={};status_geometry={};seed_shadow_geometry={};
        for(unsigned i=0;i<2;++i) {
            const auto name_label=NamePromptLabel(i);
            const auto p=NamePromptPose(frame_layout,i);
            const float s=std::min(p.scale,236.f/16),y=std::clamp(p.y,2.f,238-16*s),tx=i?144:128;
            const auto edge=PlaceButtonLabel(p.x,s,FileStatusText::Measure(*status_assets,name_label)+.75f);
            buttons.Add(edge.button,y,16*s,16*s,(tx+.5f)/256,95.5f/128,(tx+15.5f)/256,80.5f/128);
            const auto label=FileStatusText::Text(*status_assets,name_label,edge.text,y+8*s+7.5f*edge.text_scale,1000*s,.75f*edge.text_scale);
            const unsigned n=status_geometry.count;
            std::copy_n(label.positions.begin(),label.count*12,status_geometry.positions.begin()+n*12);
            std::copy_n(label.uvs.begin(),label.count*8,status_geometry.uvs.begin()+n*8);
            std::copy_n(label.colors.begin(),label.count*16,status_geometry.colors.begin()+n*16);
            status_geometry.count+=label.count;
        }
        for(unsigned i=3;i<buttons.count*16;i+=4)buttons.colors[i]=fade;
        for(unsigned i=3;i<status_geometry.count*16;i+=4)status_geometry.colors[i]=fade;
        icons.Begin(m,r,entry,buttons,assets->icons,source);
        if(icons.Pending()){stage=Stage::Icons;return;}
        BeginStatus(m,r);
    }
    template<class M,class R> void BeginName(M& m,R& r) {
        keyboard_quads=0;keyboard_alpha=0;keyboard_bounds={};keyboard_submitted=false;
        if(name_editable)if(const auto font=FileStatusText::GetAssets()) {
            auto g=KeyboardFading() && saving_keyboard?saving_keyboard->text:
                KeyboardText(m,frame_layout,name_progress,&character_growth);
            FadeSavingKeyboard(g);
            keyboard_quads=g.count;
            if(g.count)keyboard_bounds=Bounds(g,0,g.count-1);
            for(unsigned i=3;i<g.count*16;i+=4)keyboard_alpha=std::max(keyboard_alpha,g.colors[i]);
            keyboard_text.Begin(m,r,entry,g,font->pixels,source);
            if(keyboard_text.Pending()){stage=Stage::Keyboard;return;}
        }
        if(!name_editable)character_growth.Reset();
        BeginNativeName(r);
    }
    template<class R> void BeginNativeName(R& r) {
        saved=r;stage=Stage::Name;
        on_top.store(true);name_on_top.store(true);
        r[14]=entry;r[15]=NameDraw;
    }
    template<class M,class R> void BeginCards(M& m,R& r) {
        if(name_frame) {
            if(name_editable) {
                auto g=KeyboardFading() && saving_keyboard?saving_keyboard->shadow:NameShadow(m,frame_layout,name_progress);
                FadeSavingKeyboard(g);
                shadows.Begin(m,r,entry,g,ShadowPixels,source);
                if(shadows.Pending()){stage=Stage::Shadows;return;}
            }
            BeginName(m,r);return;
        }
        auto shadow_geometry=quest_frame && QuestChoice::GetPixels()?
            QuestShadows(quest_geometry,frame_layout.stone_shadow_opacity):
            StoneShadows(atlas_geometry,frame_layout.stone_shadow_opacity);
        if(RoomJoin::HideFiles())shadow_geometry={};
        if(return_frame)for(unsigned first:{6u,43u,80u})
            AddStoneShadow(shadow_geometry,Bounds(atlas_geometry,first,first+2),
                           atlas_geometry.colors[first*16+3],frame_layout.stone_shadow_opacity);
        shadows.Begin(m,r,entry,shadow_geometry,ShadowPixels,source);
        if(shadows.Pending()){stage=Stage::Shadows;return;}
        BeginAtlas(m,r);
    }
    template<class M,class R> void BeginAtlas(M& m,R& r) {
        if(quest_frame) {
            if(const auto image=QuestChoice::GetPixels()) {
                quest.Begin(m,r,entry,quest_geometry,*image,source);
                stage=quest.Pending()?Stage::Quest:Stage::Idle;return;
            }
        }
        top.Begin(m,r,entry,atlas_geometry,{},source);
        stage=top.Pending()?Stage::Atlas:Stage::Idle;
    }
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc,bool supported) {
        on_top.store(false);name_on_top.store(false);
        if(!supported || !NameActive(m))NameTyping::Publish(false);
        player_icon_geometry={};
        if(!supported || !CodeMatches(m))return;
        name_frame=NameActive(m);
        name_save.Frame();
        if(!name_frame) {was_naming=false;name_frames=0;character_growth.Reset();}
        if(!name_frame && !Active(m)){stone_focus={};particle_animation.Reset();prompt_transition={};randomizer_badges={};operation_prompt={};player_count_alpha=0;displayed_players=0;return;}
        if(name_frame)operation_prompt={};else operation_prompt.Frame(FileOperationBusy(m));
        randomizer_badges.Frame(m);
        frame_layout=PresentationLayout(GetLayout());
        stone_focus.Apply(frame_layout,FocusedStone(m,name_frame));
        name_editable=name_frame && m.Read(m.Read(NameOwner+0x6c).value_or(0))==1 &&
            m.Read(m.Read(NameOwner+0x70).value_or(0))==62;
        if(name_frame) {
            const auto state=m.Read(NameOwner+4).value_or(0);
            // Native state 2 still creates the default name's glyphs. Hold the
            // whole group offscreen until ready, then advance on every 60 Hz draw.
            // Joining an already-open keyboard must not replay its entrance.
            if(!was_naming)name_frames=state<=2?0:NameSlideFrames;
            if(state<=2 || !name_editable)name_progress=0;
            else {
                name_progress=std::min(float(name_frames)/NameSlideFrames,1.f);
                name_frames=std::min(name_frames+1,NameSlideFrames);
            }
            was_naming=true;
        }
        const auto name_state=m.Read(NameOwner+4).value_or(0),name_mode=m.Read(NameOwner+8).value_or(6);
        const bool typing=name_frame && name_editable && (name_state==3 || name_state==4) &&
            (name_mode==2 || name_mode==3);
        prompt_transition.Frame(typing,!name_frame && StandardPromptsVisible(m),
            NameEase(name_progress),name_save.Visible());
        if(!name_frame && MasterQuest::choosing.load() && !MasterQuest::cancelling.load() &&
           PausePrompts::Valid(frame_layout.quest_description)) {
            auto& p=frame_layout.parts[Prompt];if(!PausePrompts::Valid(p))p={};
            const auto extra=frame_layout.quest_description;
            p.x+=extra.x;p.y+=extra.y;p.scale*=extra.scale;
        }
        if(name_save.entering) {
            frame_layout.saved_file_slot=name_save.slot;
            const float remaining=1-std::min(name_save.entrance_frames/float(NameSaveFlow::EntranceFrames),1.f);
            frame_layout.saved_file_remaining=EasePosition(remaining,1,0,frame_layout.stone_ease_out);
        }
        const auto g=name_frame?Geometry{}:Atlas(m,false,frame_layout,&bounds);
        if(!name_frame && !g.count)return;
        {std::scoped_lock lock{mutex};touch_bounds=bounds;touch_layout=frame_layout;
            name_touch_progress=name_progress;name_touch_ready=name_editable &&
                m.Read(NameOwner+4)==4 && m.Read(NameOwner+8).value_or(6)<=4;
            name_touch_confirmation=m.Read(NameOwner+8)==4;
            name_touch_keys=CustomKeys(m,frame_layout);}
        source=m.Read(name_frame?NameOwner+0x64:Owner+8).value_or(0);entry=return_pc;
        quest_frame=!name_frame && !RoomJoin::HideFiles() && MasterQuest::choosing.load() && !MasterQuest::departing_files.load();
        Randomizer::GenerationProgress::UpdatePresentation(MasterQuest::cursor_us.load(),quest_frame && MasterQuest::loading_label.load());
        join_prompt_fade.Frame(quest_frame,Randomizer::FileStatus::enabled.load());
        profile_prompt_alpha=RoomJoin::phase==RoomJoin::Phase::ProfileLeaving?RoomJoin::opacity.load():
            RoomJoin::profile_visible.load()?std::min(1.f,profile_prompt_alpha+1.f/9):0;
        const unsigned members=RoomJoin::player_count.load();
        const bool show_players=!name_frame && m.Read(Owner+0x10)==8 && members>0 &&
            !RoomJoin::stopping.load() && (RoomJoin::hosting.load() || RoomJoin::guest==RoomJoin::Guest::Waiting);
        player_count_alpha=std::clamp(player_count_alpha+(show_players?1.f:-1.f)/9,0.f,1.f);
        if(show_players)displayed_players=members;
        return_frame=quest_frame && MasterQuest::cancelling.load();
        quest_selection=MasterQuest::highlighted.load();
        quest_geometry=QuestStones(quest_selection,MasterQuest::Offsets(frame_layout.stone_ease_out),frame_layout);
        {std::scoped_lock lock{mutex};quest_touch_bounds={Bounds(quest_geometry,1,1),Bounds(quest_geometry,3,3)};}
        alpha=name_frame?1.f:g.colors[3];
        atlas_geometry=g;
        if(name_save.returning)for(unsigned q=0;q<atlas_geometry.count;++q)Hide(atlas_geometry,q);
        badge_geometry=(name_frame || RoomJoin::HideFiles() || name_save.returning || (quest_frame && !return_frame))?BadgeGeometry{}:QuestBadges(m,g);
        // Build the destination text on this same frame, using the original
        // controller/font renderer. Native timing otherwise leaves it blank.
        const auto state=m.Read(Owner+0x10).value_or(0);
        prompt_slot=(MasterQuest::starting.load() || RoomJoin::GuestPrompt() || RoomJoin::HostPrompt() || MasterQuest::start_error!=MasterQuest::FileError::None || MasterQuest::notice_error!=MasterQuest::FileError::None)?6:TransitionPromptSlot(state,m.Read(Owner+0x2c).value_or(0)!=0,CopyDestinationSlot(m));
        if(!name_frame && prompt_slot) {
            prompt_at=0x55b548+prompt_slot*4;
            const U32 controller=m.Read(prompt_at).value_or(0);
            const U32 message=controller==MasterQuest::prompt_controller.load()?
                MasterQuest::prompt_message.load():0x950;
            if(!controller || (prompt_slot==6 && message!=MasterQuest::PromptMessage())) {
                saved=r;r[14]=entry;
                if(controller) {stage=Stage::PromptDestroy;r[0]=controller;r[15]=0x2f6944;}
                else {stage=Stage::PromptInit;r[0]=prompt_slot;r[15]=0x2ee864;}
                return;
            }
        }
        BeginBackdrop(m,r);
    }
    template<class M> void FitPromptBox(M& m) {
        fitted_prompt_text.reset();
        if(name_frame || !prompt_slot || !atlas_geometry.count)return;
        const auto controller=m.Read(0x55b548+prompt_slot*4).value_or(0);
        const auto text_bounds=MessageBounds(m,controller);
        const unsigned part=(prompt_slot==18 || prompt_slot==21 || prompt_slot==24)?LowerPrompt:Prompt;
        const unsigned first=part==Prompt?0:162,last=part==Prompt?5:170;
        const auto box=Bounds(atlas_geometry,first,last);
        if(!text_bounds.Valid() || !box.Valid() || box.right-box.left<10)return;
        auto t=PartTransform(frame_layout,part,bounds);
        const float left=t.x+t.ax+(text_bounds.left-t.ax)*t.sx;
        const float right=t.x+t.ax+(text_bounds.right-t.ax)*t.sx;
        // Atlas positions still use the centered 320px source coordinates.
        const auto fit=FitMessage(box.left+40,box.right+40,left,right);
        const float dl=fit.left-(box.left+40),dr=fit.right-(box.right+40);
        for(unsigned q=first;q<=last;++q)for(unsigned v=0;v<4;++v) {
            auto& x=atlas_geometry.positions[q*12+v*3];
            // Translate caps intact; stretch only the center slices.
            x+=(q-first)%3==0?dl:(q-first)%3==2?dr:x<box.X()?dl:dr;
        }
        const float cx=text_bounds.X(),cy=text_bounds.Y();
        t.x+=fit.dx+(1-fit.scale)*(cx-t.ax)*t.sx;
        t.y+=(1-fit.scale)*(cy-t.ay)*t.sy;
        t.sx*=fit.scale;t.sy*=fit.scale;
        fitted_prompt_text=t;
    }
    template<class M,class R> void BeginBackdrop(M& m,R& r) {
        FitPromptBox(m);
        if(const auto image=FileBackground::GetPixels()) {
            backgrounds.Begin(m,r,entry,Backgrounds(frame_layout),*image,source);
            if(backgrounds.Pending()){stage=Stage::Background;return;}
        }
        BeginVignette(m,r);
    }
    template<class M,class R> void BeginVignette(M& m,R& r) {
        vignette.Begin(m,r,entry,CornerVignette(frame_layout.vignette,Widescreen::Active()),ShadowPixels,source);
        if(vignette.Pending()){stage=Stage::Vignette;return;}
        BeginParticles(m,r);
    }
    template<class M,class R> void BeginParticles(M& m,R& r) {
        const auto g=particle_animation.Frame(frame_layout.particles,
            Widescreen::Active()?240*Widescreen::Aspect:400);
        if(g.count)if(const auto image=FileParticles::GetPixels()) {
            particles.Begin(m,r,entry,g,*image,source);
            if(particles.Pending()){stage=Stage::Particles;return;}
        }
        BeginCards(m,r);
    }
    template<class M> void Material(M& m,U32 at,U32 model=0) const {
        if(!Drawing() && !PromptDrawing())return;
        const auto word=m.Read(at);if(!word)return;
        const float native=std::bit_cast<float>(*word);
        if(NativeDrawing() && RoomJoin::phase==RoomJoin::Phase::ProfileLeaving) {
            if(std::isfinite(native))m.Write(at,std::bit_cast<U32>(native*RoomJoin::opacity.load()));
            return;
        }
        if(stage==Stage::Name && KeyboardFading() && saving_keyboard) {
            // Atlas vertices already carry the fade. Native name glyphs use
            // the same presentation clock instead of the game's exit fade.
            const bool atlas=name_atlases[0].Drawing() || name_atlases[1].Drawing();
            if(std::isfinite(native))m.Write(at,std::bit_cast<U32>(atlas?1.f:KeyboardOpacity()));
            return;
        }
        if(NativeDrawing() && (MasterQuest::starting.load() || RoomJoin::StatusPrompt() || (quest_frame && MasterQuest::loading_label.load())) && LoadingPrompt::Ready()) {
            const U32 controller=m.Read(0x55b548+6*4).value_or(0);
            if(TitleScreen::Pointer(controller,0x4c)) {
                const auto kind=m.Read(controller+0x40).value_or(0);
                const unsigned count=kind==2?9:kind==3?1:2;
                for(unsigned i=0;i<count;++i)if(m.Read(controller+0x10+4*i)==model) {
                    const float opacity=LoadingPrompt::PresentationOpacity(MasterQuest::cursor_us.load(),MasterQuest::starting.load(),quest_frame && MasterQuest::loading_label.load())*
                        (RoomJoin::WaitingScreen()?RoomJoin::opacity.load():1.f);
                    if(std::isfinite(native))m.Write(at,std::bit_cast<U32>(native*opacity));
                    return;
                }
            }
        }
        // Draw beneath the native blackout instead of fading the panels' alpha:
        // transparent panels exposed the blue sky during both exit transitions.
        if(std::isfinite(native) && (PromptDrawing() || particles.Drawing() || vignette.Drawing() || shadows.Drawing() || backgrounds.Drawing() || quest.Drawing() || badges.Drawing()))
            m.Write(at,std::bit_cast<U32>(1.f));
    }
    template<class M,class R> void NativeDecoration(M& m,R& r,unsigned site,bool supported) const {
        // These two calls belong exclusively to z_file_choose; title actors use
        // a separate path. Leave the stock path intact when top routing is unavailable.
        r[14]=Sites[site].pc+4;
        r[15]=supported && (Active(m) || NameActive(m))?r[14]:0x2d7354;
    }
    template<class R> void NameBottom(R& r) const {
        if(name_on_top.load()) {r[15]=Sites[2].pc+4;return;}
        r[14]=Sites[2].pc+4;r[15]=NameDraw;
    }
    template<class M,class R> void NameSubmit(M& m,R& r) {
        const U32 entry_pc=Sites[6].pc;
        for(auto& hook:name_atlases)if(hook.Pending()) {
            hook.Resume(m,r);
            if(!hook.Pending() && !hook.Owned(m)) {r[14]=entry_pc+4;r[15]=r[1];}
            return;
        }
        const unsigned atlas=r[4];
        r[14]=entry_pc+4;
        if(stage!=Stage::Name || !name_editable || atlas>1) {r[15]=r[1];return;}
        bool valid=false;
        auto geometry=NameAtlas(m,atlas,frame_layout,name_progress,&valid);
        if(KeyboardFading() && saving_keyboard) {geometry=saving_keyboard->atlases[atlas];valid=true;}
        if(!valid) {r[15]=r[1];return;}
        FadeSavingKeyboard(geometry);
        r[15]=entry_pc+4;
        if(!geometry.count)return; // Parked atlas, not a failure.
        name_atlases[atlas].Begin(m,r,entry_pc,geometry,{},r[0]);
    }
    template<class M,class R> void Top(M& m,R& r,bool supported,std::int64_t now=0) {
        if(multiplayer_loading.Pending()) {multiplayer_loading.Resume(m,r);return;}
        if(multiplayer_loading.after_native_fade) {
            multiplayer_loading.after_native_fade=false;
            r[15]=Sites[3].pc+4;
            multiplayer_loading.Begin(m,r,Sites[3].pc,now);return;
        }
        if(Pending()) {Resume(m,r);return;}
        r[14]=Sites[3].pc+4;r[15]=0x4228c0; // Native fade pass follows our UI.
        if(supported && !Active(m) && !NameActive(m) && multiplayer_loading.Needed()) {
            on_top=false;name_on_top=false;
            multiplayer_loading.after_native_fade=true;
            r[14]=Sites[3].pc;return; // Show loading above the native scene fade.
        }
        Begin(m,r,Sites[3].pc,supported);
    }
    template<class M,class R> void Bottom(M& m,R& r) {
        if(bottom.Pending()){bottom.Resume(m,r);return;}
        r[14]=Sites[0].pc+4;r[15]=0x41f2e4; // Displaced BL, native fallback.
        if(!on_top.load() || !Active(m))return;
        const auto g=Atlas(m,true);if(!g.count)return;
        r[15]=Sites[0].pc+4;
        bottom.Begin(m,r,Sites[0].pc,g,{},m.Read(Owner+8).value_or(0));
    }
};
}
