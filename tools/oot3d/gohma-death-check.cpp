// SPDX-License-Identifier: GPL-2.0-or-later
#define main ExistingSurfaceCheck
#include "surface-effects-check.cpp"
#undef main
#include "video_core/shader/generator/glsl_fs_shader_gen.h"
#include "video_core/shader/generator/spv_fs_shader_gen.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include <spirv-tools/libspirv.hpp>
namespace S=OOT3D::Interpolation::WaterSprites;

void BurstFixture() {
    Batch a,b;
    for(unsigned i=0;i<6;++i) {
        auto q=Star(float(i*i)*.05f,0,.8f);
        for(auto& v:q)v[0][1]+=float(i%3)*.1f;
        Add(a,q);
    }
    End(a);
    for(int i=5;i>=0;--i) {
        auto q=Star(float(i*i)*.1f+5,0,.8f);
        const float x=float(i*i)*.1f+5,y=float(i%3)*.2f+3;
        for(auto& v:q){v[0][0]=x+(v[0][0]-x)*.9f;v[0][1]=y+v[0][1]*.9f;}
        Add(b,q);
    }
    End(b);
    const auto old=S::Decode(a.inputs,a.order),now=S::Decode(b.inputs,b.order);assert(old && now);
    const auto matches=S::BurstMatches(*now,*old);
    for(unsigned i=0;i<6;++i)assert(matches[i]==5-int(i)); // Deliberately shuffled, dense burst.
    auto changed=*now;for(auto& v:changed[0].vertices)v[0][0]+=.3f;
    for(auto m:S::BurstMatches(changed,*old))assert(m==-1);
    changed=*now;changed.pop_back();for(auto m:S::BurstMatches(changed,*old))assert(m==-1);
    changed=*now;changed[0]=changed[1];for(auto m:S::BurstMatches(changed,*old))assert(m==-1);
}
void AtlasFixture(float phase) {
    const auto quad=[](float x,unsigned cell) {
        auto q=Star(x,0,.6f);
        for(auto& v:q){v[2][0]=(cell%4+v[2][0])/4;v[2][1]=1-(cell/4+1-v[2][1])/4;}
        return q;
    };
    Batch a,b;Add(a,quad(0,3));Add(a,quad(80,7));End(a);
    Add(b,quad(82,8));Add(b,quad(2,4));Add(b,quad(200,0));End(b);
    a=NonIndexed(a);b=NonIndexed(b);S::History history;
    const auto apply=[&](unsigned frame,const Batch& batch) {
        return OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(batch.inputs,batch.order,[&](const auto& v,const auto& order) {
            return history.Apply(14,frame,123,v,order);
        });
    };
    assert(!apply(1,a));const auto result=apply(2,b);assert(result && !history.old_inputs.empty());
    assert(std::abs((*result)[0][0][0]-(79+2*phase))<.0001f);
    for(unsigned i:{0u,1u,2u,3u,6u,7u,8u,9u})assert((*result)[i][1][3]==.6f);
    for(unsigned i=12;i<b.inputs.size();++i)assert((*result)[i]==b.inputs[i]);
    assert((*result)[3]==(*result)[4] && (*result)[5]==(*result)[6]);
    assert(!apply(4,b));history.Reset();assert(!apply(5,b));
    Batch bad;Add(bad,quad(0,2));End(bad);history.Reset();assert(!apply(1,a) && !apply(2,bad));
}
void Shaders(const Pica::RegsInternal& regs) {
    using namespace Pica::Shader;
    Profile profile{};profile.is_vulkan=profile.has_separable_shaders=profile.has_logic_op=1;
    FSConfig config{regs};UserConfig user{};
    const auto glsl=Generator::GLSL::GenerateFragmentShader(config,user,profile);
    assert(glsl.find("sprite_atlas_blend.x > 0.0")!=std::string::npos);
    const auto compiled=Vulkan::CompileGLSL(glsl,vk::ShaderStageFlagBits::eFragment);
    const auto spirv=Generator::SPIRV::GenerateFragmentShader(config,profile);
    spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
    validator.SetMessageConsumer([](auto,const char*,const spv_position_t&,const char* message){std::cerr<<message<<'\n';});
    assert(!compiled.empty() && validator.Validate(compiled));
    assert(!spirv.empty() && validator.Validate(spirv));
    std::cout<<"Death-puff GLSL and SPIR-V shaders compiled and validated\n";
}

