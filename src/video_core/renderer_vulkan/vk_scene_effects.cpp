// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_scene_effects.h"
#include "video_core/host_shaders/full_screen_triangle_vert.h"
#include "video_core/host_shaders/vulkan_bloom_frag.h"
#include "video_core/host_shaders/vulkan_fxaa_frag.h"
#include "video_core/host_shaders/vulkan_bloom_prefilter_frag.h"
#include "video_core/renderer_vulkan/vk_descriptor_update_queue.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_render_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Vulkan {
namespace {
constexpr std::array<vk::DescriptorSetLayoutBinding, 1> bindings{{
    {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}}};
constexpr auto stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
constexpr auto composite_source = R"(#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(set=0,binding=0) uniform sampler2D glow;
layout(push_constant) uniform Params { vec2 scale; vec2 offset; float intensity; };
void main() { color = vec4(texture(glow, uv).rgb * intensity, 0.0); }
)";
}
SceneEffects::SceneEffects(const Instance& instance, Scheduler& s, RenderManager& m, DescriptorUpdateQueue& u)
    : scheduler(s), manager(m), updates(u), device(instance.GetDevice()),
      descriptors(instance, s.GetMasterSemaphore(), bindings, 32), highlights(instance, s, m), glow(instance, s, m), filtered(instance, s, m) {
    const vk::PushConstantRange range{stages, 0, 20};
    layout = device.createPipelineLayoutUnique({.setLayoutCount=1, .pSetLayouts=&descriptors.Layout(),
        .pushConstantRangeCount=1, .pPushConstantRanges=&range});
    vertex = vk::UniqueShaderModule(Compile(HostShaders::FULL_SCREEN_TRIANGLE_VERT, vk::ShaderStageFlagBits::eVertex, device), device);
    sampler = device.createSamplerUnique({.magFilter=vk::Filter::eLinear, .minFilter=vk::Filter::eLinear,
        .addressModeU=vk::SamplerAddressMode::eClampToEdge, .addressModeV=vk::SamplerAddressMode::eClampToEdge,
        .addressModeW=vk::SamplerAddressMode::eClampToEdge});
}
vk::Pipeline SceneEffects::Pipeline(vk::RenderPass pass, Pass effect) {
    auto& cached = pipelines[{pass, effect}];
    if (cached) return *cached;
    const vk::PipelineVertexInputStateCreateInfo input{};
    const vk::PipelineInputAssemblyStateCreateInfo assembly{.topology=vk::PrimitiveTopology::eTriangleList};
    const vk::PipelineViewportStateCreateInfo viewport{.viewportCount=1,.scissorCount=1};
    const vk::PipelineRasterizationStateCreateInfo raster{.polygonMode=vk::PolygonMode::eFill,.lineWidth=1};
    const vk::PipelineMultisampleStateCreateInfo msaa{.rasterizationSamples=vk::SampleCountFlagBits::e1};
    const vk::PipelineDepthStencilStateCreateInfo depth{};
    const vk::PipelineColorBlendAttachmentState blend{.blendEnable=effect == Pass::Composite,
        .srcColorBlendFactor=vk::BlendFactor::eOneMinusDstColor,.dstColorBlendFactor=vk::BlendFactor::eOne,
        .colorBlendOp=vk::BlendOp::eAdd,.colorWriteMask=vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB};
    const vk::PipelineColorBlendStateCreateInfo blending{.attachmentCount=1,.pAttachments=&blend};
    const std::array dynamic{vk::DynamicState::eViewport,vk::DynamicState::eScissor};
    const vk::PipelineDynamicStateCreateInfo dynamics{.dynamicStateCount=2,.pDynamicStates=dynamic.data()};
    // The copy and bloom composite share a sampler shader and differ in blending.
    vk::UniqueShaderModule* fragment = &composite;
    std::string_view source = composite_source;
    switch (effect) {
    case Pass::Prefilter: fragment = &prefilter; source = HostShaders::VULKAN_BLOOM_PREFILTER_FRAG; break;
    case Pass::Blur: fragment = &blur; source = HostShaders::VULKAN_BLOOM_FRAG; break;
    case Pass::Fxaa: fragment = &fxaa; source = HostShaders::VULKAN_FXAA_FRAG; break;
    case Pass::Copy:
    case Pass::Composite: break;
    }
    if (!*fragment) *fragment = vk::UniqueShaderModule(Compile(source, vk::ShaderStageFlagBits::eFragment, device), device);
    const std::array<vk::PipelineShaderStageCreateInfo,2> shaders{{
        {.stage=vk::ShaderStageFlagBits::eVertex,.module=*vertex,.pName="main"},
        {.stage=vk::ShaderStageFlagBits::eFragment,.module=**fragment,.pName="main"}}};
    const vk::GraphicsPipelineCreateInfo info{.stageCount=2,.pStages=shaders.data(),.pVertexInputState=&input,
        .pInputAssemblyState=&assembly,.pViewportState=&viewport,.pRasterizationState=&raster,
        .pMultisampleState=&msaa,.pDepthStencilState=&depth,.pColorBlendState=&blending,
        .pDynamicState=&dynamics,.layout=*layout,.renderPass=pass};
    auto result = device.createGraphicsPipelineUnique({}, info);
    if (result.result != vk::Result::eSuccess) return {};
    cached = std::move(result.value);
    return *cached;
}
void SceneEffects::Draw(vk::Pipeline pipeline, vk::ImageView input, vk::Rect2D region,
                      const std::array<float,5>& parameters) {
    const auto set = descriptors.Commit();
    updates.AddImageSampler(set, 0, 0, input, *sampler);
    scheduler.Record([pipeline,set,region,parameters,push=*layout](vk::CommandBuffer cmd) {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,push,0,set,{});
        cmd.setViewport(0,vk::Viewport{float(region.offset.x),float(region.offset.y),
            float(region.extent.width),float(region.extent.height),0,1});
        cmd.setScissor(0,region);
        cmd.pushConstants(push,stages,0,sizeof(parameters),parameters.data());
        cmd.draw(3,1,0,0);
    });
    scheduler.MakeDirty(StateFlags::Pipeline | StateFlags::DescriptorSets);
}
bool SceneEffects::Prepare(vk::Image image, vk::ImageView view, vk::Extent2D size, vk::Rect2D region, Options options) {
    if (!size.width || !size.height || !region.extent.width || !region.extent.height) return false;
    const auto pass = manager.GetRenderpass(VideoCore::PixelFormat::RGBA8, VideoCore::PixelFormat::Invalid, false);
    manager.EndRendering();
    scheduler.Record([image](vk::CommandBuffer cmd) {
        const vk::ImageMemoryBarrier barrier{.srcAccessMask=vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask=vk::AccessFlagBits::eShaderRead,.oldLayout=vk::ImageLayout::eGeneral,
            .newLayout=vk::ImageLayout::eGeneral,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=image,
            .subresourceRange={vk::ImageAspectFlagBits::eColor,0,1,0,1}};
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,vk::PipelineStageFlagBits::eFragmentShader,
            {},{},{},barrier);
    });
    const std::array<float,5> source_rect{float(region.extent.width)/size.width,
        float(region.extent.height)/size.height,float(region.offset.x)/size.width,float(region.offset.y)/size.height,0};
    if (options.fxaa) {
        const auto pipeline = Pipeline(pass, Pass::Fxaa);
        if (!filtered.CanBatch(region.extent.width, region.extent.height)) filtered.NextFrame();
        if (!pipeline || !filtered.Begin(region.extent.width, region.extent.height)) return false;
        Draw(pipeline, view, {{0,0},region.extent}, source_rect);
        filtered.End();
    }
    if (options.bloom > 0) {
        constexpr vk::Extent2D glow_size{72, 128};
        constexpr vk::Extent2D highlight_size{glow_size.width * 2, glow_size.height * 2};
        const auto prefilter_pipeline = Pipeline(pass, Pass::Prefilter);
        const auto blur_pipeline = Pipeline(pass, Pass::Blur);
        if (!prefilter_pipeline || !blur_pipeline || !highlights.Begin(highlight_size.width, highlight_size.height)) return false;
        Draw(prefilter_pipeline, view, {{0,0},highlight_size}, source_rect);
        highlights.End();
        if (!glow.Begin(glow_size.width, glow_size.height)) return false;
        Draw(blur_pipeline, highlights.View(), {{0,0},glow_size}, {1,1,0,0,0});
        glow.End();
    }
    // Complete source sampling before its attachment is written again.
    scheduler.Record([](vk::CommandBuffer cmd) {
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,{},{},{},{});
    });
    return true;
}
void SceneEffects::Composite(vk::RenderPass pass, vk::Rect2D region, Options options) {
    // RGB-only writes retain the game's destination alpha for subsequent draws.
    if (options.fxaa) {
        if (const auto pipe = Pipeline(pass, Pass::Copy)) Draw(pipe,filtered.View(),region,{1,1,0,0,1});
    }
    if (options.bloom > 0) {
        if (const auto pipe = Pipeline(pass, Pass::Composite)) Draw(pipe,glow.View(),region,{1,1,0,0,options.bloom});
    }
}
}
