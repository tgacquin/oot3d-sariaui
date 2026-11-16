// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include "oot3d/hd_font.h"
#include "oot3d/hd_rupee_font.h"
#include "oot3d/hd_boss_timer_font.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"
#include "video_core/texture/texture_decode.h"

namespace Vulkan {
// Separate from the guest surface cache: never changes readbacks, UVs or packs.
class HDFontCache {
    struct Entry {
        OOT3D::HDFont::Descriptor descriptor;
        u64 hash, generation, last_use,variant;
        unsigned cost;
        std::unique_ptr<Surface> image;
    };
    std::vector<Entry> entries;
    static constexpr unsigned MaxBytes = 64 * 1024 * 1024, MaxEntries = 32;
    unsigned bytes{};
public:
    bool Empty() const { return entries.empty(); }
    void Collect(TextureRuntime& runtime, bool all = false) {
        const u64 completed = runtime.GetResourceFreeTick();
        std::erase_if(entries, [&](const Entry& entry) {
            if (entry.last_use > completed || (!all && entries.size() < MaxEntries)) return false;
            bytes -= entry.cost;
            return true;
        });
    }
    vk::ImageView Get(TextureRuntime& runtime, const OOT3D::HDFont::Descriptor& d,
                      u64 hash, std::span<const u8> tiled, std::span<const u8> generated = {},u64 variant=0) {
        const auto generation = OOT3D::HDFont::ArtworkGeneration();
        for (auto& entry : entries) {
            // Identical custom text atlases are used by several guest meshes.
            // Share the host image across addresses once content was verified.
            if (entry.descriptor.width == d.width && entry.descriptor.height == d.height &&
                entry.descriptor.format == d.format && entry.hash == hash && entry.generation == generation && entry.variant==variant) {
                entry.last_use = runtime.GetResourceTick();
                return entry.image ? entry.image->ImageView() : vk::ImageView{};
            }
        }
        const unsigned cost = d.width * d.height * 64;
        if (bytes + cost > MaxBytes || entries.size() >= MaxEntries) Collect(runtime, true);
        // Never wait for the GPU just to improve a font. Fall back this draw.
        if (bytes + cost > MaxBytes || entries.size() >= MaxEntries) return {};
        if (generated.empty() ? tiled.size() != OOT3D::HDFont::ByteSize(d) :
            ((d.format != 0 && d.format != 4 && d.format != 13) || generated.size() != cost ||
             tiled.size() != d.width * d.height * (d.format==13?1:d.format==4?2:4))) return {};

        std::vector<u8> rebuilt;
        if (generated.empty()) {
            Pica::Texture::TextureInfo info{};
            info.width = d.width; info.height = d.height;
            info.format = Pica::TexturingRegs::TextureFormat(d.format);
            info.SetDefaultStride();
            std::vector<u8> alpha(d.width * d.height);
            for (unsigned y = 0; y < d.height; ++y) {
                for (unsigned x = 0; x < d.width; ++x) {
                    // Match Azahar's bottom-up conversion of PICA tiled textures.
                    const auto pixel=Pica::Texture::LookupTexture(tiled.data(),x,y,info);
                    alpha[(d.height-1-y)*d.width+x]=d.format==9?
                        (pixel.r()&0xf0)|(pixel.a()>>4):pixel.a();
                }
            }
            rebuilt = d.format==9 ? OOT3D::HDFont::RedrawSystem(alpha,d.width,d.height) :
                OOT3D::HDFont::Redraw(alpha, d.width, d.height);
        }
        const auto pixels = generated.empty() ? std::span<const u8>(rebuilt) : generated;
        if (pixels.empty()) {
            // Remember misses too: unknown fonts must not be rescanned per draw.
            entries.push_back({d, hash, generation, runtime.GetResourceTick(), variant, 0, {}});
            return {};
        }
        VideoCore::SurfaceParams params;
        params.width = params.stride = d.width * OOT3D::HDFont::Scale;
        params.height = d.height * OOT3D::HDFont::Scale;
        params.pixel_format = VideoCore::PixelFormat::RGBA8;
        params.UpdateParams();
        auto image = std::make_unique<Surface>(runtime, params);
        const auto staging = runtime.FindStaging(u32(pixels.size()), true);
        std::copy(pixels.begin(), pixels.end(), staging.mapped.begin());
        image->Upload({staging.offset, staging.size, params.GetRect(), 0}, staging);
        const auto view = image->ImageView();
        entries.push_back({d, hash, generation, runtime.GetResourceTick(), variant, cost, std::move(image)});
        bytes += cost;
        return view;
    }
};
} // namespace Vulkan
