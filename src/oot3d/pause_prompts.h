// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <initializer_list>
#include "oot3d/pause_prompt_layout.h"
#include "oot3d/pause_menu.h"
#include "oot3d/guest_ui_mesh.h"
#include "oot3d/control_swap.h"
#include "oot3d/widescreen.h"

namespace OOT3D::PausePrompts {
using TitleScreen::U32;
inline bool Valid(const Pose& p) {
    return std::isfinite(p.x) && std::abs(p.x)<=400 && std::isfinite(p.y) &&
        std::abs(p.y)<=240 && std::isfinite(p.scale) && p.scale>=.1f && p.scale<=4;
}
template<class M> bool SavePromptsVisible(M& m) {
    if(!SaveMenu::on_top.load())return false;
    if(m.Read(SaveMenu::Owner+0x28)==3 ||
       (SaveMenu::returning_from_options && m.Read(SaveMenu::Owner+0x28)==1))return true;
    if(m.Read(SaveMenu::Owner+0x28)!=2)return false;
    const auto state=m.Read(SaveMenu::Owner+0x24).value_or(~0u);
    return state<=15; // Include transitions so the prompts can fade before saving.
}
template<class M> bool Visible(M& m) {
    return SavePromptsVisible(m) || (PauseMenu::Active(m) && !PauseMenu::Closing(m) && !SaveMenu::Active(m) &&
        (m.Read(0x5066f8+0x34).value_or(0)!=0 || m.Read(0x50446c+0x18).value_or(0)!=0 ||
         m.Read(0x506cb0+0x38).value_or(0)!=0));
}
template<unsigned Capacity> struct QuadGeometry {
    std::array<float,Capacity*12> positions{};
    std::array<float,Capacity*8> uvs{};
    std::array<float,Capacity*16> colors{};
    unsigned count=0;
    void Add(float x,float y,float width,float height,float u0,float v0,float u1,float v1) {
        if(count==Capacity)return;
        const std::array<float,12> v{x,y,0,x+width,y,0,x,y+height,0,x+width,y+height,0};
        const std::array<float,8> uv{u0,v0,u1,v0,u0,v1,u1,v1};
        std::copy(v.begin(),v.end(),positions.begin()+count*12);
        std::copy(uv.begin(),uv.end(),uvs.begin()+count*8);
        std::fill_n(colors.begin()+count*16,16,1.f);++count;
    }
};
using Geometry=QuadGeometry<6>;
using TextGeometry=QuadGeometry<96>;
struct Pair {Geometry icons;TextGeometry text;};
inline float SafeLeft() {return (Widescreen::Active()?(400-240*Widescreen::Aspect)/2:0)+6;}
// Fit rendered geometry, never the persisted editor pose. Move first; uniformly
// reduce only a group wider/taller than the available viewport. UVs stay intact.
template<class I,class T> void KeepMeshesOnScreen(I& icons,T& text,unsigned icons_begin=0,unsigned text_begin=0) {
    float left=INFINITY,top=INFINITY,right=-INFINITY,bottom=-INFINITY;
    const auto measure=[&](const auto& g,unsigned begin) {
        for(unsigned q=begin;q<g.count;++q)for(unsigned v=0;v<4;++v) {
            const auto i=q*12+v*3;
            left=std::min(left,g.positions[i]);right=std::max(right,g.positions[i]);
            top=std::min(top,g.positions[i+1]);bottom=std::max(bottom,g.positions[i+1]);
        }
    };
    measure(icons,icons_begin);measure(text,text_begin);
    if(!std::isfinite(left) || right<=left || bottom<=top)return;
    const float lo=SafeLeft(),hi=400-lo;
    const float scale=std::min({1.f,(hi-lo)/(right-left),236.f/(bottom-top)});
    const float x=std::clamp(left,lo,hi-(right-left)*scale);
    const float y=std::clamp(top,2.f,238.f-(bottom-top)*scale);
    const auto move=[&](auto& g,unsigned begin) {
        for(unsigned q=begin;q<g.count;++q)for(unsigned v=0;v<4;++v) {
            const auto i=q*12+v*3;
            g.positions[i]=x+(g.positions[i]-left)*scale;
            g.positions[i+1]=y+(g.positions[i+1]-top)*scale;
        }
    };
    move(icons,icons_begin);move(text,text_begin);
}
inline void KeepOnScreen(Pair& pair,unsigned icons_begin=0,unsigned text_begin=0) {
    KeepMeshesOnScreen(pair.icons,pair.text,icons_begin,text_begin);
}
// Edge prompts keep their authored group anchor. Reserve one label column
// for related prompts so localized labels end at the same button column.
// Horizontal toolbars and directional navigation hints do not use this rule.
struct EdgePositions {float button,text;};
inline EdgePositions EdgePrompt(float x,float scale,float label_width,float column_width) {
    if(x<200 || label_width<=0)return {x,x+20*scale};
    return {x+(column_width+4)*scale,x+(column_width-label_width)*scale};
}
inline float LabelWidth(const Assets& assets,unsigned row,float spacing=1) {
    float right=0;
    for(unsigned j=0;j<assets.glyphs[row].size();++j) {
        const auto g=assets.glyphs[row][j];if(!g.width)continue;
        right=std::max(right,(g.x+g.width+1+j*(spacing-float(assets.native_spacing)))*.75f);
    }
    return right;
}
inline float LabelColumn(const Assets& assets,std::initializer_list<unsigned> rows,float spacing=1) {
    float width=0;for(auto row:rows)width=std::max(width,LabelWidth(assets,row,spacing));return width;
}
inline Pair SavePrompts(const Assets& assets,const SaveMenu::Layout& layout,float alpha,bool options=false,bool select_only=false) {
    Pair out;
    for(unsigned i=0;i<2;++i) {
        if(select_only && i)break;
        const auto p=layout.prompts[select_only?1:i];if(!SaveMenu::Valid(p))continue;
        const float x=(i || select_only?300.f:28.f)+p.x,y=220.f+p.y,tx=i?144.f:128.f;
        const auto ib=out.icons.count,tb=out.text.count;
        const unsigned row=options?(i?CancelText:OkayText):(i?FileBackText:FileSelectText);
        const auto edge=EdgePrompt(x,1,LabelWidth(assets,row),
            LabelColumn(assets,options?std::initializer_list<unsigned>{OkayText,CancelText}:
                std::initializer_list<unsigned>{FileSelectText,FileBackText}));
        out.icons.Add(edge.button,y,16,16,(tx+.5f)/IconWidth,95.5f/IconHeight,
                      (tx+15.5f)/IconWidth,80.5f/IconHeight);
        for(unsigned j=0;j<assets.glyphs[row].size();++j) {
            const auto g=assets.glyphs[row][j];if(!g.width)continue;
            const float start=g.x-1,shift=j*(1.f-assets.native_spacing);
            out.text.Add(edge.text+(start+shift)*.75f,y+.5f,(g.width+2)*.75f,15,
                start/TextWidth,(row*TextRowHeight+.5f)/TextHeight,
                (start+g.width+2)/TextWidth,(row*TextRowHeight+19.5f)/TextHeight);
        }
        const auto scale=[&](auto& g,unsigned begin) {
            for(unsigned q=begin;q<g.count;++q)for(unsigned v=0;v<12;v+=3) {
                g.positions[q*12+v]=x+(g.positions[q*12+v]-x)*p.scale;
                g.positions[q*12+v+1]=y+(g.positions[q*12+v+1]-y)*p.scale;
            }
        };
        scale(out.icons,ib);scale(out.text,tb);
        KeepOnScreen(out,ib,tb);
    }
    const auto fade=[&](auto& g) {
        for(unsigned i=3;i<g.count*16;i+=4)g.colors[i]=std::clamp(alpha,0.f,1.f);
    };
    fade(out.icons);fade(out.text);return out;
}
enum class Page { Items, Gear, Map };
template<class M> Page CurrentPage(M& m) {
    if(m.Read(0x5066f8+0x34).value_or(0)!=0)return Page::Items;
    if(m.Read(0x50446c+0x18).value_or(0)!=0)return Page::Gear;
    return Page::Map;
}
template<class M> bool GearEquippable(M& m) {
    // Native A handling (438884) accepts the first three groups only:
    // swords 0..2, shields 3..5, tunics 6..8. Its availability check
    // (2e9d78) reads the owned-equipment bits at save+0xb6. The equip
    // routine (2e9b4c) also checks its age table: 9 means either age.
    const auto selected=m.Read(0x50446c+0x20).value_or(27);
    if(selected>=9)return false;
    const auto age=m.Read(0x587958+4),required_age=m.Read(0x4d47c8+selected*4);
    if(!age || !required_age || (*required_age!=9 && *required_age!=*age))return false;
    const unsigned bit=(selected/3)*4+selected%3;
    return ((m.Read(0x587958+0xb4).value_or(0)>>16)&(1u<<bit))!=0;
}
// One emulated-time phase for both navigation buttons. A triangle wave keeps
// velocity constant between the endpoints and does not depend on frame rate.
struct NavigationBob {
    std::optional<std::int64_t> last_time;
    double phase=0;
    float Advance(std::int64_t time,const Layout& layout,bool visible) {
        if(!visible || !std::isfinite(layout.navigation_distance) || layout.navigation_distance<0 ||
           layout.navigation_distance>30 || !std::isfinite(layout.navigation_speed) ||
           layout.navigation_speed<0 || layout.navigation_speed>5) {
            last_time.reset();phase=0;return 0;
        }
        if(!last_time || time<*last_time) {last_time=time;phase=0;}
        const double seconds=(double(time)-double(*last_time))/1000000.;
        last_time=time;
        if(layout.navigation_speed==0 || layout.navigation_distance==0) {phase=0;return 0;}
        phase=std::fmod(phase+seconds*layout.navigation_speed,1.);
        // Start at the editor's Y position, rise, fall, then rise to the start.
        const double triangle=std::abs(4*std::fmod(phase+.25,1.)-2)-1;
        return float(triangle*layout.navigation_distance*.5);
    }
};
inline Pair MakeGeometry(const Layout& layout,const Assets& assets,Page page=Page::Items,
                         float navigation_y=0,bool gear_equippable=true,bool navigation_enabled=true,bool arrow_chooser=false,bool fit_to_screen=true) {
    Pair out;
    if(!layout.enabled || !std::isfinite(layout.scale) || layout.scale<.1f || layout.scale>4 ||
       !std::isfinite(layout.icon_scale) || layout.icon_scale<.1f || layout.icon_scale>4 ||
       !std::isfinite(layout.letter_spacing) || layout.letter_spacing<0 ||
       layout.letter_spacing>8)return out;
    const auto label=[&](unsigned row,float left,float top,float scale) {
        // Tighten spacing by moving whole glyphs, preserving their shape
        // and native shadow. Auxiliary labels use this same font/material.
        for(unsigned j=0;j<assets.glyphs[row].size();++j) {
            const auto glyph=assets.glyphs[row][j];
            if(!glyph.width || glyph.x<1 || glyph.x+glyph.width+1>TextWidth)continue;
            const float start=glyph.x-1;
            const float shift=j*(layout.letter_spacing-float(assets.native_spacing));
            out.text.Add(left+(start+shift)*.75f*scale,top,
                (glyph.width+2)*.75f*scale,15*scale,
                start/TextWidth,(row*TextRowHeight+.5f)/TextHeight,
                (start+glyph.width+2)/TextWidth,(row*TextRowHeight+19.5f)/TextHeight);
        }
    };
    const bool map_page=page==Page::Map;
    struct Span {unsigned icons_begin=0,icons_end=0,text_begin=0,text_end=0;float left=0,right=0,scale=1;bool valid=false;};
    std::array<Span,PromptCount> spans{};
    const bool row_valid=Valid(layout.lower.pose) && std::isfinite(layout.lower.spacing) &&
        layout.lower.spacing>=-20 && layout.lower.spacing<=100;
    // Items/Gear use the shared row; Map prompts keep independent positions.
    // Keep emission order stable; visual order is Change tab, A, B, ZL, ZR.
    constexpr std::array item_rows{Choose,Return,EquipI,EquipII,ChangeTab};
    for(unsigned line=0;line<(map_page?2u:item_rows.size());++line) {
        const unsigned i=map_page?MapMove+line:item_rows[line];
        const auto& p=layout.prompts[i];
        if((!map_page && !row_valid) || !Valid(p.group) || !Valid(p.button) || assets.widths[i]<4 ||
           assets.widths[i]>TextWidth)continue;
        auto& span=spans[i];span.icons_begin=out.icons.count;span.text_begin=out.text.count;
        const float s=p.group.scale*layout.scale*(map_page?1.f:layout.lower.pose.scale),b=p.button.scale*layout.icon_scale;
        const float x=map_page?(i==MapMove?48.f:280.f)+p.group.x:0;
        const float y=map_page?228+p.group.y-8*s:0;
        span.scale=s;
        const bool pair=i==ChangeTab,trigger=i==EquipI || i==EquipII;
        // Keep all parts of L / R together under the button-relative controls.
        const auto edge=map_page?EdgePrompt(x,s,LabelWidth(assets,i,layout.letter_spacing),
            LabelColumn(assets,{MapMove,MapReturn},layout.letter_spacing)):EdgePositions{x,x+20*s};
        const float center=pair?22.f:8.f;
        const auto button_x=[&](float local){return edge.button+(center+p.button.x+(local-center)*b)*s;};
        const auto button_y=[&](float local){return y+(8+p.button.y+(local-8)*b)*s;};
        const auto icon=[&](float local,float tx,float ty) {
            // The native UI material samples raw texture row (1-v)*height.
            out.icons.Add(button_x(local),button_y(0),16*b*s,16*b*s,
                (tx+.5f)/IconWidth,(IconHeight-ty-.5f)/IconHeight,
                (tx+15.5f)/IconWidth,(IconHeight-ty-16+.5f)/IconHeight);
        };
        // Emit main text first so existing A/B geometry and settings stay stable.
        float text_x=map_page?edge.text:x+(pair?48.f:20.f)*s;
        if(trigger && !ControlSwap::shoulders.load()) {
            const unsigned row=i==EquipI?ZLText:ZRText;
            const float tracking=layout.letter_spacing-float(assets.native_spacing);
            const float width=(assets.widths[row]+tracking)*.75f;
            const float trigger_origin=button_x(8-width/2)-p.button.x*s;
            const float trigger_end=trigger_origin+(assets.widths[row]-2+tracking)*.75f*b*s;
            // One ordinary font space on either side of the dash. Measure
            // the trigger label instead of leaving a fixed 24-pixel gap.
            const float gap=(assets.glyphs[i][1].width+2*layout.letter_spacing)*.75f*s;
            text_x=trigger_end+gap-assets.glyphs[i][0].x*.75f*s;
        }
        label(i,text_x,y+.5f*s,s);
        const auto trigger_label=[&](unsigned row,float local) {
            const float width=(assets.widths[row]+layout.letter_spacing-assets.native_spacing)*.75f;
            label(row,button_x(local+8-width/2),button_y(.5f),b*s);
        };
        if(pair) {
            if(ControlSwap::shoulders.load()) {trigger_label(ZLText,0);trigger_label(ZRText,28);}
            else {icon(0,192,32);icon(28,208,32);}
            label(SlashText,button_x(18),button_y(.5f),b*s);
        } else if(trigger) {
            // OoT3D predates ZL/ZR and has no corresponding button art.
            // Render their labels with its own font, independently adjustable
            // like the icon portion of every other prompt.
            if(ControlSwap::shoulders.load())icon(0,i==EquipI?192:208,32);
            else trigger_label(i==EquipI?ZLText:ZRText,0);
        } else {
            icon(0,i==Return || i==MapReturn?144:128,i==MapMove?0:32);
        }
        span.icons_end=out.icons.count;span.text_end=out.text.count;
        // Gear uses A for owned equipment; neither item-slot trigger applies
        // anywhere on this page, even when the highlighted gear is equippable.
        if((page==Page::Gear && (trigger || (i==Choose && !gear_equippable))) ||
           (page==Page::Items && arrow_chooser && (trigger || pair))) {
            const auto dim=[](auto& mesh,unsigned begin,unsigned end) {
                for(unsigned q=begin;q<end;++q)for(unsigned v=0;v<4;++v)
                    mesh.colors[q*16+v*4+3]*=.6f;
            };
            dim(out.icons,span.icons_begin,span.icons_end);
            dim(out.text,span.text_begin,span.text_end);
        }
        if(map_page) {
            continue; // No automatic row arrangement on Map.
        }
        const auto measure=[&](const auto& mesh,unsigned begin,unsigned end) {
            for(unsigned q=begin;q<end;++q)for(unsigned v=0;v<12;v+=3) {
                const float x=mesh.positions[q*12+v];
                if(!span.valid) {span.left=span.right=x;span.valid=true;}
                else {span.left=std::min(span.left,x);span.right=std::max(span.right,x);}
            }
        };
        measure(out.icons,span.icons_begin,span.icons_end);
        measure(out.text,span.text_begin,span.text_end);
    }
    constexpr std::array order{ChangeTab,Choose,Return,EquipI,EquipII};
    const float gap=layout.lower.spacing*layout.scale*layout.lower.pose.scale;
    float width=0;unsigned members=0;
    for(auto i:order)if(spans[i].valid) {width+=spans[i].right-spans[i].left;if(members++)width+=gap;}
    float cursor=200+layout.lower.pose.x-width/2;
    for(auto i:order)if(spans[i].valid) {
        const auto span=spans[i];
        const float dx=cursor-span.left,dy=228+layout.lower.pose.y-8*span.scale;
        const auto move=[&](auto& mesh,unsigned begin,unsigned end) {
            for(unsigned q=begin;q<end;++q)for(unsigned v=0;v<12;v+=3) {
                mesh.positions[q*12+v]+=dx;mesh.positions[q*12+v+1]+=dy;
            }
        };
        move(out.icons,span.icons_begin,span.icons_end);move(out.text,span.text_begin,span.text_end);
        cursor+=span.right-span.left+gap;
    }
    // Fit the map pair together as well: a longer translated label must not
    // shift only its own button out of the authored column.
    if(fit_to_screen)KeepOnScreen(out);
    const auto navigation_icons=out.icons.count,navigation_text=out.text.count;
    if(page!=Page::Gear && navigation_enabled) {
        const unsigned i=map_page?MapToItems:ItemsToMap;
        const auto& p=layout.prompts[i];
        if(Valid(p.group) && Valid(p.button) && assets.widths[i]>=4 && assets.widths[i]<=TextWidth) {
            const float x=(map_page?4.f:360.f)+p.group.x,y=154+p.group.y;
            const float s=p.group.scale*layout.scale,b=p.button.scale*layout.icon_scale;
            const auto bx=[&](float local){return x+(18+p.button.x+(local-18)*b)*s;};
            const float by=y+(8+p.button.y-8*b)*s+(std::isfinite(navigation_y)?navigation_y:0);
            const float tx=map_page?192.f:208.f;
            if(ControlSwap::shoulders.load()) {
                const unsigned row=map_page?ZLText:ZRText;
                const float width=(assets.widths[row]+layout.letter_spacing-assets.native_spacing)*.75f;
                label(row,bx((map_page?20:0)+8-width/2),by+.5f*b*s,b*s);
            } else out.icons.Add(bx(map_page?20:0),by,16*b*s,16*b*s,
                    (tx+.5f)/IconWidth,(IconHeight-32.5f)/IconHeight,
                    (tx+15.5f)/IconWidth,(IconHeight-47.5f)/IconHeight);
            // The native orange arrow points down at raw (192,80), 16x16.
            // Rotate its UVs a quarter-turn, preserving the complete native
            // atlas and its custom-texture identity. The game's V axis is flipped.
            constexpr float u0=192.5f/IconWidth,u1=207.5f/IconWidth;
            constexpr float top=(IconHeight-80.5f)/IconHeight,bottom=(IconHeight-95.5f)/IconHeight;
            const unsigned q=out.icons.count;
            out.icons.Add(bx(map_page?0:20),by,16*b*s,16*b*s,u0,top,u1,bottom);
            const std::array<float,8> uv=map_page?
                std::array{u0,bottom,u0,top,u1,bottom,u1,top}:
                std::array{u1,top,u1,bottom,u0,top,u0,bottom};
            std::copy(uv.begin(),uv.end(),out.icons.uvs.begin()+q*8);
            const float width=(assets.widths[i]+(assets.lengths[i]-1)*
                (layout.letter_spacing-float(assets.native_spacing)))*.75f;
            label(i,x+(18-width/2)*s,y+20*s,s);
        }
    }
    if(fit_to_screen)KeepOnScreen(out,navigation_icons,navigation_text);
    return out;
}
constexpr unsigned NavigationShadowSize=128;
using NavigationShadowPixels=std::array<U32,NavigationShadowSize*NavigationShadowSize>;
inline unsigned NavigationShadowTexel(unsigned x,unsigned y) {
    unsigned morton=0;
    for(unsigned bit=0;bit<3;++bit)morton|=((x>>bit)&1)<<(2*bit)|((y>>bit)&1)<<(2*bit+1);
    return ((y/8)*(NavigationShadowSize/8)+x/8)*64+morton;
}
inline NavigationShadowPixels MakeNavigationShadowTexture(float softness) {
    NavigationShadowPixels pixels{};
    softness=std::isfinite(softness)?std::clamp(softness,.01f,1.f):.7f;
    for(unsigned y=0;y<NavigationShadowSize;++y)for(unsigned x=0;x<NavigationShadowSize;++x) {
        // Navigation uses the right half against the screen edge.
        const float u=(x+.5f)/NavigationShadowSize,v=2*(y+.5f)/NavigationShadowSize-1;
        const float t=std::clamp((1-std::hypot(u,v))/softness,0.f,1.f);
        pixels[NavigationShadowTexel(x,NavigationShadowSize-1-y)]=U32(std::lround(255*t*t*(3-2*t)));
    }
    return pixels;
}
// The quad can move and fade every frame while its texture stays resident.
struct SoftShadowTexture {
    NavigationShadowPixels pixels{};
    float softness=-1;
    template<class M,class Mesh>
    bool Prepare(M& m,const Mesh& mesh,float requested) {
        const float value=std::isfinite(requested)?std::clamp(requested,.01f,1.f):.7f;
        if(value==softness)return true;
        pixels=MakeNavigationShadowTexture(value);
        if(mesh.Owned(m) && !GuestUI::WriteTextureWords(m,mesh.texture,pixels)) {
            softness=-1;return false;
        }
        softness=value;return true;
    }
};
inline QuadGeometry<1> MakeNavigationShadow(const Layout& layout,const Assets& assets,Page page) {
    QuadGeometry<1> out;
    const auto& p=layout.prompts[MapToItems];const auto& shadow=layout.map_navigation_shadow;
    if(page!=Page::Map || !layout.enabled || !shadow.enabled || !Valid(p.group) || !Valid(p.button) ||
       assets.widths[MapToItems]<4 || assets.widths[MapToItems]>TextWidth ||
       !std::isfinite(layout.scale) || layout.scale<.1f || layout.scale>4 ||
       !std::isfinite(layout.icon_scale) || layout.icon_scale<.1f || layout.icon_scale>4 ||
       !std::isfinite(layout.letter_spacing) || layout.letter_spacing<0 || layout.letter_spacing>8 ||
       !std::isfinite(shadow.x) || !std::isfinite(shadow.y) || !std::isfinite(shadow.width) ||
       !std::isfinite(shadow.height) || !std::isfinite(shadow.opacity) || shadow.opacity<=0)return out;
    const float s=p.group.scale*layout.scale;
    const float w=std::clamp(shadow.width,8.f,200.f)*s,h=std::clamp(shadow.height,8.f,240.f)*s;
    const float x=4+p.group.x+(-12+std::clamp(shadow.x,-200.f,200.f))*s;
    const float y=154+p.group.y+(17.5f+std::clamp(shadow.y,-240.f,240.f))*s;
    out.Add(x,y-h/2,w,h,0,0,1,1);
    for(unsigned i=0;i<4;++i)out.colors[i*4+3]=std::clamp(shadow.opacity,0.f,1.f);
    return out;
}
using CornerGeometry=QuadGeometry<144>;
inline constexpr auto CornerPixels=[] {std::array<U32,64> p{};p.fill(0xffffffff);return p;}();
inline CornerGeometry MapPromptCorner(const PauseMenu::PromptCorner& v,float screen_width) {
    CornerGeometry g;
    if(!v.enabled || !std::isfinite(screen_width) || screen_width<=0 ||
       !std::isfinite(v.strength) || !std::isfinite(v.width) || !std::isfinite(v.height) ||
       !std::isfinite(v.softness) || !std::isfinite(v.x) || !std::isfinite(v.y) ||
       !std::all_of(v.color.begin(),v.color.end(),[](float c){return std::isfinite(c);}))return g;
    const float strength=std::clamp(v.strength,0.f,1.f);if(strength<=0)return g;
    const float w=std::clamp(v.width,8.f,600.f),h=std::clamp(v.height,8.f,240.f);
    const float soft=std::clamp(v.softness,.05f,1.f);
    const float right=200+screen_width/2+std::clamp(v.x,-400.f,400.f),bottom=240+std::clamp(v.y,-240.f,240.f);
    for(unsigned y=0;y<12;++y)for(unsigned x=0;x<12;++x) {
        g.Add(right-w+x*w/12,bottom-h+y*h/12,w/12,h/12,.5f,.5f,.5f,.5f);
        for(unsigned k=0;k<4;++k) {
            const float nx=1-(x+(k&1))/12.f,ny=1-(y+(k>>1))/12.f;
            const float t=std::clamp((std::hypot(nx,ny)-(1-soft))/soft,0.f,1.f);
            for(unsigned c=0;c<3;++c)
                g.colors[(g.count-1)*16+k*4+c]=std::clamp(v.color[c],0.f,1.f);
            g.colors[(g.count-1)*16+k*4+3]=strength*(1-t*t*(3-2*t));
        }
    }
    return g;
}
struct Hook {
    GuestUI::Hook<CornerGeometry,8,8> corner;
    CornerGeometry corner_geometry;
    GuestUI::Hook<QuadGeometry<1>,NavigationShadowSize,NavigationShadowSize> shadow;
    GuestUI::Hook<Geometry,IconWidth,IconHeight,4> icons;
    GuestUI::Hook<TextGeometry,TextWidth,TextHeight> text;
    std::shared_ptr<const Assets> image;
    TextGeometry lettering;
    Geometry buttons;
    SoftShadowTexture shadow_texture;
    U32 entry=0,source=0;
    bool bottom_projection=false,shared_dialog=false;
    NavigationBob navigation;
    float save_prompt_alpha=1;
    std::int64_t save_prompt_time=0;
    bool Pending() const {return shadow.Pending() || corner.Pending() || icons.Pending() || text.Pending();}
    bool Drawing() const {return shadow.Drawing() || corner.Drawing() || icons.Drawing() || text.Drawing();}
    template<class M,class R> void BeginCorner(M& m,R& r) {
        if(!image || !(shared_dialog || Visible(m)))return;
        corner.Begin(m,r,entry,corner_geometry,CornerPixels,source);
        if(!corner.Pending())icons.Begin(m,r,entry,buttons,image->icons,source);
    }
    template<class M,class R> void Resume(M& m,R& r) {
        if(shadow.Pending()) {
            shadow.Resume(m,r);
            if(!shadow.Pending())BeginCorner(m,r);
        } else if(corner.Pending()) {
            corner.Resume(m,r);
            if(!corner.Pending() && image && (shared_dialog || Visible(m)))icons.Begin(m,r,entry,buttons,image->icons,source);
        } else if(icons.Pending()) {
            icons.Resume(m,r);
            if(!icons.Pending() && image && (shared_dialog || Visible(m)))
                text.Begin(m,r,entry,lettering,image->text,source);
        } else if(text.Pending())text.Resume(m,r);
        if(!Pending())image.reset();
    }
    // Reuse the save dialog's exact assets and saved poses for Game Over and
    // Boss Challenge. No separate editor elements or configuration.
    template<class M,class R> void BeginDialog(M& m,R& r,U32 return_pc,U32 donor,float alpha,bool select_only=false) {
        image=GetAssets();shared_dialog=true;bottom_projection=false;
        if(!image || !GetLayout().enabled || alpha<=0) {image.reset();return;}
        const auto geometry=SavePrompts(*image,SaveMenu::GetLayout(),alpha,false,select_only);
        entry=return_pc;source=donor;buttons=geometry.icons;lettering=geometry.text;
        icons.Begin(m,r,entry,buttons,image->icons,source);
        if(!Pending())image.reset();
    }
    template<class M,class R> void Begin(M& m,R& r,U32 return_pc,bool bottom,std::int64_t time) {
        shared_dialog=false;corner_geometry={};
        image=GetAssets();
        const auto layout=GetLayout();
        const bool visible=image && (shared_dialog || Visible(m)) && layout.enabled;
        const float bob=navigation.Advance(time,layout,visible);
        if(!visible) {image.reset();save_prompt_time=0;save_prompt_alpha=1;return;}
        if(SavePromptsVisible(m)) {
            const auto state=m.Read(SaveMenu::Owner+0x24).value_or(0);
            const bool busy=!SaveMenu::OptionsActive(m) && state>=7 && state<=9;
            const float dt=save_prompt_time && time>=save_prompt_time?
                std::clamp(float(time-save_prompt_time)/1000000.f,0.f,.1f):0;
            save_prompt_time=time;
            save_prompt_alpha=std::clamp(save_prompt_alpha+(busy?-dt/.08f:dt/.10f),0.f,1.f);
            const float fade=save_prompt_alpha*save_prompt_alpha*(3-2*save_prompt_alpha);
            const auto geometry=SavePrompts(*image,SaveMenu::GetLayout(),
                fade*((SaveMenu::OptionsActive(m) || SaveMenu::returning_from_options || state==15)?1:SaveMenu::CurrentAnimation(m).prompt),
                SaveMenu::OptionsActive(m));
            entry=return_pc;bottom_projection=bottom;lettering=geometry.text;buttons=geometry.icons;
            source=m.Read(SaveMenu::Owner+4).value_or(0);
            icons.Begin(m,r,entry,buttons,image->icons,source);
            if(!Pending())image.reset();
            return;
        }
        const auto page=CurrentPage(m);
        if(page==Page::Map)
            corner_geometry=MapPromptCorner(PauseMenu::DungeonPage(m)?
                PauseMenu::GetLayout().dungeon_prompt_vignette:PauseMenu::GetLayout().world_prompt_vignette,
                Widescreen::Active()?240*Widescreen::Aspect:400);
        const auto geometry=MakeGeometry(layout,*image,page,bob,GearEquippable(m),!BossChallenge::InBattle(m),PauseMenu::ArrowChooserOpen(m));
        entry=return_pc;bottom_projection=bottom;lettering=geometry.text;buttons=geometry.icons;
        source=m.Read(0x5066f8+0x20).value_or(0);
        const auto backdrop=MakeNavigationShadow(layout,*image,page);
        if(backdrop.count && shadow_texture.Prepare(m,shadow,layout.map_navigation_shadow.softness))
            shadow.Begin(m,r,entry,backdrop,shadow_texture.pixels,source);
        if(!shadow.Pending())BeginCorner(m,r);
        if(!Pending())image.reset();
    }
};
}
