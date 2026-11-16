// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_prompts.h"
#include "oot3d/hud_hooks.h"
#include "oot3d/hud_buttons_layout.h"

namespace OOT3D::HUDButtons {
using TitleScreen::U32;
using Geometry=PausePrompts::QuadGeometry<1+2*(Count-1)>;
using Lettering=PausePrompts::QuadGeometry<4>;
using SwordGeometry=PausePrompts::QuadGeometry<1>;
struct Pair {Geometry circles;Lettering text;};
template<class M> bool NativeBVisible(M& m) {
    // 42b194 parks quad 7 at +420,+240 in ordinary gameplay, and returns it
    // to its native position for special HUD modes. Read the fresh vertices
    // so we also respect that routine's other native visibility conditions.
    const U32 helper=m.Read(HUD::Action+0x18).value_or(0);
    if(m.Read(helper)!=std::optional<U32>{8})return false;
    // Gallery/horseback owns the native weapon bubble even if the editor has moved it
    // offscreen or disabled it. Its transformed bounds must not summon a
    // second custom bubble/sword at the edge of the screen.
    if(HUD::UsesBLayout(m))return true;
    std::array<float,12> v;
    if(!TitleScreen::ReadFloats(m,m.Read(helper+0x10).value_or(0)+7*48,v))return false;
    const float left=std::min({v[0],v[3],v[6],v[9]}),right=std::max({v[0],v[3],v[6],v[9]});
    const float top=std::min({v[1],v[4],v[7],v[10]}),bottom=std::max({v[1],v[4],v[7],v[10]});
    return right>left && bottom>top && left<400 && right>0 && top<240 && bottom>0;
}
inline std::optional<PausePrompts::Pose> ButtonPose(const Layout& l,unsigned i,bool native_b=false) {
    const auto button=l.buttons[i];auto p=button.pose;
    if(!button.enabled || !PausePrompts::Valid(p) || (i==B && native_b))return {};
    if(i!=B) {
        if(!std::isfinite(l.item_scale) || l.item_scale<.1f || l.item_scale>4 ||
           !std::isfinite(l.item_x) || !std::isfinite(l.item_y))return {};
        // Hidden buttons still participate in the pivot.
        float cx=0,cy=0;
        for(unsigned j=X;j<Count;++j) {cx+=l.buttons[j].pose.x/4;cy+=l.buttons[j].pose.y/4;}
        if(!std::isfinite(cx) || !std::isfinite(cy))return {};
        p.x=cx+(p.x-cx)*l.item_scale+l.item_x;
        p.y=cy+(p.y-cy)*l.item_scale+l.item_y;
        p.scale*=l.item_scale;
        // Apply individual offsets after the shared pivot transform. Clamp the
        // offset, not the final center: a scaled group can cross the screen edge.
        const auto personal=UserHUD::Sanitize(l.user_items[i-X]);
        p.x+=personal.x;p.y+=personal.y;
        if(personal.scale!=0)p.scale=std::clamp(p.scale*UserHUD::Factor(personal),.1f,16.f);
    }
    return p;
}
inline Pair MakeGeometry(const Layout& l,const PausePrompts::Assets& assets,float alpha,bool native_b) {
    Pair out;if(!std::isfinite(alpha) || alpha<=0)return out;
    for(unsigned i=0;i<Count;++i) {
        const auto pose=ButtonPose(l,i,native_b);if(!pose)continue;
        const auto p=*pose;const float scale=p.scale;
        const float size=(i==B?48.f:40.f)*scale,left=p.x-size/2,top=p.y-size*47/48/2;
        if(i==B) {
            out.circles.Add(left,top,size,size*47/48,16.5f/256,246.5f/256,63.5f/256,200.5f/256);
        } else {
            // Native A tile: (64,0), 38x38 in the HUD atlas. Reflect its
            // lower half upward to remove the letter, retaining the native
            // material/texture-pack lookup and the existing bubble bounds.
            const float half_height=size*47/48/2;
            constexpr float u0=64.5f/256,u1=101.5f/256;
            constexpr float middle_v=237.f/256,bottom_v=218.5f/256;
            out.circles.Add(left,top,size,half_height,u0,bottom_v,u1,middle_v);
            out.circles.Add(left,p.y,size,half_height,u0,middle_v,u1,bottom_v);
        }
        if(i!=B) {
            if(assets.cursor_ready && PausePrompts::Valid(l.item_labels)) {
                const float tx=(i==X || i==I)?64.f:80.f,ty=(i==X || i==Y)?48.f:32.f;
                const auto label=l.item_labels;
                // Scale each label around its own center; whole-button scaling
                // carries both the artwork and its local offsets with the circle.
                const float label_size=16*scale*label.scale;
                out.text.Add(p.x+label.x*scale-label_size/2,
                    p.y+(-11+label.y)*scale-label_size/2,label_size,label_size,
                    (tx+.5f)/128,(128-ty-.5f)/128,(tx+15.5f)/128,(128-ty-15.5f)/128);
            }
        }
    }
    for(unsigned i=0;i<out.circles.count*16;++i)out.circles.colors[i]=i%4==3?std::clamp(alpha,0.f,1.f):1;
    for(unsigned i=0;i<out.text.count*16;++i)
        out.text.colors[i]=i%4==3?std::clamp(alpha,0.f,1.f):1.f;
    return out;
}
// Read fresh native UVs and disabled-item colors, then fit the sprite into
// its bubble. The source model's screen projection/position is not inherited.
template<class M> SwordGeometry IconQuad(M& m,U32 helper,unsigned quad,
        const PausePrompts::Pose& p,float size,float alpha,const PausePrompts::Pose& item) {
    SwordGeometry g;
    if(!PausePrompts::Valid(item))return g;
    const auto count=m.Read(helper);
    if(!count || *count>256 || quad>=*count)return g;
    std::array<float,12> positions;
    // +0xc is the current source rectangle; +0x10 is the animated upload
    // buffer, which can still contain the previous bottom-screen frame.
    if(!TitleScreen::ReadFloats(m,m.Read(helper+0xc).value_or(0)+quad*48,positions) ||
       !TitleScreen::ReadFloats(m,m.Read(helper+0x14).value_or(0)+quad*32,g.uvs) ||
       !TitleScreen::ReadFloats(m,m.Read(helper+0x18).value_or(0)+quad*64,g.colors))return {};
    const float left=std::min({positions[0],positions[3],positions[6],positions[9]});
    const float right=std::max({positions[0],positions[3],positions[6],positions[9]});
    const float top=std::min({positions[1],positions[4],positions[7],positions[10]});
    const float bottom=std::max({positions[1],positions[4],positions[7],positions[10]});
    const float u0=std::min({g.uvs[0],g.uvs[2],g.uvs[4],g.uvs[6]});
    const float u1=std::max({g.uvs[0],g.uvs[2],g.uvs[4],g.uvs[6]});
    const float v0=std::min({g.uvs[1],g.uvs[3],g.uvs[5],g.uvs[7]});
    const float v1=std::max({g.uvs[1],g.uvs[3],g.uvs[5],g.uvs[7]});
    // Empty slots have collapsed UVs even when their screen rectangle exists.
    if(right<=left || bottom<=top || u1<=u0 || v1<=v0)return {};
    const float scale=size*p.scale*item.scale/std::max(right-left,bottom-top);
    for(unsigned v=0;v<12;v+=3) {
        g.positions[v]=p.x+item.x*p.scale+(positions[v]-(left+right)/2)*scale;
        g.positions[v+1]=p.y+(5+item.y)*p.scale+(positions[v+1]-(top+bottom)/2)*scale;
    }
    for(unsigned c=3;c<16;c+=4)g.colors[c]=std::clamp(g.colors[c],0.f,1.f)*std::clamp(alpha,0.f,1.f);
    g.count=1;return g;
}
// 444ce0 selects the equipped sword (including broken knife and Biggoron).
template<class M> SwordGeometry Sword(M& m,const Layout& l,float alpha,bool native_b) {
    const auto p=ButtonPose(l,B,native_b);
    if(!l.show_items || !p || !std::isfinite(alpha) || alpha<=0)return {};
    const U32 helper=m.Read(HUD::Main+4).value_or(0);
    if(m.Read(helper)!=std::optional<U32>{92})return {};
    return IconQuad(m,helper,35,*p,30,alpha,l.item_icons);
}
using ItemGeometry=PausePrompts::QuadGeometry<4>;
constexpr std::array<unsigned,4> ItemSlots{3,4,1,2};
template<class M> ItemGeometry Items(M& m,const Layout& l,float alpha) {
    ItemGeometry g;
    if(!l.show_items || !std::isfinite(alpha) || alpha<=0)return g;
    const U32 controller=m.Read(HUD::Main+0x14).value_or(0);
    const U32 helper=m.Read(controller+0x10c).value_or(0);
    if(m.Read(helper)!=std::optional<U32>{5})return g;
    // Native controller: 0=ocarina, 1=I, 2=II, 3=X, 4=Y (444a90).
    for(unsigned i=X;i<Count;++i) {
        const auto p=ButtonPose(l,i);if(!p)continue;
        const auto icon=IconQuad(m,helper,ItemSlots[i-X],*p,26,alpha,l.item_icons);
        if(!icon.count)continue;
        std::copy(icon.positions.begin(),icon.positions.end(),g.positions.begin()+g.count*12);
        std::copy(icon.uvs.begin(),icon.uvs.end(),g.uvs.begin()+g.count*8);
        std::copy(icon.colors.begin(),icon.colors.end(),g.colors.begin()+g.count*16);
        ++g.count;
    }
    return g;
}
using CounterGeometry=PausePrompts::QuadGeometry<4>;
template<class M> U32 ItemCounter(M& m,unsigned i) {
    if(i<X || i>=Count)return 0;
    const U32 owner=m.Read(HUD::Main+0x14).value_or(0);
    if(!TitleScreen::Pointer(owner,0x43c))return 0;
    const unsigned slot=ItemSlots[i-X];
    // The native item controller explicitly enables counters only for items
    // with ammunition. A stale counter pointer alone does not mean it is active.
    const U32 flag_at=owner+0x434+slot;
    if(((m.Read(flag_at&~3u).value_or(0)>>((flag_at&3)*8))&255)==0)return 0;
    return m.Read(owner+0xc+slot*4).value_or(0);
}
template<class M> CounterGeometry ItemCount(M& m,const Layout& l,unsigned i,float alpha) {
    CounterGeometry out;
    if(i<X || i>=Count)return out;
    const auto p=ButtonPose(l,i);
    if(!l.show_items || !p || !PausePrompts::Valid(l.item_icons) ||
       !std::isfinite(alpha) || alpha<=0)return out;
    const U32 owner=m.Read(HUD::Main+0x14).value_or(0);
    const U32 icon_helper=m.Read(owner+0x10c).value_or(0);
    if(m.Read(icon_helper)!=std::optional<U32>{5} ||
       !IconQuad(m,icon_helper,ItemSlots[i-X],*p,26,alpha,l.item_icons).count)return out;
    const U32 counter=ItemCounter(m,i);
    if(!TitleScreen::Pointer(counter,0x14))return out;
    const U32 helper=m.Read(counter+8).value_or(0);
    const unsigned count=m.Read(helper).value_or(0);
    if(count==0 || count>4)return out;
    float right=-INFINITY,bottom=-INFINITY;
    for(unsigned q=0;q<count;++q) {
        std::array<float,12> positions;
        std::array<float,8> uv;
        std::array<float,16> colors;
        if(!TitleScreen::ReadFloats(m,m.Read(helper+0xc).value_or(0)+q*48,positions) ||
           !TitleScreen::ReadFloats(m,m.Read(helper+0x14).value_or(0)+q*32,uv) ||
           !TitleScreen::ReadFloats(m,m.Read(helper+0x18).value_or(0)+q*64,colors))return {};
        // 2fcb04 collapses the UV rectangle of unused leading digits. Read
        // fresh source data, avoiding the previous bottom draw's upload buffer.
        const auto [umin,umax]=std::minmax({uv[0],uv[2],uv[4],uv[6]});
        const auto [vmin,vmax]=std::minmax({uv[1],uv[3],uv[5],uv[7]});
        if(umax<=umin || vmax<=vmin)continue;
        for(unsigned v=0;v<4;++v) {
            if(!std::isfinite(positions[v*3]) || !std::isfinite(positions[v*3+1]))return {};
            right=std::max(right,positions[v*3]);bottom=std::max(bottom,positions[v*3+1]);
            colors[v*4+3]=std::clamp(colors[v*4+3],0.f,1.f)*std::clamp(alpha,0.f,1.f);
        }
        std::copy(positions.begin(),positions.end(),out.positions.begin()+out.count*12);
        std::copy(uv.begin(),uv.end(),out.uvs.begin()+out.count*8);
        std::copy(colors.begin(),colors.end(),out.colors.begin()+out.count*16);
        ++out.count;
    }
    const auto item=l.item_icons;
    const float scale=p->scale*item.scale;
    const float number_scale=std::isfinite(l.counter_scale)?std::clamp(l.counter_scale,.1f,4.f):1.f;
    const float number_x=std::isfinite(l.counter_x)?std::clamp(l.counter_x,-400.f,400.f):0;
    const float number_y=std::isfinite(l.counter_y)?std::clamp(l.counter_y,-240.f,240.f):0;
    const auto hd=HUD::GetEffectiveLayout();
    const bool hd_ammo=hd.hd_numbers && hd.hd_ammo_numbers && HUD::Valid(hd.hd_ammo_text) &&
        std::isfinite(hd.hd_ammo_spacing) && hd.hd_ammo_spacing>=-8 && hd.hd_ammo_spacing<=24;
    // Match native B ammunition: spacing is in the original digit units,
    // before button/counter scaling. Keep the right/bottom anchor fixed.
    if(hd_ammo)
        HUD::NumberPositions(std::span<float>(out.positions.data(),out.count*12),{},hd.hd_ammo_spacing,true);
    for(unsigned v=0;v<out.count*12;v+=3) {
        out.positions[v]=p->x+item.x*p->scale+18*scale+(out.positions[v]-right)*scale*26/32*number_scale+number_x*scale;
        out.positions[v+1]=p->y+(5+item.y)*p->scale+13*scale+(out.positions[v+1]-bottom)*scale*26/32*number_scale+number_y*scale;
    }
    // Preserve the existing HD-only X/Y offsets and size adjustment.
    if(hd_ammo)
        HUD::NumberPositions(std::span<float>(out.positions.data(),out.count*12),hd.hd_ammo_text,0,true);
    return out;
}
using Sampler=std::array<U32,0x60/4>;
template<class M> std::optional<Sampler> ReadSampler(M& m,U32 model) {
    if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=std::optional<U32>{0x4ebd60})return {};
    const U32 mesh=m.Read(model+4).value_or(0);
    if(!TitleScreen::Pointer(mesh,0x19c))return {};
    Sampler sampler;
    for(unsigned i=0;i<sampler.size();++i) {
        const auto v=m.Read(mesh+0x13c+i*4);if(!v)return {};
        sampler[i]=*v;
    }
    return sampler;
}
struct Hook {
    GuestUI::Hook<Geometry,1,1,0,true> circles;
    GuestUI::Hook<Lettering,128,128,4> text;
    GuestUI::Hook<SwordGeometry,1,1,0,true> sword;
    GuestUI::Hook<ItemGeometry,1,1,0,true> items;
    std::array<GuestUI::Hook<CounterGeometry,1,1,0,true>,4> counters;
    SwordGeometry sword_geometry;
    ItemGeometry item_geometry;
    std::optional<Sampler> sword_sampler,item_sampler;
    std::array<CounterGeometry,4> counter_geometry;
    std::array<std::optional<Sampler>,4> counter_samplers;
    unsigned counter_index=0;
    std::shared_ptr<const PausePrompts::Assets> assets;
    Lettering lettering;
    U32 entry=0,source=0;
    bool Pending() const {return circles.Pending() || text.Pending() || sword.Pending() || items.Pending() ||
        std::any_of(counters.begin(),counters.end(),[](const auto& c){return c.Pending();});}
    bool Drawing() const {return circles.Drawing() || text.Drawing() || sword.Drawing() || items.Drawing() ||
        std::any_of(counters.begin(),counters.end(),[](const auto& c){return c.Drawing();});}
    template<class M,class R> void BeginSword(M& m,R& r) {
        if(sword_sampler)sword.Begin(m,r,entry,sword_geometry,{},source);
        if(!sword.Pending())BeginItems(m,r);
    }
    template<class M,class R> void BeginItems(M& m,R& r) {
        if(item_sampler)items.Begin(m,r,entry,item_geometry,{},source);
        if(!items.Pending())BeginCounts(m,r);
    }
    template<class M,class R> void BeginCounts(M& m,R& r) {
        while(counter_index<counters.size()) {
            auto& c=counters[counter_index];
            if(counter_samplers[counter_index])
                c.Begin(m,r,entry,counter_geometry[counter_index],{},source);
            if(c.Pending())return;
            ++counter_index;
        }
        BeginText(m,r);
    }
    template<class M,class R> void BeginText(M& m,R& r) {
        if(assets)text.Begin(m,r,entry,lettering,assets->cursor,source);
    }
    template<class M,class Mesh> void BindIconSampler(M& m,Mesh& mesh,const std::optional<Sampler>& sampler) {
        if(!mesh.Drawing() || !sampler)return;
        // These independent meshes use A's top-screen model/material with
        // the original icon texture binding. Never change a native sampler.
        for(unsigned i=0;i<sampler->size();++i)m.Write(mesh.block+0x13c+i*4,(*sampler)[i]);
    }
    template<class M,class R> void Resume(M& m,R& r) {
        if(circles.Pending()) {
            circles.Resume(m,r);
            if(!circles.Pending())BeginSword(m,r);
        } else if(sword.Pending()) {
            sword.Resume(m,r);if(!sword.Pending())BeginItems(m,r);
        } else if(items.Pending()) {
            items.Resume(m,r);if(!items.Pending())BeginCounts(m,r);
        } else if(counter_index<counters.size() && counters[counter_index].Pending()) {
            auto& c=counters[counter_index];c.Resume(m,r);
            if(!c.Pending()){++counter_index;BeginCounts(m,r);}
        } else if(text.Pending())text.Resume(m,r);
        BindIconSampler(m,sword,sword_sampler);BindIconSampler(m,items,item_sampler);
        for(unsigned i=0;i<counters.size();++i)BindIconSampler(m,counters[i],counter_samplers[i]);
        if(!Pending())assets.reset();
    }
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc,float alpha) {
        if(HUD::MinigameActive(m))return;
        assets=PausePrompts::GetAssets();if(!assets || alpha<=0)return;
        const U32 helper=m.Read(HUD::Action+0x18).value_or(0);
        source=m.Read(HUD::Action+0x20).value_or(0);
        if(m.Read(helper)!=std::optional<U32>{8})return;
        const auto layout=GetEffectiveLayout();const bool native_b=NativeBVisible(m);
        const auto geometry=MakeGeometry(layout,*assets,alpha,native_b);
        sword_geometry=Sword(m,layout,alpha,native_b);
        item_geometry=Items(m,layout,alpha);
        sword_sampler=ReadSampler(m,m.Read(HUD::Main+0xc).value_or(0));
        const U32 controller=m.Read(HUD::Main+0x14).value_or(0);
        item_sampler=ReadSampler(m,m.Read(controller+8).value_or(0));
        counter_index=0;
        for(unsigned i=0;i<counters.size();++i) {
            counter_geometry[i]=ItemCount(m,layout,X+i,alpha);
            HUD::PublishNumberGeometry(counter_geometry[i],HUD::NumberKind::Ammo);
            const U32 counter=ItemCounter(m,X+i);
            counter_samplers[i]=TitleScreen::Pointer(counter,0x14)?
                ReadSampler(m,m.Read(counter+0x10).value_or(0)):std::nullopt;
        }
        lettering=geometry.text;entry=return_pc;
        circles.Begin(m,r,entry,geometry.circles,{},source);
        if(!Pending())assets.reset();
    }
};
}
