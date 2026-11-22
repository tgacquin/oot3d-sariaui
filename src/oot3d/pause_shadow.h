// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_header.h"
#include "oot3d/assets/panel_shadow.h"
#include "oot3d/guest_ui_mesh.h"

namespace OOT3D::PauseShadow {
using TitleScreen::U32;
// Four background quads, up to sixteen clipped gradient pieces, and two eight-piece
// gear shadows that can each split into four rectangles per piece.
constexpr U32 Quads=84, Descriptor=0x1c0, Positions=0x2e0, UVs=Positions+Quads*48,
    Colors=UVs+Quads*32, Indices=Colors+Quads*64, Cookie=(Indices+Quads*12+15)&~15u,
    AllocationSize=(Cookie+16+63)&~31u;
constexpr U32 Magic=0x4f534831;
constexpr float Depth=8;
struct Rect { float left=0,top=0,right=0,bottom=0; };
template<unsigned Capacity=Quads> struct MeshGeometry {
    std::array<float,Capacity*12> positions{};
    std::array<float,Capacity*8> uvs{};
    std::array<float,Capacity*16> colors{};
    unsigned count=0;
    void Add(Rect box,float opacity,bool right=false,bool solid=false,bool left=false) {
        if(count==Capacity || opacity<=0 || box.right<=box.left || box.bottom<=box.top)return;
        const std::array<float,12> v{box.left,box.bottom,0,box.left,box.top,0,
            box.right,box.bottom,0,box.right,box.top,0};
        constexpr float near=.5f/64,far=63.5f/64;
        const std::array<float,8> uv=solid?std::array{near,.5f,near,.5f,near,.5f,near,.5f}:
            left?std::array{far,.5f,far,.5f,near,.5f,near,.5f}:
            right?std::array{near,.5f,near,.5f,far,.5f,far,.5f}:
                  std::array{far,.5f,near,.5f,far,.5f,near,.5f};
        std::copy(v.begin(),v.end(),positions.begin()+count*12);
        std::copy(uv.begin(),uv.end(),uvs.begin()+count*8);
        for(unsigned i=0;i<16;++i)colors[count*16+i]=i%4==3?std::clamp(opacity,0.f,1.f):1;
        ++count;
    }
};
using Geometry=MeshGeometry<>;
inline float Width(const PauseMenu::Shadow& s) {
    return std::isfinite(s.width)?std::clamp(s.width,.1f,4.f):1;
}
inline float Opacity(const PauseMenu::Shadow& s) {
    return std::isfinite(s.opacity)?std::clamp(s.opacity,0.f,1.f):.45f;
}
inline Geometry Header(const PauseMenu::Layout& layout,float offset,bool wide) {
    Geometry g;
    const float width=wide?240*Widescreen::Aspect:400, y=offset+layout.header_y+PauseHeader::Height;
    g.Add({(400-width)/2,y,(400+width)/2,y+Depth*Width(layout.header_shadow)},Opacity(layout.header_shadow));
    return g;
}
template<class M> std::optional<Rect> Bounds(M& m,U32 vertices,unsigned count,U32 model) {
    if(!count || count>256 || !TitleScreen::Pointer(vertices,count*12))return {};
    std::array<float,12> matrix{};
    if(!TitleScreen::ReadFloats(m,model+0xc,matrix))return {};
    Rect b{1e9f,1e9f,-1e9f,-1e9f};
    for(unsigned i=0;i<count;++i) {
        std::array<float,3> v{};if(!TitleScreen::ReadFloats(m,vertices+i*12,v))return {};
        const float x=matrix[0]*v[0]+matrix[1]*v[1]+matrix[2]*v[2]+matrix[3];
        const float y=matrix[4]*v[0]+matrix[5]*v[1]+matrix[6]*v[2]+matrix[7];
        b.left=std::min(b.left,x);b.right=std::max(b.right,x);
        b.top=std::min(b.top,y);b.bottom=std::max(b.bottom,y);
    }
    return b;
}
// Record the plate only when it is actually drawn. The view already contains
// the editor pose and any opening or retained closing animation.
template<class M> void ObserveTextbox(M& m,U32 model,U32 view,std::optional<Rect>& bounds) {
    const U32 pane=m.Read(ScreenModels::Message+0x56c).value_or(0);
    if(!TitleScreen::Pointer(pane,0xb4) || m.Read(pane+0x64)!=model || m.Read(pane)!=0x4ed234 ||
       m.Read(pane+4)!=ScreenModels::Message+0x168 || m.Read(model)!=0x4ebd60)return;
    const U32 mesh=m.Read(model+4).value_or(0),desc=m.Read(mesh).value_or(0);
    const auto count=m.Read(desc+12),slot=m.Read(mesh+0x124);
    if(!count || (*count!=0 && *count!=4) || !slot || *slot>=2)return;
    const U32 live=m.Read(mesh+0x1a0+4*(*slot)).value_or(0);
    std::array<float,12> vertices,pose,camera;
    if(!TitleScreen::Pointer(live,48) || !TitleScreen::ReadFloats(m,live,vertices) ||
       !TitleScreen::ReadFloats(m,model+12,pose) || !TitleScreen::ReadFloats(m,view,camera))return;
    Rect box{1e9f,1e9f,-1e9f,-1e9f};
    for(unsigned i=0;i<12;i+=3) {
        std::array<float,3> world{};
        for(unsigned row=0;row<3;++row) {
            const auto* r=pose.data()+row*4;
            world[row]=r[0]*vertices[i]+r[1]*vertices[i+1]+r[2]*vertices[i+2]+r[3];
        }
        const float x=camera[0]*world[0]+camera[1]*world[1]+camera[2]*world[2]+camera[3];
        const float y=camera[4]*world[0]+camera[5]*world[1]+camera[6]*world[2]+camera[7];
        box.left=std::min(box.left,x);box.right=std::max(box.right,x);
        box.top=std::min(box.top,y);box.bottom=std::max(box.bottom,y);
    }
    if(box.right>box.left && box.bottom>box.top)bounds=box;
}
inline Rect ToTop(Rect b,const PauseMenu::Pose& p) {
    const auto point=[&](float x,float y) {
        return std::array{200+(x-160)*PauseMenu::Scale*p.scale+p.x,
            120+(y-120)*PauseMenu::Scale*p.scale+PauseMenu::OffsetY+p.y};
    };
    const auto a=point(b.left,b.top),z=point(b.right,b.bottom);
    return {a[0],a[1],z[0],z[1]};
}
// Observe the largest map surface in the actual draw buffers, joining its
// tiled quads and excluding small markers and collapsed hidden cards. These
// positions already contain the map's independent pose and shared animation.
// Reset each routed frame so missing maps never leave a stale shadow behind.
struct MapEdges {
    std::optional<Rect> bounds;
    std::array<std::optional<Rect>,2> stones{};
    std::array<float,2> stone_alpha{};
    bool dungeon=false;
    float area=0,map_alpha=0;
    void Reset() {bounds.reset();area=0;map_alpha=0;stones={};stone_alpha={};dungeon=false;}
    template<class M> void Observe(M& m,U32 model) {
        const U32 mesh=m.Read(model+4).value_or(0);
        if(!PauseMenu::MapPage(m) || !PauseMenu::Owned(m,mesh) ||
           m.Read(PauseMenu::UI+0x10)==std::optional<U32>{model})return;
        const U32 desc=m.Read(mesh).value_or(0),current=m.Read(mesh+0x124).value_or(2);
        const U32 count=m.Read(desc+0xc).value_or(0);
        const U32 live=current<2?m.Read(mesh+0x1a0+4*current).value_or(0):0;
        if(!live || count>1024 || count%4)return;
        if(PauseMenu::DungeonPage(m)) {
            dungeon=true;
            if(count!=200 || PauseMenu::DungeonQuadPart(m,mesh,1,50)!=PauseMenu::DungeonLeft)return;
            if(auto b=Bounds(m,live,4,model);b && b->right>b->left && b->bottom>b->top)
                bounds=ToTop(*b,{});
            const U32 helper=m.Read(0x506cb0+0x14).value_or(0);
            const U32 colors=m.Read(helper+0x18).value_or(0);
            std::array<float,16> map_color{};
            if(TitleScreen::ReadFloats(m,colors,map_color)) {
                map_alpha=1;
                for(unsigned v=0;v<4;++v)map_alpha=std::min(map_alpha,std::clamp(map_color[v*4+3],0.f,1.f));
            }
            for(unsigned side=0;side<2;++side) {
                const unsigned q=side+1;
                const auto b=Bounds(m,live+q*48,4,model);
                std::array<float,16> rgba{};
                if(!b || b->right<=b->left || b->bottom<=b->top ||
                   !TitleScreen::ReadFloats(m,colors+q*64,rgba))continue;
                float alpha=1;
                for(unsigned v=0;v<4;++v)alpha=std::min(alpha,std::clamp(rgba[v*4+3],0.f,1.f));
                stones[side]=ToTop(*b,{});stone_alpha[side]=alpha;
            }
            return;
        }
        std::optional<Rect> surface;
        const float scale=PauseMenu::GetLayout()[PauseMenu::Map].scale;
        for(unsigned q=0;q<count/4;++q)if(auto b=Bounds(m,live+q*48,4,model)) {
            const float w=b->right-b->left,h=b->bottom-b->top;
            // Use native-sized dimensions even at the smallest editor scale.
            if(w<=32*scale || h<=32*scale)continue;
            if(!surface)surface=b;
            else {
                surface->left=std::min(surface->left,b->left);surface->top=std::min(surface->top,b->top);
                surface->right=std::max(surface->right,b->right);surface->bottom=std::max(surface->bottom,b->bottom);
            }
        }
        if(surface) {
            const float size=(surface->right-surface->left)*(surface->bottom-surface->top);
            if(size>area) {area=size;bounds=ToTop(*surface,{});}
        }
    }
    Geometry Make(const PauseMenu::Layout& layout) const {
        Geometry g;
        if(dungeon) {
            for(unsigned side=0;side<2;++side)if(stones[side]) {
                const auto b=*stones[side];
                const auto& shadow=side?layout.dungeon_right_shadow:layout.dungeon_left_shadow;
                const auto part=side?PauseMenu::DungeonRight:PauseMenu::DungeonLeft;
                const float depth=Depth*Width(shadow)*PauseMenu::Scale*layout[part].scale;
                const float alpha=Opacity(shadow)*stone_alpha[side];
                if(side)g.Add({b.right,b.top,b.right+depth,b.bottom},alpha,true);
                else g.Add({b.left-depth,b.top,b.left,b.bottom},alpha,false,false,true);
            }
            if(bounds && layout.dungeon_edge_shadows) {
                const auto b=*bounds;
                const float depth=Depth*Width(layout.dungeon_edge_shadow);
                const float alpha=Opacity(layout.dungeon_edge_shadow)*map_alpha;
                g.Add({b.left-depth,b.top,b.left,b.bottom},alpha,false,false,true);
                g.Add({b.right,b.top,b.right+depth,b.bottom},alpha,true);
            }
            return g;
        }
        if(!bounds || !layout.map_side_shadows)return g;
        const auto b=*bounds;
        const float depth=Depth*Width(layout.map_shadow)*PauseMenu::Scale*layout[PauseMenu::Map].scale;
        g.Add({b.left-depth,b.top,b.left,b.bottom},Opacity(layout.map_shadow),false,false,true);
        g.Add({b.right,b.top,b.right+depth,b.bottom},Opacity(layout.map_shadow),true);
        return g;
    }
};
template<unsigned N> void Perimeter(MeshGeometry<N>& g,Rect box,float depth,float opacity) {
    g.Add({box.left,box.bottom,box.right,box.bottom+depth},opacity);
    g.Add({box.left-depth,box.top,box.left,box.bottom},opacity,false,false,true);
    g.Add({box.right,box.top,box.right+depth,box.bottom},opacity,true);
    const unsigned top=g.count;
    g.Add({box.left,box.top-depth,box.right,box.top},opacity);
    if(g.count>top)for(unsigned i=top*8;i<top*8+8;i+=2)g.uvs[i]=1-g.uvs[i];
    // Join the edges with corners fading outward in both axes.
    for(unsigned corner=0;corner<4;++corner) {
        const bool right=corner&1,bottom=corner&2;
        const unsigned q=g.count;
        g.Add({right?box.right:box.left-depth,bottom?box.bottom:box.top-depth,
               right?box.right+depth:box.left,bottom?box.bottom+depth:box.top},
              opacity,right,false,!right);
        if(g.count>q)for(unsigned v=0;v<4;++v)
            if((v%2==0)==bottom)g.colors[q*16+v*4+3]=0;
    }
}
// Remove the neighboring green rectangle from a shadow. Interpolate its UVs
// and alpha when clipping, preserving the existing falloff instead of squeezing it.
template<unsigned N,unsigned Capacity> void ClipShadow(MeshGeometry<Capacity>& out,const MeshGeometry<N>& source,unsigned q,Rect obstacle) {
    const auto* v=source.positions.data()+q*12;
    const Rect b{v[0],v[4],v[6],v[1]};
    const float left=std::max(b.left,obstacle.left),right=std::min(b.right,obstacle.right);
    const float top=std::max(b.top,obstacle.top),bottom=std::min(b.bottom,obstacle.bottom);
    const auto add=[&](Rect r) {
        if(r.right<=r.left || r.bottom<=r.top || out.count>=Capacity)return;
        const unsigned dst=out.count;out.Add(r,1);
        for(unsigned i=0;i<4;++i) {
            const float x=(out.positions[dst*12+i*3]-b.left)/(b.right-b.left);
            const float y=(out.positions[dst*12+i*3+1]-b.top)/(b.bottom-b.top);
            const auto sample=[&](const auto& values,unsigned stride,unsigned c) {
                const auto* a=values.data()+q*4*stride;
                return (a[stride+c]*(1-x)+a[3*stride+c]*x)*(1-y)+
                       (a[c]*(1-x)+a[2*stride+c]*x)*y;
            };
            for(unsigned c=0;c<2;++c)out.uvs[dst*8+i*2+c]=sample(source.uvs,2,c);
            for(unsigned c=0;c<4;++c)out.colors[dst*16+i*4+c]=sample(source.colors,4,c);
        }
    };
    if(left>=right || top>=bottom) {add(b);return;}
    add({b.left,b.top,left,b.bottom});add({right,b.top,b.right,b.bottom});
    add({left,b.top,right,top});add({left,bottom,right,b.bottom});
}
inline void GearPanels(Geometry& g,const PauseMenu::Layout& layout,float alpha,float motion) {
    std::array<Rect,2> boxes;
    const std::array poses{layout.gear_panel,layout.gear_panel_lower};
    for(unsigned i=0;i<2;++i) {
        const auto p=poses[i];
        boxes[i]={200-128*p.width+p.x+motion,122-75.2f*p.height+p.y,
                  200+128*p.width+p.x+motion,122+75.2f*p.height+p.y};
    }
    for(unsigned i=0;i<2;++i) {
        MeshGeometry<8> ring;
        Perimeter(ring,boxes[i],Depth*Width(layout.gear_shadows[i])*PauseMenu::Scale,
            Opacity(layout.gear_shadows[i])*alpha);
        for(unsigned q=0;q<ring.count;++q)ClipShadow(g,ring,q,boxes[1-i]);
    }
}
template<class M> void ItemPanel(M& m,Geometry& g,const PauseMenu::Layout& layout,U32 helper,U32 model) {
    const U32 vertices=m.Read(helper+0x10).value_or(0),color=m.Read(helper+0x18).value_or(0);
    if(m.Read(helper)!=std::optional<U32>{37})return;
    std::array<float,12> matrix{};
    if(!TitleScreen::ReadFloats(m,model+0xc,matrix))return;
    Rect bounds{1e9f,1e9f,-1e9f,-1e9f};float alpha=1;
    for(unsigned q=0;q<2;++q) {
        std::array<float,12> v{};std::array<float,16> rgba{};
        if(!TitleScreen::ReadFloats(m,vertices+q*48,v) || !TitleScreen::ReadFloats(m,color+q*64,rgba))return;
        PauseMenu::RestingQuad(m,m.Read(model+4).value_or(0),q,v);
        const float left=std::min({v[0],v[3],v[6],v[9]}),right=std::max({v[0],v[3],v[6],v[9]});
        const float top=std::min({v[1],v[4],v[7],v[10]}),bottom=std::max({v[1],v[4],v[7],v[10]});
        if(left>=320 || top>=240 || right<=0 || bottom<=0 || right<=left || bottom<=top)return;
        for(unsigned i=0;i<4;++i) {
            alpha=std::min(alpha,std::clamp(rgba[i*4+3],0.f,1.f));
            const auto p=PauseMenu::AnimatedMove(v[i*3],v[i*3+1],PauseMenu::Items,layout[PauseMenu::Items]);
            const float x=matrix[0]*p[0]+matrix[1]*p[1]+matrix[2]*v[i*3+2]+matrix[3];
            const float y=matrix[4]*p[0]+matrix[5]*p[1]+matrix[6]*v[i*3+2]+matrix[7];
            bounds.left=std::min(bounds.left,x);bounds.right=std::max(bounds.right,x);
            bounds.top=std::min(bounds.top,y);bounds.bottom=std::max(bounds.bottom,y);
        }
    }
    const auto box=ToTop(bounds,{});
    const float depth=Depth*Width(layout.items_shadow)*PauseMenu::Scale*layout[PauseMenu::Items].scale;
    const float opacity=Opacity(layout.items_shadow)*alpha;
    Perimeter(g,box,depth,opacity);
}

inline void BottomCorners(Geometry& g,Rect panel,const PauseMenu::BackgroundVignette& settings,float visibility) {
    if(!settings.enabled)return;
    const float screen_width=Widescreen::Active()?240*Widescreen::Aspect:400;
    const float left=std::max(panel.left,(400-screen_width)/2);
    const float right=std::min(panel.right,(400+screen_width)/2);
    const float top=std::max(panel.top,0.f),bottom=std::min(panel.bottom,240.f);
    const float width=(right-left)*std::clamp(settings.width_percent,0.f,50.f)/100;
    const float height=(bottom-top)*std::clamp(settings.height_percent,0.f,100.f)/100;
    const float opacity=settings.opacity*std::clamp(visibility,0.f,1.f);
    for(bool left_corner:{true,false}) {
        const unsigned q=g.count;
        const float x=left_corner?left:right-width;
        g.Add({x,bottom-height,x+width,bottom},opacity,left_corner,false,!left_corner);
        if(g.count>q)g.colors[q*16+7]=g.colors[q*16+15]=0;
    }
}

template<class M> Geometry Panels(M& m,const PauseMenu::Layout& layout,float visibility=1,
                                std::optional<Rect> textbox={}) {
    Geometry g;
    // The independent top-screen pass now owns this shade after teardown.
    if(SaveMenu::QuitTransition(m))return g;
    if(SaveMenu::BackdropActive(m)) {
        const float width=Widescreen::Active()?240*Widescreen::Aspect:400;
        g.Add({(400-width)/2,0,(400+width)/2,240},
            SaveMenu::BackdropOpacity(m),false,true);
        return g;
    }
    const U32 bg=m.Read(PauseMenu::UI+0x10).value_or(0),mesh=m.Read(bg+4).value_or(0);
    const U32 desc=m.Read(mesh).value_or(0);
    // The descriptor's original positions can point to expired native stack
    // storage. Read the currently published GPU vertex buffer instead.
    const U32 current=m.Read(mesh+0x124).value_or(2);
    const U32 live=current<2?m.Read(mesh+0x1a0+4*current).value_or(0):0;
    Rect brown_panel{};
    if(auto b=Bounds(m,live,m.Read(desc+0xc).value_or(4),bg)) {
        if(PauseMenu::MapPage(m)) {
            for(unsigned side=0;side<2;++side) {
                const auto& background=layout.map_backgrounds[side];
                auto pose=background.pose;pose.x+=PauseMenu::motion_x.load();
                auto box=ToTop(*b,pose);box.top-=240*PauseMenu::Scale*pose.scale;
                g.Add(box,PauseMenu::DungeonPage(m)?layout.dungeon_background_darkness[side]:background.darkness,false,true);
            }
            return g;
        }
        auto pose=layout[PauseMenu::Background];pose.x+=PauseMenu::motion_x.load();
        brown_panel=ToTop(*b,pose);
        if(layout.stack_background)brown_panel.top-=240*PauseMenu::Scale*pose.scale;
    }
    if(layout.bottom_gradient && PausePrompts::CurrentPage(m)!=PausePrompts::Page::Map) {
        const float screen_width=Widescreen::Active()?240*Widescreen::Aspect:400;
        const float width=screen_width*(std::clamp(layout.bottom_gradient_width_percent,0.f,100.f)/100.f);
        const float height=std::clamp(layout.bottom_gradient_height,1.f,240.f);
        MeshGeometry<1> gradient;
        gradient.Add({(400-width)/2,240-height,(400+width)/2,240},
                     layout.bottom_gradient_opacity*visibility,false,true);
        if(gradient.count) {
            gradient.colors[7]=gradient.colors[15]=0; // Transparent top vertices.
            if(textbox) {
                MeshGeometry<4> pieces;
                ClipShadow(pieces,gradient,0,brown_panel);
                for(unsigned q=0;q<pieces.count;++q)ClipShadow(g,pieces,q,*textbox);
            } else ClipShadow(g,gradient,0,brown_panel);
        }
    }
    if(brown_panel.right>brown_panel.left && brown_panel.bottom>brown_panel.top) {
        // Both stacked backgrounds share one exterior edge, with no dark seam.
        const float depth=Depth*Width(layout.background_shadow)*layout[PauseMenu::Background].scale;
        g.Add({brown_panel.right,brown_panel.top,brown_panel.right+depth,brown_panel.bottom},Opacity(layout.background_shadow),true);
        g.Add(brown_panel,layout.background_darkness,false,true);
        BottomCorners(g,brown_panel,layout.background_vignette,visibility);
    }
    if(PauseMenu::Active(m) && m.Read(0x50446c+0x18).value_or(0)!=0) {
        const U32 helper=m.Read(0x50446c+0xc).value_or(0);
        std::array<float,4> color{};
        if(m.Read(helper)==41 && TitleScreen::ReadFloats(m,m.Read(helper+0x18).value_or(0),color))
            GearPanels(g,layout,std::clamp(color[3],0.f,1.f),PauseMenu::motion_x.load());
    }
    if(m.Read(PauseHeader::Items+0x34).value_or(0)==0)return g;
    const U32 helper=m.Read(PauseHeader::Items+0x24).value_or(0);
    const U32 vertices=m.Read(helper+0x10).value_or(0);
    const U32 model=m.Read(PauseHeader::Items+0x20).value_or(0);
    ItemPanel(m,g,layout,helper,model);
    std::array<float,12> v{},matrix{};
    if(m.Read(helper)!=std::optional<U32>{37} ||
       !TitleScreen::ReadFloats(m,vertices+12*48,v) ||
       !TitleScreen::ReadFloats(m,model+0xc,matrix))return g;
    PauseMenu::RestingQuad(m,m.Read(model+4).value_or(0),12,v);
    const float min_x=std::min({v[0],v[3],v[6],v[9]}),max_x=std::max({v[0],v[3],v[6],v[9]});
    const float min_y=std::min({v[1],v[4],v[7],v[10]}),max_y=std::max({v[1],v[4],v[7],v[10]});
    if(min_x>=320 || min_y>=240 || max_x<=0 || max_y<=0 || max_x<=min_x || max_y<=min_y)return g;
    std::array<float,16> colors{};
    const U32 color=m.Read(helper+0x18).value_or(0);
    if(!TitleScreen::ReadFloats(m,color+12*64,colors))return g;
    const float alpha=std::clamp((colors[3]+colors[7]+colors[11]+colors[15])*.25f,0.f,1.f);
    Rect b{1e9f,1e9f,-1e9f,-1e9f};
    for(unsigned i=0;i<12;i+=3) {
        float x=v[i],y=v[i+1];
        if(layout.horizontal) {x=288+(v[i+1]-120);y=120-(v[i]-288);}
        const auto p=PauseMenu::AnimatedMove(x,y,PauseMenu::Slots,layout[PauseMenu::Slots]);
        x=matrix[0]*p[0]+matrix[1]*p[1]+matrix[3];
        y=matrix[4]*p[0]+matrix[5]*p[1]+matrix[7];
        b.left=std::min(b.left,x);b.right=std::max(b.right,x);
        b.top=std::min(b.top,y);b.bottom=std::max(b.bottom,y);
    }
    const auto box=ToTop(b,{});
    const float depth=Depth*Width(layout.slots_shadow)*PauseMenu::Scale*layout[PauseMenu::Slots].scale;
    g.Add({box.left,box.bottom,box.right,box.bottom+depth},Opacity(layout.slots_shadow)*alpha);
    return g;
}
// Independent mesh with the mod's original shadow texture.
struct Hook : GuestUI::Hook<Geometry,64,8> {
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc,Geometry g) {
        GuestUI::Hook<Geometry,64,8>::Begin(m,r,return_pc,g,Assets::PanelShadow,
            m.Read(PauseHeader::Items+0x20).value_or(0));
    }
};
// Reuse the stones' alpha silhouettes, including their beveled/transparent
// corners. Live positions/UVs already contain the selected size and editor pose.
// Separate capacity: never enlarge every other shadow's stack/guest allocation.
struct TabHook : GuestUI::Hook<MeshGeometry<32>,1,1,0,true> {
    template<class M> static MeshGeometry<32> Make(M& m,U32 native,const PauseMenu::ArrowShadow& shadow) {
        MeshGeometry<32> g;
        if(!shadow.enabled || !PauseMenu::Active(m) || SaveMenu::Active(m))return g;
        const bool items=m.Read(PauseHeader::Items+0x34).value_or(0)!=0;
        const bool gear=m.Read(0x50446c+0x18).value_or(0)!=0;
        if(!items && !gear)return g;
        const U32 root=items?PauseHeader::Items:0x50446c,count=items?37:41;
        const U32 mesh=m.Read(root+(items?0x1c:0)).value_or(0);
        const U32 helper=m.Read(root+(items?0x24:0xc)).value_or(0);
        if(m.Read(root+(items?0x20:4))!=native || m.Read(native)!=0x4ebd60 ||
           m.Read(native+4)!=mesh || m.Read(helper)!=count)return g;
        const U32 desc=m.Read(mesh).value_or(0),current=m.Read(mesh+0x124).value_or(2);
        const U32 live=current<2?m.Read(mesh+0x1a0+current*4).value_or(0):0;
        if(m.Read(desc+12)!=count*4 || !TitleScreen::Pointer(live,count*144))return g;
        std::array<float,12> matrix;
        if(!TitleScreen::ReadFloats(m,native+12,matrix))return g;
        const auto safe=[](float v,float lo,float hi,float fallback) {
            return std::isfinite(v)?std::clamp(v,lo,hi):fallback;
        };
        const float radius=safe(shadow.softness,.5f,12,3),ox=safe(shadow.x,-12,12,0),oy=safe(shadow.y,-12,12,1);
        const float strength=safe(shadow.strength,0,1,.5f);
        if(strength==0)return g;
        const float weight=1-std::pow(1-strength,1.f/16);
        constexpr std::array<std::array<float,2>,8> directions{{
            {1,0},{.7071068f,.7071068f},{0,1},{-.7071068f,.7071068f},
            {-1,0},{-.7071068f,-.7071068f},{0,-1},{.7071068f,-.7071068f}}};
        // Native layout tables: Items 9/11; Gear 21/23. Other cells are
        // labels, pressed overlays or the removed rail, not stone silhouettes.
        for(unsigned q:items?std::array{9u,11u}:std::array{21u,23u}) {
            std::array<float,12> p;std::array<float,8> uv;std::array<float,16> color;
            if(!TitleScreen::ReadFloats(m,live+q*48,p) ||
               !TitleScreen::ReadFloats(m,live+count*48+q*32,uv) ||
               !TitleScreen::ReadFloats(m,live+count*80+q*64,color))return {};
            const float width=std::max({p[0],p[3],p[6],p[9]})-std::min({p[0],p[3],p[6],p[9]});
            const float height=std::max({p[1],p[4],p[7],p[10]})-std::min({p[1],p[4],p[7],p[10]});
            if(width<=0 || height<=0)continue; // Native parked/hidden cells were collapsed.
            for(unsigned i=0;i<4;++i) {
                const float x=matrix[0]*p[i*3]+matrix[1]*p[i*3+1]+matrix[2]*p[i*3+2]+matrix[3];
                const float y=matrix[4]*p[i*3]+matrix[5]*p[i*3+1]+matrix[6]*p[i*3+2]+matrix[7];
                p[i*3]=200+(x-160)*PauseMenu::Scale;
                p[i*3+1]=120+(y-120)*PauseMenu::Scale+PauseMenu::OffsetY;p[i*3+2]=0;
            }
            for(float ring:{.5f,1.f})for(const auto& d:directions) {
                const unsigned at=g.count++;
                std::copy(uv.begin(),uv.end(),g.uvs.begin()+at*8);
                for(unsigned i=0;i<4;++i) {
                    g.positions[at*12+i*3]=p[i*3]+ox+d[0]*radius*ring;
                    g.positions[at*12+i*3+1]=p[i*3+1]+oy+d[1]*radius*ring;
                    // Vertex RGB zero makes a black copy; texture alpha keeps
                    // the exact stone contour, with no rectangular corner fill.
                    g.colors[at*16+i*4+3]=std::clamp(color[i*4+3],0.f,1.f)*weight;
                }
            }
        }
        return g;
    }
    template<class M,class R> bool NativeDraw(M& m,R& r,bool enabled) {
        if(!enabled || Pending())return false;
        const U32 native=r[0];
        auto g=Make(m,native,PauseMenu::GetLayout().tab_shadow);
        if(!g.count)return false;
        Begin(m,r,PauseMenu::Sites[19].pc,g,{},native);return Pending();
    }
    template<class M,class R> void ResumeNative(M& m,R& r) {
        if(stage==Stage::Colors) {
            // Items and Gear use different atlases. Refresh only our sampler
            // on every draw, including page changes that reuse this allocation.
            const U32 mesh=m.Read(source+4).value_or(0);
            std::array<U32,24> bindings{};
            bool valid=true;
            for(unsigned i=0;i<bindings.size();++i) {
                const auto value=m.Read(mesh+0x13c+i*4);
                if(!value){valid=false;break;}bindings[i]=*value;
            }
            if(valid) {
                for(unsigned i=0;i<bindings.size();++i)m.Write(block+0x13c+i*4,bindings[i]);
                m.Write(block+0x13c,1);m.Write(block+0x140,3);
            }
        }
        Resume(m,r);
        if(!Pending()) {r[1]=0x54c8c8;r[15]=0x3f9684;}
    }
};
// Draw immediately before the native arrow panel, above the Items surface.
struct ArrowHook : GuestUI::Hook<MeshGeometry<8>,64,8> {
    template<class M,class R> bool NativeDraw(M& m,R& r,bool enabled) {
        const auto shadow=PauseMenu::GetLayout().arrow_shadow;
        if(!enabled || Pending() || !shadow.enabled || !PauseMenu::ArrowChooserOpen(m))return false;
        // +4c is the gray backing panel (441fcc). +28 is the moving
        // selection cursor, drawn last; shadowing it follows the chosen item.
        const U32 owner=m.Read(PauseHeader::Items+0x4c).value_or(0);
        const U32 native=m.Read(owner+4).value_or(0);
        if(!native || native!=r[0])return false;
        const U32 mesh=m.Read(native+4).value_or(0),desc=m.Read(mesh).value_or(0);
        const U32 current=m.Read(mesh+0x124).value_or(2);
        const U32 live=current<2?m.Read(mesh+0x1a0+4*current).value_or(0):0;
        auto bounds=Bounds(m,live,m.Read(desc+0xc).value_or(0),native);
        if(!bounds)return false;
        auto b=ToTop(*bounds,{});
        if(b.right<=b.left || b.bottom<=b.top)return false;
        const auto safe=[](float f,float lo,float hi,float fallback) {
            return std::isfinite(f)?std::clamp(f,lo,hi):fallback;
        };
        const float d=safe(shadow.softness,1,24,8),x=safe(shadow.x,-12,12,0),y=safe(shadow.y,-12,12,2);
        const float alpha=safe(shadow.strength,0,1,.45f);
        b.left+=x;b.right+=x;b.top+=y;b.bottom+=y;
        MeshGeometry<8> g;
        g.Add({b.left-d,b.top,b.left,b.bottom},alpha,false,false,true);
        g.Add({b.right,b.top,b.right+d,b.bottom},alpha,true);
        g.Add({b.left,b.bottom,b.right,b.bottom+d},alpha);
        g.Add({b.left,b.top-d,b.right,b.top},alpha);
        if(g.count==4)for(unsigned i=24;i<32;i+=2)g.uvs[i]=1-g.uvs[i];
        // Feather the corners in both axes so the perimeter has no gaps.
        for(unsigned corner=0;corner<4;++corner) {
            const bool right=corner&1,bottom=corner&2;
            const unsigned q=g.count;
            g.Add({right?b.right:b.left-d,bottom?b.bottom:b.top-d,
                   right?b.right+d:b.left,bottom?b.bottom+d:b.top},alpha,right,false,!right);
            if(g.count>q)for(unsigned v=0;v<4;++v)
                if((v%2==0)==bottom)g.colors[q*16+v*4+3]=0;
        }
        Begin(m,r,PauseMenu::Sites[19].pc,g,Assets::PanelShadow,
            m.Read(PauseHeader::Items+0x20).value_or(0));
        return Pending();
    }
    template<class M,class R> void ResumeNative(M& m,R& r) {
        Resume(m,r);
        if(!Pending()) {r[1]=0x54c8c8;r[15]=0x3f9684;}
    }
};

}
