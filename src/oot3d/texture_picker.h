// Copyright 2026 MM3D presentation contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <array>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <span>
#include "oot3d/overlay.h"
#include "oot3d/inspection_clock.h"
namespace Vulkan { class Scheduler; }
namespace OOT3D::Textures {
constexpr unsigned Limit = 256;
struct Thumbnail {
    vk::Device device;
    vk::DeviceMemory memory{};
    vk::Image image{};
    vk::ImageView view{};
    std::uint64_t key{};
    unsigned width{}, height{};
    std::vector<std::uint8_t> alpha; // Detail-only, upright rows; never saved to disk.
    ~Thumbnail();
};
bool TrimVisible(std::span<const std::uint8_t> alpha, unsigned width, unsigned height,
                 std::array<float,4>& rectangle);
inline std::atomic<std::int64_t> capture_until{0}, hide_until{0};
inline std::atomic<unsigned> hidden{0};
inline thread_local bool hide_current_draw = false;
void ToggleSelection(std::uint64_t key);
void SetSelection(const std::vector<std::uint64_t>& keys);
bool IsSelected(std::uint64_t key);
unsigned SelectionCount();
std::vector<std::uint64_t> SelectedKeys();
std::string ContentHash(std::span<const std::uint8_t> bytes);
bool StartRecording(const std::string& path, const std::string& phase);
bool RecordingKey(std::uint64_t key);
bool NeedsRecordingHash(std::uint64_t key);
void RecordBinding(std::uint64_t key, std::span<const std::uint8_t> bytes, const std::string& context);
std::string RecordingStatus();
void HideSelected();
void Restore();
bool Matches(std::uint64_t key);
std::uint64_t Key(unsigned address, unsigned width, unsigned height, unsigned format);
void Start();
bool Wants(std::uint64_t key);
void RequestDetail(std::uint64_t key, const std::string& hash);
bool WantsDetail(std::uint64_t key, const std::string& hash = {});
std::shared_ptr<Thumbnail> Detail();
void Capture(const Vulkan::Instance&, Vulkan::Scheduler&, vk::Image source, vk::Format format,
             unsigned width, unsigned height, std::uint64_t key,
             vk::ImageLayout source_layout = vk::ImageLayout::eGeneral, bool detail = false);
std::vector<std::shared_ptr<Thumbnail>> Snapshot();
void Clear();
}
