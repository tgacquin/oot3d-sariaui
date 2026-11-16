// SPDX-License-Identifier: GPL-2.0-or-later
// Timed, read-only actor diagnostics, adapted from DekUI's capture workflow.
#pragma once
#include <chrono>
#include "oot3d/interpolation_pose.h"
namespace OOT3D::Interpolation::ActorCapture {
using U32=std::uint32_t;
inline constexpr unsigned MaxPoses=4096,MaxDraws=8192,MaxRejections=256;
struct Draw {
    std::uint64_t frame{},shader{},swizzle{};
    std::int64_t time{};
    U32 vertices{},vertex_base{},index_offset{},first_vertex{},last_vertex{},texture{};
    unsigned topology{};
    bool cpu{},indexed{},index16{},camera_midpoint{},pose_midpoint{},frame_failed{},perspective{};
    ReplayFailure failure{};
    CameraSample camera;
    Matrix draw_camera{},projection{},model_view{};
    Pose::Palette palette{};
    // GPU path: native input positions; CPU path: transformed clip positions.
    std::array<std::array<float,4>,16> positions{};
    unsigned position_count{};
};
struct Rejection {std::uint64_t frame{};U32 renderer{},primitive{};Pose::CaptureStage reason{};};
struct Packet {
    unsigned session{};
    bool truncated{};
    std::vector<Pose::Sample> poses;
    std::vector<Draw> draws;
    std::vector<Rejection> rejections;
    std::array<unsigned,unsigned(Pose::CaptureStage::CountStages)> rejected{};
};
inline std::mutex mutex;
inline Packet packet;
inline std::atomic<bool> active{false};
inline std::atomic<std::int64_t> begins{},ends{};
inline std::atomic<unsigned> pose_count{},draw_count{},session{};
inline std::int64_t Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline bool Recording(std::int64_t now=-1) {
    if(!active.load())return false;
    if(now<0)now=Now();
    return now>=begins.load() && now<ends.load();
}
inline bool SampleFrame(std::uint64_t frame,std::int64_t now=-1) {
    // Keep adjacent triples throughout the recording, rather than filling the
    // bounded buffer with the first few seconds in a busy scene.
    return frame%10<3 && Recording(now);
}
inline bool Start(std::int64_t now=Now()) {
    std::scoped_lock lock{mutex};if(active)return false;
    packet={};packet.session=++session;pose_count=0;draw_count=0;
    begins=now+3000;ends=now+13000;active=true;return true;
}
inline void Observe(const Pose::Sample& pose,std::int64_t now=-1) {
    if(!SampleFrame(pose.frame,now))return;
    std::scoped_lock lock{mutex};if(!Recording(now))return;
    if(packet.poses.size()>=MaxPoses){packet.truncated=true;return;}
    packet.poses.push_back(pose);pose_count=unsigned(packet.poses.size());
}
inline void Observe(Draw draw,std::int64_t now=-1) {
    if(!SampleFrame(draw.frame,now))return;
    std::scoped_lock lock{mutex};if(!Recording(now))return;
    if(packet.draws.size()>=MaxDraws){packet.truncated=true;return;}
    packet.draws.push_back(std::move(draw));draw_count=unsigned(packet.draws.size());
}
inline void Reject(Pose::CaptureStage reason,U32 renderer,U32 primitive,
                   std::uint64_t frame,std::int64_t now=-1) {
    if(!SampleFrame(frame,now))return;
    std::scoped_lock lock{mutex};if(!Recording(now))return;
    ++packet.rejected[unsigned(reason)];
    if(packet.rejections.size()<MaxRejections)packet.rejections.push_back({frame,renderer,primitive,reason});
    else packet.truncated=true;
}
inline std::optional<Packet> Finish(std::int64_t now=Now()) {
    if(!active.load() || now<ends.load())return {};
    std::scoped_lock lock{mutex};if(!active || now<ends.load())return {};
    active=false;auto result=std::move(packet);packet={};return result;
}
bool Export(const Packet& data,const std::string& path);
}
