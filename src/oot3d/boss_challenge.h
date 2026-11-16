// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/boss_challenge_layout.h"
#include "oot3d/file_background_assets.h"
#include "oot3d/guest_ui_mesh.h"
#include "oot3d/pause_prompts.h"
#include "oot3d/file_select.h"
#include "oot3d/shop_hint_assets.h"
#include "oot3d/boss_title_assets.h"
#include "oot3d/boss_border.h"
#include "oot3d/boss_rounding.h"
#include "oot3d/hud_fade.h"
#include "oot3d/hud_layout.h"
#include "oot3d/ammo_backdrop.h"
#include <unordered_map>

namespace OOT3D::BossChallenge {
constexpr U32 FirstSVC=0xfff280;
constexpr std::array<TitleScreen::Site,4> Sites{{
    {0x300538,0xeb04a745,0xe59602d4}, // Top challenge draw in the main UI pass.
    {0x2fd718,0xe1a00000,0xe92d41f0}, // Selected boss initialized; native tail call.
    {0x4298f4,0xe7901105,0xe59f0120}, // Initial navigation neighbor.
    {0x429958,0xe7901105,0xeaffffe7}, // Skip a locked neighbor.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)if(pc==Sites[i].pc && word==Sites[i].instruction)
        return (word&0xf0000000)|0x0f000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
        m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return m.Read(0x4414bc)==std::optional<U32>{0xe92d43f8} &&
        m.Read(0x448e28)==std::optional<U32>{Owner};
}
constexpr bool RoutedPane(unsigned pane) {
    return (pane>=3 && pane<=8) || pane==100 ||
        (pane>=10 && pane<=53 && (pane-10)%5<4) ||
        (pane>=85 && pane<=88) || (pane>=90 && pane<=93);
}
inline ScreenModels::Transform BottomTransform(unsigned pane,U32 selected,const Layout& l) {
    if((pane>=3 && pane<=8) || pane==100)return Transform(l[Prompt],160,24,40);
    if(pane>=10 && pane<=53)return StoneTransform((pane-10)/5,l);
    if(((pane>=85 && pane<=88) || (pane>=90 && pane<=93)) && selected<9)
        return StoneTransform(selected,l);
    return {40,0};
}
inline Layout touch_layout;
using BackgroundGeometry=PausePrompts::QuadGeometry<2>;
inline BackgroundGeometry BrownBackground(const Layout& l) {
    BackgroundGeometry g;
    for(unsigned panel=0;panel<2;++panel) {
        const auto p=l[Background+panel];
        if(!Valid(p) || !std::isfinite(l.background_darkness[panel]))continue;
        const float w=320*p.scale,h=240*p.scale;
        // The hidden challenge pane samples only a 160x120 fragment of the
        // shared atlas. Use the complete native brown backdrop, as file select
        // does, excluding the unused atlas padding. No screen capture is involved.
        g.Add(200+p.x-w/2,120+p.y-h/2,w,h,
            .5f/FileBackground::Width,255.5f/FileBackground::Height,
            319.5f/FileBackground::Width,16.5f/FileBackground::Height);
        const float brightness=1-std::clamp(l.background_darkness[panel],0.f,1.f);
        for(unsigned v=0;v<4;++v)for(unsigned c=0;c<3;++c)
            g.colors[(g.count-1)*16+v*4+c]=brightness;
    }
    return g;
}
using ShadowGeometry=PausePrompts::QuadGeometry<9>;
inline constexpr auto ShadowPixels=[] {std::array<U32,64> a{};a.fill(0x000000ff);return a;}();
inline ShadowGeometry PreviewShadow(const Layout& l) {
    ShadowGeometry g;
    const auto p=l[Preview];
    if(!Valid(p) || !std::isfinite(l.shadow_opacity) || !std::isfinite(l.shadow_softness))return g;
    const float alpha=std::clamp(l.shadow_opacity,0.f,1.f);
    if(alpha<=0)return g;
    // Native boss artwork (including its baked-in frame and record strip)
    // occupies 400x240. Apply the very same center/scale as the preview draw.
    const float w=400*p.scale,h=(240+(l.border_bottom?BossBorder::FooterHeight:0))*p.scale;
    const float x=200+p.x-w/2,y=120+p.y-120*p.scale;
    const float d=std::clamp(l.shadow_softness,0.f,40.f)*p.scale;
    const auto add=[&](float left,float top,float width,float height,std::array<float,4> a) {
        g.Add(left,top,width,height,.5f,.5f,.5f,.5f);
        for(unsigned v=0;v<4;++v)g.colors[(g.count-1)*16+v*4+3]=a[v]*alpha;
    };
    if(d>0) {
        add(x-d,y,d,h,{0,1,0,1});add(x+w,y,d,h,{1,0,1,0});
        add(x,y-d,w,d,{0,0,1,1});add(x,y+h,w,d,{1,1,0,0});
        add(x-d,y-d,d,d,{0,0,0,1});add(x+w,y-d,d,d,{0,0,1,0});
        add(x-d,y+h,d,d,{0,1,0,0});add(x+w,y+h,d,d,{1,0,0,0});
    }
    add(x,y,w,h,{1,1,1,1});
    return g;
}
// The gauntlet has no boss-intro artwork; retain its menu label only.
inline FileStatusText::Geometry NameGeometry(const Layout& l,unsigned selected,const FileStatusText::Assets& font) {
    const auto p=l[BossName];if(selected!=8 || !Valid(p))return {};
    constexpr std::string_view name="Boss Gauntlet";float width=0;
    for(auto c:FileStatusText::Codepoints(name))if(const auto glyph=font.Find(c))width+=(glyph->width+1)*p.scale;
    return FileStatusText::Text(font,name,p.x-width/2,p.y,width+1,p.scale);
}
using TitleGeometry=PausePrompts::QuadGeometry<1>;
inline TitleGeometry NativeNameGeometry(const Layout& l,unsigned selected) {
    TitleGeometry g;const auto p=l[BossName];if(selected>=8 || !Valid(p))return g;
    const float w=128*p.scale,h=32*p.scale;
    g.Add(p.x-w/2,p.y,w,h,.5f/BossTitle::Width,(BossTitle::Height-selected*64-.5f)/BossTitle::Height,
          255.5f/BossTitle::Width,(BossTitle::Height-selected*64-63.5f)/BossTitle::Height);
    return g;
}
using HintGeometry=PausePrompts::QuadGeometry<4>;
inline HintGeometry NavigationHints(const Layout& l,float bob) {
    HintGeometry g;
    namespace P=PausePrompts;
    if(!std::isfinite(bob))bob=0;
    for(unsigned side=0;side<2;++side) {
        const auto p=l[LeftStick+side*2];if(!Valid(p))continue;
        const float x=p.x-18*p.scale,y=p.y-8*p.scale+bob;
        // Same Circle Pad and orange arrow atlas used by pause navigation.
        g.Add(x+(side?0:20)*p.scale,y,16*p.scale,16*p.scale,
            128.5f/P::IconWidth,127.5f/P::IconHeight,143.5f/P::IconWidth,112.5f/P::IconHeight);
        const unsigned q=g.count;
        constexpr float u0=192.5f/P::IconWidth,u1=207.5f/P::IconWidth;
        constexpr float top=47.5f/P::IconHeight,bottom=32.5f/P::IconHeight;
        g.Add(x+(side?20:0)*p.scale,y,16*p.scale,16*p.scale,u0,top,u1,bottom);
        const std::array<float,8> uv=side?std::array{u1,top,u1,bottom,u0,top,u0,bottom}:
            std::array{u0,bottom,u0,top,u1,bottom,u1,top};
        std::copy(uv.begin(),uv.end(),g.uvs.begin()+q*8);
    }
    return g;
}
inline PausePrompts::Pair ButtonPrompts(const Layout& l,const PausePrompts::Assets& assets) {
    FileSelect::Layout shared;
    for(unsigned i=0;i<2;++i) {const auto p=l[SelectPrompt+i];shared.prompts[i]={p.x,p.y,p.scale};}
    return FileSelect::Prompts(shared,assets,1);
}
inline std::optional<std::array<float,2>> MapPointer(float u,float v,float width) {
    if(!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(width) || width<=0)return {};
    std::scoped_lock lock{mutex};const auto& l=touch_layout;
    if(!Valid(l[Stones]))return {};
    const float x=(u-.5f)*width+160,y=v*240;
    for(unsigned i=0;i<9;++i) {
        const auto c=StoneCenter(i,l),n=NativeStone(i);
        const float nx=n[0]+(x-c[0])/l[Stones].scale,ny=n[1]+(y-c[1])/l[Stones].scale;
        if(std::abs(nx-n[0])<49 && std::abs(ny-n[1])<23)return std::array{nx/320,ny/240};
    }
    return {};
}
struct Hook {
    enum class Stage {Idle,Background,Vignette,Particles,Shadow,TimerBackdrop,Native,Border,Panes,Hints,Icons,Text,Name,Fade};
    Stage stage=Stage::Idle;
    std::array<U32,16> saved{};
    std::array<U32,256> routed{};
    Layout frame_layout;
    unsigned next=0,pane=256,submitted=0;
    U32 selected=0xffffffff;
    GuestUI::Hook<FileSelect::BackgroundGeometry,FileBackground::Width,FileBackground::Height,FileBackground::Format> background;
    GuestUI::Hook<ShadowGeometry,8,8> shadow;
    using TimerBackdropGeometry=PausePrompts::QuadGeometry<32*8>;
    GuestUI::Hook<TimerBackdropGeometry,8,8> timer_backdrop;
    GuestUI::Hook<FileSelect::VignetteGeometry,8,8> vignette;
    GuestUI::Hook<FileParticles::Geometry,FileParticles::Width,FileParticles::Height> particles;
    GuestUI::Hook<HintGeometry,PausePrompts::IconWidth,PausePrompts::IconHeight,4> hints;
    PausePrompts::NavigationBob hint_animation;
    float hint_bob=0;
    GuestUI::Hook<PausePrompts::Geometry,PausePrompts::IconWidth,PausePrompts::IconHeight,4> icons;
    GuestUI::Hook<PausePrompts::TextGeometry,PausePrompts::TextWidth,PausePrompts::TextHeight> text;
    GuestUI::Hook<FileStatusText::Geometry,FileStatusText::Width,FileStatusText::Height> name;
    GuestUI::Hook<TitleGeometry,BossTitle::Width,BossTitle::Height> title;
    std::shared_ptr<const BossTitle::Pixels> title_assets;
    BossBorder::Hook border;
    BossRounding::Hook rounding;
    BossBorder::Rect preview_bounds;
    FileSelect::Layout backdrop_layout;
    FileParticles::Animation particle_animation;
    std::shared_ptr<const PausePrompts::Assets> prompt_assets;
    std::shared_ptr<const FileStatusText::Assets> name_assets;
    PausePrompts::TextGeometry lettering;
    U32 background_source=0;
    std::optional<U32> selection_return;
    bool active=false,battle=false,results=false,presenting=false;
    template<class M> static bool ResultsVisible(M& m) {
        // Native state9 prepares the results; state10 begins presenting them.
        // Stale cleared-title panes during the warp must not claim the timer.
        const auto state=m.Read(Owner+8).value_or(0)&255;
        if(state<10 || state>13)return false;
        // The title's animated layers do not own the result timer's lifetime.
        // The persistent Time label also establishes result presentation.
        for(unsigned index:{6u,8u,220u}) {
            const U32 p=m.Read(NativeLayout+0x418+4*index).value_or(0);
            if(TitleScreen::Pointer(p,0xb4) && m.Read(p+4)==NativeLayout+0x18 &&
               ((m.Read(p+0x6c).value_or(0)>>8)&255)!=0 &&
               std::int32_t(m.Read(p+0xb0).value_or(0))>=3)return true;
        }
        return false;
    }
    struct PaneSnapshot {U32 address=0,flags=0,state=0,group=0,group_flags=0;};
    float prompt_alpha=0;
    std::int64_t prompt_start=0;
    bool prompt_visible=false;
    static bool PromptPart(unsigned p) {return (p>=3 && p<=8) || p==100;}
    template<class M> static bool VisiblePane(M& m,U32 p) {
        if(!TitleScreen::Pointer(p,0xb4) || m.Read(p+4)!=NativeLayout+0x18 ||
           ((m.Read(p+0x6c).value_or(0)>>8)&255)==0 || m.Read(p+0xb0).value_or(0)<3)return false;
        const U32 resource=m.Read(p+0x78).value_or(256);
        const U32 group=resource<256?m.Read(NativeLayout+0x18+4*resource).value_or(0):0;
        return !TitleScreen::Pointer(group,0x394) || (m.Read(group+0x390).value_or(0)&255)!=0;
    }
    std::array<PaneSnapshot,512> selector_panes{};
    PaneSnapshot restore_pane;
    struct ModelSnapshot {std::array<U32,12> matrix{};U32 alpha=0;};
    std::unordered_map<U32,ModelSnapshot> selector_models,restore_models;
    template<class M> void RestoreSelection(M& m) {
        if(restore_pane.address) {
            m.Write(restore_pane.address+0x6c,restore_pane.flags);
            m.Write(restore_pane.address+0xb0,restore_pane.state);
            if(restore_pane.group)m.Write(restore_pane.group+0x390,restore_pane.group_flags);
            restore_pane={};
        }
        for(const auto& [model,s]:restore_models) {
            for(unsigned i=0;i<12;++i)m.Write(model+0xc+4*i,s.matrix[i]);
            m.Write(model+0xfc,s.alpha);
        }
        restore_models.clear();
    }
    template<class M> void SelectionPane(M& m,U32 address,unsigned index) {
        if(!presenting || index==250 || index>=512)return;
        PaneSnapshot current{address,m.Read(address+0x6c).value_or(0),m.Read(address+0xb0).value_or(0)};
        const U32 resource=m.Read(address+0x78).value_or(256);
        const U32 group=resource<256?m.Read(NativeLayout+0x18+4*resource).value_or(0):0;
        if(TitleScreen::Pointer(group,0x394)) {current.group=group;current.group_flags=m.Read(group+0x390).value_or(0);}
        auto& saved=selector_panes[index];
        if(!battle) {
            if(index>=256 && PromptPart(index-256) && prompt_visible) {
                restore_pane=current;m.Write(address+0x6c,current.flags|0x100);
            }
            saved=current;
            if(index>=256 && PromptPart(index-256) && prompt_visible)saved.flags|=0x100;
            return;
        }
        if(saved.address!=address)return;
        restore_pane=current;m.Write(address+0x6c,saved.flags);m.Write(address+0xb0,saved.state);
        if(saved.group && saved.group==current.group)m.Write(saved.group+0x390,saved.group_flags);
    }
    template<class M> void PromptModel(M& m,U32 model) {
        if(stage!=Stage::Panes || pane<3 || pane>8 || !presenting)return;
        const U32 p=routed[pane],foreground=m.Read(p+0x64).value_or(0);
        if(!foreground || (model!=foreground && m.Read(p+0x68)!=model))return;
        ModelSnapshot current;
        for(unsigned i=0;i<12;++i) {const auto v=m.Read(model+0xc+4*i);if(!v)return;current.matrix[i]=*v;}
        current.alpha=m.Read(model+0xfc).value_or(0);
        std::array<float,12> fg;
        if(!TitleScreen::ReadFloats(m,foreground+0xc,fg))return;
        // The foreground may already have drawn in this pane; use its original
        // pose for the shadow offset rather than the temporary normalized pose.
        if(const auto original=restore_models.find(foreground);original!=restore_models.end())
            for(unsigned i=0;i<12;++i)fg[i]=std::bit_cast<float>(original->second.matrix[i]);
        const float dx=std::bit_cast<float>(current.matrix[3])-fg[3];
        const float dy=std::bit_cast<float>(current.matrix[7])-fg[7];
        restore_models.try_emplace(model,current);
        // Each of the six atlas slices already contains its full-size geometry.
        // Remove only the animated XY model transform; retain native depth.
        const std::array<float,8> fixed{1,0,0,160+dx,0,1,0,24+dy};
        for(unsigned i=0;i<8;++i)m.Write(model+0xc+4*i,std::bit_cast<U32>(fixed[i]));
    }
    template<class M> void SelectionModel(M& m,U32 model) {
        if(!presenting || (stage!=Stage::Native && stage!=Stage::Panes) || pane==250 || TimerDraw())return;
        ModelSnapshot current;
        for(unsigned i=0;i<12;++i) {const auto v=m.Read(model+0xc+4*i);if(!v)return;current.matrix[i]=*v;}
        current.alpha=m.Read(model+0xfc).value_or(0);
        if(!battle) {selector_models[model]=current;return;}
        const auto found=selector_models.find(model);if(found==selector_models.end())return;
        restore_models.try_emplace(model,current);
        for(unsigned i=0;i<12;++i)m.Write(model+0xc+4*i,found->second.matrix[i]);
        m.Write(model+0xfc,found->second.alpha);
    }
    template<class M> void SelectionAlpha(M& m,U32 model,U32 alpha) const {
        if(stage==Stage::Panes && PromptPart(pane) && presenting) {
            m.Write(alpha,std::bit_cast<U32>(prompt_alpha));return;
        }
        if(!presenting || !battle || stage==Stage::Fade || TimerDraw())return;
        if(stage!=Stage::Native && stage!=Stage::Panes) {m.Write(alpha,std::bit_cast<U32>(1.f));return;}
        if(const auto found=selector_models.find(model);found!=selector_models.end())m.Write(alpha,found->second.alpha);
    }
    float timer_opacity=0;
    bool TimerDraw() const {return stage==Stage::Native && pane>=210 && pane<=217;}
    template<class M> static float GameplayTimerOpacity(M& m,bool in_battle,float hud) {
        const U32 play=m.Read(HUDFade::UI+0xc).value_or(0);
        if(!in_battle || !HUDFade::GameplayReady(m,play) || !std::isfinite(hud))return 0;
        return std::clamp(hud,0.f,1.f);
    }
    bool Drawing() const {return active && (timer_backdrop.Drawing() || border.Drawing() || background.Drawing() || shadow.Drawing() || vignette.Drawing() || particles.Drawing() ||
        hints.Drawing() || icons.Drawing() || text.Drawing() || name.Drawing() || title.Drawing() ||
        ((stage==Stage::Native || stage==Stage::Panes || stage==Stage::Fade) && pane<256));}
    template<class M> void PreviewMaterial(M& m,U32 rgb) const {
        // Tint the native preview itself, rather than relying on an overlay
        // quad's inherited alpha-test/material state. Uniforms are draw-local.
        if(!active || !presenting || results)return;
        if(stage==Stage::Border && border.Drawing()) {
            // One draw-local tint covers both quads. Do not depend on the
            // borrowed material's vertex-color combiner or inherited tint.
            for(unsigned c=0;c<3;++c)m.Write(rgb+c*4,std::bit_cast<U32>(BossBorder::Brightness(frame_layout)));
            return;
        }
        if(stage!=Stage::Native || pane>=210 || (pane>=6 && pane<=8))return;
        const float brightness=1-(std::isfinite(frame_layout.panel_darkness)?std::clamp(frame_layout.panel_darkness,0.f,1.f):0);
        for(unsigned c=0;c<3;++c)if(const auto value=m.Read(rgb+c*4)) {
            const float v=std::bit_cast<float>(*value);
            if(std::isfinite(v))m.Write(rgb+c*4,std::bit_cast<U32>(v*brightness));
        }
    }
    template<class M> void ObservePreview(M& m,U32 model) {
        if(!presenting || stage!=Stage::Native || pane!=1)return;
        const U32 mesh=m.Read(model+4).value_or(0),desc=m.Read(mesh).value_or(0);
        const unsigned count=m.Read(desc+0xc).value_or(0),current=m.Read(mesh+0x124).value_or(2);
        const U32 vertices=current<2?m.Read(mesh+0x1a0+4*current).value_or(0):0;
        if(count<4 || count>256 || !TitleScreen::Pointer(vertices,count*12))return;
        std::array<float,12> matrix{};if(!TitleScreen::ReadFloats(m,model+0xc,matrix))return;
        BossBorder::Rect rect{1e9f,1e9f,-1e9f,-1e9f};
        for(unsigned i=0;i<count;++i) {
            std::array<float,3> v{};if(!TitleScreen::ReadFloats(m,vertices+12*i,v))return;
            const float x=matrix[0]*v[0]+matrix[1]*v[1]+matrix[2]*v[2]+matrix[3];
            const float y=matrix[4]*v[0]+matrix[5]*v[1]+matrix[6]*v[2]+matrix[7];
            rect.left=std::min(rect.left,x);rect.right=std::max(rect.right,x);
            rect.top=std::min(rect.top,y);rect.bottom=std::max(rect.bottom,y);
        }
        if(rect.right-rect.left>=300 && rect.right-rect.left<=500 &&
           rect.bottom-rect.top>=180 && rect.bottom-rect.top<=300 &&
           std::abs(rect.left)<100 && std::abs(rect.top)<100)preview_bounds=rect;
    }
    struct SamplerRestore {U32 mesh=0,mag=0,min=0;};
    std::array<SamplerRestore,2> title_samplers{},draw_samplers{};
    std::array<U32,2> failed_models{};
    template<class M> void RestoreTitleFilter(M& m) {
        for(auto& sampler:draw_samplers)if(sampler.mesh) {
            m.Write(sampler.mesh+0x13c,sampler.mag);m.Write(sampler.mesh+0x140,sampler.min);sampler={};
        }
        for(auto& sampler:title_samplers)if(sampler.mesh) {
            m.Write(sampler.mesh+0x13c,sampler.mag);m.Write(sampler.mesh+0x140,sampler.min);sampler={};
        }
        failed_models={};
    }
    template<class M> void FilterFailedTitle(M& m,U32 native_pane) {
        for(unsigned i=0;i<2;++i) {
            const U32 model=m.Read(native_pane+0x64+i*4).value_or(0);
            if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=0x4ebd60)continue;
            failed_models[i]=model;
            const U32 mesh=m.Read(model+4).value_or(0);
            if(!TitleScreen::Pointer(mesh,0x19c) || (i && mesh==title_samplers[0].mesh))continue;
            const auto mag=m.Read(mesh+0x13c),min=m.Read(mesh+0x140);
            if(!mag || !min || *mag>1 || *min>5)continue;
            title_samplers[i]={mesh,*mag,*min};
            m.Write(mesh+0x13c,1);m.Write(mesh+0x140,3);
        }
    }
    template<class M> void FilterTitleAtDraw(M& m,U32 model) const {
        if(stage!=Stage::Native || pane!=7 ||
           (model!=failed_models[0] && model!=failed_models[1]))return;
        for(const auto& sampler:title_samplers)if(sampler.mesh) {
            m.Write(sampler.mesh+0x13c,1);m.Write(sampler.mesh+0x140,3);
        }
    }
    template<class M> void DrawMaterial(M& m,U32 renderer,U32 alpha) {
        if(active && stage==Stage::TimerBackdrop && timer_backdrop.Drawing() &&
           m.Read(renderer+0x354)==std::optional<U32>{timer_backdrop.model}) {
            // Geometry already contains the native timer fade envelope.
            m.Write(alpha,std::bit_cast<U32>(1.f));return;
        }
        if(!active || stage!=Stage::Native || (!(pane>=6 && pane<=8) && pane!=12 && pane!=220 && !TimerDraw()))return;
        const U32 native=m.Read(NativeLayout+0x418+4*pane).value_or(0);
        const U32 model=m.Read(renderer+0x354).value_or(0);
        if(!model || (m.Read(native+0x64)!=model && m.Read(native+0x68)!=model))return;
        // Material preparation precedes 3fbb30 -> 307e34, which emits the
        // actual sampler. ScreenModels runs later, after sampler submission.
        const U32 mesh=m.Read(renderer+0x350).value_or(0);
        if(TitleScreen::Pointer(mesh,0x19c)) {
            auto slot=draw_samplers.end();
            for(auto it=draw_samplers.begin();it!=draw_samplers.end();++it)
                if(it->mesh==mesh) {slot=it;break;}
            if(slot==draw_samplers.end())for(auto it=draw_samplers.begin();it!=draw_samplers.end();++it)
                if(!it->mesh) {slot=it;break;}
            const auto mag=m.Read(mesh+0x13c),min=m.Read(mesh+0x140);
            if(slot!=draw_samplers.end() && mag && min && *mag<=1 && *min<=5) {
                if(!slot->mesh)*slot={mesh,*mag,*min};
                m.Write(mesh+0x13c,1);m.Write(mesh+0x140,3);
            }
        }
        if(TimerDraw()) {
            const float a=std::bit_cast<float>(m.Read(alpha).value_or(0));
            m.Write(alpha,std::bit_cast<U32>(TimerAlpha(m,native,a)));
        }
    }
    template<class M> float TimerAlpha(M& m,U32 native,float alpha) const {
        if(!std::isfinite(alpha))return 0;
        if(!results)return std::clamp(alpha,0.f,1.f)*timer_opacity;
        const U32 label=m.Read(NativeLayout+0x418+220*4).value_or(0);
        float limit=VisiblePane(m,label)?1.f:0.f;
        if(m.Read(native+0xc).value_or(0)!=0) {
            const float frame=std::bit_cast<float>(m.Read(native+0x18).value_or(0));
            if(!std::isfinite(frame) || frame<=0)limit=0;
        }
        return std::clamp(alpha,0.f,limit);
    }

    template<class M> ScreenModels::Transform DrawTransform(M& m,U32 model) const {
        auto t=ModelTransform();
        if(!TimerDraw())return t;
        const U32 native=m.Read(NativeLayout+0x418+4*pane).value_or(0);
        const U32 foreground=m.Read(native+0x64).value_or(0);
        if(!foreground || (model!=foreground && m.Read(native+0x68)!=model))return t;
        std::array<float,12> matrix;std::array<float,2> base;
        if(!results && TitleScreen::ReadFloats(m,foreground+0xc,matrix) &&
           TitleScreen::ReadFloats(m,native+0x80,base)) {
            // Cancel the foreground's native slide for both layers, preserving
            // the shadow offset and the editor's fixed final position/scale.
            t.x+=(base[0]-matrix[3])*t.sx;
            t.y+=(base[1]-matrix[7])*t.sy;
        }
        const auto hd=HUD::GetEffectiveLayout();const auto p=hd.hd_timer_text;
        if(hd.hd_numbers && hd.hd_timer_numbers && TitleScreen::Valid({p.x,p.y,p.scale}) &&
           std::isfinite(hd.hd_timer_spacing) && hd.hd_timer_spacing>=-8 && hd.hd_timer_spacing<=24 &&
           (p.x!=0 || p.y!=0 || p.scale!=1 || hd.hd_timer_spacing!=0)) {
            float hx=14+frame_layout[Timer].x,hy=18+frame_layout[Timer].y;
            if(results) {
                const U32 first=m.Read(NativeLayout+0x418+210*4).value_or(0);
                const U32 model0=m.Read(first+0x64).value_or(0);
                if(!TitleScreen::ReadFloats(m,model0+0xc,matrix))return t;
                hx=matrix[3];hy=matrix[7];
            }
            // Compose after the existing gameplay/result placement. Both
            // foreground and native shadow receive the same local adjustment.
            const float tx=t.x+(t.anchored?t.ax:0)*(1-t.sx);
            const float ty=t.y+(t.anchored?t.ay:0)*(1-t.sy);
            t.x=hx+(tx-hx)*p.scale+p.x+(pane-210)*hd.hd_timer_spacing*p.scale*t.sx;
            t.y=hy+(ty-hy)*p.scale+p.y;
            t.sx*=p.scale;t.sy*=p.scale;t.ax=t.ay=0;t.anchored=true;
        }
        return t;
    }
    // Pane animations and model matrices are evaluated before the native pane
    // draw loop (2f1280 only checks visibility and submits the existing models).
    // Measure all eight current foregrounds, then draw one oval before any digit.
    template<class M> std::pair<TimerBackdropGeometry,U32> MakeTimerBackdrop(M& m) {
        TimerBackdropGeometry g;U32 source=0;
        const auto hd=HUD::GetEffectiveLayout();
        if(stage!=Stage::Native || !AmmoBackdrop::TimerEnabled(hd))return {g,0};
        const unsigned previous_pane=pane;
        float left=INFINITY,top=INFINITY,right=-INFINITY,bottom=-INFINITY,alpha=0;
        for(pane=210;pane<=217;++pane) {
            const U32 p=m.Read(NativeLayout+0x418+4*pane).value_or(0);
            if(!VisiblePane(m,p))continue;
            const U32 model=m.Read(p+0x64).value_or(0);
            if(!TitleScreen::Pointer(model,0x1e4))continue;
            const float a=TimerAlpha(m,p,std::bit_cast<float>(m.Read(model+0xfc).value_or(0)));
            if(a<=0)continue;
            const U32 mesh=m.Read(model+4).value_or(0),desc=m.Read(mesh).value_or(0);
            const unsigned count=m.Read(desc+0xc).value_or(0),current=m.Read(mesh+0x124).value_or(2);
            const U32 vertices=current<2?m.Read(mesh+0x1a0+4*current).value_or(0):0;
            std::array<float,12> matrix;
            if(count!=4 || !TitleScreen::Pointer(vertices,48) || !TitleScreen::ReadFloats(m,model+0xc,matrix))continue;
            const auto t=DrawTransform(m,model);
            const float ax=t.anchored?t.ax:matrix[3],ay=t.anchored?t.ay:matrix[7];
            std::array<std::array<float,3>,4> corners;
            bool valid=true;
            for(unsigned v=0;v<4;++v)valid &= TitleScreen::ReadFloats(m,vertices+12*v,corners[v]);
            if(!valid)continue;
            float l=INFINITY,u=INFINITY,r=-INFINITY,d=-INFINITY;
            for(const auto& v:corners) {
                const float x=(matrix[0]*v[0]+matrix[1]*v[1]+matrix[2]*v[2]+matrix[3])*t.sx+t.x+ax*(1-t.sx);
                const float y=(matrix[4]*v[0]+matrix[5]*v[1]+matrix[6]*v[2]+matrix[7])*t.sy+t.y+ay*(1-t.sy);
                l=std::min(l,x);r=std::max(r,x);u=std::min(u,y);d=std::max(d,y);
            }
            if(!(r>l && d>u) || !std::isfinite(l+r+u+d))continue;
            left=std::min(left,l);right=std::max(right,r);top=std::min(top,u);bottom=std::max(bottom,d);
            alpha=std::max(alpha,a);if(!source)source=model;
        }
        pane=previous_pane;
        if(!source)return {g,0};
        const auto& b=hd.hd_timer_backdrop;
        const auto clean=[](float x,float fallback,float lo,float hi) {
            return std::isfinite(x)?std::clamp(x,lo,hi):fallback;
        };
        const float cx=(left+right)*.5f+clean(b.x,0,-20,20),cy=(top+bottom)*.5f+clean(b.y,1,-20,20);
        const float rx=(right-left)*.5f*clean(b.width,1.25f,.25f,4),ry=(bottom-top)*.5f*clean(b.height,1.25f,.25f,4);
        const float softness=clean(b.softness,.75f,0,1),opacity=alpha*clean(b.opacity,0,0,1);
        for(unsigned ring=0;ring<8;++ring)for(unsigned angle=0;angle<32;++angle) {
            const unsigned q=g.count;g.Add(0,0,1,1,.5f,.5f,.5f,.5f);
            for(unsigned v=0;v<4;++v) {
                const float radius=(ring+(v%2))/8.f;
                const float theta=(angle+(v/2))*2*std::numbers::pi_v<float>/32;
                g.positions[q*12+v*3]=cx+rx*radius*std::cos(theta);
                g.positions[q*12+v*3+1]=cy+ry*radius*std::sin(theta);
                const float f=softness>0?std::clamp((1-radius)/softness,0.f,1.f):1;
                g.colors[q*16+v*4+3]=opacity*f*f*(3-2*f);
            }
        }
        return {g,source};
    }
    ScreenModels::Transform ModelTransform() const {
        if(stage!=Stage::Native && stage!=Stage::Panes && stage!=Stage::Fade)return {}; // Already in top-screen coordinates.
        if(pane==250 && Widescreen::Active())
            return {0,0,240*Widescreen::Aspect/400,1,200,120,true};
        // challenge_top.qly pane 7 uses resource 162: Challenge Failed.
        if(stage==Stage::Native && pane==7)return Transform(frame_layout[FailedTitle],200,102);
        if(stage==Stage::Native && (pane==6 || pane==8))return Transform(frame_layout[ClearedTitle],200,102);
        if(stage==Stage::Native && pane==12)return Transform(frame_layout[NewRecord],284,156);
        if(stage==Stage::Panes)return BottomTransform(pane,selected,frame_layout);
        // Native code reuses the timer digits beside the result's Time label.
        // Preserve their result position, animation and alpha independently of HUD.
        if(results)return {};
        if(pane>=210 && pane<=217)return Transform(frame_layout[Timer],14,18);
        // The preview remains visible briefly after InBattle flips. Keep its
        // editor pose until native code hides it instead of snapping to 1x.
        if((pane>=1 && pane<=5) || (presenting && pane!=250))return Transform(frame_layout[Preview],200,120);
        return {};
    }
    template<class M,class R> void Top(M& m,R& r,bool supported,float hud_opacity=1,std::int64_t now_us=0) {
        RestoreSelection(m);
        RestoreTitleFilter(m);
        if(stage==Stage::Idle) {
            active=supported && r[0]==Owner && Loaded(m);battle=InBattle(m);results=active && ResultsVisible(m);
            const U32 preview=m.Read(NativeLayout+0x418+4).value_or(0);
            // The native preview owns menu lifetime. A reset battle flag during
            // loading must not resurrect only the generated border/name/buttons.
            presenting=active && !results && VisiblePane(m,preview);
            const U32 prompt=m.Read(NativeLayout+0x818+3*4).value_or(0);
            const bool show=presenting && VisiblePane(m,prompt);
            if(!battle) {
                if(show && (!prompt_visible || now_us<prompt_start))prompt_start=now_us;
                prompt_visible=show;
                const float t=show?std::clamp(float(now_us-prompt_start)/150000.f,0.f,1.f):0;
                prompt_alpha=t*t*(3-2*t);
            }
            if(!presenting) {prompt_visible=false;prompt_alpha=0;}
            hint_bob=hint_animation.Advance(now_us,PausePrompts::GetLayout(),presenting);
            timer_opacity=GameplayTimerOpacity(m,battle,hud_opacity);
            preview_bounds={};frame_layout=GetLayout();backdrop_layout=FileSelect::GetLayout();pane=256;routed={};submitted=0;
            on_top=false;
            if(!active) {selector_models.clear();selector_panes={};r[14]=Sites[0].pc+4;r[15]=0x42a254;return;}
            saved=r;saved[14]=saved[15]=Sites[0].pc+4;
            if(!battle || !presenting)selected=m.Read(Owner+0x3e4).value_or(0xffffffff);
            next=0;stage=Stage::Native;background_source=0;
            // Draw the full native background before the preview and controls.
            // Borrow only the pane's material; leave its hidden state untouched.
            const U32 p=m.Read(NativeLayout+0x818+4).value_or(0);
            if(presenting && TitleScreen::Pointer(p,0xb4) && m.Read(p+4)==NativeLayout+0x18) {
                background_source=m.Read(p+0x64).value_or(0);
                if(const auto image=FileBackground::GetPixels()) {
                    r=saved;
                    background.Begin(m,r,Sites[0].pc,FileSelect::Backgrounds(backdrop_layout),*image,
                        background_source);
                    if(background.Pending()) {stage=Stage::Background;return;}
                }
                BeginVignette(m,r);if(stage!=Stage::Native)return;
            }
        }
        if(stage==Stage::Background) {
            background.Resume(m,r);if(background.Pending())return;
            BeginVignette(m,r);if(stage!=Stage::Native)return;
        }
        if(stage==Stage::Vignette) {
            vignette.Resume(m,r);if(vignette.Pending())return;
            BeginParticles(m,r);if(stage!=Stage::Native)return;
        }
        if(stage==Stage::Particles) {
            particles.Resume(m,r);if(particles.Pending())return;
            BeginShadow(m,r);if(stage==Stage::Shadow)return;
        }
        if(stage==Stage::Shadow) {
            shadow.Resume(m,r);if(shadow.Pending())return;
            stage=Stage::Native;
        }
        if(stage==Stage::TimerBackdrop) {
            timer_backdrop.Resume(m,r);if(timer_backdrop.Pending())return;
            stage=Stage::Native;
            // The original pane remains next, so no timer digit is skipped.
        }
        // After the background, use the native pane renderer in its original order, then draw the
        // relocated bottom controls. Defer the top fade until both are drawn.
        // This avoids adding a hook to every other menu's per-pane draw loop.
        if(stage==Stage::Hints || stage==Stage::Icons || stage==Stage::Text || stage==Stage::Name) {
            if(ResumeOverlays(m,r))return;
            BeginFade(m,r);if(pane==250)return;
        }
        if(stage==Stage::Border) {
            border.Resume(m,r);if(border.Pending())return;
            stage=Stage::Panes;next=0;
        }
        while(stage!=Stage::Fade) {
            while(next<256) {
                const unsigned index=next++;
                const bool bottom=stage==Stage::Panes;
                if(!bottom && index==210 && pane!=210) {
                    auto [geometry,source]=MakeTimerBackdrop(m);
                    if(source && geometry.count) {
                        pane=210;r=saved;
                        timer_backdrop.Begin(m,r,Sites[0].pc,geometry,ShadowPixels,source);
                        if(timer_backdrop.Pending()) {stage=Stage::TimerBackdrop;--next;return;}
                    }
                }
                if(bottom?!RoutedPane(index):index==250)continue;
                const U32 p=m.Read(NativeLayout+(bottom?0x818:0x418)+4*index).value_or(0);
                if(!TitleScreen::Pointer(p,0xb4) || m.Read(p+4)!=NativeLayout+0x18)continue;
                SelectionPane(m,p,index+(bottom?256:0));
                if(bottom) {routed[index]=p;++submitted;}
                if(!bottom && index==7)FilterFailedTitle(m,p);
                pane=index;r=saved;r[0]=p;r[14]=Sites[0].pc;r[15]=0x2f1280;return;
            }
            if(stage==Stage::Native) {
                if(presenting) {
                    stage=Stage::Border;r=saved;
                    border.BeginBorder(m,r,Sites[0].pc,frame_layout,background_source,preview_bounds);
                    if(border.Pending())return;
                }
                stage=Stage::Panes;next=0;
            }
            else {
                if(presenting && submitted && BeginOverlays(m,r))return;
                BeginFade(m,r);if(pane==250)return;
            }
        }
        {std::scoped_lock lock{mutex};touch_layout=frame_layout;}
        // Input belongs to the selector only, not the timer during gameplay.
        on_top=submitted!=0 && !battle;
        stage=Stage::Idle;pane=256;r=saved;
    }
    template<class M,class R> void BeginVignette(M& m,R& r) {
        r=saved;
        vignette.Begin(m,r,Sites[0].pc,FileSelect::CornerVignette(backdrop_layout.vignette,Widescreen::Active()),ShadowPixels,background_source);
        if(vignette.Pending()){stage=Stage::Vignette;return;}
        BeginParticles(m,r);
    }
    template<class M,class R> void BeginParticles(M& m,R& r) {
        r=saved;
        if(const auto image=FileParticles::GetPixels()) {
            particles.Begin(m,r,Sites[0].pc,particle_animation.Frame(backdrop_layout.particles,
                Widescreen::Active()?240*Widescreen::Aspect:400),*image,background_source);
            if(particles.Pending()){stage=Stage::Particles;return;}
        }
        BeginShadow(m,r);
    }
    template<class M,class R> void BeginFade(M& m,R& r) {
        stage=Stage::Fade;pane=256;
        const U32 p=m.Read(NativeLayout+0x418+4*250).value_or(0);
        if(TitleScreen::Pointer(p,0xb4) && m.Read(p+4)==NativeLayout+0x18) {
            pane=250;r=saved;r[0]=p;r[14]=Sites[0].pc;r[15]=0x2f1280;
        }
    }
    template<class M,class R> bool BeginOverlays(M& m,R& r) {
        prompt_assets=PausePrompts::GetAssets();name_assets=FileStatusText::GetAssets();
        title_assets=BossTitle::GetPixels();
        r=saved;stage=Stage::Hints;
        if(prompt_assets) {
            hints.Begin(m,r,Sites[0].pc,NavigationHints(frame_layout,hint_bob),prompt_assets->icons,background_source);
            if(hints.Pending())return true;
        }
        return ResumeOverlays(m,r);
    }
    template<class M,class R> bool ResumeOverlays(M& m,R& r) {
        if(stage==Stage::Hints) {
            hints.Resume(m,r);if(hints.Pending())return true;
            stage=Stage::Icons;r=saved;
            if(prompt_assets) {
                const auto g=ButtonPrompts(frame_layout,*prompt_assets);lettering=g.text;
                icons.Begin(m,r,Sites[0].pc,g.icons,prompt_assets->icons,background_source);
                if(icons.Pending())return true;
            }
        }
        if(stage==Stage::Icons) {
            icons.Resume(m,r);if(icons.Pending())return true;
            stage=Stage::Text;r=saved;
            if(prompt_assets) {
                text.Begin(m,r,Sites[0].pc,lettering,prompt_assets->text,background_source);
                if(text.Pending())return true;
            }
        }
        if(stage==Stage::Text) {
            text.Resume(m,r);if(text.Pending())return true;
            stage=Stage::Name;r=saved;
            if(selected<8 && title_assets) {
                title.Begin(m,r,Sites[0].pc,NativeNameGeometry(frame_layout,selected),*title_assets,background_source);
                if(title.Pending())return true;
            }
            if(selected==8 && name_assets) {
                name.Begin(m,r,Sites[0].pc,NameGeometry(frame_layout,selected,*name_assets),name_assets->pixels,background_source);
                if(name.Pending())return true;
            }
        }
        if(stage==Stage::Name) {
            if(selected<8 && title_assets) {title.Resume(m,r);if(title.Pending())return true;}
            if(selected==8 && name_assets) {name.Resume(m,r);if(name.Pending())return true;}
        }
        prompt_assets.reset();name_assets.reset();title_assets.reset();return false;
    }
    template<class M,class R> void BeginShadow(M& m,R& r) {
        r=saved;
        shadow.Begin(m,r,Sites[0].pc,PreviewShadow(frame_layout),ShadowPixels,background_source);
        stage=shadow.Pending()?Stage::Shadow:Stage::Native;
    }
    template<class M,class R> bool SkipBottom(M& m,const R& r) const {
        return active && Loaded(m) && r[5]==NativeLayout && r[4]<routed.size() &&
            r[0]!=0 && routed[r[4]]==r[0] && RoutedPane(r[4]);
    }
    template<class M,class R> void Input(M& m,R& r,unsigned site,bool supported) {
        if(site==1) {
            if(selection_return) {
                r[14]=*selection_return;selection_return.reset();
                r[0]=Owner;r[15]=0x4414bc;return;
            }
            // The original choice handler has already checked availability,
            // played its sound, selected the boss and seeded the gauntlet.
            // Tail-call the SAME native Battle handler used by confirmation.
            if(supported && r[0]==Owner && Loaded(m) &&
               (m.Read(Owner+8).value_or(0)&255)==5 && m.Read(Owner+0x3e8).value_or(9)<9) {
                // Finish the original tail call before entering Battle. Its
                // preview/selection bookkeeping must not be bypassed either.
                selection_return=r[14];r[14]=Sites[1].pc;r[15]=0x2fd71c;
            }
            return; // Displaced MOV r0,r0 has no side effects.
        }
        const U32 address=r[0]+4*r[5];
        const auto native=m.Read(address);r[1]=native.value_or(0xffffffff);
        if(supported && r[4]==Owner && Loaded(m) && r[0]>=r[8] &&
           (r[0]-r[8])%16==0 && (r[0]-r[8])/16<9 && r[5]<4) {
            const auto l=GetLayout();
            r[1]=Neighbor((r[0]-r[8])/16,r[5],l.two_columns,l.horizontal);
        }
    }
};
}
