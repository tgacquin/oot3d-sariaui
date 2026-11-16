// SPDX-License-Identifier: GPL-2.0-or-later
// Read-only native draw diagnostics for animated materials and water effects.
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace OOT3D::Interpolation::SurfaceCapture {
using U32 = std::uint32_t;
using Vec4 = std::array<float, 4>;
inline constexpr unsigned MaxPerFrame = 192, MaxPerBucket = 768;
inline constexpr unsigned TailDraws = 64;
inline constexpr unsigned BucketMs = 250, BucketCount = 40;
inline constexpr unsigned MaxDraws = MaxPerBucket * BucketCount;
struct Texture {
    U32 enabled{}, address{}, width{}, height{}, format{}, wrap_s{}, wrap_t{};
    U32 min_filter{}, mag_filter{}, type{};
};
struct Vertex {
    Vec4 position{}, color{};
    std::array<float, 6> uv{};
    float uv0_w{};
};
struct TextboxSample {
    std::uint64_t frame{},hash{};
    U32 model{},mesh{},pane{},state{},flags{},vertices{},buffer{},generation{};
    std::int64_t elapsed_ms{};
    std::string stage;
    std::array<Vec4,3> matrix{};
};
struct Draw {
    std::uint64_t frame{}, shader{}, swizzle{};
    std::int64_t time_us{}, elapsed_ms{};
    U32 ordinal{}, generation{}, scene{}, target{}, vertices{}, vertex_base{}, index_offset{};
    U32 first_vertex{}, last_vertex{}, topology{}, blend_color{}, boolean_uniforms{};
    bool cpu{}, indexed{}, index16{}, succeeded{};
    std::uint64_t textbox_hash{};
    std::array<U32,4> textbox_source{};
    bool textbox_active{},textbox_match{};
    std::array<Vec4, 96> uniforms{};
    std::array<std::array<U32, 4>, 4> integer_uniforms{};
    std::array<Texture, 3> textures{};
    // sources, modifiers, operations, constant RGBA, scales
    std::array<std::array<U32, 5>, 6> tev{};
    // enable, source/destination RGB, source/destination alpha, RGB/alpha equations
    std::array<U32, 7> blend{};
    std::array<U32, 16> attribute_registers{};
    std::array<U32, 3> alpha_test{}, depth{};
    // CPU draws: transformed clip vertices, including colors and all three UV sets.
    std::array<Vertex, 33> output_vertices{};
    unsigned output_count{};
    // Accelerated draws: first three input vertices, all attribute slots.
    std::array<std::array<Vec4, 16>, 3> inputs{};
    unsigned input_count{}, attribute_count{};
    // Eligible CPU effects: exact replay inputs/order, including duplicate
    // connectors. This avoids reconstructing geometry from clip-space samples.
    std::vector<std::array<Vec4,4>> effect_inputs;
    std::vector<U32> effect_order;
};
struct Packet {
    unsigned session{};
    std::uint64_t dropped{};
    std::vector<Draw> draws;
    std::vector<TextboxSample> textboxes;
    unsigned textbox_dropped{};
};
inline std::mutex mutex;
inline Packet packet;
inline std::atomic<bool> active{false};
inline std::atomic<std::int64_t> begins{}, ends{};
inline std::atomic<unsigned> session{}, draw_count{};
inline std::array<unsigned, BucketCount> bucket_counts{};
inline std::uint64_t previous_frame{};
inline unsigned frame_count{};
inline std::size_t frame_start{}, frame_stored{};
inline bool have_frame{};
inline std::int64_t Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline bool Recording(std::int64_t now) {
    return active.load() && now >= begins.load() && now < ends.load();
}
inline bool Start(std::int64_t now = Now()) {
    std::scoped_lock lock{mutex};
    if (active) return false;
    packet = {}; packet.session = ++session;
    draw_count = 0; bucket_counts = {}; have_frame = false;
    begins = now + 3000; ends = now + 13000; active = true;
    return true;
}
inline void ObserveTextbox(TextboxSample sample) {
    if(!active.load() || sample.frame%10>=3)return;
    const auto now=Now();
    if(!Recording(now))return;
    std::scoped_lock lock{mutex};
    if(!Recording(now))return;
    if(packet.textboxes.size()>=8192){++packet.textbox_dropped;return;}
    sample.elapsed_ms=now-begins.load();packet.textboxes.push_back(std::move(sample));
}
template<class Fill> void Observe(std::uint64_t frame, Fill&& fill, std::int64_t now = -1) {
    if (!active.load() || frame % 10 >= 3) return;
    if (now < 0) now = Now();
    if (!Recording(now)) return;
    std::scoped_lock lock{mutex};
    if (!Recording(now)) return;
    if (!have_frame || frame != previous_frame) {
        have_frame = true; previous_frame = frame; frame_count = 0;
        frame_start = packet.draws.size(); frame_stored = 0;
    }
    const auto ordinal = frame_count++;
    const auto bucket = unsigned((now - begins.load()) / BucketMs);
    // Each wall-time bucket has its own budget: a busy scene cannot consume
    // the entire capture in its first seconds, or after a guest clock reset.
    const bool replace_tail = frame_stored == MaxPerFrame;
    if (!replace_tail && (bucket_counts[bucket] >= MaxPerBucket ||
        packet.draws.size() >= MaxDraws)) {
        ++packet.dropped; return;
    }
    Draw draw;
    draw.frame = frame; draw.ordinal = ordinal; draw.elapsed_ms = now - begins.load();
    fill(draw);
    if (replace_tail) {
        // Transparent particles are often drawn last. Retain the first 128 and
        // final 64 draws instead of always discarding late effects in busy scenes.
        const auto slot = MaxPerFrame - TailDraws + (ordinal - MaxPerFrame) % TailDraws;
        packet.draws[frame_start + slot] = std::move(draw);
        ++packet.dropped;
    } else {
        packet.draws.push_back(std::move(draw));
        ++frame_stored; ++bucket_counts[bucket];
    }
    draw_count = unsigned(packet.draws.size());
}
inline std::optional<Packet> Finish(std::int64_t now = Now()) {
    if (!active.load() || now < ends.load()) return {};
    std::scoped_lock lock{mutex};
    if (!active || now < ends.load()) return {};
    active = false;
    auto result = std::move(packet); packet = {};
    return result;
}
bool Export(const Packet& data, const std::string& path);
}
