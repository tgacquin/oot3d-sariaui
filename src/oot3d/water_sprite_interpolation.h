// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/debris_interpolation.h"
namespace OOT3D::Interpolation::WaterSprites {
namespace F=MM3D::ForestParticleInterpolation;
namespace W=MM3D::WaterParticleInterpolation;
using F::Vertex;using F::Quad;
// Captured shared soft effects: water bubbles, Zora droplets, splash atlas,
// Syabom burst and lava atlas. No scene-dependent texture addresses.
template<class Regs> unsigned Candidate(const Regs& r) {
    const auto& t=r.texturing;const auto& m=r.framebuffer.output_merger;
    if(unsigned(r.pipeline.triangle_topology.Value())!=1 || !r.pipeline.num_vertices ||
       r.pipeline.num_vertices>768 || r.pipeline.num_vertices%6 ||
       r.vs.GetRegisterForAttribute(0)!=0 || r.vs.GetRegisterForAttribute(1)!=2 ||
       r.vs.GetRegisterForAttribute(2)!=3 || !t.main_config.texture0_enable ||
       t.main_config.texture1_enable || t.main_config.texture2_enable ||
       !m.depth_test_enable || m.depth_write_enable || m.stencil_test.enable || !m.alphablend_enable ||
       unsigned(m.alpha_blending.factor_source_rgb.Value())!=6 ||
       unsigned(m.alpha_blending.blend_equation_rgb.Value())!=0)return 0;
    const unsigned w=t.texture0.width,h=t.texture0.height,f=unsigned(t.texture0_format.Value());
    // Navi's faint outer halo is three soft quads, separate from the colored
    // billboard and white core. Its mirrored UVs cover 0..2, not an atlas.
    if(r.pipeline.num_vertices==18 && w==32 && h==32 && f==5 &&
       unsigned(t.texture0.wrap_s.Value())==3 && unsigned(t.texture0.wrap_t.Value())==3 &&
       unsigned(m.alpha_blending.factor_dest_rgb.Value())==1 &&
       t.tev_stage0.sources_raw==0x0e300e30 && !t.tev_stage0.modifiers_raw &&
       t.tev_stage0.ops_raw==0x10001 && !t.tev_stage0.scales_raw &&
       t.tev_stage1.sources_raw==0x0fff0fff && !t.tev_stage1.modifiers_raw &&
       !t.tev_stage1.ops_raw && !t.tev_stage1.scales_raw)return 15;
    if(unsigned(t.texture0.wrap_s.Value())!=0 || unsigned(t.texture0.wrap_t.Value())!=0)return 0;
    // Shared 4x4 IA8 death-puff sheet. Tint is native, including Gohma's
    // orange/brown variant; no boss, color or allocation-address restriction.
    if(w==128 && h==128 && f==5 && unsigned(m.alpha_blending.factor_dest_rgb.Value())==7 &&
       t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.modifiers_raw==32 &&
       t.tev_stage0.ops_raw==0x10008 && !t.tev_stage0.scales_raw &&
       t.tev_stage1.sources_raw==0x0fff0fff && !t.tev_stage1.ops_raw)return 14;
    // soft_hit1/2/3: shared 4x4 ETC1 impact sheets, captured on Gohma hits.
    // Keep the material independent of enemy type, texture address and tint.
    if(w==256 && h==256 && f==12 && unsigned(m.alpha_blending.factor_dest_rgb.Value())==1 &&
       t.tev_stage0.sources_raw==0x0ee00e30 && !t.tev_stage0.modifiers_raw &&
       t.tev_stage0.ops_raw==1 && t.tev_stage0.scales_raw==1 &&
       t.tev_stage1.sources_raw==0x0fff0fff && !t.tev_stage1.ops_raw)return 13;
    if(w==32 && h==16 && f==5 && unsigned(m.alpha_blending.factor_dest_rgb.Value())==7 &&
       t.tev_stage0.sources_raw==0x0e300e30 && t.tev_stage0.modifiers_raw==32 &&
       t.tev_stage0.ops_raw==0x10008 && t.tev_stage1.sources_raw==0x0fff0fff)return 11;
    if(unsigned(m.alpha_blending.factor_dest_rgb.Value())!=1 ||
       t.tev_stage0.sources_raw!=0x0ee00ee3 || t.tev_stage0.ops_raw!=0 || t.tev_stage0.scales_raw!=2 ||
       t.tev_stage1.sources_raw!=0x0eee0efe || t.tev_stage1.ops_raw!=1 ||
       t.tev_stage2.sources_raw!=0x0ee00f30 || t.tev_stage2.ops_raw!=8)return 0;
    if(t.tev_stage0.modifiers_raw==2 && t.tev_stage2.modifiers_raw==32)
        return w==256 && h==128 && f==8?10:0;
    if(t.tev_stage0.modifiers_raw || t.tev_stage2.modifiers_raw || f!=7)return 0;
    if(w==32 && h==16)return 8;
    if(w==16 && h==16)return 9;
    return w==256 && h==128?12:0;
}
inline std::optional<std::vector<Quad>> Decode(std::span<const Vertex> inputs,
                                             std::span<const std::uint32_t> order,float uv_limit=1) {
    auto qs=W::DecodeQuads(inputs,order);if(!qs)return {};
    for(auto& q:*qs) {
        float lo[2]{1,1},hi[2]{};
        for(const auto& v:q.vertices)for(unsigned c=0;c<2;++c) {
            lo[c]=std::min(lo[c],v[2][c]);hi[c]=std::max(hi[c],v[2][c]);
        }
        if(lo[0]<0 || lo[1]<0 || hi[0]>uv_limit || hi[1]>uv_limit || hi[0]<=lo[0] || hi[1]<=lo[1])return {};
        Quad sorted{};unsigned used=0;
        for(unsigned i=0;i<4;++i) {
            unsigned corner=4;
            for(unsigned k=0;k<4;++k)if(F::Near(q.vertices[i][2][0],k%2?hi[0]:lo[0]) &&
                F::Near(q.vertices[i][2][1],k/2?lo[1]:hi[1]))corner=k;
            if(corner==4 || (used&(1u<<corner)))return {};
            used|=1u<<corner;sorted.vertices[corner]=q.vertices[i];sorted.indices[corner]=q.indices[i];
        }
        q=sorted;
        for(unsigned edge:{1u,2u}) {
            float length=0;for(unsigned c=0;c<3;++c)length+=std::pow(q.vertices[edge][0][c]-q.vertices[0][0][c],2);
            if(length<.00001f || length>256*256)return {};
        }
    }
    return qs;
}
inline float Score(const Quad& a,const Quad& b,unsigned kind) {
    const auto ac=F::Center(a),bc=F::Center(b);float distance=0;
    for(unsigned c=0;c<3;++c)distance+=std::pow(ac[c]-bc[c],2);
    if(distance>32*32 || a.vertices[0][1][3]-b.vertices[0][1][3]>.3f)return INFINITY;
    for(unsigned edge:{1u,2u}) {
        float aa=0,bb=0,ab=0;
        for(unsigned c=0;c<3;++c) {
            const float u=a.vertices[edge][0][c]-a.vertices[0][0][c];
            const float v=b.vertices[edge][0][c]-b.vertices[0][0][c];aa+=u*u;bb+=v*v;ab+=u*v;
        }
        if(aa<bb*.16f || aa>bb*6.25f || ab<.7f*std::sqrt(aa*bb))return INFINITY;
        distance+=std::pow(std::sqrt(aa)-std::sqrt(bb),2);
    }
    if(kind==10 || kind==12 || kind==13 || kind==14) {
        const unsigned columns=kind==10?8:4,rows=kind==10?2:4;
        const auto cell=[&](const Quad& q){return int(std::lround(q.vertices[0][2][0]*columns))+
            int(std::lround((1-q.vertices[0][2][1])*rows))*columns;};
        const int delta=cell(a)-cell(b);
        // Hit sheets can advance two cells in one native tick (captured).
        if(delta<0 || delta>(kind==13?2:1))return INFINITY;
    }
    return distance;
}
// Impact bubbles expand from a common origin while shrinking uniformly.
// Their centers can be closer together than their per-tick travel, so ordinary
// nearest-neighbor matching rejects most of the burst. Fit its translation and
// expansion, then require a unique geometric correspondence for EVERY member.
// This uses neither draw order nor particle slots, and fails closed at births,
// deaths, duplicate centers, mixed bursts or a discontinuity.
inline std::vector<int> BurstMatches(const std::vector<Quad>& now,const std::vector<Quad>& old) {
    std::vector<int> matches(now.size(),-1);std::vector<bool> visited(now.size());
    const auto same=[](const Quad& a,const Quad& b) {
        if(std::abs(Debris::Size(a)-Debris::Size(b))>.005f)return false;
        for(unsigned c=0;c<4;++c)if(!F::Near(a.vertices[0][1][c],b.vertices[0][1][c]))return false;
        return a.vertices[0][2]==b.vertices[0][2];
    };
    for(unsigned seed=0;seed<now.size();++seed) {
        if(visited[seed])continue;
        std::vector<unsigned> ns;
        for(unsigned i=seed;i<now.size();++i)if(same(now[seed],now[i])){ns.push_back(i);visited[i]=true;}
        // Native impact emitters release six particles per tick.
        if(ns.size()<6)continue;
        std::vector<std::vector<unsigned>> candidates;
        std::vector<bool> old_visited(old.size());
        for(unsigned p=0;p<old.size();++p) {
            if(old_visited[p])continue;
            std::vector<unsigned> os;
            for(unsigned j=p;j<old.size();++j)if(same(old[p],old[j])){os.push_back(j);old_visited[j]=true;}
            const float shrink=Debris::Size(old[p])-Debris::Size(now[seed]);
            if(os.size()!=ns.size() || shrink<.01f || shrink>.5f)continue;
            bool tint=true;
            for(unsigned c=0;c<4;++c)tint&=F::Near(now[seed].vertices[0][1][c],old[p].vertices[0][1][c]);
            if(!tint)continue;
            std::array<double,3> nc{},oc{};
            for(unsigned k=0;k<ns.size();++k) {
                const auto a=F::Center(now[ns[k]]),b=F::Center(old[os[k]]);
                for(unsigned c=0;c<3;++c){nc[c]+=a[c]/double(ns.size());oc[c]+=b[c]/double(ns.size());}
            }
            double nr=0,orr=0;
            for(unsigned k=0;k<ns.size();++k) {
                const auto a=F::Center(now[ns[k]]),b=F::Center(old[os[k]]);
                for(unsigned c=0;c<3;++c){nr+=std::pow(a[c]-nc[c],2);orr+=std::pow(b[c]-oc[c],2);}
            }
            if(orr<.0001)continue;
            const double scale=std::sqrt(nr/orr);
            if(scale<1 || scale>2.1)continue;
            std::vector<unsigned> mapping;std::vector<bool> used(os.size());
            for(auto i:ns) {
                const auto a=F::Center(now[i]);unsigned found=os.size(),count=0;
                for(unsigned j=0;j<os.size();++j) {
                    const auto b=F::Center(old[os[j]]);double error=0;
                    for(unsigned c=0;c<3;++c)error+=std::pow(a[c]-(nc[c]+scale*(b[c]-oc[c])),2);
                    if(error<.0004 && std::isfinite(Score(now[i],old[os[j]],8))){found=j;++count;}
                }
                if(count!=1 || used[found]){mapping.clear();break;}
                used[found]=true;mapping.push_back(os[found]);
            }
            if(mapping.size()==ns.size())candidates.push_back(std::move(mapping));
        }
        if(candidates.size()==1)for(unsigned k=0;k<ns.size();++k)matches[ns[k]]=candidates[0][k];
    }
    return matches;
}
class History {
    struct Group {std::uint64_t frame{};std::vector<Quad> previous,current;std::vector<bool> used;bool halo_duplicate=false;};
    std::map<std::array<unsigned,2>,Group> groups;
public:
    std::vector<Vertex> old_inputs;
    void Reset(){groups.clear();old_inputs.clear();}
    std::optional<std::vector<Vertex>> Apply(unsigned kind,std::uint64_t frame,unsigned texture,
        std::span<const Vertex> inputs,std::span<const std::uint32_t> order) {
        old_inputs.clear();const auto current=Decode(inputs,order,kind==15?2.f:1.f);if(!current)return {};
        if(kind==15)for(const auto& q:*current) {
            if(q.vertices[0][2][0]!=0 || q.vertices[0][2][1]!=2 ||
               q.vertices[3][2][0]!=2 || q.vertices[3][2][1]!=0)return {};
        }
        if(kind==13 || kind==14)for(const auto& q:*current) {
            // Hit sheets use complete, aligned cells. Never sweep through
            // neighbors or treat arbitrary cropped quads as this animation.
            if(!F::Near(q.vertices[1][2][0]-q.vertices[0][2][0],.25f) ||
               !F::Near(q.vertices[0][2][1]-q.vertices[2][2][1],.25f))return {};
            for(unsigned c=0;c<2;++c)if(!F::Near(q.vertices[0][2][c]*4,
                std::round(q.vertices[0][2][c]*4)))return {};
        }
        for(auto i=groups.begin();i!=groups.end();)if(i->second.frame>frame || frame-i->second.frame>1)i=groups.erase(i);else ++i;
        const std::array<unsigned,2> key{kind,texture};
        if(!groups.contains(key) && groups.size()>=16)return {};
        auto& g=groups[key];
        if(g.frame!=frame) {
            g.previous=g.frame+1==frame?std::move(g.current):std::vector<Quad>{};
            g.current.clear();g.used.assign(g.previous.size(),false);g.frame=frame;g.halo_duplicate=false;
        }
        if(kind==15) {
            // These are ordered lens-glow layers, not independent particles.
            // Nearest-neighbor matching can swap overlapping layers when Navi
            // moves. Require the same descending size pattern and coherent
            // movement, then pair each layer with its own previous layer.
            const auto ordered=[](const std::vector<Quad>& qs) {
                if(qs.size()<2 || qs.size()>3)return false;
                for(unsigned i=1;i<qs.size();++i)
                    if(Debris::Size(qs[i])>=Debris::Size(qs[i-1])*.95f)return false;
                return true;
            };
            bool valid=!g.halo_duplicate && g.current.empty() && ordered(*current) && ordered(g.previous) &&
                current->size()==g.previous.size();
            if(valid) {
                const auto a=F::Center((*current)[0]),b=F::Center(g.previous[0]);
                for(unsigned i=0;i<current->size();++i) {
                    const auto& q=(*current)[i];const auto& p=g.previous[i];
                    valid &= std::isfinite(Score(q,p,kind));
                    valid &= std::abs(Debris::Size(q)/Debris::Size((*current)[0])-
                        Debris::Size(p)/Debris::Size(g.previous[0]))<.01f;
                    const auto c=F::Center(q),d=F::Center(p);float change=0;
                    for(unsigned axis=0;axis<3;++axis)change+=std::pow((c[axis]-d[axis])-(a[axis]-b[axis]),2);
                    valid &= change<8*8;
                }
            }
            std::vector<Vertex> out(inputs.begin(),inputs.end());
            if(valid)for(unsigned i=0;i<current->size();++i)for(unsigned k=0;k<4;++k) {
                const auto& q=(*current)[i];const auto& p=g.previous[i];auto& v=out[q.indices[k]];
                for(unsigned c=0;c<3;++c)v[0][c]=SampleMix(p.vertices[k][0][c],v[0][c]);
                for(unsigned c=0;c<4;++c)v[1][c]=SampleMix(p.vertices[k][1][c],v[1][c]);
            }
            if(g.halo_duplicate)g.current.clear();
            else if(g.current.empty())g.current=*current;
            else {g.current.clear();g.halo_duplicate=true;} // Ambiguous repeated submission cannot seed history.
            return valid?std::optional{out}:std::nullopt;
        }
        std::vector<Vertex> out(inputs.begin(),inputs.end()),old=out;
        for(auto& v:old)v[1][3]=0;
        bool matched=false,crossfade=false;
        const auto burst=kind==8?BurstMatches(*current,g.previous):std::vector<int>(current->size(),-1);
        for(unsigned qi=0;qi<current->size();++qi) {
            const auto& q=(*current)[qi];float best=INFINITY,second=INFINITY;unsigned found=g.previous.size();
            for(unsigned i=0;i<g.previous.size();++i)if(!g.used[i]) {
                const float score=Score(q,g.previous[i],kind);
                if(score<best){second=best;best=score;found=i;}else second=std::min(second,score);
            }
            const bool burst_match=burst[qi]>=0 && !g.used[burst[qi]];
            if(burst_match)found=unsigned(burst[qi]);
            else if(found==g.previous.size() || second<=best*1.5f+.01f)continue;
            // Reciprocal within this batch: close crossing particles stay native.
            bool unique=true;
            if(!burst_match)for(unsigned i=0;i<current->size();++i)if(i!=qi && Score((*current)[i],g.previous[found],kind)<=best)unique=false;
            if(!unique)continue;
            const auto& p=g.previous[found];g.used[found]=true;matched=true;
            bool sheet_changed=false;
            for(unsigned k=0;k<4;++k)sheet_changed|=q.vertices[k][2]!=p.vertices[k][2];
            // Fade between atlas CELLS; interpolating UV coordinates would
            // sweep across unrelated frames on the sheet.
            const bool fade=sheet_changed && (kind==10 || kind==12 || kind==13 || kind==14) && sample_fraction<1;
            for(unsigned k=0;k<4;++k) {
                auto v=q.vertices[k];
                for(unsigned c=0;c<3;++c)v[0][c]=SampleMix(p.vertices[k][0][c],v[0][c]);
                for(unsigned c=0;c<4;++c)v[1][c]=SampleMix(p.vertices[k][1][c],v[1][c]);
                auto prior=v;prior[2]=p.vertices[k][2];
                if(fade){
                    // Alpha-blended death puffs combine the two texture cells
                    // before TEV/blending, keeping native opacity in one pass.
                    if(kind!=14){prior[1][3]*=1-sample_fraction;v[1][3]*=sample_fraction;}
                    old[q.indices[k]]=prior;crossfade=true;
                }
                out[q.indices[k]]=v;
            }
        }
        if(g.current.size()+current->size()<=512)g.current.insert(g.current.end(),current->begin(),current->end());
        if(crossfade)old_inputs=std::move(old);
        return matched?std::optional{out}:std::nullopt;
    }
};
}
