// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"
#include "oot3d/map_location.h"
#include "oot3d/widescreen.h"
#include "oot3d/guest_ui_mesh.h"
#include "oot3d/dungeon_location.h"
#include <chrono>

namespace OOT3D::PauseHeader {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff090, Tick=0x3004d8, Entry=0x300564, BannerOwner=0x4fc648;
constexpr U32 Items=0x5066f8, AllocationSize=0x500, Magic=0x4f484453;
constexpr U32 Descriptor=0x1c0, Positions=0x2e0, UVs=0x370, Colors=0x3d0, Indices=0x490;
constexpr U32 Cookie=0x4c0, Quads=3, PositionBytes=Quads*48, UVBytes=Quads*32;
constexpr float Height=40.f, TileWidth=240.f*Height/64.f, Duration=PauseMenu::DefaultTransitionSeconds;
inline std::atomic<std::int64_t> diagnostic_end_ms{0};
inline std::atomic<unsigned> diagnostic_lines{0};
inline std::int64_t DiagnosticTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void ArmDiagnostics() {diagnostic_lines=0;diagnostic_end_ms=DiagnosticTime()+12000;}
inline bool DiagnosticActive() {
    const auto end=diagnostic_end_ms.load();
    return end!=0 && DiagnosticTime()<end;
}
inline bool DiagnosticLine() {return DiagnosticActive() && diagnostic_lines.fetch_add(1)<2400;}
constexpr std::array<TitleScreen::Site,10> Signatures{{
    {Tick,0xe320f000,0xeb04ad3b},
    {Entry,0xe320f000,0xebfffe34},
    {0x2fd84c,0xe59f2080,0xe52de004},
    {0x313ce0,0xe92d4010,0xeb00a84a},
    {0x348f34,0xe92d4070,0xe3a05000},
    {0x34897c,0xe92d41f0,0xe1b06001},
    {0x317d1c,0xe92d41f0,0xe1a04000},
    {0x2a8bdc,0xe1a02000,0xe1a00001},
    {0x3f9680,0xe59f1030,0xe5902178},
    {0x2f9934,0xe92d41f0,0xe1a04000},
}};
// The native five-frame exit helpers otherwise park/remove panels before a
// longer custom slide is finished. Keep their page state alive until it ends.
constexpr std::array<TitleScreen::Site,4> CloseSites{{
    {0x433e38,0xe320f000,0xeb004515}, // Items, before its exit helper.
    {0x4249f8,0xe320f000,0xeb004ef7}, // Gear.
    {0x42d538,0xe320f000,0xeb005615}, // Dungeon map.
    {0x42d550,0xed9f8a40,0xe3a04000}, // Overworld map's inline exit loop.
}};
// Both native upload and submission stop when map cleanup clears owner+38.
// Keep their rendering alive through unpause without reviving the controller.
constexpr U32 CarryFirstSVC=FirstSVC+2+CloseSites.size();
constexpr std::array<TitleScreen::Site,2> CarrySites{{
    {0x42cd48,0xe5940038,0xe3500000},
    {0x42d9f8,0xe5940038,0xe3500000},
}};
// State 9 already refreshes the world-map data, but native state 1 waits
// until after the entrance to create the location banner and preview.
constexpr U32 LocationSetupSVC=CarryFirstSVC+CarrySites.size();
constexpr TitleScreen::Site LocationSetupSite{0x42d364,0xed9f8abb,0xe3a04000};
constexpr U32 BackdropSVC=LocationSetupSVC+1;
constexpr TitleScreen::Site BackdropSite{0x41ebe8,0xebfc537e,0xe28dd03c};
constexpr U32 DungeonEntrySVC=BackdropSVC+1;
constexpr TitleScreen::Site DungeonEntrySite{0x42d32c,0xe320f000,0xeb005836};
constexpr U32 FloorFirstSVC=DungeonEntrySVC+1;
// Map cleanup calls the shared floor setter; the following gameplay-map
// update also writes that field through three map branches.
constexpr std::array<TitleScreen::Site,4> FloorSites{{
    {0x2e6670,0xe5810038,0xe12fff1e},
    {0x42c2e8,0xe5860038,0xe3a00000},
    {0x42c540,0xe5860038,0xe3a00000},
    {0x42c444,0xe5867038,0xe320f000},
}};
constexpr U32 LastSVC=FloorFirstSVC+FloorSites.size()-1;
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<FloorSites.size();++i)
        if(pc==FloorSites[i].pc && word==FloorSites[i].instruction)return 0xef000000|(FloorFirstSVC+i);
    if(pc==DungeonEntrySite.pc && word==DungeonEntrySite.instruction)return 0xef000000|DungeonEntrySVC;
    if(pc==BackdropSite.pc && word==BackdropSite.instruction)return 0xef000000|BackdropSVC;
    if(pc==LocationSetupSite.pc && word==LocationSetupSite.instruction)
        return 0xef000000|LocationSetupSVC;
    if(pc==Tick && word==Signatures[0].instruction)return 0xef000000|FirstSVC;
    if(pc==0x3f9680 && word==0xe59f1030)return 0xef000000|(FirstSVC+1);
    for(unsigned i=0;i<CloseSites.size();++i)
        if(pc==CloseSites[i].pc && word==CloseSites[i].instruction)return 0xef000000|(FirstSVC+2+i);
    for(unsigned i=0;i<CarrySites.size();++i)
        if(pc==CarrySites[i].pc && word==CarrySites[i].instruction)return 0xef000000|(CarryFirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:FloorSites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    if(m.Read(DungeonEntrySite.pc)!=DungeonEntrySite.instruction ||
       m.Read(DungeonEntrySite.pc+4)!=DungeonEntrySite.following)return false;
    if(m.Read(BackdropSite.pc)!=std::optional<U32>{BackdropSite.instruction} ||
       m.Read(BackdropSite.pc+4)!=std::optional<U32>{BackdropSite.following} ||
       m.Read(0x42e5bc)!=std::optional<U32>{0x42f064} ||
       m.Read(0x42e5a4)!=std::optional<U32>{0x42ed18} || // Main state 14 opens Save.
       m.Read(0x43ae74)!=std::optional<U32>{SaveMenu::Owner})return false;
    if(m.Read(LocationSetupSite.pc)!=std::optional<U32>{LocationSetupSite.instruction} ||
       m.Read(LocationSetupSite.pc+4)!=std::optional<U32>{LocationSetupSite.following} ||
       m.Read(0x441668)!=std::optional<U32>{0xe92d4070} ||
       m.Read(0x44166c)!=std::optional<U32>{0xe1a05000})return false;
    for(const auto& s:Signatures)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    for(const auto& s:CloseSites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    for(const auto& s:CarrySites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return PauseMenu::CodeMatches(m);
}
struct Slide {
    float progress=0;
    void Advance(bool open,float seconds,float duration=Duration) {
        const float step=std::clamp(seconds,0.f,.05f)/PauseMenu::TransitionSeconds(duration);
        progress=std::clamp(progress+(open?step:-step),0.f,1.f);
    }
    float Y(float distance=Height) const {return -distance*(1-progress*progress*(3-2*progress));}
};
inline float Travel(const PauseMenu::Layout& layout) {
    const auto& title=layout[PauseMenu::Titles];
    const float shadow=layout.header_shadow.opacity>0?8.f*std::clamp(layout.header_shadow.width,.1f,4.f):0;
    return std::max({Height,Height+layout.header_y+shadow,24.f+title.y+16.f*title.scale});
}
inline float SlotsTravel(const PauseMenu::Layout& layout) {
    const auto& slot_pose=layout[PauseMenu::Slots];
    const float shadow=layout.slots_shadow.opacity>0?8.f*std::clamp(layout.slots_shadow.width,.1f,4.f):0;
    float bottom=layout.horizontal?32.f:120.f;
    for(unsigned slot=0;slot<4;++slot)
        bottom=std::max(bottom,PauseMenu::SlotCenter(slot,layout)[1]-120+(slot==1 || slot==2?26.f:28.f)*layout.slot_content_scale);
    return std::max(0.f,138+slot_pose.y+PauseMenu::Scale*slot_pose.scale*(bottom+shadow));
}
inline float PanelTravel(const PauseMenu::Layout& layout,bool wide) {
    const auto& bg=layout[PauseMenu::Background];const auto& items=layout[PauseMenu::Items];
    const float edge=(400-(wide?240*Widescreen::Aspect:400))/2;
    const float shadow=layout.background_shadow.opacity>0?8.f*std::clamp(layout.background_shadow.width,.1f,4.f)*bg.scale:0;
    const float background_right=200+128*bg.scale+bg.x+shadow;
    const float items_right=200+PauseMenu::Scale*(-32+124*items.scale)+items.x;
    const auto& map=layout[PauseMenu::Map];
    const float map_right=200+PauseMenu::Scale*(-32+192*map.scale)+map.x;
    float gear_right=0;
    for(const auto& gear:{layout.gear_panel,layout.gear_panel_lower})
        gear_right=std::max(gear_right,200+160*PauseMenu::Scale*gear.width+gear.x);
    const auto& frame=layout.map_frame;
    const float frame_right=frame.enabled ? 200+PauseMenu::Scale*200*frame.cut_scale+frame.cut_x : 0;
    float map_background_right=0;
    for(const auto& b:layout.map_backgrounds)
        map_background_right=std::max(map_background_right,200+128*b.pose.scale+b.pose.x);
    const float map_shadow=layout.map_side_shadows?8*std::clamp(layout.map_shadow.width,.1f,4.f)*PauseMenu::Scale*map.scale:0;
    const float extension=layout.map_extension.enabled?std::clamp(layout.map_extension.width,0.f,400.f):0;
    float dungeon_right=0;
    constexpr std::array<float,7> dungeon_extent{104,160,40,42,96,40,42};
    for(unsigned i=PauseMenu::DungeonMap;i<=PauseMenu::DungeonFloors;++i) {
        const auto& p=layout[i];const auto a=PauseMenu::Anchors[i];
        const float shadow=i==PauseMenu::DungeonRight && layout.dungeon_right_shadow.opacity>0?
            8*std::clamp(layout.dungeon_right_shadow.width,.1f,4.f):0;
        dungeon_right=std::max(dungeon_right,200+PauseMenu::Scale*(a[0]-160+
            (dungeon_extent[i-PauseMenu::DungeonMap]+shadow)*p.scale)+p.x);
    }
    for(unsigned side=0;side<2;++side) {
        const auto& b=layout.dungeon_blocks[side];const auto p=layout.map_backgrounds[side].pose;
        if(b.enabled && b.opacity>0)
            dungeon_right=std::max(dungeon_right,200+p.x+(side?-128:128)*p.scale+b.x+(side?0:std::clamp(b.width,0.f,600.f)));
    }
    if(layout.dungeon_edge_shadows && layout.dungeon_edge_shadow.opacity>0) {
        const auto p=layout[PauseMenu::DungeonMap];
        dungeon_right=std::max(dungeon_right,200+104*PauseMenu::Scale*p.scale+p.x+8*std::clamp(layout.dungeon_edge_shadow.width,.1f,4.f));
    }
    float gear_overlay_right=0;
    for(unsigned i=PauseMenu::GearLeft;i<=PauseMenu::GearBottomRight;++i) {
        const auto& p=layout[i];const auto a=PauseMenu::Anchors[i];
        gear_overlay_right=std::max(gear_overlay_right,200+PauseMenu::Scale*(a[0]-160+100*p.scale)+p.x);
    }
    return std::max({0.f,background_right-edge,items_right-edge,map_right+std::max(map_shadow,extension)-edge,
        map_background_right-edge,gear_right-edge,gear_overlay_right-edge,frame_right-edge,dungeon_right-edge})+1;
}
inline float BottomTravel(const PauseMenu::Layout& layout) {
    float distance=0;
    for(unsigned i=PauseMenu::Back;i<=PauseMenu::ItemsTab;++i) {
        const auto part=static_cast<PauseMenu::Part>(i);
        auto p=PauseMenu::Move(PauseMenu::Anchors[i][0],198,part,layout[i]);
        p=PauseMenu::Move(p[0],p[1],PauseMenu::Tabs,layout[PauseMenu::Tabs]);
        distance=std::max(distance,240-PauseMenu::OffsetY-PauseMenu::Scale*p[1]);
    }
    return distance+1;
}
inline std::array<float,Quads*12> Vertices(float y,bool wide) {
    const float width=wide?240*Widescreen::Aspect:400;
    const float left=(400-width)/2;
    std::array<float,Quads*12> result{};
    for(unsigned q=0;q<Quads;++q) {
        const float x=left+q*TileWidth,right=left+std::min((q+1)*TileWidth,width);
        const std::array<float,12> v{x,y+Height,0,x,y,0,right,y+Height,0,right,y,0};
        std::copy(v.begin(),v.end(),result.begin()+q*12);
    }
    return result;
}
inline std::array<float,Quads*8> TileUVs(const std::array<float,8>& atlas,bool wide) {
    const float width=wide?240*Widescreen::Aspect:400;
    std::array<float,2> lo{1,1},hi{0,0};
    for(unsigned v=0;v<4;++v)for(unsigned axis=0;axis<2;++axis) {
        lo[axis]=std::min(lo[axis],atlas[v*2+axis]);
        hi[axis]=std::max(hi[axis],atlas[v*2+axis]);
    }
    std::array<float,Quads*8> result{};
    for(unsigned q=0;q<Quads;++q) {
        const float fraction=std::clamp((width-q*TileWidth)/TileWidth,0.f,1.f);
        auto uv=atlas;
        for(unsigned i=0;i<4;++i)uv[4+i]=atlas[i]+(atlas[4+i]-atlas[i])*fraction;
        // The shared header is a rotated 240x64 crop of the 256x256 item
        // atlas. At each repeat, native edge UVs let linear filtering read
        // the neighboring sprite (or wrap at V=1), producing a thin line.
        // Clamp to this crop's texel centers AFTER trimming the last tile,
        // keeping its interior endpoint and the entire layout unchanged.
        constexpr float half_texel=.5f/256;
        for(unsigned i=0;i<uv.size();++i) {
            const unsigned axis=i%2;
            if(hi[axis]-lo[axis]>=2*half_texel)
                uv[i]=std::clamp(uv[i],lo[axis]+half_texel,hi[axis]-half_texel);
        }
        std::copy(uv.begin(),uv.end(),result.begin()+q*8);
    }
    return result;
}
// After the routed pause panels, draw the tiled header and banner together.
// Their buffers and positions are independent of the item slot bar.
struct Hook {
    enum class Stage { Idle, Allocate, Mesh, Model, Upload, Texture, ColorUpload, Draw, Banner, Foreground, DungeonPaper, DungeonTitle, DungeonIconDraw };
    Stage stage=Stage::Idle;
    U32 block=0,model=0,source_model=0;
    std::array<U32,16> saved{};
    std::int64_t last_time=0;
    Slide slide;
    Interpolation::MenuPage last_tab=Interpolation::MenuPage::None;
    bool map_open_entrance=false;
    float location_entry_alpha=1;
    bool presenting=false,bottom_projection=false;
    float offset=-Height;
    std::array<float,8> atlas{};
    std::array<U32,512> foreground{};
    unsigned foreground_count=0,foreground_index=0;
    bool location_setup_pending=false;
    std::array<U32,16> location_setup_regs{};
    std::array<U32,64> location_setup_floats{};
    GuestUI::Hook<DungeonLocation::PaperGeometry,256,256,13> dungeon_paper;
    GuestUI::Hook<DungeonLocation::TitleGeometry,DungeonLocation::Width,DungeonLocation::Height> dungeon_title;
    using IconGeometry=PausePrompts::QuadGeometry<4>;
    GuestUI::Hook<IconGeometry,1,1,0,true> dungeon_icons;
    bool dungeon_drawn=false;
    unsigned dungeon_index=0;
    bool ForegroundGeometryDrawing() const {return dungeon_paper.Drawing() || dungeon_title.Drawing() || dungeon_icons.Drawing();}
    template<class M,class R> void Backdrop(M& m,R& r,bool supported) const {
        r[14]=BackdropSite.pc+4; // Displaced BL to the native fullscreen tint.
        // Controller state 20 prepares a map opening from gameplay. It sets
        // pause active before 2f8708 starts the map's entrance (state 9).
        // Do not enqueue the dimming rectangle during those empty frames.
        // State 21 is the exit; tab changes do not use state 20.
        const bool preparing=supported && PauseMenu::Active(m) && PauseMenu::MapPage(m) &&
            m.Read(0x50af34+0x34)==20 && m.Read(0x506cb0+0x38)==0;
        // Main state 14 waits for the HUD transition before 0x42edcc calls
        // 0x43ac88 to activate Save. During this wait Save+28 is still zero:
        // native tint must not precede the prompt's fade (captured four frames).
        const bool save_preparing=supported && PauseMenu::Active(m) &&
            m.Read(0x50af34+0x34)==14 && m.Read(SaveMenu::Owner+0x28)==0;
        // The Save backdrop already includes this tint and fades with its
        // prompt. Suppress only this duplicate; scene/quit blackouts are separate.
        const bool save_backdrop=supported && (m.Read(SaveMenu::Owner+0x28)==1 ||
            SaveMenu::QuitTransition(m) || (SaveMenu::animate && SaveMenu::BackdropActive(m)));
        if(!preparing && !save_preparing && !save_backdrop)r[15]=0x3339e8;
    }
    template<class M> void EnterDungeon(M& m,bool supported) const {
        // Run the native entrance helper's final step on the first update.
        // It clears all 50 atlas offsets and reveals the separate icon models;
        // normal setup below it still parks unavailable items/floors correctly.
        if(supported && PauseMenu::Active(m) && PauseMenu::DungeonPage(m) &&
           m.Read(0x506cb0+0x38)==7 && m.Read(0x506cb0+0x44))
            m.Write(0x506cb0+0x44,0);
    }
    template<class M,class R,class F> void PrepareLocation(M& m,R& r,F& f,bool supported) {
        if(location_setup_pending) {
            r=location_setup_regs;f=location_setup_floats;location_setup_pending=false;
        } else if(supported && PauseMenu::Active(m) && PauseMenu::MapPage(m) &&
                  !PauseMenu::Closing(m) && m.Read(0x4fda6c+0x50)==0 &&
                  m.Read(0x506cb0+0x38)==9 && m.Read(MapLocation::Owner+0x38)==0 &&
                  TitleScreen::Pointer(r[6],0x108)) {
            const auto scene=m.Read(r[6]+0x104).value_or(0)&0xffff;
            if(scene>=0x51 && scene<=0x64) {
                // Same native, idempotent setup used by state 1, following
                // the same map-data refresh. Keep native cleanup ownership.
                location_setup_regs=r;location_setup_floats=f;location_setup_pending=true;
                r[0]=r[6];r[14]=LocationSetupSite.pc;r[15]=0x441668;return;
            }
        }
        f[16]=m.Read(0x42d658).value_or(0); // Displaced VLDR; next CMP replaces flags.
    }
    template<class M,class R,class F> void CloseStep(M& m,R& r,F& f,unsigned site,bool supported) {
        if(site==3)f[16]=m.Read(0x42d658).value_or(0); // Displaced VLDR s16.
        if(!supported || !PauseMenu::Closing(m))return;
        const bool finished=slide.progress<=0;
        if(site==3)r[15]=finished?0x42d5b8:0x42d678;
        else {r[0]=finished?1:0;r[15]=CloseSites[site].pc+8;}
    }
    template<class M,class R> void CarryMap(M& m,R& r,bool supported) const {
        r[0]=m.Read(r[4]+0x38).value_or(0); // Displaced LDR, no controller writes.
        if(supported && r[4]==0x506cb0 && (PauseMenu::world_exit_hold || PauseMenu::dungeon_exit_hold) &&
           PauseMenu::Active(m) && PauseMenu::Closing(m) && PauseMenu::MapPage(m) &&
           r[0]==0)r[0]=1;
    }
    template<class M,class R> void FloorStep(M& m,R& r,unsigned site,bool supported) const {
        const U32 owner=r[site==0?1:6];
        // Preserve the shared selected floor only while its pause artwork is
        // still on-screen. Native gameplay writes resume as soon as pause ends;
        // no cached floor or delayed write can leak into another room/session.
        // The cleanup can run before the presentation tick latches exit_hold.
        // Use the accepted close and live map type, even after owner+38 clears.
        // 42c0a4 clears owner+50 at 42c1d8 BEFORE choosing its map
        // branch. At both dungeon floor stores it is still zero; only the
        // following instructions restore it to one. Identify those stores
        // by their guarded call sites, not this transient map-type field.
        const bool dungeon_write=site==1 || site==2 || m.Read(owner+0x50)==1;
        const bool hold=supported && owner==0x4fda6c &&
            PauseMenu::Active(m) && PauseMenu::Closing(m) && PauseMenu::MapPage(m) &&
            dungeon_write &&
            m.Read(owner+0x38).value_or(0xffffffff)<16;
        if(!hold)m.Write(owner+0x38,r[site==3?7:0]); // Displaced STR.
    }
    bool ShowContents() const {return slide.progress>0 || PauseMenu::world_exit_hold || PauseMenu::dungeon_exit_hold;}
    template<class M> void Update(M& m,std::int64_t time,bool supported) {
        foreground_count=foreground_index=0;
        const bool active=PauseMenu::Active(m);
        PauseMenu::floor_growth_animation.Update(time,supported && active && PauseMenu::DungeonPage(m),
            PauseMenu::Closing(m),m.Read(0x506cb0+0x30).value_or(5),PauseMenu::DungeonFloorRows(m),
            PauseMenu::GetLayout().dungeon_floor_growth);
        if(!supported || !active) {last_tab=Interpolation::MenuPage::None;slide={};map_open_entrance=false;location_entry_alpha=1;presenting=false;last_time=time;offset=-Height;PauseMenu::motion_x=0;PauseMenu::motion_y=0;PauseMenu::motion_slots=0;PauseMenu::motion_bottom=0;PauseMenu::world_location_y=0;PauseMenu::world_panel_y=0;PauseMenu::world_location_alpha=1;PauseMenu::world_exit_hold=false;PauseMenu::dungeon_exit_hold=false;return;}
        // Native menu-open byte changes on the accepted B/Back action, before
        // the brown panel and remaining pause widgets finish closing.
        const bool open=active && (m.Read(0x54ac4c).value_or(0)>>24)!=0;
        const float elapsed=last_time && time>=last_time?float(time-last_time)/1000000.f:1.f/30;
        const auto layout=PauseMenu::GetLayout();
        const bool world_page=PauseMenu::MapPage(m) &&
            m.Read(0x4fda6c+0x50)==std::optional<U32>{0};
        const bool dungeon_page=PauseMenu::MapPage(m) && m.Read(0x4fda6c+0x50)==1;
        // Opening pause directly onto either map gets an entrance.
        // Leaving the page cancels it for the rest of this pause session.
        if(!presenting)map_open_entrance=world_page || dungeon_page;
        if(!world_page && !dungeon_page)map_open_entrance=false;
        const auto tab=PauseMenu::PresentationPage(m);
        // Both tab controllers can be idle while the next page is being built.
        // Only count Map as a destination once its controller is active.
        if(open && tab!=Interpolation::MenuPage::None &&
           (tab!=Interpolation::MenuPage::Map || m.Read(0x506cb0+0x38).value_or(0))) {
            if(last_tab!=Interpolation::MenuPage::None && tab!=last_tab) {
                slide.progress=1;
                map_open_entrance=false;
            }
            last_tab=tab;
        }
        last_time=time;slide.Advance(open,elapsed,layout.transition_seconds);
        presenting=active || slide.progress>0;
        offset=slide.Y(Travel(layout));
        PauseMenu::motion_y=active?offset:0;
        // Share time/easing, not distance: a low item-slot bar must not keep
        // the short green banner offscreen for most of its entrance/exit.
        PauseMenu::motion_slots=active?slide.Y(SlotsTravel(layout)):0;
        const bool world_closing=!open && world_page;
        if(!world_closing)PauseMenu::world_exit_hold=false;
        else if(m.Read(0x506cb0+0x38).value_or(0)!=0)PauseMenu::world_exit_hold=true;
        if(open || !dungeon_page)PauseMenu::dungeon_exit_hold=false;
        else if(m.Read(0x506cb0+0x38).value_or(0)!=0)PauseMenu::dungeon_exit_hold=true;
        // Map art appears at rest immediately on opening and stays there
        // through unpause. Header and location-banner motion remain independent.
        PauseMenu::motion_x=world_page || dungeon_page?0:slide.Y(PanelTravel(layout,Widescreen::Active()));
        // Use the green header's exact clock and easing, without waiting for
        // model creation or starting a second timer. Late models join the
        // current phase; tab changes show the location widgets at rest.
        const float remaining=slide.progress*slide.progress*(3-2*slide.progress);
        if(open)location_entry_alpha=map_open_entrance?remaining:1;
        const float entered=location_entry_alpha;
        const bool map_closing=!open && (world_page || dungeon_page);
        const float visibility=map_closing?std::min(remaining,entered):entered;
        PauseMenu::world_location_y=world_page || dungeon_page?-16.f*(1-visibility):0;
        PauseMenu::world_panel_y=world_page?16.f*(1-entered)-(world_closing?16.f*(1-remaining):0):0;
        PauseMenu::world_location_alpha=world_page || dungeon_page?visibility:1;
        PauseMenu::motion_bottom=dungeon_page?0:active?-slide.Y(BottomTravel(layout)):0;
    }
    template<class M> bool LocationVisible(M& m) const {
        return PauseMenu::MapPage(m) && (!PauseMenu::Closing(m) ||
            (!PauseMenu::DungeonPage(m) && slide.progress>0));
    }
    template<class M> bool DungeonDescription(M& m) const {
        const auto message=m.Read(ScreenModels::Message+0xf40).value_or(0);
        return PauseMenu::DungeonPage(m) && message>=0x774 && message<=0x776;
    }
    template<class M> bool ApplyForegroundDescription(M& m,U32 view,U32 candidate) const {
        if(!ScreenModels::MessageElement(m,candidate))return false;
        Textboxes::Layout box;
        if(DungeonDescription(m)) {
            const auto p=PauseMenu::GetLayout()[PauseMenu::DungeonDescription];
            box={p.x,p.y,1,{},p.scale};
        } else if(SaveMenu::OptionsActive(m)) {
            const auto p=SaveMenu::GetLayout()[SaveMenu::HelpMessage];
            box={p.x,p.y,Textboxes::GetLayout().width,{},p.scale};
        } else return false;
        // Deferred top-screen messages are now submitted under the pause
        // pass's projection. Undo its 320/400 X stretch before the editor pose.
        ScreenModels::Apply(m,view,candidate,{0,0,bottom_projection?.8f:1.f,1,0,0,true});
        if(auto transform=ScreenModels::Select(m,candidate,box,{}))
            ScreenModels::Apply(m,view,candidate,*transform);
        return true;
    }
    template<class M> bool DungeonIcons(M& m,U32 candidate) const {
        if(!PauseMenu::Active(m) || !PauseMenu::MapPage(m))return false;
        const auto mesh=m.Read(candidate+4).value_or(0);
        const auto icons=m.Read(0x506cb0+0x20).value_or(0);
        return m.Read(icons+8)==std::optional<U32>{candidate} &&
            PauseMenu::DungeonQuadPart(m,mesh,3,4)==PauseMenu::DungeonSkulltula;
    }
    template<class M> std::optional<IconGeometry> DungeonIconGeometry(M& m,U32 model) const {
        if(!DungeonIcons(m,model))return {};
        const U32 mesh=m.Read(model+4).value_or(0),desc=m.Read(mesh).value_or(0);
        const U32 current=m.Read(mesh+0x124).value_or(2);
        const U32 live=current<2?m.Read(mesh+0x1a0+current*4).value_or(0):0;
        if(m.Read(desc+12)!=16 || !TitleScreen::Pointer(live,16*36))return {};
        IconGeometry g;std::array<float,12> matrix;
        if(!TitleScreen::ReadFloats(m,live,g.positions) ||
           !TitleScreen::ReadFloats(m,live+16*12,g.uvs) ||
           !TitleScreen::ReadFloats(m,live+16*20,g.colors) ||
           !TitleScreen::ReadFloats(m,model+12,matrix))return {};
        for(unsigned v=0;v<16;++v) {
            const auto* p=g.positions.data()+v*3;
            const float x=matrix[0]*p[0]+matrix[1]*p[1]+matrix[2]*p[2]+matrix[3];
            const float y=matrix[4]*p[0]+matrix[5]*p[1]+matrix[6]*p[2]+matrix[7];
            g.positions[v*3]=200+(x-160)*PauseMenu::Scale;
            g.positions[v*3+1]=120+(y-120)*PauseMenu::Scale+PauseMenu::OffsetY;
            g.positions[v*3+2]=0;
            if(v>=12)g.colors[v*4+3]*=PauseMenu::world_location_alpha.load();
        }
        g.count=4;return g;
    }
    template<class M> bool ForegroundModel(M& m,U32 candidate) const {
        // 4422e0 opens native messages 774..776 for the dungeon's owned
        // boss key, compass and map. Their top-screen box must be submitted
        // after the routed parchment, just like the Options help box.
        const bool dungeon_description=DungeonDescription(m);
        return PauseMenu::Active(m) &&
            ((LocationVisible(m) && MapLocation::Select(m,candidate)) || DungeonIcons(m,candidate) ||
             (!PauseMenu::Closing(m) && (SaveMenu::OptionsActive(m) || dungeon_description) &&
              ScreenModels::MessageElement(m,candidate)));
    }
    template<class M,class R> void NativeDraw(M& m,R& r,bool supported) {
        r[1]=0x54c8c8; // Displaced LDR r1,[pc,#0x30].
        if(supported && PauseMenu::Closing(m) && ScreenModels::MessageElement(m,r[0],false)) {
            r[15]=r[14];return;
        }
        // Keep overworld location models through the shared closing animation.
        // Suppress stale native draws after closing or on another pause page.
        if(supported && PauseMenu::Active(m) && MapLocation::Select(m,r[0]) &&
           !LocationVisible(m)) {
            r[15]=r[14];return;
        }
        // Queue only models the game actually submitted this frame. This keeps
        // native visibility and draws lettering after its paper/box. Options
        // help must also follow the fullscreen dark overlay and menu controls.
        if(supported && presenting && stage!=Stage::Foreground &&
           ForegroundModel(m,r[0]) && foreground_count<foreground.size()) {
            if(std::find(foreground.begin(),foreground.begin()+foreground_count,r[0])==
               foreground.begin()+foreground_count && foreground_count<foreground.size())
                foreground[foreground_count++]=r[0];
            r[15]=r[14];return;
        }
        if(supported && presenting && (stage!=Stage::Banner || SaveMenu::HideHeader(m)) &&
           m.Read(BannerOwner+0x28)==std::optional<U32>{r[0]})r[15]=r[14];
    }
    template<class M> bool Owned(M& m,U32 source) const {
        return TitleScreen::Pointer(block,AllocationSize) && TitleScreen::Pointer(model,0x1e4) &&
            m.Read(block+Cookie)==std::optional<U32>{Magic} &&
            m.Read(block+Cookie+4)==std::optional<U32>{source} &&
            m.Read(block+Cookie+8)==std::optional<U32>{model} &&
            m.Read(model)==std::optional<U32>{0x4ebd60} &&
            m.Read(model+4)==std::optional<U32>{block};
    }
    template<class M> bool Prepare(M& m) {
        const U32 mesh=m.Read(source_model+4).value_or(0);
        const U32 descriptor=m.Read(mesh).value_or(0);
        const U32 helper=m.Read(Items+0x24).value_or(0);
        const U32 uv=m.Read(helper+0x14).value_or(0);
        if(!TitleScreen::Pointer(block,AllocationSize) || !TitleScreen::Pointer(descriptor,0x120) ||
           m.Read(helper)!=std::optional<U32>{37} || !TitleScreen::Pointer(uv,37*32))return false;
        std::array<U32,0x120/4> desc{};
        std::array<U32,8> tex{};
        for(unsigned i=0;i<desc.size();++i) {
            const auto word=m.Read(descriptor+i*4);if(!word)return false;desc[i]=*word;
        }
        for(unsigned i=0;i<tex.size();++i) {
            const auto word=m.Read(uv+12*32+i*4);if(!word)return false;tex[i]=*word;
        }
        for(U32 i=0;i<AllocationSize;i+=4)if(!m.Read(block+i))return false;
        desc[0]=block+Positions;desc[1]=block+UVs;desc[2]=block+Colors;
        desc[3]=Quads*4;desc[4]=block+Indices;desc[5]=Quads*6;
        for(unsigned i=0;i<desc.size();++i)m.Write(block+Descriptor+i*4,desc[i]);
        for(unsigned i=0;i<tex.size();++i)atlas[i]=std::bit_cast<float>(tex[i]);
        for(unsigned i=0;i<Quads*16;++i)m.Write(block+Colors+i*4,std::bit_cast<U32>(1.f));
        for(U32 q=0;q<Quads;++q) {
            const U32 base=q*4,at=block+Indices+q*12;
            m.Write(at,base|((base+2)<<16));m.Write(at+4,(base+1)|((base+1)<<16));
            m.Write(at+8,(base+2)|((base+3)<<16));
        }
        SetVertices(m);
        return true;
    }
    template<class M> void SetVertices(M& m) {
        const auto layout=PauseMenu::GetLayout();
        const auto tint=PauseMenu::OverlayTint(PauseMenu::BannerColor(m,layout));
        for(unsigned i=0;i<Quads*16;++i)
            m.Write(block+Colors+i*4,std::bit_cast<U32>(i%4==3?1.f:tint[i%4]));
        const auto v=Vertices(offset+layout.header_y,Widescreen::Active());
        for(unsigned i=0;i<v.size();++i)m.Write(block+Positions+i*4,std::bit_cast<U32>(v[i]));
        const auto uv=TileUVs(atlas,Widescreen::Active());
        for(unsigned i=0;i<uv.size();++i)m.Write(block+UVs+i*4,std::bit_cast<U32>(uv[i]));
    }
    template<class M> bool FinishModel(M& m,U32 created) {
        if(!TitleScreen::Pointer(created,0x1e4) || m.Read(created)!=std::optional<U32>{0x4ebd60})return false;
        const auto node=m.Read(created+8);if(!node)return false;
        std::array<U32,0x1e4/4> original{};
        for(unsigned i=0;i<original.size();++i) {
            const auto word=m.Read(source_model+i*4);if(!word || !m.Read(created+i*4))return false;
            original[i]=*word;
        }
        original[1]=block;original[2]=*node;original[0x174/4]=Quads*6;original[0x178/4]&=~0x60u;
        original[0x1dc/4]=0; // Borrow the live native material; never own/free it.
        constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
        for(unsigned i=0;i<identity.size();++i)original[3+i]=std::bit_cast<U32>(identity[i]);
        for(unsigned i=0;i<original.size();++i)m.Write(created+i*4,original[i]);
        model=created;m.Write(block+Cookie,Magic);m.Write(block+Cookie+4,source_model);m.Write(block+Cookie+8,model);
        return true;
    }
    template<class M,class R> void Handle(M& m,R& r,bool routed) {
        const auto call=[&](Stage next,U32 function,U32 a=0,U32 b=0,U32 c=0) {
            stage=next;r[0]=a;r[1]=b;r[2]=c;r[3]=0;r[14]=Entry;r[15]=function;
        };
        const auto finish=[&] {
            if(!dungeon_drawn) {
                dungeon_drawn=true;
                if(const auto index=DungeonLocation::Current(m);index && slide.progress>0 &&
                   TitleScreen::Pointer(source_model,0x1e4) && !SaveMenu::HideHeader(m)) {
                    dungeon_index=*index;stage=Stage::DungeonPaper;
                    const auto image=DungeonLocation::GetAssets();
                    dungeon_paper.Begin(m,r,Entry,DungeonLocation::Paper(PauseMenu::GetLayout(),
                        PauseMenu::world_location_alpha,PauseMenu::world_location_y),image->paper,source_model);
                    if(dungeon_paper.Pending())return;
                }
            }
            while(foreground_index<foreground_count) {
                const U32 next=foreground[foreground_index++];
                if(ForegroundModel(m,next)) {
                    if(const auto icons=DungeonIconGeometry(m,next)) {
                        stage=Stage::DungeonIconDraw;
                        dungeon_icons.Begin(m,r,Entry,*icons,{},next);
                        if(dungeon_icons.Pending())return;
                    }
                    call(Stage::Foreground,0x3f9680,next);return;
                }
            }
            foreground_count=foreground_index=0;r=saved;stage=Stage::Idle;
        };
        if(stage==Stage::DungeonIconDraw) {
            const bool drew=dungeon_icons.Drawing();
            if(dungeon_icons.Pending())dungeon_icons.Resume(m,r);
            if(dungeon_icons.Pending())return;
            if(!drew) {call(Stage::Foreground,0x3f9680,dungeon_icons.source);return;}
            finish();return;
        }
        if(stage==Stage::DungeonPaper) {
            if(dungeon_paper.Pending())dungeon_paper.Resume(m,r);
            if(dungeon_paper.Pending())return;
            if(const auto image=DungeonLocation::GetAssets()) {
                stage=Stage::DungeonTitle;
                dungeon_title.Begin(m,r,Entry,DungeonLocation::Title(PauseMenu::GetLayout(),dungeon_index,
                    PauseMenu::world_location_alpha,PauseMenu::world_location_y),image->titles,source_model);
                if(dungeon_title.Pending())return;
            }
            finish();return;
        }
        if(stage==Stage::DungeonTitle) {
            if(dungeon_title.Pending())dungeon_title.Resume(m,r);
            if(!dungeon_title.Pending())finish();
            return;
        }
        if(stage==Stage::Allocate) {
            block=r[0];if(!Prepare(m)){block=0;finish();return;}
            call(Stage::Mesh,0x348f34,block,block+Descriptor);return;
        }
        if(stage==Stage::Mesh) {
            // Texture samplers live outside the descriptor copied by the
            // constructor. Share atlas bindings, never its vertex/index buffers.
            const U32 source_mesh=m.Read(source_model+4).value_or(0);
            std::array<U32,24> bindings{};
            for(unsigned i=0;i<bindings.size();++i) {
                const auto value=m.Read(source_mesh+0x13c+i*4);
                if(!value){finish();return;}bindings[i]=*value;
            }
            for(unsigned i=0;i<bindings.size();++i)m.Write(block+0x13c+i*4,bindings[i]);
            const U32 material=m.Read(source_model+0x1dc).value_or(0);
            if(!TitleScreen::Pointer(material,0x234)){finish();return;}
            call(Stage::Model,0x34897c,0,block,material);return;
        }
        if(stage==Stage::Model) {
            if(!FinishModel(m,r[0])){finish();return;}
            call(Stage::Upload,0x36759c,block,PositionBytes,block+Positions);return;
        }
        if(stage==Stage::Upload) {call(Stage::Texture,0x317d1c,block,UVBytes,block+UVs);return;}
        if(stage==Stage::Texture) {call(Stage::ColorUpload,0x2f9934,block,Quads*64,block+Colors);return;}
        if(stage==Stage::ColorUpload) {
            // Upload writes the next native buffer; publish it as the model's
            // regular update does before submitting the draw.
            const U32 count=m.Read(block+0x128).value_or(0), current=m.Read(block+0x124).value_or(0);
            if(count<1 || count>2 || current>=count){finish();return;}
            if(!GuestUI::BindMaterial(m,model,source_model)){finish();return;}
            const U32 mesh=m.Read(source_model+4).value_or(0);
            for(U32 at=0x13c;at<0x19c;at+=4)m.Write(block+at,m.Read(mesh+at).value_or(0));
            m.Write(block+0x124,(current+1)%count);
            call(Stage::Draw,0x3f9680,model);return;
        }
        if(stage==Stage::Draw) {
            const U32 banner=m.Read(BannerOwner+0x28).value_or(0);
            if(TitleScreen::Pointer(banner,0x1e4) && m.Read(banner)==std::optional<U32>{0x4ebd60}) {
                call(Stage::Banner,0x3f9680,banner);return;
            }
            finish();return;
        }
        if(stage==Stage::Banner || stage==Stage::Foreground) {finish();return;}
        if(!presenting || !ShowContents()) {foreground_count=foreground_index=0;return;}
        bottom_projection=routed;
        dungeon_drawn=false;
        std::copy(r.begin(),r.end(),saved.begin());
        // The dungeon map survives the header's final slide frame until
        // native unpause completes. Its compass/map/boss-key model shares
        // the deferred Skulltula draw; drain that queue even with no banner.
        if(slide.progress==0 || SaveMenu::HideHeader(m)) {finish();return;}
        source_model=m.Read(Items+0x20).value_or(0);
        if(!TitleScreen::Pointer(source_model,0x1e4) || m.Read(source_model)!=std::optional<U32>{0x4ebd60}) {
            finish();return;
        }
        if(!Owned(m,source_model)) {
            block=model=0;call(Stage::Allocate,0x313ce0,AllocationSize);return;
        }
        SetVertices(m);call(Stage::Upload,0x36759c,block,PositionBytes,block+Positions);
    }
};
} // namespace OOT3D::PauseHeader
