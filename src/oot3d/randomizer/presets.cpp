// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/randomizer/presets.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <mutex>

namespace OOT3D::Randomizer::Presets {
namespace {
std::mutex mutex;
std::vector<Entry> custom;
std::string storage_error;
bool readable=true;
constexpr unsigned MaximumPresets=64;
QString Fold(const std::string& name) {return QString::fromStdString(name).toCaseFolded();}
std::string CleanName(const std::string& name) {
    return QString::fromStdString(name).simplified().toStdString();
}
bool ValidName(const std::string& name) {return !name.empty() && name.size()<=96 && name.find("##")==std::string::npos;}
bool ValidConfig(const Config& config) {
    if(Sanitize(config)!=config)return false;
    for(const auto& [key,value]:config.advanced) {
        const auto& catalog=AdvancedOptions();
        const auto it=std::find_if(catalog.begin(),catalog.end(),[&](const auto& o){return o.key==key;});
        if(it==catalog.end() || value<0 || std::size_t(value)>=it->choices.size())return false;
    }
    return true;
}
Config Capture(Config config) {
    config.enabled=false;config.seed.clear();
    // Pin absent advanced defaults so a saved custom preset is a complete recipe.
    for(const auto& option:AdvancedOptions())config.advanced.try_emplace(option.key,option.initial);
    for(auto it=config.advanced.begin();it!=config.advanced.end();)
        if(it->first.starts_with("MP_"))it=config.advanced.erase(it);else ++it;
    return config;
}
}
const std::vector<Entry>& BuiltIns() {
    // Adapted from Shipwright's Beginner / Standard / Advanced seed presets:
    // https://github.com/HarbourMasters/Shipwright/tree/develop/soh/assets/custom/presets
    // Match intent using this generator's option meanings, not SoH enum numbers.
    static const auto entries=[] {
        std::vector<Entry> result{{"Defaults","Restore the standard OoT3D randomizer rules.",{},true}};
        Config beginner;
        beginner.options[Fortress]=1;beginner.options[KokiriSword]=1;
        beginner.options[Ocarina]=1;beginner.options[Maps]=5;
        beginner.options[EponaRace]=1;beginner.options[HintDistribution]=2;
        beginner.options[GanonKey]=9;
        beginner.advanced={{"Logic",0},{"ZorasFountain",1},{"Bridge",4},{"BridgeRewardCount",6},
            {"RandomGanonsTrials",0},{"GanonsTrialsCount",0},{"LACSRewardCount",6},
            {"ShuffleOcarinas",1},{"FreeScarecrow",1},{"BigPoeTargetCount",0},
            {"NumRequiredCuccos",1},{"ClearerHints",2},{"BonusGossipHints",1},
            {"Exclude/448",1},{"Exclude/449",1},{"Exclude/569",1},{"Exclude/666",1}};
        result.push_back({"Beginner","Glitchless; start with sword, ocarina and maps. Keys stay in their dungeons. Strong hints, six rewards, no trials; four lengthy checks excluded.",beginner,true});
        Config standard=beginner;
        standard.options[KokiriSword]=0;standard.options[SmallKeys]=4;
        standard.options[Songs]=1;standard.options[Tokens]=3;
        standard.options[Shops]=5;standard.options[ShopPrices]=1;
        standard.options[HintDistribution]=0;
        for(const auto* key:{"Exclude/448","Exclude/449","Exclude/569","Exclude/666"})standard.advanced.erase(key);
        standard.advanced["BridgeRewardCount"]=7;standard.advanced["LACSRewardCount"]=7;
        standard.advanced["ShuffleKokiriSword"]=1;standard.advanced["ShuffleGerudoToken"]=1;
        standard.advanced["KeyRings"]=1;standard.advanced["Scrubsanity"]=1;
        standard.advanced["ShuffleMerchants"]=2;
        result.push_back({"Standard","Glitchless; seven rewards, shuffled songs and tokens, key rings anywhere, affordable shops and scrubs. Entrances stay familiar.",standard,true});
        Config advanced=standard;
        advanced.options[StartingAge]=2;advanced.options[Ocarina]=0;
        advanced.options[BossKeys]=4;advanced.options[Shops]=6;
        advanced.advanced["BridgeRewardCount"]=8;advanced.advanced["LACSRewardCount"]=8;
        advanced.advanced["ShuffleEntrances"]=1;advanced.advanced["ShuffleDungeonEntrances"]=2;
        advanced.advanced["ShuffleBossEntrances"]=1;advanced.advanced["ShuffleOverworldSpawns"]=1;
        advanced.advanced["ShuffleCows"]=1;advanced.advanced["ShuffleAdultTradeQuest"]=1;
        advanced.advanced["ShuffleMasterSword"]=1;
        result.push_back({"Advanced","Glitchless; eight rewards, random starting age, dungeon and age-matched boss entrances, shuffled spawns, cows and adult trade. Boss keys anywhere.",advanced,true});
        return result;
    }();
    return entries;
}
Config Apply(const Entry& preset,const Config& current) {
    Config next=preset.config;next.enabled=current.enabled;next.seed=current.seed;
    // Keep multiplayer session choices out of single-player difficulty presets.
    for(auto it=next.advanced.begin();it!=next.advanced.end();)
        if(it->first.starts_with("MP_"))it=next.advanced.erase(it);else ++it;
    for(const auto& [key,value]:current.advanced)if(key.starts_with("MP_"))next.advanced[key]=value;
    return next;
}
std::vector<Entry> List() {
    std::scoped_lock lock(mutex);auto result=BuiltIns();
    result.insert(result.end(),custom.begin(),custom.end());return result;
}
std::string SaveNew(std::string name,const Config& config) {
    name=CleanName(name);
    if(!ValidName(name))return "Enter a short name without ##.";
    if(!ValidConfig(config))return "These settings contain an unsupported option.";
    const auto entries=List();
    for(const auto& e:entries)if(Fold(e.name)==Fold(name))return "That name exists. Use Update selected or a new name.";
    std::scoped_lock lock(mutex);
    if(!readable)return storage_error;
    if(custom.size()>=MaximumPresets)return "Preset limit reached (64). Delete one first.";
    custom.push_back({name,"Your saved randomizer settings.",Capture(config),false});dirty=true;return {};
}
std::string Update(const std::string& name,const Config& config) {
    if(!ValidConfig(config))return "These settings contain an unsupported option.";
    std::scoped_lock lock(mutex);
    if(!readable)return storage_error;
    for(auto& e:custom)if(e.name==name){e.config=Capture(config);dirty=true;return {};}
    return "Select a custom preset to update.";
}
std::string Remove(const std::string& name) {
    std::scoped_lock lock(mutex);
    if(!readable)return storage_error;
    const auto it=std::find_if(custom.begin(),custom.end(),[&](const auto& e){return e.name==name;});
    if(it==custom.end())return "Select a custom preset to delete.";
    custom.erase(it);dirty=true;return {};
}
void Load(QSettings& settings) {
    std::vector<Entry> loaded;
    const auto bytes=settings.value(QStringLiteral("RandomizerPresets/library")).toByteArray();
    bool valid=true;
    if(!bytes.isEmpty()) {
        const auto object=QJsonDocument::fromJson(bytes).object();
        valid=bytes.size()<=4*1024*1024 && object["format"].toInt()==1 && object["presets"].isArray();
        const auto array=object["presets"].toArray();valid&=array.size()<=MaximumPresets;
        for(const auto& item:array) {
            const auto o=item.toObject();const auto name=CleanName(o["name"].toString().toStdString());
            const auto json=o["config"].toObject();const auto config=FromJson(json);
            valid&=ValidName(name) && !json.isEmpty() && ToJson(config)==json && ValidConfig(config);
            for(const auto& e:BuiltIns())valid&=Fold(e.name)!=Fold(name);
            for(const auto& e:loaded)valid&=Fold(e.name)!=Fold(name);
            loaded.push_back({name,"Your saved randomizer settings.",Capture(config),false});
        }
    }
    std::scoped_lock lock(mutex);readable=valid;custom=valid?std::move(loaded):std::vector<Entry>{};
    storage_error=valid?"":"Could not read saved presets. The stored library was left unchanged.";dirty=false;
}
void Save(QSettings& settings) {
    // Mutations occur on the UI thread; persistence runs on Qt's thread.
    std::scoped_lock lock(mutex);
    if(!readable)return;
    QJsonArray array;
    for(const auto& e:custom)array.append(QJsonObject{{"name",QString::fromStdString(e.name)},{"config",ToJson(e.config)}});
    const QJsonObject object{{"format",1},{"presets",array}};
    settings.setValue(QStringLiteral("RandomizerPresets/library"),QJsonDocument(object).toJson(QJsonDocument::Compact));
    settings.sync();
    storage_error=settings.status()==QSettings::NoError?"":"Could not save presets. Check folder permissions.";
    if(!storage_error.empty())dirty=true;
}
std::string StorageError() {std::scoped_lock lock(mutex);return storage_error;}
} // namespace OOT3D::Randomizer::Presets
