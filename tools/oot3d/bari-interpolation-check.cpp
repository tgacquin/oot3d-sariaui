// SPDX-License-Identifier: GPL-2.0-or-later
#define main GohmaCaptureCheck
#include "gohma-egg-interpolation-check.cpp"
#undef main
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1]);assert(f);json capture;f>>capture;
    struct Event {std::int64_t time;bool draw;unsigned index;};std::vector<Event> events;
    for(unsigned i=0;i<capture["poses"].size();++i)
        if(capture["poses"][i]["model"]=="vali")events.push_back({capture["poses"][i]["time_us"],false,i});
    for(unsigned i=0;i<capture["draws"].size();++i)
        events.push_back({capture["draws"][i]["time_us"],true,i});
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    for(float fraction:{0.f,.25f,.5f,.75f,1.f}) {
        sample_fraction=fraction;Pose::History history;unsigned repaired=0,ready=0,adjacent=0,draws=0,expanded=0;
        for(const auto& e:events) {
            if(e.draw) {
                const auto& d=capture["draws"][e.index];
                if(!d.at("perspective").get<bool>())continue;
                Pose::Palette palette{};
                for(unsigned i=0;i<10;++i)palette[i]=ReadMatrix(d.at("palette").at(i));
                const auto camera=ReadCamera(d.at("camera"));
                bool matched=false;
                for(const auto& p:history.Pairs()) {
                    const auto& s=p.current;
                    if(!p.ready || e.time<s.time || e.time-s.time>100000 ||
                       d.at("frame").get<std::uint64_t>()<s.frame ||
                       d.at("frame").get<std::uint64_t>()-s.frame>1 ||
                       !Math::Near(camera.view,s.camera.view,.002))continue;
                    bool same=true;
                    for(unsigned i=0;i<s.key.count;++i)same &= Pose::PaletteNear(palette[i],s.native[i]);
                    matched |= same;
                }
                if(!matched)continue;
                const auto resolved=Pose::Resolve(history.Pairs(),palette,camera,d.at("frame"),e.time);
                assert(resolved);++draws;
                continue;
            }
            const auto s=ReadPose(capture["poses"][e.index]);history.Observe(s);
            const auto& pair=*std::find_if(history.Pairs().begin(),history.Pairs().end(),
                [&](const auto& p){return p.current.key==s.key;});
            if(pair.previous.frame+1!=s.frame)continue;
            ++adjacent;assert(pair.ready);++ready;
            for(unsigned i=0;i<s.key.count;++i) {
                const auto blend=Pose::BlendPalette(pair.previous,s,i,fraction);assert(blend);
                if(fraction==0)assert(Math::Near(*blend,pair.previous.world[i],1e-5));
                if(fraction==1)assert(Math::Near(*blend,s.world[i],1e-5));
                if(const auto rigid=Pose::Half(pair.previous.world[i],s.world[i],fraction)) {
                    assert(*blend==*rigid);continue;
                }
                ++repaired;
                if(!Pose::HalfGohmaEgg(pair.previous.world[i],s.world[i],fraction))++expanded;
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                    assert((*blend)[r][c]==std::lerp(pair.previous.world[i][r][c],s.world[i][r][c],double(fraction)));
                auto wrong=s;wrong.model="unrelated";assert(!Pose::BlendPalette(pair.previous,wrong,i,fraction));
                wrong=s;wrong.bone_ids[i]=99;assert(!Pose::BlendPalette(pair.previous,wrong,i,fraction));
                // Even two agreeing palettes cannot opt an unknown layout in.
                auto old_wrong=pair.previous;old_wrong.bone_ids[i]=99;
                assert(!Pose::BlendPalette(old_wrong,wrong,i,fraction));
                for(unsigned kind=0;kind<6;++kind) {
                    wrong=s;auto& m=wrong.world[i];
                    if(kind==0)m[0][0]=NAN;
                    if(kind==1)for(unsigned r=0;r<3;++r)m[r][0]=0;
                    if(kind==2)for(unsigned r=0;r<3;++r)m[r][0]*=-1;
                    if(kind==3)for(unsigned r=0;r<3;++r)m[r][1]=m[r][0];
                    if(kind==4)for(unsigned r=0;r<3;++r)m[r][0]*=3;
                    if(kind==5)for(unsigned r=0;r<3;++r){m[r][0]*=-1;m[r][1]*=-1;}
                    assert(!Pose::BlendPalette(pair.previous,wrong,i,fraction));
                }
            }
        }
        assert(adjacent>0 && ready==adjacent && repaired>0 && draws>0);
        std::cout<<"Bari phase "<<fraction<<": "<<ready<<"/"<<adjacent<<" ready palettes, "
            <<repaired<<" sheared joints ("<<expanded<<" beyond egg limit), "<<draws<<" captured draws resolved\n";
    }
    return 0;
}
