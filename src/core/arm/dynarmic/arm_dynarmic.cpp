#ifdef ENABLE_OOT3D_PRESENTATION
#include <bit>
#include <span>
#include "oot3d/hd_font.h"
#include "core/hle/service/nwm/nwm_uds.h"
#include "core/hle/service/service.h"
#include "oot3d/actor_draw_distance.h"
#include "oot3d/native_prompt_diagnostics.h"
#include "oot3d/save_wait_diagnostics.h"
#include "core/hle/kernel/thread.h"
#include "oot3d/randomizer/hooks.h"
#include "oot3d/room_join_chime.h"
#include "common/cityhash.h"
#include "oot3d/title_hooks.h"
#include "oot3d/title_prompt.h"
#include "oot3d/input.h"
#include "oot3d/hud_hooks.h"
#include "oot3d/dpad_hud.h"
#include "oot3d/screen_model_hooks.h"
#include "oot3d/textbox_interpolation_hooks.h"
#include "oot3d/textbox_open.h"
#include "oot3d/camera.h"
#include "oot3d/orbit_camera.h"
#include "oot3d/dialogue_camera.h"
#include "oot3d/dialogue_trace.h"
#include "oot3d/camera_transition_diagnostics.h"
#include "oot3d/pause_menu.h"
#include "oot3d/pause_description.h"
#include "oot3d/first_person_diagnostics.h"
#include "oot3d/pause_framerate.h"
#include "oot3d/ocarina.h"
#include "oot3d/pause_header.h"
#include "oot3d/heap_diagnostics.h"
#include "oot3d/graphics_memory.h"
#include "oot3d/graphics_allocation_diagnostics.h"
#include "oot3d/graphics_memory_diagnostics.h"
#include "oot3d/music.h"
#include "oot3d/pause_prompts.h"
#include "oot3d/save_loading.h"
#include "oot3d/file_select.h"
#include "oot3d/heart_pulse.h"
#include "oot3d/boss_challenge.h"
#include "oot3d/hud_buttons.h"
#include "oot3d/pause_shadow.h"
#include "oot3d/save_quit_backdrop.h"
#include "oot3d/dungeon_sides.h"
#include "oot3d/item_equip_input.h"
#include "oot3d/item_use_input.h"
#include "oot3d/interpolation.h"
#include "oot3d/interpolation_pose_hooks.h"
#include "oot3d/qol_hooks.h"
#include "oot3d/arrow_cycle.h"
#include "oot3d/sun_readback_hooks.h"
#include "oot3d/shop_controls.h"
#include "oot3d/difficulty_hooks.h"
#include "oot3d/rumble_hooks.h"
#include "oot3d/prices.h"
#include "oot3d/price_runtime.h"
#include "oot3d/gear_panel.h"
#include "oot3d/map_extension.h"
#include "oot3d/map_particles.h"
#include "oot3d/world_map.h"
#include "oot3d/world_map_clouds.h"
#include "video_core/texture/etc1.h"
#endif
#ifdef ENABLE_MM3D_PRESENTATION
#include "mm3d/interpolation_performance.h"
#endif
// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef ENABLE_MM3D_PRESENTATION
#include "mm3d/classic_double_time.h"
#include "mm3d/notebook_inspector.h"
#include "mm3d/camera_instruction_probe.h"
#include "mm3d/form_camera.h"
#include "mm3d/fast_time.h"
#include "mm3d/camera_live.h"
#include "mm3d/camera_midpoint.h"
#include "mm3d/link_pose_capture.h"
#include "mm3d/diagnostics.h"
#include "mm3d/minimap.h"
#include "mm3d/native_minimap.h"
#include "mm3d/minimap_shortcut.h"
#include "mm3d/native_inventory.h"
#include "mm3d/dungeon_map.h"
#include "mm3d/difficulty.h"
#include "mm3d/actor_draw_distance.h"
#include "mm3d/prop_draw_distance.h"
#include "mm3d/vegetation_draw_distance.h"
#include "mm3d/death_penalties.h"
#include "mm3d/classic_save_hooks.h"
#include "mm3d/bottle_healing.h"
#include "mm3d/inventory_shadow.h"
#include "mm3d/items_map_prompt.h"
#include "mm3d/menu_help.h"
#include "mm3d/world_map_prompt.h"
#include "mm3d/menu_tabs.h"
#include "mm3d/pause_capture.h"
#include "mm3d/visibility_depth.h"
#include "mm3d/native_notebook.h"
#include "mm3d/native_file_select.h"
#include "mm3d/file_select_interpolation.h"
#include "mm3d/file_select_input.h"
#include "mm3d/file_select_prompt.h"
#include "mm3d/ocarina_prompt.h"
#include "mm3d/magic_border.h"
#include "mm3d/minigame_hud.h"
#include "mm3d/player_hud.h"
#include "mm3d/song_list.h"
#include "mm3d/double_time.h"
#include "mm3d/song_reset.h"
#include "mm3d/hud_buttons.h"
#include "mm3d/banners.h"
#include "mm3d/banner_interpolation.h"
#include "mm3d/song_effect.h"
#include "mm3d/native_item_present.h"
#include "mm3d/inventory_interpolation.h"
#include "mm3d/inventory_navigation.h"
#include "mm3d/menu_page_switch.h"
#include "mm3d/menu_memory.h"
#include "mm3d/menu_backdrop.h"
#include "mm3d/screen_shrink.h"
#include "mm3d/single_screen_text.h"
#include "mm3d/prices.h"
#include "mm3d/menu_sound.h"
#include "mm3d/music.h"
#include "mm3d/tatl_reminders.h"
#include "mm3d/owl_sword.h"
#include "mm3d/owl_intro.h"
#include "mm3d/bomber_approach.h"
#include "mm3d/menu_close.h"
#include "mm3d/menu_shortcuts.h"
#include "mm3d/options_menu.h"
#include "mm3d/native_startup.h"
#include "mm3d/item_preview.h"
#include "mm3d/dialogue_scale.h"
#include "mm3d/message_layout.h"
#include "mm3d/classic_owl_draw.h"
#include "mm3d/classic_file_marker.h"
#include "mm3d/widescreen.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#endif
#include <csignal>
#include <cstring>
#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/interface/optimization_flags.h>
#include "common/assert.h"
#include "common/microprofile.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/dynarmic/arm_exclusive_monitor.h"
#include "core/arm/dynarmic/arm_tick_counts.h"
#include "core/arm/exception_handler.h"
#include "core/core.h"
#include "core/core_timing.h"
#ifdef ENABLE_GDBSTUB
#include "core/gdbstub/gdbstub.h"
#endif
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

#ifndef SIGILL
constexpr u32 SIGILL = 4;
#endif

#ifndef SIGTRAP
constexpr u32 SIGTRAP = 5;
#endif

namespace Core {

#ifdef ENABLE_OOT3D_PRESENTATION
// Synthetic SVCs are dispatched by ID, independently of their original PC.
// Check complete ranges (not just their starting IDs) as part of every build.
// Add new OoT handlers here when adding them to CallSVC below.
static_assert([] {
    using namespace OOT3D;
    struct Range { std::uint32_t first; std::size_t count; };
    constexpr auto ranges=std::to_array<Range>({
        {TitleScreen::FirstSVC,TitleScreen::Sites.size()},
        {HUD::FirstSVC,HUD::Sites.size()},
        {Camera::FirstSVC,Camera::Sites.size()},
        {ScreenModels::FirstSVC,ScreenModels::Sites.size()},
        {OrbitCamera::FirstSVC,2},
        {PauseMenu::FirstSVC,PauseMenu::Sites.size()},
        {PauseHeader::FirstSVC,PauseHeader::LastSVC-PauseHeader::FirstSVC+1},
        {PauseDescription::FirstSVC,PauseDescription::Sites.size()},
        {FileSelect::FirstSVC,FileSelect::Sites.size()},
        {DPad::FirstSVC,DPad::AcceptedSVC-DPad::FirstSVC+1},
        {MinimapControl::FirstSVC,1},
        {Music::SVC,1},
        {PauseFramerate::FirstSVC,PauseFramerate::Sites.size()},
        {Ocarina::FirstSVC,Ocarina::Sites.size()},
        {PauseFramerate::SaveFirstSVC,PauseFramerate::SaveSites.size()},
        {ItemEquip::FirstSVC,ItemEquip::Sites.size()},
        {ItemUse::FirstSVC,ItemUse::Sites.size()},
        {Interpolation::FirstSVC,1},
        {Interpolation::Pose::SVC,1},
        {Interpolation::Pose::FishingFoliage::FirstSVC,Interpolation::Pose::FishingFoliage::Sites.size()},
        {Interpolation::RefreshSVC,1},
        {QOL::FirstSVC,QOL::Sites.size()},
        {ArrowCycle::SVC,1},
        {SunReadbackProbe::FirstSVC,2},
        {Difficulty::FirstSVC,Difficulty::Sites.size()},
        {Rumble::FirstSVC,Rumble::Sites.size()},
        {DialogueCamera::FirstSVC,DialogueCamera::Sites.size()},
        {FileSelectTiming::FirstSVC,FileSelectTiming::Sites.size()},
        {MasterQuest::FirstSVC,MasterQuest::Sites.size()},
        {HeartPulse::FirstSVC,1},
        {DeathMenu::FirstSVC,DeathMenu::Sites.size()},
        {Prices::FirstSVC,Prices::Sites.size()},
        {HDFont::FirstSVC,1},
        {BossChallenge::FirstSVC,BossChallenge::Sites.size()},
        {Shops::FirstSVC,Shops::Sites.size()},
        {Randomizer::FirstSVC,16},
        {Prices::TextSVC,1},
        {ActorDrawDistance::FirstSVC,ActorDrawDistance::Sites.size()},
    });
    for(std::size_t i=0;i<ranges.size();++i) {
        const auto a=ranges[i];
        if(!a.count || a.first<0xfff000 || a.first+a.count>0x1000000)return false;
        for(std::size_t j=0;j<i;++j) {
            const auto b=ranges[j];
            if(a.first<b.first+b.count && b.first<a.first+a.count)return false;
        }
    }
    return true;
}(), "OoT synthetic SVC handlers must have non-overlapping ID ranges");
#endif

class DynarmicUserCallbacks final : public Dynarmic::A32::UserCallbacks {
#ifdef ENABLE_OOT3D_PRESENTATION
    // Shared by pause geometry, model transforms and map-shadow observation.
    // These paths discover model ownership among mixed pointer/state fields.
    struct OotMappedMemory {
        Memory::MemorySystem& memory;
        const Kernel::Process* process;
        std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
        void Write(u32 at,u32 value) {memory.Write32(at,value);}
        bool ReadWords(u32 at,std::span<u32> words) {
            if(words.empty())return true;
            if(words.size_bytes()>0x3fffffff || !Mapped(at,unsigned(words.size_bytes())))return false;
            if constexpr(std::endian::native==std::endian::little)
                memory.ReadBlock(*process,at,words.data(),words.size_bytes());
            else for(unsigned i=0;i<words.size();++i)words[i]=memory.Read32(at+4*i);
            return true;
        }
        bool WriteWords(u32 at,std::span<const u32> words) {
            if(words.empty())return true;
            if(words.size_bytes()>0x3fffffff || !Mapped(at,unsigned(words.size_bytes())))return false;
            if constexpr(std::endian::native==std::endian::little)
                memory.WriteBlock(*process,at,words.data(),words.size_bytes());
            else for(unsigned i=0;i<words.size();++i)memory.Write32(at+4*i,words[i]);
            return true;
        }
        bool Mapped(u32 at,unsigned bytes) {
            if(!process || !OOT3D::TitleScreen::Pointer(at,bytes) || !bytes)return false;
            for(u32 page=at&~0xfffu;page<=((at+bytes-1)&~0xfffu);page+=0x1000)
                if(!memory.IsValidVirtualAddress(*process,page))return false;
            return true;
        }
    };
    OOT3D::TitleScreen::FadeOverride oot_title_fade;
    OOT3D::TitlePrompt::Hook oot_title_prompt;
    OOT3D::HUD::RoutingState oot_hud_routing;
    OOT3D::Interpolation::Hook oot_interpolation;
    OOT3D::SunReadbackProbe::Hook oot_sun_probe;
    OOT3D::Interpolation::FirstPersonDiagnostics::Gate oot_aim_trace;
    OOT3D::DPad::BootsHook oot_boots;
    std::array<OOT3D::DPad::Mesh,OOT3D::DPad::TextureCount> oot_dpad;
    OOT3D::DPad::NaviDisplay oot_navi_display;
    OOT3D::OrbitCamera::Hook oot_orbit;
    OOT3D::DialogueCamera::Hook oot_dialogue_camera;
    OOT3D::DialogueCamera::Trace oot_dialogue_trace;
    OOT3D::CameraTransitionDiagnostics::Trace oot_transition_trace;
    void TraceOotTransition(OOT3D::CameraTransitionDiagnostics::Stage stage,u32 cam,
                           float lens=0,u32 eye=0,u32 at=0,u32 dispatch=0) {
        if(!OOT3D::CameraTransitionDiagnostics::Recording())return;
        struct Guest {
            Memory::MemorySystem& memory;
            std::optional<u32> Read(u32 address) {return memory.Read32OrNullopt(address);}
        } guest{memory};
        oot_transition_trace.Observe(guest,stage,cam,OOT3D::Interpolation::guest_frames.load(),
            parent.system.CoreTiming().GetGlobalTimeUs().count(),oot_orbit,lens,eye,at,dispatch,
            [&](const std::string& line) {LOG_INFO(Core_ARM11,"OOT entrance capture core={}: {}",parent.GetID(),line);});
    }
    void TraceOotDialogue(OOT3D::DialogueCamera::Trace::Stage stage,u32 cam,
                         float lens=0,u32 eye=0,u32 at=0) {
        struct Guest {
            Memory::MemorySystem& memory;
            std::optional<u32> Read(u32 address) {return memory.Read32OrNullopt(address);}
        } guest{memory};
        oot_dialogue_trace.Observe(guest,stage,cam,OOT3D::Interpolation::guest_frames.load(),
            parent.system.CoreTiming().GetGlobalTimeUs().count(),oot_orbit,oot_dialogue_camera,
            lens,eye?eye:cam+0x8c,at?at:cam+0x80,[&](const auto&... values) {
                LOG_INFO(Core_ARM11,"OOT dialogue trace core={}: stage={} frame={} us={} cam={:08x} mode={} setting={} profile={:08x} status={} sub={} actor_flags={:08x} target={:08x} flags={:08x}/{:08x} allowed={} gameplay_lens={} orbit={} pending={} pending_frame={} release={} release_frame={} orbit_fov={} mm={} seeded={} gameplay_seen={} gameplay_frame={} phase={} timer={} native_fov={} view_fov={} effective_fov={} eye=[{},{},{}] at=[{},{},{}]",
                    parent.GetID(),values...);
            });
    }
    OOT3D::PauseMenu::Hook oot_pause;
    OOT3D::PauseDescription::Hook oot_description;
    OOT3D::Shops::UpdateHook oot_shop_description;
    OOT3D::PauseFramerate::Hook oot_pause_framerate;
    OOT3D::ArrowCycle::Hook oot_arrow_cycle;
    OOT3D::Ocarina::Hook oot_ocarina;
    OOT3D::Ocarina::PlaybackNoteFilter oot_playback_note_filter;
    OOT3D::PauseHeader::Hook oot_header;
    OOT3D::PausePrompts::Hook oot_pause_prompts,oot_death_prompts;
    OOT3D::FileSelect::Hook oot_file_select;
    unsigned oot_keyboard_frames=0,oot_keyboard_reports=0;
    std::array<u32,10> oot_keyboard_signature{};
    OOT3D::NativePromptDiagnostics::Trace oot_native_prompts;
    OOT3D::GraphicsMemory::Monitor oot_graphics_memory;
    OOT3D::SaveWaitDiagnostics::Monitor oot_save_wait;
    OOT3D::DeathMenu::Hook oot_death_menu;
    OOT3D::SaveLoading::Hook oot_save_loading,oot_death_loading;
    OOT3D::BossChallenge::Hook oot_boss_challenge;
    OOT3D::HeartPulse::Hook oot_heart_pulse;
    OOT3D::MasterQuest::Hook oot_master_quest;
    OOT3D::RoomJoinChime::Hook oot_room_join_chime;
    OOT3D::HUDButtons::Hook oot_hud_buttons;
    OOT3D::PauseShadow::Hook oot_panel_shadow,oot_header_shadow;
    OOT3D::SaveMenu::QuitBackdrop oot_quit_backdrop;
    OOT3D::GearPanel::Hook oot_gear_panel;
    OOT3D::HeapDiagnostics::Hook oot_heap_diagnostics;
    OOT3D::PauseShadow::MapEdges oot_map_edges;
    OOT3D::PauseShadow::Hook oot_map_shadow;
    OOT3D::PauseShadow::ArrowHook oot_arrow_shadow;
    OOT3D::PauseShadow::TabHook oot_tab_shadow;
    OOT3D::DungeonSides::Hook oot_dungeon_sides;
    OOT3D::MapExtension::Hook oot_map_extension;
    OOT3D::MapParticles::Hook oot_map_particles;
    OOT3D::WorldMap::Hook oot_world_map;
    OOT3D::WorldMap::Clouds oot_world_clouds;
#endif
public:
    explicit DynarmicUserCallbacks(ARM_Dynarmic& parent)
        : parent(parent), svc_context(parent.system), memory(parent.memory) {}
    ~DynarmicUserCallbacks() = default;

#ifdef ENABLE_MM3D_PRESENTATION
    MM3D::FormCamera::Hook form_camera;
    MM3D::FormCamera::ArrivalLens arrival_lens;
    MM3D::FormCamera::OcarinaReturnLens ocarina_return_lens;
    MM3D::FormCamera::DekuFreeCamera deku_free_camera;
    MM3D::FormCamera::EntryRecoveryOrbit entry_recovery_orbit;
    unsigned form_trace_count=0,form_projection_count=0;
    float form_last_native=-1.f,form_last_output=-1.f,form_last_radius=-1.f;
    u32 form_last_mode=~0u;
    std::array<u32,6> form_last_pose{};
    bool form_trace_was_active=false;
    MM3D::NativeMinimap::Hook minimap_hook;
    MM3D::DungeonMap::Hook dungeon_map_hook;
    MM3D::InventoryShadow::Hook inventory_shadow_hook;
    MM3D::ItemsMapPrompt::Hook items_map_prompt_hook,map_items_prompt_hook;
    MM3D::MenuHelp::Hook menu_help_hook,select_help_hook;
    MM3D::WorldMapPrompt::Hook world_map_prompt_hook;
    MM3D::MenuTabs::Hook menu_tabs_hook;
    MM3D::NativeInventory::Hook inventory_hook;
    MM3D::OptionsMenu::Hook options_hook;
    MM3D::MenuPageSwitch::Hook menu_page_switch;
    MM3D::MenuMemory::Hook menu_memory;
    MM3D::MenuBackdrop::Hook menu_backdrop;
    u32 menu_sound_context=0;
    MM3D::PauseCapture::Hook pause_capture_hook;
    MM3D::VisibilityDepth::Hook visibility_depth_hook;
    MM3D::NativeNotebook::Hook notebook_hook;
    MM3D::NativeFileSelect::Hook file_select_hook;
    MM3D::FileSelectPrompt::Hook file_select_prompt_hook;
    MM3D::OcarinaPrompt::Hook ocarina_hook;
    MM3D::MagicBorder::Hook magic_hook;
    MM3D::MinigameHud::Hook minigame_hook;
    MM3D::PlayerHud::Hook player_hud_hook;
    MM3D::SongList::Hook song_hook;
    MM3D::DoubleTime::Hook double_time_hook;
    MM3D::ClassicDoubleTime::Hook classic_double_time_hook;
    MM3D::SongReset::Hook song_reset_hook;
    MM3D::HudButtons::Hook hud_buttons_hook;
    MM3D::Banners::Hook banner_hook;
    MM3D::NativeItemPresent::Hook item_present_hook;
    MM3D::NativeStartup::Hook startup_hook;
    MM3D::LinkPoseCapture::Hook link_pose_hook;
    MM3D::NativeStartup::PressHook press_hook;
    MM3D::NativeStartup::LoadingIconHook loading_icon_hook;
    MM3D::ItemPreview::Hook item_preview_hook;
    MM3D::ClassicSaving::Hook classic_save_hook;
    MM3D::DeathPenalties::RespawnSound death_respawn_sound;
    MM3D::ClassicSaving::OwlDraw classic_owl_draw;
    MM3D::ClassicSaving::FileMarker classic_file_marker;
    // Bounded observation for the unresolved sword-activation movement hold.
    u32 owl_trace_actor=0,owl_trace_play=0,owl_trace_ticks=0;
    void TraceOwlState(const char* stage) {
        if(!owl_trace_actor||!owl_trace_play)return;
        const u32 player=memory.Read32OrNullopt(owl_trace_play+0x20dc).value_or(0);
        const auto read=[&](u32 at){return memory.Read32OrNullopt(at).value_or(0xffffffff);};
        LOG_INFO(Core_ARM11,"Owl activation trace: stage={} tick={} owl={:08x} play={:08x} player={:08x} owl_state={:08x} owl_action={:08x} actor_flags={:08x} player_action={:08x} talk_actor={:08x} talk_state={:08x} player_flags={:08x}/{:08x} cs={:08x}/{:08x}/{:08x} message={:08x}",
            stage,owl_trace_ticks,owl_trace_actor,owl_trace_play,player,
            read(owl_trace_actor+0x264),read(owl_trace_actor+0x268),read(owl_trace_actor+4),
            player?read(player+0x92c):0,player?read(player+0x930):0,player?read(player+0x950):0,
            player?read(player+0x11db0):0,player?read(player+0x11db4):0,
            read(0x687c14),read(0x687c18),read(0x687c20),read(owl_trace_play+0x825c));
    }

