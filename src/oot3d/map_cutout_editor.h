// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <mutex>
namespace Vulkan { class Instance; }
namespace OOT3D {
class MapCutoutEditor {
public:
    MapCutoutEditor(const Vulkan::Instance&,std::mutex&,bool world_map=false);
    ~MapCutoutEditor();
    void Draw();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
