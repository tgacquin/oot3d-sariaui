// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
namespace OOT3D::DungeonCollection {
enum class Kind {Chest,Collect,Skulltula};
struct Mask {Kind kind=Kind::Chest;unsigned scene=0;std::uint32_t bits=0;};
// Derived from externals/oot3d-randomizer/source/{dungeon,item_location}.cpp.
// Per dungeon: normal, MQ. Chests (including shared/boss-room locations),
// freestanding keys, and Gold Skulltulas only; no repeatable pickups.
// Spirit's outdoor chests and the separate boss/tower scenes belong here too.
inline constexpr std::array<std::array<Mask,4>,24> Locations{{
    {{{Kind::Chest,0x00,0x0000007eu},{Kind::Skulltula,0x00,0x0000000fu}}}, // Deku Tree normal
    {{{Kind::Chest,0x00,0x0000007fu},{Kind::Skulltula,0x00,0x0000000fu}}}, // Deku Tree MQ
    {{{Kind::Chest,0x01,0x00000570u},{Kind::Chest,0x12,0x00000001u},{Kind::Skulltula,0x01,0x0000001fu}}}, // Dodongo's Cavern normal
    {{{Kind::Chest,0x01,0x0000003fu},{Kind::Chest,0x12,0x00000001u},{Kind::Skulltula,0x01,0x0000001fu}}}, // Dodongo's Cavern MQ
    {{{Kind::Chest,0x02,0x00000016u},{Kind::Skulltula,0x02,0x0000000fu}}}, // Jabu Jabu's Belly normal
    {{{Kind::Chest,0x02,0x000007ffu},{Kind::Skulltula,0x02,0x0000000fu}}}, // Jabu Jabu's Belly MQ
    {{{Kind::Chest,0x03,0x0000fabfu},{Kind::Skulltula,0x03,0x0000001fu}}}, // Forest Temple normal
    {{{Kind::Chest,0x03,0x0000fa6fu},{Kind::Skulltula,0x03,0x0000001fu}}}, // Forest Temple MQ
    {{{Kind::Chest,0x04,0x00003fffu},{Kind::Skulltula,0x04,0x0000001fu}}}, // Fire Temple normal
    {{{Kind::Chest,0x04,0x000019ffu},{Kind::Collect,0x04,0x10000000u},{Kind::Skulltula,0x04,0x0000001fu}}}, // Fire Temple MQ
    {{{Kind::Chest,0x05,0x000007efu},{Kind::Skulltula,0x05,0x0000001fu}}}, // Water Temple normal
    {{{Kind::Chest,0x05,0x00000067u},{Kind::Collect,0x05,0x00000002u},{Kind::Skulltula,0x05,0x0000001fu}}}, // Water Temple MQ
    {{{Kind::Chest,0x06,0x0034f5ffu},{Kind::Chest,0x5c,0x00000a00u},{Kind::Skulltula,0x06,0x0000001fu}}}, // Spirit Temple normal
    {{{Kind::Chest,0x06,0xff0491ffu},{Kind::Chest,0x5c,0x00000a00u},{Kind::Skulltula,0x06,0x0000001fu}}}, // Spirit Temple MQ
    {{{Kind::Chest,0x07,0x00703ffeu},{Kind::Collect,0x07,0x00000002u},{Kind::Skulltula,0x07,0x0000001fu}}}, // Shadow Temple normal
    {{{Kind::Chest,0x07,0x0071fffeu},{Kind::Collect,0x07,0x00000040u},{Kind::Skulltula,0x07,0x0000001fu}}}, // Shadow Temple MQ
    {{{Kind::Chest,0x08,0x001157beu},{Kind::Collect,0x08,0x00000002u},{Kind::Skulltula,0x08,0x00000007u}}}, // Bottom of the Well normal
    {{{Kind::Chest,0x08,0x0000000eu},{Kind::Collect,0x08,0x00000006u},{Kind::Skulltula,0x08,0x00000007u}}}, // Bottom of the Well MQ
    {{{Kind::Chest,0x09,0x00000007u},{Kind::Skulltula,0x09,0x00000007u}}}, // Ice Cavern normal
    {{{Kind::Chest,0x09,0x00000007u},{Kind::Skulltula,0x09,0x00000007u}}}, // Ice Cavern MQ
    {{{Kind::Chest,0x0b,0x001fffffu},{Kind::Collect,0x0b,0x00000002u}}}, // Gerudo Training Grounds normal
    {{{Kind::Chest,0x0b,0x000e6fffu}}}, // Gerudo Training Grounds MQ
    {{{Kind::Chest,0x0a,0x00000800u},{Kind::Chest,0x0d,0x0017ffe0u}}}, // Ganon's Castle normal
    {{{Kind::Chest,0x0a,0x00000800u},{Kind::Chest,0x0d,0x001007ffu},{Kind::Collect,0x0d,0x00000002u}}}, // Ganon's Castle MQ
}};
}