    MM3D::DialogueScale::Hook dialogue_hook;
    MM3D::CameraWriter::Instruction camera_instruction;
    void TraceCameraWrite(VAddr address, std::uint32_t value) {
        if(!parent.camera_tracing || !MM3D::CameraWriter::active.load() ||
           !MM3D::CameraWriter::Matches(value)) return;
        MM3D::CameraWriter::Hit hit;
        hit.instruction = camera_instruction; hit.address = address; hit.value = value;
        hit.core = parent.GetID();
        const auto process = parent.system.Kernel().GetCurrentProcess();
        if(!process) return;
        hit.process = process->process_id;
        hit.instruction_word = memory.Read32OrNullopt(hit.instruction.pc & ~3u).value_or(0);
        for(unsigned i=0; i<hit.stack.size(); ++i) {
            const std::uint64_t at = std::uint64_t(hit.instruction.sp) + i*4;
            if(at > 0xffffffffULL) break;
            const auto word = memory.Read32OrNullopt(static_cast<VAddr>(at));
            if(word) { hit.stack[i] = *word; hit.stack_valid |= 1u << i; }
        }
        MM3D::CameraWriter::Observe(hit);
    }
#endif
    std::optional<std::uint32_t> MemoryReadCode(VAddr vaddr) override {
        const auto instruction=memory.Read32OrNullopt(vaddr);
#ifdef ENABLE_OOT3D_PRESENTATION
        if(instruction && OOT3D::GraphicsMemory::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::GraphicsMemory::Supported(code))return OOT3D::GraphicsMemory::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::GraphicsAllocations::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::GraphicsAllocations::CodeMatches(code))
                return OOT3D::GraphicsAllocations::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::HeapDiagnostics::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::HeapDiagnostics::CodeMatches(code))return OOT3D::HeapDiagnostics::Instruction(vaddr,*instruction);
        }
        if(instruction && vaddr==OOT3D::Prices::TextSite.pc && *instruction==OOT3D::Prices::TextSite.instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
                OOT3D::Prices::TextCodeMatches(code))return 0xef000000|OOT3D::Prices::TextSVC;
        }
        if (instruction && OOT3D::Randomizer::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
                OOT3D::Randomizer::CodeMatches(code))
                return OOT3D::Randomizer::Instruction(vaddr,*instruction);
        }
        if (instruction && OOT3D::HDFont::Instruction(vaddr, *instruction) != *instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            const auto process = parent.system.Kernel().GetCurrentProcess();
            if (process && process->codeset &&
                process->codeset->program_id == OOT3D::TitleScreen::ProgramId &&
                OOT3D::HDFont::CodeMatches(code))
                return OOT3D::HDFont::Instruction(vaddr, *instruction);
        }
        if(instruction && OOT3D::MasterQuest::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::MasterQuest::CodeMatches(code))
                return OOT3D::MasterQuest::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::MinimapControl::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::MinimapControl::CodeMatches(code))
                return OOT3D::MinimapControl::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::FileSelectTiming::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::FileSelectTiming::CodeMatches(code))
                return OOT3D::FileSelectTiming::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::HeartPulse::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::HeartPulse::CodeMatches(code))return OOT3D::HeartPulse::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::BossChallenge::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::BossChallenge::CodeMatches(code))return OOT3D::BossChallenge::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::DeathMenu::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::DeathMenu::CodeMatches(code))return OOT3D::DeathMenu::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::FileSelect::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::FileSelect::CodeMatches(code))return OOT3D::FileSelect::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::Ocarina::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::Ocarina::CodeMatches(code))return OOT3D::Ocarina::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::PauseFramerate::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::PauseFramerate::CodeMatches(code) && OOT3D::PauseDescription::CodeMatches(code))
                return OOT3D::PauseFramerate::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::PauseDescription::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::PauseDescription::CodeMatches(code))
                return OOT3D::PauseDescription::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::PauseHeader::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::PauseHeader::CodeMatches(code))return OOT3D::PauseHeader::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::PauseMenu::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::PauseMenu::CodeMatches(code))
                return OOT3D::PauseMenu::Instruction(vaddr,*instruction);
        }
        if (instruction && OOT3D::OrbitCamera::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::OrbitCamera::CodeMatches(code))
                return OOT3D::OrbitCamera::Instruction(vaddr,*instruction);
        }
        if (instruction && OOT3D::DialogueCamera::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::DialogueCamera::CodeMatches(code))
                return OOT3D::DialogueCamera::Instruction(vaddr,*instruction);
        }
        if (instruction && OOT3D::Camera::Instruction(vaddr, *instruction) != *instruction) {
            const auto process = parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            if (process && process->codeset &&
                process->codeset->program_id == OOT3D::TitleScreen::ProgramId &&
                OOT3D::Camera::CodeMatches(code))
                return OOT3D::Camera::Instruction(vaddr, *instruction);
        }
        if (instruction && OOT3D::ScreenModels::Instruction(vaddr, *instruction) != *instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            const auto process = parent.system.Kernel().GetCurrentProcess();
            if (process && process->codeset &&
                process->codeset->program_id == OOT3D::TitleScreen::ProgramId &&
                OOT3D::ScreenModels::CodeMatches(code))
                return OOT3D::ScreenModels::Instruction(vaddr, *instruction);
        }
        if(instruction && OOT3D::ItemEquip::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::ItemEquip::CodeMatches(code))
                return OOT3D::ItemEquip::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::ItemUse::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::ItemUse::CodeMatches(code))
                return OOT3D::ItemUse::Instruction(vaddr,*instruction);
        }
        if(instruction&&OOT3D::Music::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000033500ULL&&OOT3D::Music::CodeMatches(code))
                return OOT3D::Music::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::Rumble::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::Rumble::CodeMatches(code))
                return OOT3D::Rumble::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::Difficulty::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::Difficulty::CodeMatches(code))
                return OOT3D::Difficulty::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::Prices::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::Prices::CodeMatches(code))
                return OOT3D::Prices::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::Shops::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::Shops::CodeMatches(code))
                return OOT3D::Shops::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::ActorDrawDistance::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct Code { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
            } code{memory};
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::ActorDrawDistance::CodeMatches(code))
                return OOT3D::ActorDrawDistance::Instruction(vaddr,*instruction);
        }
        if(instruction && (vaddr==OOT3D::SunReadbackProbe::Entry || vaddr==OOT3D::SunReadbackProbe::Load)) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId) {
                if(OOT3D::SunReadbackProbe::CodeMatches(code)) {
                    OOT3D::SunReadbackProbe::installed.fetch_or(vaddr==OOT3D::SunReadbackProbe::Entry?1:2);
                    return OOT3D::SunReadbackProbe::Instruction(vaddr,*instruction);
                }
                if(OOT3D::SunReadbackProbe::rejected.fetch_add(1)==0)
                    LOG_WARNING(Core_ARM11,"OoT sun-readback verification: unsupported native code at {:08X}",vaddr);
            }
        }
        if(instruction && OOT3D::ArrowCycle::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::ArrowCycle::CodeMatches(code))
                return OOT3D::ArrowCycle::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::QOL::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::QOL::CodeMatches(code))
                return OOT3D::QOL::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::Interpolation::Pose::FishingFoliage::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::Interpolation::Pose::FishingFoliage::CodeMatches(code))
                return OOT3D::Interpolation::Pose::FishingFoliage::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::Interpolation::Pose::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::Interpolation::Pose::CodeMatches(code))
                return OOT3D::Interpolation::Pose::Instruction(vaddr,*instruction);
        }
        if(instruction && OOT3D::Interpolation::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId && OOT3D::Interpolation::CodeMatches(code))
                return OOT3D::Interpolation::Instruction(vaddr,*instruction);
        }
        if (instruction && OOT3D::DPad::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process && process->codeset && process->codeset->program_id==OOT3D::TitleScreen::ProgramId &&
               OOT3D::DPad::CodeMatches(code))return OOT3D::DPad::Instruction(vaddr,*instruction);
        }
        if (instruction && OOT3D::HUD::Instruction(vaddr, *instruction) != *instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            const auto process = parent.system.Kernel().GetCurrentProcess();
            if (process && process->codeset &&
                process->codeset->program_id == OOT3D::TitleScreen::ProgramId &&
                OOT3D::HUD::CodeMatches(code))
                return OOT3D::HUD::Instruction(vaddr, *instruction);
        }
        if (instruction && OOT3D::TitleScreen::Instruction(vaddr, *instruction) != *instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            const auto process = parent.system.Kernel().GetCurrentProcess();
            if (process && process->codeset &&
                process->codeset->program_id == OOT3D::TitleScreen::ProgramId &&
                OOT3D::TitleScreen::CodeMatches(code))
                return OOT3D::TitleScreen::Instruction(vaddr, *instruction);
        }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
        if(instruction&&MM3D::ScreenShrink::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::ScreenShrink::CodeMatches(code))
                return MM3D::ScreenShrink::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::ClassicSaving::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::ClassicSaving::CodeMatches(code))
                return MM3D::ClassicSaving::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::MinimapShortcut::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::MinimapShortcut::CodeMatches(code))
                return MM3D::MinimapShortcut::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::BottleHealing::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::BottleHealing::CodeMatches(code))
                return MM3D::BottleHealing::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::DeathPenalties::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::DeathPenalties::CodeMatches(code))
                return MM3D::DeathPenalties::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::Difficulty::SpawnInstruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::Difficulty::SpawnCodeMatches(code,vaddr))
                return MM3D::Difficulty::SpawnInstruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::PropDrawDistance::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::PropDrawDistance::CodeMatches(code,vaddr))
                return MM3D::PropDrawDistance::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::VegetationDrawDistance::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::VegetationDrawDistance::CodeMatches(code,vaddr))
                return MM3D::VegetationDrawDistance::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::ActorDrawDistance::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::ActorDrawDistance::CodeMatches(code,vaddr))
                return MM3D::ActorDrawDistance::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::Difficulty::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::Difficulty::CodeMatches(code,vaddr))
                return MM3D::Difficulty::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::FastTime::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::FastTime::CodeMatches(code))
                return MM3D::FastTime::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::NativeStartup::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::NativeStartup::CodeMatches(code))
                return MM3D::NativeStartup::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::ScheduleRows::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::ScheduleRows::CodeMatches(code))
                return MM3D::ScheduleRows::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::NotebookRows::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::NotebookRows::CodeMatches(code))
                return MM3D::NotebookRows::Instruction(vaddr,*instruction);
        }
        if(instruction && MM3D::NativeNotebook::MapInstruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);} } code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::NativeNotebook::MapCodeMatches(code))
                return MM3D::NativeNotebook::MapInstruction(vaddr,*instruction);
        }
        if(instruction && MM3D::NativeNotebook::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);} } code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::NativeNotebook::CodeMatches(code))
                return MM3D::NativeNotebook::Instruction(vaddr,*instruction);
        }
        if(instruction && MM3D::ItemPreview::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            if(process && process->codeset && process->codeset->program_id==0x0004000000125500ULL &&
               MM3D::ItemPreview::CodeMatches(code))
                return MM3D::ItemPreview::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::HudButtons::VisibilityInstruction(vaddr,*instruction)!=*instruction){
            struct ReadMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::HudButtons::VisibilityCodeMatches(code))
                return MM3D::HudButtons::VisibilityInstruction(vaddr,*instruction);
        }
        if(instruction&&vaddr==MM3D::SongList::NavPC&&*instruction==MM3D::SongList::NavWord) {
            struct ReadMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::SongList::CodeMatches(code))return 0xef000000|MM3D::SongList::NavSVC;
        }
        if(instruction&&MM3D::DungeonMap::Instruction(vaddr,*instruction)!=*instruction) {
            struct ReadMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::DungeonMap::CodeMatches(code))
                return MM3D::DungeonMap::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::OptionsMenu::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::OptionsMenu::CodeMatches(code))
                return MM3D::OptionsMenu::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::MenuShortcuts::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::MenuShortcuts::CodeMatches(code))
                return MM3D::MenuShortcuts::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::MenuClose::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::MenuClose::CodeMatches(code))
                return MM3D::MenuClose::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::BomberApproach::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::BomberApproach::CodeMatches(code))
                return MM3D::BomberApproach::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::OwlIntro::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::OwlIntro::CodeMatches(code))
                return MM3D::OwlIntro::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::OwlSword::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::OwlSword::CodeMatches(code))
                return MM3D::OwlSword::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::TatlReminders::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::TatlReminders::CodeMatches(code))
                return MM3D::TatlReminders::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::Music::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::Music::CodeMatches(code))
                return MM3D::Music::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::MenuSound::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::MenuSound::CodeMatches(code))
                return MM3D::MenuSound::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::ClassicDoubleTime::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::ClassicDoubleTime::CodeMatches(code))
                return MM3D::ClassicDoubleTime::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::Prices::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::Prices::CodeMatches(code))
                return MM3D::Prices::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::SingleScreenText::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::SingleScreenText::CodeMatches(code))
                return MM3D::SingleScreenText::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::MenuBackdrop::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::MenuBackdrop::CodeMatches(code))
                return MM3D::MenuBackdrop::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::MenuMemory::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::MenuMemory::CodeMatches(code))
                return MM3D::MenuMemory::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::MenuPageSwitch::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::MenuPageSwitch::CodeMatches(code))
                return MM3D::MenuPageSwitch::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::InventoryNavigation::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::InventoryNavigation::CodeMatches(code))
                return MM3D::InventoryNavigation::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::NativeItemPresent::IntroInstruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::NativeItemPresent::IntroCodeMatches(code))
                return MM3D::NativeItemPresent::IntroInstruction(vaddr,*instruction);
        }
        if (instruction && MM3D::VisibilityDepth::Instruction(vaddr, *instruction) != *instruction) {
            struct CodeMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            const auto process = parent.system.Kernel().GetCurrentProcess();
            if (process && process->codeset && process->codeset->program_id == MM3D::VisibilityDepth::Title &&
                MM3D::VisibilityDepth::CodeMatches(code))
                return MM3D::VisibilityDepth::Instruction(vaddr, *instruction);
        }
        if(instruction&&MM3D::PauseCapture::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::PauseCapture::CodeMatches(code))
                return MM3D::PauseCapture::Instruction(vaddr,*instruction);
        }
        if(instruction && MM3D::NativeInventory::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            if(process && process->codeset && process->codeset->program_id==0x0004000000125500ULL &&
               MM3D::NativeInventory::CodeMatches(code))
                return MM3D::NativeInventory::Instruction(vaddr,*instruction);
        }
        if(instruction && MM3D::DialogueScale::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            if(process && process->codeset && process->codeset->program_id==0x0004000000125500ULL &&
               MM3D::DialogueScale::CodeMatches(code))
                return MM3D::DialogueScale::Instruction(vaddr,*instruction);
        }
        if (instruction && MM3D::NativeMinimap::Instruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            if (process && process->codeset && process->codeset->program_id==0x0004000000125500ULL &&
                MM3D::NativeMinimap::CodeMatches(code))
                return MM3D::NativeMinimap::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::FormCamera::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::FormCamera::CodeMatches(code))
                return MM3D::FormCamera::Instruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::CameraLive::ChangeModeInstruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::CameraLive::ChangeModeCodeMatches(code))
                return MM3D::CameraLive::ChangeModeInstruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::CameraLive::DialogueCameraInstruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::CameraLive::DialogueCameraCodeMatches(code))
                return MM3D::CameraLive::DialogueCameraInstruction(vaddr,*instruction);
        }
        if(instruction&&MM3D::SongEffect::Instruction(vaddr,*instruction)!=*instruction) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} code{memory};
            const auto process=parent.system.Kernel().GetCurrentProcess();
            if(process&&process->codeset&&process->codeset->program_id==0x0004000000125500ULL&&MM3D::SongEffect::CodeMatches(code))
                return MM3D::SongEffect::Instruction(vaddr,*instruction);
        }
        if(instruction && MM3D::CameraLive::ProjectionInstruction(vaddr,*instruction)!=*instruction) {
            const auto process=parent.system.Kernel().GetCurrentProcess();
            struct CodeMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } code{memory};
            if(process && process->codeset && process->codeset->program_id==0x0004000000125500ULL &&
               MM3D::CameraLive::CodeMatches(code,vaddr))
                return MM3D::CameraLive::ProjectionInstruction(vaddr,*instruction);
        }
#endif
        return instruction;
    }

    std::uint8_t MemoryRead8(VAddr vaddr) override {
        return memory.Read8(vaddr);
    }
    std::uint16_t MemoryRead16(VAddr vaddr) override {
        return memory.Read16(vaddr);
    }
    std::uint32_t MemoryRead32(VAddr vaddr) override {
#ifdef ENABLE_MM3D_PRESENTATION
        if(vaddr>=Memory::VRAM_VADDR && vaddr<Memory::VRAM_VADDR_END &&
           MM3D::InterpolationPerformance::pending.load(std::memory_order_relaxed)) {
            const auto& regs=parent.jit->Regs();
            MM3D::InterpolationPerformance::GuestReadScope origin{vaddr,regs[15],regs[14]};
            return memory.Read32(vaddr);
        }
#endif
        return memory.Read32(vaddr);
    }
    std::uint64_t MemoryRead64(VAddr vaddr) override { return memory.Read64(vaddr); }

    void MemoryWrite8(VAddr vaddr, std::uint8_t value) override {
        memory.Write8(vaddr, value);
    }
    void MemoryWrite16(VAddr vaddr, std::uint16_t value) override {
        memory.Write16(vaddr, value);
    }
    void MemoryWrite32(VAddr vaddr, std::uint32_t value) override {
        memory.Write32(vaddr, value);
#ifdef ENABLE_MM3D_PRESENTATION
        TraceCameraWrite(vaddr, value);
#endif
    }
    void MemoryWrite64(VAddr vaddr, std::uint64_t value) override {
        memory.Write64(vaddr, value);
#ifdef ENABLE_MM3D_PRESENTATION
        TraceCameraWrite(vaddr, std::uint32_t(value));
        TraceCameraWrite(vaddr + 4, std::uint32_t(value >> 32));
#endif
    }

    bool MemoryWriteExclusive8(u32 vaddr, u8 value, u8 expected) override {
        return memory.WriteExclusive8(vaddr, value, expected);
    }
    bool MemoryWriteExclusive16(u32 vaddr, u16 value, u16 expected) override {
        return memory.WriteExclusive16(vaddr, value, expected);
    }
    bool MemoryWriteExclusive32(u32 vaddr, u32 value, u32 expected) override {
        const bool written = memory.WriteExclusive32(vaddr, value, expected);
#ifdef ENABLE_MM3D_PRESENTATION
        if(written) {
            TraceCameraWrite(vaddr, std::uint32_t(value));
        }
#endif
        return written;
    }
    bool MemoryWriteExclusive64(u32 vaddr, u64 value, u64 expected) override {
        const bool written = memory.WriteExclusive64(vaddr, value, expected);
#ifdef ENABLE_MM3D_PRESENTATION
        if(written) {
            TraceCameraWrite(vaddr, std::uint32_t(value));
            TraceCameraWrite(vaddr + 4, std::uint32_t(value >> 32));
        }
#endif
        return written;
    }

    void InterpreterFallback(VAddr pc, std::size_t num_instructions) override {
        // Should never happen.
        UNREACHABLE_MSG("InterpeterFallback reached with pc = 0x{:08x}, code = 0x{:08x}, num = {}",
                        pc, MemoryReadCode(pc).value(), num_instructions);
    }

#ifdef ENABLE_OOT3D_PRESENTATION
    void ReportGraphicsAllocations() {
        struct GuestMemory {Memory::MemorySystem& memory;
            std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
        } guest{memory};
        const auto now=parent.system.CoreTiming().GetGlobalTimeUs().count();
        std::scoped_lock lock{OOT3D::GraphicsAllocations::mutex};
        OOT3D::GraphicsAllocations::tracker.Report(guest,now,
            [&](const auto& t,const auto& pool,const auto& c,const char* reason) {
                LOG_INFO(Core_ARM11,"OoT graphics ledger: us={} reason={} generation={} pool_valid={} capacity={} occupied_with_overhead={} free={} largest={} tracked_live_bytes={} tracked_live_count={} allocations={} frees={} failures={} dropped={} unknown_frees={} address_reuses={} scene={} file={} save={}/{} items={} gear={} map={}",
                    now,reason,t.generation,pool.valid,pool.capacity,pool.valid?pool.capacity-pool.bytes:0,
                    pool.bytes,pool.largest,t.live_bytes,t.live.size(),t.allocations,t.frees,t.failures,
                    t.dropped,t.unknown_frees,t.reused,c[0],c[1],c[2],c[3],c[4],c[5],c[6]);
                if(t.failures!=t.reported_failures) {
                    const auto& k=t.failed_key;const auto& o=k.origin;
                    LOG_ERROR(Core_ARM11,"OoT graphics failed request: bytes={} caller={:08X} stack_hints={:08X}/{:08X}/{:08X} scene={} file={} save={}/{} items={} gear={} map={} interval_failures={}",
                        t.failed_size,k.caller,k.hints[0],k.hints[1],k.hints[2],o[0],o[1],o[2],o[3],o[4],o[5],o[6],t.failures-t.reported_failures);
                }
            },[&](const auto& c) {
                const auto& k=c.key;const auto& o=k.origin;
                LOG_INFO(Core_ARM11,"OoT graphics caller: us={} caller={:08X} stack_hints={:08X}/{:08X}/{:08X} origin_scene={} file={} save={}/{} items={} gear={} map={} live_bytes={} live_count={} delta_bytes={} peak_bytes={} allocations={} frees={} failures={} first_us={}",
                    now,k.caller,k.hints[0],k.hints[1],k.hints[2],o[0],o[1],o[2],o[3],o[4],o[5],o[6],
                    c.live_bytes,c.live_count,c.Delta(),c.peak_bytes,c.allocations,c.frees,c.failures,c.first);
            });
    }
