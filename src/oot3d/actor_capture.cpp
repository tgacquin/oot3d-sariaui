// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/actor_capture.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <map>
namespace OOT3D::Interpolation::ActorCapture {
namespace {
template<class Rows> QJsonArray MatrixJSON(const Rows& matrix) {
    QJsonArray result;
    for(const auto& row:matrix)for(const auto value:row)
        result.append(std::isfinite(value)?QJsonValue(double(value)):QJsonValue(QJsonValue::Null));
    return result;
}
QJsonArray PaletteJSON(const Pose::Palette& palette,unsigned count) {
    QJsonArray result;for(unsigned i=0;i<std::min(count,10u);++i)result.append(MatrixJSON(palette[i]));return result;
}
QJsonObject CameraJSON(const CameraSample& camera) {
    QJsonArray lenses;for(const auto& p:camera.projections)lenses.append(MatrixJSON(p));
    return {{"owner",double(camera.owner)},{"scene",double(camera.scene)},{"epoch",int(camera.epoch)},
        {"time_us",double(camera.time)},{"valid",camera.valid},{"view",MatrixJSON(camera.view)},
        {"projections_valid",camera.projections_valid},{"projections",lenses},
        {"attached_valid",camera.attached_valid},{"attached_view",MatrixJSON(camera.attached_view)}};
}
}
bool Export(const Packet& data,const std::string& path) {
    QJsonArray poses,draws,rejections,counts,skeletons;
    std::map<const Pose::Skeleton*,int> skeleton_ids;
    for(const auto count:data.rejected)counts.append(int(count));
    for(const auto& p:data.poses) {
        QJsonArray ids;for(unsigned i=0;i<p.key.count;++i)ids.append(int(p.bone_ids[i]));
        int skeleton_id=-1;
        if(p.skeleton_pose) {
            const auto [entry,inserted]=skeleton_ids.emplace(p.skeleton_pose.get(),int(skeleton_ids.size()));
            skeleton_id=entry->second;
            if(inserted) {
                QJsonArray bones;
                for(const auto& bone:p.skeleton_pose->bones)
                    bones.append(QJsonObject{{"parent",int(bone.parent)},
                        {"world",MatrixJSON(bone.world)},{"inverse_bind",MatrixJSON(bone.bind)}});
                skeletons.append(QJsonObject{{"id",skeleton_id},{"bones",bones}});
            }
        }
        poses.append(QJsonObject{{"frame",double(p.frame)},{"time_us",double(p.time)},
            {"renderer",double(p.key.renderer)},{"skeleton",double(p.key.skeleton)},
            {"resource",double(p.key.resource)},{"primitive",double(p.key.primitive)},
            {"model",QString::fromStdString(p.model)},{"bone_count",int(p.bone_count)},
            {"skinning",int(p.skinning)},{"bone_ids",ids},{"camera",CameraJSON(p.camera)},
            {"skeleton_snapshot",skeleton_id},{"vertex_offset",MatrixJSON(p.vertex_offset)},
            {"draw_view",MatrixJSON(p.draw_view)},{"native",PaletteJSON(p.native,p.key.count)},
            {"world",PaletteJSON(p.world,p.key.count)}});
    }
    for(const auto& d:data.draws) {
        QJsonArray positions;for(unsigned i=0;i<d.position_count;++i) {
            QJsonArray point;for(float v:d.positions[i])point.append(std::isfinite(v)?QJsonValue(v):QJsonValue(QJsonValue::Null));
            positions.append(point);
        }
        draws.append(QJsonObject{{"frame",double(d.frame)},{"time_us",double(d.time)},
            {"shader",QString::number(d.shader,16)},{"swizzle",QString::number(d.swizzle,16)},
            {"vertices",double(d.vertices)},{"vertex_base",double(d.vertex_base)},
            {"index_offset",double(d.index_offset)},{"first_vertex",double(d.first_vertex)},
            {"last_vertex",double(d.last_vertex)},{"texture",double(d.texture)},
            {"topology",int(d.topology)},{"cpu",d.cpu},{"indexed",d.indexed},{"index16",d.index16},
            {"perspective",d.perspective},{"camera_midpoint",d.camera_midpoint},{"pose_midpoint",d.pose_midpoint},
            {"frame_failed",d.frame_failed},{"failure",ReplayFailureNames[unsigned(d.failure)]},
            {"camera",CameraJSON(d.camera)},{"draw_camera",MatrixJSON(d.draw_camera)},
            {"projection",MatrixJSON(d.projection)},{"model_view",MatrixJSON(d.model_view)},
            {"palette",PaletteJSON(d.palette,10)},{"position_space",d.cpu?"clip":"native input"},
            {"positions",positions}});
    }
    constexpr std::array reasons{"calls","camera","renderer","skeleton","resource","count","view","matrix"};
    for(const auto& r:data.rejections)rejections.append(QJsonObject{{"frame",double(r.frame)},
        {"renderer",double(r.renderer)},{"primitive",double(r.primitive)},{"reason",reasons[unsigned(r.reason)]}});
    const QJsonObject root{{"format","oot3d-actor-capture-v1"},{"session",int(data.session)},
        {"delay_ms",3000},{"duration_ms",10000},{"sample_frames","frame % 10 < 3"},
        {"truncated",data.truncated},{"poses",poses},{"draws",draws},{"rejections",rejections},
        {"rejection_counts",counts},{"skeleton_snapshots",skeletons}};
    const auto filename=QString::fromStdString(path);
    if(!QDir().mkpath(QFileInfo(filename).absolutePath()))return false;
    QSaveFile file(filename);if(!file.open(QIODevice::WriteOnly))return false;
    const auto json=QJsonDocument(root).toJson(QJsonDocument::Compact);
    return file.write(json)==json.size() && file.commit();
}
}
