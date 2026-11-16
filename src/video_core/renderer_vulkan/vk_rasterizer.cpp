#ifdef ENABLE_OOT3D_PRESENTATION
#include "oot3d/pickup_fog.h"
#include "oot3d/interpolation_timing.h"
#endif
#include "mm3d/effect_capture.h"
#ifdef ENABLE_OOT3D_PRESENTATION
#include "oot3d/first_person_diagnostics.h"
#include "oot3d/qol.h"
#include "oot3d/actor_capture.h"
#include "oot3d/surface_capture.h"
#endif
#ifdef ENABLE_OOT3D_PRESENTATION
#include "oot3d/hd_font.h"
#include "common/cityhash.h"
#include "oot3d/texture_picker.h"
#include "oot3d/hud_routing.h"
#include "oot3d/magic_meter.h"
#include "oot3d/widescreen.h"
#include "oot3d/lens.h"
#include "core/core_timing.h"
#endif
// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/literals.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "core/memory.h"
#include "video_core/pica/pica_core.h"
#include "video_core/pica/vertex_loader.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include <cstdlib>
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture/texture_decode.h"

#ifdef ENABLE_MM3D_PRESENTATION
#include "mm3d/diagnostics.h"
#include "mm3d/cpu_interpolation.h"
#include "mm3d/glare_interpolation.h"
#include "mm3d/ripple_interpolation.h"
#include "mm3d/splash_interpolation.h"
#include "mm3d/input_latency.h"
#include "mm3d/interpolation_performance.h"
#include "mm3d/screen_shrink.h"
#include "mm3d/link_pose_capture.h"
#include "mm3d/bloom_sampling.h"
#include "mm3d/prompt_layer.h"
#include "mm3d/minimap.h"
#include "mm3d/notebook.h"
#include "mm3d/draw_probe.h"
#include "mm3d/texture_picker.h"
#include "mm3d/trace.h"
#include "mm3d/camera_live.h"
#include <chrono>
#include "mm3d/widescreen.h"
#include "mm3d/widescreen_probe.h"
#endif

namespace Vulkan {

namespace {

MICROPROFILE_DEFINE(Vulkan_VS, "Vulkan", "Vertex Shader Setup", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(Vulkan_GS, "Vulkan", "Geometry Shader Setup", MP_RGB(128, 192, 128));
MICROPROFILE_DEFINE(Vulkan_Drawing, "Vulkan", "Drawing", MP_RGB(128, 128, 192));

using TriangleTopology = Pica::PipelineRegs::TriangleTopology;
using VideoCore::SurfaceType;

using namespace Common::Literals;
using namespace Pica::Shader::Generator;

constexpr u64 STREAM_BUFFER_SIZE = 64_MiB;
constexpr u64 UNIFORM_BUFFER_SIZE = 8_MiB;
constexpr u64 TEXTURE_BUFFER_SIZE = 2_MiB;

constexpr vk::BufferUsageFlags BUFFER_USAGE =
    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer;

struct DrawParams {
    u32 vertex_count;
    s32 vertex_offset;
    u32 binding_count;
    std::array<u32, 16> bindings;
    bool is_indexed;
};

[[nodiscard]] u64 TextureBufferSize(const Instance& instance) {
    // Use the smallest texel size from the texel views
    // which corresponds to eR32G32Sfloat
    const u64 max_size = instance.MaxTexelBufferElements() * 8;
    return std::min(max_size, TEXTURE_BUFFER_SIZE);
}

} // Anonymous namespace

RasterizerVulkan::RasterizerVulkan(Memory::MemorySystem& memory, Pica::PicaCore& pica,
                                   VideoCore::CustomTexManager& custom_tex_manager,
                                   VideoCore::RendererBase& renderer,
                                   Frontend::EmuWindow& emu_window, const Instance& instance,
                                   Scheduler& scheduler, RenderManager& renderpass_cache,
                                   DescriptorUpdateQueue& update_queue_, u32 image_count)
    : RasterizerAccelerated{memory, pica}, instance{instance}, scheduler{scheduler},
      renderpass_cache{renderpass_cache}, update_queue{update_queue_},
      pipeline_cache{instance, scheduler, renderpass_cache, update_queue},
      runtime{instance, scheduler, renderpass_cache, update_queue, image_count},
      res_cache{memory, custom_tex_manager, runtime, regs, renderer},
      stream_buffer{instance, scheduler, BUFFER_USAGE, STREAM_BUFFER_SIZE},
      uniform_buffer{instance, scheduler, vk::BufferUsageFlagBits::eUniformBuffer,
                     UNIFORM_BUFFER_SIZE},
      texture_buffer{instance, scheduler, vk::BufferUsageFlagBits::eUniformTexelBuffer,
                     TextureBufferSize(instance)},
      texture_lf_buffer{instance, scheduler, vk::BufferUsageFlagBits::eUniformTexelBuffer,
                        TextureBufferSize(instance)},
      async_shaders{Settings::values.async_shader_compilation.GetValue()} {

#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::SunVisibility::backend.store(this);
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    ResetPause();
    MM3D::PauseCapture::backend.store(this);
    const char* depth_option = std::getenv("DEKUI_ASYNC_VISIBILITY_DEPTH");
    visibility_depth_enabled = !depth_option || std::string_view{depth_option} != "0";
    const char* refresh_option = std::getenv("DEKUI_REFRESH_VISIBILITY_DEPTH");
    visibility_depth_refresh_enabled = !refresh_option || std::string_view{refresh_option} != "0";
    MM3D::VisibilityDepth::backend.store(this);
    LOG_INFO(Render_Vulkan, "Bounded visibility depth snapshots enabled={} background_refresh={}",
             visibility_depth_enabled, visibility_depth_refresh_enabled);
#endif
    vertex_buffers.fill(stream_buffer.Handle());

    // Query uniform buffer alignment.
    uniform_buffer_alignment = instance.UniformMinAlignment();
    uniform_size_aligned_vs_pica =
        Common::AlignUp<u32>(sizeof(VSPicaUniformData), uniform_buffer_alignment);
    uniform_size_aligned_vs = Common::AlignUp<u32>(sizeof(VSUniformData), uniform_buffer_alignment);
    constexpr u32 fs_size=sizeof(FSUniformData)
#ifdef ENABLE_MM3D_PRESENTATION
        +sizeof(MM3D::Atlas::Uniforms)
#endif
        ;
    uniform_size_aligned_fs = Common::AlignUp<u32>(fs_size, uniform_buffer_alignment);

    // Define vertex layout for software shaders
    MakeSoftwareVertexLayout();
    pipeline_info.state.vertex_layout = software_layout;

    const vk::Device device = instance.GetDevice();
    texture_lf_view = device.createBufferViewUnique({
        .buffer = texture_lf_buffer.Handle(),
        .format = vk::Format::eR32G32Sfloat,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    });
    texture_rg_view = device.createBufferViewUnique({
        .buffer = texture_buffer.Handle(),
        .format = vk::Format::eR32G32Sfloat,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    });
    texture_rgba_view = device.createBufferViewUnique({
        .buffer = texture_buffer.Handle(),
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    });

    scheduler.RegisterOnSubmit([&renderpass_cache] { renderpass_cache.EndRendering(); });

    // Prepare the static buffer descriptor set.
    const auto buffer_set = pipeline_cache.Acquire(DescriptorHeapType::Buffer);
    update_queue.AddBuffer(buffer_set, 0, uniform_buffer.Handle(), 0, sizeof(VSPicaUniformData));
    update_queue.AddBuffer(buffer_set, 1, uniform_buffer.Handle(), 0, sizeof(VSUniformData));
    update_queue.AddBuffer(buffer_set, 2, uniform_buffer.Handle(), 0, fs_size);
    update_queue.AddTexelBuffer(buffer_set, 3, *texture_lf_view);
    update_queue.AddTexelBuffer(buffer_set, 4, *texture_rg_view);
    update_queue.AddTexelBuffer(buffer_set, 5, *texture_rgba_view);

    const auto texture_set = pipeline_cache.Acquire(DescriptorHeapType::Texture);
    Surface& null_surface = res_cache.GetSurface(VideoCore::NULL_SURFACE_ID);
    Sampler& null_sampler = res_cache.GetSampler(VideoCore::NULL_SAMPLER_ID);

    // Prepare texture and utility descriptor sets.
    for (u32 i = 0; i < 3; i++) {
        update_queue.AddImageSampler(texture_set, i, 0, null_surface.ImageView(),
                                     null_sampler.Handle());
    }

    const auto utility_set = pipeline_cache.Acquire(DescriptorHeapType::Utility);
    update_queue.AddStorageImage(utility_set, 0, null_surface.StorageView());
    update_queue.AddImageSampler(utility_set, 1, 0, null_surface.ImageView(),
                                 null_sampler.Handle());
    update_queue.Flush();
}

RasterizerVulkan::~RasterizerVulkan() {
#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::SunVisibility::Backend* sun_expected = this;
    OOT3D::SunVisibility::backend.compare_exchange_strong(sun_expected, nullptr);
    // Release pending snapshot buffers while scheduler/runtime still exist.
    oot_sun_depth.reset();
#endif
#ifdef ENABLE_OOT3D_PRESENTATION
    // Cached font images may still be referenced by queued command buffers.
    if (!hd_fonts.Empty() || oot_name_renderer) scheduler.Finish();
    oot_name_renderer.reset();
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    MM3D::PauseCapture::Backend* expected=this;
    MM3D::PauseCapture::backend.compare_exchange_strong(expected,nullptr);
    MM3D::VisibilityDepth::Backend* depth_expected = this;
    MM3D::VisibilityDepth::backend.compare_exchange_strong(depth_expected, nullptr);
    runtime.Finish();
#endif
}

void RasterizerVulkan::TickFrame() {
#ifdef ENABLE_OOT3D_PRESENTATION
    const auto hd_layout=OOT3D::HUD::GetEffectiveLayout();
    if (!OOT3D::HDFont::enabled.load() && (!hd_layout.hd_numbers ||
        (!hd_layout.hd_rupee_numbers && !hd_layout.hd_timer_numbers &&
         !hd_layout.hd_ammo_numbers && !hd_layout.hd_score_numbers && !hd_layout.hd_skulltula_numbers)) && !hd_fonts.Empty())
        hd_fonts.Collect(runtime, true);
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    MM3D::Widescreen::corrected_draws.store(MM3D::Widescreen::frame_corrected_draws);
    MM3D::Widescreen::frame_corrected_draws=0;
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    MM3D::CameraMidpoint::copied_draws=midpoint_copies;
    MM3D::CameraMidpoint::rejected_draws=midpoint_rejected;
    MM3D::CameraMidpoint::fallback_draws=midpoint_fallback;
    midpoint_copies=midpoint_rejected=midpoint_fallback=0;
    if(!MM3D::CameraMidpoint::enabled.load()) {
        midpoint_history.Reset();world_map_history.Reset();glare_cpu_history.Reset();message_history.Reset();message_override.reset();item_slide_history.Reset();item_slide_override.reset();file_select_history.Reset();file_select_override.reset();inventory_history.Reset();banner_history.Reset();magic_meter_history.Reset();heart_history.Reset();lens_history.Reset();letterbox_history.Reset();inventory_override.reset();
    }
    MM3D::Minimap::NextFrame();
    MM3D::Profiles::CompleteDetectionFrame();
    const auto preview_now=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    for (auto& [id,layer] : prompt_layers) {
        if(id!="@camera_midpoint" && id!="@camera_midpoint_fade" && id!="@camera_midpoint_hud") {
            layer->NextFrame();
        } else if(!MM3D::CameraMidpoint::enabled.load() ||
                  midpoint_generation!=MM3D::CameraMidpoint::generation.load() ||
                  preview_now-midpoint_last_draw_ms>200) {
            layer->NextFrame();
            midpoint_target_frame.reset();
        }
    }
#endif
    scheduler.WaitWorker();
    res_cache.TickFrame();
}

void RasterizerVulkan::LoadDefaultDiskResources(
    const std::atomic_bool& stop_loading, const VideoCore::DiskResourceLoadCallback& callback) {

    u64 program_id;
    if (Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id) !=
        Loader::ResultStatus::Success) {
        program_id = 0;
    }

    if (callback) {
        callback(VideoCore::LoadCallbackStage::Prepare, 0, 0, "");
    }

    pipeline_cache.SetProgramID(program_id);
    pipeline_cache.SetAccurateMul(accurate_mul);
    pipeline_cache.LoadCache(stop_loading, callback);

