#include <cstdio>
#include <stdexcept>
#include <QCoreApplication>
#include <QJsonArray>
#include <QRandomGenerator>
#include "oot3d/randomizer/data.h"
#include "oot3d/randomizer/difficulty.h"
#include "hint_list.hpp"
#include "item_list.hpp"
#include "item_location.hpp"
#include "enemizer.hpp"
#include "settings.hpp"
#include "playthrough.hpp"
#include "random.hpp"
#include "custom_messages.hpp"
#include "spoiler_log.hpp"
#include "hints.hpp"
#include "patch.hpp"
#include "shops.hpp"
#include "advanced.h"
#include "runtime_tables.h"
#include <QJsonDocument>
#include <QFile>
#include <QSaveFile>
#include "fill.hpp"
#include "s_dungeon.h"
#include "oot3d/randomizer/generation_progress.h"

namespace R = OOT3D::Randomizer;
static_assert(FillAttemptLimit==R::GenerationProgress::AttemptLimit);
static QString progress_path;
static void ReportAttempt(unsigned attempt) {
    if(progress_path.isEmpty())return;
    QSaveFile file(progress_path);
    const auto bytes=QByteArray::number(attempt)+'\n';
    if(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size())file.commit();
}

static void CheckItemOverrides(const std::vector<ItemOverride>& items, const R::GuestData& data) {
    // Match ItemTable_GetItemRow in the shipped ARM payload. Each 24-byte row
    // starts with baseItemId; a zero there marks a hole in the table.
    constexpr unsigned rowSize = 24;
    const auto table = data.Find("rItemTable");
    if (table.address < R::GuestBase || !table.size || table.size % rowSize ||
        std::uint64_t(table.address - R::GuestBase) + table.size > std::uint64_t(data.bytes.size()))
        throw std::runtime_error("Invalid bundled item table");
    const auto validItem = [&](unsigned id) {
        return id < table.size / rowSize && data.bytes.at(table.address - R::GuestBase + id * rowSize) != 0;
    };
    u32 previousKey = 0;
    for (const auto& entry : items) {
        if (entry.key.all <= previousKey)
            throw std::runtime_error("Unordered or empty item location key: " + std::to_string(entry.key.all));
        previousKey = entry.key.all;
        // Regular shop stock indexes the game's 50 EnGirlA shop entries, including ID zero.
        const auto id = entry.value.itemId;
        if (entry.value.player == 0xff ? id >= 50 : !validItem(id))
            throw std::runtime_error("Invalid item " + std::to_string(id) + " at location " + std::to_string(entry.key.all));
        const auto appearance = entry.value.looksLikeItemId;
        if (appearance && !validItem(appearance))
            throw std::runtime_error("Invalid appearance " + std::to_string(appearance) + " at location " + std::to_string(entry.key.all));
    }
}

static void CheckDungeonRewards(const SettingsContext& settings) {
    constexpr u32 rewardBits = 0x001c003f; // Quest inventory bits for the three stones and six medallions.
    const auto pocket = settings.linksPocketRewardBitMask;
    if (((settings.startingDungeonReward | pocket) & ~rewardBits) || (pocket & (pocket - 1)))
        throw std::runtime_error("Invalid starting dungeon reward bits");
    if (settings.shuffleRewards != REWARDSHUFFLE_END_OF_DUNGEON) return;

    const auto& rewards = Settings::rDungeonRewardOverrides;
    u32 placed = 0;
    u32 expectedPocket = 0;
    for (std::size_t slot = 0; slot < rewards.size(); ++slot) {
        const auto id = rewards[slot];
        if (id == INVALID_DUNGEON_REWARD) continue;
        if (id < 0 || id >= DUNGEON_REWARDS_COUNT)
            throw std::runtime_error("Invalid dungeon reward " + std::to_string(id) + " at slot " + std::to_string(slot));
        const u32 bit = 1u << (id < R_FOREST_MEDALLION ? 18 + id : id - R_FOREST_MEDALLION);
        if (placed & bit) throw std::runtime_error("Duplicate dungeon reward " + std::to_string(id));
        placed |= bit;
        if (slot + 1 == rewards.size()) expectedPocket = bit;
    }
    if (pocket != expectedPocket)
        throw std::runtime_error("Link's Pocket disagrees with the dungeon reward table");
    // Vanilla logic keeps rewards in their original locations even when starting with them.
    if (settings.logic != LOGIC_VANILLA &&
        ((placed & settings.startingDungeonReward) || (placed | settings.startingDungeonReward) != rewardBits))
        throw std::runtime_error("Placed dungeon rewards disagree with starting inventory");
}

