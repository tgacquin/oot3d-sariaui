// SPDX-License-Identifier: GPL-2.0-or-later
// Reuse the capture decoder/geometry fixtures; this entry point audits the
// November 11 families at all three intermediate phases of a 120 Hz frame.
#define main ExistingSurfaceCheck
#include "surface-effects-check.cpp"
#undef main
#include "oot3d/model_effect_interpolation.h"
#include <map>
namespace M=OOT3D::Interpolation::ModelEffects;
namespace S=OOT3D::Interpolation::WaterSprites;
void SpriteRegression() {
    using OOT3D::Interpolation::ReplayEffectStrip;
    const auto quad=[](float x,unsigned cell) {
        auto q=Star(x,0,.8f);
        for(auto& v:q){v[2][0]=(cell%4+v[2][0])/4;v[2][1]=1-(cell/4+1-v[2][1])/4;}
        return q;
    };
    Batch a,b;Add(a,quad(0,3));Add(a,quad(80,3));End(a);
    Add(b,quad(82,4));Add(b,quad(2,4));End(b);a=NonIndexed(a);b=NonIndexed(b);
    S::History history;
    const auto apply=[&](const Batch& batch,unsigned frame) {
        return ReplayEffectStrip<E::Vertex>(batch.inputs,batch.order,[&](const auto& v,const auto& order) {
            return history.Apply(12,frame,123,v,order);
        });
    };
    assert(!apply(a,10));const auto result=apply(b,11);assert(result);
    assert(std::abs((*result)[0][0][0]-80)<1e-6); // Reordered particle halfway from80 to82, left corner.
    assert((*result)[3]==(*result)[4] && (*result)[5]==(*result)[6]);
    const auto old=ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto&,const auto&) {
        return std::optional{history.old_inputs};
    });
    assert(old && (*old)[3]==(*old)[4] && (*old)[5]==(*old)[6]);
    for(unsigned i:{0u,1u,2u,3u,6u,7u,8u,9u}) {
        assert(std::abs((*old)[i][1][3]+(*result)[i][1][3]-.8f)<1e-6);
        assert((*old)[i][2]!=(*result)[i][2]);
    }
    assert(!apply(b,13)); // Capture gaps never invent motion.
    history.Reset();assert(!apply(b,14));
    Batch duplicate;Add(duplicate,quad(0,3));Add(duplicate,quad(0,3));End(duplicate);
    history.Reset();assert(!apply(duplicate,1));assert(!apply(duplicate,2)); // Ambiguous identity.
    Batch invalid=b;invalid.inputs[0][0][0]=NAN;assert(!apply(invalid,3));
    // Birth and atlas restart must remain native.
    Batch born;Add(born,quad(400,0));End(born);history.Reset();assert(!apply(a,1));assert(!apply(born,2));
    M::History models;M::Key key{1,2,0,0,0,0,0,M::Biri};M::Sample model;
    model.model=OOT3D::Interpolation::Math::Identity();model.texture=10;
    const auto camera=OOT3D::Interpolation::Math::Identity();
    assert(!models.Apply(1,key,camera,model));model.texture=20;model.model[0][3]=2;
    const auto half=models.Apply(2,key,camera,model);assert(half && half->previous_texture==10 && half->model[0][3]==1);
    ++key[4];assert(!models.Apply(3,key,camera,model));
}
int main(int argc,char** argv) {
    assert(argc>1);
    OOT3D::Interpolation::sample_fraction=.5f;SpriteRegression();
    for(float fraction:{.25f,.5f,.75f}) {
        OOT3D::Interpolation::sample_fraction=fraction;
        std::map<unsigned,unsigned> seen,paired,changed;
        unsigned uv_pairs=0,texture_pairs=0,atlas_pairs=0;
        for(int file=1;file<argc;++file) {
            QFile input(argv[file]);assert(input.open(QIODevice::ReadOnly));
            const auto json=QJsonDocument::fromJson(input.readAll());assert(json.isObject());
            M::History models;W::History water;E::History sprites;
            for(const auto& value:json.object()["draws"].toArray()) {
                const auto d=value.toObject();const auto regs=Regs(d);
                const auto u=d["uniforms_f"].toArray();const auto frame=d["frame"].toInteger();
                const auto tex=regs.texturing.texture0.GetPhysicalAddress();
                if(W::Candidate(regs)) {
                    W::UV uv;for(unsigned r=0;r<W::Rows.size();++r)for(unsigned c=0;c<4;++c)uv[r][c]=u[W::Rows[r]].toArray()[c].toDouble();
                    if(const auto half=water.Apply(frame,{unsigned(d["vertex_base"].toInteger()),unsigned(d["index_offset"].toInteger()),tex,
                        regs.texturing.texture1.GetPhysicalAddress(),0,unsigned(d["generation"].toInt()),unsigned(d["scene"].toInt())},uv)) {
                        assert(*half!=uv);++uv_pairs;
                    }
                }
                if(const auto kind=M::Candidate(regs);kind!=M::None) {
                    ++seen[100+kind];M::Sample s;s.texture=tex;
                    auto camera=OOT3D::Interpolation::Math::Identity();
                    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){s.model[r][c]=u[20+r].toArray()[c].toDouble();camera[r][c]=u[76+r].toArray()[c].toDouble();}
                    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)s.uv[r][c]=u[M::Rows[r]].toArray()[c].toDouble();
                    for(unsigned c=0;c<4;++c)s.color[c]=u[8].toArray()[c].toDouble();
                    const auto tev=d["tev"].toArray();for(unsigned r=0;r<6;++r)for(unsigned c=0;c<4;++c)s.colors[r][c]=float((tev[r].toArray()[3].toInteger()>>(8*c))&255)/255.f;
                    if(const auto half=models.Apply(frame,{unsigned(d["vertex_base"].toInteger()),unsigned(d["index_offset"].toInteger()),kind==M::Biri?0u:tex,
                        regs.texturing.main_config.texture1_enable?regs.texturing.texture1.GetPhysicalAddress():0u,
                        unsigned(d["generation"].toInt()),unsigned(d["scene"].toInt()),0,unsigned(kind)},camera,s)) {
                        ++paired[100+kind];changed[100+kind]+=half->model!=s.model || half->uv!=s.uv || half->colors!=s.colors;
                        texture_pairs+=half->previous_texture!=0;
                    }
                    auto bad=regs;bad.framebuffer.output_merger.depth_write_enable.Assign(1);assert(M::Candidate(bad)==M::None);
                }
                const auto kind=S::Candidate(regs);if(!kind)continue;
                ++seen[kind];
                // Old captures keep the first three input corners. Reconstruct
                // the first quad only; full-batch identity is separately tested.
                if(d["input_attributes"].toArray().size()!=3)continue;
                auto b=CapturedEffect(d);const auto original=b.inputs;
                sprites.Context(d["generation"].toInt(),d["scene"].toInt());
                const auto half=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                    return sprites.Apply(kind,frame,tex,v,order);
                });
                assert(b.inputs==original);
                if(half){++paired[kind];changed[kind]+=*half!=original;atlas_pairs+=!sprites.OldSpriteInputs().empty();}
            }
        }
        std::cout<<"phase "<<fraction<<" water UV pairs "<<uv_pairs<<" Biri texture blends "<<texture_pairs<<" atlas blends "<<atlas_pairs<<'\n';
        for(const auto& [kind,n]:seen)std::cout<<kind<<": draws="<<n<<" pairs="<<paired[kind]<<" changed="<<changed[kind]<<'\n';
        assert(uv_pairs>500 && texture_pairs>10 && atlas_pairs>30);
        assert(paired[103]>200 && paired[104]>1500);
        for(unsigned k:{8u,9u,10u,11u,12u,101u,102u,103u,104u,105u})assert(seen[k] && paired[k] && changed[k]);
    }
}
