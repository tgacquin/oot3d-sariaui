// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/remote_names_geometry.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include <map>
namespace Vulkan {
class StreamBuffer;
class RemoteNameRenderer {
public:
    struct Draw {
        u32 offset{},count{};
        vk::Pipeline pipeline{};
        vk::DescriptorSet descriptor{};
        std::array<float,4> depth{};
    };
    RemoteNameRenderer(const Instance&,Scheduler&,RenderManager&,DescriptorUpdateQueue&,TextureRuntime&);
    ~RemoteNameRenderer();
    Draw Prepare(StreamBuffer&,const OOT3D::RemoteNames::Scene&,float fraction,
                 VideoCore::PixelFormat color,VideoCore::PixelFormat depth,vk::CompareOp compare);
    // Call in the scene render pass, before its HUD, with the scene viewport.
    void Render(const Draw&,vk::Buffer,vk::Viewport,vk::Rect2D);
private:
    vk::Pipeline Pipeline(VideoCore::PixelFormat color,VideoCore::PixelFormat depth,vk::CompareOp compare);
    const Instance& instance;
    Scheduler& scheduler;
    RenderManager& manager;
    DescriptorUpdateQueue& updates;
    TextureRuntime& runtime;
    vk::Device device;
    DescriptorHeap descriptors;
    vk::UniquePipelineLayout layout;
    vk::UniqueShaderModule vertex_shader,fragment_shader;
    vk::UniqueSampler sampler;
    std::map<std::tuple<VideoCore::PixelFormat,VideoCore::PixelFormat,vk::CompareOp>,vk::UniquePipeline> pipelines;
    OOT3D::RemoteNames::Font font;
    std::unique_ptr<Surface> texture;
};
}
