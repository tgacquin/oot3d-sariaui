// SPDX-License-Identifier: GPL-2.0-or-later
#define main GohmaCaptureCheck
#include "gohma-egg-interpolation-check.cpp"
#undef main

int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream file(argv[1]);
    json capture;file>>capture;
    struct Event {std::int64_t time;unsigned index;bool draw;};
    std::vector<Event> events;
    for(unsigned i=0;i<capture["poses"].size();++i) {
        const auto& p=capture["poses"][i];
        if(p["model"]=="peehat" || p["model"]=="peehat_tail")
            events.push_back({p["time_us"],i,false});
    }
    for(unsigned i=0;i<capture["draws"].size();++i) {
        const auto& d=capture["draws"][i];
        if(d["perspective"]==true)events.push_back({d["time_us"],i,true});
    }
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    for(float phase:{0.f,.25f,.5f,.75f,1.f}) {
        sample_fraction=phase;
        Pose::History history;
        unsigned adjacent=0,ready=0,matched=0,resolved=0,elastic=0;
        for(const auto& event:events) {
            if(!event.draw) {
                const auto s=ReadPose(capture["poses"][event.index]);
                history.Observe(s);
                const auto& pairs=history.Pairs();
                const auto& pair=*std::find_if(pairs.begin(),pairs.end(),[&](const auto& p){return p.current.key==s.key;});
                if(pair.previous.frame+1!=s.frame)continue;
                ++adjacent;
                if(!pair.ready)continue;
                ++ready;
                for(unsigned i=0;i<s.key.count;++i) {
                    const auto& old=pair.previous;
                    const auto mid=Pose::BlendPalette(old,s,i,phase);assert(mid);
                    if(const auto rigid=Pose::Half(old.world[i],s.world[i],phase))assert(*mid==*rigid);
                    else {
                        ++elastic;
                        if(phase==0)assert(*mid==old.world[i]);
                        if(phase==1)assert(*mid==s.world[i]);
                        auto other=s;other.model="unrelated";assert(!Pose::BlendPalette(old,other,i,phase));
                        other=s;other.world[i][0][0]=NAN;assert(!Pose::BlendPalette(old,other,i,phase));
                    }
                }
                continue;
            }
            const auto& d=capture["draws"][event.index];const auto camera=ReadCamera(d["camera"]);
            const auto frame=d["frame"].get<std::uint64_t>();Pose::Palette palette{};
            for(unsigned i=0;i<10;++i)palette[i]=ReadMatrix(d["palette"][i]);
            bool known=false;
            for(const auto& p:history.Pairs()) {
                const auto& s=p.current;
                if(!p.ready || frame<s.frame || frame-s.frame>1 || event.time<s.time ||
                   event.time-s.time>100000 || !Math::Near(camera.view,s.camera.view,.002))continue;
                bool same=true;
                for(unsigned i=0;i<s.key.count;++i)same &= Pose::PaletteNear(palette[i],s.native[i]);
                known|=same;
            }
            if(!known)continue;
            ++matched;
            if(Pose::Resolve(history.Pairs(),palette,camera,frame,event.time))++resolved;
        }
        std::cout<<"Peahat phase "<<phase<<": "<<ready<<"/"<<adjacent<<" palettes, "
                 <<resolved<<"/"<<matched<<" draws, "<<elastic<<" deformed joints"<<std::endl;
        assert(ready==adjacent && elastic>0 && matched>0 && resolved==matched);
    }
}
