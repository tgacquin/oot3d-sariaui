// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <mutex>
namespace Vulkan { class Instance; }
namespace OOT3D {
class TextureInspector {
public:
    TextureInspector(const Vulkan::Instance&, std::mutex& submit_mutex);
    ~TextureInspector();
    void Draw();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
