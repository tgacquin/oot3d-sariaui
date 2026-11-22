// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_shadow.h"
#include "oot3d/map_frame_asset.h"
#include <chrono>

namespace OOT3D::DungeonParchment {
using TitleScreen::U32;
using PauseShadow::Rect;
constexpr unsigned Width=1024,Height=512;
using Pixels=std::array<U32,Width*Height>;
using Color=std::array<float,4>;
struct Geometry {
    std::array<float,12> positions{};
    std::array<float,8> uvs{};
    std::array<float,16> colors{};
    unsigned count=0;
};
inline bool Enabled(const PauseMenu::Layout& layout) {
    return std::any_of(layout.dungeon_parchments.begin(),layout.dungeon_parchments.end(),
        [](const auto& p){return p.enabled && PauseMenu::Valid(p.pose);});
}
inline float Mirror(float x,float size) {
    x=std::fmod(x,2*size);if(x<0)x+=2*size;
    return x<=size?x:2*size-x;
}
inline Color Sample(const std::vector<Color>& image,unsigned w,unsigned h,float x,float y) {
    x=std::clamp(x,0.f,float(w-1));y=std::clamp(y,0.f,float(h-1));
    const unsigned ix=unsigned(x),iy=unsigned(y),nx=std::min(ix+1,w-1),ny=std::min(iy+1,h-1);
    Color c;
    for(unsigned k=0;k<4;++k)c[k]=std::lerp(std::lerp(image[iy*w+ix][k],image[iy*w+nx][k],x-ix),
        std::lerp(image[ny*w+ix][k],image[ny*w+nx][k],x-ix),y-iy);
    return c;
}
struct Paper {
    unsigned width=0,height=0;
    std::vector<Color> original,soft;
    Rect grid{};
    void Set(std::vector<Color> image,unsigned w,unsigned h) {
        original=std::move(image);width=w;height=h;soft.resize(w*h);
        // Separate broad parchment color from the fine grid/texture. One shared
        // fine layer prevents independently positioned papers doubling the ink.
        // Integral-image filtering is cached with the native texture contents.
        std::vector<Color> sum((w+1)*(h+1));
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<4;++c)
            sum[(y+1)*(w+1)+x+1][c]=original[y*w+x][c]+sum[y*(w+1)+x+1][c]+
                sum[(y+1)*(w+1)+x][c]-sum[y*(w+1)+x][c];
        constexpr unsigned radius=12;
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
            const unsigned l=x>radius?x-radius:0,t=y>radius?y-radius:0;
            const unsigned r=std::min(x+radius+1,w),b=std::min(y+radius+1,h);
            for(unsigned c=0;c<4;++c)soft[y*w+x][c]=(sum[b*(w+1)+r][c]-sum[t*(w+1)+r][c]-
                sum[b*(w+1)+l][c]+sum[t*(w+1)+l][c])/float((r-l)*(b-t));
        }
        // Extend from an interior grid line, not the baked dark outside rim.
        // The lowest residual profile locates that line in the live texture;
        // no copied texture or hardcoded paper colors are needed.
        const auto edge=[&](bool horizontal,bool far) {
            const unsigned n=horizontal?w:h,across=horizontal?h:w;
            const unsigned first=far?n-std::min(32u,n/3):4,last=far?n-5:std::min(32u,n/3);
            float best=1e9f;unsigned at=first;
            for(unsigned k=first;k<=last;++k) {
                float score=0;
                for(unsigned t=across/4;t<across*3/4;++t) {
                    const unsigned index=horizontal?t*w+k:k*w+t;
                    for(unsigned c=0;c<3;++c)score+=original[index][c]-soft[index][c];
                }
                if(score<best){best=score;at=k;}
            }
            return float(at);
        };
        grid={edge(true,false),edge(false,false),edge(true,true),edge(false,true)};
    }
    Color At(float x,float y,std::span<const Rect> pieces,float blend) const {
        Color color{};float weights=0,alpha=0;
        for(const auto& b:pieces) {
            if(x<b.left || x>b.right || y<b.top || y>b.bottom)continue;
            const float u=(x-b.left)/(b.right-b.left)*(width-1),v=(y-b.top)/(b.bottom-b.top)*(height-1);
            const auto broad=Sample(soft,width,height,u,v),native=Sample(original,width,height,u,v);
            const float depth=std::min({x-b.left,b.right-x,y-b.top,b.bottom-y});
            const float t=blend>0?std::clamp(depth/blend,0.f,1.f):1;
            const float weight=std::max(.00001f,t*t*(3-2*t))*native[3];
            for(unsigned c=0;c<3;++c)color[c]+=weight*broad[c];
            weights+=weight;alpha=std::max(alpha,native[3]);
        }
        if(weights<=0)return {};
        const auto& base=pieces[0];
        const float u=grid.left+Mirror((x-base.left)/(base.right-base.left)*(width-1)-grid.left,grid.right-grid.left);
        const float v=grid.top+Mirror((y-base.top)/(base.bottom-base.top)*(height-1)-grid.top,grid.bottom-grid.top);
        const auto fine=Sample(original,width,height,u,v),broad=Sample(soft,width,height,u,v);
        for(unsigned c=0;c<3;++c)color[c]=std::clamp(color[c]/weights+fine[c]-broad[c],0.f,255.f);
        color[3]=alpha;return color;
    }
};
inline Rect Union(std::span<const Rect> pieces) {
    auto b=pieces[0];for(const auto& p:pieces) {
        b.left=std::min(b.left,p.left);b.top=std::min(b.top,p.top);
        b.right=std::max(b.right,p.right);b.bottom=std::max(b.bottom,p.bottom);
    }return b;
}
constexpr unsigned TiledOffset(unsigned x,unsigned y) {
    unsigned morton=0;
    for(unsigned bit=0;bit<3;++bit)morton|=((x>>bit)&1)<<(2*bit)|((y>>bit)&1)<<(2*bit+1);
    return ((y/8)*(Width/8)+x/8)*64+morton;
}
// Sampling coordinates are separable: calculate interpolation indices and
// edge distances once per column/row, rather than for every output pixel.
struct AxisSample {
    unsigned lo,hi;float fraction;
    static AxisSample Make(float p,unsigned n) {
        p=std::clamp(p,0.f,float(n-1));const unsigned lo=unsigned(p);
        return {lo,std::min(lo+1,n-1),p-lo};
    }
};
inline Color SamplePrepared(const std::vector<Color>& image,unsigned width,AxisSample x,AxisSample y) {
    Color c;
    for(unsigned k=0;k<4;++k)c[k]=std::lerp(
        std::lerp(image[y.lo*width+x.lo][k],image[y.lo*width+x.hi][k],x.fraction),
        std::lerp(image[y.hi*width+x.lo][k],image[y.hi*width+x.hi][k],x.fraction),y.fraction);
    return c;
}
inline std::shared_ptr<Pixels> Compose(const Paper& paper,std::span<const Rect> pieces,float blend,Rect bounds) {
    struct Axis {AxisSample sample;float depth;bool inside;};
    std::vector<std::array<Axis,Width>> columns(pieces.size());
    std::vector<std::array<Axis,Height>> rows(pieces.size());
    std::array<AxisSample,Width> fine_x;
    std::array<AxisSample,Height> fine_y;
    for(unsigned i=0;i<pieces.size();++i) {
        const auto& b=pieces[i];
        for(unsigned x=0;x<Width;++x) {
            const float p=std::lerp(bounds.left,bounds.right,(x+.5f)/Width);
            columns[i][x]={AxisSample::Make((p-b.left)/(b.right-b.left)*(paper.width-1),paper.width),
                std::min(p-b.left,b.right-p),p>=b.left && p<=b.right};
            if(i==0)fine_x[x]=AxisSample::Make(paper.grid.left+Mirror(
                (p-b.left)/(b.right-b.left)*(paper.width-1)-paper.grid.left,
                paper.grid.right-paper.grid.left),paper.width);
        }
        for(unsigned y=0;y<Height;++y) {
            const float p=std::lerp(bounds.top,bounds.bottom,(y+.5f)/Height);
            rows[i][y]={AxisSample::Make((p-b.top)/(b.bottom-b.top)*(paper.height-1),paper.height),
                std::min(p-b.top,b.bottom-p),p>=b.top && p<=b.bottom};
            if(i==0)fine_y[y]=AxisSample::Make(paper.grid.top+Mirror(
                (p-b.top)/(b.bottom-b.top)*(paper.height-1)-paper.grid.top,
                paper.grid.bottom-paper.grid.top),paper.height);
        }
    }
    auto out=std::make_shared<Pixels>();
    for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x) {
        Color c{};float weights=0,alpha=0;
        for(unsigned i=0;i<pieces.size();++i) {
            const auto& cx=columns[i][x];const auto& cy=rows[i][y];
            if(!cx.inside || !cy.inside)continue;
            const auto broad=SamplePrepared(paper.soft,paper.width,cx.sample,cy.sample);
            const auto native=SamplePrepared(paper.original,paper.width,cx.sample,cy.sample);
            const float depth=std::min(cx.depth,cy.depth);
            const float t=blend>0?std::clamp(depth/blend,0.f,1.f):1;
            const float weight=std::max(.00001f,t*t*(3-2*t))*native[3];
            for(unsigned k=0;k<3;++k)c[k]+=weight*broad[k];
            weights+=weight;alpha=std::max(alpha,native[3]);
        }
        if(weights>0) {
            const auto fine=SamplePrepared(paper.original,paper.width,fine_x[x],fine_y[y]);
            const auto broad=SamplePrepared(paper.soft,paper.width,fine_x[x],fine_y[y]);
            for(unsigned k=0;k<3;++k)c[k]=std::clamp(c[k]/weights+fine[k]-broad[k],0.f,255.f);
            c[3]=alpha;
        }
        U32 pixel=0;for(unsigned k=0;k<4;++k)pixel|=U32(std::clamp(std::lround(c[k]),0l,255l))<<(24-8*k);
        (*out)[TiledOffset(x,Height-1-y)]=pixel;
    }
    return out;
}
struct Recipe {
    std::array<Rect,4> boxes{};
    unsigned count=0;
    std::array<float,17> key{};
    Recipe(const PauseMenu::Layout& layout,Rect raw) {
        const auto add=[&](const PauseMenu::Pose& pose) {
            const auto a=PauseMenu::Move(raw.left,raw.top,PauseMenu::DungeonMap,pose);
            const auto b=PauseMenu::Move(raw.right,raw.bottom,PauseMenu::DungeonMap,pose);
            boxes[count++]={a[0],a[1],b[0],b[1]};
        };
        add(layout[PauseMenu::DungeonMap]);for(const auto& extra:layout.dungeon_parchments)if(extra.enabled && PauseMenu::Valid(extra.pose))add(extra.pose);
        for(unsigned i=0;i<count;++i) {
            key[4*i]=boxes[i].left;key[4*i+1]=boxes[i].top;key[4*i+2]=boxes[i].right;key[4*i+3]=boxes[i].bottom;
        }
        key[16]=std::isfinite(layout.parchment_blend)?std::clamp(layout.parchment_blend,0.f,80.f):18;
    }
    Rect Bounds() const {
        auto b=Union({boxes.data(),count});
        b.left-=1;b.top-=1;b.right+=1;b.bottom+=1;
        return b;
    }
    std::shared_ptr<Pixels> Render(const Paper& paper) const {
        return Compose(paper,{boxes.data(),count},key[16]/PauseMenu::Scale,Bounds());
    }
};
struct Warm {
    std::array<float,17> key;
    std::shared_ptr<const std::vector<U32>> atlas;
    std::shared_ptr<Pixels> pixels;
};
inline Common::AtomicSharedPtr<const Warm> warm;
using Clock=std::chrono::steady_clock;
inline double Milliseconds(Clock::time_point start) {
    return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
}
struct Timings {
    double read_ms=0,decode_ms=0,compose_ms=0,observe_ms=0,upload_ms=0;
    unsigned observations=0,rebuilds=0,uploads=0;
};
struct Measure {
    double& total;Clock::time_point start=Clock::now();
    ~Measure(){total+=Milliseconds(start);}
};

