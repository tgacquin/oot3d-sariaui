// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/song_visual_interpolation.h"
#include "oot3d/storm_clouds.h"
#include "video_core/pica/regs_internal.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cassert>
#include <iostream>
namespace S=OOT3D::Interpolation::SongVisual;
namespace M=OOT3D::Interpolation::Math;
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
    stage(3,r.texturing.tev_stage3);stage(4,r.texturing.tev_stage4);stage(5,r.texturing.tev_stage5);
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
    using OOT3D::Interpolation::sample_fraction;
    assert(argc>1);
    for(int arg=1;arg<argc;++arg) {
        QFile f(argv[arg]);assert(f.open(QIODevice::ReadOnly));
        const auto capture=QJsonDocument::fromJson(f.readAll()).object();
        assert(!capture["truncated"].toBool() && capture["dropped_draws"].toInt()==0);
        const auto draws=capture["draws"].toArray();
        for(float phase:{.25f,.5f,.75f,1.f}) {
            sample_fraction=phase;S::History history;
            std::map<S::Key,std::pair<std::uint64_t,S::Sample>> previous;
            unsigned recognized=0,paired=0,scroll=0,fade=0,moved=0,clouds=0;
            for(const auto value:draws) {
                const auto d=value.toObject();const auto r=Regs(d);
                if(d["shader"].toString()!="6352a3be33cb99c6" || d["swizzle"].toString()!="ba817d0ef27276d0")continue;
                auto layer=S::Candidate(r);
                const bool cloud=OOT3D::StormClouds::Candidate(r);
                if(cloud)layer=S::Layer::Clouds;
                if(layer==S::Layer::None)continue;
                ++recognized;clouds+=cloud;
                const auto u=d["uniforms_f"].toArray();
                S::Sample s;auto camera=M::Identity();
                if(!cloud)for(unsigned row=0;row<3;++row)for(unsigned c=0;c<4;++c) {
                    camera[row][c]=u[76+row].toArray()[c].toDouble();
                    s.model_view[row][c]=u[20+row].toArray()[c].toDouble();
                }
                for(unsigned row=0;row<S::UVRows(layer);++row)for(unsigned c=0;c<4;++c)
                    s.uv[row][c]=u[S::Rows[row]].toArray()[c].toDouble();
                for(unsigned c=0;c<4;++c)s.color[c]=u[8].toArray()[c].toDouble();
                s.alpha=float(r.texturing.tev_stage0.const_color>>24)/255.f;
                const auto frame=d["frame"].toInteger();
                const S::Key key{cloud?0u:std::uint32_t(d["vertex_base"].toInteger()),
                    cloud?0u:std::uint32_t(d["index_offset"].toInteger()),
                    layer==S::Layer::Backdrop?0u:r.texturing.texture0.GetPhysicalAddress(),
                    std::uint32_t(d["generation"].toInt()),std::uint32_t(d["scene"].toInt()),1,1,unsigned(layer)};
                if(cloud) {
                    std::array<OOT3D::Widescreen::Row,4> projection,vertices;
                    std::array<OOT3D::Widescreen::Row,3> model;
                    for(unsigned row=0;row<4;++row)for(unsigned c=0;c<4;++c) {
                        projection[row][c]=u[row].toArray()[c].toDouble();
                        if(row<3)model[row][c]=u[20+row].toArray()[c].toDouble();
                    }
                    const auto attrs=d["input_attributes"].toArray();
                    if(!attrs.empty()) {
                        assert(attrs.size()==3);
                        for(unsigned i=0;i<3;++i)for(unsigned c=0;c<4;++c)vertices[i][c]=attrs[i].toArray()[0].toArray()[c].toDouble();
                        vertices[3]={400,240,0,1};
                        assert(OOT3D::StormClouds::Geometry(projection,model,vertices));
                        vertices[3][0]=399;assert(!OOT3D::StormClouds::Geometry(projection,model,vertices));
                    } else {
                        std::array<OOT3D::Widescreen::Row,6> clip;
                        const auto output=d["output_vertices"].toArray();assert(output.size()==6);
                        for(unsigned i=0;i<6;++i)for(unsigned c=0;c<4;++c)clip[i][c]=output[i].toObject()["clip_position"].toArray()[c].toDouble();
                        assert(OOT3D::Widescreen::FadeClipGeometry(clip));
                    }
                    auto changed=r;changed.texturing.tev_stage0.ops_raw=0;assert(!OOT3D::StormClouds::Candidate(changed));
                } else {
                    auto changed=r;changed.framebuffer.output_merger.depth_write_enable.Assign(1);assert(S::Candidate(changed)==S::Layer::None);
                }
                const auto half=history.Apply(frame,key,layer,camera,s);
                const auto it=previous.find(key);
                const bool adjacent=it!=previous.end() && it->second.first+1==frame;
                assert(bool(half)==adjacent);
                if(half) {
                    ++paired;const auto& old=it->second.second;
                    for(unsigned row=0;row<S::UVRows(layer);++row) {
                        float delta=s.uv[row][3]-old.uv[row][3];
                        if(layer==S::Layer::Storm)delta-=std::round(delta);
                        assert(std::abs(half->uv[row][3]-(s.uv[row][3]-delta*(1-phase)))<.00001f);
                        scroll+=std::abs(delta)>.000001f;
                    }
                    for(unsigned c=0;c<4;++c)assert(std::abs(half->color[c]-std::lerp(old.color[c],s.color[c],phase))<.000001f);
                    assert(std::abs(half->alpha-std::lerp(old.alpha,s.alpha,phase))<.000001f);
                    fade+=old.color[3]!=s.color[3] || old.alpha!=s.alpha;
                    moved+=half->model_blended && !M::Near(old.model_view,s.model_view,.001);
                    if(phase==1 && half->model_blended)assert(M::Near(half->model_view,s.model_view,.0001));
                    // A replay must never change its native source sample.
                    const auto copy=s;
                    S::History reset;assert(!reset.Apply(frame,key,layer,camera,s));
                    assert(!reset.Apply(frame,key,layer,camera,s)); // Duplicate draw invalidates pairing.
                    assert(!reset.Apply(frame+1,key,layer,camera,s));
                    assert(reset.Apply(frame+2,key,layer,camera,s));
                    auto other=key;++other[3];assert(!reset.Apply(frame+3,other,layer,camera,s));
                    assert(s.uv==copy.uv && s.color==copy.color && s.model_view==copy.model_view);
                }
                previous[key]={frame,s};
            }
            std::cout<<argv[arg]<<" phase="<<phase<<" layers="<<recognized<<" pairs="<<paired
                <<" scroll="<<scroll<<" fade="<<fade<<" movement="<<moved<<" clouds="<<clouds<<std::endl;
            assert(recognized>80 && paired>50 && scroll>0 && fade>0);
        }
    }
    sample_fraction=.5f;
}
