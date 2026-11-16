// SPDX-License-Identifier: GPL-2.0-or-later
#define main GohmaCaptureCheck
#include "gohma-egg-interpolation-check.cpp"
#undef main

int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1]);assert(f);json capture;f>>capture;
    struct Event {std::int64_t time;bool draw;unsigned index;};std::vector<Event> events;
    for(unsigned i=0;i<capture["poses"].size();++i)
        if(capture["poses"][i]["model"]=="likelike")events.push_back({capture["poses"][i]["time_us"],false,i});
    for(unsigned i=0;i<capture["draws"].size();++i)
        events.push_back({capture["draws"][i]["time_us"],true,i});
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    for(float fraction:{0.f,.25f,.5f,.75f,1.f}) {
        sample_fraction=fraction;Pose::History history;
        unsigned adjacent=0,repaired=0,rigid=0,draws=0;
        for(const auto& e:events) {
            if(e.draw) {
                const auto& d=capture["draws"][e.index];
                if(!d.at("perspective").get<bool>())continue;
                Pose::Palette palette{};
                for(unsigned i=0;i<10;++i)palette[i]=ReadMatrix(d.at("palette").at(i));
                const auto camera=ReadCamera(d.at("camera"));
                const Pose::Pair* matched=nullptr;
                for(const auto& p:history.Pairs()) {
                    const auto& s=p.current;
                    if(!p.ready || e.time<s.time || e.time-s.time>100000 ||
                       d.at("frame").get<std::uint64_t>()<s.frame ||
                       d.at("frame").get<std::uint64_t>()-s.frame>1 ||
                       !Math::Near(camera.view,s.camera.view,.002))continue;
                    bool same=true;
                    for(unsigned i=0;i<s.key.count;++i)same &= Pose::PaletteNear(palette[i],s.native[i]);
                    if(same)matched=&p;
                }
                if(!matched)continue;
                const auto result=Pose::Resolve(history.Pairs(),palette,camera,d.at("frame"),e.time);
                assert(result && result->count==5);++draws;
                for(unsigned i=0;i<result->count;++i) {
                    const auto blend=Pose::BlendPalette(matched->previous,matched->current,i,fraction);
                    assert(blend && Pose::PaletteNear(result->palette[i],
                        Math::Multiply(matched->current.draw_view,*blend)));
                }
                continue;
            }
            const auto s=ReadPose(capture["poses"][e.index]);history.Observe(s);
            const auto& pair=*std::find_if(history.Pairs().begin(),history.Pairs().end(),
                [&](const auto& p){return p.current.key==s.key;});
            if(pair.previous.frame+1!=s.frame)continue;
            ++adjacent;assert(pair.ready);
            for(unsigned i=0;i<s.key.count;++i) {
                const auto blend=Pose::BlendPalette(pair.previous,s,i,fraction);assert(blend);
                if(const auto old=Pose::Half(pair.previous.world[i],s.world[i],fraction)) {
                    // Preserve the existing rigid path, which tolerates tiny
                    // basis errors when reconstructing its native endpoints.
                    assert(*blend==*old);++rigid;continue;
                }
                if(fraction==0)assert(*blend==pair.previous.world[i]);
                if(fraction==1)assert(*blend==s.world[i]);
                ++repaired;
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                    assert((*blend)[r][c]==std::lerp(pair.previous.world[i][r][c],s.world[i][r][c],double(fraction)));
                // Scope the deformation exception to the verified model/layout,
                // including when both samples have the same incorrect metadata.
                for(unsigned kind=0;kind<4;++kind) {
                    auto before=pair.previous,after=s;
                    const auto alter=[&](auto& sample) {
                        if(kind==0)sample.model="unrelated";
                        if(kind==1)sample.bone_ids[i]=99;
                        if(kind==2)sample.skinning=2;
                        if(kind==3)sample.bone_count=6;
                    };
                    alter(after);assert(!Pose::BlendPalette(before,after,i,fraction));
                    alter(before);assert(!Pose::BlendPalette(before,after,i,fraction));
                }
                for(unsigned kind=0;kind<6;++kind) {
                    auto wrong=s;auto& m=wrong.world[i];
                    if(kind==0)m[0][0]=NAN;
                    if(kind==1)for(unsigned r=0;r<3;++r)m[r][0]=0;
                    if(kind==2)for(unsigned r=0;r<3;++r)m[r][0]*=-1;
                    if(kind==3)for(unsigned r=0;r<3;++r)m[r][1]=m[r][0];
                    if(kind==4)for(unsigned r=0;r<3;++r)m[r][0]*=3;
                    if(kind==5)for(unsigned r=0;r<3;++r){m[r][0]*=-1;m[r][1]*=-1;}
                    assert(!Pose::BlendPalette(pair.previous,wrong,i,fraction));
                }
            }
            // Stale frames and teleports must still break history pairing.
            Pose::History gap;gap.Observe(pair.previous);
            auto wrong=s;++wrong.frame;gap.Observe(wrong);assert(!gap.Pairs().back().ready);
            Pose::History teleport;teleport.Observe(pair.previous);wrong=s;
            for(unsigned i=0;i<wrong.key.count;++i)wrong.world[i][0][3]+=10000;
            teleport.Observe(wrong);assert(!teleport.Pairs().back().ready);
        }
        assert(adjacent==56 && repaired>0 && rigid>0 && draws>0);
        std::cout<<"Like Like phase "<<fraction<<": "<<adjacent<<" adjacent palettes, "
            <<repaired<<" sheared joints, "<<rigid<<" unchanged rigid joints, "
            <<draws<<" captured draws resolved\n";
    }
    return 0;
}
