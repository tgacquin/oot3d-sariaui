// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <mutex>
#include <span>
#include <fmt/format.h>
#include "video_core/renderer_vulkan/vk_platform.h"
namespace Vulkan { class Instance; }
namespace OOT3D {
class Overlay {
public:
    Overlay(const Vulkan::Instance& instance, vk::Format format, unsigned image_count,
            std::mutex& submit_mutex);
    ~Overlay(); // The caller joins the presenter and drains the GPU first.
    void Draw(vk::CommandBuffer command, vk::Framebuffer framebuffer,
              unsigned width, unsigned height);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace OOT3D