    if (callback) {
        callback(VideoCore::LoadCallbackStage::Complete, 0, 0, "");
    }
}

void RasterizerVulkan::SyncDrawState() {
    SyncDrawUniforms();

    // SyncCullMode();
    pipeline_info.state.rasterization.cull_mode.Assign(regs.rasterizer.cull_mode);
    // If the framebuffer is flipped, request to also flip vulkan viewport
    const bool is_flipped = regs.framebuffer.framebuffer.IsFlipped();
    pipeline_info.state.rasterization.flip_viewport.Assign(is_flipped);
    // SyncBlendEnabled();
    pipeline_info.state.blending.blend_enable = regs.framebuffer.output_merger.alphablend_enable;
    // SyncBlendFuncs();
    pipeline_info.state.blending.color_blend_eq.Assign(
        regs.framebuffer.output_merger.alpha_blending.blend_equation_rgb);
    pipeline_info.state.blending.alpha_blend_eq.Assign(
        regs.framebuffer.output_merger.alpha_blending.blend_equation_a);
    pipeline_info.state.blending.src_color_blend_factor.Assign(
        regs.framebuffer.output_merger.alpha_blending.factor_source_rgb);
    pipeline_info.state.blending.dst_color_blend_factor.Assign(
        regs.framebuffer.output_merger.alpha_blending.factor_dest_rgb);
    pipeline_info.state.blending.src_alpha_blend_factor.Assign(
        regs.framebuffer.output_merger.alpha_blending.factor_source_a);
    pipeline_info.state.blending.dst_alpha_blend_factor.Assign(
        regs.framebuffer.output_merger.alpha_blending.factor_dest_a);
    // SyncBlendColor();
    pipeline_info.dynamic_info.blend_color = regs.framebuffer.output_merger.blend_const.raw;
    // SyncLogicOp();
    // SyncColorWriteMask();
    pipeline_info.state.blending.logic_op = regs.framebuffer.output_merger.logic_op;

    const u32 color_mask = regs.framebuffer.framebuffer.allow_color_write != 0
                               ? (regs.framebuffer.output_merger.depth_color_mask >> 8) & 0xF
                               : 0;
    pipeline_info.state.blending.color_write_mask = color_mask;

    // SyncStencilTest();
    const auto& stencil_test = regs.framebuffer.output_merger.stencil_test;
    const bool test_enable = stencil_test.enable && regs.framebuffer.framebuffer.depth_format ==
                                                        Pica::FramebufferRegs::DepthFormat::D24S8;

    pipeline_info.state.depth_stencil.stencil_test_enable.Assign(test_enable);
    pipeline_info.state.depth_stencil.stencil_fail_op.Assign(stencil_test.action_stencil_fail);
    pipeline_info.state.depth_stencil.stencil_pass_op.Assign(stencil_test.action_depth_pass);
    pipeline_info.state.depth_stencil.stencil_depth_fail_op.Assign(stencil_test.action_depth_fail);
    pipeline_info.state.depth_stencil.stencil_compare_op.Assign(stencil_test.func);
    pipeline_info.dynamic_info.stencil_reference = stencil_test.reference_value;
    pipeline_info.dynamic_info.stencil_compare_mask = stencil_test.input_mask;
    // SyncStencilWriteMask();
    pipeline_info.dynamic_info.stencil_write_mask =
        (regs.framebuffer.framebuffer.allow_depth_stencil_write != 0)
            ? static_cast<u32>(regs.framebuffer.output_merger.stencil_test.write_mask)
            : 0;
    // SyncDepthTest();
    const bool test_enabled = regs.framebuffer.output_merger.depth_test_enable == 1 ||
                              regs.framebuffer.output_merger.depth_write_enable == 1;
    const auto compare_op = regs.framebuffer.output_merger.depth_test_enable == 1
                                ? regs.framebuffer.output_merger.depth_test_func.Value()
                                : Pica::FramebufferRegs::CompareFunc::Always;

    pipeline_info.state.depth_stencil.depth_test_enable.Assign(test_enabled);
    pipeline_info.state.depth_stencil.depth_compare_op.Assign(compare_op);
    // SyncDepthWriteMask();
    const bool write_enable = (regs.framebuffer.framebuffer.allow_depth_stencil_write != 0 &&
                               regs.framebuffer.output_merger.depth_write_enable);
    pipeline_info.state.depth_stencil.depth_write_enable.Assign(write_enable);
}

void RasterizerVulkan::SetupVertexArray() {
    const auto [vs_input_index_min, vs_input_index_max, vs_input_size] = vertex_info;
    auto [array_ptr, array_offset, invalidate] = stream_buffer.Map(vs_input_size, 16);

    /**
     * The Nintendo 3DS has 12 attribute loaders which are used to tell the GPU
     * how to interpret vertex data. The program firsts sets GPUREG_ATTR_BUF_BASE to the base
     * address containing the vertex array data. The data for each attribute loader (i) can be found
     * by adding GPUREG_ATTR_BUFi_OFFSET to the base address. Attribute loaders can be thought
     * as something analogous to Vulkan bindings. The user can store attributes in separate loaders
     * or interleave them in the same loader.
     **/
    const auto& vertex_attributes = regs.pipeline.vertex_attributes;
    const PAddr base_address = vertex_attributes.GetPhysicalBaseAddress(); // GPUREG_ATTR_BUF_BASE
    const u32 stride_alignment = instance.GetMinVertexStrideAlignment();

    VertexLayout& layout = pipeline_info.state.vertex_layout;
    layout.binding_count = 0;
    layout.attribute_count = 16;
    enable_attributes.fill(false);

    u32 buffer_offset = 0;
    for (const auto& loader : vertex_attributes.attribute_loaders) {
        if (loader.component_count == 0 || loader.byte_count == 0) {
            continue;
        }

        // Analyze the attribute loader by checking which attributes it provides
        u32 offset = 0;
        for (u32 comp = 0; comp < loader.component_count && comp < 12; comp++) {
            const u32 attribute_index = loader.GetComponent(comp);
            if (attribute_index >= 12) {
                // Attribute ids 12, to 15 signify 4, 8, 12 and 16-byte paddings respectively.
                offset = Common::AlignUp(offset, 4);
                offset += (attribute_index - 11) * 4;
                continue;
            }

            const u32 size = vertex_attributes.GetNumElements(attribute_index);
            if (size == 0) {
                continue;
            }

            offset =
                Common::AlignUp(offset, vertex_attributes.GetElementSizeInBytes(attribute_index));

            const u32 input_reg = regs.vs.GetRegisterForAttribute(attribute_index);
            const auto format = vertex_attributes.GetFormat(attribute_index);

            VertexAttribute& attribute = layout.attributes[input_reg];
            attribute.binding.Assign(layout.binding_count);
            attribute.location.Assign(input_reg);
            attribute.offset.Assign(offset);
            attribute.type.Assign(format);
            attribute.size.Assign(size);

            enable_attributes[input_reg] = true;
            offset += vertex_attributes.GetStride(attribute_index);
        }

        const PAddr data_addr =
            base_address + loader.data_offset + (vs_input_index_min * loader.byte_count);
        const u32 vertex_num = vs_input_index_max - vs_input_index_min + 1;
        u32 data_size = loader.byte_count * vertex_num;
#ifdef ENABLE_MM3D_PRESENTATION
        MM3D::InterpolationPerformance::ReadbackScope readback{"vertex data",data_addr,data_size};
#endif
        res_cache.FlushRegion(data_addr, data_size);

        const MemoryRef src_ref = memory.GetPhysicalRef(data_addr);
        if (src_ref.GetSize() < data_size) {
            LOG_ERROR(Render_Vulkan,
                      "Vertex buffer size {} exceeds available space {} at address {:#016X}",
                      data_size, src_ref.GetSize(), data_addr);
        }

        const u8* src_ptr = src_ref.GetPtr();
        u8* dst_ptr = array_ptr + buffer_offset;

        // Align stride up if required by Vulkan implementation.
        const u32 aligned_stride =
            Common::AlignUp(static_cast<u32>(loader.byte_count), stride_alignment);
        if (aligned_stride == loader.byte_count) {
            std::memcpy(dst_ptr, src_ptr, data_size);
        } else {
            for (std::size_t vertex = 0; vertex < vertex_num; vertex++) {
                std::memcpy(dst_ptr + vertex * aligned_stride, src_ptr + vertex * loader.byte_count,
                            loader.byte_count);
            }
        }

        // Create the binding associated with this loader
        VertexBinding& binding = layout.bindings[layout.binding_count];
        binding.binding.Assign(layout.binding_count);
        binding.fixed.Assign(0);
        // Will be adjusted on pipeline build, to keep the info transferable.
        binding.byte_count.Assign(loader.byte_count);

        // Keep track of the binding offsets so we can bind the vertex buffer later
        binding_offsets[layout.binding_count++] = static_cast<u32>(array_offset + buffer_offset);
        buffer_offset += Common::AlignUp(aligned_stride * vertex_num, 4);
    }

    stream_buffer.Commit(buffer_offset);

#ifdef ENABLE_MM3D_PRESENTATION
    glare_vertex_offset.reset();
    const auto& merger=regs.framebuffer.output_merger;
    const auto& loader=vertex_attributes.attribute_loaders[0];
    const auto& uv=vertex_attributes.attribute_loaders[1];
    if(MM3D::CameraMidpoint::present_60.load() &&
       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL &&
       regs.pipeline.num_vertices==72 && vs_input_index_min==0 && vs_input_index_max==48 &&
       loader.component_count==1 && loader.GetComponent(0)==0 && loader.byte_count==12 &&
       uv.component_count==1 && uv.GetComponent(0)==1 && uv.byte_count==8 &&
       vertex_attributes.GetFormat(0)==Pica::PipelineRegs::VertexAttributeFormat::FLOAT &&
       vertex_attributes.GetNumElements(0)==3 && regs.vs.GetRegisterForAttribute(0)==0 &&
       merger.alphablend_enable && merger.depth_color_mask==3856 &&
       merger.alpha_blending.factor_source_rgb==Pica::FramebufferRegs::BlendFactor::SourceAlpha &&
       merger.alpha_blending.factor_dest_rgb==Pica::FramebufferRegs::BlendFactor::One &&
       merger.alpha_blending.blend_equation_rgb==Pica::FramebufferRegs::BlendEquation::Add &&
       pica.vs_setup.uniforms.f[3].w.ToFloat32()==1.f) {
        const u64 address=u64(base_address)+loader.data_offset;
        if(address<=0xffffffff) {
            const auto src=memory.GetPhysicalRef(static_cast<PAddr>(address));
            MM3D::GlareInterpolation::Points points;
            if(src.GetSize()>=sizeof(points)) {
                std::memcpy(points.data(),src.GetPtr(),sizeof(points));
                if(auto blended=glare_history.Apply(MM3D::guest_frames.load(),
                    regs.texturing.texture0.GetPhysicalAddress(),MM3D::CameraMidpoint::generation.load(),points)) {
                    const auto stride=Common::AlignUp(12u,stride_alignment);
                    auto [dst,offset,unused]=stream_buffer.Map(stride*49,16);
                    for(unsigned i=0;i<49;++i)std::memcpy(dst+i*stride,(*blended)[i].data(),12);
                    stream_buffer.Commit(stride*49);
                    glare_vertex_offset=static_cast<u32>(offset);
                }
            }
        }
    }
    ripple_offsets.reset();
    const auto& color_loader=vertex_attributes.attribute_loaders[1];
    const auto& tex_loader=vertex_attributes.attribute_loaders[2];
    const unsigned ripple_count=vs_input_index_max+1;
    if(MM3D::CameraMidpoint::present_60.load() &&
       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL &&
       vs_input_index_min==0 && ripple_count>=9 && ripple_count<=257 && (ripple_count-1)%8==0 &&
       regs.pipeline.num_vertices==(ripple_count-1)/8*12 &&
       loader.component_count==1 && loader.GetComponent(0)==0 && loader.byte_count==12 &&
       color_loader.component_count==1 && color_loader.GetComponent(0)==1 && color_loader.byte_count==16 &&
       tex_loader.component_count==1 && tex_loader.GetComponent(0)==2 && tex_loader.byte_count==8 &&
       vertex_attributes.GetFormat(0)==Pica::PipelineRegs::VertexAttributeFormat::FLOAT && vertex_attributes.GetNumElements(0)==3 &&
       vertex_attributes.GetFormat(1)==Pica::PipelineRegs::VertexAttributeFormat::FLOAT && vertex_attributes.GetNumElements(1)==4 &&
       vertex_attributes.GetFormat(2)==Pica::PipelineRegs::VertexAttributeFormat::FLOAT && vertex_attributes.GetNumElements(2)==2 &&
       regs.vs.GetRegisterForAttribute(0)==0 && regs.vs.GetRegisterForAttribute(1)==3 && regs.vs.GetRegisterForAttribute(2)==4 &&
       merger.alphablend_enable && merger.alpha_blending.factor_source_rgb==Pica::FramebufferRegs::BlendFactor::SourceAlpha &&
       merger.alpha_blending.factor_dest_rgb==Pica::FramebufferRegs::BlendFactor::One &&
       merger.alpha_blending.blend_equation_rgb==Pica::FramebufferRegs::BlendEquation::Add) {
        const u64 pa=u64(base_address)+loader.data_offset,ca=u64(base_address)+color_loader.data_offset,
                  ta=u64(base_address)+tex_loader.data_offset;
        if(pa<=0xffffffff && ca<=0xffffffff && ta<=0xffffffff) {
            const auto positions=memory.GetPhysicalRef(PAddr(pa)),colors=memory.GetPhysicalRef(PAddr(ca)),coords=memory.GetPhysicalRef(PAddr(ta));
            if(positions.GetSize()>=ripple_count*12 && colors.GetSize()>=ripple_count*16 && coords.GetSize()>=ripple_count*8) {
                bool valid=true;
                for(unsigned i=0;i+1<ripple_count;++i) {
                    std::array<float,2> uv_value;std::memcpy(uv_value.data(),coords.GetPtr()+i*8,8);
                    valid &= uv_value[0]==(i%2?0.f:1.f) && uv_value[1]==(i%4<2?0.f:1.f);
                }
                if(valid) {
                    MM3D::RippleInterpolation::Mesh mesh;mesh.points.resize(ripple_count);mesh.colors.resize(ripple_count);
                    std::memcpy(mesh.points.data(),positions.GetPtr(),ripple_count*12);
                    std::memcpy(mesh.colors.data(),colors.GetPtr(),ripple_count*16);
                    if(auto half=ripple_history.Apply(MM3D::guest_frames.load(),regs.texturing.texture0.GetPhysicalAddress(),
                                                     MM3D::CameraMidpoint::generation.load(),mesh)) {
                        std::array<u32,2> offsets{};
                        for(unsigned part=0;part<2;++part) {
                            const unsigned size=part?16:12,stride=Common::AlignUp(size,stride_alignment);
                            auto [dst,offset,unused]=stream_buffer.Map(stride*ripple_count,16);
                            for(unsigned i=0;i<ripple_count;++i)std::memcpy(dst+i*stride,
                                part?static_cast<const void*>(half->colors[i].data()):static_cast<const void*>(half->points[i].data()),size);
                            stream_buffer.Commit(stride*ripple_count);offsets[part]=u32(offset);
                        }
                        ripple_offsets=offsets;
                    }
                }
            }
        }
    }
    splash_offsets.reset();splash_blend=false;
    if(MM3D::CameraMidpoint::present_60.load() &&
       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL &&
       regs.pipeline.num_vertices==6 && vs_input_index_min==0 && vs_input_index_max==4 &&
       loader.component_count==1 && loader.GetComponent(0)==0 && loader.byte_count==12 &&
       color_loader.component_count==1 && color_loader.GetComponent(0)==1 && color_loader.byte_count==16 &&
       tex_loader.component_count==1 && tex_loader.GetComponent(0)==2 && tex_loader.byte_count==8 &&
       vertex_attributes.GetFormat(0)==Pica::PipelineRegs::VertexAttributeFormat::FLOAT && vertex_attributes.GetNumElements(0)==3 &&
       vertex_attributes.GetFormat(1)==Pica::PipelineRegs::VertexAttributeFormat::FLOAT && vertex_attributes.GetNumElements(1)==4 &&
       vertex_attributes.GetFormat(2)==Pica::PipelineRegs::VertexAttributeFormat::FLOAT && vertex_attributes.GetNumElements(2)==2 &&
       regs.vs.GetRegisterForAttribute(0)==0 && regs.vs.GetRegisterForAttribute(1)==3 && regs.vs.GetRegisterForAttribute(2)==4 &&
       merger.alphablend_enable && merger.alpha_blending.factor_source_rgb==Pica::FramebufferRegs::BlendFactor::SourceAlpha &&
       merger.alpha_blending.factor_dest_rgb==Pica::FramebufferRegs::BlendFactor::One &&
       merger.alpha_blending.blend_equation_rgb==Pica::FramebufferRegs::BlendEquation::Add) {
        const u64 pa=u64(base_address)+loader.data_offset,ca=u64(base_address)+color_loader.data_offset,ta=u64(base_address)+tex_loader.data_offset;
        if(pa<=0xffffffff && ca<=0xffffffff && ta<=0xffffffff) {
            const auto p=memory.GetPhysicalRef(PAddr(pa)),c=memory.GetPhysicalRef(PAddr(ca)),t=memory.GetPhysicalRef(PAddr(ta));
            if(p.GetSize()>=60 && c.GetSize()>=80 && t.GetSize()>=40) {
                MM3D::SplashInterpolation::Sprite sprite;
                std::memcpy(sprite.points.data(),p.GetPtr(),60);std::memcpy(sprite.colors.data(),c.GetPtr(),80);std::memcpy(sprite.uv.data(),t.GetPtr(),40);
                if(auto mid=splash_history.Apply(MM3D::guest_frames.load(),regs.texturing.texture0.GetPhysicalAddress(),MM3D::CameraMidpoint::generation.load(),sprite)) {
                    std::array<u32,3> offsets;
                    for(unsigned part=0;part<3;++part) {
                        const unsigned size=part==0?12:part==1?16:8,stride=Common::AlignUp(size,stride_alignment);
                        auto [dst,offset,unused]=stream_buffer.Map(stride*5,16);
                        for(unsigned i=0;i<5;++i)std::memcpy(dst+i*stride,part==0?static_cast<const void*>(mid->sprite.points[i].data()):
                            part==1?static_cast<const void*>(mid->sprite.colors[i].data()):static_cast<const void*>(mid->old_uv[i].data()),size);
                        stream_buffer.Commit(stride*5);offsets[part]=u32(offset);
                    }
                    splash_offsets=offsets;splash_blend=mid->blend;
                }
            }
        }
    }
#endif
    // Assign the rest of the attributes to the last binding
    SetupFixedAttribs();
}

void RasterizerVulkan::SetupFixedAttribs() {
    const auto& vertex_attributes = regs.pipeline.vertex_attributes;
    VertexLayout& layout = pipeline_info.state.vertex_layout;

    auto [fixed_ptr, fixed_offset, _] = stream_buffer.Map(16 * sizeof(Common::Vec4f), 0);
    binding_offsets[layout.binding_count] = static_cast<u32>(fixed_offset);

    // Reserve the last binding for fixed and default attributes
    // Place the default attrib at offset zero for easy access
    static const Common::Vec4f default_attrib{0.f, 0.f, 0.f, 1.f};
    std::memcpy(fixed_ptr, default_attrib.AsArray(), sizeof(Common::Vec4f));

    // Find all fixed attributes and assign them to the last binding
    u32 offset = sizeof(Common::Vec4f);
    for (std::size_t i = 0; i < 16; i++) {
        if (vertex_attributes.IsDefaultAttribute(i)) {
            const u32 reg = regs.vs.GetRegisterForAttribute(i);
            if (!enable_attributes[reg]) {
                const auto& attr = pica.input_default_attributes[i];
                const std::array data = {attr.x.ToFloat32(), attr.y.ToFloat32(), attr.z.ToFloat32(),
                                         attr.w.ToFloat32()};

                const u32 data_size = sizeof(float) * static_cast<u32>(data.size());
                std::memcpy(fixed_ptr + offset, data.data(), data_size);

                VertexAttribute& attribute = layout.attributes[reg];
                attribute.binding.Assign(layout.binding_count);
                attribute.location.Assign(reg);
                attribute.offset.Assign(offset);
                attribute.type.Assign(Pica::PipelineRegs::VertexAttributeFormat::FLOAT);
                attribute.size.Assign(4);

                offset += data_size;
                enable_attributes[reg] = true;
            }
        }
    }

    // Loop one more time to find unused attributes and assign them to the default one
    // If the attribute is just disabled, shove the default attribute to avoid
    // errors if the shader ever decides to use it.
    for (u32 i = 0; i < 16; i++) {
        if (!enable_attributes[i]) {
            VertexAttribute& attribute = layout.attributes[i];
            attribute.binding.Assign(layout.binding_count);
            attribute.location.Assign(i);
            attribute.offset.Assign(0);
            attribute.type.Assign(Pica::PipelineRegs::VertexAttributeFormat::FLOAT);
            attribute.size.Assign(4);
        }
    }

    // Define the fixed+default binding
    VertexBinding& binding = layout.bindings[layout.binding_count];
    binding.binding.Assign(layout.binding_count++);
    binding.fixed.Assign(1);
    binding.byte_count.Assign(offset);

    stream_buffer.Commit(offset);
}

bool RasterizerVulkan::SetupVertexShader() {
    MICROPROFILE_SCOPE(Vulkan_VS);
    return pipeline_cache.UseProgrammableVertexShader(regs, pica.vs_setup,
                                                      pipeline_info.state.vertex_layout);
}

bool RasterizerVulkan::SetupGeometryShader() {
    MICROPROFILE_SCOPE(Vulkan_GS);

    if (regs.pipeline.use_gs != Pica::PipelineRegs::UseGS::No) {
        LOG_ERROR(Render_Vulkan, "Accelerate draw doesn't support geometry shader");
        return false;
    }

    // Enable the quaternion fix-up geometry-shader only if we are actually doing per-fragment
    // lighting and care about proper quaternions. Otherwise just use standard vertex+fragment
    // shaders. We also don't need a geometry shader if the barycentric extension is supported,
    // but that will be decided later as the GS config needs to be cached anyways.
    if (regs.lighting.disable) {
        pipeline_cache.UseTrivialGeometryShader();
        return true;
    }

    return pipeline_cache.UseFixedGeometryShader(regs);
}

bool RasterizerVulkan::AccelerateDrawBatch(bool is_indexed) {
#ifdef ENABLE_OOT3D_PRESENTATION
    if(is_indexed && OOT3D::AmmoBackdrop::Enabled(OOT3D::HUD::GetEffectiveLayout(),true)) {
        bool counter=false;
        if(const auto mesh=OotHudMesh(&counter);mesh && mesh->second && counter)return false;
    }
    if(is_indexed && OOT3D::AmmoBackdrop::Enabled(OOT3D::HUD::GetEffectiveLayout()))
        if(const auto mesh=OotUIMeshHash(32);mesh &&
           OOT3D::HUD::FindNumberMesh(mesh->first,mesh->second/4)==OOT3D::HUD::NumberKind::Ammo)
            return false;
    if(is_indexed && OOT3D::AmmoBackdrop::TimerEnabled(OOT3D::HUD::GetEffectiveLayout()))
        if(const auto mesh=OotUIMeshHash(32);mesh && OOT3D::HUD::IsTimerMesh(mesh->first,mesh->second/4))
            return false;
    // Only the verified top HUD batch needs CPU vertex output for slicing the
    // magic frame and interpolating the heart pulse. Native fragment shading stays
    // on the existing path, including custom texture replacements and fades.
    if(is_indexed && (OOT3D::MagicMeter::Changed(OOT3D::HUD::GetEffectiveLayout()) ||
                      OOT3D::Interpolation::Replaying() || OOT3D::CarrotAnimation::Get().valid)) {
        const auto mesh=OotHudMesh();
        if(mesh && mesh->first==92 && mesh->second)return false;
    }
#endif
    if (regs.pipeline.use_gs != Pica::PipelineRegs::UseGS::No) {
        if (regs.pipeline.gs_config.mode != Pica::PipelineRegs::GSMode::Point) {
            return false;
        }
        if (regs.pipeline.triangle_topology != Pica::PipelineRegs::TriangleTopology::Shader) {
            return false;
        }
    }

    pipeline_info.state.rasterization.topology.Assign(regs.pipeline.triangle_topology);
    if (regs.pipeline.triangle_topology == TriangleTopology::Fan &&
        !instance.IsTriangleFanSupported()) {
        LOG_DEBUG(Render_Vulkan,
                  "Skipping accelerated draw with unsupported triangle fan topology");
        return false;
    }

    // Vertex data setup might involve scheduler flushes so perform it
    // early to avoid invalidating our state in the middle of the draw.
    vertex_info = AnalyzeVertexArray(is_indexed, instance.GetMinVertexStrideAlignment());
    if (vertex_info.Invalid()) {
        // Do not draw anything if the vertex array is invalid.
        return true;
    }
    SetupVertexArray();

    if (!SetupVertexShader()) {
        return false;
    }
    if (!SetupGeometryShader()) {
        return false;
    }

    return Draw(true, is_indexed);
}

bool RasterizerVulkan::AccelerateDrawBatchInternal(bool is_indexed) {
    if (is_indexed && !scheduler.ReplayBatchingEnabled()) {
        SetupIndexArray();
    }

    const bool wait_built = !async_shaders || regs.pipeline.num_vertices <= 6;
    if (!pipeline_cache.BindPipeline(pipeline_info, wait_built)) {
        return true;
    }

    const DrawParams params = {
        .vertex_count = regs.pipeline.num_vertices,
        .vertex_offset = -static_cast<s32>(vertex_info.vs_input_index_min),
        .binding_count = pipeline_info.state.vertex_layout.binding_count,
        .bindings = binding_offsets,
        .is_indexed = is_indexed,
    };

    scheduler.Record([params, buffers = vertex_buffers, index_buffer = stream_buffer.Handle(),
                      index_offset = replay_index_offset, index_type = replay_index_type](vk::CommandBuffer cmdbuf) {
        std::array<vk::DeviceSize, 16> offsets;
        std::transform(params.bindings.begin(), params.bindings.end(), offsets.begin(),
                       [](u32 offset) { return static_cast<vk::DeviceSize>(offset); });
        cmdbuf.bindVertexBuffers(0, params.binding_count, buffers.data(), offsets.data());
        if (params.is_indexed) {
            cmdbuf.bindIndexBuffer(index_buffer, index_offset, index_type);
            cmdbuf.drawIndexed(params.vertex_count, 1, 0, params.vertex_offset, 0);
        } else {
            cmdbuf.draw(params.vertex_count, 1, 0, 0);
        }
    });

    return true;
}

void RasterizerVulkan::SetupIndexArray() {
    const bool index_u8 = regs.pipeline.index_array.format == 0;
    const bool native_u8 = index_u8 && instance.IsIndexTypeUint8Supported();
    const u32 index_buffer_size = regs.pipeline.num_vertices * (native_u8 ? 1 : 2);
    const vk::IndexType index_type = native_u8 ? vk::IndexType::eUint8EXT : vk::IndexType::eUint16;

    const u8* index_data =
        memory.GetPhysicalPointer(regs.pipeline.vertex_attributes.GetPhysicalBaseAddress() +
                                  regs.pipeline.index_array.offset);

    auto [index_ptr, index_offset, _] = stream_buffer.Map(index_buffer_size, 2);

    if (index_u8 && !native_u8) {
        u16* index_ptr_u16 = reinterpret_cast<u16*>(index_ptr);
        for (u32 i = 0; i < regs.pipeline.num_vertices; i++) {
            index_ptr_u16[i] = index_data[i];
        }
    } else {
        std::memcpy(index_ptr, index_data, index_buffer_size);
    }

#ifdef ENABLE_OOT3D_PRESENTATION
    if (const auto mesh = OotHudMesh()) {
        const auto [quads, top] = *mesh;
        // Degenerate unwanted triangles in our private GPU upload. The guest's
        // shared atlas/index data remains intact for its other screen draw.
        for (unsigned i = 0; i < regs.pipeline.num_vertices; i += 3) {
            if (native_u8) {
                if (!OOT3D::HUD::KeepTriangle(quads, index_ptr[i], top))
                    index_ptr[i+1] = index_ptr[i+2] = index_ptr[i];
            } else {
                auto* indices = reinterpret_cast<u16*>(index_ptr);
                if (!OOT3D::HUD::KeepTriangle(quads, indices[i], top))
                    indices[i+1] = indices[i+2] = indices[i];
            }
        }
    }
#endif

    stream_buffer.Commit(index_buffer_size);
    replay_index_offset = index_offset;
    replay_index_type = index_type;

    // AccelerateDrawBatchInternal binds the captured index range with its draw.
}

#ifdef ENABLE_OOT3D_PRESENTATION
std::optional<std::array<float,3>> RasterizerVulkan::OotSongSlide() {
    using namespace OOT3D;
    if(!OcarinaSlide::Active(Interpolation::guest_frames.load(),Interpolation::generation.load()))return {};
    const unsigned count=regs.pipeline.num_vertices;
    if(!OcarinaSlide::ValidDrawCount(count) ||
       (regs.pipeline.triangle_topology!=Pica::PipelineRegs::TriangleTopology::List &&
        regs.pipeline.triangle_topology!=Pica::PipelineRegs::TriangleTopology::Shader))return {};
    const auto mesh=OotUIMeshHash(Ocarina::DrawQuadCount*4);
    return mesh?OcarinaSlide::Find(mesh->first,mesh->second,Interpolation::guest_frames.load(),Interpolation::generation.load()):std::nullopt;
}

std::optional<std::pair<std::uint64_t,unsigned>> RasterizerVulkan::OotUIMeshHash(unsigned max_vertices, bool allow_strip) {
    using namespace OOT3D;
    const unsigned count=regs.pipeline.num_vertices;
    const bool strip=regs.pipeline.triangle_topology==Pica::PipelineRegs::TriangleTopology::Strip;
    if(strip && !allow_strip)return {};
    const unsigned vertices=TextboxInterpolation::MeshVertices(count,strip);
    if(!vertices || vertices>max_vertices ||
       (!strip && regs.pipeline.triangle_topology!=Pica::PipelineRegs::TriangleTopology::List &&
        regs.pipeline.triangle_topology!=Pica::PipelineRegs::TriangleTopology::Shader))return {};
    const auto& attributes=regs.pipeline.vertex_attributes;
    std::array<PAddr,3> sources{};
    for(const auto& loader:attributes.attribute_loaders) {
        if(loader.component_count!=1)continue;
        const unsigned attribute=loader.GetComponent(0);
        if(attribute>=12 || attributes.GetFormat(attribute)!=Pica::PipelineRegs::VertexAttributeFormat::FLOAT)continue;
        const unsigned elements=attributes.GetNumElements(attribute);
        if(elements<2 || elements>4 || loader.byte_count!=elements*4)continue;
        const unsigned slot=elements==3?0:elements==2?1:2;
        const u64 address=u64(attributes.GetPhysicalBaseAddress())+loader.data_offset;
        if(address>0xffffffff || sources[slot])return {};
        sources[slot]=PAddr(address);
    }
    auto hash=HUD::HashStart;
    constexpr std::array strides{48u,32u,64u};
    for(unsigned slot=0;slot<3;++slot) {
        if(!sources[slot])return {};
        const auto data=memory.GetPhysicalRef(sources[slot]);
        const unsigned bytes=strides[slot]*(vertices/4);
        if(data.GetSize()<bytes)return {};
        for(unsigned i=0;i<bytes;i+=4) {
            u32 word;std::memcpy(&word,data.GetPtr()+i,4);hash=HUD::HashWord(hash,word);
        }
    }
    return std::pair{hash,vertices};
}

std::optional<std::pair<unsigned, bool>> RasterizerVulkan::OotHudMesh(bool* hd_counter) {
    using namespace OOT3D::HUD;
    if(hd_counter)*hd_counter=false;
    const unsigned count = regs.pipeline.num_vertices;
    if ((count != 92 * 6 && (count == 0 || count > 4 * 6 || count % 6)) ||
        (regs.pipeline.triangle_topology != Pica::PipelineRegs::TriangleTopology::List &&
         regs.pipeline.triangle_topology != Pica::PipelineRegs::TriangleTopology::Shader))
        return {};
    const unsigned quads = count / 6;
    const auto& attributes = regs.pipeline.vertex_attributes;
    std::array<PAddr, 3> sources{}; // Position, UV, color (native helper order).
    std::array<std::uint64_t,3> attribute_hashes{};
    const bool diagnostic=quads==92 && RoutingCapture::Active();
    for (const auto& loader : attributes.attribute_loaders) {
        if (loader.component_count != 1) continue;
        const unsigned attribute = loader.GetComponent(0);
        if (attribute >= 12 || attributes.GetFormat(attribute) !=
            Pica::PipelineRegs::VertexAttributeFormat::FLOAT) continue;
        const unsigned elements = attributes.GetNumElements(attribute);
        if (elements < 2 || elements > 4 || loader.byte_count != elements * 4) continue;
        const unsigned slot = elements == 3 ? 0 : elements == 2 ? 1 : 2;
        const u64 address = u64(attributes.GetPhysicalBaseAddress()) + loader.data_offset;
        if (address > 0xffffffff || sources[slot]) return {};
        sources[slot] = PAddr(address);
    }
    auto hash = HashStart;
    std::uint64_t geometry_hash=0;
    constexpr std::array strides{48u, 32u, 64u};
    for (unsigned slot = 0; slot < 3; ++slot) {
        if (!sources[slot]) return {};
        const auto data = memory.GetPhysicalRef(sources[slot]);
        const unsigned bytes = strides[slot] * quads;
        if (data.GetSize() < bytes) return {};
        auto attribute_hash=HashStart;
        for (unsigned i = 0; i < bytes; i += 4) {
            u32 word;
            std::memcpy(&word, data.GetPtr() + i, 4);
            hash = HashWord(hash, word);
            if(diagnostic)attribute_hash=HashWord(attribute_hash,word);
        }
        attribute_hashes[slot]=attribute_hash;
        if(slot==1)geometry_hash=hash;
    }
    const bool exact=IsRouted(hash,quads),geometry=exact || IsRoutedGeometry(geometry_hash,quads);
    if(diagnostic) {
        const auto& fb=regs.framebuffer.framebuffer;
        const auto frame=OOT3D::Interpolation::guest_frames.load();
        static std::array<std::uint64_t,4> previous{};
        const std::array<std::uint64_t,4> key{frame,hash,fb.GetColorBufferPhysicalAddress(),std::uint64_t(RoutingCapture::ends.load())};
        if(key!=previous && RoutingCapture::Take()) {
            previous=key;const auto expected=LatestMainMesh();
            LOG_INFO(Render_Vulkan,"OoT HUD routing capture: frame={} target={:08x} size={}x{} exact={} geometry={} sources={:08x}/{:08x}/{:08x} draw={:016x} xyz={:016x} uv={:016x} rgba={:016x} latest_routed={} latest={:016x} xyz={:016x} uv={:016x} rgba={:016x}",
                frame,fb.GetColorBufferPhysicalAddress(),fb.GetWidth(),fb.GetHeight(),exact,geometry,
                sources[0],sources[1],sources[2],hash,attribute_hashes[0],attribute_hashes[1],attribute_hashes[2],
                expected.routed,expected.hash,expected.attributes[0],expected.attributes[1],expected.attributes[2]);
        }
    }
    if (!exact) {
        if(!geometry)return {};
        // Bounded evidence for the reported flash: no frame-by-frame log spam.
        static unsigned color_variants=0;
        ++color_variants;
        if(color_variants<=4 || (color_variants&(color_variants-1))==0)
            LOG_INFO(Render_Vulkan,"OoT HUD routing: retained screen split for color-only upload change (quads={}, occurrences={})",quads,color_variants);
    }
    // Verify the native atlas's index order, also used to filter CPU-shaded
    // triangle batches. No matching by texture or triangle count alone.
    const bool byte_indices = regs.pipeline.index_array.format == 0;
    const auto indices = memory.GetPhysicalRef(attributes.GetPhysicalBaseAddress() +
                                               regs.pipeline.index_array.offset);
    if (indices.GetSize() < count * (byte_indices ? 1u : 2u)) return {};
    constexpr std::array<unsigned, 6> pattern{0, 2, 1, 1, 2, 3};
    for (unsigned i = 0; i < count; ++i) {
        u16 index;
        if (byte_indices) index = indices.GetPtr()[i];
        else std::memcpy(&index, indices.GetPtr() + i * 2, 2);
        if (index != i / 6 * 4 + pattern[i % 6]) return {};
    }
    const auto& fb = regs.framebuffer.framebuffer;
    if(hd_counter)*hd_counter=IsHDCounterMesh(hash,geometry_hash,quads);
    if ((fb.GetWidth() == 480 && fb.GetHeight() == 400) ||
        (fb.GetWidth() == 240 && fb.GetHeight() == 400) ||
        (fb.GetWidth() == 400 && fb.GetHeight() == 240)) return {{quads, true}};
    if ((fb.GetWidth() == 240 && fb.GetHeight() == 320) ||
        (fb.GetWidth() == 320 && fb.GetHeight() == 240)) return {{quads, false}};
    return {};
}
#endif

void RasterizerVulkan::DrawTriangles() {
    if (vertex_batch.empty()) {
        return;
    }
#ifdef ENABLE_OOT3D_PRESENTATION
    oot_hud_midpoint.clear();
    if (const auto mesh = OotHudMesh(); mesh && vertex_batch.size() == regs.pipeline.num_vertices) {
        const auto [quads, top] = *mesh;
        std::vector<HardwareVertex> filtered;
        filtered.reserve(vertex_batch.size()+132);
        const auto layout=OOT3D::HUD::GetEffectiveLayout();
        bool magic_replaced=false,magic_blended=false;
        std::size_t magic_begin=0,magic_end=0;
        std::vector<HardwareVertex> midpoint_magic;
        std::size_t hearts_begin=0,carrots_begin=0;
        bool carrots_replaced=false;
        const auto carrot_time=Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count();
        for (std::size_t i = 0; i < vertex_batch.size(); i += 6) {
            if (!OOT3D::HUD::KeepTriangle(quads, unsigned(i / 6 * 4), top)) continue;
            if(top && quads==92 && i==59*6) {
                const auto mix=[](const HardwareVertex& a,const HardwareVertex& b,float t) {
                    HardwareVertex v;
                    v.position=a.position*(1-t)+b.position*t;
                    v.color=a.color*(1-t)+b.color*t;
                    v.tex_coord0=a.tex_coord0*(1-t)+b.tex_coord0*t;
                    v.tex_coord1=a.tex_coord1*(1-t)+b.tex_coord1*t;
                    v.tex_coord2=a.tex_coord2*(1-t)+b.tex_coord2*t;
                    v.tex_coord0_w=a.tex_coord0_w*(1-t)+b.tex_coord0_w*t;
                    v.normquat=a.normquat*(1-t)+b.normquat*t;
                    v.view=a.view*(1-t)+b.view*t;
                    return v;
                };
                magic_begin=filtered.size();
                if(OOT3D::Interpolation::Replaying()) {
                    std::array<HardwareVertex,24> midpoint;
                    std::copy_n(vertex_batch.begin()+i,24,midpoint.begin());
                    magic_blended=oot_magic_history.Apply(std::span<HardwareVertex>{midpoint},
                        OOT3D::Interpolation::guest_frames.load(),
                        Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count(),
                        OOT3D::Interpolation::generation.load(),
                        [](const HardwareVertex& a,const HardwareVertex& b) {
                            return a.color[3]==b.color[3] && a.tex_coord0==b.tex_coord0 &&
                                a.tex_coord1==b.tex_coord1 && a.tex_coord2==b.tex_coord2;
                        });
                    if(magic_blended && !OOT3D::MagicMeter::Append(
                            std::span<const HardwareVertex>{midpoint},layout,midpoint_magic,mix))
                        midpoint_magic.assign(midpoint.begin(),midpoint.end());
                } else oot_magic_history.seen=false;
                magic_replaced=OOT3D::MagicMeter::Append(
                    std::span<const HardwareVertex>{vertex_batch.data()+i,24},layout,filtered,mix);
            }
            if(magic_replaced && i>=59*6 && i<=62*6)continue;
            if(top && quads==92 && i==63*6)magic_end=hearts_begin=filtered.size();
            if(top && quads==92 && i==86*6) {
                carrots_begin=filtered.size();
                carrots_replaced=oot_carrot_history.Update(
                    std::span<const HardwareVertex>{vertex_batch.data()+i,36},OOT3D::CarrotAnimation::Get(),
                    carrot_time,OOT3D::Interpolation::guest_frames.load(),OOT3D::Interpolation::generation.load(),
                    layout.carrot_animation_delay);
                if(carrots_replaced)oot_carrot_history.Append(filtered,carrot_time,layout.carrot_shadow,layout.carrot_opacity);
            }
            if(carrots_replaced && i>=86*6)continue;
            for (unsigned j = 0; j < 6; ++j) filtered.push_back(vertex_batch[i+j]);
        }
        if(top && quads==92 && OOT3D::Interpolation::Replaying() && hearts_begin+120<=filtered.size()) {
            oot_hud_midpoint=filtered;
            const bool hearts_blended=oot_heart_history.Apply(
                std::span<HardwareVertex>{oot_hud_midpoint}.subspan(hearts_begin,120),
                OOT3D::Interpolation::guest_frames.load(),
                Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count(),
                OOT3D::Interpolation::generation.load());
            if(magic_blended && magic_end>magic_begin) {
                oot_hud_midpoint.erase(oot_hud_midpoint.begin()+magic_begin,oot_hud_midpoint.begin()+magic_end);
                oot_hud_midpoint.insert(oot_hud_midpoint.begin()+magic_begin,midpoint_magic.begin(),midpoint_magic.end());
            }
            if(carrots_replaced) {
                // Carrots are the final six atlas quads. Build their halfway
                // presentation, accounting for any magic replacement above.
                const auto magic_delta=magic_blended && magic_end>magic_begin?
                    std::ptrdiff_t(midpoint_magic.size())-std::ptrdiff_t(magic_end-magic_begin):0;
                oot_hud_midpoint.resize(std::size_t(std::ptrdiff_t(carrots_begin)+magic_delta));
                oot_carrot_history.Append(oot_hud_midpoint,carrot_time-16667,layout.carrot_shadow,layout.carrot_opacity);
            }
            if(!hearts_blended && !magic_blended && !carrots_replaced)oot_hud_midpoint.clear();
        }
        vertex_batch=std::move(filtered);
        if (vertex_batch.empty()) return;
    }
#endif

    pipeline_info.state.rasterization.topology.Assign(Pica::PipelineRegs::TriangleTopology::List);
    pipeline_info.state.vertex_layout = software_layout;

    pipeline_cache.UseTrivialVertexShader();
    pipeline_cache.UseTrivialGeometryShader();

    Draw(false, false);
#ifdef ENABLE_OOT3D_PRESENTATION
    oot_hud_midpoint.clear();
#endif
}

bool RasterizerVulkan::Draw(bool accelerate, bool is_indexed) {
    MICROPROFILE_SCOPE(Vulkan_Drawing);
    bool cpu_interpolation = false;
#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::Interpolation::Timing::Scope draw_timing{OOT3D::Interpolation::Timing::draw_us};
    cpu_interpolation = !accelerate && OOT3D::Interpolation::Replaying();
    if(!OOT3D::Interpolation::Replaying() ||
       oot_replay_generation!=OOT3D::Interpolation::generation.load()) {
        oot_replay_frame.Reset();oot_rate_clock.Reset();
        for(auto& slot:oot_replay_slots)slot.Reset();
        oot_replay_generation=OOT3D::Interpolation::generation.load();
    }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    cpu_interpolation = !accelerate && MM3D::CpuInterpolation::Enabled() &&
                        MM3D::CameraMidpoint::enabled.load();
    const bool replay_geometry = accelerate || cpu_interpolation;
    const bool replay_world = accelerate || (cpu_interpolation && pica.CanReplayCpuVertices());
    MM3D::InterpolationPerformance::Scope performance_draw{MM3D::InterpolationPerformance::Part::Draw};
    MM3D::Profiles::ClearDrawTexcoordBounds();
    const auto midpoint_background = [&] {
        const auto& fb=regs.framebuffer.framebuffer;
        return midpoint_color_clears.Background(MM3D::guest_frames.load(),
            fb.GetColorBufferPhysicalAddress(),fb.GetWidth(),fb.GetHeight(),
            fb.color_format==Pica::FramebufferRegs::ColorFormat::RGBA8);
    };
#endif
    SyncDrawState();

    const bool shadow_rendering = regs.framebuffer.IsShadowRendering();
    const bool has_stencil = regs.framebuffer.HasStencil();

    const bool write_color_fb = shadow_rendering || pipeline_info.GetFinalColorWriteMask(instance);
    const bool write_depth_fb = pipeline_info.IsDepthWriteEnabled();
    const bool using_color_fb =
        regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress() != 0 && write_color_fb;
#ifdef ENABLE_MM3D_PRESENTATION
    if(using_color_fb) {
        const auto& fb=regs.framebuffer.framebuffer;
        const auto address=fb.GetColorBufferPhysicalAddress();
        midpoint_scene_copies.InvalidatePadded(address,u64(address)+u64(fb.GetWidth())*
            fb.GetHeight()*Pica::FramebufferRegs::BytesPerColorPixel(fb.color_format));
    }
#endif
    const bool using_depth_fb =
        !shadow_rendering && regs.framebuffer.framebuffer.GetDepthBufferPhysicalAddress() != 0 &&
        (write_depth_fb || regs.framebuffer.output_merger.depth_test_enable != 0 ||
         (has_stencil && pipeline_info.state.depth_stencil.stencil_test_enable));

    const auto fb_helper = res_cache.GetFramebufferSurfaces(using_color_fb, using_depth_fb);
    const Framebuffer* framebuffer = fb_helper.Framebuffer();
    if (!framebuffer->Handle()) {
        return true;
    }

    pipeline_info.state.attachments.color = framebuffer->Format(SurfaceType::Color);
    pipeline_info.state.attachments.depth = framebuffer->Format(SurfaceType::Depth);

    // Update scissor uniforms
    const auto [scissor_x1, scissor_y2, scissor_x2, scissor_y1] = fb_helper.Scissor();
    if (fs_data.scissor_x1 != scissor_x1 || fs_data.scissor_x2 != scissor_x2 ||
        fs_data.scissor_y1 != scissor_y1 || fs_data.scissor_y2 != scissor_y2) {

        fs_data.scissor_x1 = scissor_x1;
        fs_data.scissor_x2 = scissor_x2;
        fs_data.scissor_y1 = scissor_y1;
        fs_data.scissor_y2 = scissor_y2;
        fs_data_dirty = true;
    }

    // Sync and bind the texture surfaces
#ifdef ENABLE_OOT3D_PRESENTATION
    // The rotated PICA display's horizontal coordinate is clip Y. Correct
    // before clipping so the extra width reveals world geometry, and cancels
    // compositor expansion for HUD sprites without changing the bottom screen.
    const auto oot_viewport = regs.rasterizer.GetViewportRect();
    bool oot_storm_clouds=false;
    const bool oot_full_width = [&] {
        if (shadow_rendering || !using_color_fb ||
            !OOT3D::Widescreen::IsTopViewport(oot_viewport.GetWidth(), oot_viewport.GetHeight()))
            return false;
        const auto& textures = regs.texturing.main_config;
        const unsigned texture_mask = unsigned(textures.texture0_enable) |
            (unsigned(textures.texture1_enable) << 1) | (unsigned(textures.texture2_enable) << 2);
        // The cover and both complementary visibility masks must expand
        // together. Identify the native lens mesh rather than widening every
        // draw that shares the game's general-purpose model shader.
        if (!shadow_rendering && using_color_fb &&
            OOT3D::Lens::Candidate(regs.pipeline.num_vertices, texture_mask)) {
            std::array<OOT3D::Widescreen::Row, 4> projection{};
            std::array<OOT3D::Widescreen::Row, 3> model{};
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 4; ++c) {
                    projection[r][c] = pica.vs_setup.uniforms.f[r][c].ToFloat32();
                    if (r < 3) model[r][c] = pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
                }
            if (!OOT3D::Lens::Transform(projection, model)) return false;
            if (!accelerate) {
                if (vertex_batch.size() != 18) return false;
                std::array<OOT3D::Widescreen::Row, 18> positions{};
                for (unsigned i = 0; i < positions.size(); ++i) {
                    const auto& p = vertex_batch[i].position;
                    positions[i] = {p.x, p.y, p.z, p.w};
                }
                return OOT3D::Lens::ClipGeometry(positions, projection, model);
            }
            if (vertex_info.vs_input_index_max - vertex_info.vs_input_index_min != 7)
                return false;
            std::array<OOT3D::Widescreen::Row, 8> positions{};
            Pica::VertexLoader loader(memory, regs.pipeline);
            Pica::AttributeBuffer input{}, defaults{};
            for (unsigned i = 0; i < positions.size(); ++i) {
                const unsigned index = vertex_info.vs_input_index_min + i;
                loader.LoadVertex(regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(),
                                  index, index, input, defaults);
                for (unsigned c = 0; c < 4; ++c)
                    positions[i][c] = input[regs.vs.GetRegisterForAttribute(0)][c].ToFloat32();
            }
            return OOT3D::Lens::Geometry(positions);
        }
        const bool clear = OOT3D::Widescreen::ClearState(pica.vs_setup.GetProgramCodeHash(),
                pica.vs_setup.GetSwizzleDataHash(), regs.pipeline.num_vertices,
                unsigned(regs.pipeline.triangle_topology.Value()),
                u32(regs.framebuffer.output_merger.depth_color_mask), texture_mask,
                regs.framebuffer.output_merger.alpha_test.enable,
                pipeline_info.state.depth_stencil.stencil_test_enable);
        const auto& blend = regs.framebuffer.output_merger;
        const bool fade = OOT3D::Widescreen::FadeState(pica.vs_setup.GetProgramCodeHash(),
                pica.vs_setup.GetSwizzleDataHash(), regs.pipeline.num_vertices,
                unsigned(regs.pipeline.triangle_topology.Value()),
                u32(blend.depth_color_mask), texture_mask, blend.alpha_test.enable,
                pipeline_info.state.depth_stencil.stencil_test_enable,
                {unsigned(blend.alphablend_enable), unsigned(blend.alpha_blending.blend_equation_rgb.Value()),
                 unsigned(blend.alpha_blending.factor_source_rgb.Value()),
                 unsigned(blend.alpha_blending.factor_dest_rgb.Value())});
        const bool clouds=pica.vs_setup.GetProgramCodeHash()==OOT3D::Interpolation::ModelShader &&
            pica.vs_setup.GetSwizzleDataHash()==OOT3D::Interpolation::ModelSwizzle && OOT3D::StormClouds::Candidate(regs);
        if (!clear && !fade && !clouds) return false;
        if (!accelerate) {
            if (vertex_batch.size() != 6) return false;
            std::array<OOT3D::Widescreen::Row, 6> positions{};
            for (unsigned i = 0; i < positions.size(); ++i) {
                const auto& p = vertex_batch[i].position;
                positions[i] = {p.x, p.y, p.z, p.w};
            }
            if(clouds)return oot_storm_clouds=OOT3D::Widescreen::FadeClipGeometry(positions);
            return clear ? OOT3D::Widescreen::ClearClipGeometry(positions) :
                           (OOT3D::Widescreen::FadeClipGeometry(positions) ||
                            OOT3D::Widescreen::LetterboxClipGeometry(positions));
        }
        if (vertex_info.vs_input_index_max - vertex_info.vs_input_index_min != 3) return false;
        std::array<OOT3D::Widescreen::Row, 4> projection{}, vertices{};
        std::array<OOT3D::Widescreen::Row, 3> model{};
        for (unsigned r = 0; r < 4; ++r)
            for (unsigned c = 0; c < 4; ++c) {
                projection[r][c] = pica.vs_setup.uniforms.f[r][c].ToFloat32();
                if (r < 3) model[r][c] = pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
            }
        Pica::VertexLoader loader(memory, regs.pipeline);
        Pica::AttributeBuffer input{}, defaults{};
        for (unsigned i = 0; i < 4; ++i) {
            const unsigned index = vertex_info.vs_input_index_min + i;
            loader.LoadVertex(regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(), index, index,
                              input, defaults);
            for (unsigned c = 0; c < 4; ++c)
                vertices[i][c] = input[regs.vs.GetRegisterForAttribute(0)][c].ToFloat32();
        }
        if(clouds)return oot_storm_clouds=OOT3D::StormClouds::Geometry(projection,model,vertices);
        return clear ? OOT3D::Widescreen::ClearGeometry(projection, model, vertices) :
                       (OOT3D::Widescreen::FadeGeometry(projection, model, vertices) ||
                        OOT3D::Widescreen::LetterboxGeometry(projection, model, vertices));
    }();
    const float oot_scale = !oot_full_width && OOT3D::Widescreen::Active() && !shadow_rendering && using_color_fb &&
        OOT3D::Widescreen::IsTopViewport(oot_viewport.GetWidth(), oot_viewport.GetHeight()) ?
        OOT3D::Widescreen::HorizontalScale : 1.f;
    if (vs_data.clip_scale_x != 1.f || vs_data.clip_scale_y != oot_scale) {
        vs_data.clip_scale_x = 1.f;
        vs_data.clip_scale_y = oot_scale;
        vs_data_dirty = true;
    }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    // MM3D uses a 480x400 top viewport (the display itself is 240x400). Compress its display-X
    // clip coordinate before clipping: perspective draws reveal more world,
    // while orthographic UI recovers its proportions in the 16:9 compositor.
    const auto wide_viewport=regs.rasterizer.GetViewportRect();
    const bool wide_top=MM3D::Widescreen::Active() && !shadow_rendering && using_color_fb &&
        MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight());
    std::optional<unsigned> lens_kind,letterbox_kind;
    MM3D::LensInterpolation::Values lens_values{};
    if(!shadow_rendering && using_color_fb &&
       MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) &&
       (regs.pipeline.num_vertices==18 || regs.pipeline.num_vertices==4) &&
       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL) {
        MM3D::LensInterpolation::Material material;
        const auto& b=pipeline_info.state.blending;
        const auto& ds=pipeline_info.state.depth_stencil;
        const auto& di=pipeline_info.dynamic_info;
        const auto& textures=regs.texturing.main_config;
        material.state={unsigned(b.blend_enable),unsigned(b.src_color_blend_factor.Value()),
            unsigned(b.dst_color_blend_factor.Value()),unsigned(b.color_blend_eq.Value()),
            unsigned(b.color_write_mask),unsigned(ds.depth_test_enable),unsigned(ds.stencil_test_enable)};
        material.stencil={unsigned(ds.stencil_test_enable),unsigned(ds.stencil_compare_op.Value()),
            di.stencil_reference,di.stencil_compare_mask,di.stencil_write_mask,
            unsigned(ds.stencil_fail_op.Value()),unsigned(ds.stencil_depth_fail_op.Value()),unsigned(ds.stencil_pass_op.Value())};
        const auto& alpha=regs.framebuffer.output_merger.alpha_test;
        material.alpha={unsigned(alpha.enable),unsigned(alpha.func.Value()),unsigned(alpha.ref)};
        material.textures=unsigned(textures.texture0_enable)|(unsigned(textures.texture1_enable)<<1)|(unsigned(textures.texture2_enable)<<2);
        material.depth_write=bool(ds.depth_write_enable);
        const auto stages=regs.texturing.GetTevStages();
        for(unsigned i=0;i<6;++i)material.tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,
            stages[i].ops_raw,stages[i].const_color,stages[i].scales_raw};
        for(unsigned r=0;r<96;++r)for(unsigned c=0;c<4;++c)lens_values[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        if(regs.pipeline.num_vertices==18)lens_kind=MM3D::LensInterpolation::Identify(lens_values,material);
        else letterbox_kind=MM3D::LetterboxInterpolation::Identify(lens_values,material);
    }
    bool wide_bypass=false;
    if(wide_top && regs.pipeline.num_vertices<=24 && !write_depth_fb) {
        const auto& textures=regs.texturing.main_config;
        const auto& merger=regs.framebuffer.output_merger;
        const auto scissor=fb_helper.Scissor();
        wide_bypass=MM3D::WidescreenProbe::Observe({pica.vs_setup.GetProgramCodeHash(),
            pica.vs_setup.GetSwizzleDataHash(),u32(regs.pipeline.num_vertices),
            unsigned(wide_viewport.GetWidth()),unsigned(wide_viewport.GetHeight()),
            unsigned(textures.texture0_enable)|(unsigned(textures.texture1_enable)<<1)|(unsigned(textures.texture2_enable)<<2),
            unsigned(merger.alpha_blending.factor_source_rgb.Value())|
                (unsigned(merger.alpha_blending.factor_dest_rgb.Value())<<4)|
                (unsigned(merger.alpha_blending.blend_equation_rgb.Value())<<8)|
                (unsigned(merger.alphablend_enable)<<16),u32(merger.depth_color_mask)},
            {int(scissor.left),int(scissor.top),int(scissor.right),int(scissor.bottom)});
        if(wide_bypass) {
            const auto row=[&](unsigned r) {
                std::array<float,4> result{};
                for(unsigned c=0;c<4;++c)result[c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
                return result;
            };
            if(MM3D::WidescreenProbe::LocalLayoutPane(pica.vs_setup.GetProgramCodeHash(),
                pica.vs_setup.GetSwizzleDataHash(),u32(regs.pipeline.num_vertices),
                unsigned(textures.texture0_enable)|(unsigned(textures.texture1_enable)<<1)|
                    (unsigned(textures.texture2_enable)<<2),row(10),row(11),row(12)))
                wide_bypass=false;
        }
    }
    // All three lens passes must cover the same full viewport. Compressing
    // only the stencil masks leaves visible-state strips beside the purple cover.
    wide_bypass |= lens_kind.has_value();
    const float wide_scale=wide_top && !wide_bypass?MM3D::Widescreen::HorizontalScale:1.f;
    // Depth testing is a provisional world-draw filter, not a verified camera identity.
    // Keep zoom independent of the widescreen checkbox and exclude shadow/offscreen layouts.
    const auto& depth = regs.framebuffer.output_merger;
    const bool zoom_world = MM3D::Widescreen::supported_layout.load() &&
        !shadow_rendering && using_color_fb && !wide_bypass &&
        MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(), wide_viewport.GetHeight()) &&
        depth.depth_test_enable != 0 &&
        depth.depth_test_func != Pica::FramebufferRegs::CompareFunc::Always &&
        depth.depth_test_func != Pica::FramebufferRegs::CompareFunc::Never;
    const float zoom_scale = zoom_world ?
        1.f / std::clamp(MM3D::Widescreen::camera_zoom_out.load(), 1.f, 1.5f) : 1.f;
    if(vs_data.clip_scale_x != zoom_scale || vs_data.clip_scale_y != wide_scale * zoom_scale) {
        vs_data.clip_scale_x = zoom_scale;
        vs_data.clip_scale_y = wide_scale * zoom_scale;
        vs_data_dirty = true;
    }
    if(wide_top) ++MM3D::Widescreen::frame_corrected_draws;
    MM3D::Textures::hide_current_draw = false;
    mm3d_draw_profiles.clear();
    panel_excluded = false;
    panel_draw_identities.clear();
    std::optional<std::array<float,4>> native_uv_bounds;
    if (accelerate) {
        Pica::VertexLoader loader(memory, regs.pipeline);
        Pica::AttributeBuffer input{}, defaults{};
        float min_x=1.f,min_y=1.f,max_x=0.f,max_y=0.f;
        const u32 attr_reg=regs.vs.GetRegisterForAttribute(1); // conventional texture0 input
        for (u32 i=vertex_info.vs_input_index_min; i<=vertex_info.vs_input_index_max; ++i) {
            loader.LoadVertex(regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(), i, i, input, defaults);
            min_x=std::min(min_x,input[attr_reg][0].ToFloat32()); min_y=std::min(min_y,input[attr_reg][1].ToFloat32());
            max_x=std::max(max_x,input[attr_reg][0].ToFloat32()); max_y=std::max(max_y,input[attr_reg][1].ToFloat32());
        }
        if (max_x>=min_x && max_y>=min_y) {
            native_uv_bounds=std::array{min_x,min_y,max_x,max_y};
            MM3D::Profiles::SetDrawTexcoordBounds(min_x,min_y,max_x,max_y);
        }
    } else if (!vertex_batch.empty()) {
        float min_x=1.f,min_y=1.f,max_x=0.f,max_y=0.f;
        for (const auto& v:vertex_batch) { min_x=std::min(min_x,v.tex_coord0.x); min_y=std::min(min_y,v.tex_coord0.y); max_x=std::max(max_x,v.tex_coord0.x); max_y=std::max(max_y,v.tex_coord0.y); }
        if(max_x>=min_x && max_y>=min_y)native_uv_bounds=std::array{min_x,min_y,max_x,max_y};
        MM3D::Profiles::SetDrawTexcoordBounds(min_x,min_y,max_x,max_y);
    }
    MM3D::Minimap::marker_draw=false;
#endif
#ifdef ENABLE_OOT3D_PRESENTATION
    OOT3D::Textures::hide_current_draw = false;
#endif
    SyncTextureUnits(framebuffer);
    SyncUtilityTextures(framebuffer);
