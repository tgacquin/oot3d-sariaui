// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <string>
#include <map>

namespace OOT3D::Randomizer {
enum OptionId {
    StartingAge, Forest, DoorOfTime, Fortress,
    Rewards, Songs, Maps, SmallKeys, BossKeys, GanonKey,
    Hearts, Ocarina, Magic, Hookshot, BombBag, Bow, Slingshot, Bottles,
    KokiriSword, DekuShield, HylianShield, MirrorShield, GoronTunic, ZoraTunic,
    IronBoots, HoverBoots, Boomerang, Lens, Hammer, DinsFire, FaroresWind, NayrusLove,
    Strength, Scale, Wallet, Consumables, MaxRupees,
    ChildStealth, TowerEscape, EponaRace, MinigamePhases, FourPoes, LakeOwl, SongReplays,
    HintAccess, HintDistribution, SpoilerLog, Shops, ShopPrices, Tokens, OptionCount
};
struct OptionSpec {
    const char* key;
    const char* label;
    const char* group;
    int low, high, initial;
    const char* choices; // ImGui's double-null-terminated list, or a checkbox/number.
};
inline constexpr std::array<OptionSpec, OptionCount> Specs{{
    {"age", "Starting age", "World access", 0, 2, 0, "Child\0Adult\0Random\0"},
    {"forest", "Forest", "World access", 0, 2, 1, "Closed\0Open\0Open except Deku Tree\0"},
    {"door_of_time", "Door of Time", "World access", 0, 2, 0, "Open\0Closed\0Intended requirements\0"},
    {"fortress", "Gerudo Fortress", "World access", 0, 2, 0, "Rescue four carpenters\0Rescue one carpenter\0Open\0"},
    {"rewards", "Dungeon rewards", "Item shuffling", 0, 3, 0, "Among dungeon rewards\0Anywhere\0Any dungeon\0Overworld\0"},
    {"songs", "Songs", "Item shuffling", 0, 2, 0, "Among song locations\0Anywhere\0Dungeon rewards\0"},
    {"maps", "Maps and compasses", "Item shuffling", 0, 5, 1, "Vanilla locations\0Within own dungeon\0Any dungeon\0Overworld\0Anywhere\0Start with\0"},
    {"small_keys", "Small keys", "Item shuffling", 0, 5, 1, "Vanilla locations\0Within own dungeon\0Any dungeon\0Overworld\0Anywhere\0Start with\0"},
    {"boss_keys", "Boss keys", "Item shuffling", 0, 5, 1, "Vanilla locations\0Within own dungeon\0Any dungeon\0Overworld\0Anywhere\0Start with\0"},
    {"ganon_key", "Ganon's Boss Key", "Item shuffling", 0, 12, 1, "Vanilla location\0Within Ganon's Castle\0Any dungeon\0Overworld\0Anywhere\0Start with\0Light Arrow cutscene\0LACS medallions\0LACS stones\0LACS rewards\0LACS dungeons\0LACS tokens\0LACS hearts\0"},
    {"hearts", "Starting hearts", "Starting inventory", 3, 20, 3, nullptr},
    {"ocarina", "Ocarina", "Starting inventory", 0, 2, 0, "None\0Fairy Ocarina\0Ocarina of Time\0"},
    {"magic", "Magic meter", "Starting inventory", 0, 2, 0, "None\0Single\0Double\0"},
    {"hookshot", "Hookshot", "Starting inventory", 0, 2, 0, "None\0Hookshot\0Longshot\0"},
    {"bomb_bag", "Bomb bag", "Starting inventory", 0, 3, 0, "None\0" "20 bombs\0" "30 bombs\0" "40 bombs\0"},
    {"bow", "Bow", "Starting inventory", 0, 3, 0, "None\0" "30 arrows\0" "40 arrows\0" "50 arrows\0"},
    {"slingshot", "Slingshot", "Starting inventory", 0, 3, 0, "None\0" "30 seeds\0" "40 seeds\0" "50 seeds\0"},
    {"bottles", "Empty bottles", "Starting inventory", 0, 4, 0, nullptr},
    {"kokiri_sword", "Kokiri Sword", "Starting inventory", 0, 1, 0, nullptr},
    {"deku_shield", "Deku Shield", "Starting inventory", 0, 1, 0, nullptr},
    {"hylian_shield", "Hylian Shield", "Starting inventory", 0, 1, 0, nullptr},
    {"mirror_shield", "Mirror Shield", "Starting inventory", 0, 1, 0, nullptr},
    {"goron_tunic", "Goron Tunic", "Starting inventory", 0, 1, 0, nullptr},
    {"zora_tunic", "Zora Tunic", "Starting inventory", 0, 1, 0, nullptr},
    {"iron_boots", "Iron Boots", "Starting inventory", 0, 1, 0, nullptr},
    {"hover_boots", "Hover Boots", "Starting inventory", 0, 1, 0, nullptr},
    {"boomerang", "Boomerang", "Starting inventory", 0, 1, 0, nullptr},
    {"lens", "Lens of Truth", "Starting inventory", 0, 1, 0, nullptr},
    {"hammer", "Megaton Hammer", "Starting inventory", 0, 1, 0, nullptr},
    {"dins_fire", "Din's Fire", "Starting inventory", 0, 1, 0, nullptr},
    {"farores_wind", "Farore's Wind", "Starting inventory", 0, 1, 0, nullptr},
    {"nayrus_love", "Nayru's Love", "Starting inventory", 0, 1, 0, nullptr},
    {"strength", "Strength", "Starting inventory", 0, 3, 0, "None\0Goron Bracelet\0Silver Gauntlets\0Golden Gauntlets\0"},
    {"scale", "Diving scale", "Starting inventory", 0, 2, 0, "None\0Silver Scale\0Golden Scale\0"},
    {"wallet", "Wallet", "Starting inventory", 0, 3, 0, "Default\0Adult's Wallet\0Giant's Wallet\0Tycoon's Wallet\0"},
    {"consumables", "Start with consumables", "Starting inventory", 0, 1, 0, nullptr},
    {"max_rupees", "Start with a full wallet", "Starting inventory", 0, 1, 0, nullptr},
    {"child_stealth", "Skip castle courtyard stealth", "Skips", 0, 1, 1, nullptr},
    {"tower_escape", "Skip Ganon's Tower escape", "Skips", 0, 1, 1, nullptr},
    {"epona_race", "Skip Epona's race", "Skips", 0, 1, 0, nullptr},
    {"minigame_phases", "Skip repeated minigame phases", "Skips", 0, 1, 0, nullptr},
    {"four_poes", "Skip Forest Temple Poe introduction", "Skips", 0, 1, 1, nullptr},
    {"lake_owl", "Skip Lake Hylia owl interaction", "Skips", 0, 1, 1, nullptr},
    {"song_replays", "Song replays", "Skips", 0, 2, 0, "Play normally\0Skip without sound\0Skip with sound\0"},
    {"hint_access", "Gossip Stone hints", "Hints and spoiler log", 0, 3, 1, "Disabled\0No item required\0Require Mask of Truth\0Require Shard of Agony\0"},
    {"hint_distribution", "Hint distribution", "Hints and spoiler log", 0, 4, 0, "Balanced\0Strong\0Very strong\0Playthrough\0Useless\0"},
    {"spoiler_log", "Generate spoiler log", "Hints and spoiler log", 0, 1, 1, nullptr},
    {"shopsanity", "Shopsanity", "Item shuffling", 0, 6, 0, "Off\0Shuffle shop inventory only\0" "1 shuffled reward per shop\0" "2 shuffled rewards per shop\0" "3 shuffled rewards per shop\0" "4 shuffled rewards per shop\0Random (1-4 per shop)\0"},
    {"shop_prices", "Shopsanity prices", "Item shuffling", 0, 5, 0, "Random (0-295 rupees)\0Affordable (10 rupees)\0Child wallet\0Adult wallet\0Giant's Wallet\0Tycoon's Wallet\0"},
    {"tokensanity", "Tokensanity", "Item shuffling", 0, 3, 0, "Off\0Dungeon Skulltulas\0Overworld Skulltulas\0All Skulltulas\0"},
}};
struct Config {
    bool enabled = false;
    std::string seed;
    // Absent entries retain upstream defaults. Existing files never acquire
    // new draft settings when loaded, and keys are stable upstream names.
    std::map<std::string, int> advanced;
    std::array<int, OptionCount> options = [] {
        std::array<int, OptionCount> result{};
        for (unsigned i = 0; i < result.size(); ++i) result[i] = Specs[i].initial;
        return result;
    }();
    bool operator==(const Config&) const = default;
};
// Only future-file drafts receive product defaults. Config{} and FromJson
// retain legacy semantics so opening an old solo seed cannot enable networking.
inline Config NewFileConfig() {
    Config value;
    value.advanced["MP_Enabled"] = 1;
    value.advanced["MP_SharedProgress"] = 1;
    return value;
}
inline Config Sanitize(Config value) {
    if (value.seed.size() > 128) value.seed.resize(128);
    for (unsigned i = 0; i < Specs.size(); ++i)
        if (value.options[i] < Specs[i].low || value.options[i] > Specs[i].high)
            value.options[i] = Specs[i].initial;
    return value;
}
inline bool UnsupportedMasterQuest(const Config& config,bool mq) {
    const auto option=[&](const char* key) {
        const auto it=config.advanced.find(key);
        return it==config.advanced.end()?0:it->second;
    };
    // Same guard as the seed generator; No Logic and Vanilla remain allowed.
    return config.enabled && mq && option("Logic")<2 &&
        (option("Enemizer")!=0 || option("ShuffleEnemySouls")==1);
}
} // namespace OOT3D::Randomizer
