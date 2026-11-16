// SPDX-License-Identifier: GPL-2.0-or-later
#define main ExistingSurfaceCheck
#include "surface-effects-check.cpp"
#undef main
#include "video_core/pica/shader_setup.h"
#include "video_core/pica/shader_unit.h"
#include "video_core/shader/shader_interpreter.h"
#include <cstring>
namespace S=OOT3D::Interpolation::WaterSprites;
void FullHitBatch(float fraction) {
    const auto quad=[](float x,unsigned cell) {
        auto q=Star(x,0,.6f);
        for(auto& v:q){v[2][0]=(cell%4+v[2][0])/4;v[2][1]=1-(cell/4+1-v[2][1])/4;}
        return q;
    };
    Batch a,b;Add(a,quad(0,3));Add(a,quad(80,3));End(a);
    Add(b,quad(82,4));Add(b,quad(2,4));Add(b,quad(200,0));End(b);
    a=NonIndexed(a);b=NonIndexed(b);S::History history;
    const auto apply=[&](unsigned frame,const Batch& batch) {
        return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(batch.inputs,batch.order,[&](const auto& v,const auto& order) {
            return history.Apply(13,frame,123,v,order);
        });
    };
    assert(!apply(1,a));const auto result=apply(2,b);assert(result);
    assert(std::abs((*result)[0][0][0]-(79+2*fraction))<.0001f);
    assert(std::abs((*result)[6][0][0]-(-1+2*fraction))<.0001f);
    assert((*result)[3]==(*result)[4] && (*result)[5]==(*result)[6]);
    for(unsigned i=12;i<b.inputs.size();++i)assert((*result)[i]==b.inputs[i]); // Births stay native.
    const auto old=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto&,const auto&) {
        return std::optional{history.old_inputs};
    });
    assert(old && (*old)[3]==(*old)[4] && (*old)[5]==(*old)[6]);
    for(unsigned i:{0u,1u,2u,3u,6u,7u,8u,9u}) {
        assert(std::abs((*old)[i][1][3]+(*result)[i][1][3]-.6f)<.0001f);
        assert((*old)[i][0]==(*result)[i][0] && (*old)[i][2]!=(*result)[i][2]);
    }
    assert(!apply(4,b));history.Reset();assert(!apply(5,b));
    Batch duplicate;Add(duplicate,quad(0,3));Add(duplicate,quad(0,3));End(duplicate);
    history.Reset();assert(!apply(1,duplicate) && !apply(2,duplicate));
    Batch restarted;Add(restarted,quad(0,0));End(restarted);
    history.Reset();assert(!apply(1,a) && !apply(2,restarted));
    Batch invalid=b;
    for(unsigned i=0;i<5;++i)invalid.inputs[i][2][0]+=.01f;
    history.Reset();assert(!apply(1,a) && !apply(2,invalid)); // Valid rectangle, misaligned cell.
}
int main(int argc,char** argv) {
    assert(argc==3);
    QFile capture(argv[1]);assert(capture.open(QIODevice::ReadOnly));
    const auto draws=QJsonDocument::fromJson(capture.readAll()).object()["draws"].toArray();
    QFile shader_file(argv[2]);assert(shader_file.open(QIODevice::ReadOnly));const auto bytes=shader_file.readAll();
    const auto read=[&](unsigned at){assert(at+4<=bytes.size());std::uint32_t v;std::memcpy(&v,bytes.data()+at,4);return v;};
    Pica::ShaderSetup setup;Pica::Shader::InterpreterEngine engine;
    const unsigned dvlp=12,dvle=read(8);
    for(unsigned i=0;i<read(dvlp+12);++i)setup.UpdateProgramCode(i,read(dvlp+read(dvlp+8)+i*4));
    for(unsigned i=0;i<read(dvlp+20);++i)setup.UpdateSwizzleData(i,read(dvlp+read(dvlp+16)+i*8));
    assert(setup.GetProgramCodeHash()==0x6352a3be33cb99c6ULL && setup.GetSwizzleDataHash()==0xba817d0ef27276d0ULL);
    engine.SetupBatch(setup,read(dvle+8));
    for(float fraction:{.25f,.5f,.75f}) {
        OOT3D::Interpolation::sample_fraction=fraction;
        FullHitBatch(fraction);
        E::History history;unsigned hits=0,hit_pairs=0,atlas=0,bubbles=0,bubble_pairs=0,shader_checks=0;
        for(const auto value:draws) {
            const auto d=value.toObject();const auto regs=Regs(d);const unsigned kind=S::Candidate(regs);
            if(d["elapsed_ms"].toInt()<5000)continue;
            if(kind!=8 && kind!=13)continue;
            assert(E::Candidate(regs,d["shader"].toString().toULongLong(nullptr,16),d["swizzle"].toString().toULongLong(nullptr,16))==kind);
            auto bad=regs;bad.framebuffer.output_merger.depth_write_enable.Assign(1);assert(!S::Candidate(bad));
            if(kind==13) {
                ++hits;bad=regs;bad.texturing.texture0_format.Assign(Pica::TexturingRegs::TextureFormat(13));assert(!S::Candidate(bad));
            } else ++bubbles;
            // Gohma hit sheets have three captured corners (first-quad fixture).
            // The neighboring bubble batches retain their COMPLETE CPU input/order.
            const auto b=CapturedEffect(d);const auto original=b.inputs;
            history.Context(d["generation"].toInt(),d["scene"].toInt());
            const auto result=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                assert(S::Decode(v,order));
                return history.Apply(kind,d["frame"].toInteger(),regs.texturing.texture0.GetPhysicalAddress(),v,order);
            });
            assert(original==b.inputs);
            if(!result)continue;
            if(kind==8){++bubble_pairs;continue;}
            ++hit_pairs;
            const auto& prior=history.OldSpriteInputs();if(prior.empty())continue;
            ++atlas;
            const auto u=d["uniforms_f"].toArray();
            for(unsigned row=0;row<96;++row)for(unsigned c=0;c<4;++c)
                setup.uniforms.f[row][c]=Pica::f24::FromFloat32(u[row].toArray()[c].toDouble());
            for(unsigned i=0;i<16;++i)setup.uniforms.b[i]=(d["uniforms_b_mask"].toInt()>>i)&1;
            const auto integers=d["uniforms_i"].toArray();
            for(unsigned i=0;i<4;++i)for(unsigned c=0;c<4;++c)setup.uniforms.i[i][c]=integers[i].toArray()[c].toInt();
            const auto shade=[&](const E::Vertex& vertex) {
                Pica::ShaderUnit unit{};
                for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
                    unit.input[regs.vs.GetRegisterForAttribute(a)][c]=Pica::f24::FromFloat32(vertex[a][c]);
                engine.Run(setup,unit);
                return std::clamp(std::abs(unit.output[unit.output_bank][1].w.ToFloat32()),0.f,1.f);
            };
            // Exercise native shader + output clamp: complementary input weights
            // must survive to actual rasterized alpha, without saturation.
            const float old_alpha=shade(prior[0]),new_alpha=shade((*result)[0]);
            auto unweighted=(*result)[0];unweighted[1][3]/=fraction;
            const float native_alpha=shade(unweighted);
            assert(std::abs(old_alpha-native_alpha*(1-fraction))<.0001f && std::abs(new_alpha-native_alpha*fraction)<.0001f);
            assert(std::abs(old_alpha+new_alpha-native_alpha)<.0001f);++shader_checks;
        }
        std::cout<<"phase "<<fraction<<": hit draws="<<hits<<" first-quad pairs="<<hit_pairs<<" atlas fades="<<atlas
                 <<" shader checks="<<shader_checks<<" bubble batches="<<bubbles<<" paired="<<bubble_pairs<<std::endl;
        assert(hits==24 && hit_pairs>0 && atlas>0 && shader_checks==atlas && bubbles==23 && bubble_pairs>0);
    }
}
