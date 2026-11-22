// SPDX-License-Identifier: GPL-2.0-or-later
#define main GohmaCaptureCheck
#include "gohma-egg-interpolation-check.cpp"
#undef main

int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream file(argv[1]);json capture;file>>capture;
    struct Event {std::int64_t time;unsigned index;bool draw;};
    std::vector<Event> events;
    for(unsigned i=0;i<capture["poses"].size();++i) {
        const auto& p=capture["poses"][i];
        if(p["model"]=="ganondorfchild" || p["model"]=="ganonhorse")
            events.push_back({p["time_us"],i,false});
    }
    for(unsigned i=0;i<capture["draws"].size();++i) {
        const auto& d=capture["draws"][i];
        if(d["perspective"]==true)events.push_back({d["time_us"],i,true});
    }
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    for(float phase:{0.f,.25f,.5f,.75f,1.f}) {
        sample_fraction=phase;Pose::History history;
        unsigned rider=0,horse=0,recovered=0;
        for(const auto& event:events) {
            if(!event.draw){history.Observe(ReadPose(capture["poses"][event.index]));continue;}
            const auto& d=capture["draws"][event.index];const auto camera=ReadCamera(d["camera"]);
            const auto frame=d["frame"].get<std::uint64_t>();Pose::Palette palette{};
            for(unsigned i=0;i<10;++i)palette[i]=ReadMatrix(d["palette"][i]);
            const Pose::Pair* longest=nullptr;
            const Pose::Pair* rigid=nullptr;
            for(const auto& p:history.Pairs()) {
                const auto& s=p.current;
                if(!p.ready || frame<s.frame || frame-s.frame>1 || event.time<s.time ||
                   event.time-s.time>100000 || !Math::Near(camera.view,s.camera.view,.002))continue;
                bool match=true;
                for(unsigned i=0;i<s.key.count;++i)match &= Pose::PaletteNear(palette[i],s.native[i]);
                if(!match)continue;
                if(!longest || longest->current.key.count<s.key.count)longest=&p;
                if(s.key.count==1)rigid=&p;
            }
            if(!longest)continue;
            const auto result=Pose::Resolve(history.Pairs(),palette,camera,frame,event.time);
            assert(result && result->count==longest->current.key.count);
            if(longest->current.model=="ganonhorse"){++horse;continue;}
            ++rider;
            if(d["pose_midpoint"]==true)continue;
            ++recovered;assert(rigid && Pose::GanondorfIntroPrefix(*longest,*rigid));
            for(bool old:{false,true}) {
                auto wrong=*rigid;
                (old?wrong.previous:wrong.current).world[0][0][3]+=1;
                assert(!Pose::GanondorfIntroPrefix(*longest,wrong));
                wrong=*rigid;(old?wrong.previous:wrong.current).key.renderer++;
                assert(!Pose::GanondorfIntroPrefix(*longest,wrong));
            }
        }
        std::cout<<"Intro phase "<<phase<<": "<<rider<<" rider draws ("<<recovered
                 <<" previously skipped), "<<horse<<" horse draws"<<std::endl;
        assert(rider==36 && recovered==3 && horse==48);
    }
}
