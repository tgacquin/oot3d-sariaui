// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace OOT3D::Difficulty {
inline std::atomic<bool> file_rules{false};
inline std::atomic<int> damage_multiplier{1};
inline std::atomic<bool> disable_hearts{false},disable_fairies{false},disable_rupees{false},dirty{false};
inline std::atomic<bool> weaker_defense{false},disable_magic{false},disable_ammo{false};
inline std::atomic<bool> disable_fairy_drops{false},lose_rupees_on_death{false};
inline std::atomic<int> red_potion_hearts{20},blue_potion_hearts{20},bottled_fairy_hearts{20};
inline std::atomic<bool> increase_prices{false};
inline std::atomic<int> price_multiplier{2};
inline std::atomic<unsigned> active_price_multiplier{1};
inline std::atomic<bool> price_text_ready{false};
inline void InitializePrices() {
    // Latch on application startup, keeping loaded dialogue, affordability
    // checks and charges consistent until the next restart.
    static bool initialized=false;
    if(!initialized) {
        active_price_multiplier=increase_prices.load()?unsigned(std::clamp(price_multiplier.load(),2,3)):1;
        initialized=true;
    }
}
inline int Multiplier(float value) {
    return std::isfinite(value)?int(std::round(std::clamp(value,1.f,16.f))):1;
}
inline unsigned Healing(int hearts) {return unsigned(std::clamp(hearts,1,20))*16;}
inline std::int32_t Defense(std::int32_t damage) {
    if(!weaker_defense.load() || damage>=0)return damage>>1;
    return -std::int32_t(std::max<std::int64_t>(1,(-std::int64_t(damage)*2+1)/3));
}
inline std::int32_t Damage(std::int32_t change) {
    if(change>=0)return change;
    // Native health is signed 16-bit, with sixteen units per heart.
    // Clamp before the native signed add, including unusually large hits.
    return -static_cast<std::int32_t>(std::clamp(
        std::ceil(-double(change)*Multiplier(damage_multiplier.load())),1.,32768.));
}
inline bool BlockDrop(unsigned params) {
    const auto type=params&255;
    return (disable_hearts.load() && type==3) ||
        (disable_rupees.load() && (type<=2 || type==0x13 || type==0x14)) ||
        (disable_magic.load() && (type==0xe || type==0xf)) ||
        (disable_ammo.load() && (type==4 || type==5 || (type>=8 && type<=0xd) || type==0x10));
}
inline bool RecoveryFairy(unsigned params) {
    // EnElf modes: Navi=0, revive=1, free healing=2, Kokiri=3,
    // fountain group=4, bottle use=5, fountain healing=6, big healing=7.
    return params==2 || params==6 || params==7;
}
}