#endif
    void CallSVC(std::uint32_t swi) override {
#ifdef ENABLE_OOT3D_PRESENTATION
        if(swi>=OOT3D::GraphicsAllocations::FirstSVC &&
           swi<OOT3D::GraphicsAllocations::FirstSVC+OOT3D::GraphicsAllocations::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } guest{memory};
            bool failed=false;
            {
                std::scoped_lock lock{OOT3D::GraphicsAllocations::mutex};
                failed=OOT3D::GraphicsAllocations::tracker.Handle(guest,parent.jit->Regs(),
                    swi-OOT3D::GraphicsAllocations::FirstSVC,parent.system.CoreTiming().GetGlobalTimeUs().count());
            }
            if(failed)ReportGraphicsAllocations();
            return;
        }
        if(swi==OOT3D::HeapDiagnostics::SVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } guest{memory};
            oot_heap_diagnostics.Return(guest,parent.jit->Regs(),
                [](u32 caller,u32 size,u32 result,const auto& page,u64 requests,u64 bytes) {
                    LOG_INFO(Core_ARM11,"OoT heap allocation: caller={:08X} size={} result={:08X} save={}/{}/{} items={} gear={} map={} requests={} requested_bytes={} (cumulative, not live usage)",
                        caller,size,result,page[0],page[1],page[2],page[3],page[4],page[5],requests,bytes);
                },[](const auto& entry) {
                    LOG_INFO(Core_ARM11,"OoT heap caller: pc={:08X} requests={} requested_bytes={} failures={}",
                        entry.pc,entry.requests,entry.bytes,entry.failed);
                });
            return;
        }
        // Activation changes JIT substitutions and payload bytes on every core.
        // Dynarmic defers invalidation until the current block has returned.
        struct RandomizerInvalidation {
            Core::System& system;
            unsigned epoch=OOT3D::Randomizer::code_epoch.load();
            ~RandomizerInvalidation() {
                if(epoch!=OOT3D::Randomizer::code_epoch.load())
                    for(const auto& [first,end]:OOT3D::Randomizer::InvalidationRanges())
                        system.InvalidateCacheRange(first,end-first);
            }
        } invalidation{parent.system};
        if(swi==OOT3D::FileSelectTiming::FirstSVC || swi==OOT3D::Randomizer::FirstSVC+11) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            auto& r=parent.jit->Regs();
            u32 flags=parent.jit->Cpsr(),fpscr=parent.jit->Fpscr();
            const bool playing=oot_room_join_chime.Frame(guest,r,parent.jit->ExtRegs(),flags,fpscr,
                r[15]-4,OOT3D::Randomizer::process_epoch.load(),
                parent.system.CoreTiming().GetGlobalTimeUs().count());
            parent.jit->SetCpsr(flags);parent.jit->SetFpscr(fpscr);
            if(playing)return;
        }
        if(swi==OOT3D::Prices::TextSVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            OOT3D::Prices::SelectText(guest,parent.jit->Regs());return;
        }
        if(swi>=OOT3D::Randomizer::FirstSVC && swi<OOT3D::Randomizer::FirstSVC+16) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            if(swi==OOT3D::Randomizer::FirstSVC+15) {
                auto& r=parent.jit->Regs();
                const auto actor=r[4];
                const auto ghost=guest.Read(actor+0x1a4).value_or(0);
                const auto camera=OOT3D::Interpolation::CameraSnapshot();
                if(OOT3D::Randomizer::active && ghost && camera.valid &&
                   !OOT3D::PauseMenu::Active(guest) && !OOT3D::SaveMenu::Active(guest)) {
                    const auto node=guest.Read(ghost+0x10).value_or(0)&0xffff;
                    if(auto uds=parent.system.ServiceManager().GetService<Service::NWM::NWM_UDS>("nwm::UDS")) {
                        OOT3D::RemoteNames::Anchor anchor;anchor.name=uds->GetRoomNickname(node);
                        for(unsigned c=0;c<3;++c)anchor.position[c]=std::bit_cast<float>(guest.Read(actor+0x28+4*c).value_or(0));
                        // Keep the label close to the head; the puppet lowers
                        // child Link by 12.5 relative to the adult model.
                        anchor.position[1]+=guest.Read(ghost+0x18).value_or(0)==0?66.:54.5;
                        OOT3D::RemoteNames::Capture(actor,std::move(anchor),camera);
                    }
                }
                r[2]=0;return; // Displaced MOV immediately before the verified puppet draw.
            }
            OOT3D::Randomizer::runtime.Handle(guest,parent.jit->Regs(),swi-OOT3D::Randomizer::FirstSVC);
            if(!OOT3D::Randomizer::runtime.resource_failure.empty()) {
                parent.system.SetStatus(Core::System::ResultStatus::ErrorInvalidResource,
                    OOT3D::Randomizer::runtime.resource_failure.c_str());
                parent.jit->HaltExecution();
            }
            return;
        }
        if(swi>=OOT3D::MasterQuest::FirstSVC &&
           swi<OOT3D::MasterQuest::FirstSVC+OOT3D::MasterQuest::Sites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            oot_master_quest.Handle(guest,parent.jit->Regs(),swi-OOT3D::MasterQuest::FirstSVC,
                parent.system.CoreTiming().GetGlobalTimeUs().count(),OOT3D::Randomizer::FileFlow{});
            return;
        }
        if(swi>=OOT3D::MinimapControl::FirstSVC && swi<=OOT3D::MinimapControl::FirstSVC+3) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}
            } guest{memory};
            if(swi!=OOT3D::MinimapControl::FirstSVC) {
                oot_hud_routing.minimap_fade.Handle(guest,parent.jit->Regs(),
                    swi-OOT3D::MinimapControl::FirstSVC,OOT3D::visible.load(),
                    parent.system.CoreTiming().GetGlobalTimeUs().count());
                return;
            }
            OOT3D::MinimapControl::ToggleHook hook;
            hook.Handle(guest,parent.jit->Regs(),OOT3D::OrbitCamera::input_allowed.load() &&
                !OOT3D::visible.load() && OOT3D::DPad::Gameplay(guest));
            return;
        }
        if(swi>=OOT3D::FileSelectTiming::FirstSVC &&
           swi<OOT3D::FileSelectTiming::FirstSVC+OOT3D::FileSelectTiming::Sites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            if(swi==OOT3D::FileSelectTiming::FirstSVC) {
                OOT3D::Randomizer::runtime.CheckEpoch();
                if(OOT3D::Randomizer::runtime.room_import.Frame(guest,parent.jit->Regs()))return;
                if(OOT3D::Randomizer::runtime.ShutdownBeforeFrame(guest,parent.jit->Regs()))return;
                OOT3D::Randomizer::runtime.Frame(guest);
            }
            if(swi==OOT3D::FileSelectTiming::FirstSVC &&
               oot_file_select.name_save.FileDispatch(guest,parent.jit->Regs()))return;
            if(swi==OOT3D::FileSelectTiming::FirstSVC && OOT3D::MasterQuest::CodeMatches(guest) &&
               oot_master_quest.Dispatch(guest,parent.jit->Regs(),
                   parent.system.CoreTiming().GetGlobalTimeUs().count(),OOT3D::Randomizer::FileFlow{}))return;
            u32 flags=parent.jit->Cpsr();
            oot_pause_framerate.file.Handle(guest,parent.jit->Regs(),parent.jit->ExtRegs(),
                swi-OOT3D::FileSelectTiming::FirstSVC,&flags);
            parent.jit->SetCpsr(flags);
            return;
        }
        if (swi == OOT3D::HDFont::FirstSVC) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } guest{memory};
            auto& r = parent.jit->Regs();
            if(const auto requested=OOT3D::GraphicsMemory::FailedFontAllocation(guest,r[5])) {
                const auto now=parent.system.CoreTiming().GetGlobalTimeUs().count();
                oot_graphics_memory.AllocationFailed(now);
                const auto free=OOT3D::GraphicsMemory::TextureFree(guest);
                auto last=oot_graphics_memory.latest;
                last.free=free;last.occupied=free.valid?free.capacity-free.bytes:0;
                last.failures=oot_graphics_memory.failures;last.pressure=true;
                OOT3D::GraphicsMemory::live_diagnostics.Publish(last,now);
                const auto report=fmt::format(
                    "The game was stopped because a required text texture could not be allocated.\n"
                    "Restart the game to continue. Unsaved progress is lost.\n\n"
                    "Resource: {:08x}\nRequested: {} bytes\nGraphics heap valid: {}\n"
                    "Capacity: {} bytes\nFree: {} bytes\nLargest free block: {} bytes\n"
                    "Free blocks: {}\nSampled peak occupied (including overhead): {} bytes\n"
                    "Allocation failures: {}\n\nDetails are also recorded in azahar_log.txt.",
                    r[5],*requested,free.valid,free.capacity,free.bytes,free.largest,
                    free.blocks,oot_graphics_memory.peak_occupied,oot_graphics_memory.failures);
                LOG_CRITICAL(Core,"OoT graphics allocation failure: {}",report);
                parent.system.SetStatus(Core::System::ResultStatus::ErrorGraphicsAllocation,report.c_str());
                parent.jit->HaltExecution();
                return;
            }
            if (auto d = OOT3D::HDFont::NativeTexture(guest, r[4])) {
                oot_native_prompts.Uploaded(memory,*d,r[4],r[5]);
                const auto ptr = memory.GetPointer(d->address);
                const auto base = reinterpret_cast<std::uintptr_t>(memory.GetFCRAMPointer(0));
                const auto at = reinterpret_cast<std::uintptr_t>(ptr);
                const auto size = OOT3D::HDFont::ByteSize(*d);
                if (ptr && at >= base && at - base + size <= Memory::FCRAM_N3DS_SIZE) {
                    d->address = Memory::FCRAM_PADDR + u32(at - base);
                    OOT3D::HDFont::Register(*d, Common::CityHash64(
                        reinterpret_cast<const char*>(ptr), size));
                }
            }
            r[0] = 1; // Replay the original mov r0,#1, leaving flags and return path intact.
            r[15] = OOT3D::HDFont::HookPC + 4;
            return;
        }
        if(swi==OOT3D::HeartPulse::FirstSVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            oot_heart_pulse.Handle(guest,parent.jit->Regs(),parent.system.CoreTiming().GetGlobalTimeUs().count());
            return;
        }
        if(swi>=OOT3D::BossChallenge::FirstSVC && swi<OOT3D::BossChallenge::FirstSVC+OOT3D::BossChallenge::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            auto& r=parent.jit->Regs();
            const unsigned site=swi-OOT3D::BossChallenge::FirstSVC;
            const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                !Settings::values.upright_screen.GetValue();
            if(site==0)oot_boss_challenge.Top(guest,r,supported,
                oot_hud_routing.fade.initialized?oot_hud_routing.fade.opacity:0.f,
                parent.system.CoreTiming().GetGlobalTimeUs().count());
            else oot_boss_challenge.Input(guest,r,site,supported);
            return;
        }
        if(swi>=OOT3D::DeathMenu::FirstSVC && swi<OOT3D::DeathMenu::FirstSVC+OOT3D::DeathMenu::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            auto& r=parent.jit->Regs();
            const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                !Settings::values.upright_screen.GetValue();
            if(swi==OOT3D::DeathMenu::FirstSVC) {
                const auto loading=[&] {
                    oot_death_loading.Begin(guest,r,OOT3D::DeathMenu::Sites[0].pc,true,supported,false,
                        parent.system.CoreTiming().GetGlobalTimeUs().count());
                };
                if(oot_death_loading.Pending()) {oot_death_loading.Resume(guest,r);return;}
                if(oot_death_prompts.Pending()) {
                    oot_death_prompts.Resume(guest,r);
                    if(!oot_death_prompts.Pending())loading();
                    return;
                }
                oot_death_menu.Top(guest,r,supported);
                if(oot_death_menu.stage==OOT3D::DeathMenu::Hook::Stage::Idle) {
                    const auto state=oot_death_menu.state;
                    if(supported && oot_death_menu.route && OOT3D::DeathMenu::on_top.load() &&
                       state>=3 && state<=15 && state!=12) {
                        const auto pane=oot_death_menu.routed_panes[5];
                        oot_death_prompts.BeginDialog(guest,r,OOT3D::DeathMenu::Sites[0].pc,
                            guest.Read(pane+0x64).value_or(0),oot_death_menu.animation.buttons,state<12);
                    }
                    if(!oot_death_prompts.Pending())loading();
                }
            } else if(swi==OOT3D::DeathMenu::FirstSVC+1) {
                if(oot_boss_challenge.SkipBottom(guest,r))r[15]=OOT3D::DeathMenu::Sites[1].pc+4;
                else oot_death_menu.BottomPane(guest,r);
            }
            else if(swi==OOT3D::DeathMenu::FirstSVC+4) {
                OOT3D::TextboxOpening::Apply(guest,r,supported);
                oot_death_menu.PaneAnimation(guest,r,supported);
            }
            else if(swi==OOT3D::DeathMenu::FirstSVC+5)oot_death_menu.Update(guest,r,supported);
            else if(swi==OOT3D::DeathMenu::FirstSVC+6)oot_death_menu.ControlUpdate(guest,r);
            else if(swi>=OOT3D::DeathMenu::FirstSVC+7)
                oot_death_menu.Input(guest,r,swi-OOT3D::DeathMenu::FirstSVC,supported);
            else oot_death_menu.Transition(guest,r,swi-OOT3D::DeathMenu::FirstSVC,supported);
            return;
        }
        if(swi>=OOT3D::FileSelect::FirstSVC && swi<OOT3D::FileSelect::FirstSVC+OOT3D::FileSelect::Sites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            auto& regs=parent.jit->Regs();
            if(swi>=OOT3D::FileSelect::FirstSVC+15)oot_file_select.KeyboardTyping(guest,regs,swi-OOT3D::FileSelect::FirstSVC);
            else if(swi==OOT3D::FileSelect::FirstSVC)oot_file_select.Bottom(guest,regs);
            else if(swi==OOT3D::FileSelect::FirstSVC+6)oot_file_select.NameSubmit(guest,regs);
            else if(swi==OOT3D::FileSelect::FirstSVC+7)OOT3D::FileSelect::AcceptName(guest,regs);
            else if(swi==OOT3D::FileSelect::FirstSVC+14) {
                const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue();
                oot_file_select.NameDispatch(guest,regs,supported,
                    parent.system.CoreTiming().GetGlobalTimeUs().count());
            }
            else if(swi==OOT3D::FileSelect::FirstSVC+13) {
                const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue();
                oot_file_select.KeyboardDelete(guest,regs,supported);
            }
            else if(swi>=OOT3D::FileSelect::FirstSVC+11) {
                const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue();
                OOT3D::FileSelect::KeyboardCursor(guest,regs,parent.jit->ExtRegs(),swi-OOT3D::FileSelect::FirstSVC,supported);
            }
            else if(swi>=OOT3D::FileSelect::FirstSVC+8) {
                const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue();
                oot_file_select.KeyboardNavigation(guest,regs,swi-OOT3D::FileSelect::FirstSVC,supported);
            }
            else if(swi>=OOT3D::FileSelect::FirstSVC+4) {
                const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue();
                oot_file_select.NativeDecoration(guest,regs,swi-OOT3D::FileSelect::FirstSVC,supported);
            }
            else if(swi==OOT3D::FileSelect::FirstSVC+2)oot_file_select.NameBottom(regs);
            else if(swi==OOT3D::FileSelect::FirstSVC+3) {
                const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue();
                oot_file_select.Top(guest,regs,supported,parent.system.CoreTiming().GetGlobalTimeUs().count());
                // Read-only, bounded diagnostics for keyboards disappearing
                // after file operations. Report only settled frame changes;
                // never log names, saves, or every animation frame.
                if(!oot_file_select.Pending()) {
                    auto& file=oot_file_select;
                    if(!file.name_frame) {
                        oot_keyboard_frames=0;oot_keyboard_signature={};
                    } else if(file.name_editable && file.name_progress>=1 &&
                              ++oot_keyboard_frames%60==0 && oot_keyboard_reports<64) {
                        constexpr u32 owner=OOT3D::FileSelect::NameOwner;
                        auto& mesh=file.keyboard_text;
                        const u32 state=guest.Read(owner+4).value_or(0),mode=guest.Read(owner+8).value_or(0);
                        const u32 helper=guest.Read(owner+(mode==3?0x70:0x6c)).value_or(0);
                        const u32 colors=guest.Read(helper+0x18).value_or(0);
                        const std::array<u32,10> signature{state,mode,file.source,file.keyboard_quads,
                            std::bit_cast<u32>(file.keyboard_alpha),u32(file.keyboard_submitted),
                            u32(mesh.Owned(guest)),u32(mesh.creation_failed),u32(mesh.failed_stage),mesh.texture};
                        if(signature!=oot_keyboard_signature) {
                            oot_keyboard_signature=signature;++oot_keyboard_reports;
                            bool samples_match=!mesh.pixels.empty() && mesh.texture!=0;
                            for(unsigned i=0;samples_match && i<mesh.pixels.size();i+=257)
                                samples_match=guest.Read(mesh.texture+i*4)==mesh.pixels[i];
                            const auto b=file.keyboard_bounds;
                            LOG_INFO(Core,"OoT keyboard draw: state={} mode={} quads={} alpha={} submitted={} owned={} failed={}/{} source={:08x} model={:08x} texture={:08x} texture_samples_match={} bounds=({},{},{},{}) native_alpha={:08x} source_tint={:08x}/{:08x}/{:08x}/{:08x}",
                                state,mode,file.keyboard_quads,file.keyboard_alpha,file.keyboard_submitted,
                                signature[6],signature[7],signature[8],file.source,mesh.model,mesh.texture,samples_match,
                                b.left,b.top,b.right,b.bottom,guest.Read(colors+12).value_or(0),
                                guest.Read(file.source+0xf0).value_or(0),guest.Read(file.source+0xf4).value_or(0),
                                guest.Read(file.source+0xf8).value_or(0),guest.Read(file.source+0xfc).value_or(0));
                        }
                    }
                }
            } else if(!oot_file_select.NativeDrawing()) {
                regs[14]=OOT3D::FileSelect::Sites[1].pc+4;regs[15]=regs[1]; // Displaced BLX.
            }
            return;
        }
        if(swi>=OOT3D::Ocarina::FirstSVC && swi<OOT3D::Ocarina::FirstSVC+OOT3D::Ocarina::Sites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            auto& r=parent.jit->Regs();const auto site=swi-OOT3D::Ocarina::FirstSVC;
            const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                !Settings::values.upright_screen.GetValue();
            if(site==0)OOT3D::Ocarina::input.Update(guest,r);
            else if(site==1)OOT3D::Ocarina::input.Navigate(guest,r,supported && OOT3D::Ocarina::on_top.load());
            else if(site==2)OOT3D::Ocarina::input.AfterSound(guest,r);
            else if(site==3)oot_ocarina.Bottom(r);
            else if(site==4 && !oot_ocarina.NativeDrawing()) {r[14]=OOT3D::Ocarina::Sites[4].pc+4;r[15]=r[1];}
            else if(site==5)OOT3D::Ocarina::input.BeginList(guest,r,supported);
            else if(site==6)OOT3D::Ocarina::input.FinishList(guest,r,supported);
            else if(site==7) {
                OOT3D::Ocarina::DrawMessage(guest,r,supported);
                // Bounded diagnostics for an instruction box that survives
                // the free-play guard; no per-frame logging after the cap.
                static unsigned reports=0;
                if(reports<8 && r[0]==OOT3D::Ocarina::Message &&
                   guest.Read(r[0]+0xf40)==std::optional<u32>{0x86e} && r[15]!=r[14]) {
                    ++reports;
                    const auto play=guest.Read(0x5043e0).value_or(0);
                    LOG_INFO(Core_ARM11,"OoT ocarina prompt retained: supported={} play={:08x} context={:08x} action={:08x} phase={:08x} type={:08x} list={}",
                        supported,play,guest.Read(play+0x100).value_or(0),
                        guest.Read(play+0x2b80).value_or(0),guest.Read(play+0x2a90).value_or(0),
                        guest.Read(r[0]).value_or(0),OOT3D::Ocarina::State(guest));
                }
            }
            else if(site==8)OOT3D::Ocarina::StartRecognition(guest,r,supported);
            else if(site==9)OOT3D::Ocarina::NativeCameraLens(guest,r,parent.jit->ExtRegs(),supported);
            return;
        }
        if(swi>=OOT3D::PauseFramerate::SaveFirstSVC &&
           swi<OOT3D::PauseFramerate::SaveFirstSVC+OOT3D::PauseFramerate::SaveSites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
            } guest{memory};
            u32 cpsr=parent.jit->Cpsr();
            oot_pause_framerate.HandleSave(guest,parent.jit->Regs(),parent.jit->ExtRegs(),cpsr,
                swi-OOT3D::PauseFramerate::SaveFirstSVC);
            parent.jit->SetCpsr(cpsr);
            return;
        }
        if(swi>=OOT3D::PauseFramerate::FirstSVC &&
           swi<OOT3D::PauseFramerate::FirstSVC+OOT3D::PauseFramerate::Sites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            if(swi==OOT3D::PauseFramerate::FirstSVC &&
               parent.jit->Regs()[5]==OOT3D::PauseFramerate::Renderer) {
                const auto now=parent.system.CoreTiming().GetGlobalTimeUs().count();
                const auto save_page=guest.Read(0x50a508+0x28).value_or(~0u);
                const auto save_state=guest.Read(0x50a508+0x24).value_or(~0u);
                if(const auto sample=oot_save_wait.Poll(save_page==2 && save_state==9,now)) {
                    if(sample->exited) {
                        LOG_INFO(Core,"OoT save wait exited: elapsed_us={} page={} state={} (menu transition only)",
                            sample->elapsed,save_page,save_state);
                    } else {
                        const auto job=guest.Read(0x50bb58).value_or(0);
                        // Bounds/alignment checked before optional reads; never dump paths,
                        // save contents, or the buffer addressed by the job.
                        const auto process=parent.system.Kernel().GetCurrentProcess();
                        const bool valid_job=OOT3D::TitleScreen::Pointer(job,0x118);
                        const auto field=[&](u32 offset) {
                            return valid_job && process && memory.IsValidVirtualAddress(*process,job+offset)
                                ?guest.Read(job+offset).value_or(~0u):~0u;
                        };
                        LOG_INFO(Core,"OoT save wait: elapsed_us={} tick={} fast={} job={:08X} result={:08X} flags={:08X} op={} tail={:08X} queue_ring={:08X} capacity={} head={} count={} waiters={} producers={} worker_state={}",
                            sample->elapsed,guest.Read(0x50a508+0x2c).value_or(~0u),
                            oot_pause_framerate.fast,job,field(0),field(8),field(12),field(0x114),
                            guest.Read(0x565510).value_or(~0u),
                            guest.Read(0x565530).value_or(~0u),guest.Read(0x565534).value_or(~0u),
                            guest.Read(0x565538).value_or(~0u),guest.Read(0x56553c).value_or(~0u),
                            guest.Read(0x565540).value_or(~0u),guest.Read(0x5bdde0+0x488).value_or(~0u));
                        if(sample->elapsed>=2000000) {
                            for(u32 core=0;core<parent.system.GetNumCores();++core) {
                                for(const auto& thread:parent.system.Kernel().GetThreadManager(core).GetThreadList()) {
                                    if(thread->owner_process.lock()!=process)continue;
                                    LOG_INFO(Core,"OoT save wait thread: core={} id={} status={} entry={:08X} saved_pc={:08X} saved_lr={:08X} arbiter={:08X} wait_objects={} priority={}",
                                        core,thread->thread_id,static_cast<unsigned>(thread->status),
                                        thread->entry_point,thread->context.GetProgramCounter(),
                                        thread->context.GetLinkRegister(),thread->wait_address,
                                        thread->wait_objects.size(),thread->current_priority);
                                }
                            }
                        }
                    }
                }
                const auto previous_sample=oot_graphics_memory.sample_at;
                const auto s=oot_graphics_memory.Poll(guest,now);
                if(previous_sample!=oot_graphics_memory.sample_at) {
                    OOT3D::GraphicsMemory::live_diagnostics.Publish(oot_graphics_memory.latest,now);
                    ReportGraphicsAllocations();
                }
                if(s) {
                    LOG_INFO(Core,"OoT graphics pool: us={} valid={} capacity_bytes={} occupied_including_overhead={} free_bytes={} largest_block={} free_blocks={} sampled_peak_occupied={} sampled_min_largest={} samples={} font_failures={} pressure={} file_state={} save_page={} save_state={} items_state={} gear_state={} map_state={}",
                        now,s->free.valid,s->free.capacity,s->occupied,s->free.bytes,
                        s->free.largest,s->free.blocks,s->peak_occupied,s->min_largest,
                        s->samples,s->failures,s->pressure,
                        guest.Read(0x504fa0+0x10).value_or(~0u),
                        guest.Read(0x50a508+0x28).value_or(~0u),
                        guest.Read(0x50a508+0x24).value_or(~0u),
                        guest.Read(0x5066f8+0x34).value_or(~0u),
                        guest.Read(0x50446c+0x18).value_or(~0u),
                        guest.Read(0x506cb0+0x38).value_or(~0u));
                }
            }
            u32 cpsr=parent.jit->Cpsr();
            oot_pause_framerate.Handle(guest,parent.jit->Regs(),parent.jit->ExtRegs(),cpsr,
                swi-OOT3D::PauseFramerate::FirstSVC,parent.system.CoreTiming().GetGlobalTimeUs().count());
            parent.jit->SetCpsr(cpsr);
            return;
        }
        if(swi>=OOT3D::PauseDescription::FirstSVC &&
           swi<OOT3D::PauseDescription::FirstSVC+OOT3D::PauseDescription::Sites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                !Settings::values.upright_screen.GetValue();
            if(swi==OOT3D::PauseDescription::FirstSVC+6) {
                u32 flags=parent.jit->Cpsr(),fpscr=parent.jit->Fpscr();
                const auto now=parent.system.CoreTiming().GetGlobalTimeUs().count();
                oot_description.Present(guest,parent.jit->Regs(),parent.jit->ExtRegs(),flags,fpscr,now,supported);
                parent.jit->SetCpsr(flags);parent.jit->SetFpscr(fpscr);
                return;
            }
            oot_description.Handle(guest,parent.jit->Regs(),parent.jit->ExtRegs(),
                swi-OOT3D::PauseDescription::FirstSVC,supported,
                oot_pause_framerate.fast || oot_pause_framerate.death_fast || oot_pause_framerate.boss_fast,
                !OOT3D::PauseMenu::Active(guest) && oot_shop_description.Active(guest),
                parent.system.CoreTiming().GetGlobalTimeUs().count());
            return;
        }
        if(swi>=OOT3D::PauseHeader::FirstSVC &&
           swi<=OOT3D::PauseHeader::LastSVC) {
            OotMappedMemory guest{memory,parent.system.Kernel().GetCurrentProcess().get()};
            const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                !Settings::values.upright_screen.GetValue();
            if(swi>=OOT3D::PauseHeader::FloorFirstSVC) {
                oot_header.FloorStep(guest,parent.jit->Regs(),swi-OOT3D::PauseHeader::FloorFirstSVC,supported);
            } else if(swi==OOT3D::PauseHeader::DungeonEntrySVC) {
                oot_header.EnterDungeon(guest,supported);
            } else if(swi==OOT3D::PauseHeader::BackdropSVC) {
                oot_header.Backdrop(guest,parent.jit->Regs(),supported);
                if(OOT3D::PauseHeader::DiagnosticLine())
                    LOG_INFO(Core_ARM11,"OoT save backdrop: animate={} pause={} main={} page={} sub={} tick={} custom={} native={}",
                        OOT3D::SaveMenu::animate,OOT3D::PauseMenu::Active(guest),
                        guest.Read(0x50af34+0x34).value_or(0),
                        guest.Read(OOT3D::SaveMenu::Owner+0x28).value_or(0),
                        guest.Read(OOT3D::SaveMenu::Owner+0x24).value_or(0),
                        guest.Read(OOT3D::SaveMenu::Owner+0x2c).value_or(0),
                        OOT3D::SaveMenu::BackdropActive(guest)?OOT3D::SaveMenu::BackdropOpacity(guest):0.f,
                        parent.jit->Regs()[15]==0x3339e8);
            } else if(swi==OOT3D::PauseHeader::LocationSetupSVC) {
                oot_header.PrepareLocation(guest,parent.jit->Regs(),parent.jit->ExtRegs(),supported);
            } else if(swi>=OOT3D::PauseHeader::CarryFirstSVC) {
                oot_header.CarryMap(guest,parent.jit->Regs(),supported);
            } else if(swi>=OOT3D::PauseHeader::FirstSVC+2) {
                oot_header.CloseStep(guest,parent.jit->Regs(),parent.jit->ExtRegs(),
                    swi-OOT3D::PauseHeader::FirstSVC-2,supported);
            } else if(swi==OOT3D::PauseHeader::FirstSVC) {
                const auto now=parent.system.CoreTiming().GetGlobalTimeUs().count();
                const auto previous=oot_header.last_time;
                oot_header.Update(guest,now,supported);
                if(OOT3D::PauseHeader::DiagnosticActive()) {
                    const auto& cost=oot_dungeon_sides.parchment.timings;
                    LOG_INFO(Core_ARM11, "OoT map performance: wall_us={} game_us={} active={} main_state={} map_state={} open={} progress={} observes={} rebuilds={} uploads={} read_ms={} decode_ms={} compose_ms={} observe_ms={} upload_ms={}",
                        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(),
                        now,OOT3D::PauseMenu::Active(guest),guest.Read(0x50af34+0x34).value_or(0),
                        guest.Read(0x506cb0+0x38).value_or(0),guest.Read(0x54ac4c).value_or(0)>>24,
                        oot_header.slide.progress,cost.observations,cost.rebuilds,cost.uploads,
                        cost.read_ms,cost.decode_ms,cost.compose_ms,cost.observe_ms,cost.upload_ms);
                }
                if(OOT3D::PauseHeader::DiagnosticLine()) {
                    const auto location=guest.Read(OOT3D::MapLocation::Owner+0x38).value_or(0);
                    LOG_INFO(Core_ARM11, "OoT map banner tick: us={} active={} map={} items={} gear={} map_state={} entrance={} progress={} alpha={} y={} owner={:08x} paper={:08x} name={:08x}",
                        now,OOT3D::PauseMenu::Active(guest),OOT3D::PauseMenu::MapPage(guest),
                        guest.Read(0x5066f8+0x34).value_or(0),guest.Read(0x50446c+0x18).value_or(0),
                        guest.Read(0x506cb0+0x38).value_or(0),oot_header.map_open_entrance,
                        oot_header.slide.progress,OOT3D::PauseMenu::world_location_alpha.load(),
                        OOT3D::PauseMenu::world_location_y.load(),location,
                        location?guest.Read(location+0x10).value_or(0):0,
                        location?guest.Read(location+4).value_or(0):0);
                }
                if(OOT3D::PauseHeader::DiagnosticLine())
                    LOG_INFO(Core_ARM11, "OoT save banner tick: us={} dt={} fast={} pause={} save={} sub={} timer={} open={} progress={} y={} duration={} hidden={}",
                        now,previous?now-previous:0,oot_pause_framerate.fast,OOT3D::PauseMenu::Active(guest),
                        guest.Read(OOT3D::SaveMenu::Owner+0x28).value_or(0),
                        guest.Read(OOT3D::SaveMenu::Owner+0x24).value_or(0),
                        guest.Read(OOT3D::SaveMenu::Owner+0x2c).value_or(0),
                        guest.Read(0x54ac4c).value_or(0)>>24,oot_header.slide.progress,oot_header.offset,
                        OOT3D::PauseMenu::GetLayout().transition_seconds,OOT3D::SaveMenu::HideHeader(guest));
                oot_pause.Animate(guest);
                oot_pause.show_contents=oot_header.ShowContents();
            } else {
                auto& regs=parent.jit->Regs();
                if((oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Draw ||
                    oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Banner) && OOT3D::PauseHeader::DiagnosticLine())
                    LOG_INFO(Core_ARM11, "OoT save banner draw: us={} icon={} model={:08x} y={} buffer={} vertices_y={}",
                        parent.system.CoreTiming().GetGlobalTimeUs().count(),
                        oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Banner,regs[0],oot_header.offset,
                        guest.Read(oot_header.block+0x124).value_or(0),
                        std::bit_cast<float>(guest.Read(oot_header.block+OOT3D::PauseHeader::Positions+4).value_or(0)));
                if(OOT3D::PauseHeader::DiagnosticActive() &&
                   OOT3D::MapLocation::Select(guest,regs[0])==OOT3D::PauseMenu::LocationBanner &&
                   OOT3D::PauseHeader::DiagnosticLine())
                    LOG_INFO(Core_ARM11, "OoT map banner draw: us={} model={:08x} deferred={} alpha={}",
                        parent.system.CoreTiming().GetGlobalTimeUs().count(),regs[0],
                        oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Foreground,
                        OOT3D::PauseMenu::world_location_alpha.load());
                if(supported && oot_pause.drawing && !oot_dungeon_sides.Drawing())oot_map_edges.Observe(guest,regs[0]);
                if(supported && oot_pause.drawing && !oot_dungeon_sides.Drawing()) {
                    oot_dungeon_sides.parchment.Observe(guest,regs[0],[](std::uint64_t bits,unsigned x,unsigned y) {
                        const auto rgb=Pica::Texture::SampleETC1Subtile(bits,x,y);
                        return std::array{rgb.r(),rgb.g(),rgb.b()};
                    });
                    if(oot_dungeon_sides.parchment.ready) {
                        const auto& p=oot_dungeon_sides.parchment.prepared.positions;
                        oot_map_edges.bounds=OOT3D::PauseShadow::ToTop({p[0]+1,p[1]+1,p[3]-1,p[7]-1},{});
                    }
                }
                if(oot_boss_challenge.rounding.NativeDraw(guest,regs,
                    supported && oot_boss_challenge.active && oot_boss_challenge.presenting &&
                    oot_boss_challenge.stage==OOT3D::BossChallenge::Hook::Stage::Native &&
                    (oot_boss_challenge.pane==1 || oot_boss_challenge.pane==2),
                    oot_boss_challenge.frame_layout.corner_radius,!oot_boss_challenge.frame_layout.border_bottom))return;
                if(oot_tab_shadow.NativeDraw(guest,regs,supported && oot_pause.drawing))return;
                if(oot_arrow_shadow.NativeDraw(guest,regs,supported && oot_pause.drawing))return;
                const unsigned map_warm_hits=oot_world_map.warm_hits;
                const bool world_map_handled=oot_world_map.NativeDraw(guest,regs,supported && oot_pause.drawing,
                    [](std::uint64_t bits,unsigned x,unsigned y) {
                        const auto rgb=Pica::Texture::SampleETC1Subtile(bits,x,y);
                        return std::array{rgb.r(),rgb.g(),rgb.b()};
                    });
                if(oot_world_map.warm_hits!=map_warm_hits)
                    LOG_INFO(Core_ARM11,"OoT world map: used preloaded artwork (hit {})",oot_world_map.warm_hits);
                if(world_map_handled)return;
                if(guest.Read(0x506cb4)==regs[0] && oot_world_map.diagnostic && oot_world_map.diagnostic_reports++<4)
                    LOG_WARNING(Core_ARM11,"OoT world map fallback: {} image_block={:08x} texture={:08x} failed_stage={} foreground_block={:08x}",
                        oot_world_map.diagnostic,oot_world_map.image.block,oot_world_map.image.texture,
                        unsigned(oot_world_map.image.failed_stage),oot_world_map.contents.block);
                if(oot_world_clouds.NativeDraw(guest,regs,supported && oot_pause.drawing,
                    [](std::uint64_t bits,unsigned x,unsigned y) {
                        const auto rgb=Pica::Texture::SampleETC1Subtile(bits,x,y);
                        return std::array{rgb.r(),rgb.g(),rgb.b()};
                    }))return;
                if(oot_dungeon_sides.NativeDraw(guest,regs,supported && oot_pause.drawing))return;
                oot_header.NativeDraw(guest,regs,supported);
                if(supported && oot_pause.drawing && OOT3D::SaveMenu::BackdropActive(guest) &&
                   guest.Read(OOT3D::PauseMenu::UI+0x10)==std::optional<u32>{regs[0]})
                    regs[15]=regs[14];
                if(supported && OOT3D::PauseMenu::Active(guest) &&
                   (OOT3D::ScreenModels::HideDuringPause(guest,regs[0]) ||
                    OOT3D::ScreenModels::HideSpecialBWhenPaused(guest,regs[0])))regs[15]=regs[14];
            }
            return;
        }
        if(swi>=OOT3D::PauseMenu::FirstSVC && swi<OOT3D::PauseMenu::FirstSVC+OOT3D::PauseMenu::Sites.size()) {
            OotMappedMemory guest{memory,parent.system.Kernel().GetCurrentProcess().get()};
            const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                                 !Settings::values.upright_screen.GetValue();
            const auto site=swi-OOT3D::PauseMenu::FirstSVC;
            if(site==21) {
                u32 flags=parent.jit->Cpsr(),fpscr=parent.jit->Fpscr();
                oot_quit_backdrop.Handle(guest,parent.jit->Regs(),parent.jit->ExtRegs(),
                    flags,fpscr,OOT3D::PauseMenu::Sites[21].pc,supported);
                parent.jit->SetCpsr(flags);parent.jit->SetFpscr(fpscr);
                return;
            }
            if(site==19 && oot_boss_challenge.rounding.immediate) {
                oot_boss_challenge.rounding.ResumeNative(guest,parent.jit->Regs());return;
            }
            if(site==19 && oot_tab_shadow.Pending()) {
                oot_tab_shadow.ResumeNative(guest,parent.jit->Regs());return;
            }
            if(site==19 && oot_arrow_shadow.Pending()) {
                oot_arrow_shadow.ResumeNative(guest,parent.jit->Regs());return;
            }
            if(site==19 && oot_world_map.Pending()) {
                oot_world_map.Resume(guest,parent.jit->Regs());
                if(!oot_world_map.Pending() && oot_world_map.diagnostic && oot_world_map.diagnostic_reports++<4)
                    LOG_WARNING(Core_ARM11,"OoT world map setup: {} image_block={:08x} texture={:08x} failed_stage={} foreground_block={:08x} foreground_failed_stage={}",
                        oot_world_map.diagnostic,oot_world_map.image.block,oot_world_map.image.texture,
                        unsigned(oot_world_map.image.failed_stage),oot_world_map.contents.block,unsigned(oot_world_map.contents.failed_stage));
                return;
            }
            if(site==19 && oot_world_clouds.Pending()) {
                oot_world_clouds.Resume(guest,parent.jit->Regs());return;
            }
            if(site==19 && oot_dungeon_sides.immediate>=0) {
                oot_dungeon_sides.ResumeImmediate(guest,parent.jit->Regs());return;
            }
            if(site==19) {
                auto& regs=parent.jit->Regs();
                const auto now=parent.system.CoreTiming().GetGlobalTimeUs().count();
                if(oot_save_loading.Pending()) {oot_save_loading.Resume(guest,regs);return;}
                if(oot_ocarina.Pending()) {oot_ocarina.Resume(guest,regs);return;}
                if(oot_pause_prompts.Pending()) {
                    oot_pause_prompts.Resume(guest,regs);
                    if(!oot_pause_prompts.Pending())oot_save_loading.Begin(guest,regs,
                        OOT3D::PauseMenu::Sites[19].pc,false,supported,oot_pause.routed,now);
                    return;
                }
                oot_pause.Handle(guest,regs,site,supported);
                if(supported)oot_pause_prompts.Begin(guest,regs,OOT3D::PauseMenu::Sites[19].pc,
                                                    oot_pause.routed,parent.system.CoreTiming().GetGlobalTimeUs().count());
                if(!oot_pause_prompts.Pending()) {
                    oot_save_loading.Begin(guest,regs,OOT3D::PauseMenu::Sites[19].pc,false,supported,oot_pause.routed,now);
                    if(!oot_save_loading.Pending())oot_ocarina.Begin(guest,regs,OOT3D::PauseMenu::Sites[19].pc,supported,now);
                }
                return;
            }
            if(site==0) {oot_world_map.replaced=false;oot_dungeon_sides.Reset();oot_map_edges.Reset();oot_map_extension.ResetFrame();OOT3D::Ocarina::on_top.store(false);}
            const bool side_resuming=site==1 && oot_dungeon_sides.Finishing();
            if(side_resuming) {
                oot_dungeon_sides.Finish(guest,parent.jit->Regs(),OOT3D::PauseMenu::Sites[1].pc);
                if(oot_dungeon_sides.Pending())return;
            }
            const bool map_extension_resuming=site==1 && oot_map_extension.Pending();
            if(map_extension_resuming) {
                oot_map_extension.Resume(guest,parent.jit->Regs());
                if(oot_map_extension.Pending())return;
            }
            const bool map_shadow_resuming=site==1 && oot_map_shadow.Pending();
            if(map_shadow_resuming) {
                oot_map_shadow.Resume(guest,parent.jit->Regs());
                if(oot_map_shadow.Pending())return;
            }
            if(site==14 && oot_gear_panel.Pending()) {
                oot_gear_panel.Resume(guest,parent.jit->Regs());return;
            }
            if(site==5 && oot_panel_shadow.Pending()) {
                oot_panel_shadow.Resume(guest,parent.jit->Regs());
                if(!oot_panel_shadow.Pending() && supported)
                    oot_map_particles.Begin(guest,parent.jit->Regs(),OOT3D::PauseMenu::Sites[5].pc);
                return;
            }
            if(site==5 && oot_map_particles.Pending()) {
                oot_map_particles.Resume(guest,parent.jit->Regs());return;
            }
            if(site==1 && oot_header_shadow.Pending()) {
                oot_header_shadow.Resume(guest,parent.jit->Regs());
                if(!oot_header_shadow.Pending())oot_header.Handle(guest,parent.jit->Regs(),oot_pause.routed);
                return;
            }
            if(site==1 && oot_header.stage!=OOT3D::PauseHeader::Hook::Stage::Idle) {
                oot_header.Handle(guest,parent.jit->Regs(),oot_pause.routed);return;
            }
            if(site==1 && (OOT3D::PauseMenu::Active(guest) || OOT3D::SaveMenu::Active(guest)))
                OOT3D::RemoteNames::Reset();
            const bool draw_map=site==1 && supported &&
                (oot_pause.drawing || map_extension_resuming || map_shadow_resuming) && !side_resuming;
            if(!map_shadow_resuming && !map_extension_resuming && !side_resuming)
                oot_pause.Handle(guest,parent.jit->Regs(),site,supported);
            if(site==10 && supported) {
                auto& regs=parent.jit->Regs();
                if(OOT3D::AtlasSeams::Copy(guest,regs[4],regs[0],regs[7],regs[6]))
                    regs[15]=0x317db8;
            }
            if(site==9 && supported) {
                auto& regs=parent.jit->Regs();
                const float opacity=OOT3D::MinimapControl::Opacity();
                if(OOT3D::MinimapControl::CopyArrows(guest,regs[4],regs[0],regs[8],regs[7],opacity))
                    regs[15]=0x2f99f0; // Same native tail-copy return as the slot tint.
            }
            if(draw_map) {
                if(!map_extension_resuming && !map_shadow_resuming && !oot_world_map.replaced) {
                    oot_map_extension.Begin(guest,parent.jit->Regs(),OOT3D::PauseMenu::Sites[1].pc);
                    if(oot_map_extension.Pending())return;
                }
                if(!map_shadow_resuming && OOT3D::PauseMenu::MapPage(guest)) {
                    oot_map_shadow.Begin(guest,parent.jit->Regs(),OOT3D::PauseMenu::Sites[1].pc,
                        oot_map_edges.Make(OOT3D::PauseMenu::GetLayout()));
                    if(oot_map_shadow.Pending())return;
                }
            }
            if(site==14 && supported && oot_pause.drawing) {
                oot_gear_panel.Begin(guest,parent.jit->Regs(),OOT3D::PauseMenu::Sites[14].pc,
                    OOT3D::GearPanel::Current(guest,OOT3D::PauseMenu::GetLayout()));
            }
            if(site==5 && supported && oot_pause.drawing && !oot_pause.background_copy) {
                oot_panel_shadow.Begin(guest,parent.jit->Regs(),OOT3D::PauseMenu::Sites[5].pc,
                    OOT3D::PauseShadow::Panels(guest,OOT3D::PauseMenu::GetLayout()));
                if(!oot_panel_shadow.Pending())
                    oot_map_particles.Begin(guest,parent.jit->Regs(),OOT3D::PauseMenu::Sites[5].pc);
            }
            if(site==1 && supported) {
                if(!side_resuming) {
                    oot_dungeon_sides.Finish(guest,parent.jit->Regs(),OOT3D::PauseMenu::Sites[1].pc);
                    if(oot_dungeon_sides.Pending())return;
                }
                if(oot_header.presenting && oot_header.slide.progress>0 && !OOT3D::SaveMenu::HideHeader(guest)) {
                    oot_header_shadow.Begin(guest,parent.jit->Regs(),OOT3D::PauseHeader::Entry,
                        OOT3D::PauseShadow::Header(OOT3D::PauseMenu::GetLayout(),oot_header.offset,OOT3D::Widescreen::Active()));
                }
                if(!oot_header_shadow.Pending())oot_header.Handle(guest,parent.jit->Regs(),oot_pause.routed);
            }
            return;
        }
        if(swi==OOT3D::OrbitCamera::FirstSVC || swi==OOT3D::OrbitCamera::FirstSVC+1) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
                void Write(u32 at,u32 value) { memory.Write32(at,value); }
            } guest{memory};
            auto& regs=parent.jit->Regs();
            if(swi==OOT3D::OrbitCamera::FirstSVC) {
                const u32 native_target=regs[1];
                TraceOotTransition(OOT3D::CameraTransitionDiagnostics::Begin,regs[0],0,0,0,native_target);
                TraceOotDialogue(OOT3D::DialogueCamera::Trace::Begin,regs[0]);
                oot_orbit.Begin(guest,regs,OOT3D::Interpolation::guest_frames.load());
                if(regs[15]==(native_target&~1u))
                    parent.jit->SetCpsr((parent.jit->Cpsr()&~0x20u)|((native_target&1u)<<5));
            } else {
                u32 flags=parent.jit->Cpsr(),fpscr=parent.jit->Fpscr();
                TraceOotTransition(OOT3D::CameraTransitionDiagnostics::NativeReturn,regs[4]);
                oot_orbit.Finish(guest,regs,&parent.jit->ExtRegs(),&flags,&fpscr);
                parent.jit->SetCpsr(flags);parent.jit->SetFpscr(fpscr);
                if(regs[15]==OOT3D::OrbitCamera::Resume) {
                    TraceOotTransition(OOT3D::CameraTransitionDiagnostics::Finish,regs[4]);
                    TraceOotDialogue(OOT3D::DialogueCamera::Trace::Finish,regs[4]);
                }
            }
            return;
        }
        if(swi>=OOT3D::DialogueCamera::FirstSVC &&
           swi<OOT3D::DialogueCamera::FirstSVC+OOT3D::DialogueCamera::Sites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            auto& regs=parent.jit->Regs();
            const auto frame=OOT3D::Interpolation::guest_frames.load();
            if(swi==OOT3D::DialogueCamera::FirstSVC)
                oot_dialogue_camera.Projection(guest,regs,parent.jit->ExtRegs(),frame,
                    parent.system.CoreTiming().GetGlobalTimeUs().count(),&oot_orbit);
            else if(swi==OOT3D::DialogueCamera::FirstSVC+1)
                oot_dialogue_camera.LookAt(guest,regs,frame);
            else if(swi==OOT3D::DialogueCamera::FirstSVC+2)
                oot_dialogue_camera.NativeEntry(guest,regs,frame);
            else if(swi==OOT3D::DialogueCamera::FirstSVC+3)
                oot_dialogue_camera.NativeParameters(guest,regs,parent.jit->ExtRegs());
            else if(swi==OOT3D::DialogueCamera::FirstSVC+4)
                oot_dialogue_camera.NativeLens(guest,regs,parent.jit->ExtRegs());
            else if(swi==OOT3D::DialogueCamera::FirstSVC+5)
                oot_dialogue_camera.NativeSide(guest,regs,parent.jit->ExtRegs());
            else if(swi==OOT3D::DialogueCamera::FirstSVC+6)
                oot_dialogue_camera.NativeRadius(guest,regs,parent.jit->ExtRegs());
            else oot_dialogue_camera.FixedIndoorZoom(guest,regs,parent.jit->ExtRegs());
            using Trace=OOT3D::DialogueCamera::Trace;
            using namespace OOT3D::CameraTransitionDiagnostics;
            if(swi==OOT3D::DialogueCamera::FirstSVC && regs[15]==OOT3D::DialogueCamera::Sites[0].pc+4)
                TraceOotTransition(Projection,regs[4]-0x188+0x364,
                    std::bit_cast<float>(parent.jit->ExtRegs()[16]),regs[4]+0x30,regs[4]+0x3c);
            else if(swi==OOT3D::DialogueCamera::FirstSVC+1)
                TraceOotTransition(LookAt,regs[4]-0x188+0x364,0,regs[5],regs[7]);
            if(swi==OOT3D::DialogueCamera::FirstSVC && regs[15]==OOT3D::DialogueCamera::Sites[0].pc+4)
                TraceOotDialogue(Trace::Projection,regs[4]-0x188+0x364,
                    std::bit_cast<float>(parent.jit->ExtRegs()[16]),regs[4]+0x30,regs[4]+0x3c);
            else if(swi==OOT3D::DialogueCamera::FirstSVC+1)
                TraceOotDialogue(Trace::LookAt,regs[4]-0x188+0x364,0,regs[5],regs[7]);
            else if(swi==OOT3D::DialogueCamera::FirstSVC+2)
                TraceOotDialogue(Trace::Entry,regs[6]);
            return;
        }
        if (swi >= OOT3D::Camera::FirstSVC &&
            swi < OOT3D::Camera::FirstSVC + OOT3D::Camera::Sites.size()) {
            struct GuestMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
            } guest{memory};
            const u32 view=parent.jit->Regs()[4];
            const bool gameplay_lens=OOT3D::OrbitCamera::GameplayLens(guest,view) ||
                oot_orbit.DoorStartLens(guest,view) || oot_orbit.SaveQuitLens(guest,view);
            const float lens=gameplay_lens?oot_orbit.GameplayFov(guest,view):
                std::bit_cast<float>(parent.jit->ExtRegs()[16]);
            oot_orbit.RememberSaveView(guest,view,lens);
            OOT3D::Camera::Handle(guest,view,parent.jit->ExtRegs(),
                                  OOT3D::Camera::ConfiguredFov(),gameplay_lens,lens);
            const auto& fp=parent.jit->ExtRegs();
            const float radians=std::bit_cast<float>(fp[23]);
            TraceOotTransition(OOT3D::CameraTransitionDiagnostics::Lens,parent.jit->Regs()[4]-0x188+0x364,
                radians!=0?std::bit_cast<float>(fp[0])/radians:0,
                parent.jit->Regs()[4]+0x30,parent.jit->Regs()[4]+0x3c);
            TraceOotDialogue(OOT3D::DialogueCamera::Trace::Lens,parent.jit->Regs()[4]-0x188+0x364,
                radians!=0?std::bit_cast<float>(fp[0])/radians:0);
            return;
        }
        if (swi >= OOT3D::ScreenModels::FirstSVC &&
            swi < OOT3D::ScreenModels::FirstSVC + OOT3D::ScreenModels::Sites.size()) {
            OotMappedMemory guest{memory,parent.system.Kernel().GetCurrentProcess().get()};
            oot_playback_note_filter.Restore(guest);
            oot_interpolation.Apply(guest,parent.jit->Regs()[1],parent.system.CoreTiming().GetGlobalTimeUs().count(),
                Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                !Settings::values.upright_screen.GetValue());
            if(oot_description.indicator.Transform(guest,parent.jit->Regs(),
                    parent.system.CoreTiming().GetGlobalTimeUs().count(),
                    Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue(),&oot_description.close) ||
               oot_description.choice.Transform(guest,parent.jit->Regs(),
                    parent.system.CoreTiming().GetGlobalTimeUs().count(),
                    Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue(),&oot_description.close)) {
                OOT3D::TextboxInterpolation::Capture(guest,parent.jit->Regs()[0],parent.jit->Regs()[1]);
                return;
            }
            if(oot_description.close.Transform(guest,parent.jit->Regs(),
                    parent.system.CoreTiming().GetGlobalTimeUs().count())) {
                OOT3D::TextboxInterpolation::Capture(guest,parent.jit->Regs()[0],parent.jit->Regs()[1]);
                return;
            }
            if(OOT3D::Ocarina::HidePlaybackNote(guest,parent.jit->Regs()[0],
                Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                !Settings::values.upright_screen.GetValue())) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],{0,0,0,0,0,0,true});
                return;
            }
            if(oot_ocarina.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                oot_ocarina.CaptureSlide(guest,regs[0],regs[1]);
                if(OOT3D::OcarinaSlide::DiagnosticActive() && oot_ocarina.atlas.Drawing() && OOT3D::Interpolation::Replaying() &&
                   oot_ocarina.slide.progress>0 && oot_ocarina.slide.progress<1 &&
                   OOT3D::OcarinaSlide::capture_logs.fetch_add(1)<200) {
                    using namespace OOT3D;
                    LOG_INFO(Core, "OoT song slide capture: frame={} time={} progress={} offset={} midpoint={} attempts={} published={} desc={:08x} vertices={} indices={}",
                        Interpolation::guest_frames.load(),parent.system.CoreTiming().GetGlobalTimeUs().count(),
                        oot_ocarina.slide.progress,oot_ocarina.offset,oot_ocarina.midpoint_shift,
                        OcarinaSlide::capture_attempts.load(),OcarinaSlide::capture_published.load(),
                        OcarinaSlide::capture_desc.load(),OcarinaSlide::capture_vertices.load(),OcarinaSlide::capture_indices.load());
                }
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],oot_ocarina.ModelTransform(guest,regs[0]));
                return;
            }
            if(oot_boss_challenge.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                oot_boss_challenge.PromptModel(guest,regs[0]);
                oot_boss_challenge.SelectionModel(guest,regs[0]);
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],oot_boss_challenge.DrawTransform(guest,regs[0]));
                oot_boss_challenge.ObservePreview(guest,regs[0]);
                oot_boss_challenge.FilterTitleAtDraw(guest,regs[0]);
                return;
            }
            if(oot_save_loading.Drawing() || oot_death_loading.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                const bool bottom=oot_save_loading.Drawing() && oot_save_loading.bottom_projection;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],{0,0,bottom?.8f:1.f,1,0,0,true});
                return;
            }
            if(const auto fade=oot_death_menu.NativeFadeTransform(guest,parent.jit->Regs()[0])) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],*fade);
                return;
            }
            if(oot_death_menu.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],oot_death_menu.ModelTransform(guest,regs[0]));
                return;
            }
            if(oot_file_select.Drawing() || oot_file_select.PromptDrawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                if(oot_file_select.NativeDrawing())
                    oot_native_prompts.Draw(guest,memory,regs[0],
                        parent.system.CoreTiming().GetGlobalTimeUs().count()/16667);
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                    oot_file_select.ModelTransform(guest,regs[0]));
                return;
            }
            if(oot_pause_prompts.Drawing() || oot_death_prompts.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                const float sx=oot_pause_prompts.Drawing() && oot_pause_prompts.bottom_projection?.8f:1.f;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],{0,0,sx,1,0,0,true});
                return;
            }

            if(oot_dungeon_sides.Drawing() || oot_world_map.Drawing() || oot_world_clouds.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                    {0,OOT3D::PauseMenu::OffsetY,.8f*OOT3D::PauseMenu::Scale,OOT3D::PauseMenu::Scale,160,120,true});
                return;
            }
            if(oot_dungeon_sides.blocks.Drawing() || oot_dungeon_sides.top_fade.Drawing() || oot_dungeon_sides.top_shadow.Drawing() || oot_dungeon_sides.floor_line.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],{0,0,.8f,1,0,0,true});
                return;
            }
            if(oot_map_particles.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],{0,0,.8f,1,0,0,true});
                return;
            }
            if(oot_tab_shadow.Drawing() || oot_arrow_shadow.Drawing() || oot_panel_shadow.Drawing() || oot_header_shadow.Drawing() || oot_map_shadow.Drawing() || oot_map_extension.Drawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                const float x_scale=(oot_tab_shadow.Drawing() || oot_arrow_shadow.Drawing() || oot_panel_shadow.Drawing() || oot_map_shadow.Drawing() || oot_map_extension.Drawing() || oot_pause.routed)?.8f:1.f;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],{0,0,x_scale,1,0,0,true});
                return;
            }
            if(oot_header.DungeonDrawing()) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                    {0,0,oot_header.bottom_projection?.8f:1.f,1,0,0,true});
                return;
            }
            if(oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Draw ||
               oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Banner ||
               oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Foreground) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                const float x_scale=oot_header.bottom_projection?.8f:1.f;
                if(oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Draw) {
                    OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],{0,0,x_scale,1,0,0,true});
                } else if(oot_header.stage==OOT3D::PauseHeader::Hook::Stage::Foreground) {
                    if(oot_header.DungeonIcons(guest,regs[0])) {
                        OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                            {200*x_scale-160,OOT3D::PauseMenu::OffsetY,
                             x_scale*OOT3D::PauseMenu::Scale,OOT3D::PauseMenu::Scale,160,120,true});
                    } else if(oot_header.ApplyForegroundDescription(guest,regs[1],regs[0])) {
                        // The deferred message has its own projection correction and editor pose.
                    } else if(const auto part=OOT3D::MapLocation::Select(guest,regs[0]))
                        OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                            OOT3D::MapLocation::Transform(*part,OOT3D::PauseMenu::GetLayout(),
                                                        oot_header.bottom_projection));
                } else {
                    const auto p=OOT3D::PauseMenu::GetLayout()[OOT3D::PauseMenu::Titles];
                    if(OOT3D::PauseMenu::Valid(p))OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                        {p.x*x_scale+200*(x_scale-1),p.y+oot_header.offset,
                         p.scale*x_scale,p.scale,200,24,true});
                }
                return;
            }
            if(oot_pause.drawing) {
                auto& regs=parent.jit->Regs();
                regs[2]=regs[0]+0xc;
                // Bottom UI uses a 320-wide projection. Match top pixel scale,
                // leaving room above the panel for the native page title.
                auto pose=OOT3D::PauseMenu::Pose{};
                oot_map_edges.Observe(guest,regs[0]);
                oot_map_extension.Observe(guest,regs[0],[](std::uint64_t bits,unsigned x,unsigned y) {
                    const auto rgb=Pica::Texture::SampleETC1Subtile(bits,x,y);
                    return std::array{rgb.r(),rgb.g(),rgb.b()};
                });
                if(guest.Read(OOT3D::PauseMenu::UI+0x10)==std::optional<u32>{regs[0]}) {
                    pose=oot_pause.BackgroundPose(guest,OOT3D::PauseMenu::GetLayout());
                }
                OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                    {.8f*pose.x,OOT3D::PauseMenu::OffsetY+pose.y,
                     .8f*OOT3D::PauseMenu::Scale*pose.scale,OOT3D::PauseMenu::Scale*pose.scale,160,120,true});
                if(OOT3D::PauseMenu::DungeonContentModel(guest,regs[0])) {
                    const auto p=OOT3D::PauseMenu::GetLayout()[OOT3D::PauseMenu::DungeonContents];
                    if(OOT3D::PauseMenu::Valid(p))OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                        {(p.x+OOT3D::PauseMenu::motion_x.load())/OOT3D::PauseMenu::Scale,
                         p.y/OOT3D::PauseMenu::Scale,p.scale,p.scale,160,120,true});
                }
                if(OOT3D::SaveMenu::Active(guest))if(const auto part=OOT3D::SaveMenu::PromptPart(guest,regs[0])) {
                    const auto layout=OOT3D::SaveMenu::GetLayout();
                    if(OOT3D::SaveMenu::Valid(layout[*part]) && OOT3D::SaveMenu::Valid(layout[OOT3D::SaveMenu::Prompt])) {
                        const auto p=OOT3D::SaveMenu::ControlPose(*part,layout);
                        const auto anchor=OOT3D::SaveMenu::Anchors[*part];
                        OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                            {p.x/.8f,p.y/.8f,p.scale,p.scale,anchor[0],anchor[1],true});
                    }
                }
                return;
            }
            if(OOT3D::PauseMenu::Active(guest)) {
                auto& regs=parent.jit->Regs();regs[2]=regs[0]+0xc;
                const auto layout=OOT3D::PauseMenu::GetLayout();
                if(guest.Read(0x4fc648+0x28)==std::optional<u32>{regs[0]}) {
                    const auto p=layout[OOT3D::PauseMenu::Titles];
                    if(OOT3D::PauseMenu::Valid(p))OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],
                        {p.x,p.y,p.scale,p.scale,200,24,true});
                } else {
                    const auto p=layout[OOT3D::PauseMenu::DungeonPage(guest)?
                        OOT3D::PauseMenu::DungeonDescription:OOT3D::PauseMenu::Description];
                    const OOT3D::Textboxes::Layout description{
                        p.x,p.y,OOT3D::Textboxes::GetLayout().width,{},p.scale};
                    if(auto t=OOT3D::ScreenModels::Select(guest,regs[0],description,{}))
                        OOT3D::ScreenModels::Apply(guest,regs[1],regs[0],*t);
                }
                oot_description.close.Capture(guest,regs[0],regs[1],parent.system.CoreTiming().GetGlobalTimeUs().count());
                OOT3D::TextboxInterpolation::Capture(guest,regs[0],regs[1]);
                return;
            }
            OOT3D::ScreenModels::Handle(guest, parent.jit->Regs(),
                OOT3D::Textboxes::GetLayout(),
                OOT3D::MinimapControl::LayoutPose(guest,OOT3D::HUD::GetEffectiveLayout()));
            oot_description.close.Capture(guest,parent.jit->Regs()[0],parent.jit->Regs()[1],parent.system.CoreTiming().GetGlobalTimeUs().count());
            OOT3D::TextboxInterpolation::Capture(guest,parent.jit->Regs()[0],parent.jit->Regs()[1]);
            return;
        }
        if(swi>=OOT3D::ItemEquip::FirstSVC && swi<OOT3D::ItemEquip::FirstSVC+OOT3D::ItemEquip::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
            } guest{memory};
            OOT3D::ItemEquip::input.Handle(guest,parent.jit->Regs(),swi-OOT3D::ItemEquip::FirstSVC,
                parent.system.CoreTiming().GetGlobalTimeUs().count());
            return;
        }
        if(swi>=OOT3D::ItemUse::FirstSVC && swi<OOT3D::ItemUse::FirstSVC+OOT3D::ItemUse::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
            } guest{memory};
            OOT3D::ItemUse::input.Handle(guest,parent.jit->Regs(),swi-OOT3D::ItemUse::FirstSVC,
                parent.system.CoreTiming().GetGlobalTimeUs().count());
            return;
        }
        if(swi==OOT3D::Music::SVC) {
            struct SoundMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 word){memory.Write32(at,word);}} guest{memory};
            const auto& regs=parent.jit->Regs();
            OOT3D::Music::Store(guest,regs[4],regs[5],parent.jit->ExtRegs()[19]);return;
        }
        if(swi>=OOT3D::Rumble::FirstSVC && swi<OOT3D::Rumble::FirstSVC+OOT3D::Rumble::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 value){memory.Write32(a,value);}
            } guest{memory};
            OOT3D::Rumble::Handle(guest,parent.jit->Regs(),swi-OOT3D::Rumble::FirstSVC,parent.jit->ExtRegs()[0]);
            return;
        }
        if(swi>=OOT3D::Difficulty::FirstSVC && swi<OOT3D::Difficulty::FirstSVC+OOT3D::Difficulty::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
            } guest{memory};
            OOT3D::Difficulty::Handle(guest,parent.jit->Regs(),swi-OOT3D::Difficulty::FirstSVC);
            return;
        }
        if(swi>=OOT3D::Prices::FirstSVC && swi<OOT3D::Prices::FirstSVC+OOT3D::Prices::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
            } guest{memory};
            OOT3D::Prices::Handle(guest,parent.jit->Regs(),swi-OOT3D::Prices::FirstSVC);
            return;
        }
        if(swi>=OOT3D::Shops::FirstSVC && swi<OOT3D::Shops::FirstSVC+OOT3D::Shops::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
            } guest{memory};
            u32 flags=parent.jit->Cpsr(),fpscr=parent.jit->Fpscr();
            oot_shop_description.Handle(guest,parent.jit->Regs(),parent.jit->ExtRegs(),flags,fpscr,
                swi-OOT3D::Shops::FirstSVC);
            parent.jit->SetCpsr(flags);parent.jit->SetFpscr(fpscr);
            return;
        }
        if(swi>=OOT3D::ActorDrawDistance::FirstSVC &&
           swi<OOT3D::ActorDrawDistance::FirstSVC+OOT3D::ActorDrawDistance::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
            } guest{memory};
            OOT3D::ActorDrawDistance::Handle(guest,parent.jit->Regs(),parent.jit->ExtRegs(),
                swi-OOT3D::ActorDrawDistance::FirstSVC);
            return;
        }
        if(swi>=OOT3D::SunReadbackProbe::FirstSVC && swi<OOT3D::SunReadbackProbe::FirstSVC+2) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
                u32 ReadDepth(u32 a){return memory.Read32(a);}
            } guest{memory};
            oot_sun_probe.Handle(guest,parent.jit->Regs(),swi-OOT3D::SunReadbackProbe::FirstSVC,
                OOT3D::SunReadbackProbe::Now(),OOT3D::SunVisibility::enabled.load(),
                OOT3D::Interpolation::guest_frames.load(),OOT3D::Interpolation::generation.load());
            return;
        }
        if(swi==OOT3D::ArrowCycle::SVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
            } guest{memory};
            u32 flags=parent.jit->Cpsr(),fpflags=parent.jit->Fpscr();
            oot_arrow_cycle.Handle(guest,parent.jit->Regs(),parent.jit->ExtRegs(),flags,fpflags,
                parent.system.CoreTiming().GetGlobalTimeUs().count());
            parent.jit->SetCpsr(flags);parent.jit->SetFpscr(fpflags);return;
        }
        if(swi>=OOT3D::QOL::FirstSVC && swi<OOT3D::QOL::FirstSVC+OOT3D::QOL::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
            } guest{memory};
            OOT3D::QOL::Handle(guest,parent.jit->Regs(),parent.jit->ExtRegs()[0],
                swi-OOT3D::QOL::FirstSVC,parent.system.CoreTiming().GetGlobalTimeUs().count());
            return;
        }
        if(swi>=OOT3D::Interpolation::Pose::FishingFoliage::FirstSVC &&
           swi<OOT3D::Interpolation::Pose::FishingFoliage::FirstSVC+OOT3D::Interpolation::Pose::FishingFoliage::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
            } guest{memory};
            OOT3D::Interpolation::Pose::FishingFoliage::tags.Handle(guest,parent.jit->Regs(),
                swi-OOT3D::Interpolation::Pose::FishingFoliage::FirstSVC,
                parent.system.CoreTiming().GetGlobalTimeUs().count());
            return;
        }
        if(swi==OOT3D::Interpolation::Pose::SVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
            } guest{memory};
            OOT3D::Interpolation::Pose::Capture(guest,parent.jit->Regs(),parent.system.CoreTiming().GetGlobalTimeUs().count());
            return;
        }
        if(swi==OOT3D::Interpolation::FirstSVC || swi==OOT3D::Interpolation::RefreshSVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 a){return memory.Read32OrNullopt(a);}
                void Write(u32 a,u32 v){memory.Write32(a,v);}
            } guest{memory};
            const bool supported=Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                !Settings::values.upright_screen.GetValue();
            oot_interpolation.Capture(guest,parent.jit->Regs(),parent.system.CoreTiming().GetGlobalTimeUs().count(),supported);
            if(oot_aim_trace.Take(OOT3D::Interpolation::guest_frames.load(),4)) {
                using namespace OOT3D::Interpolation;
                const auto sample=CameraSnapshot();
                const auto play=guest.Read(OOT3D::PauseMenu::UI+0xc).value_or(0);
                const auto cam=play+0x364,player=guest.Read(cam+0xd8).value_or(0);
                LOG_INFO(Core_ARM11,"OoT aim capture: session={} frame={} us={} stage={} view={:08x} play={:08x} mode={} setting={} action={} flags={:08x}/{:08x} capture={} history={} valid={} age={} scene={} lens={} attached={} view_matrix=[{}] attached_matrix=[{}] projection=[{}]",
                    FirstPersonDiagnostics::capture.load(),guest_frames.load(),parent.system.CoreTiming().GetGlobalTimeUs().count(),
                    swi==RefreshSVC?"view-refresh":"full-view",
                    parent.jit->Regs()[4],play,OOT3D::OrbitCamera::Half(guest,cam+0x18c),
                    OOT3D::OrbitCamera::Half(guest,cam+0x18a),OOT3D::DPad::Byte(guest,player+0x1749),
                    guest.Read(player+0x1710).value_or(0),guest.Read(player+0x1714).value_or(0),
                    CaptureResultNames[unsigned(capture_result.load())],unsigned(status.load()),sample.valid,
                    parent.system.CoreTiming().GetGlobalTimeUs().count()-sample.time,sample.scene,
                    sample.projections_valid,sample.attached_valid,FirstPersonDiagnostics::MatrixText(sample.view),
                    FirstPersonDiagnostics::MatrixText(sample.attached_view),FirstPersonDiagnostics::MatrixText(sample.projections[0]));
            }
            return;
        }
        if(swi>=OOT3D::DPad::FirstSVC &&
           swi<=OOT3D::DPad::AcceptedSVC) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) {return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value) {memory.Write32(at,value);}
            } guest{memory};
            const auto now=parent.system.CoreTiming().GetGlobalTimeUs().count();
            if(swi==OOT3D::DPad::AcceptedSVC)
                OOT3D::DPad::AcceptAction(guest,parent.jit->Regs());
            else if(swi==OOT3D::DPad::UpdateSVC)
                OOT3D::DPad::BeginUpdate(guest,parent.jit->Regs());
            else if(swi==OOT3D::DPad::FirstSVC)
                oot_boots.Handle(guest,parent.jit->Regs(),now);
            else
                OOT3D::DPad::HandleAction(guest,parent.jit->Regs(),
                    swi-OOT3D::DPad::FirstSVC-1,now);
            return;
        }
        if (swi >= OOT3D::HUD::FirstSVC &&
            swi < OOT3D::HUD::FirstSVC + OOT3D::HUD::Sites.size()) {
            OotMappedMemory guest{memory,parent.system.Kernel().GetCurrentProcess().get()};
            auto& regs=parent.jit->Regs();
            const auto site=swi-OOT3D::HUD::FirstSVC;
            const bool minigame=OOT3D::HUD::MinigameActive(guest);
            if(site==4 && oot_hud_buttons.Pending()) {oot_hud_buttons.Resume(guest,regs);return;}
            if(site==4)for(auto& mesh:oot_dpad)if(mesh.Pending()) {
                mesh.Resume(guest,regs);
                if(!mesh.Pending())oot_hud_buttons.Begin(guest,regs,OOT3D::HUD::Sites[4].pc,oot_hud_routing.fade.opacity);
                return;
            }
            const bool draw_dpad=site==4 && regs[0]==OOT3D::HUDFade::Queue && regs[1]==5;
            OOT3D::HUD::Handle(guest, regs,
                site, OOT3D::HUD::GetEffectiveLayout(), oot_hud_routing,
                OOT3D::PauseMenu::Active(guest),
                parent.system.CoreTiming().GetGlobalTimeUs().count());
            if(site==8) {
                if(OOT3D::HUD::RoutingCapture::Active() &&
                   guest.Read(regs[4]+0x354)==guest.Read(OOT3D::HUD::Main+0xc)) {
                    if(const auto mesh=OOT3D::HUD::Mesh(guest,guest.Read(OOT3D::HUD::Main+4).value_or(0));
                       mesh && OOT3D::HUD::RoutingCapture::Take())
                        LOG_INFO(Core_ARM11,"OoT HUD routing capture material: frame={} top_model={:08x} mesh={:016x} geometry={:016x} xyz={:016x} uv={:016x} rgba={:016x}",
                            OOT3D::Interpolation::guest_frames.load(),oot_hud_routing.drawing_top_model,
                            mesh->hash,mesh->geometry_hash,mesh->attributes[0],mesh->attributes[1],mesh->attributes[2]);
                }
                OOT3D::PauseMenu::DungeonRoomMaterial(guest,guest.Read(regs[4]+0x354).value_or(0),regs[13]+0x2c);
                OOT3D::MapLocation::Material(guest,guest.Read(regs[4]+0x354).value_or(0),regs[13]+0x38);
                oot_description.indicator.Material(guest,guest.Read(regs[4]+0x354).value_or(0),regs[13]+0x38,
                    parent.system.CoreTiming().GetGlobalTimeUs().count(),
                    Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue());
                oot_description.choice.Material(guest,guest.Read(regs[4]+0x354).value_or(0),regs[13]+0x38,
                    parent.system.CoreTiming().GetGlobalTimeUs().count(),
                    Settings::values.render_3d.GetValue()==Settings::StereoRenderOption::Off &&
                    !Settings::values.upright_screen.GetValue());
                if(OOT3D::PauseHeader::DiagnosticActive() &&
                   OOT3D::MapLocation::Select(guest,guest.Read(regs[4]+0x354).value_or(0))==OOT3D::PauseMenu::LocationBanner &&
                   OOT3D::PauseHeader::DiagnosticLine())
                    LOG_INFO(Core_ARM11, "OoT map banner material: us={} model={:08x} alpha={}",
                        parent.system.CoreTiming().GetGlobalTimeUs().count(),guest.Read(regs[4]+0x354).value_or(0),
                        std::bit_cast<float>(guest.Read(regs[13]+0x38).value_or(0)));
                oot_file_select.Material(guest,regs[13]+0x38,guest.Read(regs[4]+0x354).value_or(0));
                oot_ocarina.Material(guest,regs[13]+0x38);
                oot_playback_note_filter.Material(guest,regs[4]);
                oot_boss_challenge.PreviewMaterial(guest,regs[13]+0x2c);
                oot_boss_challenge.DrawMaterial(guest,regs[4],regs[13]+0x38);
                oot_boss_challenge.SelectionAlpha(guest,guest.Read(regs[4]+0x354).value_or(0),regs[13]+0x38);
                oot_death_menu.Material(guest,regs[13]+0x38,guest.Read(regs[4]+0x354).value_or(0));
                if(oot_hud_buttons.Drawing())guest.Write(regs[13]+0x38,std::bit_cast<u32>(1.f));
            }
            if(site==8)for(auto& mesh:oot_dpad) {
                // The D-pad carries the shared HUD fade in its vertex colors.
                // Its borrowed native material must not multiply the A fade again.
                if(mesh.Drawing() && guest.Read(regs[4]+0x354)==std::optional<u32>{mesh.model})
                    guest.Write(regs[13]+0x38,std::bit_cast<u32>(1.f));
            }
            if(draw_dpad && !minigame && OOT3D::DPad::CodeMatches(guest)) {
                const auto variant=OOT3D::DPad::TextureVariant(guest);
                if(auto image=OOT3D::DPad::GetTexture(variant)) {
                    const auto geometry=OOT3D::DPad::MakeGeometry(guest,OOT3D::DPad::GetEffectiveLayout(),
                        oot_hud_routing.fade.opacity,parent.system.CoreTiming().GetGlobalTimeUs().count(),
                        &oot_navi_display,OOT3D::Interpolation::generation.load());
                    oot_dpad[variant].Begin(guest,regs,OOT3D::HUD::Sites[4].pc,geometry,*image,
                        guest.Read(OOT3D::HUD::Action+0x20).value_or(0));
                }
            }
            if(draw_dpad && !minigame && std::none_of(oot_dpad.begin(),oot_dpad.end(),
                [](const auto& mesh){return mesh.Pending();}))
                oot_hud_buttons.Begin(guest,regs,OOT3D::HUD::Sites[4].pc,oot_hud_routing.fade.opacity);
            return;
        }
        if (swi >= OOT3D::TitleScreen::FirstSVC &&
            swi < OOT3D::TitleScreen::FirstSVC + OOT3D::TitleScreen::Sites.size()) {
            struct GuestMemory { Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
                void Write(u32 at, u32 value) { memory.Write32(at, value); }
            } guest{memory};
            auto& regs=parent.jit->Regs();
            const unsigned site=swi-OOT3D::TitleScreen::FirstSVC;
            if(site==4) {OOT3D::RoomJoin::ReturnToTitle();OOT3D::RemoteNames::Reset();}
            const auto layout=OOT3D::TitleScreen::GetLayout();
            if(site==3)oot_title_prompt.Observe(guest,regs,layout);
            if(site==7 && oot_title_prompt.Return(guest,regs))return;
            OOT3D::TitleScreen::Handle(guest,regs,site,layout,oot_title_fade);
            return;
        }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
        if (swi == MM3D::VisibilityDepth::FirstSVC || swi == MM3D::VisibilityDepth::FirstSVC + 1) {
            auto& regs = parent.jit->Regs();
            const bool enabled = MM3D::CameraMidpoint::enabled.load();
            const auto generation = MM3D::CameraMidpoint::generation.load();
            if (swi == MM3D::VisibilityDepth::FirstSVC) {
                struct CodeMemory { Memory::MemorySystem& memory;
                    std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
                } code{memory};
                visibility_depth_hook.Begin(code, enabled, MM3D::guest_frames.load(), generation);
                regs[0] = 0; // displaced MOV r0,#0; flags unchanged
            } else {
                const auto cached = visibility_depth_hook.Read(regs[2], regs[0], enabled, generation);
                if (cached) regs[0] = *cached;
                else {
                    const u32 address = regs[2] + (regs[0] << 2);
                    MM3D::InterpolationPerformance::GuestReadScope origin{
                        address, MM3D::VisibilityDepth::LoadPC, regs[14]};
                    regs[0] = memory.Read32(address); // exact displaced LDR on every fallback
                }
            }
            return;
        }
        if(swi==MM3D::ScreenShrink::GuardSVC) {
            auto& regs=parent.jit->Regs();
            regs[15]=MM3D::ScreenShrink::Guard(regs[5],regs[0],parent.jit->Cpsr(),regs[3]);
            return;
        }
        if(swi==MM3D::ScreenShrink::SVC) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}
            } guest{memory};
            MM3D::ScreenShrink::Store(guest,parent.jit->Regs()[0],parent.jit->ExtRegs()[0],
                MM3D::Widescreen::supported_layout.load());
            return;
        }

        if(swi>=MM3D::ClassicSaving::FirstSVC&&swi<MM3D::ClassicSaving::FirstSVC+MM3D::ClassicSaving::Sites.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            struct Service {
                bool Begin(unsigned slot,MM3D::ClassicSaving::Kind kind){return MM3D::ClassicSaving::BeginWrite(slot,kind);}
                bool Finish(unsigned slot,MM3D::ClassicSaving::Kind kind){return MM3D::ClassicSaving::FinishWrite(slot,kind);}
                void Load(unsigned slot,std::span<const std::uint8_t> bytes){MM3D::ClassicSaving::Resume(slot,bytes);}
            } service;
            const bool trace_owl=swi==MM3D::ClassicSaving::FirstSVC+15&&
                MM3D::OwlSword::QuietCompletion(guest,parent.jit->Regs()[4]);
            if(trace_owl) {
                owl_trace_actor=parent.jit->Regs()[4];owl_trace_play=parent.jit->Regs()[6];owl_trace_ticks=0;
                TraceOwlState("completion-before-release");
            }
            const bool trace_return=swi==MM3D::ClassicSaving::FirstSVC+17&&MM3D::ClassicSaving::ResumeNoticeArmed();
            const bool trace_return_finish=swi==MM3D::ClassicSaving::FirstSVC+16&&classic_save_hook.returning.player&&parent.jit->Regs()[0]==2;
            const bool trace_finish=swi==MM3D::ClassicSaving::FirstSVC+16&&classic_save_hook.quiet_player!=0;
            const u32 native_message_state=trace_finish?parent.jit->Regs()[0]:0;
            if(swi==MM3D::ClassicSaving::FirstSVC+19&&
               (MM3D::ClassicSaving::ResumeNoticeArmed()||classic_save_hook.returning.player)) {
                const auto& r=parent.jit->Regs();
                const u32 height=(guest.Read(r[0]).value_or(0)>>8)&255;
                const bool hidden=classic_save_hook.returning.HideFocusBars(guest);
                const u32 state=height|(hidden?0x100:0)|(MM3D::ClassicSaving::ResumeNoticeArmed()?0x200:0);
                static u32 last_state=~0u;
                if(state!=last_state) {
                    last_state=state;
                    LOG_INFO(Core_ARM11,"Classic owl return bars: height={} hidden={} pending={}",
                             height,hidden,MM3D::ClassicSaving::ResumeNoticeArmed());
                }
            }
            classic_save_hook.Handle(guest,parent.jit->Regs(),swi-MM3D::ClassicSaving::FirstSVC,service);
            if(trace_return&&!MM3D::ClassicSaving::ResumeNoticeArmed())
                LOG_INFO(Core_ARM11,"Classic owl return: {}",classic_save_hook.returning.player?"native conversation started before idle input":"unsupported message resource; notice skipped");
            if(trace_return_finish)LOG_INFO(Core_ARM11,"Classic owl return: native conversation finished; movement released");
            if(trace_finish)LOG_INFO(Core_ARM11,"Owl activation trace: conversation state native={} effective={}",native_message_state,parent.jit->Regs()[0]);
            if(trace_owl) {
                const auto& r=parent.jit->Regs();
                LOG_INFO(Core_ARM11,"Owl activation trace: quiet completion r0={:08x} r1={:08x} r2={} lr={:08x} pc={:08x}",r[0],r[1],r[2],r[14],r[15]);
            }
            if(MM3D::ClassicSaving::enabled && swi<=MM3D::ClassicSaving::FirstSVC+1) {
                const auto& regs=parent.jit->Regs();
                const u32 ctx=regs[swi==MM3D::ClassicSaving::FirstSVC?0:4];
                const u32 status=guest.Read(ctx+0x180).value_or(~0u);
                static u32 last_ctx=0,last_status=~0u;
                if(ctx!=last_ctx||status!=last_status) {
                    last_ctx=ctx;last_status=status;
                    LOG_INFO(Core_ARM11,"Classic cycle trace: ctx={:08x} state={:08x} status={} window={:08x} captured={:08x}",
                        ctx,guest.Read(ctx+0x13c).value_or(0),status,
                        guest.Read(ctx+0x174).value_or(0),classic_save_hook.cycle);
                }
            }
            return;
        }
        if(swi==MM3D::MinimapShortcut::SVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            MM3D::MinimapShortcut::Handle(guest,parent.jit->Regs());
            return;
        }
        if(swi>=MM3D::BottleHealing::FirstSVC&&swi<MM3D::BottleHealing::FirstSVC+3) {
            struct GuestMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            MM3D::BottleHealing::Apply(guest,parent.jit->Regs(),swi-MM3D::BottleHealing::FirstSVC,
                MM3D::BottleHealing::red_hearts.load(),MM3D::BottleHealing::blue_hearts.load(),
                MM3D::BottleHealing::fairy_hearts.load());
            return;
        }
        if(swi==MM3D::DeathPenalties::FirstSVC||swi==MM3D::DeathPenalties::FirstSVC+1||
           swi==MM3D::DeathPenalties::SceneSoundSVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            if(swi==MM3D::DeathPenalties::FirstSVC)
                MM3D::DeathPenalties::Death(guest,parent.jit->Regs(),MM3D::DeathPenalties::lose_rupees.load());
            else if(swi==MM3D::DeathPenalties::FirstSVC+1)
                MM3D::DeathPenalties::Continue(guest,parent.jit->Regs(),MM3D::DeathPenalties::world_ends.load(),
                    MM3D::DeathPenalties::advance_six_hours.load(),&death_respawn_sound);
            else
                death_respawn_sound.SceneInit(guest,parent.jit->Regs());
            return;
        }
        if(swi>=MM3D::FastTime::FirstSVC&&swi<MM3D::FastTime::FirstSVC+MM3D::FastTime::Hooks.size()) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            MM3D::FastTime::Update(guest,parent.jit->Regs(),swi,MM3D::FastTime::enabled.load());
            return;
        }
        if(swi==MM3D::Difficulty::SpawnSVC) {
            struct GuestMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            MM3D::Difficulty::Spawn(guest,parent.jit->Regs(),MM3D::Difficulty::disable_heart_drops.load(),
                                    MM3D::Difficulty::disable_rupee_drops.load(),
                                    MM3D::Difficulty::disable_fairy_drops.load(),
                                    MM3D::Difficulty::disable_magic_drops.load(),
                                    MM3D::Difficulty::disable_ammo_pickups.load());
            return;
        }
        if(swi>=MM3D::VegetationDrawDistance::FirstSVC&&swi<MM3D::VegetationDrawDistance::FirstSVC+MM3D::VegetationDrawDistance::Hooks.size()) {
            struct GuestMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            MM3D::VegetationDrawDistance::Load(guest,swi-MM3D::VegetationDrawDistance::FirstSVC,
                parent.jit->Regs(),parent.jit->ExtRegs(),MM3D::PropDrawDistance::multiplier.load());
            return;
        }
        if(swi>=MM3D::PropDrawDistance::FirstSVC&&swi<MM3D::PropDrawDistance::FirstSVC+MM3D::PropDrawDistance::Hooks.size()) {
            MM3D::PropDrawDistance::Load(swi-MM3D::PropDrawDistance::FirstSVC,parent.jit->Regs(),
                parent.jit->ExtRegs(),MM3D::PropDrawDistance::multiplier.load());
            return;
        }
        if(swi==MM3D::ActorDrawDistance::SVC) {
            struct GuestMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            auto& ext=parent.jit->ExtRegs();
            ext[0]=MM3D::ActorDrawDistance::Apply(guest,parent.jit->Regs()[4],ext[16],
                                               MM3D::ActorDrawDistance::multiplier.load());
            ext[0]=MM3D::PropDrawDistance::Apply(guest,parent.jit->Regs()[4],ext[0],
                                              MM3D::PropDrawDistance::multiplier.load());
            return;
        }
        if(swi==MM3D::Difficulty::DefenseSVC) {
            auto& regs=parent.jit->Regs();
            regs[4]=MM3D::Difficulty::DefenseDelta(regs[4],regs[1],parent.jit->Cpsr(),
                MM3D::Difficulty::weaker_enhanced_defense.load());
            return; // MOV without S: preserve CPSR and all other registers.
        }
        if(swi==MM3D::Difficulty::SVC) {
            auto& regs=parent.jit->Regs();
            regs[2]=MM3D::Difficulty::ApplyDamage(regs[2],regs[4],MM3D::Difficulty::damage_multiplier.load());
            return; // displaced ADD does not alter CPSR or the other registers
        }
        if(swi==MM3D::NativeStartup::FirstSVC||swi==MM3D::NativeStartup::FirstSVC+1) {
            struct TitleMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& regs=parent.jit->Regs();
            if(swi==MM3D::NativeStartup::FirstSVC) {
                startup_hook.Enter(guest,regs[0]);
                link_pose_hook.Enter(guest,regs[0]);regs[5]=regs[0];
            } else {
                startup_hook.Matrix(guest,regs[5],regs[4]);
                link_pose_hook.MatrixBuilt(guest,regs[5],regs[4],MM3D::guest_frames.load());
                regs[0]=memory.Read32(regs[5]+0xc);
            }
            return;
        }
        if(swi>=MM3D::ItemPreview::FirstSVC && swi<MM3D::ItemPreview::FirstSVC+MM3D::ItemPreview::PCs.size()) {
            struct PreviewMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
                void Write(u32 at,u32 value) { memory.Write32(at,value); }
            } guest{memory};
            auto& regs=parent.jit->Regs();
            switch(swi-MM3D::ItemPreview::FirstSVC) {
            case 0:
                item_preview_hook.Enter(guest,regs[0],regs[1]);
                // Shared Items/Masks entry runs after the complete icon array is built.
                inventory_hook.Capture(guest,regs[0]);
                regs[4]=regs[0]; break;
            case 1: item_preview_hook.Leave(regs[0]); regs[4]=regs[0]; break;
            case 2: item_preview_hook.ModelFrame(regs[0]); dungeon_map_hook.ModelFrame(guest,regs[0]); regs[4]=regs[0]; break;
            case 3: item_preview_hook.ModelBegin(guest,regs[4],regs[6]); dungeon_map_hook.preview.ModelBegin(guest,regs[4],regs[6]); regs[1]=regs[6]; break;
            case 4: dungeon_map_hook.preview.ModelEnd(guest); item_preview_hook.ModelEnd(guest); break; // displaced NOP
            case 5:
                item_preview_hook.EnterGear(guest,regs[0],regs[1]);
                inventory_hook.CaptureGear(guest,regs[0]);
                regs[4]=regs[0]; break;
            case 6:
                // This shared Gear Close instruction is dispatched through
                // ItemPreview first; release the backdrop's owned pause bit too.
                menu_backdrop.Release(guest);
                item_preview_hook.Leave(regs[0]);
                if(inventory_hook.gear_page==regs[0]) inventory_hook.Capture(guest,0);
                regs[5]=regs[0]; break;
            }
            return;
        }
        if(swi==MM3D::ScheduleRows::FirstSVC||swi==MM3D::ScheduleRows::FirstSVC+1) {
            struct RowMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& regs=parent.jit->Regs();
            if(swi==MM3D::ScheduleRows::FirstSVC) {
                notebook_hook.schedule_rows.Enter(guest,regs[0]);regs[4]=regs[0];
            } else {
                regs[1]=0; // Native stop/reset path for Schedule vertical scrolling.
                regs[2]=0; // Displaced MOV r2,#0.
            }
            return;
        }
        if(swi==MM3D::NotebookRows::SyncSVC) {
            struct RowMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            auto& regs=parent.jit->Regs();
            if(notebook_hook.row_window.ImmediateScroll(guest,regs[0],regs[2],regs[3])) {
                regs[2]|=8; // Populate the changed window now.
                regs[3]=0; // Native reset path, without the deferred scroll animation.
            }
            regs[4]=regs[0];return;
        }
        if(swi==MM3D::NotebookRows::FirstSVC||swi==MM3D::NotebookRows::FirstSVC+1) {
            struct RowMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& regs=parent.jit->Regs();
            if(swi==MM3D::NotebookRows::FirstSVC){notebook_hook.row_window.Enter(guest,regs[0]);regs[4]=regs[0];}
            else {notebook_hook.row_window.Leave(guest);regs[0]=regs[4];}
            return;
        }
        if(swi==MM3D::NativeNotebook::MapInputSVC||swi==0xffffd3) {
            struct NotebookMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);} } guest{memory};
            const auto& regs=parent.jit->Regs();
            parent.jit->SetCpsr(swi==0xffffd3 ?
                MM3D::NativeNotebook::ScheduleInputFlags(guest,regs[4],regs[3],parent.jit->Cpsr()) :
                MM3D::NativeNotebook::MapInputFlags(guest,regs[4],regs[3],parent.jit->Cpsr()));
            return;
        }
        if(swi==MM3D::NativeNotebook::SVC) {
            struct NotebookMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);} } guest{memory};
            auto& regs=parent.jit->Regs();notebook_hook.Capture(guest,regs[0]);regs[4]=regs[0];return;
        }
        if(swi==MM3D::SongList::NavSVC){
            auto& r=parent.jit->Regs();
            if(MM3D::SongList::enabled&&song_hook.panel&&memory.Read32OrNullopt(r[4]+0x14)==std::optional<u32>(song_hook.panel)){
                auto context=memory.Read32OrNullopt(r[7]+4);
                auto input=context?memory.Read32OrNullopt(*context+0x16c):std::optional<u32>{};
                auto a=input?memory.Read32OrNullopt(*input+0x34):std::optional<u32>{};
                auto b=input?memory.Read32OrNullopt(*input+0x8c):std::optional<u32>{};
                auto flags=memory.Read32OrNullopt(r[5]+0x1f8),mask=memory.Read32OrNullopt(r[6]+0x1c);
                if(a&&b&&flags&&mask)r[2]=MM3D::SongList::Selection(r[1],*a|*b,(*flags&*mask)!=0);
            }
            const u32 a=r[1],b=r[2],d=a-b;
            const u32 flags=(d&0x80000000u)|(d==0?0x40000000u:0)|(a>=b?0x20000000u:0)|(((a^b)&(a^d)&0x80000000u)>>3);
            parent.jit->SetCpsr((parent.jit->Cpsr()&0x0fffffffu)|flags);return;
        }
        if(swi==MM3D::BomberApproach::FirstSVC||swi==MM3D::BomberApproach::FirstSVC+1) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            auto& r=parent.jit->Regs();
            const bool suppress=MM3D::BomberApproach::Suppress(guest,r[4]);
            if(swi==MM3D::BomberApproach::FirstSVC) {
                const auto original=static_cast<u32>(static_cast<s32>(static_cast<s16>(memory.Read16(r[5]+0x8c))));
                r[0]=MM3D::BomberApproach::State(original,suppress);
            } else if(suppress)r[15]=MM3D::BomberApproach::ResetPC;
            else r[1]=0xf;
            return;
        }
        if(swi==MM3D::OwlIntro::FirstSVC||swi==MM3D::OwlIntro::FirstSVC+1) {
            auto& r=parent.jit->Regs();
            if(swi==MM3D::OwlIntro::FirstSVC)MM3D::OwlIntro::Enter(r);
            else r[0]=MM3D::OwlIntro::TatlFlags(memory.Read8(r[0]+0x2d3));
            return;
        }
        if(swi==MM3D::OwlSword::FirstSVC||swi==MM3D::OwlSword::FirstSVC+1) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& r=parent.jit->Regs();
            if(swi==MM3D::OwlSword::FirstSVC) {
                r[4]=r[0];
                const auto before=guest.Read(r[4]+0x268);
                MM3D::OwlSword::BlockFeatherAcceptance(guest,r[4],MM3D::ClassicSaving::enabled.load());
                MM3D::OwlSword::Update(guest,r[4]);
                if(before!=guest.Read(r[4]+0x268)&&guest.Read(r[4]+0x268)==MM3D::OwlSword::Activate) {
                    owl_trace_actor=r[4];owl_trace_play=r[1];owl_trace_ticks=0;
                    TraceOwlState("sword-hit");
                }
                if(owl_trace_actor==r[4]&&owl_trace_play==r[1]&&owl_trace_ticks<180) {
                    ++owl_trace_ticks;
                    if(owl_trace_ticks<=3||owl_trace_ticks%30==0)TraceOwlState("actor-update");
                }
            }
            else if(MM3D::OwlSword::Locked(guest,r[0]) ||
                    MM3D::OwlSword::FeatherBlocked(guest,r[0],MM3D::ClassicSaving::enabled.load()))r[0]=0;
            else {r[14]=MM3D::OwlSword::OfferPC+4;r[15]=0x3c322c;}
            return;
        }
        if(swi==MM3D::TatlReminders::SVC) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& r=parent.jit->Regs();
            r[4]=MM3D::TatlReminders::Request(guest,r[0],r[1]);return;
        }
        if(swi==MM3D::Music::SVC) {
            struct SoundMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 word){memory.Write32(at,word);}} guest{memory};
            const auto& regs=parent.jit->Regs();
            MM3D::Music::Store(guest,regs[4],regs[5],parent.jit->ExtRegs()[16]);return;
        }
        if(swi==MM3D::MenuSound::SVC) {
            struct SoundMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& r=parent.jit->Regs();
            MM3D::MenuSound::Store(guest,menu_sound_context,r[4],r[0]);return;
        }
        if(swi>=MM3D::ClassicDoubleTime::FirstSVC&&swi<MM3D::ClassicDoubleTime::FirstSVC+MM3D::ClassicDoubleTime::Sites.size()) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            const auto site=swi-MM3D::ClassicDoubleTime::FirstSVC;
            const bool owned=classic_double_time_hook.target.has_value();
            auto& r=parent.jit->Regs();
            classic_double_time_hook.Handle(guest,r,site);
            if(site==0&&MM3D::ClassicDoubleTime::enabled.load())
                LOG_INFO(Core_ARM11,"Classic Double Time: dispatch PC={:08X}, question={}, message_data={:08X}",
                    r[15],classic_double_time_hook.target?int(classic_double_time_hook.target->question):-1,
                    classic_double_time_hook.message_data);
            else if(owned&&(site==2||site==3))
                LOG_INFO(Core_ARM11,"Classic Double Time: choice site={}, continuation={:08X}",site,r[15]);
            return;
        }
        if(swi>=MM3D::Prices::FirstSVC&&swi<MM3D::Prices::FirstSVC+MM3D::Prices::Sites.size()) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            MM3D::Prices::Execute(guest,parent.jit->Regs(),swi-MM3D::Prices::FirstSVC,MM3D::Prices::multiplier.load());
            return;
        }
        if(swi==MM3D::SingleScreenText::SVC) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& r=parent.jit->Regs();
            classic_double_time_hook.Message(guest,r[0],r[1]);
            MM3D::ClassicSaving::ObserveMessage(r[1]);
            MM3D::SingleScreenText::Apply(guest,r[0],r[1]);
            if(MM3D::Prices::CodeMatches(guest))MM3D::Prices::Message(guest,r[0],r[1],MM3D::Prices::multiplier.load());
            r[3]=memory.Read32(r[0]+4); // original LDR r3,[r0,#4]
            return;
        }
        if(swi>=MM3D::MenuBackdrop::FirstSVC&&swi<MM3D::MenuBackdrop::FirstSVC+MM3D::MenuBackdrop::PCs.size()) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& r=parent.jit->Regs();
            switch(swi-MM3D::MenuBackdrop::FirstSVC) {
            case 0:
                menu_backdrop.Pause(guest,r[7],r[4],r[1],r[6]);
                LOG_INFO(Core_ARM11,"MM3D Gear backdrop: full_pause={} captured_scene={}",
                    (guest.Read(r[7]+0xaac).value_or(0)&0xffff)==3,menu_backdrop.Active(guest));
                break;
            case 1: case 2: menu_backdrop.Pause(guest,r[5],r[4],r[1],r[0]);break;
            case 3: menu_backdrop.Release(guest);r[5]=r[0];break;
            case 4: menu_backdrop.CloseMap(guest,r,menu_page_switch.KeepCapture(guest,MM3D::MenuBackdrop::Hook::Game(guest,r[1])+0x11010));break;
            case 5: menu_backdrop.GearImageImport(guest,r);break;
            case 6: menu_backdrop.FinishGearPhoto(guest,r);break;
            }
            return;
        }
        if(swi>=MM3D::MenuMemory::FirstSVC&&swi<MM3D::MenuMemory::FirstSVC+MM3D::MenuMemory::PCs.size()) {
            struct Guest {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& r=parent.jit->Regs();
            if(swi==MM3D::MenuMemory::FirstSVC) {
                if(!menu_memory.Prepare(guest,r)&&
                   !(MM3D::MenuBackdrop::CodeMatches(guest)&&menu_backdrop.Prepare(guest,r))) {
                    r[15]=r[2]&~1u;
                    parent.jit->SetCpsr((parent.jit->Cpsr()&~0x20u)|((r[2]&1u)<<5));
                }
            } else if(swi==MM3D::MenuMemory::FirstSVC+1)menu_memory.Ready(guest,r[4],menu_backdrop.PreparedReady(guest,r[4],r[0]));
            else if(!(MM3D::MenuBackdrop::CodeMatches(guest)&&menu_backdrop.KeepGearCapture(guest,r[0],r[4],r[7])))
                menu_memory.GearOpenCapture(guest,r[0],r[6],r[4],r[7]);
            return;
        }
        if(swi>=MM3D::MenuPageSwitch::FirstSVC&&swi<MM3D::MenuPageSwitch::FirstSVC+MM3D::MenuPageSwitch::PCs.size()) {
            struct PageMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& r=parent.jit->Regs();
            switch(swi-MM3D::MenuPageSwitch::FirstSVC) {
            case 0:
                menu_sound_context=r[0];
                menu_backdrop.Tick(guest);
                if(MM3D::MenuMemory::CodeMatches(guest))menu_memory.Tick(guest,r[0]);
                menu_page_switch.Tick(guest,r[0],menu_backdrop.HasRetainedCapture()&&menu_backdrop.Active(guest));r[4]=r[0];break;
            case 1:
                r[14]=MM3D::MenuPageSwitch::PCs[1]+4;
                if(menu_page_switch.PairPending(guest,r[1]))r[0]=1;
                else {r[15]=r[2]&~1u;parent.jit->SetCpsr((parent.jit->Cpsr()&~0x20u)|((r[2]&1u)<<5));}
                break;
            case 2:
                if(menu_page_switch.FinishLayout(guest,r[0]))parent.jit->ExtRegs()[0]=std::bit_cast<u32>(10.f);
                r[4]=r[0];break;
            case 6: parent.jit->ExtRegs()[1]=MM3D::MenuBackdrop::CodeMatches(guest)&&menu_backdrop.Active(guest)?0:MM3D::MenuPageSwitch::GearBackdropAlpha();break;
            case 7: menu_page_switch.MapCloseSound(guest,r);break;
            default:
                if(swi==MM3D::MenuPageSwitch::FirstSVC+5&&
                   menu_backdrop.CloseGearPhoto(guest,r,menu_page_switch.KeepCapture(guest,r[0])))break;
                menu_page_switch.CaptureCall(guest,r,swi-MM3D::MenuPageSwitch::FirstSVC);break;
            }
            return;
        }
        if(swi>=MM3D::InventoryNavigation::FirstSVC&&swi<=MM3D::InventoryNavigation::FirstSVC+2) {
            struct NavMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& regs=parent.jit->Regs();const bool vertical=swi==MM3D::InventoryNavigation::FirstSVC;
            if(swi==MM3D::InventoryNavigation::FirstSVC+2) {
                regs[2]|=regs[12];
                if(auto next=MM3D::InventoryNavigation::GearMove(guest,regs[0],regs[2])) {
                    const auto current=memory.Read8(regs[0]+0x195);
                    regs[2]=*next;regs[15]=*next==current?0x5a56e0:0x5a574c;
                }
                return;
            }
            if(MM3D::InventoryNavigation::PresentMove(guest,regs[4],regs[13],vertical,regs[5])||
               MM3D::InventoryNavigation::Move(guest,regs[4],regs[13],vertical,regs[5]))
                regs[15]=vertical?0x5a0eb0:0x5a14c0;
            else regs[1]=memory.Read8(regs[4]+(vertical?0x19:0x18));
            return;
        }
        if(swi==MM3D::NativeItemPresent::IntroSVC) {
            struct ReadMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            auto& regs=parent.jit->Regs();
            regs[14]=MM3D::NativeItemPresent::IntroPC+4;
            if(MM3D::NativeItemPresent::SkipIntro(guest,regs[5])) {
                regs[0]=1;regs[15]=regs[14]; // Native caller closes the message and advances focus.
            } else regs[15]=0x1adb90; // Replay the original BL for unsupported/disabled cases.
            return;
        }
        if(swi>=MM3D::PauseCapture::FirstSVC&&swi<MM3D::PauseCapture::FirstSVC+3) {
            struct ReadMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            auto& regs=parent.jit->Regs();
            switch(swi-MM3D::PauseCapture::FirstSVC) {
            case 0: if(!menu_backdrop.HasRetainedCapture())pause_capture_hook.Convert(guest,regs[7],regs[8],regs[4],regs[5]);regs[0]=regs[7]+(regs[8]<<2);break;
            case 1: if(!menu_backdrop.HasRetainedCapture())pause_capture_hook.Bind(guest,regs[3],regs[2]);regs[12]=regs[0]+0x1c;break;
            case 2: if(!menu_backdrop.HasRetainedCapture())pause_capture_hook.Reset();regs[4]=regs[0];break;
            }
            return;
        }
        if(swi>=MM3D::NativeInventory::FirstSVC && swi<MM3D::NativeInventory::FirstSVC+4) {
            struct InventoryMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
                void Write(u32 at,u32 value) { memory.Write32(at,value); }
            } guest{memory};
            auto& regs=parent.jit->Regs();
            switch(swi-MM3D::NativeInventory::FirstSVC) {
            case 0: inventory_hook.Capture(guest,regs[0]); regs[4]=regs[0]; break;
            case 1: MM3D::ScreenShrink::Update(guest); if(MM3D::MenuBackdrop::CodeMatches(guest))menu_backdrop.Pass(guest,regs[0],regs[1]);
                options_hook.Pass(guest,regs[0],regs[1]); song_reset_hook.Pass(guest,regs[0],regs[1]); double_time_hook.Pass(guest,regs[0],regs[1]); dungeon_map_hook.Pass(guest,regs[0],regs[1]); song_hook.Pass(guest,regs[0],regs[1]); ocarina_hook.Pass(guest,regs[0],regs[1]); item_present_hook.Pass(guest,regs[0],regs[1]);
                if(regs[1]==4) MM3D::ItemSlideInterpolation::Publish({MM3D::guest_frames.load(),
                    MM3D::LinkPoseCapture::interpolation_generation.load(),item_present_hook.panel?item_present_hook.owner:0,
                    item_present_hook.transition_x,item_present_hook.transition_time,item_present_hook.transition_travel,
                    item_present_hook.transitioning,item_present_hook.transition_opening});
                press_hook.Pass(guest,regs[0],regs[1]); inventory_hook.Pass(guest,regs[0],regs[1]); notebook_hook.Pass(guest,regs[0],regs[1]); file_select_hook.Pass(guest,regs[0],regs[1]);
                if(regs[1]==5) {
                    // Inspect the submitted queues after routing. A stale page
                    // pointer alone cannot keep interpolation active after exit.
                    const u32 menu_owner=inventory_hook.SubmittedInterpolationOwner(guest,regs[0]);
                    MM3D::InventoryInterpolation::Publish(MM3D::guest_frames.load(),
                        MM3D::LinkPoseCapture::interpolation_generation.load(),menu_owner,
                        inventory_hook.SubmittedInterpolationOwner(guest,regs[0],true));
                }
                if(regs[1]==5) MM3D::FileSelectInput::Observe(file_select_hook.active &&
                    file_select_hook.entrance_pending);
                if(regs[1]==5) MM3D::FileSelectInterpolation::Publish({MM3D::guest_frames.load(),
                    MM3D::LinkPoseCapture::interpolation_generation.load(),
                    file_select_hook.active?file_select_hook.panel:options_hook.panel,
                    {file_select_hook.files_slide,file_select_hook.summary_slide,file_select_hook.actions_slide},
                    file_select_hook.presentation,file_select_hook.stone_slide});
                if(regs[1]==5&&file_select_hook.route_status!=file_select_hook.last_route_status) {
                    LOG_INFO(Core_ARM11,"MM3D FileSelect: active={} stage={} panel={:08x} summary={:08x} popup={:08x}",
                        file_select_hook.active,file_select_hook.route_status,file_select_hook.panel,file_select_hook.summary,file_select_hook.popup);
                    file_select_hook.last_route_status=file_select_hook.route_status;
                }
                banner_hook.Pass(guest,regs[0],regs[1]);
                if(regs[1]==0) MM3D::BannerInterpolation::Publish(MM3D::guest_frames.load(),
                    MM3D::LinkPoseCapture::interpolation_generation.load(),banner_hook.owner);
                regs[4]=regs[0]; break;
            case 2: classic_save_hook.BeginCycleDraw(guest,regs[4]); minigame_hook.Begin(guest,regs[4]); hud_buttons_hook.Begin(guest,regs[4]); song_reset_hook.Begin(guest,regs[4]); banner_hook.Begin(guest,regs[4],MM3D::Widescreen::Active()); double_time_hook.Begin(guest,regs[4]); dungeon_map_hook.Begin(guest,regs[4]); song_hook.Begin(guest,regs[4]); item_present_hook.Begin(guest,regs[4]); loading_icon_hook.Begin(guest,regs[4]); press_hook.Begin(guest,regs[4]); inventory_hook.Begin(guest,regs[4],MM3D::Widescreen::Active()); notebook_hook.Begin(guest,regs[4],MM3D::Widescreen::Active()); file_select_hook.Begin(guest,regs[4],MM3D::Widescreen::Active());
                if(MM3D::ClassicSaving::enabled && classic_save_hook.cycle) {
                    static std::vector<u32> traced_layouts;
                    static u32 traced_cycle=0;
                    if(traced_cycle!=classic_save_hook.cycle) {
                        traced_cycle=classic_save_hook.cycle;traced_layouts.clear();
                    }
                    if(traced_layouts.size()<32 && std::find(traced_layouts.begin(),traced_layouts.end(),regs[4])==traced_layouts.end()) {
                        traced_layouts.push_back(regs[4]);
                        const auto window=guest.Read(traced_cycle+0x174).value_or(0);
                        LOG_INFO(Core_ARM11,"Classic cycle draw: layout={:08x} message={} owned={:08x} inner={:08x} flags={:08x} hidden={}",
                            regs[4],MM3D::NativeInventory::LayoutNamed(guest,regs[4],"MessageWindow"),window,
                            window?guest.Read(window+0x358).value_or(0):0,
                            guest.Read(regs[4]+8+0xd4).value_or(0),
                            !classic_save_hook.cycle_draws.empty() && classic_save_hook.cycle_draws.back().address!=0);
                    }
                }
                options_hook.Begin(guest,regs[4]);
                player_hud_hook.Begin(guest,regs[4]);
                regs[2]=notebook_hook.PromptRoot(guest,regs[4],regs[13]);
                {const auto root=ocarina_hook.Root(guest,regs[4],regs[13],MM3D::Widescreen::Active());
                 if(root!=regs[4]+8)regs[2]=root;}
                {const auto root=magic_hook.Root(guest,regs[4],regs[13]);
                 if(root!=regs[4]+8)regs[2]=root;}
                {const auto root=song_hook.ShadowRoot(guest,regs[4],regs[13]);
                 if(root!=regs[4]+8)regs[2]=root;}
                {const auto content=regs[2];
                 regs[2]=song_hook.HeaderRoot(guest,regs[4],content,content!=regs[4]+8?content:regs[13]);}
                {const auto content=regs[2];
                 regs[2]=song_hook.PanelRoot(guest,regs[4],content,content!=regs[4]+8?content:regs[13]);}
                {const auto root=inventory_shadow_hook.Root(guest,inventory_hook,regs[4],regs[13],&item_present_hook);
                 if(root!=regs[4]+8)regs[2]=root;}
                items_map_prompt_hook.ResolveDescription(guest,item_preview_hook.owner,inventory_hook.panel);
                {const auto existing_root=regs[2];
                 const auto prompt_sp=existing_root!=regs[4]+8?existing_root:regs[13];
                 const auto root=items_map_prompt_hook.Root(guest,inventory_hook,regs[4],
                    prompt_sp,existing_root,MM3D::Widescreen::Active(),
                    parent.system.CoreTiming().GetGlobalTimeUs().count());
                 if(root!=regs[4]+8)regs[2]=root;}
                {const auto root=dungeon_map_hook.PromptRoot(guest,regs[4],regs[13],MM3D::Widescreen::Active());
                 if(root!=regs[4]+8)regs[2]=root;}
                {const auto root=dungeon_map_hook.PanelRoot(guest,regs[4],regs[13]);
                 if(root!=regs[4]+8)regs[2]=root;}
                {const auto root=dungeon_map_hook.ShadowRoot(guest,regs[4],regs[13]);
                 if(root!=regs[4]+8)regs[2]=root;}
                regs[2]=map_items_prompt_hook.MapRoot(guest,dungeon_map_hook,regs[4],
                    regs[2]!=regs[4]+8?regs[2]:regs[13],regs[2],MM3D::Widescreen::Active(),
                    parent.system.CoreTiming().GetGlobalTimeUs().count());
                regs[2]=world_map_prompt_hook.Root(guest,dungeon_map_hook,regs[4],
                    regs[2]!=regs[4]+8?regs[2]:regs[13],regs[2],MM3D::Widescreen::Active());
                regs[2]=menu_tabs_hook.Root(guest,inventory_hook,regs[4],
                    regs[2]!=regs[4]+8?regs[2]:regs[13],regs[2]);
                menu_help_hook.source.ResolveDescription(guest,item_preview_hook.owner,inventory_hook.panel);
                regs[2]=menu_help_hook.Root(guest,inventory_hook,regs[4],
                    regs[2]!=regs[4]+8?regs[2]:regs[13],regs[2]);
                if(!item_present_hook.IsDescription(guest,select_help_hook.source.description))select_help_hook.source.description=0;
                regs[2]=select_help_hook.Root(guest,inventory_hook,regs[4],
                    regs[2]!=regs[4]+8?regs[2]:regs[13],regs[2],&item_present_hook);
                regs[2]=file_select_prompt_hook.Root(guest,file_select_hook,options_hook,regs[4],
                    regs[2]!=regs[4]+8?regs[2]:regs[13],regs[2],MM3D::Widescreen::Active());
                regs[2]=classic_file_marker.Root(guest,file_select_hook,regs[4],regs[2],
                    regs[2]!=regs[4]+8?regs[2]:regs[13],
                    file_select_hook.active?MM3D::ClassicSaving::PendingMask():0,
                    parent.system.CoreTiming().GetGlobalTimeUs().count());
                regs[2]=classic_owl_draw.Root(guest,regs[4],regs[2],
                    regs[2]!=regs[4]+8?regs[2]:regs[13],
                    classic_save_hook.presentation.window,classic_save_hook.presentation.frames);
                {
                    static unsigned last_owl_draw_status=0;
                    const auto status=MM3D::ClassicSaving::owl_draw_status.load();
                    if(status!=last_owl_draw_status) {
                        LOG_INFO(Core_ARM11,"Classic owl draw: status={} (layout=1 labels=2 cursor=4 fade=8)",status);
                        last_owl_draw_status=status;
                    }
                }
                regs[2]=double_time_hook.ShadeRoot(guest,regs[4],regs[2],
                    regs[2]!=regs[4]+8?regs[2]:regs[13]);
                if(regs[2]!=regs[4]+8)regs[13]=regs[2];
                if(MM3D::CameraMidpoint::present_60.load())
                    MM3D::MessageInterpolation::Publish(MM3D::guest_frames.load(),
                        MM3D::LinkPoseCapture::interpolation_generation.load(),
                        MM3D::MessageLayout::Read(guest,regs[4]));
                break;
            case 3:
                if(const auto sp=double_time_hook.shade_scratch.End(guest))regs[13]=sp;
                classic_save_hook.EndCycleDraw(guest);
                if(const auto sp=classic_owl_draw.End(guest))regs[13]=sp;
                if(const auto sp=classic_file_marker.scratch.End(guest))regs[13]=sp;
                if(const auto sp=file_select_prompt_hook.End(guest))regs[13]=sp;
                if(const auto sp=select_help_hook.scratch.End(guest))regs[13]=sp;
                if(const auto sp=menu_help_hook.scratch.End(guest))regs[13]=sp;
                if(const auto sp=menu_tabs_hook.scratch.End(guest))regs[13]=sp;
                if(const auto sp=world_map_prompt_hook.End(guest))regs[13]=sp;
                if(const auto sp=map_items_prompt_hook.prompt.End(guest))regs[13]=sp;
                if(const auto sp=dungeon_map_hook.shadow_scratch.End(guest))regs[13]=sp;
                if(const auto sp=dungeon_map_hook.panel_scratch.End(guest))regs[13]=sp;
                if(const auto sp=dungeon_map_hook.map_prompts.scratch.End(guest))regs[13]=sp;
                if(const auto sp=items_map_prompt_hook.prompt.End(guest))regs[13]=sp;
                if(const auto sp=inventory_shadow_hook.scratch.End(guest))regs[13]=sp;
                if(const auto sp=song_hook.backdrop.scratch.End(guest))regs[13]=sp;
                if(const auto sp=song_hook.header.scratch.End(guest))regs[13]=sp;
                if(const auto sp=song_hook.shadow_scratch.End(guest))regs[13]=sp;
                if(const auto sp=magic_hook.End(guest))regs[13]=sp;
                if(const auto sp=ocarina_hook.End(guest))regs[13]=sp;
                if(const auto sp=notebook_hook.prompt_hook.End(guest))regs[13]=sp;
                player_hud_hook.End(guest);
                options_hook.End(guest);
                song_hook.End(guest); item_present_hook.End(guest); file_select_hook.End(guest); notebook_hook.End(guest); inventory_hook.End(guest); press_hook.End(guest); loading_icon_hook.End(guest); dungeon_map_hook.End(guest); double_time_hook.End(guest); banner_hook.End(guest); song_reset_hook.End(guest); hud_buttons_hook.End(guest); minigame_hook.End(guest); guest.Write(regs[6],regs[0]); break;
            }
            return;
        }
        if(swi==MM3D::DialogueScale::BeforeSVC || swi==MM3D::DialogueScale::AfterSVC) {
            struct PoseMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
                void Write(u32 at,u32 value) { memory.Write32(at,value); }
            } pose{memory};
            const auto layout=parent.jit->Regs()[4];
            if(swi==MM3D::DialogueScale::BeforeSVC) {
                const bool description=item_preview_hook.Description(pose,layout);
                const bool present_description=item_present_hook.IsDescription(pose,layout);
                if(present_description){select_help_hook.source.description=layout;
                    select_help_hook.source.description_panel=item_present_hook.panel;}
                const auto& setting=(item_present_hook.mask_menu?MM3D::NativeItemPresent::mask_poses:
                    MM3D::NativeItemPresent::poses)[MM3D::NativeItemPresent::Description];
                const MM3D::GearMenu::Pose description_pose{setting.x.load(),setting.y.load(),setting.scale.load()};
                const bool map_description=dungeon_map_hook.IsDescription(pose,layout);
                const auto& map_setting=MM3D::DungeonMap::poses[MM3D::DungeonMap::Description];
                const MM3D::GearMenu::Pose map_pose{map_setting.x.load(),map_setting.y.load(),map_setting.scale.load()};
                const bool reset_description=song_reset_hook.IsDescription(pose,layout);
                const auto& reset_setting=MM3D::SongReset::poses[MM3D::SongReset::Description];
                const MM3D::GearMenu::Pose reset_pose{reset_setting.x.load(),reset_setting.y.load(),reset_setting.scale.load()};
                const bool options_description=options_hook.IsDescription(pose,layout);
                dialogue_hook.Begin(pose,layout,description,description&&item_preview_hook.gear,
                    options_description?&options_hook.frame.description:reset_description?&reset_pose:map_description?&map_pose:present_description?&description_pose:nullptr);
                item_present_hook.BeginPromptPose(pose,layout);
                parent.jit->Regs()[0]=layout+8; // displaced ADD r0,r4,#8
            } else {
                item_present_hook.EndPromptPose(pose);
                dialogue_hook.End(pose,layout);
                parent.jit->Regs()[13]+=8; // displaced ADD sp,sp,#8
            }
            return;
        }
        if(swi>=MM3D::DungeonMap::FirstSVC&&swi<MM3D::DungeonMap::FirstSVC+MM3D::DungeonMap::PCs.size()) {
            struct MapMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& regs=parent.jit->Regs();
            switch(swi-MM3D::DungeonMap::FirstSVC) {
            case 0: dungeon_map_hook.Capture(guest,regs[0],regs[1]);regs[4]=regs[0];break;
            case 1:
                dungeon_map_hook.BeforeMap(guest);
                regs[14]=MM3D::DungeonMap::PCs[1]+4;regs[15]=0x2a326c;break;
            case 2: dungeon_map_hook.AfterMap(guest);regs[0]=memory.Read8(regs[4]+0x74c);break;
            case 3:
                if(dungeon_map_hook.valid) MM3D::DungeonMap::LogInput(guest,regs[4],memory.Read8(regs[6]+0xc));
                regs[0]=memory.Read8(regs[6]+0xc);
                regs[0]=MM3D::DungeonMap::FilterA(guest,regs[4],regs[0]);
                break;
            case 4:
                regs[0]=memory.Read32(regs[4]+0x1c);
                if(dungeon_map_hook.SkipWorld(guest,regs[4]))regs[15]=MM3D::DungeonMap::WorldEnd;
                break;
            }
            return;
        }
        if(swi==MM3D::OptionsMenu::DrawSVC||swi==MM3D::OptionsMenu::SelectSVC) {
            struct CodeMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            auto& regs=parent.jit->Regs();
            if(swi==MM3D::OptionsMenu::DrawSVC)options_hook.Capture(guest,regs[0]);
            else regs[2]=MM3D::OptionsMenu::Selection(guest,regs[0],regs[2]);
            regs[4]=regs[0];return;
        }
        if (swi==MM3D::NativeMinimap::BeforeSVC || swi==MM3D::NativeMinimap::AfterSVC) {
            struct MapMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
                void Write(u32 at,u32 value) { memory.Write32(at,value); }
            } map_memory{memory};
            const auto screen=parent.jit->Regs()[4];
            if (swi==MM3D::NativeMinimap::BeforeSVC) {
                minimap_hook.Begin(map_memory,screen,MM3D::Minimap::enabled.load(),parent.jit->Regs()[0]);
            } else {
                minimap_hook.End(map_memory,screen,MM3D::Minimap::enabled.load(),
                    MM3D::Minimap::scale.load(),MM3D::Minimap::opacity.load(),
                    MM3D::Widescreen::Active() ? 400.f+(240.f*16.f/9.f-400.f)/2.f : 400.f);
                // Emulate the displaced LDR r0,[r4,#0x38], including paths which skip map drawing.
                parent.jit->Regs()[0]=memory.Read32(screen+0x38);
            }
            return;
        }
        if(swi>=MM3D::SongEffect::FirstSVC&&swi<MM3D::SongEffect::FirstSVC+MM3D::SongEffect::Loads.size()) {
            struct EffectMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 value){memory.Write32(at,value);}} guest{memory};
            auto& regs=parent.jit->Regs();
            MM3D::SongEffect::Expand(guest,regs[13],MM3D::CameraLive::desired_fov.load(),
                MM3D::Widescreen::Active(),MM3D::Widescreen::camera_zoom_out.load());
            regs[1]=regs[13]; // displaced MOV, including guarded fallback
            return;
        }
        if(swi==MM3D::FormCamera::EntrySVC||swi==MM3D::FormCamera::RadiusSVC||swi==MM3D::FormCamera::LockSVC) {
            struct CameraMemory {Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}
                void Write(u32 at,u32 word){memory.Write32(at,word);}} guest{memory};
            auto& regs=parent.jit->Regs();
            if(swi==MM3D::FormCamera::EntrySVC) {
                const bool native_entry_orbit=entry_recovery_orbit.Native(guest,regs[0]);
                form_camera.Entry(guest,regs,native_entry_orbit);
                const auto deku_lens_before=guest.Read(regs[0]+0x110).value_or(0);
                deku_free_camera.Step(guest,regs[0]);
                if(MM3D::FormCamera::diagnostics.load()) {
                    const auto actor=guest.Read(regs[0]+0xa4).value_or(0);
                    LOG_INFO(Core_ARM11,"Deku free camera: armed={} recovering={} flags3={:08x} ground={:08x} lens_before={} lens_after={}",
                        deku_free_camera.armed,deku_free_camera.recovering,
                        actor?guest.Read(actor+0x11db8).value_or(0):0,
                        actor?guest.Read(actor+0x94).value_or(0):0,
                        std::bit_cast<float>(deku_lens_before),
                        std::bit_cast<float>(guest.Read(regs[0]+0x110).value_or(0)));
                }
                const bool ocarina_before=ocarina_return_lens.pending||ocarina_return_lens.active;
                const auto ocarina_bits=ocarina_before?guest.Read(regs[0]+0x110).value_or(0):0;
                ocarina_return_lens.Step(guest,regs[0]);
                if(ocarina_before&&MM3D::FormCamera::diagnostics.load())
                    LOG_INFO(Core_ARM11,"Ocarina return lens: cam={:08x} active={} before={} after={}",
                        regs[0],ocarina_return_lens.active,std::bit_cast<float>(ocarina_bits),
                        std::bit_cast<float>(guest.Read(regs[0]+0x110).value_or(0)));
                const bool arrival_before=arrival_lens.active;
                const auto arrival_bits=guest.Read(regs[0]+0x110).value_or(0);
                const auto expected_bits=arrival_lens.last_bits;
                arrival_lens.Step(guest,regs[0]);
                if((arrival_before||arrival_lens.active)&&MM3D::FormCamera::diagnostics.load())
                    LOG_INFO(Core_ARM11,"Arrival lens step: active={} setting={} normal={} before={} expected={} after={} rate={} orbit={} orbit_target={}",
                        arrival_lens.active,guest.Read(regs[0]+0x154).value_or(0)>>16,
                        arrival_lens.NormalFov(guest,guest.Read(regs[0]+0x154).value_or(0)>>16).value_or(-1.f),
                        std::bit_cast<float>(arrival_bits),std::bit_cast<float>(expected_bits),
                        std::bit_cast<float>(guest.Read(regs[0]+0x110).value_or(0)),
                        std::bit_cast<float>(guest.Read(regs[0]+0xe8).value_or(0)),
                        arrival_lens.radius,arrival_lens.radius_target);
                if(MM3D::FormCamera::diagnostics.load())
                    LOG_INFO(Core_ARM11,"Form camera entry: cam={:08x} phase={:08x} target={:08x} radius={} saved={} native_entry_orbit={}",
                        regs[4],guest.Read(regs[4]+0x170).value_or(0),guest.Read(regs[4]+0xbc).value_or(0),
                        form_camera.radius,form_camera.saved,native_entry_orbit);
            }
            else if(swi==MM3D::FormCamera::RadiusSVC) {
                const auto before=guest.Read(regs[13]);
                form_camera.Radius(guest,regs);
                arrival_lens.Radius(guest,regs,form_camera);
                if(MM3D::FormCamera::diagnostics.load()) {
                    const auto after=guest.Read(regs[13]);
                    LOG_INFO(Core_ARM11,"Form camera orbit: cam={:08x} native={} applied={} saved={}",
                        regs[4],before?std::bit_cast<float>(*before):0.f,
                        after?std::bit_cast<float>(*after):0.f,form_camera.saved);
                }
            } else {
                const auto requested=regs[1];
                const auto cam=regs[0];
                form_camera.LockMode(guest,regs);
                if(MM3D::FormCamera::diagnostics.load()) {
                    const auto target=form_camera.Target(guest,cam,true);
                    const u32 player=target?(*target)[0]:0;
                    LOG_INFO(Core_ARM11,"Form camera lock request: cam={:08x} requested={} mode={} native_lock_allowed={} camera_flags={:08x} camera_target={:08x} player_flags={:08x} player_target={:08x}",
                        cam,requested,guest.Read(cam+0x158).value_or(0)&65535,
                        regs[15]==regs[14]&&regs[0]==0,
                        guest.Read(cam+0x170).value_or(0),guest.Read(cam+0xbc).value_or(0),
                        player?guest.Read(player+0x11db0).value_or(0):0,
                        player?guest.Read(player+0xdfc).value_or(0):0);
                }
            }
            return;
        }
        if(swi==MM3D::FormCamera::SVC) {
            struct CameraMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            const auto cam=parent.jit->Regs()[4];
            const float before_s0=std::bit_cast<float>(parent.jit->ExtRegs()[0]);
            const float before_s1=std::bit_cast<float>(parent.jit->ExtRegs()[1]);
            const auto mode=guest.Read(cam+0x158),native=guest.Read(cam+0xf0);
            form_camera.Distance(guest,cam,parent.jit->ExtRegs());
            MM3D::FormCamera::DekuFreeCamera::Distance(guest,cam,parent.jit->ExtRegs());
            const bool capture=MM3D::FormCamera::diagnostics.load();
            if(capture&&!form_trace_was_active) {form_trace_count=form_projection_count=0;form_last_native=form_last_output=form_last_radius=-1.f;form_last_mode=~0u;}
            form_trace_was_active=capture;
            if(capture) {
                ++form_trace_count;
                LOG_INFO(Core_ARM11,"Form camera distance: cam={:08x} mode={} native_radius={} s0_before={} s1_before={} s0_after={} s1_after={} saved_radius={} saved={}",
                    cam,mode?(*mode&65535):999u,native?std::bit_cast<float>(*native):0.f,
                    before_s0,before_s1,std::bit_cast<float>(parent.jit->ExtRegs()[0]),
                    std::bit_cast<float>(parent.jit->ExtRegs()[1]),form_camera.radius,form_camera.saved);
            }
            return;
        }
        if(swi==MM3D::CameraLive::ChangeModeSVC) {
            struct CameraMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            const auto cam=parent.jit->Regs()[0];
            const auto requested=parent.jit->Regs()[1];
            const auto mode_before=guest.Read(cam+0x158);
            const auto effective=form_camera.RequestedMode(guest,cam,requested);
            parent.jit->Regs()[1]=effective;
            const bool arrival_before=arrival_lens.active;
            arrival_lens.Observe(guest,cam,effective);
            ocarina_return_lens.Observe(guest,cam,effective);
            form_camera.ChangeMode(guest,cam,effective);
            entry_recovery_orbit.Observe(guest,cam,effective);
            deku_free_camera.Observe(cam,effective);
            if(MM3D::FormCamera::diagnostics.load()) {
                const auto player=guest.Read(cam+0xa4).value_or(0);
                LOG_INFO(Core_ARM11,"Arrival lens mode: before={} active={} setting={} normal={} native={} ground={:08x} requested={}",
                    arrival_before,arrival_lens.active,guest.Read(cam+0x154).value_or(0)>>16,
                    arrival_lens.NormalFov(guest,guest.Read(cam+0x154).value_or(0)>>16).value_or(-1.f),
                    std::bit_cast<float>(guest.Read(cam+0x110).value_or(0)),
                    player?guest.Read(player+0x94).value_or(0):0,effective);
                ++form_trace_count;
                LOG_INFO(Core_ARM11,"Form camera mode: cam={:08x} requested={} mode_before={} effective={} radius={} saved={}",
                    cam,requested,mode_before?(*mode_before&65535):999u,effective,form_camera.radius,form_camera.saved);
            }
            parent.jit->Regs()[4]=cam+0x100; // displaced ADD; native mode selection retained
            return;
        }
        if(swi==MM3D::CameraLive::ProjectionSVC) {
            // SVC is a synchronization boundary: R4 and S0 are live here.
            // Emulate the replaced VLDR, changing only its destination register.
            const u32 native=memory.Read32(parent.jit->Regs()[4]+0x18);
            struct CameraMemory {Memory::MemorySystem& memory;std::optional<u32> Read(u32 at){return memory.Read32OrNullopt(at);}} guest{memory};
            const auto projection=parent.jit->Regs()[4];
            const float configured=MM3D::CameraLive::ConfiguredFov(guest,projection);
            const auto output=MM3D::CameraLive::ProjectionValue(native,configured);
            parent.jit->ExtRegs()[0]=output;
            // Manual form capture remains available for all supported forms, including Deku.
            if(MM3D::FormCamera::diagnostics.load()) {
                const auto ctx=memory.Read32(0x72f5c0);
                std::optional<u32> game,player,form;
                if(ctx) game=memory.Read32(ctx+0x16c);
                if(game) player=memory.Read32(*game+0x20dc);
                if(player) form=memory.Read32(*player+0x1fc);
                const bool form_match=form&&(MM3D::FormCamera::SupportedForm(*form>>24)||(*form>>24)==3);
                if(form_match&&memory.Read32(MM3D::CameraLive::CameraPointer)==projection) {
                    const auto indices=game?memory.Read32(*game+0xa78):std::optional<u32>{};
                    const unsigned index=indices?(*indices>>16):99;
                    const auto camera=(game&&index<4)?(*game+0x408+index*0x198):0;
                    const auto mode=camera?memory.Read32(camera+0x158):std::optional<u32>{};
                    const auto radius=camera?memory.Read32(camera+0xf0):std::optional<u32>{};
                    const u32 mode_value=mode?(u32(*mode)&65535):999u;
                    const float native_value=std::bit_cast<float>(native),output_value=std::bit_cast<float>(output),
                        radius_value=radius?std::bit_cast<float>(*radius):0.f;
                    std::array<u32,6> pose{};
                    for(unsigned axis=0;axis<6;++axis)pose[axis]=memory.Read32(projection+0x28+axis*4);
                    const bool changed=pose!=form_last_pose||mode_value!=form_last_mode||std::abs(native_value-form_last_native)>.05f||
                        std::abs(output_value-form_last_output)>.05f||std::abs(radius_value-form_last_radius)>.05f;
                    if(changed) {
                        ++form_projection_count;form_last_pose=pose;form_last_mode=mode_value;form_last_native=native_value;
                        form_last_output=output_value;form_last_radius=radius_value;
                        LOG_INFO(Core_ARM11,"Form camera projection: form={} mode={} native_fov={} configured={} output_fov={} radius={} projection={:08x} eye=({},{},{}) at=({},{},{}) target={:08x} flags={:08x} camera={:08x} index={} setting={}",
                            *form>>24,mode_value,native_value,configured,output_value,radius_value,projection,
                            std::bit_cast<float>(pose[0]),std::bit_cast<float>(pose[1]),std::bit_cast<float>(pose[2]),
                            std::bit_cast<float>(pose[3]),std::bit_cast<float>(pose[4]),std::bit_cast<float>(pose[5]),
                            camera?guest.Read(camera+0xbc).value_or(0):0,camera?guest.Read(camera+0x170).value_or(0):0,
                            camera,index,camera?guest.Read(camera+0x154).value_or(0)>>16:0);
                    }
                }
            }
            return;
        }
