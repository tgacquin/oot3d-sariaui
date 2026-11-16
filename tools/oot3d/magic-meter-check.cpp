// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/magic_meter.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace OOT3D;
struct Vertex {
    std::array<float,4> position{},color{};
    std::array<float,2> uv{};
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
    return v;
}
std::vector<Vertex> Native(float width,float fraction,float scale,bool rotated) {
    std::vector<Vertex> out;
    const auto patches=MagicMeter::Patches(width,fraction,{});
    for(unsigned q=0;q<4;++q) {
        const float x0=q==3?2:patches[q*3].x0;
        const float x1=q==3?2+(width-4)*fraction:patches[q*3].x1;
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
    // Fill only: half-frame drain/refill, empty endpoints, projection changes,
    // hide/fade, missed frames and savestate generations.
    for(float width:{82.f,164.f})for(bool rotated:{false,true}) {
        MagicMeter::History<Vertex> history;
        const auto same=[](const Vertex& a,const Vertex& b){return a.color==b.color && a.uv==b.uv;};
        auto first=Native(width,1,.001f,rotated);
        assert(!history.Apply(std::span<Vertex>{first},1,33333,0,same));
        unsigned frame=1;float previous=1;
        for(float amount:{.7f,.2f,0.f,.3f,.8f,1.f}) {
            auto next=Native(width,amount,.001f,rotated);const auto native=next;
            ++frame;assert(history.Apply(std::span<Vertex>{next},frame,frame*33333,0,same));
            const auto expected=Native(width,(previous+amount)/2,.001f,rotated);
            for(unsigned i=0;i<24;++i)for(unsigned c=0;c<4;++c)
                assert(Near(next[i].position[c],expected[i].position[c]));
            for(unsigned i=0;i<24;++i)assert(same(next[i],native[i]));
            previous=amount;
        }
        auto shifted=Native(width,.5f,.001f,rotated);
        for(auto& v:shifted)v.position[0]+=.1f;
        ++frame;assert(!history.Apply(std::span<Vertex>{shifted},frame,frame*33333,0,same));
        auto faded=shifted;for(auto& v:faded)v.color[3]=0;
        ++frame;assert(!history.Apply(std::span<Vertex>{faded},frame,frame*33333,0,same));
        frame+=3;assert(!history.Apply(std::span<Vertex>{faded},frame,frame*33333,0,same));
        ++frame;assert(!history.Apply(std::span<Vertex>{faded},frame,frame*33333,1,same));
    }
    HUD::Layout l;
    assert(!MagicMeter::Changed(l));
    l.magic_height=std::numeric_limits<float>::quiet_NaN();
    assert(!MagicMeter::Changed(l));
    l={};l.magic_height=2;
    for(float width:{82.f,164.f})for(float fraction:{0.f,.25f,1.f})
    for(float scale:{.001f,.5f,2.f})for(bool rotated:{false,true})
    for(float height:{1.f,2.f,8.f})for(float border:{1.f,2.f,4.f})
    for(float width_scale:{.5f,1.f,2.f,4.f}) {
        l.magic_height=height;l.magic_border=border;l.magic_width=width_scale;
        if(!MagicMeter::Changed(l))continue;
        const auto native=Native(width,fraction,scale,rotated);
        auto input=native;
        std::vector<Vertex> output;
        assert(MagicMeter::Append(std::span<const Vertex>{input},l,output,Mix));
        assert(input==native);
        assert(output.size()==(fraction==0?54:60));
        const auto coord=[&](const Vertex& v,bool y) {
            if(rotated)return y?(v.position[0]-.4f)/scale:(-.7f-v.position[1])/scale;
            return y?(v.position[1]+.7f)/scale:(v.position[0]-.4f)/scale;
        };
        // Top/left edges stay fixed; width preserves the native fill fraction.
        assert(Near(coord(output[0],false),0) && Near(coord(output[0],true),0));
        assert(Near(coord(output[53],false),width*width_scale));
        assert(Near(coord(output[53],true),4*border+4*height));
        // Border strips preserve their source rows, independent of new height.
        assert(Near(coord(output[1],true),2*border));
        assert(Near(coord(output[13],true),4*border+4*height));
        assert(Near(output[1].uv[1],.75f));
        assert(Near(output[12].uv[1],.25f));
        if(fraction>0) {
            assert(Near(coord(output[54],false),2*border));
            assert(Near(coord(output[54],true),2*border));
            assert(Near(coord(output[59],false),2*border+(width*width_scale-4*border)*fraction));
            assert(Near(coord(output[59],true),2*border+4*height));
        }
        for(const auto& v:output) {
            assert(Near(v.color[3],.35f) && Near(v.position[2],.2f) && v.position[3]==1);
        }
    }
    // Opacity affects the entire black interior, including the one-pixel
    // padding around the green fill. Its white rim and green fill retain the
    // original fading alpha at every border/height setting.
    for(float opacity:{0.f,.25f,.75f})for(float width:{82.f,164.f})
    for(float height:{1.f,3.f})for(float border:{1.f,4.f}) {
        l={};l.magic_background_opacity=opacity;l.magic_height=height;l.magic_border=border;
        const auto input=Native(width,.5f,1,false);std::vector<Vertex> output;
        assert(MagicMeter::Append(std::span<const Vertex>{input},l,output,Mix));
        assert(output.size()==(opacity==0?102:156));
        unsigned background_vertices=0,fill_vertices=0;
        for(unsigned q=0;q<output.size()/6;++q) {
            const auto* v=output.data()+q*6;
            const float x=(v[0].position[0]+v[5].position[0])/2-.4f;
            const float y=(v[0].position[1]+v[5].position[1])/2+.7f;
            const bool fill=v[0].uv[0]>=3;
            const bool black=!fill && x>border && x<width-border && y>border && y<3*border+4*height;
            if(black)background_vertices+=6;
            if(fill)fill_vertices+=6;
            for(unsigned i=0;i<6;++i) {
                assert(Near(v[i].color[3],.35f*(black?opacity:1)));
                assert(Near(v[i].color[0],.8f) && Near(v[i].color[1],.9f));
            }
        }
        assert(fill_vertices==6 && background_vertices==(opacity==0?0:54));
        auto faded=input;for(auto& v:faded)v.color[3]=0;
        output.clear();assert(MagicMeter::Append(std::span<const Vertex>{faded},l,output,Mix));
        for(const auto& v:output)assert(v.color[3]==0);
    }
    l={};l.magic_background_opacity=std::numeric_limits<float>::quiet_NaN();assert(!MagicMeter::Changed(l));
    l.magic_background_opacity=-.1f;assert(!MagicMeter::Changed(l));
    l.magic_background_opacity=1.1f;assert(!MagicMeter::Changed(l));
    l={};l.magic_height=2;
    auto hidden=Native(82,0,1,false);
    for(auto& v:hidden)v.position={0,0,0,1};
    std::vector<Vertex> out;
    assert(!MagicMeter::Append(std::span<const Vertex>{hidden},l,out,Mix) && out.empty());
    auto wrong=Native(100,1,1,false);
    assert(!MagicMeter::Append(std::span<const Vertex>{wrong},l,out,Mix) && out.empty());
    // Settings at 1x preserve the unmodified native rendering path.
    auto native=Native(82,1,1,false);
    assert(!MagicMeter::Append(std::span<const Vertex>{native},{},out,Mix) && out.empty());
    std::cout<<"Magic height, frame thickness, black interior opacity, fill ratios, hidden state, rotated/scaled projection and native alpha passed\n";
}
