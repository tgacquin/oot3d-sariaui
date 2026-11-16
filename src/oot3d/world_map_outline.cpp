// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/world_map_assets.h"
#include "common/file_util.h"
#include <QFile>
#include "oot3d/presentation_asset.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace OOT3D::WorldMap {
static QString Path() {
    return QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir))+
        QStringLiteral("oot3d-world-map-outline.json");
}
bool ApplyOutline(const Outline& value) {
    if(!Valid(value))return false;
    QJsonArray points;for(auto p:value.cut.hole)points.append(QJsonArray{p.x,p.y});
    const QJsonObject object{{"version",1},{"feather",value.feather},{"rounding",value.rounding},{"points",points}};
    const auto bytes=QJsonDocument(object).toJson();QSaveFile file(Path());
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())return false;
    std::atomic_store(&outline,std::shared_ptr<const Outline>(std::make_shared<Outline>(value)));
    return true;
}
void LoadOutline() {
    std::atomic_store(&outline,std::shared_ptr<const Outline>{});
    QFile file(ReleaseDefaults::PresentationAsset(Path()));if(!file.open(QIODevice::ReadOnly) || file.size()>32768)return;
    const auto o=QJsonDocument::fromJson(file.readAll()).object();
    if(o.value("version").toInt()!=1 || !o.value("feather").isDouble())return;
    Outline value;value.feather=float(o.value("feather").toDouble());
    if(o.contains("rounding")) {
        if(!o.value("rounding").isDouble())return;
        value.rounding=float(o.value("rounding").toDouble());
    }
    for(const auto v:o.value("points").toArray()) {
        const auto p=v.toArray();
        if(p.size()!=2 || !p[0].isDouble() || !p[1].isDouble())return;
        value.cut.hole.push_back({float(p[0].toDouble()),float(p[1].toDouble())});
    }
    if(Valid(value))std::atomic_store(&outline,std::shared_ptr<const Outline>(std::make_shared<Outline>(value)));
}
}