#endif
        svc_context.CallSVC(swi);
    }

    void ExceptionRaised(VAddr pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::UndefinedInstruction:
        case Dynarmic::A32::Exception::UnpredictableInstruction:
        case Dynarmic::A32::Exception::DecodeError:
        case Dynarmic::A32::Exception::NoExecuteFault:
            break;
        case Dynarmic::A32::Exception::Breakpoint:
#ifdef ENABLE_GDBSTUB
            if (GDBStub::IsConnected()) {
                parent.SetPC(pc);
                parent.ServeBreak(SIGTRAP);
                return;
            }
#endif
            break;
        case Dynarmic::A32::Exception::SendEvent:
        case Dynarmic::A32::Exception::SendEventLocal:
        case Dynarmic::A32::Exception::WaitForInterrupt:
        case Dynarmic::A32::Exception::WaitForEvent:
        case Dynarmic::A32::Exception::Yield:
        case Dynarmic::A32::Exception::PreloadData:
        case Dynarmic::A32::Exception::PreloadDataWithIntentToWrite:
        case Dynarmic::A32::Exception::PreloadInstruction:
            return;
        }

        static constexpr auto ExceptionToString = [](Dynarmic::A32::Exception e) -> std::string {
            switch (e) {
            case Dynarmic::A32::Exception::UndefinedInstruction:
                return "UndefinedInstruction";
            case Dynarmic::A32::Exception::UnpredictableInstruction:
                return "UnpredictableInstruction";
            case Dynarmic::A32::Exception::DecodeError:
                return "DecodeError";
            case Dynarmic::A32::Exception::NoExecuteFault:
                return "NoExecuteFault";
            case Dynarmic::A32::Exception::Breakpoint:
                return "Breakpoint";
            default:
                return fmt::format("Unknown({})", e);
            }
        };

        parent.SetPC(pc);
