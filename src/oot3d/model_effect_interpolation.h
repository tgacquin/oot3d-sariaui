// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_pose.h"
#include "oot3d/interpolation_rate.h"
#include <map>
namespace OOT3D::Interpolation::ModelEffects {
using UV=std::array<std::array<float,4>,4>;
using Colors=std::array<std::array<float,4>,6>;
inline constexpr std::array<unsigned,4> Rows{10,11,14,15};
enum Kind : unsigned {None,FireMouth,FireBreath,ElectricArc,ElectricSpark,Biri,Syabom};
template<class Regs> Kind Candidate(const Regs& r) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    if(unsigned(r.pipeline.triangle_topology.Value())!=3 || r.vs.GetRegisterForAttribute(0)!=0 ||
       !t.main_config.texture0_enable || t.main_config.texture2_enable ||
       !m.depth_test_enable || m.depth_write_enable || m.stencil_test.enable || !m.alphablend_enable ||
       unsigned(m.alpha_blending.factor_source_rgb.Value())!=6 ||
       unsigned(m.alpha_blending.factor_dest_rgb.Value())!=1 ||
       unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0)return None;
    const auto n=unsigned(r.pipeline.num_vertices),w=unsigned(t.texture0.width),h=unsigned(t.texture0.height);
    const auto fmt=unsigned(t.texture0_format.Value());
    if(n==6 && w==64 && h==64 && !t.main_config.texture1_enable && r.vs.GetRegisterForAttribute(1)==3) {
        if(fmt==13 && t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.ops_raw==0x10001 &&
           t.tev_stage0.scales_raw==1 && t.tev_stage1.sources_raw==0x0e1f0eef &&
           t.tev_stage1.modifiers_raw==32 && t.tev_stage1.ops_raw==8 &&
           unsigned(t.texture0.wrap_s.Value())==2 && unsigned(t.texture0.wrap_t.Value())==2)return Biri;
        // Intact Syabom bubble: unscaled color combiner, one lit billboard.
        // Its wobble is a sheared model matrix, not a skeletal animation.
        if(fmt==13 && t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.modifiers_raw==0 &&
           t.tev_stage0.ops_raw==0x10001 && t.tev_stage0.scales_raw==0 &&
           t.tev_stage1.sources_raw==0x0e1f0eef && t.tev_stage1.modifiers_raw==32 &&
           t.tev_stage1.ops_raw==8 && t.tev_stage1.scales_raw==0 &&
           t.tev_stage2.sources_raw==0x0fff0fff && t.tev_stage2.modifiers_raw==0 &&
           t.tev_stage2.ops_raw==0 && t.tev_stage2.scales_raw==0 &&
           unsigned(t.texture0.wrap_s.Value())==2 && unsigned(t.texture0.wrap_t.Value())==2)return Syabom;
        if(fmt==7 && t.tev_stage0.sources_raw==0x0e3e0ee3 && t.tev_stage0.ops_raw==1 &&
           t.tev_stage1.sources_raw==0x0eef0e1f && t.tev_stage1.ops_raw==0x10000)return ElectricSpark;
    }
    if(fmt!=7 || r.vs.GetRegisterForAttribute(1)!=1 || r.vs.GetRegisterForAttribute(2)!=3)return None;
    if(n==72 && w==64 && h==128 && !t.main_config.texture1_enable &&
       t.tev_stage0.sources_raw==0x0ee00e33 && t.tev_stage0.ops_raw==0x10001 &&
       t.tev_stage1.sources_raw==0x0e1f0eef && t.tev_stage1.ops_raw==1)return ElectricArc;
    if(!t.main_config.texture1_enable || unsigned(t.texture1_format.Value())!=7 ||
       t.texture1.width!=32 || t.texture1.height!=64)return None;
    if(n==270 && w==64 && h==64 && t.tev_stage0.sources_raw==0x0e300343 &&
       t.tev_stage0.ops_raw==0x10009 && t.tev_stage1.sources_raw==0x0e1f0eef && t.tev_stage1.ops_raw==1)return FireMouth;
    if((n==30 || n==66) && w==32 && h==64 && t.tev_stage0.sources_raw==0x0ee00343 &&
       t.tev_stage0.ops_raw==0x10009 && t.tev_stage1.sources_raw==0x0e1f0eff && t.tev_stage1.ops_raw==1)return FireBreath;
    return None;
}
struct Sample {Matrix model=Math::Identity();UV uv{};Colors colors{};std::array<float,4> color{};unsigned texture{},previous_texture{};};
// CmbVShader's lit Biri path can output alpha > 1 (3 in the capture).
// Weight the converted/clamped vertex colors, not f8.w before the shader;
// otherwise both half-weight passes can still saturate to full opacity.
template<class Vertex> std::vector<Vertex> TextureCrossfade(std::vector<Vertex>& current,float fraction) {
    auto previous=current;
    fraction=std::clamp(fraction,0.f,1.f);
    for(unsigned i=0;i<current.size();++i) {
        previous[i].color[3]*=1-fraction;
        current[i].color[3]*=fraction;
    }
    return previous;
}
using Key=std::array<unsigned,8>; // geometry/index/textures/epoch/scene/cut/kind
class History {
    struct Entry {std::uint64_t frame{};std::vector<Sample> previous,current;std::vector<bool> used;};
    std::map<Key,Entry> entries;
public:
    void Reset(){entries.clear();}
    std::optional<Sample> Apply(std::uint64_t frame,Key key,const Matrix& camera,Sample now) {
        const auto view=Math::InverseCamera(camera);if(!view || !Math::Near(now.model,now.model,0))return {};
        const Kind kind=Kind(key.back());
        if(kind==Syabom && !Pose::InverseBone(now.model))return {};
        now.model=Math::Multiply(camera,now.model);

        for(const auto& uv:now.uv)for(float f:uv)if(!std::isfinite(f))return {};
        for(auto i=entries.begin();i!=entries.end();)if(i->second.frame>frame || frame-i->second.frame>1)i=entries.erase(i);else ++i;
        if(!entries.contains(key) && entries.size()>=64)return {};
        auto& e=entries[key];
        if(e.frame!=frame) {
            e.previous=e.frame+1==frame?std::move(e.current):std::vector<Sample>{};
            e.current.clear();e.used.assign(e.previous.size(),false);e.frame=frame;
        }
        double best=INFINITY,second=INFINITY;unsigned index=e.previous.size();
        for(unsigned i=0;i<e.previous.size();++i)if(!e.used[i]) {
            double score=0;for(unsigned c=0;c<3;++c)score+=std::pow(now.model[c][3]-e.previous[i].model[c][3],2);
            if(score>64*64)continue;
            if(score<best){second=best;best=score;index=i;}else second=std::min(second,score);
        }
        std::optional<Sample> result;
        if(index<e.previous.size() && second>best*1.5+.01) {
            const auto& old=e.previous[index];auto out=now;
            const auto inverse=Pose::InverseBone(old.model);
            const auto relative=inverse?Math::Multiply(*inverse,now.model):Math::Identity();
            double change=0;for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)change+=std::pow(relative[r][c]-(r==c),2);
            bool valid=true;
            if(const auto rigid=Pose::Half(old.model,now.model,sample_fraction))out.model=*rigid;
            else {
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)out.model[r][c]=std::lerp(old.model[r][c],now.model[r][c],double(sample_fraction));
                valid &= inverse && change<1.5 && bool(Pose::InverseBone(out.model));
            }
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
                float delta=now.uv[r][c]-old.uv[r][c];
                // Only the repeating scrolling layer wraps. Rotation/pivot
                // coefficients and clamped texture coordinates never wrap.
                if(c==3 && r>=2 && (kind==FireMouth || kind==FireBreath))delta-=std::round(delta);
                valid &= std::abs(delta)<.5f;
                out.uv[r][c]=now.uv[r][c]-delta*(1-sample_fraction);
            }
            for(unsigned c=0;c<4;++c)out.color[c]=SampleMix(old.color[c],now.color[c]);
            for(unsigned r=0;r<6;++r)for(unsigned c=0;c<4;++c)out.colors[r][c]=SampleMix(old.colors[r][c],now.colors[r][c]);
            if(valid) {
                out.model=Math::Multiply(*view,out.model);
                if(kind==Biri && old.texture!=now.texture)out.previous_texture=old.texture;
                e.used[index]=true;result=out;
            }
        }
        if(e.current.size()<128)e.current.push_back(now);
        return result;
    }
};
}