#ifdef ENABLE_OOT3D_PRESENTATION
    // Only append after the verified number atlas has actually been uploaded and
    // bound. Async font loading and texture-pack fallbacks keep native geometry.
    if(!accelerate && oot_number_backdrop)
        OOT3D::AmmoBackdrop::Prepend(vertex_batch,*oot_number_backdrop);
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    const bool use_pause=pause_draw[0]||pause_draw[1]||pause_draw[2]||pause_draw[3];
    const auto inventory_owner=MM3D::InventoryInterpolation::Owner(MM3D::guest_frames.load(),
        MM3D::LinkPoseCapture::interpolation_generation.load());
    const bool padded_scene_copy=[&] {
        if(!MM3D::CameraMidpoint::present_60.load() || !replay_geometry || shadow_rendering ||
           !using_color_fb || use_pause ||
           !MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) ||
           pica.vs_setup.GetProgramCodeHash()!=0x89b1ab98da3d9241ULL ||
           pica.vs_setup.GetSwizzleDataHash()!=0x6f0b398b1fc186a9ULL)return false;
        const auto frame=MM3D::guest_frames.load();
        const auto& row=pica.vs_setup.uniforms.f[3];
        const bool ortho=row.x.ToFloat32()==0 && row.y.ToFloat32()==0 &&
            row.z.ToFloat32()==0 && row.w.ToFloat32()==1;
        const auto& blend=pipeline_info.state.blending;
        const auto& textures=regs.texturing.main_config;
        const unsigned texture_mask=unsigned(textures.texture0_enable)|
            (unsigned(textures.texture1_enable)<<1)|(unsigned(textures.texture2_enable)<<2);
        using BF=Pica::FramebufferRegs::BlendFactor;
        const bool opaque=blend.blend_enable && blend.src_color_blend_factor==BF::One &&
            blend.dst_color_blend_factor==BF::Zero &&
            blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add && blend.color_write_mask==15;
        if(!MM3D::ScreenShrink::native_normalized.load() || !ortho || !opaque ||
           regs.pipeline.num_vertices!=4 || texture_mask!=1 || !native_uv_bounds ||
           !midpoint_scene_clear.Pending(frame) ||
           midpoint_scene_clear.target!=regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress() ||
           pipeline_info.state.depth_stencil.depth_test_enable ||
           regs.framebuffer.output_merger.alpha_test.enable ||
           regs.texturing.texture0.type!=Pica::TexturingRegs::TextureConfig::Texture2D ||
           !midpoint_scene_copies.PaddedTextureContains(frame,
               regs.texturing.texture0.GetPhysicalAddress(),regs.texturing.texture0.width,
               regs.texturing.texture0.height,
               regs.texturing.texture0_format==Pica::TexturingRegs::TextureFormat::RGBA8))return false;
        const auto stages=regs.texturing.GetTevStages();
        for(unsigned i=0;i<stages.size();++i) {
            const auto& stage=stages[i];
            if(stage.sources_raw!=(i?0x0fff0fffU:0x0ee30ee3U) || stage.modifiers_raw ||
               stage.ops_raw || stage.scales_raw || stage.const_color!=(i?0xffffffffU:0U))return false;
        }
        MM3D::CameraMidpoint::Matrix projection{},model=MM3D::CameraMidpoint::Identity();
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
            projection[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
            if(r<3)model[r][c]=pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
        }
        return MM3D::CameraMidpoint::NormalizedShrinkMesh(projection,model,*native_uv_bounds);
    }();
    // This texture already contains the widescreen-corrected scene and HUD.
    // A second clip correction squeezes only native frames: midpoint replay
    // omits this redundant copy. Keep both presentations at the same width.
    if(padded_scene_copy && (vs_data.clip_scale_x!=1.f || vs_data.clip_scale_y!=1.f)) {
        vs_data.clip_scale_x=vs_data.clip_scale_y=1.f;
        vs_data_dirty=true;
    }
    using AtlasBF=Pica::FramebufferRegs::BlendFactor;
    const auto& atlas_blend=pipeline_info.state.blending;
    const bool atlas_supported=!shadow_rendering &&
        !pipeline_info.state.depth_stencil.stencil_test_enable && !write_depth_fb &&
        (!pipeline_info.state.depth_stencil.depth_test_enable ||
         pipeline_info.state.depth_stencil.depth_compare_op==Pica::FramebufferRegs::CompareFunc::Always) &&
        atlas_blend.blend_enable && atlas_blend.src_color_blend_factor==AtlasBF::SourceAlpha &&
        atlas_blend.dst_color_blend_factor==AtlasBF::OneMinusSourceAlpha &&
        atlas_blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add &&
        (atlas_blend.color_write_mask&7)==7 && regs.lighting.disable.Value()!=0 &&
        regs.texturing.texture0.type==Pica::TexturingRegs::TextureConfig::Texture2D;
    std::erase_if(mm3d_draw_profiles,[&](const auto& p){
        if(p.atlas_selection && !atlas_supported) { ++MM3D::prompt_unsupported; return true; }
        return false;
    });
    MM3D::Atlas::Uniforms guest_regions;
    // Overlapping guest groups use the first match, so the same pixels are never drawn twice.
    std::erase_if(mm3d_draw_profiles,[&](const auto& p){
        if(!p.atlas_selection || p.presentation) return false;
        MM3D::Atlas::Uniforms mask;
        if(!MM3D::Atlas::Build(p,mask) || MM3D::Atlas::Overlaps(guest_regions,mask)) {++MM3D::prompt_unsupported;return true;}
        auto combined=guest_regions;
        for(int i=0;i<mask.info[0];++i) if(!MM3D::Atlas::Add(combined,mask.regions[i])) {++MM3D::prompt_unsupported;return true;}
        guest_regions=combined;return false;
    });
    const bool had_atlas=atlas_uniforms.info[0]>0||atlas_uniforms.info[2]||atlas_uniforms.pause[0]||atlas_uniforms.pause[1]||atlas_uniforms.pause[2]||atlas_uniforms.pause[3];
    guest_regions.pause=pause_draw;
    guest_regions.info[2]=padded_scene_copy?1:0;
    atlas_uniforms=guest_regions;
    atlas_uniforms.info[1]=1; // Original pass keeps everything outside the selected regions.
    fs_data_dirty |= had_atlas || guest_regions.info[0]>0 || use_pause || padded_scene_copy;
#endif

    bool pickup_fog=false;
#ifdef ENABLE_OOT3D_PRESENTATION
    if(OOT3D::PickupFog::Candidate(regs) &&
       pica.vs_setup.GetProgramCodeHash()==OOT3D::Interpolation::ModelShader &&
       pica.vs_setup.GetSwizzleDataHash()==OOT3D::Interpolation::ModelSwizzle) {
        std::array<OOT3D::PickupFog::Matrix,10> native{};
        for(unsigned b=0;b<10;++b)for(unsigned i=0;i<12;++i)
            native[b][i]=pica.vs_setup.uniforms.f[20+3*b+i/4][i%4].ToFloat32();
        pickup_fog=OOT3D::PickupFog::Match(native,regs.pipeline.num_vertices,OOT3D::Interpolation::guest_frames.load(),
            OOT3D::Interpolation::generation.load(),OOT3D::Interpolation::native_scene.load(),
            Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count());
    }
#endif
    // Sync and bind the shader
    pipeline_cache.UseFragmentShader(regs, user_config
#ifdef ENABLE_MM3D_PRESENTATION
        , guest_regions.info[0]>0 || use_pause || padded_scene_copy
#else
        , false
#endif
        , pickup_fog);

    // Sync the LUTs within the texture buffer
    SyncAndUploadLUTs();
    SyncAndUploadLUTsLF();
#ifdef ENABLE_MM3D_PRESENTATION
    if (MM3D::Trace::active.load() || MM3D::CameraCapture::active.load()) {
        MM3D::Trace::Uniforms observed{};
        if (accelerate) {
            for (unsigned slot = 0; slot < 96; ++slot) {
                const auto& value = pica.vs_setup.uniforms.f[slot];
                observed[slot] = {value.x.ToFloat32(), value.y.ToFloat32(),
                                  value.z.ToFloat32(), value.w.ToFloat32()};
            }
        }
        if(!shadow_rendering && using_color_fb) {
            MM3D::CameraCapture::Observe(MM3D::guest_frames.load(), pica.vs_setup.GetProgramCodeHash(),
                pica.vs_setup.GetSwizzleDataHash(), regs.pipeline.num_vertices, accelerate,
                wide_viewport.GetWidth(), wide_viewport.GetHeight(),
                u32(regs.framebuffer.output_merger.depth_color_mask), observed,
                {.vertex_base=regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(),
                 .first_vertex=accelerate ? vertex_info.vs_input_index_min : 0,
                 .last_vertex=accelerate ? vertex_info.vs_input_index_max : 0,
                 .index_offset=is_indexed ? u32(regs.pipeline.index_array.offset) : 0,
                 .topology=u32(regs.pipeline.triangle_topology.Value()),
                 .indexed=is_indexed, .index16=is_indexed && regs.pipeline.index_array.format!=0,
                 .valid=accelerate});
        }
        MM3D::Trace::Observe(MM3D::guest_frames.load(), pica.vs_setup.GetProgramCodeHash(),
            pica.vs_setup.GetSwizzleDataHash(), regs.pipeline.num_vertices, accelerate, observed,
            std::span<const u32>(pica.vs_setup.GetProgramCode()).first(
                std::min<u32>(pica.vs_setup.GetBiggestProgramSize(), Pica::MAX_PROGRAM_CODE_LENGTH)));
    }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    const bool glare_candidate=[&] {
        using BF=Pica::FramebufferRegs::BlendFactor;
        const auto& blend=pipeline_info.state.blending;
        const auto& depth=pipeline_info.state.depth_stencil;
        const auto& texture=regs.texturing.main_config;
        if(!MM3D::CameraMidpoint::present_60.load() || shadow_rendering || !using_color_fb ||
           !MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) ||
           pica.vs_setup.GetProgramCodeHash()!=0x89b1ab98da3d9241ULL ||
           pica.vs_setup.GetSwizzleDataHash()!=0x6f0b398b1fc186a9ULL ||
           regs.pipeline.num_vertices!=72 || !blend.blend_enable ||
           blend.src_color_blend_factor!=BF::SourceAlpha || blend.dst_color_blend_factor!=BF::One ||
           blend.color_blend_eq!=Pica::FramebufferRegs::BlendEquation::Add || blend.color_write_mask!=15 ||
           depth.depth_test_enable || depth.depth_write_enable || depth.stencil_test_enable ||
           !texture.texture0_enable || texture.texture1_enable || texture.texture2_enable)return false;
        MM3D::CameraMidpoint::Matrix projection{},model=MM3D::CameraMidpoint::Identity();
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)projection[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)model[r][c]=pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
        MM3D::GlareInterpolation::Tev tev{};const auto stages=regs.texturing.GetTevStages();
        for(unsigned i=0;i<6;++i)tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,
            stages[i].ops_raw,stages[i].const_color,stages[i].scales_raw};
        return MM3D::GlareInterpolation::ScreenDraw(projection,model,tev);
    }();
    // The original PICA palette is still available when vertices ran on the CPU.
    // Capture both methods so actor poses can be correlated with their actual draws.
    if(MM3D::LinkPoseCapture::Recording() && !shadow_rendering && using_color_fb &&
       MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) &&
       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL) {
        MM3D::LinkPoseCapture::Draw sample;
        sample.frame=MM3D::guest_frames.load();sample.vertices=regs.pipeline.num_vertices;
        sample.vertex_base=regs.pipeline.vertex_attributes.GetPhysicalBaseAddress();
        sample.index_offset=is_indexed?u32(regs.pipeline.index_array.offset):0;
        for(unsigned bone=0;bone<10;++bone)for(unsigned i=0;i<12;++i)
            sample.palette[bone][i]=pica.vs_setup.uniforms.f[20+bone*3+i/4][i%4].ToFloat32();
        MM3D::LinkPoseCapture::Observe(std::move(sample));
    }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    if(MM3D::EffectCapture::Recording(MM3D::guest_frames.load()) && !shadow_rendering && using_color_fb &&
       MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight())) {
        MM3D::EffectCapture::Draw d;
        d.frame=MM3D::guest_frames.load();d.shader=pica.vs_setup.GetProgramCodeHash();
        d.swizzle=pica.vs_setup.GetSwizzleDataHash();d.vertices=regs.pipeline.num_vertices;
        d.base=regs.pipeline.vertex_attributes.GetPhysicalBaseAddress();
        d.offset=is_indexed?u32(regs.pipeline.index_array.offset):0;
        d.indexed=is_indexed;d.accelerated=accelerate;
        d.textures={regs.texturing.texture0.GetPhysicalAddress(),regs.texturing.texture1.GetPhysicalAddress(),regs.texturing.texture2.GetPhysicalAddress()};
        const auto& b=pipeline_info.state.blending;
        d.state={unsigned(b.blend_enable),unsigned(b.src_color_blend_factor.Value()),unsigned(b.dst_color_blend_factor.Value()),
            unsigned(b.color_blend_eq.Value()),unsigned(b.color_write_mask),
            unsigned(pipeline_info.state.depth_stencil.depth_test_enable),unsigned(pipeline_info.state.depth_stencil.stencil_test_enable)};
        d.depth_write=bool(pipeline_info.state.depth_stencil.depth_write_enable);
        const auto& native_fb=regs.framebuffer.framebuffer;
        d.framebuffer=native_fb.GetColorBufferPhysicalAddress();
        d.framebuffer_width=native_fb.GetWidth();d.framebuffer_height=native_fb.GetHeight();
        d.framebuffer_format=unsigned(native_fb.color_format.Value());
        const auto& texture_units=regs.texturing.main_config;
        d.texture_mask=unsigned(texture_units.texture0_enable)|
            (unsigned(texture_units.texture1_enable)<<1)|(unsigned(texture_units.texture2_enable)<<2);
        d.alpha_blend={unsigned(b.src_alpha_blend_factor.Value()),unsigned(b.dst_alpha_blend_factor.Value()),
            unsigned(b.alpha_blend_eq.Value())};
        const auto& merger=regs.framebuffer.output_merger;
        d.alpha_test={unsigned(merger.alpha_test.enable),unsigned(merger.alpha_test.func.Value()),unsigned(merger.alpha_test.ref)};
        const auto& stencil_state=pipeline_info.state.depth_stencil;
        const auto& dynamic=pipeline_info.dynamic_info;
        d.stencil={unsigned(stencil_state.stencil_test_enable),unsigned(stencil_state.stencil_compare_op.Value()),
            dynamic.stencil_reference,dynamic.stencil_compare_mask,dynamic.stencil_write_mask,
            unsigned(stencil_state.stencil_fail_op.Value()),unsigned(stencil_state.stencil_depth_fail_op.Value()),
            unsigned(stencil_state.stencil_pass_op.Value())};
        d.clip_scale={vs_data.clip_scale_x,vs_data.clip_scale_y};
        const bool small_stencil=stencil_state.stencil_test_enable && d.vertices<=144;
        if(small_stencil && !accelerate && vertex_batch.size()<=432) {
            for(const auto& vertex:vertex_batch)
                d.clip_positions.push_back({vertex.position.x,vertex.position.y,vertex.position.z,vertex.position.w});
        }
        d.blend_constant=merger.blend_const.raw;
        const auto stages=regs.texturing.GetTevStages();
        for(unsigned i=0;i<stages.size();++i)
            d.tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,stages[i].ops_raw,
                stages[i].const_color,stages[i].scales_raw};
        for(unsigned r=0;r<96;++r)for(unsigned c=0;c<4;++c)d.uniforms[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        // Capture full ripple and four-attribute debris strips, including
        // Majora's hundred-fragment burst (600 submitted vertices).
        const bool particle_strip=d.vertices<=768 &&
            regs.pipeline.triangle_topology==Pica::PipelineRegs::TriangleTopology::Strip &&
            regs.vs.GetRegisterForAttribute(0)==0 &&
            ((regs.vs.max_input_attribute_index==2 && regs.vs.GetRegisterForAttribute(1)==3 &&
              regs.vs.GetRegisterForAttribute(2)==4) ||
             (regs.vs.max_input_attribute_index==3 && regs.vs.GetRegisterForAttribute(1)==1 &&
              regs.vs.GetRegisterForAttribute(2)==3 && regs.vs.GetRegisterForAttribute(3)==4));
        const unsigned cpu_capture_limit=particle_strip?768u:144u;
        if(cpu_interpolation && pica.CanReplayCpuVertices() &&
           d.shader==0x89b1ab98da3d9241ULL && d.vertices<=cpu_capture_limit &&
           (glare_candidate || small_stencil || (b.blend_enable && b.src_color_blend_factor==Pica::FramebufferRegs::BlendFactor::SourceAlpha &&
           (pipeline_info.state.depth_stencil.depth_test_enable || d.vertices==4)))) {
            const auto& replay=pica.GetCpuVertexReplay();
            if(replay.Inputs().size()<=cpu_capture_limit && replay.Order().size()<=cpu_capture_limit) {
                d.cpu_topology=unsigned(regs.pipeline.triangle_topology.Value());
                d.cpu_attribute_count=unsigned(regs.vs.max_input_attribute_index)+1;
                for(unsigned a=0;a<16;++a)d.cpu_input_registers[a]=regs.vs.GetRegisterForAttribute(a);
                d.cpu_inputs.resize(replay.Inputs().size());
                for(std::size_t i=0;i<replay.Inputs().size();++i)
                    for(unsigned a=0;a<16;++a)for(unsigned c=0;c<4;++c)
                        d.cpu_inputs[i][a][c]=replay.Inputs()[i][a][c].ToFloat32();
                d.cpu_order=replay.Order();
            }
        }
        if(accelerate && d.shader==0x89b1ab98da3d9241ULL && d.vertices<=144 &&
           (glare_candidate || small_stencil || (b.blend_enable && b.src_color_blend_factor==Pica::FramebufferRegs::BlendFactor::SourceAlpha &&
           (pipeline_info.state.depth_stencil.depth_test_enable || d.vertices==4)))) {
            const auto& attributes=regs.pipeline.vertex_attributes;
            for(unsigned i=0;i<12;++i)d.formats[i]={unsigned(attributes.GetFormat(i)),
                attributes.GetNumElements(i),regs.vs.GetRegisterForAttribute(i)};
            unsigned total=0;
            for(const auto& loader:attributes.attribute_loaders) {
                if(!loader.component_count || !loader.byte_count ||
                   vertex_info.vs_input_index_max<vertex_info.vs_input_index_min)continue;
                const u64 address=u64(attributes.GetPhysicalBaseAddress())+loader.data_offset+
                    u64(vertex_info.vs_input_index_min)*loader.byte_count;
                const u64 size=(u64(vertex_info.vs_input_index_max)-vertex_info.vs_input_index_min+1)*loader.byte_count;
                if(address>0xffffffff || !size || size>4096 || total+size>8192)continue;
                const auto src=memory.GetPhysicalRef(static_cast<PAddr>(address));
                if(src.GetSize()<size)continue;
                MM3D::EffectCapture::Buffer buffer;
                buffer.first=vertex_info.vs_input_index_min;buffer.last=vertex_info.vs_input_index_max;
                buffer.stride=loader.byte_count;
                for(unsigned i=0;i<loader.component_count && i<12;++i)buffer.components.push_back(loader.GetComponent(i));
                buffer.bytes.assign(src.GetPtr(),src.GetPtr()+size);total+=unsigned(size);
                d.buffers.push_back(std::move(buffer));
            }
        }
        MM3D::EffectCapture::Observe(std::move(d));
    }
#endif
#ifdef ENABLE_OOT3D_PRESENTATION
    if(OOT3D::Widescreen::Active() && !shadow_rendering && using_color_fb &&
       OOT3D::Widescreen::IsTopViewport(oot_viewport.GetWidth(),oot_viewport.GetHeight()) &&
       pica.vs_setup.GetProgramCodeHash()==OOT3D::Interpolation::ModelShader &&
       pica.vs_setup.GetSwizzleDataHash()==OOT3D::Interpolation::ModelSwizzle) {
        OOT3D::Interpolation::Pose::Palette palette;
        OOT3D::Interpolation::Matrix projection;
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
            projection[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        for(unsigned i=0;i<10;++i) {
            palette[i]=OOT3D::Interpolation::Math::Identity();
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                palette[i][r][c]=pica.vs_setup.uniforms.f[20+i*3+r][c].ToFloat32();
        }
        const auto camera=OOT3D::Interpolation::CameraSnapshot();
        if(oot_pose_replay.SongEffect(palette,camera,OOT3D::Interpolation::guest_frames.load(),
           Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count())) {
            // These effects were sized for the native camera. Undo the custom
            // lens widening and let the backdrop stretch across the extra side
            // columns. Shared clip uniforms cover both CPU/accelerated draws
            // and native/midpoint passes without changing any world geometry.
            const float zoom=OOT3D::SongWidescreen::Zoom(camera.native_fov,projection);
            vs_data.clip_scale_x=zoom;vs_data.clip_scale_y=zoom;vs_data_dirty=true;
        }
    }
#endif
    UploadUniforms(accelerate);
#ifdef ENABLE_OOT3D_PRESENTATION
    if (OOT3D::Textures::hide_current_draw) {
        OOT3D::Textures::hidden.fetch_add(1);
        return true;
    }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    if (MM3D::Textures::hide_current_draw) {
        MM3D::Textures::hidden.fetch_add(1);
        return true;
    }
    if (MM3D::draw_probe.Hide(MM3D::guest_frames.load(), pica.vs_setup.GetProgramCodeHash(),
                              regs.pipeline.num_vertices, accelerate)) {
        return true;
    }
#endif

    // CPU geometry must be uploaded outside recording scopes: Map can drain
    // batches or submit work. Retain immutable offsets for both target draws.
    struct CpuDraw { u32 offset{}, count{}; };
    CpuDraw native_cpu_draw{}, midpoint_cpu_draw{}, splash_old_cpu_draw{};
    bool use_midpoint_cpu_draw = false;
    const auto upload_cpu_draw = [&](const std::vector<HardwareVertex>& vertices) {
        CpuDraw draw;
        draw.count = static_cast<u32>(vertices.size());
        if (!draw.count) return draw;
        const u32 bytes = draw.count * sizeof(HardwareVertex);
        const auto [data, offset, invalidated] = stream_buffer.Map(bytes, sizeof(HardwareVertex));
        std::memcpy(data, vertices.data(), bytes);
        stream_buffer.Commit(bytes);
        draw.offset = offset;
        return draw;
    };
    if (cpu_interpolation) native_cpu_draw = upload_cpu_draw(vertex_batch);

#ifdef ENABLE_OOT3D_PRESENTATION
    // Capture the camera/projection and depth convention from verified scene
    // draws. The name pass uses these exact transforms, not presentation math.
    const auto name_frame=OOT3D::Interpolation::guest_frames.load();
    if(oot_name_scene.frame!=name_frame){oot_name_scene={};oot_name_scene.frame=name_frame;}
    const bool name_session=[] {
        const auto session=OOT3D::MultiplayerStartup::GetSession();return session && session->role;
    }();
    bool oot_name_hud=false;
    if(name_session && OOT3D::RemoteNames::GetLayout().enabled && !shadow_rendering && using_color_fb &&
       OOT3D::Widescreen::IsTopViewport(oot_viewport.GetWidth(),oot_viewport.GetHeight())) {
        using namespace OOT3D::Interpolation;
        const auto& fb=regs.framebuffer.framebuffer;
        const auto sample=CameraSnapshot();
        Matrix lens{},camera=Math::Identity();
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)lens[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)camera[r][c]=pica.vs_setup.uniforms.f[InverseViewRow+r][c].ToFloat32();
        using CF=Pica::FramebufferRegs::CompareFunc;
        const auto compare=regs.framebuffer.output_merger.depth_test_func.Value();
        const bool compared=compare==CF::LessThan || compare==CF::LessThanOrEqual ||
            compare==CF::GreaterThan || compare==CF::GreaterThanOrEqual;
        if(!oot_name_scene.drawn && using_depth_fb && write_depth_fb && compared &&
           pica.vs_setup.GetProgramCodeHash()==ModelShader && pica.vs_setup.GetSwizzleDataHash()==ModelSwizzle &&
           WorldProjection(lens,sample) && CameraMatches(camera,sample)) {
            auto& n=oot_name_scene;n.scene.camera=sample;n.scene.projection=lens;n.scene.inverse_view=camera;
            n.scene.clip_x=vs_data.clip_scale_x;n.scene.clip_y=vs_data.clip_scale_y;
            n.scene.depth_scale=fs_data.depth_scale;n.scene.depth_offset=fs_data.depth_offset;
            n.scene.wbuffer=regs.rasterizer.depthmap_enable==Pica::RasterizerRegs::DepthBuffering::WBuffering;
            n.color=fb.GetColorBufferPhysicalAddress();n.depth=fb.GetDepthBufferPhysicalAddress();
            n.compare=(compare==CF::GreaterThan || compare==CF::GreaterThanOrEqual)?vk::CompareOp::eGreaterOrEqual:vk::CompareOp::eLessOrEqual;
            const auto v=fb_helper.Viewport();const auto r=fb_helper.DrawRect();
            n.viewport={float(v.x),float(v.y),float(v.width),float(v.height),0,1};
            n.scissor={{s32(r.left),s32(r.bottom)},{r.GetWidth(),r.GetHeight()}};
        }
        if(const auto hud=OotHudMesh();hud && hud->first==92 && hud->second)
            oot_name_hud=true;
        auto& n=oot_name_scene;
        if(oot_name_hud && !n.drawn && n.color==fb.GetColorBufferPhysicalAddress() &&
           n.depth && n.depth==fb.GetDepthBufferPhysicalAddress() && n.scene.camera.epoch==generation.load()) {
            n.drawn=true;
            if(!OOT3D::RemoteNames::World(n.scene.camera,1,n.scene.inverse_view).anchors.empty()) {
                if(!oot_name_renderer)oot_name_renderer=std::make_unique<RemoteNameRenderer>(instance,scheduler,renderpass_cache,update_queue,runtime);
                const auto name_fb=res_cache.GetFramebufferSurfaces(true,true);
                const auto* target=name_fb.Framebuffer();
                const auto draw=oot_name_renderer->Prepare(stream_buffer,n.scene,1,
                    target->Format(SurfaceType::Color),target->Format(SurfaceType::Depth),n.compare);
                if(draw.count && target->Handle()) {
                    renderpass_cache.BeginRendering(target,name_fb.DrawRect());
                    oot_name_renderer->Render(draw,stream_buffer.Handle(),n.viewport,n.scissor);
                }
            }
        }
    }
#endif

    // Begin rendering
    // Mapping indices may submit work and end an open pass. In batched mode
    // upload once before beginning the native pass, then reuse for its replays.
    if (accelerate && is_indexed && scheduler.ReplayBatchingEnabled()) SetupIndexArray();
    Scheduler::NativeDrawScope native_draw_scope{scheduler,
        (accelerate || cpu_interpolation) && !shadow_rendering};
    const auto draw_rect = fb_helper.DrawRect();
    renderpass_cache.BeginRendering(framebuffer, draw_rect);

    // Configure viewport and scissor
    const auto viewport = fb_helper.Viewport();
    pipeline_info.dynamic_info.viewport = Common::Rectangle<s32>{
        viewport.x,
        viewport.y,
        viewport.x + viewport.width,
        viewport.y + viewport.height,
    };
    pipeline_info.dynamic_info.scissor = draw_rect;
#ifdef ENABLE_MM3D_PRESENTATION
    auto guest_profile = std::find_if(mm3d_draw_profiles.begin(),mm3d_draw_profiles.end(),
        [](const auto& p){return !p.presentation && !p.atlas_selection && p.id == "@preview";});
    if (guest_profile == mm3d_draw_profiles.end())
        guest_profile = std::find_if(mm3d_draw_profiles.begin(),mm3d_draw_profiles.end(),
            [](const auto& p){return !p.presentation && !p.atlas_selection;});
    const auto transform_viewport=[&](const MM3D::Profiles::Profile& transform) {
        // Diagnostic screen-space transform; shader animation and guest data stay intact.
        // Preserve viewport orientation, including flipped/negative-height viewports.
        auto& bounds = pipeline_info.dynamic_info.viewport;
        const s32 dx = bounds.right - bounds.left;
        const s32 dy = bounds.bottom - bounds.top;
        if (std::abs(dx) >= 2 && std::abs(dy) >= 2) {
            const float scale = std::clamp(transform.scale, .1f, 2.f);
            const s32 width = static_cast<s32>(dx * scale);
            const s32 height = static_cast<s32>(dy * scale);
            const s32 cx = bounds.left + dx / 2 + static_cast<s32>(dx * transform.y);
            const s32 cy = bounds.top + dy / 2 + static_cast<s32>(dy * transform.x);
            bounds.left = cx - width / 2;
            bounds.top = cy - height / 2;
            bounds.right = bounds.left + width;
            bounds.bottom = bounds.top + height;
            // Crop follows the transformed draw. Display X/Y are rotated relative to PICA X/Y.
            const auto& crop = transform.crop;
            auto& scissor = pipeline_info.dynamic_info.scissor;
            const s32 min_x = std::min(bounds.left,bounds.right), min_y = std::min(bounds.top,bounds.bottom);
            const s32 size_x = std::abs(bounds.right-bounds.left), size_y = std::abs(bounds.bottom-bounds.top);
            const s32 left = std::clamp<s32>(min_x+size_x*(1.f-crop[3]),scissor.left,scissor.right);
            const s32 right = std::clamp<s32>(min_x+size_x*(1.f-crop[1]),left,scissor.right);
            const s32 bottom = std::clamp<s32>(min_y+size_y*crop[0],scissor.bottom,scissor.top);
            const s32 top = std::clamp<s32>(min_y+size_y*crop[2],bottom,scissor.top);
            scissor = {u32(left),u32(top),u32(right),u32(bottom)};
            MM3D::Textures::scaled_draws.fetch_add(1);
        }
    };
    if (guest_profile != mm3d_draw_profiles.end()) transform_viewport(*guest_profile);
    MM3D::draw_probe.ObserveScissor(MM3D::guest_frames.load(),
        pica.vs_setup.GetProgramCodeHash(), regs.pipeline.num_vertices,
        pipeline_info.dynamic_info.scissor.left, pipeline_info.dynamic_info.scissor.top,
        pipeline_info.dynamic_info.scissor.right, pipeline_info.dynamic_info.scissor.bottom);
    if(guest_regions.info[0]>0 || padded_scene_copy) pipeline_cache.BindPipeline(pipeline_info,true);
#endif

    // Every replay must use the same geometry path as the original draw. CPU
    // shader output is already a triangle list in vertex_batch; do not route it
    // through the accelerated path's guest attribute/index bindings.
    const auto draw_batch = [&]() -> bool {
        if (accelerate) {
            return AccelerateDrawBatchInternal(is_indexed);
        }
        if (!pipeline_cache.BindPipeline(pipeline_info, true)) {
            return false;
        }
        if (cpu_interpolation) {
            const auto draw = use_midpoint_cpu_draw ? midpoint_cpu_draw : native_cpu_draw;
            scheduler.Record([buffer = stream_buffer.Handle(), draw](vk::CommandBuffer cmdbuf) {
                cmdbuf.bindVertexBuffers(0, buffer, draw.offset);
                cmdbuf.draw(draw.count, 1, 0, 0);
            });
            return true;
        }
        const u32 vertex_count = static_cast<u32>(vertex_batch.size());
        const u32 vertex_size = vertex_count * sizeof(HardwareVertex);
        const auto [buffer, offset, _] = stream_buffer.Map(vertex_size, sizeof(HardwareVertex));
        std::memcpy(buffer, vertex_batch.data(), vertex_size);
        stream_buffer.Commit(vertex_size);
        scheduler.Record([this, offset, vertex_count](vk::CommandBuffer cmdbuf) {
            cmdbuf.bindVertexBuffers(0, stream_buffer.Handle(), offset);
            cmdbuf.draw(vertex_count, 1, 0, 0);
        });
        return true;
    };
#ifdef ENABLE_MM3D_PRESENTATION
    if(MM3D::CameraMidpoint::suppress_transform_feedback.load() &&
       MM3D::CameraMidpoint::present_60.load() && replay_geometry && !shadow_rendering &&
       using_color_fb && !use_pause &&
       MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) &&
       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL) {
        using namespace MM3D::CameraMidpoint;
        Matrix projection{},model=Identity();
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)
            projection[i][j]=pica.vs_setup.uniforms.f[i][j].ToFloat32();
        for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)
            model[i][j]=pica.vs_setup.uniforms.f[20+i][j].ToFloat32();
        const auto& blend=pipeline_info.state.blending;
        const auto& textures=regs.texturing.main_config;
        using BF=Pica::FramebufferRegs::BlendFactor;
        if(blend.blend_enable && blend.src_color_blend_factor==BF::SourceAlpha &&
           blend.dst_color_blend_factor==BF::OneMinusSourceAlpha &&
           blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add &&
           blend.color_write_mask==15 && !pipeline_info.state.depth_stencil.stencil_test_enable &&
           TransformationFeedback(projection,model,
               midpoint_scene_copies.PreviousContains(MM3D::guest_frames.load(),
                   regs.texturing.texture0.GetPhysicalAddress()),regs.pipeline.num_vertices,
               unsigned(textures.texture0_enable)|(unsigned(textures.texture1_enable)<<1)|
                   (unsigned(textures.texture2_enable)<<2))) {
            vertex_batch.clear();return true;
        }
    }
#endif
    const bool succeeded = draw_batch();
    native_draw_scope.End();