#ifdef ENABLE_GDBSTUB
        if (GDBStub::IsConnected()) {
            parent.ServeBreak(SIGILL);
        } else
#endif
        {
            Core::ExceptionType exc_type;
            switch (exception) {
            case Dynarmic::A32::Exception::UndefinedInstruction:
            case Dynarmic::A32::Exception::UnpredictableInstruction:
            case Dynarmic::A32::Exception::DecodeError:
            case Dynarmic::A32::Exception::NoExecuteFault:
                exc_type = Core::ExceptionType::UndefinedInstruction;
                break;
            case Dynarmic::A32::Exception::Breakpoint:
                exc_type = Core::ExceptionType::Break;
                break;
            default:
                exc_type = Core::ExceptionType::UndefinedInstruction;
                break;
            }
            Core::LogException(parent.system, exc_type);
        }
    }

    void AddTicks(std::uint64_t ticks) override {
        parent.GetTimer().AddTicks(ticks);
    }
    std::uint64_t GetTicksRemaining() override {
        s64 ticks = parent.GetTimer().GetDowncount();
        return static_cast<u64>(ticks <= 0 ? 0 : ticks);
    }
    std::uint64_t GetTicksForCode(bool is_thumb, VAddr, std::uint32_t instruction) override {
        return Core::TicksForInstruction(is_thumb, instruction);
    }

    ARM_Dynarmic& parent;
    Kernel::SVCContext svc_context;
    Memory::MemorySystem& memory;
};

