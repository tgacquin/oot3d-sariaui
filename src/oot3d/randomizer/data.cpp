// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/randomizer/data.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <stdexcept>

static void InitializeResources() { Q_INIT_RESOURCE(oot_randomizer_assets); }

namespace OOT3D::Randomizer {
Symbol GuestData::Find(const char* name) const {
    const auto object = symbols.value(QLatin1String(name)).toObject();
    return {std::uint32_t(object.value("address").toInteger()),
            std::uint32_t(object.value("size").toInteger())};
}
const GuestData& Data() {
    static const GuestData data = [] {
        InitializeResources();
        const auto read = [](const char* path) {
            QFile file{QLatin1String(path)};
            if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing randomizer resources");
            return file.readAll();
        };
        GuestData result;
        result.bytes = read(":/oot3d/randomizer/guest.bin");
        result.assets = read(":/oot3d/randomizer/assets.zar");
        const auto metadata = read(":/oot3d/randomizer/guest.json");
        result.digest = QCryptographicHash::hash(result.bytes + metadata, QCryptographicHash::Sha256).toHex();
        const auto rewards=QJsonDocument::fromJson(read(":/oot3d/randomizer/rewards.json")).object();
        if(rewards.value("runtime").toString().toLatin1()!=result.digest)
            throw std::runtime_error("Reward extension requires rebuilding against the bundled runtime");
        result.reward_base=std::uint32_t(rewards.value("base").toInteger());
        result.rewards=read(":/oot3d/randomizer/rewards.bin");
        result.reward_symbols=rewards.value("symbols").toObject();
        if(result.reward_base!=0x900000 || result.rewards.size()>0x10000 ||
           GuestBase+result.bytes.size()>result.reward_base)
            throw std::runtime_error("Invalid reward extension reservation");
        for(const auto& value:rewards.value("hooks").toArray()) {
            const auto h=value.toObject();
            const auto address=std::uint32_t(h.value("address").toInteger());
            const auto instruction=std::uint32_t(h.value("instruction").toInteger());
            const auto target=std::uint32_t(h.value("target").toInteger());
            if(address<GuestBase || std::uint64_t(address)+4>GuestBase+result.bytes.size() ||
               target<result.reward_base || target>=result.reward_base+result.rewards.size() ||
               (address|target)&3)
                throw std::runtime_error("Invalid reward extension hook");
            std::uint32_t original=0;
            for(unsigned i=0;i<4;++i)original|=std::uint32_t(std::uint8_t(result.bytes[address-GuestBase+i]))<<(8*i);
            if(original!=instruction)throw std::runtime_error("Reward extension hook does not match runtime");
            result.reward_hooks.push_back({address,instruction,target});
        }
        const auto root = QJsonDocument::fromJson(metadata).object();
        if (root.value("base").toInteger() != GuestBase || result.bytes.size() > GuestEnd - GuestBase)
            throw std::runtime_error("Invalid randomizer payload");
        result.symbols = root.value("symbols").toObject();
        for (const auto& value : root.value("patches").toArray()) {
            const auto patch = value.toObject();
            result.patches.push_back({std::uint32_t(patch.value("address").toInteger()),
                QByteArray::fromBase64(patch.value("data").toString().toLatin1()),
                patch.value("name").toString().toStdString()});
        }
        return result;
    }();
    return data;
}
QJsonObject ToJson(const Config& config) {
    QJsonObject result{{"enabled", config.enabled}, {"seed", QString::fromStdString(config.seed)}};
    for (unsigned i = 0; i < Specs.size(); ++i) result[QLatin1String(Specs[i].key)] = config.options[i];
    QJsonObject advanced;
    for(const auto& [key,value]:config.advanced)advanced[QString::fromStdString(key)]=value;
    if(!advanced.isEmpty())result["advanced"]=advanced;
    return result;
}
Config FromJson(const QJsonObject& object) {
    Config result;
    result.enabled = object.value("enabled").toBool();
    result.seed = object.value("seed").toString().toStdString();
    for (unsigned i = 0; i < Specs.size(); ++i)
        result.options[i] = object.value(QLatin1String(Specs[i].key)).toInt(Specs[i].initial);
    const auto advanced=object.value("advanced").toObject();
    for(auto it=advanced.begin();it!=advanced.end();++it)
        result.advanced[it.key().toStdString()]=it.value().toInt(-1);
    return Sanitize(result);
}
const std::vector<AdvancedOption>& AdvancedOptions() {
    static const auto options=[] {
        InitializeResources();
        QFile file(":/oot3d/randomizer/options.json");
        if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Missing randomizer option catalog");
        std::vector<AdvancedOption> result;
        for(const auto& value:QJsonDocument::fromJson(file.readAll()).array()) {
            const auto object=value.toObject();
            AdvancedOption option{object["key"].toString().toStdString(),object["label"].toString().toStdString(),
                object["group"].toString().toStdString(),object["description"].toString().toStdString(),{},{},object["default"].toInt()};
            for(const auto& choice:object["choices"].toArray())option.choices.push_back(choice.toString().toStdString());
            for(const auto& description:object["descriptions"].toArray())option.descriptions.push_back(description.toString().toStdString());
            result.push_back(std::move(option));
        }
        return result;
    }();
    return options;
}
bool WriteJson(const QString& path, const QJsonObject& object) {
    QSaveFile file(path);
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
QJsonObject ReadJson(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
} // namespace OOT3D::Randomizer