#ifdef ENABLE_OOT3D_PRESENTATION
    if(OOT3D::Interpolation::Replaying() && !accelerate && !shadow_rendering && using_color_fb &&
       OOT3D::Widescreen::IsTopViewport(oot_viewport.GetWidth(),oot_viewport.GetHeight())) {
        const auto kind=OOT3D::Interpolation::SurfaceEffects::Candidate(regs,
            pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash());
        if(kind)++OOT3D::Interpolation::SurfaceEffects::replay_counts[kind].seen;
    }
    if(OOT3D::Interpolation::SurfaceCapture::active.load() && !shadow_rendering && using_color_fb &&
       OOT3D::Widescreen::IsTopViewport(oot_viewport.GetWidth(),oot_viewport.GetHeight())) {
        using namespace OOT3D::Interpolation;
        SurfaceCapture::Observe(guest_frames.load(),[&](SurfaceCapture::Draw& d) {
            d.time_us=Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count();
            d.generation=generation.load();d.scene=CameraSnapshot().scene;
            d.shader=pica.vs_setup.GetProgramCodeHash();d.swizzle=pica.vs_setup.GetSwizzleDataHash();
            d.target=regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress();
            d.vertices=regs.pipeline.num_vertices;d.cpu=!accelerate;d.succeeded=succeeded;
            namespace T=OOT3D::TextboxInterpolation;
            d.textbox_active=T::Active(d.frame,d.generation);
            if(d.textbox_active)if(const auto hash=OotUIMeshHash(T::MaxVertices,true)) {
                d.textbox_hash=hash->first;
                T::Matrix matrix{};
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                    matrix[r][c]=pica.vs_setup.uniforms.f[T::ModelViewRow+r][c].ToFloat32();
                if(const auto route=T::Find(hash->first,hash->second,matrix,d.frame,d.generation)) {
                    d.textbox_match=true;d.textbox_source=route->source;
                }
            }
            d.vertex_base=regs.pipeline.vertex_attributes.GetPhysicalBaseAddress();
            d.indexed=is_indexed;d.index16=is_indexed && regs.pipeline.index_array.format!=0;
            d.index_offset=is_indexed?u32(regs.pipeline.index_array.offset):0;
            d.topology=unsigned(regs.pipeline.triangle_topology.Value());
            for(unsigned r=0;r<96;++r)for(unsigned c=0;c<4;++c)
                d.uniforms[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                d.integer_uniforms[r][c]=pica.vs_setup.uniforms.i[r][c];
            for(unsigned i=0;i<16;++i)if(pica.vs_setup.uniforms.b[i])d.boolean_uniforms|=1u<<i;
            const auto textures=regs.texturing.GetTextures();
            for(unsigned i=0;i<3;++i) {
                const auto& t=textures[i];const auto& cfg=t.config;
                d.textures[i]={t.enabled,cfg.GetPhysicalAddress(),u32(cfg.width),u32(cfg.height),
                    u32(t.format),u32(cfg.wrap_s.Value()),u32(cfg.wrap_t.Value()),
                    u32(cfg.min_filter.Value()),u32(cfg.mag_filter.Value()),i==0?u32(cfg.type.Value()):0u};
            }
            const auto stages=regs.texturing.GetTevStages();
            for(unsigned i=0;i<6;++i) {
                const auto& t=stages[i];
                d.tev[i]={t.sources_raw,t.modifiers_raw,t.ops_raw,t.const_color,t.scales_raw};
            }
            const auto& b=pipeline_info.state.blending;
            d.blend={u32(b.blend_enable),u32(b.src_color_blend_factor.Value()),
                u32(b.dst_color_blend_factor.Value()),u32(b.src_alpha_blend_factor.Value()),
                u32(b.dst_alpha_blend_factor.Value()),u32(b.color_blend_eq.Value()),u32(b.alpha_blend_eq.Value())};
            d.blend_color=regs.framebuffer.output_merger.blend_const.raw;
            for(unsigned i=0;i<16;++i)d.attribute_registers[i]=regs.vs.GetRegisterForAttribute(i);
            const auto& alpha=regs.framebuffer.output_merger.alpha_test;
            d.alpha_test={u32(alpha.enable),u32(alpha.func.Value()),u32(alpha.ref)};
            const auto& depth=pipeline_info.state.depth_stencil;
            d.depth={u32(depth.depth_test_enable),u32(depth.depth_write_enable),u32(depth.stencil_test_enable)};
            if(accelerate && succeeded) {
                d.first_vertex=vertex_info.vs_input_index_min;d.last_vertex=vertex_info.vs_input_index_max;
                d.attribute_count=regs.pipeline.vertex_attributes.GetNumTotalAttributes();
                if(d.vertices && d.last_vertex>=d.first_vertex) {
                    d.input_count=unsigned(std::min<std::uint64_t>(d.inputs.size(),
                        std::uint64_t(d.last_vertex)-d.first_vertex+1));
                    Pica::VertexLoader loader(memory,regs.pipeline);
                    Pica::AttributeBuffer input{},defaults=pica.input_default_attributes;
                    for(unsigned i=0;i<d.input_count;++i) {
                        const unsigned index=d.first_vertex+i;
                        loader.LoadVertex(d.vertex_base,index,index,input,defaults);
                        for(unsigned a=0;a<16;++a)for(unsigned c=0;c<4;++c)
                            d.inputs[i][a][c]=input[a][c].ToFloat32();
                    }
                }
            } else if(!accelerate) {
                if(OOT3D::Interpolation::SurfaceEffects::Candidate(regs,
                    pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash()) && pica.CanReplayCpuVertices()) {
                    const auto& replay=pica.GetCpuVertexReplay();
                    if(replay.Inputs().size()<=768 && replay.Order().size()<=768) {
                        d.effect_inputs.resize(replay.Inputs().size());d.effect_order=replay.Order();
                        for(unsigned i=0;i<d.effect_inputs.size();++i)for(unsigned a=0;a<4;++a)for(unsigned c=0;c<4;++c)
                            d.effect_inputs[i][a][c]=replay.Inputs()[i][a][c].ToFloat32();
                    }
                }
                d.output_count=unsigned(std::min(d.output_vertices.size(),vertex_batch.size()));
                for(unsigned i=0;i<d.output_count;++i) {
                    const auto& v=vertex_batch[i];auto& out=d.output_vertices[i];
                    out.uv0_w=v.tex_coord0_w;
                    for(unsigned c=0;c<4;++c){out.position[c]=v.position[c];out.color[c]=v.color[c];}
                    for(unsigned c=0;c<2;++c) {
                        out.uv[c]=v.tex_coord0[c];out.uv[2+c]=v.tex_coord1[c];out.uv[4+c]=v.tex_coord2[c];
                    }
                }
            }
        });
    }
    if(OOT3D::Interpolation::Replaying() && !shadow_rendering && using_color_fb &&
       OOT3D::Widescreen::IsTopViewport(oot_viewport.GetWidth(),oot_viewport.GetHeight())) {
        using namespace OOT3D::Interpolation;
        const auto frame=guest_frames.load();
        const auto address=regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress();
        const bool first_draw=oot_replay_frame.Begin(frame,address);
        if(first_draw) {
            int rate=TargetFPS();
            if(Settings::values.use_vsync.GetValue())rate=std::min(rate,
                std::clamp(int(std::lround(display_refresh_rate.load())),30,120));
            oot_rate_plan=oot_rate_clock.Next(frame,rate);
        }
        for(unsigned slot_index=0;slot_index<oot_rate_plan.sample_slots;++slot_index) {
        // At exact multiples of 30 the last slot is always the existing native
        // image. It needs neither a private attachment nor duplicate history.
        if(oot_rate_plan.fps%30==0 && oot_rate_plan.phases[slot_index]>=1.f)continue;
        auto& slot=oot_replay_slots[slot_index];
        splash_old_cpu_draw={};
        vk::DescriptorSet old_effect_texture_set{};
        auto& oot_replay_camera=slot.camera;
        auto& oot_surface_effects=slot.effects;
        auto& oot_waterfall_history=slot.waterfall;
        auto& oot_debris=slot.debris;
        auto& oot_fairy_glow_history=slot.fairy;
        auto& oot_textbox_history=slot.textboxes;
        auto& oot_circle_wipe_history=slot.circle_wipe;
        sample_fraction=oot_rate_plan.phases[slot_index];
        if(!slot.layer)slot.layer=std::make_unique<MM3D::PromptLayer>(instance,scheduler,renderpass_cache,true);
        auto& layer=*slot.layer;
        if(first_draw) {
            const auto& fb=regs.framebuffer.framebuffer;
            layer.NextFrame(oot_color_clears.Background(frame,address,
                fb.GetWidth(),fb.GetHeight(),fb.color_format==Pica::FramebufferRegs::ColorFormat::RGBA8));
        }
        Matrix projection{},camera=Math::Identity();
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
            projection[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
            camera[r][c]=pica.vs_setup.uniforms.f[InverseViewRow+r][c].ToFloat32();
        const bool model_shader=pica.vs_setup.GetProgramCodeHash()==ModelShader &&
            pica.vs_setup.GetSwizzleDataHash()==ModelSwizzle;
        const auto sample=CameraSnapshot();
        const bool perspective=model_shader && WorldProjection(projection,sample);
        oot_fairy_glow.reset();
        oot_waterfall_uv.reset();
        oot_song_visual.reset();
        oot_model_effect.reset();
        oot_sparkle_color.reset();
        oot_song_visual_layer=SongVisual::Layer::None;
        if(oot_storm_clouds && SurfaceEffects::enabled) {
            SongVisual::Sample visual;
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                visual.uv[r][c]=pica.vs_setup.uniforms.f[SongVisual::Rows[r]][c].ToFloat32();
            for(unsigned c=0;c<4;++c)visual.color[c]=pica.vs_setup.uniforms.f[8][c].ToFloat32();
            visual.alpha=float(regs.texturing.tev_stage0.const_color>>24)/255.f;
            oot_song_visual_layer=SongVisual::Layer::Clouds;
            // CPU and accelerated submissions can alternate for this overlay.
            // Its unique full-screen material owns history, not a transient VBO.
            oot_song_visual=slot.songs.Apply(frame,{0,0,regs.texturing.texture0.GetPhysicalAddress(),
                generation.load(),sample.scene,sample.owner,sample.cut_group,unsigned(oot_song_visual_layer)},
                oot_song_visual_layer,Math::Identity(),visual);
            if(oot_song_visual)fs_data_dirty=true;
        }
        if(perspective && SurfaceEffects::enabled && Waterfall::Candidate(regs,sample.scene)) {
            Waterfall::UV uv;
            for(unsigned r=0;r<Waterfall::Rows.size();++r)for(unsigned c=0;c<4;++c)
                uv[r][c]=pica.vs_setup.uniforms.f[Waterfall::Rows[r]][c].ToFloat32();
            oot_waterfall_uv=oot_waterfall_history.Apply(frame,{
                regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(),u32(regs.pipeline.index_array.offset),
                regs.texturing.texture0.GetPhysicalAddress(),regs.texturing.texture1.GetPhysicalAddress(),
                regs.texturing.main_config.texture2_enable?regs.texturing.texture2.GetPhysicalAddress():0,
                generation.load(),sample.scene},uv);
        }
        if(perspective) {
            const auto now=Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count();
            oot_replay_projection=oot_replay_camera.Observe(frame,sample,camera,projection,now);
            if(oot_replay_projection) {
                oot_replay_frame.world=true;
                Pose::Palette palette;
                for(unsigned i=0;i<10;++i) {
                    palette[i]=Math::Identity();
                    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                        palette[i][r][c]=pica.vs_setup.uniforms.f[20+i*3+r][c].ToFloat32();
                }
                oot_pose_override=oot_pose_replay.Select(palette,sample,frame,now);
                if(SurfaceEffects::enabled) {
                    if(const auto kind=ModelEffects::Candidate(regs);kind!=ModelEffects::None) {
                        ModelEffects::Sample visual;visual.model=palette[0];
                        visual.texture=regs.texturing.texture0.GetPhysicalAddress();
                        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                            visual.uv[r][c]=pica.vs_setup.uniforms.f[ModelEffects::Rows[r]][c].ToFloat32();
                        for(unsigned c=0;c<4;++c)visual.color[c]=pica.vs_setup.uniforms.f[8][c].ToFloat32();
                        const auto stages=regs.texturing.GetTevStages();
                        for(unsigned r=0;r<6;++r)for(unsigned c=0;c<4;++c)visual.colors[r][c]=float((stages[r].const_color>>(8*c))&255)/255.f;
                        oot_model_effect=slot.models.Apply(frame,{
                            regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(),u32(regs.pipeline.index_array.offset),
                            kind==ModelEffects::Biri?0u:visual.texture,
                            regs.texturing.main_config.texture1_enable?regs.texturing.texture1.GetPhysicalAddress():0u,
                            generation.load(),sample.scene,sample.cut_group,unsigned(kind)},camera,visual);
                        // The single Syabom billboard carries its collision
                        // squash/shear in this matrix. A rigid actor palette
                        // must not replace the dedicated deformation sample.
                        if(oot_model_effect && kind==ModelEffects::Syabom)oot_pose_override.reset();
                        if(oot_model_effect)fs_data_dirty=true;
                        if(oot_model_effect && oot_model_effect->previous_texture && sample_fraction<1) {
                            auto config=regs.texturing.texture0;
                            config.address.Assign(oot_model_effect->previous_texture/8);
                            const Pica::TexturingRegs::FullTextureConfig texture{1,config,regs.texturing.texture0_format};
                            auto& surface=res_cache.GetTextureSurface(texture);
                            auto& sampler=res_cache.GetSampler(texture.config);
                            old_effect_texture_set=pipeline_cache.Acquire(DescriptorHeapType::Texture);
                            update_queue.AddImageSampler(old_effect_texture_set,0,0,surface.ImageView(),sampler.Handle());
                            auto& null=res_cache.GetSurface(VideoCore::NULL_SURFACE_ID);
                            auto& null_sampler=res_cache.GetSampler(VideoCore::NULL_SAMPLER_ID);
                            for(unsigned i=1;i<3;++i)update_queue.AddImageSampler(old_effect_texture_set,i,0,null.ImageView(),null_sampler.Handle());
                            pipeline_cache.Select(DescriptorHeapType::Texture,oot_native_texture_set);
                        }
                    }
                    const auto kind=SongVisual::Candidate(regs);
                    if(kind!=SongVisual::Layer::None && oot_pose_replay.SongEffect(palette,sample,frame,now)) {
                        SongVisual::Sample visual;
                        visual.model_view=palette[0];
                        for(unsigned c=0;c<4;++c)visual.color[c]=pica.vs_setup.uniforms.f[8][c].ToFloat32();
                        for(unsigned r=0;r<SongVisual::UVRows(kind);++r)for(unsigned c=0;c<4;++c)
                            visual.uv[r][c]=pica.vs_setup.uniforms.f[SongVisual::Rows[r]][c].ToFloat32();
                        visual.alpha=float(regs.texturing.tev_stage0.const_color>>24)/255.f;
                        oot_song_visual=slot.songs.Apply(frame,{
                            regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(),u32(regs.pipeline.index_array.offset),
                            kind==SongVisual::Layer::Backdrop?0u:regs.texturing.texture0.GetPhysicalAddress(),
                            generation.load(),sample.scene,sample.owner,sample.cut_group,unsigned(kind)},kind,camera,visual);
                        oot_song_visual_layer=kind;
                        if(oot_song_visual)fs_data_dirty=true;
                    }
                }
                if(SurfaceEffects::enabled)if(const auto kind=FairyGlow::Candidate(regs)) {
                    std::array<FairyGlow::Vertex,4> vertices{};
                    bool valid=false;
                    if(accelerate && vertex_info.vs_input_index_max==vertex_info.vs_input_index_min+3) {
                        Pica::VertexLoader loader(memory,regs.pipeline);
                        Pica::AttributeBuffer input{},defaults=pica.input_default_attributes;
                        for(unsigned i=0;i<4;++i) {
                            const auto index=vertex_info.vs_input_index_min+i;
                            loader.LoadVertex(regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(),index,index,input,defaults);
                            for(unsigned a=0;a<2;++a)for(unsigned c=0;c<4;++c)vertices[i][a][c]=input[a][c].ToFloat32();
                        }
                        valid=FairyGlow::Geometry(vertices);
                    } else if(!accelerate && pica.CanReplayCpuVertices() && pica.GetCpuVertexReplay().Inputs().size()==4) {
                        for(unsigned i=0;i<4;++i)for(unsigned a=0;a<2;++a)for(unsigned c=0;c<4;++c)
                            vertices[i][a][c]=pica.GetCpuVertexReplay().Inputs()[i][a][c].ToFloat32();
                        valid=FairyGlow::Geometry(vertices);
                    }
                    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                        if(std::abs(pica.vs_setup.uniforms.f[4+r][c].ToFloat32()-float(r==c))>.0001f)valid=false;
                    if(valid && !oot_pose_override) {
                        FairyGlow::Color color;for(unsigned c=0;c<4;++c)color[c]=pica.vs_setup.uniforms.f[8][c].ToFloat32();
                        oot_fairy_glow_history.Context(generation.load(),sample.scene);
                        oot_fairy_glow=oot_fairy_glow_history.Apply(frame,{
                            regs.texturing.texture0.GetPhysicalAddress(),regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(),
                            u32(regs.pipeline.index_array.offset),kind},camera,palette[0],color);
                    }
                }
            }
            else {
                if(!oot_replay_frame.failed)RecordDiagnostic(camera,projection,sample,now);
                oot_replay_frame.Fail(oot_replay_camera.failure);
            }
        }
        // Song-menu models retain their current selection/artwork, but replay
        // the shared slide halfway between native frames in both CPU/GPU modes.
        oot_song_slide=(!perspective && model_shader)?OotSongSlide():std::nullopt;
        if(oot_song_slide)for(auto& value:*oot_song_slide)value*=2*(1-sample_fraction);
        if(OOT3D::OcarinaSlide::DiagnosticActive() && regs.pipeline.num_vertices==OOT3D::Ocarina::DrawQuadCount*6 && OOT3D::OcarinaSlide::renderer_logs.fetch_add(1)<400) {
            LOG_INFO(Render_Vulkan, "OoT song slide replay: frame={} cpu={} shader={:016x} swizzle={:016x} perspective={} route_active={} matched={} failed={} world={}",
                frame,!accelerate,pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash(),
                perspective,OOT3D::OcarinaSlide::Active(frame,generation.load()),bool(oot_song_slide),
                oot_replay_frame.failed,oot_replay_frame.world);
        }
        // Failed draws invalidate the entire pair.
        if(!succeeded)oot_replay_frame.Fail(ReplayFailure::Draw);
        if(viewport.x!=0 || viewport.y!=0 || viewport.width<=0 || viewport.height<=0)
            oot_replay_frame.Fail(ReplayFailure::Viewport);
        if(!accelerate && (oot_replay_projection || oot_song_slide || oot_song_visual)) {
            if(!pica.CanReplayCpuVertices())oot_replay_frame.Fail(ReplayFailure::Cpu);
            else {
                auto uniforms=pica.vs_setup.uniforms;
                if(oot_model_effect) {
                    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                        uniforms.f[ModelEffects::Rows[r]][c]=Pica::f24::FromFloat32(oot_model_effect->uv[r][c]);
                    for(unsigned c=0;c<4;++c)uniforms.f[8][c]=Pica::f24::FromFloat32(oot_model_effect->color[c]);
                    if(!oot_pose_override)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                        uniforms.f[20+r][c]=Pica::f24::FromFloat32(float(oot_model_effect->model[r][c]));
                }
                if(oot_fairy_glow) {
                    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                        uniforms.f[20+r][c]=Pica::f24::FromFloat32(float(oot_fairy_glow->model_view[r][c]));
                    for(unsigned c=0;c<4;++c)uniforms.f[8][c]=Pica::f24::FromFloat32(float(oot_fairy_glow->color[c]));
                }
                if(oot_waterfall_uv)for(unsigned r=0;r<Waterfall::Rows.size();++r)for(unsigned c=0;c<4;++c)
                    uniforms.f[Waterfall::Rows[r]][c]=Pica::f24::FromFloat32((*oot_waterfall_uv)[r][c]);
                if(oot_song_visual) {
                    for(unsigned c=0;c<4;++c)uniforms.f[8][c]=Pica::f24::FromFloat32(oot_song_visual->color[c]);
                    for(unsigned r=0;r<SongVisual::UVRows(oot_song_visual_layer);++r)for(unsigned c=0;c<4;++c)
                        uniforms.f[SongVisual::Rows[r]][c]=Pica::f24::FromFloat32(oot_song_visual->uv[r][c]);
                    if(!oot_pose_override && oot_song_visual->model_blended)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                        uniforms.f[20+r][c]=Pica::f24::FromFloat32(float(oot_song_visual->model_view[r][c]));
                }
                std::vector<Pica::AttributeBuffer> effect_inputs;
                std::vector<Pica::AttributeBuffer> old_sprite_inputs;
                const auto effect_kind=perspective?SurfaceEffects::Candidate(regs,
                    pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash()):0;
                if(effect_kind)++SurfaceEffects::replay_counts[effect_kind].attempted;
                if(effect_kind==3) {
                    const auto& replay=pica.GetCpuVertexReplay();
                    std::vector<Debris::Vertex> inputs(replay.Inputs().size());
                    for(unsigned i=0;i<inputs.size();++i)for(unsigned a=0;a<4;++a)for(unsigned c=0;c<4;++c)
                        inputs[i][a][c]=replay.Inputs()[i][a][c].ToFloat32();
                    oot_debris.Context(generation.load(),sample.scene);
                    if(const auto half=ReplayEffectStrip<Debris::Vertex>(inputs,replay.Order(),[&](const auto& vertices,const auto& order) {
                        ++SurfaceEffects::replay_counts[effect_kind].canonical;
                        return oot_debris.Apply(frame,regs.texturing.texture0.GetPhysicalAddress(),vertices,order);
                    })) {
                        if(*half!=inputs)++SurfaceEffects::replay_counts[effect_kind].changed;
                        effect_inputs=replay.Inputs();
                        for(unsigned i=0;i<inputs.size();++i)for(unsigned a=0;a<4;++a)for(unsigned c=0;c<4;++c)
                            effect_inputs[i][a][c]=Pica::f24::FromFloat32((*half)[i][a][c]);
                    }
                } else if(effect_kind) {
                    const auto& replay=pica.GetCpuVertexReplay();
                    std::vector<SurfaceEffects::Vertex> inputs(replay.Inputs().size());
                    for(unsigned i=0;i<inputs.size();++i)for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
                        inputs[i][a][c]=replay.Inputs()[i][a][c].ToFloat32();
                    oot_surface_effects.Context(generation.load(),sample.scene);
                    if(const auto half=ReplayEffectStrip<SurfaceEffects::Vertex>(inputs,replay.Order(),[&](const auto& vertices,const auto& order) {
                        ++SurfaceEffects::replay_counts[effect_kind].canonical;
                        WarpSparkles::Color constant;
                        for(unsigned c=0;c<4;++c)constant[c]=float((regs.texturing.tev_stage1.const_color>>(8*c))&255)/255.f;
                        return oot_surface_effects.Apply(effect_kind,frame,regs.texturing.texture0.GetPhysicalAddress(),vertices,order,constant);
                    })) {
                        if(*half!=inputs)++SurfaceEffects::replay_counts[effect_kind].changed;
                        effect_inputs=replay.Inputs();
                        for(unsigned i=0;i<inputs.size();++i)for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
                            effect_inputs[i][a][c]=Pica::f24::FromFloat32((*half)[i][a][c]);
                        if(effect_kind==6) {
                            oot_sparkle_color=oot_surface_effects.SparkleColor();
                            if(oot_sparkle_color)fs_data_dirty=true;
                        }
                        if(effect_kind>=8 && !oot_surface_effects.OldSpriteInputs().empty()) {
                            const auto prior=ReplayEffectStrip<SurfaceEffects::Vertex>(inputs,replay.Order(),[&](const auto&,const auto&) {
                                return std::optional{oot_surface_effects.OldSpriteInputs()};
                            });
                            if(prior) {
                                old_sprite_inputs=replay.Inputs();
                                for(unsigned i=0;i<inputs.size();++i)for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
                                    old_sprite_inputs[i][a][c]=Pica::f24::FromFloat32((*prior)[i][a][c]);
                            }
                        }
                    }
                }
                if(oot_replay_projection)for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                    uniforms.f[r][c]=Pica::f24::FromFloat32(float((*oot_replay_projection)[r][c]));
                if(oot_song_slide)for(unsigned r=0;r<3;++r)
                    uniforms.f[4+r][3]=Pica::f24::FromFloat32(
                        uniforms.f[4+r][3].ToFloat32()+(*oot_song_slide)[r]);
                if(oot_pose_override)for(unsigned i=0;i<oot_pose_override->count;++i)
                    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                        uniforms.f[20+i*3+r][c]=Pica::f24::FromFloat32(float(oot_pose_override->palette[i][r][c]));
                std::vector<HardwareVertex> vertices;
                const bool replayed=pica.ReplayCpuVertices(uniforms,[&](const auto& a,const auto& b,const auto& c) {
                    AppendTriangle(vertices,a,b,c);
                },effect_inputs);
                if(!replayed || vertices.size()!=vertex_batch.size())oot_replay_frame.Fail(ReplayFailure::Cpu);
                else {
                    if(old_effect_texture_set) {
                        const auto previous=ModelEffects::TextureCrossfade(vertices,sample_fraction);
                        splash_old_cpu_draw=upload_cpu_draw(previous);
                    }
                    midpoint_cpu_draw=upload_cpu_draw(vertices);use_midpoint_cpu_draw=true;
                }
                if(!old_sprite_inputs.empty()) {
                    auto current=std::move(vertices);
                    vertices.clear();
                    if(pica.ReplayCpuVertices(uniforms,[&](const auto& a,const auto& b,const auto& c) {
                        AppendTriangle(vertices,a,b,c);
                    },old_sprite_inputs) && vertices.size()==vertex_batch.size()) {
                        if(effect_kind==14 && current.size()==vertices.size()) {
                            // Combine cells before native alpha blending. Two
                            // translucent passes would darken overlapping puffs.
                            for(unsigned i=0;i<current.size();++i)
                                current[i].tex_coord1=vertices[i].color.w>0?vertices[i].tex_coord0:current[i].tex_coord0;
                            midpoint_cpu_draw=upload_cpu_draw(current);
                            fs_data.sprite_atlas_blend.x=sample_fraction;fs_data_dirty=true;
                        } else splash_old_cpu_draw=upload_cpu_draw(vertices);
                    }
                    else oot_replay_frame.Fail(ReplayFailure::Cpu);
                }
            }
        }
        if(!accelerate && !perspective && !oot_hud_midpoint.empty()) {
            auto vertices=oot_hud_midpoint;
            if(vertices.size()==vertex_batch.size())for(unsigned i=0;i<vertices.size();++i)
                for(unsigned c=0;c<4;++c) {
                    vertices[i].position[c]=vertex_batch[i].position[c]+
                        (vertices[i].position[c]-vertex_batch[i].position[c])*2*(1-sample_fraction);
                    vertices[i].color[c]=std::clamp(vertex_batch[i].color[c]+
                        (vertices[i].color[c]-vertex_batch[i].color[c])*2*(1-sample_fraction),0.f,1.f);
                }
            midpoint_cpu_draw=upload_cpu_draw(vertices);use_midpoint_cpu_draw=true;
        }
        if(!accelerate && !perspective && OOT3D::CircleWipe::Candidate(regs,
           pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash())) {
            OOT3D::CircleWipe::Projection lens{};
            for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
                lens[row][col]=pica.vs_setup.uniforms.f[row][col].ToFloat32();
            auto vertices=vertex_batch;
            const bool paired=oot_circle_wipe_history.Apply(vertices,frame,
               Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count(),generation.load(),sample.scene,
               regs.texturing.texture0.GetPhysicalAddress(),address,lens);
            if(OOT3D::Interpolation::SurfaceCapture::active.load())
                LOG_INFO(Render_Vulkan,"OoT circle wipe: frame={} vertices={} paired={} cpu_upload={} world={} failed={} failure={}",
                    frame,vertices.size(),paired,cpu_interpolation,oot_replay_frame.world,
                    oot_replay_frame.failed,ReplayFailureNames[unsigned(last_failure.load())]);
            if(paired) {
                midpoint_cpu_draw=upload_cpu_draw(vertices);use_midpoint_cpu_draw=true;
            }
        }
        if(!accelerate && !perspective && model_shader &&
           OOT3D::TextboxInterpolation::Active(frame,generation.load())) {
            namespace T=OOT3D::TextboxInterpolation;
            if(const auto mesh=OotUIMeshHash(T::MaxVertices,true)) {
                T::Matrix matrix{};
                std::array<std::array<float,4>,4> projection_values{};
                for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                    projection_values[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                    matrix[r][c]=pica.vs_setup.uniforms.f[T::ModelViewRow+r][c].ToFloat32();
                if(const auto route=T::Find(mesh->first,mesh->second,matrix,frame,generation.load())) {
                    auto vertices=vertex_batch;
                    const bool interpolated=oot_textbox_history.Apply(vertices,*route,frame,
                       Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count(),
                       regs.texturing.texture0.GetPhysicalAddress(),address,projection_values,
                       regs.framebuffer.output_merger.depth_test_enable==0 &&
                       regs.framebuffer.output_merger.depth_write_enable==0);
                    if(SurfaceCapture::active.load()) {
                        SurfaceCapture::TextboxSample trace;
                        trace.frame=frame;trace.model=route->source[0];trace.mesh=route->source[1];
                        trace.pane=route->source[3];trace.generation=route->generation;
                        trace.stage=interpolated?"midpoint_applied":"history_rejected";
                        trace.state=oot_textbox_history.rejection;
                        SurfaceCapture::ObserveTextbox(std::move(trace));
                    }
                    if(interpolated) {
                        midpoint_cpu_draw=upload_cpu_draw(vertices);use_midpoint_cpu_draw=true;
                    }
                }
            }
        }
        // Names enter each interpolated scene immediately before its HUD, using
        // the same corrected world projection and the same depth attachment.
        if(slot.names.frame!=frame){slot.names={};slot.names.frame=frame;}
        if(name_session && perspective && oot_replay_projection && using_depth_fb && write_depth_fb &&
           !slot.names.drawn && oot_name_scene.color==address) {
            slot.names=oot_name_scene;slot.names.drawn=false;
            slot.names.scene.projection=*oot_replay_projection;
        }
        RemoteNameRenderer::Draw name_draw;
        if(oot_name_hud && !slot.names.drawn && slot.names.color==address &&
           slot.names.scene.camera.epoch==generation.load() && oot_name_renderer && !oot_replay_frame.failed) {
            slot.names.drawn=true;
            name_draw=oot_name_renderer->Prepare(stream_buffer,slot.names.scene,sample_fraction,
                VideoCore::PixelFormat::RGBA8,VideoCore::PixelFormat::D24S8,slot.names.compare);
        }
        if(!oot_replay_frame.failed && sample_fraction<1.f) {
            const auto saved=pipeline_info;
            pica.vs_setup.uniforms_dirty=true;UploadUniforms(accelerate);
            using CF=Pica::FramebufferRegs::CompareFunc;
            const auto compare=regs.framebuffer.output_merger.depth_test_func.Value();
            const bool reverse=compare==CF::GreaterThan || compare==CF::GreaterThanOrEqual;
            const unsigned width=std::max<unsigned>(draw_rect.right,viewport.width);
            const unsigned height=std::max<unsigned>(draw_rect.top,viewport.height);
            if(layer.Visible() && (layer.Width()!=width || layer.Height()!=height))oot_replay_frame.Fail(ReplayFailure::Layer);
            else if(layer.Begin(width,height,reverse?0.f:1.f)) {
                pipeline_info.state.attachments.color=VideoCore::PixelFormat::RGBA8;
                pipeline_info.state.attachments.depth=VideoCore::PixelFormat::D24S8;
                pipeline_info.dynamic_info.viewport={viewport.x,viewport.y,viewport.x+viewport.width,viewport.y+viewport.height};
                pipeline_info.dynamic_info.scissor=draw_rect;
                if(name_draw.count)oot_name_renderer->Render(name_draw,stream_buffer.Handle(),slot.names.viewport,slot.names.scissor);
                if(!draw_batch())oot_replay_frame.Fail(ReplayFailure::Draw);
                else {++replayed_draws;if(oot_pose_override)++Pose::blended_draws;}
                if(splash_old_cpu_draw.count) {
                    const auto current=midpoint_cpu_draw;midpoint_cpu_draw=splash_old_cpu_draw;
                    if(old_effect_texture_set)pipeline_cache.Select(DescriptorHeapType::Texture,old_effect_texture_set);
                    if(!draw_batch())oot_replay_frame.Fail(ReplayFailure::Draw);
                    if(old_effect_texture_set)pipeline_cache.Select(DescriptorHeapType::Texture,oot_native_texture_set);
                    midpoint_cpu_draw=current;
                }
                layer.x=viewport.x;layer.y=viewport.y;
                layer.viewport_width=viewport.width;layer.viewport_height=viewport.height;
                layer.End(false);
            } else oot_replay_frame.Fail(ReplayFailure::Layer);
            pipeline_info=saved;
        }
        if(slot_index==0 && ActorCapture::SampleFrame(frame)) {
            ActorCapture::Draw capture;
            capture.frame=frame;capture.time=Core::System::GetInstance().CoreTiming().GetGlobalTimeUs().count();
            capture.shader=pica.vs_setup.GetProgramCodeHash();capture.swizzle=pica.vs_setup.GetSwizzleDataHash();
            capture.vertices=regs.pipeline.num_vertices;capture.cpu=!accelerate;
            capture.vertex_base=regs.pipeline.vertex_attributes.GetPhysicalBaseAddress();
            capture.indexed=is_indexed;capture.index16=is_indexed && regs.pipeline.index_array.format!=0;
            capture.index_offset=is_indexed?u32(regs.pipeline.index_array.offset):0;
            capture.texture=regs.texturing.texture0.GetPhysicalAddress();
            capture.topology=unsigned(regs.pipeline.triangle_topology.Value());
            capture.perspective=perspective;capture.camera_midpoint=bool(oot_replay_projection);
            capture.pose_midpoint=bool(oot_pose_override);capture.frame_failed=oot_replay_frame.failed;
            capture.failure=oot_replay_frame.failed?last_failure.load():oot_replay_camera.failure;
            capture.camera=sample;capture.draw_camera=camera;capture.projection=projection;
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                capture.model_view[r][c]=pica.vs_setup.uniforms.f[4+r][c].ToFloat32();
            for(unsigned i=0;i<10;++i) {
                capture.palette[i]=Math::Identity();
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                    capture.palette[i][r][c]=pica.vs_setup.uniforms.f[20+i*3+r][c].ToFloat32();
            }
            if(accelerate) {
                capture.first_vertex=vertex_info.vs_input_index_min;capture.last_vertex=vertex_info.vs_input_index_max;
                if(capture.vertices && capture.last_vertex>=capture.first_vertex &&
                   !regs.pipeline.vertex_attributes.IsDefaultAttribute(0)) {
                    capture.position_count=unsigned(std::min<std::uint64_t>(16,
                        std::uint64_t(capture.last_vertex)-capture.first_vertex+1));
                    Pica::VertexLoader loader(memory,regs.pipeline);
                    Pica::AttributeBuffer input{},defaults{};
                    for(unsigned i=0;i<capture.position_count;++i) {
                        const unsigned index=capture.first_vertex+i;
                        loader.LoadVertex(capture.vertex_base,index,index,input,defaults);
                        for(unsigned c=0;c<4;++c)capture.positions[i][c]=input[0][c].ToFloat32();
                    }
                }
            } else {
                capture.position_count=unsigned(std::min<std::size_t>(16,vertex_batch.size()));
                for(unsigned i=0;i<capture.position_count;++i)for(unsigned c=0;c<4;++c)
                    capture.positions[i][c]=vertex_batch[i].position[c];
            }
            ActorCapture::Observe(std::move(capture));
        }
        static FirstPersonDiagnostics::Gate aim_draw,aim_failure;
        if((model_shader && aim_draw.Take(frame,2)) || (oot_replay_frame.failed && aim_failure.Take(frame)))
            LOG_INFO(Render_Vulkan,"OoT aim draw: session={} frame={} cpu={} shader={:016x} swizzle={:016x} vertices={} model={} perspective={} camera_midpoint={} pose_midpoint={} world={} failed={} failure={} target={:08x} viewport={},{},{},{} camera=[{}] projection=[{}]",
                FirstPersonDiagnostics::capture.load(),frame,!accelerate,pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash(),
                unsigned(regs.pipeline.num_vertices),model_shader,perspective,bool(oot_replay_projection),bool(oot_pose_override),
                oot_replay_frame.world,oot_replay_frame.failed,ReplayFailureNames[unsigned(last_failure.load())],address,
                viewport.x,viewport.y,viewport.width,viewport.height,FirstPersonDiagnostics::MatrixText(camera),FirstPersonDiagnostics::MatrixText(projection));
        if(oot_song_visual){oot_song_visual.reset();fs_data_dirty=true;}
        if(oot_model_effect){oot_model_effect.reset();fs_data_dirty=true;}
        if(oot_sparkle_color){oot_sparkle_color.reset();fs_data_dirty=true;}
        if(fs_data.sprite_atlas_blend.x!=0){fs_data.sprite_atlas_blend.x=0;fs_data_dirty=true;}
        oot_replay_projection.reset();oot_pose_override.reset();oot_song_slide.reset();oot_waterfall_uv.reset();oot_fairy_glow.reset();use_midpoint_cpu_draw=false;splash_old_cpu_draw={};
        pica.vs_setup.uniforms_dirty=true;UploadUniforms(accelerate);
        } // Replay samples of this native draw.
        sample_fraction=.5f;
    }
#endif

#ifdef ENABLE_MM3D_PRESENTATION
    const bool midpoint_title_logo=[&] {
        if(!MM3D::CameraMidpoint::enabled.load() ||
           pica.vs_setup.GetProgramCodeHash()!=0x89b1ab98da3d9241ULL ||
           pica.vs_setup.GetSwizzleDataHash()!=0x6f0b398b1fc186a9ULL)return false;
        auto camera=MM3D::CameraMidpoint::Identity();
        MM3D::CameraMidpoint::Matrix projection{};
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
            projection[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
            camera[r][c]=pica.vs_setup.uniforms.f[70+r][c].ToFloat32();
        return MM3D::CameraMidpoint::TitleLogoCamera(camera,projection);
    }();
    // Immediate replay retains the original draw's geometry/textures. Only this
    // private diagnostic target gets a different camera; the guest target stays native.
    // Depth-disabled/always-pass perspective draws include sky and translucent
    // effects. Keep their native depth/blending state and original draw order;
    // perspective + verified shader identify this path, not depth testing alone.
    std::optional<MM3D::EffectCapture::MapReplay> map_trace;
    if(MM3D::EffectCapture::Recording(MM3D::guest_frames.load()) &&
       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL) {
        MM3D::CameraMidpoint::Matrix p{};
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)p[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        if(MM3D::WorldMapInterpolation::Projection(p)) {
            map_trace.emplace();auto& t=*map_trace;
            t.frame=MM3D::guest_frames.load();t.texture=regs.texturing.texture0.GetPhysicalAddress();
            t.vertices=regs.pipeline.num_vertices;
            t.owner=MM3D::InventoryInterpolation::Owner(t.frame,MM3D::LinkPoseCapture::interpolation_generation.load(),true);
            t.flags=(pica.CanReplayCpuVertices()?4u:0u)|(cpu_interpolation?32u:0u)|
                (use_pause?64u:0u)|(succeeded?128u:0u);
            t.gates=(MM3D::CameraMidpoint::enabled.load()?1u:0u)|(replay_world?2u:0u)|
                (MM3D::Widescreen::supported_layout.load()?4u:0u)|(!shadow_rendering&&using_color_fb?8u:0u)|
                (MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight())?16u:0u)|
                ((!pipeline_info.state.depth_stencil.stencil_test_enable||midpoint_stencil_frame==t.frame)?32u:0u)|
                (!guest_regions.info[0]?64u:0u)|
                (viewport.x==0&&viewport.y==0&&viewport.width>0&&viewport.height>0?128u:0u);
        }
    }
    if(MM3D::CameraMidpoint::enabled.load() && !midpoint_title_logo && replay_world && succeeded &&
       MM3D::Widescreen::supported_layout.load() && !shadow_rendering && using_color_fb &&
       MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) &&
       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL &&
       (!pipeline_info.state.depth_stencil.stencil_test_enable ||
        midpoint_stencil_frame==MM3D::guest_frames.load()) && !guest_regions.info[0] &&
       !use_pause && viewport.x==0 && viewport.y==0 && viewport.width>0 && viewport.height>0) {
        MM3D::InterpolationPerformance::Scope performance_replay{MM3D::InterpolationPerformance::Part::Replay};
        using namespace MM3D::CameraMidpoint;
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto cpu=MM3D::CameraLive::GetSnapshot();
        Matrix camera=Identity(),projection{};
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)
            projection[i][j]=pica.vs_setup.uniforms.f[i][j].ToFloat32();
        for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)
            camera[i][j]=pica.vs_setup.uniforms.f[70+i][j].ToFloat32();
        bool matches=cpu.valid && now>=cpu.sampled_at_ms && now-cpu.sampled_at_ms<=100;
        double direction_length=0,direction_dot=0;
        for(unsigned i=0;i<3;++i) {
            matches &= std::abs(camera[i][3]-cpu.eye[i])<1.;
            const double direction=cpu.target[i]-cpu.eye[i];
            direction_length+=direction*direction;direction_dot-=direction*camera[i][2];
        }
        matches &= direction_length>1e-6 && direction_dot/std::sqrt(std::max(direction_length,1e-6))>.9999;
        const bool valid_transform=Rigid(camera) && WorldProjection(projection);
        const auto epoch=generation.load();
        if(epoch!=midpoint_generation || midpoint_width!=unsigned(viewport.width) || midpoint_height!=unsigned(viewport.height)) {
            midpoint_scene_clear.Reset();
            midpoint_history.Reset();world_map_history.Reset();glare_cpu_history.Reset();forest_particle_history.Reset();snow_history.Reset();fire_impact_history.Reset();leaf_particle_history.Reset();moth_history.Reset();message_history.Reset();message_override.reset();item_slide_history.Reset();item_slide_override.reset();file_select_history.Reset();file_select_override.reset();inventory_history.Reset();banner_history.Reset();magic_meter_history.Reset();heart_history.Reset();lens_history.Reset();letterbox_history.Reset();inventory_override.reset();torch_history.Reset();water_particle_history.Reset();water_model_history.Reset();moving_effect_history.Reset();splash_cpu_history.Reset();burst_particle_history.Reset();spark_history.Reset();firework_history.Reset();chest_particle_history.Reset();sword_trail_history.Reset();shockwave_history.Reset();bubble_history.Reset();midpoint_target_frame.reset();midpoint_world_frame.reset();midpoint_opaque_world_frame.reset();midpoint_stencil_frame.reset();midpoint_generation=epoch;
            link_history.Reset();song_history.Reset();song_overrides.reset();link_frame.reset();link_overrides={};
            midpoint_width=unsigned(viewport.width);midpoint_height=unsigned(viewport.height);
        }
        using CF=Pica::FramebufferRegs::CompareFunc;
        const auto compare=regs.framebuffer.output_merger.depth_test_func.Value();
        const bool valid_depth=!regs.framebuffer.output_merger.depth_test_enable ||
            compare==CF::Always || compare==CF::LessThan || compare==CF::LessThanOrEqual ||
            compare==CF::GreaterThan || compare==CF::GreaterThanOrEqual;
        if(valid_transform && valid_depth) {
            if(map_trace)map_trace->gates|=256;
            // Sample both accepted and rejected cameras together. Title/attract
            // rendering can mix cameras; rejection counters alone cannot tell
            // an auxiliary draw from a stale CPU snapshot. Bound trace volume.
            if(MM3D::InputLatency::Active()) {
                static std::uint64_t trace_frame=~std::uint64_t{};
                static unsigned trace_draws[2]{};
                const auto frame=MM3D::guest_frames.load();
                if(trace_frame!=frame) { trace_frame=frame;trace_draws[0]=trace_draws[1]=0; }
                if(frame%30<2 && trace_draws[matches?1:0]++<24) {
                    LOG_INFO(Render_Vulkan,
                        "Camera match trace frame={} vertices={} matches={} cpu_valid={} age={} base={:08x} eye={},{},{} target={},{},{} camera={},{},{},{};{},{},{},{};{},{},{},{} lens={},{},{},{}",
                        frame,regs.pipeline.num_vertices,matches,cpu.valid,now-cpu.sampled_at_ms,cpu.base,
                        cpu.eye[0],cpu.eye[1],cpu.eye[2],cpu.target[0],cpu.target[1],cpu.target[2],
                        camera[0][0],camera[0][1],camera[0][2],camera[0][3],
                        camera[1][0],camera[1][1],camera[1][2],camera[1][3],
                        camera[2][0],camera[2][1],camera[2][2],camera[2][3],
                        projection[0][0],projection[1][1],projection[2][2],projection[2][3]);
                }
            }
            bool fallback=false;
            const auto map_owner=MM3D::InventoryInterpolation::Owner(MM3D::guest_frames.load(),
                MM3D::LinkPoseCapture::interpolation_generation.load(),true);
            auto map_model_view=MM3D::CameraMidpoint::Identity();
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                map_model_view[r][c]=pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
            const bool map_camera=world_map_history.Recognize(MM3D::guest_frames.load(),
                map_owner,epoch,camera,projection,map_model_view);
            const auto map_half=world_map_history.Apply(MM3D::guest_frames.load(),now,
                map_owner,epoch,camera,projection,map_model_view,native_preview.load());
            // Map geometry already contains its current view transform. The
            // private projection applies the halfway camera/lens to every map
            // material; its shared skeleton must not interpolate it a second time.
            Matrix selected;
            if(map_camera) {
                fallback=!map_half;
                selected=map_half.value_or(projection);
                if(map_trace) {
                    map_trace->flags|=1u|(map_half?2u:0u);
                    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                        map_trace->uniform_delta=std::max(map_trace->uniform_delta,float(std::abs(selected[r][c]-projection[r][c])));
                }
            } else {
                selected=midpoint_history.Preview(MM3D::guest_frames.load(),now,
                    camera,projection,matches,native_preview.load(),fallback);
            }
            {
                auto& layer=prompt_layers["@camera_midpoint"];
                if(!layer) {
                    layer=std::make_unique<MM3D::PromptLayer>(instance,scheduler,renderpass_cache,true);
                    layer->profile.id="@camera_midpoint";layer->profile.source_top=true;
                    layer->profile.scale=.42f;layer->profile.x=.26f;layer->profile.y=-.26f;
                }
                const auto frame=MM3D::guest_frames.load();
                if(midpoint_target_frame!=frame) {
                    layer->NextFrame(midpoint_background());
                    midpoint_target_frame=frame;
                    midpoint_frame_interpolated=false;
                    if(midpoint_fade_frame!=frame) {
                        if(const auto fade=prompt_layers.find("@camera_midpoint_fade");fade!=prompt_layers.end())
                            fade->second->NextFrame();
                    }
                }
                const auto saved=pipeline_info;
                midpoint_projection=selected;
                if(MM3D::LinkPoseCapture::Enabled() && !map_camera) {
                    const auto link_epoch=MM3D::LinkPoseCapture::interpolation_generation.load();
                    if(link_epoch!=link_generation) {
                        link_history.Reset();song_history.Reset();leaf_particle_history.Reset();moth_history.Reset();message_history.Reset();message_override.reset();item_slide_history.Reset();item_slide_override.reset();file_select_history.Reset();file_select_override.reset();inventory_history.Reset();banner_history.Reset();magic_meter_history.Reset();heart_history.Reset();lens_history.Reset();letterbox_history.Reset();inventory_override.reset();torch_history.Reset();water_particle_history.Reset();water_model_history.Reset();moving_effect_history.Reset();splash_cpu_history.Reset();burst_particle_history.Reset();spark_history.Reset();firework_history.Reset();chest_particle_history.Reset();sword_trail_history.Reset();shockwave_history.Reset();bubble_history.Reset();song_overrides.reset();link_frame.reset();link_generation=link_epoch;
                    }
                    if(link_frame!=frame) {
                        link_poses=MM3D::LinkPoseCapture::ForFrame(frame);link_frame=frame;
                    }
                    MM3D::LinkInterpolation::Palette palette;
                    for(unsigned bone=0;bone<10;++bone) {
                        palette[bone]=Identity();
                        for(unsigned i=0;i<12;++i)
                            palette[bone][i/4][i%4]=pica.vs_setup.uniforms.f[20+bone*3+i/4][i%4].ToFloat32();
                    }
                    // Sample consecutive frames of the verified song effect during
                    // an explicit timing trace, before midpoint uniforms are applied.
                    if(MM3D::InputLatency::Active() && frame%10<3) {
                        bool song_effect=false;
                        for(const auto& pose:link_poses) {
                            if(pose.signature!=0x04e3fb346228fd20ull ||
                               pose.pose.bones.size()!=4 || pose.bind.size()!=4)continue;
                            std::vector<MM3D::LinkPoseMath::Matrix> bones;
                            for(const auto& bone:pose.pose.bones)
                                bones.push_back(MM3D::LinkPoseMath::Expand(bone));
                            if(MM3D::LinkInterpolation::ResolvePalette(palette[0],pose,bones,Identity()))
                                song_effect=true;
                        }
                        if(song_effect) {
                            const auto& blend=pipeline_info.state.blending;
                            LOG_INFO(Render_Vulkan,
                                "Song effect draw frame={} vertices={} shader={:016x} swizzle={:016x} textures={:08x},{:08x},{:08x} blend={} src={} dst={} equation={}",
                                frame,regs.pipeline.num_vertices,pica.vs_setup.GetProgramCodeHash(),
                                pica.vs_setup.GetSwizzleDataHash(),regs.texturing.texture0.GetPhysicalAddress(),
                                regs.texturing.texture1.GetPhysicalAddress(),regs.texturing.texture2.GetPhysicalAddress(),
                                unsigned(blend.blend_enable),unsigned(blend.src_color_blend_factor.Value()),
                                unsigned(blend.dst_color_blend_factor.Value()),unsigned(blend.color_blend_eq.Value()));
                            const auto& uniforms=pica.vs_setup.uniforms.f;
                            for(unsigned index=0;index<uniforms.size();++index) {
                                const auto& v=uniforms[index];
                                LOG_INFO(Render_Vulkan,"Song effect uniform frame={} vertices={} index={} value={},{},{},{}",
                                    frame,regs.pipeline.num_vertices,index,v.x.ToFloat32(),v.y.ToFloat32(),
                                    v.z.ToFloat32(),v.w.ToFloat32());
                            }
                        }
                    }
                    song_overrides.reset();
                    if(pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
                       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL) {
                        MM3D::SongEffectInterpolation::Values values{};
                        for(unsigned r=0;r<MM3D::SongEffectInterpolation::Rows.size();++r)for(unsigned c=0;c<4;++c)
                            values[r][c]=pica.vs_setup.uniforms.f[MM3D::SongEffectInterpolation::Rows[r]][c].ToFloat32();
                        const auto& blend=pipeline_info.state.blending;
                        using BF=Pica::FramebufferRegs::BlendFactor;
                        const bool portal=MM3D::SongEffectInterpolation::BossEntrance(regs.pipeline.num_vertices,values);
                        const auto& textures=regs.texturing.main_config;
                        const unsigned texture_mask=unsigned(textures.texture0_enable) |
                            (unsigned(textures.texture1_enable)<<1) | (unsigned(textures.texture2_enable)<<2);
                        const auto* waterfall=MM3D::SongEffectInterpolation::WoodfallWater(regs.pipeline.num_vertices,values,texture_mask);
                        const bool epona=MM3D::SongEffectInterpolation::EponaSingleTexture(regs.pipeline.num_vertices,values,texture_mask).has_value();
                        const auto* moon=MM3D::SongEffectInterpolation::MoonEffect(regs.pipeline.num_vertices,values,texture_mask);
                        namespace Bay=MM3D::GreatBayWaterInterpolation;
                        const auto bay=Bay::Identify(regs.pipeline.num_vertices,texture_mask,values);
                        const auto coastal=MM3D::CoastalWaterInterpolation::Identify(regs.pipeline.num_vertices,texture_mask,values);
                        bool bay_material=false,coastal_material=false;
                        if(bay || coastal) {
                            Bay::Tev tev{};const auto stages=regs.texturing.GetTevStages();
                            for(unsigned i=0;i<6;++i)tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,
                                stages[i].ops_raw,stages[i].const_color,stages[i].scales_raw};
                            if(bay)bay_material=Bay::MatchesMaterial(*bay,unsigned(blend.src_color_blend_factor.Value()),
                                unsigned(blend.dst_color_blend_factor.Value()),
                                pipeline_info.state.depth_stencil.depth_write_enable,tev);
                            if(coastal)coastal_material=MM3D::CoastalWaterInterpolation::MatchesMaterial(*coastal,
                                unsigned(blend.src_color_blend_factor.Value()),unsigned(blend.dst_color_blend_factor.Value()),
                                pipeline_info.state.depth_stencil.depth_write_enable,tev);
                        }
                        if((coastal_material || bay_material || (MM3D::SongEffectInterpolation::Recognized(regs.pipeline.num_vertices,values,texture_mask) &&
                           (!moon || (!pipeline_info.state.depth_stencil.depth_write_enable &&
                            unsigned(blend.color_blend_eq.Value())==moon->equation &&
                            blend.dst_color_blend_factor==(moon->additive?BF::One:BF::OneMinusSourceAlpha))) &&
                           (!epona || (!pipeline_info.state.depth_stencil.depth_write_enable && blend.dst_color_blend_factor==BF::One)) &&
                           (!waterfall || (texture_mask==waterfall->texture_mask &&
                            !pipeline_info.state.depth_stencil.depth_write_enable &&
                            blend.dst_color_blend_factor==(waterfall->additive?BF::One:BF::OneMinusSourceAlpha))) &&
                           (!portal || (textures.texture0_enable && textures.texture1_enable && textures.texture2_enable &&
                            !pipeline_info.state.depth_stencil.depth_write_enable && blend.dst_color_blend_factor==BF::One)) &&
                           blend.src_color_blend_factor==BF::SourceAlpha &&
                           (blend.dst_color_blend_factor==BF::One || blend.dst_color_blend_factor==BF::OneMinusSourceAlpha))) &&
                           blend.blend_enable &&
                           (blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add || (moon && moon->equation==2)) &&
                           pipeline_info.state.depth_stencil.depth_test_enable &&
                           !pipeline_info.state.depth_stencil.stencil_test_enable && blend.color_write_mask==15) {
                            song_overrides=song_history.Apply(frame,{
                                regs.pipeline.vertex_attributes.GetPhysicalBaseAddress(),
                                regs.texturing.texture0.GetPhysicalAddress(),(epona || (coastal_material && !(texture_mask&2)))?0u:regs.texturing.texture1.GetPhysicalAddress(),
                                (epona || (coastal_material && !(texture_mask&4)))?0u:regs.texturing.texture2.GetPhysicalAddress(),unsigned(blend.dst_color_blend_factor.Value())|(texture_mask<<8)|(unsigned(blend.color_blend_eq.Value())<<16)|(coastal_material?1u<<24:0u),
                                regs.pipeline.num_vertices,is_indexed?u32(regs.pipeline.index_array.offset):0},
                                values,!fallback && !native_preview.load(),texture_mask,coastal_material?coastal:std::nullopt);
                        }
                    }
                    {
                        MM3D::InterpolationPerformance::Scope performance_skeleton{MM3D::InterpolationPerformance::Part::Skeleton};
                        link_overrides=link_history.Apply(frame,now,camera,palette,link_poses,
                            !fallback && !native_preview.load(),regs.pipeline.num_vertices);
                    }
                    if(MM3D::InputLatency::Active() && frame%10<3 &&
                       (regs.pipeline.num_vertices==4 || regs.pipeline.num_vertices==18 ||
                        regs.pipeline.num_vertices==24)) {
                        for(const auto& pose:link_poses) {
                            if(pose.signature!=0x09e2df0cf6765f8eull)continue;
                            const bool glow=MM3D::LinkInterpolation::FairyGlow(palette[0],pose,regs.pipeline.num_vertices);
                            if(!glow && !MM3D::LinkInterpolation::Identify(palette[0],pose))continue;
                            const auto output=link_overrides[0].value_or(palette[0]);
                            LOG_INFO(Render_Vulkan,
                                "Fairy alignment frame={} instance={:08x} vertices={} glow={} cpu_match={} fallback={} override={} camera={},{},{} native={},{},{} output={},{},{}",
                                frame,pose.pose.instance,regs.pipeline.num_vertices,glow,matches,fallback,
                                bool(link_overrides[0]),camera[0][3],camera[1][3],camera[2][3],
                                palette[0][0][3],palette[0][1][3],palette[0][2][3],
                                output[0][3],output[1][3],output[2][3]);
                        }
                    }
                    if(MM3D::InputLatency::Active() && frame%10==0 &&
                       regs.pipeline.num_vertices==1284 &&
                       std::any_of(link_poses.begin(),link_poses.end(),[](const auto& pose) {
                           return pose.signature==0x79937db4f6cb5c95ull;
                       })) {
                        const auto scale=[](const auto& m) {
                            return std::sqrt(m[0][0]*m[0][0]+m[1][0]*m[1][0]+m[2][0]*m[2][0]);
                        };
                        for(const auto& pose:link_poses)if(pose.signature==0x79937db4f6cb5c95ull) {
                            for(unsigned bone=0;bone<pose.pose.bones.size();++bone)
                                LOG_INFO(Render_Vulkan,"Deku pipe skeleton frame={} instance={} bone={} scale={}",
                                    frame,pose.pose.instance,bone,scale(MM3D::LinkPoseMath::Expand(pose.pose.bones[bone])));
                        }
                        for(unsigned slot=0;slot<6;++slot)
                            LOG_INFO(Render_Vulkan,
                                "Deku pipe interpolation frame={} slot={} fallback={} comparison={} override={} scale={} midpoint_scale={}",
                                frame,slot,fallback,native_preview.load(),bool(link_overrides[slot]),
                                scale(palette[slot]),link_overrides[slot]?scale(*link_overrides[slot]):-1.0);
                    }
                }
                // Water impacts use independent model instances, not actor bones.
                // Recover their world transforms so camera movement is applied
                // once by the ordinary midpoint projection on both CPU and GPU.
                {
                    using BF=Pica::FramebufferRegs::BlendFactor;
                    const auto& blend=pipeline_info.state.blending;
                    const auto& depth=pipeline_info.state.depth_stencil;
                    const auto& tex=regs.texturing.main_config;
                    const unsigned count=regs.pipeline.num_vertices;
                    if((count==120 || count==192 || count==288) && !map_camera &&
                       blend.blend_enable && blend.src_color_blend_factor==BF::SourceAlpha &&
                       blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add &&
                       blend.color_write_mask==15 && depth.depth_test_enable &&
                       !depth.depth_write_enable && !depth.stencil_test_enable) {
                        namespace W=MM3D::WaterModelInterpolation;
                        W::Sample sample{};auto model=Identity();
                        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                            model[r][c]=pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
                        sample.model=MM3D::CameraMidpoint::Multiply(camera,model);
                        for(unsigned r=0;r<6;++r)for(unsigned c=0;c<4;++c)
                            sample.uv[r][c]=pica.vs_setup.uniforms.f[MM3D::SongEffectInterpolation::Rows[r]][c].ToFloat32();
                        const unsigned mask=unsigned(tex.texture0_enable)|(unsigned(tex.texture1_enable)<<1)|
                            (unsigned(tex.texture2_enable)<<2);
                        W::Tev tev{};const auto stages=regs.texturing.GetTevStages();
                        for(unsigned i=0;i<6;++i)tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,
                            stages[i].ops_raw,stages[i].const_color,stages[i].scales_raw};
                        if(const auto kind=W::Identify(count,mask,unsigned(blend.dst_color_blend_factor.Value()),sample.uv,tev)) {
                            const auto alpha_stage=W::Patterns[*kind].alpha_stage;
                            sample.alpha=float(tev[alpha_stage][3]>>24)/255.f;
                            const auto half=water_model_history.Apply(frame,
                                {regs.texturing.texture0.GetPhysicalAddress(),
                                 tex.texture1_enable?regs.texturing.texture1.GetPhysicalAddress():0u,
                                 tex.texture2_enable?regs.texturing.texture2.GetPhysicalAddress():0u,epoch,*kind},sample);
                            if(half && !fallback && !native_preview.load()) {
                                link_overrides[0]=MM3D::CameraMidpoint::Multiply(MM3D::CameraMidpoint::InverseRigid(camera),half->model);
                                song_overrides=half->uv;
                                water_model_alpha_override=std::pair{alpha_stage,half->alpha};fs_data_dirty=true;
                            }
                        }
                    }
                }
                // Moving flame and wake layers need per-instance histories;
                // sharing only texture addresses would mix nearby enemies.
                {
                    using BF=Pica::FramebufferRegs::BlendFactor;
                    const auto& blend=pipeline_info.state.blending;
                    const auto& depth=pipeline_info.state.depth_stencil;
                    const unsigned count=regs.pipeline.num_vertices;
                    if((count==12 || count==24 || count==144 || count==180 || count==324) && !map_camera &&
                       blend.blend_enable && blend.src_color_blend_factor==BF::SourceAlpha &&
                       blend.dst_color_blend_factor==BF::One &&
                       blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add &&
                       blend.color_write_mask==15 && depth.depth_test_enable &&
                       !depth.depth_write_enable && !depth.stencil_test_enable) {
                        namespace E=MM3D::MovingEffectInterpolation;
                        E::Sample sample{};auto model=Identity();
                        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                            model[r][c]=pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
                        sample.model=Multiply(camera,model);
                        for(unsigned r=0;r<6;++r)for(unsigned c=0;c<4;++c)
                            sample.uv[r][c]=pica.vs_setup.uniforms.f[MM3D::SongEffectInterpolation::Rows[r]][c].ToFloat32();
                        const auto& tex=regs.texturing.main_config;
                        const unsigned mask=unsigned(tex.texture0_enable)|(unsigned(tex.texture1_enable)<<1)|(unsigned(tex.texture2_enable)<<2);
                        E::Tev tev{};const auto stages=regs.texturing.GetTevStages();
                        for(unsigned i=0;i<6;++i)tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,
                            stages[i].ops_raw,stages[i].const_color,stages[i].scales_raw};
                        if(const auto kind=E::Identify(count,mask,sample.uv,tev)) {
                            if(E::AnimatedColors(*kind))for(unsigned stage=0;stage<6;++stage)for(unsigned c=0;c<4;++c)
                                sample.colors[stage][c]=fs_data.const_color[stage][c];
                            const auto half=moving_effect_history.Apply(frame,
                                {regs.texturing.texture0.GetPhysicalAddress(),regs.texturing.texture1.GetPhysicalAddress(),
                                 tex.texture2_enable?regs.texturing.texture2.GetPhysicalAddress():0u,epoch,*kind},sample);
                            if(half && !fallback && !native_preview.load()) {
                                link_overrides[0]=Multiply(InverseRigid(camera),half->model);
                                song_overrides=half->uv;
                                if(E::AnimatedColors(*kind)){moving_effect_colors=half->colors;fs_data_dirty=true;}
                            }
                        }
                    }
                }
                if (cpu_interpolation) {
                    MM3D::InterpolationPerformance::Scope cpu_replay_timing{
                        MM3D::InterpolationPerformance::Part::CpuVertexReplay};
                    // Override only verified particle input layouts. Work
                    // on a copy; the native draw and its replay cache stay intact.
                    std::vector<Pica::AttributeBuffer> particle_inputs, splash_old_inputs;
                    std::optional<Matrix> moth_override;
                    const auto& blend = pipeline_info.state.blending;
                    using BF = Pica::FramebufferRegs::BlendFactor;
                    const bool burst_flame = regs.vs.max_input_attribute_index == 4 &&
                        regs.vs.GetRegisterForAttribute(3) == 5 && regs.vs.GetRegisterForAttribute(4) == 6 &&
                        regs.texturing.main_config.texture1_enable && regs.texturing.main_config.texture2_enable &&
                        blend.dst_color_blend_factor == BF::One;
                    const bool burst_sprite = regs.vs.max_input_attribute_index == 2 &&
                        !regs.texturing.main_config.texture1_enable && !regs.texturing.main_config.texture2_enable &&
                        blend.dst_color_blend_factor == BF::OneMinusSourceAlpha;
                    if ((burst_flame || burst_sprite) &&
                        pica.vs_setup.GetProgramCodeHash() == 0x89b1ab98da3d9241ULL &&
                        pica.vs_setup.GetSwizzleDataHash() == 0x6f0b398b1fc186a9ULL &&
                        regs.pipeline.triangle_topology == Pica::PipelineRegs::TriangleTopology::Strip &&
                        regs.pipeline.num_vertices <= 768 && pica.GetCpuVertexReplay().Inputs().size() <= 513 &&
                        regs.vs.GetRegisterForAttribute(0) == 0 && regs.vs.GetRegisterForAttribute(1) == 3 &&
                        regs.vs.GetRegisterForAttribute(2) == 4 && regs.texturing.main_config.texture0_enable &&
                        blend.blend_enable && blend.src_color_blend_factor == BF::SourceAlpha &&
                        blend.color_blend_eq == Pica::FramebufferRegs::BlendEquation::Add && blend.color_write_mask == 15 &&
                        pipeline_info.state.depth_stencil.depth_test_enable &&
                        !pipeline_info.state.depth_stencil.depth_write_enable &&
                        !pipeline_info.state.depth_stencil.stencil_test_enable) {
                        const auto& replay = pica.GetCpuVertexReplay();
                        const unsigned attributes = burst_flame ? 5 : 3;
                        std::vector<MM3D::BurstParticleInterpolation::Vertex> inputs(replay.Inputs().size());
                        for (unsigned i = 0; i < inputs.size(); ++i)
                            for (unsigned a = 0; a < attributes; ++a)
                                for (unsigned c = 0; c < 4; ++c)
                                    inputs[i][a][c] = replay.Inputs()[i][a][c].ToFloat32();
                        std::array<std::array<u32,5>,6> burst_tev{};
                        const auto stages=regs.texturing.GetTevStages();
                        for(unsigned i=0;i<6;++i)burst_tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,
                            stages[i].ops_raw,stages[i].const_color,stages[i].scales_raw};
                        if (const auto quads = MM3D::BurstParticleInterpolation::Decode(inputs, replay.Order(), attributes,
                            MM3D::BurstParticleInterpolation::BombMaterial(burst_tev))) {
                            const auto half = burst_particle_history.Apply(frame, {
                                regs.texturing.texture0.GetPhysicalAddress(), regs.texturing.texture1.GetPhysicalAddress(),
                                regs.texturing.texture2.GetPhysicalAddress(), epoch, static_cast<u32>(quads->front().kind)}, *quads);
                            if (half && !fallback && !native_preview.load()) {
                                particle_inputs = replay.Inputs();
                                for (const auto& q : *half)
                                    for (unsigned v = 0; v < 4; ++v)
                                        for (unsigned a = 0; a < attributes; ++a)
                                            for (unsigned c = 0; c < 4; ++c)
                                                particle_inputs[q.indices[v]][a][c] = Pica::f24::FromFloat32(q.vertices[v][a][c]);
                            }
                        }
                    }
                    if(regs.pipeline.triangle_topology==Pica::PipelineRegs::TriangleTopology::Strip &&
                       regs.pipeline.num_vertices<=384 && pica.GetCpuVertexReplay().Inputs().size()<=257 &&
                       regs.vs.max_input_attribute_index==2 && regs.vs.GetRegisterForAttribute(0)==0 &&
                       regs.vs.GetRegisterForAttribute(1)==3 && regs.vs.GetRegisterForAttribute(2)==4 &&
                       pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
                       pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL &&
                       blend.blend_enable && blend.src_color_blend_factor==BF::SourceAlpha &&
                       blend.dst_color_blend_factor==BF::OneMinusSourceAlpha &&
                       blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add && blend.color_write_mask==15 &&
                       pipeline_info.state.depth_stencil.depth_test_enable &&
                       !pipeline_info.state.depth_stencil.depth_write_enable &&
                       !pipeline_info.state.depth_stencil.stencil_test_enable && regs.texturing.main_config.texture0_enable &&
                       !regs.texturing.main_config.texture1_enable && !regs.texturing.main_config.texture2_enable) {
                        MM3D::SnowInterpolation::Tev tev{};
                        const auto stages=regs.texturing.GetTevStages();
                        for(unsigned i=0;i<stages.size();++i)
                            tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,stages[i].ops_raw,
                                stages[i].const_color,stages[i].scales_raw};
                        if(MM3D::SnowInterpolation::Material(tev)) {
                            const auto& replay=pica.GetCpuVertexReplay();
                            std::vector<MM3D::SnowInterpolation::Vertex> inputs(replay.Inputs().size());
                            for(unsigned i=0;i<inputs.size();++i)for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
                                inputs[i][a][c]=replay.Inputs()[i][a][c].ToFloat32();
                            if(const auto quads=MM3D::SnowInterpolation::Decode(inputs,replay.Order())) {
                                const auto half=snow_history.Apply(frame,{regs.texturing.texture0.GetPhysicalAddress(),epoch},*quads);
                                if(half && !fallback && !native_preview.load()) {
                                    particle_inputs=replay.Inputs();
                                    for(const auto& q:*half)for(unsigned v=0;v<4;++v) {
                                        for(unsigned c=0;c<3;++c)
                                            particle_inputs[q.indices[v]][0][c]=Pica::f24::FromFloat32(q.vertices[v][0][c]);
                                        particle_inputs[q.indices[v]][1][3]=Pica::f24::FromFloat32(q.vertices[v][1][3]);
                                    }
                                }
                            }
                        }
                    }
                    if (regs.pipeline.triangle_topology == Pica::PipelineRegs::TriangleTopology::Strip &&
                        regs.pipeline.num_vertices <= 768 &&
                        pica.GetCpuVertexReplay().Inputs().size() <= 513 &&
                        regs.vs.max_input_attribute_index == 2 &&
                        regs.vs.GetRegisterForAttribute(0) == 0 &&
                        regs.vs.GetRegisterForAttribute(1) == 3 &&
                        regs.vs.GetRegisterForAttribute(2) == 4 &&
                        blend.blend_enable && blend.src_color_blend_factor == BF::SourceAlpha &&
                        blend.dst_color_blend_factor == BF::One &&
                        blend.color_blend_eq == Pica::FramebufferRegs::BlendEquation::Add &&
                        blend.color_write_mask == 15 && pipeline_info.state.depth_stencil.depth_test_enable &&
                        !pipeline_info.state.depth_stencil.stencil_test_enable) {
                        const auto& replay = pica.GetCpuVertexReplay();
                        std::vector<MM3D::ForestParticleInterpolation::Vertex> inputs(replay.Inputs().size());
                        for (unsigned i = 0; i < inputs.size(); ++i)
                            for (unsigned a = 0; a < 3; ++a)
                                for (unsigned c = 0; c < 4; ++c)
                                    inputs[i][a][c] = replay.Inputs()[i][a][c].ToFloat32();
                        std::optional<std::vector<MM3D::ForestParticleInterpolation::Quad>> half;
                        auto quads = MM3D::ForestParticleInterpolation::Decode(inputs, replay.Order());
                        if (!quads) quads = MM3D::ForestParticleInterpolation::Decode(inputs, replay.Order(), true);
                        if (quads) {
                            const auto& textures=regs.texturing.main_config;
                            const bool doubled_sparkle=MM3D::ForestParticleInterpolation::Kind(quads->front().vertices[0])>=3;
                            if(!doubled_sparkle || (textures.texture0_enable && !textures.texture1_enable &&
                               !textures.texture2_enable && !pipeline_info.state.depth_stencil.depth_write_enable))
                                half = forest_particle_history.Apply(frame,
                                    regs.texturing.texture0.GetPhysicalAddress(), epoch, *quads);
                        } else if (pica.vs_setup.GetProgramCodeHash() == 0x89b1ab98da3d9241ULL &&
                                   pica.vs_setup.GetSwizzleDataHash() == 0x6f0b398b1fc186a9ULL) {
                            const MM3D::WaterParticleInterpolation::Key key{
                                regs.texturing.texture0.GetPhysicalAddress(),
                                regs.texturing.texture1.GetPhysicalAddress(),
                                regs.texturing.texture2.GetPhysicalAddress(), epoch};
                            const auto& textures = regs.texturing.main_config;
                            const auto sparks = textures.texture0_enable && !textures.texture1_enable &&
                                !textures.texture2_enable && !pipeline_info.state.depth_stencil.depth_write_enable
                                ? MM3D::SparkInterpolation::Decode(inputs, replay.Order()) : std::nullopt;
                            const auto ribbon = textures.texture0_enable && !textures.texture1_enable &&
                                !textures.texture2_enable && !pipeline_info.state.depth_stencil.depth_write_enable
                                ? MM3D::SwordTrailInterpolation::Decode(inputs, replay.Order()) : std::nullopt;
                            const auto& tev0 = regs.texturing.tev_stage0;
                            const auto& tev1 = regs.texturing.tev_stage1;
                            const bool shockwave_material = tev0.modifiers_raw == 0 &&
                                ((tev0.sources_raw == 0x0ee00e30 && tev0.ops_raw == 0x10001 && tev0.scales_raw == 0) ||
                                 (tev0.sources_raw == 0x0ee00ee3 && tev0.ops_raw == 0 && tev0.scales_raw == 2 &&
                                  tev1.sources_raw == 0x0eee0efe && tev1.modifiers_raw == 0 &&
                                  tev1.ops_raw == 1 && tev1.scales_raw == 0));
                            const auto shockwaves = shockwave_material && textures.texture0_enable && !textures.texture1_enable &&
                                !textures.texture2_enable && !pipeline_info.state.depth_stencil.depth_write_enable
                                ? MM3D::ShockwaveInterpolation::Decode(inputs, replay.Order()) : std::nullopt;
                            const bool bubble_material = tev0.sources_raw == 0x0ee00ee3 &&
                                tev0.modifiers_raw == 0 && tev0.ops_raw == 0 && tev0.scales_raw == 2 &&
                                tev1.sources_raw == 0x0eee0efe && tev1.modifiers_raw == 0 &&
                                tev1.ops_raw == 1 && tev1.scales_raw == 0;
                            const auto bubbles = bubble_material && textures.texture0_enable && !textures.texture1_enable &&
                                !textures.texture2_enable && !pipeline_info.state.depth_stencil.depth_write_enable
                                ? MM3D::BubbleInterpolation::Decode(inputs, replay.Order()) : std::nullopt;
                            std::array<std::array<u32,5>,6> firework_tev{};
                            const auto stages=regs.texturing.GetTevStages();
                            for(unsigned i=0;i<stages.size();++i)
                                firework_tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,stages[i].ops_raw,
                                    stages[i].const_color,stages[i].scales_raw};
                            const auto fireworks=MM3D::FireworkInterpolation::Material(firework_tev)&&
                                textures.texture0_enable&&!textures.texture1_enable&&!textures.texture2_enable&&
                                !pipeline_info.state.depth_stencil.depth_write_enable
                                ?MM3D::FireworkInterpolation::Decode(inputs,replay.Order()):std::nullopt;
                            const auto chest_particles=MM3D::ChestParticleInterpolation::Material(firework_tev)&&
                                textures.texture0_enable&&!textures.texture1_enable&&!textures.texture2_enable&&
                                !pipeline_info.state.depth_stencil.depth_write_enable
                                ?MM3D::ChestParticleInterpolation::Decode(inputs,replay.Order()):std::nullopt;
                            const auto fire_impact_kind=MM3D::FireImpactInterpolation::Material(firework_tev);
                            const auto fire_impact=textures.texture0_enable&&!textures.texture1_enable&&
                                !textures.texture2_enable&&!pipeline_info.state.depth_stencil.depth_write_enable
                                ?MM3D::FireImpactInterpolation::Decode(inputs,replay.Order(),fire_impact_kind):std::nullopt;
                            if(fire_impact) {
                                half=fire_impact_history.Apply(frame,regs.texturing.texture0.GetPhysicalAddress(),
                                    epoch,fire_impact_kind,*fire_impact);
                            } else if(fireworks) {
                                half=firework_history.Apply(frame,
                                    {regs.texturing.texture0.GetPhysicalAddress(),epoch},*fireworks);
                            } else if (chest_particles) {
                                half=chest_particle_history.Apply(frame,
                                    {regs.texturing.texture0.GetPhysicalAddress(),epoch},*chest_particles);
                            } else if (sparks) {
                                half = spark_history.Apply(frame,
                                    {regs.texturing.texture0.GetPhysicalAddress(), epoch}, *sparks);
                            } else if (bubbles) {
                                half = bubble_history.Apply(frame,
                                    {regs.texturing.texture0.GetPhysicalAddress(), epoch}, *bubbles);
                            } else if (shockwaves) {
                                half = shockwave_history.Apply(frame,
                                    {regs.texturing.texture0.GetPhysicalAddress(), epoch}, *shockwaves);
                            } else if (ribbon) {
                                auto model_view = Identity();
                                for (unsigned r = 0; r < 3; ++r) for (unsigned c = 0; c < 4; ++c)
                                    model_view[r][c] = pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
                                const auto trail = sword_trail_history.Apply(frame,
                                    {regs.texturing.texture0.GetPhysicalAddress(), epoch}, *ribbon,
                                    MM3D::LinkPoseMath::Multiply(camera, model_view));
                                if (trail && !fallback && !native_preview.load() && !link_overrides[0]) {
                                    particle_inputs = replay.Inputs();
                                    for (unsigned i = 0; i < trail->indices.size(); ++i) {
                                        const auto index = trail->indices[i];
                                        for (unsigned c = 0; c < 3; ++c)
                                            particle_inputs[index][0][c] = Pica::f24::FromFloat32(trail->vertices[i][0][c]);
                                        particle_inputs[index][1][3] = Pica::f24::FromFloat32(trail->vertices[i][1][3]);
                                    }
                                }
                            } else if (const auto drops = MM3D::WaterParticleInterpolation::DecodeDrops(inputs, replay.Order())) {
                                half = water_particle_history.ApplyDrops(frame, key, *drops);
                            } else if (const auto rings = MM3D::WaterParticleInterpolation::DecodeRipples(inputs, replay.Order())) {
                                const auto mesh = water_particle_history.ApplyRipples(frame, key, *rings);
                                if (mesh && !fallback && !native_preview.load()) {
                                    particle_inputs = replay.Inputs();
                                    for (unsigned v = 0; v < rings->indices.size(); ++v) {
                                        const auto index = rings->indices[v];
                                        for (unsigned c = 0; c < 3; ++c)
                                            particle_inputs[index][0][c] = Pica::f24::FromFloat32(mesh->points[v][c]);
                                        particle_inputs[index][1][3] = Pica::f24::FromFloat32(mesh->colors[v][3]);
                                    }
                                }
                            }
                        }
                        const auto splash_tev=regs.texturing.GetTevStages();
                        if(regs.texturing.main_config.texture0_enable && !regs.texturing.main_config.texture1_enable &&
                           !regs.texturing.main_config.texture2_enable && !pipeline_info.state.depth_stencil.depth_write_enable &&
                           splash_tev[0].sources_raw==249564899 && splash_tev[0].modifiers_raw==2 &&
                           splash_tev[0].ops_raw==0 && splash_tev[0].scales_raw==2 &&
                           splash_tev[1].sources_raw==250482430 && splash_tev[1].ops_raw==1 &&
                           splash_tev[2].sources_raw==249564976 && splash_tev[2].modifiers_raw==32 && splash_tev[2].ops_raw==8) {
                            if(const auto splash=MM3D::SplashInterpolation::Decode(inputs,replay.Order())) {
                                const auto mid=splash_cpu_history.Apply(frame,regs.texturing.texture0.GetPhysicalAddress(),epoch,splash->sprite);
                                if(mid && !fallback && !native_preview.load()) {
                                    particle_inputs=replay.Inputs();
                                    for(unsigned i=0;i<4;++i) {
                                        const auto at=splash->indices[i];
                                        for(unsigned c=0;c<3;++c)particle_inputs[at][0][c]=Pica::f24::FromFloat32(mid->sprite.points[i][c]);
                                        particle_inputs[at][1][3]=Pica::f24::FromFloat32(mid->sprite.colors[i][3]);
                                    }
                                    if(mid->blend) {
                                        splash_old_inputs=particle_inputs;
                                        for(unsigned i=0;i<4;++i)for(unsigned c=0;c<2;++c)
                                            splash_old_inputs[splash->indices[i]][2][c]=Pica::f24::FromFloat32(mid->old_uv[i][c]);
                                    }
                                }
                            }
                        }
                        if (half && !fallback && !native_preview.load()) {
                            particle_inputs = replay.Inputs();
                            for (const auto& q : *half)
                                for (unsigned v = 0; v < 4; ++v)
                                    for (unsigned a = 0; a < 3; ++a)
                                        for (unsigned c = 0; c < 4; ++c)
                                            particle_inputs[q.indices[v]][a][c] =
                                                Pica::f24::FromFloat32(q.vertices[v][a][c]);
                        }
                    }
                    if(regs.pipeline.triangle_topology==Pica::PipelineRegs::TriangleTopology::Strip &&
                       regs.pipeline.num_vertices<=768 && pica.GetCpuVertexReplay().Inputs().size()<=513 &&
                       regs.vs.max_input_attribute_index==3 && regs.vs.GetRegisterForAttribute(0)==0 &&
                       regs.vs.GetRegisterForAttribute(1)==1 && regs.vs.GetRegisterForAttribute(2)==3 &&
                       regs.vs.GetRegisterForAttribute(3)==4 && blend.blend_enable &&
                       blend.src_color_blend_factor==BF::SourceAlpha && blend.dst_color_blend_factor==BF::OneMinusSourceAlpha &&
                       blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add && blend.color_write_mask==15 &&
                       pipeline_info.state.depth_stencil.depth_test_enable && !pipeline_info.state.depth_stencil.depth_write_enable &&
                       !pipeline_info.state.depth_stencil.stencil_test_enable && regs.texturing.main_config.texture0_enable &&
                       !regs.texturing.main_config.texture1_enable && !regs.texturing.main_config.texture2_enable) {
                        const auto& replay=pica.GetCpuVertexReplay();
                        std::vector<MM3D::LeafParticleInterpolation::Vertex> inputs(replay.Inputs().size());
                        for(unsigned i=0;i<inputs.size();++i)for(unsigned a=0;a<4;++a)for(unsigned c=0;c<4;++c)
                            inputs[i][a][c]=replay.Inputs()[i][a][c].ToFloat32();
                        if(const auto leaves=MM3D::LeafParticleInterpolation::Decode(inputs,replay.Order())) {
                            const auto half=leaf_particle_history.Apply(frame,
                                {regs.texturing.texture0.GetPhysicalAddress(),epoch},*leaves);
                            if(half && !fallback && !native_preview.load()) {
                                particle_inputs=replay.Inputs();
                                for(const auto& q:*half)for(unsigned k=0;k<4;++k)
                                    for(unsigned a=0;a<2;++a)for(unsigned c=0;c<3;++c)
                                        particle_inputs[q.indices[k]][a][c]=Pica::f24::FromFloat32(q.vertices[k][a][c]);
                            }
                        }
                    }
                    if(regs.pipeline.triangle_topology==Pica::PipelineRegs::TriangleTopology::Shader &&
                       regs.pipeline.num_vertices==12 && pica.GetCpuVertexReplay().Inputs().size()==6 &&
                       regs.vs.max_input_attribute_index==2 && regs.vs.GetRegisterForAttribute(0)==0 &&
                       regs.vs.GetRegisterForAttribute(1)==1 && regs.vs.GetRegisterForAttribute(2)==4 &&
                       blend.blend_enable && blend.src_color_blend_factor==BF::SourceAlpha &&
                       blend.dst_color_blend_factor==BF::OneMinusSourceAlpha &&
                       blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add && blend.color_write_mask==15 &&
                       pipeline_info.state.depth_stencil.depth_test_enable && !pipeline_info.state.depth_stencil.depth_write_enable &&
                       !pipeline_info.state.depth_stencil.stencil_test_enable && regs.texturing.main_config.texture0_enable &&
                       regs.texturing.main_config.texture1_enable && !regs.texturing.main_config.texture2_enable) {
                        const auto& replay=pica.GetCpuVertexReplay();
                        std::array<MM3D::MothInterpolation::Vertex,6> inputs{};
                        for(unsigned i=0;i<6;++i)for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
                            inputs[i][a][c]=replay.Inputs()[i][a][c].ToFloat32();
                        if(MM3D::MothInterpolation::Recognized(inputs,replay.Order())) {
                            auto model=Identity();
                            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                                model[r][c]=pica.vs_setup.uniforms.f[20+r][c].ToFloat32();
                            const auto half=moth_history.Apply(frame,{
                                regs.texturing.texture0.GetPhysicalAddress(),
                                regs.texturing.texture1.GetPhysicalAddress(),epoch},Multiply(camera,model));
                            // Projection supplies the midpoint camera. Keep the
                            // model in the current view to avoid applying it twice.
                            if(half && !fallback && !native_preview.load())
                                moth_override=Multiply(InverseRigid(camera),*half);
                        }
                    }
                    auto uniforms = pica.vs_setup.uniforms;
                    if (regs.pipeline.num_vertices == 6 &&
                        pica.vs_setup.GetProgramCodeHash() == 0x89b1ab98da3d9241ULL &&
                        pica.vs_setup.GetSwizzleDataHash() == 0x6f0b398b1fc186a9ULL &&
                        regs.pipeline.triangle_topology == Pica::PipelineRegs::TriangleTopology::Shader &&
                        regs.vs.max_input_attribute_index == 2 &&
                        regs.vs.GetRegisterForAttribute(0) == 0 &&
                        regs.vs.GetRegisterForAttribute(1) == 4 &&
                        regs.vs.GetRegisterForAttribute(2) == 1 &&
                        blend.blend_enable && blend.src_color_blend_factor == BF::SourceAlpha &&
                        blend.dst_color_blend_factor == BF::One &&
                        blend.color_blend_eq == Pica::FramebufferRegs::BlendEquation::Add &&
                        blend.color_write_mask == 15 && pipeline_info.state.depth_stencil.depth_test_enable &&
                        !pipeline_info.state.depth_stencil.stencil_test_enable) {
                        const auto& replay = pica.GetCpuVertexReplay();
                        if (replay.Inputs().size() == 4) {
                            std::array<MM3D::TorchInterpolation::Vertex, 4> inputs{};
                            for (unsigned i = 0; i < 4; ++i)
                                for (unsigned a = 0; a < 3; ++a)
                                    for (unsigned c = 0; c < 4; ++c)
                                        inputs[i][a][c] = replay.Inputs()[i][a][c].ToFloat32();
                            MM3D::TorchInterpolation::Values uv{};
                            for (unsigned r = 0; r < uv.size(); ++r)
                                for (unsigned c = 0; c < 4; ++c)
                                    uv[r][c] = uniforms.f[MM3D::TorchInterpolation::Rows[r]][c].ToFloat32();
                            if (MM3D::TorchInterpolation::Recognized(inputs, replay.Order(), uv)) {
                                // Palette zero is in view space; camera is the
                                // inverse view. Recover the stationary torch origin.
                                MM3D::TorchInterpolation::Point origin{};
                                for (unsigned r = 0; r < 3; ++r) {
                                    double position = camera[r][3];
                                    for (unsigned c = 0; c < 3; ++c)
                                        position += camera[r][c] * uniforms.f[20 + c][3].ToFloat32();
                                    origin[r] = static_cast<float>(position);
                                }
                                const auto half = torch_history.Apply(frame, {
                                    regs.texturing.texture0.GetPhysicalAddress(),
                                    regs.texturing.texture1.GetPhysicalAddress(),
                                    regs.texturing.texture2.GetPhysicalAddress(), epoch}, origin, uv[3][3]);
                                if (half && !fallback && !native_preview.load()) {
                                    uniforms.f[15][3] = Pica::f24::FromFloat32((*half)[0]);
                                    uniforms.f[18][3] = Pica::f24::FromFloat32((*half)[1]);
                                }
                            }
                        }
                    }
                    for (unsigned row = 0; row < 4; ++row)
                        for (unsigned col = 0; col < 4; ++col)
                            uniforms.f[row][col] = Pica::f24::FromFloat32(float((*midpoint_projection)[row][col]));
                    for (unsigned bone = 0; bone < link_overrides.size(); ++bone) {
                        if (!link_overrides[bone]) continue;
                        for (unsigned i = 0; i < 12; ++i)
                            uniforms.f[20 + bone * 3 + i / 4][i % 4] = Pica::f24::FromFloat32(
                                float((*link_overrides[bone])[i / 4][i % 4]));
                    }
                    if(moth_override)
                        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                            uniforms.f[20+r][c]=Pica::f24::FromFloat32(float((*moth_override)[r][c]));
                    if (song_overrides)
                        for (unsigned row = 0; row < MM3D::SongEffectInterpolation::Rows.size(); ++row)
                            uniforms.f[MM3D::SongEffectInterpolation::Rows[row]][3] =
                                Pica::f24::FromFloat32((*song_overrides)[row][3]);
                    auto& vertices = midpoint_cpu_vertices;
                    vertices.clear();
                    vertices.reserve(vertex_batch.size());
                    pica.ReplayCpuVertices(uniforms, [&](const auto& a, const auto& b, const auto& c) {
                        AppendTriangle(vertices, a, b, c);
                    }, particle_inputs);
                    if(map_trace && vertices.size()==vertex_batch.size() && !vertices.empty()) {
                        map_trace->flags|=8;
                        for(unsigned i=0;i<vertices.size();++i)for(unsigned c=0;c<4;++c)
                            map_trace->position_delta=std::max(map_trace->position_delta,
                                std::abs(vertices[i].position[c]-vertex_batch[i].position[c]));
                    }
                    midpoint_cpu_draw = upload_cpu_draw(vertices);
                    if(!splash_old_inputs.empty()) {
                        vertices.clear();
                        if(pica.ReplayCpuVertices(uniforms,[&](const auto& a,const auto& b,const auto& c){
                            AppendTriangle(vertices,a,b,c);
                        },splash_old_inputs))splash_old_cpu_draw=upload_cpu_draw(vertices);
                    }
                    use_midpoint_cpu_draw = true;
                }
                UploadUniforms(accelerate);
                const bool reverse=compare==CF::GreaterThan || compare==CF::GreaterThanOrEqual;
                {
                RenderManager::ReplayScope batching{renderpass_cache,
                    layer->CanBatch(std::max<unsigned>(draw_rect.right, viewport.width),
                                    std::max<unsigned>(draw_rect.top, viewport.height)),
                    layer->Attachments()};
                if(layer->Begin(std::max<unsigned>(draw_rect.right,viewport.width),
                                std::max<unsigned>(draw_rect.top,viewport.height),reverse?0.f:1.f,
                                midpoint_world_frame!=frame)) {
                    midpoint_world_frame=frame;
                    // Sky is perspective geometry too, but an early screen
                    // darkener belongs behind opaque actors/scenery drawn next.
                    const auto& native_blend=pipeline_info.state.blending;
                    using BF=Pica::FramebufferRegs::BlendFactor;
                    Matrix world_model=Identity();
                    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
                        world_model[row][col]=pica.vs_setup.uniforms.f[20+row][col].ToFloat32();
                    const auto& world_textures=regs.texturing.main_config;
                    const unsigned world_texture_mask=unsigned(world_textures.texture0_enable)|
                        (unsigned(world_textures.texture1_enable)<<1)|(unsigned(world_textures.texture2_enable)<<2);
                    if(!OpaqueSkyDome(regs.pipeline.num_vertices,projection,world_model,world_texture_mask) &&
                       pipeline_info.state.depth_stencil.depth_test_enable &&
                       native_blend.color_write_mask==15 &&
                       (!native_blend.blend_enable ||
                        (native_blend.src_color_blend_factor==BF::One &&
                         native_blend.dst_color_blend_factor==BF::Zero &&
                         native_blend.color_blend_eq==Pica::FramebufferRegs::BlendEquation::Add)))
                        midpoint_opaque_world_frame=frame;
                    pipeline_info.state.attachments.color=VideoCore::PixelFormat::RGBA8;
                    pipeline_info.state.attachments.depth=VideoCore::PixelFormat::D24S8;
                    pipeline_info.dynamic_info.viewport={viewport.x,viewport.y,viewport.x+viewport.width,viewport.y+viewport.height};
                    pipeline_info.dynamic_info.scissor=draw_rect;
                    pipeline_cache.BindPipeline(pipeline_info,true);
                    const auto native_bindings=binding_offsets;
                    if(accelerate && ripple_offsets && !fallback && !native_preview.load()) {
                        binding_offsets[0]=(*ripple_offsets)[0];binding_offsets[1]=(*ripple_offsets)[1];
                    }
                    if(accelerate && splash_offsets && !fallback && !native_preview.load()) {
                        binding_offsets[0]=(*splash_offsets)[0];binding_offsets[1]=(*splash_offsets)[1];
                        if(splash_blend) {
                            binding_offsets[2]=(*splash_offsets)[2];draw_batch();
                            binding_offsets[2]=native_bindings[2];
                        }
                    }
                    draw_batch();
                    if(cpu_interpolation && splash_old_cpu_draw.count) {
                        const auto current=midpoint_cpu_draw;midpoint_cpu_draw=splash_old_cpu_draw;
                        draw_batch();midpoint_cpu_draw=current;
                    }
                    binding_offsets=native_bindings;
                    layer->x=viewport.x;layer->y=viewport.y;
                    layer->viewport_width=viewport.width;layer->viewport_height=viewport.height;
                    layer->End(false);++midpoint_copies;midpoint_last_draw_ms=now;
                    if(map_trace)map_trace->flags|=16;
                    const auto& scene_fb=regs.framebuffer.framebuffer;
                    midpoint_scene_copies.Begin(frame,scene_fb.GetColorBufferPhysicalAddress(),
                        scene_fb.GetWidth(),scene_fb.GetHeight(),
                        scene_fb.color_format==Pica::FramebufferRegs::ColorFormat::RGBA8);
                    if(fallback)++midpoint_fallback;
                    else midpoint_frame_interpolated=true;
                    const auto bones=std::count_if(link_overrides.begin(),link_overrides.end(),
                        [](const auto& value){return value.has_value();});
                    if(bones) {
                        ++MM3D::LinkPoseCapture::replaced_draws;
                        MM3D::LinkPoseCapture::replaced_bones.fetch_add(unsigned(bones));
                    }
                }
                }
                pipeline_info=saved;midpoint_projection.reset();link_overrides={};song_overrides.reset();
                if(water_model_alpha_override){water_model_alpha_override.reset();fs_data_dirty=true;}
                if(moving_effect_colors){moving_effect_colors.reset();fs_data_dirty=true;}
                use_midpoint_cpu_draw = false;
                pica.vs_setup.uniforms_dirty=true;UploadUniforms(accelerate);
            }
        } else ++midpoint_rejected;
    }
    if(map_trace)MM3D::EffectCapture::ObserveMapReplay(*map_trace);
