// SPDX-License-Identifier: GPL-2.0-or-later
#define main ExistingGohmaCheck
#include "gohma-egg-interpolation-check.cpp"
#undef main
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1]);assert(f);json capture;f>>capture;
    struct Event {std::int64_t time;bool draw;unsigned index;};std::vector<Event> events;
    for(unsigned i=0;i<capture["poses"].size();++i) {
        const auto model=capture["poses"][i]["model"].get<std::string>();
        if(model=="childstalker" || model=="redead__mask")
            events.push_back({capture["poses"][i]["time_us"],false,i});
    }
    for(unsigned i=0;i<capture["draws"].size();++i)
        events.push_back({capture["draws"][i]["time_us"],true,i});
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    for(float fraction:{0.f,.25f,.5f,.75f,1.f}) {
        sample_fraction=fraction;Pose::History history;unsigned resolved=0,attachments=0;
        for(const auto& e:events) {
            if(!e.draw){history.Observe(ReadPose(capture["poses"][e.index]));continue;}
            const auto& d=capture["draws"][e.index];if(!d["perspective"].get<bool>())continue;
            Pose::Palette palette{};for(unsigned i=0;i<10;++i)palette[i]=ReadMatrix(d["palette"][i]);
            const auto camera=ReadCamera(d["camera"]);const auto frame=d["frame"].get<std::uint64_t>();
            std::vector<Pose::Pair> matches;
            for(const auto& p:history.Pairs()) {
                const auto& s=p.current;
                if(!p.ready || e.time<s.time || e.time-s.time>100000 || frame<s.frame || frame-s.frame>1 ||
                   !Math::Near(camera.view,s.camera.view,.002))continue;
                bool same=true;for(unsigned i=0;i<s.key.count;++i)same &= Pose::PaletteNear(palette[i],s.native[i]);
                if(same)matches.push_back(p);
            }
            if(matches.empty())continue;
            const auto result=Pose::Resolve(history.Pairs(),palette,camera,frame,e.time);assert(result);++resolved;
            const auto head=std::find_if(matches.begin(),matches.end(),[](const auto& p){
                return p.current.model=="childstalker" && p.current.key.count==2;});
            const auto mask=std::find_if(matches.begin(),matches.end(),[](const auto& p){return p.current.model=="redead__mask";});
            if(head==matches.end() || mask==matches.end())continue;
            ++attachments;assert(result->count==2 && Pose::GraveyardMaskPrefix(*head,*mask));
            for(unsigned i=0;i<2;++i) {
                const auto blended=Pose::BlendPalette(head->previous,head->current,i,fraction);assert(blended);
                assert(Math::Near(result->palette[i],Math::Multiply(head->current.draw_view,*blended),1e-5));
            }
            // Preserve the longer native upload regardless of submission order.
            const auto reverse=Pose::Resolve({*mask,*head},palette,camera,frame,e.time);
            assert(reverse && reverse->count==2);
            // Only count matrices are initialized and uploaded; unused
            // palette slots are not part of the resolved override.
            for(unsigned i=0;i<result->count;++i)assert(reverse->palette[i]==result->palette[i]);
            for(unsigned kind=0;kind<5;++kind) {
                auto invalid=*mask;
                if(kind==0)invalid.previous.world[0][0][3]+=1; // Same current endpoint, different motion.
                if(kind==1)invalid.previous.model="other";
                if(kind==2)invalid.current.bone_ids[0]=1;
                if(kind==3)invalid.current.model="other";
                if(kind==4)--invalid.previous.frame;
                assert(!Pose::GraveyardMaskPrefix(*head,invalid));
                assert(!Pose::Resolve({*head,invalid},palette,camera,frame,e.time));
            }
        }
        assert(resolved>0 && attachments>0);
        std::cout<<"Graveyard boy phase "<<fraction<<": "<<resolved<<" captured draws resolved, "
            <<attachments<<" head/mask conflicts repaired\n";
    }
    return 0;
}
