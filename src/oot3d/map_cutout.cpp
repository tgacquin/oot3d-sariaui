// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_cutout.h"
#include "common/file_util.h"
#include <QDir>
#include <QFile>
#include "oot3d/presentation_asset.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace OOT3D::MapFrame {
static QString Path() {
    return QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir))+
        QStringLiteral("oot3d-map-cutout.json");
}
bool ApplyCutout(const Cutout& c) {
    const auto source=GetSource();
    if(!source)return false;
    const auto result=Cut(*source,c);if(!result)return false;
    QJsonArray points;
    for(auto p:c.hole)points.append(QJsonArray{p.x,p.y});
    const QJsonObject object{{"version",1},{"left",c.left},{"top",c.top},
        {"right",c.right},{"bottom",c.bottom},{"points",points}};
    QSaveFile file(Path());
    const auto bytes=QJsonDocument(object).toJson();
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())return false;
    {
        std::scoped_lock lock{cutout_mutex};saved_cutout=c;has_saved_cutout=true;
    }
    cut_image.store(std::shared_ptr<const CutImage>(result));
    return true;
}
void LoadCutout() {
    cut_image.store(std::shared_ptr<const CutImage>{});
    {std::scoped_lock lock{cutout_mutex};saved_cutout={};has_saved_cutout=false;}
    const auto source=GetSource();if(!source)return;
    QFile file(ReleaseDefaults::PresentationAsset(Path()));if(!file.open(QIODevice::ReadOnly) || file.size()>32768)return;
    const auto o=QJsonDocument::fromJson(file.readAll()).object();
    if(o.value("version").toInt()!=1)return;
    Cutout c;
    c.left=o.value("left").toInt(-1);c.top=o.value("top").toInt(-1);
    c.right=o.value("right").toInt(-1);c.bottom=o.value("bottom").toInt(-1);
    for(auto v:o.value("points").toArray()) {
        const auto p=v.toArray();
        if(p.size()!=2 || !p[0].isDouble() || !p[1].isDouble())return;
        c.hole.push_back({float(p[0].toDouble()),float(p[1].toDouble())});
    }
    const auto result=Cut(*source,c);if(!result)return;
    {std::scoped_lock lock{cutout_mutex};saved_cutout=c;has_saved_cutout=true;}
    cut_image.store(std::shared_ptr<const CutImage>(result));
}
}
