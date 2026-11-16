// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include "oot3d/world_map_assets.h"
#include "oot3d/dungeon_sides.h"
#include "oot3d/map_extension.h"
#include <future>
#include <thread>

namespace OOT3D::WorldMap {
using TitleScreen::U32;
using DungeonParchment::Rect;
using DungeonParchment::Color;
using DungeonParchment::Pixels;
using DungeonParchment::Geometry;
using ImageLayer=GuestUI::Hook<Geometry,DungeonParchment::Width,DungeonParchment::Height>;
inline float TopFade(float y,float top,const PauseMenu::MapTopFade& fade) {
    return MapTopFade::Opacity(y*PauseMenu::Scale,top*PauseMenu::Scale,fade);
}
inline std::vector<Rect> Pieces(const PauseMenu::Layout& layout) {
    std::vector<Rect> out;
    for(const auto& stack:layout.world_parchments) {
        if(!stack.enabled || !PauseMenu::Valid(stack.pose) || !std::isfinite(stack.spacing))continue;
        for(unsigned i=0;i<2;++i) {
            auto p=stack.pose;
            p.y+=(i?1:-1)*std::clamp(stack.spacing,0.f,240.f)*p.scale*.5f;
            const auto a=PauseMenu::Move(56,32,PauseMenu::DungeonMap,p);
            const auto b=PauseMenu::Move(264,208,PauseMenu::DungeonMap,p);
            out.push_back({a[0],a[1],b[0],b[1]});
        }
    }
    return out;
}
inline Color Over(Color foreground,Color background) {
    const float a=foreground[3]/255,b=background[3]/255*(1-a),total=a+b;
    Color out{};
    if(total>0)for(unsigned c=0;c<3;++c)out[c]=(foreground[c]*a+background[c]*b)/total;
    out[3]=total*255;return out;
}
// Blur the retained artwork's alpha, including its rounded/feathered outline.
// Padding allows the shadow to extend beyond every edge of the original atlas.
struct EdgeShadow {
    static constexpr unsigned Padding=48,Width=MapWidth+2*Padding,Height=MapHeight+2*Padding;
    std::vector<float> pixels;
    void Build(const std::vector<Color>& map,MapExtension::Crop crop,float width) {
        pixels.clear();
        if(!std::isfinite(width) || width<=0 || map.size()!=MapWidth*MapHeight)return;
        width=std::min(width,float(Padding));
        const int radius=int(std::ceil(width));const float sigma=std::max(width/3,.35f);
        std::vector<float> kernel(2*radius+1);float total=0;
        for(int i=-radius;i<=radius;++i)total+=kernel[i+radius]=std::exp(-.5f*i*i/(sigma*sigma));
        for(auto& weight:kernel)weight/=total;
        std::vector<float> source(Width*Height),horizontal(Width*Height);pixels.resize(Width*Height);
        for(int y=crop.top;y<crop.bottom;++y)for(int x=crop.left;x<crop.right;++x)
            source[(y+Padding)*Width+x+Padding]=map[y*MapWidth+x][3]/255;
        for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x) {
            float value=0;
            for(int i=-radius;i<=radius;++i)if(int(x)+i>=0 && int(x)+i<int(Width))
                value+=source[y*Width+x+i]*kernel[i+radius];
            horizontal[y*Width+x]=value;
        }
        for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x) {
            float value=0;
            for(int i=-radius;i<=radius;++i)if(int(y)+i>=0 && int(y)+i<int(Height))
                value+=horizontal[(y+i)*Width+x]*kernel[i+radius];
            pixels[y*Width+x]=value;
        }
    }
    float At(float x,float y) const {
        x+=Padding;y+=Padding;
        if(pixels.empty() || x<0 || y<0 || x>=Width-1 || y>=Height-1)return 0;
        const unsigned ix=unsigned(x),iy=unsigned(y);
        return std::lerp(std::lerp(pixels[iy*Width+ix],pixels[iy*Width+ix+1],x-ix),
            std::lerp(pixels[(iy+1)*Width+ix],pixels[(iy+1)*Width+ix+1],x-ix),y-iy);
    }
};
// Keep expensive artwork/outline/shadow composition independent of the clock.
// The colored top fade is applied after lighting, so cache its additive term
// separately rather than tinting it along with the parchment. Host memory only.
struct LightingCache {
    std::vector<Color> base;
    std::array<MapTimeTint::Color,DungeonParchment::Height> additive{};
    MapTimeTint::Color tint=MapTimeTint::Neutral;
    unsigned rebuilds=0;
    void Reset() {base.resize(DungeonParchment::Width*DungeonParchment::Height);++rebuilds;}
    void Row(unsigned y,float shade,const std::array<float,3>& color) {
        for(unsigned c=0;c<3;++c) {
            const float target=std::isfinite(color[c])?std::clamp(color[c],0.f,1.f):0;
            additive[y][c]=255*target*shade;
        }
    }
    void Store(unsigned x,unsigned y,Color color,float shade) {
        for(unsigned c=0;c<3;++c)color[c]*=1-shade;
        base[y*DungeonParchment::Width+x]=color;
    }
    std::shared_ptr<Pixels> Apply(MapTimeTint::Color value) {
        auto result=std::make_shared<Pixels>();
        for(unsigned y=0;y<DungeonParchment::Height;++y)for(unsigned x=0;x<DungeonParchment::Width;++x) {
            const auto& color=base[y*DungeonParchment::Width+x];
            U32 pixel=0;
            for(unsigned c=0;c<4;++c) {
                const float channel=c<3?color[c]*value[c]+additive[y][c]:color[c];
                pixel|=U32(std::clamp(std::lround(channel),0l,255l))<<(24-8*c);
            }
            (*result)[DungeonParchment::TiledOffset(x,DungeonParchment::Height-1-y)]=pixel;
        }
        tint=value;return result;
    }
};
// Paper coordinates, feather distances and mirrored fine texture coordinates
// are separable. Compute them once per row/column, not for every pixel of
// every overlapping parchment during the first pause draw.
struct PreparedPaper {
    using Sample=DungeonParchment::AxisSample;
    struct Axis {Sample sample;float depth;bool inside;};
    std::vector<std::array<Axis,DungeonParchment::Width>> columns;
    std::vector<std::array<Axis,DungeonParchment::Height>> rows;
    std::array<Sample,DungeonParchment::Width> fine_x{};
    std::array<Sample,DungeonParchment::Height> fine_y{};
    PreparedPaper(const DungeonParchment::Paper& paper,std::span<const Rect> pieces,Rect bounds) {
        columns.resize(pieces.size());rows.resize(pieces.size());
        for(unsigned i=0;i<pieces.size();++i) {
            const auto& b=pieces[i];
            const auto prepare=[&](auto& axis,auto& fine,float lo,float hi,float first,float last,
                                    unsigned size,float grid_lo,float grid_hi) {
                for(unsigned k=0;k<axis.size();++k) {
                    const float p=std::lerp(lo,hi,(k+.5f)/axis.size());
                    const float uv=(p-first)/(last-first)*(size-1);
                    axis[k]={Sample::Make(uv,size),std::min(p-first,last-p),p>=first && p<=last};
                    if(i==0)fine[k]=Sample::Make(grid_lo+DungeonParchment::Mirror(uv-grid_lo,grid_hi-grid_lo),size);
                }
            };
            prepare(columns[i],fine_x,bounds.left,bounds.right,b.left,b.right,paper.width,paper.grid.left,paper.grid.right);
            prepare(rows[i],fine_y,bounds.top,bounds.bottom,b.top,b.bottom,paper.height,paper.grid.top,paper.grid.bottom);
        }
    }
    Color At(const DungeonParchment::Paper& paper,unsigned x,unsigned y,float blend) const {
        Color color{};float weights=0,alpha=0;
        for(unsigned i=0;i<columns.size();++i) {
            const auto& cx=columns[i][x];const auto& cy=rows[i][y];
            if(!cx.inside || !cy.inside)continue;
            const auto broad=DungeonParchment::SamplePrepared(paper.soft,paper.width,cx.sample,cy.sample);
            const auto native=DungeonParchment::SamplePrepared(paper.original,paper.width,cx.sample,cy.sample);
            const float depth=std::min(cx.depth,cy.depth);
            const float t=blend>0?std::clamp(depth/blend,0.f,1.f):1;
            const float weight=std::max(.00001f,t*t*(3-2*t))*native[3];
            for(unsigned c=0;c<3;++c)color[c]+=weight*broad[c];
            weights+=weight;alpha=std::max(alpha,native[3]);
        }
        if(weights<=0)return {};
        const auto fine=DungeonParchment::SamplePrepared(paper.original,paper.width,fine_x[x],fine_y[y]);
        const auto broad=DungeonParchment::SamplePrepared(paper.soft,paper.width,fine_x[x],fine_y[y]);
        for(unsigned c=0;c<3;++c)color[c]=std::clamp(color[c]/weights+fine[c]-broad[c],0.f,255.f);
        color[3]=alpha;return color;
    }
};
// Rebuilds operate only on immutable host snapshots and disjoint output rows.
// Keep guest reads/writes on the emulation thread and join before publishing.
template<class Draw> void ComposeRows(Draw draw) {
    constexpr unsigned height=DungeonParchment::Height;
    const unsigned workers=std::clamp(std::thread::hardware_concurrency()/2,1u,4u);
    std::array<std::future<void>,3> tasks;
    for(unsigned i=1;i<workers;++i)
        tasks[i-1]=std::async(std::launch::async,[&,i] {draw(height*i/workers,height*(i+1)/workers);});
    draw(0,height/workers);
    for(unsigned i=1;i<workers;++i)tasks[i-1].get();
}
struct Artwork {
    Rect map,bounds;
    MapExtension::Crop crop;
    bool reflected,shadow_enabled;
    std::array<float,3> map_base;
    std::vector<Rect> pieces;
    float shadow_opacity,shadow_width,blend,brightness,fade_top;
    PauseMenu::MapTopFade top_fade;
    std::vector<float> key;
    Artwork(const PauseMenu::Layout& layout,std::vector<Rect> papers,Rect rect,
            MapExtension::Crop region,bool mirror,std::array<float,3> base)
        :map(rect),crop(region),reflected(mirror),map_base(base),pieces(std::move(papers)) {
        shadow_opacity=std::isfinite(layout.world_map_shadow_opacity)?std::clamp(layout.world_map_shadow_opacity,0.f,1.f):0;
        shadow_width=std::isfinite(layout.world_map_shadow_width)?std::clamp(layout.world_map_shadow_width,0.f,48.f):0;
        shadow_enabled=shadow_opacity>0 && shadow_width>0;
        top_fade=layout.world_map_top_fade;
        blend=std::isfinite(layout.world_parchment_blend)?std::clamp(layout.world_parchment_blend,0.f,80.f):18;
        brightness=PauseMenu::MapBrightness(layout.world_parchment_darkness);
        key={map.left,map.top,map.right,map.bottom,float(crop.left),float(crop.top),
            float(crop.right),float(crop.bottom),float(reflected),blend,brightness,map_base[0],map_base[1],map_base[2],shadow_opacity,shadow_width};
        key.insert(key.end(),{float(top_fade.enabled),top_fade.opacity,top_fade.height,top_fade.y,top_fade.softness});
        key.insert(key.end(),top_fade.color.begin(),top_fade.color.end());
        for(const auto& p:pieces)key.insert(key.end(),{p.left,p.top,p.right,p.bottom});
        bounds=map;
        if(shadow_enabled) {
            const float dx=shadow_width*(map.right-map.left)/(crop.right-crop.left);
            const float dy=shadow_width*(map.bottom-map.top)/(crop.bottom-crop.top);
            bounds={map.left-dx,map.top-dy,map.right+dx,map.bottom+dy};
        }
        for(const auto& p:pieces) {
            bounds.left=std::min(bounds.left,p.left);bounds.top=std::min(bounds.top,p.top);
            bounds.right=std::max(bounds.right,p.right);bounds.bottom=std::max(bounds.bottom,p.bottom);
        }
        // Anchor to the paper itself, excluding the external outline shadow.
        fade_top=map.top;
        if(!pieces.empty()) {
            fade_top=pieces.front().top;
            for(const auto& p:pieces)fade_top=std::min(fade_top,p.top);
        }
        bounds.left-=1;bounds.top-=1;bounds.right+=1;bounds.bottom+=1;
    }
    LightingCache Compose(const DungeonParchment::Paper& paper,const std::vector<Color>& map_pixels,
                          const std::shared_ptr<const Outline>& outline) const {
        LightingCache lighting;
            auto masked=map_pixels;
            if(outline) {
                const auto boundary=Boundary(*outline);
                for(unsigned y=0;y<MapHeight;++y)for(unsigned x=0;x<MapWidth;++x)
                    masked[y*MapWidth+x][3]*=Alpha(boundary,outline->feather,x+.5f,y+.5f);
            }
            EdgeShadow shadow;
            if(shadow_enabled)shadow.Build(masked,crop,shadow_width);
            lighting.Reset();
            const PreparedPaper prepared_paper(paper,pieces,bounds);
            ComposeRows([&](unsigned first,unsigned last) {
            for(unsigned y=first;y<last;++y) {
                const float py=std::lerp(bounds.top,bounds.bottom,(y+.5f)/DungeonParchment::Height);
                const float shade=TopFade(py,fade_top,top_fade);
                lighting.Row(y,shade,top_fade.color);
              for(unsigned x=0;x<DungeonParchment::Width;++x) {
                const float px=std::lerp(bounds.left,bounds.right,(x+.5f)/DungeonParchment::Width);
                const float map_x=(px-map.left)/(map.right-map.left);
                const float sx=std::lerp(float(crop.left),float(crop.right),reflected?1-map_x:map_x)-.5f;
                const float sy=std::lerp(float(crop.top),float(crop.bottom),(py-map.top)/(map.bottom-map.top))-.5f;
                Color fg{};
                if(px>=map.left && px<=map.right && py>=map.top && py<=map.bottom) {
                    fg=DungeonParchment::Sample(masked,MapWidth,MapHeight,sx,sy);
                    for(unsigned c=0;c<3;++c)fg[c]*=map_base[c];
                }
                // Opaque map artwork completely covers the backing papers and
                // shadow. Avoid sampling/blending those invisible layers.
                Color color=fg;
                if(fg[3]<255) {
                    color=prepared_paper.At(paper,x,y,blend/PauseMenu::Scale);
                    for(unsigned c=0;c<3;++c)color[c]*=brightness;
                    if(shadow_enabled)color=Over({0,0,0,255*shadow_opacity*shadow.At(sx,sy)},color);
                    color=Over(fg,color);
                }
                // Tint only map/paper RGB. Their silhouette remains intact;
                // separately replayed labels, icons and banners stay above it.
                lighting.Store(x,y,color,shade);
              }
            }
            });
        return lighting;
    }
};
struct WarmArtwork {
    std::vector<float> key;
    std::shared_ptr<const PaperImage> paper;
    std::shared_ptr<const Outline> outline;
    std::shared_ptr<const std::vector<U32>> source;
    LightingCache lighting;
};
using WarmPair=std::array<WarmArtwork,2>;
inline Common::AtomicSharedPtr<const WarmPair> warm_artwork;
inline std::shared_ptr<const WarmPair> FindWarm(const std::vector<float>& key,
        const std::vector<U32>& source,const std::shared_ptr<const PaperImage>& paper,
        const std::shared_ptr<const Outline>& outline,unsigned& index) {
    auto ready=warm_artwork.load();
    if(ready)for(unsigned i=0;i<ready->size();++i) {
        const auto& candidate=(*ready)[i];
        if(candidate.key==key && candidate.paper==paper && candidate.outline==outline &&
           candidate.source && *candidate.source==source){index=i;return ready;}
    }
    return {};
}
struct WarmSource {
    std::shared_ptr<const PaperImage> paper_image;
    std::shared_ptr<const std::vector<U32>> compressed;
    std::vector<Color> map;
    DungeonParchment::Paper paper;
};
inline Artwork WarmRecipe(const PauseMenu::Layout& layout,bool reflected) {
    // USA Rev1 world constructor 480bb8: first rectangle at 5073a0/507490
    // is (0,0)-(320,240). CopyQuads applies the user's Map pose to it.
    const auto a=PauseMenu::Move(0,0,PauseMenu::Map,layout[PauseMenu::Map]);
    const auto b=PauseMenu::Move(320,240,PauseMenu::Map,layout[PauseMenu::Map]);
    const auto round=[](float x){return std::round(x*1024)/1024;};
    const float brightness=std::round(PauseMenu::MapBrightness(layout.map_darkness)*65536.f)/65536.f;
    return Artwork(layout,Pieces(layout),{round(a[0]),round(a[1]),round(b[0]),round(b[1])},
        {},reflected,{brightness,brightness,brightness});
}
class Warmer {
    std::future<std::shared_ptr<const WarmPair>> job;
public:
    // Called by the frontend with immutable game assets, never guest memory.
    // Polling cannot wait for unfinished work; live draws keep their validated
    // synchronous fallback if settings changed immediately before opening.
    void Poll(std::shared_ptr<const WarmSource> source,const PauseMenu::Layout& layout,
              std::shared_ptr<const Outline> outline) {
        if(!source || !source->paper_image || !PauseMenu::Valid(layout[PauseMenu::Map]))return;
        if(!layout.world_map_outline)outline.reset();
        const auto normal=WarmRecipe(layout,false),mirror=WarmRecipe(layout,true);
        const auto matches=[&](const std::shared_ptr<const WarmPair>& pair) {
            return pair && (*pair)[0].key==normal.key && (*pair)[1].key==mirror.key &&
                (*pair)[0].paper==source->paper_image && (*pair)[0].source==source->compressed &&
                (*pair)[0].outline==outline;
        };
        if(job.valid()) {
            if(job.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
            auto completed=job.get();
            if(matches(completed))warm_artwork.store(std::move(completed));
        }
        if(matches(warm_artwork.load()))return;
        job=std::async(std::launch::async,[source,outline,normal,mirror] {
            auto result=std::make_shared<WarmPair>();
            for(unsigned i=0;i<2;++i) {
                const auto& recipe=i?mirror:normal;
                (*result)[i]={recipe.key,source->paper_image,outline,source->compressed,
                    recipe.Compose(source->paper,source->map,outline)};
            }
            return std::shared_ptr<const WarmPair>(std::move(result));
        });
    }
};
struct Hook {
    ImageLayer image;
    DungeonSides::Layer contents;
    std::shared_ptr<const PaperImage> paper_source;
    DungeonParchment::Paper paper;
    std::shared_ptr<const Outline> cached_outline;
    std::shared_ptr<Pixels> composed;
    LightingCache lighting;
    std::vector<U32> compressed;
    std::vector<Color> map_pixels;
    std::vector<float> cached_key;
    Geometry prepared;
    DungeonSides::Geometry foreground;
    U32 native_model=0;
    bool immediate=false,dirty=false,replaced=false;
    const char* diagnostic=nullptr;
    unsigned diagnostic_reports=0,warm_hits=0;
    bool Drawing() const {return image.Drawing() || contents.Drawing();}
    bool Pending() const {return immediate;}
    template<class M,class Decode> bool Prepare(M& m,U32 model,Decode decode) {
        constexpr U32 world=0x506cb0,quads=30,vertices=quads*4;
        if(!PauseMenu::MapPage(m) || PauseMenu::DungeonPage(m) ||
           (!m.Read(world+0x38).value_or(0) && !PauseMenu::world_exit_hold) || m.Read(world+4)!=model)return false;
        diagnostic=nullptr;
        const auto fail=[&](const char* reason){diagnostic=reason;return false;};
        const U32 mesh=m.Read(model+4).value_or(0),helper=m.Read(world+0x10).value_or(0);
        const U32 desc=m.Read(mesh).value_or(0),current=m.Read(mesh+0x124).value_or(2);
        if(m.Read(model)!=0x4ebd60 || m.Read(world)!=mesh || m.Read(helper)!=quads ||
           m.Read(desc+12)!=vertices || m.Read(mesh+0x15c)!=(512|(256u<<16)) ||
           m.Read(mesh+0x164)!=13 || current>=2)return fail("native map descriptor");
        const U32 live=m.Read(mesh+0x1a0+current*4).value_or(0),address=m.Read(mesh+0x158).value_or(0);
        if(!TitleScreen::Pointer(live,vertices*36) || address<0x14000000 || address>0x1c000000-131072)return fail("native map buffers");
        const auto layout=PauseMenu::GetLayout();
        const auto new_paper=GetPaper();
        auto pieces=new_paper?Pieces(layout):std::vector<Rect>{};
        const auto outline=layout.world_map_outline?GetOutline():nullptr;
        // Capture the source even before an outline is drawn or papers enabled.
        MapExtension::Crop crop;Rect map{};std::array<float,16> map_color{};
        float z=0;unsigned found=0;foreground={};bool reflected=false;
        const float motion=PauseMenu::motion_x.load()/PauseMenu::Scale;
        const auto round=[](float x){return std::round(x*1024)/1024;};
        for(unsigned q=0;q<quads;++q) {
            std::array<float,12> p;std::array<float,8> uv;std::array<float,16> color;
            if(!TitleScreen::ReadFloats(m,live+q*48,p) ||
               !TitleScreen::ReadFloats(m,live+vertices*12+q*32,uv) ||
               !TitleScreen::ReadFloats(m,live+vertices*20+q*64,color))return false;
            float u0=1,u1=0,v0=1,v1=0;
            for(unsigned i=0;i<4;++i){u0=std::min(u0,uv[2*i]);u1=std::max(u1,uv[2*i]);v0=std::min(v0,uv[2*i+1]);v1=std::max(v1,uv[2*i+1]);}
            MapExtension::Crop c{int(std::lround(u0*512)),int(std::lround((1-v1)*256)),
                int(std::lround(u1*512)),int(std::lround((1-v0)*256))};
            if(!MapExtension::Valid(c)) {if(!foreground.Add(p,uv,color))return false;continue;}
            if(++found!=1)return fail("multiple map quads");
            map={1e9f,1e9f,-1e9f,-1e9f};
            for(unsigned i=0;i<4;++i) {
                map.left=std::min(map.left,p[3*i]);map.right=std::max(map.right,p[3*i]);
                map.top=std::min(map.top,p[3*i+1]);map.bottom=std::max(map.bottom,p[3*i+1]);
            }
            if(map.right-map.left<1 || map.bottom-map.top<1)return fail("degenerate map quad");
            // MQ reverses the map's horizontal UVs. Preserve that reflection
            // while composing the same saved outline, parchment and shadows.
            bool normal=true,mirror=true;
            for(unsigned i=0;i<4;++i) {
                const float tx=(uv[2*i]-u0)/(u1-u0),ty=(v1-uv[2*i+1])/(v1-v0);
                const bool vertical=std::abs(p[3*i+1]-std::lerp(map.top,map.bottom,ty))<=.05f;
                normal&=vertical && std::abs(p[3*i]-std::lerp(map.left,map.right,tx))<=.05f;
                mirror&=vertical && std::abs(p[3*i]-std::lerp(map.right,map.left,tx))<=.05f;
            }
            if(!normal && !mirror)return fail("rotated map quad");
            reflected=!normal && mirror;
            map={round(map.left-motion),round(map.top),round(map.right-motion),round(map.bottom)};
            crop=c;map_color=color;z=p[2];
        }
        if(found!=1)return fail("map quad not found");
        std::vector<U32> bytes(131072/4);
        for(unsigned i=0;i<bytes.size();++i) {const auto v=m.Read(address+4*i);if(!v)return false;bytes[i]=*v;}
        const bool changed=compressed!=bytes;
        if(changed || map_pixels.empty()) {
            auto source=std::make_shared<MapFrame::SourcePixels>();map_pixels.resize(MapWidth*MapHeight);
            const auto word=[&](unsigned at){return std::uint64_t(bytes[at/4])|(std::uint64_t(bytes[at/4+1])<<32);};
            for(unsigned y=0;y<MapHeight;++y)for(unsigned x=0;x<MapWidth;++x) {
                const unsigned at=((y/8)*64+x/8)*64+((y%8)/4*2+(x%8)/4)*16;
                const auto c=decode(word(at+8),x%4,y%4);
                const U32 alpha=((word(at)>>(4*(4*(x%4)+y%4)))&15)*17;
                map_pixels[y*MapWidth+x]={float(c[0]),float(c[1]),float(c[2]),float(alpha)};
                (*source)[y*MapFrame::SourceWidth+x]=(U32(c[0])<<24)|(U32(c[1])<<16)|(U32(c[2])<<8)|alpha;
            }
            source_image.store(std::shared_ptr<const MapFrame::SourcePixels>(source));
            compressed=std::move(bytes);
        }
        const bool paper_changed=new_paper!=paper_source;
        if(paper_changed && new_paper)paper.Set(new_paper->colors,PaperWidth,PaperHeight);
        paper_source=new_paper;
        const auto paper_tint=MapTimeTint::Read(m,layout.world_map_time_tint);
        std::array<float,3> map_base{};
        for(unsigned c=0;c<3;++c)map_base[c]=std::round(map_color[c]/paper_tint[c]*65536.f)/65536.f;
        const Artwork artwork(layout,std::move(pieces),map,crop,reflected,map_base);
        if(artwork.pieces.empty() && !outline && !artwork.shadow_enabled && !artwork.top_fade.enabled)
            return fail("no map artwork enabled/loaded");
        const auto& key=artwork.key;const auto& bounds=artwork.bounds;
        const bool rebuild=!composed || changed || paper_changed || key!=cached_key || outline!=cached_outline;
        if(rebuild) {
            const unsigned rebuilds=lighting.rebuilds+1;unsigned index=0;
            if(auto ready=FindWarm(key,compressed,new_paper,outline,index)) {
                lighting=(*ready)[index].lighting;++warm_hits;
            } else lighting=artwork.Compose(paper,map_pixels,outline);
            lighting.rebuilds=rebuilds;
            cached_key=key;cached_outline=outline;
        }
        if(rebuild || paper_tint!=lighting.tint) {composed=lighting.Apply(paper_tint);dirty=true;}
        prepared.positions={bounds.left+motion,bounds.top,z,bounds.right+motion,bounds.top,z,
            bounds.left+motion,bounds.bottom,z,bounds.right+motion,bounds.bottom,z};
        prepared.uvs={0,0,1,0,0,1,1,1};prepared.colors.fill(1);
        for(unsigned i=0;i<4;++i)prepared.colors[4*i+3]=map_color[4*i+3];
        prepared.count=1;native_model=model;return true;
    }
    template<class M,class R,class Decode> bool NativeDraw(M& m,R& r,bool enabled,Decode decode) {
        if(!enabled || Drawing() || immediate || !Prepare(m,r[0],decode))return false;
        if(dirty && image.Owned(m)) {
            for(unsigned i=0;i<composed->size();++i)if(!m.Read(image.texture+4*i))return false;
            for(unsigned i=0;i<composed->size();++i)m.Write(image.texture+4*i,(*composed)[i]);
        }
        image.Begin(m,r,PauseMenu::Sites[19].pc,prepared,*composed,native_model);
        immediate=image.Pending();
        if(immediate)dirty=false;else diagnostic="image setup not started";
        return immediate;
    }
    template<class M,class R> void Resume(M& m,R& r) {
        const auto fallback=[&] {immediate=false;r[1]=0x54c8c8;r[15]=0x3f9684;};
        if(image.Pending()) {
            const bool drew=image.Drawing();image.Resume(m,r);
            if(image.Drawing()) {
                for(U32 at=0xc;at<=0x38;at+=4)m.Write(image.model+at,m.Read(native_model+at).value_or(0));
                for(U32 at=0xf0;at<=0xfc;at+=4)m.Write(image.model+at,m.Read(native_model+at).value_or(0));
            }
            if(image.Pending())return;
            if(!drew){diagnostic="image setup/draw failed";fallback();return;}
            contents.Begin(m,r,PauseMenu::Sites[19].pc,foreground,{},native_model);
            if(contents.Pending())return;
            diagnostic="foreground setup not started";fallback();return;
        }
        const bool drew=contents.Drawing();contents.Resume(m,r);
        if(contents.Pending())return;
        immediate=false;
        if(drew){replaced=true;r[15]=r[14];}else {diagnostic="foreground setup/draw failed";fallback();}
    }
};
}
