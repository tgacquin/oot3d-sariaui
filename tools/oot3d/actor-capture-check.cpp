// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/actor_capture.h"
#include <cassert>
#include <iostream>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
using namespace OOT3D::Interpolation;
static Matrix ReadMatrix(const QJsonValue& value) {
    Matrix m{};const auto a=value.toArray();assert(a.size()==16);
    for(unsigned i=0;i<16;++i)m[i/4][i%4]=a[i].toDouble();return m;
}
static void CheckActorCapture(const char* path,const std::string& model) {
    QFile file(path);assert(file.open(QIODevice::ReadOnly));
    const auto root=QJsonDocument::fromJson(file.readAll()).object();
    std::vector<std::shared_ptr<const Pose::Skeleton>> skeletons;
    for(const auto value:root["skeleton_snapshots"].toArray()) {
        auto skeleton=std::make_shared<Pose::Skeleton>();
        for(const auto bone:value.toObject()["bones"].toArray()) {
            const auto b=bone.toObject();
            skeleton->bones.push_back({unsigned(b["parent"].toInt()),
                ReadMatrix(b["world"]),ReadMatrix(b["inverse_bind"])});
        }
        skeletons.push_back(std::move(skeleton));
    }
    Pose::History history;unsigned paired=0,resolved=0,mixed=0;
    std::uint64_t frame=0;std::int64_t time=0;
    const auto check_frame=[&] {
        const auto& entries=history.Pairs();
        for(const auto& p:entries) {
            if(!p.ready || p.current.frame!=frame)continue;
            auto native=p.current.native;bool prefix=false;
            if(p.current.key.count==1)for(const auto& other:entries)
                if(other.current.key.count==2 && other.current.frame==frame &&
                    Pose::SamePalettePrefix(p.current,other.current) &&
                    Pose::PaletteNear(p.current.native[0],other.current.native[0])) {
                    native[1]=other.current.native[1];prefix=true;
                }
            const auto result=Pose::Resolve(entries,native,p.current.camera,frame,time);
            ++paired;assert(result);++resolved;
            if(prefix){assert(result->count==2);++mixed;}
        }
    };
    for(const auto value:root["poses"].toArray()) {
        const auto d=value.toObject();if(d["model"].toString().toStdString()!=model)continue;
        Pose::Sample s;s.model=model;s.frame=d["frame"].toInteger();s.time=d["time_us"].toInteger();
        if(s.frame!=frame){check_frame();frame=s.frame;}time=s.time;
        const auto ids=d["bone_ids"].toArray();s.key={unsigned(d["renderer"].toInteger()),unsigned(d["skeleton"].toInteger()),
            unsigned(d["resource"].toInteger()),unsigned(d["primitive"].toInteger()),unsigned(ids.size())};
        s.bone_count=d["bone_count"].toInt();s.skinning=d["skinning"].toInt();
        const int snapshot=d["skeleton_snapshot"].toInt(-1);
        if(snapshot>=0) {assert(unsigned(snapshot)<skeletons.size());s.skeleton_pose=skeletons[snapshot];}
        s.draw_view=ReadMatrix(d["draw_view"]);s.vertex_offset=ReadMatrix(d["vertex_offset"]);
        const auto camera=d["camera"].toObject();s.camera.valid=true;
        s.camera.owner=camera["owner"].toInteger();s.camera.scene=camera["scene"].toInteger();
        s.camera.epoch=camera["epoch"].toInt();s.camera.view=ReadMatrix(camera["view"]);
        for(unsigned i=0;i<s.key.count;++i) {
            s.bone_ids[i]=ids[i].toInt();s.native[i]=ReadMatrix(d["native"].toArray()[i]);
            s.world[i]=ReadMatrix(d["world"].toArray()[i]);
        }
        history.Observe(s);
    }
    check_frame();
    assert(paired>60 && resolved==paired && (Pose::LinkModel(model) || mixed>20));
    std::cout<<model<<" capture: "<<resolved<<" adjacent mesh poses resolved, "<<mixed<<" mixed rigid prefixes\n";
}
int main(int argc,char** argv) {
    using namespace ActorCapture;
    Pose::Sample pose;pose.frame=10;pose.key.count=1;pose.model="slingshot\"test";
    pose.native[0]=Math::Identity();pose.world[0]=Math::Identity();
    auto skeleton=std::make_shared<Pose::Skeleton>();
    skeleton->bones.push_back({65535,Math::Identity(),Math::Identity()});
    pose.skeleton_pose=skeleton;
    Draw draw;draw.frame=10;draw.position_count=1;draw.positions[0][0]=NAN;
    draw.shader=0xfedcba9876543210ULL;draw.pose_midpoint=true;
    assert(!Recording(1000));assert(Start(1000));assert(!Start(1000));
    assert(!Recording(3999) && Recording(4000) && Recording(13999) && !Recording(14000));
    Observe(pose,3999);Observe(draw,3999);assert(pose_count==0 && draw_count==0);
    for(unsigned f=10;f<20;++f) {
        pose.frame=draw.frame=f;Observe(pose,4000);Observe(draw,4000);
    }
    assert(pose_count==3 && draw_count==3);
    Reject(Pose::CaptureStage::Count,123,456,10,4000);
    assert(!Finish(13999));auto data=Finish(14000);assert(data && !active && !Finish(14000));
    Observe(pose,14000);assert(data->poses.size()==3 && data->draws.size()==3 && data->rejections.size()==1);
    QTemporaryDir dir;assert(dir.isValid());const auto path=dir.path()+"/nested/actor.json";
    assert(Export(*data,path.toStdString()));QFile file(path);assert(file.open(QIODevice::ReadOnly));
    QJsonParseError error;const auto root=QJsonDocument::fromJson(file.readAll(),&error).object();
    assert(error.error==QJsonParseError::NoError && root["format"]=="oot3d-actor-capture-v1");
    assert(root["poses"].toArray()[0].toObject()["model"]=="slingshot\"test");
    const auto snapshots=root["skeleton_snapshots"].toArray();
    assert(snapshots.size()==1 && snapshots[0].toObject()["id"]==0);
    const auto bones=snapshots[0].toObject()["bones"].toArray();
    assert(bones.size()==1 && bones[0].toObject()["parent"]==65535);
    assert(bones[0].toObject()["world"].toArray().size()==16);
    assert(bones[0].toObject()["inverse_bind"].toArray()[15]==1);
    for(const auto& p:root["poses"].toArray()) {
        assert(p.toObject()["skeleton_snapshot"]==0);
        assert(p.toObject()["vertex_offset"].toArray()[15]==1);
    }
    const auto exported=root["draws"].toArray()[0].toObject();
    assert(exported["shader"]=="fedcba9876543210" && exported["pose_midpoint"].toBool());
    assert(exported["positions"].toArray()[0].toArray()[0].isNull());
    assert(!Export(*data,path.toStdString()+"/impossible.json"));
    assert(Start(20000));assert(pose_count==0 && draw_count==0);pose.frame=draw.frame=20;
    for(unsigned i=0;i<MaxPoses+1;++i)Observe(pose,23000);
    for(unsigned i=0;i<MaxDraws+1;++i)Observe(draw,23000);
    for(unsigned i=0;i<MaxRejections+1;++i)Reject(Pose::CaptureStage::View,1,2,20,23000);
    auto bounded=Finish(33000);assert(bounded && bounded->truncated);
    assert(bounded->poses.size()==MaxPoses && bounded->draws.size()==MaxDraws && bounded->rejections.size()==MaxRejections);
    assert(bounded->rejected[unsigned(Pose::CaptureStage::View)]==MaxRejections+1);
    if(argc>1)CheckActorCapture(argv[1],argc>2?argv[2]:"staltula");
    std::cout<<"Actor capture: delay/duration, adjacent-frame sampling, reset, caps, rejection reasons, JSON and failed-export handling passed\n";
}
