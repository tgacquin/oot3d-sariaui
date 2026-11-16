// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace Vulkan { class Instance; }
namespace OOT3D::Randomizer {
// Decoded from the player's ROM, never bundled with the executable.
constexpr unsigned IconAtlasSize = 512, IconCellSize = 42, IconColumns = 12;
using IconPixels = std::array<std::uint32_t, IconAtlasSize * IconAtlasSize>;
inline Common::AtomicSharedPtr<const IconPixels> icon_pixels;
class IconAtlas {
public:
    IconAtlas(const Vulkan::Instance&, std::mutex&);
    ~IconAtlas();
    std::uint64_t Texture();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
