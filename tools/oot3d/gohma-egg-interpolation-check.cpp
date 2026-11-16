// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/interpolation_pose.h"
#include "json.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace OOT3D::Interpolation;
using nlohmann::json;
Matrix ReadMatrix(const json& v) {
    Matrix m{};assert(v.size()==16);
    for(unsigned i=0;i<16;++i)m[i/4][i%4]=v.at(i).get<double>();
    return m;
}
CameraSample ReadCamera(const json& c) {
    CameraSample s;s.valid=c.at("valid");s.owner=c.at("owner");s.scene=c.at("scene");
    s.epoch=c.at("epoch");s.time=c.at("time_us");s.view=ReadMatrix(c.at("view"));return s;
}
Pose::Sample ReadPose(const json& p) {
    Pose::Sample s;s.model=p.at("model");s.frame=p.at("frame");s.time=p.at("time_us");
    s.key={p.at("renderer"),p.at("skeleton"),p.at("resource"),p.at("primitive"),unsigned(p.at("bone_ids").size())};
    s.bone_count=p.at("bone_count");s.skinning=p.at("skinning");s.camera=ReadCamera(p.at("camera"));
    s.draw_view=ReadMatrix(p.at("draw_view"));s.vertex_offset=ReadMatrix(p.at("vertex_offset"));
    for(unsigned i=0;i<s.key.count;++i) {
        s.bone_ids[i]=p.at("bone_ids").at(i);
        s.native[i]=ReadMatrix(p.at("native").at(i));s.world[i]=ReadMatrix(p.at("world").at(i));
    }
    return s;
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1]);assert(f);json capture;f>>capture;
    struct Event {std::int64_t time;bool draw;unsigned index;};std::vector<Event> events;
    for(unsigned i=0;i<capture.at("poses").size();++i)
        if(capture["poses"][i]["model"]=="childgoma")events.push_back({capture["poses"][i]["time_us"],false,i});
    for(unsigned i=0;i<capture.at("draws").size();++i)
        events.push_back({capture["draws"][i]["time_us"],true,i});
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    for(float phase:{.25f,.5f,.75f,1.f}) {
        sample_fraction=phase;Pose::History history;unsigned repaired=0,spiders=0,draws=0;
        for(const auto& e:events) {
            if(e.draw) {
                const auto& d=capture["draws"][e.index];
                if(!d.at("perspective").get<bool>())continue;
                Pose::Palette palette{};
                for(unsigned i=0;i<10;++i)palette[i]=ReadMatrix(d.at("palette").at(i));
                const auto camera=ReadCamera(d.at("camera"));
                bool egg=false;
                for(const auto& p:history.Pairs())if(Pose::GohmaEgg(p.current) && p.ready &&
                    Pose::PaletteNear(palette[0],p.current.native[0]) &&
                    e.time>=p.current.time && e.time-p.current.time<=100000 &&
                    d.at("frame").get<std::uint64_t>()>=p.current.frame &&
                    d.at("frame").get<std::uint64_t>()-p.current.frame<=1 &&
                    Math::Near(camera.view,p.current.camera.view,.002))egg=true;
                if(!egg)continue;
                const auto result=Pose::Resolve(history.Pairs(),palette,camera,d.at("frame"),e.time);
                assert(result && result->count==1);++draws;
                continue;
            }
            const auto s=ReadPose(capture["poses"][e.index]);history.Observe(s);
            const auto it=std::find_if(history.Pairs().begin(),history.Pairs().end(),[&](const auto& p){return p.current.key==s.key;});
            assert(it!=history.Pairs().end());const auto& pair=*it;
            if(!pair.ready)continue;
            for(unsigned i=0;i<s.key.count;++i) {
                const auto old=Pose::Half(pair.previous.world[i],s.world[i],phase);
                const auto blend=Pose::BlendPalette(pair.previous,s,i,phase);assert(blend);
                if(!Pose::GohmaEgg(s)){assert(old && *old==*blend);++spiders;continue;}
                assert(!old);++repaired;
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                    assert((*blend)[r][c]==std::lerp(pair.previous.world[i][r][c],s.world[i][r][c],double(phase)));
                auto changed=s;changed.bone_ids[0]=9;
                assert(!Pose::BlendPalette(pair.previous,changed,0,phase));
                changed=s;changed.model="other";assert(!Pose::BlendPalette(pair.previous,changed,0,phase));
                for(unsigned kind=0;kind<4;++kind) {
                    auto invalid=s.world[0];
                    if(kind==0)for(unsigned r=0;r<3;++r)invalid[r][0]=0;
                    if(kind==1)for(unsigned r=0;r<3;++r)invalid[r][0]*=-1;
                    if(kind==2)invalid[0][0]=NAN;
                    if(kind==3)for(unsigned r=0;r<3;++r)invalid[r][1]=invalid[r][0];
                    assert(!Pose::HalfGohmaEgg(pair.previous.world[0],invalid,phase));
                }
            }
        }
        assert(repaired==68 && spiders>0 && draws>20);
        std::cout<<"phase="<<phase<<" egg pairs repaired="<<repaired<<" spider joints unchanged="<<spiders
            <<" captured egg draws resolved="<<draws<<std::endl;
    }
    sample_fraction=.5f;
}
