// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_rate.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
namespace OOT3D::Interpolation::Waterfall {
inline constexpr std::array<unsigned,5> Rows{10,11,14,15,18};
using UV = std::array<std::array<float,4>,Rows.size()>;
using Key = std::array<std::uint32_t,9>; // geometry base/index, three textures, epoch, scene, vertex count, pass
template<class Regs> bool BlueFlameCandidate(const Regs& r) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    // Ice Cavern blue fire: three 180-index sections, two repeating IA8
    // layers. Match the material, never the scene-dependent texture addresses.
    return r.pipeline.num_vertices==180 && unsigned(r.pipeline.triangle_topology.Value())==3 &&
        r.vs.GetRegisterForAttribute(0)==0 && r.vs.GetRegisterForAttribute(1)==1 &&
        r.vs.GetRegisterForAttribute(2)==3 &&
        t.main_config.texture0_enable && t.main_config.texture1_enable && !t.main_config.texture2_enable &&
        t.texture0.width==32 && t.texture0.height==32 && t.texture1.width==32 && t.texture1.height==64 &&
        unsigned(t.texture0_format.Value())==7 && unsigned(t.texture1_format.Value())==7 &&
        unsigned(t.texture0.wrap_s.Value())==2 && unsigned(t.texture0.wrap_t.Value())==2 &&
        unsigned(t.texture1.wrap_s.Value())==2 && unsigned(t.texture1.wrap_t.Value())==2 &&
        m.depth_test_enable && !m.depth_write_enable && !m.stencil_test.enable && m.alphablend_enable &&
        unsigned(m.alpha_blending.factor_source_rgb.Value())==6 &&
        unsigned(m.alpha_blending.factor_dest_rgb.Value())==1 &&
        unsigned(m.alpha_blending.blend_equation_rgb.Value())==0 &&
        t.tev_stage0.sources_raw==0x0ee00343 && t.tev_stage0.modifiers_raw==0 &&
        t.tev_stage0.ops_raw==0x10009 && t.tev_stage0.scales_raw==1 &&
        t.tev_stage1.sources_raw==0x0e1f034f && t.tev_stage1.modifiers_raw==0 &&
        t.tev_stage1.ops_raw==1 && t.tev_stage1.scales_raw==1 &&
        t.tev_stage2.sources_raw==0x0eef0eef && t.tev_stage2.modifiers_raw==0 &&
        t.tev_stage2.ops_raw==0x10001 && t.tev_stage2.scales_raw==1;
}
template<class Regs> bool ZoraWaterfallCandidate(const Regs& r) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    const auto count=r.pipeline.num_vertices;
    // Zora's River entrance curtain: two water sheets and the additive mist.
    // Match captured geometry/material signatures without scene allocation addresses.
    const bool mist=count==513;
    if(count!=288 && count!=192 && !mist)return false;
    if(unsigned(r.pipeline.triangle_topology.Value())!=3 ||
       r.vs.GetRegisterForAttribute(0)!=0 || r.vs.GetRegisterForAttribute(1)!=1 ||
       r.vs.GetRegisterForAttribute(2)!=2 || r.vs.GetRegisterForAttribute(3)!=3 ||
       !t.main_config.texture0_enable || t.main_config.texture2_enable ||
       t.texture0.width!=64 || t.texture0.height!=64 || unsigned(t.texture0_format.Value())!=13 ||
       unsigned(t.texture0.wrap_s.Value())!=2 || unsigned(t.texture0.wrap_t.Value())!=2 ||
       !m.depth_test_enable || m.depth_write_enable || m.stencil_test.enable || !m.alphablend_enable ||
       unsigned(m.alpha_blending.factor_source_rgb.Value())!=6 ||
       unsigned(m.alpha_blending.factor_dest_rgb.Value())!=(mist?1u:7u) ||
       unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0 ||
       t.tev_stage0.sources_raw!=0x0e300e30 || t.tev_stage0.modifiers_raw!=0 ||
       t.tev_stage0.ops_raw!=0x10001 || t.tev_stage0.scales_raw!=1)return false;
    if(mist)return !t.main_config.texture1_enable &&
        t.tev_stage1.sources_raw==0x0eef0e1f && t.tev_stage1.ops_raw==0x10000 &&
        t.tev_stage1.modifiers_raw==0 && t.tev_stage1.scales_raw==0 &&
        t.tev_stage2.sources_raw==0x0fff0fff && t.tev_stage2.ops_raw==0;
    return t.main_config.texture1_enable &&
        t.texture0.GetPhysicalAddress()==t.texture1.GetPhysicalAddress() &&
        t.texture1.width==64 && t.texture1.height==64 && unsigned(t.texture1_format.Value())==13 &&
        unsigned(t.texture1.wrap_s.Value())==2 && unsigned(t.texture1.wrap_t.Value())==2 &&
        t.tev_stage1.sources_raw==(count==288?0x0e1f0f4fu:0x0e1f0f40u) &&
        t.tev_stage1.modifiers_raw==0 && t.tev_stage1.ops_raw==8 && t.tev_stage1.scales_raw==0 &&
        t.tev_stage2.sources_raw==(count==288?0x0fff0fffu:0x0eef0e1fu) &&
        t.tev_stage2.ops_raw==(count==288?0u:0x10000u);
}
template<class Regs> bool JabuWallCandidate(const Regs& r,std::uint32_t scene) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    // bdan / bdan_dd: jab_maku_01, _02 and _03 + jab_maku_01x.
    // The blue room uses ETC1/source-color blending; the other membranes use
    // ETC1A4/source-alpha and either multiply-add or add-multiply combiners.
    // Do not key on color, room mesh size or texture allocation addresses.
    if((scene&0xffff)!=2 || unsigned(r.pipeline.triangle_topology.Value())!=3 ||
       r.vs.GetRegisterForAttribute(0)!=0 || r.vs.GetRegisterForAttribute(1)!=1 ||
       r.vs.GetRegisterForAttribute(2)!=2 || r.vs.GetRegisterForAttribute(3)!=3 ||
       !t.main_config.texture0_enable || !t.main_config.texture1_enable || t.main_config.texture2_enable ||
       t.texture0.width!=64 || t.texture0.height!=64 || t.texture1.width!=64 || t.texture1.height!=64 ||
       unsigned(t.texture1_format.Value())!=12 ||
       unsigned(t.texture0.wrap_s.Value())!=2 || unsigned(t.texture0.wrap_t.Value())!=2 ||
       unsigned(t.texture1.wrap_s.Value())!=2 || unsigned(t.texture1.wrap_t.Value())!=2 ||
       !m.depth_test_enable || m.depth_write_enable || m.stencil_test.enable || !m.alphablend_enable ||
       unsigned(m.alpha_blending.factor_dest_rgb.Value())!=1 ||
       unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0 ||
       t.tev_stage0.sources_raw!=0x0e300e30 || t.tev_stage0.modifiers_raw!=0 ||
       t.tev_stage0.ops_raw!=0x10001 || t.tev_stage0.scales_raw!=1 ||
       t.tev_stage1.scales_raw!=0 || t.tev_stage2.sources_raw!=0x0fff0fff || t.tev_stage2.ops_raw!=0)return false;
    if(unsigned(t.texture0_format.Value())==12)
        return unsigned(m.alpha_blending.factor_source_rgb.Value())==2 &&
            t.tev_stage1.sources_raw==0x0e1f0f40 && t.tev_stage1.modifiers_raw==0 && t.tev_stage1.ops_raw==8;
    return unsigned(t.texture0_format.Value())==13 && unsigned(m.alpha_blending.factor_source_rgb.Value())==6 &&
        ((t.tev_stage1.sources_raw==0x0eef0f40 && t.tev_stage1.modifiers_raw==0 && t.tev_stage1.ops_raw==0x10008) ||
         (t.tev_stage1.sources_raw==0x0eef0f43 && t.tev_stage1.modifiers_raw==2 && t.tev_stage1.ops_raw==0x10009));
}
// spot09 room 0, animated materials 9–14: waterfall curtain, foam and spray.
// The river and sky scroll too, but use different material/mesh combinations.
template<class Regs> bool GerudoWaterfallCandidate(const Regs& r,std::uint32_t scene) {
    if((scene&0xffff)!=0x5a)return false;
    const auto& t=r.texturing;
    const auto& m=r.framebuffer.output_merger;
    const unsigned n=r.pipeline.num_vertices;
    const bool second=t.main_config.texture1_enable;
    const bool spray=n==312;
    if(!(spray || n==432 || n==864 || n==540 || n==192) ||
       unsigned(r.pipeline.triangle_topology.Value())!=3 || !t.main_config.texture0_enable ||
       t.main_config.texture2_enable || !m.depth_test_enable || m.depth_write_enable ||
       m.stencil_test.enable || !m.alphablend_enable || unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0 ||
       unsigned(m.alpha_blending.factor_source_rgb.Value())!=(n==192?2u:6u) ||
       unsigned(m.alpha_blending.factor_dest_rgb.Value())!=(n==432?7u:1u) ||
       t.tev_stage0.sources_raw!=0x0e300e30 || t.tev_stage0.ops_raw!=0x10001)return false;
    const auto repeating=[](const auto& texture,unsigned size) {
        return texture.width==size && texture.height==size &&
               unsigned(texture.wrap_s.Value())==2 && unsigned(texture.wrap_t.Value())==2;
    };
    if(!repeating(t.texture0,spray?64:128) || unsigned(t.texture0_format.Value())!=(spray && !second?13u:12u))return false;
    if(!second)return (spray && t.tev_stage1.sources_raw==0x0fff0fff && t.tev_stage1.ops_raw==0) ||
                      (n==432 && t.tev_stage1.sources_raw==0x0eef0e1f && t.tev_stage1.ops_raw==0x10000);
    return n!=432 && repeating(t.texture1,n==192?128:64) && unsigned(t.texture1_format.Value())==(spray?13u:12u) &&
           t.tev_stage1.sources_raw==(spray?0x0eef0f40u:0x0e1f0f40u) && t.tev_stage1.ops_raw==(spray?0x10008u:8u);
}
template<class Regs> bool WellDripCandidate(const Regs& r, std::uint32_t scene) {
    if ((scene & 0xffff) != 8) return false;
    const auto& t = r.texturing;
    const auto& m = r.framebuffer.output_merger;
    const auto src = unsigned(m.alpha_blending.factor_source_rgb.Value());
    // hakadan_ch's ido_teki_01 is a repeating 8x128 ETC1A4 strip. Several
    // room meshes move the same texture at independent times and speeds.
    return unsigned(r.pipeline.triangle_topology.Value()) == 3 &&
        r.vs.GetRegisterForAttribute(0) == 0 && r.vs.GetRegisterForAttribute(1) == 1 &&
        r.vs.GetRegisterForAttribute(2) == 2 && r.vs.GetRegisterForAttribute(3) == 3 &&
        t.main_config.texture0_enable && !t.main_config.texture1_enable && !t.main_config.texture2_enable &&
        t.texture0.width == 8 && t.texture0.height == 128 && unsigned(t.texture0_format.Value()) == 13 &&
        unsigned(t.texture0.wrap_s.Value()) == 2 && unsigned(t.texture0.wrap_t.Value()) == 2 &&
        m.depth_test_enable && !m.depth_write_enable && !m.stencil_test.enable && m.alphablend_enable &&
        (src == 2 || src == 6) && unsigned(m.alpha_blending.factor_dest_rgb.Value()) == 1 &&
        unsigned(m.alpha_blending.blend_equation_rgb.Value()) == 0 &&
        t.tev_stage0.sources_raw == 0x0e300e30 && t.tev_stage0.ops_raw == 0x10001 &&
        !t.tev_stage0.modifiers_raw && t.tev_stage0.scales_raw == 1 &&
        !t.tev_stage1.modifiers_raw && !t.tev_stage1.scales_raw &&
        ((t.tev_stage1.sources_raw == 0x0fff0fff && t.tev_stage1.ops_raw == 0) ||
         (t.tev_stage1.sources_raw == 0x0e1f0eef && t.tev_stage1.ops_raw == 1));
}
// ddan_3 materials 4–6 carry the lava bed, lip and falling sheet.
// turibori_0 materials 24–26 carry the cascade and the water leading away from it.
template<class Regs> bool CavernOrPondFlow(const Regs& r,std::uint32_t scene) {
    const auto id=scene&0xffff;
    if(id!=1 && id!=0x49)return false;
    const auto& t=r.texturing;
    const auto& m=r.framebuffer.output_merger;
    const unsigned n=r.pipeline.num_vertices;
    const bool lava=id==1;
    if(lava ? (n!=615 && n!=108 && n!=531) : (n!=123 && n!=177 && n!=204))return false;
    const auto repeats=[](const auto& texture,unsigned size) {
        return texture.width==size && texture.height==size &&
               unsigned(texture.wrap_s.Value())==2 && unsigned(texture.wrap_t.Value())==2;
    };
    if(unsigned(r.pipeline.triangle_topology.Value())!=3 ||
       r.vs.GetRegisterForAttribute(0)!=0 || r.vs.GetRegisterForAttribute(1)!=1 ||
       r.vs.GetRegisterForAttribute(2)!=2 || r.vs.GetRegisterForAttribute(3)!=3 ||
       !t.main_config.texture0_enable || !t.main_config.texture1_enable ||
       bool(t.main_config.texture2_enable)!=lava ||
       !repeats(t.texture0,lava?128:64) || !repeats(t.texture1,64) ||
       unsigned(t.texture1_format.Value())!=12 ||
       !m.depth_test_enable || bool(m.depth_write_enable)!=(lava && n==615) || m.stencil_test.enable ||
       !m.alphablend_enable || unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0 ||
       t.tev_stage0.sources_raw!=0x0e300e30 || t.tev_stage0.ops_raw!=0x10001 ||
       t.tev_stage1.ops_raw!=8)return false;
    if(lava) {
        const bool sheet=n==531;
        return repeats(t.texture2,32) && unsigned(t.texture2_format.Value())==12 &&
               unsigned(t.texture0_format.Value())==(sheet?12u:13u) &&
               t.tev_stage1.sources_raw==(sheet?0x0e1f0f40u:0x0e1f0f34u) &&
               t.tev_stage2.sources_raw==(sheet?0x0e1f0f50u:0x0e1f0f35u) &&
               t.tev_stage2.ops_raw==8;
    }
    return unsigned(t.texture0_format.Value())==12 && t.tev_stage1.sources_raw==0x0e1f0f40 &&
           (t.texture0.GetPhysicalAddress()==t.texture1.GetPhysicalAddress())==(n!=177) &&
           unsigned(m.alpha_blending.factor_source_rgb.Value())==(n==123?6u:2u) &&
           unsigned(m.alpha_blending.factor_dest_rgb.Value())==(n==123?7u:1u);
}
template<class Regs> bool Candidate(const Regs& r,std::uint32_t scene=~0u) {
    if(CavernOrPondFlow(r,scene) || WellDripCandidate(r,scene) || BlueFlameCandidate(r) || ZoraWaterfallCandidate(r) || JabuWallCandidate(r,scene) || GerudoWaterfallCandidate(r,scene))return true;
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    // Domain water/mist has different vertex layouts and blend factors from
    // the River entrance. Both captured sections use two repeating ETC1 layers.
    const auto n=unsigned(r.pipeline.num_vertices);
    if((n==423 || n==192) && unsigned(r.pipeline.triangle_topology.Value())==3 &&
       r.vs.GetRegisterForAttribute(0)==0 &&
       r.vs.GetRegisterForAttribute(1)==(n==423?2u:1u) &&
       r.vs.GetRegisterForAttribute(2)==(n==423?3u:2u) &&
       t.main_config.texture0_enable && t.main_config.texture1_enable && !t.main_config.texture2_enable &&
       t.texture0.GetPhysicalAddress()==t.texture1.GetPhysicalAddress() &&
       t.texture0.width==64 && t.texture0.height==64 && t.texture1.width==64 && t.texture1.height==64 &&
       unsigned(t.texture0_format.Value())==12 && unsigned(t.texture1_format.Value())==12 &&
       unsigned(t.texture0.wrap_s.Value())==2 && unsigned(t.texture0.wrap_t.Value())==2 &&
       unsigned(t.texture1.wrap_s.Value())==2 && unsigned(t.texture1.wrap_t.Value())==2 &&
       m.depth_test_enable && !m.depth_write_enable && !m.stencil_test.enable && m.alphablend_enable &&
       unsigned(m.alpha_blending.factor_source_rgb.Value())==(n==423?2u:6u) &&
       unsigned(m.alpha_blending.factor_dest_rgb.Value())==(n==423?12u:7u) &&
       unsigned(m.alpha_blending.blend_equation_rgb.Value())==0 &&
       t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.modifiers_raw==0 &&
       t.tev_stage0.ops_raw==0x10001 && t.tev_stage0.scales_raw==(n==423?1u:0u) &&
       t.tev_stage1.sources_raw==0x0e1f0f40 && t.tev_stage1.ops_raw==8)return true;
    const bool fountain=r.pipeline.num_vertices==564;
    // Captured Kokiri and fairy-fountain waterfalls. The latter has a third
    // scrolling layer. Keep the nearby horizontal pools and overlays excluded.
    const unsigned size=fountain?128:64;
    const bool material=fountain?
        (t.main_config.texture2_enable && t.texture2.width==64 && t.texture2.height==64 &&
         unsigned(t.texture2_format.Value())==12 && unsigned(t.texture2.wrap_s.Value())==2 &&
         unsigned(t.texture2.wrap_t.Value())==2 && t.tev_stage0.scales_raw==1 &&
         t.tev_stage2.sources_raw==0x0e1f0f50 && t.tev_stage2.ops_raw==8 &&
         unsigned(m.alpha_blending.factor_dest_rgb.Value())==1):
        (r.pipeline.num_vertices==141 && !t.main_config.texture2_enable &&
         unsigned(m.alpha_blending.factor_dest_rgb.Value())==7);
    return material && unsigned(r.pipeline.triangle_topology.Value())==3 &&
        r.vs.GetRegisterForAttribute(0)==0 && r.vs.GetRegisterForAttribute(1)==1 &&
        r.vs.GetRegisterForAttribute(2)==2 && r.vs.GetRegisterForAttribute(3)==3 &&
        t.main_config.texture0_enable && t.main_config.texture1_enable &&
        t.texture0.GetPhysicalAddress()==t.texture1.GetPhysicalAddress() &&
        t.texture0.width==size && t.texture0.height==size && t.texture1.width==size && t.texture1.height==size &&
        unsigned(t.texture0_format.Value())==12 && unsigned(t.texture1_format.Value())==12 &&
        unsigned(t.texture0.wrap_s.Value())==2 && unsigned(t.texture0.wrap_t.Value())==2 &&
        unsigned(t.texture1.wrap_s.Value())==2 && unsigned(t.texture1.wrap_t.Value())==2 &&
        m.depth_test_enable && !m.depth_write_enable && !m.stencil_test.enable &&
        m.alphablend_enable && unsigned(m.alpha_blending.factor_source_rgb.Value())==6 &&
        t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.ops_raw==0x10001 &&
        t.tev_stage1.sources_raw==0x0e1f0f40 && t.tev_stage1.ops_raw==8;
}
template<class Regs> bool KokiriFogCandidate(const Regs& r,std::uint32_t scene) {
    // spot04_00 material 11 (s04_kawa_02) disables fog in the original CMB.
    // Reuse the captured waterfall signature, excluding pools and mist.
    return (scene&0xffff)==0x55 && r.pipeline.num_vertices==141 && Candidate(r,scene);
}
template<class Regs> bool LakeHyliaFogCandidate(const Regs& r,std::uint32_t scene) {
    // zelda_spot06_objects / c_s06beforewate, material 1, shape 1:
    // the 216-index vertical waterfall disables fog. The lake surface (1311),
    // foam (156/228) and drained-lake model are separate draws.
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    return (scene&0xffff)==0x57 && r.pipeline.num_vertices==216 &&
        unsigned(r.pipeline.triangle_topology.Value())==3 &&
        t.main_config.texture0_enable && t.main_config.texture1_enable && !t.main_config.texture2_enable &&
        t.texture0.GetPhysicalAddress()==t.texture1.GetPhysicalAddress() &&
        t.texture0.width==64 && t.texture0.height==64 && t.texture1.width==64 && t.texture1.height==64 &&
        unsigned(t.texture0_format.Value())==12 && unsigned(t.texture1_format.Value())==12 &&
        unsigned(t.texture0.wrap_s.Value())==2 && unsigned(t.texture0.wrap_t.Value())==2 &&
        unsigned(t.texture1.wrap_s.Value())==2 && unsigned(t.texture1.wrap_t.Value())==2 &&
        m.depth_test_enable && !m.depth_write_enable && !m.stencil_test.enable && m.alphablend_enable &&
        unsigned(m.alpha_blending.factor_source_rgb.Value())==2 &&
        unsigned(m.alpha_blending.factor_dest_rgb.Value())==12 &&
        unsigned(m.alpha_blending.blend_equation_rgb.Value())==0 &&
        t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.modifiers_raw==0 &&
        t.tev_stage0.ops_raw==0x10001 && t.tev_stage0.scales_raw==1 &&
        t.tev_stage1.sources_raw==0x0e1f0f40 && t.tev_stage1.modifiers_raw==0 &&
        t.tev_stage1.ops_raw==8 && t.tev_stage1.scales_raw==0;
}
template<class Regs> bool FogCandidate(const Regs& r,std::uint32_t scene) {
    return KokiriFogCandidate(r,scene) || LakeHyliaFogCandidate(r,scene);
}
class History {
    struct Entry {std::uint64_t frame{};UV uv{};};
    std::map<Key,Entry> entries;
public:
    void Reset(){entries.clear();}
    std::optional<UV> Apply(std::uint64_t frame,const Key& key,const UV& uv,bool vertical_only=false) {
        for(const auto& row:uv)for(float x:row)if(!std::isfinite(x))return {};
        for(auto it=entries.begin();it!=entries.end();)
            if(it->second.frame>frame || frame-it->second.frame>1)it=entries.erase(it);else ++it;
        if(!entries.contains(key) && entries.size()>=16)return {};
        auto& e=entries[key];
        if(e.frame==frame){e.frame=0;return {};}
        auto out=uv;bool valid=e.frame+1==frame;
        for(unsigned r=0;r<Rows.size();++r) {
            if(vertical_only && r!=1)continue; // Only texture 0 V slides.
            for(unsigned c=0;c<3;++c)if(std::abs(uv[r][c]-e.uv[r][c])>.0001f)valid=false;
            // All captured axes repeat. Take the short path through a wrapping
            // UV offset instead of sweeping the texture backwards at a loop.
            float delta=uv[r][3]-e.uv[r][3];delta-=std::round(delta);
            if(std::abs(delta)>.125f)valid=false;
            out[r][3]=uv[r][3]-delta*(1-sample_fraction);
        }
        e={frame,uv};return valid?std::optional{out}:std::nullopt;
    }
    template<class Regs> std::optional<UV> ApplyDraw(std::uint64_t frame, const Regs& r, bool indexed,
            unsigned epoch, unsigned scene, const UV& uv) {
        const auto& p = r.pipeline;
        const auto& t = r.texturing;
        const bool drips = WellDripCandidate(r, scene);
        Key key{p.vertex_attributes.GetPhysicalBaseAddress(), unsigned(p.index_array.offset),
                t.texture0.GetPhysicalAddress(), t.texture1.GetPhysicalAddress(),
                t.main_config.texture2_enable ? t.texture2.GetPhysicalAddress() : 0,
                epoch, scene, unsigned(p.num_vertices)};
        if (drips) {
            // Nonindexed room layers can share a count and texture. Identify
            // the actual vertex stream and pass, ignoring inactive texture slots.
            key[0] += unsigned(p.vertex_attributes.attribute_loaders[0].data_offset);
            if (!indexed) key[1] = p.vertex_offset;
            key[3] = key[4] = 0;
            key[8] = t.tev_stage1.sources_raw;
        }
        return Apply(frame, key, uv, drips);
    }

};
}