#endif

    // Replay verified UI draws using the same shader/uniforms/geometry. Guest target is untouched.
#ifdef ENABLE_MM3D_PRESENTATION
    for(const auto& match:mm3d_draw_profiles) {
        if(!match.atlas_selection || match.presentation) continue;
        MM3D::Atlas::Build(match,atlas_uniforms);
        fs_data_dirty=true; UploadUniforms(accelerate);
        pipeline_cache.UseFragmentShader(regs,user_config,true);
        // Uniform uploads can submit command buffers and end the current pass.
        renderpass_cache.BeginRendering(framebuffer,draw_rect);
        pipeline_info.dynamic_info.viewport={viewport.x,viewport.y,viewport.x+viewport.width,viewport.y+viewport.height};
        pipeline_info.dynamic_info.scissor=draw_rect;
        transform_viewport(match);
        pipeline_cache.BindPipeline(pipeline_info,true);
        draw_batch();
    }
    for (const auto& panel : MM3D::Profiles::Panels()) {
        const bool source_matches = viewport.x==0 && viewport.y==0 && viewport.width>0 && viewport.height>0 &&
            (panel.source_top ? s64(viewport.width)*5==s64(viewport.height)*3 :
                                s64(viewport.width)*4==s64(viewport.height)*3);
        if (!source_matches) continue;
        const bool excluded = std::any_of(panel.exclusions.begin(),panel.exclusions.end(),[&](const auto& rule) {
            return std::find(panel_draw_identities.begin(),panel_draw_identities.end(),rule)!=panel_draw_identities.end();
        });
        if (!excluded) mm3d_draw_profiles.push_back(panel);
    }
    // Manual panel experiment: replay lower-screen UI into one transparent layer.
    // Full viewport only; other render targets and partial viewports are not inferred.
    if (MM3D::Notebook::detail_preview.load() && MM3D::Notebook::transparent_copy.load() &&
        viewport.x == 0 && viewport.y == 0 && viewport.width > 0 && viewport.height > 0 &&
        s64(viewport.width)*4 == s64(viewport.height)*3) {
        if (panel_excluded) ++MM3D::Notebook::excluded_draws;
        else {
            MM3D::Profiles::Profile panel;
            panel.id = "@notebook_panel";
            panel.presentation = true;
            mm3d_draw_profiles.push_back(panel);
        }
    }
    // Keep the map's lower-screen geometry and radar sprites in a transparent
    // layer. LayoutShader controls are excluded; presentation also requires the
    // exact radar texture to have appeared in this same frame.
    const auto map_viewport=regs.rasterizer.GetViewportRect();
    if(MM3D::Minimap::LegacyCopy && MM3D::Minimap::enabled.load() && !shadow_rendering &&
       map_viewport.GetWidth()==240 && map_viewport.GetHeight()==320 &&
       (MM3D::Minimap::marker_draw || pica.vs_setup.GetProgramCodeHash()!=0xed1d199d94013c51ULL)) {
        MM3D::Profiles::Profile map;
        map.id="@minimap"; map.presentation=true;
        mm3d_draw_profiles.push_back(map);
    }
    // Reuse the verified native screen cover, with native timing/color, in a
    // separate transparent layer. Never apply a camera transform to this quad.
    if(MM3D::CameraMidpoint::enabled.load() && replay_geometry && succeeded &&
       !shadow_rendering && using_color_fb && !write_depth_fb && !use_pause &&
       !pipeline_info.state.depth_stencil.stencil_test_enable && !guest_regions.info[0]) {
        const auto& textures=regs.texturing.main_config;
        const auto& merger=regs.framebuffer.output_merger;
        MM3D::CameraMidpoint::Matrix projection{};
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)
            projection[i][j]=pica.vs_setup.uniforms.f[i][j].ToFloat32();
        const MM3D::WidescreenProbe::Draw candidate{
            pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash(),
            u32(regs.pipeline.num_vertices),unsigned(wide_viewport.GetWidth()),
            unsigned(wide_viewport.GetHeight()),
            unsigned(textures.texture0_enable)|(unsigned(textures.texture1_enable)<<1)|
                (unsigned(textures.texture2_enable)<<2),
            unsigned(merger.alpha_blending.factor_source_rgb.Value())|
                (unsigned(merger.alpha_blending.factor_dest_rgb.Value())<<4)|
                (unsigned(merger.alpha_blending.blend_equation_rgb.Value())<<8)|
                (unsigned(merger.alphablend_enable)<<16),u32(merger.depth_color_mask)};
        if(MM3D::CameraMidpoint::TransitionFade(candidate,projection)) {
            const auto frame=MM3D::guest_frames.load();
            if(midpoint_fade_frame!=frame) {
                if(const auto it=prompt_layers.find("@camera_midpoint_fade");it!=prompt_layers.end())
                    it->second->NextFrame();
                midpoint_fade_frame=frame;
            }
            MM3D::Profiles::Profile fade;
            fade.id="@camera_midpoint_fade";fade.presentation=true;fade.source_top=true;
            mm3d_draw_profiles.push_back(fade);
            // A fade-only frame can still cover the last captured scene.
            midpoint_last_draw_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }
    // Capture the reason, not just the aggregate fallback, during an explicit
    // timing trace. Limit each rejection category to one record per second.
    // Reasons: draw failed, stencil, unknown shader, layer limit, allocation, replay state.
    const auto reject_midpoint_hud = [&](unsigned reason) {
        midpoint_hud_failed=true;
        if(!MM3D::InputLatency::Active() &&
           !(MM3D::ScreenShrink::native_normalized.load() &&
             MM3D::ScreenShrink::presentation_scale.load()<1.f))return;
        const auto now=MM3D::InputLatency::Now();
        if(now-midpoint_rejection_logged[reason]<1000000)return;
        midpoint_rejection_logged[reason]=now;
        const auto& blend=pipeline_info.state.blending;
        LOG_INFO(Render_Vulkan,
            "Interpolation HUD rejection: reason={} shader={:016x} swizzle={:016x} "
            "vertices={} viewport={}x{} origin={},{} accelerated={} succeeded={} "
            "stencil={} blend={} src={} dst={} equation={} mask={} "
            "texture0={:08x} framebuffer={:08x} shrink={} normalized={} "
            "textures={} frame={} source_frame={} sources={:08x},{:08x},{:08x},{:08x} "
            "padded={:08x} texture_size={}x{} texture_format={} uv={},{},{},{}",
            reason,pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash(),
            u32(regs.pipeline.num_vertices),viewport.width,viewport.height,viewport.x,viewport.y,
            accelerate,succeeded,unsigned(pipeline_info.state.depth_stencil.stencil_test_enable),
            unsigned(blend.blend_enable),unsigned(blend.src_color_blend_factor.Value()),
            unsigned(blend.dst_color_blend_factor.Value()),unsigned(blend.color_blend_eq.Value()),
            unsigned(blend.color_write_mask),regs.texturing.texture0.GetPhysicalAddress(),
            regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),
            MM3D::ScreenShrink::presentation_scale.load(),MM3D::ScreenShrink::native_normalized.load(),
            unsigned(regs.texturing.main_config.texture0_enable)|
                (unsigned(regs.texturing.main_config.texture1_enable)<<1)|
                (unsigned(regs.texturing.main_config.texture2_enable)<<2),
            MM3D::guest_frames.load(),midpoint_scene_copies.frame,
            midpoint_scene_copies.addresses[0],midpoint_scene_copies.addresses[1],
            midpoint_scene_copies.addresses[2],midpoint_scene_copies.addresses[3],
            midpoint_scene_copies.padded_destination,u32(regs.texturing.texture0.width),
            u32(regs.texturing.texture0.height),unsigned(regs.texturing.texture0_format.Value()),
            native_uv_bounds?(*native_uv_bounds)[0]:-1.f,native_uv_bounds?(*native_uv_bounds)[1]:-1.f,
            native_uv_bounds?(*native_uv_bounds)[2]:-1.f,native_uv_bounds?(*native_uv_bounds)[3]:-1.f);
    };
    if(MM3D::CameraMidpoint::present_60.load() &&
       !shadow_rendering &&
       using_color_fb && (!use_pause || inventory_owner) &&
       MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight())) {
        const auto frame=MM3D::guest_frames.load();
        if(midpoint_hud_frame!=frame) {
            midpoint_hud_frame=frame;midpoint_hud_failed=false;
            if(const auto it=prompt_layers.find("@camera_midpoint_hud");it!=prompt_layers.end())
                it->second->NextFrame();
        }
        if(!replay_geometry || !succeeded)reject_midpoint_hud(0);
        const auto shader=pica.vs_setup.GetProgramCodeHash();
        const auto swizzle=pica.vs_setup.GetSwizzleDataHash();
        const bool world=shader==0x89b1ab98da3d9241ULL && swizzle==0x6f0b398b1fc186a9ULL;
        const bool ortho=world && pica.vs_setup.uniforms.f[3].x.ToFloat32()==0 &&
            pica.vs_setup.uniforms.f[3].y.ToFloat32()==0 &&
            pica.vs_setup.uniforms.f[3].z.ToFloat32()==0 &&
            pica.vs_setup.uniforms.f[3].w.ToFloat32()==1;
        const bool layout=shader==0x9b343fea686d98e0ULL && swizzle==0xac22b79e29209c01ULL;
        if(cpu_interpolation && world && !ortho && !midpoint_title_logo && !replay_world)
            reject_midpoint_hud(0); // Never silently omit unsupported CPU world geometry.
        const bool fade=std::any_of(mm3d_draw_profiles.begin(),mm3d_draw_profiles.end(),
            [](const auto& p){return p.id=="@camera_midpoint_fade";});
        const auto& blend=pipeline_info.state.blending;
        const auto& textures=regs.texturing.main_config;
        using BF=Pica::FramebufferRegs::BlendFactor;
        using BE=Pica::FramebufferRegs::BlendEquation;
        const unsigned texture_mask=unsigned(textures.texture0_enable)|
            (unsigned(textures.texture1_enable)<<1)|(unsigned(textures.texture2_enable)<<2);
        const bool opaque=blend.blend_enable && blend.src_color_blend_factor==BF::One &&
            blend.dst_color_blend_factor==BF::Zero && blend.color_blend_eq==BE::Add &&
            blend.color_write_mask==15;
        const bool scene_copy=MM3D::CameraMidpoint::NormalizedSceneCopy(
            MM3D::ScreenShrink::native_normalized.load(),
            midpoint_scene_copies.Contains(frame,regs.texturing.texture0.GetPhysicalAddress()) || padded_scene_copy,
            ortho,u32(regs.pipeline.num_vertices),
            texture_mask,opaque,
            pipeline_info.state.depth_stencil.stencil_test_enable);
        const auto color_address=regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress();
        const bool scene_clear=MM3D::CameraMidpoint::NormalizedSceneClear(
            MM3D::ScreenShrink::native_normalized.load(),
            midpoint_scene_copies.Contains(frame,color_address),
            ortho,u32(regs.pipeline.num_vertices),texture_mask,opaque,
            pipeline_info.state.depth_stencil.stencil_test_enable);
        if(midpoint_scene_clear.Observe(frame,color_address,scene_clear,scene_copy && succeeded))
            reject_midpoint_hud(5);
        // Accepted screen-space effects can cover an interpolated scene with a
        // native scene copy without producing a HUD rejection. Sample those
        // draws during an explicitly requested timing trace as well.
        if(MM3D::InputLatency::Active() && frame%10==0 && ortho &&
           regs.pipeline.num_vertices==4) {
            const auto& u=pica.vs_setup.uniforms.f;
            LOG_INFO(Render_Vulkan,
                "Interpolation screen quad: frame={} texture={:08x} framebuffer={:08x} "
                "scene_source={} blend={} src={} dst={} equation={} mask={} "
                "textures={} model={},{},{},{};{},{},{},{};{},{},{},{} color={},{},{},{}",
                frame,regs.texturing.texture0.GetPhysicalAddress(),
                regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),
                midpoint_scene_copies.Contains(frame,regs.texturing.texture0.GetPhysicalAddress()),
                unsigned(blend.blend_enable),unsigned(blend.src_color_blend_factor.Value()),
                unsigned(blend.dst_color_blend_factor.Value()),unsigned(blend.color_blend_eq.Value()),
                unsigned(blend.color_write_mask),
                unsigned(textures.texture0_enable)|(unsigned(textures.texture1_enable)<<1)|
                    (unsigned(textures.texture2_enable)<<2),
                u[20].x.ToFloat32(),u[20].y.ToFloat32(),u[20].z.ToFloat32(),u[20].w.ToFloat32(),
                u[21].x.ToFloat32(),u[21].y.ToFloat32(),u[21].z.ToFloat32(),u[21].w.ToFloat32(),
                u[22].x.ToFloat32(),u[22].y.ToFloat32(),u[22].z.ToFloat32(),u[22].w.ToFloat32(),
                u[8].x.ToFloat32(),u[8].y.ToFloat32(),u[8].z.ToFloat32(),u[8].w.ToFloat32());
        }
        // The compositor already fits the private world/HUD into the shrinking
        // rectangle. Copying the native scene here would cover that midpoint.
        // Preserve native screen-space draw order. Splitting bars/fades into
        // a final layer would place them above HUD drawn later by the game,
        // changing occlusion on every midpoint frame.
        // The lens mask can follow translucent sky draws. It still precedes
        // opaque scenery; sky alone must not disqualify the stencil pass.
        const bool lens_mask=lens_kind && *lens_kind<2 && midpoint_opaque_world_frame!=frame;
        if(lens_mask && replay_geometry && succeeded)midpoint_stencil_frame=frame;
        if((ortho || layout || fade || midpoint_title_logo) && !scene_copy && !scene_clear) {
            if(!replay_geometry || (pipeline_info.state.depth_stencil.stencil_test_enable &&
                              midpoint_stencil_frame!=frame && !midpoint_title_logo)) {
                reject_midpoint_hud(1);
            } else {
                MM3D::Profiles::Profile hud;
                // Background layouts and the death darkener precede opaque
                // world geometry, sometimes after perspective sky draws. Keep
                // them in the scene so later geometry covers them in native order.
                hud.id=midpoint_title_logo || (midpoint_opaque_world_frame==frame && midpoint_stencil_frame!=frame)?
                    "@camera_midpoint_hud":"@camera_midpoint";
                hud.presentation=true;hud.source_top=true;
                mm3d_draw_profiles.push_back(hud);
            }
        } else if(!world && !layout) {
            // Unknown top-screen draws must not silently vanish on alternate frames.
            reject_midpoint_hud(2);
        }
    }
    // Only the private midpoint HUD receives the animated MessageWindow plate.
    // Snapshot once per native draw, outside replay scopes; never write guest
    // uniforms or retain a "currently drawing dialogue" flag across GPU work.
    std::optional<MM3D::MessageInterpolation::Pose> message_half;
    std::optional<MM3D::MessageInterpolation::Source> message_source;
    CpuDraw message_cpu_draw{};
    if (MM3D::CameraMidpoint::present_60.load() && replay_geometry && succeeded &&
        !shadow_rendering && using_color_fb && !use_pause &&
        MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(), wide_viewport.GetHeight()) &&
        MM3D::MessageInterpolation::Candidate(pica.vs_setup.GetProgramCodeHash(),
            pica.vs_setup.GetSwizzleDataHash(), regs.pipeline.num_vertices,
            pica.vs_setup.uniforms.f[10].w.ToFloat32(),true)) {
        const auto& blend = pipeline_info.state.blending;
        const auto& depth = pipeline_info.state.depth_stencil;
        const auto& textures = regs.texturing.main_config;
        using BF = Pica::FramebufferRegs::BlendFactor;
        if (blend.blend_enable && blend.src_color_blend_factor == BF::SourceAlpha &&
            blend.dst_color_blend_factor == BF::OneMinusSourceAlpha &&
            blend.color_blend_eq == Pica::FramebufferRegs::BlendEquation::Add &&
            blend.color_write_mask == 15 && !depth.depth_test_enable &&
            !depth.stencil_test_enable && textures.texture0_enable &&
            !textures.texture1_enable && !textures.texture2_enable) {
            MM3D::MessageInterpolation::Values values{};
            for (unsigned r = 0; r < values.size(); ++r)
                for (unsigned c = 0; c < 4; ++c)
                    values[r][c] = pica.vs_setup.uniforms.f[r][c].ToFloat32();
            message_source=MM3D::MessageInterpolation::SourceFor(MM3D::guest_frames.load(),
                MM3D::LinkPoseCapture::interpolation_generation.load(),values);
            message_half = message_history.Apply(MM3D::guest_frames.load(), {
                regs.texturing.texture0.GetPhysicalAddress(),
                regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),
                MM3D::LinkPoseCapture::interpolation_generation.load()}, values, message_source);
            if (MM3D::CameraMidpoint::native_preview.load() || (!midpoint_frame_interpolated && !inventory_owner))
                message_half.reset();
            if (message_half && cpu_interpolation) {
                auto uniforms = pica.vs_setup.uniforms;
                for (unsigned r = 0; r < MM3D::MessageInterpolation::Rows.size(); ++r)
                    for (unsigned c = 0; c < 4; ++c)
                        uniforms.f[MM3D::MessageInterpolation::Rows[r]][c] =
                            Pica::f24::FromFloat32((*message_half)[r][c]);
                auto& vertices = midpoint_cpu_vertices;
                vertices.clear();
                if (pica.ReplayCpuVertices(uniforms, [&](const auto& a, const auto& b, const auto& c) {
                        AppendTriangle(vertices, a, b, c);
                    }) && vertices.size() == vertex_batch.size())
                    message_cpu_draw = upload_cpu_draw(vertices);
            }
        }
    }
    std::optional<MM3D::ItemSlideInterpolation::Pose> item_slide_half;
    CpuDraw item_slide_cpu_draw{};
    if (MM3D::CameraMidpoint::present_60.load() && replay_geometry && succeeded &&
        !shadow_rendering && using_color_fb && !use_pause &&
        MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) &&
        !pipeline_info.state.depth_stencil.depth_test_enable &&
        !pipeline_info.state.depth_stencil.stencil_test_enable &&
        MM3D::ItemSlideInterpolation::Candidate(pica.vs_setup.GetProgramCodeHash(),
            pica.vs_setup.GetSwizzleDataHash(),regs.pipeline.num_vertices)) {
        MM3D::ItemSlideInterpolation::Values values{};
        for(unsigned r=0;r<values.size();++r)for(unsigned c=0;c<4;++c)
            values[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
        const auto frame=MM3D::guest_frames.load();
        const auto generation=MM3D::LinkPoseCapture::interpolation_generation.load();
        item_slide_half=item_slide_history.Apply(frame,{
            regs.texturing.texture0.GetPhysicalAddress(),
            regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),generation},values,
            regs.pipeline.num_vertices,MM3D::ItemSlideInterpolation::Recent(frame,generation));
        if(MM3D::CameraMidpoint::native_preview.load()||!midpoint_frame_interpolated)
            item_slide_half.reset();
        if(item_slide_half&&cpu_interpolation) {
            auto uniforms=pica.vs_setup.uniforms;
            for(unsigned q=0;q<item_slide_half->count;++q)
                uniforms.f[11+q*4][0]=Pica::f24::FromFloat32(item_slide_half->x[q]);
            auto& vertices=midpoint_cpu_vertices;vertices.clear();
            if(pica.ReplayCpuVertices(uniforms,[&](const auto& a,const auto& b,const auto& c){
                    AppendTriangle(vertices,a,b,c);
                })&&vertices.size()==vertex_batch.size())
                item_slide_cpu_draw=upload_cpu_draw(vertices);
        }
    }
    std::optional<MM3D::FileSelectInterpolation::Pose> file_select_half;
    CpuDraw file_select_cpu_draw{};
    {
        const auto& blend=pipeline_info.state.blending;
        const auto& depth=pipeline_info.state.depth_stencil;
        const auto& textures=regs.texturing.main_config;
        const unsigned texture_mask=unsigned(textures.texture0_enable) |
            (unsigned(textures.texture1_enable)<<1) | (unsigned(textures.texture2_enable)<<2);
        if (MM3D::CameraMidpoint::present_60.load() && replay_geometry && succeeded &&
            !shadow_rendering && using_color_fb && !use_pause &&
            MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) &&
            !depth.depth_test_enable && !depth.stencil_test_enable && blend.color_write_mask==15 &&
            MM3D::FileSelectInterpolation::Candidate(pica.vs_setup.GetProgramCodeHash(),
                pica.vs_setup.GetSwizzleDataHash(),
                texture_mask,depth.depth_write_enable,blend.blend_enable,
                unsigned(blend.src_color_blend_factor.Value()),
                unsigned(blend.dst_color_blend_factor.Value()),
                unsigned(blend.color_blend_eq.Value()),regs.pipeline.num_vertices)) {
            MM3D::FileSelectInterpolation::Values values{};
            for(unsigned r=0;r<values.size();++r)for(unsigned c=0;c<4;++c)
                values[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
            const auto frame=MM3D::guest_frames.load();
            const auto generation=MM3D::LinkPoseCapture::interpolation_generation.load();
            const auto owner=MM3D::FileSelectInterpolation::Owner(frame,generation);
            const bool credits=MM3D::FileSelectInterpolation::CreditsLayout(values,regs.pipeline.num_vertices);
            const auto history_owner=owner?owner:(credits?regs.texturing.texture0.GetPhysicalAddress():0u);
            MM3D::FileSelectInterpolation::Tev tev{};
            const auto stages=regs.texturing.GetTevStages();
            for(unsigned i=0;i<stages.size();++i)
                tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,stages[i].ops_raw,
                    stages[i].const_color,stages[i].scales_raw};
            const MM3D::FileSelectInterpolation::Key layout_key{
                regs.texturing.texture0.GetPhysicalAddress(),
                regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),generation,history_owner};
            file_select_half=!owner && credits ? file_select_history.ApplyCredits(frame,layout_key,
                values,regs.pipeline.num_vertices,!MM3D::CameraMidpoint::native_preview.load()) :
                file_select_history.Apply(frame,layout_key,values,regs.pipeline.num_vertices,
                !MM3D::CameraMidpoint::native_preview.load(),
                owner?MM3D::FileSelectInterpolation::Motion(frame,generation,owner):std::vector<float>{},
                MM3D::FileSelectInterpolation::GlowAlpha(tev));
            if(file_select_half && cpu_interpolation) {
                auto uniforms=pica.vs_setup.uniforms;
                for(unsigned q=0;q<file_select_half->count;++q) {
                    uniforms.f[11+q*4][0]=Pica::f24::FromFloat32(file_select_half->x[q]);
                    uniforms.f[12+q*4][0]=Pica::f24::FromFloat32(file_select_half->y[q]);
                    if(file_select_half->rectangles)for(unsigned row=0;row<2;++row)for(unsigned c=0;c<4;++c)
                        uniforms.f[11+q*4+row][c]=Pica::f24::FromFloat32((*file_select_half->rectangles)[q*2+row][c]);
                }
                auto& vertices=midpoint_cpu_vertices;vertices.clear();
                if(pica.ReplayCpuVertices(uniforms,[&](const auto& a,const auto& b,const auto& c){
                        AppendTriangle(vertices,a,b,c);
                    })&&vertices.size()==vertex_batch.size())
                    file_select_cpu_draw=upload_cpu_draw(vertices);
            }
        }
    }
    std::optional<MM3D::InventoryInterpolation::Pose> inventory_half;
    CpuDraw inventory_cpu_draw{};
    const bool trace_layout=MM3D::EffectCapture::Recording(MM3D::guest_frames.load()) &&
        !shadow_rendering && using_color_fb &&
        MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight()) &&
        MM3D::FileSelectInterpolation::Layout(pica.vs_setup.GetProgramCodeHash(),
            pica.vs_setup.GetSwizzleDataHash(),regs.pipeline.num_vertices);
    MM3D::EffectCapture::LayoutReplay layout_trace{
        .frame=MM3D::guest_frames.load(),.texture=regs.texturing.texture0.GetPhysicalAddress(),
        .vertices=regs.pipeline.num_vertices,.owner=message_source?(*message_source)[0]:inventory_owner,
        .flags=(pica.CanReplayCpuVertices()?4u:0u)|(cpu_interpolation?32u:0u)|
            (use_pause?64u:0u)|(succeeded?128u:0u)|(message_source?256u:0u)|(message_half?512u:0u)};
    const auto banner_owner=MM3D::BannerInterpolation::Owner(MM3D::guest_frames.load(),
        MM3D::LinkPoseCapture::interpolation_generation.load());
    const auto magic_uv=[&](unsigned row) {
        MM3D::MagicMeterInterpolation::Row result{};
        for(unsigned c=0;c<4;++c)result[c]=pica.vs_setup.uniforms.f[row][c].ToFloat32();
        return result;
    };
    const bool magic_meter=MM3D::MagicMeterInterpolation::Candidate(
        pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash(),
        regs.pipeline.num_vertices,pica.vs_setup.uniforms.f[10].w.ToFloat32(),magic_uv(94),magic_uv(95));
    const bool heart_pulse=MM3D::HeartInterpolation::Candidate(
        pica.vs_setup.GetProgramCodeHash(),pica.vs_setup.GetSwizzleDataHash(),
        regs.pipeline.num_vertices,pica.vs_setup.uniforms.f[10].w.ToFloat32(),magic_uv(94),magic_uv(95));
    if((inventory_owner||banner_owner||magic_meter||heart_pulse) && MM3D::CameraMidpoint::present_60.load() && replay_geometry && succeeded &&
       !shadow_rendering && using_color_fb && !use_pause &&
       MM3D::Widescreen::IsTopViewport(wide_viewport.GetWidth(),wide_viewport.GetHeight())) {
        const auto& blend=pipeline_info.state.blending;
        const auto& depth=pipeline_info.state.depth_stencil;
        const auto& textures=regs.texturing.main_config;
        if(!depth.depth_test_enable && !depth.stencil_test_enable && blend.color_write_mask==15 &&
           MM3D::FileSelectInterpolation::Candidate(pica.vs_setup.GetProgramCodeHash(),
               pica.vs_setup.GetSwizzleDataHash(),unsigned(textures.texture0_enable)|
                   (unsigned(textures.texture1_enable)<<1)|(unsigned(textures.texture2_enable)<<2),
               depth.depth_write_enable,blend.blend_enable,unsigned(blend.src_color_blend_factor.Value()),
               unsigned(blend.dst_color_blend_factor.Value()),unsigned(blend.color_blend_eq.Value()),
               regs.pipeline.num_vertices)) {
            layout_trace.flags|=1;
            MM3D::InventoryInterpolation::Values values{};
            for(unsigned r=0;r<values.size();++r)for(unsigned c=0;c<4;++c)
                values[r][c]=pica.vs_setup.uniforms.f[r][c].ToFloat32();
            MM3D::InventoryInterpolation::Tev tev{};
            const auto stages=regs.texturing.GetTevStages();
            for(unsigned i=0;i<stages.size();++i)
                tev[i]={stages[i].sources_raw,stages[i].modifiers_raw,stages[i].ops_raw,
                    stages[i].const_color,stages[i].scales_raw};
            const MM3D::FileSelectInterpolation::Key key{
                regs.texturing.texture0.GetPhysicalAddress(),
                regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),
                MM3D::LinkPoseCapture::interpolation_generation.load(),inventory_owner?inventory_owner:banner_owner};
            if(heart_pulse) {
                inventory_half=heart_history.Apply(MM3D::guest_frames.load(),
                    {key[0],key[1],key[2]},values,regs.pipeline.num_vertices,tev);
            } else if(magic_meter) {
                inventory_half=magic_meter_history.Apply(MM3D::guest_frames.load(),
                    {key[0],key[1],key[2]},values,tev);
            } else {
                inventory_half=inventory_owner?
                    inventory_history.Apply(MM3D::guest_frames.load(),key,values,regs.pipeline.num_vertices,tev):
                    banner_history.Apply(MM3D::guest_frames.load(),key,values,regs.pipeline.num_vertices,tev);
            }
            if(inventory_half) {
                layout_trace.flags|=2;
                if(trace_layout)for(unsigned r=0;r<96;++r)if(inventory_half->rows[r])
                    for(unsigned c=0;c<4;++c)layout_trace.uniform_delta=std::max(
                        layout_trace.uniform_delta,std::abs(inventory_half->values[r][c]-values[r][c]));
            }
            if(MM3D::CameraMidpoint::native_preview.load())inventory_half.reset();
            if(inventory_half && cpu_interpolation) {
                auto uniforms=pica.vs_setup.uniforms;
                for(unsigned r=0;r<96;++r)if(inventory_half->rows[r])for(unsigned c=0;c<4;++c)
                    uniforms.f[r][c]=Pica::f24::FromFloat32(inventory_half->values[r][c]);
                auto& vertices=midpoint_cpu_vertices;vertices.clear();
                if(pica.ReplayCpuVertices(uniforms,[&](const auto& a,const auto& b,const auto& c){
                    AppendTriangle(vertices,a,b,c);
                }) && vertices.size()==vertex_batch.size()) {
                    inventory_cpu_draw=upload_cpu_draw(vertices);
                    layout_trace.flags|=8;
                    if(trace_layout)for(unsigned i=0;i<vertices.size();++i)for(unsigned c=0;c<4;++c)
                        layout_trace.position_delta=std::max(layout_trace.position_delta,
                            std::abs(vertices[i].position[c]-vertex_batch[i].position[c]));
                }
                else inventory_half.reset();
            }
        }
    }
    if((lens_kind || letterbox_kind) && MM3D::CameraMidpoint::present_60.load() && replay_geometry && succeeded && !use_pause) {
        if(lens_kind)inventory_half=lens_history.Apply(MM3D::guest_frames.load(),
            {regs.texturing.texture0.GetPhysicalAddress(),regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),
             MM3D::LinkPoseCapture::interpolation_generation.load()},*lens_kind,lens_values);
        else inventory_half=letterbox_history.Apply(MM3D::guest_frames.load(),
            {regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),
             MM3D::LinkPoseCapture::interpolation_generation.load()},*letterbox_kind,lens_values);
        if(MM3D::CameraMidpoint::native_preview.load())inventory_half.reset();
        if(inventory_half && cpu_interpolation) {
            auto uniforms=pica.vs_setup.uniforms;
            for(unsigned r=20;r<23;++r)for(unsigned c=0;c<4;++c)
                uniforms.f[r][c]=Pica::f24::FromFloat32(inventory_half->values[r][c]);
            auto& vertices=midpoint_cpu_vertices;vertices.clear();
            if(pica.ReplayCpuVertices(uniforms,[&](const auto& a,const auto& b,const auto& c){
                AppendTriangle(vertices,a,b,c);
            }) && vertices.size()==vertex_batch.size())inventory_cpu_draw=upload_cpu_draw(vertices);
            else inventory_half.reset();
        }
    }
    CpuDraw glare_cpu_draw{};
    std::optional<float> glare_half_alpha;
    if(glare_candidate && cpu_interpolation && succeeded && !use_pause) {
        MM3D::GlareInterpolation::ScreenSample sample;
        sample.alpha=fs_data.const_color[1].w;
        sample.vertices.resize(vertex_batch.size());
        for(unsigned i=0;i<vertex_batch.size();++i) {
            for(unsigned c=0;c<4;++c) {
                sample.vertices[i].position[c]=vertex_batch[i].position[c];
                sample.vertices[i].color[c]=vertex_batch[i].color[c];
            }
            for(unsigned c=0;c<2;++c)sample.vertices[i].uv[c]=vertex_batch[i].tex_coord0[c];
        }
        const auto half=glare_cpu_history.Apply(MM3D::guest_frames.load(),
            {regs.texturing.texture0.GetPhysicalAddress(),regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress(),
             MM3D::CameraMidpoint::generation.load()},sample);
        if(half && !MM3D::CameraMidpoint::native_preview.load()) {
            auto& vertices=midpoint_cpu_vertices;vertices=vertex_batch;
            for(unsigned i=0;i<vertices.size();++i) {
                for(unsigned c=0;c<2;++c)vertices[i].position[c]=half->vertices[i].position[c];
                for(unsigned c=0;c<4;++c)vertices[i].color[c]=half->vertices[i].color[c];
            }
            glare_cpu_draw=upload_cpu_draw(vertices);glare_half_alpha=half->alpha;
        }
    }
    const float native_clip_x=vs_data.clip_scale_x,native_clip_y=vs_data.clip_scale_y;
    for (const auto& match : mm3d_draw_profiles) {
        if (!match.presentation) continue;
        // Initial atlas presentation supports the full, rotated 320x240 lower-screen viewport.
        // Do not accidentally replay top-screen uses of the same HUD atlas.
        if(match.atlas_selection && (viewport.width<=0 || viewport.height<=0 ||
            s64(viewport.width)*4!=s64(viewport.height)*3)) {++MM3D::prompt_unsupported;continue;}
        using BF = Pica::FramebufferRegs::BlendFactor;
        using BE = Pica::FramebufferRegs::BlendEquation;
        const auto& blend = pipeline_info.state.blending;
        const bool top_ui_source = match.source_top;
        const bool minimap=match.id=="@minimap";
        const bool scene_background=match.id=="@camera_midpoint";
        const bool midpoint_ui=scene_background || match.id=="@camera_midpoint_hud";
        MM3D::InterpolationPerformance::Scope performance_overlay{MM3D::InterpolationPerformance::Part::Replay,midpoint_ui};
        const bool stencil_scene=scene_background && midpoint_stencil_frame==MM3D::guest_frames.load();
        // Sun glare uses source-alpha additive color. Keep it in the ordered
        // HUD replay, with emission but no additional coverage of the world.
        const auto& texture_units=regs.texturing.main_config;
        const auto& projection_row=pica.vs_setup.uniforms.f[3];
        const bool bloom_additive=blend.src_color_blend_factor==BF::One &&
            MM3D::BloomSampling::MidpointComposite(pica.vs_setup.GetProgramCodeHash(),
                pica.vs_setup.GetSwizzleDataHash(),regs.pipeline.num_vertices,
                unsigned(texture_units.texture0_enable)|(unsigned(texture_units.texture1_enable)<<1)|
                    (unsigned(texture_units.texture2_enable)<<2),
                projection_row.x.ToFloat32()==0 && projection_row.y.ToFloat32()==0 &&
                projection_row.z.ToFloat32()==0 && projection_row.w.ToFloat32()==1);
        const bool additive=midpoint_ui && blend.blend_enable &&
            (blend.src_color_blend_factor==BF::SourceAlpha || bloom_additive) &&
            blend.dst_color_blend_factor==BF::One && blend.color_blend_eq==BE::Add;
        // Bounded, read-only vertex capture for the verified glare candidate.
        // Matrix captures show fixed transforms; motion lives in these buffers.
        if(additive && accelerate && MM3D::InputLatency::Active() &&
           regs.pipeline.num_vertices==72 &&
           pica.vs_setup.GetProgramCodeHash()==0x89b1ab98da3d9241ULL &&
           pica.vs_setup.GetSwizzleDataHash()==0x6f0b398b1fc186a9ULL) {
            const auto session=MM3D::InputLatency::session.load();
            if(glare_trace_session!=session) {
                glare_trace_session=session;glare_trace_samples=0;glare_trace_frame=0;
            }
            const auto frame=MM3D::guest_frames.load();
            if(glare_trace_frame!=frame && glare_trace_samples<120) {
                glare_trace_frame=frame;++glare_trace_samples;
                const auto& attributes=regs.pipeline.vertex_attributes;
                std::string formats;
                for(unsigned i=0;i<12;++i)
                    formats+=fmt::format(" {}:{}/{}/{}",i,unsigned(attributes.GetFormat(i)),
                        attributes.GetNumElements(i),regs.vs.GetRegisterForAttribute(i));
                unsigned slot=0;
                for(const auto& loader:attributes.attribute_loaders) {
                    const auto current_slot=slot++;
                    if(!loader.component_count || !loader.byte_count)continue;
                    const u64 address=u64(attributes.GetPhysicalBaseAddress())+loader.data_offset+
                        u64(vertex_info.vs_input_index_min)*loader.byte_count;
                    const u64 size=u64(vertex_info.vs_input_index_max-vertex_info.vs_input_index_min+1)*loader.byte_count;
                    if(address>0xffffffff || size>4096 || !size)continue;
                    const auto source=memory.GetPhysicalRef(static_cast<PAddr>(address));
                    if(source.GetSize()<size)continue;
                    std::string bytes,components;
                    for(unsigned i=0;i<loader.component_count && i<12;++i)
                        components+=fmt::format(" {},",loader.GetComponent(i));
                    for(u64 i=0;i<size;++i)bytes+=fmt::format("{:02x}",source.GetPtr()[i]);
                    LOG_INFO(Render_Vulkan,
                        "Glare vertices: session={} frame={} loader={} first={} last={} stride={} components={} formats={} bytes={}",
                        session,frame,current_slot,vertex_info.vs_input_index_min,vertex_info.vs_input_index_max,
                        u32(loader.byte_count),components,formats,bytes);
                }
            }
        }
        const bool supported = !shadow_rendering &&
            (minimap || top_ui_source || (!pipeline_info.state.depth_stencil.stencil_test_enable &&
             (!pipeline_info.state.depth_stencil.depth_test_enable ||
              pipeline_info.state.depth_stencil.depth_compare_op == Pica::FramebufferRegs::CompareFunc::Always))) &&
            (stencil_scene || additive || (minimap && !blend.blend_enable) || (blend.blend_enable && blend.src_color_blend_factor == BF::SourceAlpha &&
            blend.dst_color_blend_factor == BF::OneMinusSourceAlpha && blend.color_blend_eq == BE::Add)) &&
            (stencil_scene || (blend.color_write_mask & 7) == 7) &&
            viewport.x >= 0 && viewport.y >= 0 && viewport.width > 0 && viewport.height > 0;
        if (supported) {
            if (!prompt_layers.contains(match.id) && prompt_layers.size() >= 16) {
                MM3D::prompt_unsupported.fetch_add(1);
                if(midpoint_ui)reject_midpoint_hud(3);
                continue;
            }
            auto& prompt_layer = prompt_layers[match.id];
            if (!prompt_layer) prompt_layer = std::make_unique<MM3D::PromptLayer>(instance, scheduler, renderpass_cache,midpoint_ui);
            if(scene_background && midpoint_target_frame!=MM3D::guest_frames.load()) {
                prompt_layer->NextFrame(midpoint_background());midpoint_target_frame=MM3D::guest_frames.load();
                midpoint_frame_interpolated=false;
            }
            if(scene_background) {
                prompt_layer->profile.id="@camera_midpoint";
                prompt_layer->profile.source_top=true;
                prompt_layer->profile.scale=.42f;
                prompt_layer->profile.x=.26f;prompt_layer->profile.y=-.26f;
            } else {
                prompt_layer->profile = match;
            }
            // Replay copies are placed with their own preserved source aspect.
            // Applying the scene correction here too would squeeze them twice.
            const float copy_clip_x=midpoint_ui?native_clip_x:1.f;
            const float copy_clip_y=midpoint_ui?native_clip_y:1.f;
            if(vs_data.clip_scale_x!=copy_clip_x || vs_data.clip_scale_y!=copy_clip_y) {
                vs_data.clip_scale_x=copy_clip_x; vs_data.clip_scale_y=copy_clip_y; vs_data_dirty=true;
            }
            atlas_uniforms={};
            if(match.atlas_selection) MM3D::Atlas::Build(match,atlas_uniforms);
            inventory_override = midpoint_ui ? inventory_half : std::nullopt;
            if(midpoint_ui && use_pause)atlas_uniforms.pause=pause_draw;
            item_slide_override = midpoint_ui ? item_slide_half : std::nullopt;
            file_select_override = midpoint_ui ? file_select_half : std::nullopt;
            message_override = midpoint_ui ? message_half : std::nullopt;
            if (midpoint_ui && message_cpu_draw.count) {
                midpoint_cpu_draw = message_cpu_draw;
                use_midpoint_cpu_draw = true;
            }
            if(midpoint_ui && item_slide_cpu_draw.count) {
                midpoint_cpu_draw=item_slide_cpu_draw;
                use_midpoint_cpu_draw=true;
            }
            if(midpoint_ui && file_select_cpu_draw.count) {
                midpoint_cpu_draw=file_select_cpu_draw;
                use_midpoint_cpu_draw=true;
            }
            if(midpoint_ui && inventory_cpu_draw.count) {
                midpoint_cpu_draw=inventory_cpu_draw;
                use_midpoint_cpu_draw=true;
            }
            if(midpoint_ui && additive && glare_cpu_draw.count && midpoint_frame_interpolated) {
                midpoint_cpu_draw=glare_cpu_draw;use_midpoint_cpu_draw=true;
                glare_alpha_override=glare_half_alpha;
            }
            fs_data_dirty=true; UploadUniforms(accelerate);
            pipeline_cache.UseFragmentShader(regs,user_config,match.atlas_selection || (midpoint_ui && use_pause));
            const unsigned w = std::max<unsigned>(draw_rect.right, viewport.x + viewport.width);
            const unsigned h = std::max<unsigned>(draw_rect.top, viewport.y + viewport.height);
            const bool title_depth=midpoint_ui && midpoint_title_logo;
            const auto depth_compare=regs.framebuffer.output_merger.depth_test_func.Value();
            using CF=Pica::FramebufferRegs::CompareFunc;
            const bool reverse_depth=depth_compare==CF::GreaterThan || depth_compare==CF::GreaterThanOrEqual;
            const bool reset_title_depth=title_depth && midpoint_title_depth_frame!=MM3D::guest_frames.load();
            if (prompt_layer->Begin(w, h,reverse_depth?0.f:1.f,reset_title_depth)) {
                if(title_depth)midpoint_title_depth_frame=MM3D::guest_frames.load();
                const auto saved = pipeline_info;
                pipeline_info.dynamic_info.viewport = {viewport.x,viewport.y,
                    viewport.x+viewport.width,viewport.y+viewport.height};
                pipeline_info.dynamic_info.scissor = draw_rect;
                pipeline_info.state.attachments.color = VideoCore::PixelFormat::RGBA8;
                pipeline_info.state.attachments.depth = midpoint_ui?VideoCore::PixelFormat::D24S8:VideoCore::PixelFormat::Invalid;
                if(!stencil_scene && !title_depth)pipeline_info.state.depth_stencil.value = 0;
                auto& copy_blend = pipeline_info.state.blending;
                if(!stencil_scene)copy_blend.color_write_mask = 15;
                // Alpha stores accumulated coverage, not brightness. An additive
                // flare contributes RGB but preserves prior coverage. Later HUD
                // or bars still attenuate it in the game's submission order.
                if(!stencil_scene) {
                    copy_blend.src_alpha_blend_factor.Assign(additive?BF::Zero:BF::One);
                    copy_blend.dst_alpha_blend_factor.Assign(additive?BF::One:BF::OneMinusSourceAlpha);
                    copy_blend.alpha_blend_eq.Assign(BE::Add);
                }
                // Block on this small diagnostic pipeline rather than publishing a skipped draw.
                pipeline_cache.BindPipeline(pipeline_info, true);
                const auto native_position=binding_offsets[0];
                if(additive && glare_vertex_offset && midpoint_frame_interpolated &&
                   !MM3D::CameraMidpoint::native_preview.load())binding_offsets[0]=*glare_vertex_offset;
                const bool replay_submitted=draw_batch();
                if(midpoint_ui && replay_submitted && message_half &&
                   (!cpu_interpolation || (use_midpoint_cpu_draw && message_cpu_draw.count)))
                    layout_trace.flags|=1024;
                if(midpoint_ui && replay_submitted && inventory_half &&
                   (!cpu_interpolation || (use_midpoint_cpu_draw && inventory_cpu_draw.count))) {
                    layout_trace.flags|=16;
                    if(MM3D::CameraMidpoint::capture_menu_pair_at_us.load()) {
                        for(unsigned r=11;r<10+4*(regs.pipeline.num_vertices/6);r+=4) {
                            if(inventory_half->rows[r] && std::abs(inventory_half->values[r][0]-
                                pica.vs_setup.uniforms.f[r][0].ToFloat32())>1.f) {
                                MM3D::CameraMidpoint::capture_menu_motion_frame=MM3D::guest_frames.load();
                                break;
                            }
                        }
                    }
                }
                // Menus can establish the alternate frame without a moving
                // world camera. Keep the frozen scene alive on UI-only pages.
                if(midpoint_ui && inventory_owner) {
                    midpoint_generation=MM3D::CameraMidpoint::generation.load();
                    midpoint_last_draw_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                }
                if(midpoint_ui && ((file_select_half && (!cpu_interpolation || file_select_cpu_draw.count)) ||
                    inventory_half || (inventory_owner && message_half && (!cpu_interpolation || message_cpu_draw.count))))
                    midpoint_frame_interpolated=true;
                binding_offsets[0]=native_position;
                prompt_layer->x = viewport.x; prompt_layer->y = viewport.y;
                prompt_layer->viewport_width = viewport.width;
                prompt_layer->viewport_height = viewport.height;
                prompt_layer->End(!midpoint_ui);
                pipeline_info = saved;
                if(minimap) ++MM3D::Minimap::draws;
                MM3D::prompt_replays.fetch_add(1);
            } else {
                MM3D::prompt_unsupported.fetch_add(1);
                if(midpoint_ui)reject_midpoint_hud(4);
            }
        } else {
            MM3D::prompt_unsupported.fetch_add(1);
            if(midpoint_ui)reject_midpoint_hud(5);
        }
        use_midpoint_cpu_draw = false;
        if(glare_alpha_override) {glare_alpha_override.reset();fs_data_dirty=true;UploadUniforms(accelerate);}
        if (message_override || item_slide_override || file_select_override || inventory_override) {
            if(inventory_override)fs_data_dirty=true;
            inventory_override.reset();
            if(file_select_override&&file_select_override->alpha)fs_data_dirty=true;
            file_select_override.reset();
            item_slide_override.reset();
            message_override.reset();
            pica.vs_setup.uniforms_dirty = true;
            UploadUniforms(accelerate);
        }
    }
    if(trace_layout)MM3D::EffectCapture::ObserveLayoutReplay(layout_trace);
