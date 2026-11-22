// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <map>
#include "mm3d/prompt_layer.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"

namespace Vulkan {
class SceneEffects {
public:
    SceneEffects(const Instance&, Scheduler&, RenderManager&, DescriptorUpdateQueue&);
    struct Options { bool fxaa; float bloom; };
    bool Prepare(vk::Image image, vk::ImageView view, vk::Extent2D size, vk::Rect2D region, Options options);
    void Composite(vk::RenderPass pass, vk::Rect2D region, Options options);
private:
    enum class Pass { Prefilter, Blur, Composite, Fxaa, Copy };
    vk::Pipeline Pipeline(vk::RenderPass pass, Pass effect);
    void Draw(vk::Pipeline pipeline, vk::ImageView input, vk::Rect2D region,
              const std::array<float, 5>& parameters);
    Scheduler& scheduler;
    RenderManager& manager;
    DescriptorUpdateQueue& updates;
    vk::Device device;
    DescriptorHeap descriptors;
    vk::UniquePipelineLayout layout;
    vk::UniqueShaderModule vertex, prefilter, blur, composite, fxaa;
    vk::UniqueSampler sampler;
    std::map<std::pair<vk::RenderPass, Pass>, vk::UniquePipeline> pipelines;
    MM3D::PromptLayer highlights, glow, filtered;
};
}
