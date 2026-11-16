// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include "oot3d/pause_ammo.h"
#include "oot3d/save_menu.h"
#include "oot3d/death_menu.h"
#include "oot3d/map_time_tint.h"
#include "oot3d/dungeon_location_assets.h"
#include "oot3d/file_particle_settings.h"
#include <atomic>
#include <mutex>
#include <vector>

namespace OOT3D::PauseMenu {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff070, UI=0x5043d4;
constexpr float Scale=.8f, OffsetY=18.f;
constexpr float DefaultTransitionSeconds=.1f/1.5f;
inline float TransitionSeconds(float value) {
    return std::isfinite(value)?std::clamp(value,.02f,2.f):DefaultTransitionSeconds;
}
inline float PreviewRotationSpeed(float value) {
    return std::isfinite(value)?std::clamp(value,.25f,5.f):1.f;
}
enum Part { Tabs, Items, Slots, Background, Back, GearTab, MapTab, ItemsTab,
    Titles, GearLeft, GearRight, GearBottomLeft, GearBottomRight, Description, Preview, Map,
    LocationMap, LocationBanner, DungeonMap, DungeonContents, DungeonLeft, DungeonRight, DungeonBanner, DungeonItems, DungeonFloors, WorldLocationPanel, DungeonDescription, DungeonLocationPaper, DungeonLocationTitle, DungeonSkulltula, PartCount };
constexpr std::array Names{"tabs","items","slots","background","back","gear_tab","map_tab","items_tab",
    "titles","gear_left","gear_right","gear_bottom_left","gear_bottom_right","description","preview","map",
    "location_map","location_banner","dungeon_map","dungeon_contents","dungeon_left","dungeon_right","dungeon_banner","dungeon_items","dungeon_floors","world_location_panel","dungeon_description","dungeon_location_paper","dungeon_location_title","dungeon_skulltula"};
constexpr std::array Labels{"All tabs (legacy)","Item panel","Item slot bar","Brown Triforce background",
    "B Back card","Gear tab","Map tab","Items tab","Page titles (Gear / Map / Items)",
    "Gear: upper left overlays","Gear: upper right overlays","Gear: lower left overlays","Gear: lower right overlays",
    "Item description textbox","Item preview model","Map","Map: location preview","Map: Current Location banner","Map parchment","Rooms and markers","Left stone background","Right stone background","Temple-name banner","Left items and keys","Right floors","Map: location-name panel","Dungeon item description","Dungeon: Current Location parchment","Dungeon: entry title artwork","Dungeon: banner Skulltula"};
struct Pose { float x=0,y=0,scale=1; };
struct ColorOverlay {
    std::array<float,3> color{1,1,1}; float opacity=0;
    bool operator==(const ColorOverlay&) const = default;
};
enum class PauseColor : unsigned {MapBanner,ItemsBanner,GearBanner,SlotBackground};
constexpr std::array ColorNames{"map_banner","items_banner","gear_banner","slot_background"};
constexpr std::array ColorLabels{"Map banner","Items banner","Gear banner","Item-slot background"};
using ColorScheme=std::array<ColorOverlay,4>;
struct ColorLayouts {
    std::array<ColorScheme,2> saved{};
    ColorScheme working{};
    unsigned active=0;
    bool initialized=false;
    void Initialize(const ColorOverlay& banner,const ColorOverlay& slot_background) {
        if(initialized)return;
        working={banner,banner,banner,slot_background};saved={working,working};
        active=0;initialized=true;
    }
    bool Unsaved() const {return initialized && working!=saved[active];}
    void Select(unsigned index) {
        if(!initialized || index>=saved.size() || index==active)return;
        active=index;working=saved[active];
    }
    void Save() {if(initialized)saved[active]=working;}
};
inline std::array<float,3> OverlayTint(const ColorOverlay& overlay) {
    const float opacity=std::isfinite(overlay.opacity)?std::clamp(overlay.opacity,0.f,1.f):0;
    std::array<float,3> tint{};
    for(unsigned c=0;c<3;++c) {
        const float color=std::isfinite(overlay.color[c])?std::clamp(overlay.color[c],0.f,1.f):1;
        tint[c]=1+opacity*(color-1);
    }
    return tint;
}
struct Shadow { float width=1, opacity=.45f; };
struct MapFramePiece { float x=0,y=0,scale=1; };
struct MapFrame {
    bool enabled=true; float x=0,y=0,width=1,height=1; float left=0,right=0,top=0,bottom=0;
    std::array<MapFramePiece,12> pieces{}; // Legacy settings; no longer used for rendering.
    float cut_x=0,cut_y=0,cut_scale=1,darkness=0;
};
struct MapBackground { Pose pose{}; float darkness=0; };
struct DungeonShadowBlock {
    bool enabled=true;
    float x=0,y=0,width=72,height=240,fade=40,opacity=.65f;
};
struct DungeonTopShadow {
    bool enabled=true;
    float height=32,opacity=.65f,y=0;
};
struct MapExtension { bool enabled=false; float width=80,blur=18,darkness=0; };
struct DungeonParchment { bool enabled=false; Pose pose{}; };
struct DungeonFloorLine {
    bool enabled=true;
    Pose pose{};
    float width=4,height=184,opacity=.8f,roundness=1;
};
struct WorldParchmentStack { bool enabled=true; Pose pose{}; float spacing=88; };
struct MapTopFade {
    bool enabled=false;
    float opacity=.65f,height=48,y=0,softness=1;
    std::array<float,3> color{0,0,0};
};
struct GearPanel { float x=0,y=0,width=1,height=1; };
inline std::array<GearPanel,2> SplitGearPanel(GearPanel original) {
    auto upper=original,lower=original;
    upper.height=lower.height=original.height*.5f;
    upper.y-=47*Scale*original.height;lower.y+=47*Scale*original.height;
    return {upper,lower};
}
struct ArrowShadow {bool enabled=true;float softness=8,x=0,y=2,strength=.45f;};
struct PromptCorner {
    bool enabled=true;
    float strength=.55f,width=128,height=72,softness=.9f,x=0,y=0;
    std::array<float,3> color{0,0,0};
};
struct FloorGrowth {
    bool enabled=false;
    float size=1.25f,speed=1.f;
};
struct FloorGrowthAnimation {
    std::array<float,8> progress{}; // Five floor rows, then boss key/compass/map.
    std::int64_t last_time=0;
    bool started=false;
    void Update(std::int64_t time,bool active,bool closing,unsigned selected,unsigned rows,
                const FloorGrowth& settings) {
        if(!active || !settings.enabled) {*this={};return;}
        if(started && time<last_time) {*this={};}
        const float dt=started?std::clamp(float(time-last_time)/1000000.f,0.f,.1f):0;
        last_time=time;started=true;
        if(closing)return;
        const float step=dt*(std::isfinite(settings.speed)?std::clamp(settings.speed,.1f,5.f):1)/.15f;
        for(unsigned row=0;row<progress.size();++row) {
            if(row<5 && !(rows&(1u<<row)))progress[row]=0;
            else progress[row]=std::clamp(progress[row]+(row==selected?step:-step),0.f,1.f);
        }
    }
    float ScaleFor(unsigned row,const FloorGrowth& settings) const {
        if(!settings.enabled || row>=progress.size() || !std::isfinite(settings.size))return 1;
        const float t=progress[row];
        return 1+(std::clamp(settings.size,1.f,2.f)-1)*t*t*(3-2*t);
    }
};
// Updated on the emulation thread by the shared pause presentation clock.
inline FloorGrowthAnimation floor_growth_animation;
struct Layout : std::array<Pose,PartCount> {
    ArrowShadow arrow_shadow{};
    ArrowShadow tab_shadow{true,3,0,1,.5f};
    bool horizontal=false, stack_background=false;
    float header_y=0, background_darkness=0;
    float transition_seconds=DefaultTransitionSeconds;
    float preview_rotation_speed=1.f;
    float selection_grow_seconds=0;
    float slot_content_scale=1.f, slot_spacing=1.f, slot_content_x=0;
    Shadow header_shadow{}, slots_shadow{}, background_shadow{}, items_shadow{};
    std::array<Shadow,2> gear_shadows{};
    ColorOverlay header_overlay{}, slots_overlay{};
    ColorLayouts color_layouts{};
    ColorOverlay dungeon_left_overlay{}, dungeon_right_overlay{};
    Shadow dungeon_left_shadow{}, dungeon_right_shadow{};
    float dungeon_brightness=1, map_darkness=0;
    FileParticles::Settings dungeon_particles{.enabled=false};
    bool dungeon_particles_progress=true;
    FileParticles::Progression dungeon_particle_endpoints{};
    PromptCorner dungeon_prompt_vignette;
    PromptCorner world_prompt_vignette{.enabled=false};
    bool world_map_time_tint=false;
    float world_skulltula_x=0,world_skulltula_y=0,world_skulltula_scale=1;
    std::array<DungeonParchment,3> dungeon_parchments{};
    float parchment_blend=18;
    std::array<WorldParchmentStack,3> world_parchments{{{true,{-80,0,1},88},{true,{80,0,1},88},{true,{0,0,1},88}}};
    float world_parchment_blend=18,world_parchment_darkness=0;
    float world_map_shadow_opacity=.35f,world_map_shadow_width=8;
    MapTopFade world_map_top_fade{};
    MapTopFade dungeon_map_top_fade{};
    bool world_map_outline=true;
    std::array<float,2> dungeon_background_darkness{0,0};
    std::array<DungeonShadowBlock,2> dungeon_blocks{};
    DungeonTopShadow dungeon_top_shadow{};
    bool dungeon_edge_shadows=true;
    Shadow dungeon_edge_shadow{2,.45f};
    std::array<float,2> dungeon_spacing{1,1};
    DungeonFloorLine dungeon_floor_line{};
    bool hide_dungeon_floor_stones=false;
    FloorGrowth dungeon_floor_growth{};
    float dungeon_cursor_scale=1;
    std::array<bool,2> gear_vertical{};
    GearPanel gear_panel=SplitGearPanel({})[0], gear_panel_lower=SplitGearPanel({})[1];
    MapFrame map_frame{};
    std::array<MapBackground,2> map_backgrounds{{{{-128,0,1},0},{{128,0,1},0}}};
    bool map_side_shadows=false;
    Shadow map_shadow{};
    MapExtension map_extension{};
};
inline bool ValidSlotContents(const Layout& l) {
    return std::isfinite(l.slot_content_scale) && l.slot_content_scale>=.25f && l.slot_content_scale<=2 &&
        std::isfinite(l.slot_spacing) && l.slot_spacing>=.25f && l.slot_spacing<=2 &&
        std::isfinite(l.slot_content_x) && std::abs(l.slot_content_x)<=240;
}
inline std::mutex layout_mutex;
inline Layout layout{};
inline std::atomic<bool> dirty{false};
inline Layout GetLayout() { std::scoped_lock lock{layout_mutex}; return layout; }
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{layout_mutex};layout=value;if(save)dirty=true;
}
inline ColorOverlay Color(const Layout& value,PauseColor part) {
    if(value.color_layouts.initialized)return value.color_layouts.working[unsigned(part)];
    return part==PauseColor::SlotBackground?value.slots_overlay:value.header_overlay;
}
template<class M> ColorOverlay BannerColor(M& m,const Layout& value) {
    // Follow the native page visibility through its opening/closing animation.
    const auto part=m.Read(0x5066f8+0x34).value_or(0)!=0?PauseColor::ItemsBanner:
        m.Read(0x50446c+0x18).value_or(0)!=0?PauseColor::GearBanner:PauseColor::MapBanner;
    return Color(value,part);
}
inline bool Valid(const Pose& p) {
    return std::isfinite(p.x) && std::abs(p.x)<=400 && std::isfinite(p.y) && std::abs(p.y)<=240 &&
           std::isfinite(p.scale) && p.scale>=.1f && p.scale<=3;
}
constexpr std::array<std::array<float,2>,PartCount> Anchors{{
    {128,220},{128,100},{288,120},{160,120},{28,220},{100,220},{160,220},{220,220},
    {200,24},{79,78},{241,78},{79,174},{241,174},{200,190},{200,100},{128,100},
    {200,104},{200,200},{160,120},{160,120},{32,120},{288,120},{160,20},{32,120},{288,120},{252,172},{200,190},{200,200},{200,200},{80,20}}};
