// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/textbox_close.h"
#include "oot3d/pause_menu.h"

namespace OOT3D::TextboxIndicator {
using TitleScreen::U32;
constexpr U32 Message=ScreenModels::Message;
constexpr std::int64_t Fade=80000;
inline float Ease(float t) {t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
inline float Speed(float value) {return std::isfinite(value)?std::clamp(value,.25f,3.f):1.f;}
inline std::array<float,2> Spring(std::int64_t elapsed,const Textboxes::IndicatorAnimation& s) {
    if(!s.spring || elapsed<0)return {1,1};
    const float t=float(elapsed)*Speed(s.speed)/180000.f;
    const float small=std::isfinite(s.small)?std::clamp(s.small,.2f,1.f):.72f;
    const float big=std::isfinite(s.big)?std::clamp(s.big,1.f,1.8f):1.18f;
    if(t<.25f) {const float v=std::lerp(1.f,small,Ease(t/.25f));return {v,v};}
    if(t<.65f) {const float u=Ease((t-.25f)/.4f);return {std::lerp(small,1.f,u),std::lerp(small,big,u)};}
    return {1,std::lerp(big,1.f,Ease((t-.65f)/.35f))};
}
struct Animation {
    bool visible=false,initialized=false,pressed=false;
    std::int64_t changed=0,press_time=0,last=0;
    float from=0;
    float Alpha(std::int64_t now) const {
        return std::lerp(from,visible?1.f:0.f,Ease(float(now-changed)/Fade));
    }
    void Show(bool show,std::int64_t now,const Textboxes::IndicatorAnimation& s,bool closing=false) {
        if(initialized && now<last)*this={};
        last=now;initialized=true;
        // Page advances can finish the tactile response. Closing the textbox
        // starts fading immediately, even during a slow spring or a final draw.
        show=s.visible && !closing && (show || (s.spring && pressed && now-press_time<180000/Speed(s.speed)));
        if(show!=visible) {from=Alpha(now);visible=show;changed=now;}
    }
    void Press(std::int64_t now,const Textboxes::IndicatorAnimation& s) {
        if(s.visible && s.spring) {pressed=true;press_time=now;}
    }
    std::array<float,2> Scale(std::int64_t now,const Textboxes::IndicatorAnimation& s) const {
        return Spring(pressed?now-press_time:-1,s);
    }
};
// Calculate a view that draws the cached model pose through the current native
// matrix. Only the fresh stack view changes; hidden-pane updates cannot move
// the outgoing icon, and no model, material, visibility or message state is revived.
inline bool RetainedView(std::array<float,12>& out,const std::array<float,12>& view,
                         const std::array<float,12>& pose,const std::array<float,12>& current) {
    const float a=current[0],b=current[1],c=current[2],d=current[4],e=current[5],f=current[6],
        g=current[8],h=current[9],i=current[10];
    const float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(det) || std::abs(det)<1e-8f)return false;
    std::array<float,12> inverse{(e*i-f*h)/det,(c*h-b*i)/det,(b*f-c*e)/det,0,
        (f*g-d*i)/det,(a*i-c*g)/det,(c*d-a*f)/det,0,
        (d*h-e*g)/det,(b*g-a*h)/det,(a*e-b*d)/det,0};
    for(unsigned r=0;r<3;++r)for(unsigned k=0;k<3;++k)inverse[r*4+3]-=inverse[r*4+k]*current[k*4+3];
    const auto multiply=[](const auto& x,const auto& y) {
        std::array<float,12> z{};
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c) {
            z[r*4+c]=c==3?x[r*4+3]:0;
            for(unsigned k=0;k<3;++k)z[r*4+c]+=x[r*4+k]*y[k*4+c];
        }
        return z;
    };
    out=multiply(multiply(view,pose),inverse);
    return std::all_of(out.begin(),out.end(),[](float v){return std::isfinite(v);});
}
// Pane 2 is the advance icon; pane 3 is the choice arrow. Native choice
// positioning (0x48b8a8) and hiding (0x2c453c) use Message+0x44+0x530.
template<unsigned PaneIndex> struct BasicState {
    static_assert(PaneIndex==2 || PaneIndex==3);
    static constexpr U32 PaneSlot=Message+0x568+4*PaneIndex;
    static Textboxes::IndicatorAnimation Settings() {
        if constexpr(PaneIndex==3)return {true,false};
        return Textboxes::GetLayout().indicator_animation;
    }
    Animation animation;
    U32 pane=0,model=0,mesh=0,play=0,scene=0;
    unsigned generation=0;
    bool ready=false,seen=false,pending=false;
    std::int64_t captured=0,draw_time=0;
    float native_alpha=1;
    std::uint64_t geometry=0;
    std::array<float,12> view{},matrix{};
    std::array<U32,16> saved{};
    std::array<U32,64> saved_fp{};
    U32 saved_flags=0,saved_fpscr=0;
    template<class M> static U32 Model(M& m) {
        if(m.Read(Message)!=0x4ed0a0)return 0;
        const U32 p=m.Read(PaneSlot).value_or(0);
        if(!TitleScreen::Pointer(p,0xb4) || m.Read(p)!=0x4ed234 || m.Read(p+4)!=Message+0x168)return 0;
        const U32 model=m.Read(p+0x64).value_or(0);
        return TitleScreen::Pointer(model,0x1e4) && m.Read(model)==0x4ebd60?model:0;
    }
    template<class M> bool Owned(M& m) const {
        return model && Model(m)==model && m.Read(PaneSlot)==pane &&
            m.Read(model+4)==mesh && m.Read(Message+8)==play && m.Read(HUDFade::UI+0xc)==play &&
            m.Read(play+0x104)==scene && generation==Interpolation::generation.load();
    }
    template<class M> bool Identify(M& m,U32 candidate) {
        if(!candidate || Model(m)!=candidate)return false;
        if(!Owned(m)) {
            *this={};model=candidate;pane=m.Read(PaneSlot).value_or(0);
            mesh=m.Read(model+4).value_or(0);play=m.Read(Message+8).value_or(0);
            scene=m.Read(play+0x104).value_or(0);generation=Interpolation::generation.load();
        }
        return Owned(m);
    }
    template<class M> static std::optional<std::uint64_t> Geometry(M& m,U32 mesh) {
        const auto desc=m.Read(mesh).value_or(0),slot=m.Read(mesh+0x124).value_or(2);
        const auto count=m.Read(desc+0xc).value_or(0),flags=m.Read(desc+0x1c).value_or(0);
        if(slot>=2 || (count!=0 && count!=4) || !(flags&0x80) || (flags&0x18))return {};
        const U32 live=m.Read(mesh+0x1a0+slot*4).value_or(0);
        if(!TitleScreen::Pointer(live,80))return {};
        auto hash=HUD::HashStart;
        // Position/UV changes mean a different sprite; color changes may be
        // the native hide itself. Sampler identity guards resource reuse.
        for(unsigned off=0;off<80;off+=4) {const auto v=m.Read(live+off);if(!v)return {};hash=HUD::HashWord(hash,*v);}
        for(U32 off:{0x158u,0x15cu,0x164u}) {const auto v=m.Read(mesh+off);if(!v)return {};hash=HUD::HashWord(hash,*v);}
        return hash;
    }
    template<class M> static bool Closing(M& m) {
        const auto state=m.Read(Message+0xf38).value_or(0)&255;
        return state==0 || state==14 || state==15;
    }
    template<class M> void Press(M& m,std::int64_t now) {
        if constexpr(PaneIndex==3)return; // Choice selection never springs.
        // Accepted native page advances can occur while the indicator isn't
        // drawing. Keep the input response independent of its cached draw age;
        // Present still validates the cached geometry before retaining a draw.
        if(Identify(m,Model(m)) && (m.Read(play+0x18).value_or(0)&3))
            animation.Press(now,Settings());
    }
    template<class M> void Material(M& m,U32 candidate,U32 alpha,std::int64_t now,bool supported) {
        if(!supported || !Identify(m,candidate))return;
        const auto bits=m.Read(alpha);if(!bits)return;
        const float value=std::bit_cast<float>(*bits);if(!std::isfinite(value))return;
        const auto settings=Settings();
        if(!pending) {
            animation.Show(true,now,settings,Closing(m));
            native_alpha=std::clamp(value,0.f,1.f);
        }
        m.Write(alpha,std::bit_cast<U32>(settings.visible?native_alpha*animation.Alpha(pending?draw_time:now):0.f));
    }
    template<class M> void FollowClose(M& m,U32 target,std::int64_t now,const TextboxClose::State* box) const {
        const auto time=pending?draw_time:now;
        if(!box || !box->ready || !box->closing || !Closing(m) || !box->Owned(m) ||
           box->generation!=Interpolation::generation.load() || time<box->started ||
           time-box->started>=TextboxClose::Duration)return;
        const float scale=TextboxClose::Scale(time-box->started);
        auto scaled=box->view;
        for(unsigned row=0;row<3;++row) {
            const auto i=4*row;
            scaled[i+3]+=scaled[i]*box->matrix[3]*(1-scale)+scaled[i+1]*box->matrix[7]*(1-scale);
            scaled[i]*=scale;scaled[i+1]*=scale;
        }
        constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
        std::array<float,12> current{},relative{},result{};
        // Apply the plate's closing transform in view space, about its actual
        // displayed center. This also carries independently positioned/scaled
        // indicators and wooden-sign layouts with their parent textbox.
        if(!TitleScreen::ReadFloats(m,target,current) ||
           !RetainedView(relative,scaled,identity,box->view) ||
           !RetainedView(result,relative,current,identity))return;
        for(unsigned i=0;i<12;++i)m.Write(target+4*i,std::bit_cast<U32>(result[i]));
    }
    template<class M,class R> bool Transform(M& m,R& r,std::int64_t now,bool supported,
                                            const TextboxClose::State* closing_box=nullptr) {
        if(!supported || !Identify(m,r[0]))return false;
        r[2]=r[0]+12;
        const auto settings=Settings();
        if(!pending) {
            auto box=Textboxes::GetLayout();
            if(PauseMenu::Active(m)) {
                const auto p=PauseMenu::GetLayout()[PauseMenu::DungeonPage(m)?PauseMenu::DungeonDescription:PauseMenu::Description];
                box.x=p.x;box.y=p.y;box.scale=p.scale;box.indicator={};
            }
            if(const auto t=ScreenModels::Select(m,model,box,{}))ScreenModels::Apply(m,r[1],model,*t);
            const auto hash=Geometry(m,mesh);
            ready=hash && TitleScreen::ReadFloats(m,r[1],view) && TitleScreen::ReadFloats(m,model+12,matrix);
            seen=true;
            if(ready) {geometry=*hash;captured=now;}
        }
        const auto scale=animation.Scale(pending?draw_time:now,settings);
        if(pending) {
            std::array<float,12> adjusted=view,current{},retained{};
            // Add the spring about the cached icon's center before composing
            // the current model inverse. Base editor offsets stay fixed.
            for(unsigned row=0;row<3;++row) {
                adjusted[row*4+3]+=adjusted[row*4]*matrix[3]*(1-scale[0])+adjusted[row*4+1]*matrix[7]*(1-scale[1]);
                adjusted[row*4]*=scale[0];adjusted[row*4+1]*=scale[1];
            }
            if(!TitleScreen::ReadFloats(m,model+12,current) || !RetainedView(retained,adjusted,matrix,current))
                ScreenModels::Apply(m,r[1],model,{0,0,0,0});
            else for(unsigned i=0;i<12;++i)m.Write(r[1]+4*i,std::bit_cast<U32>(retained[i]));
        } else ScreenModels::Apply(m,r[1],model,{0,0,settings.visible?scale[0]:0,settings.visible?scale[1]:0});
        FollowClose(m,r[1],now,closing_box);
        return true;
    }
    template<class R,class F> void Resume(R& r,F& f,U32& flags,U32& fpscr) {
        std::copy(saved.begin(),saved.end(),r.begin());std::copy(saved_fp.begin(),saved_fp.end(),f.begin());
        flags=saved_flags;fpscr=saved_fpscr;pending=false;
    }
    template<class M,class R,class F> void Present(M& m,R& r,F& f,U32 flags,U32 fpscr,std::int64_t now,bool supported) {
        const auto settings=Settings();
        if(!supported || !Owned(m) || now<captured || !settings.visible) {ready=seen=false;animation={};return;}
        const bool native=seen;seen=false;
        animation.Show(native,now,settings,Closing(m));
        if(native || !ready || animation.Alpha(now)<=0)return;
        const auto state=m.Read(Message+0xf38).value_or(0)&255;
        if(state==1 || state==2 || Geometry(m,mesh)!=geometry) {ready=false;animation={};return;}
        // Save all CPU state just like the existing closing-plate draw; this
        // rendering tail never invokes message update or consumes input.
        std::copy_n(r.begin(),16,saved.begin());std::copy_n(f.begin(),64,saved_fp.begin());
        saved_flags=flags;saved_fpscr=fpscr;draw_time=now;pending=true;
        r[0]=model;r[14]=TextboxClose::PresentSite;r[15]=0x3f9680;
    }
};
using State=BasicState<2>;
using ChoiceState=BasicState<3>;

}
