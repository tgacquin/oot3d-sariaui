// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompts.h"
namespace OOT3D::BossRounding {
using Geometry=PausePrompts::QuadGeometry<65>;
inline float Inset(float y,float height,float radius,bool bottom) {
    const float d=y<radius?radius-y:bottom && y>height-radius?y-height+radius:0;
    return d>0?radius-std::sqrt(std::max(0.f,radius*radius-d*d)):0;
}
inline Geometry Make(const std::array<float,12>& p,const std::array<float,8>& uv,float radius,bool bottom) {
    Geometry g;
    float left=p[0],right=p[0],top=p[1],end=p[1];
    for(unsigned i=0;i<4;++i) {left=std::min(left,p[i*3]);right=std::max(right,p[i*3]);top=std::min(top,p[i*3+1]);end=std::max(end,p[i*3+1]);}
    const float w=right-left,h=end-top;
    if(w<1 || h<1)return g;
    radius=std::clamp(radius,0.f,std::min(w,h)*.5f);
    unsigned tl=0,tr=0,bl=0;
    for(unsigned i=0;i<4;++i) {
        if(p[3*i]==left && p[3*i+1]==top)tl=i;
        if(p[3*i]==right && p[3*i+1]==top)tr=i;
        if(p[3*i]==left && p[3*i+1]==end)bl=i;
    }
    const auto add=[&](float a,float b) {
        const unsigned q=g.count;const float ia=Inset(a,h,radius,bottom),ib=Inset(b,h,radius,bottom);
        g.Add(left+ia,top+a,w-2*ia,b-a,0,0,0,0);
        g.positions[q*12+6]=left+ib;g.positions[q*12+9]=right-ib;
        for(unsigned i=0;i<4;++i) {
            const float x=(g.positions[q*12+3*i]-left)/w,y=(g.positions[q*12+3*i+1]-top)/h;
            g.positions[q*12+3*i+2]=p[2];
            for(unsigned c=0;c<2;++c)g.uvs[q*8+i*2+c]=uv[tl*2+c]+x*(uv[tr*2+c]-uv[tl*2+c])+y*(uv[bl*2+c]-uv[tl*2+c]);
        }
    };
    for(unsigned i=0;i<32;++i)add(radius*i/32,radius*(i+1)/32);
    add(radius,bottom?h-radius:h);
    if(bottom)for(unsigned i=0;i<32;++i)add(h-radius+radius*i/32,h-radius+radius*(i+1)/32);
    return g;
}
struct Hook : GuestUI::Hook<Geometry,8,8,0,true> {
    bool immediate=false;
    template<class M,class R> bool NativeDraw(M& m,R& r,bool enabled,float radius,bool bottom) {
        if(!enabled || immediate || Drawing() || radius<=0)return false;
        const auto source=r[0],mesh=m.Read(source+4).value_or(0),desc=m.Read(mesh).value_or(0);
        const auto current=m.Read(mesh+0x124).value_or(2);
        if(m.Read(source)!=0x4ebd60 || m.Read(desc+12)!=4 || current>=2)return false;
        const auto live=m.Read(mesh+0x1a0+4*current).value_or(0);
        std::array<float,12> p;std::array<float,8> uv;
        if(!TitleScreen::ReadFloats(m,live,p) || !TitleScreen::ReadFloats(m,live+48,uv))return false;
        auto geometry=Make(p,uv,radius,bottom);if(!geometry.count)return false;
        Begin(m,r,PauseMenu::Sites[19].pc,geometry,{},source);
        immediate=Pending();return immediate;
    }
    template<class M,class R> void ResumeNative(M& m,R& r) {
        const bool drew=Drawing();Resume(m,r);
        if(Pending())return;
        immediate=false;
        if(drew)r[15]=r[14];
        else {r[1]=0x54c8c8;r[15]=0x3f9684;}
    }
};
}
