// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/multiplayer_settings.h"
#include "oot3d/interpolation_timing.h"
#include "oot3d/sun_readback_probe.h"
#include "oot3d/sun_visibility.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cfloat>
#include <future>
#include <stdexcept>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "oot3d/actor_capture.h"
#include "oot3d/surface_capture.h"
#include "oot3d/surface_effects.h"
#include "oot3d/fairy_glow_interpolation.h"
#include "oot3d/actor_draw_distance.h"
#include "oot3d/boss_challenge_layout.h"
#include "oot3d/overlay.h"
#include "oot3d/randomizer/activity.h"
#include "oot3d/hd_font.h"
#include "oot3d/hud_routing.h"
#include "oot3d/hd_overlay_font.h"
#include "oot3d/graphics_memory_diagnostics.h"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include "oot3d/input.h"
#include "oot3d/settings.h"
#include "oot3d/player_graphics.h"
#include "oot3d/player_defaults_draft.h"
#include "oot3d/player_mode.h"
#include "oot3d/user_camera_config.h"
#include "oot3d/control_swap.h"
#include "oot3d/game_control_options.h"
#include "oot3d/gyro_aim.h"
#include "oot3d/audio_settings.h"
#include "audio_core/hle/accuracy.h"
#include "oot3d/widescreen.h"
#include "oot3d/camera.h"
#include "oot3d/camera_transition_diagnostics.h"
#include "oot3d/interpolation.h"
#include "oot3d/first_person_diagnostics.h"
#include "oot3d/interpolation_pose.h"
#include "oot3d/orbit_camera.h"
#include "oot3d/dialogue_camera.h"
#include "oot3d/pause_menu.h"
#include "oot3d/pause_header.h"
#include "oot3d/pause_prompt_layout.h"
#include "oot3d/ocarina_layout.h"
#include "oot3d/ocarina_slide.h"
#include "oot3d/file_select.h"
#include "oot3d/hud_buttons.h"
#include "oot3d/map_cutout_editor.h"
#include "oot3d/world_map_assets.h"
#include "oot3d/title_layout.h"
#include "oot3d/hud_layout.h"
#include "oot3d/dpad_layout.h"
#include "oot3d/textbox_layout.h"
#include "oot3d/texture_inspector.h"
#include "oot3d/texture_picker.h"
#include "oot3d/qol.h"
#include "oot3d/ui_language.h"
#include "oot3d/difficulty.h"
#include "oot3d/rumble.h"
#include "oot3d/randomizer/service.h"
#include "oot3d/randomizer/ui.h"
#include "oot3d/randomizer/icons.h"
#include "video_core/renderer_vulkan/vk_instance.h"