static void Configure(R::Config& config, bool mq, const QJsonObject& difficulty) {
    const auto& c = config.options;
    using namespace Settings;
    StartingAge.SetSelectedIndex(std::array{AGE_CHILD,AGE_ADULT,AGE_RANDOM}.at(c[R::StartingAge]));
    OpenForest.SetSelectedIndex(c[R::Forest]); OpenDoorOfTime.SetSelectedIndex(c[R::DoorOfTime]);
    GerudoFortress.SetSelectedIndex(c[R::Fortress]);
    ShuffleRewards.SetSelectedIndex(std::array{0,3,1,2}.at(c[R::Rewards]));
    ShuffleSongs.SetSelectedIndex(std::array{0,2,1}.at(c[R::Songs]));
    Shopsanity.SetSelectedIndex(c[R::Shops]);
    ShopsanityPrices.SetSelectedIndex(c[R::ShopPrices]);
    Tokensanity.SetSelectedIndex(c[R::Tokens]);
    MapsAndCompasses.SetSelectedIndex(std::array{1,2,3,4,5,0,7,8,9,10,11,12,13}.at(c[R::Maps])); Keysanity.SetSelectedIndex(std::array{1,2,3,4,5,0,7,8,9,10,11,12,13}.at(c[R::SmallKeys]));
    BossKeysanity.SetSelectedIndex(std::array{1,2,3,4,5,0,7,8,9,10,11,12,13}.at(c[R::BossKeys])); GanonsBossKey.SetSelectedIndex(std::array{1,2,3,4,5,0,7,8,9,10,11,12,13}.at(c[R::GanonKey]));
    MQDungeonCount.SetSelectedIndex(mq ? 12 : 0);
    Settings::MirrorWorld.SetSelectedIndex(mq ? MIRRORWORLD_ON : MIRRORWORLD_OFF);
    StartingHearts.SetSelectedIndex(c[R::Hearts]); StartingOcarina.SetSelectedIndex(c[R::Ocarina]);
    StartingMagicMeter.SetSelectedIndex(c[R::Magic]); StartingHookshot.SetSelectedIndex(c[R::Hookshot]);
    StartingBombBag.SetSelectedIndex(c[R::BombBag]); StartingBow.SetSelectedIndex(c[R::Bow]);
    StartingSlingshot.SetSelectedIndex(c[R::Slingshot]);
    const std::array bottles{&StartingBottle1, &StartingBottle2, &StartingBottle3, &StartingBottle4};
    for (unsigned i = 0; i < bottles.size(); ++i) bottles[i]->SetSelectedIndex(i < unsigned(c[R::Bottles]) ? 1 : 0);
    const std::array gear{&StartingKokiriSword, &StartingDekuShield, &StartingHylianShield, &StartingMirrorShield,
        &StartingGoronTunic, &StartingZoraTunic, &StartingIronBoots, &StartingHoverBoots, &StartingBoomerang,
        &StartingLensOfTruth, &StartingMegatonHammer, &StartingDinsFire, &StartingFaroresWind, &StartingNayrusLove,
        &StartingStrength, &StartingScale, &StartingWallet, &StartingConsumables, &StartingMaxRupees};
    for (unsigned i = 0; i < gear.size(); ++i) gear[i]->SetSelectedIndex(c[R::KokiriSword + i]);
    SkipChildStealth.SetSelectedIndex(c[R::ChildStealth]); SkipTowerEscape.SetSelectedIndex(c[R::TowerEscape]);
    SkipEponaRace.SetSelectedIndex(c[R::EponaRace]); SkipMinigamePhases.SetSelectedIndex(c[R::MinigamePhases]);
    FourPoesCutscene.SetSelectedIndex(c[R::FourPoes]); LakeHyliaOwl.SetSelectedIndex(c[R::LakeOwl]);
    SkipSongReplays.SetSelectedIndex(c[R::SongReplays]);
    GossipStoneHints.SetSelectedIndex(c[R::HintAccess]); HintDistribution.SetSelectedIndex(std::array{1,2,3,4,0}.at(c[R::HintDistribution]));
    // Glitchless, normal entrances/enemies, fixed goals.
    Settings::Logic.SetSelectedIndex(LOGIC_GLITCHLESS);
    RandomGanonsTrials.SetSelectedIndex(OFF); GanonsTrialsCount.SetSelectedIndex(6);
    Bridge.SetSelectedIndex(RAINBOWBRIDGE_MEDALLIONS); BridgeMedallionCount.SetSelectedIndex(6);
    GenerateSpoilerLog.SetSelectedIndex(OFF); // Export below to the per-file bundle.
    FreeCamControl.SetSelectedIndex(0);
    const unsigned damage = difficulty["damage"].toInt() * (mq ? 2 : 1);
    DamageMultiplier.SetSelectedIndex(damage <= 1 ? DAMAGEMULTIPLIER_DEFAULT :
        damage <= 2 ? DAMAGEMULTIPLIER_DOUBLE : damage <= 4 ? DAMAGEMULTIPLIER_QUADRUPLE :
        damage <= 8 ? DAMAGEMULTIPLIER_OCTUPLE : damage <= 16 ? DAMAGEMULTIPLIER_SEXDECUPLE : DAMAGEMULTIPLIER_OHKO);
    AmmoDrops.SetSelectedIndex(difficulty["ammo"].toBool() ? AMMODROPS_NONE : AMMODROPS_VANILLA);
    HeartDropRefill.SetSelectedIndex(difficulty["hearts"].toBool() ? HEARTDROPREFILL_NODROP : HEARTDROPREFILL_VANILLA);
    ExtraShields.SetSelectedIndex(EXTRASHIELDS_NEVER);
    // Our mod owns these behaviors; the guest payload does not replace them.
    QuickText.SetSelectedIndex(QUICKTEXT_VANILLA);
    ConfigureAdvanced(config.advanced);
    {
        if(TriforceHunt && TriforcePiecesRequired.Value<u8>()>TriforcePiecesTotal.Value<u8>())
            throw std::runtime_error("Triforce pieces required cannot exceed the total placed.");
        if(StartingAge.Is(AGE_ADULT) && (OpenForest.Is(OPENFOREST_CLOSED) ||
           (OpenDoorOfTime.Is(OPENDOOROFTIME_INTENDED) && !ShuffleOcarinas && StartingOcarina.Is(OFF))))
            throw std::runtime_error("Adult start requires an open forest and access to the Door of Time.");
        // Use the same dependency rules as the original randomizer's menus.
        ForceChange(0,nullptr);
        for(const auto& entry:AdvancedBindings()) {
            if(config.advanced.contains(entry.key)) {
                if(entry.key.starts_with("Exclude/") && config.advanced[entry.key] && !entry.option->GetSelectedOptionIndex())
                    throw std::runtime_error("This location cannot be excluded with the selected shuffle settings: "+entry.option->GetName());
                config.advanced[entry.key]=entry.option->GetSelectedOptionIndex();
            }
        }
    }
    // Dungeon layout always belongs to the selected Original/MQ file mode.
    SetDungeonTypes.SetSelectedIndex(OFF);
    MQDungeonCount.SetSelectedIndex(mq ? 12 : 0);
    if (difficulty["prices"].toBool()) StartingWallet.SetSelectedIndex(2);
    CustomMessages::SetHostPriceMultiplier(difficulty["prices"].toBool()?difficulty["price_multiplier"].toInt():1);
    seed = config.seed;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        if(argc==3 && std::string(argv[1])=="--catalog") {
            HintTable_Init();ItemTable_Init();LocationTable_Init();Enemizer::InitEnemyTypes();Settings::InitSettings();
            R::Config defaults; Configure(defaults,false,R::CaptureDifficulty());
            QFile file(QString::fromLocal8Bit(argv[2]));
            if(!file.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot write option catalog");
            const auto bytes=QJsonDocument(AdvancedCatalog()).toJson(QJsonDocument::Indented);
            if(file.write(bytes)!=bytes.size())throw std::runtime_error("Cannot finish option catalog");
            return 0;
        }
        if (argc != 4 && argc != 5) throw std::runtime_error("Usage: oot3d-seed config.json original|mq output.json [progress.txt]");
        if(argc==5)progress_path=QString::fromLocal8Bit(argv[4]);
        fillAttemptCallback=ReportAttempt;
        const auto input = R::ReadJson(QString::fromLocal8Bit(argv[1]));
        if (input.isEmpty()) throw std::runtime_error("Cannot read randomizer configuration");
        auto config = R::FromJson(input);
        if (config.seed.empty()) config.seed = QString::number(QRandomGenerator::system()->generate64(), 16).toStdString();
        const bool mq = std::string(argv[2]) == "mq";
        HintTable_Init(); ItemTable_Init(); LocationTable_Init(); Enemizer::InitEnemyTypes(); Settings::InitSettings();
        auto difficulty = input.value("difficulty").toObject();
        if (difficulty.isEmpty()) difficulty = R::CaptureDifficulty();
        if (!R::ValidDifficulty(difficulty)) throw std::runtime_error("Invalid difficulty settings");
        if(difficulty["prices"].toBool())config.options[R::Wallet]=2;
        Configure(config, mq, difficulty);
        if(mq && Settings::Logic.IsNot(LOGIC_NONE) && Settings::Logic.IsNot(LOGIC_VANILLA) &&
           (Settings::Enemizer || Settings::ShuffleEnemySouls.Is(SHUFFLEENEMYSOULS_ALL)))
            throw std::runtime_error("Master Quest logic does not support Enemizer or all-enemy souls upstream. Choose Original, boss-only souls, or explicitly select No Logic.");
        if (!Settings::ValidateSettings()) throw std::runtime_error("Conflicting goals and item pool: check heart requirements and Gloom Mode.");
        if (Playthrough::Playthrough_Init(StableSeedHash(config.seed)) != 1)
            throw std::runtime_error("No beatable placement found after ten attempts. Try another seed.");
        const auto& data = R::Data();
        QJsonObject fields;
        const auto put = [&](const char* name, const void* bytes, std::size_t size) {
            const auto symbol = data.Find(name);
            if (!symbol.address || size > symbol.size) throw std::runtime_error(std::string("Payload field mismatch: ") + name);
            fields[QLatin1String(name)] = QString::fromLatin1(QByteArray(static_cast<const char*>(bytes), size).toBase64());
        };
        std::vector<ItemOverride> items(overrides.begin(), overrides.end());
        if (items.size() >= ITEM_OVERRIDES_MAX) throw std::runtime_error("Too many item overrides");
        CheckItemOverrides(items, data);
        put("rItemOverrides", items.data(), items.size() * sizeof(ItemOverride));
        const auto settings = Settings::FillContext();
        CheckDungeonRewards(settings);
        put("gSettingsContext", &settings, sizeof(settings));
        std::array<std::int32_t, 32> shop_prices{};
        if (config.options[R::Shops] >= SHOPSANITY_ONE) {
            if (NonShopItems.size() != shop_prices.size()) throw std::runtime_error("Missing shopsanity prices");
            for (unsigned i = 0; i < shop_prices.size(); ++i) shop_prices[i] = NonShopItems[i].Price;
        }
        put("rShopsanityPrices", shop_prices.data(), sizeof(shop_prices));
        put("gSpoilerData", GetSpoilerData(), sizeof(SpoilerData));
        // ARM EABI uses short enums here; serialize the 10-byte records explicitly.
        QByteArray locations;
        const auto half = [&](unsigned value) { locations += char(value); locations += char(value >> 8); };
        for (unsigned i = 0; i < SPOILER_LOCDATS; ++i) {
            const auto& block = *GetSpoilerDataLocs(i);
            for (const auto& item : block.ItemLocations) {
                half(item.LocationStrOffset); half(item.ItemStrOffset);
                locations += char(item.CollectionCheckType); locations += char(item.LocationScene);
                locations += char(item.LocationFlag); locations += char(item.Group);
                locations += char(item.CollectType); locations += char(item.RevealType);
            }
            locations.append(block.StringData, SPOILER_STRING_DATA_SIZE);
        }
        put("gSpoilerDataLocs", locations.data(), locations.size());
        if (!config.options[R::Rewards]) put("rDungeonRewardOverrides", &Settings::rDungeonRewardOverrides, sizeof(Settings::rDungeonRewardOverrides));
        QByteArray importance;
        for (auto value : dungeonImportance) importance += char(value);
        put("rDungeonImportance", importance.data(), importance.size());
        const auto native_patches=RuntimeTables(put);
        const auto messages = CustomMessages::RawMessageData();
        const auto entries = CustomMessages::RawMessageEntryData();
        QJsonObject result{{"format", 2}, {"native_patches",native_patches}, {"runtime", QString::fromLatin1(data.digest)},
            {"config", R::ToJson(config)}, {"difficulty", difficulty}, {"mq", mq}, {"fields", fields},
            {"message_data", QString::fromLatin1(QByteArray(messages.first, messages.second).toBase64())},
            {"message_entries", QString::fromLatin1(QByteArray(entries.first, entries.second).toBase64())},
            {"message_count", int(CustomMessages::NumMessages())},
            {"hash", QString::fromStdString(GetRandomizerHash()[0] + ", " + GetRandomizerHash()[1] + ", " + GetRandomizerHash()[2] + ", " + GetRandomizerHash()[3] + ", " + GetRandomizerHash()[4])}};
        if (config.options[R::SpoilerLog]) {
            QJsonArray spoiler;
            for (auto key : allLocations) {
                const auto* location = Location(key);
                spoiler.append(QJsonObject{{"location", QString::fromStdString(location->GetName())},
                    {"item", QString::fromStdString(location->GetPlacedItemName().GetNAEnglish())}});
            }
            result["spoiler"] = spoiler;
        }
        if (!R::WriteJson(QString::fromLocal8Bit(argv[3]), result)) throw std::runtime_error("Cannot save generated seed");
        std::fprintf(stderr, "Generated %zu item overrides (%s)\n", items.size(), mq ? "MQ" : "Original");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
