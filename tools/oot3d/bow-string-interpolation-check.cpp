// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/interpolation_pose.h"
#include "json.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::Interpolation;
using nlohmann::json;
Matrix MatrixFrom(const json& values) {
    Matrix m{};for(unsigned i=0;i<16;++i)m[i/4][i%4]=values.at(i).get<double>();return m;
}
int main(int argc,char** argv) {
    auto a=Math::Identity(),b=a;
    a[1][1]=.002;b[1][1]=-.0002;
    assert(!Pose::Half(a,b));
    auto half=Pose::HalfBowString(a,b);assert(half && std::abs((*half)[1][1]-.0009)<1e-12);
    a[1][1]=0;b[1][1]=1;
    half=Pose::HalfBowString(a,b);assert(half && (*half)[1][1]==.5);
    a[0][0]=0;assert(!Pose::HalfBowString(a,b));
    a=Math::Identity();a[0][1]=.2;assert(!Pose::HalfBowString(a,b));
    a=Math::Identity();b[1][1]=5;assert(!Pose::HalfBowString(a,b));
    a=Math::Identity();b=a;b[0][1]=.12;b[1][1]=-.001;
    const auto elastic=Pose::HalfSlingshotString(a,b);
    assert(elastic && (*elastic)[0][1]==.06 && (*elastic)[1][1]==.4995);
    b[0][0]=0;assert(!Pose::HalfSlingshotString(a,b));
    b=Math::Identity();b[1][1]=5;assert(!Pose::HalfSlingshotString(a,b));
    b=Math::Identity();b[0][0]=-1;assert(!Pose::HalfSlingshotString(a,b));
    if(argc>=2) {
        const bool child=argc==3 && std::string(argv[2])=="child";
        const std::string model=child?"childlink_v2":"link_v2";
        std::ifstream file(argv[1]);assert(file);json capture;file>>capture;
        std::vector<Pose::Skeleton> skeletons;
        for(const auto& snapshot:capture.at("skeleton_snapshots")) {
            Pose::Skeleton s;
            for(const auto& bone:snapshot.at("bones"))
                s.bones.push_back({bone.at("parent").get<unsigned>(),MatrixFrom(bone.at("world")),MatrixFrom(bone.at("inverse_bind"))});
            skeletons.push_back(s);
        }
        std::map<unsigned,unsigned> frames;
        for(const auto& pose:capture.at("poses"))if(pose.at("model")==model) {
            const int id=pose.at("skeleton_snapshot").get<int>();
            if(id>=0)frames[pose.at("frame").get<unsigned>()]=unsigned(id);
        }
        const auto sample=[&](unsigned frame) {
            Pose::Sample s;
            for(const auto& p:capture.at("poses")) {
                if(p.at("model")!=model || p.at("frame").get<unsigned>()!=frame ||
                   p.at("bone_ids")!=json::array({23}))continue;
                s.model=model;s.bone_count=p.at("bone_count");s.skinning=p.at("skinning");
                s.key={p.at("renderer"),p.at("skeleton"),p.at("resource"),p.at("primitive"),1};
                s.bone_ids[0]=23;s.native[0]=MatrixFrom(p.at("native").at(0));
                s.world[0]=MatrixFrom(p.at("world").at(0));
                s.draw_view=MatrixFrom(p.at("draw_view"));s.vertex_offset=MatrixFrom(p.at("vertex_offset"));
                s.frame=frame;s.time=p.at("time_us");
                const auto& c=p.at("camera");
                s.camera.valid=true;s.camera.owner=c.at("owner");s.camera.scene=c.at("scene");
                s.camera.epoch=c.at("epoch");s.camera.view=MatrixFrom(c.at("view"));s.camera.time=c.at("time_us");
                s.skeleton_pose=std::make_shared<Pose::Skeleton>(skeletons.at(p.at("skeleton_snapshot").get<unsigned>()));
                assert(Pose::SkeletonMatches(s));return s;
            }
            return s;
        };
        unsigned replayed=0;
        unsigned checked=0,repaired=0;
        for(auto it=frames.begin();it!=frames.end();++it) {
            auto next=std::next(it);if(next==frames.end() || next->first!=it->first+1)continue;
            const auto& old=skeletons.at(it->second);const auto& current=skeletons.at(next->second);
            const auto prior=Pose::BlendSkeleton(old,current),fixed=Pose::BlendSkeleton(old,current,!child,child);
            assert(fixed.size()==25 && fixed[23]);
            for(unsigned i=0;i<25;++i)if(i!=23)assert(prior[i]==fixed[i]);
            repaired+=!prior[23];++checked;
            const auto previous=sample(it->first),current_sample=sample(next->first);
            if(previous.key.count && current_sample.key.count) {
                Pose::History history;history.Observe(previous);history.Observe(current_sample);
                assert(history.Pairs().size()==1 && history.Pairs()[0].ready);
                const auto draw=Pose::Resolve(history.Pairs(),current_sample.native,current_sample.camera,
                                             current_sample.frame,current_sample.time);
                assert(draw && draw->count==1);
                const auto skin=Pose::SkinBone(current_sample,0,*fixed[23]);assert(skin);
                assert(Math::Near(draw->palette[0],Math::Multiply(current_sample.draw_view,*skin),1e-8));
                ++replayed;
            }
        }
        assert(checked && repaired && replayed);
        std::cout<<"Captured weapon-string palettes resolved for midpoint rendering: "<<replayed<<'\n';
        std::cout<<"Captured consecutive pairs: "<<checked<<"; previously rejected string pairs repaired: "<<repaired<<'\n';
    }
    std::cout<<"Weapon strings: signed stretch, slingshot shear, scoped hierarchy and invalid-transform guards passed\n";
}