constexpr bool IsDungeon(Part p) {return p>=DungeonMap && p<=DungeonFloors;}
constexpr bool IsTab(Part p) {return p>=Back && p<=ItemsTab;}
// Map is reached from Items with R, and returns with L. Its bottom card row
// is hidden; the Map card is also removed from the Items and Gear pages.
constexpr bool TabVisible(Part p,bool items,bool gear) {
    return !IsTab(p) || (p!=MapTab && (items || gear));
}
constexpr bool IsLowerGear(Part p) {return p==GearBottomLeft || p==GearBottomRight;}
constexpr bool IsGearOverlay(Part p) {return p>=GearLeft && p<=GearBottomRight;}
inline float GearSlotX(Part p,unsigned slot) {
    // The final left-hand group is the Skulltula count, centered beside its icon.
    if(p==GearBottomLeft && slot==3)return 128.f;
    return (p==GearBottomLeft?27.f:185.f)+36.f*slot;
}
inline unsigned GearSlot(Part p,float x) {
    return static_cast<unsigned>(std::clamp(std::floor((x-GearSlotX(p,0)+18)/36),0.f,3.f));
}
inline unsigned Slot(float y) {return y<62?0:y<120?1:y<180?2:3;}
constexpr std::array<std::array<float,2>,4> SlotCenters{{{292,28},{296,94},{284,146},{292,212}}};
// Move upright slot art inside the rail; the long green background is separate.
inline std::array<float,2> SlotCenter(unsigned slot,const Layout& l) {
    const auto c=SlotCenters[slot];
    return l.horizontal?std::array{288+(c[1]-120)*l.slot_spacing+l.slot_content_x,120.f}:
        std::array{c[0]+l.slot_content_x,120+(c[1]-120)*l.slot_spacing};
}
inline std::array<float,2> SlotPoint(float x,float y,unsigned slot,const Layout& l) {
    const auto c=SlotCenters[slot],target=SlotCenter(slot,l);
    return {target[0]+(x-c[0])*l.slot_content_scale,target[1]+(y-c[1])*l.slot_content_scale};
}
inline std::array<float,2> Move(float x,float y,Part part,const Pose& p) {
    const auto a=Anchors[part];
    return {a[0]+(x-a[0])*p.scale+p.x/Scale,a[1]+(y-a[1])*p.scale+p.y/Scale};
}
// Screen-pixel displacement shared by the header/slots and background/items.
// Published once after native updates, before any routed pause draw.
inline std::atomic<float> motion_x{0}, motion_y{0}, motion_slots{0}, motion_bottom{0};
// Overworld location labels descend/fade on entry and rise/fade on exit.
// The map and parchment remain at their resting positions.
inline std::atomic<float> world_location_y{0}, world_location_alpha{1};
inline std::atomic<float> world_panel_y{0};
// Retain overworld art through the native unpause phase after the shared slide.
// This controls drawing only; the map controller still closes normally.
inline std::atomic<bool> world_exit_hold{false};
inline std::atomic<bool> dungeon_exit_hold{false};
// Skulltula offsets are local to the parchment; the whole group follows its pose.
inline std::array<float,2> MoveDungeonSkulltula(float x,float y,const Layout& l) {
    const auto paper=l[DungeonLocationPaper],icon=l[DungeonSkulltula];
    const float top_x=200+(-92+icon.x+(x-80)*Scale*icon.scale)*paper.scale+paper.x;
    const float top_y=200+(22+icon.y+(y-20)*Scale*icon.scale)*paper.scale+paper.y;
    return {160+(top_x-200)/Scale,120+(top_y-120-OffsetY)/Scale};
}
inline std::array<float,2> Motion(Part part) {
    if(part==DungeonSkulltula)return {0,world_location_y.load()};
    return {part==Items || part==Map || part==WorldLocationPanel || part==Background || IsDungeon(part) || IsGearOverlay(part)?motion_x.load():0,
            part==WorldLocationPanel?world_panel_y.load():part==Slots?motion_slots.load():part==Titles || part==DungeonBanner?motion_y.load():IsTab(part)?motion_bottom.load():0};
}
inline std::array<float,2> AnimatedMove(float x,float y,Part part,const Pose& p) {
    auto v=Move(x,y,part,p);const auto d=Motion(part);
    return {v[0]+d[0]/Scale,v[1]+d[1]/Scale};
}
// Native entrance offsets belong to the plate and slot rail only. Icon meshes
// already use resting coordinates; never subtract the plate's offset from them.
template<class M> void RestingQuad(M& m,U32 mesh,unsigned q,std::array<float,12>& v) {
    // Gear panes and the bottom cards must use the same clock as their custom
    // background. Remove only the native entrance offsets, not parked widgets
    // or tab-switch animations. Exit helpers are replaced by the shared slide.
    const bool items=m.Read(0x5066f8+0x1c)==std::optional<U32>{mesh};
    const bool gear=m.Read(0x50446c)==std::optional<U32>{mesh};
    const U32 state=m.Read(items?0x5066f8+0x34:0x50446c+0x18).value_or(0);
    if((items && state==13) || (gear && state==9)) {
        const U32 helper=m.Read(items?0x5066f8+0x24:0x50446c+0xc).value_or(0);
        const U32 count=items?37:41;
        const U32 offsets=m.Read(helper+0x1c).value_or(0);
        std::array<float,2> d{};
        if(q<count && m.Read(helper)==std::optional<U32>{count} &&
           TitleScreen::ReadFloats(m,offsets+q*8,d) && std::abs(d[0])<=200 && std::abs(d[1])<=50) {
            for(unsigned i=0;i<12;i+=3){v[i]-=d[0];v[i+1]-=d[1];}
            return;
        }
    }
    const U32 helper=m.Read(0x5066f8+0x24).value_or(0);
    if(m.Read(0x5066f8+0x1c)!=std::optional<U32>{mesh} ||
       m.Read(helper)!=std::optional<U32>{37} || !(q<2 || (q>=12 && q<=24)))return;
    const U32 offsets=m.Read(helper+0x1c).value_or(0);
    std::array<float,2> d{};
    if(!TitleScreen::ReadFloats(m,offsets+(q<2?0:12*8),d) ||
       std::abs(d[0])>256 || d[1]!=0)return;
    for(unsigned i=0;i<12;i+=3)v[i]-=d[0];
}
struct AnimatedQuad {
    U32 address; std::array<float,12> vertices; Part part;
    Part destination=PartCount;
    float progress=0;
    unsigned floor_row=8;
    std::array<float,2> floor_center{};
    bool dungeon_cursor=false;
};
inline std::array<float,2> QuadMotion(const AnimatedQuad& q) {
    auto d=Motion(q.part);
    if(q.destination!=PartCount) {
        const auto end=Motion(q.destination);
        for(unsigned i=0;i<2;++i)d[i]+=q.progress*(end[i]-d[i]);
    }
    return d;
}
inline std::array<float,12> GrowFloor(const AnimatedQuad& q,const Layout& poses) {
    auto v=q.vertices;
    float scale=floor_growth_animation.ScaleFor(q.floor_row,poses.dungeon_floor_growth);
    if(q.dungeon_cursor && std::isfinite(poses.dungeon_cursor_scale))
        scale*=std::clamp(poses.dungeon_cursor_scale,.25f,3.f);
    if(scale!=1)for(unsigned i=0;i<12;i+=3) {
        v[i]=q.floor_center[0]+(v[i]-q.floor_center[0])*scale;
        v[i+1]=q.floor_center[1]+(v[i+1]-q.floor_center[1])*scale;
    }
    return v;
}
inline std::atomic<bool> on_top{false}, item_page{false}, gear_page{false}, dungeon_page{false};
inline std::atomic<unsigned> dungeon_floor_rows{0};
constexpr std::array<TitleScreen::Site,22> Sites{{
    {0x300560,0xe1a00005,0xe320f000}, // End of top-screen UI queue.
    {0x300564,0xe320f000,0xebfffe34}, // Return from routed pause draw.
    {0x41982c,0xeb0014fe,0xe320f000}, // Original bottom UI draw.
    {0x3675e8,0xe1a01006,0xe8bd4070}, // Fresh position copy into native rotating vertex buffers.
    {0x425858,0xe1a01008,0xebfd325f}, // Fresh native item-preview matrix.
    {0x41ee30,0xe320f000,0xeb002e59}, // After the native brown background draw.
    {0x2ecb04,0xe320f000,0x0a00000d}, // Repeated directions while browsing items.
    {0x2ed128,0xe320f000,0x0a000005}, // Repeated directions while choosing an equip target.
    {0x43872c,0xe59f421c,0xe59f521c}, // Gear direction lookup, after native key repeat.
    {0x2f99e4,0xe1a01008,0xe8bd41f0}, // Fresh color upload before the native tail copy.
    {0x317dac,0xe1a01007,0xe8bd41f0}, // Fresh UV upload before the native tail copy.
    {0x41ee44,0xeb0041ce,0xe320f000}, // Gameplay HUD draw inside the shared bottom UI pass.
    {0x434c80,0xe5960060,0xe3500000}, // Native entrance/exit counter hides item contents.
    {0x434c8c,0xe5960084,0xe3500000}, // Additional icon delay after the native entrance.
    {0x41ee58,0xe320f000,0xeb001766}, // Before gear overlays: shared green panel.
    {0x2fae2c,0xe5905000,0xe3550000}, // Opaque model queue: load submitted model.
    {0x2fad48,0xe5905000,0xe3550000}, // Translucent model queue: load submitted model.
    {0x4453e4,0xe5951034,0xe5900024}, // Items tab visual-state dispatch.
    {0x438c54,0xe5951018,0xe590000c}, // Gear tab visual-state dispatch.
    {0x300568,0xebfffe34,0xe3a01001}, // After custom pause layers: button prompts.
    {0x4459c8,0xebfacc6c,0xe3a00002}, // Arrow chooser dims the complete Items atlas.
    {0x300624,0xe1a07000,0xe1a08001}, // Before queued models build their matrices: quit fade handoff.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction) return 0xef000000|FirstSVC+i;
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following}) return false;
    return m.Read(0x41ec2c)==std::optional<U32>{0xe92d40f0} &&
           m.Read(0x41eec8)==std::optional<U32>{UI};
}
template<class M> bool Active(M& m) {
    const U32 play=m.Read(UI+0xc).value_or(0);
    return !DeathMenu::Active(m) && TitleScreen::Pointer(play,0x104) &&
           m.Read(UI+0x14)==std::optional<U32>{1} &&
           m.Read(UI+0x20)==std::optional<U32>{0} &&
           m.Read(UI+0x24)==std::optional<U32>{1} &&
           (m.Read(play+0x100).value_or(0)&255)==3;
}
template<class M> bool MapPage(M& m) {
    return !SaveMenu::BackdropActive(m) && m.Read(0x5066f8+0x34)==std::optional<U32>{0} &&
        m.Read(0x50446c+0x18)==std::optional<U32>{0};
}
template<class M> bool DungeonPage(M& m) {
    return MapPage(m) && (m.Read(0x506cb0+0x38).value_or(0)!=0 ||
        (dungeon_exit_hold && Active(m))) &&
        m.Read(0x4fda6c+0x50)==std::optional<U32>{1};
}
// World-map constructor 0x480bb8: quads 12..14 are the three-piece
// location panel, 15 its localized name, 16 the key and 17 the Skulltula.
template<class M> bool WorldLocationQuad(M& m,U32 mesh,unsigned q,unsigned count) {
    if(count!=30 || q<12 || q>17 || !MapPage(m) || DungeonPage(m))return false;
    const U32 model=m.Read(0x506cb0+4).value_or(0),helper=m.Read(0x506cb0+0x10).value_or(0);
    return m.Read(0x506cb0)==mesh && m.Read(model)==0x4ebd60 &&
        m.Read(model+4)==mesh && m.Read(helper)==30;
}
inline std::array<float,2> MoveWorldLocation(float x,float y,const Layout& poses) {
    const auto a=Anchors[WorldLocationPanel];
    const auto center=Move(a[0],a[1],Map,poses[Map]),point=Move(x,y,Map,poses[Map]);
    const auto p=poses[WorldLocationPanel];
    if(p.x==0 && p.y==0 && p.scale==1)return point;
    return {center[0]+(point[0]-center[0])*p.scale+p.x/Scale,
        center[1]+(point[1]-center[1])*p.scale+p.y/Scale};
}
// The rooms and map markers are drawn separately from the 50-quad dungeon
// atlas. Their fresh view matrix gets the contents pose, never a second vertex
// transform. Gameplay marker owners at +0x20/+0x28 are deliberately excluded.
template<class M> bool DungeonContentModel(M& m,U32 model) {
    if(!DungeonPage(m) || !TitleScreen::Pointer(model,0x1e4) ||
       m.Read(model)!=std::optional<U32>{0x4ebd60})return false;
    const U32 floor=m.Read(0x4fda6c+0x38).value_or(0xffffffff);
    if(floor<16 && m.Read(0x55b450+4*floor)==std::optional<U32>{model})return true;
    for(U32 off:{0x24u,0x2cu}) {
        const U32 owner=m.Read(0x4fda6c+off).value_or(0);
        if(TitleScreen::Pointer(owner,8) && m.Read(owner+4)==std::optional<U32>{model})return true;
    }
    return false;
}
// Native 0044c94c parks unused chest markers at (400,0); 002f1444
// parks the boss marker at (400,240). Cull those quads before editor poses
// can bring them back onto the wider display. Only the two pause-map marker
// owners are eligible; gameplay minimaps and sidebar controls are separate.
template<class M> bool CopyDungeonMarkers(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    if(!Active(m) || !DungeonPage(m) || !bytes || bytes%48 || bytes>256*48 ||
       !TitleScreen::Pointer(src,bytes) || !TitleScreen::Pointer(dst,bytes))return false;
    U32 helper=0;
    for(U32 off:{0x24u,0x2cu}) {
        const U32 owner=m.Read(0x4fda6c+off).value_or(0);
        if(!TitleScreen::Pointer(owner,0x10))continue;
        const U32 model=m.Read(owner+4).value_or(0),candidate=m.Read(owner+8).value_or(0);
        if(TitleScreen::Pointer(model,0x1e4) && m.Read(model)==0x4ebd60 &&
           m.Read(model+4)==mesh && m.Read(candidate)==bytes/48)helper=candidate;
    }
    if(!helper)return false;
    const U32 offsets=m.Read(helper+0x1c).value_or(0);
    if(!TitleScreen::Pointer(offsets,bytes/6))return false;
    std::vector<std::array<float,12>> quads(bytes/48);
    for(unsigned q=0;q<quads.size();++q) {
        std::array<float,2> offset;
        auto& v=quads[q];
        if(!TitleScreen::ReadFloats(m,src+q*48,v) ||
           !TitleScreen::ReadFloats(m,offsets+q*8,offset))return false;
        if(offset[0]>=400 || offset[1]>=240)
            for(unsigned i=3;i<12;i+=3)std::copy_n(v.begin(),3,v.begin()+i);
    }
    for(U32 off=0;off<bytes;off+=4)if(!m.Read(dst+off))return false;
    for(unsigned q=0;q<quads.size();++q)for(unsigned i=0;i<12;++i)
        m.Write(dst+q*48+i*4,std::bit_cast<U32>(quads[q][i]));
    return true;
}
// The floor-plan image is a separate model from the parchment and markers.
// Tint its fresh material RGB so the map brightness control affects both map
// layers, preserving native fades and the visibility of markers and labels.
template<class M> bool DungeonRoomMaterial(M& m,U32 model,U32 rgba) {
    if(!Active(m) || !DungeonPage(m) || !TitleScreen::Pointer(model,0x1e4) ||
       m.Read(model)!=std::optional<U32>{0x4ebd60})return false;
    const U32 floor=m.Read(0x4fda6c+0x38).value_or(0xffffffff);
    if(floor>=16 || m.Read(0x55b450+4*floor)!=std::optional<U32>{model})return false;
    const float setting=GetLayout().dungeon_brightness;
    const float brightness=std::isfinite(setting)?std::clamp(setting,0.f,1.f):1;
    if(brightness==1)return false;
    std::array<float,4> color;
    if(!TitleScreen::ReadFloats(m,rgba,color))return false;
    for(unsigned c=0;c<3;++c)m.Write(rgba+c*4,std::bit_cast<U32>(color[c]*brightness));
    return true;
}
// Verified constructor 0x480e98 and layout tables at 0x506d60/0x506ef0.
// PartCount means a native bottom card/rail, hidden on the map page.
constexpr Part DungeonAtlasPart(unsigned q) {
    if(q==0)return DungeonMap;
    if(q==1)return DungeonLeft;
    if(q==2)return DungeonRight;
    if((q>=13 && q<=15) || (q>=17 && q<=19))return DungeonItems;
    if(q>=21 && q<=37)return DungeonFloors;
    if(q==16 || q==20)return DungeonBanner;
    return PartCount;
}
template<class M> std::optional<Part> DungeonQuadPart(M& m,U32 mesh,unsigned q,unsigned count) {
    if(!DungeonPage(m))return {};
    const auto matches=[&](U32 model) {
        return TitleScreen::Pointer(model,0x1e4) && m.Read(model)==std::optional<U32>{0x4ebd60} &&
            m.Read(model+4)==std::optional<U32>{mesh};
    };
    const U32 helper=m.Read(0x506cb0+0x14).value_or(0);
    if(count==50 && m.Read(helper)==std::optional<U32>{50} &&
       m.Read(0x506cb0+8)==std::optional<U32>{mesh} && matches(m.Read(0x506cb0+0xc).value_or(0)))
        return DungeonAtlasPart(q);
    const U32 icons=m.Read(0x506cb0+0x20).value_or(0);
    if(TitleScreen::Pointer(icons,0x228) && matches(m.Read(icons+8).value_or(0)) &&
       count==4 && m.Read(icons+0x224)==std::optional<U32>{4})return q==3?DungeonSkulltula:DungeonItems;
    const U32 keys=m.Read(0x506cb0+0x18).value_or(0);
    if(TitleScreen::Pointer(keys,0x14) && matches(m.Read(keys+0x10).value_or(0)))return DungeonItems;
    const U32 cursor=m.Read(0x506cb0+0x1c).value_or(0);
    if(TitleScreen::Pointer(cursor,8) && matches(m.Read(cursor+4).value_or(0))) {
        const U32 selected=m.Read(0x506cb0+0x30).value_or(8);
        return selected<5?DungeonFloors:selected<8?DungeonItems:PartCount;
    }
    return {};
}
template<class M> bool DungeonCursorMesh(M& m,U32 mesh) {
    if(!DungeonPage(m))return false;
    const U32 cursor=m.Read(0x506cb0+0x1c).value_or(0);
    const U32 model=m.Read(cursor+4).value_or(0);
    return TitleScreen::Pointer(cursor,8) && TitleScreen::Pointer(model,0x1e4) &&
        m.Read(model)==std::optional<U32>{0x4ebd60} && m.Read(model+4)==mesh;
}
// Space the row centers, preserving the size and internal offsets of each
// well, icon, number and cursor. The key row belongs to the left group too.
inline float DungeonRowCenter(Part part,unsigned row) {
    return part==DungeonFloors?27.f+46.f*row:row==3?178.f:33.f+50.f*row;
}
inline unsigned DungeonRow(Part part,float y) {
    const unsigned count=part==DungeonFloors?5:4;
    unsigned best=0;
    for(unsigned i=1;i<count;++i)
        if(std::abs(y-DungeonRowCenter(part,i))<std::abs(y-DungeonRowCenter(part,best)))best=i;
    return best;
}
// Native 0x44281c publishes the five selectable rows here after deciding
// which floors are available. Use that list, not the selected/actual floor
// or transient sprite alpha, so selection and fades cannot move the column.
template<class M> unsigned DungeonFloorRows(M& m) {
    unsigned mask=0;
    for(unsigned row=0;row<5;++row) {
        const auto available=m.Read(0x55b638+4*row);
        if(!available || *available>1)return 0;
        if(*available)mask|=1u<<row;
    }
    return mask;
}
inline float DungeonRowShift(Part part,unsigned row,const Layout& poses,unsigned floor_rows=0) {
    if(part!=DungeonItems && part!=DungeonFloors)return 0;
    const float spacing=poses.dungeon_spacing[part==DungeonFloors?1:0];
    const float factor=std::isfinite(spacing)?std::clamp(spacing,.2f,3.f):1;
    if(part==DungeonFloors && row<5 && (floor_rows&(1u<<row))) {
        const unsigned count=std::popcount(floor_rows&31u);
        const unsigned rank=std::popcount(floor_rows&((1u<<row)-1));
        // Stone centers are 28+46*row (0x4d4e94, native 52x40 stone).
        // Compact the available rows symmetrically about the floor line.
        const float center=120+46*(float(rank)-float(count-1)*.5f)*factor;
        const float line_y=Valid(poses.dungeon_floor_line.pose)?poses.dungeon_floor_line.pose.y:0;
        return center-(28+46.f*row)+line_y/(Scale*poses[DungeonFloors].scale);
    }
    return (DungeonRowCenter(part,row)-Anchors[part][1])*(factor-1);
}
template<class M> bool Closing(M& m) {
    const auto menu=m.Read(0x54ac4c);
    return Active(m) && menu && (*menu>>24)==0;
}
template<class M> bool PreviewModel(M& m,U32 model) {
    const U32 owner=m.Read(0x4fc648+0x34).value_or(0);
    return TitleScreen::Pointer(model,0xb0) &&
        m.Read(model)==std::optional<U32>{0x4ec018} && TitleScreen::Pointer(owner,0x120) &&
        (m.Read(owner+0xfc)==std::optional<U32>{model} ||
         m.Read(owner+0x100)==std::optional<U32>{model});
}
// Roots contain both references and ordinary state/flag words. Numeric pointer
// bounds alone admit flags like 0x04000000 and 0x06000000, which are unmapped.
// Validate speculative candidates against the process mappings before reading.
template<class M> bool OwnershipCandidate(M& m,U32 address,unsigned bytes) {
    if(!TitleScreen::Pointer(address,bytes))return false;
    if constexpr(requires {m.Mapped(address,bytes);})return m.Mapped(address,bytes);
    return true; // Sparse offline fixtures return nullopt for absent words.
}
// Shared counter ownership also identifies pause ammo for HD digits.
template<class M> bool OwnedItemCounter(M& m,U32 mesh) {
    return FindItemCounter(m,[&](U32,U32 model){return m.Read(model+4)==std::optional<U32>{mesh};})!=0;
}
// Match vertex-buffer owners through the native pause controllers, not textures
// shared with gameplay. The item atlas batches its grid, tabs and slot panel.
template<class M> bool Owned(M& m,U32 mesh) {
    for(U32 root:{0x5066f8u,0x50446cu,0x506cb0u,0x5093e4u,0x50a508u,0x5077f0u}) {
        for(U32 off=0;off<0xc0;off+=4) {
            const U32 p=m.Read(root+off).value_or(0);
            if(!OwnershipCandidate(m,p,0x14)) continue;
            const auto matches=[&](U32 model) {
                return OwnershipCandidate(m,model,0x1e4) && m.Read(model)==std::optional<U32>{0x4ebd60} &&
                       m.Read(model+4)==std::optional<U32>{mesh};
            };
            if(matches(p))return true;
            for(U32 slot:{4u,8u,0x10u}) if(matches(m.Read(p+slot).value_or(0)))return true;
        }
    }
    return OwnedItemCounter(m,mesh);
}
// These separate models are entirely inside the arrow popup. In particular,
// its lower cursor corners cross the native tab row when the bow is on row 4.
template<class M> bool ArrowChooserOpen(M& m) {
    return m.Read(0x5066f8+0x34).value_or(0)!=0 &&
        m.Read(0x5066f8+0x60).value_or(1)==0 &&
        m.Read(0x5066f8+0x38).value_or(0)>=2;
}
template<class M> bool ArrowChooserMesh(M& m,U32 mesh) {
    for(const auto [offset,slot]:std::array<std::array<U32,2>,3>{{{0x28,4},{0x40,8},{0x4c,4}}}) {
        const U32 owner=m.Read(0x5066f8+offset).value_or(0);
        if(!OwnershipCandidate(m,owner,slot+4))continue;
        const U32 model=m.Read(owner+slot).value_or(0);
        if(OwnershipCandidate(m,model,0x1e4) && m.Read(model)==0x4ebd60 &&
           m.Read(model+4)==mesh)return true;
    }
    return false;
}
template<class M> bool GearBackground(M& m,U32 mesh,unsigned q,unsigned count) {
    if(count!=41 || (q!=0 && q!=5 && q!=8 && q!=15))return false;
    const U32 model=m.Read(0x50446c+4).value_or(0),helper=m.Read(0x50446c+0xc).value_or(0);
    return m.Read(0x50446c)==std::optional<U32>{mesh} &&
        m.Read(model)==std::optional<U32>{0x4ebd60} && m.Read(model+4)==std::optional<U32>{mesh} &&
        m.Read(helper)==std::optional<U32>{41};
}
template<class M> bool PressedTabQuad(M& m,U32 mesh,unsigned q,unsigned count) {
    // These three-piece borders are used only while holding a page card down.
    // Native enter/exit updates overwrite their parked offsets as well, so the
    // resting visual-state dispatch alone cannot keep them hidden.
    const bool items=count==37 && q>=28 && q<=36;
    const bool gear=count==41 && q>=32 && q<=40;
    if(!items && !gear)return false;
    const U32 root=items?0x5066f8:0x50446c;
    const U32 model=m.Read(root+(items?0x20:4)).value_or(0);
    const U32 helper=m.Read(root+(items?0x24:0xc)).value_or(0);
    return m.Read(root+(items?0x1c:0))==std::optional<U32>{mesh} &&
        m.Read(model)==std::optional<U32>{0x4ebd60} &&
        m.Read(model+4)==std::optional<U32>{mesh} &&
           m.Read(helper)==std::optional<U32>{count};
}
struct ItemFlight {
    std::array<float,2> native_center{},center{};
    float scale=1,progress=0;
    Part from=Items,to=Slots;
};
// State 9 halves the remaining native distance each update (004343a8).
// State 10 is published on its last update, before those vertices upload.
// Remap the two endpoints, never classify the flying icon by its current X.
template<class M> std::optional<ItemFlight> Flight(M& m,U32 mesh,unsigned quad,
                                                 const Layout& poses) {
    constexpr U32 root=0x5066f8;
    const auto state=m.Read(root+0x34).value_or(0);
    if(state!=9 && state!=10)return {};
    const auto first=m.Read(root+0x50),second=m.Read(root+0x54);
    if(!first || !second || *first>=24 || *second>=24 || first==second)return {};
    const U32 owner=m.Read(root+0x3c).value_or(0);
    const auto matches=[&](U32 model) {
        return TitleScreen::Pointer(model,0x1e4) &&
            m.Read(model)==std::optional<U32>{0x4ebd60} &&
            m.Read(model+4)==std::optional<U32>{mesh};
    };
    if(!TitleScreen::Pointer(owner,0x534) ||
       m.Read(owner+0x224)!=std::optional<U32>{24})return {};
    int direction=-1;
    if(matches(m.Read(owner+8).value_or(0))) {
        if(quad==*first)direction=0;
        else if(quad==*second)direction=1;
    } else {
        // The selection glow follows the outbound icon; ammunition digits
        // have their own meshes, but must follow the same flight transform.
        const U32 glow=m.Read(root+0x44).value_or(0);
        if(quad==0 && TitleScreen::Pointer(glow,0x534) &&
           matches(m.Read(glow+8).value_or(0)))direction=0;
        for(unsigned i=0;i<2 && direction<0;++i) {
            const U32 counter=m.Read(owner+0xc+4*(i?*second:*first)).value_or(0);
            if(TitleScreen::Pointer(counter,0x14) &&
               matches(m.Read(counter+0x10).value_or(0)))direction=int(i);
        }
    }
    if(direction<0)return {};
    std::array<std::array<float,2>,3> points;
    for(unsigned i=0;i<3;++i) {
        const auto packed=m.Read(root+(i==2?0x14+4*direction:0xc+4*(i^direction)));
        if(!packed)return {};
        const unsigned x=*packed&0xffff,y=*packed>>16;
        if(x>=320 || y>=240)return {};
        points[i]={float(x)+21,float(y)+21}; // Center of native 42px icons.
    }
    const std::array<unsigned,2> indices{direction?*second:*first,direction?*first:*second};
    ItemFlight flight;flight.native_center=points[2];
    std::array<std::array<float,2>,2> targets;
    std::array<float,2> scales;
    for(unsigned i=0;i<2;++i) {
        const bool slot=indices[i]%6==5;
        const Part part=slot?Slots:Items;
        if(i==0)flight.from=part;else flight.to=part;
        auto p=points[i];
        if(slot)p=SlotPoint(p[0],p[1],indices[i]/6,poses);
        targets[i]=Move(p[0],p[1],part,poses[part]);
        scales[i]=poses[part].scale*(slot?poses.slot_content_scale:1);
    }
    const float dx=points[1][0]-points[0][0],dy=points[1][1]-points[0][1];
    const float length=dx*dx+dy*dy;
    if(length<.001f)return {};
    const float t=std::clamp(((points[2][0]-points[0][0])*dx+
                              (points[2][1]-points[0][1])*dy)/length,0.f,1.f);
    flight.progress=t;flight.scale=scales[0]+t*(scales[1]-scales[0]);
    for(unsigned i=0;i<2;++i)flight.center[i]=targets[0][i]+t*(targets[1][i]-targets[0][i]);
    return flight;
}
// Native state 11 expands two landing glows around their destination slots.
// Their left edges cross the old item/slot boundary as they grow, so classify
// them by the owning controller and target index rather than their bounds.
template<class M> std::optional<ItemFlight> LandingGlow(M& m,U32 mesh,unsigned quad,
                                                       const Layout& poses) {
    constexpr U32 root=0x5066f8;
    if(quad!=0 || m.Read(root+0x34)!=std::optional<U32>{11})return {};
    for(unsigned i=0;i<2;++i) {
        const U32 owner=m.Read(root+0x44+4*i).value_or(0);
        if(!TitleScreen::Pointer(owner,0x534))continue;
        const U32 model=m.Read(owner+8).value_or(0);
        if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=std::optional<U32>{0x4ebd60} ||
           m.Read(model+4)!=std::optional<U32>{mesh})continue;
        const unsigned index=m.Read(root+0xb8+4*i).value_or(24);
        if(index>=24)return {};
        const bool slot=index%6==5;
        ItemFlight f;f.from=f.to=slot?Slots:Items;
        // Native 0x2eb304 returns top-left; the pulse keeps the 42px center.
        f.native_center=slot?SlotCenters[index/6]:std::array{32.f+48*(index%6),28.f+48*(index/6)};
        auto p=f.native_center;
        if(slot)p=SlotPoint(p[0],p[1],index/6,poses);
        f.center=Move(p[0],p[1],f.from,poses[f.from]);
        f.scale=poses[f.from].scale*(slot?poses.slot_content_scale:1);
        return f;
    }
    return {};
}
template<class M> bool CopyPositions(M& m,U32 mesh,U32 dst,U32 src,U32 bytes,
        std::vector<AnimatedQuad>* pending=nullptr) {
    if(Active(m) && SaveMenu::Active(m))return SaveMenu::CopyPositions(m,mesh,dst,src,bytes);
    if(!Active(m) || !bytes || bytes%48 || bytes>256*48 ||
       !TitleScreen::Pointer(src,bytes) || !TitleScreen::Pointer(dst,bytes) || !Owned(m,mesh)) return false;
    std::vector<std::array<float,12>> quads(bytes/48);
    for(unsigned q=0;q<quads.size();++q)
        if(!TitleScreen::ReadFloats(m,src+q*48,quads[q]))return false;
    for(U32 off=0;off<bytes;off+=4)if(!m.Read(dst+off))return false;
    const auto poses=GetLayout();
    if(!ValidSlotContents(poses))return false;
    for(const auto& p:poses)if(!Valid(p))return false;
    const unsigned floor_rows=DungeonPage(m)?DungeonFloorRows(m):0;
    const bool item_page=m.Read(0x5066f8+0x34).value_or(0)!=0;
    const bool gear_page=m.Read(0x50446c+0x18).value_or(0)!=0;
    const bool arrow_chooser=item_page && ArrowChooserMesh(m,mesh);
    const bool dungeon_cursor=DungeonCursorMesh(m,mesh);
    const U32 cursor=m.Read(0x50446c+0x14).value_or(0);
    const bool gear_cursor=gear_page && TitleScreen::Pointer(cursor,0x38) &&
        m.Read(cursor)==std::optional<U32>{mesh};
    for(unsigned q=0;q<quads.size();++q) {
        auto& v=quads[q];
        if(WorldLocationQuad(m,mesh,q,quads.size())) {
            // Native world-map entrance (state 9, 0042d354) offsets every
            // non-map quad by timer*18, then resets the earned Skulltula's
            // offset separately. Remove each visible part's own entrance
            // offset so the panel, name, key and Skulltula share our 60 Hz
            // rise/fade. Keep native horizontal parking of unavailable icons.
            if(m.Read(0x506cb0+0x38)==9) {
                const U32 helper=m.Read(0x506cb0+0x10).value_or(0);
                const U32 offsets=m.Read(helper+0x1c).value_or(0);
                std::array<float,2> native{};
                if(TitleScreen::ReadFloats(m,offsets+q*8,native) && native[0]==0 &&
                   native[1]>=0 && native[1]<=90)
                    for(unsigned i=1;i<12;i+=3)v[i]-=native[1];
            }
            const float x0=std::min({v[0],v[3],v[6],v[9]}),y0=std::min({v[1],v[4],v[7],v[10]});
            const float x1=std::max({v[0],v[3],v[6],v[9]}),y1=std::max({v[1],v[4],v[7],v[10]});
            const bool hidden=x0>=320 || y0>=240 || x1<=0 || y1<=0;
            const auto d=pending?std::array{0.f,0.f}:Motion(WorldLocationPanel);
            for(unsigned i=0;i<12;i+=3) {
                if(hidden){v[i]=v[0];v[i+1]=v[1];}
                else {
                    const auto p=MoveWorldLocation(v[i],v[i+1],poses);
                    v[i]=p[0]+d[0]/Scale;v[i+1]=p[1]+d[1]/Scale;
                    if(q==16 || q==17) {
                        // The key/backing tile (including its orange mark) and
                        // Skulltula share their native center and one pose.
                        const auto center=MoveWorldLocation(200,172,poses);
                        const float scale=std::isfinite(poses.world_skulltula_scale)?
                            std::clamp(poses.world_skulltula_scale,.25f,3.f):1.f;
                        v[i]=center[0]+d[0]/Scale+(v[i]-center[0]-d[0]/Scale)*scale;
                        v[i+1]=center[1]+d[1]/Scale+(v[i+1]-center[1]-d[1]/Scale)*scale;
                        if(std::isfinite(poses.world_skulltula_x))v[i]+=std::clamp(poses.world_skulltula_x,-200.f,200.f)/Scale;
                        if(std::isfinite(poses.world_skulltula_y))v[i+1]+=std::clamp(poses.world_skulltula_y,-120.f,120.f)/Scale;
                    }
                }
            }
            if(pending && !hidden && pending->size()<4096)pending->push_back({dst+q*48,v,WorldLocationPanel});
            for(unsigned i=0;i<12;++i)m.Write(dst+q*48+i*4,std::bit_cast<U32>(v[i]));
            continue;
        }
        if(const auto part=DungeonQuadPart(m,mesh,q,quads.size())) {
            const float x0=std::min({v[0],v[3],v[6],v[9]}), y0=std::min({v[1],v[4],v[7],v[10]});
            const float x1=std::max({v[0],v[3],v[6],v[9]}), y1=std::max({v[1],v[4],v[7],v[10]});
            // Respect native parking of unavailable icons and floor indicators.
            // Native 44281c: 21..25 are stone surrounds, 26..30 the
            // stone faces, 31..35 the labels. Preserve labels and markers.
            const bool floor_stone=poses.hide_dungeon_floor_stones && quads.size()==50 &&
                *part==DungeonFloors && q>=21 && q<=30;
            const auto location_assets=DungeonLocation::GetAssets();
            const U32 play=m.Read(UI+0xc).value_or(0);
            const auto location_index=DungeonLocation::TitleIndex(m.Read(play+0x104).value_or(~0u)&0xffff);
            const bool replaced_title=quads.size()==50 && *part==DungeonBanner && location_assets &&
                location_index && (location_assets->available&(1u<<*location_index));
            const bool hidden=*part==PartCount || floor_stone || replaced_title || x0>=320 || y0>=240 || x1<=0 || y1<=0;
            // Cursor corners share the selected row on either side. Floor
            // markers 36/37 follow their own floor, not the current selection.
            const unsigned selected=m.Read(0x506cb0+0x30).value_or(8);
            const unsigned row=dungeon_cursor?(*part==DungeonFloors?selected:selected-5):
                DungeonRow(*part,(y0+y1)/2);
            const float dy=DungeonRowShift(*part,row,poses,floor_rows);
            AnimatedQuad animation{dst+q*48,{},*part};
            if((*part==DungeonFloors && row<5) || (*part==DungeonItems && row<3)) {
                animation.floor_row=*part==DungeonFloors?row:5+row;
                animation.floor_center=Move(*part==DungeonFloors?290.f:32.f,
                    (*part==DungeonFloors?28+46.f*row:DungeonRowCenter(*part,row))+dy,*part,poses[*part]);
                animation.dungeon_cursor=dungeon_cursor;
            }
            for(unsigned i=0;i<12;i+=3) {
                if(hidden) {v[i]=v[0];v[i+1]=v[1];}
                else {
                    auto p=*part==DungeonSkulltula?MoveDungeonSkulltula(v[i],v[i+1],poses):
                        Move(v[i],v[i+1]+dy,*part,poses[*part]);
                    const auto d=pending?std::array{0.f,0.f}:Motion(*part);
                    v[i]=p[0]+d[0]/Scale;v[i+1]=p[1]+d[1]/Scale;
                }
            }
            if(!hidden) {
                animation.vertices=v;
                if(pending && pending->size()<4096)pending->push_back(animation);
                else if(!pending) {
                    const auto d=Motion(*part);
                    animation.floor_center[0]+=d[0]/Scale;
                    animation.floor_center[1]+=d[1]/Scale;
                    v=GrowFloor(animation,poses);
                }
            }
            for(unsigned i=0;i<12;++i)m.Write(dst+q*48+i*4,std::bit_cast<U32>(v[i]));
            continue;
        }
        RestingQuad(m,mesh,q,v);
        const float min_x=std::min({v[0],v[3],v[6],v[9]}), max_x=std::max({v[0],v[3],v[6],v[9]});
        const float min_y=std::min({v[1],v[4],v[7],v[10]}), max_y=std::max({v[1],v[4],v[7],v[10]});
        if(item_page && min_x<320 && min_y<240 && max_x>0 && max_y>0 &&
           max_x>min_x && max_y>min_y) {
            auto flight=Flight(m,mesh,q,poses);
            if(!flight) {
                flight=LandingGlow(m,mesh,q,poses);
                // The native rectangle helper truncates half-pixel offsets.
                // Recenter the interpolated landing copy before applying the
                // user's slot layout so odd widths don't wobble diagonally.
                if(flight)flight->native_center={(min_x+max_x)*.5f,(min_y+max_y)*.5f};
            }
            if(flight) {
                AnimatedQuad animation{dst+q*48,{},flight->from,flight->to,flight->progress};
                const auto d=pending?std::array{0.f,0.f}:QuadMotion(animation);
                for(unsigned i=0;i<12;i+=3) {
                    v[i]=flight->center[0]+(v[i]-flight->native_center[0])*flight->scale+d[0]/Scale;
                    v[i+1]=flight->center[1]+(v[i+1]-flight->native_center[1])*flight->scale+d[1]/Scale;
                }
                animation.vertices=v;
                if(pending && pending->size()<4096)pending->push_back(animation);
                for(unsigned i=0;i<12;++i)m.Write(dst+q*48+i*4,std::bit_cast<U32>(v[i]));
                continue;
            }
        }
        // The shared grey rail behind the three cards is not a fourth tab.
        // Each card has its own complete frame; discard the connecting rail.
        const bool rail=min_y==202 && max_y==240 &&
            ((min_x==64 && max_x==72) || (min_x==72 && max_x==248) || (min_x==248 && max_x==256));
        const float cx=(min_x+max_x)/2,cy=(min_y+max_y)/2;
        const Part part=arrow_chooser?Items:item_page && min_x>=256?Slots:min_y>=198?
            (cx<64?Back:cx<140?GearTab:cx<198?MapTab:ItemsTab):gear_page?
            (cy<152?(cx<160?GearLeft:GearRight):(cx<160?GearBottomLeft:GearBottomRight)):(item_page?Items:Map);
        const bool hidden=min_x>=320 || min_y>=240 || max_x<=0 || max_y<=0 || rail ||
            !TabVisible(part,item_page,gear_page) ||
            PressedTabQuad(m,mesh,q,quads.size()) ||
            (gear_page && GearBackground(m,mesh,q,quads.size()));
        for(unsigned i=0;i<12;i+=3) {
            if(hidden) {v[i]=v[0];v[i+1]=v[1];}
            else {
                float x=v[i],y=v[i+1];
                if(part==Slots) {
                    if(max_y-min_y>100) {
                        if(poses.horizontal) {x=288+(v[i+1]-120);y=120-(v[i]-288);}
                    } else {
                        const auto point=SlotPoint(x,y,Slot(cy),poses);x=point[0];y=point[1];
                    }
                }
                if(IsLowerGear(part) && poses.gear_vertical[part-GearBottomLeft]) {
                    const auto a=Anchors[part];
                    // Rotate the long background/placeholder strips. Translate each
                    // icon and its cursor pieces as a group so their art stays upright.
                    if(gear_cursor || max_x-min_x>60) {x=a[0]-(v[i+1]-a[1]);y=a[1]+v[i]-a[0];}
                    else {
                        const float center=GearSlotX(part,GearSlot(part,cx));
                        x+=a[0]-center;y+=center-a[0];
                    }
                }
                auto p=Move(x,y,part,poses[part]);
                if(IsTab(part)) p=Move(p[0],p[1],Tabs,poses[Tabs]);
                // Draw-time counters are already past the shared animation tick.
                // Apply this frame's offset now; update-time uploads defer it.
                const auto d=pending?std::array{0.f,0.f}:Motion(part);
                v[i]=p[0]+d[0]/Scale;v[i+1]=p[1]+d[1]/Scale;
            }
        }
        if(pending && !hidden && (part==Items || part==Map || part==Slots || IsGearOverlay(part) || IsTab(part)) && pending->size()<4096)
            pending->push_back({dst+q*48,v,part});
        for(unsigned i=0;i<12;++i)m.Write(dst+q*48+i*4,std::bit_cast<U32>(v[i]));
    }
    return true;
}
// The inventory plate is two mirrored copies of one atlas region. Its shared
// edge sampled the neighboring texel at U=124/256, creating a bright crease.
// The slot stones/surrounds also border other artwork. Keep their sampling
// inside each crop, including mirrored halves and one-texel stretched strips.
template<class M> bool CopyPanelUVs(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    if(!Active(m) || bytes!=37*32 || !TitleScreen::Pointer(src,bytes) || !TitleScreen::Pointer(dst,bytes))return false;
    const U32 model=m.Read(0x5066f8+0x20).value_or(0),helper=m.Read(0x5066f8+0x24).value_or(0);
    if(m.Read(model)!=std::optional<U32>{0x4ebd60} || m.Read(model+4)!=std::optional<U32>{mesh} ||
       m.Read(helper)!=std::optional<U32>{37} || m.Read(helper+0x14)!=std::optional<U32>{src})return false;
    std::array<U32,37*8> uv{};
    for(unsigned i=0;i<uv.size();++i) {
        const auto word=m.Read(src+4*i);if(!word || !m.Read(dst+4*i))return false;uv[i]=*word;
    }
    constexpr std::array<float,16> plate{0,1,124.f/256,1,0,56.f/256,124.f/256,56.f/256,
        124.f/256,1,0,1,124.f/256,56.f/256,0,56.f/256};
    for(unsigned i=0;i<plate.size();++i)if(uv[i]!=std::bit_cast<U32>(plate[i]))return false;
    for(unsigned i:{2u,6u,8u,12u})uv[i]=std::bit_cast<U32>(123.5f/256);
    // Native 46b534: 13..16 stone surrounds, 17/18 stone faces,
    // 19/20 and 22/23 mirrored round surrounds, 21/24 round faces.
    // Item icons, counts, labels, tabs and the green rail are separate.
    for(unsigned q=13;q<=24;++q)for(unsigned axis=0;axis<2;++axis) {
        float lo=1,hi=0;
        for(unsigned v=0;v<4;++v) {
            const float f=std::bit_cast<float>(uv[q*8+v*2+axis]);
            if(!std::isfinite(f))return false;
            lo=std::min(lo,f);hi=std::max(hi,f);
        }
        constexpr float half=.5f/256;
        if(lo<0 || hi>1 || hi-lo<2*half)continue;
        for(unsigned v=0;v<4;++v) {
            auto& word=uv[q*8+v*2+axis];
            word=std::bit_cast<U32>(std::clamp(std::bit_cast<float>(word),lo+half,hi-half));
        }
    }
    for(unsigned i=0;i<uv.size();++i)m.Write(dst+4*i,uv[i]);
    return true;
}
// Tint only the rail quad in the item controller's own color upload. Copy the
// entire fresh upload, preserving icon colors and every native alpha value.
template<class M> bool CopyDungeonStoneColors(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    if(!Active(m) || bytes!=50*64 || !TitleScreen::Pointer(src,bytes) ||
       !TitleScreen::Pointer(dst,bytes) || DungeonQuadPart(m,mesh,1,50)!=DungeonLeft)return false;
    const U32 helper=m.Read(0x506cb0+0x14).value_or(0);
    if(m.Read(helper+0x18)!=std::optional<U32>{src})return false;
    const auto layout=GetLayout();
    const float brightness=std::isfinite(layout.dungeon_brightness)?std::clamp(layout.dungeon_brightness,0.f,1.f):1;
    const std::array tints{std::array<float,3>{brightness,brightness,brightness},
        OverlayTint(layout.dungeon_left_overlay),OverlayTint(layout.dungeon_right_overlay)};
    if(tints[0]==std::array<float,3>{1,1,1} && tints[1]==tints[0] && tints[2]==tints[0])return false;
    std::array<U32,50*16> colors{};
    for(unsigned i=0;i<colors.size();++i) {
        const auto value=m.Read(src+i*4);if(!value || !m.Read(dst+i*4))return false;colors[i]=*value;
    }
    // Quad 0 is the map parchment; 1 and 2 are stone backings. Floor cards, item wells,
    // lettering, icons and cursor use other quads/models; preserve their colors.
    for(unsigned q=0;q<3;++q)for(unsigned v=0;v<4;++v)for(unsigned c=0;c<3;++c) {
        const unsigned i=q*16+v*4+c;
        const float value=std::bit_cast<float>(colors[i]);if(!std::isfinite(value))return false;
        colors[i]=std::bit_cast<U32>(value*tints[q][c]);
    }
    for(unsigned i=0;i<colors.size();++i)m.Write(dst+i*4,colors[i]);
    return true;
}
inline float MapBrightness(float darkness) {
    return 1-(std::isfinite(darkness)?std::clamp(darkness,0.f,1.f):0);
}
template<class M> bool CopyWorldMapColors(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    constexpr U32 world=0x506cb0,quads=30;
    if(!Active(m) || !MapPage(m) || DungeonPage(m) || bytes!=quads*64 ||
       !TitleScreen::Pointer(src,bytes) || !TitleScreen::Pointer(dst,bytes))return false;
    const U32 model=m.Read(world+4).value_or(0),helper=m.Read(world+0x10).value_or(0);
    if(m.Read(world)!=mesh || m.Read(model)!=0x4ebd60 || m.Read(model+4)!=mesh ||
       m.Read(helper)!=quads || m.Read(helper+0x18)!=src)return false;
    const auto layout=GetLayout();
    const float brightness=MapBrightness(layout.map_darkness);
    const auto tint=MapTimeTint::Read(m,layout.world_map_time_tint);
    const float opacity=world_location_alpha.load();
    if(brightness==1 && opacity==1 && tint==MapTimeTint::Neutral)return false;
    const U32 uvs=m.Read(helper+0x14).value_or(0);
    if(!TitleScreen::Pointer(uvs,quads*32))return false;
    std::array<U32,quads*16> colors{};
    for(unsigned i=0;i<colors.size();++i) {
        const auto word=m.Read(src+4*i);if(!word || !m.Read(dst+4*i))return false;colors[i]=*word;
    }
    bool found=false;
    for(unsigned q=0;q<quads;++q) {
        if(q>=12 && q<=17 && opacity!=1) {
            found=true;
            for(unsigned v=0;v<4;++v) {
                auto& word=colors[q*16+v*4+3];
                const float alpha=std::bit_cast<float>(word);
                if(!std::isfinite(alpha))return false;
                word=std::bit_cast<U32>(alpha*opacity);
            }
        }
        std::array<float,8> uv;
        if(!TitleScreen::ReadFloats(m,uvs+q*32,uv))return false;
        float left=1,right=0,top=1,bottom=0;
        for(unsigned i=0;i<8;i+=2) {
            left=std::min(left,uv[i]);right=std::max(right,uv[i]);
            top=std::min(top,uv[i+1]);bottom=std::max(bottom,uv[i+1]);
        }
        // Same parchment crop as MapExtension. Exclude markers, labels and
        // tabs in the rest of this atlas, and preserve every native alpha.
        if(left<0 || right>320.f/512+.002f || top<16.f/256-.002f || bottom>1.002f ||
           (right-left)*512<100 || (bottom-top)*256<100)continue;
        found=true;
        for(unsigned v=0;v<4;++v)for(unsigned c=0;c<3;++c) {
            const unsigned i=q*16+v*4+c;
            const float value=std::bit_cast<float>(colors[i]);if(!std::isfinite(value))return false;
            colors[i]=std::bit_cast<U32>(value*brightness*tint[c]);
        }
    }
    if(!found)return false;
    for(unsigned i=0;i<colors.size();++i)m.Write(dst+4*i,colors[i]);
    return true;
}
template<class M> bool CopySlotColors(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    if(!Active(m) || bytes!=37*64 || !TitleScreen::Pointer(dst,bytes) ||
       !TitleScreen::Pointer(src,bytes))return false;
    const U32 model=m.Read(0x5066f8+0x20).value_or(0),helper=m.Read(0x5066f8+0x24).value_or(0);
    if(m.Read(model)!=std::optional<U32>{0x4ebd60} || m.Read(model+4)!=std::optional<U32>{mesh} ||
       m.Read(helper)!=std::optional<U32>{37} || m.Read(helper+0x18)!=std::optional<U32>{src})return false;
    const auto tint=OverlayTint(Color(GetLayout(),PauseColor::SlotBackground));
    if(tint==std::array<float,3>{1,1,1})return false;
    std::array<U32,37*16> colors{};
    for(unsigned i=0;i<colors.size();++i) {
        const auto value=m.Read(src+4*i);if(!value || !m.Read(dst+4*i))return false;colors[i]=*value;
    }
    for(unsigned vertex=0;vertex<4;++vertex)for(unsigned c=0;c<3;++c) {
        const unsigned i=12*16+vertex*4+c;
        const float value=std::bit_cast<float>(colors[i]);if(!std::isfinite(value))return false;
        colors[i]=std::bit_cast<U32>(value*tint[c]);
    }
    for(unsigned i=0;i<colors.size();++i)m.Write(dst+4*i,colors[i]);
    return true;
}
// Inverse of the draw transform. Resolve overlapping panels in draw order;
// the decorative background never intercepts input.
inline std::array<float,2> Unmove(float x,float y,Part part,const Pose& p) {
    const auto a=Anchors[part];
    return {a[0]+(x-a[0]-p.x/Scale)/p.scale,a[1]+(y-a[1]-p.y/Scale)/p.scale};
}
inline std::optional<std::array<float,2>> MapPointer(float u,float v,float width,const Layout& poses,
        bool has_slots=true,bool is_gear=false,bool is_dungeon=false,unsigned floor_rows=0) {
    if(!std::isfinite(u) || !std::isfinite(v) || width<=0 || !std::isfinite(width))return {};
    if(!ValidSlotContents(poses))return {};
    for(const auto& p:poses)if(!Valid(p))return {};
    const float x=(u-.5f)*width/Scale+160;
    const float y=((v-.5f)*240-OffsetY)/Scale+120;
    if(is_dungeon) {
        for(Part part:{DungeonFloors,DungeonItems}) {
            const auto d=Motion(part);
            const auto p=Unmove(x-d[0]/Scale,y-d[1]/Scale,part,poses[part]);
            for(unsigned row=0;row<(part==DungeonFloors?5u:3u);++row) {
                if(part==DungeonFloors && floor_rows && !(floor_rows&(1u<<row)))continue;
                const float ny=p[1]-DungeonRowShift(part,row,poses,floor_rows);
                const bool inside=part==DungeonFloors?p[0]>=256 && p[0]<320 && ny>=0 && ny<240:
                    p[0]>=0 && p[0]<64 && ny>=0 && ny<156;
                if(inside && DungeonRow(part,ny)==row)return std::array{p[0]/320,ny/240};
            }
        }
        return {};
    }
    for(Part part:{Slots,ItemsTab,MapTab,GearTab,Back,GearBottomRight,GearBottomLeft,GearRight,GearLeft,Items,Map}) {
        if(!TabVisible(part,has_slots,is_gear))continue;
        if((part==Slots && !has_slots) || (part>=GearLeft && part<=GearBottomRight && !is_gear) ||
           (part==Items && (is_gear || !has_slots)) || (part==Map && (is_gear || has_slots)))continue;
        auto p=IsTab(part)?Unmove(x,y,Tabs,poses[Tabs]):std::array{x,y};
        const auto d=Motion(part);
        p=Unmove(p[0]-d[0]/Scale,p[1]-d[1]/Scale,part,poses[part]);
        float nx=p[0],ny=p[1];bool inside=false;
        if(part==Slots) {
            for(unsigned slot=0;slot<4;++slot) {
                const auto c=SlotCenters[slot],target=SlotCenter(slot,poses);
                const float sx=c[0]+(nx-target[0])/poses.slot_content_scale;
                const float sy=c[1]+(ny-target[1])/poses.slot_content_scale;
                if(sx>=256 && sx<320 && sy>=0 && sy<240 && Slot(sy)==slot) {
                    nx=sx;ny=sy;inside=true;break;
                }
            }
        } else if(IsTab(part)) {
            const float lo=part==Back?0:part==GearTab?64:part==MapTab?140:198;
            const float hi=part==Back?64:part==GearTab?140:part==MapTab?198:256;
            inside=nx>=lo && nx<hi && ny>=198 && ny<240;
        } else if(part>=GearLeft && part<=GearBottomRight) {
            const bool right=part==GearRight || part==GearBottomRight;
            const bool bottom=part==GearBottomLeft || part==GearBottomRight;
            if(bottom && poses.gear_vertical[part-GearBottomLeft]) {
                const auto a=Anchors[part];
                for(unsigned slot=0;slot<4;++slot) {
                    const float center=GearSlotX(part,slot);
                    const float sx=nx-(a[0]-center),sy=ny-(center-a[0]);
                    if(sx>=(right?160:0) && sx<(right?320:160) && sy>=152 && sy<198 &&
                       GearSlot(part,sx)==slot) {nx=sx;ny=sy;inside=true;break;}
                }
            } else inside=nx>=(right?160:0) && nx<(right?320:160) && ny>=(bottom?152:0) && ny<(bottom?198:152);
        } else inside=nx>=0 && nx<(has_slots?256:320) && ny>=0 && ny<198;
        if(inside)return std::array{nx/320,ny/240};
    }
    return {};
}

