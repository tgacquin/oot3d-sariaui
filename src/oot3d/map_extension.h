// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_shadow.h"
#include "oot3d/map_frame_asset.h"

namespace OOT3D::MapExtension {
using TitleScreen::U32;
constexpr unsigned Width=512,Height=256,Side=256;
using Pixels=std::array<U32,Width*Height>;
struct Crop { int left=0,top=0,right=320,bottom=240; bool operator==(const Crop&) const=default; };
inline bool Valid(Crop c) {
    return c.left>=0 && c.top>=0 && c.right<=320 && c.bottom<=240 &&
        c.right-c.left>=100 && c.bottom-c.top>=100;
}
// Continue the pale paper at the edge, without mirroring inland terrain into it.
// Fractional summed-area sampling preserves the join; dark ink fades into a
// paper color estimated from the brighter edge pixels. No external asset is used.
inline std::shared_ptr<Pixels> Extend(const std::vector<U32>& source,unsigned width,unsigned height,float blur,float darkness) {
    // Construct the 512 KiB texture directly on the heap. Returning Pixels by
    // value into make_shared reserved that entire temporary on the emulator's
    // small thread stack, even on Observe's early-out paths.
    auto result=std::make_shared<Pixels>();
    auto& out=*result;
    if(width<2 || height<2 || source.size()!=width*height || width>320 || height>240)return result;
    blur=std::isfinite(blur)?std::clamp(blur,0.f,40.f):18;
    darkness=std::isfinite(darkness)?std::clamp(darkness,0.f,1.f):0;
    using Color=std::array<double,4>;
    const unsigned strip=std::min(6u,width);
    const auto luminance=[](U32 p) {
        return .2126*((p>>24)&255)+.7152*((p>>16)&255)+.0722*((p>>8)&255);
    };
    std::array<std::vector<Color>,2> paper;
    for(unsigned side=0;side<2;++side) {
        std::vector<double> levels;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<strip;++x) {
            const auto p=source[y*width+(side?width-1-x:x)];
            if((p&255)>=128)levels.push_back(luminance(p));
        }
        // Transparent pixels must not influence the paper estimate.
        if(levels.empty())for(unsigned y=0;y<height;++y)for(unsigned x=0;x<strip;++x) {
            const auto p=source[y*width+(side?width-1-x:x)];
            if(p&255)levels.push_back(luminance(p));
        }
        const auto percentile=levels.size()*3/4;
        if(!levels.empty())std::nth_element(levels.begin(),levels.begin()+percentile,levels.end());
        const double threshold=levels.empty()?0:levels[percentile];
        std::vector<Color> rows(height);
        Color total{};
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<strip;++x) {
            const auto p=source[y*width+(side?width-1-x:x)];
            const double light=std::clamp((luminance(p)-threshold+16)/16.,0.,1.);
            const double weight=light*light*(p&255)/255.;
            rows[y][3]+=weight;total[3]+=weight;
            for(unsigned c=0;c<3;++c) {
                const double value=((p>>(24-8*c))&255)*weight;
                rows[y][c]+=value;total[c]+=value;
            }
        }
        paper[side].resize(height);
        const double sigma=12+blur;
        for(unsigned y=0;y<height;++y) {
            // A small global contribution also fills rows made entirely of dark
            // border, instead of spreading the brown lower edge across the fill.
            Color sum{};
            for(unsigned c=0;c<4;++c)sum[c]=total[c]*.08;
            for(unsigned yy=0;yy<height;++yy) {
                const double distance=(double(yy)-y)/sigma;
                const double weight=std::exp(-.5*distance*distance);
                for(unsigned c=0;c<4;++c)sum[c]+=rows[yy][c]*weight;
            }
            if(sum[3]>0)for(unsigned c=0;c<3;++c)paper[side][y][c]=sum[c]/sum[3];
        }
    }
    std::vector<Color> sums((width+1)*(height+1));
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const U32 p=source[y*width+x];const double alpha=(p&255)/255.;
        for(unsigned c=0;c<4;++c) {
            const double value=c==3?(p&255):((p>>(24-8*c))&255)*alpha;
            sums[(y+1)*(width+1)+x+1][c]=value+sums[y*(width+1)+x+1][c]+
                sums[(y+1)*(width+1)+x][c]-sums[y*(width+1)+x][c];
        }
    }
    const auto integral=[&](float x,float y,unsigned c) {
        x=std::clamp(x,0.f,float(width));y=std::clamp(y,0.f,float(height));
        const unsigned ix=std::min(unsigned(x),width-1),iy=std::min(unsigned(y),height-1);
        const float fx=x-ix,fy=y-iy;
        return std::lerp(std::lerp(sums[iy*(width+1)+ix][c],sums[iy*(width+1)+ix+1][c],fx),
            std::lerp(sums[(iy+1)*(width+1)+ix][c],sums[(iy+1)*(width+1)+ix+1][c],fx),fy);
    };
    for(unsigned side=0;side<2;++side)for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Side;++x) {
        const float t=float(x)/(Side-1),smooth=t*t*(3-2*t);
        const float sx=side?width-1-3*t:3*t;
        const float sy=float(y)*(height-1)/(Height-1),radius=blur*smooth;
        const float l=std::max(side?float(width-strip):0.f,sx-radius);
        const float r=std::min(side?float(width):float(strip),sx+radius+1);
        const float top=std::max(0.f,sy-radius),bottom=std::min(float(height),sy+radius+1);
        Color color{};
        for(unsigned c=0;c<4;++c)color[c]=(integral(r,bottom,c)-integral(l,bottom,c)-
            integral(r,top,c)+integral(l,top,c))/((r-l)*(bottom-top));
        U32 pixel=U32(std::clamp(std::lround(color[3]),0l,255l));
        for(unsigned c=0;c<3;++c) {
            const unsigned row=std::min(unsigned(sy),height-2);
            const double base=std::lerp(paper[side][row][c],paper[side][row+1][c],sy-row);
            // Keep the exact seam, then remove dark features over a short join.
            const double blend=1-std::pow(1-double(t),12);
            const double value=color[3]>0?std::lerp(color[c]*255/color[3],base,blend)*(1-darkness*smooth):0;
            pixel|=U32(std::clamp(std::lround(value),0l,255l))<<(24-8*c);
        }
        out[MapFrame::TiledOffset(side*Side+x,Height-1-y)]=pixel;
    }
    return result;
}
struct Geometry {
    std::array<float,24> positions{};
    std::array<float,16> uvs{};
    std::array<float,32> colors{};
    unsigned count=0;
};
inline Geometry Make(PauseShadow::Rect b,const PauseMenu::MapExtension& settings,float alpha=1,float darkness=0) {
    Geometry g;
    if(!settings.enabled || !std::isfinite(settings.width) || settings.width<=0 || settings.width>400 ||
       b.right<=b.left || b.bottom<=b.top || alpha<=0)return g;
    for(unsigned side=0;side<2;++side) {
        const float left=side?b.right:b.left-settings.width,right=side?b.right+settings.width:b.left;
        const std::array<float,12> p{left,b.top,0,right,b.top,0,left,b.bottom,0,right,b.bottom,0};
        // Texel centers prevent sampling the other extension at the atlas seam.
        const float near=(side*Side+.5f)/Width,far=(side*Side+Side-.5f)/Width;
        const float u0=side?near:far,u1=side?far:near;
        const std::array<float,8> uv{u0,.5f/Height,u1,.5f/Height,u0,1-.5f/Height,u1,1-.5f/Height};
        std::copy(p.begin(),p.end(),g.positions.begin()+side*12);
        std::copy(uv.begin(),uv.end(),g.uvs.begin()+side*8);
    }
    for(unsigned i=0;i<32;++i)g.colors[i]=i%4==3?std::clamp(alpha,0.f,1.f):PauseMenu::MapBrightness(darkness);
    g.count=2;return g;
}
struct Hook : GuestUI::Hook<Geometry,Width,Height> {
    static constexpr U32 World=0x506cb0,Quads=30;
    std::optional<PauseShadow::Rect> bounds;
    float alpha=1,cached_blur=-1,cached_darkness=-1;
    Crop cached_crop{};
    std::vector<U32> compressed,readback,decoded;
    std::shared_ptr<Pixels> extension;
    bool dirty=false;
    void ResetFrame() {bounds.reset();}
    template<class M,class Decode> void Observe(M& m,U32 native_model,Decode decode) {
        const auto settings=PauseMenu::GetLayout().map_extension;
        // Verified world-map controller and its 30-quad atlas (0x480bb8).
        // The sibling model at +0xc is the dungeon page. Dungeon maps,
        // gameplay minimaps, description models and marker models are excluded.
        if(!settings.enabled || !PauseMenu::MapPage(m) ||
           m.Read(World+4)!=std::optional<U32>{native_model} || (!m.Read(World+0x38).value_or(0) && !PauseMenu::world_exit_hold) ||
           m.Read(0x4fda6c+0x50).value_or(1)==1)return;
        const U32 mesh=m.Read(native_model+4).value_or(0),helper=m.Read(World+0x10).value_or(0);
        if(m.Read(native_model)!=std::optional<U32>{0x4ebd60} || m.Read(World)!=std::optional<U32>{mesh} ||
           m.Read(helper)!=std::optional<U32>{Quads} || m.Read(mesh+0x15c)!=std::optional<U32>{512|(256u<<16)})return;
        const U32 desc=m.Read(mesh).value_or(0),count=m.Read(desc+12).value_or(0);
        const U32 current=m.Read(mesh+0x124).value_or(2);
        const U32 live=current<2?m.Read(mesh+0x1a0+4*current).value_or(0):0;
        const U32 address=m.Read(mesh+0x158).value_or(0);
        if(count!=Quads*4 || !TitleScreen::Pointer(live,count*36) ||
           address<0x14000000 || address>0x1c000000-131072)return;
        Crop crop;std::optional<PauseShadow::Rect> box;float best=0,fade=0;
        for(unsigned q=0;q<Quads;++q) {
            std::array<float,8> uv{};std::array<float,16> color{};
            if(!TitleScreen::ReadFloats(m,live+count*12+q*32,uv))return;
            float u0=1,u1=0,v0=1,v1=0;
            for(unsigned i=0;i<8;i+=2) {u0=std::min(u0,uv[i]);u1=std::max(u1,uv[i]);v0=std::min(v0,uv[i+1]);v1=std::max(v1,uv[i+1]);}
            if(u0<0 || u1>320.f/512+.002f || v0<16.f/256-.002f || v1>1.002f)continue;
            Crop c{int(std::lround(u0*512)),int(std::lround((1-v1)*256)),
                int(std::lround(u1*512)),int(std::lround((1-v0)*256))};
            if(!Valid(c))continue;
            const auto b=PauseShadow::Bounds(m,live+q*48,4,native_model);if(!b)continue;
            const float area=(b->right-b->left)*(b->bottom-b->top);
            if(area<=best || b->right<=b->left || b->bottom<=b->top)continue;
            if(!TitleScreen::ReadFloats(m,live+count*20+q*64,color))return;
            fade=1;for(unsigned i=3;i<16;i+=4)fade=std::min(fade,std::clamp(color[i],0.f,1.f));
            best=area;box=PauseShadow::ToTop(*b,{});crop=c;
        }
        if(!box || fade<=0)return;
        // Compare the current guest data, not just its address: save states and
        // language changes can reuse a texture allocation with different pixels.
        readback.resize(131072/4);
        for(unsigned i=0;i<readback.size();++i) {auto word=m.Read(address+4*i);if(!word)return;readback[i]=*word;}
        if(!extension || crop!=cached_crop || readback!=compressed) {
            decoded.resize((crop.right-crop.left)*(crop.bottom-crop.top));
            const auto word64=[&](unsigned offset) {return std::uint64_t(readback[offset/4])|(std::uint64_t(readback[offset/4+1])<<32);};
            for(int y=crop.top;y<crop.bottom;++y)for(int x=crop.left;x<crop.right;++x) {
                const unsigned at=((y/8)*64+x/8)*64+((y%8)/4*2+(x%8)/4)*16;
                const auto rgb=decode(word64(at+8),x%4,y%4);
                const U32 a=((word64(at)>>(4*(4*(x%4)+y%4)))&15)*17;
                decoded[(y-crop.top)*(crop.right-crop.left)+x-crop.left]=
                    (U32(rgb[0])<<24)|(U32(rgb[1])<<16)|(U32(rgb[2])<<8)|a;
            }
            compressed=readback;cached_crop=crop;dirty=true;
        }
        if(dirty || settings.blur!=cached_blur || settings.darkness!=cached_darkness) {
            extension=Extend(decoded,crop.right-crop.left,crop.bottom-crop.top,settings.blur,settings.darkness);
            cached_blur=settings.blur;cached_darkness=settings.darkness;dirty=true;
        }
        bounds=box;alpha=fade;
    }
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc) {
        if(!bounds || !extension || !PauseMenu::MapPage(m) || PauseMenu::DungeonPage(m))return;
        if(dirty && Owned(m)) {
            for(unsigned i=0;i<extension->size();++i)if(!m.Read(texture+4*i))return;
            for(unsigned i=0;i<extension->size();++i)m.Write(texture+4*i,(*extension)[i]);
        }
        dirty=false;
        const auto layout=PauseMenu::GetLayout();
        auto geometry=Make(*bounds,layout.map_extension,alpha,layout.map_darkness);
        const auto tint=MapTimeTint::Read(m,layout.world_map_time_tint);
        for(unsigned i=0;i<geometry.colors.size();++i)if(i%4!=3)geometry.colors[i]*=tint[i%4];
        GuestUI::Hook<Geometry,Width,Height>::Begin(m,r,return_pc,
            geometry,*extension,m.Read(World+4).value_or(0));
    }
};
}