int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    if(argc==3) {
        QFile water(argv[2]);assert(water.open(QIODevice::ReadOnly));
        const auto water_draws=QJsonDocument::fromJson(water.readAll()).object()["draws"].toArray();
        unsigned waterfalls=0;
        for(const auto value:water_draws) {
            const auto d=value.toObject();auto r=Regs(d);
            if(!W::KokiriFogCandidate(r,d["scene"].toInt()))continue;
            ++waterfalls;assert(!W::KokiriFogCandidate(r,0x56));
            r.pipeline.num_vertices=564;assert(!W::KokiriFogCandidate(r,0x55));
        }
        assert(waterfalls>=80);std::cout<<"Kokiri waterfall fog: "<<waterfalls<<" captured draws; other scene/mesh guards passed\n";
    }
    QFile file(argv[1]);assert(file.open(QIODevice::ReadOnly));
    const auto draws=QJsonDocument::fromJson(file.readAll()).object()["draws"].toArray();
    BurstFixture();bool shaders=false;
    for(float phase:{.25f,.5f,.75f}) {
    OOT3D::Interpolation::sample_fraction=phase;AtlasFixture(phase);
    D::History debris;E::History effects;
    std::map<unsigned,std::array<unsigned,4>> counts;
    for(const auto value:draws) {
        const auto d=value.toObject();const auto regs=Regs(d);
        const auto kind=E::Candidate(regs,d["shader"].toString().toULongLong(nullptr,16),d["swizzle"].toString().toULongLong(nullptr,16));
        if(kind!=3 && kind<8)continue;
        if(kind==14 && !shaders){Shaders(regs);shaders=true;}
        auto& n=counts[kind];++n[0];
        if(kind==3) {
            const auto b=CapturedDebris(d);
            OOT3D::Interpolation::ReplayEffectStrip<D::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                const auto q=D::Decode(v,order);assert(q);n[1]+=q->size();
                const auto result=debris.Apply(d["frame"].toInteger(),regs.texturing.texture0.GetPhysicalAddress(),v,order);
                if(result){
                    ++n[2];const auto decoded=D::Decode(*result,order);assert(decoded && decoded->size()==q->size());
                    for(unsigned i=0;i<order.size();i+=6)n[3]+=(*result)[order[i]][0]!=v[order[i]][0];
                    for(unsigned i=0;i<v.size();++i)assert((*result)[i][2]==v[i][2] && (*result)[i][3]==v[i][3]);
                }
                return result;
            });
        } else {
            const auto b=CapturedEffect(d);
            OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,[&](const auto& v,const auto& order) {
                const auto q=OOT3D::Interpolation::WaterSprites::Decode(v,order);if(q)n[1]+=q->size();
                const auto result=effects.Apply(kind,d["frame"].toInteger(),regs.texturing.texture0.GetPhysicalAddress(),v,order);
                if(result){++n[2];for(unsigned i=0;i<order.size();i+=6)n[3]+=(*result)[order[i]][0]!=v[order[i]][0];}
                return result;
            });
        }
    }
    for(auto [kind,n]:counts)std::cout<<kind<<": draws "<<n[0]<<" decoded quads "<<n[1]<<" paired draws "<<n[2]<<" moving quads "<<n[3]<<std::endl;
    assert(counts[3][0]==81 && counts[3][1]==8034 && counts[3][2]==54 && counts[3][3]>4500);
    assert(counts[8][0]==39 && counts[8][3]>500);
    assert(counts[13][0]==21 && counts[13][2]>=13);
    assert(counts[14][0]==46 && counts[14][2]>=24);
    }
}
