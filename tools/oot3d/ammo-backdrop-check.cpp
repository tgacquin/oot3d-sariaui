// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/ammo_backdrop.h"
#include "oot3d/heart_contour.h"
#include "oot3d/magic_meter.h"
#include <cassert>
#include <iostream>

struct Vertex {
    std::array<float,4> position{},color{};
    std::array<float,2> tex_coord0{};
    float tex_coord0_w{};
    std::array<float,3> view{};
    std::array<float,2> tex_coord2{},tex_coord1{};
    bool operator==(const Vertex&) const=default;
};
int main() {
    {
        OOT3D::HUD::Layout l;l.hd_numbers=l.hd_rupee_numbers=true;
        l.hd_rupee_backdrop.enabled=true;
        assert(OOT3D::AmmoBackdrop::Enabled(l,true));
        assert(!OOT3D::AmmoBackdrop::Enabled(l));
        l.hd_rupee_numbers=false;assert(!OOT3D::AmmoBackdrop::Enabled(l,true));
        l.hd_rupee_numbers=true;l.hd_numbers=false;assert(!OOT3D::AmmoBackdrop::Enabled(l,true));
    }

    using namespace OOT3D;
    const auto digit=[](float x,float alpha) {
        std::vector<Vertex> out;
        for(auto p:std::array<std::array<float,2>,6>{{{0,0},{.1f,0},{0,.05f},{0,.05f},{.1f,0},{.1f,.05f}}})
            out.push_back({{p[0],x+p[1],.2f,1},{1,1,1,alpha},{p[0],p[1]}});
        return out;
    };
    {
        for(bool defense:{false,true})for(unsigned fill=0;fill<5;++fill) {
            std::vector<Vertex> source(21*6);
            const auto heart=digit(.2f,1);
            std::copy(heart.begin(),heart.end(),source.begin());
            std::copy(heart.begin(),heart.end(),source.begin()+120); // Next HUD element.
            const float row=1-((defense?0:80)+16*fill)/256.f;
            for(unsigned i=0;i<6;++i)source[i].tex_coord0[1]=row-source[i].tex_coord0[1];
            auto plain=source;
            assert(!HUD::PrepareHeartContours(plain,0,{}) && plain==source);
            for(bool use_defense:{false,true})for(bool shadow:{false,true}) {
                HUD::HeartContours layers;
                layers.shadow={.enabled=shadow,.width=2,.height=3,.opacity=.4f,.softness=1};
                layers.outline={.enabled=true,.x=1,.y=-1};
                layers.double_defense.enabled=use_defense;
                // Fade out and reverse back in. The entire heart always uses
                // one quad and the same source alpha, including its last frame.
                for(float alpha:{1.f,.5f,.1f,0.f,.1f,.5f,1.f}) {
                    auto output=source;
                    for(unsigned i=0;i<6;++i)output[i].color[3]=alpha;
                    const bool styled=shadow || !defense || use_defense;
                    assert(HUD::PrepareHeartContours(output,0,layers)==styled);
                    assert(output.size()==source.size());
                    assert(std::equal(output.begin()+6,output.end(),source.begin()+6));
                    for(unsigned i=0;i<6;++i) {
                        const auto& v=output[i];
                        assert(v.color[3]==alpha);
                        if(!styled) {assert(v.position==source[i].position);continue;}
                        assert(v.tex_coord0_w==(defense?-.25f:-.125f));
                        assert(v.view[2]==1); // U follows the vertical screen axis in this fixture.
                        const float dx=-(v.position[1]-source[i].position[1])*200;
                        const float dy=-(v.position[0]-source[i].position[0])*120;
                        assert(std::abs(v.tex_coord0[0]-source[i].tex_coord0[0]-dy*v.view[0])<1e-5f);
                        const float remap=defense && use_defense?80.f/256:0;
                        assert(std::abs(v.tex_coord0[1]-source[i].tex_coord0[1]+remap-dx*v.view[1])<1e-5f);
                        assert(v.tex_coord1[0]==0 && v.tex_coord2[0]==.1f);
                        assert(v.tex_coord2[1]==row-remap);
                    }
                }
            }
        }
    }
    {
        HUD::Layout layout;
        layout.rupee_contour.enabled=true;
        layout.rupee_contour.x=5;layout.rupee_contour.y=-3;
        for(float alpha:{0.f,.1f,.5f,1.f,.5f,.1f,0.f}) {
            auto gem=digit(.2f,alpha);
            const auto source=gem;
            assert(HUD::PrepareRupeeContour(gem.data(),layout.rupee_contour));
            for(unsigned i=0;i<6;++i) {
                assert(gem[i].color==source[i].color && gem[i].tex_coord0_w==-.375f);
                const float dx=-(gem[i].position[1]-source[i].position[1])*200;
                const float dy=-(gem[i].position[0]-source[i].position[0])*120;
                assert(std::abs(dx-(source[i].position[1]==.2f?6.f:0.f))<.00001f);
                assert(std::abs(dy-(source[i].position[0]==0?0.f:-4.f))<.00001f);
            }
        }
    }
    {
        constexpr unsigned fill_quad=62,first_heart=63;
        std::vector<Vertex> batch(92*6);
        const auto sprite=digit(.2f,.5f);
        std::copy(sprite.begin(),sprite.end(),batch.begin()+first_heart*6);
        std::copy(sprite.begin(),sprite.end(),batch.begin()+fill_quad*6);
        for(auto& v:batch)v.tex_coord0_w=-3;
        const auto source=batch;
        HUD::Layout hud;
        for(float brightness:{0.f,.5f,1.f,2.f})for(bool recolor:{false,true}) {
            auto styled=source;
            hud.heart_brightness=brightness;
            hud.heart_color=recolor?std::array{.2f,.4f,.9f}:HUD::Layout::NativeHeartColor;
            const bool custom=brightness!=1 || recolor;
            MagicMeter::PrepareHudColors(std::span{styled},fill_quad,hud);
            for(unsigned i=0;i<styled.size();++i) {
                const bool heart=custom && i/6>=first_heart && i/6<first_heart+20;
                auto expected=source[i];
                expected.tex_coord0_w=heart?.5f:0.f;
                if(i/6>=fill_quad-3 && i/6<fill_quad) {
                    expected.tex_coord0_w=MagicMeter::BackgroundMarker;
                    expected.view[0]=0;expected.view[1]=1;
                }
                if(heart){expected.view=hud.heart_color;expected.tex_coord2[0]=brightness;}
                assert(styled[i]==expected);
            }
            if(!custom)continue;
            HUD::HeartContours layers;layers.shadow.enabled=true;
            assert(HUD::PrepareHeartContours(styled,first_heart*6,layers));
            assert(std::count_if(styled.begin(),styled.end(),[](const auto& v){return v.tex_coord0_w==.5f;})==114);
            assert(styled[first_heart*6].tex_coord0_w==-.125f);
        }
        hud={};
        hud.magic_fill_color={.2f,.3f,1};
        hud.heart_contours.shadow.enabled=true;
        MagicMeter::PrepareHudColors(std::span{batch},fill_quad,hud);
        for(unsigned i=0;i<batch.size();++i) {
            const bool fill=i/6==fill_quad;
            const bool background=i/6>=fill_quad-3 && i/6<fill_quad;
            assert(batch[i].tex_coord0_w==(fill?-2.f:background?MagicMeter::BackgroundMarker:0.f));
            if(!background)assert(batch[i].view==(fill?hud.magic_fill_color:source[i].view));
            assert(batch[i].color==source[i].color && batch[i].tex_coord0==source[i].tex_coord0);
        }
        assert(HUD::PrepareHeartContours(batch,first_heart*6,hud.heart_contours));
        assert(std::count_if(batch.begin(),batch.end(),[](auto& v){return v.tex_coord0_w==-2;})==6);
        assert(std::count_if(batch.begin(),batch.end(),[](auto& v){return v.tex_coord0_w==-.125f;})==6);
    }
    auto native=digit(0,.5f);const auto next=digit(.07f,.5f);
    HUD::Layout layout;
    for(bool global:{false,true})for(bool rupee:{false,true})for(bool enabled:{false,true}) {
        layout.hd_numbers=global;
        layout.hd_rupee_numbers=rupee;
        layout.rupee_digit_outline.enabled=enabled;
        assert(layout.OutlineHDRupees()==(global && rupee && enabled));
    }
    for(bool global:{false,true})for(bool timer:{false,true})for(bool enabled:{false,true}) {
        layout.hd_numbers=global;
        layout.hd_timer_numbers=timer;
        layout.hd_rupee_numbers=false;
        layout.rupee_digit_outline.enabled=enabled;
        assert(layout.OutlineHDTimer()==(global && timer && enabled));
    }
    for(bool global:{false,true})for(bool skulltula:{false,true})for(bool enabled:{false,true}) {
        layout.hd_numbers=global;
        layout.hd_skulltula_numbers=skulltula;
        layout.hd_rupee_numbers=layout.hd_timer_numbers=false;
        layout.rupee_digit_outline.enabled=enabled;
        assert(layout.OutlineHDSkulltula()==(global && skulltula && enabled));
    }
    for(unsigned count=1;count<=8;++count) {
        auto batch=digit(-.6f,.3f); // Already-prepended backdrop.
        for(unsigned i=0;i<count;++i) {
            auto face=digit(i*.07f,i==3?0.f:.5f);
            for(auto& v:face)v.tex_coord0={v.tex_coord0[0]>0?12.f/256:0.f,v.tex_coord0[1]>0?21.f/128:0.f};
            batch.insert(batch.end(),face.begin(),face.end());
        }
        const auto before=batch;
        for(float thickness:{.25f,1.f,2.5f,3.f}) {
            auto styled=before;
            layout.rupee_digit_outline.thickness=thickness;
            assert(HUD::PrepareRupeeDigits(styled,6*count,layout.rupee_digit_outline));
            assert(styled.size()==before.size() && std::equal(before.begin(),before.begin()+6,styled.begin()));
            for(unsigned i=6;i<styled.size();++i) {
                assert(styled[i].tex_coord0_w==-.4375f && styled[i].color==before[i].color);
                assert(std::abs(256*std::abs(styled[i].tex_coord0[0]-before[i].tex_coord0[0])-thickness)<.0001f);
                assert(std::abs(128*std::abs(styled[i].tex_coord0[1]-before[i].tex_coord0[1])-thickness)<.0001f);
            }
        }
    }
    native.insert(native.end(),next.begin(),next.end());
    HUD::AmmoBackdrop b;auto out=native;
    assert(!AmmoBackdrop::Prepend(out,b) && out==native);
    b.enabled=true;b.opacity=.4f;
    assert(AmmoBackdrop::Prepend(out,b));
    assert(std::equal(native.begin(),native.end(),out.end()-native.size()));
    float max_alpha=0;
    for(unsigned i=0;i<out.size()-native.size();++i) {
        const auto& v=out[i];max_alpha=std::max(max_alpha,v.color[3]);
        assert(v.color[0]==0 && v.color[1]==0 && v.color[2]==0);
        assert(v.color[3]>=0 && v.color[3]<=.2f);
        assert(v.position[2]==.2f && v.position[3]==1);
        assert(v.tex_coord0[0]==252.f/256 && v.tex_coord0[1]==1-4.f/128);
    }
    assert(std::abs(max_alpha-.2f)<.0001f);
    const auto original=out;b.x=4;b.y=-2;out=native;
    assert(AmmoBackdrop::Prepend(out,b));
    for(unsigned i=0;i<out.size()-native.size();++i) {
        assert(std::abs(out[i].position[0]-original[i].position[0]-3.f/120)<.0001f);
        assert(std::abs(out[i].position[1]-original[i].position[1]+4.f/200)<.0001f);
    }
    out=digit(0,0);const auto invisible=out;
    assert(!AmmoBackdrop::Prepend(out,b) && out==invisible);
    out=native;for(auto& v:out)v.tex_coord0={};
    assert(!AmmoBackdrop::Prepend(out,b));
    out=native;b.opacity=0;assert(!AmmoBackdrop::Prepend(out,b) && out==native);
    HUD::Layout l;l.hd_ammo_backdrop.enabled=true;assert(AmmoBackdrop::Enabled(l));
    l.hd_numbers=false;assert(!AmmoBackdrop::Enabled(l));
    l.hd_numbers=true;l.hd_ammo_numbers=false;assert(!AmmoBackdrop::Enabled(l));
    // Check the maximum supported batch size for a number backdrop.
    native.clear();for(unsigned i=0;i<8;++i) {
        const auto q=digit(-.4f+i*.07f,.35f);native.insert(native.end(),q.begin(),q.end());
    }
    b={true,0,1,1.25f,1.25f,.75f,.6f};out=native;
    assert(AmmoBackdrop::Prepend(out,b) && out.size()==native.size()+32*6*8);
    assert(std::equal(native.begin(),native.end(),out.end()-native.size()));
    for(unsigned i=0;i<32*6*8;++i)assert(out[i].color[3]<=.35f*.6f+.00001f);
    std::cout<<"Number backdrop: whole-counter geometry, visibility, fade, offsets, native digits and toggles passed\n";
}
