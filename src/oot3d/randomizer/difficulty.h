// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/difficulty.h"
#include <QJsonObject>
#include <optional>

namespace OOT3D::Randomizer {
// Difficulty switches are captured and restored per save, including price text.
inline QJsonObject CaptureDifficulty() {
    using namespace Difficulty;
    return {{"damage", damage_multiplier.load()}, {"hearts", disable_hearts.load()},
        {"fairies", disable_fairies.load()}, {"rupees", disable_rupees.load()},
        {"defense", weaker_defense.load()}, {"magic", disable_magic.load()},
        {"ammo", disable_ammo.load()}, {"fairy_drops", disable_fairy_drops.load()},
        {"death_rupees", lose_rupees_on_death.load()}, {"red", red_potion_hearts.load()},
        {"blue", blue_potion_hearts.load()}, {"bottle", bottled_fairy_hearts.load()}, {"prices", increase_prices.load()}, {"price_multiplier", price_multiplier.load()}};
}
inline bool ValidDifficulty(const QJsonObject& value) {
    for (const auto* key : {"hearts", "fairies", "rupees", "defense", "magic", "ammo", "fairy_drops", "death_rupees", "prices"})
        if (!value.value(key).isBool()) return false;
    for (const auto* key : {"damage", "red", "blue", "bottle"}) {
        const auto number = value.value(key);
        if (!number.isDouble() || number.toDouble() != number.toInt() || number.toInt() < 1 ||
            number.toInt() > (std::string_view(key) == "damage" ? 16 : 20)) return false;
    }
    return value["price_multiplier"].toInt()>=2 && value["price_multiplier"].toInt()<=3;
}
inline void ApplyDifficulty(const QJsonObject& value) {
    using namespace Difficulty;
    damage_multiplier = value["damage"].toInt(); disable_hearts = value["hearts"].toBool();
    disable_fairies = value["fairies"].toBool(); disable_rupees = value["rupees"].toBool();
    weaker_defense = value["defense"].toBool(); disable_magic = value["magic"].toBool();
    disable_ammo = value["ammo"].toBool(); disable_fairy_drops = value["fairy_drops"].toBool();
    lose_rupees_on_death = value["death_rupees"].toBool(); red_potion_hearts = value["red"].toInt();
    blue_potion_hearts = value["blue"].toInt(); bottled_fairy_hearts = value["bottle"].toInt();
    increase_prices=value["prices"].toBool();price_multiplier=value["price_multiplier"].toInt();
    active_price_multiplier=increase_prices.load()?unsigned(price_multiplier.load()):1;
}
inline std::optional<QJsonObject> previous_difficulty;
inline unsigned previous_price_factor=1;
inline void RestoreDifficulty() {
    if (previous_difficulty) { ApplyDifficulty(*previous_difficulty); previous_difficulty.reset(); Difficulty::active_price_multiplier=previous_price_factor; }
    Difficulty::file_rules = false;
}
inline void UseDifficulty(const QJsonObject& value) {
    RestoreDifficulty();
    Difficulty::file_rules = true;
    previous_price_factor=Difficulty::active_price_multiplier.load();
    previous_difficulty = CaptureDifficulty();
    ApplyDifficulty(value);
}
}