ARM_Dynarmic::ARM_Dynarmic(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                           std::shared_ptr<Core::Timing::Timer> timer_,
                           Core::ExclusiveMonitor& exclusive_monitor_)
    : ARM_Interface(core_id_, timer_), system(system_), memory(memory_),
      cb(std::make_unique<DynarmicUserCallbacks>(*this)),
      exclusive_monitor{dynamic_cast<Core::DynarmicExclusiveMonitor&>(exclusive_monitor_)} {
#ifdef ENABLE_OOT3D_PRESENTATION
    if(core_id_==0)OOT3D::GraphicsAllocations::Reset();
#endif
    SetPageTable(memory.GetCurrentPageTable());
}

ARM_Dynarmic::~ARM_Dynarmic() = default;

MICROPROFILE_DEFINE(ARM_Jit, "ARM JIT", "ARM JIT", MP_RGB(255, 64, 64));

void ARM_Dynarmic::Run() {
    ASSERT(memory.GetCurrentPageTable() == current_page_table);
    MICROPROFILE_SCOPE(ARM_Jit);
    if (break_flag) [[unlikely]] {
        return;
    }

#ifdef ENABLE_MM3D_PRESENTATION
    if(MM3D::CameraMidpoint::enabled.load() || MM3D::CameraLive::enabled.load() || MM3D::CameraLive::capture_requested.load() || MM3D::CameraLive::desired_fov.load()>45.f || MM3D::CameraLive::needs_service.load()) {
        const auto process=system.Kernel().GetCurrentProcess();
        if(process && process->codeset && process->codeset->program_id==0x0004000000125500ULL) {
            struct CameraMemory {
                Memory::MemorySystem& memory;
                std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }


            } camera_memory{memory};
            MM3D::CameraLive::Service(camera_memory,current_page_table.get(),std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }
    }
    UpdateCameraTraceMode();
    if(camera_tracing && MM3D::CameraWriter::ShouldTrace()) {
        // Bound host time independently of guest progress/rendering. Each Step
        // returns after one instruction, including conditional and idle-loop code.
        for(unsigned i=0; i<256 && GetTimer().GetDowncount()>0; ++i) {
            if(!MM3D::CameraWriter::ShouldTrace()) break;
            const auto halt = MM3D::CameraWriter::StepWithSnapshot(*jit, cb->camera_instruction);
            if((halt & Dynarmic::HaltReason::UserDefined1) == Dynarmic::HaltReason::UserDefined1) break;
        }
    } else
#endif
    {
        jit->Run();
    }
#ifdef ENABLE_MM3D_PRESENTATION
    if (MM3D::NotebookInspector::enabled.load() && current_page_table) {
        struct Pages {
            Memory::MemorySystem& memory;
            Memory::PageTable& table;
            size_t size() const { return Memory::PAGE_TABLE_NUM_ENTRIES; }
            u8* operator[](size_t i) {
                if (table.attributes[i] == Memory::PageType::RasterizerCachedMemory)
                    return memory.GetPointer(static_cast<u32>(i << 12));
                return table.GetPointerArray()[i];
            }
        } pages{memory, *current_page_table};
        MM3D::NotebookInspector::Poll(pages, current_page_table.get());
    }
#endif
}

