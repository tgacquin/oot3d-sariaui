// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_rate.h"
#include <atomic>
#include <cstdint>
#include <map>
#include "oot3d/interpolation_math.h"

namespace OOT3D::Interpolation::FairyGlow {
using Math::Matrix;
using Color=std::array<double,4>;
using Vertex=std::array<std::array<float,4>,2>;
using Key=std::array<std::uint32_t,4>; // texture, vertex base, index offset, layer
inline std::atomic<unsigned> observed{},paired{};
template<class Regs> unsigned Candidate(const Regs& r) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    if(r.pipeline.num_vertices!=4 || unsigned(r.pipeline.triangle_topology.Value())!=1 ||
       r.vs.GetRegisterForAttribute(0)!=0 || r.vs.GetRegisterForAttribute(1)!=3 ||
       !t.main_config.texture0_enable || t.main_config.texture1_enable || t.main_config.texture2_enable ||
       unsigned(t.texture0.wrap_s.Value())!=3 || unsigned(t.texture0.wrap_t.Value())!=3 ||
       !m.alphablend_enable || m.depth_test_enable || m.depth_write_enable || m.stencil_test.enable ||
       unsigned(m.alpha_blending.factor_source_rgb.Value())!=6 ||
       unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0 ||
       t.tev_stage0.sources_raw!=0x0e300e30 || t.tev_stage0.modifiers_raw || t.tev_stage0.scales_raw ||
       t.tev_stage1.sources_raw!=0x0fff0fff || t.tev_stage1.ops_raw)return 0;
    if(t.texture0.width==64 && t.texture0.height==64 && unsigned(t.texture0_format.Value())==7 &&
       unsigned(m.alpha_blending.factor_dest_rgb.Value())==1 && t.tev_stage0.ops_raw==1)return 1;
    if(t.texture0.width==32 && t.texture0.height==32 && unsigned(t.texture0_format.Value())==5 &&
       unsigned(m.alpha_blending.factor_dest_rgb.Value())==7 && t.tev_stage0.ops_raw==0x10001)return 2;
    return 0;
}
inline bool Geometry(const std::array<Vertex,4>& vertices) {
    unsigned used=0;
    for(const auto& v:vertices) {
        unsigned corner=4;
        for(unsigned k=0;k<4;++k) {
            const Vertex expected{{{float(k%2)-.5f,.5f-float(k/2),0,1},
                                   {float(k%2)*2,float(1-k/2)*2,0,1}}};
            bool same=true;
            for(unsigned a=0;a<2;++a)for(unsigned c=0;c<4;++c)
                if(!std::isfinite(v[a][c]) || std::abs(v[a][c]-expected[a][c])>.0001f)same=false;
            if(same)corner=k;
        }
        if(corner==4 || (used&(1u<<corner)))return false;
        used|=1u<<corner;
    }
    return true;
}
struct Sample {Matrix rigid;double size;Color color;};
struct Override {Matrix model_view;Color color;};
inline std::optional<Sample> Decode(const Matrix& camera,const Matrix& model_view,const Color& color) {
    if(!Math::Rigid(camera) || !Math::Near(model_view,model_view,0))return {};
    // Native fairy color transitions slightly overshoot RGB (captured -0.027
    // and 1.027). Preserve those values until the shader's normal clamping.
    for(unsigned c=0;c<4;++c)
        if(!std::isfinite(color[c]) || color[c]<(c==3?0:-.125) || color[c]>(c==3?1.001:1.125))return {};
    auto world=Math::Multiply(camera,model_view);
    double size=0;for(unsigned r=0;r<3;++r)size+=world[r][0]*world[r][0];size=std::sqrt(size);
    if(size<.01 || size>128)return {};
    // The native pulse can cross through zero to a negative uniform scale.
    // Preserve that reflection as scale, never feed it to rotation blending.
    const double determinant=world[0][0]*(world[1][1]*world[2][2]-world[1][2]*world[2][1])-
        world[0][1]*(world[1][0]*world[2][2]-world[1][2]*world[2][0])+
        world[0][2]*(world[1][0]*world[2][1]-world[1][1]*world[2][0]);
    if(determinant<0)size=-size;
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)world[r][c]/=size;
    if(!Math::Rigid(world))return {};
    return Sample{world,size,color};
}
class History {
    struct Entry {Sample sample;std::uint64_t frame;bool unique;};
    std::map<Key,Entry> entries;
    unsigned epoch{},scene{};
public:
    void Reset(){entries.clear();}
    void Context(unsigned e,unsigned s){if(epoch!=e || scene!=s)Reset();epoch=e;scene=s;}
    std::optional<Override> Apply(std::uint64_t frame,Key key,const Matrix& camera,
                                 const Matrix& model_view,const Color& color) {
        ++observed;
        const auto current=Decode(camera,model_view,color);
        const auto view=Math::InverseCamera(camera);
        if(!current || !view){entries.erase(key);return {};}
        for(auto it=entries.begin();it!=entries.end();)
            if(it->second.frame>frame || frame-it->second.frame>1)it=entries.erase(it);else ++it;
        const auto it=entries.find(key);
        if(it!=entries.end() && it->second.frame==frame){it->second.unique=false;return {};}
        std::optional<Override> out;
        if(it!=entries.end() && it->second.unique && it->second.frame+1==frame) {
            const auto& old=it->second.sample;
            double distance=0,trace=0;
            for(unsigned r=0;r<3;++r) {
                distance+=std::pow(current->rigid[r][3]-old.rigid[r][3],2);
                for(unsigned c=0;c<3;++c)trace+=current->rigid[r][c]*old.rigid[r][c];
            }
            // Captured flight is at most 24 units/update. Reject teleports,
            // large orientation cuts, and sudden replacement of a glow layer.
            if(distance<=32*32 && trace>=2 && current->size*old.size>0 &&
               std::max(std::abs(current->size),std::abs(old.size))<=2*std::min(std::abs(current->size),std::abs(old.size))) {
                auto half=Math::Blend(old.rigid,current->rigid,sample_fraction);
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)half[r][c]*=SampleMix(old.size,current->size);
                Color tint;for(unsigned c=0;c<4;++c)tint[c]=SampleMix(old.color[c],color[c]);
                out=Override{Math::Multiply(*view,half),tint};++paired;
            }
        }
        if(it!=entries.end())it->second={*current,frame,true};
        else if(entries.size()<16)entries.emplace(key,Entry{*current,frame,true});
        return out;
    }
};
}
