// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <map>
#include <string>
#include <vector>
#include "oot3d/interpolation_replay.h"
#include "oot3d/multiplayer_startup.h"
#include "oot3d/remote_names_layout.h"
namespace OOT3D::RemoteNames {
using namespace Interpolation;
struct Anchor {std::string name;std::array<double,3> position;};
struct Snapshot {std::uint64_t frame{};CameraSample camera;std::map<unsigned,Anchor> anchors;};
inline std::mutex mutex;
inline Snapshot previous,current;
inline void Reset() {std::scoped_lock lock(mutex);previous={};current={};}
inline void Capture(unsigned actor,Anchor anchor,const CameraSample& camera) {
    if(!camera.valid || !camera.projections_valid || anchor.name.empty() || anchor.name.size()>64)return;
    for(double x:anchor.position)if(!std::isfinite(x) || std::abs(x)>100000)return;
    std::scoped_lock lock(mutex);
    if(current.camera.time!=camera.time || current.camera.epoch!=camera.epoch) {
        previous=std::move(current);current={};current.camera=camera;current.frame=guest_frames.load();
    }
    if(current.anchors.size()<3 || current.anchors.contains(actor))current.anchors[actor]=std::move(anchor);
}
// Anchors stay in world space. Rendering supplies the actual scene projection
// and interpolated camera; no second screen-space camera reconstruction.
struct WorldLabels {std::vector<Anchor> anchors;Matrix billboard;};
inline WorldLabels World(const CameraSample& camera,float fraction,const Matrix& inverse_view) {
    WorldLabels out{{},inverse_view};
    const auto session=MultiplayerStartup::GetSession();
    if(!session || !session->role || !camera.valid || !Math::Rigid(inverse_view))return out;
    std::scoped_lock lock(mutex);
    if(current.camera.time!=camera.time || current.camera.epoch!=camera.epoch ||
       guest_frames.load()>current.frame+1 || guest_frames.load()<current.frame)return out;
    const auto before=Math::InverseCamera(previous.camera.view);
    bool continuous=previous.camera.valid && before &&
        previous.camera.scene==camera.scene && previous.camera.epoch==camera.epoch &&
        previous.camera.owner==camera.owner && previous.camera.cut_group==camera.cut_group &&
        camera.time>previous.camera.time && camera.time-previous.camera.time<=50000 &&
        ContinuousLens(previous.camera.projections[0],camera.projections[0]);
    if(continuous) {
        double distance=0,trace=0;
        for(unsigned r=0;r<3;++r){distance+=std::pow((*before)[r][3]-inverse_view[r][3],2);
            for(unsigned c=0;c<3;++c)trace+=(*before)[r][c]*inverse_view[r][c];}
        continuous=distance<=std::pow(distance_limit.load(),2) &&
            (trace-1)/2>=std::cos(angle_limit.load()*3.141592653589793/180.);
    }
    if(continuous && fraction<1)out.billboard=Math::Blend(*before,inverse_view,fraction);
    for(const auto& [actor,anchor]:current.anchors) {
        auto value=anchor;
        if(continuous && fraction<1)if(const auto old=previous.anchors.find(actor);
            old!=previous.anchors.end() && old->second.name==anchor.name) {
            double distance=0;for(unsigned c=0;c<3;++c)distance+=std::pow(value.position[c]-old->second.position[c],2);
            if(distance<100*100)for(unsigned c=0;c<3;++c)value.position[c]=std::lerp(old->second.position[c],value.position[c],double(fraction));
        }
        out.anchors.push_back(std::move(value));
    }
    return out;
}
}
