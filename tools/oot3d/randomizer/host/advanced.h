#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <map>
#include <stdexcept>
#include "settings.hpp"
#include "item_location.hpp"
#include "enemizer.hpp"

struct AdvancedBinding { std::string key, group; Option* option; };
inline std::vector<AdvancedBinding> AdvancedBindings() {
    std::vector<AdvancedBinding> result;
#define OPTION(name, group) result.push_back({#name, group, &Settings::name});
#include "advanced_options.inc"
#undef OPTION
    for(unsigned i=0;i<Settings::enemizerListOptions.size();++i)
        if(!Enemizer::enemyTypes[i].validLocTypes.empty())
            result.push_back({"Enemy/"+std::to_string(i),"Enemy pool",Settings::enemizerListOptions[i]});
    for(unsigned i=0;i<Settings::startingEnemySoulsOptions.size();++i)
        result.push_back({"StartingSoul/"+std::to_string(i),"Starting enemy souls",Settings::startingEnemySoulsOptions[i]});
    for(unsigned i=0;i<Settings::startingOcarinaButtonsOptions.size();++i)
        result.push_back({"StartingNote/"+std::to_string(i),"Starting ocarina buttons",Settings::startingOcarinaButtonsOptions[i]});
    for(auto key:everyPossibleLocation)
        if(key!=GANON && key!=MARKET_BOMBCHU_BOWLING_BOMBCHUS)result.push_back({"Exclude/"+std::to_string(key),"Excluded locations",Location(key)->GetExcludedOption()});
    return result;
}
inline QJsonArray AdvancedCatalog() {
    QJsonArray result;
    std::map<std::string,bool> seen;
    for(const auto& entry:AdvancedBindings()) {
        if(!seen.emplace(entry.key,true).second)throw std::runtime_error("Duplicate randomizer option: "+entry.key);
        QJsonArray choices;
        for(const auto& text:entry.option->GetOptions())choices.append(QString::fromStdString(text));
        if(entry.key=="ShuffleBGM" || entry.key=="ShuffleMelodies")choices.removeLast(); // Custom-archive-only mode.
        QJsonArray descriptions;
        const auto selected=entry.option->GetSelectedOptionIndex();
        for(int i=0;i<choices.size();++i) {
            entry.option->SetSelectedIndex(i);
            descriptions.append(QString::fromUtf8(entry.option->GetSelectedOptionDescription()).replace('\n',' '));
        }
        entry.option->SetSelectedIndex(selected);
        bool varying=false;
        for(const auto& d:descriptions)if(d!=descriptions[0])varying=true;
        if(!varying)descriptions={};
        auto label=QString::fromStdString(entry.option->GetName()).simplified();
        result.append(QJsonObject{{"key",QString::fromStdString(entry.key)}, {"label",label},
            {"group",QString::fromStdString(entry.group)}, {"choices",choices},
            {"default",entry.option->GetSelectedOptionIndex()}, {"descriptions",descriptions},
            {"description",QString::fromUtf8(entry.option->GetSelectedOptionDescription()).replace('\n',' ')}});
    }
    return result;
}
inline void ConfigureAdvanced(const std::map<std::string,int>& values) {
    std::map<std::string,Option*> options;
    for(const auto& entry:AdvancedBindings())options.emplace(entry.key,entry.option);
    for(const auto& [key,value]:values) {
        const auto it=options.find(key);
        if(it==options.end() || value<0 || unsigned(value)>=it->second->GetOptionCount() ||
           ((key=="ShuffleBGM" || key=="ShuffleMelodies") && value>2))
            throw std::runtime_error("Invalid randomizer option: "+key);
        it->second->SetSelectedIndex(value);
        it->second->Unhide(); // Validate explicit selections instead of silently overriding them.
    }
}