template<class M> bool PreviewMatrix(M& m,U32 owner,U32 model,U32 matrix,const Pose& p) {
    if(!Active(m) || !Valid(p) || m.Read(0x4fc648+0x34)!=std::optional<U32>{owner} ||
       !TitleScreen::Pointer(owner,0x120) ||
       (m.Read(owner+0xfc)!=std::optional<U32>{model} && m.Read(owner+0x100)!=std::optional<U32>{model}))return false;
    std::array<float,12> v;
    if(!TitleScreen::ReadFloats(m,matrix,v))return false;
    // The native preview is centered in a perspective view, looking down -Z.
    // Adjust only its freshly composed matrix, preserving rotation and animation.
    const float units=std::abs(v[11])*2.f*.5773502692f/240.f;
    // MQ mirrors the perspective projection, so a screen-right editor offset
    // needs the opposite view-space translation. Keep the model itself mirrored.
    const bool mirrored=((m.Read(0x587958+0xc).value_or(0)>>16)&255)!=0;
    v[3]+=(mirrored?-p.x:p.x)*units;v[7]-=p.y*units;
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)v[row*4+col]*=p.scale;
    for(unsigned i=0;i<12;++i)m.Write(matrix+i*4,std::bit_cast<U32>(v[i]));
    return true;
}

