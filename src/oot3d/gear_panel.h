// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"
#include "oot3d/guest_ui_mesh.h"

namespace OOT3D::GearPanel {
using TitleScreen::U32;
constexpr U32 Owner=0x50446c, Quads=18, Descriptor=0x1c0, Positions=0x2e0,
    UVs=Positions+Quads*48, Colors=UVs+Quads*32, Indices=Colors+Quads*64,
    Cookie=(Indices+Quads*12+15)&~15u, AllocationSize=(Cookie+16+31)&~31u;
constexpr U32 Magic=0x4f475032;
struct Geometry {
    std::array<float,Quads*12> positions{};
    std::array<float,Quads*8> uvs{};
    std::array<float,Quads*16> colors{};
    unsigned count=0;
};
inline bool Valid(const PauseMenu::GearPanel& p) {
    return PauseMenu::Valid({p.x,p.y,1}) && std::isfinite(p.width) && p.width>=.1f && p.width<=3 &&
        std::isfinite(p.height) && p.height>=.05f && p.height<=3;
}
// Nine slices from the LARGE upper green region (atlas pixels 0..158,0..144).
// Right/bottom borders mirror the left/top strips. Interior sampling stops
// short of the smaller rectangle below; border thickness is independent of size.
inline Geometry Make(const PauseMenu::GearPanel& p,const std::array<float,4>& color,float motion) {
    Geometry g;if(!Valid(p))return g;
    const float left=160-160*p.width+(p.x+motion)/PauseMenu::Scale;
    const float top=100-94*p.height+p.y/PauseMenu::Scale;
    const std::array<float,4> x{left,left+2,left+320*p.width-2,left+320*p.width};
    const std::array<float,4> y{top,top+2,top+188*p.height-2,top+188*p.height};
    constexpr std::array<std::array<float,2>,3> u{{{.5f,2.5f},{2.5f,157.5f},{2.5f,.5f}}};
    constexpr std::array<std::array<float,2>,3> v{{{.5f,2.5f},{2.5f,142.5f},{2.5f,.5f}}};
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col) {
        const unsigned q=row*3+col;
        const std::array<float,12> pos{x[col],y[row],0,x[col+1],y[row],0,
            x[col],y[row+1],0,x[col+1],y[row+1],0};
        const std::array<float,8> uv{u[col][0]/512,1-v[row][0]/256,u[col][1]/512,1-v[row][0]/256,
            u[col][0]/512,1-v[row][1]/256,u[col][1]/512,1-v[row][1]/256};
        std::copy(pos.begin(),pos.end(),g.positions.begin()+q*12);
        std::copy(uv.begin(),uv.end(),g.uvs.begin()+q*8);
        for(unsigned i=0;i<16;++i)g.colors[q*16+i]=color[i%4];
    }
    g.count=9;return g;
}
template<class M> Geometry Current(M& m,const PauseMenu::Layout& layout) {
    if(!PauseMenu::Active(m) || !m.Read(Owner+0x18).value_or(0))return {};
    const U32 helper=m.Read(Owner+0xc).value_or(0);
    if(m.Read(helper)!=std::optional<U32>{41})return {};
    std::array<float,4> color{};
    if(!TitleScreen::ReadFloats(m,m.Read(helper+0x18).value_or(0),color))return {};
    auto g=Make(layout.gear_panel,color,PauseMenu::motion_x.load());
    const auto lower=Make(layout.gear_panel_lower,color,PauseMenu::motion_x.load());
    if(lower.count) {
        std::copy_n(lower.positions.begin(),lower.count*12,g.positions.begin()+g.count*12);
        std::copy_n(lower.uvs.begin(),lower.count*8,g.uvs.begin()+g.count*8);
        std::copy_n(lower.colors.begin(),lower.count*16,g.colors.begin()+g.count*16);
        g.count+=lower.count;
    }
    return g;
}
// Separate guest mesh; the native atlas and all overlay buffers remain intact.
struct Hook {
    enum class Stage {Idle,Allocate,Mesh,Model,Positions,UVs,Colors,Draw};
    Stage stage=Stage::Idle;
    U32 block=0,model=0,source=0,entry=0;
    std::array<U32,16> saved{};
    Geometry geometry;
    bool Drawing() const {return stage==Stage::Draw;}
    bool Pending() const {return stage!=Stage::Idle;}
    template<class M> bool Owned(M& m) const {
        return TitleScreen::Pointer(block,AllocationSize) && TitleScreen::Pointer(model,0x1e4) &&
            m.Read(block+Cookie)==std::optional<U32>{Magic} &&
            m.Read(block+Cookie+4)==std::optional<U32>{source} &&
            m.Read(block+Cookie+8)==std::optional<U32>{model} &&
            m.Read(model)==std::optional<U32>{0x4ebd60} && m.Read(model+4)==std::optional<U32>{block};
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
        const auto finish=[&]{r=saved;stage=Stage::Idle;};
        if(stage==Stage::Allocate) {
            block=r[0];if(!Prepare(m)){block=0;finish();return;}
            call(Stage::Mesh,0x348f34,block,block+Descriptor);return;
        }
        if(stage==Stage::Mesh) {
            // Share only the native gear atlas bindings; own all geometry.
            const U32 mesh=m.Read(source+4).value_or(0);
            std::array<U32,24> bindings{};
            for(unsigned i=0;i<bindings.size();++i) {
                const auto word=m.Read(mesh+0x13c+4*i);if(!word){finish();return;}bindings[i]=*word;
            }
            for(unsigned i=0;i<bindings.size();++i)m.Write(block+0x13c+4*i,bindings[i]);
            const U32 material=m.Read(source+0x1dc).value_or(0);
            if(!TitleScreen::Pointer(material,0x234)){finish();return;}
            call(Stage::Model,0x34897c,0,block,material);return;
        }
        if(stage==Stage::Model) {
            model=r[0];
            if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=std::optional<U32>{0x4ebd60}){finish();return;}
            const auto node=m.Read(model+8);if(!node){finish();return;}
            std::array<U32,0x1e4/4> copy{};
            for(unsigned i=0;i<copy.size();++i) {
                auto v=m.Read(source+4*i);if(!v || !m.Read(model+4*i)){finish();return;}copy[i]=*v;
            }
            copy[1]=block;copy[2]=*node;copy[0x174/4]=Quads*6;copy[0x178/4]&=~0x60u;
            copy[0x1dc/4]=0; // Borrow the live native material; never own/free it.
            constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
            for(unsigned i=0;i<identity.size();++i)copy[3+i]=std::bit_cast<U32>(identity[i]);
            for(unsigned i=0;i<copy.size();++i)m.Write(model+4*i,copy[i]);
            m.Write(block+Cookie,Magic);m.Write(block+Cookie+4,source);m.Write(block+Cookie+8,model);
            call(Stage::Positions,0x36759c,block,sizeof(geometry.positions),block+Positions);return;
        }
        if(stage==Stage::Positions) {call(Stage::UVs,0x317d1c,block,sizeof(geometry.uvs),block+UVs);return;}
        if(stage==Stage::UVs) {call(Stage::Colors,0x2f9934,block,sizeof(geometry.colors),block+Colors);return;}
        if(stage==Stage::Colors) {
            const U32 count=m.Read(block+0x128).value_or(0),current=m.Read(block+0x124).value_or(0);
            if(count<1 || count>2 || current>=count){finish();return;}
            if(!GuestUI::BindMaterial(m,model,source)){finish();return;}
            const U32 mesh=m.Read(source+4).value_or(0);
            for(U32 at=0x13c;at<0x19c;at+=4)m.Write(block+at,m.Read(mesh+at).value_or(0));
            m.Write(block+0x124,(current+1)%count);call(Stage::Draw,0x3f9680,model);return;
        }
        finish();
    }
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc,Geometry g) {
        if(!g.count)return;
        source=m.Read(Owner+4).value_or(0);
        if(!TitleScreen::Pointer(source,0x1e4) || m.Read(source)!=std::optional<U32>{0x4ebd60})return;
        geometry=g;entry=return_pc;std::copy(r.begin(),r.end(),saved.begin());
        r[3]=0;r[14]=entry;
        if(!Owned(m)) {
            block=model=0;stage=Stage::Allocate;r[0]=AllocationSize;r[15]=0x313ce0;
        } else {
            SetGeometry(m);stage=Stage::Positions;r[0]=block;r[1]=sizeof(geometry.positions);
            r[2]=block+Positions;r[15]=0x36759c;
        }
    }
};
} // namespace OOT3D::GearPanel
