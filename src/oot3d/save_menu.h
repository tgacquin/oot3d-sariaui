// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include <atomic>
#include <mutex>
#include <vector>

namespace OOT3D::SaveMenu {
using TitleScreen::U32;
constexpr U32 Owner=0x50a508;
enum Part { DontSave, Save, Prompt, Back, Options, OptionsItems, HelpMessage, Cancel, Okay, CardWarning, Count };
constexpr std::array Names{"dont_save","save","prompt","back","options","options_items","help_message","cancel","okay","card_warning"};
constexpr std::array Labels{"Don't save button","Save button","Prompt box and text","B Back","Options button",
    "Options controls (all)","Options help message","Options: Cancel button","Options: OK button","Game Card warning box and text"};
constexpr std::array<std::array<float,2>,Count> Anchors{{
    {96,120},{224,120},{160,24},{28,220},{160,220},{160,120},{200,195},{52,220},{268,220},{160,204}}};
struct Pose {float x=0,y=0,scale=1;};
struct Layout : std::array<Pose,Count> {float darkness=.45f;bool hide_header=false;std::array<Pose,2> prompts{};};
inline Layout layout;
inline std::mutex mutex;
inline std::atomic<bool> dirty{false},on_top{false};
enum class InputPage { Hidden, Save, Options };
inline std::atomic<InputPage> input_page{InputPage::Hidden};
inline bool animate=false; // Set by the native 60 Hz pause clock on the emulation thread.
inline bool returning_from_options=false;
struct Animation {float buttons=1,prompt=1,rise=0;};
template<class M> Animation CurrentAnimation(M& m) {
    if(!animate)return {};
    if(m.Read(Owner+0x28)==3) {
        const auto sub=m.Read(Owner+0x3c).value_or(2);
        const float t=std::clamp((float(m.Read(Owner+0x2c).value_or(0))-1)/8.f,0.f,1.f);
        const float ease=1-(1-t)*(1-t)*(1-t);
        if(sub==1)return {ease,ease,18*(1-ease)};
        if(sub==3)return {1-ease,1-ease,18*ease};
        return {};
    }
    if(m.Read(Owner+0x28)!=2)return {};
    const auto state=m.Read(Owner+0x24).value_or(1);
    // The native update increments its clock after evaluating the pose.
    const auto tick=m.Read(Owner+0x2c).value_or(0);
    const float t=std::clamp((float(tick)-1)/8.f,0.f,1.f);
    const float ease=1-(1-t)*(1-t)*(1-t);
    if(state==0)return {ease,ease,18*(1-ease)};
    if(state==10)return {ease,1,18*(1-ease)};
    if(state==2 || state==15 || state==14)return {1-ease,1-ease,18*ease};
    if(state==7 || state==8)return {1-ease,1,18*ease};
    if(state==9)return {0,1,18}; // Saving: buttons remain hidden until the next prompt.
    return {};
}
inline Layout GetLayout() {std::scoped_lock lock{mutex};return layout;}
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{mutex};layout=value;if(save)dirty=true;
}
constexpr bool RisingControl(Part part) {return part==Save || part==DontSave || part==Options;}
inline bool Valid(const Pose& p) {
    return std::isfinite(p.x) && std::abs(p.x)<=400 && std::isfinite(p.y) &&
        std::abs(p.y)<=240 && std::isfinite(p.scale) && p.scale>=.1f && p.scale<=3;
}
template<class M> bool Active(M& m) {
    const auto state=m.Read(Owner+0x28).value_or(0);
    return state>=1 && state<=3;
}
template<class M> bool QuitTransition(M& m) {
    // 4392a8 state 16 requests the return-to-menu transition, then 2e9920
    // clears +28 before the scene fade completes. Keep only the backdrop
    // alive; the destroyed save widgets must not be revived.
    if(m.Read(Owner+0x28)!=0 || m.Read(Owner+0x24)!=16 || m.Read(0x588e3c)!=1)return false;
    const U32 play=m.Read(0x5043d4+0xc).value_or(0);
    return TitleScreen::Pointer(play,0x5c78) && (m.Read(play+0x100).value_or(0)&0xffff)==0x203 &&
        ((m.Read(play+0x5c2c).value_or(0)>>8)&255)==20 &&
        ((m.Read(play+0x5c74).value_or(0)>>16)&255)==2;
}
template<class M> bool Quitting(M& m) {
    // State 16 is selected only by Quit; Continue selects state 14.
    return (m.Read(Owner+0x28)==2 && m.Read(Owner+0x24)==16) || QuitTransition(m);
}
template<class M> bool BackdropActive(M& m) {return Active(m) || QuitTransition(m);}
// The native fullscreen pause tint (0x41ec14) used to stack with our
// configurable backdrop on a later frame. Preserve their settled composite,
// but submit one rectangle on the prompt's own opening clock.
constexpr float NativeBackdropAlpha=.7f;
template<class M> float BackdropOpacity(M& m) {
    const float configured=GetLayout().darkness;
    const float darkness=std::isfinite(configured)?std::clamp(configured,0.f,1.f):.45f;
    const auto page=m.Read(Owner+0x28).value_or(0);
    if(returning_from_options && page>=1 && page<=3)
        return animate?1-(1-NativeBackdropAlpha)*(1-darkness):darkness;
    if(page==1)return 0; // Controller preparation, before prompt construction.
    if(!animate && !QuitTransition(m))return darkness;
    const float settled=1-(1-NativeBackdropAlpha)*(1-darkness);
    const auto state=m.Read(Owner+0x24).value_or(~0u);
    // State 15 hands off to Options (43a1cc), rather than returning to gameplay.
    if(page==2 && (state==0 || state==2))
        return settled*CurrentAnimation(m).prompt;
    // Internal Save/Options/confirmation transitions and quitting retain the
    // existing shade; they must not replay an entrance or expose the world.
    return settled;
}
template<class M> bool HideHeader(M& m) {
    const auto state=m.Read(Owner+0x28).value_or(0);
    return state==3 || ((state==1 || state==2 || QuitTransition(m)) && GetLayout().hide_header);
}
template<class M> bool OptionsActive(M& m) {
    return m.Read(Owner+0x28)==std::optional<U32>{3};
}
template<class M> InputPage CurrentInputPage(M& m) {
    const auto state=m.Read(Owner+0x28).value_or(0);
    if(state==2) {
        // Native 0x2e9768 parks the cursor for opening, closing, saving and
        // switching pages. Only these states have a selectable save control.
        const auto sub=m.Read(Owner+0x24).value_or(0);
        if(sub==1 || (sub>=3 && sub<=6) || (sub>=11 && sub<=13))
            return InputPage::Save;
    } else if(state==3) {
        // State 3 is the Options page, not the save menu's closing animation.
        const auto sub=m.Read(Owner+0x3c).value_or(1);
        if(sub==0 || sub==2 || (sub>=4 && sub<=8))return InputPage::Options;
    }
    return InputPage::Hidden;
}
template<class M> std::optional<Part> PromptPart(M& m,U32 model) {
    for(U32 off:{0x1cu,0x20u}) {
        const U32 text=m.Read(Owner+off).value_or(0);
        if(!TitleScreen::Pointer(text,0x4c))continue;
        const auto kind=m.Read(text+0x40).value_or(0);
        const unsigned count=kind==2?9:kind==3?1:2;
        for(unsigned i=0;i<count;++i)
            if(m.Read(text+0x10+4*i)==std::optional<U32>{model})
                return off==0x20 && m.Read(Owner+0x28)==2 && m.Read(Owner+0x24)==9?CardWarning:Prompt;
    }
    return {};
}
template<class M> bool PromptModel(M& m,U32 model) {return PromptPart(m,model).has_value();}
// Warning controls add to the existing prompt layout so saved arrangements
// remain unchanged until the new controls are adjusted.
inline Pose ControlPose(Part part,const Layout& poses) {
    if(part!=CardWarning)return poses[part];
    const auto base=poses[Prompt],extra=poses[CardWarning];
    return {base.x+extra.x,base.y+extra.y+.8f*(Anchors[CardWarning][1]-Anchors[Prompt][1])*(base.scale-1),
            base.scale*extra.scale};
}
inline std::array<float,2> Move(float x,float y,Part part,const Pose& p) {
    const auto a=Anchors[part];
    return {a[0]+(x-a[0])*p.scale+p.x/.8f,a[1]+(y-a[1])*p.scale+p.y/.8f};
}
constexpr bool OptionsButton(Part part) {return part==Cancel || part==Okay;}
// Individual button adjustments compose with the existing whole-Options pose,
// preserving saved layouts. X/Y remain screen-pixel offsets.
inline std::array<float,2> MoveControl(float x,float y,Part part,const Layout& poses) {
    if(!OptionsButton(part))return Move(x,y,part,ControlPose(part,poses));
    const auto a=Anchors[part];
    const auto center=Move(a[0],a[1],OptionsItems,poses[OptionsItems]);
    const auto point=Move(x,y,OptionsItems,poses[OptionsItems]);
    const auto p=poses[part];
    return {center[0]+(point[0]-center[0])*p.scale+p.x/.8f,
            center[1]+(point[1]-center[1])*p.scale+p.y/.8f};
}
// The save controller's atlas has fixed semantic groups. Never classify these
// by their animated coordinates or through the map/item layout.
inline std::optional<Part> AtlasPart(unsigned q,bool labels) {
    if(labels)return q==0 || (q>=2 && q<=4)?DontSave:Save;
    if(q<6 || (q>=22 && q<=30))return Prompt;
    if((q>=6 && q<=9) || (q>=34 && q<=36))return Save;
    if((q>=10 && q<=13) || (q>=31 && q<=33))return DontSave;
    if((q>=15 && q<=17) || (q>=37 && q<=39))return Back;
    if((q>=18 && q<=21) || (q>=40 && q<=42))return Options;
    if((q>=64 && q<=67) || (q>=72 && q<=74))return Cancel;
    if((q>=68 && q<=71) || (q>=75 && q<=77))return Okay;
    if(q>=43 && q<64)return OptionsItems;
    return {};
}
template<class M> bool CopyPositions(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    const bool atlas=m.Read(Owner)==std::optional<U32>{mesh};
    const bool labels=m.Read(Owner+0xc)==std::optional<U32>{mesh};
    const U32 cursor=m.Read(Owner+0x18).value_or(0);
    const bool selection=cursor && m.Read(cursor)==std::optional<U32>{mesh};
    if(!Active(m) || (!atlas && !labels && !selection) ||
       bytes!=(atlas?78u:8u)*48 || !TitleScreen::Pointer(src,bytes) ||
       !TitleScreen::Pointer(dst,bytes))return false;
    const auto poses=GetLayout();
    for(const auto& p:poses)if(!Valid(p))return false;
    std::vector<std::array<float,12>> quads(bytes/48);
    for(unsigned q=0;q<quads.size();++q)
        if(!TitleScreen::ReadFloats(m,src+q*48,quads[q]))return false;
    for(U32 off=0;off<bytes;off+=4)if(!m.Read(dst+off))return false;
    const auto page=CurrentInputPage(m);
    const auto state=m.Read(Owner+0x28).value_or(0);
    const auto sub=m.Read(Owner+0x24).value_or(1);
    const auto animation=CurrentAnimation(m);
    const U32 helper=m.Read(Owner+(atlas?8:0x14)).value_or(0);
    const U32 offsets=m.Read(helper+0x1c).value_or(0);
    for(unsigned q=0;q<quads.size();++q) {
        const auto part=selection?
            (page==InputPage::Save?std::optional<Part>{
                m.Read(Owner+0x38).value_or(0)==0?
                    (m.Read(Owner+0x34).value_or(1)?Save:DontSave):Options}:
                page==InputPage::Options?std::optional<Part>{
                    m.Read(Owner+0x30).value_or(0)==3?Cancel:
                    m.Read(Owner+0x30).value_or(0)==4?Okay:OptionsItems}:std::nullopt):
            (atlas && state==2 && sub==9 && q>=22 && q<=30?
                std::optional<Part>{CardWarning}:AtlasPart(q,labels));
        auto& v=quads[q];
        // Undo the actual native horizontal slide only for the quads animated
        // in this state. In particular, original Save/Don't Save lettering
        // (9/13) must remain parked when the post-save confirmation replaces it.
        const bool primary=sub==0 || sub==2 || sub==7 || sub==8 || sub==15;
        const bool confirmation=sub==10 || sub==14;
        const bool moving=atlas?(q>=6 && q<14 && (primary || (confirmation && q!=9 && q!=13))):
            labels && confirmation && q<2;
        const bool options_moving=atlas && q>=18 && q<22 && primary;
        if(animate && state==2 && (moving || options_moving) && m.Read(helper)==std::optional<U32>{atlas?78u:8u}) {
            std::array<float,2> offset{};
            if(TitleScreen::Pointer(offsets,quads.size()*8) &&
               TitleScreen::ReadFloats(m,offsets+q*8,offset) && std::abs(offset[0])<=320 && std::abs(offset[1])<=64)
                for(unsigned i=0;i<12;i+=3) {
                    if(moving)v[i]-=offset[0];
                    if(options_moving)v[i+1]-=offset[1];
                }
        }
        // Options rows slide horizontally; its footer stones slide vertically.
        // Remove only their live native offsets before applying the shared rise.
        if(animate && state==3 && atlas && q>=43 && q<72 &&
           (m.Read(Owner+0x3c)==1 || m.Read(Owner+0x3c)==3)) {
            std::array<float,2> offset{};
            if(TitleScreen::ReadFloats(m,offsets+q*8,offset) &&
               std::abs(offset[0])<=640 && std::abs(offset[1])<=128)
                for(unsigned i=0;i<12;i+=3) {
                    if(q<64)v[i]-=offset[0];else v[i+1]-=offset[1];
                }
        }
        const float min_x=std::min({v[0],v[3],v[6],v[9]}),max_x=std::max({v[0],v[3],v[6],v[9]});
        const float min_y=std::min({v[1],v[4],v[7],v[10]}),max_y=std::max({v[1],v[4],v[7],v[10]});
        // Native widgets are parked beyond the 320x240 viewport. Test their
        // original positions before editor transforms so hidden labels, panels
        // and cursor corners cannot reappear in the widescreen margins.
        const bool hidden=state==1 || min_x>=320 || max_x<=0 || min_y>=240 || max_y<=0 ||
            (selection && page==InputPage::Hidden) ||
            (labels && state==3) || (atlas && ((q>=43)!=(state==3)));
        if(hidden)for(unsigned i=3;i<12;i+=3) {
            v[i]=v[0];v[i+1]=v[1];v[i+2]=v[2];
        }
        else if(part)for(unsigned i=0;i<12;i+=3) {
            const auto p=MoveControl(v[i],v[i+1],*part,poses);v[i]=p[0];v[i+1]=p[1];
            if(RisingControl(*part) || (state==3 && (*part==OptionsItems || OptionsButton(*part))))
                v[i+1]+=animation.rise;
        }
        for(unsigned i=0;i<12;++i)m.Write(dst+q*48+i*4,std::bit_cast<U32>(v[i]));
    }
    return true;
}
template<class M> bool CopyColors(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    if(!animate || !Active(m))return false;
    const auto animation=CurrentAnimation(m);
    if(animation.buttons==1 && animation.prompt==1)return false;
    const bool atlas=m.Read(Owner)==std::optional<U32>{mesh};
    const bool labels=m.Read(Owner+0xc)==std::optional<U32>{mesh};
    bool prompt=false;
    for(U32 off:{0x1cu,0x20u}) {
        const U32 text=m.Read(Owner+off).value_or(0);
        if(!TitleScreen::Pointer(text,0x4c))continue;
        const auto kind=m.Read(text+0x40).value_or(0);
        const unsigned count=kind==2?9:kind==3?1:2;
        for(unsigned i=0;i<count;++i) {
            const U32 model=m.Read(text+0x10+4*i).value_or(0);
            if(TitleScreen::Pointer(model,8) && m.Read(model+4)==std::optional<U32>{mesh})prompt=true;
        }
    }
    if((!atlas && !labels && !prompt) || bytes==0 || bytes%64 || bytes>65536 ||
       (atlas && bytes!=78*64) || (labels && bytes!=8*64) ||
       !TitleScreen::Pointer(src,bytes) || !TitleScreen::Pointer(dst,bytes))return false;
    std::vector<U32> colors(bytes/4);
    for(unsigned i=0;i<colors.size();++i) {
        const auto value=m.Read(src+4*i);
        if(!value || !m.Read(dst+4*i))return false;
        colors[i]=*value;
    }
    for(unsigned q=0;q<bytes/64;++q) {
        const auto part=prompt?std::optional<Part>{Prompt}:AtlasPart(q,labels);
        const float opacity=part==Prompt?animation.prompt:
            (part && (RisingControl(*part) || (OptionsActive(m) &&
                (*part==OptionsItems || OptionsButton(*part)))))?animation.buttons:1;
        for(unsigned v=0;v<4;++v) {
            auto& color=colors[q*16+v*4+3];
            const float alpha=std::bit_cast<float>(color);
            if(!std::isfinite(alpha))return false;
            color=std::bit_cast<U32>(alpha*opacity);
        }
    }
    for(unsigned i=0;i<colors.size();++i)m.Write(dst+4*i,colors[i]);
    return true;
}
inline std::optional<std::array<float,2>> MapPointer(float u,float v,float width,const Layout& poses,
        InputPage page=InputPage::Save) {
    if(!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(width) || width<=0)return {};
    const float x=(u-.5f)*width/.8f+160,y=((v-.5f)*240-18)/.8f+120;
    if(page==InputPage::Hidden)return {};
    if(page==InputPage::Options) {
        // Test individually moved buttons first, then the three settings rows.
        const auto p=poses[OptionsItems];if(!Valid(p))return {};
        const auto a=Anchors[OptionsItems];
        for(Part part:{Cancel,Okay}) {
            const auto b=poses[part];if(!Valid(b))continue;
            const auto anchor=Anchors[part];
            const auto center=Move(anchor[0],anchor[1],OptionsItems,p);
            const float bx=center[0]+(x-center[0]-b.x/.8f)/b.scale;
            const float by=center[1]+(y-center[1]-b.y/.8f)/b.scale;
            const float nx=a[0]+(bx-a[0]-p.x/.8f)/p.scale;
            const float ny=a[1]+(by-a[1]-p.y/.8f)/p.scale;
            if(nx>=anchor[0]-48 && nx<=anchor[0]+48 && ny>=204 && ny<=236)
                return std::array{nx/320,ny/240};
        }
        const float nx=a[0]+(x-a[0]-p.x/.8f)/p.scale,ny=a[1]+(y-a[1]-p.y/.8f)/p.scale;
        if(nx>=0 && nx<320 && ny>=0 && ny<198)return std::array{nx/320,ny/240};
        return {};
    }
    for(Part part:{Back,Options,Save,DontSave}) {
        const auto p=poses[part];if(!Valid(p))continue;
        const auto a=Anchors[part];
        const float nx=a[0]+(x-a[0]-p.x/.8f)/p.scale,ny=a[1]+(y-a[1]-p.y/.8f)/p.scale;
        const float left=part==Back?4:part==Options?112:part==Save?164:39;
        const float right=left+(part==Back?52:part==Options?96:120);
        const float top=part==Back || part==Options?204:96;
        const float bottom=part==Back || part==Options?236:144;
        if(nx>=left && nx<=right && ny>=top && ny<=bottom)
            return std::array{nx/320,ny/240};
    }
    return {};
}
} // namespace OOT3D::SaveMenu
