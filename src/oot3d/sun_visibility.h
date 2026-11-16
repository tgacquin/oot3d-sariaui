// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "mm3d/visibility_depth.h"
#include "oot3d/camera.h"

namespace OOT3D::SunVisibility {
using U32 = std::uint32_t;
using U64 = std::uint64_t;
using Context = MM3D::VisibilityDepth::Context;
// Session-only override for diagnosis. Only the guarded USA Rev1 sun LDR can
// use this backend; ordinary depth reads and guest memory remain exact.
inline std::atomic<bool> enabled{true};
inline std::atomic<U64> cached{}, pending{}, exact{}, clear{};
struct Sample {U32 value{};bool pending{};};
struct Backend {
    virtual ~Backend() = default;
    virtual std::optional<Sample> ReadSunDepth(U32 address, const Context& context) = 0;
    virtual void ResetSunDepth() = 0;
};
inline std::atomic<Backend*> backend{};
template<class M> std::optional<Context> LiveContext(M& memory, U64 frame, U32 generation) {
    const auto owner = memory.Read(0x5043e0);
    if (!owner || !Camera::GameplayView(memory, *owner + 0x188)) return {};
    const auto scene = memory.Read(*owner + 0x104);
    if (!scene || (*scene & 0xffff) > 0x65) return {};
    return Context{frame, 0, generation, *owner, *scene};
}
struct Reader {
    Context context{};
    bool active{};
    template<class M> void Begin(M& memory, bool use, U64 frame, U32 generation) {
        active = false;
        const auto batch = context.batch + 1;
        if (use) if (const auto live = LiveContext(memory, frame, generation)) {
            context = *live;context.batch = batch;active = true;
        }
        if (!active) if (auto* b = backend.load()) b->ResetSunDepth();
    }
    std::optional<Sample> Read(U32 base, U32 index) const {
        auto* b = backend.load();
        const auto address = MM3D::VisibilityDepth::Physical(base, index);
        if (!active || !b || !address) return {};
        return b->ReadSunDepth(*address, context);
    }
};
}
