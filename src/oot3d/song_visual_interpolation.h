// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_math.h"
#include "oot3d/interpolation_pose.h"
#include "oot3d/interpolation_rate.h"
#include <cstdint>
#include <map>

namespace OOT3D::Interpolation::SongVisual {
// Captured Epona/Saria and Song of Time backdrops. The caller additionally
// verifies native efc_ocarina model ownership and the world shader/projection.
enum class Layer : unsigned { None, Backdrop, Scroll, Streaks, Time, Storm, Clouds };
inline constexpr std::array<unsigned,4> Rows{10,11,14,15};
inline unsigned UVRows(Layer layer){return layer==Layer::Backdrop?0:layer==Layer::Clouds?4:2;}
template<class Regs> Layer Candidate(const Regs& r) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    if(unsigned(r.pipeline.triangle_topology.Value())!=3 ||
       r.vs.GetRegisterForAttribute(0)!=0 || r.vs.GetRegisterForAttribute(1)!=1 ||
       r.vs.GetRegisterForAttribute(2)!=2 ||
       !m.depth_test_enable || m.depth_write_enable || m.stencil_test.enable ||
       !m.alphablend_enable || unsigned(m.alpha_blending.factor_source_rgb.Value())!=6 ||
       unsigned(m.alpha_blending.factor_dest_rgb.Value())!=1 ||
       unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0 ||
       t.main_config.texture1_enable || t.main_config.texture2_enable ||
       t.tev_stage0.modifiers_raw || t.tev_stage0.scales_raw)return Layer::None;
    const auto stages=t.GetTevStages();
    for(unsigned i=1;i<6;++i)if(stages[i].sources_raw!=0x0fff0fff ||
        stages[i].modifiers_raw || stages[i].ops_raw || stages[i].scales_raw)return Layer::None;
    const auto n=r.pipeline.num_vertices;
    if(!t.main_config.texture0_enable)
        return n==90 && t.tev_stage0.sources_raw==0x0ee00e00 && t.tev_stage0.ops_raw==0x10000
            ?Layer::Backdrop:Layer::None;
    if(r.vs.GetRegisterForAttribute(3)!=3 || unsigned(t.texture0_format.Value())!=7 ||
       unsigned(t.texture0.wrap_s.Value())!=2 || unsigned(t.texture0.wrap_t.Value())!=2 ||
       t.tev_stage0.ops_raw!=0x10001)return Layer::None;
    if(n==192 && t.texture0.width==32 && t.texture0.height==32 &&
       t.tev_stage0.sources_raw==0x0e300e30)return Layer::Storm;
    if(t.tev_stage0.sources_raw!=0x0ee00e30)return Layer::None;
    if(n==90 && t.texture0.width==64 && t.texture0.height==64)return Layer::Scroll;
    if(n==288 && t.texture0.width==32 && t.texture0.height==128)return Layer::Streaks;
    if(n==60 && t.texture0.width==32 && t.texture0.height==32)return Layer::Time;
    return Layer::None;
}
using Color=std::array<float,4>;
using UV=std::array<std::array<float,4>,4>;
using Key=std::array<std::uint32_t,8>; // geometry, index, active texture, epoch, scene, owner, cut, layer
struct Sample {
    Math::Matrix model_view=Math::Identity();
    UV uv{};
    Color color{};
    float alpha{}; // TEV stage 0, separate from vertex uniform colour alpha
    bool model_blended=false;
};
class History {
    struct Entry {std::uint64_t frame{};Sample sample;Math::Matrix world;bool unique=true;};
    std::map<Key,Entry> entries;
public:
    void Reset(){entries.clear();}
    std::optional<Sample> Apply(std::uint64_t frame,const Key& key,Layer layer,
                               const Math::Matrix& camera,const Sample& sample) {
        const auto inverse=Math::InverseCamera(camera);
        bool valid=inverse && layer!=Layer::None && unsigned(layer)==key.back() &&
            Math::Near(sample.model_view,sample.model_view,0) &&
            std::isfinite(sample.alpha) && sample.alpha>=0 && sample.alpha<=1;
        for(float x:sample.color)valid &= std::isfinite(x) && x>=0 && x<=1.001f;
        // The captured backdrops are rigid camera-facing models at scale .1.
        // Reject another geometry family instead of guessing its animation.
        if(layer!=Layer::Storm && layer!=Layer::Clouds)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
            valid &= std::abs(sample.model_view[r][c]-(r==c?.1:0))<.0001;
        for(unsigned r=0;r<UVRows(layer);++r)for(unsigned c=0;c<4;++c)
            valid &= std::isfinite(sample.uv[r][c]) &&
                (c==3 || std::abs(sample.uv[r][c]-float(r%2==c))<.0001f);
        if(!valid){entries.erase(key);return {};}
        auto world=Math::Multiply(camera,sample.model_view);
        for(auto it=entries.begin();it!=entries.end();)
            if(it->second.frame>frame || frame-it->second.frame>1)it=entries.erase(it);else ++it;
        const auto found=entries.find(key);
        if(found==entries.end()) {
            if(entries.size()<16)entries.emplace(key,Entry{frame,sample,world});
            return {};
        }
        auto& e=found->second;
        if(e.frame==frame){e.unique=false;return {};}
        valid=e.unique && e.frame+1==frame;
        double distance=0;
        for(unsigned r=0;r<3;++r)distance+=std::pow(world[r][3]-e.world[r][3],2);
        valid &= distance<64*64 && (layer==Layer::Storm ||
            std::abs(sample.model_view[2][3]-e.sample.model_view[2][3])<16);
        Sample out=sample;
        for(unsigned c=0;c<4;++c) {
            valid &= std::abs(sample.color[c]-e.sample.color[c])<.2f;
            out.color[c]=SampleMix(e.sample.color[c],sample.color[c]);
        }
        valid &= std::abs(sample.alpha-e.sample.alpha)<.2f;
        out.alpha=SampleMix(e.sample.alpha,sample.alpha);
        for(unsigned r=0;r<UVRows(layer);++r) {
            // These native animations keep unwrapped offsets. Do not mistake
            // restarting a song for a repeating texture crossing an integer.
            float delta=sample.uv[r][3]-e.sample.uv[r][3];
            if(layer==Layer::Storm)delta-=std::round(delta); // Native repeating CMAB.
            valid &= std::abs(delta)<.125f;
            out.uv[r][3]=sample.uv[r][3]-delta*(1-sample_fraction);
        }
        if(layer!=Layer::Clouds)if(const auto blended=Pose::Half(e.world,world,sample_fraction)) {
            out.model_view=Math::Multiply(*inverse,*blended);out.model_blended=true;
        }
        e={frame,sample,world};
        return valid?std::optional{out}:std::nullopt;
    }
};
}
