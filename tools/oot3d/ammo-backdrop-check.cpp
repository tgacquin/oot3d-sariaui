// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/ammo_backdrop.h"
#include <cassert>
#include <iostream>

struct Vertex {
    std::array<float,4> position{},color{};
    std::array<float,2> tex_coord0{};
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
    auto native=digit(0,.5f);const auto next=digit(.07f,.5f);
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
    l={};l.hd_timer_backdrop.enabled=true;
    assert(AmmoBackdrop::TimerEnabled(l) && !AmmoBackdrop::Enabled(l) && !AmmoBackdrop::Enabled(l,true));
    l.hd_timer_numbers=false;assert(!AmmoBackdrop::TimerEnabled(l));
    l.hd_timer_numbers=true;l.hd_numbers=false;assert(!AmmoBackdrop::TimerEnabled(l));
    // Full eight-quad timer, including punctuation, gets one oval and unchanged digits.
    native.clear();for(unsigned i=0;i<8;++i) {
        const auto q=digit(-.4f+i*.07f,.35f);native.insert(native.end(),q.begin(),q.end());
    }
    b={true,0,1,1.25f,1.25f,.75f,.6f};out=native;
    assert(AmmoBackdrop::Prepend(out,b) && out.size()==native.size()+32*6*8);
    assert(std::equal(native.begin(),native.end(),out.end()-native.size()));
    for(unsigned i=0;i<32*6*8;++i)assert(out[i].color[3]<=.35f*.6f+.00001f);
    std::cout<<"Number backdrop: whole-counter geometry, visibility, fade, offsets, native digits and toggles passed\n";
}
