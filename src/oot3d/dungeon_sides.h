// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_shadow.h"
#include "oot3d/dungeon_parchment.h"
#include "oot3d/dungeon_floor_line.h"
#include "oot3d/map_top_fade.h"

namespace OOT3D::DungeonSides {
using TitleScreen::U32;
using PauseShadow::Rect;
constexpr unsigned Capacity=200; // Capacity of the shared guest layer buffers.
struct Geometry {
    std::array<float,Capacity*12> positions{};
    std::array<float,Capacity*8> uvs{};
    std::array<float,Capacity*16> colors{};
    unsigned count=0;
    bool Add(const std::array<float,12>& p,const std::array<float,8>& uv,const std::array<float,16>& c) {
        if(count==Capacity)return false;
        std::copy(p.begin(),p.end(),positions.begin()+count*12);
        std::copy(uv.begin(),uv.end(),uvs.begin()+count*8);
        std::copy(c.begin(),c.end(),colors.begin()+count*16);++count;return true;
    }
};
// The source atlas can contain map parchment, wells and floor buttons in one
// draw. Split only verified dungeon owners, preserving their native samplers,
// material fades and UVs. Deferred side contents render above the shadows.
using Layer=GuestUI::Hook<Geometry,64,8,0,true>;
// The atlas and its replacement parchment share local coordinates. Trim
// only the floor artwork, preserving UVs/colors rather than squeezing it.
inline void ClipFloorRight(std::array<float,12>& p,std::array<float,8>& uv,
                           std::array<float,16>& color,float edge) {
    if(!std::isfinite(edge))return;
    const auto original=p;const auto tex=uv;const auto rgba=color;
    for(unsigned i=0;i<4;++i) {
        if(original[i*3]<=edge)continue;
        for(unsigned j=0;j<4;++j) {
            if(original[j*3]>=original[i*3] || original[j*3+1]!=original[i*3+1])continue;
            const float t=std::clamp((edge-original[j*3])/(original[i*3]-original[j*3]),0.f,1.f);
            p[i*3]=std::lerp(original[j*3],original[i*3],t);
            for(unsigned c=0;c<2;++c)uv[i*2+c]=std::lerp(tex[j*2+c],tex[i*2+c],t);
            for(unsigned c=0;c<4;++c)color[i*4+c]=std::lerp(rgba[j*4+c],rgba[i*4+c],t);
            break;
        }
    }
}
template<class M> std::optional<std::array<Rect,2>> Backgrounds(M& m,const PauseMenu::Layout& layout) {
    const U32 model=m.Read(PauseMenu::UI+0x10).value_or(0),mesh=m.Read(model+4).value_or(0);
    const U32 desc=m.Read(mesh).value_or(0),current=m.Read(mesh+0x124).value_or(2);
    const U32 live=current<2?m.Read(mesh+0x1a0+4*current).value_or(0):0;
    const auto b=PauseShadow::Bounds(m,live,m.Read(desc+0xc).value_or(0),model);
    if(!b || b->left>=b->right || b->top>=b->bottom)return {};
    std::array<Rect,2> out;
    for(unsigned i=0;i<2;++i) {
        auto p=layout.map_backgrounds[i].pose;p.x+=PauseMenu::motion_x.load();
        out[i]=PauseShadow::ToTop(*b,p);
        out[i].top-=240*PauseMenu::Scale*p.scale;
    }
    return out;
}
inline PauseShadow::Geometry Blocks(const PauseMenu::Layout& layout,const std::array<Rect,2>& backgrounds,float alpha) {
    PauseShadow::Geometry g;
    for(unsigned side=0;side<2;++side) {
        const auto& s=layout.dungeon_blocks[side];
        if(!s.enabled || !std::isfinite(s.x+s.y+s.width+s.height+s.fade+s.opacity))continue;
        const float width=std::clamp(s.width,0.f,600.f),height=std::clamp(s.height,0.f,600.f);
        const float fade=std::clamp(s.fade,0.f,width),opacity=std::clamp(s.opacity,0.f,1.f)*alpha;
        const float edge=(side?backgrounds[side].left:backgrounds[side].right)+s.x;
        const Rect box{side?edge-width:edge,s.y,side?edge:edge+width,s.y+height};
        if(side) {
            g.Add({box.left,box.top,box.left+fade,box.bottom},opacity,false,false,true);
            g.Add({box.left+fade,box.top,box.right,box.bottom},opacity,false,true);
        } else {
            g.Add({box.left,box.top,box.right-fade,box.bottom},opacity,false,true);
            g.Add({box.right-fade,box.top,box.right,box.bottom},opacity,true);
        }
    }
    return g;
}
inline PauseShadow::Geometry TopShadow(const PauseMenu::Layout& layout,Rect paper,float alpha) {
    PauseShadow::Geometry g;
    const auto& s=layout.dungeon_top_shadow;
    if(!s.enabled || !std::isfinite(s.height+s.opacity+s.y+alpha) || s.height<=0)return g;
    const float end=paper.top+s.y,start=end-std::clamp(s.height,0.f,240.f);
    g.Add({paper.left,start,paper.right,std::min(end,paper.top)},std::clamp(s.opacity,0.f,1.f)*alpha);
    // Cast onto the background above the paper: darkest against its top edge,
    // fading outward. A positive offset clips at the paper rather than shading it.
    if(g.count) {
        constexpr float near=.5f/64,far=63.5f/64;
        for(unsigned i=0;i<4;++i)
            g.uvs[i*2]=std::lerp(near,far,(end-g.positions[i*3+1])/(end-start));
    }
    return g;
}
struct Hook {
    std::array<Layer,4> fronts{},backs{};
    std::array<U32,4> sources{};
    std::array<Geometry,4> geometry{};
    unsigned used=0,next=0;
    int immediate=-1;
    PauseShadow::Hook blocks;
    MapTopFade::Hook top_fade;
    bool top_fade_started=false;
    PauseShadow::Hook top_shadow;
    std::optional<Rect> paper_bounds;
    bool top_shadow_started=false;
    DungeonFloorLine::Hook floor_line;
    bool floor_line_started=false;
    DungeonParchment::Hook parchment;
    bool parchment_first=false;
    std::optional<std::array<Rect,2>> backgrounds;
    float alpha=0;
    bool finish_started=false;
    bool Drawing() const {
        if(parchment.Drawing())return true;
        for(const auto& layer:fronts)if(layer.Drawing())return true;
        for(const auto& layer:backs)if(layer.Drawing())return true;
        return false;
    }
    bool Finishing() const {return finish_started;}
    void Reset() {parchment.ready=false;parchment_first=false;used=next=0;immediate=-1;backgrounds.reset();paper_bounds.reset();alpha=0;finish_started=false;floor_line_started=false;top_shadow_started=false;top_fade_started=false;}
    template<class M,class R> bool NativeDraw(M& m,R& r,bool enabled) {
        if(!enabled || Drawing() || blocks.Drawing() || top_fade.Drawing() || top_shadow.Drawing() || floor_line.Drawing() || !PauseMenu::DungeonPage(m))return false;
        const U32 model=r[0],mesh=m.Read(model+4).value_or(0),desc=m.Read(mesh).value_or(0);
        const unsigned vertices=m.Read(desc+0xc).value_or(0),current=m.Read(mesh+0x124).value_or(2);
        if(!vertices || vertices%4 || vertices>200 || current>=2 || used==sources.size())return false;
        if(!PauseMenu::DungeonQuadPart(m,mesh,0,vertices/4))return false;
        // The native four-icon group includes the banner Skulltula. Let the
        // header queue draw it after the location paper/title, not in our back pass.
        if(vertices==16 && PauseMenu::DungeonQuadPart(m,mesh,3,4)==PauseMenu::DungeonSkulltula)return false;
        const auto layout=PauseMenu::GetLayout();
        const auto panels=Backgrounds(m,layout);if(!panels)return false;
        const U32 live=m.Read(mesh+0x1a0+current*4).value_or(0);
        std::optional<float> floor_edge;
        if(vertices==200 && PauseMenu::DungeonQuadPart(m,mesh,0,50)==PauseMenu::DungeonMap) {
            std::array<float,12> paper;
            if(parchment.ready && parchment.native_model==model) {
                paper=parchment.prepared.positions;
                floor_edge=std::max({paper[0],paper[3],paper[6],paper[9]})-1;
            } else if(TitleScreen::ReadFloats(m,live,paper))
                floor_edge=std::max({paper[0],paper[3],paper[6],paper[9]});
            std::array<float,12> matrix{};
            if(floor_edge && TitleScreen::ReadFloats(m,model+0xc,matrix)) {
                Rect bounds{1e9f,1e9f,-1e9f,-1e9f};
                for(unsigned i=0;i<4;++i) {
                    const float x=matrix[0]*paper[i*3]+matrix[1]*paper[i*3+1]+matrix[2]*paper[i*3+2]+matrix[3];
                    const float y=matrix[4]*paper[i*3]+matrix[5]*paper[i*3+1]+matrix[6]*paper[i*3+2]+matrix[7];
                    bounds.left=std::min(bounds.left,x);bounds.right=std::max(bounds.right,x);
                    bounds.top=std::min(bounds.top,y);bounds.bottom=std::max(bounds.bottom,y);
                }
                paper_bounds=PauseShadow::ToTop(bounds,{});
            }
        }
        Geometry front,back;
        for(unsigned q=0;q<vertices/4;++q) {
            const auto part=PauseMenu::DungeonQuadPart(m,mesh,q,vertices/4);if(!part)return false;
            std::array<float,12> p;std::array<float,8> uv;std::array<float,16> color;
            if(!TitleScreen::ReadFloats(m,live+q*48,p) ||
               !TitleScreen::ReadFloats(m,live+vertices*12+q*32,uv) ||
               !TitleScreen::ReadFloats(m,live+vertices*20+q*64,color))return false;
            // Floor artwork stops at the visible paper's right border. The
            // separate cursor model and left controls retain their full size.
            // Use the merged paper, not the first piece of an extended map.
            if(floor_edge && *part==PauseMenu::DungeonFloors)ClipFloorRight(p,uv,color,*floor_edge);
            // Keep these editor-positioned controls above the shadows.
            // CopyPositions already collapses native unavailable/parked quads.
            if(*part==PauseMenu::DungeonItems || *part==PauseMenu::DungeonFloors ||
               *part==PauseMenu::DungeonBanner) {
                if(!front.Add(p,uv,color))return false;
            } else if(!(*part==PauseMenu::DungeonMap && parchment.ready && parchment.native_model==model) && !back.Add(p,uv,color))return false;
            if(*part==PauseMenu::DungeonMap) {
                alpha=1;for(unsigned v=0;v<4;++v)alpha=std::min(alpha,std::clamp(color[v*4+3],0.f,1.f));
            }
        }
        backgrounds=panels;sources[used]=model;geometry[used]=front;
        const unsigned slot=used++;
        if(parchment.ready && parchment.native_model==model) {
            backs[slot].geometry=back;
            parchment.Begin(m,r,PauseMenu::Sites[19].pc);
            if(parchment.Pending()) {parchment_first=true;immediate=int(slot);return true;}
            geometry[slot]={};return false; // Leave native paper visible if allocation cannot start.
        }
        if(back.count) {
            backs[slot].Begin(m,r,PauseMenu::Sites[19].pc,back,{},model);
            if(backs[slot].Pending()) {immediate=int(slot);return true;}
        }
        r[15]=r[14];return true;
    }
    template<class M,class R> void ResumeImmediate(M& m,R& r) {
        if(immediate<0)return;
        const unsigned slot=unsigned(immediate);auto& layer=backs[slot];
        if(parchment_first) {
            const bool drew=parchment.Drawing();parchment.Resume(m,r);
            if(parchment.Pending())return;
            parchment_first=false;
            if(!drew) { // Failure before the replacement drew: retain the complete native atlas.
                geometry[slot]={};immediate=-1;r[1]=0x54c8c8;r[15]=0x3f9684;return;
            }
            layer.Begin(m,r,PauseMenu::Sites[19].pc,layer.geometry,{},sources[slot]);
            if(layer.Pending())return;
            immediate=-1;r[15]=r[14];return;
        }
        const bool drew=layer.Drawing();layer.Resume(m,r);
        if(!layer.Pending()) {
            immediate=-1;
            if(drew)r[15]=r[14];
            else {
                // Allocation/ownership failure: continue the original draw.
                // Do not also replay the foreground copy from this source.
                geometry[slot]={};
                r[1]=0x54c8c8;r[15]=0x3f9684;
            }
        }
    }
    // Resume at the existing end-of-pause-draw site. Backdrop shadows go below
    // all deferred wells, floor cards, labels, icons, key counts and cursors.
    template<class M,class R> void Finish(M& m,R& r,U32 entry) {
        if(blocks.Pending()) {blocks.Resume(m,r);if(blocks.Pending())return;}
        if(top_fade.Pending()) {top_fade.Resume(m,r);if(top_fade.Pending())return;}
        if(top_shadow.Pending()) {top_shadow.Resume(m,r);if(top_shadow.Pending())return;}
        if(floor_line.Pending()) {floor_line.Resume(m,r);if(floor_line.Pending())return;}
        if(next && fronts[next-1].Pending()) {
            fronts[next-1].Resume(m,r);if(fronts[next-1].Pending())return;
        }
        if(!finish_started) {
            finish_started=true;
            if(backgrounds && PauseMenu::DungeonPage(m)) {
                blocks.Begin(m,r,entry,Blocks(PauseMenu::GetLayout(),*backgrounds,alpha));
                if(blocks.Pending())return;
            }
        }
        if(!top_fade_started) {
            top_fade_started=true;
            if(paper_bounds && PauseMenu::DungeonPage(m)) {
                top_fade.Begin(m,r,entry,MapTopFade::Make(PauseMenu::GetLayout().dungeon_map_top_fade,*paper_bounds,alpha),
                    MapTopFade::Pixels,m.Read(PauseHeader::Items+0x20).value_or(0));
                if(top_fade.Pending())return;
            }
        }
        if(!top_shadow_started) {
            top_shadow_started=true;
            if(paper_bounds && PauseMenu::DungeonPage(m)) {
                top_shadow.Begin(m,r,entry,TopShadow(PauseMenu::GetLayout(),*paper_bounds,alpha));
                if(top_shadow.Pending())return;
            }
        }
        if(!floor_line_started) {
            floor_line_started=true;
            if(used && PauseMenu::DungeonPage(m)) {
                floor_line.Begin(m,r,entry,DungeonFloorLine::Make(PauseMenu::GetLayout(),alpha,PauseMenu::DungeonFloorRows(m)),
                    DungeonFloorLine::Pixels,m.Read(PauseHeader::Items+0x20).value_or(0));
                if(floor_line.Pending())return;
            }
        }
        while(next<used) {
            const unsigned i=next++;
            fronts[i].Begin(m,r,entry,geometry[i],{},sources[i]);
            if(fronts[i].Pending())return;
        }
    }
    bool Pending() const {
        if(blocks.Pending() || top_fade.Pending() || top_shadow.Pending() || floor_line.Pending())return true;
        for(const auto& layer:fronts)if(layer.Pending())return true;
        return false;
    }
};
}