#endif
    vertex_batch.clear();
    return succeeded;
}

void RasterizerVulkan::SyncTextureUnits(const Framebuffer* framebuffer) {
    using TextureType = Pica::TexturingRegs::TextureConfig::TextureType;
#ifdef ENABLE_OOT3D_PRESENTATION
    oot_number_backdrop.reset();
    // Exact native indicator ownership, also published with interpolation off.
    // Other sprites sharing its atlas retain their original sampling.
    bool smooth_indicator=false;
    namespace TI=OOT3D::TextboxInterpolation;
    if(!regs.framebuffer.output_merger.depth_test_enable &&
       TI::Active(OOT3D::Interpolation::guest_frames.load(),OOT3D::Interpolation::generation.load())) {
        if(const auto hash=OotUIMeshHash(4,true)) {
            TI::Matrix matrix{};
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                matrix[r][c]=pica.vs_setup.uniforms.f[TI::ModelViewRow+r][c].ToFloat32();
            if(const auto route=TI::Find(hash->first,hash->second,matrix,
                OOT3D::Interpolation::guest_frames.load(),OOT3D::Interpolation::generation.load()))
                smooth_indicator=TI::FilteredArrow(route->source[3]);
        }
    }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    pause_draw={};
#endif
    const auto pica_textures = regs.texturing.GetTextures();
    const bool use_cube_heap =
        pica_textures[0].enabled && pica_textures[0].config.type == TextureType::ShadowCube;
    const auto texture_set = pipeline_cache.Acquire(use_cube_heap ? DescriptorHeapType::Texture
                                                                  : DescriptorHeapType::Texture);
#ifdef ENABLE_OOT3D_PRESENTATION
    oot_native_texture_set=texture_set;
#endif

    for (u32 texture_index = 0; texture_index < pica_textures.size(); ++texture_index) {
        const auto& texture = pica_textures[texture_index];

        // If the texture unit is disabled bind a null surface to it
        if (!texture.enabled) {
            switch (texture.config.type.Value()) {
            case TextureType::TextureCube:
            case TextureType::ShadowCube: {
                Surface& null_surface = res_cache.GetSurface(VideoCore::NULL_SURFACE_CUBE_ID);
                const Sampler& null_sampler = res_cache.GetSampler(VideoCore::NULL_SURFACE_CUBE_ID);
                update_queue.AddImageSampler(texture_set, texture_index, 0,
                                             null_surface.ImageView(), null_sampler.Handle());
                break;
            }
            default: {
                Surface& null_surface = res_cache.GetSurface(VideoCore::NULL_SURFACE_ID);
                const Sampler& null_sampler = res_cache.GetSampler(VideoCore::NULL_SURFACE_ID);
                update_queue.AddImageSampler(texture_set, texture_index, 0,
                                             null_surface.ImageView(), null_sampler.Handle());
                break;
            }
            }
            continue;
        }

        // Handle special tex0 configurations
        if (texture_index == 0) {
            switch (texture.config.type.Value()) {
            case TextureType::Shadow2D: {
                Surface& surface = res_cache.GetTextureSurface(texture);
                Sampler& sampler = res_cache.GetSampler(texture.config);
                surface.flags |= VideoCore::SurfaceFlagBits::ShadowSource;
                update_queue.AddImageSampler(texture_set, texture_index, 0, surface.StorageView(),
                                             sampler.Handle());
                continue;
            }
            case TextureType::ShadowCube: {
                BindShadowCube(texture, texture_set);
                continue;
            }
            case TextureType::TextureCube: {
                BindTextureCube(texture, texture_set);
                continue;
            }
            default:
                break;
            }
        }

        // Bind the texture provided by the rasterizer cache
        Surface& surface = res_cache.GetTextureSurface(texture);
        auto sampler_config=texture.config;
#ifdef ENABLE_OOT3D_PRESENTATION
        if(smooth_indicator) {
            using Filter=Pica::TexturingRegs::TextureConfig::TextureFilter;
            sampler_config.mag_filter.Assign(Filter::Linear);
            sampler_config.min_filter.Assign(Filter::Linear);
        }
#endif
        Sampler& sampler = res_cache.GetSampler(sampler_config
#ifdef ENABLE_OOT3D_PRESENTATION
            , smooth_indicator
#endif
        );
        const vk::ImageView color_view = framebuffer->ImageView(SurfaceType::Color);
        const bool is_feedback_loop = color_view == surface.FramebufferView();
        vk::ImageView texture_view =
            is_feedback_loop ? surface.CopyImageView() : surface.ImageView();
#ifdef ENABLE_OOT3D_PRESENTATION
        const OOT3D::HDFont::Descriptor font_descriptor{texture.config.GetPhysicalAddress(),
            texture.config.width, texture.config.height, unsigned(texture.format)};
        if (!is_feedback_loop && !surface.IsCustom() && surface.levels == 1 &&
            texture.config.type == TextureType::Texture2D) {
            const auto expected = OOT3D::HDFont::Find(font_descriptor);
            const bool custom_text = OOT3D::HDFont::TextAtlasCandidate(
                font_descriptor.width, font_descriptor.height, font_descriptor.format);
            const auto number_layout=OOT3D::HUD::GetEffectiveLayout();
            const bool boss_timer = font_descriptor.width==512 && font_descriptor.height==256 &&
                font_descriptor.format==13 && number_layout.hd_numbers && number_layout.hd_timer_numbers;
            std::shared_ptr<const OOT3D::HDFont::RupeeAtlas> rupees;
            bool ammo_backdrop=false,rupee_backdrop=false,timer_backdrop=false;
            if(number_layout.hd_numbers && font_descriptor.width==256 && font_descriptor.height==128 && font_descriptor.format==4) {
                const auto& layout=number_layout;
                if(layout.hd_rupee_numbers) {
                    bool counter=false;
                    const auto mesh=OotHudMesh(&counter);
                    if(mesh && mesh->second && counter) {
                        rupee_backdrop=texture_index==0 && OOT3D::AmmoBackdrop::Enabled(layout,true);
                        rupees=OOT3D::HDFont::StyledRupeeAtlas(layout.hd_rupee_shadow,false,rupee_backdrop,layout.hd_numbers_bold);
                    }
                }
                if(!rupees && (layout.hd_timer_numbers || layout.hd_ammo_numbers || layout.hd_score_numbers || layout.hd_skulltula_numbers)) {
                    const auto mesh=OotUIMeshHash(32);
                    if(mesh) {
                        if(layout.hd_timer_numbers && OOT3D::HUD::IsTimerMesh(mesh->first,mesh->second/4)) {
                            timer_backdrop=texture_index==0 && OOT3D::AmmoBackdrop::TimerEnabled(layout);
                            rupees=OOT3D::HDFont::StyledRupeeAtlas(layout.hd_timer_shadow,true,timer_backdrop,layout.hd_numbers_bold);
                        }
                        if(const auto kind=OOT3D::HUD::FindNumberMesh(mesh->first,mesh->second/4))
                            if((*kind==OOT3D::HUD::NumberKind::Ammo && layout.hd_ammo_numbers) ||
                               (*kind==OOT3D::HUD::NumberKind::Score && layout.hd_score_numbers) ||
                               (*kind==OOT3D::HUD::NumberKind::Skulltula && layout.hd_skulltula_numbers))
                            {
                                ammo_backdrop=texture_index==0 && *kind==OOT3D::HUD::NumberKind::Ammo &&
                                    OOT3D::AmmoBackdrop::Enabled(layout);
                                rupees=OOT3D::HDFont::StyledRupeeAtlas(layout.hd_rupee_shadow,false,ammo_backdrop,layout.hd_numbers_bold);
                            }
                    }
                }
            }
            if (expected || custom_text || rupees || boss_timer) {
                const auto ref = memory.GetPhysicalRef(font_descriptor.address);
                const auto size = boss_timer ? 512*256 : rupees ? 256*128*2 : custom_text ? font_descriptor.width * font_descriptor.height * 4 :
                    OOT3D::HDFont::ByteSize(font_descriptor);
                if (ref && ref.GetSize() >= size) {
                    const auto bytes = ref.GetReadBytes<u8>(size);
                    if (surface.oot_font_hash_tick != surface.ModificationTick() ||
                        surface.oot_font_address != font_descriptor.address || surface.oot_font_size != size) {
                        surface.oot_font_hash = Common::CityHash64(
                            reinterpret_cast<const char*>(bytes.data()), bytes.size());
                        surface.oot_font_hash_tick = surface.ModificationTick();
                        surface.oot_font_address = font_descriptor.address;
                        surface.oot_font_size = size;
                    }
                    const auto generated = custom_text ? OOT3D::HDFont::FindTextAtlas(
                        font_descriptor.width, font_descriptor.height, font_descriptor.format,
                        surface.oot_font_hash) : nullptr;
                    if(rupees && rupees->hash!=surface.oot_font_hash)rupees.reset();
                    if(boss_timer)rupees=OOT3D::HDFont::StyledBossTimerAtlas(
                        surface.oot_font_hash,number_layout.hd_timer_shadow,number_layout.hd_numbers_bold);
                    if ((expected && *expected == surface.oot_font_hash) || generated || rupees) {
                        if (const auto view = hd_fonts.Get(runtime, font_descriptor, surface.oot_font_hash,
                            bytes, rupees ? std::span<const u8>(rupees->rgba) :
                                generated ? std::span<const u8>(*generated) : std::span<const u8>{},rupees?rupees->variant:0)) {
                            texture_view = view;
                            if(rupees && (ammo_backdrop || rupee_backdrop || timer_backdrop))
                                oot_number_backdrop=timer_backdrop?number_layout.hd_timer_backdrop:
                                    rupee_backdrop?number_layout.hd_rupee_backdrop:number_layout.hd_ammo_backdrop;
                            ++OOT3D::HDFont::replacements;
                        }
                    }
                }
            }
        }
        const auto oot_key = OOT3D::Textures::Key(texture.config.GetPhysicalAddress(),
            texture.config.width, texture.config.height, unsigned(texture.format));
        OOT3D::Textures::hide_current_draw |= OOT3D::Textures::Matches(oot_key);
        if (OOT3D::Textures::RecordingKey(oot_key)) {
            const u64 size = u64(texture.config.width) * texture.config.height * surface.GetFormatBpp() / 8;
            std::span<const u8> bytes;
            if (!is_feedback_loop && size <= 16 * 1024 * 1024 && OOT3D::Textures::NeedsRecordingHash(oot_key)) {
                const auto ref = memory.GetPhysicalRef(texture.config.GetPhysicalAddress());
                if (ref.GetSize() >= size) bytes = ref.GetReadBytes<u8>(size);
            }
            OOT3D::Textures::RecordBinding(oot_key, bytes, fmt::format(
                "size={}x{} format={} unit={} shader={:016x} swizzle={:016x} vertices={} feedback={}",
                unsigned(texture.config.width), unsigned(texture.config.height), unsigned(texture.format), texture_index,
                pica.vs_setup.GetProgramCodeHash(), pica.vs_setup.GetSwizzleDataHash(),
                regs.pipeline.num_vertices, is_feedback_loop));
        }
        if (!is_feedback_loop && surface.Aspect() == vk::ImageAspectFlagBits::eColor) {
            const bool detail = OOT3D::Textures::WantsDetail(oot_key);
            if (detail || OOT3D::Textures::Wants(oot_key)) {
                renderpass_cache.EndRendering();
                OOT3D::Textures::Capture(instance, scheduler, surface.Image(Type::Base),
                    instance.GetTraits(surface.pixel_format).native, surface.GetWidth(), surface.GetHeight(),
                    oot_key, vk::ImageLayout::eGeneral, detail);
            }
        }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
        if (MM3D::BloomSampling::enabled.load() && surface.GetResScale() > 1 &&
            !surface.IsCustom() && texture.config.type == TextureType::Texture2D &&
            MM3D::BloomSampling::Matches({pica.vs_setup.GetProgramCodeHash(),
                pica.vs_setup.GetSwizzleDataHash(), unsigned(regs.pipeline.num_vertices),
                texture.config.width, texture.config.height, unsigned(texture.format),
                u32(regs.rasterizer.viewport_size_x), u32(regs.rasterizer.viewport_size_y),
                regs.framebuffer.output_merger.alphablend_enable != 0, is_feedback_loop})) {
            const auto native_view = surface.NativeSampleView();
            if (native_view != texture_view) {
                texture_view = native_view;
                MM3D::BloomSampling::bindings.fetch_add(1);
            }
        }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
        if(MM3D::PauseCapture::enabled.load()&&!is_feedback_loop&&texture.config.width==512&&
           texture.config.height==256&&texture.format==Pica::TexturingRegs::TextureFormat::RGB565&&
           texture.config.type==TextureType::Texture2D) {
            for(unsigned eye=0;eye<2;++eye) {
                if(!pause_textures[eye]||pause_textures[eye]!=texture.config.GetPhysicalAddress())continue;
                const auto& image=pause_images[eye]?pause_images[eye]:pause_images[0];
                if(image){texture_view=image->ImageView();pause_draw[texture_index]=int(pause_orientation[eye]);}
                break;
            }
        }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
        const auto texture_key = MM3D::Textures::Key(texture.config.GetPhysicalAddress(),
            texture.config.width, texture.config.height, unsigned(texture.format));
#ifndef ENABLE_MM3D_GAME_FOLDER
        // Unlike content identities, this session-only probe follows animated buffers.
        // Include feedback reads; skip their consumer draw without modifying the image.
        if (MM3D::Textures::hide_by_address.load() && MM3D::Textures::Matches(texture_key))
            MM3D::Textures::hide_current_draw = true;
#endif
        const u32 match_width = texture.config.width;
        const u32 match_height = texture.config.height;
        if (!is_feedback_loop && !surface.IsCustom() &&
            (MM3D::Minimap::Candidate(match_width,match_height) || MM3D::Textures::WantsDetail(texture_key) || MM3D::Profiles::Candidate(match_width, match_height, unsigned(texture.format)) ||
             MM3D::Notebook::ExclusionCandidate(match_width, match_height, unsigned(texture.format)))) {
            const u64 size = u64(match_width) * match_height * surface.GetFormatBpp() / 8;
            if (match_width % 8 == 0 && match_height % 8 == 0 && size > 0 && size <= 4 * 1024 * 1024) {
                if (surface.mm3d_hash_tick != surface.ModificationTick() ||
                    surface.mm3d_hash_key != texture_key) {
                    renderpass_cache.EndRendering();
                    MM3D::InterpolationPerformance::ReadbackScope readback{
                        "presentation texture identity",texture.config.GetPhysicalAddress(),size};
                    res_cache.FlushRegion(texture.config.GetPhysicalAddress(), static_cast<u32>(size));
                    scheduler.Finish();
                    const auto ref = memory.GetPhysicalRef(texture.config.GetPhysicalAddress());
                    surface.mm3d_hash = ref && ref.GetSize() >= size
                        ? MM3D::Textures::ContentHash(ref.GetReadBytes<u8>(size)) : std::string{};
                    surface.mm3d_hash_tick = surface.ModificationTick();
                    surface.mm3d_hash_key = texture_key;
                }
                const auto map_viewport=regs.rasterizer.GetViewportRect();
                if(MM3D::Minimap::Candidate(match_width,match_height) &&
                   map_viewport.GetWidth()==240 && map_viewport.GetHeight()==320 &&
                   surface.mm3d_hash==MM3D::Minimap::RadarHash) {
                    MM3D::Minimap::marker_draw=true;
                    MM3D::Minimap::marker_seen=true;
                }
                const auto context = fmt::format(
                    "unit={} shader={:016x} swizzle={:016x} vertices={} blend={} viewport_raw={},{}",
                    texture_index, pica.vs_setup.GetProgramCodeHash(), pica.vs_setup.GetSwizzleDataHash(),
                    regs.pipeline.num_vertices, unsigned(regs.framebuffer.output_merger.alphablend_enable),
                    u32(regs.rasterizer.viewport_size_x), u32(regs.rasterizer.viewport_size_y));
                const MM3D::Profiles::Rule identity{match_width,match_height,unsigned(texture.format),surface.mm3d_hash,context};
                MM3D::Profiles::Observe(texture_key,identity);
                std::array<float,7> fade_alpha{};
                const auto fade_stages=regs.texturing.GetTevStages();
                for(size_t stage=0;stage<fade_stages.size();++stage)
                    fade_alpha[stage]=float(fade_stages[stage].const_a.Value())/255.f;
                fade_alpha[6]=float((regs.framebuffer.output_merger.blend_const.raw>>24)&255)/255.f;
                MM3D::Profiles::ObserveFade(identity,fade_alpha);
                panel_excluded |= MM3D::Notebook::Excluded(identity);
                panel_draw_identities.push_back(identity);
                if(!surface.mm3d_hash.empty() && MM3D::Textures::WantsDetail(texture_key,surface.mm3d_hash)) {
                    renderpass_cache.EndRendering();
                    MM3D::Textures::Capture(instance,scheduler,surface.Image(Type::Base),
                        instance.GetTraits(surface.pixel_format).native,surface.GetWidth(),surface.GetHeight(),
                        texture_key,vk::ImageLayout::eGeneral,true);
                }
                MM3D::Textures::hide_current_draw |= MM3D::Profiles::Hidden(identity);
                for (const auto& match : MM3D::Profiles::Match(identity))
                    if (std::none_of(mm3d_draw_profiles.begin(),mm3d_draw_profiles.end(),
                        [&](const auto& p){return p.id==match.id;})) {
                        mm3d_draw_profiles.push_back(match);
                        if (match.presentation) MM3D::prompt_matches.fetch_add(1);
                    }
            }
        }
        if (MM3D::Textures::RecordingKey(texture_key)) {
            std::span<const u8> bytes;
            const u32 width = texture.config.width;
            const u32 height = texture.config.height;
            const u64 size = u64(width) * height * surface.GetFormatBpp() / 8;
            if (MM3D::Textures::NeedsRecordingHash(texture_key) && !is_feedback_loop &&
                width % 8 == 0 && height % 8 == 0 && size > 0 && size <= 4 * 1024 * 1024) {
                MM3D::InterpolationPerformance::ReadbackScope readback{
                    "diagnostic texture recording",texture.config.GetPhysicalAddress(),size};
                // Read current guest encoded bytes, not a scaled/replaced GPU thumbnail.
                renderpass_cache.EndRendering();
                res_cache.FlushRegion(texture.config.GetPhysicalAddress(), static_cast<u32>(size));
                scheduler.Finish();
                const auto ref = memory.GetPhysicalRef(texture.config.GetPhysicalAddress());
                if (ref && ref.GetSize() >= size) bytes = ref.GetReadBytes<u8>(size);
            }
            const auto& fb = regs.framebuffer.framebuffer;
            MM3D::Textures::RecordBinding(texture_key, bytes, fmt::format(
                "size={}x{} format={} unit={} shader={:016x} swizzle={:016x} vertices={} "
                "target={:08x} depth={:08x} blend={} viewport_raw={},{} feedback={} "
                "surface={}x{} scale={} address_hide={}",
                width, height, unsigned(texture.format), texture_index,
                pica.vs_setup.GetProgramCodeHash(), pica.vs_setup.GetSwizzleDataHash(),
                regs.pipeline.num_vertices, fb.GetColorBufferPhysicalAddress(), fb.GetDepthBufferPhysicalAddress(),
                unsigned(regs.framebuffer.output_merger.alphablend_enable),
                u32(regs.rasterizer.viewport_size_x), u32(regs.rasterizer.viewport_size_y), is_feedback_loop,
                surface.GetWidth(), surface.GetHeight(), surface.GetResScale(),
                MM3D::Textures::hide_by_address.load() && MM3D::Textures::Matches(texture_key)));
        }
        if (!is_feedback_loop && surface.Aspect() == vk::ImageAspectFlagBits::eColor &&
            MM3D::Textures::Wants(texture_key)) {
            renderpass_cache.EndRendering();
            MM3D::Textures::Capture(instance, scheduler, surface.Image(Type::Base),
                instance.GetTraits(surface.pixel_format).native,
                surface.GetWidth(), surface.GetHeight(), texture_key);
        }
#endif
        update_queue.AddImageSampler(texture_set, texture_index, 0, texture_view, sampler.Handle());
    }
}

