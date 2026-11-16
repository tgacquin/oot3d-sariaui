// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <QByteArray>
#include <QJsonObject>
#include "oot3d/randomizer/config.h"

namespace OOT3D::Randomizer {
constexpr std::uint32_t GuestBase = 0x5c7000, GuestEnd = 0xa00000;
// BSS follows the initialized data's actual size, not its page-rounded size.
// The loader deliberately overallocates those two regions separately.
constexpr bool CanReserveGuestMemory(std::uint32_t address, std::uint32_t initialized_size,
                                     std::uint32_t bss_size, std::uint32_t allocated_size) {
    const auto native_end = std::uint64_t(address) + initialized_size + bss_size;
    const auto allocated_end = std::uint64_t(address) + allocated_size;
    return native_end <= GuestBase && native_end <= allocated_end && allocated_end <= GuestEnd;
}
struct Patch { std::uint32_t address; QByteArray bytes; std::string name; };
struct Symbol { std::uint32_t address = 0, size = 0; };
struct GuestData {
    QByteArray bytes, assets, digest;
    QByteArray rewards;
    struct RewardHook {std::uint32_t address,instruction,target;};
    std::vector<RewardHook> reward_hooks;
    QJsonObject reward_symbols;
    std::uint32_t reward_base=0;
    std::vector<Patch> patches;
    QJsonObject symbols;
    Symbol Find(const char* name) const;
};
const GuestData& Data();
struct AdvancedOption {
    std::string key, label, group, description;
    std::vector<std::string> choices, descriptions;
    int initial = 0;
};
const std::vector<AdvancedOption>& AdvancedOptions();
QJsonObject ToJson(const Config& config);
Config FromJson(const QJsonObject& object);
bool WriteJson(const QString& path, const QJsonObject& object);
QJsonObject ReadJson(const QString& path);
} // namespace OOT3D::Randomizer
