// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include <span>
#include <tuple>
#include <vector>

namespace OOT3D::GuestUI {
using TitleScreen::U32;
// BoardModel +0x1dc OWNS a material; BoardRenderer +0x358 only borrows it.
// Native menu teardown frees the former. Never copy that ownership into a
// generated model, and never retain a borrow across menu destruction/recreation.
template<class M> bool BindMaterial(M& m,U32 model,U32 source) {
    const U32 renderer=m.Read(model+8).value_or(0);
    const U32 material=m.Read(source+0x1dc).value_or(0);
    if(!TitleScreen::Pointer(renderer,0x35c) || !TitleScreen::Pointer(material,0x234))return false;
    m.Write(model+0x1dc,0);
    m.Write(renderer+0x358,material);
    return true;
}
// Independent guest mesh, sampler and linear texture allocation. Shared by
// generated shadows and locally extracted frames; never modifies a native atlas.
template<class Geometry,unsigned TextureWidth,unsigned TextureHeight,unsigned TextureFormat=0,bool BorrowTexture=false> struct Hook {
    static_assert(TextureFormat==0 || TextureFormat==4 || TextureFormat==12 || TextureFormat==13); // RGBA8 / RGBA4 / ETC1 / ETC1A4.
    static constexpr U32 Quads=std::tuple_size_v<decltype(Geometry::positions)>/12;
    static constexpr U32 Descriptor=0x1c0,Positions=0x2e0,UVs=Positions+Quads*48,
        Colors=UVs+Quads*32,Indices=Colors+Quads*64, Cookie=(Indices+Quads*12+15)&~15u,
        AllocationSize=(Cookie+16+63)&~31u,
        TextureBytes=TextureFormat==13?TextureWidth*TextureHeight:TextureFormat==12?TextureWidth*TextureHeight/2:
            TextureWidth*TextureHeight*(TextureFormat==4?2:4);
    static constexpr U32 Magic=0x4f554931;
    std::vector<U32> pixels;
    enum class Stage {Idle,Allocate,TextureAllocate,Mesh,Model,Positions,UVs,Colors,Draw};
    Stage stage=Stage::Idle;
    bool creation_failed=false;
    Stage failed_stage=Stage::Idle;
    U32 block=0,model=0,source=0,entry=0,texture=0,renderer=0;
    std::array<U32,16> saved{};
    Geometry geometry;
    bool Drawing() const {return stage==Stage::Draw;}
    bool Pending() const {return stage!=Stage::Idle;}
    U32 TextureAddress() const {return texture;}
    template<class M> bool Owned(M& m) const {
        return TitleScreen::Pointer(block,AllocationSize) && TitleScreen::Pointer(model,0x1e4) &&
            m.Read(block+Cookie)==std::optional<U32>{Magic} &&
            m.Read(block+Cookie+4)==std::optional<U32>{renderer} &&
            m.Read(block+Cookie+8)==std::optional<U32>{model} &&
            m.Read(model)==std::optional<U32>{0x4ebd60} && m.Read(model+4)==std::optional<U32>{block} &&
            TitleScreen::Pointer(renderer,0x35c) && m.Read(model+8)==std::optional<U32>{renderer} &&
            (BorrowTexture || (!BorrowTexture && texture>=0x14000000 && texture<=0x1c000000-TextureBytes && !(texture&127) &&
            m.Read(block+Cookie+12)==std::optional<U32>{texture} &&
            m.Read(block+0x158)==std::optional<U32>{texture} &&
            m.Read(block+0x164)==std::optional<U32>{TextureFormat}));
    }
    template<class M> bool RefreshModel(M& m) {
        std::array<U32,0x1e4/4> copy{};
        for(unsigned i=0;i<copy.size();++i) {
            auto v=m.Read(source+4*i);if(!v || !m.Read(model+4*i))return false;copy[i]=*v;
        }
        copy[1]=block;copy[2]=renderer;copy[0x174/4]=Quads*6;copy[0x178/4]&=~0x60u;
        copy[0x1dc/4]=0; // The native factory also leaves this zero for a borrowed material.
        constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
        for(unsigned i=0;i<identity.size();++i)copy[3+i]=std::bit_cast<U32>(identity[i]);
        for(unsigned i=0;i<copy.size();++i)m.Write(model+4*i,copy[i]);
        return BindMaterial(m,model,source);
    }
    template<class M> void SetGeometry(M& m) {
        const auto write=[&](U32 at,const auto& a) {
            for(unsigned i=0;i<a.size();++i)m.Write(block+at+4*i,std::bit_cast<U32>(a[i]));
        };
        write(Positions,geometry.positions);write(UVs,geometry.uvs);write(Colors,geometry.colors);
    }
    template<class M> bool Prepare(M& m) {
        const U32 desc=m.Read(m.Read(source+4).value_or(0)).value_or(0);
        if(!TitleScreen::Pointer(block,AllocationSize) || !TitleScreen::Pointer(desc,0x120))return false;
        std::array<U32,0x120/4> copy{};
        for(unsigned i=0;i<copy.size();++i) {auto v=m.Read(desc+4*i);if(!v)return false;copy[i]=*v;}
        for(U32 i=0;i<AllocationSize;i+=4)if(!m.Read(block+i))return false;
        copy[0]=block+Positions;copy[1]=block+UVs;copy[2]=block+Colors;
        copy[3]=Quads*4;copy[4]=block+Indices;copy[5]=Quads*6;
        // Every quad below is an independent indexed triangle pair. Native
        // single-sprite donors can use triangle strips and uniform attributes;
        // inheriting that topology connects adjacent panels with stray triangles.
        // Enable our UV/vertex-color streams too (darkness and soft shadows).
        copy[7]=(copy[7]|0x120u)&~0x18u;
        for(unsigned i=0;i<copy.size();++i)m.Write(block+Descriptor+4*i,copy[i]);
        for(U32 q=0;q<Quads;++q) {
            const U32 b=q*4,at=block+Indices+q*12;
            m.Write(at,b|((b+2)<<16));m.Write(at+4,(b+1)|((b+1)<<16));m.Write(at+8,(b+2)|((b+3)<<16));
        }
        SetGeometry(m);return true;
    }
    template<class M,class R> void Resume(M& m,R& r) {
        const auto call=[&](Stage next,U32 fn,U32 a,U32 b=0,U32 c=0) {
            stage=next;r[0]=a;r[1]=b;r[2]=c;r[3]=0;r[14]=entry;r[15]=fn;
        };
        const auto finish=[&]{
            // Retain any partially constructed resources until shutdown rather
            // than abandoning them and allocating another set every frame.
            if(stage!=Stage::Draw && block && !Owned(m)) {
                creation_failed=true;failed_stage=stage;
            }
            r=saved;stage=Stage::Idle;
        };
        if(stage==Stage::Allocate) {
            block=r[0];
            if(block<0x14000000 || block>0x1c000000-AllocationSize || (block&127) || !Prepare(m)) {
                finish();return;
            }
            if constexpr(BorrowTexture) {
                call(Stage::Mesh,0x348f34,block,block+Descriptor);return;
            }
            // The SDK texture allocator supplies 128-byte-aligned linear RAM.
            // Ordinary guest heap addresses cannot be translated by the GPU.
            call(Stage::TextureAllocate,0x2a8bdc,0x10000,0x101,0);
            r[3]=TextureBytes;return;
        }
        if(stage==Stage::TextureAllocate) {
            texture=r[0];
            if(texture<0x14000000 || texture>0x1c000000-TextureBytes || (texture&127)) {finish();return;}
            for(unsigned i=0;i<pixels.size();++i)if(!m.Read(texture+4*i)){finish();return;}
            for(unsigned i=0;i<pixels.size();++i)m.Write(texture+4*i,pixels[i]);
            call(Stage::Mesh,0x348f34,block,block+Descriptor);return;
        }
        if(stage==Stage::Mesh) {
            if constexpr(BorrowTexture) {
                const U32 mesh=m.Read(source+4).value_or(0);
                for(U32 i=0;i<0x60;i+=4) {
                    auto word=m.Read(mesh+0x13c+i);if(!word){finish();return;}
                    m.Write(block+0x13c+i,*word);
                }
            } else {
            // Own sampler, linear filtering, clamp to edge, no mipmaps.
            for(U32 i=0;i<0x60;i+=4)m.Write(block+0x13c+i,0);
            m.Write(block+0x13c,1);m.Write(block+0x140,3);
            m.Write(block+0x158,TextureAddress());m.Write(block+0x15c,TextureWidth|(TextureHeight<<16));
            m.Write(block+0x164,TextureFormat);
            }
            const U32 material=m.Read(source+0x1dc).value_or(0);
            if(!TitleScreen::Pointer(material,0x234)){finish();return;}
            call(Stage::Model,0x34897c,0,block,material);return;
        }
        if(stage==Stage::Model) {
            model=r[0];
            if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=std::optional<U32>{0x4ebd60}){finish();return;}
            renderer=m.Read(model+8).value_or(0);
            if(!RefreshModel(m)){finish();return;}
            m.Write(block+Cookie,Magic);m.Write(block+Cookie+4,renderer);m.Write(block+Cookie+8,model);m.Write(block+Cookie+12,texture);
            call(Stage::Positions,0x36759c,block,sizeof(geometry.positions),block+Positions);return;
        }
        if(stage==Stage::Positions) {call(Stage::UVs,0x317d1c,block,sizeof(geometry.uvs),block+UVs);return;}
        if(stage==Stage::UVs) {call(Stage::Colors,0x2f9934,block,sizeof(geometry.colors),block+Colors);return;}
        if(stage==Stage::Colors) {
            const U32 count=m.Read(block+0x128).value_or(0),current=m.Read(block+0x124).value_or(0);
            if(count<1 || count>2 || current>=count){finish();return;}
            if constexpr(BorrowTexture) {
                // Refresh native pose, tint and sampler each frame, including fades.
                for(U32 at=0xc;at<=0x38;at+=4)m.Write(model+at,m.Read(source+at).value_or(0));
                for(U32 at=0xf0;at<=0xfc;at+=4)m.Write(model+at,m.Read(source+at).value_or(0));
                const U32 mesh=m.Read(source+4).value_or(0);
                for(U32 at=0x13c;at<0x19c;at+=4)m.Write(block+at,m.Read(mesh+at).value_or(0));
            }
            m.Write(block+0x124,(current+1)%count);call(Stage::Draw,0x3f9680,model);return;
        }
        finish();
    }
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc,Geometry g,std::span<const U32> image,U32 source_model) {
        if(!g.count || (!BorrowTexture && image.size()*4!=TextureBytes))return;
        source=source_model;
        if(!TitleScreen::Pointer(source,0x1e4) || m.Read(source)!=std::optional<U32>{0x4ebd60})return;
        // Menus can briefly expose a model before its descriptor/material is
        // ready. Wait before allocating anything, and retry incomplete setup
        // using the resources already retained by this hook.
        const U32 material=m.Read(source+0x1dc).value_or(0);
        const U32 desc=m.Read(m.Read(source+4).value_or(0)).value_or(0);
        if(!TitleScreen::Pointer(material,0x234) || !TitleScreen::Pointer(desc,0x120))return;
        if(creation_failed && failed_stage!=Stage::Allocate &&
           failed_stage!=Stage::TextureAllocate && failed_stage!=Stage::Mesh)return;
        geometry=g;entry=return_pc;std::copy(r.begin(),r.end(),saved.begin());
        r[3]=0;r[14]=entry;
        if(creation_failed) {
            creation_failed=false;
            pixels.assign(image.begin(),image.end());
            stage=failed_stage;
            if(stage!=Stage::Allocate)SetGeometry(m);
            if(stage==Stage::TextureAllocate) {
                r[0]=0x10000;r[1]=0x101;r[2]=0;r[3]=TextureBytes;r[15]=0x2a8bdc;
            } else {
                if(stage==Stage::Allocate)r[0]=block;
                Resume(m,r);
            }
            return;
        }
        if(!Owned(m)) {
            pixels.assign(image.begin(),image.end());
            // The ordinary C++ heap is only 512 KiB. Retained file-select and
            // pause vertex arrays can exhaust it, starving native text/layout
            // allocations. Use the SDK linear resource heap for these CPU/GPU
            // accessible buffers, just as we do for our textures.
            block=model=texture=0;stage=Stage::Allocate;
            r[0]=0x10000;r[1]=0x101;r[2]=0;r[3]=AllocationSize;r[15]=0x2a8bdc;
        } else {
            // The allocation belongs to us, not to whichever menu supplied its
            // template last frame. Rebind in place, including same-address native
            // model reuse, instead of leaking allocations on every naming visit.
            if(!RefreshModel(m)){r=saved;return;}
            SetGeometry(m);stage=Stage::Positions;r[0]=block;r[1]=sizeof(geometry.positions);
            r[2]=block+Positions;r[15]=0x36759c;
        }
    }
};
}
