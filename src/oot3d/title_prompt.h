// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompts.h"
#include "oot3d/title_prompt_assets.h"

namespace OOT3D::TitlePrompt {
using TitleScreen::U32;
using Geometry=PausePrompts::QuadGeometry<24>;
constexpr U32 Queue=0x5c0ba8;
inline Geometry MakeGeometry(const Assets& image,const TitleScreen::Layout& layout,
                             const std::array<float,4>& bounds,const std::array<float,4>& rgba) {
    Geometry g;const auto pose=layout[3];
    if(!TitleScreen::Valid(pose) || !image.width || image.width>Width ||
       bounds[1]<=bounds[0] || bounds[3]<=bounds[2] || rgba[3]<=0 ||
       !std::isfinite(layout.letter_spacing) || layout.letter_spacing<-4 || layout.letter_spacing>12)return g;
    for(float c:layout.text_color)if(!std::isfinite(c) || c<0 || c>1)return g;
    // Follow native entrance sizing, independently of the original prompt's editor pose.
    const float s=pose.scale*(bounds[1]-bounds[0])/32;
    const float tracking=layout.letter_spacing-image.native_spacing;
    const float width=(image.width+(image.length-1)*tracking)*.75f*s,height=15*s;
    const float x=(bounds[2]+bounds[3])/2+40+pose.x;
    const float y=(bounds[0]+bounds[1])/2+70+pose.y;
    for(unsigned i=0;i<image.length;++i) {
        const auto glyph=image.glyphs[i];
        if(!glyph.width || glyph.x<1 || glyph.x+glyph.width+1>Width)return {};
        const float start=glyph.x-1;
        g.Add(x-width/2+(start+i*tracking)*.75f*s,y-height/2,
            (glyph.width+2)*.75f*s,height,start/Width,.5f/Height,
            (start+glyph.width+2)/Width,19.5f/Height);
    }
    for(unsigned i=0;i<g.count*16;++i)
        g.colors[i]=i%4==3?rgba[3]:rgba[i%4]*layout.text_color[i%4];
    PausePrompts::Geometry empty;
    PausePrompts::KeepMeshesOnScreen(empty,g);
    return g;
}
struct Hook : GuestUI::Hook<Geometry,Width,Height> {
    using Base=GuestUI::Hook<Geometry,Width,Height>;
    Geometry next;
    U32 sprite_sp=0,controller=0;
    template<class M,class R> void Observe(M& m,const R& r,const TitleScreen::Layout& layout) {
        next={};sprite_sp=controller=0;
        const auto owner=m.Read(0x50bb50);
        if(!owner || !TitleScreen::Pointer(*owner,0x49c) || r[4]!=*owner+0x10c ||
           m.Read(r[4]+8)!=std::optional<U32>{std::bit_cast<U32>(208.f)} ||
           m.Read(r[4]+12)!=std::optional<U32>{std::bit_cast<U32>(32.f)})return;
        const auto image=GetAssets();
        std::array<float,4> bounds,rgba;
        if(!image || !TitleScreen::ReadFloats(m,r[3],bounds) ||
           !TitleScreen::ReadFloats(m,r[2],rgba))return;
        next=MakeGeometry(*image,layout,bounds,rgba);
        sprite_sp=r[13];controller=*owner;
    }
    template<class M> void Normalize(M& m) {
        constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
        // The source is a pooled native sprite. Its transform, UV crop and tint
        // belong to that sprite; our geometry already includes position and alpha.
        for(U32 offset:{0xcu,0x54u,0x84u,0x110u})
            for(unsigned i=0;i<identity.size();++i)m.Write(model+offset+4*i,std::bit_cast<U32>(identity[i]));
        for(unsigned i=0;i<3;++i) {
            m.Write(model+0x3c+4*i,0);
            m.Write(model+0x48+4*i,std::bit_cast<U32>(1.f));
        }
        for(unsigned i=0;i<4;++i)m.Write(model+0xf0+4*i,std::bit_cast<U32>(1.f));
        m.Write(model+0x178,m.Read(model+0x178).value_or(0)&~0x71u);
    }
    template<class M,class R> bool Return(M& m,R& r) {
        if(Pending()) {
            const bool prepare=stage==Stage::Allocate,enqueue=stage==Stage::Colors;
            const U32 buffer=enqueue?m.Read(block+0x124).value_or(0):0;
            Base::Resume(m,r);
            if(prepare && stage==Stage::TextureAllocate) {
                // Native pooled sprites use strips and a uniform tint. Our glyphs
                // use independent triangle pairs, vertex colors, UVs and streamed
                // buffers. Keeping native flags clips triangles and ignores fading.
                const U32 flags=m.Read(block+Descriptor+0x1c).value_or(0);
                m.Write(block+Descriptor+0x1c,(flags|0x120u)&~0x18u);
            }
            if(enqueue && stage==Stage::Draw) {
                // Base prepares immediate draws; the queue's native model update
                // advances this buffer itself. Advancing twice selects stale data.
                m.Write(block+0x124,buffer);
                Normalize(m);
                // Queue with the native top-screen sprites, rather than drawing
                // during the bottom-screen update with the wrong projection.
                r[0]=Queue;r[1]=5;r[2]=model;r[3]=0;r[15]=0x328350;
            }
            return Pending();
        }
        const auto image=GetAssets();
        const auto geometry=next;next={};
        if(!geometry.count || !image || r[13]!=sprite_sp || r[4]!=controller+0x10c ||
           m.Read(0x50bb50)!=std::optional<U32>{controller})return false;
        // The first pooled sprite is a stable template across title frames.
        Base::Begin(m,r,TitleScreen::Sites[7].pc,geometry,image->pixels,m.Read(Queue+0x154).value_or(0));
        return Pending();
    }
};
}
