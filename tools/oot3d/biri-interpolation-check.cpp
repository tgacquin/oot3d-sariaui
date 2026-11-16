// SPDX-License-Identifier: GPL-2.0-or-later
#define main ExistingSurfaceCheck
#include "surface-effects-check.cpp"
#undef main
#include "oot3d/model_effect_interpolation.h"
#include "video_core/pica/shader_setup.h"
#include "video_core/pica/shader_unit.h"
#include "video_core/shader/shader_interpreter.h"
#include <cstring>
namespace M=OOT3D::Interpolation::ModelEffects;
int main(int argc,char** argv) {
    assert(argc==4);
    QFile shader_file(argv[3]);assert(shader_file.open(QIODevice::ReadOnly));const auto bytes=shader_file.readAll();
    const auto read=[&](unsigned at){assert(at+4<=bytes.size());std::uint32_t v;std::memcpy(&v,bytes.data()+at,4);return v;};
    Pica::ShaderSetup setup;Pica::Shader::InterpreterEngine engine;
    const unsigned dvlp=12,dvle=read(8);
    for(unsigned i=0;i<read(dvlp+12);++i)setup.UpdateProgramCode(i,read(dvlp+read(dvlp+8)+i*4));
    for(unsigned i=0;i<read(dvlp+20);++i)setup.UpdateSwizzleData(i,read(dvlp+read(dvlp+16)+i*8));
    assert(setup.GetProgramCodeHash()==0x6352a3be33cb99c6ULL);
    assert(setup.GetSwizzleDataHash()==0xba817d0ef27276d0ULL);
    engine.SetupBatch(setup,read(dvle+8));
    QJsonArray model_inputs;unsigned shader_checked=0;
    for(int file=1;file<=2;++file) {
        QFile input(argv[file]);assert(input.open(QIODevice::ReadOnly));
        const auto draws=QJsonDocument::fromJson(input.readAll()).object()["draws"].toArray();
        M::History history;unsigned seen=0,paired=0,blends=0;bool checked=false;
        for(const auto value:draws) {
            const auto d=value.toObject();const auto regs=Regs(d);
            if(M::Candidate(regs)!=M::Biri)continue;
            ++seen;const auto u=d["uniforms_f"].toArray();
            M::Sample s;s.texture=regs.texturing.texture0.GetPhysicalAddress();
            auto camera=OOT3D::Interpolation::Math::Identity();
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){s.model[r][c]=u[20+r].toArray()[c].toDouble();camera[r][c]=u[76+r].toArray()[c].toDouble();}
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)s.uv[r][c]=u[M::Rows[r]].toArray()[c].toDouble();
            for(unsigned c=0;c<4;++c)s.color[c]=u[8].toArray()[c].toDouble();
            if(const auto half=history.Apply(d["frame"].toInteger(),{unsigned(d["vertex_base"].toInteger()),unsigned(d["index_offset"].toInteger()),0,0,
                unsigned(d["generation"].toInt()),unsigned(d["scene"].toInt()),0,M::Biri},camera,s)) {
                ++paired;blends+=half->previous_texture!=0;
            }
            const auto inputs=d["input_attributes"].toArray();
            if(!inputs.empty())model_inputs=inputs;
            // The newer CPU capture lacks inputs. Reuse the earlier capture's
            // same bili.cmb vertex, with the new capture's actual uniforms.
            if(!checked && !model_inputs.empty()) {
                for(unsigned row=0;row<96;++row)for(unsigned c=0;c<4;++c)
                    setup.uniforms.f[row][c]=Pica::f24::FromFloat32(u[row].toArray()[c].toDouble());
                for(unsigned i=0;i<16;++i)setup.uniforms.b[i]=(d["uniforms_b_mask"].toInt()>>i)&1;
                const auto integers=d["uniforms_i"].toArray();
                for(unsigned i=0;i<4;++i)for(unsigned c=0;c<4;++c)setup.uniforms.i[i][c]=integers[i].toArray()[c].toInt();
                const auto shade=[&](float alpha) {
                    setup.uniforms.f[8].w=Pica::f24::FromFloat32(alpha);
                    Pica::ShaderUnit unit{};const auto in=model_inputs[0].toArray();
                    for(unsigned a=0;a<8;++a)for(unsigned c=0;c<4;++c)
                        unit.input[regs.vs.GetRegisterForAttribute(a)][c]=Pica::f24::FromFloat32(in[a].toArray()[c].toDouble());
                    engine.Run(setup,unit);
                    std::array<float,4> color;
                    for(unsigned c=0;c<4;++c)color[c]=unit.output[unit.output_bank][1][c].ToFloat32();
                    return color;
                };
                const float alpha=u[8].toArray()[3].toDouble();
                auto raw=shade(alpha);assert(raw[3]>1);
                // Reproduces the old bug using the ROM shader: half the input
                // alpha still saturates to 1 when OutputVertex clamps it.
                assert(shade(alpha*.5f)[3]>=1);
                struct Vertex {std::array<float,4> color,position;};
                Vertex native;native.position={1,2,3,4};
                for(unsigned c=0;c<4;++c)native.color[c]=std::clamp(std::abs(raw[c]),0.f,1.f);
                for(float fraction:{0.f,.25f,.5f,.75f,1.f}) {
                    std::vector<Vertex> current{native};
                    const auto previous=M::TextureCrossfade(current,fraction);
                    assert(current[0].color[3]==native.color[3]*fraction);
                    assert(previous[0].color[3]==native.color[3]*(1-fraction));
                    assert(current[0].color[3]+previous[0].color[3]==native.color[3]);
                    assert(current[0].position==native.position && previous[0].position==native.position);
                    for(unsigned c=0;c<3;++c)assert(current[0].color[c]==native.color[c] && previous[0].color[c]==native.color[c]);
                }
                // Native opacity is also conserved when the actor is fading.
                native.color[3]=.3f;std::vector<Vertex> fading{native};
                const auto previous=M::TextureCrossfade(fading,.25f);
                assert(std::abs(fading[0].color[3]+previous[0].color[3]-.3f)<1e-6);
                std::cout<<"ROM shader alpha="<<raw[3]<<"; post-clamp crossfade and opacity conservation passed\n";
                checked=true;++shader_checked;
            }
        }
        std::cout<<file<<": Biri draws="<<seen<<" pairs="<<paired<<" blends="<<blends<<'\n';
        assert(seen && paired && blends);
    }
    assert(shader_checked==2);
}
