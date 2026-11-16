// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/circle_wipe_interpolation.h"
#include "video_core/pica/regs_internal.h"
#include <cassert>
#include <iostream>
#include "video_core/pica/shader_setup.h"
#include "video_core/pica/shader_unit.h"
#include "video_core/shader/shader_interpreter.h"
#include <cstring>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
struct Vertex {
    std::array<float,4> position{},color{};
    std::array<float,2> tex_coord0{},tex_coord1{},tex_coord2{};
    float tex_coord0_w=1;
};
Pica::RegsInternal Regs(const QJsonObject& d) {
    Pica::RegsInternal r{};
    r.pipeline.num_vertices=d["vertices"].toInt();
    r.pipeline.triangle_topology.Assign(Pica::PipelineRegs::TriangleTopology(d["topology"].toInt()));
    const auto attrs=d["attribute_registers"].toArray();
    for(unsigned i=0;i<8;++i)r.vs.input_attribute_to_register_map_low|=attrs[i].toInt()<<(i*4);
    auto textures=d["textures"].toArray();
    auto texture=[&](unsigned i,auto& cfg) {
        const auto t=textures[i].toObject();
        cfg.width.Assign(t["width"].toInt());cfg.height.Assign(t["height"].toInt());
        cfg.address.Assign(t["address"].toInteger()/8);
        cfg.wrap_s.Assign(Pica::TexturingRegs::TextureConfig::WrapMode(t["wrap_s"].toInt()));
        cfg.wrap_t.Assign(Pica::TexturingRegs::TextureConfig::WrapMode(t["wrap_t"].toInt()));
    };
    texture(0,r.texturing.texture0);texture(1,r.texturing.texture1);texture(2,r.texturing.texture2);
    r.texturing.texture0_format.Assign(Pica::TexturingRegs::TextureFormat(textures[0].toObject()["format"].toInt()));
    r.texturing.texture1_format.Assign(Pica::TexturingRegs::TextureFormat(textures[1].toObject()["format"].toInt()));
    r.texturing.texture2_format.Assign(Pica::TexturingRegs::TextureFormat(textures[2].toObject()["format"].toInt()));
    r.texturing.main_config.texture0_enable.Assign(textures[0].toObject()["enabled"].toInt());
    r.texturing.main_config.texture1_enable.Assign(textures[1].toObject()["enabled"].toInt());
    r.texturing.main_config.texture2_enable.Assign(textures[2].toObject()["enabled"].toInt());
    const auto tev=d["tev"].toArray();
    auto stage=[&](unsigned i,auto& s) {
        const auto v=tev[i].toArray();s.sources_raw=v[0].toInteger();s.modifiers_raw=v[1].toInteger();
        s.ops_raw=v[2].toInteger();s.const_color=v[3].toInteger();s.scales_raw=v[4].toInteger();
    };
    stage(0,r.texturing.tev_stage0);stage(1,r.texturing.tev_stage1);stage(2,r.texturing.tev_stage2);
    const auto blend=d["blend"].toArray(),depth=d["depth"].toArray();
    auto& m=r.framebuffer.output_merger;
    m.alphablend_enable.Assign(blend[0].toInt());m.depth_test_enable.Assign(depth[0].toInt());
    m.depth_write_enable.Assign(depth[1].toInt());m.stencil_test.enable.Assign(depth[2].toInt());
    m.alpha_blending.factor_source_rgb.Assign(Pica::FramebufferRegs::BlendFactor(blend[1].toInt()));
    m.alpha_blending.factor_dest_rgb.Assign(Pica::FramebufferRegs::BlendFactor(blend[2].toInt()));
    m.alpha_blending.blend_equation_rgb.Assign(Pica::FramebufferRegs::BlendEquation(blend[5].toInt()));
    return r;
}
int main(int argc,char** argv) {
    assert(argc==3);
    QFile shader_file(argv[2]);assert(shader_file.open(QIODevice::ReadOnly));const auto bytes=shader_file.readAll();
    const auto read=[&](unsigned at){assert(at+4<=bytes.size());std::uint32_t v;std::memcpy(&v,bytes.data()+at,4);return v;};
    Pica::ShaderSetup setup;Pica::Shader::InterpreterEngine engine;
    const unsigned dvlp=12,dvle=read(8);
    for(unsigned i=0;i<read(dvlp+12);++i)setup.UpdateProgramCode(i,read(dvlp+read(dvlp+8)+i*4));
    for(unsigned i=0;i<read(dvlp+20);++i)setup.UpdateSwizzleData(i,read(dvlp+read(dvlp+16)+i*8));
    assert(setup.GetProgramCodeHash()==0x6352a3be33cb99c6ULL);
    assert(setup.GetSwizzleDataHash()==0xba817d0ef27276d0ULL);
    engine.SetupBatch(setup,read(dvle+8));
    QFile f(argv[1]);assert(f.open(QIODevice::ReadOnly));
    const auto draws=QJsonDocument::fromJson(f.readAll()).object()["draws"].toArray();
    namespace C=OOT3D::CircleWipe;
    C::History<Vertex> history;
    unsigned found=0,pairs=0,cpu_draws=0;
    for(const auto value:draws) {
        const auto d=value.toObject();auto regs=Regs(d);
        const auto shader=d["shader"].toString().toULongLong(nullptr,16),swizzle=d["swizzle"].toString().toULongLong(nullptr,16);
        if(!C::Candidate(regs,shader,swizzle))continue;
        ++found;const bool cpu=d["cpu"].toBool();cpu_draws+=cpu;
        assert(d["vertices"].toInt()==13 && (cpu || d["last_vertex"].toInt()==8));
        regs.texturing.main_config.texture1_enable.Assign(1);assert(!C::Candidate(regs,shader,swizzle));
        regs.texturing.main_config.texture1_enable.Assign(0);
        regs.texturing.main_config.texture2_enable.Assign(1);assert(!C::Candidate(regs,shader,swizzle));
        regs.texturing.main_config.texture2_enable.Assign(0);
        regs.framebuffer.output_merger.depth_test_enable.Assign(1);assert(!C::Candidate(regs,shader,swizzle));
        C::Projection projection{};const auto u=d["uniforms_f"].toArray();
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)projection[r][c]=u[r].toArray()[c].toDouble();
        // Accelerated capture retains the first three corners, not all nine.
        // Execute the actual ROM shader for these corners; do not substitute
        // synthetic outputs, which miss the changing unused UV channels.
        const auto inputs=d["input_attributes"].toArray();std::vector<Vertex> vertices(3);
        for(unsigned row=0;row<96;++row)for(unsigned c=0;c<4;++c)
            setup.uniforms.f[row][c]=Pica::f24::FromFloat32(u[row].toArray()[c].toDouble());
        for(unsigned i=0;i<16;++i)setup.uniforms.b[i]=(d["uniforms_b_mask"].toInt()>>i)&1;
        const auto integers=d["uniforms_i"].toArray();
        for(unsigned i=0;i<4;++i)for(unsigned c=0;c<4;++c)setup.uniforms.i[i][c]=integers[i].toArray()[c].toInt();
        if(cpu) {
            const auto outputs=d["output_vertices"].toArray();assert(!outputs.empty());
            vertices.resize(outputs.size());
            for(unsigned i=0;i<vertices.size();++i) {
                const auto o=outputs[i].toObject();
                for(unsigned c=0;c<4;++c) {
                    vertices[i].position[c]=o["clip_position"].toArray()[c].toDouble();
                    vertices[i].color[c]=o["color"].toArray()[c].toDouble();
                }
                for(unsigned c=0;c<2;++c) {
                    vertices[i].tex_coord0[c]=o["uv012"].toArray()[c].toDouble();
                    vertices[i].tex_coord1[c]=o["uv012"].toArray()[2+c].toDouble();
                    vertices[i].tex_coord2[c]=o["uv012"].toArray()[4+c].toDouble();
                }
                // Older captures omitted the projective coordinate. They verify
                // only captured channels, not full runtime eligibility.
                vertices[i].tex_coord0_w=o["uv0_w"].toDouble(1);
            }
        } else for(unsigned i=0;i<3;++i) {
            Pica::ShaderUnit unit;
            const auto input=inputs[i].toArray(),map=d["attribute_registers"].toArray();
            for(unsigned a=0;a<8;++a)for(unsigned c=0;c<4;++c)
                unit.input[map[a].toInt()][c]=Pica::f24::FromFloat32(input[a].toArray()[c].toDouble());
            engine.Run(setup,unit);
            const auto& o=unit.output[unit.output_bank];
            for(unsigned c=0;c<4;++c){vertices[i].position[c]=o[0][c].ToFloat32();vertices[i].color[c]=std::clamp(std::abs(o[1][c].ToFloat32()),0.f,1.f);}
            for(unsigned c=0;c<2;++c){vertices[i].tex_coord0[c]=o[2][c].ToFloat32();vertices[i].tex_coord1[c]=o[3][c].ToFloat32();vertices[i].tex_coord2[c]=o[4][c].ToFloat32();}
            vertices[i].tex_coord0_w=o[2][2].ToFloat32();
        }
        const auto original=vertices;const auto previous=history.previous;
        if(history.Apply(vertices,d["frame"].toInteger(),d["time_us"].toInteger(),d["generation"].toInt(),
            d["scene"].toInt(),regs.texturing.texture0.GetPhysicalAddress(),d["target"].toInteger(),projection)) {
            ++pairs;
            for(unsigned i=0;i<vertices.size();++i)for(unsigned c=0;c<2;++c)
                assert(vertices[i].tex_coord0[c]==(original[i].tex_coord0[c]+previous[i].tex_coord0[c])*.5f);
            assert(vertices[0].color==original[0].color && vertices[0].position[0]==original[0].position[0]);
            for(unsigned i=0;i<vertices.size();++i) {
                assert(vertices[i].tex_coord1==original[i].tex_coord1);
                assert(vertices[i].tex_coord2==original[i].tex_coord2);
            }
        }
    }
    assert(cpu_draws ? (found==38 && pairs==24) : (found==43 && pairs==28));
    // Cover the full CPU-expanded 13-index strip budget, while retaining
    // the capture limitation: only its first three distinct vertices survived.
    auto expanded=history.previous;while(expanded.size()<33)expanded.push_back(expanded[expanded.size()%3]);
    C::History<Vertex> strip;
    auto p=history.projection;
    assert(!strip.Apply(expanded,1,100000,1,1,2,3,p));
    auto next=expanded;
    for(auto& v:next){v.tex_coord0[0]+=.1f;v.tex_coord1[0]+=.3f;v.tex_coord2[0]-=.2f;}
    assert(strip.Apply(next,2,133333,1,1,2,3,p));
    assert(std::abs(next.back().tex_coord0[0]-expanded.back().tex_coord0[0]-.05f)<.00001f);
    // Ambiguous duplicate submissions, changed framebuffer/projection,
    // resets and invalid inputs must not borrow a previous wipe.
    const auto guard=[&](unsigned epoch,unsigned scene,unsigned target,C::Projection lens,bool duplicate) {
        C::History<Vertex> h;auto v=expanded;assert(!h.Apply(v,1,100000,1,1,2,3,p));
        if(duplicate){assert(!h.Apply(v,1,100000,1,1,2,3,p));}
        assert(!h.Apply(v,2,133333,epoch,scene,2,target,lens));
    };
    guard(2,1,3,p,false);guard(1,2,3,p,false);guard(1,1,4,p,false);guard(1,1,3,p,true);
    auto changed=p;changed[0][1]*=2;guard(1,1,3,changed,false);
    auto native=history.previous;
    assert(!history.Apply(native,history.frame+1,history.time+33333,history.generation,history.scene+1,history.texture,history.target,p));
    native=history.previous;
    assert(!history.Apply(native,history.frame+2,history.time+66666,history.generation,history.scene,history.texture,history.target,p));
    native=history.previous;native[0].tex_coord0[0]+=10;
    assert(!history.Apply(native,history.frame+1,history.time+33333,history.generation,history.scene,history.texture,history.target,p));
    std::cout<<"Circle wipe: "<<found<<" captured draws, "<<pairs<<" adjacent captured/shader UV/depth pairs; depth-tested draws, scene changes, gaps and UV jumps rejected\n";
}
