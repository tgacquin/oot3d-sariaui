// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/world_map.h"

namespace OOT3D::WorldMap {
// The native world-map controller owns a separate 16-quad cloud model at
// +0x28 (constructor 002d2190, texture slot 8). Read its live geometry at its
// own draw, after native discovery visibility and pause transforms are applied.
// Never alter the cloud atlas, discovery flags, or the marker models.
struct Clouds {
    ImageLayer image;
    Geometry prepared;
    std::shared_ptr<Pixels> composed;
    LightingCache lighting;
    std::shared_ptr<const Outline> cached_outline;
    std::vector<U32> compressed,bytes;
    std::vector<Color> atlas;
    std::vector<float> cached_key;
    U32 native_model=0;
    bool dirty=false;
    bool Drawing() const {return image.Drawing();}
    bool Pending() const {return image.Pending();}
    struct Quad {
        std::array<float,12> p;
        std::array<float,8> uv;
        std::array<float,16> color;
    };
    template<class M,class Decode> bool Prepare(M& m,U32 model,Decode decode) {
        constexpr U32 world=0x506cb0,quads=16,vertices=64;
        if(!PauseMenu::MapPage(m) || PauseMenu::DungeonPage(m) ||
           (!m.Read(world+0x38).value_or(0) && !PauseMenu::world_exit_hold))return false;
        const U32 owner=m.Read(world+0x28).value_or(0);
        if(!TitleScreen::Pointer(owner,16) || m.Read(owner+4)!=model || m.Read(model)!=0x4ebd60)return false;
        const auto layout=PauseMenu::GetLayout();
        const auto outline=layout.world_map_outline?GetOutline():nullptr;
        if(!outline && !layout.world_map_time_tint && layout.map_darkness==0 &&
           !layout.world_map_top_fade.enabled)return false;
        const U32 mesh=m.Read(model+4).value_or(0),helper=m.Read(owner+8).value_or(0);
        if(!TitleScreen::Pointer(mesh,0x1b8) || m.Read(owner)!=mesh || !TitleScreen::Pointer(helper,0x20))return false;
        const U32 desc=m.Read(mesh).value_or(0);
        const U32 flags=m.Read(desc+0x1c).value_or(0);
        const U32 current=m.Read(mesh+0x124).value_or(2);
        if(m.Read(helper)!=quads || m.Read(desc+12)!=vertices ||
           !(flags&0x80) || (flags&0x10) ||
           m.Read(mesh+0x15c)!=(256|(256u<<16)) || m.Read(mesh+0x164)!=13 || current>=2)return false;
        // Native descriptor 004d2330 has flag 8: constant color, NO color
        // stream. Reading beyond its position/UV buffer samples unrelated heap
        // data and would invalidate the cloud texture cache every frame.
        const bool constant_color=(flags&8)!=0;
        const U32 live=m.Read(mesh+0x1a0+current*4).value_or(0),address=m.Read(mesh+0x158).value_or(0);
        if(!TitleScreen::Pointer(live,vertices*(constant_color?20:36)) || address<0x14000000 || address>0x1c000000-65536)return false;
        const U32 map_model=m.Read(world+4).value_or(0),map_mesh=m.Read(map_model+4).value_or(0);
        const U32 map_desc=m.Read(map_mesh).value_or(0),map_current=m.Read(map_mesh+0x124).value_or(2);
        if(m.Read(world)!=map_mesh || m.Read(map_model)!=0x4ebd60 ||
           m.Read(m.Read(world+0x10).value_or(0))!=30 || m.Read(map_desc+12)!=120 || map_current>=2)return false;
        const U32 map_live=m.Read(map_mesh+0x1a0+4*map_current).value_or(0);
        std::array<float,12> map_matrix,cloud_matrix;
        if(!TitleScreen::ReadFloats(m,map_model+0xc,map_matrix) ||
           !TitleScreen::ReadFloats(m,model+0xc,cloud_matrix))return false;
        const float determinant=cloud_matrix[0]*cloud_matrix[5]-cloud_matrix[1]*cloud_matrix[4];
        if(std::abs(determinant)<1e-6f)return false;
        Rect map{};MapExtension::Crop crop;unsigned found=0;bool reflected=false;
        for(unsigned q=0;q<30;++q) {
            std::array<float,8> uv;
            if(!TitleScreen::ReadFloats(m,map_live+120*12+q*32,uv))return false;
            const float u0=std::min({uv[0],uv[2],uv[4],uv[6]}),u1=std::max({uv[0],uv[2],uv[4],uv[6]});
            const float v0=std::min({uv[1],uv[3],uv[5],uv[7]}),v1=std::max({uv[1],uv[3],uv[5],uv[7]});
            MapExtension::Crop c{int(std::lround(u0*512)),int(std::lround((1-v1)*256)),
                int(std::lround(u1*512)),int(std::lround((1-v0)*256))};
            if(!MapExtension::Valid(c))continue;
            if(++found!=1)return false;
            std::array<float,12> p;
            if(!TitleScreen::ReadFloats(m,map_live+q*48,p))return false;
            // Express the map bounds in the cloud model's local coordinates.
            for(unsigned i=0;i<4;++i) {
                const float x=map_matrix[0]*p[3*i]+map_matrix[1]*p[3*i+1]+map_matrix[2]*p[3*i+2]+map_matrix[3]-cloud_matrix[3];
                const float y=map_matrix[4]*p[3*i]+map_matrix[5]*p[3*i+1]+map_matrix[6]*p[3*i+2]+map_matrix[7]-cloud_matrix[7];
                p[3*i]=(cloud_matrix[5]*x-cloud_matrix[1]*y)/determinant;
                p[3*i+1]=(cloud_matrix[0]*y-cloud_matrix[4]*x)/determinant;
            }
            map={std::min({p[0],p[3],p[6],p[9]}),std::min({p[1],p[4],p[7],p[10]}),
                 std::max({p[0],p[3],p[6],p[9]}),std::max({p[1],p[4],p[7],p[10]})};
            if(map.right-map.left<1 || map.bottom-map.top<1)return false;
            bool normal=true,mirror=true;
            for(unsigned i=0;i<4;++i) {
                const float tx=(uv[2*i]-u0)/(u1-u0);
                const bool vertical=std::abs(p[3*i+1]-std::lerp(map.top,map.bottom,(v1-uv[2*i+1])/(v1-v0)))<=.05f;
                normal&=vertical && std::abs(p[3*i]-std::lerp(map.left,map.right,tx))<=.05f;
                mirror&=vertical && std::abs(p[3*i]-std::lerp(map.right,map.left,tx))<=.05f;
            }
            if(!normal && !mirror)return false;
            reflected=!normal && mirror;
            crop=c;
        }
        if(found!=1)return false;
        std::array<Quad,quads> cloud;
        float alpha=0,z=0;
        for(unsigned q=0;q<quads;++q) {
            auto& v=cloud[q];
            if(!TitleScreen::ReadFloats(m,live+q*48,v.p) ||
               !TitleScreen::ReadFloats(m,live+vertices*12+q*32,v.uv))return false;
            if(constant_color)v.color.fill(1); // Native material tint/fade is copied at draw time.
            else if(!TitleScreen::ReadFloats(m,live+vertices*20+q*64,v.color))return false;
            // Native board quads are TL,TR,BL,BR. Collapsed/parked clouds
            // remain absent; UV orientation (including flips) is preserved.
            if(std::abs(v.p[1]-v.p[4])>.05f || std::abs(v.p[7]-v.p[10])>.05f ||
               std::abs(v.p[0]-v.p[6])>.05f || std::abs(v.p[3]-v.p[9])>.05f)return false;
            if(v.p[3]<=v.p[0] || v.p[7]<=v.p[1] || v.p[3]<=map.left ||
               v.p[0]>=map.right || v.p[7]<=map.top || v.p[1]>=map.bottom) {
                v={};continue;
            }
            for(unsigned i=0;i<4;++i) {
                alpha=std::max(alpha,std::clamp(v.color[4*i+3],0.f,1.f));
                v.p[3*i]=std::round((v.p[3*i]-map.left)/(map.right-map.left)*4096)/4096;
                v.p[3*i+1]=std::round((v.p[3*i+1]-map.top)/(map.bottom-map.top)*4096)/4096;
            }
            z=v.p[2];
        }
        bytes.resize(65536/4);
        if(!GuestUI::ReadTextureWords(m,address,bytes))return false;
        const bool changed=bytes!=compressed;
        if(changed || atlas.empty()) {
            atlas.resize(256*256);
            const auto word=[&](unsigned at){return std::uint64_t(bytes[at/4])|(std::uint64_t(bytes[at/4+1])<<32);};
            for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x) {
                const unsigned at=((y/8)*32+x/8)*64+((y%8)/4*2+(x%8)/4)*16;
                const auto c=decode(word(at+8),x%4,y%4);
                atlas[y*256+x]={float(c[0]),float(c[1]),float(c[2]),float(((word(at)>>(4*(4*(x%4)+y%4)))&15)*17)};
            }
            compressed.swap(bytes);
        }
        const auto tint=MapTimeTint::Read(m,layout.world_map_time_tint);
        const float brightness=PauseMenu::MapBrightness(layout.map_darkness);
        const auto fade=layout.world_map_top_fade;
        float fade_top=map.top;
        const auto pieces=GetPaper()?Pieces(layout):std::vector<Rect>{};
        if(!pieces.empty()) {fade_top=pieces[0].top;for(const auto& p:pieces)fade_top=std::min(fade_top,p.top);}
        std::vector<float> key{float(crop.left),float(crop.top),float(crop.right),float(crop.bottom),brightness};
        if(fade.enabled) {
            key.insert(key.end(),{map.top,map.bottom,fade_top,fade.opacity,fade.height,fade.y,fade.softness});
            key.insert(key.end(),fade.color.begin(),fade.color.end());
        }
        for(auto& q:cloud) {
            for(unsigned i=0;i<4;++i)q.color[4*i+3]=alpha>0?q.color[4*i+3]/alpha:0;
            // Draw depth is geometry, not image content.
            for(unsigned i=0;i<4;++i)key.insert(key.end(),{q.p[3*i],q.p[3*i+1]});
            key.insert(key.end(),q.uv.begin(),q.uv.end());
            key.insert(key.end(),q.color.begin(),q.color.end());
        }
        key.push_back(float(reflected));
        const bool rebuild=!composed || changed || cached_key!=key || cached_outline!=outline;
        if(rebuild) {
            constexpr auto width=DungeonParchment::Width,height=DungeonParchment::Height;
            std::vector<Color> raster(width*height);
            lighting.Reset();
            const auto boundary=outline?Boundary(*outline):std::vector<MapFrame::Point>{};
            ComposeRows([&](unsigned first,unsigned last) {
            for(const auto& q:cloud) {
                const float left=q.p[0],right=q.p[3],top=q.p[1],bottom=q.p[7];
                if(right<=left || bottom<=top)continue;
                // The 16 sprites are packed edge-to-edge. Keep bilinear taps
                // inside this sprite, including when its UVs are flipped.
                const auto texels=[&](unsigned axis) {
                    const auto coord=[&](unsigned v) {
                        const float uv=q.uv[2*v+axis];
                        return (axis==0?uv:1-uv)*256;
                    };
                    const auto [lo,hi]=std::minmax({coord(0),coord(1),coord(2),coord(3)});
                    return std::array{std::min(lo,(lo+hi)*.5f-.5f),
                                      std::max(hi-1,(lo+hi)*.5f-.5f)};
                };
                const auto u=texels(0),v=texels(1);
                const int x0=int(std::clamp(std::floor(left*width),0.f,float(width))),x1=int(std::clamp(std::ceil(right*width),0.f,float(width)));
                const int y0=int(std::clamp(std::floor(top*height),0.f,float(height))),y1=int(std::clamp(std::ceil(bottom*height),0.f,float(height)));
                for(int y=std::max(y0,int(first));y<std::min(y1,int(last));++y)for(int x=x0;x<x1;++x) {
                    const float tx=((x+.5f)/width-left)/(right-left),ty=((y+.5f)/height-top)/(bottom-top);
                    if(tx<0 || tx>1 || ty<0 || ty>1)continue;
                    const auto interpolate=[&](const auto& a,unsigned stride,unsigned c) {
                        return std::lerp(std::lerp(a[c],a[stride+c],tx),std::lerp(a[2*stride+c],a[3*stride+c],tx),ty);
                    };
                    auto pixel=DungeonParchment::Sample(atlas,256,256,
                        std::clamp(interpolate(q.uv,2,0)*256-.5f,u[0],u[1]),
                        std::clamp((1-interpolate(q.uv,2,1))*256-.5f,v[0],v[1]));
                    for(unsigned c=0;c<4;++c)pixel[c]*=interpolate(q.color,4,c);
                    raster[y*width+x]=Over(pixel,raster[y*width+x]);
                }
            }
            OutlineRow row;
            for(unsigned y=first;y<last;++y) {
              if(outline)row.Prepare(boundary,outline->feather,
                  std::lerp(float(crop.top),float(crop.bottom),(y+.5f)/height));
              const float shade=TopFade(std::lerp(map.top,map.bottom,(y+.5f)/height),fade_top,fade);
              lighting.Row(y,shade,fade.color);
              for(unsigned x=0;x<width;++x) {
                auto color=raster[y*width+x];
                if(color[3]>0 && outline)color[3]*=row.At(
                    std::lerp(float(crop.left),float(crop.right),reflected?1-(x+.5f)/width:(x+.5f)/width));
                for(unsigned c=0;c<3;++c) {
                    color[c]*=brightness;
                }
                lighting.Store(x,y,color,shade);
              }
            }
            });
            cached_key=std::move(key);cached_outline=outline;
        }
        if(rebuild || tint!=lighting.tint) {composed=lighting.Apply(tint);dirty=true;}
        prepared.positions={map.left,map.top,z,map.right,map.top,z,map.left,map.bottom,z,map.right,map.bottom,z};
        prepared.uvs={0,0,1,0,0,1,1,1};prepared.colors.fill(1);
        for(unsigned i=0;i<4;++i)prepared.colors[4*i+3]=alpha;
        prepared.count=1;native_model=model;return true;
    }
    template<class M,class R,class Decode> bool NativeDraw(M& m,R& r,bool enabled,Decode decode) {
        if(!enabled || Pending() || !Prepare(m,r[0],decode))return false;
        if(dirty && image.Owned(m)) {
            if(!GuestUI::WriteTextureWords(m,image.texture,*composed))return false;
        }
        image.Begin(m,r,PauseMenu::Sites[19].pc,prepared,*composed,native_model);
        if(Pending())dirty=false;
        return Pending();
    }
    template<class M,class R> void Resume(M& m,R& r) {
        const bool drew=Drawing();image.Resume(m,r);
        if(Drawing()) {
            for(U32 at=0xc;at<=0x38;at+=4)m.Write(image.model+at,m.Read(native_model+at).value_or(0));
            for(U32 at=0xf0;at<=0xfc;at+=4)m.Write(image.model+at,m.Read(native_model+at).value_or(0));
        }
        if(!Pending()) {
            if(drew)r[15]=r[14];
            else {r[1]=0x54c8c8;r[15]=0x3f9684;}
        }
    }
};
}