struct Hook : GuestUI::Hook<Geometry,Width,Height> {
    Timings timings;
    bool ready=false,dirty=false;
    U32 native_model=0;
    std::vector<U32> compressed,readback;
    std::array<float,8> cached_uv{};
    std::array<float,17> cached_key{};
    Paper paper;
    Rect bounds{};
    std::shared_ptr<Pixels> composed;
    Geometry prepared{};
    template<class M,class Decode> void Observe(M& m,U32 owner,Decode decode) {
        ready=false;
        const auto layout=PauseMenu::GetLayout();
        if(!Enabled(layout) || !PauseMenu::DungeonPage(m) || m.Read(0x506cb0+0xc)!=owner)return;
        const U32 mesh=m.Read(owner+4).value_or(0),desc=m.Read(mesh).value_or(0);
        if(PauseMenu::DungeonQuadPart(m,mesh,0,50)!=PauseMenu::DungeonMap ||
           m.Read(desc+12)!=200 || m.Read(mesh+0x15c)!=(512u|(256u<<16)) || m.Read(mesh+0x164)!=13)return;
        ++timings.observations;Measure observation{timings.observe_ms};
        const U32 current=m.Read(mesh+0x124).value_or(2);
        const U32 live=current<2?m.Read(mesh+0x1a0+current*4).value_or(0):0;
        const U32 address=m.Read(mesh+0x158).value_or(0);
        std::array<float,12> p;std::array<float,8> uv;std::array<float,16> color;
        if(address<0x14000000 || address>0x1c000000-131072 ||
           !TitleScreen::ReadFloats(m,live,p) || !TitleScreen::ReadFloats(m,live+200*12,uv) ||
           !TitleScreen::ReadFloats(m,live+200*20,color))return;
        const auto first=layout[PauseMenu::DungeonMap];if(!PauseMenu::Valid(first))return;
        Rect raw{1e9f,1e9f,-1e9f,-1e9f};
        const float motion=PauseMenu::motion_x.load()/PauseMenu::Scale;
        for(unsigned i=0;i<4;++i) {
            const float x=160+(p[3*i]-160-first.x/PauseMenu::Scale-motion)/first.scale;
            const float y=120+(p[3*i+1]-120-first.y/PauseMenu::Scale)/first.scale;
            raw.left=std::min(raw.left,x);raw.right=std::max(raw.right,x);
            raw.top=std::min(raw.top,y);raw.bottom=std::max(raw.bottom,y);
        }
        if(raw.right-raw.left<1 || raw.bottom-raw.top<1)return;
        // Round subpixel reconstruction noise, so pause entrance never rebuilds
        // a half-million-pixel texture merely because its slide position changed.
        const auto round=[](float f){return std::round(f*1024)/1024;};
        raw={round(raw.left),round(raw.top),round(raw.right),round(raw.bottom)};
        const Recipe recipe(layout,raw);
        const auto& key=recipe.key;
        const auto read_start=Clock::now();
        readback.resize(131072/4);
        if(!GuestUI::ReadTextureWords(m,address,readback))return;
        timings.read_ms+=Milliseconds(read_start);
        const bool texture_changed=compressed!=readback || cached_uv!=uv;
        if(!composed || texture_changed || key!=cached_key) {
            const auto cached=warm.load();
            float u0=1,u1=0,v0=1,v1=0;
            for(unsigned i=0;i<4;++i){u0=std::min(u0,uv[2*i]);u1=std::max(u1,uv[2*i]);v0=std::min(v0,uv[2*i+1]);v1=std::max(v1,uv[2*i+1]);}
            const int left=std::lround(u0*512),right=std::lround(u1*512),top=std::lround((1-v1)*256),bottom=std::lround((1-v0)*256);
            if(left<0 || right>512 || top<0 || bottom>256 || right-left<16 || bottom-top<16)return;
            const bool native_crop=left==256 && right==464 && top==0 && bottom==176;
            if(cached && cached->key==key && native_crop && *cached->atlas==readback) {
                composed=cached->pixels;
                if(texture_changed)paper={};
            } else {
                if(texture_changed || paper.original.empty()) {
                    Measure decode_time{timings.decode_ms};
                    const unsigned w=right-left,h=bottom-top;std::vector<Color> source(w*h);
                    const auto word64=[&](unsigned at){return std::uint64_t(readback[at/4])|(std::uint64_t(readback[at/4+1])<<32);};
                    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
                        const unsigned tx=left+x,ty=top+y,at=((ty/8)*64+tx/8)*64+((ty%8)/4*2+(tx%8)/4)*16;
                        const auto rgb=decode(word64(at+8),tx%4,ty%4);
                        source[y*w+x]={float(rgb[0]),float(rgb[1]),float(rgb[2]),float(((word64(at)>>(4*(4*(tx%4)+ty%4)))&15)*17)};
                    }
                    paper.Set(std::move(source),w,h);
                }
                ++timings.rebuilds;Measure compose_time{timings.compose_ms};
                composed=recipe.Render(paper);
            }
            if(texture_changed){compressed.swap(readback);cached_uv=uv;}
            bounds=recipe.Bounds();
            cached_key=key;dirty=true;
        }
        prepared.positions={bounds.left+motion,bounds.top,p[2],bounds.right+motion,bounds.top,p[2],
            bounds.left+motion,bounds.bottom,p[2],bounds.right+motion,bounds.bottom,p[2]};
        prepared.uvs={0,0,1,0,0,1,1,1};prepared.colors=color;prepared.count=1;
        native_model=owner;ready=true;
    }
    template<class M,class R> void Resume(M& m,R& r) {
        const bool uploading=stage==GuestUI::Hook<Geometry,Width,Height>::Stage::TextureAllocate;
        const auto upload_start=Clock::now();
        GuestUI::Hook<Geometry,Width,Height>::Resume(m,r);
        if(uploading){++timings.uploads;timings.upload_ms+=Milliseconds(upload_start);}
        if(Drawing()) {
            // Generated paper shares the native atlas transform/material fade.
            // Only the sampler differs from the borrowed stone/label layers.
            for(U32 at=0xc;at<=0x38;at+=4)m.Write(model+at,m.Read(source+at).value_or(0));
            for(U32 at=0xf0;at<=0xfc;at+=4)m.Write(model+at,m.Read(source+at).value_or(0));
        }
    }
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc) {
        if(!ready || !composed)return;
        if(dirty && Owned(m)) {
            ++timings.uploads;Measure upload_time{timings.upload_ms};
            if(!GuestUI::WriteTextureWords(m,texture,*composed))return;
        }
        dirty=false;GuestUI::Hook<Geometry,Width,Height>::Begin(m,r,return_pc,prepared,*composed,native_model);
    }
};
}
