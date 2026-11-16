// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "oot3d/hd_font_artwork.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace OOT3D::HDFont {
using U32 = std::uint32_t;
using U64 = std::uint64_t;
inline std::atomic<bool> enabled{false}, dirty{false};
inline std::atomic<bool> thin{false};
enum Mode {Original=0,Thin=1,Heavy=2};
inline int CurrentMode() {return enabled.load()?(thin.load()?Thin:Heavy):Original;}
inline void SetMode(int mode) {
    if(mode<Original || mode>Heavy)mode=Original;
    thin.store(mode==Thin);enabled.store(mode!=Original);
}
inline std::atomic<unsigned> registrations{0}, replacements{0}, redrawn_glyphs{0};
constexpr U32 FirstSVC = 0xfff260;
constexpr U32 HookPC = 0x2ccf68, HookWord = 0xe3a00001;
constexpr unsigned Scale = 4;

inline U32 Instruction(U32 pc, U32 word) {
    return pc == HookPC && word == HookWord ? 0xef000000 | FirstSVC : word;
}
template<class M> bool CodeMatches(M& m) {
    return m.Read(HookPC) == HookWord && m.Read(HookPC + 4) == 0xe28dd028 &&
           m.Read(HookPC - 4) == 0xeb017896 && m.Read(0x2ccf30) == 0xe92d4070;
}
struct Descriptor {
    U32 address{}, width{}, height{}, format{};
    bool operator==(const Descriptor&) const = default;
};
inline bool Supported(const Descriptor& d) {
    // Verified QBF uploads: alpha fonts plus the IA4 system/date font.
    return d.address && d.width >= 8 && d.height >= 8 && d.width <= 512 &&
           d.height <= 512 && !(d.width & (d.width - 1)) &&
           !(d.height & (d.height - 1)) && (d.format == 8 || d.format == 9 || d.format == 11);
}
inline unsigned ByteSize(const Descriptor& d) {
    return Supported(d) ? d.width * d.height / (d.format == 11 ? 2 : 1) : 0;
}
// Called only after the verified QBF upload routine has returned. Its current
// GL texture describes the final tiled allocation, not the linear input atlas.
template<class M> std::optional<Descriptor> NativeTexture(M& m, U32 engine) {
    const U32 context = m.Read(0x54cc54).value_or(0);
    const U32 table = m.Read(0x54a814).value_or(0);
    if (!context || !table || !engine) return {};
    const U32 unit = m.Read(context + 0x58).value_or(4);
    if (unit > 2) return {};
    const auto binding = m.Read(context + 0x5c + unit * 4);
    if (!binding) return {};
    const U32 entry = *binding ? m.Read(table + 0x810 + unit * 4).value_or(0) : table;
    const U32 object = entry ? m.Read(entry).value_or(0) : 0;
    if (!object) return {};
    const U32 desc = object + 0x34;
    Descriptor d{m.Read(desc + 4).value_or(0), m.Read(desc + 12).value_or(0),
                 m.Read(desc + 16).value_or(0), m.Read(desc + 28).value_or(99)};
    if (!Supported(d) || m.Read(engine + 0x3e0) != d.width ||
        m.Read(engine + 0x3e4) != d.height) return {};
    return d;
}
struct Identity { Descriptor descriptor; U64 hash{}; };
inline std::mutex registry_mutex;
inline std::vector<Identity> registry;
inline void Reset() {
    std::lock_guard lock(registry_mutex);
    registry.clear(); registrations = 0; replacements = 0; redrawn_glyphs = 0;
    ClearArtwork();
}
inline void Register(Descriptor d, U64 hash) {
    if (!Supported(d)) return;
    std::lock_guard lock(registry_mutex);
    std::erase_if(registry, [&](const Identity& i) { return i.descriptor.address == d.address; });
    if (registry.size() == 256) registry.erase(registry.begin());
    registry.push_back({d, hash});
    ++registrations;
}
inline std::optional<U64> Find(const Descriptor& d) {
    if (!enabled.load() || !Supported(d)) return {};
    std::lock_guard lock(registry_mutex);
    for (const auto& i : registry) if (i.descriptor == d) return i.hash;
    return {};
}

} // namespace OOT3D::HDFont
