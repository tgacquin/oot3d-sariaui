// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/loading_icon_state.h"
#include "oot3d/hud_layout.h"
#include "oot3d/pause_ammo.h"
#include "oot3d/hud_buttons_layout.h"
#include "oot3d/atlas_seams.h"
#include "oot3d/hud_routing.h"
#include "oot3d/hud_fade.h"
#include "oot3d/hud_return.h"
#include "oot3d/carrot_animation.h"
#include "oot3d/rumble.h"
#include "oot3d/minimap_control.h"
#include "oot3d/title_hooks.h"

namespace OOT3D::HUD {
using TitleScreen::U32;
using TitleScreen::Pointer;
constexpr U32 FirstSVC = 0xfff020;
constexpr U32 Main = 0x50af34, Action = 0x4fc648;
// Native 467344 stores the gear page's token-number controller at +8.
constexpr U32 SkulltulaCounter = 0x50446c+8;
constexpr float MinimapAnchorX = 40, MinimapAnchorY = 204;
constexpr float TimerAnchorX=159, TimerAnchorY=216, MinigameAnchorX=200, MinigameAnchorY=216;
// 0x469590 creates the targeting hint at (341,5)..(396,60).
// 0x4698a0 creates the Shard of Agony and its separately animated glow.
constexpr float ShardAnchorX=36, ShardAnchorY=34, TargetAnchorX=368.5f, TargetAnchorY=32.5f;
template<class M> std::optional<Element> AuxiliaryModel(M& m,U32 model) {
    if(!Pointer(model,0x1e4) || m.Read(model)!=std::optional<U32>{0x4ebd60})return {};
    if(m.Read(Action+0x78)==std::optional<U32>{model})return TargetIndicator;
    if(m.Read(Action+0x80)==std::optional<U32>{model} ||
       m.Read(Action+0x88)==std::optional<U32>{model})return ShardAgony;
    return {};
}
// Native 0x444544 selects mode 1 from the minigame ammo at play+0x5c74,
// mode 2 for the Bombchu Bowling actor in scene 0x4b, mode 3 from
// play+0x2e40, mode 4 from save+0x94, and mode 5 while riding Epona.
// All five modes use the native B bubble and its separate weapon/count.
template<class M> bool MinigameActive(M& memory) {
    return memory.Read(Action+0x48)==std::optional<U32>{4};
}
template<class M> bool MountedHUD(M& memory) {
    return memory.Read(Action+0x48)==std::optional<U32>{5};
}
template<class M> bool UsesBLayout(M& memory) {
    const auto mode=memory.Read(Action+0x48).value_or(0);
    return mode>=1 && mode<=5;
}
template<class M> HUDButtons::Button SpecialBButton(M& memory) {
    const auto layout=HUDButtons::GetEffectiveLayout();
    auto button=layout.buttons[HUDButtons::B];
    const auto mode=memory.Read(Action+0x48).value_or(0);
    // Modes 1..4 are native minigame HUDs. Ordinary horseback mode (5)
    // retains the regular B position, as does the normal on-foot HUD.
    if(mode>=1 && mode<=4 && std::isfinite(layout.minigame_b_x))
        button.pose.x+=std::clamp(layout.minigame_b_x,-100.f,100.f);
    return button;
}
// USA Rev 1. Each site follows regeneration of the upload vertices from the
// native geometry plus animation offsets. Never modify the source geometry:
// hidden/animated elements and Reset must not accumulate our transforms.
constexpr std::array<TitleScreen::Site, 10> Sites{{
    {0x42dda8, 0xe5940004, 0xe5900010},
    {0x42b860, 0xe5940018, 0xe5900010},
    {0x2fc968, 0xe5940008, 0xe5900010},
    {0x42deec, 0xe594002c, 0xe3500000}, // Fresh main mesh and model matrix ready.
    {0x2fea38, 0xe0811081, 0xe5962754}, // Render queue, after native model preparation.
    {0x41f278, 0xe28d3044, 0xe1a01006}, // Loading icon: freshly generated screen rectangle.
    {0x2fea98, 0xe5900214, 0xe5901000}, // Queue model draw begins.
    {0x2feaa8, 0xe5960014, 0xe2844001}, // Queue model draw returns.
    {0x3fba04, 0xe28d102c, 0xed8d0a12}, // Fresh material color, before uniform upload.
    {0x47946c, 0xe1d410f6, 0xedcd0a16}, // Native title-card Y, before screen-space draw.
}};
inline U32 Instruction(U32 pc, U32 word) {
    for (unsigned i = 0; i < Sites.size(); ++i)
        if (pc == Sites[i].pc && word == Sites[i].instruction)
            return 0xef000000 | (FirstSVC + i);
    return word;
}
template<class M> bool CodeMatches(M& memory) {
    for (const auto& s : Sites)
        if (memory.Read(s.pc) != std::optional<U32>{s.instruction} ||
            memory.Read(s.pc + 4) != std::optional<U32>{s.following}) return false;
    return memory.Read(0x46b064) == std::optional<U32>{Main} &&
           memory.Read(0x42b9bc) == std::optional<U32>{Action} &&
           memory.Read(0x41f280) == std::optional<U32>{0xebfb9cd2} &&
           memory.Read(0x41a350) == std::optional<U32>{0x4ed274};
}
inline bool Valid(const Pose& p) { return TitleScreen::Valid({p.x, p.y, p.scale}); }
inline Pose TopPose(Pose pose, Element element) {
    if (element == Hearts) { pose.x -= 70; pose.y += 8; }
    if (element == Magic) { pose.x -= 66; pose.y += 8; }
    if (element == Rupees) { pose.x += 10; pose.y += 101; }
    if (element == Keys) { pose.x += 10; pose.y += 41; }
    if (element == Carrots) pose.x += 40; // Center the 320-wide row on the top screen.
    return pose;
}
template<class M> std::optional<RoutedMesh> Mesh(M& memory, U32 helper,bool timer=false) {
    if (!Pointer(helper, 0x20)) return {};
    const unsigned count = memory.Read(helper).value_or(0);
    if (count != 92 && (count == 0 || count > (timer?8u:4u))) return {};
    auto hash = HashStart;
    std::uint64_t geometry_hash=0;
    std::array<std::uint64_t,3> attribute_hashes{};
    unsigned attribute_index=0;
    const bool diagnostic=count==92 && RoutingCapture::Active();
    // XYZ, UV and RGBA arrays in exactly the order uploaded by the game.
    for (const auto [offset, stride] : std::array<std::array<unsigned, 2>, 3>{{{0x10,48},{0x14,32},{0x18,64}}}) {
        const U32 base = memory.Read(helper + offset).value_or(0);
        if (!Pointer(base, count * stride)) return {};
        auto attribute_hash=HashStart;
        for (unsigned at = 0; at < count * stride; at += 4) {
            const auto value = memory.Read(base + at);
            if (!value) return {};
            const auto corrected=count==92 && offset==0x14?
                AtlasSeams::MagicWord(at/32,(at%32)/4,*value):*value;
            hash = HashWord(hash, corrected);
            if(diagnostic)attribute_hash=HashWord(attribute_hash,corrected);
        }
        attribute_hashes[attribute_index++]=attribute_hash;
        if(offset==0x14)geometry_hash=hash;
    }
    return RoutedMesh{hash, count, false,geometry_hash,false,attribute_hashes};
}
// Pause routing edits the rotating upload after number preparation. Identify
// the bytes actually selected by native 313ad8, not the helper's source arrays.
// The same packed XYZ/UV/RGBA layout is used by textbox interpolation.
template<class M> std::optional<RoutedMesh> SubmittedNumberMesh(M& m,U32 model) {
    if(!Pointer(model,0x178))return {};
    const U32 mesh=m.Read(model+4).value_or(0);
    if(!Pointer(mesh,0x1a8))return {};
    const U32 desc=m.Read(mesh).value_or(0);
    if(!Pointer(desc,0x20))return {};
    const auto count=m.Read(desc+12);if(!count)return {};
    const unsigned vertices=*count?*count:4,flags=m.Read(desc+0x1c).value_or(0);
    const unsigned current=m.Read(mesh+0x124).value_or(2);
    if(!vertices || vertices%4 || vertices>32 || current>=2 ||
       !(flags&0x80) || !(flags&0x20) || (flags&0x18) ||
       m.Read(model+0x174)!=std::optional<U32>{vertices/4*6})return {};
    const U32 live=m.Read(mesh+0x1a0+4*current).value_or(0);
    if(!Pointer(live,vertices*36))return {};
    auto hash=HashStart;std::uint64_t geometry=0;
    for(unsigned offset=0;offset<vertices*36;offset+=4) {
        const auto word=m.Read(live+offset);if(!word)return {};
        hash=HashWord(hash,*word);
        if(offset+4==vertices*20)geometry=hash;
    }
    return RoutedMesh{hash,vertices/4,false,geometry};
}
// Equivalent to the native model queue insertion (00328350), with capacity and
// destination validation before any write. No allocation or borrowed lifetimes.
template<class M> bool QueueTop(M& memory, U32 model) {
    constexpr U32 queue = 0x5c0ba8, count_address = queue + 0x28;
    constexpr U32 models = queue + 5 * 0x60 + 0x214;
    constexpr U32 priorities = queue + 5 * 0x60 + 0x4b4;
    if (!Pointer(model, 0x1e4) || memory.Read(model) != std::optional<U32>{0x4ebd60}) return false;
    const auto count = memory.Read(count_address);
    if (!count || *count >= 24) return false;
    for (unsigned i = 0; i <= *count; ++i)
        if (!memory.Read(models + i * 4) || !memory.Read(priorities + i * 4)) return false;
    unsigned position = *count;
    while (position && std::int32_t(*memory.Read(priorities + (position - 1) * 4)) > 0) {
        memory.Write(models + position * 4, *memory.Read(models + (position - 1) * 4));
        memory.Write(priorities + position * 4, *memory.Read(priorities + (position - 1) * 4));
        --position;
    }
    memory.Write(models + position * 4, model);
    memory.Write(priorities + position * 4, 0);
    memory.Write(count_address, *count + 1);
    return true;
}
struct RoutingState {
    bool preparing_counter = false, counter_ready = false;
    bool preparing_keys = false, keys_ready = false;
    unsigned counter_stage = 0;
    U32 drawing_top_model = 0;
    HUDFade::State fade;
    MinimapControl::InstrumentFade minimap_fade;
    HUDReturn::State hud_return;
};
// Four XYZ vertices per quad. Leave Z, UVs, colors and native alpha untouched.
template<class M> void Quads(M& memory, U32 helper, unsigned first, unsigned count,
                            const Pose& pose, float anchor_x, float anchor_y, float native_width = 400, float spacing = 0,
                            std::array<float,2> adjustment = {}, float max_scale = 3) {
    // TopPose adds the native screen-routing offsets to the editor pose.
    // Allow that margin at the slider extremes instead of reverting to the
    // unmodified native geometry when an element is moved offscreen.
    if (!std::isfinite(pose.scale) || pose.scale<.1f || pose.scale>max_scale ||
        !std::isfinite(pose.x) || !std::isfinite(pose.y) ||
        std::abs(pose.x)>480 || std::abs(pose.y)>360 || !Pointer(helper, 0x20)) return;
    const auto total = memory.Read(helper);
    const auto vertices = memory.Read(helper + 0x10);
    if (!total || *total > 256 || first > *total || count > *total - first ||
        !vertices || !Pointer(*vertices, *total * 48)) return;
    for (unsigned q = first; q < first + count; ++q) {
        const U32 address = *vertices + q * 48;
        std::array<float, 12> v;
        if (!TitleScreen::ReadFloats(memory, address, v)) continue;
        // Native hidden sprites can be full-sized rectangles parked beyond
        // either edge of their source screen. Collapse those before routing or
        // scaling, otherwise a smaller pose can bring unused digits back on-screen.
        const float min_x=std::min({v[0],v[3],v[6],v[9]});
        const float max_x=std::max({v[0],v[3],v[6],v[9]});
        const float min_y=std::min({v[1],v[4],v[7],v[10]});
        const float max_y=std::max({v[1],v[4],v[7],v[10]});
        // An empty magic fill still has its native height. Route that
        // zero-width edge with the frame so interpolation can reach/from zero
        // without jumping back to the unmodified bottom-screen coordinates.
        const bool empty_magic=first==59 && count==4 && q==62 && min_y<max_y;
        if((min_x==max_x && !empty_magic) || min_y==max_y) continue;
        const bool hidden=min_x>=native_width || min_y>=240 || max_x<=0 || max_y<=0;
        for (unsigned i = 0; i < 12; i += 3) {
            if(hidden) {v[i]=v[0];v[i+1]=v[1];}
            else {
                v[i] = anchor_x + (v[i] - anchor_x + ((q-first)%10)*spacing) * pose.scale + pose.x + adjustment[0];
                v[i+1] = anchor_y + (v[i+1] - anchor_y) * pose.scale + pose.y + adjustment[1];
            }
        }
        for (unsigned i = 0; i < 12; i += 3) {
            memory.Write(address + i * 4, std::bit_cast<U32>(v[i]));
            memory.Write(address + (i+1) * 4, std::bit_cast<U32>(v[i+1]));
        }
    }
}
// Apply after the shared Rupees pose, to its fresh number upload only. The
// native counter uses right-to-left quads at x=20+10*(count-q-1), y=110.
// Keep its fixed-width alignment and leave collapsed leading digits collapsed.
template<class M> void RupeeNumberLayout(M& memory,U32 helper,const Layout& layout) {
    const auto group=layout[Rupees];
    const auto text=(layout.hd_numbers && layout.hd_rupee_numbers)?layout.hd_rupee_text:Pose{};
    const float spacing=(layout.hd_numbers && layout.hd_rupee_numbers)?layout.hd_rupee_spacing:0;
    if(!Valid(group) || !Valid(text) || !std::isfinite(layout.rupee_number_gap) ||
       std::abs(layout.rupee_number_gap)>100 || !std::isfinite(spacing) || spacing< -8 || spacing>24)return;
    if(layout.rupee_number_gap==0 && text.x==0 && text.y==0 && text.scale==1 && spacing==0)return;
    const auto count=memory.Read(helper);
    const U32 vertices=memory.Read(helper+0x10).value_or(0);
    if(!count || !*count || *count>4 || !Pointer(vertices,*count*48))return;
    const auto pose=TopPose(group,Rupees);
    const float ax=2+(20-2)*group.scale+pose.x,ay=111+(110-111)*group.scale+pose.y;
    for(unsigned q=0;q<*count;++q) {
        std::array<float,12> v;
        if(!TitleScreen::ReadFloats(memory,vertices+q*48,v))continue;
        const float dx=group.scale*(layout.rupee_number_gap+text.x+spacing*(*count-q-1)*text.scale);
        for(unsigned i=0;i<12;i+=3) {
            memory.Write(vertices+q*48+i*4,std::bit_cast<U32>(ax+(v[i]-ax)*text.scale+dx));
            memory.Write(vertices+q*48+(i+1)*4,std::bit_cast<U32>(ay+(v[i+1]-ay)*text.scale+group.scale*text.y));
        }
    }
}
// Keys use the same 12x21 atlas cells as rupees, but their HUD group can
// have a different scale. Match only HD digit size to Rupees' effective text
// scale. Keep the key icon/pose independent and scale around the visible
// number's left edge and vertical center; parked leading digits stay parked.
template<class M> void KeyNumberLayout(M& m,U32 helper,const Layout& layout) {
    if(!layout.hd_numbers || !layout.hd_rupee_numbers || !Valid(layout[Keys]) || !Valid(layout[Rupees]) ||
       !Valid(layout.hd_rupee_text))return;
    const float scale=layout[Rupees].scale*layout.hd_rupee_text.scale/layout[Keys].scale;
    if(scale==1)return;
    const unsigned count=m.Read(helper).value_or(0);
    const U32 vertices=m.Read(helper+0x10).value_or(0);
    if(!count || count>4 || !Pointer(vertices,count*48))return;
    std::array<std::array<float,12>,4> quads{};
    std::array<bool,4> visible{};
    float left=INFINITY,top=INFINITY,bottom=-INFINITY;
    for(unsigned q=0;q<count;++q) {
        if(!TitleScreen::ReadFloats(m,vertices+q*48,quads[q]))return;
        const auto& v=quads[q];
        visible[q]=v[3]>v[0] && v[7]>v[1];
        if(!visible[q])continue;
        left=std::min(left,v[0]);top=std::min(top,v[1]);bottom=std::max(bottom,v[7]);
    }
    if(!std::isfinite(left))return;
    const float center=(top+bottom)*.5f;
    for(unsigned q=0;q<count;++q)if(visible[q])for(unsigned i=0;i<12;i+=3) {
        const auto& v=quads[q];
        m.Write(vertices+q*48+i*4,std::bit_cast<U32>(left+(v[i]-left)*scale));
        m.Write(vertices+q*48+(i+1)*4,std::bit_cast<U32>(center+(v[i+1]-center)*scale));
    }
}
// Local HD-only placement; source geometry is regenerated each frame.
inline void NumberPositions(std::span<float> positions,const Pose& p,float spacing,bool right=false,
                            std::optional<float> center_x=std::nullopt) {
    const unsigned count=positions.size()/12;
    if(!count || count>8 || positions.size()!=count*12 || !Valid(p) ||
       !std::isfinite(spacing) || spacing< -8 || spacing>24)return;
    if(center_x && !std::isfinite(*center_x))return;
    if(!center_x && p.x==0 && p.y==0 && p.scale==1 && spacing==0)return;
    std::array<float,96> source{};
    std::copy(positions.begin(),positions.end(),source.begin());
    std::array<bool,8> visible{};
    float left=INFINITY,top=INFINITY,end=-INFINITY,bottom=-INFINITY;
    for(unsigned q=0;q<count;++q) {
        const float* v=source.data()+q*12;
        for(unsigned i=0;i<12;++i)if(!std::isfinite(v[i]))return;
        visible[q]=v[3]>v[0] && v[7]>v[1];
        if(!visible[q])continue;
        left=std::min(left,v[0]);top=std::min(top,v[1]);end=std::max(end,v[3]);bottom=std::max(bottom,v[7]);
    }
    if(!std::isfinite(left))return;
    const float ax=center_x?(left+end)*.5f:right?end:left,ay=right?bottom:top;
    const unsigned digits=std::count(visible.begin(),visible.end(),true);
    for(unsigned q=0;q<count;++q)if(visible[q]) {
        int rank=0;
        for(unsigned other=0;other<count;++other)if(visible[other])
            rank+=right?-(source[other*12]>source[q*12]+.01f):(source[other*12]<source[q*12]-.01f);
        const float centered_rank=center_x?rank+(right?1.f:-1.f)*(digits-1)*.5f:rank;
        for(unsigned i=0;i<12;i+=3) {
            positions[q*12+i]=center_x.value_or(ax)+(source[q*12+i]-ax+centered_rank*spacing)*p.scale+p.x;
            positions[q*12+i+1]=ay+(source[q*12+i+1]-ay)*p.scale+p.y;
        }
    }
}
template<class M> void NumberLayout(M& m,U32 helper,const Pose& p,float spacing,bool right=false,
                                   std::optional<float> center_x=std::nullopt) {
    const unsigned count=m.Read(helper).value_or(0);
    const U32 vertices=m.Read(helper+0x10).value_or(0);
    if(!count || count>8 || !Pointer(vertices,count*48))return;
    std::array<float,96> positions{};
    for(unsigned i=0;i<count*12;++i) {
        const auto v=m.Read(vertices+i*4);if(!v)return;
        positions[i]=std::bit_cast<float>(*v);
    }
    // Native unused leading digits are parked at x=400. Leave them parked.
    auto active=std::span<float>(positions.data(),count*12);
    std::array<bool,8> hidden{};
    for(unsigned q=0;q<count;++q) {
        hidden[q]=positions[q*12]>=400 || positions[q*12+1]>=240;
        if(hidden[q])positions[q*12+3]=positions[q*12];
    }
    NumberPositions(active,p,spacing,right,center_x);
    for(unsigned q=0;q<count;++q)if(!hidden[q])for(unsigned i=0;i<12;i+=3) {
        m.Write(vertices+q*48+i*4,std::bit_cast<U32>(positions[q*12+i]));
        m.Write(vertices+q*48+(i+1)*4,std::bit_cast<U32>(positions[q*12+i+1]));
    }
}
template<class M> void TimerNumberLayout(M& m,U32 helper,const Layout& layout) {
    if(layout.hd_numbers && layout.hd_timer_numbers)
        NumberLayout(m,helper,layout.hd_timer_text,layout.hd_timer_spacing);
}
// Native ammunition controllers: five equipped slots and the special B item
// used by shooting galleries, bowling and horseback archery.
template<class M,class F> U32 FindAmmoCounter(M& m,F match,bool* pause=nullptr,bool* b_ammo=nullptr) {
    if(pause)*pause=false;
    if(b_ammo)*b_ammo=false;
    for(const U32 root:{Main+0x14,Action+0x40}) {
        const U32 owner=m.Read(root).value_or(0);
        if(!Pointer(owner,0x43c))continue;
        const unsigned kind=m.Read(owner+0x428).value_or(99);
        const unsigned count=root==Main+0x14?5:kind<2?std::min(m.Read(owner+0x224).value_or(0),5u):
            (kind==2 || kind==3 || kind==5 || kind==6)?1:0;
        for(unsigned i=0;i<count;++i) {
            const U32 counter=m.Read(owner+0xc+i*4).value_or(0);
            if(Pointer(counter,0x14) && match(counter)) {
                if(b_ammo)*b_ammo=root==Action+0x40;
                return counter;
            }
        }
    }
    const U32 counter=PauseMenu::FindItemCounter(m,[&](U32 counter,U32){return match(counter);});
    if(pause)*pause=counter!=0;
    return counter;
}
// Collapse only fresh upload positions. Source rectangles and offsets remain
// native, so every element is restored automatically when the game ends.
template<class M> void HideQuads(M& memory,U32 helper,unsigned first,unsigned count) {
    const auto total=memory.Read(helper);
    const U32 vertices=memory.Read(helper+0x10).value_or(0);
    if(!total || *total>256 || first>*total || count>*total-first ||
       !Pointer(vertices,*total*48))return;
    for(unsigned q=first;q<first+count;++q) {
        std::array<float,12> v;
        if(!TitleScreen::ReadFloats(memory,vertices+q*48,v))continue;
        for(unsigned i=1;i<4;++i)for(unsigned c=0;c<2;++c)
            memory.Write(vertices+q*48+i*12+c*4,std::bit_cast<U32>(v[c]));
    }
}
// misc/throbber.ctxb draws three animated layers from the same screen-space
// rectangle. Transform only that fresh rectangle; UV animation, alpha, screen
// selection and native visibility remain owned by the game.
template<class M> bool LoadingRect(M& memory, U32 controller, U32 rect, const Pose& pose) {
    if (!Valid(pose) || !Pointer(controller, 0x24) ||
        memory.Read(controller) != std::optional<U32>{0x4ed274}) return false;
    const auto flags = memory.Read(controller + 8);
    const auto visibility = memory.Read(controller + 0x20);
    if (!flags || (*flags & 255) != 1 || ((*flags >> 8) & 255) > 1 ||
        !visibility || ((*visibility >> 8) & 255) != 1) return false;
    std::array<float, 4> bounds;
    std::array<float, 2> center;
    if (!TitleScreen::ReadFloats(memory, rect, bounds) ||
        !TitleScreen::ReadFloats(memory, controller + 0x14, center)) return false;
    const float half = ((*flags >> 8) & 255) == 0 ? 8.f : 16.f;
    const std::array expected{center[1]-half, center[1]+half, center[0]-half, center[0]+half};
    for (unsigned i = 0; i < 4; ++i)
        if (std::abs(bounds[i] - expected[i]) > .001f) return false;
    // +0x20 selects the native screen: nonzero uses top queue 5; zero
    // uses bottom queue 6 (including the save indicator at 284,24).
    // Capture the unedited top-screen rectangle only, so both presentations
    // apply the shared user offsets/scale exactly once.
    if((*visibility & 255)!=0)
        OOT3D::LoadingIcon::ObserveTop({center[0],center[1],half});
    if (pose.x == 0 && pose.y == 0 && pose.scale == 1) return true;
    for (unsigned i = 0; i < 4; ++i) {
        const float anchor = center[i < 2 ? 1 : 0];
        const float offset = i < 2 ? pose.y : pose.x;
        memory.Write(rect + i * 4, std::bit_cast<U32>(anchor + offset + (bounds[i]-anchor)*pose.scale));
    }
    return true;
}
template<class M, class R> void Handle(M& memory, R& regs, unsigned site, const Layout& layout,
                                      RoutingState& routing, bool paused = false,
                                      std::int64_t now_us = 0) {
    if(site==9) {
        const auto xy=memory.Read(regs[4]+4).value_or(0);
        const int native_y=static_cast<std::int16_t>(xy>>16);
        regs[1]=static_cast<U32>(UserHUD::AreaTitleY(native_y,layout.reposition_area_titles,layout.area_title_y));
        return;
    }
    if(site==6) {
        regs[0]=memory.Read(regs[0]+0x214).value_or(0); // Displaced LDR.
        routing.drawing_top_model=regs[6]==HUDFade::Queue+5*4 &&
            regs[7]==HUDFade::Queue+5*0x60 ? regs[0] : 0;
        return;
    }
    if(site==7) {
        regs[0]=memory.Read(regs[6]+0x14).value_or(0); // Displaced LDR.
        routing.drawing_top_model=0;
        return;
    }
    if(site==8) {
        regs[1]=regs[13]+0x2c; // Displaced ADD; flags unchanged.
        const U32 owner=memory.Read(regs[4]+0x354).value_or(0);
        const U32 skulltula=memory.Read(SkulltulaCounter).value_or(0);
        if(Pointer(skulltula,0x14) && memory.Read(skulltula+0x10)==std::optional<U32>{owner}) {
            auto mesh=SubmittedNumberMesh(memory,owner);
            if(!mesh)mesh=Mesh(memory,memory.Read(skulltula+8).value_or(0),true);
            if(mesh)PublishNumberMesh(*mesh,NumberKind::Skulltula);
        }
        const U32 score=memory.Read(Action+0x30).value_or(0);
        if(Pointer(score,0x14) && memory.Read(score+0x10)==std::optional<U32>{owner})
            if(auto mesh=Mesh(memory,memory.Read(score+8).value_or(0),true))PublishNumberMesh(*mesh,NumberKind::Score);
        const U32 ammo=FindAmmoCounter(memory,[&](U32 c){return memory.Read(c+0x10)==std::optional<U32>{owner};});
        if(ammo) {
            auto mesh=SubmittedNumberMesh(memory,owner);
            // Non-rotating legacy helpers retain their existing registration.
            if(!mesh)mesh=Mesh(memory,memory.Read(ammo+8).value_or(0),true);
            if(mesh)PublishNumberMesh(*mesh,NumberKind::Ammo);
        }
        HUDFade::CaptureAction(memory,routing.fade,owner,regs[13]+0x38);
        routing.hud_return.Action(memory,owner,regs[13]+0x38,now_us,routing.fade);
        if(Rumble::enabled.load() && AuxiliaryModel(memory,owner)==ShardAgony) {
            memory.Write(regs[13]+0x38,std::bit_cast<U32>(0.f));return;
        }
        MinimapControl::Material(memory,owner,regs[13]+0x38,MinigameActive(memory)?0.f:MinimapControl::Opacity()*routing.minimap_fade.opacity);
        if(MinigameActive(memory)) {
            const auto number=[&](U32 root,U32 offset) {
                const U32 counter=memory.Read(root+offset).value_or(0);
                return Pointer(counter,0x14) && memory.Read(counter+0x10)==std::optional<U32>{owner};
            };
            if(memory.Read(Main+0xc)==std::optional<U32>{owner} || number(Main,0x1c) ||
               number(Main,0x20) || number(Action,0x2c) || number(Action,0x30) ||
               AuxiliaryModel(memory,owner)) {
                memory.Write(regs[13]+0x38,std::bit_cast<U32>(0.f));return;
            }
        }
        // The timer is a separate native number model drawn beside the
        // clock in A's mesh. Share A's current effective opacity, including
        // menu-return fades; retain native timer activation and digit colors.
        const U32 timer=memory.Read(Action+0x2c).value_or(0);
        const bool timer_draw=Pointer(timer,0x14) && memory.Read(timer+0x10)==std::optional<U32>{owner};
        // 41e23c draws the timer directly through 2fc950, outside QueueTop.
        // Observe its actual material submission, which covers direct draws too.
        if(timer_draw)
            if(auto mesh=Mesh(memory,memory.Read(timer+8).value_or(0),true))PublishTimerMesh(*mesh);
        if(timer_draw && HUDFade::CodeMatches(memory)) {
            const float envelope=routing.hud_return.hidden?0.f:
                routing.hud_return.returning?routing.hud_return.opacity:1.f;
            memory.Write(regs[13]+0x38,std::bit_cast<U32>(routing.fade.action_opacity*envelope));
        }
        const U32 model=routing.drawing_top_model;
        // 3f9680 draws model+8 (the renderer); 3fb9ac reads its owning
        // model from +0x354 before copying that model's material color.
        if(!model || memory.Read(regs[4]+0x354)!=std::optional<U32>{model})return;
        const U32 counter=memory.Read(Main+0x1c).value_or(0);
        const U32 keys=memory.Read(Main+0x20).value_or(0);
        if(memory.Read(Main+0xc)==std::optional<U32>{model} ||
           (Pointer(counter,0x14) && memory.Read(counter+0x10)==std::optional<U32>{model}) ||
           (Pointer(keys,0x14) && memory.Read(keys+0x10)==std::optional<U32>{model}))
            HUDFade::Apply(memory,regs[13]+0x38,routing.fade);
        return;
    }
    if (site == 5) {
        regs[3] = regs[13] + 0x44; // Displaced ADD r3,sp,#0x44; flags unchanged.
        LoadingRect(memory, regs[4], regs[3], layout[LoadingIcon]);
        return;
    }

    if (site == 3) {
        // Prepare both native number models before the top pass. Each return
        // resumes here; advance the stage so neither refresh can repeat.
        const unsigned state=memory.Read(Main+0x34).value_or(0);
        const bool visible=state!=0 && state!=0x17;
        if(routing.preparing_counter) {
            routing.preparing_counter=false;routing.counter_ready=true;routing.counter_stage=1;
        }
        if(routing.preparing_keys) {
            routing.preparing_keys=false;routing.keys_ready=true;routing.counter_stage=2;
        }
        if(routing.counter_stage==0)routing.counter_ready=routing.keys_ready=false;
        while(routing.counter_stage<2) {
            const bool key=routing.counter_stage==1;
            const U32 counter=memory.Read(Main+(key?0x20:0x1c)).value_or(0);
            const bool allowed=visible && (key?state<0xc:
                (routing.hud_return.returning || !(state>=0xc && state<0x14)));
            if(allowed && Pointer(counter,0x14) && Pointer(memory.Read(counter+8).value_or(0),0x20)) {
                if(key)routing.preparing_keys=true;else routing.preparing_counter=true;
                regs[0]=counter;regs[14]=Sites[3].pc;regs[15]=0x2fc950;return;
            }
            ++routing.counter_stage;
        }
        routing.counter_stage=0;
        regs[0] = memory.Read(regs[4] + 0x2c).value_or(0);
        return;
    }
    if (site == 4) {
        if(regs[0]==HUDFade::Queue && regs[1]==3) {
            routing.hud_return.Track(memory,paused,now_us);
            HUDFade::Begin(routing.fade);
        }
        if (regs[0] == 0x5c0ba8 && regs[1] == 5) {
            HUDFade::Update(memory,routing.fade,paused || routing.hud_return.hidden,now_us,
                routing.hud_return.returning?std::optional<float>{routing.hud_return.opacity}:std::nullopt);
            const unsigned state = memory.Read(Main + 0x34).value_or(0);

        if (!paused && state != 0 && state != 0x17) {
            if (auto mesh = Mesh(memory, memory.Read(Main + 4).value_or(0))) {
                mesh->routed = QueueTop(memory, memory.Read(Main + 0xc).value_or(0));
                PublishMesh(*mesh);
            }
            for(unsigned i=0;i<2;++i) {
                const bool ready=i?routing.keys_ready:routing.counter_ready;
                if(!ready || (i && state>=0xc))continue;
                const U32 counter=memory.Read(Main+(i?0x20:0x1c)).value_or(0);
                if (auto mesh = Mesh(memory, memory.Read(counter + 8).value_or(0))) {
                    mesh->routed = QueueTop(memory, memory.Read(counter + 0x10).value_or(0));
                    mesh->hd_counter = true; // Rupees and small keys share the HD toggle.
                    PublishMesh(*mesh);
                }
            }
        }
        }
        regs[1] *= 3; // Displaced ADD r1,r1,r1,LSL #1.
        return;
    }
    constexpr std::array<U32, 3> offsets{4, 0x18, 8};
    if (site >= offsets.size()) return;
    const auto helper = memory.Read(regs[4] + offsets[site]);
    regs[0] = helper.value_or(0); // Displaced LDR r0,[r4,#offset].
    if (!helper || !Pointer(*helper, 0x20)) return;
    bool pause_ammo=false,b_ammo=false;
    if (site == 0 && regs[4] == Main && memory.Read(*helper) == std::optional<U32>{92}) {
        CarrotAnimation::Publish(memory,paused);
        routing.hud_return.Track(memory,paused,now_us);
        if(routing.hud_return.returning || routing.hud_return.hidden || MountedHUD(memory)) {
            HUDReturn::Stationary(memory,*helper,59,24);
        }
        if(routing.hud_return.returning) {
            HUDReturn::Stationary(memory,*helper,26,1);
        }
        Quads(memory, *helper, 63, 20, TopPose(layout[Hearts], Hearts), 82, 2, 320,
              std::isfinite(layout.heart_spacing)?std::clamp(layout.heart_spacing,-8.f,20.f):0);
        Quads(memory, *helper, 59, 4, TopPose(layout[Magic], Magic), 78, 28, 320);
        Quads(memory, *helper, 26, 1, TopPose(layout[Rupees], Rupees), 2, 111, 320);
        Quads(memory, *helper, 27, 1, TopPose(layout[Keys], Keys), 2, 151, 320);
        // 0x444544 updates six carrot quads (0x56..0x5b) from play+0x2e18.
        // Source rectangles are (88+24*i,8)..(112+24*i,32). Transform the
        // whole row around its center; preserve native used/unused UVs/fades.
        const float carrot_spacing=std::isfinite(layout.carrot_spacing)?
            std::clamp(layout.carrot_spacing,-20.f,40.f):0;
        const auto carrot_pose=TopPose(layout[Carrots], Carrots);
        Quads(memory, *helper, 86, 6, carrot_pose, 160, 20, 320, carrot_spacing,
              {-2.5f*carrot_spacing*carrot_pose.scale,0});
    } else if (site == 1 && regs[4] == Action &&
               memory.Read(*helper) == std::optional<U32>{8}) {
        // Circle and localized action label share this pair of quads.
        const auto action_pose=layout[AButton];
        // 42b360's native mode table sends fishing (3) and horseback (5)
        // to 42b380, raising both A quads by 35. Cancel that mode offset
        // before the user's ordinary transform, preserving the press animation.
        const bool raised=memory.Read(Action+0x48)==3 || MountedHUD(memory);
        Quads(memory, *helper, 0, 2, action_pose, ActionAnchorX, ActionAnchorY,400,0,
              {0,raised?35*action_pose.scale:0});
        if(UsesBLayout(memory)) {
            const auto button=SpecialBButton(memory);
            const auto p=button.pose;
            if(!button.enabled)HideQuads(memory,*helper,7,1);
            else if(std::isfinite(p.x) && std::abs(p.x)<=500 &&
                    std::isfinite(p.y) && std::abs(p.y)<=240)
                Quads(memory,*helper,7,1,{0,0,p.scale},336,215.5f,400,0,
                      {p.x-336,p.y-215.5f},4);
        }
        // Dedicated Navi label: native atlas cell (64,112), 64x18.
        Quads(memory, *helper, 4, 1, layout.navi_text, 160, 137);
        // The player and entrance arrows are quads 2/3 in the action atlas,
        // separate from the map image and its chest/item sprite models.
        const auto map_pose=MinimapControl::LayoutPose(memory,layout);
        if(Valid(map_pose)) {
            const auto p=MinimapControl::Place(memory,map_pose,layout.minimap_lock_corner);
            // The corner correction may exceed the editor's offset range for
            // large native maps. It is derived from validated geometry.
            Quads(memory, *helper, 2, 2, map_pose, p.ax, p.ay,400,0,
                  {p.x-map_pose.x,p.y-map_pose.y});
        }
        // Native 41decc hides an inactive timer by setting clock-quad alpha
        // to zero (its geometry stays on screen). Modulate that visibility by
        // A's fade; replacing it would reveal a clock with no active timer.
        const U32 colors=memory.Read(*helper+0x18).value_or(0);
        if(Pointer(colors,8*64)) {
            std::array<float,16> circle,clock;
            if(TitleScreen::ReadFloats(memory,colors,circle) && circle[3]>=0 && circle[3]<=1 &&
               circle[3]==circle[7] && circle[3]==circle[11] && circle[3]==circle[15] &&
               TitleScreen::ReadFloats(memory,colors+5*64,clock) && clock[3]>=0 && clock[3]<=1 &&
               clock[3]==clock[7] && clock[3]==clock[11] && clock[3]==clock[15])
                for(unsigned v=0;v<4;++v)
                    memory.Write(colors+5*64+v*16+12,std::bit_cast<U32>(clock[3]*circle[3]));
        }
        Quads(memory, *helper, 5, 1, layout[Timer], TimerAnchorX, TimerAnchorY);
        Quads(memory, *helper, 6, 1, layout[Minigame], MinigameAnchorX, MinigameAnchorY);
        if(MinigameActive(memory)) {
            // The remaining special-mode action quads are not displayed.
            HideQuads(memory,*helper,0,7);
        }
    } else if(site==2 && memory.Read(Action+0x2c)==std::optional<U32>{regs[4]}) {
        TimerNumberLayout(memory,*helper,layout);
    } else if(site==2 && memory.Read(SkulltulaCounter)==std::optional<U32>{regs[4]}) {
        // Center visible digits for either font; HD-only edits stay optional.
        if(layout.hd_numbers && layout.hd_skulltula_numbers)
            // Native three-digit field starts at 112 (46734c/2fcc88), but
            // parks unused leading digits. Center only the visible run on
            // the token's x=128 before the shared gear-page transformations.
            NumberLayout(memory,*helper,layout.hd_skulltula_text,layout.hd_skulltula_spacing,true,128.f);
        else
            NumberLayout(memory,*helper,Pose{},0,true,128.f);
    } else if(site==2 && memory.Read(Action+0x30)==std::optional<U32>{regs[4]}) {
        if(layout.hd_numbers && layout.hd_score_numbers)
            NumberLayout(memory,*helper,layout.hd_score_text,layout.hd_score_spacing);
    } else if(site==2 && FindAmmoCounter(memory,[&](U32 c){return c==regs[4];},&pause_ammo,&b_ammo)) {
        if(layout.hd_numbers && layout.hd_ammo_numbers)
            NumberLayout(memory,*helper,pause_ammo?layout.hd_pause_ammo_text:layout.hd_ammo_text,layout.hd_ammo_spacing,true);
        if(b_ammo && UsesBLayout(memory)) {
            const auto p=HUDButtons::GetEffectiveLayout().b_ammo;
            NumberLayout(memory,*helper,{p.x,p.y,p.scale},0,true);
        }
    } else if (site == 2 && (memory.Read(Main + 0x1c) == std::optional<U32>{regs[4]} ||
                            memory.Read(Main + 0x20) == std::optional<U32>{regs[4]})) {
        const bool key=memory.Read(Main+0x20)==std::optional<U32>{regs[4]};
        if(!key && routing.hud_return.returning)
            HUDReturn::RupeeDigits(memory,regs[4],*helper);
        const auto count = memory.Read(*helper);
        if (count && *count <= 4) {
            Quads(memory, *helper, 0, *count, TopPose(layout[key?Keys:Rupees], key?Keys:Rupees), 2, key?151:111, 320);
            if(key)KeyNumberLayout(memory,*helper,layout);
            else RupeeNumberLayout(memory,*helper,layout);
        }
    }
}
} // namespace OOT3D::HUD