void RasterizerVulkan::SyncUtilityTextures(const Framebuffer* framebuffer) {
    const bool shadow_writing = regs.framebuffer.IsShadowRendering();
    const bool shadow_reading = regs.lighting.config0.enable_shadow;
    const auto utility_set = pipeline_cache.Acquire(DescriptorHeapType::Utility);

    // Reading and writing are mutually exclusive
    assert(!(shadow_writing && shadow_reading));

    if (shadow_writing) {
        update_queue.AddStorageImage(utility_set, 0, framebuffer->ImageView(SurfaceType::Color));
    } else if (shadow_reading) {
        const u32 shadow_texture_unit = regs.lighting.config0.shadow_selector.Value();
        const auto shadow_texture = regs.texturing.GetTextures()[shadow_texture_unit];
        Surface& shadow_surface = res_cache.GetTextureSurface(shadow_texture);
        update_queue.AddStorageImage(utility_set, 0, shadow_surface.StorageView());
    } else {
        Surface& null_surface = res_cache.GetSurface(VideoCore::NULL_SURFACE_ID);
        update_queue.AddStorageImage(utility_set, 0, null_surface.StorageView());
    }
}

void RasterizerVulkan::BindShadowCube(const Pica::TexturingRegs::FullTextureConfig& texture,
                                      vk::DescriptorSet texture_set) {
    using CubeFace = Pica::TexturingRegs::CubeFace;
    auto info = Pica::Texture::TextureInfo::FromPicaRegister(texture.config, texture.format);
    constexpr std::array faces = {
        CubeFace::PositiveX, CubeFace::NegativeX, CubeFace::PositiveY,
        CubeFace::NegativeY, CubeFace::PositiveZ, CubeFace::NegativeZ,
    };

    Sampler& sampler = res_cache.GetSampler(texture.config);

    for (CubeFace face : faces) {
        const u32 binding = static_cast<u32>(face);
        info.physical_address = regs.texturing.GetCubePhysicalAddress(face);

        const VideoCore::SurfaceId surface_id = res_cache.GetTextureSurface(info);
        Surface& surface = res_cache.GetSurface(surface_id);
        surface.flags |= VideoCore::SurfaceFlagBits::ShadowSource;
        update_queue.AddImageSampler(texture_set, 0, binding, surface.StorageView(),
                                     sampler.Handle());
    }
}