void ARM_Dynarmic::Step() {
    if (break_flag) [[unlikely]] {
        return;
    }

#ifdef ENABLE_MM3D_PRESENTATION
    UpdateCameraTraceMode();
    if(camera_tracing && MM3D::CameraWriter::ShouldTrace()) MM3D::CameraWriter::StepWithSnapshot(*jit, cb->camera_instruction);
    else
#endif
        jit->Step();
}

#ifdef ENABLE_MM3D_PRESENTATION
void ARM_Dynarmic::UpdateCameraTraceMode() {
    const bool wanted = MM3D::CameraWriter::ShouldTrace();
    if(wanted == camera_tracing) return;
    ThreadContext saved{};
    SaveContext(saved);
    // Switching backends may spuriously fail an outstanding exclusive store;
    // ARM permits this. Do not carry a stale local reservation into the new JIT.
    jit->ClearExclusiveState();
    jit = nullptr;
    jits.swap(camera_other_jits);
    camera_tracing = wanted;
    SetPageTable(current_page_table);
    jit->ClearExclusiveState();
    LoadContext(saved);
}
#endif
void ARM_Dynarmic::SetPC(u32 pc) {
    jit->Regs()[15] = pc;
}

u32 ARM_Dynarmic::GetPC() const {
    return jit->Regs()[15];
}

