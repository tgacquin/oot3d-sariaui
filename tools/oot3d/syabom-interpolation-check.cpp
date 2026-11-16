// SPDX-License-Identifier: GPL-2.0-or-later
#define main ExistingSurfaceCheck
#include "surface-effects-check.cpp"
#undef main
#include "oot3d/model_effect_interpolation.h"
namespace M=OOT3D::Interpolation::ModelEffects;
namespace Math=OOT3D::Interpolation::Math;
namespace Pose=OOT3D::Interpolation::Pose;
void Guards() {
    M::History h;M::Key key{1,2,3,0,1,2,0,M::Syabom};
    M::Sample a,b;a.model=Math::Identity();b=a;b.model[0][1]=.15;b.model[0][3]=2;
    const auto camera=Math::Identity();
    assert(!h.Apply(10,key,camera,a));const auto half=h.Apply(11,key,camera,b);assert(half);
    assert(std::abs(half->model[0][1]-.075)<1e-6 && half->model[0][3]==1);
    assert(!h.Apply(13,key,camera,a)); // Missing native frame.
    ++key[4];assert(!h.Apply(14,key,camera,b)); // Scene/reset isolation.
    h.Reset();assert(!h.Apply(10,key,camera,a));b=a;b.model[0][3]=100;
    assert(!h.Apply(11,key,camera,b)); // Spawn/teleport.
    h.Reset();h.Apply(10,key,camera,a);h.Apply(10,key,camera,a);
    assert(!h.Apply(11,key,camera,a)); // Ambiguous overlapping instances.
    h.Reset();h.Apply(10,key,camera,a);b=a;b.model[0][0]=-1;
    assert(!h.Apply(11,key,camera,b)); // Reflection is not a wobble.
    h.Reset();h.Apply(10,key,camera,a);b=a;b.model[0][0]=0;
    assert(!h.Apply(11,key,camera,b)); // Singular transform.
}
int main(int argc,char** argv) {
    assert(argc>1);OOT3D::Interpolation::sample_fraction=.5f;Guards();
    for(int arg=1;arg<argc;++arg)for(float phase:{.25f,.5f,.75f}) {
        OOT3D::Interpolation::sample_fraction=phase;
        QFile file(argv[arg]);assert(file.open(QIODevice::ReadOnly));
        const auto json=QJsonDocument::fromJson(file.readAll()).object();
        assert(!json["truncated"].toBool() && json["dropped_draws"].toInt()==0);
        M::History history;unsigned seen=0,paired=0,sheared=0;
        std::uint64_t previous_frame=0;std::vector<OOT3D::Interpolation::Matrix> previous,current;
        for(const auto value:json["draws"].toArray()) {
            const auto d=value.toObject();auto regs=Regs(d);
            if(M::Candidate(regs)!=M::Syabom)continue;
            ++seen;const auto frame=d["frame"].toInteger();
            if(frame!=previous_frame) {
                previous=frame==previous_frame+1?current:std::vector<OOT3D::Interpolation::Matrix>{};
                current.clear();previous_frame=frame;
            }
            const auto u=d["uniforms_f"].toArray();M::Sample s;
            auto camera=Math::Identity();
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){s.model[r][c]=u[20+r].toArray()[c].toDouble();camera[r][c]=u[76+r].toArray()[c].toDouble();}
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)s.uv[r][c]=u[M::Rows[r]].toArray()[c].toDouble();
            for(unsigned c=0;c<4;++c)s.color[c]=u[8].toArray()[c].toDouble();
            s.texture=regs.texturing.texture0.GetPhysicalAddress();
            const auto world=Math::Multiply(camera,s.model);current.push_back(world);
            if(const auto result=history.Apply(frame,{unsigned(d["vertex_base"].toInteger()),unsigned(d["index_offset"].toInteger()),s.texture,0,
                unsigned(d["generation"].toInt()),unsigned(d["scene"].toInt()),0,M::Syabom},camera,s)) {
                ++paired;assert(!previous.empty() && !result->previous_texture);
                const OOT3D::Interpolation::Matrix* old=nullptr;double best=INFINITY;
                for(const auto& p:previous) {
                    double distance=0;for(unsigned c=0;c<3;++c)distance+=std::pow(p[c][3]-world[c][3],2);
                    if(distance<best){best=distance;old=&p;}
                }
                assert(old);auto expected=Pose::Half(*old,world,phase);
                if(!expected) {
                    ++sheared;expected=Math::Identity();
                    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)(*expected)[r][c]=std::lerp((*old)[r][c],world[r][c],double(phase));
                }
                const auto actual=Math::Multiply(camera,result->model);
                assert(Math::Near(actual,*expected,1e-5));
                assert(!Math::Near(result->model,s.model,1e-5));
                // Exercise the captured quad's corners, including its shear,
                // rather than checking only the center's movement.
                for(const auto input:d["input_attributes"].toArray()) {
                    const auto pos=input.toArray()[0].toArray();
                    for(unsigned r=0;r<3;++r) {
                        double a=0,b=0;for(unsigned c=0;c<4;++c){a+=actual[r][c]*pos[c].toDouble();b+=(*expected)[r][c]*pos[c].toDouble();}
                        assert(std::abs(a-b)<1e-4);
                    }
                }
            }
            regs.framebuffer.output_merger.depth_write_enable.Assign(1);assert(M::Candidate(regs)==M::None);
            regs.framebuffer.output_merger.depth_write_enable.Assign(0);
            regs.texturing.tev_stage2.ops_raw=1;assert(M::Candidate(regs)==M::None);
        }
        std::cout<<argv[arg]<<" phase="<<phase<<" bubbles="<<seen<<" paired="<<paired<<" sheared="<<sheared<<'\n';
        assert(seen==180 && paired==120 && sheared>100);
    }
}