void RasterizerVulkan::BindTextureCube(const Pica::TexturingRegs::FullTextureConfig& texture,
                                       vk::DescriptorSet texture_set) {
    using CubeFace = Pica::TexturingRegs::CubeFace;
    const VideoCore::TextureCubeConfig config = {
        .px = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveX),
        .nx = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeX),
        .py = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveY),
        .ny = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeY),
        .pz = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveZ),
        .nz = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeZ),
        .width = texture.config.width,
        .levels = texture.config.lod.max_level + 1,
        .format = texture.format,
    };

    Surface& surface = res_cache.GetTextureCube(config);
    Sampler& sampler = res_cache.GetSampler(texture.config);
    update_queue.AddImageSampler(texture_set, 0, 0, surface.ImageView(), sampler.Handle());
}

#ifdef ENABLE_OOT3D_PRESENTATION
std::optional<OOT3D::SunVisibility::Sample> RasterizerVulkan::ReadSunDepth(
    u32 address, const OOT3D::SunVisibility::Context& context) {
    // Only inspect existing GPU-owned depth. Clean memory uses the native read;
    // no cache creation, guest-memory flush, or historical writeback is allowed.
    auto* surface = res_cache.GetDirtySurface(address, 4);
    if (!surface) return {};
    if (!oot_sun_depth) oot_sun_depth = std::make_unique<VisibilityDepthCache>(instance, scheduler);
    bool pending{};
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto value = oot_sun_depth->Read(runtime, *surface, address, context, now, true, &pending);
    if (!value) return {};
    return OOT3D::SunVisibility::Sample{*value, pending};
}
void RasterizerVulkan::ResetSunDepth() {
    if (oot_sun_depth) oot_sun_depth->Reset();
}
#endif

#ifdef ENABLE_MM3D_PRESENTATION
void RasterizerVulkan::EndGuestFrame() {
    if (!visibility_depth_refresh_enabled) return;
    if (!visibility_depth || !visibility_depth->SourceAddress()) return;
    if (!visibility_depth_enabled || !MM3D::CameraMidpoint::enabled.load()) {
        ResetVisibilityDepth();
        return;
    }
    MM3D::InterpolationPerformance::Scope timing{
        MM3D::InterpolationPerformance::Part::VisibilityRefresh};
    struct GuestMemory {
        Memory::MemorySystem& memory;
        std::optional<u32> Read(u32 at) { return memory.Read32OrNullopt(at); }
    } guest{memory};
    const auto context = MM3D::VisibilityDepth::LiveContext(
        guest, MM3D::guest_frames.load(), MM3D::CameraMidpoint::generation.load());
    if (!context || !MM3D::VisibilityDepth::CodeMatches(guest)) {
        ResetVisibilityDepth();
        return;
    }
    // Inspect only the existing GPU-owned source. Never load/create a surface,
    // clean guest memory, or retain a raw surface pointer between guest frames.
    auto* surface = res_cache.GetDirtySurface(*visibility_depth->SourceAddress(), 4);
    if (!surface) return;
    visibility_depth->Refresh(runtime, *surface, *context, MM3D::InterpolationPerformance::Now());
}

std::optional<u32> RasterizerVulkan::ReadVisibilityDepth(u32 address, const MM3D::VisibilityDepth::Context& context) {
    if (!visibility_depth_enabled || !MM3D::CameraMidpoint::enabled.load()) {
        ResetVisibilityDepth();
        return {};
    }
    auto* surface = res_cache.GetDirtySurface(address, 4);
    if (!surface) { // Clean/CPU-owned memory uses the exact ordinary read.
        MM3D::InterpolationPerformance::RecordVisibilityEvent(
            MM3D::InterpolationPerformance::VisibilityEvent::CleanMemory);
        return {};
    }
    if (!visibility_depth) visibility_depth = std::make_unique<VisibilityDepthCache>(instance, scheduler);
    return visibility_depth->Read(runtime, *surface, address, context, MM3D::InterpolationPerformance::Now());
}
void RasterizerVulkan::ResetVisibilityDepth() {
    if (visibility_depth) visibility_depth->Reset();
}
void RasterizerVulkan::ResetPause() {
    pause_textures={};pause_orientation={};pause_draw={};
    MM3D::PauseCapture::captured_scale=0;MM3D::PauseCapture::bound_textures=0;
}
void RasterizerVulkan::CapturePause(u32 address,unsigned eye) {
    if(eye>1)return;
    if(eye==0) {
        ResetPause();
        // Retire the previous frozen images only after all queued draws finish.
        runtime.Finish();pause_images={};
    }
    VideoCore::SurfaceParams params;
    params.addr=address;params.width=params.stride=240;params.height=400;
    params.pixel_format=VideoCore::PixelFormat::RGB8;params.is_tiled=false;params.UpdateParams();
    const auto [id,rect]=res_cache.GetSurfaceSubRect(params,VideoCore::ScaleMatch::Ignore,true);
    if(!id)return;
    Surface& source=res_cache.GetSurface(id);
    const auto scale=source.GetResScale();
    if(scale<=1||source.IsCustom()||rect.GetWidth()!=240*scale||rect.GetHeight()!=400*scale)return;
    params.addr=0;params.res_scale=scale;params.UpdateParams();
    auto image=std::make_unique<Surface>(runtime,params);
    const VideoCore::TextureCopy copy{
        .src_level=source.LevelOf(address),.dst_level=0,
        .src_offset={rect.left,rect.bottom},.dst_offset={0,0},
        .extent={240*scale,400*scale},
    };
    if(!runtime.CopyTextures(source,*image,copy)){runtime.Finish();return;}
    pause_images[eye]=std::move(image);MM3D::PauseCapture::captured_scale=scale;
}
void RasterizerVulkan::BindPause(u32 texture,unsigned eye,const MM3D::PauseCapture::Probes& probes) {
    if(eye>1||!(pause_images[eye]||pause_images[0]))return;
    const auto data=memory.GetPhysicalRef(texture);
    if(!data||data.GetSize()<0x40000)return;
    const auto bytes=data.GetReadBytes<u8>(0x40000);
    const auto orientation=MM3D::PauseCapture::Orientation([&](u32 off)->std::optional<u32>{
        return u32(bytes[off])|(u32(bytes[off+1])<<8);
    },probes);
    if(!orientation)return;
    pause_textures[eye]=texture;pause_orientation[eye]=orientation;
    MM3D::PauseCapture::bound_textures=unsigned(pause_textures[0]!=0)+unsigned(pause_textures[1]!=0);
}
#endif

void RasterizerVulkan::FlushAll() {
#ifdef ENABLE_OOT3D_PRESENTATION
    ResetSunDepth();
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    ResetVisibilityDepth();
    MM3D::InterpolationPerformance::ReadbackScope readback{"flush all"};
#endif
    res_cache.FlushAll();
}

void RasterizerVulkan::FlushRegion(PAddr addr, u32 size) {
#ifdef ENABLE_MM3D_PRESENTATION
    MM3D::InterpolationPerformance::ReadbackScope readback{"external memory read",addr,size};
#endif
    res_cache.FlushRegion(addr, size);
}

void RasterizerVulkan::InvalidateRegion(PAddr addr, u32 size) {
#ifdef ENABLE_OOT3D_PRESENTATION
    if (oot_sun_depth) oot_sun_depth->Invalidate(addr, size);
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    midpoint_scene_copies.InvalidatePadded(addr,u64(addr)+size);
    midpoint_color_clears.Invalidate(addr,u64(addr)+size);
    if (visibility_depth) visibility_depth->Invalidate(addr, size);
    MM3D::InterpolationPerformance::ReadbackScope readback{"memory invalidation",addr,size};
#endif
    res_cache.InvalidateRegion(addr, size);
}

void RasterizerVulkan::FlushAndInvalidateRegion(PAddr addr, u32 size) {
#ifdef ENABLE_OOT3D_PRESENTATION
    if (oot_sun_depth) oot_sun_depth->Invalidate(addr, size);
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    midpoint_scene_copies.InvalidatePadded(addr,u64(addr)+size);
    midpoint_color_clears.Invalidate(addr,u64(addr)+size);
    if (visibility_depth) visibility_depth->Invalidate(addr, size);
    MM3D::InterpolationPerformance::ReadbackScope readback{"external memory read/write",addr,size};
#endif
    res_cache.FlushRegion(addr, size);
    res_cache.InvalidateRegion(addr, size);
}

void RasterizerVulkan::ClearAll(bool flush) {
#ifdef ENABLE_OOT3D_PRESENTATION
    ResetSunDepth();
    ++OOT3D::Interpolation::generation;
    oot_replay_frame.Reset();oot_rate_clock.Reset();
    for(auto& slot:oot_replay_slots)slot.Reset();
    oot_color_clears.Reset();OOT3D::Interpolation::Pose::Reset();
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    midpoint_scene_copies={};
    midpoint_color_clears.Reset();
    ResetVisibilityDepth();
    ResetPause();
    midpoint_history.Reset();world_map_history.Reset();glare_cpu_history.Reset();forest_particle_history.Reset();snow_history.Reset();fire_impact_history.Reset();leaf_particle_history.Reset();moth_history.Reset();message_history.Reset();message_override.reset();item_slide_history.Reset();item_slide_override.reset();file_select_history.Reset();file_select_override.reset();inventory_history.Reset();banner_history.Reset();magic_meter_history.Reset();heart_history.Reset();lens_history.Reset();letterbox_history.Reset();inventory_override.reset();torch_history.Reset();water_particle_history.Reset();water_model_history.Reset();moving_effect_history.Reset();splash_cpu_history.Reset();burst_particle_history.Reset();spark_history.Reset();firework_history.Reset();chest_particle_history.Reset();sword_trail_history.Reset();shockwave_history.Reset();bubble_history.Reset();link_history.Reset();song_history.Reset();song_overrides.reset();link_frame.reset();link_overrides={};
    midpoint_scene_clear.Reset();
    midpoint_target_frame.reset();midpoint_world_frame.reset();midpoint_opaque_world_frame.reset();midpoint_stencil_frame.reset();midpoint_fade_frame.reset();midpoint_title_depth_frame.reset();
    for(const auto id : {"@camera_midpoint","@camera_midpoint_fade","@camera_midpoint_hud"})
        if(const auto it=prompt_layers.find(id);it!=prompt_layers.end())it->second->NextFrame();
#endif
    res_cache.ClearAll(flush);
}

bool RasterizerVulkan::AccelerateDisplayTransfer(const Pica::DisplayTransferConfig& config) {
    const bool result=res_cache.AccelerateDisplayTransfer(config);
#ifdef ENABLE_MM3D_PRESENTATION
    if(MM3D::ScreenShrink::native_normalized.load() &&
       MM3D::ScreenShrink::presentation_scale.load()<1.f) {
        const auto now=MM3D::InputLatency::Now();
        auto& stamp=midpoint_transfer_logged[0];
        if(now-stamp>=1000000) {
            stamp=now;
            LOG_INFO(Render_Vulkan,"Interpolation shrink DisplayTransfer: success={} input={:08x} output={:08x} frame={} source_frame={} source={:08x}",
                result,config.GetPhysicalInputAddress(),config.GetPhysicalOutputAddress(),
                MM3D::guest_frames.load(),midpoint_scene_copies.frame,midpoint_scene_copies.addresses[0]);
        }
    }
    midpoint_scene_copies.Transfer(MM3D::guest_frames.load(),
        result?config.GetPhysicalInputAddress():0,config.GetPhysicalOutputAddress());
#endif
    return result;
}

bool RasterizerVulkan::AccelerateTextureCopy(const Pica::DisplayTransferConfig& config) {
    const bool result=res_cache.AccelerateTextureCopy(config);
#ifdef ENABLE_MM3D_PRESENTATION
    if(MM3D::ScreenShrink::native_normalized.load() &&
       MM3D::ScreenShrink::presentation_scale.load()<1.f) {
        const auto now=MM3D::InputLatency::Now();
        auto& stamp=midpoint_transfer_logged[1];
        if(now-stamp>=1000000) {
            stamp=now;
            LOG_INFO(Render_Vulkan,"Interpolation shrink TextureCopy: success={} input={:08x} output={:08x} frame={} source_frame={} source={:08x} size={} input_width={} input_gap={} output_width={} output_gap={}",
                result,config.GetPhysicalInputAddress(),config.GetPhysicalOutputAddress(),
                MM3D::guest_frames.load(),midpoint_scene_copies.frame,midpoint_scene_copies.addresses[0],
                config.texture_copy.size&~15U,u32(config.texture_copy.input_width)*16,
                u32(config.texture_copy.input_gap)*16,u32(config.texture_copy.output_width)*16,
                u32(config.texture_copy.output_gap)*16);
        }
    }
    midpoint_scene_copies.TextureTransfer(MM3D::guest_frames.load(),
        config.GetPhysicalInputAddress(),config.GetPhysicalOutputAddress(),
        config.texture_copy.size&~15U,config.texture_copy.input_width*16,
        config.texture_copy.input_gap*16,config.texture_copy.output_width*16,
        config.texture_copy.output_gap*16,result);
#endif
    return result;
}

bool RasterizerVulkan::AccelerateFill(const Pica::MemoryFillConfig& config) {
#ifdef ENABLE_OOT3D_PRESENTATION
    if(OOT3D::Interpolation::Replaying()) {
        oot_color_clears.Observe(OOT3D::Interpolation::guest_frames.load(),config.GetStartAddress(),
            config.GetEndAddress(),config.value_32bit,config.fill_32bit?4u:config.fill_24bit?3u:2u);
    }
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    midpoint_scene_copies.InvalidatePadded(config.GetStartAddress(),config.GetEndAddress());
    if(MM3D::CameraMidpoint::enabled.load())
        midpoint_color_clears.Observe(MM3D::guest_frames.load(),config.GetStartAddress(),
            config.GetEndAddress(),config.value_32bit,config.fill_32bit?4u:config.fill_24bit?3u:2u);
    MM3D::EffectCapture::ObserveFill({.frame=MM3D::guest_frames.load(),
        .start=config.GetStartAddress(),.end=config.GetEndAddress(),.value=config.value_32bit,
        .bytes=config.fill_32bit?4u:config.fill_24bit?3u:2u});
#endif
    return res_cache.AccelerateFill(config);
}

bool RasterizerVulkan::AccelerateDisplay(const Pica::FramebufferConfig& config,
                                         PAddr framebuffer_addr, u32 pixel_stride,
                                         ScreenInfo& screen_info) {
    if (framebuffer_addr == 0) [[unlikely]] {
        return false;
    }

    VideoCore::SurfaceParams src_params;
    src_params.addr = framebuffer_addr;
    src_params.width = std::min(config.width.Value(), pixel_stride);
    src_params.height = config.height;
    src_params.stride = pixel_stride;
    src_params.is_tiled = false;
    src_params.pixel_format = VideoCore::PixelFormatFromGPUPixelFormat(config.color_format);
    src_params.UpdateParams();

    const auto [src_surface_id, src_rect] =
        res_cache.GetSurfaceSubRect(src_params, VideoCore::ScaleMatch::Ignore, true);

    if (!src_surface_id) {
        return false;
    }

    Surface& src_surface = res_cache.GetSurface(src_surface_id);
    const u32 scaled_width = src_surface.GetScaledWidth();
    const u32 scaled_height = src_surface.GetScaledHeight();

    screen_info.texcoords = Common::Rectangle<f32>(
        (float)src_rect.bottom / (float)scaled_height, (float)src_rect.left / (float)scaled_width,
        (float)src_rect.top / (float)scaled_height, (float)src_rect.right / (float)scaled_width);

    screen_info.image_view = src_surface.ImageView();

    return true;
}

void RasterizerVulkan::MakeSoftwareVertexLayout() {
    constexpr std::array sizes = {4, 4, 2, 2, 2, 1, 4, 3};

    software_layout = VertexLayout{
        .binding_count = 1,
        .attribute_count = 8,
    };

    for (u32 i = 0; i < software_layout.binding_count; i++) {
        VertexBinding& binding = software_layout.bindings[i];
        binding.binding.Assign(i);
        binding.fixed.Assign(0);
        binding.byte_count.Assign(sizeof(HardwareVertex));
    }

    u32 offset = 0;
    for (u32 i = 0; i < 8; i++) {
        VertexAttribute& attribute = software_layout.attributes[i];
        attribute.binding.Assign(0);
        attribute.location.Assign(i);
        attribute.offset.Assign(offset);
        attribute.type.Assign(Pica::PipelineRegs::VertexAttributeFormat::FLOAT);
        attribute.size.Assign(sizes[i]);
        offset += sizes[i] * sizeof(float);
    }
}

void RasterizerVulkan::SyncAndUploadLUTsLF() {
    constexpr std::size_t max_size =
        sizeof(Common::Vec2f) * 256 * Pica::LightingRegs::NumLightingSampler +
        sizeof(Common::Vec2f) * 128; // fog

    if (!pica.lighting.lut_dirty && !pica.fog.lut_dirty) {
        return;
    }

    std::size_t bytes_used = 0;
    auto [buffer, offset, invalidate] = texture_lf_buffer.Map(max_size, sizeof(Common::Vec4f));

    if (invalidate) {
        pica.lighting.lut_dirty = pica.lighting.LutAllDirty;
        pica.fog.lut_dirty = true;
    }

    // Sync the lighting luts
    while (pica.lighting.lut_dirty) {
        u32 index = std::countr_zero(pica.lighting.lut_dirty);
        pica.lighting.lut_dirty &= ~(1 << index);

        Common::Vec2f* new_data = reinterpret_cast<Common::Vec2f*>(buffer + bytes_used);
        const auto& source_lut = pica.lighting.luts[index];
        for (u32 i = 0; i < source_lut.size(); i++) {
            new_data[i] = {source_lut[i].ToFloat(), source_lut[i].DiffToFloat()};
        }
        fs_data.lighting_lut_offset[index / 4][index % 4] =
            static_cast<int>((offset + bytes_used) / sizeof(Common::Vec2f));
        fs_data_dirty = true;
        bytes_used += source_lut.size() * sizeof(Common::Vec2f);
    }

    // Sync the fog lut
    if (pica.fog.lut_dirty) {
        Common::Vec2f* new_data = reinterpret_cast<Common::Vec2f*>(buffer + bytes_used);
        for (u32 i = 0; i < pica.fog.lut.size(); i++) {
            new_data[i] = {pica.fog.lut[i].ToFloat(), pica.fog.lut[i].DiffToFloat()};
        }
        fs_data.fog_lut_offset = static_cast<int>((offset + bytes_used) / sizeof(Common::Vec2f));
        fs_data_dirty = true;
        bytes_used += pica.fog.lut.size() * sizeof(Common::Vec2f);
        pica.fog.lut_dirty = false;
    }

    texture_lf_buffer.Commit(static_cast<u32>(bytes_used));
}

void RasterizerVulkan::SyncAndUploadLUTs() {
    const auto& proctex = pica.proctex;
    constexpr std::size_t max_size =
        sizeof(Common::Vec2f) * 128 * 3 + // proctex: noise + color + alpha
        sizeof(Common::Vec4f) * 256 +     // proctex
        sizeof(Common::Vec4f) * 256;      // proctex diff

    if (!pica.proctex.lut_dirty) {
        return;
    }

    std::size_t bytes_used = 0;
    auto [buffer, offset, invalidate] = texture_buffer.Map(max_size, sizeof(Common::Vec4f));

    if (invalidate) {
        pica.proctex.table_dirty = pica.proctex.TableAllDirty;
    }

    // helper function for SyncProcTexNoiseLUT/ColorMap/AlphaMap
    const auto sync_proctex_value_lut =
        [&](const std::array<Pica::PicaCore::ProcTex::ValueEntry, 128>& lut, int& lut_offset) {
            Common::Vec2f* new_data = reinterpret_cast<Common::Vec2f*>(buffer + bytes_used);
            for (u32 i = 0; i < lut.size(); i++) {
                new_data[i] = {lut[i].ToFloat(), lut[i].DiffToFloat()};
            }
            lut_offset = static_cast<int>((offset + bytes_used) / sizeof(Common::Vec2f));
            fs_data_dirty = true;
            bytes_used += lut.size() * sizeof(Common::Vec2f);
        };

    // Sync the proctex noise lut
    if (pica.proctex.noise_lut_dirty) {
        sync_proctex_value_lut(proctex.noise_table, fs_data.proctex_noise_lut_offset);
    }

    // Sync the proctex color map
    if (pica.proctex.color_map_dirty) {
        sync_proctex_value_lut(proctex.color_map_table, fs_data.proctex_color_map_offset);
    }

    // Sync the proctex alpha map
    if (pica.proctex.alpha_map_dirty) {
        sync_proctex_value_lut(proctex.alpha_map_table, fs_data.proctex_alpha_map_offset);
    }

    // Sync the proctex lut
    if (pica.proctex.lut_dirty) {
        Common::Vec4f* new_data = reinterpret_cast<Common::Vec4f*>(buffer + bytes_used);
        for (u32 i = 0; i < proctex.color_table.size(); i++) {
            new_data[i] = proctex.color_table[i].ToVector() / 255.0f;
        }
        fs_data.proctex_lut_offset =
            static_cast<int>((offset + bytes_used) / sizeof(Common::Vec4f));
        fs_data_dirty = true;
        bytes_used += proctex.color_table.size() * sizeof(Common::Vec4f);
    }

    // Sync the proctex difference lut
    if (pica.proctex.diff_lut_dirty) {
        Common::Vec4f* new_data = reinterpret_cast<Common::Vec4f*>(buffer + bytes_used);
        for (u32 i = 0; i < proctex.color_diff_table.size(); i++) {
            new_data[i] = proctex.color_diff_table[i].ToVector() / 255.0f;
        }
        fs_data.proctex_diff_lut_offset =
            static_cast<int>((offset + bytes_used) / sizeof(Common::Vec4f));
        fs_data_dirty = true;
        bytes_used += proctex.color_diff_table.size() * sizeof(Common::Vec4f);
    }

    pica.proctex.table_dirty = 0;

    texture_buffer.Commit(static_cast<u32>(bytes_used));
}

void RasterizerVulkan::UploadUniforms(bool accelerate_draw) {
    const bool sync_vs_pica = accelerate_draw && (pica.vs_setup.uniforms_dirty
#ifdef ENABLE_MM3D_PRESENTATION
        || midpoint_projection.has_value() || message_override.has_value() || item_slide_override.has_value()
        || file_select_override.has_value() || inventory_override.has_value()
#endif
    );
    if (!sync_vs_pica && !vs_data_dirty && !fs_data_dirty) {
        return;
    }

    const u32 uniform_size =
        uniform_size_aligned_vs_pica + uniform_size_aligned_vs + uniform_size_aligned_fs;
    auto [uniforms, offset, invalidate] =
        uniform_buffer.Map(uniform_size, uniform_buffer_alignment);

    u32 used_bytes = 0;

    if (vs_data_dirty || invalidate) {
        std::memcpy(uniforms + used_bytes, &vs_data, sizeof(vs_data));
        pipeline_cache.UpdateRange(1, offset + used_bytes);
        vs_data_dirty = false;
        used_bytes += uniform_size_aligned_vs;
    }

    if (fs_data_dirty || invalidate) {
#ifdef ENABLE_OOT3D_PRESENTATION
        if(oot_song_visual || oot_sparkle_color || oot_model_effect) {
            auto replay_fs=fs_data;
            if(oot_model_effect)for(unsigned r=0;r<6;++r)for(unsigned c=0;c<4;++c)
                replay_fs.const_color[r][c]=oot_model_effect->colors[r][c];
            if(oot_song_visual)replay_fs.const_color[0].w=oot_song_visual->alpha;
            if(oot_sparkle_color)for(unsigned c=0;c<4;++c)replay_fs.const_color[1][c]=(*oot_sparkle_color)[c];
            std::memcpy(uniforms+used_bytes,&replay_fs,sizeof(replay_fs));
        } else
#endif
#ifdef ENABLE_MM3D_PRESENTATION
        // Patch only the uploaded midpoint copy. Native combiner state remains
        // intact for the original draw and all later HUD/scene submissions.
        if(moving_effect_colors) {
            auto midpoint_fs=fs_data;
            for(unsigned stage=0;stage<6;++stage)for(unsigned c=0;c<4;++c)
                midpoint_fs.const_color[stage][c]=(*moving_effect_colors)[stage][c];
            std::memcpy(uniforms + used_bytes,&midpoint_fs,sizeof(midpoint_fs));
        } else if(water_model_alpha_override) {
            auto midpoint_fs=fs_data;
            midpoint_fs.const_color[water_model_alpha_override->first].w=water_model_alpha_override->second;
            std::memcpy(uniforms + used_bytes,&midpoint_fs,sizeof(midpoint_fs));
        } else if(glare_alpha_override) {
            auto midpoint_fs=fs_data;
            midpoint_fs.const_color[1].w=*glare_alpha_override;
            std::memcpy(uniforms + used_bytes,&midpoint_fs,sizeof(midpoint_fs));
        } else if(inventory_override) {
            auto midpoint_fs=fs_data;
            midpoint_fs.const_color[0].w=inventory_override->alpha;
            std::memcpy(uniforms + used_bytes,&midpoint_fs,sizeof(midpoint_fs));
        } else if(file_select_override&&file_select_override->alpha) {
            auto midpoint_fs=fs_data;
            midpoint_fs.const_color[0].w=*file_select_override->alpha;
            std::memcpy(uniforms + used_bytes,&midpoint_fs,sizeof(midpoint_fs));
        } else
#endif
        std::memcpy(uniforms + used_bytes, &fs_data, sizeof(fs_data));
#ifdef ENABLE_MM3D_PRESENTATION
        std::memcpy(uniforms + used_bytes + sizeof(fs_data), &atlas_uniforms, sizeof(atlas_uniforms));
#endif
        pipeline_cache.UpdateRange(2, offset + used_bytes);
        fs_data_dirty = false;
        used_bytes += uniform_size_aligned_fs;
    }

    if (sync_vs_pica || invalidate) {
        VSPicaUniformData vs_uniforms;
        vs_uniforms.SetFromRegs(pica.vs_setup);
#ifdef ENABLE_OOT3D_PRESENTATION
        if(oot_replay_projection)for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
            vs_uniforms.f[r][c]=float((*oot_replay_projection)[r][c]);
        if(oot_waterfall_uv)for(unsigned r=0;r<OOT3D::Interpolation::Waterfall::Rows.size();++r)for(unsigned c=0;c<4;++c)
            vs_uniforms.f[OOT3D::Interpolation::Waterfall::Rows[r]][c]=(*oot_waterfall_uv)[r][c];
        if(oot_model_effect) {
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                vs_uniforms.f[OOT3D::Interpolation::ModelEffects::Rows[r]][c]=oot_model_effect->uv[r][c];
            for(unsigned c=0;c<4;++c)vs_uniforms.f[8][c]=oot_model_effect->color[c];
            if(!oot_pose_override)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                vs_uniforms.f[20+r][c]=float(oot_model_effect->model[r][c]);
        }
        if(oot_fairy_glow) {
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                vs_uniforms.f[20+r][c]=float(oot_fairy_glow->model_view[r][c]);
            for(unsigned c=0;c<4;++c)vs_uniforms.f[8][c]=float(oot_fairy_glow->color[c]);
        }
        if(oot_song_visual) {
            for(unsigned c=0;c<4;++c)vs_uniforms.f[8][c]=oot_song_visual->color[c];
            for(unsigned r=0;r<OOT3D::Interpolation::SongVisual::UVRows(oot_song_visual_layer);++r)
                for(unsigned c=0;c<4;++c)vs_uniforms.f[OOT3D::Interpolation::SongVisual::Rows[r]][c]=oot_song_visual->uv[r][c];
            if(!oot_pose_override && oot_song_visual->model_blended)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                vs_uniforms.f[20+r][c]=float(oot_song_visual->model_view[r][c]);
        }
        if(oot_song_slide)for(unsigned r=0;r<3;++r)vs_uniforms.f[4+r][3]+=(*oot_song_slide)[r];
        if(oot_pose_override)for(unsigned i=0;i<oot_pose_override->count;++i)
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                vs_uniforms.f[20+i*3+r][c]=float(oot_pose_override->palette[i][r][c]);
#endif
#ifdef ENABLE_MM3D_PRESENTATION
        if(midpoint_projection)for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)
            vs_uniforms.f[i][j]=float((*midpoint_projection)[i][j]);
        if(midpoint_projection && song_overrides)for(unsigned row=0;row<MM3D::SongEffectInterpolation::Rows.size();++row)
            vs_uniforms.f[MM3D::SongEffectInterpolation::Rows[row]][3]=(*song_overrides)[row][3];
        if(item_slide_override)for(unsigned q=0;q<item_slide_override->count;++q)
            vs_uniforms.f[11+q*4][0]=item_slide_override->x[q];
        if(file_select_override)for(unsigned q=0;q<file_select_override->count;++q) {
            vs_uniforms.f[11+q*4][0]=file_select_override->x[q];
            vs_uniforms.f[12+q*4][0]=file_select_override->y[q];
            if(file_select_override->rectangles)for(unsigned row=0;row<2;++row)for(unsigned c=0;c<4;++c)
                vs_uniforms.f[11+q*4+row][c]=(*file_select_override->rectangles)[q*2+row][c];
        }
        if(inventory_override)for(unsigned r=0;r<96;++r)if(inventory_override->rows[r])
            for(unsigned c=0;c<4;++c)vs_uniforms.f[r][c]=inventory_override->values[r][c];
        if(message_override)for(unsigned r=0;r<MM3D::MessageInterpolation::Rows.size();++r)
            for(unsigned c=0;c<4;++c)
                vs_uniforms.f[MM3D::MessageInterpolation::Rows[r]][c]=(*message_override)[r][c];
        if(midpoint_projection)for(unsigned bone=0;bone<10;++bone)if(link_overrides[bone])
            for(unsigned i=0;i<12;++i)
                vs_uniforms.f[20+bone*3+i/4][i%4]=float((*link_overrides[bone])[i/4][i%4]);
#endif
        std::memcpy(uniforms + used_bytes, &vs_uniforms, sizeof(vs_uniforms));
        pipeline_cache.UpdateRange(0, offset + used_bytes);
        pica.vs_setup.uniforms_dirty = false;
        used_bytes += uniform_size_aligned_vs_pica;
    }

    uniform_buffer.Commit(used_bytes);
}

void RasterizerVulkan::SwitchDiskResources(u64 title_id) {
#ifdef ENABLE_OOT3D_PRESENTATION
    ++OOT3D::Interpolation::generation;
    oot_replay_frame.Reset();oot_rate_clock.Reset();
    for(auto& slot:oot_replay_slots)slot.Reset();
    oot_color_clears.Reset();OOT3D::Interpolation::Pose::Reset();
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    midpoint_color_clears.Reset();
    ResetPause();
    midpoint_history.Reset();world_map_history.Reset();glare_cpu_history.Reset();forest_particle_history.Reset();snow_history.Reset();fire_impact_history.Reset();leaf_particle_history.Reset();moth_history.Reset();message_history.Reset();message_override.reset();item_slide_history.Reset();item_slide_override.reset();file_select_history.Reset();file_select_override.reset();inventory_history.Reset();banner_history.Reset();magic_meter_history.Reset();heart_history.Reset();lens_history.Reset();letterbox_history.Reset();inventory_override.reset();torch_history.Reset();water_particle_history.Reset();water_model_history.Reset();moving_effect_history.Reset();splash_cpu_history.Reset();burst_particle_history.Reset();spark_history.Reset();firework_history.Reset();chest_particle_history.Reset();sword_trail_history.Reset();shockwave_history.Reset();bubble_history.Reset();link_history.Reset();song_history.Reset();song_overrides.reset();link_frame.reset();link_overrides={};
    midpoint_target_frame.reset();midpoint_world_frame.reset();midpoint_opaque_world_frame.reset();midpoint_stencil_frame.reset();midpoint_fade_frame.reset();midpoint_title_depth_frame.reset();
    for(const auto id : {"@camera_midpoint","@camera_midpoint_fade","@camera_midpoint_hud"})
        if(const auto it=prompt_layers.find(id);it!=prompt_layers.end())it->second->NextFrame();
#endif
    std::atomic_bool stop_loading = false;

    if (switch_disk_resources_callback) {
        switch_disk_resources_callback(VideoCore::LoadCallbackStage::Prepare, 0, 0, "");
    }

    pipeline_cache.SetAccurateMul(accurate_mul);
    pipeline_cache.SwitchCache(title_id, stop_loading, switch_disk_resources_callback);

    if (switch_disk_resources_callback) {
        switch_disk_resources_callback(VideoCore::LoadCallbackStage::Complete, 0, 0, "");
    }
}

} // namespace Vulkan