u32 ARM_Dynarmic::GetReg(int index) const {
    return jit->Regs()[index];
}

void ARM_Dynarmic::SetReg(int index, u32 value) {
    jit->Regs()[index] = value;
}

u32 ARM_Dynarmic::GetVFPReg(int index) const {
    return jit->ExtRegs()[index];
}

void ARM_Dynarmic::SetVFPReg(int index, u32 value) {
    jit->ExtRegs()[index] = value;
}

u32 ARM_Dynarmic::GetVFPSystemReg(VFPSystemRegister reg) const {
    switch (reg) {
    case VFP_FPSCR:
        return jit->Fpscr();
    case VFP_FPEXC:
        return fpexc;
    default:
        UNREACHABLE_MSG("Unknown VFP system register: {}", reg);
    }

    return UINT_MAX;
}

void ARM_Dynarmic::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    switch (reg) {
    case VFP_FPSCR:
        jit->SetFpscr(value);
        return;
    case VFP_FPEXC:
        fpexc = value;
        return;
    default:
        UNREACHABLE_MSG("Unknown VFP system register: {}", reg);
    }
}

u32 ARM_Dynarmic::GetCPSR() const {
    return jit->Cpsr();
}

void ARM_Dynarmic::SetCPSR(u32 cpsr) {
    jit->SetCpsr(cpsr);
}

u32 ARM_Dynarmic::GetCP15Register(CP15Register reg) const {
    switch (reg) {
    case CP15_THREAD_UPRW:
        return cp15_state.cp15_thread_uprw;
    case CP15_THREAD_URO:
        return cp15_state.cp15_thread_uro;
    default:
        UNREACHABLE_MSG("Unknown CP15 register: {}", reg);
    }

    return 0;
}

void ARM_Dynarmic::SetCP15Register(CP15Register reg, u32 value) {
    switch (reg) {
    case CP15_THREAD_UPRW:
        cp15_state.cp15_thread_uprw = value;
        return;
    case CP15_THREAD_URO:
        cp15_state.cp15_thread_uro = value;
        return;
    default:
        UNREACHABLE_MSG("Unknown CP15 register: {}", reg);
    }
}

void ARM_Dynarmic::SaveContext(ThreadContext& ctx) {
    ctx.cpu_registers = jit->Regs();
    ctx.cpsr = jit->Cpsr();
    ctx.fpu_registers = jit->ExtRegs();
    ctx.fpscr = jit->Fpscr();
    ctx.fpexc = fpexc;
}

void ARM_Dynarmic::LoadContext(const ThreadContext& ctx) {
    jit->Regs() = ctx.cpu_registers;
    jit->SetCpsr(ctx.cpsr);
    jit->ExtRegs() = ctx.fpu_registers;
    jit->SetFpscr(ctx.fpscr);
    fpexc = ctx.fpexc;
}

void ARM_Dynarmic::PrepareReschedule() {
    if (jit->IsExecuting()) {
        jit->HaltExecution();
    }
}

void ARM_Dynarmic::ClearInstructionCache() {
#ifdef ENABLE_MM3D_PRESENTATION
    cb->menu_page_switch={};cb->menu_memory={};cb->menu_backdrop={};cb->menu_sound_context=0;
    for(const auto& j : camera_other_jits) j.second->ClearCache();
#endif
    for (const auto& j : jits) {
        j.second->ClearCache();
    }
}

void ARM_Dynarmic::InvalidateCacheRange(u32 start_address, std::size_t length) {
    jit->InvalidateCacheRange(start_address, length);
#ifdef ENABLE_MM3D_PRESENTATION
    // Also invalidate the dormant mode's cache for the active process.
    auto other = camera_other_jits.find(current_page_table);
    if(other != camera_other_jits.end()) other->second->InvalidateCacheRange(start_address, length);
#endif
}

void ARM_Dynarmic::ClearExclusiveState() {
    jit->ClearExclusiveState();
}

std::shared_ptr<Memory::PageTable> ARM_Dynarmic::GetPageTable() const {
    return current_page_table;
}

void ARM_Dynarmic::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    current_page_table = page_table;
    ThreadContext ctx{};
    if (jit) {
        SaveContext(ctx);
    }

    auto iter = jits.find(current_page_table);
    if (iter != jits.end()) {
        jit = iter->second.get();
        LoadContext(ctx);
        return;
    }

    auto new_jit = MakeJit();
    jit = new_jit.get();
    LoadContext(ctx);
    jits.emplace(current_page_table, std::move(new_jit));
}

void ARM_Dynarmic::ServeBreak([[maybe_unused]] int signal) {
#ifdef ENABLE_GDBSTUB
    GDBStub::Break(signal);
#endif
}

std::unique_ptr<Dynarmic::A32::Jit> ARM_Dynarmic::MakeJit() {
    Dynarmic::A32::UserConfig config;
    config.callbacks = cb.get();
    if (current_page_table
#ifdef ENABLE_MM3D_PRESENTATION
        && !camera_tracing
#endif
    ) {
        config.page_table = &current_page_table->GetPointerArray();
    }
#ifdef ENABLE_MM3D_PRESENTATION
    if(camera_tracing) config.code_cache_size = 16 * 1024 * 1024;
#endif
    config.coprocessors[15] = std::make_shared<DynarmicCP15>(cp15_state);
    config.define_unpredictable_behaviour = true;

    // Multi-process state
    config.processor_id = GetID();
    config.global_monitor = &exclusive_monitor.monitor;

    return std::make_unique<Dynarmic::A32::Jit>(config);
}

} // namespace Core
