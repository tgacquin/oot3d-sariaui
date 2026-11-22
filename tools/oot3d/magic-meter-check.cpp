// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/magic_meter.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace OOT3D;
struct Vertex {
    std::array<float,4> position{},color{};
    std::array<float,2> uv{};
    float tex_coord0_w{};
    std::array<float,2> tex_coord1{},tex_coord2{};
    std::array<float,3> view{};
    bool operator==(const Vertex&) const=default;
};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
Vertex Mix(const Vertex& a,const Vertex& b,float t) {
    Vertex v;
    for(unsigned i=0;i<4;++i) {
        v.position[i]=a.position[i]*(1-t)+b.position[i]*t;
        v.color[i]=a.color[i]*(1-t)+b.color[i]*t;
    }
    for(unsigned i=0;i<2;++i)v.uv[i]=a.uv[i]*(1-t)+b.uv[i]*t;
    v.tex_coord0_w=std::lerp(a.tex_coord0_w,b.tex_coord0_w,t);
    return v;
}
std::vector<Vertex> Native(float width,float fraction,float scale,bool rotated) {
    std::vector<Vertex> out;
    const std::array edges{0.f,8.f,width-8,width};
    for(unsigned q=0;q<4;++q) {
        const float x0=q==3?2:edges[q];
        const float x1=q==3?2+(width-4)*fraction:edges[q+1];
        const float y0=q==3?2:0,y1=q==3?6:8;
        std::array<Vertex,4> v;
        for(unsigned i=0;i<4;++i) {
            const float x=(i%2?x1:x0)*scale,y=(i/2?y1:y0)*scale;
            v[i].position=rotated?std::array{.4f+y,-.7f-x,.2f,1.f}:
                std::array{.4f+x,-.7f+y,.2f,1.f};
            v[i].color={.8f,.9f,1,.35f};
            v[i].uv={float(q)+(i%2?1.f:0.f),i/2?0.f:1.f};
        }
        for(unsigned i:{0u,2u,1u,1u,2u,3u})out.push_back(v[i]);
    }
    return out;
}
int main() {
    MagicMeter::LensPulse lens;
    assert(lens.Sample(false,600000,1)==-1 && lens.Sample(true,610000,1)==0);
    assert(lens.Sample(true,710000,1)==100000 && lens.Sample(true,710000,1)==100000);
    assert(lens.Sample(true,700000,1)==0 && lens.Sample(true,750000,2)==0);
    assert(lens.Sample(false,760000,2)==-1 && lens.Sample(true,800000,2)==0);
    constexpr float gray=150.f/255.f;
    for(int cycle=0;cycle<3;++cycle) {
        const auto light=[&](int us){return MagicMeter::LensBrightness(cycle*300000+us);};
        assert(light(0)==1 && Near(light(50000),(1+gray)/2));
        assert(light(100000)==gray && light(149999)==gray);
        assert(Near(light(200000),(1+gray)/2) && light(250000)==1 && light(299999)==1);
    }
    // The Lens pulses only the custom outline.
    for(bool reshape:{false,true})for(float width:{82.f,164.f}) {
        HUD::Layout layout;
        layout.magic_border_roundness=.53f;
        layout.magic_contour={.enabled=true,.width=.44f,.height=.44f,.opacity=.8f,.color={1,1,1}};
        layout.magic_shadow.enabled=true;
        if(reshape){layout.magic_height=2;layout.magic_background_opacity=.5f;}
        for(const auto color:{std::array{1.f,1.f,1.f},std::array{.3f,.6f,.9f}}) {
            layout.magic_contour.color=color;
            const auto draw=[&](float brightness) {
                auto vertices=Native(width,.5f,.01f,true);
                std::vector<Vertex> output;
                assert(MagicMeter::Append(std::span<const Vertex>{vertices},layout,output,Mix,brightness));
                return output;
            };
            const auto full=draw(1.f);
            assert(full[0].tex_coord0_w==-5 && full[6].tex_coord0_w==-4);
            for(float brightness:{gray,(gray+1.f)*.5f}) {
                const auto dimmed=draw(brightness);
                assert(dimmed.size()==full.size());
                for(std::size_t i=0;i<full.size();++i) {
                    const bool frame=i>=6 && i<12;
                    assert(dimmed[i].color[3]==full[i].color[3]);
                    for(unsigned c=0;c<3;++c)
                        assert(Near(dimmed[i].color[c],full[i].color[c]*(frame?brightness:1.f)));
                }
            }
        }
    }
    // Fill only: half-frame drain/refill, empty endpoints, projection changes,
    // hide/fade, missed frames and savestate generations.
    for(float width:{82.f,164.f})for(bool rotated:{false,true}) {
        MagicMeter::History<Vertex> history;

        auto first=Native(width,1,.001f,rotated);
        assert(!history.Observe(first,1,33333,0));
        unsigned frame=1;float previous=1;
        for(float amount:{.7f,.2f,0.f,.3f,.8f,1.f}) {
            auto next=Native(width,amount,.001f,rotated);const auto native=next;
            // File entry fades the HUD while its green fill advances.
            for(auto& v:next)v.color[3]=float(frame)/10;
            ++frame;assert(history.Observe(next,frame,frame*33333,0));
            for(float phase:{.25f,.5f,.75f,1.f}) {
                auto sampled=next;
                history.Sample(std::span<Vertex,24>{sampled.data(),24},phase);
                const auto expected=Native(width,std::lerp(previous,amount,phase),.001f,rotated);
                for(unsigned i=0;i<24;++i) {
                    for(unsigned c=0;c<4;++c)
                        assert(std::abs(sampled[i].position[c]-expected[i].position[c])<.00001f);
                    assert(sampled[i].color==next[i].color && sampled[i].uv==native[i].uv);
                }
            }
            previous=amount;
        }
        auto shifted=Native(width,.5f,.001f,rotated);
        for(auto& v:shifted)v.position[0]+=.1f;
        ++frame;assert(!history.Observe(shifted,frame,frame*33333,0));
        auto faded=shifted;for(auto& v:faded)v.color[3]=0;
        ++frame;assert(!history.Observe(faded,frame,frame*33333,0));
        frame+=3;assert(!history.Observe(faded,frame,frame*33333,0));
        ++frame;assert(!history.Observe(faded,frame,frame*33333,1));
    }
    HUD::Layout l;
    // Measure the rendered gap from rim to fill with the saved meter style.
    HUD::Layout authored;
    authored[HUD::Magic].scale=.67f;
    authored.magic_height=1.86f;
    authored.magic_border=.62f;
    authored.magic_contour={.enabled=true,.width=1.09f,.height=1.09f};
    for(float base:{.5f,1.f,2.f})for(float user:{.5f,1.f,2.f})for(float element:{-90.f,0.f,200.f}) {
        authored.overall_scale=base;HUD::SetLayout(authored,false);
        UserHUD::Layout personal{};
        personal.overall_scale=user;personal[UserHUD::Magic].scale=element;
        UserHUD::SetLayout(personal,false);
        const auto effective=HUD::GetEffectiveLayout();
        const auto input=Native(82,.5f,effective[HUD::Magic].scale*.01f,true);
        std::vector<Vertex> output;
        assert(MagicMeter::Append(std::span<const Vertex>{input},effective,output,Mix));
        assert(output.size()==18);
        const auto& rim=output[0].position;
        const auto& background=output[6].position;
        const auto& fill=output[12].position;
        assert(Near((background[0]-rim[0])*120,1.09f));
        assert(Near((rim[1]-background[1])*200,1.09f));
        const float pad=.01f*.67f*2*.62f;
        assert(Near((fill[0]-background[0])*120,pad*120));
        assert(Near((background[1]-fill[1])*200,pad*200));
        assert(output[17].position[0]>fill[0] && output[17].position[1]<fill[1]);
    }
    HUD::SetLayout({},false);UserHUD::SetLayout({},false);
    assert(MagicMeter::Valid(l));
    l.magic_height=std::numeric_limits<float>::quiet_NaN();
    assert(!MagicMeter::Valid(l));
    l={};l.magic_height=2;
    for(float width:{82.f,164.f})for(float fraction:{0.f,.25f,1.f})
    for(float scale:{.001f,.5f,2.f})for(bool rotated:{false,true})
    for(float height:{1.f,2.f,8.f})for(float border:{0.f,.25f,1.f,2.f,4.f})
    for(float width_scale:{.5f,1.f,2.f,4.f}) {
        l.magic_height=height;l.magic_border=border;l.magic_width=width_scale;
        const auto native=Native(width,fraction,scale,rotated);
        auto input=native;
        std::vector<Vertex> output;
        assert(MagicMeter::Append(std::span<const Vertex>{input},l,output,Mix));
        assert(input==native);
        assert(output.size()==(fraction==0?6:12));
        const auto coord=[&](const Vertex& v,bool y) {
            if(rotated)return y?(v.position[0]-.4f)/scale:(-.7f-v.position[1])/scale;
            return y?(v.position[1]+.7f)/scale:(v.position[0]-.4f)/scale;
        };
        // Top/left edges stay fixed; width preserves the native fill fraction.
        assert(Near(coord(output[0],false),0) && Near(coord(output[0],true),0));
        assert(Near(coord(output[5],false),width*width_scale));
        assert(Near(coord(output[5],true),4*border+4*height));
        for(unsigned i=0;i<6;++i)assert(output[i].tex_coord0_w==MagicMeter::BackgroundMarker);
        if(fraction>0) {
            assert(Near(coord(output[6],false),2*border));
            assert(Near(coord(output[6],true),2*border));
            assert(Near(coord(output[11],false),2*border+(width*width_scale-4*border)*fraction));
            assert(Near(coord(output[11],true),2*border+4*height));
        }
        for(const auto& v:output) {
            assert(Near(v.color[3],.35f) && Near(v.position[2],.2f) && v.position[3]==1);
        }
    }
    // The single black quad follows the native fade and the background opacity setting.
    for(float opacity:{0.f,.25f,.75f})for(float width:{82.f,164.f})
    for(float height:{1.f,3.f})for(float border:{0.f,.25f,1.f,4.f}) {
        l={};l.magic_background_opacity=opacity;l.magic_height=height;l.magic_border=border;
        const auto input=Native(width,.5f,1,false);std::vector<Vertex> output;
        assert(MagicMeter::Append(std::span<const Vertex>{input},l,output,Mix));
        assert(output.size()==(opacity==0?6:12));
        for(unsigned i=0;i<output.size();++i) {
            const bool background=opacity>0 && i<6;
            assert(output[i].tex_coord0_w==(background?MagicMeter::BackgroundMarker:0.f));
            assert(Near(output[i].color[3],.35f));
            if(background)assert(output[i].view[1]==opacity);
        }
        auto faded=input;for(auto& v:faded)v.color[3]=0;
        output.clear();assert(MagicMeter::Append(std::span<const Vertex>{faded},l,output,Mix));
        for(const auto& v:output)assert(v.color[3]==0);
    }
    // The contour follows both meter sizes and is independent of fill level.
    for(float width:{82.f,164.f})for(float fill:{0.f,.5f,1.f})for(bool resize:{false,true}) {
        l={};
        if(resize){l.magic_width=1.5f;l.magic_height=3;l.magic_background_opacity=0;}
        const auto input=Native(width,fill,.01f,true);
        std::vector<Vertex> plain;
        if(!MagicMeter::Append(std::span<const Vertex>{input},l,plain,Mix))plain=input;
        auto& contour=l.magic_contour;
        contour={.enabled=true,.width=2,.height=3,.opacity=.8f,.x=5,.y=-4,.softness=.5f,.color={.2f,.4f,.6f}};
        std::vector<Vertex> result;
        assert(MagicMeter::Append(std::span<const Vertex>{input},l,result,Mix));
        assert(result.size()==6+plain.size() && std::equal(plain.begin(),plain.end(),result.begin()+6));
        for(unsigned i=0;i<6;++i) {
            const auto& v=result[i];
            assert(v.tex_coord0_w==-4 && std::equal(contour.color.begin(),contour.color.end(),v.color.begin()));
            assert(Near(v.color[3],.35f*.8f) && v.view[0]==0 && v.view[1]==.5f);
        }
        assert(Near(result[0].position[0],.4f+(-4.f-3)/120));
        assert(Near(result[0].position[1],-.7f-(5.f-2)/200));
        assert(Near(result[5].position[0],.4f+.01f*(4+4*l.magic_height)+(-4.f+3)/120));
        assert(Near(result[5].position[1],-.7f-.01f*width*l.magic_width-(5.f+2)/200));
        for(float thickness:{.25f,1.f,8.f})for(float roundness:{.3f,1.f}) {
            contour.width=contour.height=thickness;l.magic_border_roundness=roundness;
            auto tinted=input;
            for(unsigned i=18;i<24;++i)tinted[i].tex_coord0_w=-2;
            result.clear();
            assert(MagicMeter::Append(std::span<const Vertex>{tinted},l,result,Mix));
            assert(result.size()==6+plain.size());
            const auto& border=result[0];
            const auto half=border.tex_coord2;
            const float radius=roundness*(1+std::min(half[0],half[1]));
            assert(Near(border.view[0],radius));
            assert(Near(border.tex_coord1[0],-half[0]-1) && Near(border.tex_coord1[1],-half[1]-1));
            for(unsigned i=6;i<result.size();++i) {
                const auto& v=result[i];
                const bool green=plain[i-6].tex_coord0_w!=MagicMeter::BackgroundMarker;
                assert(v.position==plain[i-6].position && v.uv==plain[i-6].uv);
                if(radius<=1)assert(v.tex_coord0_w==(green?-2.f:MagicMeter::BackgroundMarker));
                else {
                    assert(v.tex_coord2==half && Near(v.tex_coord0_w,green?-(5+radius):MagicMeter::BackgroundMarker));
                    if(!green)assert(Near(v.view[0],radius-1));
                    assert(Near(v.tex_coord1[0],(-.7f-v.position[1])*200/thickness-half[0]));
                    assert(Near(v.tex_coord1[1],(v.position[0]-.4f)*120/thickness-half[1]));
                }
            }
        }
        // Shadow-only and bordered bars use the same silhouette at every fill level.
        for(bool bordered:{false,true})for(float softness:{0.f,4.f,12.f}) {
            contour.enabled=bordered;contour.width=contour.height=2;
            l.magic_border_roundness=.75f;
            auto& shadow=l.magic_shadow;
            shadow={.enabled=false,.x=-4,.y=5,.blur=softness,.strength=.4f};
            std::vector<Vertex> foreground;
            if(!MagicMeter::Append(std::span<const Vertex>{input},l,foreground,Mix))foreground=input;
            shadow.enabled=true;
            result.clear();
            assert(MagicMeter::Append(std::span<const Vertex>{input},l,result,Mix));
            assert(result.size()==foreground.size()+6);
            assert(std::equal(foreground.begin(),foreground.end(),result.begin()+6));
            const float unit=bordered?contour.width:1;
            const float half_width=width*l.magic_width*.01f*200/(2*unit)+(bordered?1:0);
            const float half_height=(4*l.magic_height+4*l.magic_border)*.01f*120/(2*unit)+(bordered?1:0);
            for(unsigned j=0;j<6;++j) {
                const auto& v=result[j];
                assert(v.tex_coord0_w==-5 && v.color[0]==0 && v.color[1]==0 && v.color[2]==0);
                assert(Near(v.color[3],.35f*shadow.strength));
                assert(Near(v.tex_coord2[0],half_width) && Near(v.tex_coord2[1],half_height));
                assert(Near(v.view[0],bordered?.75f*std::min(half_width,half_height):0));
                assert(Near(v.view[1],softness/unit));
            }
            // The transparent feather extends either side of the silhouette boundary.
            const auto& v=result[0];
            assert(Near(v.position[0],.4f+((bordered?contour.y-contour.height:0)+shadow.y-softness)/120));
            assert(Near(v.position[1],-.7f-((bordered?contour.x-contour.width:0)+shadow.x-softness)/200));
            shadow.strength=0;
            result.clear();
            if(!MagicMeter::Append(std::span<const Vertex>{input},l,result,Mix))result=input;
            assert(result==foreground);
        }
        l.magic_shadow.strength=.6f;
        auto hidden=input;
        for(auto& v:hidden)v.color[3]=0;
        result.clear();
        assert(MagicMeter::Append(std::span<const Vertex>{hidden},l,result,Mix));
        assert(result.size()==plain.size());
    }
    l={};l.magic_background_opacity=std::numeric_limits<float>::quiet_NaN();assert(!MagicMeter::Valid(l));
    l.magic_background_opacity=-.1f;assert(!MagicMeter::Valid(l));
    l.magic_background_opacity=1.1f;assert(!MagicMeter::Valid(l));
    l={};l.magic_height=2;
    auto hidden=Native(82,0,1,false);
    for(auto& v:hidden)v.position={0,0,0,1};
    std::vector<Vertex> out;
    assert(!MagicMeter::Append(std::span<const Vertex>{hidden},l,out,Mix) && out.empty());
    auto wrong=Native(100,1,1,false);
    assert(!MagicMeter::Append(std::span<const Vertex>{wrong},l,out,Mix) && out.empty());
    // Default settings also discard all three textured rim tiles.
    auto native=Native(82,1,1,false);
    assert(MagicMeter::Append(std::span<const Vertex>{native},{},out,Mix) && out.size()==12);
    std::cout<<"Magic frame, rounded border, drop shadow, fill ratios, visibility, projection and native alpha passed\n";
}
