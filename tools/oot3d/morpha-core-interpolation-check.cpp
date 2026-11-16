// SPDX-License-Identifier: GPL-2.0-or-later
#define main GohmaCaptureCheck
#include "gohma-egg-interpolation-check.cpp"
#undef main

static void CheckRotatingStretch() {
    auto shape=Math::Identity();
    shape[0][0]=shape[1][1]=shape[2][2]=.01;
    shape[0][1]=shape[1][0]=.002;
    auto turn=Math::Identity();const double angle=3.141592653589793/3;
    turn[0][0]=turn[2][2]=std::cos(angle);
    turn[0][2]=std::sin(angle);turn[2][0]=-std::sin(angle);turn[0][3]=20;
    const auto rotated=Math::Multiply(turn,shape);
    assert(!Pose::Half(shape,rotated));
    for(double fraction:{0.,.25,.5,.75,1.}) {
        const auto result=Pose::HalfMorphaCore(shape,rotated,fraction);
        assert(result && Math::Near(*result,
            Math::Multiply(Math::Blend(Math::Identity(),turn,fraction),shape),1e-10));
    }
    // Large jumps, reflections, excessive shear/scale and non-affine inputs
    // must never enter the rotating-deformation path.
    for(unsigned kind=0;kind<7;++kind) {
        auto wrong=rotated;
        if(kind==0)wrong[0][0]=NAN;
        if(kind==1)for(unsigned r=0;r<3;++r)wrong[r][0]=0;
        if(kind==2)for(unsigned r=0;r<3;++r)wrong[r][0]*=-1;
        if(kind==3)for(unsigned r=0;r<3;++r)wrong[r][1]=wrong[r][0];
        if(kind==4)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)wrong[r][c]*=3;
        if(kind==5) {
            auto flip=Math::Identity();flip[0][0]=flip[2][2]=-1;
            wrong=Math::Multiply(flip,shape);
        }
        if(kind==6)wrong[3][0]=.1;
        assert(!Pose::HalfMorphaCore(shape,wrong));
    }
}

int main(int argc,char** argv) {
    CheckRotatingStretch();
    assert(argc==2);std::ifstream f(argv[1]);assert(f);json capture;f>>capture;
    struct Event {std::int64_t time;bool draw;unsigned index;};std::vector<Event> events;
    for(unsigned i=0;i<capture["poses"].size();++i) {
        const auto model=capture["poses"][i]["model"];
        if(model=="co_in_modelT" || model=="co_out_modelT")
            events.push_back({capture["poses"][i]["time_us"],false,i});
    }
    for(unsigned i=0;i<capture["draws"].size();++i)
        events.push_back({capture["draws"][i]["time_us"],true,i});
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    for(float fraction:{0.f,.25f,.5f,.75f,1.f}) {
        sample_fraction=fraction;Pose::History history;
        unsigned adjacent=0,repaired=0,rigid=0,draws=0,overlaps=0;
        for(const auto& e:events) {
            if(e.draw) {
                const auto& d=capture["draws"][e.index];
                if(!d.at("perspective").get<bool>())continue;
                Pose::Palette palette{};
                for(unsigned i=0;i<10;++i)palette[i]=ReadMatrix(d.at("palette").at(i));
                const auto camera=ReadCamera(d.at("camera"));
                const Pose::Pair* matched=nullptr;unsigned count=0;
                for(const auto& p:history.Pairs()) {
                    const auto& s=p.current;
                    if(!p.ready || e.time<s.time || e.time-s.time>100000 ||
                       d.at("frame").get<std::uint64_t>()<s.frame ||
                       d.at("frame").get<std::uint64_t>()-s.frame>1 ||
                       !Math::Near(camera.view,s.camera.view,.002))continue;
                    if(Pose::PaletteNear(palette[0],s.native[0])) {matched=&p;++count;}
                }
                if(!matched)continue;
                const auto result=Pose::Resolve(history.Pairs(),palette,camera,d.at("frame"),e.time);
                assert(result && result->count==1);++draws;overlaps+=count>1;
                const auto blend=Pose::BlendPalette(matched->previous,matched->current,0,fraction);
                assert(blend && Pose::PaletteNear(result->palette[0],
                    Math::Multiply(matched->current.draw_view,*blend)));
                // Inner/outer draws must agree regardless of capture order.
                auto reversed=history.Pairs();std::reverse(reversed.begin(),reversed.end());
                const auto reverse=Pose::Resolve(reversed,palette,camera,d.at("frame"),e.time);
                assert(reverse && Pose::PaletteNear(result->palette[0],reverse->palette[0]));
                continue;
            }
            const auto s=ReadPose(capture["poses"][e.index]);history.Observe(s);
            const auto& pair=*std::find_if(history.Pairs().begin(),history.Pairs().end(),
                [&](const auto& p){return p.current.key==s.key;});
            if(pair.previous.frame+1!=s.frame)continue;
            ++adjacent;assert(pair.ready);
            const auto blend=Pose::BlendPalette(pair.previous,s,0,fraction);assert(blend);
            if(const auto old=Pose::Half(pair.previous.world[0],s.world[0],fraction)) {
                assert(*blend==*old);++rigid;continue;
            }
            ++repaired;
            if(fraction==0)assert(Math::Near(*blend,pair.previous.world[0],1e-10));
            if(fraction==1)assert(Math::Near(*blend,s.world[0],1e-10));
            for(unsigned kind=0;kind<5;++kind) {
                auto before=pair.previous,after=s;
                const auto alter=[&](auto& sample) {
                    if(kind==0)sample.model="morpha"; // Not the tentacle.
                    if(kind==1)sample.bone_ids[0]=1;
                    if(kind==2)sample.skinning=2;
                    if(kind==3)sample.bone_count=2;
                    if(kind==4)sample.key.count=2;
                };
                alter(after);assert(!Pose::BlendPalette(before,after,0,fraction));
                alter(before);assert(!Pose::BlendPalette(before,after,0,fraction));
            }
            auto other=s;other.model=s.model=="co_in_modelT"?"co_out_modelT":"co_in_modelT";
            assert(!Pose::BlendPalette(pair.previous,other,0,fraction));
        }
        assert(adjacent==58 && repaired==24 && rigid==34 && draws>0 && overlaps>0);
        std::cout<<"Morpha core phase "<<fraction<<": "<<adjacent<<" adjacent palettes, "
            <<repaired<<" rotating/sheared pairs, "<<rigid<<" unchanged rigid pairs, "
            <<draws<<" captured draws ("<<overlaps<<" matching both layers) resolved\n";
    }
    return 0;
}
