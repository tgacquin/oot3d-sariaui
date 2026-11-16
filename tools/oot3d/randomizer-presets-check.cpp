// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/randomizer/presets.h"
#include "oot3d/randomizer/service.h"
#include "common/logging/backend.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    Common::Log::DisableLoggingInTests();
    namespace R=OOT3D::Randomizer;
    namespace P=R::Presets;
    QTemporaryDir dir;assert(dir.isValid());
    QSettings settings(dir.filePath("presets.ini"),QSettings::IniFormat);
    settings.setValue("HUD/authored",73);settings.setValue("Randomizer/seed","live-seed");
    const auto old_keys=settings.allKeys();P::Load(settings);
    assert(P::List().size()==4 && settings.allKeys()==old_keys && !P::dirty);
    // The draft default enables host/join, without changing legacy packages.
    R::Initialize(settings, dir.path().toStdString());
    assert(R::GetConfig().advanced.at("MP_Enabled") == 1);
    assert(R::NewFileConfig().advanced.at("MP_Enabled") == 1);
    assert(!R::FromJson(QJsonObject{}).advanced.contains("MP_Enabled"));
    for (const auto& preset : P::BuiltIns())
        assert(P::Apply(preset, R::GetConfig()).advanced.at("MP_Enabled") == 1);
    settings.setValue("Randomizer/advanced/MP_Enabled", 0);
    R::Initialize(settings, dir.path().toStdString());
    assert(R::GetConfig().advanced.at("MP_Enabled") == 0);
    for (const auto& preset : P::BuiltIns())
        assert(P::Apply(preset, R::GetConfig()).advanced.at("MP_Enabled") == 0);
    R::SaveConfig(settings);
    assert(settings.value("Randomizer/advanced/MP_Enabled").toInt() == 0);
    R::Config current;current.enabled=true;current.seed="keep-this-seed";
    current.options[R::Shops]=6;current.advanced={{"TriforceHunt",1},{"TriforcePiecesTotal",199},
        {"Logic",2},{"Enemizer",1},{"MP_Enabled",1},{"MP_SharedProgress",1}};
    const auto original=current;
    for(const auto& e:P::BuiltIns()) {
        assert(R::Sanitize(e.config)==e.config && e.built_in);
        for(const auto& [key,index]:e.config.advanced) {
            const auto& options=R::AdvancedOptions();
            const auto it=std::find_if(options.begin(),options.end(),[&](const auto& o){return o.key==key;});
            assert(it!=options.end() && index>=0 && unsigned(index)<it->choices.size());
        }
        const auto applied=P::Apply(e,current);
        assert(applied.seed==current.seed && applied.enabled==current.enabled);
        assert(applied.advanced.at("MP_Enabled")==1 && applied.advanced.at("MP_SharedProgress")==1);
        assert(!applied.advanced.contains("TriforceHunt") && !applied.advanced.contains("Enemizer"));
        assert(!R::UnsupportedMasterQuest(applied,true) && current==original);
        assert(P::Apply(e,applied)==applied); // Reapplying cannot accumulate old rules.
        if(argc==2) {
            QDir().mkpath(QString::fromLocal8Bit(argv[1]));
            auto export_config=e.config;export_config.enabled=true;export_config.seed="presets-regression-20261114";
            assert(R::WriteJson(QString::fromLocal8Bit(argv[1])+"/"+QString::fromStdString(e.name)+".json",R::ToJson(export_config)));
        }
    }
    // A custom snapshot pins every advanced default; exercise that full recipe too.
    assert(P::SaveNew("Saved Standard",P::BuiltIns()[2].config).empty());
    if(argc==2) {
        auto snapshot=P::List().back().config;snapshot.enabled=true;snapshot.seed="presets-regression-20261114";
        assert(R::WriteJson(QString::fromLocal8Bit(argv[1])+"/Saved Standard.json",R::ToJson(snapshot)));
    }
    assert(P::Remove("Saved Standard").empty());
    assert(!P::SaveNew("  ",current).empty());
    assert(!P::SaveNew("Beginner",current).empty());
    assert(!P::SaveNew("bad##name",current).empty());
    assert(P::SaveNew("  My   setup  ",current).empty());
    assert(!P::SaveNew("my setup",current).empty());
    auto saved=P::List().back();assert(saved.name=="My setup" && !saved.built_in);
    assert(saved.config.seed.empty() && !saved.config.enabled && !saved.config.advanced.contains("MP_Enabled"));
    assert(saved.config.advanced.at("TriforcePiecesTotal")==199);
    P::Save(settings);assert(P::StorageError().empty());
    assert(settings.value("HUD/authored").toInt()==73 && settings.value("Randomizer/seed")=="live-seed");
    QSettings reopened(dir.filePath("presets.ini"),QSettings::IniFormat);P::Load(reopened);
    assert(P::List().back().config==saved.config && !P::dirty);
    auto changed=current;changed.options[R::Shops]=0;changed.advanced["TriforcePiecesTotal"]=69;
    assert(P::Update("My setup",changed).empty());
    assert(P::List().back().config.options[R::Shops]==0);
    assert(P::List().back().config.advanced.at("TriforcePiecesTotal")==69);
    assert(!P::Update("Beginner",changed).empty() && !P::Remove("Beginner").empty());
    P::Save(settings);P::Load(settings);assert(P::List().back().config.advanced.at("TriforcePiecesTotal")==69);
    assert(P::Remove("My setup").empty());P::Save(settings);P::Load(settings);assert(P::List().size()==4);
    settings.setValue("RandomizerPresets/library",QByteArray("invalid-json"));P::Load(settings);
    assert(!P::StorageError().empty() && !P::SaveNew("new",current).empty());
    P::Save(settings);assert(settings.value("RandomizerPresets/library").toByteArray()=="invalid-json");
    auto invalid=R::ToJson(current);invalid["shopsanity"]=999;
    const QJsonObject malformed{{"format",1},{"presets",QJsonArray{QJsonObject{{"name","invalid"},{"config",invalid}}}}};
    settings.setValue("RandomizerPresets/library",QJsonDocument(malformed).toJson());P::Load(settings);
    assert(!P::StorageError().empty() && P::List().size()==4);
    std::cout<<"Presets: catalog mappings, replacement/isolation, custom CRUD, persistence and invalid-library preservation passed\n";
}
