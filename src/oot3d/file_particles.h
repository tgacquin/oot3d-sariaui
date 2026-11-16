// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/file_particle_settings.h"
#include "oot3d/pause_prompts.h"
#include "oot3d/map_frame_asset.h"
#include <chrono>
#include <string_view>

namespace OOT3D::FileParticles {
constexpr unsigned SpriteSize=64, Width=128, Height=64, MaxParticles=64;
using Pixels=std::array<std::uint32_t,Width*Height>;
template<unsigned Limit> using GeometryFor=PausePrompts::QuadGeometry<Limit*5>;
using Geometry=GeometryFor<MaxParticles>;
inline std::shared_ptr<const Pixels> pixels;
inline auto GetPixels() {return std::atomic_load(&pixels);}

constexpr unsigned Tiled(unsigned x,unsigned y,unsigned width=Width) {
    unsigned morton=0;
    for(unsigned bit=0;bit<3;++bit)
        morton|=((x>>bit)&1)<<(2*bit)|((y>>bit)&1)<<(2*bit+1);
    return ((y/8)*(width/8)+x/8)*64+morton;
}

// Read the native floating-insect sprite from the user's shared actor archive.
// ZAR1 entries and CTXB metadata are bounded before any payload is consumed.
inline std::optional<Pixels> MakePixels(std::span<const std::uint8_t> archive) {
    using MapFrame::LE32;
    if(archive.size()<24 || LE32(archive,0)!=0x0152415a)return {};
    const unsigned count=archive[10]|unsigned(archive[11])<<8;
    const auto files=LE32(archive,16), offsets=LE32(archive,20);
    if(!count || count>4096 || files>archive.size() || offsets>archive.size() ||
       count*8>archive.size()-files || count*4>archive.size()-offsets)return {};
    constexpr std::string_view name="tex/acto_huyuchu1.ctxb";
    for(unsigned i=0;i<count;++i) {
        const auto size=LE32(archive,files+i*8), at=LE32(archive,offsets+i*4),
                   text=LE32(archive,files+i*8+4);
        if(text>=archive.size() || name.size()+1>archive.size()-text)continue;
        if(archive[text+name.size()]!=0 ||
           !std::equal(name.begin(),name.end(),archive.begin()+text))continue;
        if(at>archive.size() || size>archive.size()-at || size!=72+SpriteSize*SpriteSize)return {};
        const auto source=archive.subspan(at,size);
        if(LE32(source,0)!=0x62787463 || LE32(source,4)!=size || LE32(source,8)!=1 ||
           LE32(source,16)!=24 || LE32(source,20)!=72 || LE32(source,24)!=0x20786574 ||
           LE32(source,32)!=1 || LE32(source,36)!=SpriteSize*SpriteSize ||
           LE32(source,44)!=(SpriteSize|(SpriteSize<<16)) || LE32(source,48)!=0x14016757)return {};
        Pixels out{};
        // A two-cell atlas: the native glint, plus a smooth light falloff.
        // White RGB and intensity alpha avoid dark rectangles at sprite edges.
        for(unsigned y=0;y<SpriteSize;++y)for(unsigned x=0;x<SpriteSize;++x) {
            out[Tiled(x,y)]=0xffffff00u|source[72+Tiled(x,y,SpriteSize)];
            const float dx=(x+.5f-32)/31.f,dy=(y+.5f-32)/31.f;
            const float falloff=std::max(0.f,1-dx*dx-dy*dy);
            const unsigned alpha=unsigned(std::lround(255*falloff*falloff*falloff));
            out[Tiled(x+SpriteSize,y)]=0xffffff00u|alpha;
        }
        return out;
    }
    return {};
}
inline float Random(unsigned seed) {
    std::uint32_t x=seed+0x9e3779b9u;
    x=(x^(x>>16))*0x85ebca6bu;x=(x^(x>>13))*0xc2b2ae35u;x^=x>>16;
    return float(x&0xffffff)/16777216.f;
}
inline float Smooth(float v) {v=std::clamp(v,0.f,1.f);return v*v*(3-2*v);}
// Clip the rotated sprite, including UVs, against the top-screen viewport.
// A clipped square has at most eight vertices (three paired-triangle quads).
struct Vertex {float x,y,u,v;};
template<unsigned Capacity>
inline void AddSprite(PausePrompts::QuadGeometry<Capacity>& out,float cx,float cy,float diameter,float aspect,float angle,
                      float alpha,const std::array<float,3>& color,unsigned tile,float width) {
    if(alpha<=0)return;
    const float cs=std::cos(angle),sn=std::sin(angle),half=diameter/2;
    const float u0=(tile*SpriteSize+.5f)/Width,u1=(tile*SpriteSize+63.5f)/Width;
    constexpr float v0=.5f/Height,v1=63.5f/Height;
    std::array<Vertex,12> polygon{},scratch{};
    unsigned count=4;
    constexpr std::array<std::array<float,2>,4> corners{{{-1,-1},{1,-1},{1,1},{-1,1}}};
    for(unsigned i=0;i<4;++i) {
        const auto corner=corners[i];const float x=corner[0]*half,y=corner[1]*half*aspect;
        polygon[i]={cx+x*cs-y*sn,cy+x*sn+y*cs,corner[0]<0?u0:u1,corner[1]<0?v0:v1};
    }
    const float left=(400-width)/2;
    for(unsigned edge=0;edge<4 && count;++edge) {
        const auto distance=[&](const Vertex& v) {
            switch(edge) {case 0:return v.x-left;case 1:return left+width-v.x;
                case 2:return v.y;default:return 240-v.y;}
        };
        unsigned next=0;
        auto previous=polygon[count-1];float pd=distance(previous);
        for(unsigned i=0;i<count;++i) {
            const auto current=polygon[i];const float cd=distance(current);
            if((cd>=0)!=(pd>=0)) {
                const float t=pd/(pd-cd);
                scratch[next++]={std::lerp(previous.x,current.x,t),std::lerp(previous.y,current.y,t),
                    std::lerp(previous.u,current.u,t),std::lerp(previous.v,current.v,t)};
            }
            if(cd>=0)scratch[next++]=current;
            previous=current;pd=cd;
        }
        count=next;polygon=scratch;
    }
    for(unsigned i=1;i+1<count;i+=2) {
        if(out.count==Capacity)return;
        const std::array<Vertex,4> quad{polygon[i],polygon[i+1],polygon[0],polygon[std::min(i+2,count-1)]};
        for(unsigned v=0;v<4;++v) {
            const unsigned p=out.count*12+v*3,uv=out.count*8+v*2,c=out.count*16+v*4;
            out.positions[p]=quad[v].x;out.positions[p+1]=quad[v].y;
            out.uvs[uv]=quad[v].u;out.uvs[uv+1]=quad[v].v;
            std::copy(color.begin(),color.end(),out.colors.begin()+c);
            out.colors[c+3]=std::clamp(alpha,0.f,1.f);
        }
        ++out.count;
    }
}
inline float Visibility(double seconds,unsigned i) {
    const double period=5+Random(i*13+71)*6;
    const float phase=float(std::fmod(seconds/period+Random(i*13+72),1.));
    // Brief invisible interval between smooth fade-in and fade-out; each mote
    // follows its own phase rather than blinking the entire field together.
    return Smooth((phase-.08f)/.22f)*Smooth((.92f-phase)/.22f);
}
template<unsigned Limit=MaxParticles>
inline GeometryFor<Limit> Draw(const Settings& s,double seconds,float entrance,float screen_width,
                     double rotation_seconds=-1,float density_override=-1) {
    GeometryFor<Limit> out;
    if(!s.enabled || !std::isfinite(s.glow) || !std::isfinite(s.size) ||
       !std::isfinite(s.light_strength) || !std::isfinite(s.light_radius) || !std::isfinite(s.spin) ||
       !std::isfinite(seconds) || seconds<0 || !std::isfinite(entrance) ||
       !std::isfinite(rotation_seconds) || !std::isfinite(density_override) ||
       !std::isfinite(screen_width) || screen_width<200 || screen_width>1200)return out;
    const float density=std::clamp(density_override<0?float(s.density):density_override,0.f,float(Limit));
    const unsigned count=unsigned(std::ceil(density));
    const float size=std::clamp(s.size,.25f,3.f),glow=std::clamp(s.glow,0.f,2.f);
    const float light=std::clamp(s.light_strength,0.f,1.f),radius=std::clamp(s.light_radius,.25f,3.f);
    if(rotation_seconds<0)rotation_seconds=seconds*std::clamp(s.spin,0.f,3.f);
    const float left=(400-screen_width)/2;
    // Broad pools of light first, then every halo/core, so a later light never
    // washes over another particle. File-select foreground controls draw later.
    for(unsigned pass=0;pass<2;++pass)for(unsigned i=0;i<count;++i) {
        const double life=20+Random(i*7+2)*20;
        const float phase=float(std::fmod(seconds/life+Random(i*7+1),1.));
        const float x=left+Random(i*7+3)*screen_width+
            9*float(std::sin(seconds*.22+Random(i*7+4)*6.2831853));
        const float y=258-276*phase+3*float(std::sin(seconds*.37+i));
        const float fade=Smooth(phase/.13f)*Smooth((1-phase)/.17f)*Smooth(entrance)*Visibility(seconds,i);
        const float diameter=3.2f*size*(.7f+Random(i*7+5));
        const float opacity=fade*(.45f+.3f*Random(i*7+6))*std::clamp(density-i,0.f,1.f);
        const unsigned variant=s.variations?i%3:0;
        constexpr std::array<std::array<float,3>,3> colors{{{.9f,1,.65f},{.68f,1,.8f},{1,.92f,.65f}}};
        const auto color=colors[variant];
        if(pass==0) {
            // A broad, low-opacity light pool brightens the dark parchment;
            // the independent smooth texture avoids a magnified star outline.
            AddSprite(out,x,y,diameter*15*radius,1,0,opacity*light,color,1,screen_width);
        } else {
            AddSprite(out,x,y,diameter*3.4f,1,0,opacity*.25f*glow,color,1,screen_width);
            const double angle=Random(i*11+51)*6.28318530718+
                rotation_seconds*(.8+Random(i*11+52)*1.4)*(i%2?-1:1);
            // Round soft motes alternate with glints and elongated spinning
            // glints. These are presentation variations, not native actor types.
            AddSprite(out,x,y,diameter,variant==2?.65f:1.f,float(std::remainder(angle,6.28318530718)),
                opacity,color,variant==1?1:0,screen_width);
        }
    }
    return out;
}
struct Animation {
    using Clock=std::chrono::steady_clock;
    Clock::time_point last{};
    double seconds=0,rotation_seconds=0;
    float entrance=0;
    bool started=false;
    void Reset() {*this={};}
    template<unsigned Limit=MaxParticles>
    GeometryFor<Limit> Frame(const Settings& s,float screen_width,Clock::time_point now=Clock::now(),
                            float density_override=-1,float drift_speed_override=-1) {
        if(!s.enabled) {Reset();return {};}
        const double dt=started?std::clamp(std::chrono::duration<double>(now-last).count(),0.,.05):0.;
        last=now;started=true;
        // Per-particle phases are already distributed across their lifetimes.
        // Present that established field immediately with the menu background.
        entrance=1;
        const float speed=std::isfinite(s.speed)?std::clamp(s.speed,0.f,3.f):1.f;
        const float drift=std::isfinite(drift_speed_override) && drift_speed_override>=0?
            std::clamp(drift_speed_override,0.f,6.f):speed;
        seconds+=dt*drift;
        const float spin=std::isfinite(s.spin)?std::clamp(s.spin,0.f,3.f):1.f;
        rotation_seconds+=dt*spin;
        return Draw<Limit>(s,seconds,entrance,screen_width,rotation_seconds,density_override);
    }
};
} // namespace OOT3D::FileParticles
