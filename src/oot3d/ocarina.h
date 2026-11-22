// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/ocarina_layout.h"
#include "oot3d/ocarina_input.h"
#include "oot3d/pause_prompts.h"
#include "oot3d/screen_model_hooks.h"
#include "oot3d/widescreen.h"
#include "oot3d/ocarina_slide.h"
#include "oot3d/atlas_seams.h"

namespace OOT3D::Ocarina {
using Geometry=PausePrompts::QuadGeometry<QuadCount>;
inline bool VisibleStone(const Geometry& g,unsigned q) {
        float left=INFINITY,right=-INFINITY,top=INFINITY,bottom=-INFINITY;
        float umin=INFINITY,umax=-INFINITY,vmin=INFINITY,vmax=-INFINITY,alpha=0;
        for(unsigned v=0;v<4;++v) {
            left=std::min(left,g.positions[q*12+v*3]);right=std::max(right,g.positions[q*12+v*3]);
            top=std::min(top,g.positions[q*12+v*3+1]);bottom=std::max(bottom,g.positions[q*12+v*3+1]);
            umin=std::min(umin,g.uvs[q*8+v*2]);umax=std::max(umax,g.uvs[q*8+v*2]);
            vmin=std::min(vmin,g.uvs[q*8+v*2+1]);vmax=std::max(vmax,g.uvs[q*8+v*2+1]);
            alpha=std::max(alpha,g.colors[q*16+v*4+3]);
        }
    return alpha>0 && right>left && bottom>top && umax>umin && vmax>vmin;
}
inline unsigned QuadPart(unsigned q) {
    if((q>=3 && q<=14) || (q>=18 && q<=29))return Stones;
    if(q<=2 || (q>=39 && q<=46))return Notes;
    if(q>=35 && q<=37)return Name;
    return Count;
}
inline unsigned StoneIndex(unsigned q) {
    const unsigned n=q>=18?q-18:q-3;
    return n/3+4*(n%3); // Native quads are column-major, selection is row-major.
}
inline void PromptUVs(Geometry& g) {
    for(unsigned q=39;q<=46;++q)
        AtlasSeams::SongNoteUVs(std::span<float,8>{g.uvs.data()+q*8,8});
    for(unsigned q:{0u,1u,2u,35u,36u,37u})for(unsigned c=0;c<8;++c) {
        auto& uv=g.uvs[q*8+c];
        uv=std::bit_cast<float>(AtlasSeams::SongPanelWord(q,c,std::bit_cast<U32>(uv)));
    }
}
inline ScreenModels::Transform Transform(const Layout& l,unsigned part,unsigned stone=0) {
    if(part>=Count || !PausePrompts::Valid(l[part]))return {0,0,0,0,0,0,true};
    auto p=l[part];
    float ax=160,ay=part==Notes?36.f:220.f,tx=200,ty=ay;
    if(part==Stones) {
        if(stone>=12)return {0,0,0,0,0,0,true};
        ax=52+72*(stone%4);ay=90+44*(stone/4);
        const unsigned columns=l.Columns(),rows=12/columns;
        tx=200+(float(stone%columns)-float(columns-1)/2)*(72+l.column_spacing)*p.scale;
        ty=134+(float(stone/columns)-float(rows-1)/2)*(44+l.row_spacing)*p.scale;
        p.scale*=l.stone_size;
    }
    return {tx-ax+p.x,ty-ay+p.y,p.scale,p.scale,ax,ay,true};
}
inline std::optional<std::array<float,2>> MapPointer(float u,float v,float width,const Layout& l) {
    const float x=u*width-(width-400)/2-slide_x.load(),y=v*240;
    for(unsigned i=0;i<12;++i) {
        const auto t=Transform(l,Stones,i);if(t.sx<=0)continue;
        const float nx=t.ax+(x-t.x-t.ax)/t.sx,ny=t.ay+(y-t.y-t.ay)/t.sy;
        if(std::abs(nx-t.ax)<34 && std::abs(ny-t.ay)<20)
            return std::array{nx/320,ny/240};
    }
    return {};
}
template<class M> bool HasAtlas(M& m) {
    const auto model=m.Read(Owner+4).value_or(0),helper=m.Read(Owner+8).value_or(0);
    return TitleScreen::Pointer(model,0x1e4) && m.Read(model)==std::optional<U32>{0x4ebd60} &&
        m.Read(model+4)==m.Read(Owner) && m.Read(helper)==std::optional<U32>{QuadCount};
}
template<class M> Geometry Atlas(M& m,const Layout& l,bool normalize=false) {
    Geometry out;if((!List(State(m)) && State(m)!=13) || !HasAtlas(m))return out;
    const auto helper=m.Read(Owner+8).value_or(0);
    const U32 positions=m.Read(helper+0x10).value_or(0),uvs=m.Read(helper+0x14).value_or(0),
        colors=m.Read(helper+0x18).value_or(0);
    if(!TitleScreen::Pointer(positions,sizeof(out.positions)) ||
       !TitleScreen::Pointer(uvs,sizeof(out.uvs)) ||
       !TitleScreen::Pointer(colors,sizeof(out.colors)))return {};
    for(unsigned q=0;q<QuadCount;++q) {
        std::array<float,12> p{};std::array<float,8> uv{};std::array<float,16> c{};
        if(!TitleScreen::ReadFloats(m,positions+q*48,p) ||
           !TitleScreen::ReadFloats(m,uvs+q*32,uv) ||
           !TitleScreen::ReadFloats(m,colors+q*64,c)) {
            // Native 0x2f67d4 initializes only the sizes of unused note quads.
            // Their position/UV stack entries can contain NaNs or huge floats;
            // rejecting the entire atlas makes the backdrop, staff and stones
            // blink when selection refreshes that stack. Keep just those notes
            // as zeroed, transparent quads; never reuse the previous song.
            if(q>=39 && q<=46)continue;
            return {};
        }
        std::copy(p.begin(),p.end(),out.positions.begin()+q*12);
        std::copy(uv.begin(),uv.end(),out.uvs.begin()+q*8);
        std::copy(c.begin(),c.end(),out.colors.begin()+q*16);
    }
    PromptUVs(out);
    for(unsigned q=0;q<QuadCount;++q) {
        const unsigned part=QuadPart(q);
        const auto t=Transform(l,part,part==Stones?StoneIndex(q):0);
        // The native entrance comes from the right and squeezes the staff.
        // Restore the cartridge rectangles before applying our shared left slide.
        // Keep native UVs/colors, including learned-song visibility and tint.
        if(normalize && (part==Stones || q<=2 || part==Name)) {
            std::array<float,2> xy{},size{};
            if(!TitleScreen::ReadFloats(m,Owner+0x68+q*8,xy) ||
               !TitleScreen::ReadFloats(m,Owner+0x3c8+q*8,size))return {};
            for(unsigned v=0;v<4;++v) {
                out.positions[q*12+v*3]=xy[0]+(v%2)*size[0];
                out.positions[q*12+v*3+1]=xy[1]+(v/2)*size[1];
            }
        }
        const float x=out.positions[q*12],y=out.positions[q*12+1];
        // Native inactive sprites are parked outside the 320x240 source.
        // Do not bring them back into view through scaling/repositioning.
        float left=INFINITY,right=-INFINITY,top=INFINITY,bottom=-INFINITY;
        for(unsigned v=0;v<4;++v) {
            const auto i=q*12+v*3;
            left=std::min(left,out.positions[i]);right=std::max(right,out.positions[i]);
            top=std::min(top,out.positions[i+1]);bottom=std::max(bottom,out.positions[i+1]);
        }
        const bool hide=part==Count || right<=0 || left>=320 || bottom<=0 || top>=240;
        for(unsigned v=0;v<4;++v) {
            const auto i=q*12+v*3;
            if(hide) {out.positions[i]=x;out.positions[i+1]=y;out.colors[q*16+v*4+3]=0;}
            else {
                out.positions[i]=t.ax+(out.positions[i]-t.ax)*t.sx+t.x;
                out.positions[i+1]=t.ay+(out.positions[i+1]-t.ay)*t.sy+t.y;
            }
        }
    }
    // Each learned icon (18..29) sits on a larger dark placeholder (3..14).
    // Keep placeholders for missing songs, but do not let their exposed rim
    // outline the learned artwork. Only alter this relocated draw's alpha.
    for(unsigned q=18;q<=29;++q)if(VisibleStone(out,q))
        for(unsigned v=0;v<4;++v)out.colors[(q-15)*16+v*4+3]=0;
    out.count=QuadCount;return out;
}
// One clock and one offset for the backdrop, atlas and native cursor/text.
struct Slide {
    std::optional<std::int64_t> time;
    float progress=0;
    OcarinaSlide::Interval interval;
    std::int64_t previous_time{};
    float Advance(std::int64_t now,bool visible) {
        if(!time || now<*time) {time=now;previous_time=now;interval={};progress=0;return 0;}
        previous_time=*time;
        const float dt=std::clamp(float(now-*time)/180000.f,0.f,1.f);time=now;
        interval={progress,progress+(visible?dt:-dt)};
        progress=std::clamp(interval.end,0.f,1.f);
        return 1-std::pow(1-progress,3.f);
    }
};
inline bool Opening(U32 state) {return state==1 || state==13 || state==14;}
inline bool Closing(U32 state) {return state==2 || state==10 || state==11;}
using DrawGeometry=PausePrompts::QuadGeometry<DrawQuadCount>;
inline float LeftEdge() {return (400-(Widescreen::Active()?240*Widescreen::Aspect:400))/2;}
inline float Travel(const Geometry& g,const Layout& l,float left) {
    float right=l.panel.right;
    for(unsigned q=0;q<g.count;++q)for(unsigned v=0;v<4;++v)
        if(g.colors[q*16+v*4+3]>0)right=std::max(right,g.positions[q*12+v*3]);
    return std::max(1.f,right-left+8);
}
inline DrawGeometry WithPanel(const Geometry& g,const Layout& l,float slide,float left) {
    DrawGeometry out;if(!g.count)return out;
    // Sample the clean middle row of the native right cap. Its black fill is
    // already translucent; the last five texels contain the pale right border.
    // Use atlas texel coordinates, independent of the staff's edge insets.
    constexpr float u=104.f/512,v=1-192.f/256,du=1.f/512,dv=-1.f/256;
    const float row=v+24.5f*dv,edge=std::clamp(l.panel.right,left+1,800.f);
    const float border=std::clamp(l.panel.border,0.f,32.f);
    out.Add(left+slide,0,std::max(0.f,edge-left-border),240,u+20.5f*du,row,u+20.5f*du,row);
    out.Add(edge-border+slide,0,border,240,u+26.5f*du,row,u+31.5f*du,row);
    for(unsigned i=3;i<32;i+=4)out.colors[i]=std::clamp(l.panel.opacity,0.f,1.f);
    // Reuse each icon's alpha silhouette for a soft downward shadow, drawn
    // behind all artwork. Offsets scale with the stones and share their slide.
    // Fixed slots keep the interpolation mesh stable as songs become visible.
    constexpr std::array<std::array<float,2>,ShadowLayers> offsets{{
        {0,3},{-1,3},{1,3},{0,2},{0,4}}};
    const float scale=Transform(l,Stones).sx;
    for(unsigned q=18;q<=29;++q)for(unsigned tap=0;tap<ShadowLayers;++tap) {
        const unsigned dest=out.count++;
        const bool visible=VisibleStone(g,q);
        std::copy_n(g.uvs.begin()+q*8,8,out.uvs.begin()+dest*8);
        for(unsigned v=0;v<4;++v) {
            out.positions[dest*12+v*3]=g.positions[q*12+v*3]+slide+offsets[tap][0]*scale;
            out.positions[dest*12+v*3+1]=g.positions[q*12+v*3+1]+offsets[tap][1]*scale;
            out.positions[dest*12+v*3+2]=g.positions[q*12+v*3+2];
            // RGB stays black; texture alpha supplies the rounded silhouette.
            out.colors[dest*16+v*4+3]=visible?g.colors[q*16+v*4+3]*(tap==0?.18f:.10f):0;
        }
    }
    std::copy(g.positions.begin(),g.positions.end(),out.positions.begin()+AtlasDrawOffset*12);
    std::copy(g.uvs.begin(),g.uvs.end(),out.uvs.begin()+AtlasDrawOffset*8);
    std::copy(g.colors.begin(),g.colors.end(),out.colors.begin()+AtlasDrawOffset*16);
    for(unsigned i=AtlasDrawOffset*12;i<out.positions.size();i+=3)out.positions[i]+=slide;
    out.count=g.count+AtlasDrawOffset;return out;
}
inline float BottomTravel(const Geometry& g) {
    float top=240;
    for(unsigned q=0;q<g.count;++q)if(QuadPart(q)==Notes || QuadPart(q)==Name)
        for(unsigned v=0;v<4;++v)if(g.colors[q*16+v*4+3]>0)
            top=std::min(top,g.positions[q*12+v*3+1]);
    return std::max(0.f,241-top);
}
inline DrawGeometry SplitPanel(const Geometry& g,const Layout& l,float x,float y,float left,bool bottom) {
    auto out=WithPanel(g,l,bottom?0:x,left);
    for(unsigned q=0;q<out.count;++q) {
        const auto part=q>=AtlasDrawOffset?QuadPart(q-AtlasDrawOffset):Count;
        const bool panel=part==Notes || part==Name;
        for(unsigned v=0;v<4;++v) {
            if(panel!=bottom)out.colors[q*16+v*4+3]=0;
            else if(bottom)out.positions[q*12+v*3+1]+=y;
        }
    }
    return out;
}
struct Fade {
    std::optional<std::int64_t> time;
    float alpha=0;
    float Advance(std::int64_t now,bool visible) {
        if(!time || now<*time) {time=now;alpha=0;return alpha;}
        const float dt=std::clamp(float(now-*time)/150000.f,0.f,1.f);time=now;
        alpha=std::clamp(alpha+(visible?dt:-dt),0.f,1.f);return alpha;
    }
};
inline PausePrompts::Pair Prompts(const Layout& l,const PausePrompts::Assets& assets,
                                  bool list,float alpha,bool songs_allowed=true) {
    PausePrompts::Pair out;
    for(unsigned i=0;i<2;++i) {
        if(!i && !songs_allowed)continue;
        const auto p=l[i?BackPrompt:SongsPrompt];if(!PausePrompts::Valid(p))continue;
        const unsigned ib=out.icons.count,tb=out.text.count;
        const float s=p.scale;
        const unsigned row=i?(list?PausePrompts::FileBackText:PausePrompts::CancelText):PausePrompts::SongsText;
        const auto edge=PausePrompts::EdgePrompt(p.x,s,PausePrompts::LabelWidth(assets,row),
            PausePrompts::LabelColumn(assets,{PausePrompts::SongsText,PausePrompts::FileBackText,PausePrompts::CancelText}));
        if(i)out.icons.Add(edge.button,p.y,16*s,16*s,144.5f/256,95.5f/128,159.5f/256,80.5f/128);
        else out.icons.Add(edge.button,p.y,16*s,16*s,224.5f/256,95.5f/128,239.5f/256,80.5f/128);
        for(unsigned j=0;j<assets.glyphs[row].size();++j) {
            const auto glyph=assets.glyphs[row][j];if(!glyph.width)continue;
            const float start=glyph.x-1,shift=j*(1.f-assets.native_spacing);
            out.text.Add(edge.text+(start+shift)*.75f*s,p.y+.5f*s,
                (glyph.width+2)*.75f*s,15*s,start/PausePrompts::TextWidth,
                (row*PausePrompts::TextRowHeight+.5f)/PausePrompts::TextHeight,
                (start+glyph.width+2)/PausePrompts::TextWidth,(row*PausePrompts::TextRowHeight+19.5f)/PausePrompts::TextHeight);
        }
        PausePrompts::KeepOnScreen(out,ib,tb);
    }
    const auto fade=[&](auto& g) {for(unsigned i=3;i<g.count*16;i+=4)g.colors[i]=std::clamp(alpha,0.f,1.f);};
    fade(out.icons);fade(out.text);return out;
}
using VignetteGeometry=PausePrompts::QuadGeometry<12*12>;
inline constexpr auto VignettePixels=[] {std::array<U32,64> p{};p.fill(0x000000ff);return p;}();
inline VignetteGeometry PromptCorner(const PromptVignette& v,float alpha,float screen_width) {
    VignetteGeometry g;
    if(!v.enabled || !std::isfinite(alpha) || alpha<=0 || !std::isfinite(screen_width) || screen_width<=0 ||
       !std::isfinite(v.strength) || !std::isfinite(v.width) || !std::isfinite(v.height) ||
       !std::isfinite(v.softness) || !std::isfinite(v.x) || !std::isfinite(v.y))return g;
    const float strength=std::clamp(v.strength,0.f,1.f)*std::clamp(alpha,0.f,1.f);
    if(strength<=0)return g;
    const float w=std::clamp(v.width,8.f,600.f),h=std::clamp(v.height,8.f,240.f);
    const float soft=std::clamp(v.softness,.05f,1.f);
    const float right=200+screen_width/2+std::clamp(v.x,-400.f,400.f),bottom=240+std::clamp(v.y,-240.f,240.f);
    for(unsigned y=0;y<12;++y)for(unsigned x=0;x<12;++x) {
        g.Add(right-w+x*w/12,bottom-h+y*h/12,w/12,h/12,.5f,.5f,.5f,.5f);
        for(unsigned k=0;k<4;++k) {
            const float nx=1-(x+(k&1))/12.f,ny=1-(y+(k>>1))/12.f;
            const float t=std::clamp((std::hypot(nx,ny)-(1-soft))/soft,0.f,1.f);
            g.colors[(g.count-1)*16+k*4+3]=strength*(1-t*t*(3-2*t));
        }
    }
    return g;
}
struct Hook {
    GuestUI::Hook<DrawGeometry,1,1,0,true> atlas,bottom_atlas;
    GuestUI::Hook<PausePrompts::Geometry,PausePrompts::IconWidth,PausePrompts::IconHeight,4> icons;
    GuestUI::Hook<PausePrompts::TextGeometry,PausePrompts::TextWidth,PausePrompts::TextHeight> text;
    GuestUI::Hook<VignetteGeometry,8,8> vignette;
    VignetteGeometry corner;
    enum class Stage {Idle,Atlas,BottomAtlas,Native,Vignette,Icons,Text};
    Stage stage=Stage::Idle;
    std::array<U32,16> saved{};
    std::shared_ptr<const PausePrompts::Assets> assets;
    PausePrompts::Pair prompts;
    Layout frame_layout;
    U32 entry=0,source=0;
    Fade fade;
    Slide slide;
    OcarinaSlide::History slide_history,bottom_history;
    float midpoint_shift=0,bottom_midpoint=0,bottom_offset=0;
    Geometry last_list;
    U32 last_source=0;
    bool draw_native=false;
    bool song_completed=false;
    float offset=0;
    float cover=1;
    bool Pending() const {return stage!=Stage::Idle;}
    bool Drawing() const {return atlas.Drawing() || bottom_atlas.Drawing() || stage==Stage::Native || vignette.Drawing() || icons.Drawing() || text.Drawing();}
    bool NativeDrawing() const {return stage==Stage::Native;}
    template<class M> void CaptureSlide(M& m,U32 model,U32 view) const {
        const auto cursor=m.Read(Owner+0xc).value_or(0);
        const bool title=NativeDrawing() && (!cursor || m.Read(cursor+4)!=std::optional<U32>{model});
        OcarinaSlide::CursorPath path;
        if(NativeDrawing() && !title && input.move_count && slide.time &&
           *slide.time>slide.previous_time && *slide.time-slide.previous_time<=50000 &&
           m.Read(Owner+0x28)==0) {
            const unsigned selected=input.moves[input.move_count-1].to;
            if(m.Read(Owner+0x18)==selected%4 && m.Read(Owner+0x1c)==selected/4) {
                const auto next=Transform(frame_layout,Stones,selected);
                for(unsigned i=0;i<input.move_count;++i) {
                    const auto& move=input.moves[i];
                    if(move.time<=slide.previous_time || move.time>*slide.time)continue;
                    const auto old=Transform(frame_layout,Stones,move.from);
                    path.steps[path.count++]={old.ax+old.x-next.ax-next.x,old.ay+old.y-next.ay-next.y,
                        float(move.time-slide.previous_time)/float(*slide.time-slide.previous_time)};
                }
            }
        }
        if(frame_layout.bottom_panels && (bottom_atlas.Drawing() || title))
            OcarinaSlide::Capture(m,model,view,0,bottom_midpoint,slide.interval,{},title);
        else if(atlas.Drawing() || NativeDrawing())OcarinaSlide::Capture(m,model,view,midpoint_shift,0,slide.interval,path,title);
    }
    template<class M> ScreenModels::Transform ModelTransform(M& m,U32 model) const {
        if(!NativeDrawing())return {};
        const auto cursor=m.Read(Owner+0xc).value_or(0);
        if(cursor && m.Read(cursor+4)==std::optional<U32>{model}) {
            const auto x=m.Read(Owner+0x18).value_or(4),y=m.Read(Owner+0x1c).value_or(3);
            if(x>=4 || y>=3 || m.Read(Owner+0x28)!=std::optional<U32>{0})return {0,0,0,0,0,0,true};
            // Native code parks the cursor at (400,400) until its first list
            // update. Scaling that parked geometry can bring it onto the top
            // screen, so reject it before applying the user's layout.
            const U32 helper=m.Read(cursor+8).value_or(0);
            std::array<float,12> first{};
            if(!TitleScreen::ReadFloats(m,m.Read(helper+0x10).value_or(0),first) ||
               first[0]>=320 || first[1]>=240 || first[0]<-32 || first[1]<-32)
                return {0,0,0,0,0,0,true};
            auto t=Transform(frame_layout,Stones,x+4*y);t.x+=offset;return t;
        }
        auto t=Transform(frame_layout,Name);if(frame_layout.bottom_panels)t.y+=bottom_offset;else t.x+=offset;return t;
    }
    template<class M,class R> void BeginPrompts(M& m,R& r) {
        if(assets && prompts.icons.count) {
            vignette.Begin(m,r,entry,corner,VignettePixels,source);
            if(vignette.Pending()){stage=Stage::Vignette;return;}
        }
        BeginIcons(m,r);
    }
    template<class M,class R> void BeginIcons(M& m,R& r) {
        if(assets && prompts.icons.count) {
            icons.Begin(m,r,entry,prompts.icons,assets->icons,source);
            if(icons.Pending()){stage=Stage::Icons;return;}
        }
        assets.reset();stage=Stage::Idle;
    }
    template<class M,class R> void Resume(M& m,R& r) {
        if(stage==Stage::Atlas || stage==Stage::BottomAtlas) {
            if(stage==Stage::Atlas) {
                atlas.Resume(m,r);if(atlas.Pending())return;
                if(frame_layout.bottom_panels) {
                    bottom_atlas.Begin(m,r,entry,SplitPanel(last_list,frame_layout,offset,bottom_offset,LeftEdge(),true),{},source);
                    if(bottom_atlas.Pending()){stage=Stage::BottomAtlas;return;}
                }
            } else {bottom_atlas.Resume(m,r);if(bottom_atlas.Pending())return;}
            if(atlas.Owned(m))on_top.store(true);
            if(atlas.Owned(m) && draw_native) {
                saved=r;stage=Stage::Native;
                if(BackTitle(m)) {
                    // Native list drawing suppresses text during its exit and
                    // would also submit the parked cursor. Draw only the still
                    // owned title, with the same transform and slide capture.
                    r[0]=back_title.load();r[14]=entry;r[15]=0x2fb944;return;
                }
                r[14]=entry;r[15]=0x426748;return;
            }
            BeginPrompts(m,r);return;
        }
        if(stage==Stage::Native) {r=saved;BeginPrompts(m,r);return;}
        if(stage==Stage::Vignette) {
            vignette.Resume(m,r);if(vignette.Pending())return;
            BeginIcons(m,r);return;
        }
        if(stage==Stage::Icons) {
            icons.Resume(m,r);if(icons.Pending())return;
            text.Begin(m,r,entry,prompts.text,assets->text,source);
            if(text.Pending()){stage=Stage::Text;return;}
        } else if(stage==Stage::Text) {text.Resume(m,r);if(text.Pending())return;}
        assets.reset();stage=Stage::Idle;
    }
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc,bool supported,std::int64_t time) {
        on_top.store(false);
        back_title_visible=false;
        const U32 state=State(m);
        const bool valid=supported && CodeMatches(m) && HasAtlas(m);
        song_completed=valid && SongCompleted(m);
        // Switching between playable notes and the list must not fade the
        // persistent Start/B prompts just because input is temporarily locked.
        const bool switching=(state==13 || state==14 || state==10 || state==11) &&
            m.Read(Owner+0x40)==std::optional<U32>{1};
        const float alpha=fade.Advance(time,valid && (Ready(m) || switching));
        const U32 current_source=m.Read(Owner+4).value_or(0);
        if(!valid || !state || current_source!=last_source || (slide.time && time<*slide.time)) {
            slide={};slide_history={};bottom_history={};midpoint_shift=bottom_midpoint=bottom_offset=0;
            last_list={};slide_x.store(0);last_source=current_source;
        }
        if(!valid || !state)return;
        const bool list_requested=(List(state) || state==13) && !Closing(state) && !song_completed;
        // State 13 still belongs to native resource cleanup. Do not slide in
        // empty/static placeholders before the finalized song artwork is ready.
        const bool wanted=list_requested && state!=13;
        const float amount=slide.Advance(time,wanted);
        back_title_visible=BackTitle(m) && amount>0 && !song_completed;
        if(Closing(state) && atlas.Owned(m))on_top.store(true);
        if(!wanted && amount==0 && alpha==0)return;
        frame_layout=GetLayout();assets=PausePrompts::GetAssets();entry=return_pc;
        source=current_source;
        cover=std::min(HUDFade::Visibility(m,4),HUDFade::Visibility(m,6));
        prompts=assets?Prompts(frame_layout,*assets,list_requested || amount>0,alpha,
            m.Read(Owner+0x40)==std::optional<U32>{1}):PausePrompts::Pair{};
        corner=PromptCorner(frame_layout.prompt_vignette,alpha,Widescreen::Active()?240*Widescreen::Aspect:400);
        if(wanted)last_list=Atlas(m,frame_layout,Opening(state));
        offset=-(1-amount)*Travel(last_list,frame_layout,LeftEdge());slide_x.store(offset);
        midpoint_shift=slide_history.Observe(Interpolation::guest_frames.load(),time,
            Interpolation::generation.load(),OcarinaSlide::LayoutKey(frame_layout),offset);
        bottom_offset=(1-amount)*BottomTravel(last_list);
        bottom_midpoint=bottom_history.Observe(Interpolation::guest_frames.load(),time,
            Interpolation::generation.load(),OcarinaSlide::LayoutKey(frame_layout),bottom_offset);
        draw_native=(List(state) && !Closing(state) && !song_completed) || back_title_visible.load();
        if((wanted || amount>0 || (Interpolation::Replaying() && midpoint_shift!=0)) && last_list.count) {
            atlas.Begin(m,r,entry,frame_layout.bottom_panels?SplitPanel(last_list,frame_layout,offset,bottom_offset,LeftEdge(),false):WithPanel(last_list,frame_layout,offset,LeftEdge()),{},source);
            if(atlas.Pending()){stage=Stage::Atlas;return;}
        }
        BeginPrompts(m,r);
    }
    template<class M> void Material(M& m,U32 at) const {
        if(!Drawing())return;
        const float native=std::bit_cast<float>(m.Read(at).value_or(0));
        if(std::isfinite(native))m.Write(at,std::bit_cast<U32>(cover*((vignette.Drawing() || icons.Drawing() || text.Drawing())?1.f:native)));
    }
    template<class R> void Bottom(R& r) {
        r[14]=Sites[3].pc+4;r[15]=(on_top.load() || song_completed)?Sites[3].pc+4:0x426748;
    }
};
}
