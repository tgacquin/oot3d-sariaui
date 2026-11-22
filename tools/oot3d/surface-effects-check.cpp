// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/surface_effects.h"
#include "oot3d/waterfall_interpolation.h"
#include "oot3d/fairy_glow_interpolation.h"
#include "video_core/pica/regs_internal.h"
#include <cassert>
#include <iostream>
#include <string_view>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
namespace E=OOT3D::Interpolation::SurfaceEffects;
namespace W=OOT3D::Interpolation::Waterfall;
namespace D=OOT3D::Interpolation::Debris;
namespace G=OOT3D::Interpolation::FairyGlow;
namespace T=OOT3D::Interpolation::WarpTrail;
struct DebrisBatch {std::vector<D::Vertex> inputs;std::vector<std::uint32_t> order;};
// New CPU captures retain the complete strip. Older accelerated captures
// contain three input corners: reconstruct only their first-quad fixture.
DebrisBatch CapturedDebris(const QJsonObject& d) {
    DebrisBatch b;
    const auto exact=d["effect_inputs"].toArray();
    if(!exact.empty()) {
        b.inputs.resize(exact.size());
        for(unsigned i=0;i<b.inputs.size();++i)for(unsigned a=0;a<4;++a)for(unsigned c=0;c<4;++c)
            b.inputs[i][a][c]=exact[i].toArray()[a].toArray()[c].toDouble();
        for(auto i:d["effect_order"].toArray())b.order.push_back(i.toInt());
        return b;
    }
    const auto attrs=d["input_attributes"].toArray();assert(attrs.size()==3);
    b.inputs.resize(5);b.order={0,1,2,3,3,4};
    for(unsigned i=0;i<3;++i)for(unsigned a=0;a<4;++a)for(unsigned c=0;c<4;++c)
        b.inputs[i][a][c]=attrs[i].toArray()[a].toArray()[c].toDouble();
    b.inputs[3]=b.inputs[0];
    for(unsigned c=0;c<3;++c)b.inputs[3][0][c]=b.inputs[1][0][c]+b.inputs[2][0][c]-b.inputs[0][0][c];
    b.inputs[3][3]=b.inputs[2][3];b.inputs[3][3][0]=b.inputs[1][3][0];
    return b;
}
struct Batch {std::vector<E::Vertex> inputs;std::vector<std::uint32_t> order;};
// Prefer the full, original replay inputs. Legacy CPU captures only retain
// output triangles: those are reconstructed as across-quad geometry fixtures,
// not a faithful record of the original input order or the complete batch.
Batch CapturedEffect(const QJsonObject& d) {
    Batch b;
    const auto exact=d["effect_inputs"].toArray();
    if(!exact.empty()) {
        b.inputs.resize(exact.size());
        for(unsigned i=0;i<b.inputs.size();++i)for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
            b.inputs[i][a][c]=exact[i].toArray()[a].toArray()[c].toDouble();
        for(auto i:d["effect_order"].toArray())b.order.push_back(i.toInt());
        return b;
    }
    const auto attrs=d["input_attributes"].toArray();
    if(!attrs.empty()) {
        b.inputs.resize(5);b.order={0,1,2,3,3,4};
        for(unsigned i=0;i<3;++i)for(unsigned a=0;a<3;++a)for(unsigned c=0;c<4;++c)
            b.inputs[i][a][c]=attrs[i].toArray()[a].toArray()[c].toDouble();
        b.inputs[3]=b.inputs[0];
        for(unsigned c=0;c<3;++c)b.inputs[3][0][c]=b.inputs[1][0][c]+b.inputs[2][0][c]-b.inputs[0][0][c];
        b.inputs[3][2]=b.inputs[2][2];b.inputs[3][2][0]=b.inputs[1][2][0];
        return b;
    }
    const auto outputs=d["output_vertices"].toArray(),u=d["uniforms_f"].toArray();
    assert(outputs.size()==12 || outputs.size()==30);
    double inverse[3][6]{};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<6;++c)
        inverse[r][c]=c<3?u[20+r].toArray()[c].toDouble():double(c-3==r);
    for(unsigned c=0;c<3;++c) {
        unsigned pivot=c;for(unsigned r=c+1;r<3;++r)if(std::abs(inverse[r][c])>std::abs(inverse[pivot][c]))pivot=r;
        for(unsigned k=0;k<6;++k)std::swap(inverse[c][k],inverse[pivot][k]);
        const double divisor=inverse[c][c];assert(std::abs(divisor)>.000001);
        for(auto& v:inverse[c])v/=divisor;
        for(unsigned r=0;r<3;++r)if(r!=c) {const double factor=inverse[r][c];for(unsigned k=0;k<6;++k)inverse[r][k]-=factor*inverse[c][k];}
    }
    // Legacy fixtures use across-quad corners from the first output triangles.
    // The native input order may instead run down the quad (outputs 0,1,2,5).
    // Two quads produce ten strip triangles. Retain the down-then-across
    // order of each quad and the connector to the next one.
    std::vector<unsigned> corners;
    if(outputs.size()==30)corners={0,1,2,5,8,11,18,19,20,23,26,29};
    else corners={0,2,1,5,8,11};
    for(unsigned index:corners) {
        const auto v=outputs[index].toObject();const auto clip=v["clip_position"].toArray();
        double eye[]{clip[1].toDouble()/u[1].toArray()[0].toDouble(),
                     clip[0].toDouble()/u[0].toArray()[1].toDouble(),-clip[3].toDouble()};
        for(unsigned c=0;c<3;++c)eye[c]-=u[20+c].toArray()[3].toDouble();
        E::Vertex input{};input[0][3]=input[2][3]=1;
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)input[0][r]+=float(inverse[r][c+3]*eye[c]);
        for(unsigned c=0;c<4;++c)input[1][c]=v["color"].toArray()[c].toDouble();
        for(unsigned c=0;c<2;++c)input[2][c]=v["uv012"].toArray()[c].toDouble();
        b.order.push_back(b.inputs.size());b.inputs.push_back(input);
    }
    return b;
}
Batch NonIndexed(const Batch& b) {
    Batch out;
    for(auto index:b.order){out.order.push_back(out.inputs.size());out.inputs.push_back(b.inputs[index]);}
    return out;
}
void Add(Batch& b,const std::array<E::Vertex,4>& q) {
    const auto n=unsigned(b.inputs.size());
    for(auto v:q)b.inputs.push_back(v);
    for(unsigned i:{0u,1u,2u,3u,3u,4u})b.order.push_back(n+i);
}
void End(Batch& b){b.inputs.push_back({});}
std::array<E::Vertex,4> Star(float x,float angle,float alpha=0.5f) {
    std::array<E::Vertex,4> q{};
    for(unsigned i=0;i<4;++i) {
        const float u=i%2?1.f:-1.f,v=i<2?-1.f:1.f;
        q[i][0]={x+u*std::cos(angle)-v*std::sin(angle),u*std::sin(angle)+v*std::cos(angle),0,1};
        q[i][1]={5.f/255,75.f/255,155.f/255,alpha};q[i][2]={float(i%2),float(1-i/2),0,1};
    }
    return q;
}
Batch Ring(float radius,float alpha) {
    Batch b;
    constexpr int x[]{-1,1,-1,1,1,-1,1,-1},z[]{-1,-1,0,0,1,1,0,0};
    for(unsigned half=0;half<2;++half) {
        std::array<E::Vertex,4> q{};
        for(unsigned i=0;i<4;++i) {
            const auto k=half*4+i;
            q[i][0]={379.23987f+x[k]*radius,-11,-498.f+z[k]*radius,1};
            q[i][1]={0,0,0,alpha};q[i][2]={float(1-i%2),float(i/2),0,1};
        }
        Add(b,q);
    }
    End(b);return b;
}
Batch Wave(float size,float alpha,bool down_strip) {
    std::array<E::Vertex,4> q{};
    for(unsigned i=0;i<4;++i) {
        const float x=(float(i%2)-.5f)*size,z=(float(i/2)-.5f)*size;
        // A tilted plane, centered at a fixed point as the wave expands.
        q[i][0]={x,1.f-.036f*x-.024f*z,z,1};
        q[i][1]={.2f,.2f,.2f,alpha};q[i][2]={float(i%2)*2,float(1-i/2)*2,0,1};
    }
    if(down_strip)std::swap(q[1],q[2]);
    Batch b;Add(b,q);End(b);return b;
}
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
// Legacy captures retain 33 output vertices, enough for two complete quads.
Batch CapturedFirstTwoQuads(const QJsonObject& d) {
    if(!d["effect_inputs"].toArray().empty())return CapturedEffect(d);
    const auto outputs=d["output_vertices"].toArray();assert(outputs.size()==33);
    Batch b;
    for(unsigned start:{0u,18u}) {
        auto copy=d;QJsonArray part;
        for(unsigned i=0;i<12;++i)part.push_back(outputs[start+std::min(i,5u)]);
        copy["output_vertices"]=part;
        const auto reconstructed=CapturedEffect(copy);
        std::array<E::Vertex,4> q;
        std::copy_n(reconstructed.inputs.begin(),4,q.begin());Add(b,q);
    }
    End(b);return NonIndexed(b);
}
void PondRingCheck(const char* path) {
    QFile file(path);assert(file.open(QIODevice::ReadOnly));
    const auto draws=QJsonDocument::fromJson(file.readAll()).object()["draws"].toArray();
    for(float phase:{.25f,.5f,.75f}) {
        OOT3D::Interpolation::sample_fraction=phase;
        E::History history;
        Batch previous;
        unsigned seen=0,paired=0,regular=0;
        qint64 last=-1;
        for(const auto value:draws) {
            const auto d=value.toObject();const auto r=Regs(d);
            const auto kind=E::Candidate(r,d["shader"].toString().toULongLong(nullptr,16),
                                           d["swizzle"].toString().toULongLong(nullptr,16));
            if(kind!=E::PondRings && kind!=2)continue;
            const auto b=kind==E::PondRings?CapturedFirstTwoQuads(d):CapturedEffect(d);
            const auto frame=d["frame"].toInteger();
            history.Context(d["generation"].toInt(),d["scene"].toInt());
            const auto result=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,
                [&](const auto& v,const auto& order) {
                    assert(E::Rings(v,order,kind==E::PondRings));
                    return history.Apply(kind,frame,r.texturing.texture0.GetPhysicalAddress(),v,order);
                });
            if(kind==2) {++regular;continue;}
            ++seen;
            if(result) {
                assert(last+1==frame);
                for(unsigned k=0;k<12;++k) {
                    const auto& v=(*result)[b.order[k]];
                    const auto& old=previous.inputs[previous.order[k]];
                    const auto& now=b.inputs[b.order[k]];
                    assert(v[2]==now[2]);
                    if(k==11)continue; // Last connector is outside the reconstructed ring.
                    for(unsigned a=0;a<2;++a)for(unsigned c=0;c<4;++c)
                        assert(std::abs(v[a][c]-std::lerp(old[a][c],now[a][c],phase))<.00025f);
                }
                assert((*result)[b.order[0]][1][3]>b.inputs[b.order[0]][1][3]);
                ++paired;
            }
            last=frame;previous=b;
        }
        assert(seen==88 && regular==10 && paired==41);
        std::cout<<"Pond ripple capture at "<<phase<<": "<<seen<<" batches, "<<paired
                 <<" matched first-ring transitions, "<<regular<<" ordinary ripple draws\n";
    }
}
void HaloCheck(const char* path) {
    QFile f(path);assert(f.open(QIODevice::ReadOnly));
    const auto draws=QJsonDocument::fromJson(f.readAll()).object()["draws"].toArray();
    for(float phase:{0.f,.25f,.5f,.75f,1.f}) {
        OOT3D::Interpolation::sample_fraction=phase;E::History history;
        unsigned seen=0,paired=0,moved=0;std::uint64_t previous_frame=0;Batch previous;
        for(const auto value:draws) {
            const auto d=value.toObject();const auto r=Regs(d);
            if(E::Candidate(r,0x6352a3be33cb99c6ULL,0xba817d0ef27276d0ULL)!=15)continue;
            ++seen;const auto b=CapturedFirstTwoQuads(d);const auto frame=d["frame"].toInteger();
            history.Context(d["generation"].toInt(),d["scene"].toInt());
            const auto result=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,
                [&](const auto& v,const auto& order){return history.Apply(15,frame,r.texturing.texture0.GetPhysicalAddress(),v,order);});
            if(result) {
                ++paired;assert(previous_frame+1==frame);
                for(unsigned i=0;i<result->size();++i) {
                    assert((*result)[i][2]==b.inputs[i][2]);
                    if(phase==1)assert((*result)[i]==b.inputs[i]);
                    if(i%6<4) {
                        bool changed=false;
                        for(unsigned c=0;c<3;++c)changed|=(*result)[i][0][c]!=b.inputs[i][0][c];
                        moved+=changed;
                        if(changed)for(unsigned c=0;c<3;++c)
                            assert(std::abs((*result)[i][0][c]-std::lerp(previous.inputs[i][0][c],b.inputs[i][0][c],phase))<.0002f);
                    }
                }
                assert(history.OldSpriteInputs().empty()); // No atlas crossfade.
            }
            previous_frame=frame;previous=b;
            auto other=r;other.pipeline.num_vertices=4;
            assert(E::Candidate(other,0x6352a3be33cb99c6ULL,0xba817d0ef27276d0ULL)!=15);
            other=r;other.texturing.texture0.wrap_s.Assign(Pica::TexturingRegs::TextureConfig::WrapMode(0));
            assert(E::Candidate(other,0x6352a3be33cb99c6ULL,0xba817d0ef27276d0ULL)!=15);
        }
        std::cout<<"Outer halo phase "<<phase<<": "<<seen<<" draws, "<<paired<<" paired partial batches, "<<moved<<" moving vertices\n";
        assert(seen==75 && paired>=40 && (phase==1 || moved>200));
    }
    // All three layers, changing opacity, movement, exact endpoints and
    // duplicated strip connectors. Gaps, scene changes and teleports fail closed.
    for(float phase:{0.f,.25f,.5f,.75f,1.f}) {
        OOT3D::Interpolation::sample_fraction=phase;E::History history;history.Context(1,85);
        const auto batch=[](float shift,float alpha) {
            Batch b;
            for(unsigned i=0;i<3;++i) {
                auto q=Star(0,0,alpha);
                for(auto& v:q) {
                    for(unsigned c=0;c<3;++c)v[0][c]*=13.f-i*3.f;
                    v[0][0]+=shift;v[0][2]=float(i)*16;
                    v[1][0]=v[1][1]=v[1][2]=1.f/3;
                    v[2][0]*=2;v[2][1]*=2;
                }
                Add(b,q);
            }
            End(b);return NonIndexed(b);
        };
        const auto a=batch(0,.2f),b=batch(2,.3f);
        const auto apply=[&](unsigned frame,const Batch& b) {
            return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,
                [&](const auto& v,const auto& order){return history.Apply(15,frame,42,v,order);});
        };
        assert(!apply(1,a));const auto result=apply(2,b);assert(result);
        for(unsigned i=0;i<18;++i) {
            if(i==17)continue; // Native trailing degenerate.
            assert(std::abs((*result)[i][0][0]-std::lerp(a.inputs[i][0][0],b.inputs[i][0][0],phase))<.00001f);
            assert(std::abs((*result)[i][1][3]-std::lerp(.2f,.3f,phase))<.00001f);
            assert((*result)[i][2]==b.inputs[i][2]);
        }
        assert(!apply(4,b));history.Context(2,85);assert(!apply(5,b));
        assert(!apply(6,batch(1000,.3f)));
        history.Reset();assert(!apply(1,a));assert(apply(2,b));
        assert(!apply(2,b) && !apply(2,b) && !apply(3,b));
    }
}
void LakeFogFixture() {
    Pica::RegsInternal r{};auto& t=r.texturing;auto& m=r.framebuffer.output_merger;
    r.pipeline.num_vertices=216;r.pipeline.triangle_topology.Assign(Pica::PipelineRegs::TriangleTopology(3));
    t.main_config.texture0_enable.Assign(1);t.main_config.texture1_enable.Assign(1);
    for(auto* tex:{&t.texture0,&t.texture1}) {
        tex->width.Assign(64);tex->height.Assign(64);tex->address.Assign(0x10000);
        tex->wrap_s.Assign(Pica::TexturingRegs::TextureConfig::WrapMode(2));
        tex->wrap_t.Assign(Pica::TexturingRegs::TextureConfig::WrapMode(2));
    }
    t.texture0_format.Assign(Pica::TexturingRegs::TextureFormat(12));
    t.texture1_format.Assign(Pica::TexturingRegs::TextureFormat(12));
    m.depth_test_enable.Assign(1);m.alphablend_enable.Assign(1);
    m.alpha_blending.factor_source_rgb.Assign(Pica::FramebufferRegs::BlendFactor(2));
    m.alpha_blending.factor_dest_rgb.Assign(Pica::FramebufferRegs::BlendFactor(12));
    t.tev_stage0.sources_raw=0x0e300e30;t.tev_stage0.ops_raw=0x10001;t.tev_stage0.scales_raw=1;
    t.tev_stage1.sources_raw=0x0e1f0f40;t.tev_stage1.ops_raw=8;
    assert(W::FogCandidate(r,0x57) && W::FogCandidate(r,0x40057));
    assert(!W::FogCandidate(r,0x55) && !W::FogCandidate(r,0x56));
    for(unsigned mesh:{120u,126u,1311u,156u,228u}) {
        auto other=r;other.pipeline.num_vertices=mesh;assert(!W::FogCandidate(other,0x57));
    }
    auto other=r;other.texturing.texture1.address.Assign(0x20000);assert(!W::FogCandidate(other,0x57));
    other=r;other.framebuffer.output_merger.depth_write_enable.Assign(1);assert(!W::FogCandidate(other,0x57));
    other=r;other.texturing.tev_stage1.ops_raw=1;assert(!W::FogCandidate(other,0x57));
    std::cout<<"Lake Hylia waterfall fog material, scene and neighboring-mesh guards passed\n";
}
void WellDripCheck(const char* path) {
    QFile file(path); assert(file.open(QIODevice::ReadOnly));
    const auto draws = QJsonDocument::fromJson(file.readAll()).object()["draws"].toArray();
    for (float phase : {0.f, .25f, .5f, .75f, 1.f}) {
        OOT3D::Interpolation::sample_fraction = phase;
        W::History history;
        std::map<std::array<unsigned,2>,std::pair<unsigned,float>> previous;
        unsigned paired=0, moved=0, held=0;
        for (const auto value : draws) {
            const auto d=value.toObject();
            const auto regs=Regs(d);
            const unsigned scene=d["scene"].toInt(), frame=d["frame"].toInt();
            if (!W::WellDripCandidate(regs,scene)) continue;
            assert(W::Candidate(regs,scene) && !W::WellDripCandidate(regs,1));
            auto unrelated=regs;
            unrelated.framebuffer.output_merger.depth_write_enable.Assign(1);
            assert(!W::WellDripCandidate(unrelated,scene));
            const auto u=d["uniforms_f"].toArray(); W::UV uv{};
            for(unsigned r=0;r<W::Rows.size();++r)for(unsigned c=0;c<4;++c)
                uv[r][c]=u[W::Rows[r]].toArray()[c].toDouble();
            auto& old=previous[{unsigned(regs.pipeline.num_vertices),regs.texturing.tev_stage1.sources_raw}];
            const auto sample=history.ApplyDraw(frame,regs,d["indexed"].toBool(),d["generation"].toInt(),scene,uv);
            assert(bool(sample)==(old.first+1==frame));
            if(sample) {
                const float end=uv[1][3];
                float delta=end-old.second;
                delta-=std::round(delta);
                assert(std::abs((*sample)[1][3]-(end-delta*(1-phase)))<1e-6f);
                for(unsigned r=0;r<W::Rows.size();++r)if(r!=1)assert((*sample)[r]==uv[r]);
                if (std::abs(delta)>1e-5f) ++moved; else ++held;
                ++paired;
            }
            old={frame,uv[1][3]};
        }
        assert(previous.size()==5 && paired==300 && moved>0 && held>0);
        std::cout<<"Well drips at phase "<<phase<<": "<<moved<<" moving + "<<held<<" stationary layer pairs"<<std::endl;
    }
}
void GroundFlakeCheck(const char* path, bool stalchild) {
    QFile file(path);
    assert(file.open(QIODevice::ReadOnly));
    const auto draws = QJsonDocument::fromJson(file.readAll()).object()["draws"].toArray();
    for (float phase : {.25f, .5f, .75f}) {
        OOT3D::Interpolation::sample_fraction = phase;
        D::History flakes;
        E::History dust;
        unsigned flake_batches = 0, moved_flakes = 0, dust_pairs = 0;
        for (const auto value : draws) {
            const auto d = value.toObject();
            const auto regs = Regs(d);
            const auto kind = E::Candidate(regs, d["shader"].toString().toULongLong(nullptr,16),
                                           d["swizzle"].toString().toULongLong(nullptr,16));
            const auto frame = d["frame"].toInteger();
            const auto texture = regs.texturing.texture0.GetPhysicalAddress();
            if (kind == 3) {
                const auto b = CapturedDebris(d);
                flakes.Context(d["generation"].toInt(), d["scene"].toInt());
                auto sampled = OOT3D::Interpolation::ReplayEffectStrip<D::Vertex>(b.inputs,b.order,
                    [&](const auto& v,const auto& order) { return flakes.Apply(frame,texture,v,order); });
                if (!sampled) continue;
                ++flake_batches;
                for (unsigned i=0; i<b.order.size(); i+=6) {
                    const unsigned k=b.order[i], next=b.order[i+1];
                    assert((*sampled)[b.order[i+3]]==(*sampled)[b.order[i+4]]);
                    if (i+6<b.order.size()) assert((*sampled)[b.order[i+5]]==(*sampled)[b.order[i+6]]);
                    if ((*sampled)[k][0]==b.inputs[k][0]) continue;
                    ++moved_flakes;
                    float before=0, after=0;
                    for (unsigned c=0;c<3;++c) {
                        before+=std::pow(b.inputs[next][0][c]-b.inputs[k][0][c],2);
                        after+=std::pow((*sampled)[next][0][c]-(*sampled)[k][0][c],2);
                    }
                    assert(std::abs(std::sqrt(before)-std::sqrt(after))<.015f);
                    assert((*sampled)[k][2]==b.inputs[k][2] && (*sampled)[k][3]==b.inputs[k][3]);
                }
            } else if (kind==14) {
                const auto b=CapturedEffect(d);
                dust.Context(d["generation"].toInt(), d["scene"].toInt());
                const auto sampled=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,
                    [&](const auto& v,const auto& order){ return dust.Apply(kind,frame,texture,v,order); });
                if(sampled && *sampled!=b.inputs) ++dust_pairs;
            }
        }
        std::cout<<(stalchild ? "Stalchild" : "Peahat")<<" phase "<<phase<<": "<<moved_flakes
                 <<" moving flakes in "<<flake_batches<<" batches; "<<dust_pairs<<" dust batches changed"<<std::endl;
        if (stalchild) {
            assert(flake_batches == 44 && moved_flakes == 598);
        } else {
            assert(flake_batches == 60 && moved_flakes >= 2700 && dust_pairs == 120);
        }
    }
}
int main(int argc,char** argv) {
    LakeFogFixture();
    if(argc==3 && std::string_view(argv[1])=="--pond-ripples") {PondRingCheck(argv[2]);return 0;}
    if(argc==3 && std::string_view(argv[1])=="--peahat-ground") {GroundFlakeCheck(argv[2],false);return 0;}
    if(argc==3 && std::string_view(argv[1])=="--stalchild-ground") {GroundFlakeCheck(argv[2],true);return 0;}
    if(argc==3 && std::string_view(argv[1])=="--well-drips") {WellDripCheck(argv[2]);return 0;}
    if(argc==3 && std::string_view(argv[1])=="--halo-only") {HaloCheck(argv[2]);return 0;}
    {
        const auto batch=[](float offset,bool birth=false,std::array<float,4> color={55.f/255,0,1,1}) {
            Batch b;
            for(unsigned node=0;node<(birth?3u:2u);++node)for(unsigned j=0;j<2;++j) {
                auto q=Star(float(node)*10.25f+offset+j,0,1);
                for(auto& v:q)v[1]=color;
                Add(b,q);
            }
            End(b);return NonIndexed(b);
        };
        for(const auto color:std::array<std::array<float,4>,7>{{
                {55.f/255,0,1,1},{1,0,0,1},{0,1,0,1},{0,0,1,1},
                {1,1,0,1},{0,1,1,1},{1,1,1,1}}})
        for(float phase:{.25f,.5f,.75f,1.f}) {
            OOT3D::Interpolation::sample_fraction=phase;E::History h;h.Context(1,2);
            const auto apply=[&](unsigned frame,const Batch& b) {
                return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order){return h.Apply(7,frame,7,v,order);});
            };
            const auto a=batch(0,false,color),b=batch(1,true,color);
            assert(!apply(10,a));const auto half=apply(11,b);assert(half);
            assert(std::abs((*half)[0][0][0]-std::lerp(a.inputs[0][0][0],b.inputs[0][0][0],phase))<.0001f);
            for(unsigned i=24;i<b.inputs.size();++i)assert((*half)[i]==b.inputs[i]);
            if(phase==1)assert(*half==b.inputs);
            assert(!apply(11,b) && !apply(12,b)); // Duplicate draw invalidates prior batch.
            assert(!apply(14,b));h.Context(2,2);assert(!apply(15,b));
            h.Context(2,3);assert(!apply(16,b));
            auto bad=b;bad.inputs[0][1][0]=1.1f;assert(!apply(17,bad));assert(!apply(18,b));
            bad=b;bad.inputs[0][1][0]=NAN;assert(!apply(19,bad));assert(!apply(20,b));
            auto other=color;other[0]=other[0]>.5f?0.f:1.f;
            assert(!apply(21,batch(1,true,other))); // Different color cannot inherit this trail.
            auto ambiguous=Star(0,0);auto duplicate=ambiguous;
            std::vector<E::Quad> quads(4);for(auto& q:quads)q.vertices=duplicate;
            E::Quad q;q.vertices=ambiguous;assert(!T::Match(q,quads));
        }
    }
    {
        const auto batch=[](float x,float angle,bool birth=false) {
            Batch b;
            const auto add=[&](float at) {
                auto q=Star(at,angle,1);
                for(auto& v:q){v[1]={0,0,0,1};v[2][0]*=2;v[2][1]*=2;}
                Add(b,q);
            };
            add(x);add(x+20);if(birth)add(x+40);End(b);return NonIndexed(b);
        };
        for(float phase:{.25f,.5f,.75f,1.f}) {
            OOT3D::Interpolation::sample_fraction=phase;
            E::History h;h.Context(1,2);
            const auto apply=[&](unsigned frame,const Batch& b,float alpha) {
                return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                    return h.Apply(6,frame,7,v,order,{1,1,1,alpha});
                });
            };
            const auto a=batch(0,0),b=batch(.5f,.35f);
            assert(!apply(10,a,.5f));const auto half=apply(11,b,.6f);assert(half);
            assert(h.SparkleColor() && std::abs((*h.SparkleColor())[3]-(.5f+.1f*phase))<.00001f);
            for(unsigned i=0;i<half->size();++i)assert((*half)[i][2]==b.inputs[i][2]);
            assert((*half)[3]==(*half)[4] && (*half)[5]==(*half)[6]);
            if(phase==1)assert(*half==b.inputs);
            const auto birth=batch(1,.7f,true);const auto added=apply(12,birth,.7f);assert(added);
            for(unsigned i=12;i<added->size();++i)assert((*added)[i]==birth.inputs[i]);
            assert(!h.SparkleColor());
            assert(!apply(12,birth,.7f));assert(!apply(13,birth,.7f)); // Ambiguous duplicate batch.
            assert(!apply(15,birth,.7f)); // Missing update.
            h.Context(1,3);assert(!apply(16,birth,.7f)); // Scene/warp reset.
            auto malformed=b;malformed.inputs[0][2][0]=.3f;
            assert(!apply(17,malformed,.7f));
        }
        OOT3D::Interpolation::sample_fraction=.5f;
    }
    {
        G::History history;history.Context(1,2);const G::Key key{1,2,3,1};
        auto camera=OOT3D::Interpolation::Math::Identity(),first=OOT3D::Interpolation::Math::Identity(),second=first;
        for(unsigned i=0;i<3;++i)first[i][i]=10;
        second[0]={12*std::cos(.2),-12*std::sin(.2),0,6};
        second[1]={12*std::sin(.2),12*std::cos(.2),0,0};second[2][2]=12;
        const G::Color a{0,0,1,.4},b{0,0,1,.6};
        assert(!history.Apply(10,key,camera,first,a));
        // Changing the view alone must not become fairy movement.
        camera[0]={std::cos(.3),-std::sin(.3),0,100};
        camera[1]={std::sin(.3),std::cos(.3),0,0};
        const auto view=OOT3D::Interpolation::Math::InverseCamera(camera);assert(view);
        auto current=OOT3D::Interpolation::Math::Multiply(*view,second);
        const auto half=history.Apply(11,key,camera,current,b);assert(half);
        const auto world=OOT3D::Interpolation::Math::Multiply(camera,half->model_view);
        assert(std::abs(world[0][3]-3)<.00001);
        assert(std::abs(std::hypot(world[0][0],world[1][0])-11)<.00001);
        assert(std::abs(std::atan2(world[1][0],world[0][0])-.1)<.00001);
        assert(std::abs(half->color[3]-.5)<.00001);
        assert(!history.Apply(11,key,camera,current,b)); // Ambiguous repeated draw.
        assert(!history.Apply(12,key,camera,current,b));
        assert(history.Apply(13,key,camera,current,b));
        assert(!history.Apply(15,key,camera,current,b));
        current[0][3]+=100;assert(!history.Apply(16,key,camera,current,b)); // Teleport.
        history.Context(2,2);assert(!history.Apply(17,key,camera,current,b));
        current[0][0]=std::numeric_limits<double>::quiet_NaN();assert(!history.Apply(18,key,camera,current,b));
        camera=OOT3D::Interpolation::Math::Identity();
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){first[r][c]*=-1;second[r][c]*=-1;}
        history.Reset();assert(!history.Apply(20,key,camera,first,a));
        const auto reflected=history.Apply(21,key,camera,second,b);assert(reflected);
        const auto decoded=G::Decode(camera,reflected->model_view,reflected->color);
        assert(decoded && std::abs(decoded->size+11)<.00001);
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)second[r][c]*=-1;
        assert(!history.Apply(22,key,camera,second,b)); // Do not rotate through a reflection.
        assert(G::Decode(camera,second,{-.027,0,1.027,.5}));
        std::array<G::Vertex,4> quad{};
        for(unsigned k=0;k<4;++k)quad[k]={{{float(k%2)-.5f,.5f-float(k/2),0,1},{float(k%2)*2,float(1-k/2)*2,0,1}}};
        assert(G::Geometry(quad));quad[3]=quad[0];assert(!G::Geometry(quad));
    }
    // Two independent 27-droplet bursts share a texture. Move the centers,
    // expand the clouds and reverse both draw order and droplet order.
    {
        namespace Blood=OOT3D::Interpolation::GreenParticles;
        const auto burst=[](unsigned age,float origin,bool reverse) {
            Batch b;
            for(unsigned n=0;n<27;++n) {
                const int i=reverse?26-int(n):int(n);
                auto quad=Star(origin+float(i-13)*age,.1f*age,1.f-age/24.f);
                for(auto& v:quad)v[0][1]+=float(i%3-1)*age;
                Add(b,quad);
            }
            End(b);return b;
        };
        for(float phase:{.25f,.5f,.75f}) {
            OOT3D::Interpolation::sample_fraction=phase;
            Blood::History history;
            const auto apply=[&](unsigned frame,unsigned age,float x,bool reverse) {
                const auto b=burst(age,x,reverse);
                return history.Apply(frame,42,b.inputs,b.order);
            };
            assert(!apply(10,2,0,false));assert(!apply(10,3,24,false));
            const auto check=[&](unsigned age,float old_x,float x) {
                const auto out=apply(11,age,x,true);assert(out);
                for(unsigned n=0;n<27;++n) {
                    float cx=0;
                    for(unsigned k=0;k<4;++k)cx+=(*out)[4*n+k][0][0]*.25f;
                    const float dx=13.f-float(n);
                    assert(std::abs(cx-std::lerp(old_x+dx*(age-1),x+dx*age,phase))<.001f);
                }
            };
            check(4,24,29);check(3,0,5);
            assert(!apply(11,1,0,true)); // Overlapping newborn.
            assert(!apply(13,4,10,true)); // Missing native frame.
            history.Context(2,82);assert(!apply(14,5,15,true));
        }
        OOT3D::Interpolation::sample_fraction=.5f;
    }
    // Blood sprites retain corner-specific lighting for every tint, including
    // dim green and neutral colors. Keep matching independent of tint while
    // still rejecting incompatible geometry, births and invalid colors.
    for(const auto tint:std::array<std::array<float,3>,6>{{
        {.02f,.45f,.02f},{.45f,.02f,.02f},{.02f,.02f,.45f},
        {.45f,.02f,.45f},{.3f,.3f,.3f},{0,.01f,0}}})
    for(bool down:{false,true})for(bool nonindexed:{false,true}) {
        const auto batch=[&](float shift,float angle,float alpha,bool reverse=false) {
            Batch b;
            for(unsigned i=0;i<3;++i) {
                auto q=Star(float(reverse?2-i:i)*20+shift,angle,alpha);
                for(unsigned k=0;k<4;++k)q[k][1]={tint[0]+float(k)*.03f,
                    tint[1]+float(k)*.03f,tint[2]+float(k)*.03f,alpha};
                if(down)std::swap(q[1],q[2]);
                Add(b,q);
            }
            End(b);return nonindexed?NonIndexed(b):b;
        };
        const auto first=batch(0,0,.8f),second=batch(.5f,.2f,.76f,true);
        E::History history;history.Context(1,85);
        const auto apply=[&](const Batch& b,unsigned frame) {
            return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& vertices,const auto& order) {
                return history.Apply(5,frame,42,vertices,order);
            });
        };
        assert(!apply(first,10));const auto half=apply(second,11);assert(half);
        for(unsigned i=0;i<second.order.size();i+=6) {
            float center=0;
            for(unsigned k=0;k<4;++k) {
                const auto index=second.order[i+k];const auto& v=(*half)[index];
                center+=v[0][0]*.25f;
                assert(v[2]==second.inputs[index][2]);
                for(unsigned c=0;c<3;++c)assert(v[1][c]==second.inputs[index][1][c]);
                assert(std::abs(v[1][3]-.78f)<.0001f);
            }
            assert(std::abs(center-(float(2-i/6)*20+.25f))<.0001f);
            // Read the U edge by UV order to check midpoint spin and size.
            const auto& a=(*half)[second.order[i]];
            const auto& b=(*half)[second.order[i+(down?2:1)]];
            const float x=b[0][0]-a[0][0],y=b[0][1]-a[0][1];
            assert(std::abs(std::hypot(x,y)-2)<.0001f);
            assert(std::abs(std::atan2(y,x)-.1f)<.0001f);
            assert((*half)[second.order[i+3]]==(*half)[second.order[i+4]]);
            if(i+6<second.order.size())assert((*half)[second.order[i+5]]==(*half)[second.order[i+6]]);
        }
        assert(half->back()==second.inputs.back());
        for(float invalid_color:{-.1f,1.1f,std::numeric_limits<float>::quiet_NaN()}) {
            auto invalid=second;invalid.inputs[0][1][0]=invalid_color;
            assert(!apply(invalid,12));
        }
        auto invalid=second;invalid.inputs[0][2]=invalid.inputs[1][2];assert(!apply(invalid,12));
        assert(!apply(first,13));assert(!apply(second,15));
        assert(!apply(batch(.6f,.3f,1),16)); // Newly spawned opacity cannot inherit old history.
        history.Context(2,85);assert(!apply(first,17));
    }
    for(bool down:{false,true})for(bool nonindexed:{false,true}) {
        auto first=Wave(160,.8f,down),second=Wave(190,.7f,down);
        if(nonindexed){first=NonIndexed(first);second=NonIndexed(second);}
        E::History history;
        const auto apply=[&](const Batch& b,unsigned frame) {
            return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                return history.Apply(4,frame,42,v,order);
            });
        };
        assert(!apply(first,10));const auto half=apply(second,11);assert(half);
        for(unsigned i=0;i<4;++i)for(unsigned a=0;a<2;++a)for(unsigned c=0;c<4;++c)
            assert(std::abs((*half)[second.order[i]][a][c]-(first.inputs[first.order[i]][a][c]+second.inputs[second.order[i]][a][c])*.5f)<.0001f);
        assert((*half)[second.order[3]]==(*half)[second.order[4]]);
        assert(half->back()==second.inputs.back());
        auto malformed=second;malformed.inputs[0][2][0]=.5f;assert(!apply(malformed,12));
        assert(!apply(first,13)); // Invalid geometry cleared history.
        assert(!apply(first,15)); // No pairing across a missing update.
        assert(!apply(Wave(10,1,down),16)); // A new wave cannot inherit the old one.
        // The explosive's ring persists when the enemy's ring joins the same draw.
        const auto together=[&](const Batch& first_wave,const Batch& next_wave) {
            Batch batch;
            for(const auto* source:{&first_wave,&next_wave}) {
                std::array<E::Vertex,4> q;
                for(unsigned i=0;i<4;++i)q[i]=source->inputs[source->order[i]];
                Add(batch,q);
            }
            End(batch);return nonindexed?NonIndexed(batch):batch;
        };
        history.Reset();assert(!apply(first,20));
        const auto pair=together(Wave(20,1,down),second);
        const auto born=apply(pair,21);assert(born);
        for(unsigned i=0;i<4;++i) {
            assert((*born)[pair.order[i]]==pair.inputs[pair.order[i]]);
            assert((*born)[pair.order[6+i]]==(*half)[second.order[i]]);
        }
        const auto remaining=Wave(40,.9f,down);
        assert(apply(remaining,22));
    }
    Batch a,b;for(unsigned i=0;i<32;++i) {Add(a,Star(i*10,0));Add(b,Star(i*10+.5f,.2f,.48f));}End(a);End(b);
    {
        Batch small_a,small_b;
        for(unsigned i=0;i<2;++i){Add(small_a,Star(i*10,0));Add(small_b,Star(i*10+.5f,.2f,.48f));}
        End(small_a);End(small_b);
        E::History original,shared;
        assert(!T::Decode(small_a.inputs,small_a.order));
        assert(!original.Apply(1,10,42,small_a.inputs,small_a.order));
        assert(!shared.Apply(7,10,42,small_a.inputs,small_a.order));
        const auto expected=original.Apply(1,11,42,small_b.inputs,small_b.order);
        assert(expected && expected==shared.Apply(7,11,42,small_b.inputs,small_b.order));
    }
    E::History h;h.Context(1,85);
    assert(E::Forest(a.inputs,a.order)->size()==32);
    assert(!h.Apply(1,10,42,a.inputs,a.order));auto half=h.Apply(1,11,42,b.inputs,b.order);assert(half);
    const float dx=(*half)[1][0][0]-(*half)[0][0][0],dy=(*half)[1][0][1]-(*half)[0][0][1];
    assert(std::abs(std::hypot(dx,dy)-2)<.001f);assert(std::abs(std::atan2(dy,dx)-.1f)<.001f);
    assert(std::abs((*half)[0][1][3]-.49f)<.0001f && (*half).back()==b.inputs.back());
    assert(!h.Apply(1,13,42,b.inputs,b.order));h.Context(2,85);assert(!h.Apply(1,14,42,b.inputs,b.order));
    h.Context(2,86);assert(!h.Apply(1,15,42,b.inputs,b.order));
    auto invalid=b;invalid.inputs[0][2][0]=.2f;assert(!E::Forest(invalid.inputs,invalid.order));
    invalid=b;invalid.order[4]=0;assert(!E::Forest(invalid.inputs,invalid.order));
    auto ra=Ring(42.96f,.09803922f),rb=Ring(43.12f,.06666667f);
    assert(E::Rings(ra.inputs,ra.order));assert(!h.Apply(2,20,43,ra.inputs,ra.order));
    auto rh=h.Apply(2,21,43,rb.inputs,rb.order);assert(rh);
    assert(std::abs((*rh)[0][0][0]-(379.23987f-43.04f))<.001f);
    // Real puddle strips run down each half, with shared connector indices.
    // Check both strip directions and duplicated-input replay, including
    // simultaneous rings whose halves/connectors must not become crossed.
    for(bool pond:{false,true})for(bool down:{false,true})for(bool nonindexed:{false,true}) {
        const auto batch=[&](float radius,float alpha) {
            Batch b;
            for(unsigned ring=0;ring<(pond?30u:2u);++ring) {
                const auto source=Ring(radius+ring*5,alpha);
                for(unsigned half=0;half<2;++half) {
                    std::array<E::Vertex,4> q{};
                    for(unsigned k=0;k<4;++k) {
                        q[k]=source.inputs[(pond?(half^1u):half)*4+k];
                        if(pond) {
                            q[k][0][0]+=ring*18;
                            q[k][1]={alpha*.2f,.1f,.08f,alpha};
                        }
                    }
                    if(down)std::swap(q[1],q[2]);
                    Add(b,q);
                }
            }
            End(b);return nonindexed?NonIndexed(b):b;
        };
        const auto first=batch(30,.6f),second=batch(32,.5f);
        E::History history;history.Context(1,85);
        const auto apply=[&](const Batch& b,unsigned frame) {
            return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& vertices,const auto& order) {
                return history.Apply(pond?E::PondRings:2,frame,42,vertices,order);
            });
        };
        assert(!apply(first,10));const auto half=apply(second,11);assert(half);
        for(unsigned i=0;i<second.order.size();i+=6) {
            for(unsigned k=0;k<4;++k)for(unsigned a=0;a<2;++a)for(unsigned c=0;c<4;++c)
                assert(std::abs((*half)[second.order[i+k]][a][c]-(first.inputs[first.order[i+k]][a][c]+second.inputs[second.order[i+k]][a][c])*.5f)<.0001f);
            assert((*half)[second.order[i+3]]==(*half)[second.order[i+4]]);
            if(i+6<second.order.size())assert((*half)[second.order[i+5]]==(*half)[second.order[i+6]]);
        }
        assert(half->back()==second.inputs.back());
        auto invalid=second;invalid.inputs[0][2]=invalid.inputs[1][2];
        assert(!apply(invalid,12));
        history.Context(2,85);assert(!apply(first,12)); // Scene generation reset.
        assert(!apply(second,14)); // No interpolation across skipped updates.
    }
    // Exercise the exact non-indexed submission path, including aliases on
    // both ends of every connector, rather than just ideal shared indices.
    for(unsigned kind:{1u,2u}) {
        const auto first=NonIndexed(kind==1?a:ra),second=NonIndexed(kind==1?b:rb);
        E::History history;
        const auto apply=[&](const Batch& batch,unsigned frame) {
            return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(batch.inputs,batch.order,[&](const auto& v,const auto& indices) {
                return history.Apply(kind,frame,42,v,indices);
            });
        };
        assert(!apply(first,10));const auto result=apply(second,11);assert(result);
        for(unsigned i=0;i<second.order.size();i+=6) {
            assert((*result)[i+4]==(*result)[i+3]);
            if(i+6<second.order.size())assert((*result)[i+5]==(*result)[i+6]);
        }
        assert(result->back()==second.inputs.back());
        auto broken=second;broken.inputs[4][0][0]+=1;assert(!apply(broken,12));
        assert(!apply(second,14));
    }
    assert(std::abs((*rh)[0][1][3]-(.09803922f+.06666667f)*.5f)<.0001f);
    auto birth=Ring(5,.7f);assert(!h.Apply(2,22,43,birth.inputs,birth.order));
    auto bad=ra;bad.inputs[0][0][1]+=1;assert(!E::Rings(bad.inputs,bad.order));
    W::History wh;W::Key key{1,2,3,3,1,85};W::UV uv{};uv[0][0]=uv[1][1]=uv[2][0]=uv[3][1]=1;
    uv[1][3]=.99f;assert(!wh.Apply(10,key,uv));uv[1][3]=.01f;
    auto uh=wh.Apply(11,key,uv);assert(uh && std::abs((*uh)[1][3])<.00001f);
    uv[1][3]=.4f;assert(!wh.Apply(12,key,uv));uv[0][0]=2;assert(!wh.Apply(13,key,uv));
    assert(!wh.Apply(15,key,uv));assert(!wh.Apply(15,key,uv));
    const bool uv_only=argc>1 && std::string_view(argv[1])=="--uv-only";
    const bool glow_only=argc>1 && std::string_view(argv[1])=="--glow-only";
    for(float capture_phase:{.25f,.5f,.75f}) {
    OOT3D::Interpolation::sample_fraction=capture_phase;
    for(int arg=(uv_only || glow_only)?2:1;arg<argc;++arg) {
        QFile f(argv[arg]);assert(f.open(QIODevice::ReadOnly));
        const auto draws=QJsonDocument::fromJson(f.readAll()).object()["draws"].toArray();
        unsigned forest=0,rings=0,ring_pairs=0,water=0,water_pairs=0,debris=0,debris_pairs=0,shock=0,shock_pairs=0,green=0,green_pairs=0,glows=0,glow_pairs=0;
        unsigned sparkles=0,sparkle_pairs=0,sparkle_fades=0; E::History sparkle_history;
        unsigned trails=0,trail_nodes=0,trail_births=0; E::History trail_history;
        std::array<E::History,7> trail_color_histories;
        struct TrailFrame {std::uint64_t frame{};unsigned scene{};std::vector<E::Quad> quads;};
        std::map<unsigned,TrailFrame> trail_frames;
        unsigned debris_quads=0,debris_changed_quads=0;
        unsigned forest_exact=0,forest_pairs=0,forest_motion=0,forest_fades=0;
        E::History forest_history;
        G::History glow_history;
        struct GlowFrame {std::uint64_t frame;G::Sample sample;};
        std::map<G::Key,GlowFrame> glow_frames;
        unsigned glow_adjacent=0,glow_motion=0,glow_pulse=0,glow_tint=0;
        W::History recorded;D::History debris_history;E::History shock_history,green_history,ring_history;
        for(const auto value:draws) {
            const auto d=value.toObject();const auto r=Regs(d);
            if(uv_only && !W::Candidate(r,d["scene"].toInt()))continue;
            if(glow_only && !G::Candidate(r))continue;
            if(const auto layer=G::Candidate(r)) {
                const auto u=d["uniforms_f"].toArray(),attrs=d["input_attributes"].toArray();
                assert(attrs.size()==3);
                std::array<G::Vertex,4> vertices{};
                for(unsigned i=0;i<3;++i)for(unsigned a=0;a<2;++a)for(unsigned c=0;c<4;++c)
                    vertices[i][a][c]=attrs[i].toArray()[a].toArray()[c].toDouble();
                // First three captured corners plus the unit quad's fourth.
                vertices[3]={{{.5f,-.5f,0,1},{2,0,0,1}}};assert(G::Geometry(vertices));
                auto camera=OOT3D::Interpolation::Math::Identity(),model=camera;G::Color color;
                for(unsigned row=0;row<3;++row)for(unsigned c=0;c<4;++c) {
                    camera[row][c]=u[76+row].toArray()[c].toDouble();
                    model[row][c]=u[20+row].toArray()[c].toDouble();
                }
                for(unsigned c=0;c<4;++c)color[c]=u[8].toArray()[c].toDouble();
                const auto current=G::Decode(camera,model,color);assert(current);++glows;
                const auto frame=std::uint64_t(d["frame"].toInteger());
                const G::Key key{r.texturing.texture0.GetPhysicalAddress(),
                    std::uint32_t(d["vertex_base"].toInteger()),std::uint32_t(d["index_offset"].toInteger()),layer};
                const auto previous=glow_frames.find(key);
                if(previous!=glow_frames.end() && previous->second.frame+1==frame)++glow_adjacent;
                glow_history.Context(d["generation"].toInt(),d["scene"].toInt());
                const auto half=glow_history.Apply(frame,key,camera,model,color);
                if(half) {
                    ++glow_pairs;const auto blended=G::Decode(camera,half->model_view,half->color);assert(blended);
                    assert(previous!=glow_frames.end() && previous->second.frame+1==frame);
                    const auto& old=previous->second.sample;
                    double movement=0,tint_change=0;
                    for(unsigned axis=0;axis<3;++axis) {
                        assert(std::abs(blended->rigid[axis][3]-std::lerp(old.rigid[axis][3],current->rigid[axis][3],double(capture_phase)))<.0001);
                        movement+=std::abs(current->rigid[axis][3]-old.rigid[axis][3]);
                    }
                    assert(std::abs(blended->size-std::lerp(old.size,current->size,double(capture_phase)))<.0001);
                    for(unsigned c=0;c<4;++c) {
                        assert(std::abs(blended->color[c]-std::lerp(old.color[c],color[c],double(capture_phase)))<.0001);
                        tint_change+=std::abs(color[c]-old.color[c]);
                    }
                    glow_motion+=movement>.0001;glow_pulse+=std::abs(current->size-old.size)>.0001;
                    glow_tint+=tint_change>.0001;
                }
                glow_frames.insert_or_assign(key,GlowFrame{frame,*current});
            }
            if(glow_only)continue;
            auto kind=E::Candidate(r,d["shader"].toString().toULongLong(nullptr,16),d["swizzle"].toString().toULongLong(nullptr,16));
            if(kind==7) {
                const auto b=CapturedEffect(d);
                if(!T::Decode(b.inputs,b.order))kind=1; // Same geometry fallback as production.
            }
            forest+=kind==1;rings+=kind==2;
            if(kind==7) {
                ++trails;const auto b=CapturedEffect(d),original=b;
                const auto frame=std::uint64_t(d["frame"].toInteger());
                const unsigned scene=d["scene"].toInt(),texture=r.texturing.texture0.GetPhysicalAddress();
                trail_history.Context(d["generation"].toInt(),scene);
                auto& previous=trail_frames[texture];
                const auto half=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                    const auto q=T::Decode(v,order);assert(q);
                    auto endpoint_history=trail_history;
                    OOT3D::Interpolation::sample_fraction=1;
                    const auto endpoint=endpoint_history.Apply(7,frame,texture,v,order);
                    if(endpoint)assert(std::equal(endpoint->begin(),endpoint->end(),v.begin(),v.end()));
                    OOT3D::Interpolation::sample_fraction=capture_phase;
                    auto out=trail_history.Apply(7,frame,texture,v,order);
                    const auto old=previous;previous={frame,scene,*q};
                    unsigned matches=0;
                    if(old.frame+1==frame && old.scene==scene)for(unsigned i=0;i<q->size();i+=2) {
                        const auto match=T::Match((*q)[i],old.quads);
                        if(!match || T::Match(old.quads[*match],*q)!=i) {
                            ++trail_births;
                            if(out)for(unsigned j=0;j<2;++j)for(unsigned k=0;k<4;++k)
                                assert((*out)[(*q)[i+j].indices[k]]==(*q)[i+j].vertices[k]);
                            continue;
                        }
                        ++matches;++trail_nodes;assert(out);
                        for(unsigned j=0;j<2;++j) {
                            auto blended=(*q)[i+j];
                            for(unsigned k=0;k<4;++k)blended.vertices[k]=(*out)[blended.indices[k]];
                            const auto center=E::F::Center(blended),a=E::F::Center(old.quads[*match+j]),z=E::F::Center((*q)[i+j]);
                            for(unsigned c=0;c<3;++c)
                                assert(std::abs(center[c]-std::lerp(a[c],z[c],capture_phase))<.002f);
                            const float size=std::lerp(D::Size(old.quads[*match+j]),D::Size((*q)[i+j]),capture_phase);
                            assert(std::abs(D::Size(blended)-size)<.002f);
                        }
                    }
                    assert(bool(out)==bool(matches));
                    return out;
                });
                assert(b.inputs==original.inputs && b.order==original.order);
                if(half) {
                    for(unsigned i=0;i<b.inputs.size();++i) {
                        assert((*half)[i][1]==b.inputs[i][1] && (*half)[i][2]==b.inputs[i][2]);
                    }
                    for(unsigned i=0;i<b.order.size();i+=6) {
                        assert((*half)[b.order[i+4]]==(*half)[b.order[i+3]]);
                        if(i+6<b.order.size())assert((*half)[b.order[i+5]]==(*half)[b.order[i+6]]);
                    }
                    assert(half->back()==b.inputs.back());
                }
                // Replay measured full geometry with alternate tints. These
                // are recolored fixtures, not claims of captured destinations.
                constexpr std::array<std::array<float,4>,7> colors{{
                    {55.f/255,0,1,1},{1,0,0,1},{0,1,0,1},{0,0,1,1},
                    {1,1,0,1},{0,1,1,1},{1,1,1,1}}};
                for(unsigned ci=0;ci<colors.size();++ci) {
                    auto recolored=b;for(auto& v:recolored.inputs)v[1]=colors[ci];
                    auto material=r;material.texturing.tev_stage1.const_color=0;
                    for(unsigned c=0;c<4;++c)
                        material.texturing.tev_stage1.const_color|=std::uint32_t(colors[ci][c]*255)<<(8*c);
                    assert(E::Candidate(material,0x6352a3be33cb99c6ULL,0xba817d0ef27276d0ULL)==7);
                    auto& history=trail_color_histories[ci];history.Context(d["generation"].toInt(),scene);
                    const auto tinted=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(recolored.inputs,recolored.order,
                        [&](const auto& v,const auto& order){return history.Apply(7,frame,texture,v,order);});
                    assert(bool(tinted)==bool(half));
                    if(tinted)for(unsigned i=0;i<tinted->size();++i) {
                        assert((*tinted)[i][0]==(*half)[i][0] && (*tinted)[i][2]==(*half)[i][2]);
                        assert((*tinted)[i][1]==recolored.inputs[i][1]);
                    }
                }
            }
            if(kind==6) {
                ++sparkles;const auto b=CapturedEffect(d);
                sparkle_history.Context(d["generation"].toInt(),d["scene"].toInt());
                OOT3D::Interpolation::WarpSparkles::Color color;
                for(unsigned c=0;c<4;++c)color[c]=float((r.texturing.tev_stage1.const_color>>(8*c))&255)/255.f;
                const auto half=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                    assert(OOT3D::Interpolation::WarpSparkles::Decode(v,order));
                    return sparkle_history.Apply(6,d["frame"].toInteger(),r.texturing.texture0.GetPhysicalAddress(),v,order,color);
                });
                if(half) {
                    ++sparkle_pairs;assert((*half)[0][0]!=b.inputs[0][0]);
                    for(unsigned i=0;i<b.inputs.size();++i) {
                        assert((*half)[i][1]==b.inputs[i][1]);assert((*half)[i][2]==b.inputs[i][2]);
                    }
                    assert(half->back()==b.inputs.back());
                    if(sparkle_history.SparkleColor())sparkle_fades+=*sparkle_history.SparkleColor()!=color;
                }
                auto other=r;other.texturing.texture0.width.Assign(32);
                assert(E::Candidate(other,d["shader"].toString().toULongLong(nullptr,16),d["swizzle"].toString().toULongLong(nullptr,16))!=6);
            }

            if(kind==1 && !d["effect_inputs"].toArray().empty()) {
                ++forest_exact;const auto b=CapturedEffect(d);
                forest_history.Context(d["generation"].toInt(),d["scene"].toInt());
                const auto half=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& indices) {
                    assert(E::Forest(v,indices));
                    return forest_history.Apply(1,d["frame"].toInteger(),r.texturing.texture0.GetPhysicalAddress(),v,indices);
                });
                if(half) {
                    ++forest_pairs;bool motion=false,fade=false;
                    for(unsigned i=0;i<b.inputs.size();++i) {
                        motion |= (*half)[i][0]!=b.inputs[i][0];
                        fade |= (*half)[i][1][3]!=b.inputs[i][1][3];
                        assert((*half)[i][2]==b.inputs[i][2]);
                    }
                    forest_motion+=motion;forest_fades+=fade;
                    assert(half->back()==b.inputs.back());
                }
            }
            if(kind==2 && d["cpu"].toBool()) {
                auto b=CapturedEffect(d);
                // Older CPU captures retain only the first half-ring. Build a
                // mirrored-half fixture from that measured half; new captures
                // carry all inputs and use the complete submitted batch.
                if(d["effect_inputs"].toArray().empty()) {
                    std::array<E::Vertex,4> first{},second{};
                    for(unsigned k=0;k<4;++k)first[k]=b.inputs[k];
                    const float x=(first[0][0][0]+first[1][0][0])*.5f,z=first[2][0][2];
                    for(unsigned k=0;k<4;++k) {
                        second[k]=first[k];second[k][0][0]=2*x-first[k][0][0];second[k][0][2]=2*z-first[k][0][2];
                    }
                    b={};Add(b,first);Add(b,second);End(b);b=NonIndexed(b);
                }
                const auto half=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& indices) {
                    assert(E::Rings(v,indices));
                    return ring_history.Apply(2,d["frame"].toInteger(),r.texturing.texture0.GetPhysicalAddress(),v,indices);
                });
                if(half)++ring_pairs;
            }
            if(kind==5) {
                const auto b=CapturedEffect(d);
                const auto result=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& inputs,const auto& order) {
                    const auto decoded=OOT3D::Interpolation::GreenParticles::Decode(inputs,order);
                    if(!d["effect_inputs"].toArray().empty())assert(decoded);
                    if(!decoded)return std::optional<std::vector<E::Vertex>>{};
                    ++green;
                    return green_history.Apply(5,d["frame"].toInteger(),r.texturing.texture0.GetPhysicalAddress(),inputs,order);
                });
                if(result){++green_pairs;if(d["cpu"].toBool())assert((*result)[b.order[3]]==(*result)[b.order[4]]);}
                const bool original_decodes=bool(OOT3D::Interpolation::GreenParticles::Decode(b.inputs,b.order));
                for(unsigned channel:{0u,2u}) {
                    auto recolored=b;
                    for(auto& v:recolored.inputs)std::swap(v[1][1],v[1][channel]);
                    assert(bool(OOT3D::Interpolation::GreenParticles::Decode(recolored.inputs,recolored.order))==original_decodes);
                }
                auto invalid=b;for(auto& v:invalid.inputs)v[1]={1.1f,0,0,1};
                assert(!OOT3D::Interpolation::GreenParticles::Decode(invalid.inputs,invalid.order));
            }
            if(kind==4) {
                const auto b=CapturedEffect(d);
                const auto frame=d["frame"].toInteger();
                const auto half=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& inputs,const auto& order) {
                    const auto wave=E::Shockwaves(inputs,order);
                    if(!d["effect_inputs"].toArray().empty())assert(wave);
                    if(!wave)return std::optional<std::vector<E::Vertex>>{};
                    ++shock;
                    return shock_history.Apply(4,frame,r.texturing.texture0.GetPhysicalAddress(),inputs,order);
                });
                if(half) {
                    ++shock_pairs;
                    if(b.order.size()==12) {
                        for(unsigned base:{0u,6u}) {
                            assert((*half)[b.order[base]][1][3]>b.inputs[b.order[base]][1][3]);
                            assert((*half)[b.order[base]][0][0]>b.inputs[b.order[base]][0][0]);
                        }
                        assert((*half)[b.order[5]]==(*half)[b.order[6]]);
                    }
                    if(d["cpu"].toBool())assert((*half)[b.order[3]]==(*half)[b.order[4]]);
                    assert((*half)[0][1][3]>b.inputs[0][1][3]);
                    assert((*half)[0][0][0]>b.inputs[0][0][0]);
                }
                // Resetting to a newborn wave at the same location must not
                // borrow the old expansion/fade, even on adjacent frames.
                if(d["cpu"].toBool())continue;
                E::History newborn;newborn.Apply(4,10,42,b.inputs,b.order);
                auto small=b;const auto center=E::F::Center(E::Shockwaves(b.inputs,b.order)->front());
                for(unsigned i=0;i<4;++i) {
                    small.inputs[i][0][0]=center[0]+(i%2?1.f:-1.f);
                    small.inputs[i][0][2]=center[2]+(i/2?1.f:-1.f);
                    small.inputs[i][1][3]=1;
                }
                assert(!newborn.Apply(4,11,42,small.inputs,small.order));
            }
            if(kind==3 && (!d["input_attributes"].toArray().empty() || !d["effect_inputs"].toArray().empty())) {
                ++debris;const auto b=CapturedDebris(d);
                if(debris==1 && r.pipeline.num_vertices>384) {
                    // Capture retained only three corners. Exercise a full
                    // 80-flake batch separately; do not call it recorded data.
                    DebrisBatch full;
                    for(unsigned q=0;q<80;++q)for(unsigned k=0;k<6;++k) {
                        auto v=b.inputs[k<4?k:k==4?3:0];
                        v[0][0]+=100.f*(q+(k==5 && q<79?1:0));
                        full.order.push_back(full.inputs.size());full.inputs.push_back(v);
                    }
                    D::History large;
                    auto apply=[&](unsigned frame,const DebrisBatch& batch) {
                        return OOT3D::Interpolation::ReplayEffectStrip<D::Vertex>(batch.inputs,batch.order,
                            [&](const auto& vertices,const auto& order) {
                                const auto decoded=D::Decode(vertices,order);assert(decoded && decoded->size()==80);
                                return large.Apply(frame,42,vertices,order);
                            });
                    };
                    assert(!apply(1,full));auto fallen=full;
                    for(auto& v:fallen.inputs)v[0][1]-=24;
                    const auto half=apply(2,fallen);assert(half);
                    for(unsigned q=0;q<80;++q)assert(std::abs((*half)[q*6][0][1]-full.inputs[q*6][0][1]+24*capture_phase)<.01f);
                }

                const auto frame=d["frame"].toInteger();
                debris_history.Context(d["generation"].toInt(),d["scene"].toInt());
                const auto half=OOT3D::Interpolation::ReplayEffectStrip<D::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                    const auto decoded=D::Decode(v,order);assert(decoded);
                    debris_quads+=decoded->size();
                    const auto result=debris_history.Apply(frame,r.texturing.texture0.GetPhysicalAddress(),v,order);
                    if(result) {
                        const auto qs=D::Decode(*result,order);assert(qs && qs->size()==decoded->size());
                        for(unsigned q=0;q<qs->size();++q) {
                            assert(std::abs(D::Size((*qs)[q])-D::Size((*decoded)[q]))<.02f);
                            debris_changed_quads+=(*qs)[q].vertices[0][0]!=(*decoded)[q].vertices[0][0];
                        }
                    }
                    return result;
                });
                if(half) {
                    ++debris_pairs;
                    for(unsigned i=0;i<b.inputs.size();++i) {
                        assert((*half)[i][3]==b.inputs[i][3]); // Native atlas cells.
                        assert((*half)[i][2]==b.inputs[i][2]); // Native color.
                    }
                    for(unsigned i=0;i<b.order.size();i+=6) {
                        assert((*half)[b.order[i+3]]==(*half)[b.order[i+4]]);
                        if(i+6<b.order.size())assert((*half)[b.order[i+5]]==(*half)[b.order[i+6]]);
                    }
                    assert((*half)[b.order.back()]==b.inputs[b.order.back()]);
                }
            }
            const unsigned scene=d["scene"].toInt();
            if(!W::Candidate(r,scene))continue;
            ++water;W::UV sample{};const auto floats=d["uniforms_f"].toArray();
            for(unsigned i=0;i<W::Rows.size();++i)for(unsigned j=0;j<4;++j)sample[i][j]=floats[W::Rows[i]].toArray()[j].toDouble();
            const W::Key captured_key{std::uint32_t(d["vertex_base"].toInteger()),
                std::uint32_t(d["index_offset"].toInteger()),r.texturing.texture0.GetPhysicalAddress(),
                r.texturing.texture1.GetPhysicalAddress(),r.texturing.main_config.texture2_enable?
                    r.texturing.texture2.GetPhysicalAddress():0,
                unsigned(d["generation"].toInt()),unsigned(d["scene"].toInt()),unsigned(r.pipeline.num_vertices)};
            const auto result=recorded.Apply(d["frame"].toInteger(),captured_key,sample);
            if(result) {
                ++water_pairs;const bool fountain=r.pipeline.num_vertices==564;
                if(W::JabuWallCandidate(r,scene)) {
                    assert(!W::JabuWallCandidate(r,1));
                    auto unrelated=r;unrelated.framebuffer.output_merger.depth_write_enable.Assign(1);
                    assert(!W::JabuWallCandidate(unrelated,scene));
                    // Exact CMAB ROOM7 speeds, confirmed by adjacent capture
                    // samples: slot 0 U -1/50; slot 1 U -1/180, V +1/225.
                    constexpr std::array<float,5> speed{-.02f,0,-1.f/180,1.f/225,0};
                    for(unsigned row=0;row<W::Rows.size();++row)for(unsigned c=0;c<4;++c) {
                        const float expected=sample[row][c]-(c==3?speed[row]*(1-capture_phase):0);
                        assert(std::abs((*result)[row][c]-expected)<.00001f);
                    }
                    // Other colors use the same family but different native
                    // texture alpha/blending and TEV (bdan and bdan_dd assets).
                    for(bool second:{false,true}) {
                        auto colored=r;colored.pipeline.num_vertices=1236;
                        colored.texturing.texture0_format.Assign(Pica::TexturingRegs::TextureFormat(13));
                        colored.framebuffer.output_merger.alpha_blending.factor_source_rgb.Assign(Pica::FramebufferRegs::BlendFactor(6));
                        auto& s=colored.texturing.tev_stage1;
                        s.sources_raw=second?0x0eef0f43:0x0eef0f40;s.modifiers_raw=second?2:0;
                        s.ops_raw=second?0x10009:0x10008;s.const_color=0xff6040ff;
                        assert(W::JabuWallCandidate(colored,scene) && W::Candidate(colored,scene));
                        s.ops_raw=0;assert(!W::JabuWallCandidate(colored,scene));
                    }
                    continue;
                }
                if(W::CavernOrPondFlow(r,scene)) {
                    assert(!W::CavernOrPondFlow(r,0x57));
                    const unsigned n=r.pipeline.num_vertices;
                    const float speed0=n==531?.01f:n==123?.02f:n==177?7.f/300:n==204?1.f/120:0;
                    const float speed1=n==531?1.f/150:n==123?2.f/75:n==177?1.f/60:n==204?0:-1.f/600;
                    for(unsigned row=0;row<W::Rows.size();++row)for(unsigned c=0;c<4;++c) {
                        const float speed=row==1?speed0:row==3?speed1:0;
                        const float expected=sample[row][c]-(c==3?speed*(1-capture_phase):0);
                        assert(std::abs((*result)[row][c]-expected)<.00001f);
                    }
                    continue;
                }
                if(W::GerudoWaterfallCandidate(r,scene)) {
                    assert(!W::GerudoWaterfallCandidate(r,0x55));
                    const unsigned count=r.pipeline.num_vertices;
                    const bool second=r.texturing.main_config.texture1_enable;
                    const std::array<float,5> per_tick{0,count!=312?.025f:second?.02f:1.f/120,
                        second && count!=312?-1.f/300:0,second?(count==192?.02f:1.f/60):0,0};
                    for(unsigned row=0;row<W::Rows.size();++row)for(unsigned c=0;c<4;++c)
                        assert(std::abs((*result)[row][c]-sample[row][c]+(c==3?per_tick[row]*(1-capture_phase):0))<.00001f);
                    continue;
                }
                if(W::ZoraWaterfallCandidate(r)) {
                    auto unrelated=r;unrelated.texturing.texture0_format.Assign(Pica::TexturingRegs::TextureFormat(12));
                    assert(!W::Candidate(unrelated));
                    unrelated=r;unrelated.framebuffer.output_merger.depth_write_enable.Assign(1);
                    assert(!W::Candidate(unrelated));
                    const unsigned count=r.pipeline.num_vertices;
                    const float speed0=count==288?2.f/75:count==192?.03f:-.01f;
                    const float speed1=count==288?.04f:count==192?1.f/30:0.f;
                    assert(std::abs((*result)[1][3]-(sample[1][3]-speed0*(1-capture_phase)))<.00001f);
                    assert(std::abs((*result)[3][3]-(sample[3][3]-speed1*(1-capture_phase)))<.00001f);
                    for(unsigned row=0;row<W::Rows.size();++row)for(unsigned c=0;c<4;++c)
                        if(c!=3 || (row!=1 && row!=3))assert((*result)[row][c]==sample[row][c]);
                    continue;
                }
                if(W::BlueFlameCandidate(r)) {
                    auto unrelated=r;unrelated.texturing.texture1.height.Assign(32);
                    assert(!W::Candidate(unrelated));
                    unrelated=r;unrelated.framebuffer.output_merger.depth_write_enable.Assign(1);
                    assert(!W::Candidate(unrelated));
                    // Both animated axes advance -1/15 per native tick.
                    assert(std::abs((*result)[0][3]-(sample[0][3]+(1-capture_phase)/15))<.00001f);
                    assert(std::abs((*result)[3][3]-(sample[3][3]+(1-capture_phase)/15))<.00001f);
                    for(unsigned row=0;row<W::Rows.size();++row)for(unsigned c=0;c<4;++c)
                        if(c!=3 || (row!=0 && row!=3))assert((*result)[row][c]==sample[row][c]);
                    continue;
                }
                assert(std::abs((*result)[1][3]-(sample[1][3]-(fountain?.04f:.015f)*(1-capture_phase)))<.00001f);
                if(fountain)assert(std::abs((*result)[4][3]-(sample[4][3]-(1-capture_phase)/60))<.00001f);
            }
        }
        std::cout<<argv[arg]<<" phase "<<capture_phase<<": forest batches "<<forest<<", rings "<<rings<<", paired ring fixtures "<<ring_pairs<<", waterfall "<<water<<", adjacent UV pairs "<<water_pairs<<", debris "<<debris<<", paired debris batches "<<debris_pairs<<", shockwaves "<<shock<<", paired shockwaves "<<shock_pairs<<", green batches "<<green<<", paired first-quad fixtures "<<green_pairs<<std::endl;
        if(trails){std::cout<<"Warp trail full batches "<<trails<<", matched nodes "<<trail_nodes<<", births "<<trail_births<<std::endl;assert(trails==72 && trail_nodes==596 && trail_births==42);}
        if(sparkles){std::cout<<"Warp sparkle first-quad fixtures "<<sparkles<<", pairs "<<sparkle_pairs<<", fades "<<sparkle_fades<<std::endl;assert(sparkle_pairs>=20 && sparkle_fades>0);}
        if(glows)std::cout<<"Fairy glow layers "<<glows<<", adjacent "<<glow_adjacent<<", paired "<<glow_pairs
            <<", moving "<<glow_motion<<", pulsing "<<glow_pulse<<", changing color/opacity "<<glow_tint<<std::endl;
        if(debris)std::cout<<"Debris decoded quads "<<debris_quads<<", interpolated quads "<<debris_changed_quads<<std::endl;
        if(forest_exact)std::cout<<"  Exact forest batches "<<forest_exact<<", midpoint pairs "<<forest_pairs
            <<", moving batches "<<forest_motion<<", fading batches "<<forest_fades<<std::endl;
        assert(forest || rings || water || debris || shock || green || glows || sparkles || trails);
        if(glows)assert(glow_pairs>0);
        // The transport fixture leaves Kokiri mid-capture, retaining only
        // 45 waterfall draws (30 adjacent pairs). Dedicated captures are longer.
        if(water)assert(trails ? (water==45 && water_pairs==30) : water_pairs>40);
        if(debris)assert(debris_pairs>20);
        // The blood capture also contains four recoverable wave samples in
        // two separate adjacent pairs; full wave captures contain five or more.
        if(shock>=5)assert(shock_pairs>=3);
        else if(shock>1)assert(shock_pairs>0);
        if(green)assert(green_pairs>=4);
        if(ring_pairs)assert(ring_pairs>10);

    }
    }
    OOT3D::Interpolation::sample_fraction=.5f;
    std::cout<<"Effect checks: 32 sprites, movement/fade/rotation without shrinkage, ring expansion, births, scene/gap reset, malformed geometry, UV wrapping passed\n";
}
