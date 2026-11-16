// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_cutout.h"
#include "oot3d/pause_prompts.h"
#include "oot3d/boss_challenge_layout.h"
namespace OOT3D::BossBorder {
constexpr unsigned Width=512,Height=512,FooterHeight=20;
inline float Brightness(const BossChallenge::Layout& l) {
    const auto amount=[](float v){return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;};
    return (1-amount(l.panel_darkness))*(1-amount(l.border_darkness));
}
using Pixels=std::array<std::uint32_t,Width*Height>;
using Geometry=PausePrompts::QuadGeometry<2>;
constexpr unsigned Tile(unsigned x,unsigned y) {
    return MapFrame::TiledOffset(x,y); // Same 512-wide Morton layout.
}
inline std::shared_ptr<Pixels> MakePixels(const MapFrame::SourcePixels& source,const MapFrame::CutImage& cut,float radius=0,bool footer=true,float shadow_width=0,float shadow_opacity=0) {
    auto out=std::make_shared<Pixels>();
    for(int y=0;y<cut.height;++y)for(int x=0;x<cut.width;++x)
        (*out)[Tile(x+cut.left,Height-1-y-cut.top)]=(*cut.pixels)[MapFrame::TiledOffset(x,255-y)];
    // Distance from the opaque cutout into its transparent image opening.
    // Restrict this to the saved cutout, leaving record boxes and exterior alone.
    shadow_width=std::isfinite(shadow_width)?std::clamp(shadow_width,0.f,24.f):0;
    shadow_opacity=std::isfinite(shadow_opacity)?std::clamp(shadow_opacity,0.f,1.f):0;
    if(shadow_width>0 && shadow_opacity>0) {
        const int w=cut.width,h=cut.height;
        std::vector<float> distance(w*h,1000.f);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)
            if((*cut.pixels)[MapFrame::TiledOffset(x,255-y)]&255)distance[y*w+x]=0;
        for(int pass=0;pass<2;++pass)for(int row=0;row<h;++row)for(int col=0;col<w;++col) {
            const int x=pass?w-1-col:col,y=pass?h-1-row:row,step=pass?1:-1;
            auto& d=distance[y*w+x];
            for(int dx=-1;dx<=1;++dx) {
                const int nx=x+dx,ny=y+step;
                if(nx>=0 && nx<w && ny>=0 && ny<h)
                    d=std::min(d,distance[ny*w+nx]+(dx?1.41421356f:1.f));
            }
            if(x+step>=0 && x+step<w)d=std::min(d,distance[y*w+x+step]+1);
        }
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            auto& pixel=(*out)[Tile(x+cut.left,Height-1-y-cut.top)];
            if(pixel&255)continue;
            const float t=std::clamp(1-distance[y*w+x]/(shadow_width+1),0.f,1.f);
            pixel=unsigned(std::lround(255*shadow_opacity*t*t*(3-2*t)));
        }
    }
    // The saved cutout ends above the record boxes. Include the remaining
    // brown surround, but never cover the boxes (or their native lettering).
    for(unsigned y=216;y<240;++y)for(unsigned x=0;x<400;++x) {
        const bool record=y>=220 && y<=235 &&
            ((x>=24 && x<=195) || (x>=204 && x<=375));
        (*out)[Tile(x,Height-1-y)]=record?0:source[y*400+x];
    }
    // Continue the last brown row into the extension, then smoothly join the
    // rotated side texture. Sample beyond its dark inner rule; that rule was
    // making a second horizontal seam. No pixels above row 239 enter the blend.
    for(unsigned x=0;x<400;++x)for(unsigned y=0;y<FooterHeight;++y) {
        const unsigned sx=384+y*15/(FooterHeight-1);
        const unsigned sy=64+((x/96)%2?95-x%96:x%96);
        const auto edge=source[239*400+x],side=source[sy*400+sx];
        const float t=std::min(y/8.f,1.f),blend=t*t*(3-2*t);
        std::uint32_t pixel=255;
        for(unsigned shift:{8u,16u,24u}) {
            const float a=(edge>>shift)&255,b=(side>>shift)&255;
            pixel|=std::uint32_t(std::lround(a+(b-a)*blend))<<shift;
        }
        (*out)[Tile(x,Height-1-240-y)]=pixel;
    }
    // Feather only the outside silhouette; interior record borders stay intact.
    radius=std::isfinite(radius)?std::clamp(radius,0.f,32.f):0;
    for(unsigned y=0;y<240+FooterHeight;++y)for(unsigned x=0;x<400;++x) {
        const float r=y<240?radius:std::min(radius,float(FooterHeight));
        const bool corner=y<r || (!footer && y<240 && y>=240-r) || (footer && y>=260-r);
        if(!corner || r<=0)continue;
        const float cx=std::clamp(x+.5f,r,400-r);
        const float cy=y<r?r:footer?260-r:240-r;
        const float d=std::hypot(x+.5f-cx,y+.5f-cy);
        auto& pixel=(*out)[Tile(x,Height-1-y)];
        const float alpha=std::clamp(r+.5f-d,0.f,1.f);
        pixel=(pixel&~255u)|unsigned(std::lround((pixel&255)*alpha));
    }
    return out;
}
struct Rect {float left=0,top=0,right=400,bottom=240;};
inline Geometry Make(const BossChallenge::Layout& l,const Rect& rect={}) {
    namespace B=BossChallenge;Geometry g;const auto p=l[B::Preview],b=l[B::Border];
    if(!B::Valid(p) || !B::Valid(b))return g;
    const auto add=[&](float top,float height,const B::Pose& pose) {
        if(!B::Valid(pose))return;
        const float sx=(rect.right-rect.left)/400,sy=(rect.bottom-rect.top)/240;
        const float scale=p.scale*pose.scale;
        float y=120+p.y+(rect.top+(120+pose.y+(top-120)*pose.scale)*sy-120)*p.scale;
        float h=height*scale*sy;
        if(top==240) {
            // Moving the extension upward must not paint over the record boxes.
            // Keep the join sample at the visible top, even for an overlapping pose.
            const float join=120+p.y+(rect.bottom-120)*p.scale;
            const float clipped=std::max(y,join);
            h-=clipped-y;y=clipped;
            if(h<=0)return;
        }
        g.Add(200+p.x+(rect.left+(200+pose.x-200*pose.scale)*sx-200)*p.scale,
              y,400*scale*sx,h,
              .5f/Width,(top+.5f)/Height,399.5f/Width,(top+height-.5f)/Height);
    };
    if(l.border_enabled)add(0,240,b);
    if(l.border_bottom)add(240,FooterHeight,l[B::Footer]);
    return g;
}
struct Hook : GuestUI::Hook<Geometry,Width,Height> {
    std::shared_ptr<const MapFrame::CutImage> uploaded;
    std::shared_ptr<const Pixels> image;
    float uploaded_shadow_width=-1,uploaded_shadow_opacity=-1;
    float uploaded_radius=-1;bool uploaded_footer=false;
    template<class M,class R> void BeginBorder(M& m,R& r,unsigned entry,const BossChallenge::Layout& l,unsigned donor,const Rect& rect={}) {
        const auto cut=MapFrame::GetCutImage();const auto source=MapFrame::GetSource();
        if(!cut || !source)return;
        if(cut!=uploaded || uploaded_radius!=l.corner_radius || uploaded_footer!=l.border_bottom || uploaded_shadow_width!=l.inner_shadow_width || uploaded_shadow_opacity!=l.inner_shadow_opacity) {
            const auto next=MakePixels(*source,*cut,l.corner_radius,l.border_bottom,l.inner_shadow_width,l.inner_shadow_opacity);
            if(Owned(m)) {
                for(unsigned i=0;i<next->size();++i)if(!m.Read(texture+4*i))return;
                for(unsigned i=0;i<next->size();++i)m.Write(texture+4*i,(*next)[i]);
            }
            uploaded_shadow_width=l.inner_shadow_width;uploaded_shadow_opacity=l.inner_shadow_opacity;
            image=next;uploaded=cut;uploaded_radius=l.corner_radius;uploaded_footer=l.border_bottom;
        }
        Begin(m,r,entry,Make(l,rect),*image,donor);
    }
};
}