// Native cursor origins and sizes include a seven-pixel corner inset. Use
// the same centers, panel transforms, and lower-panel turn as the renderer.
inline bool GearNavigationChanged(const Layout& poses) {
    if(poses.gear_vertical[0] || poses.gear_vertical[1])return true;
    for(Part p:{GearLeft,GearRight,GearBottomLeft,GearBottomRight})
        if(poses[p].x!=0 || poses[p].y!=0 || poses[p].scale!=1)return true;
    return false;
}
using GearPoints=std::array<std::array<float,2>,27>;
template<class M> std::optional<GearPoints> GearFocusPoints(M& m,const Layout& poses) {
    for(Part p:{GearLeft,GearRight,GearBottomLeft,GearBottomRight})if(!Valid(poses[p]))return {};
    GearPoints points{};
    for(unsigned i=0;i<points.size();++i) {
        std::array<float,4> box{};
        constexpr std::array<U32,4> tables{0x504bd4,0x504c40,0x504cac,0x504d18};
        for(unsigned k=0;k<4;++k) {
            const auto word=m.Read(tables[k]+i*4);if(!word)return {};
            box[k]=std::bit_cast<float>(*word);
            if(!std::isfinite(box[k]))return {};
        }
        if(box[0]<0 || box[0]>320 || box[1]<0 || box[1]>240 ||
           box[2]<=0 || box[2]>100 || box[3]<=0 || box[3]>100)return {};
        float x=box[0]+box[2]/2+7,y=box[1]+box[3]/2+7;
        const Part part=i<9?GearRight:i>=19 && i<=21?GearBottomLeft:
            i>=22 && i<=25?GearBottomRight:GearLeft;
        if(IsLowerGear(part) && poses.gear_vertical[part-GearBottomLeft]) {
            const auto a=Anchors[part];const float old_x=x;
            x=a[0]-(y-a[1]);y=a[1]+old_x-a[0];
        }
        points[i]=Move(x,y,part,poses[part]);
    }
    return points;
}
inline U32 GearNeighbor(const GearPoints& points,U32 selected,U32 keys) {
    if(selected>=points.size())return 0xffffffff;
    // HID directions after native repeat filtering; opposing keys cancel.
    float dx=float(bool(keys&0x10))-float(bool(keys&0x20));
    float dy=float(bool(keys&0x80))-float(bool(keys&0x40));
    const float length=std::sqrt(dx*dx+dy*dy);
    if(length==0)return 0xffffffff;
    dx/=length;dy/=length;
    float best=1e9f;U32 next=0xffffffff;
    for(U32 i=0;i<points.size();++i) {
        if(i==selected)continue;
        const float x=points[i][0]-points[selected][0],y=points[i][1]-points[selected][1];
        const float forward=x*dx+y*dy,side=std::abs(x*dy-y*dx);
        if(forward<=.5f || side>2*forward)continue;
        // Favor nearby targets along the requested direction over distant
        // aligned targets or almost perpendicular hops into another panel.
        const float score=forward+2*side+side*side/forward;
        if(score<best) {best=score;next=i;}
    }
    return next; // At an outer edge, keep the selection instead of wrapping.
}

