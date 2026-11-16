// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace OOT3D::HDFont {
struct RupeeShadow {
    bool enabled=true;
    float x=.75f,y=.75f,blur=0,strength=.8f;
    bool operator==(const RupeeShadow&) const=default;
};
struct RupeeAtlas {
    std::uint64_t hash{},variant{};
    std::vector<std::uint8_t> rgba,native;
};
inline Common::AtomicSharedPtr<const RupeeAtlas> rupee_atlas;
// Host-only replacement. Native UVs, fixed digit advances, colors and fades
// remain untouched; the renderer binds this only to an owned rupee or gameplay timer draw.
std::vector<std::uint8_t> MakeRupeeAtlas(std::span<const std::uint8_t> ctxb,RupeeShadow shadow={},bool timer=false,bool backdrop=false,bool bold=false);
std::shared_ptr<const RupeeAtlas> StyledRupeeAtlas(RupeeShadow shadow,bool timer=false,bool backdrop=false,bool bold=false);
}