namespace OOT3D {
static bool DrawOverallHUDScale(float& scale) {
    float percent=scale*100.f;
    if(!ImGui::SliderFloat("Overall HUD scale",&percent,50,200,"%.1f%%",
                           ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoRoundToFormat))return false;
    scale=percent/100.f;
    return true;
}
static bool DrawHUDPadding(UserHUD::Padding& padding) {
    bool changed=false;
    changed|=ImGui::SliderInt("Top padding",&padding.top,-UserHUD::VerticalPaddingLimit,UserHUD::VerticalPaddingLimit,"%+d",ImGuiSliderFlags_AlwaysClamp);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Positive moves the upper HUD down; negative moves it up.");
    changed|=ImGui::SliderInt("Bottom padding",&padding.bottom,-UserHUD::VerticalPaddingLimit,UserHUD::VerticalPaddingLimit,"%+d",ImGuiSliderFlags_AlwaysClamp);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Positive moves the lower HUD up; negative moves it down.");
    changed|=ImGui::SliderInt("Left padding",&padding.left,-UserHUD::SidePaddingLimit,UserHUD::SidePaddingLimit,"%+d",ImGuiSliderFlags_AlwaysClamp);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Moves hearts, magic, D-pad, Shard of Agony, rupees and keys.\nPositive moves inward; negative moves outward.");
    changed|=ImGui::SliderInt("Right padding",&padding.right,-UserHUD::SidePaddingLimit,UserHUD::SidePaddingLimit,"%+d",ImGuiSliderFlags_AlwaysClamp);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Moves buttons, targeting indicator, minimaps, loading icon and minigame counter.\nPositive moves inward; negative moves outward.");
    if(ImGui::Button("Reset padding")) {padding={};changed=true;}
    return changed;
}
static bool DrawHUDOpacity(const char* label,float& opacity) {
    float percent=opacity*100.f;
    if(!ImGui::SliderFloat(label,&percent,0,100,"%.0f%%",ImGuiSliderFlags_AlwaysClamp))return false;
    opacity=percent/100.f;
    return true;
}
static bool DrawMagicBorderThickness(float& thickness) {
    return ImGui::SliderFloat("Outer border thickness",&thickness,.25f,8.f,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
}
static bool DrawSpriteContour(const char* label,HUD::SpriteContour& layer,const char* tooltip=nullptr,
                              float* border_roundness=nullptr) {
    bool changed=false;
    ImGui::PushID(label);
    changed|=ImGui::Checkbox(label,&layer.enabled);
    if(tooltip && ImGui::IsItemHovered())ImGui::SetTooltip("%s",tooltip);
    if(layer.enabled) {
        changed|=ImGui::ColorEdit3("Color",layer.color.data(),ImGuiColorEditFlags_NoOptions);
        if(border_roundness) {
            changed|=ImGui::SliderFloat("Corner roundness",border_roundness,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        } else {
            changed|=ImGui::SliderFloat("Width",&layer.width,.25f,3,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
            changed|=ImGui::SliderFloat("Height",&layer.height,.25f,3,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
        }
        changed|=ImGui::DragFloat("Horizontal offset",&layer.x,.1f,-20,20,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
        changed|=ImGui::DragFloat("Vertical offset",&layer.y,.1f,-20,20,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
        changed|=ImGui::SliderFloat("Softness",&layer.softness,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        changed|=ImGui::SliderFloat("Opacity",&layer.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::PopID();
    return changed;
}

static bool DrawSoftBackdrop(HUD::AmmoBackdrop& b) {
    bool changed=false;
    changed|=ImGui::Checkbox("Enable backdrop",&b.enabled);
    ImGui::BeginDisabled(!b.enabled);
    changed|=ImGui::SliderFloat("Opacity",&b.opacity,0,1,"%.2f");
    changed|=ImGui::DragFloat("Horizontal offset",&b.x,.1f,-20,20,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::DragFloat("Vertical offset",&b.y,.1f,-20,20,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::DragFloat("Width",&b.width,.01f,.25f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::DragFloat("Height",&b.height,.01f,.25f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Softness",&b.softness,0,1,"%.2f");
    if(ImGui::Button("Reset backdrop")){b={};changed=true;}
    ImGui::EndDisabled();
    return changed;
}

static void DrawPlayerCameraSettings(PlayerDefaultsDraft* defaults = nullptr) {
    auto value=defaults ? UserCamera::ReadValues(*defaults) : UserCamera::Get();
    bool changed=false;
    const auto slider=[&](const char* label,UserCamera::Scale setting,float effective,const char* unit) {
        const bool edited=ImGui::SliderFloat(label,&value.scales[setting],
            UserCamera::MinimumScale(setting),UserCamera::MaximumScale(setting),"%.2fx",
            ImGuiSliderFlags_NoInput|(UserCamera::FramingScale(setting)?0:ImGuiSliderFlags_Logarithmic));
        if(defaults && ImGui::IsItemHovered()) ImGui::SetTooltip("1.00x uses the authored camera setting.");
        else if(ImGui::IsItemHovered())
            ImGui::SetTooltip("1.00x uses the default. Current: %.2f %s. Camera limits still apply.",effective,unit);
        return edited;
    };
    changed|=slider("Camera FOV",UserCamera::Fov,Camera::ConfiguredFov(),"degrees");
    ImGui::SameLine();
    if(ImGui::Button("Reset FOV")){value.scales[UserCamera::Fov]=1;changed=true;}
    changed|=slider("Camera distance",UserCamera::Distance,OrbitCamera::ConfiguredDistance(),"x");
    ImGui::SameLine();
    if(ImGui::Button("Reset distance")){value.scales[UserCamera::Distance]=1;changed=true;}
    changed|=slider("Sensitivity",UserCamera::Sensitivity,OrbitCamera::ConfiguredSensitivity(),"x");
    changed|=slider("Acceleration time",UserCamera::Acceleration,OrbitCamera::ConfiguredAcceleration(),"seconds");
    changed|=slider("Deceleration time",UserCamera::Deceleration,OrbitCamera::ConfiguredDeceleration(),"seconds");
    ImGui::Spacing();
    const auto toggle=[&](const char* label,UserCamera::Toggle setting,bool authored) {
        bool enabled=UserCamera::Resolve(value.toggles[setting],authored);
        if(!ImGui::Checkbox(label,&enabled))return false;
        value.toggles[setting]=enabled==authored?-1:int(enabled);
        return true;
    };
    changed|=toggle("Smooth camera pullback",UserCamera::SmoothPullback,defaults ? defaults->value("Camera/smooth_collision_return",true).toBool() : OrbitCamera::smooth_collision_return.load());
    changed|=toggle("Invert horizontal",UserCamera::InvertX,defaults ? defaults->value("Camera/invert_x",false).toBool() : OrbitCamera::invert_x.load());
    changed|=toggle("Invert vertical",UserCamera::InvertY,defaults ? defaults->value("Camera/invert_y",false).toBool() : OrbitCamera::invert_y.load());
    if(ImGui::Button("Reset orbit controls")) {
        UserCamera::ResetOrbit(value);changed=true;
        if(!defaults) OrbitCamera::reset_epoch.fetch_add(1);
    }
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Restore default sensitivity, acceleration, deceleration, pullback and invert controls.");
    if(changed) {
        if(defaults) UserCamera::WriteValues(*defaults,value);
        else UserCamera::Set(value);
    }
}

static void DrawCameraSettings(bool editor) {
    if(!editor) {DrawPlayerCameraSettings();return;}
    float fov = Camera::desired_fov.load();
    if (ImGui::SliderFloat("Camera FOV", &fov, Camera::NativeFov, Camera::MaximumFov,
                           "%.0f degrees", ImGuiSliderFlags_NoInput)) {
        Camera::desired_fov.store(fov);
        Camera::dirty.store(true);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("52 degrees is the native gameplay reference. Orbit and Z-targeting share your selected field of view. Dialogue and scripted zooms remain relative to your setting. HUD size is unchanged.");
    ImGui::SameLine();
    if (ImGui::Button("Reset FOV")) {
        Camera::desired_fov.store(Camera::NativeFov);
        Camera::dirty.store(true);
    }
    float distance=OrbitCamera::distance_scale.load();
    bool distance_changed=ImGui::SliderFloat("Camera distance",&distance,
        OrbitCamera::MinimumDistance,OrbitCamera::MaximumDistance,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Sets orbit distance and protects Link's apparent size during Z-targeting. Targeting keeps native aim and can pull farther back to frame the target. Walls can bring the camera closer.");
    ImGui::SameLine();
    if (ImGui::Button("Reset distance")) {distance=OrbitCamera::DefaultDistance;distance_changed=true;}
    if (distance_changed) {
        OrbitCamera::distance_scale.store(OrbitCamera::SanitizeDistance(distance));
        OrbitCamera::dirty.store(true);
    }
    bool on=OrbitCamera::enabled.load(), ix=OrbitCamera::invert_x.load(), iy=OrbitCamera::invert_y.load();
    float speed=OrbitCamera::sensitivity.load();
    float acceleration=OrbitCamera::acceleration_time.load();
    float deceleration=OrbitCamera::deceleration_time.load();
    bool changed=false;
    changed |= ImGui::SliderFloat("Sensitivity",&speed,.25f,3.f,"%.2fx",ImGuiSliderFlags_NoInput);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Maximum rotation speed at full stick tilt.");
    changed |= ImGui::SliderFloat("Acceleration time",&acceleration,0.f,1.f,"%.2f s",ImGuiSliderFlags_NoInput);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Approximate time to settle near full speed using MM-style smoothing. Reversals respond toward the new direction immediately; 0 responds instantly.");
    changed |= ImGui::SliderFloat("Deceleration time",&deceleration,0.f,1.f,"%.2f s",ImGuiSliderFlags_NoInput);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Approximate time to lose 98% of motion after releasing the stick. Higher values coast longer; 0 stops instantly.");
    ImGui::Spacing();
    if(editor)changed |= ImGui::Checkbox("Enable orbit camera",&on);
    bool smooth_return=OrbitCamera::smooth_collision_return.load();
    if(ImGui::Checkbox("Smooth camera pullback",&smooth_return)) {
        OrbitCamera::smooth_collision_return.store(smooth_return);
        OrbitCamera::dirty.store(true);
    }
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Gradually restore camera distance after a wall brings it close. Turn off to return immediately when space clears.");
    changed |= ImGui::Checkbox("Invert horizontal",&ix);
    changed |= ImGui::Checkbox("Invert vertical",&iy);
    if (ImGui::Button("Reset orbit controls")) {
        speed=1.f; acceleration=OrbitCamera::DefaultAccelerationTime;
        deceleration=OrbitCamera::DefaultDecelerationTime;
        ix=iy=false; changed=true;
        OrbitCamera::reset_epoch.fetch_add(1);
    }
    if(changed) {
        if (on!=OrbitCamera::enabled.load()) OrbitCamera::reset_epoch.fetch_add(1);
        OrbitCamera::enabled.store(on);OrbitCamera::sensitivity.store(speed);
        OrbitCamera::acceleration_time.store(acceleration);
        OrbitCamera::deceleration_time.store(deceleration);
        OrbitCamera::invert_x.store(ix);OrbitCamera::invert_y.store(iy);
        OrbitCamera::dirty.store(true);
    }
    if(editor)ImGui::TextWrapped("Use the configured C-stick to orbit Link. Targeting and scripted scenes use the game's camera.");
}

static void DrawGraphicsMemoryDiagnostics() {
    using namespace GraphicsMemory;
    const auto snapshot=live_diagnostics.Read();
    ImGui::TextUnformatted("Graphics memory");
    ImGui::TextWrapped("Updates once per emulated second. This is the game's graphics pool, not your computer's RAM or VRAM.");
    if(!snapshot.observed) {
        ImGui::TextDisabled("Waiting for the first graphics-memory sample...");
        return;
    }
    const auto& s=snapshot.sample;
    const double age=std::chrono::duration<double>(LiveDiagnostics::Clock::now()-snapshot.captured_at).count();
    ImGui::Text("Last sample: %.1f seconds ago",age);
    if(age>3)ImGui::TextWrapped("Samples are not updating. They resume when the game is running and the frame hook is active.");
    if(!s.free.valid) {
        ImGui::TextWrapped("Graphics heap is not available or could not be validated. Current usage is unknown.");
        return;
    }
    if(s.pressure)
        ImGui::TextColored({1.f,.65f,.2f,1.f},"Memory pressure detected");
    else
        ImGui::TextUnformatted("No memory pressure detected in this sample");
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Pressure: less than 1 MiB free, or largest free block below 64 KiB.\nThis is an early warning, not an allocation failure or a guarantee of sufficient space.");
    const float used=float(s.occupied)/s.free.capacity;
    const auto bar=fmt::format("{:.2f} / {:.2f} MiB occupied ({:.1f}%)",
        double(s.occupied)/MiB,double(s.free.capacity)/MiB,100.f*used);
    ImGui::ProgressBar(used,{-1,0},bar.c_str());
    if(ImGui::BeginTable("Graphics memory values",2,ImGuiTableFlags_SizingStretchProp)) {
        const auto row=[](const char* label,U32 bytes) {
            ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();ImGui::Text("%.3f MiB (%u bytes)",double(bytes)/MiB,bytes);
        };
        row("Pool capacity",s.free.capacity);
        row("Occupied (includes overhead)",s.occupied);
        row("Free memory",s.free.bytes);
        row("Largest free block",s.free.largest);
        row("Sampled peak occupied",s.peak_occupied);
        row("Smallest observed largest block",s.min_largest);
        ImGui::EndTable();
    }
    ImGui::Text("Free blocks: %u",s.free.blocks);
    ImGui::Text("Valid samples: %llu",static_cast<unsigned long long>(s.samples));
    ImGui::Text("Text allocation failures: %llu",static_cast<unsigned long long>(s.failures));
    ImGui::TextWrapped("A texture needs one contiguous block; total free memory alone is not enough. Peaks are sampled and may miss short spikes.");
    ImGui::Separator();
    ImGui::TextUnformatted("Recent history (up to 120 samples)");
    const int offset=snapshot.count==snapshot.occupied_mib.size()?snapshot.next:0;
    const float height=70.f*ImGui::GetIO().FontGlobalScale;
    ImGui::TextUnformatted("Occupied memory (MiB)");
    ImGui::PlotLines("##graphics occupied",snapshot.occupied_mib.data(),snapshot.count,offset,
        nullptr,0,float(s.free.capacity)/MiB,{-1,height});
    ImGui::TextUnformatted("Largest free block (KiB)");
    ImGui::PlotLines("##graphics largest",snapshot.largest_kib.data(),snapshot.count,offset,
        nullptr,0,FLT_MAX,{-1,height});
    ImGui::TextWrapped("Retained resources can grow during initial use. Compare repeated visits to the same menus and areas to see whether usage levels off. Logs continue in azahar_log.txt.");
}

struct Overlay::Impl {
    const Vulkan::Instance& instance;
    ImGuiContext* context = nullptr;
    ImFont* activity_font = nullptr;
    vk::RenderPass renderpass;
    bool backend_ready = false;
    ImGuiStyle base_style;
    int settings_size_step = 0;
    int defaults_size_step = 0;
    bool editing_defaults_size = false;
    int applied_size_step = -100;
    bool editing_settings_size = false;
    ImVec2 window_position{};
    bool has_window_position = false;
    std::unique_ptr<TextureInspector> texture_inspector;
    std::unique_ptr<Randomizer::IconAtlas> randomizer_icons;
    std::unique_ptr<MapCutoutEditor> map_cutout_editor,world_map_editor;
    std::future<bool> actor_export;
    std::string actor_capture_path,actor_capture_status;
    std::future<bool> surface_export;
    std::string surface_capture_path,surface_capture_status;
    std::chrono::steady_clock::time_point previous = std::chrono::steady_clock::now();
    double startup_hint_start=-1;
    bool startup_hint_shown=false;
    explicit Impl(const Vulkan::Instance& instance_) : instance(instance_) {}
    ~Impl() {
        available = false;
        visible = false;
        if (context) {
            ImGui::SetCurrentContext(context);
            world_map_editor.reset();
            randomizer_icons.reset();
            map_cutout_editor.reset();
            texture_inspector.reset();
            if (backend_ready) ImGui_ImplVulkan_Shutdown();
            ImGui::DestroyContext(context);
        }
        instance.GetDevice().destroyRenderPass(renderpass);
        std::scoped_lock lock{input_mutex};
        pointer_events.clear();key_events.clear();text_input=false;
        wheel_x = wheel_y = 0;
    }
};

Overlay::Overlay(const Vulkan::Instance& instance, vk::Format format, unsigned image_count,
                 std::mutex& submit_mutex)
    : impl(std::make_unique<Impl>(instance)) {
    auto& state = *impl;
    const vk::AttachmentDescription attachment{{}, format, vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferSrcOptimal};
    const vk::AttachmentReference reference{0, vk::ImageLayout::eGeneral};
    const vk::SubpassDescription subpass{{}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &reference};
    const std::array dependencies{
        vk::SubpassDependency{VK_SUBPASS_EXTERNAL, 0,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentWrite,
            vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite},
        vk::SubpassDependency{0, VK_SUBPASS_EXTERNAL,
            vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead}};
    state.renderpass = instance.GetDevice().createRenderPass({
        .attachmentCount = 1, .pAttachments = &attachment,
        .subpassCount = 1, .pSubpasses = &subpass,
        .dependencyCount = static_cast<unsigned>(dependencies.size()), .pDependencies = dependencies.data()});
    state.context = ImGui::CreateContext();
    ImGui::SetCurrentContext(state.context);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.Fonts->AddFontDefault();
    state.activity_font = HDFont::AddThinOverlayFont(*io.Fonts);
    ImGui::StyleColorsDark();
    state.base_style = ImGui::GetStyle();
    const auto loader = [](const char* name, void* data) -> PFN_vkVoidFunction {
        return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
            static_cast<const Vulkan::Instance*>(data)->GetInstance(), name);
    };
    if (!ImGui_ImplVulkan_LoadFunctions(Vulkan::TargetVulkanApiVersion, loader,
                                       const_cast<Vulkan::Instance*>(&instance)))
        throw std::runtime_error("OoT3D ImGui Vulkan function loading failed");
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = Vulkan::TargetVulkanApiVersion;
    info.Instance = instance.GetInstance();
    info.PhysicalDevice = instance.GetPhysicalDevice();
    info.Device = instance.GetDevice();
    info.QueueFamily = instance.GetGraphicsQueueFamilyIndex();
    info.Queue = instance.GetGraphicsQueue();
    info.RenderPass = state.renderpass;
    info.MinImageCount = 2;
    info.ImageCount = std::max(2u, image_count);
    info.DescriptorPoolSize = Textures::Limit + 16;
    info.CheckVkResultFn = [](VkResult result) {
        if (result < 0) throw std::runtime_error("OoT3D ImGui Vulkan error");
    };
    if (!ImGui_ImplVulkan_Init(&info)) throw std::runtime_error("OoT3D ImGui initialization failed");
    state.backend_ready = true;
    ImGui_ImplVulkan_CreateFontsTexture();
    state.map_cutout_editor = std::make_unique<MapCutoutEditor>(instance, submit_mutex);
    state.world_map_editor = std::make_unique<MapCutoutEditor>(instance, submit_mutex,true);
    state.texture_inspector = std::make_unique<TextureInspector>(instance, submit_mutex);
    state.randomizer_icons = std::make_unique<Randomizer::IconAtlas>(instance, submit_mutex);
    available = true;
}

Overlay::~Overlay() = default;

void Overlay::Draw(vk::CommandBuffer command, vk::Framebuffer framebuffer,
                   unsigned width, unsigned height) {
    auto& state = *impl;
    ImGui::SetCurrentContext(state.context);
    // Finish even with F1 closed. Serialize on a worker so a completed capture
    // does not stall gameplay while writing its diagnostic file.
    if(auto capture=Interpolation::ActorCapture::Finish()) {
        state.actor_capture_status="Saving actor capture...";
        state.actor_export=std::async(std::launch::async,
            [data=std::move(*capture),path=state.actor_capture_path] {
                try {return Interpolation::ActorCapture::Export(data,path);}catch(...) {return false;}
            });
    }
    if(state.actor_export.valid() && state.actor_export.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        const bool ok=state.actor_export.get();
        state.actor_capture_status=(ok?"Saved actor capture: ":"Failed to save actor capture: ")+state.actor_capture_path;
        LOG_INFO(Render_Vulkan,"{}",state.actor_capture_status);
    }
    if(auto capture=Interpolation::SurfaceCapture::Finish()) {
        state.surface_capture_status="Saving animated-surface capture...";
        state.surface_export=std::async(std::launch::async,
            [data=std::move(*capture),path=state.surface_capture_path] {
                try {return Interpolation::SurfaceCapture::Export(data,path);}catch(...) {return false;}
            });
    }
    if(state.surface_export.valid() && state.surface_export.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        const bool ok=state.surface_export.get();
        state.surface_capture_status=(ok?"Saved animated-surface capture: ":"Failed to save animated-surface capture: ")+state.surface_capture_path;
        LOG_INFO(Render_Vulkan,"{}",state.surface_capture_status);
    }
    auto& io = ImGui::GetIO();
    const auto now = std::chrono::steady_clock::now();
    io.DeltaTime = std::clamp(std::chrono::duration<float>(now - state.previous).count(), .00001f, .25f);
    state.previous = now;
    io.DisplaySize = {static_cast<float>(width), static_cast<float>(height)};
    const int size_step = settings_size.load();
    const float user_scale = 1.f + size_step * .1f;
    const float ui_scale = std::clamp(static_cast<float>(height) / 720.f, 1.f, 3.f) * user_scale;
    io.FontGlobalScale = ui_scale;
    ImGui::GetStyle() = state.base_style;
    ImGui::GetStyle().ScaleAllSizes(ui_scale);
    {
        std::scoped_lock lock{input_mutex};
        for (const auto& event : pointer_events) {
            io.AddMousePosEvent(event.x, event.y);
            if (event.button >= 0) io.AddMouseButtonEvent(event.button, event.down);
        }
        pointer_events.clear();
        for(const auto& event:key_events) {
            if(event.key!=ImGuiKey_None)io.AddKeyEvent(ImGuiKey(event.key),event.down);
            io.AddKeyEvent(ImGuiMod_Ctrl,event.ctrl);io.AddKeyEvent(ImGuiMod_Shift,event.shift);
            io.AddKeyEvent(ImGuiMod_Alt,event.alt);io.AddKeyEvent(ImGuiMod_Super,event.super);
            if(!event.text.empty())io.AddInputCharactersUTF8(event.text.c_str());
        }
        key_events.clear();
        if (wheel_x != 0 || wheel_y != 0) io.AddMouseWheelEvent(wheel_x, wheel_y);
        wheel_x = wheel_y = 0;
    }
    if (!visible.load()) {
        io.ClearInputKeys();
        io.AddMouseButtonEvent(0, false);
        io.AddMouseButtonEvent(1, false);
    }
    WorldMap::Warmup();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

#ifndef __ANDROID__
    const double seconds=std::chrono::duration<double>(now.time_since_epoch()).count();
    if(StartupHint::requested.exchange(false) && !state.startup_hint_shown) {
        state.startup_hint_shown=true;
        if(StartupHint::enabled.load())state.startup_hint_start=seconds;
    }
    if(!StartupHint::enabled.load())state.startup_hint_start=-1;
    if(state.startup_hint_start>=0) {
        // Match DekUI: two seconds at full opacity, then a two-second fade.
        const float fade=std::clamp(float((seconds-state.startup_hint_start-2)/2),0.f,1.f);
        const float alpha=1-fade*fade*(3-2*fade);
        if(alpha>0) {
            const auto label=UILanguage::Pick("Press F1 to access settings",
                "Appuyez sur F1 pour ouvrir les paramètres","Pulsa F1 para abrir los ajustes");
            const float scale=std::clamp(float(height)/720.f,.8f,3.f);
            const float font_size=18*scale,margin=16*scale,padding=8*scale;
            const auto size=state.activity_font->CalcTextSizeA(font_size,FLT_MAX,0,label.data());
            const ImVec2 pos{margin+padding,float(height)-margin-padding-size.y};
            auto* draw=ImGui::GetForegroundDrawList();
            draw->AddRectFilled({pos.x-padding,pos.y-padding},
                {pos.x+size.x+padding,pos.y+size.y+padding},IM_COL32(0,0,0,int(145*alpha)),5*scale);
            draw->AddText(state.activity_font,font_size,pos,IM_COL32(255,255,255,int(255*alpha)),label.data());
        } else state.startup_hint_start=-1;
    }
#endif

    {
        const auto messages=Randomizer::Activity::Visible();
        const float scale=float(height)/720.f;
        const float font_size=18.f*scale;
        const float x=18.f*scale;
        float y=height*.46f;
        auto* draw=ImGui::GetForegroundDrawList();
        for(const auto& line:messages) {
            if(!Randomizer::Activity::enabled.load() && !line.local)continue;
            const float age=std::chrono::duration<float>(now-line.born).count();
            const float alpha=std::clamp(age/.15f,0.f,1.f)*std::clamp((6-age)/1.f,0.f,1.f);
            const float wrap_width=width*.45f;
            const auto size=state.activity_font->CalcTextSizeA(font_size,FLT_MAX,wrap_width,line.text.c_str());
            draw->AddRectFilled({x-7*scale,y-4*scale},{x+size.x+7*scale,y+size.y+4*scale},
                IM_COL32(0,0,0,int(130*alpha)),4*scale);
            draw->AddText(state.activity_font,font_size,{x+scale,y+scale},IM_COL32(0,0,0,int(230*alpha)),line.text.c_str(),nullptr,wrap_width);
            draw->AddText(state.activity_font,font_size,{x,y},IM_COL32(255,255,255,int(255*alpha)),line.text.c_str(),nullptr,wrap_width);
            y+=size.y+10*scale;
        }
    }
    if (visible.load()) {
        const bool resize = state.applied_size_step != size_step;
        const ImVec2 size{std::min(width * .55f * user_scale, float(width)),
                          std::min(height * .65f * user_scale, float(height))};
        ImGui::SetNextWindowSize(size, resize ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            {std::min(300.f * ui_scale, float(width)), std::min(200.f * ui_scale, float(height))},
            {float(width), float(height)});
        state.applied_size_step = size_step;
        if (resize) {
            const auto position = state.has_window_position ? state.window_position :
                ImVec2{width * .225f, height * .175f};
            ImGui::SetNextWindowPos({std::clamp(position.x, 0.f, float(width) - size.x),
                                     std::clamp(position.y, 0.f, float(height) - size.y)});
        }
        bool open = true;
        PlayerDefaultsDraft* defaults = nullptr;
#ifndef ENABLE_OOT3D_PLAYER_MODE
        const auto view=settings_view.load();
        if (view==SettingsView::Defaults) {
            static std::unique_ptr<PlayerDefaultsDraft> draft;
            static unsigned revision=0;
            const auto current=defaults_revision.load();
            if(!draft || revision!=current) {
                draft=std::make_unique<PlayerDefaultsDraft>(QString::fromUtf8(PlayerDefaultsDirectory()));
                revision=current;
            }
            defaults=draft.get();
        }
#endif
#ifdef ENABLE_OOT3D_PLAYER_MODE
        constexpr bool player=true;
        const bool expanded = ImGui::Begin("SariaUI Settings (F1)", &open);
#else
        const bool player=view!=SettingsView::Editor;
        const bool expanded = ImGui::Begin(defaults ? "Player defaults (F3)" : player ? "SariaUI Settings (F2)" : "OoT3D Editor (F1)", &open);
#endif
        state.window_position = ImGui::GetWindowPos();
        state.has_window_position = true;
        if (expanded && defaults) {
            ImGui::TextWrapped(defaults->Writable() ? "Changes are saved for new player profiles in the next build." :
                "Cannot save player defaults. Check write access to src/oot3d/defaults/player.ini.");
        }
        if (expanded && (!defaults || defaults->Writable())) {
            if (ImGui::BeginTabBar(player ? "Player settings tabs" : "Settings tabs")) {
                if (ImGui::BeginTabItem("General")) {
                    int language=-1;
                    const int selected_language = defaults
                        ? defaults->value("General/initial_game_language", 1).toInt()
                        : GameLanguage::game_language.load();
                    for (unsigned i = 0; i < GameLanguage::Languages.size(); ++i) {
                        if (selected_language == GameLanguage::Languages[i]) {
                            language = int(i);
                            break;
                        }
                    }
                    if(ImGui::Combo("Game language",&language,"English\0Français\0Español\0")) {
                        if(defaults) defaults->setValue("General/initial_game_language",GameLanguage::Languages[language]);
                        else {
                            GameLanguage::game_language=GameLanguage::Languages[language];
                            GameLanguage::game_language_status=1;
                            GameLanguage::set_game_language=GameLanguage::Languages[language];
                        }
                    }
                    if(ImGui::IsItemHovered())
                        ImGui::SetTooltip("Restart the game to apply a language change.\nSettings menus remain in English.");
                    if(!defaults && GameLanguage::game_language_status.load()==3)
                        ImGui::TextWrapped("Could not save the language. Please select it again.");
                    ImGui::Separator();
                    auto& editing_size=defaults ? state.editing_defaults_size : state.editing_settings_size;
                    auto& size_value=defaults ? state.defaults_size_step : state.settings_size_step;
                    if (!editing_size) size_value = defaults ? defaults->value("General/settings_size",0).toInt() : size_step;
                    ImGui::SliderInt("Settings size", &size_value, -5, 10, "%+d",
                                     ImGuiSliderFlags_NoInput);
                    editing_size = ImGui::IsItemActive();
                    bool apply = ImGui::IsItemDeactivatedAfterEdit();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("0 is the default size. Each step changes the size by one tenth. Release to apply.");
                    ImGui::SameLine();
                    if (ImGui::Button("Reset size")) { size_value = 0; apply = true; }
                    if (apply && defaults) defaults->setValue("General/settings_size",size_value);
                    else if (apply) {
                        settings_size.store(size_value);
                        settings_size_dirty.store(true);
                    }
                    ImGui::Separator();
                    bool startup_hint=defaults ? defaults->value("General/show_startup_settings_hint",true).toBool() : StartupHint::enabled.load();
                    if(ImGui::Checkbox("Show F1 settings hint",&startup_hint)) {
                        if(defaults) defaults->setValue("General/show_startup_settings_hint",startup_hint);
                        else {StartupHint::enabled=startup_hint;StartupHint::dirty=true;}
                    }
                    ImGui::Separator();
                    if(!defaults && ImGui::Button("Quit game"))quit_requested=true;
                    ImGui::EndTabItem();
                }
                if(!player) {
                    const auto multiplayer_flags=MultiplayerSettings::select_tab.exchange(false)?ImGuiTabItemFlags_SetSelected:ImGuiTabItemFlags_None;
                    if(ImGui::BeginTabItem("Multiplayer",nullptr,multiplayer_flags)) {
                        MultiplayerSettings::Draw();
                        ImGui::EndTabItem();
                    }
                }
                if (ImGui::BeginTabItem("Graphics")) {
                    if(player) {
                        using namespace PlayerGraphics;
                        ImGui::SeparatorText("Frame rate");
                        int mode=int(CurrentMode());
                        if(defaults) {
                            const bool enabled=defaults->value("Interpolation/enabled",true).toBool();
                            const bool cpu=defaults->value("Interpolation/cpu_vertices",true).toBool();
                            mode=int(!enabled ? Mode::Original : cpu ? Mode::CPU : Mode::GPU);
                        }
                        bool changed=ImGui::RadioButton("Original (30 FPS)",&mode,0);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Original gameplay frame rate. Native 60 FPS menus keep their normal speed.");
                        changed|=ImGui::RadioButton("CPU interpolation",&mode,1);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Runs vertex processing on the CPU. Can reduce shader-compilation hitches, but increases CPU work.");
                        changed|=ImGui::RadioButton("GPU interpolation",&mode,2);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Uses hardware vertex shaders where supported. Lower CPU cost; new shaders can cause compilation hitches.");
                        if(changed && defaults) {
                            defaults->setValue("Interpolation/enabled",mode!=int(Mode::Original));
                            if(mode!=int(Mode::Original)) {
                                defaults->setValue("Interpolation/cpu_vertices",mode==int(Mode::CPU));
                                defaults->setValue("Renderer/use_hw_shader",mode==int(Mode::GPU));
                                defaults->setValue("Renderer/use_vsync",true);
                            }
                        } else if(changed)SelectMode(static_cast<Mode>(mode));
                        ImGui::BeginDisabled(mode==int(Mode::Original));
                        bool match=defaults ? defaults->value("Interpolation/match_refresh_rate",false).toBool() : Interpolation::match_refresh_rate.load();
                        if(ImGui::Checkbox("Match display rate",&match)) {
                            if(defaults) defaults->setValue("Interpolation/match_refresh_rate",match);
                            else {
                                Interpolation::match_refresh_rate=match;
                                ++Interpolation::generation;
                                Interpolation::settings_dirty=true;
                            }
                        }
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Follows the game window's display refresh rate, up to 120 FPS.");
                        int fps=defaults ? defaults->value("Interpolation/target_fps",60).toInt() : Interpolation::target_fps.load();
                        ImGui::BeginDisabled(match);
                        if(ImGui::SliderInt("Target FPS",&fps,30,120,"%d FPS",ImGuiSliderFlags_AlwaysClamp)) {
                            if(defaults) defaults->setValue("Interpolation/target_fps",fps);
                            else {
                                Interpolation::target_fps=fps;
                                ++Interpolation::generation;
                                Interpolation::settings_dirty=true;
                            }
                        }
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Higher rates increase rendering work without changing gameplay speed. Disable Match display rate to choose a rate manually. VSync limits output to the display rate.");
                        ImGui::EndDisabled();
                        ImGui::EndDisabled();
                        ImGui::SeparatorText("Rendering");
                        bool sync=vsync.load(),disk=cache.load(),hw=hardware.load(),async=async_shaders.load();
                        if(defaults) {
                            sync=defaults->value("Renderer/use_vsync",true).toBool();
                            disk=defaults->value("Renderer/use_disk_shader_cache",true).toBool();
                            hw=defaults->value("Renderer/use_hw_shader",false).toBool();
                            async=defaults->value("Renderer/async_shader_compilation",false).toBool();
                        }
                        if(ImGui::Checkbox("VSync",&sync)) {
                            if(defaults) defaults->setValue("Renderer/use_vsync",sync); else set_vsync=sync;
                        }
                        if(ImGui::Checkbox("Remember compiled shaders",&disk)) {
                            if(defaults) defaults->setValue("Renderer/use_disk_shader_cache",disk); else set_cache=disk;
                        }
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Keeps compiled shaders between sessions. Cache changes apply fully on the next launch.");
                        ImGui::BeginDisabled(mode!=int(Mode::Original));
                        if(ImGui::Checkbox("Hardware shaders",&hw)) {
                            if(defaults) defaults->setValue("Renderer/use_hw_shader",hw); else set_hardware=hw;
                        }
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Runs vertex shaders on the GPU. During interpolation, the CPU/GPU choice above controls this option.");
                        ImGui::EndDisabled();
                        if(ImGui::Checkbox("Async shader compilation",&async)) {
                            if(defaults) defaults->setValue("Renderer/async_shader_compilation",async); else set_async=async;
                        }
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Applies next launch. Compiles shaders in the background; graphics may briefly be missing while shaders compile.");
                        bool fog=defaults ? defaults->value("Graphics/disable_fog",false).toBool() : disable_fog.load();
                        if(ImGui::Checkbox("Disable fog",&fog)) {
                            if(defaults) defaults->setValue("Graphics/disable_fog",fog);
                            else {
                                disable_fog=fog;
                                fog_dirty=true;
                            }
                        }
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Removes distance fog. As fog is used for atmospheric effect, it is not recommended to disable it.");
                    }
                    bool fxaa=defaults ? defaults->value("Graphics/fxaa",false).toBool() : PlayerGraphics::fxaa.load();
                    if(ImGui::Checkbox("Anti-aliasing (FXAA)",&fxaa)) {
                        if(defaults) defaults->setValue("Graphics/fxaa",fxaa);
                        else {
                            PlayerGraphics::fxaa=fxaa;
                            PlayerGraphics::fxaa_dirty=true;
                        }
                    }
                    if(ImGui::IsItemHovered())
                        ImGui::SetTooltip("Smooths jagged edges in the 3D scene. HUD and text stay sharp.");
                    bool bloom=defaults ? defaults->value("Graphics/bloom",false).toBool() : PlayerGraphics::bloom.load();
                    if(ImGui::Checkbox("Bloom",&bloom)) {
                        if(defaults) defaults->setValue("Graphics/bloom",bloom);
                        else {
                            PlayerGraphics::bloom=bloom;
                            PlayerGraphics::bloom_dirty=true;
                        }
                    }
                    if(ImGui::IsItemHovered())
                        ImGui::SetTooltip("Adds a soft glow around bright highlights in the game view.");
                    if((!player || defaults) && bloom) {
                        int intensity=defaults ? defaults->value("Graphics/bloom_intensity",55).toInt() : PlayerGraphics::bloom_intensity.load();
                        if(ImGui::SliderInt("Bloom intensity",&intensity,0,100,"%d%%",ImGuiSliderFlags_AlwaysClamp)) {
                            if(defaults) defaults->setValue("Graphics/bloom_intensity",intensity);
                            else {
                                PlayerGraphics::bloom_intensity=intensity;
                                PlayerGraphics::bloom_dirty=true;
                            }
                        }
                    }
                    bool textures=defaults ? defaults->value("Utility/custom_textures",false).toBool() : custom_textures.load();
                    if(ImGui::Checkbox("Use custom textures",&textures)) { if(defaults) defaults->setValue("Utility/custom_textures",textures); else requested_custom_textures=textures; }
                    if(ImGui::IsItemHovered())
                        ImGui::SetTooltip("Switch your installed texture pack on or off immediately.\nRestart the game after adding or replacing texture files.");
                    if(!defaults && ImGui::Button("Open texture folder"))open_texture_folder=true;
                    if(!defaults && texture_folder_failed.load())
                        ImGui::TextColored({1.f,.3f,.3f,1.f},"Could not open the texture folder.");
                    if(!defaults && ImGui::Button("Toggle fullscreen"))fullscreen_requested=true;
                    float draw_distance=defaults ? defaults->value("Graphics/actor_draw_distance",3.75f).toFloat() : ActorDrawDistance::multiplier.load();
                    bool distance_changed=ImGui::SliderFloat("Actor draw distance",&draw_distance,
                        ActorDrawDistance::Minimum,ActorDrawDistance::Maximum,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                    if(ImGui::IsItemHovered())
                        ImGui::SetTooltip("Extends visibility for actors such as signs, NPCs, gates and bushes,\nincluding the spawn/despawn distance of bush and rock groups.\n1x is original. Applies immediately; higher values can reduce performance.\nUnloaded rooms, fog and objects with separate visibility rules still apply.");
                    ImGui::SameLine();
                    if(ImGui::Button("Reset##actor_draw_distance")) {draw_distance=ActorDrawDistance::Default;distance_changed=true;}
                    if(distance_changed) {
                        if(defaults) defaults->setValue("Graphics/actor_draw_distance",draw_distance);
                        else {
                            ActorDrawDistance::multiplier=ActorDrawDistance::Clamp(draw_distance);
                            ActorDrawDistance::dirty=true;
                        }
                    }
                    int font_mode = defaults ? defaults->value("Graphics/font_mode",1).toInt() : HDFont::CurrentMode();
                    if (ImGui::Combo("Font", &font_mode, "Original\0HD Thin\0HD Heavy\0")) {
                        if(defaults) defaults->setValue("Graphics/font_mode",font_mode);
                        else {
                            HDFont::SetMode(font_mode);
                            HDFont::dirty.store(true);
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Original uses the game's lettering. HD Thin and HD Heavy use\nour redrawn lettering at 4x resolution, including the keyboard\nand custom button prompts. Spacing, colors and animations stay the same.\nBoth HD styles include HD date/time lettering.");
                    if (!player && font_mode!=HDFont::Original)
                        ImGui::TextDisabled("Font uploads: %u | HD draws: %u | Glyphs redrawn: %u",
                            HDFont::registrations.load(), HDFont::replacements.load(),
                            HDFont::redrawn_glyphs.load());
                    if(!player) {
                        bool wide = Widescreen::enabled.load();
                        if (ImGui::Checkbox("16:9 widescreen", &wide)) {
                            Widescreen::enabled.store(wide);
                            Widescreen::dirty.store(true);
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Shows more of the world while preserving HUD proportions.");
                        if (wide && !Widescreen::supported_layout.load())
                            ImGui::TextWrapped("Use landscape screens with stereoscopic 3D off for widescreen.");
                    }
                    int factor = defaults ? defaults->value("Renderer/resolution_factor",3).toInt() : resolution.load();
                    if (ImGui::SliderInt("Resolution", &factor, 1, 9, "%dx",
                                         ImGuiSliderFlags_NoInput)) {
                        if(defaults) defaults->setValue("Renderer/resolution_factor",factor);
                        else {
                            requested_resolution.store(factor);
                        }
                    }
                    ImGui::EndTabItem();
                }
                if (player && ImGui::BeginTabItem("Camera")) {
                    DrawPlayerCameraSettings(defaults);
                    ImGui::EndTabItem();
                }
                if (player && !defaults && ImGui::BeginTabItem("HUD")) {
                    auto layout=UserHUD::GetLayout();
                    const auto authored=HUD::GetLayout();
                    bool changed=false;
                    int hd_override=UserHUD::hd_numbers_override.load();
                    bool hd_numbers=UserHUD::HDNumbers(authored.hd_numbers);
                    if(ImGui::Checkbox("Enable HD Numbers",&hd_numbers)) {
                        hd_override=hd_numbers?1:0;changed=true;
                    }
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Changes your HUD only. Reset all HUD elements restores the default.");
                    changed|=DrawOverallHUDScale(layout.overall_scale);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s","100% uses the default HUD size. Combines with your individual element and button scales.");
                    changed|=DrawHUDOpacity("Overall HUD opacity",layout.overall_opacity);
                    ImGui::Separator();
                    if(ImGui::BeginTabBar("User HUD options")) {
                        if(ImGui::BeginTabItem("Elements")) {
                            ImGui::TextWrapped("Zero uses the default layout. X and Y move each element; scale changes its size by a percentage.");
                            const auto adjustment=[&](UserHUD::Element i) {
                                auto& p=layout[i];
                                changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Scale",&p.scale,.1f,-90,200,"%.2f%%",ImGuiSliderFlags_AlwaysClamp);
                                if(i==UserHUD::CounterGroup) {
                                    const float base=authored.counter_distance;
                                    changed|=ImGui::SliderFloat("Counter spacing offset",&layout.counter_distance_offset,
                                        -base,UserHUD::Layout::MaxCounterDistance-base,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Zero uses the default gap. Negative brings the counters closer; positive moves them apart.");
                                }
                                if(i==UserHUD::Minimap)changed|=DrawHUDOpacity("Minimap opacity",layout.minimap_opacity);
                                if(i==UserHUD::Hearts) {
                                    changed|=ImGui::SliderFloat("Heart spacing offset",&layout.heart_spacing,
                                        -8.f-authored.heart_spacing,20.f-authored.heart_spacing,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Zero uses the default spacing. Negative brings hearts closer together. Positive moves them apart.");
                                }
                                if(i==UserHUD::Magic) {
                                    float height=layout.magic_height==0 ? authored.magic_height : layout.magic_height;
                                    float border=layout.magic_outer_thickness;
                                    if(border==0)border=(authored.magic_contour.width+authored.magic_contour.height)*.5f;
                                    if(DrawMagicBorderThickness(border)) {layout.magic_outer_thickness=border;changed=true;}
                                    if(ImGui::SliderFloat("Height",&height,1,8,"%.2fx",ImGuiSliderFlags_AlwaysClamp)) {layout.magic_height=height;changed=true;}
                                    changed|=ImGui::SliderFloat("Width",&layout.magic_width,.5f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                }
                                if(ImGui::Button("Reset")) {
                                    p={};changed=true;
                                    if(i==UserHUD::CounterGroup) {layout[UserHUD::Keys]={};layout[UserHUD::Rupees]={};layout.counter_distance_offset=0;}
                                    if(i==UserHUD::Hearts)layout.heart_spacing=0;
                                    if(i==UserHUD::Minimap)layout.minimap_opacity=1;
                                    if(i==UserHUD::Magic) {layout.magic_height=layout.magic_outer_thickness=0;layout.magic_width=1;}
                                }
                            };
                            if(ImGui::CollapsingHeader("Buttons")) {
                                ImGui::PushID("Buttons");
                                changed|=ImGui::DragFloat("Group X",&layout.button_x,.1f,-400,400,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Group Y",&layout.button_y,.1f,-240,240,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::SliderFloat("Group scale",&layout.button_scale,-90,200,"%.0f%%",ImGuiSliderFlags_AlwaysClamp);
                                for(const auto i:UserHUD::ButtonElements) {
                                    ImGui::PushID(static_cast<int>(i));
                                    if(ImGui::TreeNode(UserHUD::Labels[i])) {adjustment(i);ImGui::TreePop();}
                                    ImGui::PopID();
                                }
                                if(ImGui::Button("Reset all buttons")) {
                                    for(const auto i:UserHUD::ButtonElements)layout[i]={};
                                    layout[UserHUD::ItemButtons]={};layout.button_x=layout.button_y=layout.button_scale=0;changed=true;
                                }
                                ImGui::PopID();
                            }
                            for(const auto i:UserHUD::EditableElements) {
                                ImGui::PushID(static_cast<int>(i));
                                if(ImGui::CollapsingHeader(UserHUD::Labels[i])) {
                                    adjustment(i);
                                }
                                ImGui::PopID();
                            }
                            ImGui::EndTabItem();
                        }
                        if(ImGui::BeginTabItem("Padding")) {
                            changed|=DrawHUDPadding(layout);
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    ImGui::Separator();
                    if(ImGui::Button("Reset all HUD elements")) {layout={};changed=true;hd_override=-1;}
                    if(changed) {
                        UserHUD::hd_numbers_override=hd_override;
                        UserHUD::SetLayout(layout);
                    }
                    if(ImGui::Button("Import HUD settings..."))hud_preset_request=HUDPresetAction::Import;
                    ImGui::SameLine();
                    if(ImGui::Button("Export HUD settings..."))hud_preset_request=HUDPresetAction::Export;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Difficulty")) {
                    if (!defaults && Difficulty::file_rules.load()) ImGui::TextWrapped("Difficulty is locked to the settings saved with this randomized file.");
                    ImGui::BeginDisabled(!defaults && Difficulty::file_rules.load());
                    int multiplier=Difficulty::Multiplier(defaults ? defaults->value("Difficulty/damage_multiplier",1).toInt() : Difficulty::damage_multiplier.load());
                    bool hearts=defaults ? defaults->value("Difficulty/disable_hearts",false).toBool() : Difficulty::disable_hearts.load();
                    bool fairies=defaults ? defaults->value("Difficulty/disable_fairies",false).toBool() : Difficulty::disable_fairies.load();
                    bool rupees=defaults ? defaults->value("Difficulty/disable_rupees",false).toBool() : Difficulty::disable_rupees.load();
                    bool defense=defaults ? defaults->value("Difficulty/weaker_defense",false).toBool() : Difficulty::weaker_defense.load();
                    bool magic=defaults ? defaults->value("Difficulty/disable_magic",false).toBool() : Difficulty::disable_magic.load();
                    bool ammo=defaults ? defaults->value("Difficulty/disable_ammo",false).toBool() : Difficulty::disable_ammo.load();
                    bool fairy_drops=defaults ? defaults->value("Difficulty/disable_fairy_drops",false).toBool() : Difficulty::disable_fairy_drops.load();
                    bool lose_rupees=defaults ? defaults->value("Difficulty/lose_rupees_on_death",false).toBool() : Difficulty::lose_rupees_on_death.load();
                    bool prices=defaults ? defaults->value("Difficulty/increase_prices",false).toBool() : Difficulty::increase_prices.load();
                    int red=defaults ? defaults->value("Difficulty/red_potion_hearts",20).toInt() : Difficulty::red_potion_hearts.load();
                    int blue=defaults ? defaults->value("Difficulty/blue_potion_hearts",20).toInt() : Difficulty::blue_potion_hearts.load();
                    int fairy_hearts=defaults ? defaults->value("Difficulty/bottled_fairy_hearts",20).toInt() : Difficulty::bottled_fairy_hearts.load();
                    int price=defaults ? defaults->value("Difficulty/price_multiplier",2).toInt() : Difficulty::price_multiplier.load();
                    bool changed=ImGui::SliderInt("Damage multiplier",&multiplier,1,16,"%dx",ImGuiSliderFlags_AlwaysClamp);
                    constexpr const char* damage_help="Multiplies damage taken after defense adjustments in both quests. Replaces Master Quest's built-in double damage. Healing is unchanged.";
                    if(player) {if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",damage_help);}
                    else ImGui::TextWrapped("%s",damage_help);
                    changed|=ImGui::SliderInt("Red potion healing",&red,1,20,"%d hearts",ImGuiSliderFlags_AlwaysClamp);
                    changed|=ImGui::SliderInt("Blue potion healing",&blue,1,20,"%d hearts",ImGuiSliderFlags_AlwaysClamp);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Hearts restored, up to your maximum health. Blue potions still refill magic.");
                    changed|=ImGui::SliderInt("Bottled fairy healing",&fairy_hearts,1,20,"%d hearts",ImGuiSliderFlags_AlwaysClamp);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Hearts restored when a bottled fairy is released or revives you. Wild fairy healing is unchanged.");
                    changed|=ImGui::Checkbox("Weaker enhanced defense",&defense);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Once earned, enhanced defense reduces damage by one-third instead of one-half. Combines with the damage multiplier.");
                    changed|=ImGui::Checkbox("Disable heart drops",&hearts);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Removes dropped recovery hearts. Heart pieces and containers remain available.");
                    changed|=ImGui::Checkbox("Disable fairies",&fairies);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Disables collectible healing fairies, bottled-fairy healing and automatic revival. Navi and Great Fairies remain available.");
                    changed|=ImGui::Checkbox("Disable fairy drops",&fairy_drops);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Removes loose healing fairies. Bottled fairies, companions and Great Fairies remain available.");
                    changed|=ImGui::Checkbox("Disable rupee drops",&rupees);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Removes dropped green, blue, red, purple and orange rupees. Chest rewards and silver-rupee puzzles remain available.");
                    changed|=ImGui::Checkbox("Disable magic drops",&magic);
                    changed|=ImGui::Checkbox("Disable ammo pickups",&ammo);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Removes loose bombs, arrows, Deku Seeds, sticks and nuts. Purchases and chest rewards remain available.");
                    changed|=ImGui::Checkbox("Lose rupees on death",&lose_rupees);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Lose all carried rupees on death. Successful fairy revivals and pit falls are unaffected.");
                    constexpr const char* price_help="Purchase prices and fees cost at most 500 rupees. Ordinary files require a restart after changing prices. Randomized files keep their own prices without restarting.";
                    changed|=ImGui::Checkbox("Increase purchase prices and fees",&prices);
                    if(player && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",price_help);
                    ImGui::BeginDisabled(!prices);
                    changed|=ImGui::SliderInt("Price multiplier",&price,2,3,"%dx",ImGuiSliderFlags_AlwaysClamp);
                    if(player && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",price_help);
                    ImGui::EndDisabled();
                    if(!player)ImGui::TextWrapped("%s",price_help);
                    if(!defaults && Difficulty::active_price_multiplier.load()>1 && !Difficulty::price_text_ready.load())
                        ImGui::TextWrapped("Increased prices are inactive until supported English/French/Spanish game text is loaded.");
                    if(ImGui::Button("Reset difficulty")) {
                        multiplier=1;red=blue=fairy_hearts=20;price=2;
                        hearts=fairies=rupees=defense=magic=ammo=fairy_drops=lose_rupees=prices=false;changed=true;
                    }
                    if(changed && defaults) {
                        defaults->setValue("Difficulty/disable_hearts",hearts);
                        defaults->setValue("Difficulty/disable_fairies",fairies);
                        defaults->setValue("Difficulty/disable_rupees",rupees);
                        defaults->setValue("Difficulty/weaker_defense",defense);
                        defaults->setValue("Difficulty/disable_magic",magic);
                        defaults->setValue("Difficulty/disable_ammo",ammo);
                        defaults->setValue("Difficulty/disable_fairy_drops",fairy_drops);
                        defaults->setValue("Difficulty/lose_rupees_on_death",lose_rupees);
                        defaults->setValue("Difficulty/increase_prices",prices);
                        defaults->setValue("Difficulty/damage_multiplier",multiplier);
                        defaults->setValue("Difficulty/red_potion_hearts",red);
                        defaults->setValue("Difficulty/blue_potion_hearts",blue);
                        defaults->setValue("Difficulty/bottled_fairy_hearts",fairy_hearts);
                        defaults->setValue("Difficulty/price_multiplier",price);
                    } else if(changed) {
                        Difficulty::damage_multiplier.store(Difficulty::Multiplier(multiplier));
                        Difficulty::disable_hearts.store(hearts);Difficulty::disable_fairies.store(fairies);
                        Difficulty::disable_rupees.store(rupees);
                        Difficulty::weaker_defense=defense;Difficulty::disable_magic=magic;Difficulty::disable_ammo=ammo;
                        Difficulty::disable_fairy_drops=fairy_drops;Difficulty::lose_rupees_on_death=lose_rupees;
                        Difficulty::red_potion_hearts=red;Difficulty::blue_potion_hearts=blue;Difficulty::bottled_fairy_hearts=fairy_hearts;
                        Difficulty::increase_prices=prices;Difficulty::price_multiplier=price;
                        Difficulty::dirty.store(true);
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("QOL")) {
                    bool navi=defaults ? defaults->value("QOL/disable_navi",false).toBool() : QOL::disable_navi.load();
                    bool beep=defaults ? defaults->value("QOL/disable_heart_beep",false).toBool() : QOL::disable_heart_beep.load();
                    bool ladder=defaults ? defaults->value("QOL/fast_ladder",false).toBool() : QOL::fast_ladder.load();
                    bool loading=defaults ? defaults->value("QOL/fast_loading",true).toBool() : QOL::fast_loading.load();
                    bool bombchu=defaults ? defaults->value("QOL/no_bombchu_first_person",false).toBool() : QOL::no_bombchu_first_person.load();
                    bool hide_stones=QOL::hide_sheikah_stones.load();
                    bool changed=false;
                    changed|=ImGui::Checkbox(UILanguage::Pick("Faster loading###fast_loading",
                        "Chargements rapides###fast_loading","Carga rápida###fast_loading").data(),&loading);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",UILanguage::Pick(
                        "Shortens loading pauses between areas. Fade speed is unchanged.",
                        "Réduit l’attente entre les zones sans modifier la vitesse des fondus.",
                        "Reduce la espera entre zonas sin cambiar la velocidad de los fundidos.").data());
                    changed|=ImGui::Checkbox("Disable Navi",&navi);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Disables Navi's unsolicited reminders. Story dialogue and targeting remain available.");
                    changed|=ImGui::Checkbox("Disable heart beep",&beep);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Mutes only the repeating low-health alarm; other sound effects remain audible.");
                    changed|=ImGui::Checkbox("Rapid B climbing boost",&ladder);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("MM3D climbing: each B press gives a brief 2x speed burst. Keep tapping while climbing ladders, vines or walls. Holding B does not sustain the boost.");
                    changed|=ImGui::Checkbox("Stay in third person after using a Bombchu",&bombchu);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Stops the automatic first-person view after releasing a Bombchu. You can still look manually.");
                    if(!player) {
                        changed|=ImGui::Checkbox("Hide tutorial Sheikah Stones",&hide_stones);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Editor preview for this session. Tutorial stones are always hidden on startup; ordinary Gossip Stones are unaffected. Re-enter the area to apply.");
                    }
                    if(ImGui::Button("Reset QOL options")){navi=beep=ladder=bombchu=false;loading=hide_stones=true;changed=true;if(!defaults)QOL::Reset();}
                    if(changed && defaults) {
                        defaults->setValue("QOL/disable_navi",navi);
                        defaults->setValue("QOL/disable_heart_beep",beep);
                        defaults->setValue("QOL/fast_ladder",ladder);
                        defaults->setValue("QOL/fast_loading",loading);
                        defaults->setValue("QOL/no_bombchu_first_person",bombchu);
                    } else if(changed){
                        QOL::disable_navi.store(navi);QOL::disable_heart_beep.store(beep);QOL::fast_ladder.store(ladder);
                        QOL::no_bombchu_first_person.store(bombchu);
                        QOL::fast_loading.store(loading);
                        QOL::hide_sheikah_stones.store(hide_stones);
                        if(!ladder)QOL::Reset();
                        QOL::dirty.store(true);
                    }

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Audio")) {
                    if(!player) {
                        bool accurate=AudioCore::HLE::accurate_audio.load();
                        if(ImGui::Checkbox("Accurate HLE audio",&accurate)) {
                            AudioCore::HLE::accurate_audio.store(accurate);
                            AudioSettings::dirty.store(true);
                        }
                        if(ImGui::IsItemHovered())ImGui::SetTooltip(
                            "Uses Dekui's accurate decoding and filtering.\nMay sound softer; closer to the original 3DS audio.\nApplies when using HLE audio.");
                    }
                    bool muted=defaults ? defaults->value("Audio/mute_music",false).toBool() : AudioSettings::mute_music.load();
                    if(ImGui::Checkbox("Mute music",&muted)) {
                        if(defaults) defaults->setValue("Audio/mute_music",muted);
                        else {
                            AudioSettings::mute_music.store(muted);
                            AudioSettings::dirty.store(true);
                        }
                    }
                    constexpr const char* mute_help="Mutes background and battle music. Keeps sound effects, short jingles and playable songs.";
                    if(player) {if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",mute_help);}
                    else ImGui::TextWrapped("%s",mute_help);
                    int level=defaults ? int(std::lround(defaults->value("Audio/volume",1.f).toFloat()*100)) : AudioSettings::volume.load();
                    if(ImGui::SliderInt("Volume",&level,0,100,"%d%%")) {
                        if(defaults) defaults->setValue("Audio/volume",level/100.f);
                        else AudioSettings::requested_volume.store(level);
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Controls")) {
                    if(!defaults && ImGui::Button("Configure controller / keyboard"))configure_controls=true;
                    if(ImGui::Button("Fullscreen shortcut...")) {
                        if(defaults) defaults_dialog=DefaultsDialog::FullscreenShortcut;
                        else configure_fullscreen_shortcut=true;
                    }
                    if(ImGui::Button("Custom textures shortcut...")) {
                        if(defaults) defaults_dialog=DefaultsDialog::TexturesShortcut;
                        else configure_textures_shortcut=true;
                    }
                    int targeting=defaults ? defaults->value("Controller/hold_targeting",true).toBool() : GameControlOptions::hold_targeting.load();
                    const char* targeting_modes[]={UILanguage::Pick("Switch","Alterner","Alternar").data(),
                        UILanguage::Pick("Hold","Maintenir","Mantener").data()};
                    if(ImGui::Combo(UILanguage::Pick("Targeting style","Mode de ciblage","Modo de fijación").data(),
                                    &targeting,targeting_modes,2)) {
                        if(defaults) defaults->setValue("Controller/hold_targeting",targeting==1);
                        else {
                            GameControlOptions::hold_targeting=targeting==1;
                            GameControlOptions::dirty=true;
                        }
                    }
                    bool invert=defaults ? defaults->value("Controller/invert_camera_y",false).toBool() : GameControlOptions::invert_camera_y.load();
                    if(ImGui::Checkbox(UILanguage::Pick("Invert first-person Y-axis","Inverser l’axe Y en vue subjective",
                                                       "Invertir el eje Y en primera persona").data(),&invert)) {
                        if(defaults) defaults->setValue("Controller/invert_camera_y",invert);
                        else {
                            GameControlOptions::invert_camera_y=invert;
                            GameControlOptions::dirty=true;
                        }
                    }
                    bool motion=defaults ? defaults->value("Controller/motion_controls",true).toBool() : GameControlOptions::motion_controls.load();
                    if(ImGui::Checkbox(UILanguage::Pick("Motion controls","Commandes par mouvements",
                                                       "Controles de movimiento").data(),&motion)) {
                        if(defaults) defaults->setValue("Controller/motion_controls",motion);
                        else {
                            GameControlOptions::motion_controls=motion;
                            GameControlOptions::dirty=true;
                        }
                    }
                    if(!player) {
                        bool gyro_aim=GyroAim::enabled.load();
                        if(ImGui::Checkbox(UILanguage::Pick("Responsive gyro aim (experimental)",
                            "Visée gyroscopique réactive (expérimental)",
                            "Puntería giroscópica rápida (experimental)").data(),&gyro_aim)) {
                            GyroAim::history.Reset();
                            GyroAim::enabled=gyro_aim;
                        }
                    }
                    bool shoulders=defaults ? defaults->value("Controller/swap_shoulders",false).toBool() : ControlSwap::shoulders.load();
                    if(ImGui::Checkbox("Swap L / R with ZL / ZR",&shoulders)) {
                        if(defaults) defaults->setValue("Controller/swap_shoulders",shoulders);
                        else {
                            ControlSwap::shoulders=shoulders;
                            ControlSwap::dirty=true;
                        }
                    }
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Swaps targeting, shielding, item slots and menu navigation. Button prompts follow the swap.");
                    bool swapped=defaults ? defaults->value("Controller/swap_start_select",false).toBool() : ControlSwap::enabled.load();
                    if(ImGui::Checkbox("Swap Start and Select",&swapped)) {
                        if(defaults) defaults->setValue("Controller/swap_start_select",swapped);
                        else {
                            ControlSwap::enabled=swapped;
                            ControlSwap::dirty=true;
                        }
                    }
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",swapped?"Start opens Items; Select opens Save. Start always opens Songs while playing the ocarina.":
                        "Select opens Items; Start opens Save. Start always opens Songs while playing the ocarina.");
                    bool rumble = defaults ? defaults->value("Controller/rumble_enabled",true).toBool() : Rumble::enabled.load();
                    if (ImGui::Checkbox("Enable rumble", &rumble)) {
                        if(defaults) defaults->setValue("Controller/rumble_enabled",rumble);
                        else {
                            Rumble::enabled = rumble;
                            Rumble::dirty = true;
                        }
                    }
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Rumbles for impacts, fishing and nearby secrets. Replaces the Shard of Agony sound and visual alert while enabled. Uses your most recently used gamepad.");
                    ImGui::BeginDisabled(!rumble);
                    int strength = defaults ? defaults->value("Controller/rumble_strength",100).toInt() : Rumble::strength.load();
                    if (ImGui::SliderInt("Rumble strength", &strength, 0, 100, "%d%%")) {
                        if(defaults) defaults->setValue("Controller/rumble_strength",strength);
                        else {
                            Rumble::strength = strength;
                            Rumble::dirty = true;
                        }
                    }
                    if (!defaults && ImGui::Button("Test rumble")) Rumble::Request(180, 20, 100);
                    if(!defaults && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        if(Rumble::output_status.load()<0)ImGui::SetTooltip("No rumble-capable gamepad responded. Connect a supported controller and press one of its buttons, then test again.");
                        else ImGui::SetTooltip("Sends a short rumble to your most recently used gamepad.");
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }
                if (player && ImGui::BeginTabItem("Randomizer")) {
                    Randomizer::DrawSettings(state.randomizer_icons->Texture(),defaults);
                    ImGui::EndTabItem();
                }
                if (!player && ImGui::BeginTabItem("Camera")) {
                    DrawCameraSettings(true);
                    if(ImGui::CollapsingHeader("Dialogue transitions")) {
                        bool mm_style=DialogueCamera::mm_style.load();
                        if(ImGui::Checkbox("MM-style dialogue camera",&mm_style)) {
                            DialogueCamera::mm_style=mm_style;DialogueCamera::dirty=true;
                        }
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Applies to the next conversation. Uses MM's wider framing and one-second angular slowdown for ordinary NPCs and signs. Scene-specific cameras retain their own framing.");
                        if(mm_style)ImGui::TextWrapped("The camera chooses a side and slows into dialogue framing over one second.");
                        ImGui::BeginDisabled(mm_style);
                        float ease_in=DialogueCamera::ease_in_time.load(),ease_out=DialogueCamera::ease_out_time.load();
                        bool edit=ImGui::SliderFloat("Ease-in (acceleration)",&ease_in,0.f,DialogueCamera::MaximumTime,
                            "%.2f s",ImGuiSliderFlags_AlwaysClamp);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("How long the camera takes to build speed when moving into dialogue framing. Zero skips the acceleration phase.");
                        edit|=ImGui::SliderFloat("Ease-out (deceleration)",&ease_out,0.f,DialogueCamera::MaximumTime,
                            "%.2f s",ImGuiSliderFlags_AlwaysClamp);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("How long the camera slows down as it approaches the character or sign framing. Zero skips the braking phase.");
                        ImGui::Text("Total inward transition: %.2f s",ease_in+ease_out);
                        ImGui::TextWrapped("Both sliders shape the movement into dialogue. Set both to zero for native movement.");
                        if(ImGui::Button("Reset dialogue easing")) {
                            ease_in=DialogueCamera::DefaultEaseIn;ease_out=DialogueCamera::DefaultEaseOut;edit=true;
                        }
                        if(edit) {
                            DialogueCamera::ease_in_time=DialogueCamera::Sanitize(ease_in,DialogueCamera::DefaultEaseIn);
                            DialogueCamera::ease_out_time=DialogueCamera::Sanitize(ease_out,DialogueCamera::DefaultEaseOut);
                            DialogueCamera::dirty=true;
                        }
                        ImGui::EndDisabled();
                    }
                    if(!player && ImGui::CollapsingHeader("Entrance camera capture")) {
                        namespace Capture=CameraTransitionDiagnostics;
                        const auto now=Capture::Now();
                        ImGui::BeginDisabled(Capture::Pending(now));
                        if(ImGui::Button("Capture area / door transition (30 seconds)")) {
                            Capture::Arm();
                            LOG_INFO(Core_ARM11,"OOT entrance capture armed: capture={} delay_ms=3000 duration_ms=30000",Capture::capture.load());
                        }
                        ImGui::EndDisabled();
                        ImGui::TextWrapped("Keep your current camera settings. Close F1, then enter an area or go through a sliding door that causes the zoom snap. Keep playing for a few seconds afterward. Records to azahar_log.txt; interpolation is not required.");
                        if(Capture::Pending(now)) {
                            if(now<Capture::begins.load())ImGui::Text("Starts in %.1f seconds",(Capture::begins.load()-now)/1000.);
                            else ImGui::Text("Recording: %.1f seconds remaining",(Capture::ends.load()-now)/1000.);
                            if(ImGui::Button("Stop entrance capture"))Capture::Stop();
                        } else if(Capture::capture.load())ImGui::TextUnformatted("Capture finished.");
                        ImGui::Text("Samples: %u",Capture::samples.load());
                    }
                    ImGui::EndTabItem();
                }
                if (!player && ImGui::BeginTabItem("Diagnostics")) {
                    DrawGraphicsMemoryDiagnostics();
                    ImGui::Separator();
                    bool mismatch=Randomizer::simulate_file2_runtime_mismatch.load();
                    if(ImGui::Checkbox("Simulate File 2 runtime mismatch",&mismatch)) {
                        Randomizer::simulate_file2_runtime_mismatch=mismatch;
                        if(!mismatch)Randomizer::SetStatus("");
                    }
                    ImGui::TextWrapped("Temporary test: open randomized File 2 and press Start to show its compatibility warning. Saves and seed data are unchanged. Turn this off to restore access; it resets when the app closes.");
                    ImGui::EndTabItem();
                }
                if (!player && ImGui::BeginTabItem("Interpolation")) {
                    using namespace Interpolation;
                    ImGui::TextUnformatted("Developer interpolation");
                    bool on=present_60.load();
                    if(ImGui::Checkbox("Enable frame interpolation",&on)) {
                        present_60=on;enabled=on;preview=false;generation.fetch_add(1);settings_dirty=true;
                    }
                    ImGui::TextWrapped("Replays the scene between native frames using DekUI's emulator approach. Gameplay timing stays unchanged. Requires Vulkan, landscape orientation and stereo off.");
                    bool match=match_refresh_rate.load();
                    if(ImGui::Checkbox("Match Refresh Rate",&match)) {
                        match_refresh_rate=match;++generation;settings_dirty=true;
                    }
                    int fps=target_fps.load();
                    ImGui::BeginDisabled(match);
                    if(ImGui::SliderInt("Target FPS",&fps,30,120,fps==30?"Original (%d)":"%d")) {
                        target_fps=fps;++generation;settings_dirty=true;
                    }
                    ImGui::EndDisabled();
                    ImGui::Text("Display: %.1f Hz | Target: %d FPS",display_refresh_rate.load(),TargetFPS());
                    ImGui::TextWrapped("Match Refresh Rate follows the game window's display, up to 120 FPS. VSync limits output to the display rate. Higher FPS increases rendering cost; gameplay speed is unchanged. Native 60 FPS menus keep their existing cadence.");
#if defined(__APPLE__)
                    if(TargetFPS()>60)
                        ImGui::TextWrapped("Actual presentation rate depends on macOS and display synchronization.");
#endif
                    ImGui::Text("Submitted: %.1f FPS",Timing::submitted_fps.load());
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Measured presentation submissions, not a physical display scanout counter.");
                    if(Timing::display_callback_fps.load()>0) {
                        ImGui::Text("macOS display callbacks: %.1f Hz",Timing::display_callback_fps.load());
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Measured display-link callback cadence; macOS may vary ProMotion refresh. This is not physical scanout.");
                    }
#if defined(__APPLE__)
                    ImGui::Text("Metal reported displayed: %.1f FPS",Timing::displayed_fps.load());
                    ImGui::Text("Display intervals: %.2f - %.2f ms",Timing::display_gap_min_ms.load(),Timing::display_gap_max_ms.load());
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Drawable presentation timestamps reported by Metal. At steady 120 FPS, intervals should stay near 8.33 ms. Occlusion and background windows can affect these measurements.");
#endif
                    if(ImGui::TreeNode("Sun slowdown verification")) {
                        bool asynchronous=SunVisibility::enabled.load();
                        if(ImGui::Checkbox("Asynchronous sun visibility (this session)",&asynchronous))
                            SunVisibility::enabled=asynchronous;
                        const auto probe=SunReadbackProbe::current.load();
                        const auto now=SunReadbackProbe::Now();
                        const bool running=probe && probe->Running(now);
                        ImGui::BeginDisabled(running);
                        if(ImGui::Button("Compare native and asynchronous sun checks (30 seconds)")) {
                            SunReadbackProbe::Start(SunReadbackProbe::Now(),true);
                            LOG_INFO(Render_Vulkan,"OoT sun-readback verification v3 armed: 3-second delay, native/asynchronous/native 10 seconds each; fresh FPS attempt at each phase");
                        }
                        ImGui::EndDisabled();
                        ImGui::TextWrapped("Face the sun, start, then close F1 and keep the camera still for 33 seconds. Compares native checks with recent completed depth snapshots, preserving the flare. Each phase retries your target FPS with slowdown protection active. Your session setting resumes afterward. Logs include actual submissions and skipped frames.");
                        ImGui::Text("Query hook: %s | Depth-load hook: %s | Rejections: %u",
                            (SunReadbackProbe::installed.load()&1)?"ready":"not seen",
                            (SunReadbackProbe::installed.load()&2)?"ready":"not seen",SunReadbackProbe::rejected.load());
                        if(running) {
                            if(now<probe->begins)ImGui::Text("Starts in %.1f seconds",(probe->begins-now)/1e6);
                            else ImGui::Text("Recording %s: %.1f seconds remaining",probe->Name(probe->Phase(now)),(probe->begins+3*SunReadbackProbe::Duration-now)/1e6);
                            if(ImGui::Button("Cancel comparison"))probe->cancelled=true;
                        } else if(probe)ImGui::TextUnformatted(probe->reported.load()?"Comparison finished; results saved to log.":"Comparison ended; waiting for the next presented frame to report.");
                        ImGui::TreePop();
                    }
                    bool cpu=cpu_vertices.load();
                    if(ImGui::Checkbox("Use CPU vertex replay",&cpu)) {cpu_vertices=cpu;++generation;settings_dirty=true;PlayerGraphics::set_hardware=!cpu;}
                    ImGui::TextWrapped("Off uses accelerated shaders where available. On runs vertex shaders on the CPU and replays their captured inputs.");
                    bool actors=Pose::enabled.load();
                    if(ImGui::Checkbox("Interpolate skeletal models",&actors)) {Pose::enabled=actors;++generation;}
                    float distance=distance_limit.load(),angle=angle_limit.load();
                    if(ImGui::SliderFloat("Camera cut distance",&distance,1,1000,"%.1f")) {distance_limit=distance;++generation;}
                    if(ImGui::SliderFloat("Camera cut angle",&angle,1,90,"%.1f degrees")) {angle_limit=angle;++generation;}
                    const char* replay="Off";
                    switch(replay_status.load()) {
                    case ReplayStatus::Settings:replay="Check stereo and screen layout";break;
                    case ReplayStatus::Waiting:replay="Waiting for gameplay";break;
                    case ReplayStatus::Incomplete:replay="Native frame: replay or camera pair unavailable";break;
                    case ReplayStatus::Cadence:replay="Native frame: waiting for stable 30 FPS cadence";break;
                    case ReplayStatus::Active:replay="Presenting intermediate and native frames";break;
                    case ReplayStatus::Off:break;
                    }
                    ImGui::TextUnformatted(replay);
                    ImGui::Text("Presented pairs: %u  Native fallbacks: %u",presented_pairs.load(),fallback_frames.load());
                    ImGui::Text("Replayed draws: %u",replayed_draws.load());
                    ImGui::Text("Captured palettes: %u | Blended model draws: %u",Pose::captures.load(),Pose::blended_draws.load());
                    ImGui::Text("Link hierarchy: %u verified | %u rejected | %u paired",
                        Pose::hierarchy_captures.load(),Pose::hierarchy_rejections.load(),Pose::hierarchy_pairs.load());
                    ImGui::TextWrapped("Camera and matched skeletal palettes are interpolated. Unsupported poses and individual effects retain their native animation states. Experimental; session-only settings.");
                    bool effects=SurfaceEffects::enabled.load();
                    if(ImGui::Checkbox("Interpolate captured particles and water effects",&effects)) {
                        SurfaceEffects::enabled=effects;++generation;
                    }
                    ImGui::Text("Effect pairs: forest %u | rings %u | unrecognized batches %u",
                        SurfaceEffects::forest_pairs.load(),SurfaceEffects::ring_pairs.load(),SurfaceEffects::rejected_batches.load());
                    ImGui::Text("Falling debris pairs: %u | shockwaves %u",Debris::pairs.load(),SurfaceEffects::shockwave_pairs.load());
                    ImGui::Text("Green hit particle pairs: %u",GreenParticles::pairs.load());
                    ImGui::Text("Fairy glow layers: observed %u | paired %u",FairyGlow::observed.load(),FairyGlow::paired.load());
                    if(ImGui::TreeNode("Effect replay diagnostics")) {
                        constexpr std::array names{"", "Forest", "Puddle rings", "Debris", "Shockwaves", "Green particles", "Warp sparkles", "Warp trail",
                            "Bubbles", "Droplets", "Splash atlas", "Bubble burst", "Lava atlas", "Hit atlas", "Death puffs", "Fairy halo", "Beamos charge", "Hit streaks", "Fishing Pond ripples"};
                        for(unsigned kind=1;kind<names.size();++kind) {
                            const auto& count=SurfaceEffects::replay_counts[kind];
                            ImGui::Text("%s: seen %u | attempted %u | strips %u | changed %u",names[kind],
                                count.seen.load(),count.attempted.load(),count.canonical.load(),count.changed.load());
                        }
                        ImGui::TreePop();
                    }
                    ImGui::BeginDisabled(!Replaying() || !Pose::enabled.load() || ActorCapture::active.load() || state.actor_export.valid());
                    if(ImGui::Button("Capture actor skeletons (10 seconds)")) {
                        const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        state.actor_capture_path=FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir)+
                            "oot3d-captures/actor-skeleton-"+std::to_string(stamp)+".json";
                        ActorCapture::Start();state.actor_capture_status.clear();
                    }
                    ImGui::EndDisabled();
                    ImGui::TextWrapped("Requires 60 FPS and skeletal interpolation. Starts after 3 seconds, then records for 10 seconds. Close F1 and repeat the affected action with Link visible. Saves Link's full skeleton and inverse binds, bone palettes, draw matching and sampled geometry to config/oot3d-captures.");
                    if(ActorCapture::active.load()) {
                        const auto time=ActorCapture::Now();
                        if(time<ActorCapture::begins.load())ImGui::Text("Capture starts in %.1f seconds",(ActorCapture::begins.load()-time)/1000.);
                        else ImGui::Text("Recording: %.1f seconds remaining",std::max(0.,(ActorCapture::ends.load()-time)/1000.));
                    }
                    ImGui::Text("Recorded palettes: %u | Draws: %u",ActorCapture::pose_count.load(),ActorCapture::draw_count.load());
                    if(!state.actor_capture_status.empty())ImGui::TextWrapped("%s",state.actor_capture_status.c_str());
                    ImGui::BeginDisabled(SurfaceCapture::active.load() || state.surface_export.valid());
                    if(ImGui::Button("Capture animated surfaces (10 seconds)")) {
                        const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        state.surface_capture_path=FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir)+
                            "oot3d-captures/animated-surfaces-"+std::to_string(stamp)+".json";
                        SurfaceCapture::Start();state.surface_capture_status.clear();
                    }
                    ImGui::EndDisabled();
                    ImGui::TextWrapped("Starts after 3 seconds; close F1. For waterfalls, keep the camera still and face the effect. For puddle rings, make a separate capture while walking through water. For floating particles, record separately with the camera still. Records native texture settings, material uniforms, colors and sampled geometry for 10 seconds. Works with interpolation on or off; saves automatically to config/oot3d-captures.");
                    if(SurfaceCapture::active.load()) {
                        const auto time=SurfaceCapture::Now();
                        if(time<SurfaceCapture::begins.load())ImGui::Text("Surface capture starts in %.1f seconds",(SurfaceCapture::begins.load()-time)/1000.);
                        else ImGui::Text("Surface recording: %.1f seconds remaining",std::max(0.,(SurfaceCapture::ends.load()-time)/1000.));
                    }
                    ImGui::Text("Recorded surface draws: %u",SurfaceCapture::draw_count.load());
                    if(!state.surface_capture_status.empty())ImGui::TextWrapped("%s",state.surface_capture_status.c_str());
                    if(ImGui::Button("Capture song-menu slide (12 seconds)"))OcarinaSlide::ArmDiagnostics();
                    if(OcarinaSlide::DiagnosticActive())
                        ImGui::TextWrapped("Recording song-menu diagnostics. Close F1 and open/close Songs a few times. Results go to azahar_log.txt.");
                    if(ImGui::Button("Capture HUD screen routing (20 seconds)")) {
                        HUD::RoutingCapture::Start();
                        LOG_INFO(Render_Vulkan,"OoT HUD routing capture armed: 3-second delay, then 20 seconds; reproduce the rupee count-up flash");
                    }
                    ImGui::TextWrapped("Starts after 3 seconds. Close F1, then collect the large rupee reward. Records screen routing to azahar_log.txt; no actor capture needed.");
                    if(ImGui::CollapsingHeader("Camera diagnostics")) {
                        if(ImGui::Button("Capture first-person aiming (20 seconds)"))FirstPersonDiagnostics::Arm();
                        if(FirstPersonDiagnostics::Active())
                            ImGui::TextWrapped("Recording to azahar_log.txt. Close F1, look around normally, then aim the slingshot or hookshot and pan left/right and up/down. Exit aiming before the capture ends.");
                        ImGui::Text("Last capture: %s",CaptureResultNames[unsigned(capture_result.load())]);
                        ImGui::Text("Last fallback: %s",ReplayFailureNames[unsigned(last_failure.load())]);
                        ImGui::Text("Native lens draws: %u | Warped: %u | Foreground view: %u",
                            native_lens_draws.load(),warped_lens_draws.load(),attached_view_draws.load());
                        for(unsigned i=1;i<unsigned(ReplayFailure::Count);++i)
                            if(const auto count=replay_failures[i].load())
                                ImGui::Text("%s: %u",ReplayFailureNames[i],count);
                        bool capture=enabled.load(),show=preview.load();
                        ImGui::BeginDisabled(present_60.load());
                        if(ImGui::Checkbox("Capture camera history",&capture)) {enabled=capture;++generation;}
                        if(ImGui::Checkbox("Preview camera in native frame",&show)) {preview=show;enabled=show || capture;++generation;}
                        float fraction=phase.load();
                        if(ImGui::SliderFloat("Preview fraction",&fraction,0,1,"%.3f"))phase=fraction;
                        ImGui::EndDisabled();
                    }
                    if(ImGui::Button("Reset history and counters")) {
                        generation.fetch_add(1);samples=0;pairs=0;applied=0;rejected=0;
                        replayed_draws=0;presented_pairs=0;fallback_frames=0;
                        for(auto& count:replay_failures)count=0;
                        last_failure=ReplayFailure::None;
                        native_lens_draws=0;warped_lens_draws=0;attached_view_draws=0;
                        Pose::captures=0;Pose::blended_draws=0;Pose::Reset();
                        Pose::hierarchy_captures=0;Pose::hierarchy_rejections=0;Pose::hierarchy_pairs=0;
                    }
                    const char* state="Off";
                    switch(status.load()) {
                    case Status::Waiting:state="Waiting for consecutive gameplay cameras";break;
                    case Status::Ready:state="Camera pair ready";break;
                    case Status::Cut:state="Native camera: cut detected";break;
                    case Status::Gap:state="Native camera: history interrupted";break;
                    case Status::Invalid:state="Native camera: invalid transform";break;
                    case Status::Off:break;
                    }
                    ImGui::TextUnformatted(state);
                    ImGui::Text("Samples: %u  Pairs: %u",samples.load(),pairs.load());
                    ImGui::Text("Adjusted draws: %u  Rejected samples: %u",applied.load(),rejected.load());
                    ImGui::TextWrapped("These experimental controls are session-only and start disabled.");
                    ImGui::EndTabItem();
                }
                if (!player && ImGui::BeginTabItem("Editor")) {
                    if(ImGui::CollapsingHeader("File select")) {
                        ImGui::PushID("File select");
                        auto layout=FileSelect::GetLayout();bool changed=false;
                        if(ImGui::TreeNode("Kokiri particles")) {
                            auto& p=layout.particles;
                            changed|=ImGui::Checkbox("Enabled",&p.enabled);
                            changed|=ImGui::SliderInt("Density",&p.density,0,64);
                            changed|=ImGui::SliderFloat("Glow strength",&p.glow,0,2,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Background light strength",&p.light_strength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Background light radius",&p.light_radius,.25f,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Size",&p.size,.25f,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Drift speed",&p.speed,0,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Spin speed",&p.spin,0,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::Checkbox("Varied particles",&p.variations);
                            if(ImGui::Button("Reset particles")){p={};changed=true;}
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Corner vignette")) {
                            auto& v=layout.vignette;
                            changed|=ImGui::Checkbox("Enabled",&v.enabled);
                            changed|=ImGui::SliderFloat("Strength",&v.strength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Bottom-right extra strength",&v.bottom_right_boost,0,4,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Width",&v.width,.05f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Height",&v.height,.05f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::SliderFloat("Softness",&v.softness,.05f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            if(ImGui::Button("Reset vignette")){v={};changed=true;}
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Brown background panels")) {
                            changed|=ImGui::Checkbox("Show panels",&layout.backgrounds_enabled);
                            for(unsigned i=0;i<layout.backgrounds.size();++i) {
                                ImGui::PushID(static_cast<int>(i));
                                if(ImGui::TreeNode(i==0?"Left stack":"Right stack")) {
                                    auto& b=layout.backgrounds[i];
                                    changed|=ImGui::DragFloat("X",&b.pose.x,.1f,-400,400,"%.1f");
                                    changed|=ImGui::DragFloat("Y",&b.pose.y,.1f,-240,240,"%.1f");
                                    changed|=ImGui::DragFloat("Scale",&b.pose.scale,.01f,.1f,3,"%.2fx");
                                    changed|=ImGui::SliderFloat("Darkness",&b.darkness,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::Button("Reset stack")){b=FileSelect::Layout{}.backgrounds[i];changed=true;}
                                    ImGui::TreePop();
                                }
                                ImGui::PopID();
                            }
                            ImGui::TextWrapped("Two panels per stack, over the sky and behind the file stones and prompt box.");
                            ImGui::TreePop();
                        }
                        changed|=ImGui::SliderFloat("Stone shadow opacity",&layout.stone_shadow_opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                        ImGui::TextDisabled("Shared by save slots, action/confirmation stones and Normal/Master Quest stones.");
                        changed|=ImGui::Checkbox("Grow highlighted stones (+5%)",&layout.stone_focus_grow);
                        changed|=ImGui::SliderFloat("Selection grow time",&layout.stone_focus_seconds,0,.3f,"%.2f s",ImGuiSliderFlags_AlwaysClamp);
                        ImGui::TextDisabled("Also applies to Save, Death and Boss Rush pause choices. Set the time to 0 for instant growth.");
                        if(ImGui::TreeNode("File stones")) {
                            auto& p=layout.stones;
                            changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f");
                            changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                            changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                            changed|=ImGui::DragFloat("Spacing",&layout.stone_spacing,.01f,0,4,"%.2fx");
                            changed|=ImGui::SliderFloat("Stone transition ease-out",&layout.stone_ease_out,0,4,"%.2f");
                            ImGui::TextDisabled("0 = linear; higher values slow more near the destination.");
                            if(ImGui::Button("Reset")) {
                                p={};layout.stone_spacing=1;layout.stone_ease_out=2;
                                for(unsigned i=0;i<3;++i)layout.parts[i]={};
                                changed=true;
                            }
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Name entry")) {
                            changed|=ImGui::Checkbox("Native-font keyboard",&layout.keyboard.enabled);
                            if(layout.keyboard.enabled) {
                                changed|=ImGui::DragFloat("Column spacing",&layout.keyboard.spacing_x,.1f,-10,24,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Row spacing",&layout.keyboard.spacing_y,.1f,-10,24,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Character scale",&layout.keyboard.font_scale,.01f,.4f,2,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::Checkbox("Grow highlighted character",&layout.keyboard.selected_grow);
                                float growth=layout.keyboard.selected_growth*100;
                                if(ImGui::SliderFloat("Character growth",&growth,0,100,"%.1f%%",ImGuiSliderFlags_AlwaysClamp)) {
                                    layout.keyboard.selected_growth=growth/100;changed=true;
                                }
                                changed|=ImGui::SliderFloat("Character grow time",&layout.keyboard.selected_growth_seconds,0,.5f,"%.2f s",ImGuiSliderFlags_AlwaysClamp);
                                ImGui::TextDisabled("0 = instant. Smooth growth and shrink at 60 FPS.");
                                ImGui::TextDisabled("Alphabetical grid using the original characters and tabs.");
                                changed|=ImGui::SliderFloat("Cursor X offset",&layout.keyboard.cursor_x,-32,32,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::SliderFloat("Cursor Y offset",&layout.keyboard.cursor_y,-32,32,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset cursor offsets")) {
                                    layout.keyboard.cursor_x=layout.keyboard.cursor_y=0;changed=true;
                                }
                            }
                            for(unsigned i=0;i<layout.name_prompts.size();++i)if(ImGui::TreeNode(i?"B Clear prompt":"A Choose prompt")) {
                                auto& p=layout.name_prompts[i];
                                ImGui::TextDisabled("Offsets from the file-select prompt; Reset matches its position and size.");
                                changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f");
                                changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                                changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                                if(ImGui::Button("Reset")){p={};changed=true;}
                                ImGui::TreePop();
                            }
                            if(ImGui::TreeNode("Keyboard shadow")) {
                                auto& s=layout.keyboard_shadow;
                                changed|=ImGui::Checkbox("Enabled",&s.enabled);
                                changed|=ImGui::SliderFloat("Opacity",&s.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::SliderFloat("Softness",&s.softness,0,40,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::SliderFloat("Spread",&s.spread,-20,40,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("X offset",&s.x,.1f,-80,80,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Y offset",&s.y,.1f,-80,80,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Top padding",&s.padding_top,.1f,-240,400,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Left padding",&s.padding_left,.1f,-240,400,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Bottom padding",&s.padding_bottom,.1f,-240,400,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Right padding",&s.padding_right,.1f,-240,400,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset shadow")){s={};changed=true;}
                                ImGui::TreePop();
                            }
                            for(unsigned i=0;i<FileSelect::NameCount;++i)if(ImGui::TreeNode(FileSelect::NameLabels[i])) {
                                auto& p=layout.name_parts[i];
                                if(i==FileSelect::NameBox) {
                                    changed|=ImGui::SliderFloat("Edge fade",&layout.name_box_edge_fade,0,148,"%.1f px");
                                    ImGui::TextDisabled("Fades both ends; zero restores the side borders.");
                                }
                                changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f");
                                changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                                changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                                if(ImGui::Button("Reset")){
                                    p={};if(i==FileSelect::NameBox)layout.name_box_edge_fade=48;
                                    changed=true;
                                }
                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("RANDO file label")) {
                            auto& p=layout.randomizer_badge;
                            changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f");
                            changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                            changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                            changed|=ImGui::ColorEdit3("Color",layout.randomizer_badge_color.data());
                            ImGui::TextDisabled("Shared by all randomized files. Scales from the edge beside the hearts.");
                            if(ImGui::Button("Reset")){p={};layout.randomizer_badge_color={1.f,.18f,.18f};changed=true;}
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Seed and room ID")) {
                            auto& p=layout.seed_text;
                            changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f");
                            changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                            changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                            changed|=ImGui::SliderFloat("Spacing between lines",&layout.room_id_gap,0,60,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                            if(ImGui::Button("Reset")){p={};layout.room_id_gap=3;changed=true;}
                            if(ImGui::TreeNode("Rounded shadow")) {
                                auto& s=layout.seed_shadow;
                                changed|=ImGui::Checkbox("Enabled",&s.enabled);
                                changed|=ImGui::DragFloat("X offset",&s.x,.1f,-400,400,"%.1f");
                                changed|=ImGui::DragFloat("Y offset",&s.y,.1f,-240,240,"%.1f");
                                changed|=ImGui::DragFloat("Width",&s.width,.01f,.1f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Height",&s.height,.01f,.1f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::SliderFloat("Corner radius",&s.radius,0,80,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::SliderFloat("Softness",&s.softness,0,40,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::SliderFloat("Opacity",&s.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset shadow")){s={};changed=true;}
                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Quest selection and multiplayer prompts")) {
                            constexpr std::array labels{"Original stone","Master Quest stone","Game mode prompt","R Randomizer on/off","X Join room (new file)","X Join room (existing file)","Y Host / Stop hosting","Player count"};
                            for(unsigned i=0;i<labels.size();++i)if(ImGui::TreeNode(labels[i])) {
                                auto& p=i<2?layout.quest_stones[i]:i==2?layout.quest_description:i==3?layout.randomizer_prompt:i==4?layout.join_prompt:i==5?layout.profile_join_prompt:i==6?layout.host_prompt:layout.player_count;
                                changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f");
                                changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                                changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                                if(ImGui::Button("Reset")){
                                    p=i<2?FileSelect::Layout{}.quest_stones[i]:i==4?layout.randomizer_prompt:PausePrompts::Pose{};changed=true;
                                }
                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }
                        for(unsigned i=FileSelect::Prompt;i<FileSelect::DeleteCancel;++i)if(i!=FileSelect::LowerPrompt && ImGui::TreeNode(FileSelect::Names[i])) {
                            auto& p=layout.parts[i];
                            changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f");
                            changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                            changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                            if(ImGui::Button("Reset")){p={};changed=true;}
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Delete confirmation")) {
                            for(unsigned i=FileSelect::DeleteCancel;i<=FileSelect::DeleteErase;++i)if(ImGui::TreeNode(FileSelect::Names[i])) {
                                auto& p=layout.parts[i];
                                changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.2f");
                                changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.2f");
                                changed|=ImGui::DragFloat("Scale",&p.scale,.005f,.1f,4,"%.3fx");
                                if(ImGui::Button("Reset")){p={};changed=true;}
                                ImGui::TreePop();
                            }
                            ImGui::TextDisabled("Uses the shared Stone transition ease-out and shadow opacity.");
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Copy / Overwrite")) {
                            for(unsigned i:{unsigned(FileSelect::OverwriteCancel),unsigned(FileSelect::OverwriteAccept),
                                            unsigned(FileSelect::LowerPrompt),unsigned(FileSelect::CopyArrow)})
                                if(ImGui::TreeNode(FileSelect::Names[i])) {
                                    auto& p=layout.parts[i];
                                    changed|=ImGui::DragFloat("X",&p.x,.1f,-800,800,"%.2f");
                                    changed|=ImGui::DragFloat("Y",&p.y,.1f,-480,480,"%.2f");
                                    changed|=ImGui::DragFloat("Scale",&p.scale,.005f,.1f,4,"%.3fx");
                                    if(ImGui::Button("Reset")){p={};changed=true;}
                                    ImGui::TreePop();
                                }
                            ImGui::TextDisabled("Uses the shared Stone transition ease-out and shadow opacity.");
                            ImGui::TreePop();
                        }
                        constexpr std::array labels{"A Select","B Back"};
                        for(unsigned i=0;i<labels.size();++i)if(ImGui::TreeNode(labels[i])) {
                            auto& p=layout.prompts[i];
                            float x=FileSelect::VisiblePromptX(p.x,p.scale);
                            if(ImGui::DragFloat("X",&x,.1f,PausePrompts::SafeLeft(),FileSelect::VisiblePromptX(400,p.scale),
                                                "%.1f",ImGuiSliderFlags_AlwaysClamp)) {
                                p.x=x;changed=true;
                            }
                            changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                            changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                            if(ImGui::Button("Reset")){p=FileSelect::Layout{}.prompts[i];changed=true;}
                            ImGui::TreePop();
                        }
                        if(changed)FileSelect::SetLayout(layout);
                        ImGui::PopID();
                    }
                    if(ImGui::CollapsingHeader("Ocarina")) {
                        ImGui::PushID("Ocarina");
                        auto layout=Ocarina::GetLayout();bool changed=false;
                        changed|=ImGui::Checkbox("Song title and staff enter from bottom",&layout.bottom_panels);
                        for(unsigned i=0;i<Ocarina::Count;++i)if(ImGui::TreeNode(Ocarina::Labels[i])) {
                            auto& p=layout[i];
                            changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f");
                            changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f");
                            changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx");
                            if(i==Ocarina::Stones) {
                                changed|=ImGui::Checkbox("Two stones per row",&layout.two_columns);
                                changed|=ImGui::DragFloat("Row spacing",&layout.row_spacing,.1f,-40,200,"%.1f");
                                changed|=ImGui::DragFloat("Column spacing",&layout.column_spacing,.1f,-68,200,"%.1f");
                                changed|=ImGui::DragFloat("Stone size",&layout.stone_size,.01f,.1f,4,"%.2fx");
                                ImGui::TextDisabled("Stone size changes all stones without moving their centers.");
                            }
                            if(ImGui::Button("Reset")) {
                                p=Ocarina::Layout{}[i];
                                if(i==Ocarina::Stones) {
                                    layout.two_columns=false;layout.row_spacing=layout.column_spacing=0;
                                    layout.stone_size=1;
                                }
                                changed=true;
                            }
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Side panel")) {
                            changed|=ImGui::DragFloat("Right edge",&layout.panel.right,.1f,0,800,"%.1f");
                            changed|=ImGui::SliderFloat("Opacity",&layout.panel.opacity,0,1,"%.2f");
                            changed|=ImGui::DragFloat("Border width",&layout.panel.border,.1f,0,32,"%.1f");
                            if(ImGui::Button("Reset")){layout.panel={};changed=true;}
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Prompt corner vignette")) {
                            auto& v=layout.prompt_vignette;
                            changed|=ImGui::Checkbox("Enabled",&v.enabled);
                            changed|=ImGui::SliderFloat("Strength",&v.strength,0,1,"%.2f");
                            changed|=ImGui::DragFloat("Width",&v.width,.5f,8,600,"%.1f");
                            changed|=ImGui::DragFloat("Height",&v.height,.5f,8,240,"%.1f");
                            changed|=ImGui::SliderFloat("Softness",&v.softness,.05f,1,"%.2f");
                            changed|=ImGui::DragFloat("X",&v.x,.1f,-400,400,"%.1f");
                            changed|=ImGui::DragFloat("Y",&v.y,.1f,-240,240,"%.1f");
                            ImGui::TextDisabled("Fades with Select / B prompts; anchored to the bottom-right corner.");
                            if(ImGui::Button("Reset")){v={};changed=true;}
                            ImGui::TreePop();
                        }
                        if(changed)Ocarina::SetLayout(layout);
                        ImGui::PopID();
                    }
                    if(ImGui::CollapsingHeader("Death screen")) {
                        ImGui::TextWrapped("Uses the Save menu positions and scales below, including the Game Card warning. Save/Continue uses the Save stone controls; Don't save/Quit uses Don't save. Boss Challenge uses this same placement, with additional Quit/Continue adjustments in its section.");
                    }
                    if(ImGui::CollapsingHeader("Save menu")) {
                        ImGui::PushID("Save menu");
                        auto layout=SaveMenu::GetLayout();bool changed=false;
                        changed|=ImGui::Checkbox("Hide green banner and Save icon",&layout.hide_header);
                        if(ImGui::Button("Capture Save banner motion (12 seconds)"))PauseHeader::ArmDiagnostics();
                        if(PauseHeader::DiagnosticActive())
                            ImGui::TextWrapped("Recording. Close F1, then open and close Save a few times. The capture goes to azahar_log.txt.");
                        ImGui::TextWrapped("Gameplay stays behind the menu. Game Card warning adjustments apply only while saving, on top of the prompt layout.");
                        changed|=ImGui::SliderFloat("Background darkness",&layout.darkness,0,1,"%.2f",ImGuiSliderFlags_NoInput);
                        for(auto i:{SaveMenu::DontSave,SaveMenu::Save,SaveMenu::Prompt,SaveMenu::Back,SaveMenu::CardWarning})if(ImGui::TreeNode(SaveMenu::Labels[i])) {
                            auto& p=layout[i];
                            changed|=ImGui::SliderFloat("X",&p.x,-400,400,"%.1f",ImGuiSliderFlags_NoInput);
                            changed|=ImGui::SliderFloat("Y",&p.y,-240,240,"%.1f",ImGuiSliderFlags_NoInput);
                            changed|=ImGui::SliderFloat("Scale",&p.scale,.1f,3,"%.2fx",ImGuiSliderFlags_NoInput);
                            if(ImGui::Button("Reset")) {p={};changed=true;}
                            ImGui::TreePop();
                        }
                        for(unsigned i=0;i<2;++i)if(ImGui::TreeNode(i?"B Back prompt":"A Select prompt")) {
                            auto& p=layout.prompts[i];
                            changed|=ImGui::SliderFloat("X",&p.x,-400,400,"%.1f");
                            changed|=ImGui::SliderFloat("Y",&p.y,-240,240,"%.1f");
                            changed|=ImGui::SliderFloat("Scale",&p.scale,.1f,3,"%.2fx");
                            if(ImGui::Button("Reset")){p={};changed=true;}
                            ImGui::TreePop();
                        }
                        if(changed)SaveMenu::SetLayout(layout);
                        ImGui::PopID();
                    }
                    if(ImGui::CollapsingHeader("Boss Challenge")) {
                        ImGui::PushID("boss_challenge");
                        auto layout=BossChallenge::GetLayout();bool changed=false;
                        ImGui::TextWrapped("Backdrop, darkness, vignette and particles follow File Select settings.");
                        for(unsigned i=0;i<BossChallenge::Count;++i)if(i!=BossChallenge::Background && i!=BossChallenge::Background2 && i!=BossChallenge::LeftArrow && i!=BossChallenge::RightArrow && ImGui::TreeNode(BossChallenge::Labels[i])) {
                            auto& p=layout[i];
                            changed|=ImGui::SliderFloat("X",&p.x,-400,400,"%.1f");
                            changed|=ImGui::SliderFloat("Y",&p.y,-240,240,"%.1f");
                            changed|=ImGui::SliderFloat("Scale",&p.scale,.1f,3,"%.2f");
                            if(i==BossChallenge::Stones) {
                                int arrangement=layout.horizontal?2:layout.two_columns?1:0;
                                if(ImGui::Combo("Arrangement",&arrangement,"Vertical column\0Two columns\0Horizontal row\0")) {
                                    layout.horizontal=arrangement==2;layout.two_columns=arrangement==1;changed=true;
                                }
                                changed|=ImGui::SliderFloat("X spacing",&layout.spacing_x,-50,200,"%.1f");
                                changed|=ImGui::SliderFloat("Y spacing",&layout.spacing_y,-25,120,"%.1f");
                            }
                            if(i==BossChallenge::Background || i==BossChallenge::Background2)
                                changed|=ImGui::SliderFloat("Darkness",&layout.background_darkness[i-BossChallenge::Background],0,1,"%.2f");
                            if(i==BossChallenge::Footer) {
                                changed|=ImGui::Checkbox("Show bottom strip",&layout.border_bottom);
                                ImGui::TextWrapped("Follows the preview bottom edge. X, Y and scale adjust this strip independently.");
                            }
                            if(i==BossChallenge::Border) {
                                state.map_cutout_editor->Draw();
                                changed|=ImGui::Checkbox("Show cutout",&layout.border_enabled);
                                changed|=ImGui::SliderFloat("Border darkness",&layout.border_darkness,0,1,"%.2f");
                                changed|=ImGui::SliderFloat("Inner shadow fade",&layout.inner_shadow_width,0,24,"%.1f");
                                changed|=ImGui::SliderFloat("Inner shadow opacity",&layout.inner_shadow_opacity,0,1,"%.2f");
                                ImGui::TextWrapped("Position and scale follow the boss preview; these controls adjust the border relative to it.");
                            }
                            if(i==BossChallenge::Preview) {
                                changed|=ImGui::SliderFloat("Corner rounding",&layout.corner_radius,0,32,"%.1f");
                                changed|=ImGui::SliderFloat("Whole panel darkness",&layout.panel_darkness,0,1,"%.2f");
                                changed|=ImGui::SliderFloat("Shadow opacity",&layout.shadow_opacity,0,1,"%.2f");
                                changed|=ImGui::SliderFloat("Shadow softness",&layout.shadow_softness,0,40,"%.1f");
                            }
                            if(ImGui::Button("Reset")) {
                                const BossChallenge::Layout defaults;p=defaults[i];changed=true;
                                if(i==BossChallenge::Background || i==BossChallenge::Background2)
                                    layout.background_darkness[i-BossChallenge::Background]=defaults.background_darkness[i-BossChallenge::Background];
                                if(i==BossChallenge::Preview) {
                                    layout.shadow_opacity=defaults.shadow_opacity;layout.shadow_softness=defaults.shadow_softness;layout.panel_darkness=defaults.panel_darkness;layout.corner_radius=defaults.corner_radius;
                                }
                                if(i==BossChallenge::Border) {layout.border_enabled=true;layout.border_darkness=0;layout.inner_shadow_width=defaults.inner_shadow_width;layout.inner_shadow_opacity=defaults.inner_shadow_opacity;}
                                if(i==BossChallenge::Footer)layout.border_bottom=true;
                                if(i==BossChallenge::Stones) {
                                    layout.two_columns=defaults.two_columns;
                                    layout.horizontal=defaults.horizontal;
                                    layout.spacing_x=defaults.spacing_x;layout.spacing_y=defaults.spacing_y;
                                }
                            }
                            ImGui::TreePop();
                        }
                        if(changed)BossChallenge::SetLayout(layout);
                        ImGui::PopID();
                    }
                    if (ImGui::CollapsingHeader("Texture capturer")) state.texture_inspector->Draw();
                    const bool hud_tabs=ImGui::CollapsingHeader("HUD") && ImGui::BeginTabBar("Authored HUD options");
                    if (hud_tabs && ImGui::BeginTabItem("Elements")) {
                        auto layout = HUD::GetLayout();
                        bool changed = false;
                        constexpr std::array names{"Hearts", "Rupees and small keys", "Magic bar", "A button", "Triforce loading icon", "Minimap", "Small keys", "Timer", "Minigame HUD", "Shard of Agony", "Targeting 3DS icon", "Horse carrots"};
                        ImGui::PushID("HUD layout");
                        changed|=DrawOverallHUDScale(layout.overall_scale);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s","Sets the default size used at 100% in the player HUD settings.");
                        if(ImGui::TreeNode("Multiplayer names")) {
                            MultiplayerSettings::DrawNameSettings();
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Area title banners")) {
                            changed|=ImGui::Checkbox("Reposition area titles",&layout.reposition_area_titles);
                            changed|=ImGui::SliderFloat("Title Y offset",&layout.area_title_y,-240,240,"%.0f");
                            changed|=ImGui::SliderFloat("Title scale",&layout.area_title_scale,
                                HUD::Layout::MinAreaTitleScale,HUD::Layout::MaxAreaTitleScale,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                            if(ImGui::Button("Center lower titles")) {
                                layout.area_title_y=-72;layout.reposition_area_titles=true;changed=true;
                            }
                            ImGui::SameLine();
                            if(ImGui::Button("Reset scale")) {layout.area_title_scale=1;changed=true;}
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Navi text")) {
                            changed|=ImGui::DragFloat("X",&layout.navi_text.x,.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                            changed|=ImGui::DragFloat("Y",&layout.navi_text.y,.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                            if(ImGui::Button("Reset")){layout.navi_text={};changed=true;}
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("HD Numbers")) {
                            changed|=ImGui::Checkbox("Enable HD numbers",&layout.hd_numbers);
                            ImGui::BeginDisabled(!layout.hd_numbers);
                            int number_font=layout.hd_numbers_bold?1:0;
                            if(ImGui::Combo("Font",&number_font,"Thin\0Bold\0")) {
                                layout.hd_numbers_bold=number_font==1;changed=true;
                            }
                            if(ImGui::TreeNode("Shadow")) {
                                ImGui::TextDisabled("Applies to counters other than timers.");
                                auto& s=layout.hd_rupee_shadow;
                                changed|=ImGui::Checkbox("Enable shadow",&s.enabled);
                                changed|=ImGui::DragFloat("Horizontal offset",&s.x,.05f,-3,3,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Vertical offset",&s.y,.05f,-3,3,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Blur",&s.blur,.05f,0,2,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::SliderFloat("Strength",&s.strength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset shadow")){s={};changed=true;}
                                ImGui::TreePop();
                            }
                            struct Section {const char* label;bool* enabled;HUD::Pose* pose;float* spacing;};
                            for(const auto section:{Section{"Rupees and small keys",&layout.hd_rupee_numbers,&layout.hd_rupee_text,&layout.hd_rupee_spacing},
                                Section{"Timers",&layout.hd_timer_numbers,&layout.hd_timer_text,&layout.hd_timer_spacing},
                                Section{"Ammo counts",&layout.hd_ammo_numbers,&layout.hd_ammo_text,&layout.hd_ammo_spacing},
                                Section{"Minigame scores",&layout.hd_score_numbers,&layout.hd_score_text,&layout.hd_score_spacing},
                                Section{"Skulltula count",&layout.hd_skulltula_numbers,&layout.hd_skulltula_text,&layout.hd_skulltula_spacing}}) {
                                if(!ImGui::TreeNode(section.label))continue;
                                changed|=ImGui::Checkbox("HD font",section.enabled);
                                ImGui::BeginDisabled(!*section.enabled);
                                auto& p=*section.pose;
                                if(section.pose==&layout.hd_ammo_text)ImGui::TextDisabled("Gameplay placement");
                                changed|=ImGui::DragFloat("X",&p.x,.1f,-100,100,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Y",&p.y,.1f,-100,100,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Digit spacing",section.spacing,.1f,-8,24,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset placement")){p={};*section.spacing=0;changed=true;}
                                if(section.pose==&layout.hd_ammo_text && ImGui::TreeNode("Items screen")) {
                                    auto& pose=layout.hd_pause_ammo_text;
                                    changed|=ImGui::DragFloat("X",&pose.x,.1f,-100,100,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::DragFloat("Y",&pose.y,.1f,-100,100,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::DragFloat("Scale",&pose.scale,.01f,.1f,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::Button("Reset placement")){pose={};changed=true;}
                                    ImGui::TreePop();
                                }
                                if((section.pose==&layout.hd_ammo_text || section.pose==&layout.hd_rupee_text) && ImGui::TreeNode("Soft backdrop")) {
                                    auto& b=section.pose==&layout.hd_rupee_text?layout.hd_rupee_backdrop:layout.hd_ammo_backdrop;
                                    if(section.pose==&layout.hd_rupee_text)ImGui::TextDisabled("Shared by rupee and small-key numbers.");
                                    changed|=DrawSoftBackdrop(b);
                                    ImGui::TreePop();
                                }
                                ImGui::EndDisabled();ImGui::TreePop();
                            }
                            ImGui::EndDisabled();ImGui::TreePop();
                        }
                        for (unsigned i = 0; i < layout.size(); ++i) {
                            if(i==HUD::AButton || i==HUD::Keys)continue;
                            ImGui::PushID(static_cast<int>(i));
                            if(!ImGui::TreeNode(names[i])) {
                                ImGui::PopID();continue;
                            }
                            if(i==HUD::Minimap) {
                                changed |= ImGui::Checkbox("Lock bottom-right corner",&layout.minimap_lock_corner);
                                ImGui::TextDisabled("X/Y move the visible corner. Scale expands up and left.");
                                for(auto [label,p]:{std::pair{"Dungeons",&layout[HUD::Minimap]},
                                                     std::pair{"Outside dungeons",&layout.outside_minimap}}) {
                                    if(!ImGui::TreeNode(label))continue;
                                    changed |= ImGui::DragFloat("X",&p->x,.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                    changed |= ImGui::DragFloat("Y",&p->y,.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                    changed |= ImGui::DragFloat("Scale",&p->scale,.01f,.1f,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::Button("Move to bottom-right")) {
                                        HUD::MinimapBottomRight(*p);layout.minimap_lock_corner=true;changed=true;
                                    }
                                    if(ImGui::Button("Reset")) {*p={};changed=true;}
                                    ImGui::TreePop();
                                }
                                ImGui::TreePop();ImGui::PopID();continue;
                            }
                            auto& pose = layout[i];
                            changed |= ImGui::SliderFloat("X", &pose.x, -400.f, 400.f, "%.1f", ImGuiSliderFlags_NoInput);
                            changed |= ImGui::SliderFloat("Y", &pose.y, -240.f, 240.f, "%.1f", ImGuiSliderFlags_NoInput);
                            changed |= ImGui::SliderFloat("Scale", &pose.scale, .1f, 3.f, "%.2fx", ImGuiSliderFlags_NoInput);
                            if (i == HUD::Hearts) {
                                changed |= ImGui::SliderFloat("Spacing", &layout.heart_spacing, -8.f, 20.f, "%+.1f px", ImGuiSliderFlags_NoInput);
                                changed |= ImGui::ColorEdit3("Heart color",layout.heart_color.data(),ImGuiColorEditFlags_NoOptions);
                                changed |= ImGui::SliderFloat("Brightness",&layout.heart_brightness,0,2,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                changed |= DrawSpriteContour("Shadow contour",layout.heart_contours.shadow);
                                changed |= DrawSpriteContour("Colored outline",layout.heart_contours.outline,
                                    "Applies to ordinary hearts.");
                                changed |= DrawSpriteContour("Custom double-defense hearts",layout.heart_contours.double_defense,
                                    "Uses regular heart artwork with a separate contour for double defense.");
                            }
                            if (i == HUD::Rupees) {
                                changed |= ImGui::SliderFloat("Distance between counters",&layout.counter_distance,0,UserHUD::Layout::MaxCounterDistance,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed |= DrawSpriteContour("Rupee icon contour",layout.rupee_contour);
                                auto& outline=layout.rupee_digit_outline;
                                changed |= ImGui::Checkbox("Outline counter numbers",&outline.enabled);
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Applies to HD rupee, small-key, timer and Skulltula digits.");
                                if(outline.enabled) {
                                    changed |= ImGui::SliderFloat("Outline thickness",&outline.thickness,
                                        outline.MinThickness,outline.MaxThickness,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                    changed |= ImGui::ColorEdit3("Outline color",outline.color.data(),ImGuiColorEditFlags_NoOptions);
                                }
                                changed |= ImGui::DragFloat("Icon-to-number spacing",&layout.rupee_number_gap,.1f,-100,100,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                            }
                            if (i == HUD::Carrots) {
                                changed |= ImGui::SliderFloat("Spacing", &layout.carrot_spacing, -20.f, 40.f, "%+.1f px", ImGuiSliderFlags_NoInput);
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Adjusts gaps between carrots while keeping the row centered.");
                                changed |= ImGui::SliderFloat("Opacity", &layout.carrot_opacity, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                                changed |= ImGui::SliderFloat("Animation delay", &layout.carrot_animation_delay, 0.f, 2.f, "%.2f s", ImGuiSliderFlags_NoInput);
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Delays the used carrot's vertical bounce and switch to gray.");
                                if(ImGui::TreeNode("Hard shadow")) {
                                    auto& shadow=layout.carrot_shadow;
                                    changed |= ImGui::Checkbox("Enable shadow",&shadow.enabled);
                                    changed |= ImGui::SliderFloat("Horizontal offset",&shadow.x,-12,12,"%.1f px",ImGuiSliderFlags_NoInput);
                                    changed |= ImGui::SliderFloat("Vertical offset",&shadow.y,-12,12,"%.1f px",ImGuiSliderFlags_NoInput);
                                    changed |= ImGui::SliderFloat("Opacity",&shadow.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::Button("Reset shadow")){shadow={};changed=true;}
                                    ImGui::TreePop();
                                }
                            }
                            if (i == HUD::Magic) {
                                changed |= ImGui::DragFloat("One heart row Y offset",&layout.magic_one_row_y,.1f,-240,240,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed |= ImGui::DragFloat("Two heart rows Y offset",&layout.magic_two_row_y,.1f,-240,240,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                changed |= ImGui::DragFloat("Height scale",&layout.magic_height,.01f,1,8,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                auto& border=layout.magic_contour;
                                float thickness=(border.width+border.height)*.5f;
                                if(DrawMagicBorderThickness(thickness)) {
                                    border.width=border.height=thickness;
                                    border.enabled=true;
                                    changed=true;
                                }
                                changed |= ImGui::DragFloat("Fill padding",&layout.magic_border,.01f,0,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                changed |= ImGui::SliderFloat("Black background opacity",&layout.magic_background_opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                changed |= ImGui::ColorEdit3("Fill color",layout.magic_fill_color.data(),ImGuiColorEditFlags_NoOptions);
                                auto& shadow=layout.magic_shadow;
                                ImGui::PushID("magic_shadow");
                                changed |= ImGui::Checkbox("Drop shadow",&shadow.enabled);
                                if(shadow.enabled) {
                                    changed |= ImGui::SliderFloat("Softness",&shadow.blur,0,12,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    changed |= ImGui::SliderFloat("Opacity",&shadow.strength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    changed |= ImGui::DragFloat("Horizontal offset",&shadow.x,.1f,-20,20,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    changed |= ImGui::DragFloat("Vertical offset",&shadow.y,.1f,-20,20,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                }
                                ImGui::PopID();
                                changed |= DrawSpriteContour("Outer border",layout.magic_contour,nullptr,&layout.magic_border_roundness);
                                ImGui::TextDisabled("Height extends downward; the top edge stays fixed.");
                            }
                            if (ImGui::Button("Reset")) {
                                pose = {};
                                if (i == HUD::Hearts) {
                                    layout.heart_spacing=0;layout.heart_contours={};
                                    layout.heart_color=HUD::Layout::NativeHeartColor;layout.heart_brightness=1;
                                }
                                if (i == HUD::Rupees) {layout.counter_distance=HUD::Layout{}.counter_distance;layout.rupee_number_gap=0;layout.rupee_contour={};layout.rupee_digit_outline={};}
                                if (i == HUD::Carrots) {layout.carrot_opacity = 1;layout.carrot_spacing = 0;layout.carrot_animation_delay = 0;layout.carrot_shadow={};}
                                if (i == HUD::Magic) {
                                    layout.magic_one_row_y=HUD::Layout{}.magic_one_row_y;
                                    layout.magic_two_row_y=HUD::Layout{}.magic_two_row_y;
                                    layout.magic_height=layout.magic_border=layout.magic_background_opacity=1;
                                    layout.magic_fill_color=HUD::NativeMagicFill;
                                    layout.magic_contour={};layout.magic_border_roundness=0;layout.magic_shadow={};
                                }
                                changed = true;
                            }
                            ImGui::TreePop();
                            ImGui::PopID();
                        }
                        ImGui::PopID();
                        if(ImGui::TreeNode("Shop prompts")) {
                            for(unsigned side=0;side<2;++side) {
                                if(!ImGui::TreeNode(side?"Right arrow and stick":"Left arrow and stick"))continue;
                                auto& p=layout.shop_prompt_offsets[side];
                                changed|=ImGui::DragFloat("X",&p[0],.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Y",&p[1],.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset")) {p={};changed=true;}
                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Item button backgrounds")) {
                            auto buttons=HUDButtons::GetLayout();bool edited=false;
                            edited|=ImGui::Checkbox("Show equipped item icons",&buttons.show_items);
                            if(ImGui::TreeNode("A button")) {
                                auto& p=layout[HUD::AButton];
                                changed|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                changed|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset")) {p={};changed=true;}
                                ImGui::TreePop();
                            }
                            edited|=ImGui::DragFloat("Four-button X",&buttons.item_x,.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                            edited|=ImGui::DragFloat("Four-button Y",&buttons.item_y,.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                            edited|=ImGui::DragFloat("Four-button scale",&buttons.item_scale,.01f,.1f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                            bool arrange=ImGui::SliderFloat("Diamond rotation",&buttons.diamond_rotation,-180,180,"%.1f degrees",ImGuiSliderFlags_AlwaysClamp);
                            arrange|=ImGui::SliderFloat("Diamond spacing",&buttons.diamond_spacing,0,150,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                            arrange|=ImGui::Button("Arrange diamond");
                            if(arrange) {HUDButtons::ArrangeDiamond(buttons);edited=true;}
                            if(ImGui::TreeNode("X / Y / I / II labels")) {
                                auto& p=buttons.item_labels;
                                edited|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                edited|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                edited|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset")){p={};edited=true;}
                                ImGui::TreePop();
                            }
                            if(ImGui::TreeNode("Item inside bubble")) {
                                ImGui::TextDisabled("Applies to B, X, Y, I, and II.");
                                auto& item=buttons.item_icons;
                                edited|=ImGui::DragFloat("X",&item.x,.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                edited|=ImGui::DragFloat("Y",&item.y,.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                edited|=ImGui::DragFloat("Scale",&item.scale,.01f,.1f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::Button("Reset")){item={};edited=true;}
                                ImGui::TreePop();
                            }
                            edited|=ImGui::SliderFloat("Item count scale",&buttons.counter_scale,.1f,4,"%.2fx");
                            edited|=ImGui::DragFloat("Item count X",&buttons.counter_x,.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                            edited|=ImGui::DragFloat("Item count Y",&buttons.counter_y,.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                            if(ImGui::Button("Reset shared controls")) {
                                buttons.item_x=buttons.item_y=0;buttons.item_scale=1;buttons.item_labels={};
                                buttons.item_icons={};buttons.counter_scale=1;buttons.counter_x=buttons.counter_y=0;
                                buttons.diamond_rotation=0;buttons.diamond_spacing=34;
                                HUDButtons::ArrangeDiamond(buttons);edited=true;
                            }
                            for(unsigned i=0;i<HUDButtons::Count;++i)if(ImGui::TreeNode(HUDButtons::Labels[i])) {
                                auto& button=buttons.buttons[i];auto& p=button.pose;
                                edited|=ImGui::Checkbox("Show",&button.enabled);
                                edited|=ImGui::DragFloat("X",&p.x,.1f,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                edited|=ImGui::DragFloat("Y",&p.y,.1f,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                edited|=ImGui::DragFloat("Scale",&p.scale,.01f,.1f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                if(i==HUDButtons::B) {
                                    edited|=ImGui::SliderFloat("Minigame horizontal offset",&buttons.minigame_b_x,-100,100,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Moves the minigame B button, label, weapon and ammo together. Positive values move right; regular gameplay is unchanged.");
                                    if(ImGui::TreeNode("Ammo count")) {
                                        auto& ammo=buttons.b_ammo;
                                        ImGui::TextDisabled("Moves only B's ammo; works with native and HD numbers.");
                                        edited|=ImGui::DragFloat("X",&ammo.x,.1f,-100,100,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        edited|=ImGui::DragFloat("Y",&ammo.y,.1f,-100,100,"%+.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        edited|=ImGui::DragFloat("Scale",&ammo.scale,.01f,.1f,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                        if(ImGui::Button("Reset ammo")){ammo={};edited=true;}
                                        ImGui::TreePop();
                                    }
                                }
                                if(ImGui::Button("Reset")){button=HUDButtons::Layout{}.buttons[i];if(i==HUDButtons::B){buttons.minigame_b_x=0;buttons.b_ammo={};}edited=true;}
                                ImGui::TreePop();
                            }
                            if(edited)HUDButtons::SetLayout(buttons);
                            ImGui::TreePop();
                        }
                        if(changed)HUD::SetLayout(layout);
                        if(ImGui::TreeNode("D-pad")) {
                            auto dpad=DPad::GetLayout();
                            bool edited=ImGui::Checkbox("Enable D-pad shortcuts",&dpad.enabled);
                            edited|=ImGui::SliderFloat("Navi pulse speed",&dpad.navi_pulse_speed,.25f,4.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                            const auto controls=[&](DPad::Pose& pose) {
                                edited|=ImGui::SliderFloat("X",&pose.x,-400,400,"%.1f",ImGuiSliderFlags_NoInput);
                                edited|=ImGui::SliderFloat("Y",&pose.y,-240,240,"%.1f",ImGuiSliderFlags_NoInput);
                                edited|=ImGui::SliderFloat("X scale",&pose.scale_x,.1f,4,"%.2fx",ImGuiSliderFlags_NoInput);
                                edited|=ImGui::SliderFloat("Y scale",&pose.scale_y,.1f,4,"%.2fx",ImGuiSliderFlags_NoInput);
                                if(ImGui::Button("Reset")) {pose={};edited=true;}
                            };
                            ImGui::TextUnformatted("Entire D-pad");
                            ImGui::PushID("D-pad group");controls(dpad.group);ImGui::PopID();
                            for(unsigned i=0;i<DPad::ElementCount;++i)
                                if(ImGui::TreeNode(DPad::Labels[i])) {
                                    controls(dpad.icons[i]);ImGui::TreePop();
                                }
                            if(edited)DPad::SetLayout(dpad);
                            ImGui::TreePop();
                        }
                        ImGui::EndTabItem();
                    }
                    if (hud_tabs) {
                        if (ImGui::BeginTabItem("Padding")) {
                            auto layout=HUD::GetLayout();
                            if(DrawHUDPadding(layout.padding))HUD::SetLayout(layout);
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    if (ImGui::CollapsingHeader("Pause screen")) {
                        auto layout=PauseMenu::GetLayout();bool changed=false;
                        ImGui::PushID("Pause screen");
                        if(ImGui::TreeNode("Banner colors")) {
                            auto& colors=layout.color_layouts;
                            colors.Initialize(layout.header_overlay,layout.slots_overlay);
                            for(unsigned i=0;i<2;++i) {
                                if(i)ImGui::SameLine();
                                if(ImGui::RadioButton(i?"Layout 2":"Layout 1",colors.active==i)) {
                                    colors.Select(i);changed=true;
                                }
                            }
                            if(ImGui::Button("Save layout")) {colors.Save();changed=true;}
                            ImGui::SameLine();
                            ImGui::TextDisabled(colors.Unsaved()?"Unsaved changes":"Saved");
                            ImGui::TextWrapped("Changes preview live. Save layout before switching to keep your edits.");
                            for(unsigned i=0;i<PauseMenu::ColorLabels.size();++i) {
                                ImGui::PushID(int(i));
                                ImGui::TextUnformatted(PauseMenu::ColorLabels[i]);
                                auto& color=colors.working[i];
                                changed|=ImGui::ColorEdit3("Color",color.color.data(),ImGuiColorEditFlags_NoOptions);
                                changed|=ImGui::SliderFloat("Opacity",&color.opacity,0,1,"%.2f");
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Tint strength; preserves the original texture detail.");
                                if(ImGui::Button("Reset color")) {color={};changed=true;}
                                ImGui::PopID();
                            }
                            ImGui::TreePop();
                        }
                        const auto shadow_controls=[&](PauseMenu::Shadow& shadow) {
                            if(ImGui::TreeNode("Drop shadow")) {
                                changed |= ImGui::SliderFloat("Width",&shadow.width,.1f,4.f,"%.2fx",ImGuiSliderFlags_NoInput);
                                changed |= ImGui::SliderFloat("Opacity",&shadow.opacity,0.f,1.f,"%.2f",ImGuiSliderFlags_NoInput);
                                if(ImGui::Button("Reset shadow")) {shadow={};changed=true;}
                                ImGui::TreePop();
                            }
                        };
                        const auto color_controls=[&](PauseMenu::ColorOverlay& overlay) {
                            if(ImGui::TreeNode("Color overlay")) {
                                changed |= ImGui::ColorEdit3("Color",overlay.color.data(),ImGuiColorEditFlags_NoOptions);
                                changed |= ImGui::SliderFloat("Opacity",&overlay.opacity,0.f,1.f,"%.2f",ImGuiSliderFlags_NoInput);
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Controls the strength of the color tint while preserving the texture detail.");
                                if(ImGui::Button("Reset color")) {overlay={};changed=true;}
                                ImGui::TreePop();
                            }
                        };
                        auto prompts=PausePrompts::GetLayout();
                        bool edited=false;
                        const auto pose_controls=[&](PausePrompts::Pose& pose) {
                            bool edit=ImGui::DragFloat("X",&pose.x,.05f,-400,400,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            edit|=ImGui::DragFloat("Y",&pose.y,.05f,-240,240,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                            edit|=ImGui::DragFloat("Scale",&pose.scale,.005f,.1f,4,"%.3fx",ImGuiSliderFlags_AlwaysClamp);
                            return edit;
                        };
                        const auto prompt_controls=[&](unsigned i,bool lower) {
                            if(ImGui::TreeNode(PausePrompts::Names[i],"%s",PausePrompts::Label(i))) {
                                auto& prompt=prompts.prompts[i];
                                if(lower)edited|=ImGui::DragFloat("Prompt scale",&prompt.group.scale,.005f,.1f,4,"%.3fx",ImGuiSliderFlags_AlwaysClamp);
                                else edited|=pose_controls(prompt.group);
                                if(ImGui::TreeNode(i<PausePrompts::ItemsToMap?"Button relative to text":"Button and arrow relative to text")) {
                                    edited|=pose_controls(prompt.button);
                                    if(ImGui::Button("Reset button")) {prompt.button={};edited=true;}
                                    ImGui::TreePop();
                                }
                                if(i==PausePrompts::MapToItems && ImGui::TreeNode("Rounded backdrop shadow")) {
                                    auto& shadow=prompts.map_navigation_shadow;
                                    edited|=ImGui::Checkbox("Enabled",&shadow.enabled);
                                    edited|=ImGui::SliderFloat("Opacity",&shadow.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    edited|=ImGui::SliderFloat("Width",&shadow.width,8,200,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    edited|=ImGui::SliderFloat("Height",&shadow.height,8,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    edited|=ImGui::SliderFloat("Softness",&shadow.softness,.01f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    edited|=ImGui::DragFloat("X offset",&shadow.x,.1f,-200,200,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    edited|=ImGui::DragFloat("Y offset",&shadow.y,.1f,-240,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    ImGui::TextWrapped("A soft semicircle behind the arrow, L button and Items label. Follows this prompt's position and scale.");
                                    if(ImGui::Button("Reset shadow")){shadow={};edited=true;}
                                    ImGui::TreePop();
                                }
                                if(ImGui::Button("Reset prompt")) {prompt={};edited=true;}
                                ImGui::TreePop();
                            }
                        };
                        const auto part_controls=[&](PauseMenu::Part part) {
                            ImGui::PushID(static_cast<int>(part));
                            if(ImGui::TreeNode(PauseMenu::Labels[part])) {
                                auto& pose=layout[part];
                                if(part==PauseMenu::Map) {
                                    changed |= ImGui::SliderFloat("Darkness",&layout.map_darkness,0.f,1.f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::TreeNode("Blurred edge extension")) {
                                        auto& e=layout.map_extension;
                                        changed |= ImGui::Checkbox("Enabled",&e.enabled);
                                        changed |= ImGui::SliderFloat("Width",&e.width,0.f,400.f,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                        changed |= ImGui::SliderFloat("Blur strength",&e.blur,0.f,40.f,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                        changed |= ImGui::SliderFloat("Darkness",&e.darkness,0.f,1.f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        ImGui::TextWrapped("Extends the world map's parchment on both sides. Blur and darkness increase away from the map.");
                                        if(ImGui::Button("Reset extension")) {e={};changed=true;}
                                        ImGui::TreePop();
                                    }
                                    changed |= ImGui::Checkbox("Left and right drop shadows",&layout.map_side_shadows);
                                    if(layout.map_side_shadows)shadow_controls(layout.map_shadow);
                                }
                                if(part==PauseMenu::Slots) {
                                    changed |= ImGui::Checkbox("Horizontal",&layout.horizontal);
                                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Up from the top item row enters the slots. Left / Right selects a slot; Down returns to the item grid.");
                                    if(ImGui::TreeNode("Stones, labels and items")) {
                                        changed |= ImGui::SliderFloat("Scale",&layout.slot_content_scale,.25f,2.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                        changed |= ImGui::SliderFloat("Spacing",&layout.slot_spacing,.25f,2.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                        changed |= ImGui::SliderFloat("X",&layout.slot_content_x,-240.f,240.f,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                        ImGui::TextWrapped("Adjusts the slot contents inside the green strip. The strip keeps its own size and position.");
                                        if(ImGui::Button("Reset slot contents")) {
                                            layout.slot_content_scale=layout.slot_spacing=1;layout.slot_content_x=0;changed=true;
                                        }
                                        ImGui::TreePop();
                                    }
                                    shadow_controls(layout.slots_shadow);
                                }
                                if(part==PauseMenu::Items)shadow_controls(layout.items_shadow);
                                if(part==PauseMenu::Background) {
                                    changed |= ImGui::Checkbox("Stack 2 backgrounds",&layout.stack_background);
                                    changed |= ImGui::SliderFloat("Darkness",&layout.background_darkness,0.f,1.f,"%.2f",ImGuiSliderFlags_NoInput);
                                    shadow_controls(layout.background_shadow);
                                }
                                if(PauseMenu::IsLowerGear(part)) {
                                    changed |= ImGui::Checkbox("Vertical",&layout.gear_vertical[part-PauseMenu::GearBottomLeft]);
                                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Stacks this panel vertically, keeping icons and numbers upright. Use Up / Down to move along it.");
                                }
                                changed |= ImGui::SliderFloat("X",&pose.x,-400.f,400.f,"%.1f",ImGuiSliderFlags_NoInput);
                                changed |= ImGui::SliderFloat("Y",&pose.y,-240.f,240.f,"%.1f",ImGuiSliderFlags_NoInput);
                                changed |= ImGui::SliderFloat("Scale",&pose.scale,.1f,3.f,"%.2fx",ImGuiSliderFlags_NoInput);
                                if(ImGui::Button("Reset")) {
                                    pose={};
                                    if(part==PauseMenu::Slots)layout.horizontal=false;
                                    if(PauseMenu::IsLowerGear(part))layout.gear_vertical[part-PauseMenu::GearBottomLeft]=false;
                                    if(part==PauseMenu::Background) {layout.stack_background=false;layout.background_darkness=0;layout.background_shadow={};}
                                    if(part==PauseMenu::Slots) {layout.slots_shadow={};layout.slot_content_scale=layout.slot_spacing=1;layout.slot_content_x=0;}
                                    if(part==PauseMenu::Map)layout.map_darkness=0;
                                    if(part==PauseMenu::Items)layout.items_shadow={};
                                    changed=true;
                                }
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        };
                        if(ImGui::BeginTabBar("Pause screen sections")) {
                            if(ImGui::BeginTabItem("Common")) {
                                changed |= ImGui::SliderFloat("2D selection grow duration",&layout.selection_grow_seconds,0.f,1.f,"%.3f s",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Time for highlighted 2D icons on Items and Gear to grow. Zero keeps the native instant enlargement.");
                                changed |= ImGui::SliderFloat("Slide duration",&layout.transition_seconds,.02f,2.f,"%.3f s",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Time for the pause panels, banner, titles and item slots to slide in or out. Higher values are slower; their movement stays synchronized.");
                                if(ImGui::Button("Capture banner motion (12 seconds)"))PauseHeader::ArmDiagnostics();
                                if(ImGui::Button("Reset slide duration")) {layout.transition_seconds=PauseMenu::DefaultTransitionSeconds;changed=true;}
                                if(ImGui::TreeNode("Prompt defaults")) {
                                    edited|=ImGui::Checkbox("Show pause button prompts",&prompts.enabled);
                                    edited|=ImGui::DragFloat("All prompts scale",&prompts.scale,.005f,.1f,4,"%.3fx",ImGuiSliderFlags_AlwaysClamp);
                                    edited|=ImGui::DragFloat("All button icons scale",&prompts.icon_scale,.005f,.1f,4,"%.3fx",ImGuiSliderFlags_AlwaysClamp);
                                    edited|=ImGui::DragFloat("Letter spacing",&prompts.letter_spacing,.01f,0,8,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
                                    ImGui::TextDisabled("Drag for fine adjustments; Ctrl+click to enter a value.");
                                    ImGui::TreePop();
                                }
                                if(ImGui::TreeNode("Items / Gear shared prompts")) {
                                    edited|=pose_controls(prompts.lower.pose);
                                    edited|=ImGui::SliderFloat("Spacing",&prompts.lower.spacing,-20,100,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    ImGui::TextWrapped("These prompts are shared by the Items and Gear pages.");
                                    if(ImGui::Button("Reset row")) {prompts.lower={};edited=true;}
                                    for(unsigned i:{PausePrompts::ChangeTab,PausePrompts::Choose,PausePrompts::Return,
                                                    PausePrompts::EquipI,PausePrompts::EquipII})prompt_controls(i,true);
                                    ImGui::TreePop();
                                }
                                if(ImGui::TreeNode("Page header")) {
                                    changed |= ImGui::SliderFloat("Y",&layout.header_y,-240.f,240.f,"%.1f",ImGuiSliderFlags_NoInput);
                                    shadow_controls(layout.header_shadow);
                                    if(ImGui::Button("Reset")) {layout.header_y=0;layout.header_shadow={};changed=true;}
                                    ImGui::TreePop();
                                }
                                if(ImGui::TreeNode("Items / Gear bottom gradient")) {
                                    changed|=ImGui::Checkbox("Enabled",&layout.bottom_gradient);
                                    changed|=ImGui::SliderFloat("Width",&layout.bottom_gradient_width_percent,0,100,"%.0f%%",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::SliderFloat("Height",&layout.bottom_gradient_height,1,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::SliderFloat("Bottom opacity",&layout.bottom_gradient_opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::Button("Reset gradient")) {
                                        const PauseMenu::Layout defaults;
                                        layout.bottom_gradient=defaults.bottom_gradient;
                                        layout.bottom_gradient_width_percent=defaults.bottom_gradient_width_percent;
                                        layout.bottom_gradient_height=defaults.bottom_gradient_height;
                                        layout.bottom_gradient_opacity=defaults.bottom_gradient_opacity;
                                        changed=true;
                                    }
                                    ImGui::TreePop();
                                }
                                if(ImGui::TreeNode("Items / Gear brown background")) {
                                    part_controls(PauseMenu::Background);
                                    if(ImGui::TreeNode("Bottom corner vignette")) {
                                        auto& vignette=layout.background_vignette;
                                        changed|=ImGui::Checkbox("Enabled",&vignette.enabled);
                                        changed|=ImGui::SliderFloat("Width per corner",&vignette.width_percent,0,50,"%.0f%%",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Height",&vignette.height_percent,0,100,"%.0f%%",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Opacity",&vignette.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        if(ImGui::Button("Reset vignette")) {vignette={};changed=true;}
                                        ImGui::TreePop();
                                    }
                                    ImGui::TextDisabled("This background is shared by the Items and Gear pages. Map backgrounds are under Map.");
                                    ImGui::TreePop();
                                }
                                part_controls(PauseMenu::Titles);
                                ImGui::EndTabItem();
                            }
                            if(ImGui::BeginTabItem("Gear")) {
                                for(unsigned i=0;i<2;++i)if(ImGui::TreeNode(i?"Green gear panel: lower":"Green gear panel: upper")) {
                                    auto& panel=i?layout.gear_panel_lower:layout.gear_panel;
                                    changed |= ImGui::SliderFloat("X",&panel.x,-400.f,400.f,"%.1f",ImGuiSliderFlags_NoInput);
                                    changed |= ImGui::SliderFloat("Y",&panel.y,-240.f,240.f,"%.1f",ImGuiSliderFlags_NoInput);
                                    changed |= ImGui::SliderFloat("Width",&panel.width,.1f,3.f,"%.2fx",ImGuiSliderFlags_NoInput);
                                    changed |= ImGui::SliderFloat("Height",&panel.height,.05f,3.f,"%.2fx",ImGuiSliderFlags_NoInput);
                                    shadow_controls(layout.gear_shadows[i]);
                                    if(ImGui::Button("Reset")) {panel=PauseMenu::SplitGearPanel({})[i];changed=true;}
                                    ImGui::TreePop();
                                }
                                if(ImGui::TreeNode("Gear overlays")) {
                                    for(auto part:{PauseMenu::GearLeft,PauseMenu::GearRight,PauseMenu::GearBottomLeft,PauseMenu::GearBottomRight})part_controls(part);
                                    ImGui::TreePop();
                                }
                                ImGui::EndTabItem();
                            }
                            if(ImGui::BeginTabItem("Items")) {
                                part_controls(PauseMenu::Items);
                                if(ImGui::TreeNode("Arrow selection panel shadow")) {
                                    auto& shadow=layout.arrow_shadow;
                                    changed|=ImGui::Checkbox("Enabled",&shadow.enabled);
                                    changed|=ImGui::SliderFloat("Softness",&shadow.softness,1,24,"%.1f px");
                                    changed|=ImGui::SliderFloat("X offset",&shadow.x,-12,12,"%.1f px");
                                    changed|=ImGui::SliderFloat("Y offset",&shadow.y,-12,12,"%.1f px");
                                    changed|=ImGui::SliderFloat("Strength",&shadow.strength,0,1,"%.2f");
                                    if(ImGui::Button("Reset shadow")){shadow={};changed=true;}
                                    ImGui::TreePop();
                                }
                                part_controls(PauseMenu::Slots);
                                part_controls(PauseMenu::Description);
                                part_controls(PauseMenu::Preview);
                                changed|=ImGui::SliderFloat("Item rotation speed",&layout.preview_rotation_speed,.25f,5.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Rotation speed of the displayed item model. 1x is the original speed.");
                                if(ImGui::Button("Reset item rotation speed")){layout.preview_rotation_speed=1.f;changed=true;}
                                ImGui::EndTabItem();
                            }
                            if(ImGui::BeginTabItem("Map")) {
                                changed|=ImGui::Checkbox("Time-of-day map tint",&layout.world_map_time_tint);
                                if(ImGui::IsItemHovered())ImGui::SetTooltip("Colors the overworld map, backing parchments and current-location title banner with warm dawn/sunset tones and cooler darkness at night, following the in-game clock.");
                                if(ImGui::TreeNode("Overworld bottom-right vignette")) {
                                    auto& v=layout.world_prompt_vignette;
                                    changed|=ImGui::Checkbox("Enabled",&v.enabled);
                                    changed|=ImGui::ColorEdit3("Color",v.color.data());
                                    changed|=ImGui::SliderFloat("Opacity",&v.strength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::SliderFloat("Width",&v.width,8,600,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::SliderFloat("Height",&v.height,8,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::SliderFloat("Softness",&v.softness,.05f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::SliderFloat("X offset",&v.x,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::SliderFloat("Y offset",&v.y,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                    if(ImGui::Button("Reset vignette")){v=PauseMenu::Layout{}.world_prompt_vignette;changed=true;}
                                    ImGui::TreePop();
                                }
                                if(ImGui::TreeNode("Overworld parchments and outline")) {
                                    if(ImGui::TreeNode("Top-edge fade")) {
                                        auto& fade=layout.world_map_top_fade;
                                        changed|=ImGui::Checkbox("Enable fade",&fade.enabled);
                                        changed|=ImGui::ColorEdit3("Color",fade.color.data());
                                        changed|=ImGui::SliderFloat("Opacity",&fade.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Height",&fade.height,1,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::DragFloat("Y offset",&fade.y,.1f,-240,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Softness",&fade.softness,.2f,4,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        if(ImGui::Button("Reset fade")){fade={};changed=true;}
                                        ImGui::TreePop();
                                    }
                                    changed|=ImGui::Checkbox("Use saved map outline",&layout.world_map_outline);
                                    state.world_map_editor->Draw();
                                    if(ImGui::TreeNode("Map edge shadow")) {
                                        changed|=ImGui::SliderFloat("Opacity",&layout.world_map_shadow_opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Width",&layout.world_map_shadow_width,0,48,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        ImGui::TextWrapped("Follows the map outline above the parchment. Set opacity to zero to disable.");
                                        if(ImGui::Button("Reset shadow")){layout.world_map_shadow_opacity=.35f;layout.world_map_shadow_width=8;changed=true;}
                                        ImGui::TreePop();
                                    }
                                    changed|=ImGui::SliderFloat("Parchment overlap blend",&layout.world_parchment_blend,0,80,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                    changed|=ImGui::SliderFloat("Parchment darkness",&layout.world_parchment_darkness,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                    for(unsigned i=0;i<layout.world_parchments.size();++i) {
                                        ImGui::PushID(int(i));
                                        if(ImGui::TreeNode("stack","Parchment stack %u",i+1)) {
                                            auto& stack=layout.world_parchments[i];
                                            changed|=ImGui::Checkbox("Show stack",&stack.enabled);
                                            changed|=ImGui::DragFloat("X",&stack.pose.x,.1f,-400,400,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                            changed|=ImGui::DragFloat("Y",&stack.pose.y,.1f,-240,240,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                            changed|=ImGui::DragFloat("Scale",&stack.pose.scale,.005f,.1f,3,"%.3fx",ImGuiSliderFlags_AlwaysClamp);
                                            changed|=ImGui::SliderFloat("Distance between papers",&stack.spacing,0,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                            if(ImGui::Button("Reset stack")){stack=PauseMenu::Layout{}.world_parchments[i];changed=true;}
                                            ImGui::TreePop();
                                        }
                                        ImGui::PopID();
                                    }
                                    ImGui::TreePop();
                                }
                                part_controls(PauseMenu::Map);
                                part_controls(PauseMenu::WorldLocationPanel);
                                if(ImGui::TreeNode("Location label Skulltula group")) {
                                    changed|=ImGui::SliderFloat("X offset",&layout.world_skulltula_x,-200,200,"%.1f");
                                    changed|=ImGui::SliderFloat("Y offset",&layout.world_skulltula_y,-120,120,"%.1f");
                                    changed|=ImGui::SliderFloat("Scale",&layout.world_skulltula_scale,.25f,3.f,"%.2fx");
                                    if(ImGui::Button("Reset")){layout.world_skulltula_x=layout.world_skulltula_y=0;layout.world_skulltula_scale=1;changed=true;}
                                    ImGui::TreePop();
                                }
                                part_controls(PauseMenu::LocationMap);
                                part_controls(PauseMenu::LocationBanner);
                                if(ImGui::Button("Capture map banner fade (12 seconds)"))PauseHeader::ArmDiagnostics();
                                if(ImGui::TreeNode("Map backgrounds")) {
                                    for(unsigned i=0;i<layout.map_backgrounds.size();++i) {
                                        ImGui::PushID(static_cast<int>(i));
                                        if(ImGui::TreeNode(i==0?"Left stack":"Right stack")) {
                                            auto& b=layout.map_backgrounds[i];
                                            changed |= ImGui::SliderFloat("X",&b.pose.x,-400.f,400.f,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::SliderFloat("Y",&b.pose.y,-240.f,240.f,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::SliderFloat("Scale",&b.pose.scale,.1f,3.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::SliderFloat("Darkness",&b.darkness,0.f,1.f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                            if(ImGui::Button("Reset stack")) {b=PauseMenu::Layout{}.map_backgrounds[i];changed=true;}
                                            ImGui::TreePop();
                                        }
                                        ImGui::PopID();
                                    }
                                    ImGui::TextWrapped("Each stack contains two brown panels. These backgrounds appear only on the map page.");
                                    ImGui::TreePop();
                                }
                                if(ImGui::TreeNode("Dungeon map")) {
                                    if(ImGui::TreeNode("Top-edge fade")) {
                                        auto& fade=layout.dungeon_map_top_fade;
                                        changed|=ImGui::Checkbox("Enable fade",&fade.enabled);
                                        changed|=ImGui::ColorEdit3("Color",fade.color.data());
                                        changed|=ImGui::SliderFloat("Opacity",&fade.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Height",&fade.height,1,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::DragFloat("Y offset",&fade.y,.1f,-240,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Softness",&fade.softness,.2f,4,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        ImGui::TextDisabled("Below the existing shadow, banners and floor controls.");
                                        if(ImGui::Button("Reset fade")){fade={};changed=true;}
                                        ImGui::TreePop();
                                    }
                                    if(ImGui::TreeNode("Button prompt vignette")) {
                                        auto& v=layout.dungeon_prompt_vignette;
                                        changed|=ImGui::Checkbox("Enabled",&v.enabled);
                                        changed|=ImGui::SliderFloat("Strength",&v.strength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Width",&v.width,8,600,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Height",&v.height,8,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Softness",&v.softness,.05f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("X offset",&v.x,-400,400,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Y offset",&v.y,-240,240,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                        if(ImGui::Button("Reset vignette")){v={};changed=true;}
                                        ImGui::TreePop();
                                    }
                                    if(ImGui::TreeNode("Kokiri particles")) {
                                        auto& p=layout.dungeon_particles;
                                        changed|=ImGui::Checkbox("Enabled",&p.enabled);
                                        changed|=ImGui::Checkbox("Scale with dungeon collection",&layout.dungeon_particles_progress);
                                        if(layout.dungeon_particles_progress) {
                                            auto& e=layout.dungeon_particle_endpoints;
                                            changed|=ImGui::SliderInt("Starting density",&e.density_start,0,FileParticles::DungeonMaxParticles,"%d",ImGuiSliderFlags_AlwaysClamp);
                                            changed|=ImGui::SliderInt("Completed density",&e.density_end,0,FileParticles::DungeonMaxParticles,"%d",ImGuiSliderFlags_AlwaysClamp);
                                            changed|=ImGui::SliderFloat("Starting drift speed",&e.drift_start,0,6,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                            changed|=ImGui::SliderFloat("Completed drift speed",&e.drift_end,0,6,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                            if(ImGui::Button("Reset progression")){e={};changed=true;}
                                            ImGui::TextWrapped("Starting values apply at zero collection; completed values apply at full collection. Values blend between them. 1x is normal drift speed.");
                                            ImGui::TextWrapped("Counts opened chests, freestanding keys and Gold Skulltulas for this dungeon's normal or Master Quest layout. Chest contents count once; repeatable pickups are excluded.");
                                        } else {
                                            changed|=ImGui::SliderInt("Density",&p.density,0,FileParticles::DungeonMaxParticles,"%d",ImGuiSliderFlags_AlwaysClamp);
                                            changed|=ImGui::SliderFloat("Drift speed",&p.speed,0,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        }
                                        changed|=ImGui::SliderFloat("Glow strength",&p.glow,0,2,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Background light strength",&p.light_strength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Background light radius",&p.light_radius,.25f,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Size",&p.size,.25f,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Spin speed",&p.spin,0,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::Checkbox("Varied particles",&p.variations);
                                        ImGui::TextDisabled("Behind dungeon parchment and banners; independent of file select.");
                                        if(ImGui::Button("Reset particles")){p=PauseMenu::Layout{}.dungeon_particles;layout.dungeon_particles_progress=true;layout.dungeon_particle_endpoints={};changed=true;}
                                        ImGui::TreePop();
                                    }
                                    if(ImGui::Button("Capture map performance (12 seconds)"))PauseHeader::ArmDiagnostics();
                                    if(PauseHeader::DiagnosticActive())
                                        ImGui::Text("Recording map timings: %.1f seconds remaining",
                                            std::max(0.0,double(PauseHeader::diagnostic_end_ms.load()-PauseHeader::DiagnosticTime())/1000.0));
                                    ImGui::TextWrapped("Start recording, close the Editor, then open and close the dungeon map twice. Timings are written to the log.");
                                    if(ImGui::TreeNode("Top-edge shadow")) {
                                        auto& s=layout.dungeon_top_shadow;
                                        changed|=ImGui::Checkbox("Enabled",&s.enabled);
                                        changed|=ImGui::SliderFloat("Opacity",&s.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::SliderFloat("Height",&s.height,0,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        changed|=ImGui::DragFloat("Y offset",&s.y,.1f,-240,240,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                        if(ImGui::Button("Reset shadow")){s={};changed=true;}
                                        ImGui::TreePop();
                                    }
                                    if(ImGui::TreeNode("Brown panel darkness")) {
                                        changed |= ImGui::SliderFloat("Left",&layout.dungeon_background_darkness[0],0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed |= ImGui::SliderFloat("Right",&layout.dungeon_background_darkness[1],0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        ImGui::TreePop();
                                    }
                                    if(ImGui::TreeNode("Map edge shadows")) {
                                        changed |= ImGui::Checkbox("Enabled",&layout.dungeon_edge_shadows);
                                        changed |= ImGui::SliderFloat("Opacity",&layout.dungeon_edge_shadow.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed |= ImGui::SliderFloat("Length",&layout.dungeon_edge_shadow.width,.1f,4.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                        ImGui::TreePop();
                                    }
                                    if(ImGui::TreeNode("Side shadow blocks")) {
                                        for(unsigned side=0;side<2;++side) {
                                            ImGui::PushID(static_cast<int>(side));
                                            if(ImGui::TreeNode(side?"Right block":"Left block")) {
                                                auto& b=layout.dungeon_blocks[side];
                                                changed |= ImGui::Checkbox("Enabled",&b.enabled);
                                                changed |= ImGui::DragFloat("X",&b.x,.1f,-600,600,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                                changed |= ImGui::DragFloat("Y",&b.y,.1f,-600,600,"%.1f",ImGuiSliderFlags_AlwaysClamp);
                                                changed |= ImGui::DragFloat("Width",&b.width,.1f,0,600,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                                changed |= ImGui::DragFloat("Height",&b.height,.1f,0,600,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                                changed |= ImGui::DragFloat("Inward fade length",&b.fade,.1f,0,600,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                                changed |= ImGui::SliderFloat("Opacity",&b.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                                ImGui::TextDisabled(side?"Fades to the left; X follows the right brown panel.":"Fades to the right; X follows the left brown panel.");
                                                if(ImGui::Button("Reset block")) {b={};changed=true;}
                                                ImGui::TreePop();
                                            }
                                            ImGui::PopID();
                                        }
                                        ImGui::TreePop();
                                    }
                                    for(auto part:{PauseMenu::DungeonMap,PauseMenu::DungeonContents,PauseMenu::DungeonLeft,
                                                   PauseMenu::DungeonRight,PauseMenu::DungeonBanner,PauseMenu::DungeonItems,
                                                   PauseMenu::DungeonFloors,PauseMenu::DungeonDescription,
                                                   PauseMenu::DungeonLocationPaper,PauseMenu::DungeonLocationTitle,PauseMenu::DungeonSkulltula}) {
                                        ImGui::PushID(static_cast<int>(part));
                                        ImGui::SeparatorText(PauseMenu::Labels[part]);
                                        if(part==PauseMenu::DungeonSkulltula)
                                            ImGui::TextWrapped("Position relative to the parchment. Follows its position, scale and fade; draws over the banner.");
                                        auto& pose=layout[part];
                                        changed |= ImGui::DragFloat("X",&pose.x,.1f,-400.f,400.f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed |= ImGui::DragFloat("Y",&pose.y,.1f,-240.f,240.f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                        changed |= ImGui::DragFloat("Scale",&pose.scale,.005f,.1f,3.f,"%.3fx",ImGuiSliderFlags_AlwaysClamp);
                                        if(part==PauseMenu::DungeonMap) {
                                            float darkness=1-layout.dungeon_brightness;
                                            if(ImGui::SliderFloat("Darkness",&darkness,0.f,1.f,"%.2f",ImGuiSliderFlags_AlwaysClamp)) {layout.dungeon_brightness=1-darkness;changed=true;}
                                        }
                                        if(part==PauseMenu::DungeonMap && ImGui::TreeNode("Additional parchments")) {
                                            changed |= ImGui::SliderFloat("Overlap blend",&layout.parchment_blend,0,80,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                            ImGui::TextWrapped("The four pieces share one aligned grid. Each extra piece has its own position and size; overlap blend softens the paper-color joins without blurring the grid.");
                                            for(unsigned i=0;i<3;++i) {
                                                ImGui::PushID(static_cast<int>(i));
                                                if(ImGui::TreeNode("parchment","Parchment %u",i+2)) {
                                                    auto& paper=layout.dungeon_parchments[i];
                                                    changed |= ImGui::Checkbox("Enabled",&paper.enabled);
                                                    changed |= ImGui::DragFloat("X",&paper.pose.x,.1f,-400,400,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                                    changed |= ImGui::DragFloat("Y",&paper.pose.y,.1f,-240,240,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                                    changed |= ImGui::DragFloat("Scale",&paper.pose.scale,.005f,.1f,3,"%.3fx",ImGuiSliderFlags_AlwaysClamp);
                                                    if(ImGui::Button("Reset parchment")) {paper={};changed=true;}
                                                    ImGui::TreePop();
                                                }
                                                ImGui::PopID();
                                            }
                                            ImGui::TreePop();
                                        }
                                        if(part==PauseMenu::DungeonFloors) {
                                            changed |= ImGui::Checkbox("Hide floor stones",&layout.hide_dungeon_floor_stones);
                                            auto& growth=layout.dungeon_floor_growth;
                                            changed |= ImGui::Checkbox("Grow selected floor / item",&growth.enabled);
                                            ImGui::BeginDisabled(!growth.enabled);
                                            changed |= ImGui::SliderFloat("Selected size",&growth.size,1.f,2.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::SliderFloat("Growth speed",&growth.speed,.1f,5.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                            ImGui::EndDisabled();
                                            changed |= ImGui::SliderFloat("Cursor scale",&layout.dungeon_cursor_scale,.25f,3.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                        }
                                        if(part==PauseMenu::DungeonFloors && ImGui::TreeNode("Floor-list line")) {
                                            auto& line=layout.dungeon_floor_line;
                                            changed |= ImGui::Checkbox("Enabled",&line.enabled);
                                            changed |= ImGui::DragFloat("X",&line.pose.x,.1f,-400,400,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::DragFloat("Y",&line.pose.y,.1f,-240,240,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::DragFloat("Scale",&line.pose.scale,.005f,.1f,3,"%.3fx",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::DragFloat("Width",&line.width,.1f,0,80,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::DragFloat("Maximum height",&line.height,.1f,0,600,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::SliderFloat("Darkness / opacity",&line.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                            changed |= ImGui::SliderFloat("Roundness",&line.roundness,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                                            ImGui::TextDisabled("Fits the available floors. X/Y are offsets from the floor list.");
                                            if(ImGui::Button("Reset line")) {line={};changed=true;}
                                            ImGui::TreePop();
                                        }
                                        if(part==PauseMenu::DungeonItems || part==PauseMenu::DungeonFloors)
                                            changed |= ImGui::SliderFloat("Spacing",&layout.dungeon_spacing[part==PauseMenu::DungeonFloors?1:0],.2f,3.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                                        if(part==PauseMenu::DungeonLeft || part==PauseMenu::DungeonRight) {
                                            color_controls(part==PauseMenu::DungeonLeft?layout.dungeon_left_overlay:layout.dungeon_right_overlay);
                                            shadow_controls(part==PauseMenu::DungeonLeft?layout.dungeon_left_shadow:layout.dungeon_right_shadow);
                                        }
                                        if(ImGui::Button("Reset")) {
                                            pose={};
                                            if(part==PauseMenu::DungeonMap)layout.dungeon_brightness=1;
                                            if(part==PauseMenu::DungeonFloors) {layout.dungeon_floor_growth={};layout.dungeon_cursor_scale=1;}
                                            if(part==PauseMenu::DungeonItems || part==PauseMenu::DungeonFloors)layout.dungeon_spacing[part==PauseMenu::DungeonFloors?1:0]=1;
                                            changed=true;
                                        }
                                        ImGui::PopID();
                                    }
                                    ImGui::TreePop();
                                }
                                if(ImGui::TreeNode("Map button prompts")) {
                                    prompt_controls(PausePrompts::MapMove,false);
                                    prompt_controls(PausePrompts::MapReturn,false);
                                    if(ImGui::TreeNode("Side navigation prompts")) {
                                        if(ImGui::TreeNode("Navigation button animation")) {
                                            edited|=ImGui::SliderFloat("Distance",&prompts.navigation_distance,0,30,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
                                            edited|=ImGui::SliderFloat("Speed",&prompts.navigation_speed,0,5,"%.2f cycles/s",ImGuiSliderFlags_AlwaysClamp);
                                            ImGui::TextWrapped("Shared by Items → Map and Map → Items. Distance is total vertical travel. Set distance or speed to zero to stop. Captions stay still.");
                                            if(ImGui::Button("Reset animation")) {prompts.navigation_distance=PausePrompts::Layout{}.navigation_distance;prompts.navigation_speed=PausePrompts::Layout{}.navigation_speed;edited=true;}
                                            ImGui::TreePop();
                                        }
                                        prompt_controls(PausePrompts::ItemsToMap,false);
                                        prompt_controls(PausePrompts::MapToItems,false);
                                        ImGui::TreePop();
                                    }
                                    ImGui::TreePop();
                                }
                                ImGui::EndTabItem();
                            }
                            if(ImGui::BeginTabItem("Card tabs")) {
                                ImGui::TextDisabled("These are the separate bottom card controls. Page elements are in the Gear, Items, and Map tabs.");
                                changed|=ImGui::Checkbox("Gear / Items stone shadows",&layout.tab_shadow.enabled);
                                if(layout.tab_shadow.enabled && ImGui::TreeNode("Stone shadow settings")) {
                                    changed|=ImGui::SliderFloat("Softness",&layout.tab_shadow.softness,.5f,12,"%.1f px");
                                    changed|=ImGui::SliderFloat("Strength",&layout.tab_shadow.strength,0,1,"%.2f");
                                    changed|=ImGui::SliderFloat("X offset",&layout.tab_shadow.x,-12,12,"%.1f px");
                                    changed|=ImGui::SliderFloat("Y offset",&layout.tab_shadow.y,-12,12,"%.1f px");
                                    ImGui::TreePop();
                                }
                                part_controls(PauseMenu::Back);
                                part_controls(PauseMenu::GearTab);
                                part_controls(PauseMenu::ItemsTab);
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                        if(edited)PausePrompts::SetLayout(prompts);
                        ImGui::PopID();if(changed)PauseMenu::SetLayout(layout);
                    }
                    if (ImGui::CollapsingHeader("Textboxes")) {
                        auto layout = Textboxes::GetLayout();
                        bool changed = false;
                        ImGui::PushID("Textbox layout");
                        ImGui::BeginDisabled(Interpolation::SurfaceCapture::active.load() || state.surface_export.valid());
                        if(ImGui::Button("Capture textbox opening (10 seconds)")) {
                            const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            state.surface_capture_path=FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir)+
                                "oot3d-captures/animated-surfaces-"+std::to_string(stamp)+".json";
                            Interpolation::SurfaceCapture::Start();state.surface_capture_status.clear();
                        }
                        ImGui::EndDisabled();
                        ImGui::TextWrapped("Close F1 during the countdown, then open and close a dialogue or sign box a few times.");
                        if(!state.surface_capture_status.empty())ImGui::TextWrapped("%s",state.surface_capture_status.c_str());
                        changed |= ImGui::Checkbox("MM-style closing animation", &layout.mm_close);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Briefly swells the textbox background, then shrinks it away like Majora's Mask. Does not delay dialogue completion.");
                        changed |= ImGui::Checkbox("Smooth text reveal", &layout.smooth_reveal);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Spreads new letters across interpolated frames while keeping the original typing speed. Requires a frame rate above Original.");
                        ImGui::SeparatorText("Opening animation");
                        changed |= ImGui::SliderFloat("Global opening speed", &layout.opening_speed, .25f, 3.f, "%.2fx", ImGuiSliderFlags_NoInput);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Controls the opening pop for dialogue, signs and pause descriptions. 1x is native speed; does not change text reveal or closing speed.");
                        if(ImGui::Button("Reset opening speed")){layout.opening_speed=1;changed=true;}
                        ImGui::SeparatorText("Dialogue box");
                        changed |= ImGui::SliderFloat("X", &layout.x, -400.f, 400.f, "%.1f", ImGuiSliderFlags_NoInput);
                        changed |= ImGui::SliderFloat("Y", &layout.y, -240.f, 240.f, "%.1f", ImGuiSliderFlags_NoInput);
                        changed |= ImGui::SliderFloat("Scale", &layout.scale, .1f, 3.f, "%.2fx", ImGuiSliderFlags_NoInput);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Scales the whole dialogue box uniformly, preserving letter proportions.");
                        changed |= ImGui::SliderFloat("Background width", &layout.width, 1.f, 3.f, "%.2fx", ImGuiSliderFlags_NoInput);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Extends dialogue and pause-menu description backgrounds equally to the left and right. Does not change lettering size or spacing.");
                        if (ImGui::Button("Reset box")) { layout.x = layout.y = 0; layout.width = layout.scale = 1; changed = true; }
                        ImGui::SeparatorText("Sign textboxes");
                        changed |= ImGui::SliderFloat("Sign Y offset", &layout.sign_y, -240.f, 240.f, "%.1f", ImGuiSliderFlags_NoInput);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Moves wooden sign boxes, text and arrows together. Negative values move upward; ordinary dialogue is unaffected.");
                        if(ImGui::Button("Reset sign position")){layout.sign_y=0;changed=true;}
                        ImGui::SeparatorText("Textbox indicator");
                        ImGui::PushID("Advance arrow");
                        auto& animation=layout.indicator_animation;
                        changed |= ImGui::Checkbox("Show indicator",&animation.visible);
                        changed |= ImGui::Checkbox("Spring when advancing text",&animation.spring);
                        ImGui::BeginDisabled(!animation.spring);
                        changed |= ImGui::SliderFloat("Small size",&animation.small,.2f,1.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                        changed |= ImGui::SliderFloat("Spring height",&animation.big,1.f,1.8f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                        changed |= ImGui::SliderFloat("Spring speed",&animation.speed,.25f,3.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
                        ImGui::EndDisabled();
                        changed |= ImGui::SliderFloat("X", &layout.indicator.x, -400.f, 400.f, "%.1f", ImGuiSliderFlags_NoInput);
                        changed |= ImGui::SliderFloat("Y", &layout.indicator.y, -240.f, 240.f, "%.1f", ImGuiSliderFlags_NoInput);
                        changed |= ImGui::SliderFloat("Scale", &layout.indicator.scale, .1f, 3.f, "%.2fx", ImGuiSliderFlags_NoInput);
                        if (ImGui::Button("Reset indicator")) { layout.indicator = {}; animation={}; changed = true; }
                        ImGui::PopID();
                        ImGui::PopID();
                        if (changed) Textboxes::SetLayout(layout);
                    }
                    if (ImGui::CollapsingHeader("Title Screen", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto layout = TitleScreen::GetLayout();
                        bool changed = false;
                        constexpr std::array names{"Logo", "Copyright", "Press Start", "Additional PRESS START"};
                        for (unsigned i = 0; i < layout.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::SeparatorText(names[i]);
                            auto& pose = layout[i];
                            changed |= ImGui::SliderFloat("X", &pose.x, -400.f, 400.f, "%.1f", ImGuiSliderFlags_NoInput);
                            changed |= ImGui::SliderFloat("Y", &pose.y, -240.f, 240.f, "%.1f", ImGuiSliderFlags_NoInput);
                            changed |= ImGui::SliderFloat("Scale", &pose.scale, .1f, 3.f, "%.2fx", ImGuiSliderFlags_NoInput);
                            if(i==3) {
                                changed |= ImGui::ColorEdit3("Text color",layout.text_color.data());
                                changed |= ImGui::DragFloat("Letter spacing",&layout.letter_spacing,.05f,-4.f,12.f,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
                            }
                            if (ImGui::Button("Reset")) {
                                pose = TitleScreen::DefaultLayout[i];
                                if(i==3) {
                                    layout.text_color=TitleScreen::DefaultLayout.text_color;
                                    layout.letter_spacing=TitleScreen::DefaultLayout.letter_spacing;
                                }
                                changed = true;
                            }
                            ImGui::PopID();
                        }
                        if (changed) TitleScreen::SetLayout(layout);
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        if(defaults) defaults->Flush();
        if (!open) visible = false;
    }
    const auto surface_end=Interpolation::SurfaceCapture::ends.load();
    const auto surface_now=Interpolation::SurfaceCapture::Now();
    if(surface_end && surface_now<surface_end+4000) {
        ImGui::SetNextWindowPos({12.f*ui_scale,55.f*ui_scale},ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(.85f);
        ImGui::Begin("Surface capture status",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|
            ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav);
        const auto begins=Interpolation::SurfaceCapture::begins.load();
        if(surface_now<begins)ImGui::Text("Capture starts in %.1f s",float(begins-surface_now)/1000.f);
        else if(surface_now<surface_end)ImGui::Text("Recording textbox/particles... %.1f s",float(surface_end-surface_now)/1000.f);
        else ImGui::TextUnformatted("Recording complete");
        ImGui::End();
    }
    const auto banner_end=PauseHeader::diagnostic_end_ms.load();
    const auto routing_end=HUD::RoutingCapture::ends.load(),routing_now=HUD::RoutingCapture::Now();
    if(routing_end && routing_now<routing_end+4000) {
        ImGui::SetNextWindowPos({12.f*ui_scale,98.f*ui_scale},ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(.85f);
        ImGui::Begin("HUD routing capture status",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|
            ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav);
        const auto begins=HUD::RoutingCapture::begins.load();
        if(routing_now<begins)ImGui::Text("HUD capture starts in %.1f s",float(begins-routing_now)/1000);
        else if(routing_now<routing_end)ImGui::Text("Recording HUD routing... %.1f s",float(routing_end-routing_now)/1000);
        else ImGui::TextUnformatted("HUD capture saved to game log");
        ImGui::End();
    }
    const auto banner_now=PauseHeader::DiagnosticTime();
    if(banner_end && banner_now<banner_end+4000) {
        ImGui::SetNextWindowPos({12.f*ui_scale,12.f*ui_scale},ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(.85f);
        ImGui::Begin("Banner capture status",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|
            ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav);
        if(banner_now<banner_end && PauseHeader::diagnostic_lines.load()<2400)
            ImGui::Text("Recording banner motion... %.1f s",float(banner_end-banner_now)/1000.f);
        else ImGui::TextUnformatted("Banner capture complete — recorded in the game log");
        ImGui::End();
    }
    text_input.store(visible.load() && io.WantTextInput);
    ImGui::Render();
    if (ImGui::GetDrawData()->TotalVtxCount == 0) return;
    const vk::RenderPassBeginInfo begin{.renderPass = state.renderpass, .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {width, height}}};
    command.beginRenderPass(begin, vk::SubpassContents::eInline);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command);
    command.endRenderPass();
}
} // namespace OOT3D