struct Hook {
    bool drawing=false, routed=false, background_copy=false, show_contents=true;
    unsigned background_index=0;
    template<class M> Pose BackgroundPose(M& m,const Layout& layout) const {
        auto p=MapPage(m)?layout.map_backgrounds[std::min(background_index/2,1u)].pose:layout[Background];
        p.x+=motion_x.load();
        if(MapPage(m)?background_index%2:background_copy)p.y-=240*Scale*p.scale;
        return p;
    }
    std::vector<AnimatedQuad> pending;
    template<class M> void Animate(M& m) {
        // Some native widgets upload more than once. Match the fresh buffer
        // before editing it so stale uploads cannot overwrite the last update.
        const auto poses=GetLayout();
        for(auto it=pending.rbegin();it!=pending.rend();++it) {
            const auto& q=*it;const auto d=QuadMotion(q);
            const auto vertices=GrowFloor(q,poses);
            if(d[0]==0 && d[1]==0 && vertices==q.vertices)continue;
            bool fresh=true;
            for(unsigned i=0;i<12;++i)
                fresh &= m.Read(q.address+4*i)==std::optional<U32>{std::bit_cast<U32>(q.vertices[i])};
            if(!fresh)continue;
            for(unsigned i=0;i<12;i+=3) {
                m.Write(q.address+4*i,std::bit_cast<U32>(vertices[i]+d[0]/Scale));
                m.Write(q.address+4*(i+1),std::bit_cast<U32>(vertices[i+1]+d[1]/Scale));
            }
        }
        pending.clear();
    }
    template<class M,class R> void Handle(M& m,R& r,unsigned site,bool supported=true) {
        if(site==20) {
            r[14]=Sites[20].pc+4;
            // Skip only the popup-opening batch dim, retaining the original
            // colors, disabled-item styling and native input/update sequence.
            if(!(supported && Active(m) && r[8]==0x5066f8 &&
                 m.Read(0x5066f8+0x34)==12 && m.Read(0x5066f8+0x38)==1 &&
                 m.Read(0x5066f8+0x24)==r[0] && r[1]==37 && r[2]==0))r[15]=0x2f8b80;
            return;
        }
        if(site==19) {
            r[14]=Sites[19].pc+4;r[15]=0x2ffe40; // Displaced BL.
            return;
        }
        if(site==17 || site==18) {
            const bool items=site==17;
            r[1]=m.Read(r[5]+(items?0x34:0x18)).value_or(0); // Displaced LDR.
            const U32 first=items?4:5;
            // These are visual dispatchers, separate from the input/page state.
            // The resting branch restores normal card opacity/offsets and hides
            // the pressed overlays, retaining the page's selected/unselected art.
            if(supported && Active(m) && r[5]==(items?0x5066f8u:0x50446cu) &&
               r[1]>=first && r[1]<first+3)r[1]=2;
            return;
        }
        if(site==15 || site==16) {
            r[5]=m.Read(r[0]).value_or(0); // Displaced LDR r5,[r0].
            // The native CMP/BEQ skips this entry in both render passes. Leave
            // the actual queue, model updates and resource teardown untouched.
            if(supported && Closing(m) && PreviewModel(m,r[5]))r[5]=0;
            return;
        }
        if(site==14)return; // Displaced NOP; the independent panel draws here.
        if(site==12 || site==13) {
            r[0]=m.Read(r[6]+(site==12?0x60:0x84)).value_or(0); // Displaced LDR.
            const U32 state=m.Read(0x5066f8+0x34).value_or(0);
            // These buffers are already updated during the entrance. Draw them
            // with our shared slide instead of waiting for the old lateral slide
            // and its extra icon frame. Preserve the native draw's countdown
            // side effect so its final delayed frame cannot reappear later.
            if(supported && drawing && Active(m) && r[6]==0x5066f8 &&
               (state==13 || state==14 || (state==1 && site==13))) {
                if(site==13 && r[0]>0 && r[0]<0x80000000)m.Write(r[6]+0x84,r[0]-1);
                r[0]=0;
            }
            return;
        }
        if(site==11) {
            // The native gameplay HUD resumes before the pause controller clears.
            // Do not draw its map, slots or counters inside our routed menu pass.
            r[14]=Sites[11].pc+4;
            if(!supported || !drawing)r[15]=0x42f584;
            return;
        }
        if(site==10) {
            r[1]=r[7]; // Displaced MOV r1,r7.
            if(supported && CopyPanelUVs(m,r[4],r[0],r[7],r[6]))r[15]=0x317db8;
            return;
        }
        if(site==9) {
            r[1]=r[8]; // Displaced MOV r1,r8.
            if(supported && (SaveMenu::CopyColors(m,r[4],r[0],r[8],r[7]) ||
                             CopySlotColors(m,r[4],r[0],r[8],r[7]) ||
                             CopyDungeonStoneColors(m,r[4],r[0],r[8],r[7]) ||
                             CopyWorldMapColors(m,r[4],r[0],r[8],r[7])))r[15]=0x2f99f0;
            return;
        }
        if(site==8) {
            r[4]=m.Read(0x438950).value_or(0); // Displaced LDR r4,[pc,#0x21c].
            const U32 selected=m.Read(0x50446c+0x20).value_or(27);
            const auto poses=GetLayout();
            if(!supported || !GearNavigationChanged(poses) || !Active(m) ||
               m.Read(0x50446c+0x18)!=std::optional<U32>{2} || selected>=27)return;
            const auto points=GearFocusPoints(m,poses);if(!points)return;
            const U32 next=GearNeighbor(*points,selected,r[0]);
            r[4]=next;r[5]=0x50446c;r[6]=0;
            r[15]=0x438850; // Native selection sound, refresh, and A/B handling.
            return;
        }
        if(site==6 || site==7) {
            // Native coordinates are five inventory columns plus one slot column.
            // Only replace the directional step; native repeat timing, selection,
            // equip actions, cursor refresh and movement sound still run normally.
            constexpr U32 items=0x5066f8;
            const auto state=m.Read(items+0x34).value_or(0);
            const auto x=m.Read(items+0x78).value_or(6), y=m.Read(items+0x7c).value_or(4);
            if(supported && GetLayout().horizontal && Active(m) && r[5]==items &&
               state==(site==6?2u:8u) && x<6 && y<4) {
                U32 nx=x,ny=y;
                const U32 direction=r[0]&0xf0;
                if(x==5) {
                    if(direction&0x80) {nx=(y*4+1)/3;ny=0;}
                    else if(direction&0x20) ny=(y+3)%4;
                    else if(direction&0x10) ny=(y+1)%4;
                } else {
                    if(direction&0x40) {
                        if(y) --ny;
                        else {nx=5;ny=(x*3+2)/4;}
                    } else if(direction&0x80) ny=(y+1)%4;
                    else if(direction&0x20) nx=(x+4)%5;
                    else if(direction&0x10) nx=(x+1)%5;
                }
                m.Write(items+0x78,nx);m.Write(items+0x7c,ny);
                r[4]=(nx!=x || ny!=y)?1:0;
                r[15]=0x2ed1cc; // Native movement sound and cursor/description refresh.
            }
            return; // Displaced NOP; preserve flags for the native fallback.
        }
        if(site==5) {
            if(SaveMenu::BackdropActive(m)) {background_copy=false;background_index=0;return;}
            const auto layout=GetLayout();
            const unsigned count=MapPage(m)?4:layout.stack_background?2:1;
            const U32 model=m.Read(UI+0x10).value_or(0);
            if(supported && drawing && background_index+1<count &&
               TitleScreen::Pointer(model,0x1e4) && m.Read(model)==std::optional<U32>{0x4ebd60}) {
                ++background_index;background_copy=true;r[0]=model;r[14]=Sites[5].pc;r[15]=0x3f9680;
            } else {background_copy=false;background_index=0;}
            return; // Displaced NOP.
        }
        if(site==4) {
            r[1]=r[8]; // Displaced MOV r1,r8.
            if(supported)PreviewMatrix(m,r[4],r[0],r[1],GetLayout()[Preview]);
            return;
        }
        if(site==3) {
            r[1]=r[6]; // Displaced MOV.
            if(supported && (CopyDungeonMarkers(m,r[4],r[0],r[6],r[5]) ||
                CopyPositions(m,r[4],r[0],r[6],r[5],drawing?nullptr:&pending)))r[15]=0x3675f4; // Native POP/return.
            return;
        }
        if(site==0) {
            r[0]=r[5]; // Displaced MOV r0,r5.
            routed=supported && Active(m); drawing=routed && show_contents;background_copy=false;background_index=0;
            item_page.store(m.Read(0x5066f8+0x34).value_or(0)!=0);
            gear_page.store(m.Read(0x50446c+0x18).value_or(0)!=0);
            dungeon_page.store(routed && DungeonPage(m));
            dungeon_floor_rows.store(routed && DungeonPage(m)?DungeonFloorRows(m):0);
            on_top.store(routed);
            SaveMenu::on_top.store(routed && SaveMenu::Active(m));
            SaveMenu::input_page.store(routed?SaveMenu::CurrentInputPage(m):SaveMenu::InputPage::Hidden);
            if(drawing) {r[14]=Sites[1].pc;r[15]=0x41ec2c;}
        } else if(site==1) {
            if(drawing) r[0]=r[5];
            drawing=false;
        } else {
            // Draw once, preserving native animation and counters in the UI draw routines.
            const bool skip=routed && Active(m);
            r[14]=Sites[2].pc+4;
            if(skip) r[0]=0;
            else {r[15]=0x41ec2c;on_top.store(false);}
            routed=false;
        }
    }
};
} // namespace OOT3D::PauseMenu
