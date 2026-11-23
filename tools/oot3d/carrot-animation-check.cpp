// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/carrot_animation.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
struct Memory {
    std::map<std::uint32_t,std::uint32_t> words;
    std::optional<std::uint32_t> Read(std::uint32_t at) {
        auto i=words.find(at);return i==words.end()?std::nullopt:std::optional{i->second};
    }
};
struct Vertex {
    std::array<float,4> position{},color{};
    std::array<float,2> tex_coord0{},tex_coord1{},tex_coord2{};
    float tex_coord0_w=0;
    bool operator==(const Vertex&) const=default;
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);
    std::uint32_t word;for(std::uint32_t at=0x100000;file.read(reinterpret_cast<char*>(&word),4);at+=4)m.words[at]=word;
    assert(m.Read(0x42df88)==0x12848b0a && m.Read(0x42df8c)==0x12888fe9);
    constexpr auto play=0x8000000u,player=0x8100000u,horse=0x8200000u;
    m.words[HUDFade::UI+12]=play;m.words[play+0x20ac]=player;m.words[player+0x124]=horse;
    m.words[play+0x100]=0x203;m.words[play+0x7f24]=0x100;m.words[HUDFade::Main+0x34]=1;
    m.words[play+0x5c2c]=0;m.words[play+0x7f10]=0;m.words[HUDFade::Action+0x48]=5;
    m.words[player+0x1708]=1;m.words[play+0x2ba4+0x274]=6;
    auto before=m.words;CarrotAnimation::Publish(m,false);auto in=CarrotAnimation::Get();
    assert(in.valid && in.visible && in.count==6 && m.words==before);
    m.words[player+0x1708]=CarrotAnimation::DismountAction;
    CarrotAnimation::Publish(m,false);assert(!CarrotAnimation::Get().visible); // HUD still mode 5.
    CarrotAnimation::Publish(m,true);assert(!CarrotAnimation::Get().valid);
    std::array<Vertex,36> mesh{};
    for(unsigned i=0;i<36;++i) {mesh[i].position={float(i%6)*.01f,float(i/6)*.1f,0,1};mesh[i].color={1,1,1,1};}
    const auto original=mesh;
    CarrotAnimation::History<Vertex> h;std::vector<Vertex> out;
    auto update=[&](std::int64_t t) {assert(h.Update(mesh,in,t,in.frame,in.generation));};
    auto render=[&](std::int64_t t) {out.clear();h.Append(out,t);};
    update(1000000);render(1000000);assert(out.empty());
    update(1075000);render(1075000);assert(out.size()==36 && out[0].color[3]==.5f);
    update(1150000);render(1150000);assert(out.size()==36 && std::equal(out.begin(),out.end(),original.begin()));
    in.count=5;for(unsigned j=30;j<36;++j)mesh[j].tex_coord0[1]=1;
    update(1180000);render(1180000);assert(out.size()==36 && out[30].tex_coord0[1]==1);
    update(1260000);render(1260000);assert(out.size()==36);
    assert(out[0]==original[0] && out[30].color[3]==1);
    assert(out[30].position[0]>mesh[30].position[0] && out[30].tex_coord0[1]==1);
    for(unsigned j=30;j<36;++j) {
        assert(std::abs(out[j].position[0]-mesh[j].position[0]-3.f/120)<1e-6f);
        for(unsigned axis=1;axis<4;++axis)assert(out[j].position[axis]==mesh[j].position[axis]);
        assert(out[j].color==mesh[j].color); // Pure translation, constant size and alpha.
    }
    auto same=out;update(1260000);render(1260000);assert(out==same);
    in.count=6;mesh=original;update(1280000);render(1280000);assert(out.size()==36 && out[30]==original[30]);
    in.visible=false;mesh={};update(1300000);render(1375000);
    assert(out.size()==36 && out[0].position==original[0].position && out[0].color[3]==.5f);
    render(1450000);assert(out.empty());
    in.visible=true;mesh=original;update(1400000);render(1400000);assert(out[0].color[3]>0 && out[0].color[3]<1);
    ++in.generation;update(1430000);render(1430000);assert(out.empty());
    assert(!h.Update(mesh,in,1460000,in.frame+2,in.generation));
    assert(mesh==original);
    h.Reset();in.count=6;in.visible=true;mesh=original;
    update(2000000);update(2150000);
    in.count=5;for(unsigned j=30;j<36;++j)mesh[j].tex_coord0[1]=1;
    assert(h.Update(mesh,in,2180000,in.frame,in.generation,.5f));
    render(2679999);assert(out.size()==36 && out[30]==original[30]);
    render(2760000);assert(out.size()==36 && out[30].color[3]==1 && out[30].position[0]>original[30].position[0]);
    render(2840000);assert(out.size()==36 && out[30].tex_coord0[1]==1);
    std::array<Vertex,6> cell{};
    for(unsigned j=0;j<6;++j)cell[j].tex_coord0={float(j%2)*24/512,float(j/3)*24/256};
    const auto uv=CarrotAnimation::InsetUV(cell,0);
    assert(std::abs(uv[0]-.5f/512)<1e-7f && std::abs(uv[1]-.5f/256)<1e-7f);
    assert(CarrotAnimation::DelayUs(-1)==0 && CarrotAnimation::DelayUs(3)==2000000);
    // Shadow copies preserve the sprite mask and animation, precede every
    // colored sprite, and leave the original geometry/colors untouched.
    h.Reset();in.visible=true;in.count=6;mesh=original;
    update(3000000);update(3150000);render(3150000);const auto foreground=out;
    OOT3D::HUD::CarrotShadow shadow{true,3,2,.6f};
    out.clear();h.Append(out,3150000,shadow);assert(out.size()==72);
    assert(std::equal(foreground.begin(),foreground.end(),out.begin()+36));
    for(unsigned j=0;j<36;++j) {
        assert(out[j].tex_coord0_w==-1 && out[j].color[0]==0 && out[j].color[1]==0 && out[j].color[2]==0);
        assert(std::abs(out[j].color[3]-.6f)<1e-6f && out[j].tex_coord0==foreground[j].tex_coord0);
        assert(std::abs(out[j].position[0]-(foreground[j].position[0]-2.f/120))<1e-6f);
        assert(std::abs(out[j].position[1]-(foreground[j].position[1]-3.f/200))<1e-6f);
    }
    shadow.opacity=0;out.clear();h.Append(out,3150000,shadow);assert(out==foreground);
    shadow.opacity=.8f;shadow.enabled=false;out.clear();h.Append(out,3150000,shadow);assert(out==foreground);
    assert(mesh==original);
    in.count=5;for(unsigned j=30;j<36;++j)mesh[j].tex_coord0[1]=1;
    update(3180000);shadow={true,-2,-3,.8f};
    out.assign(1,original[0]);h.Append(out,3260000,shadow,.5f);
    assert(out.size()==73 && out.front()==original[0]);
    for(unsigned j=0;j<36;++j) {
        const auto& back=out[1+j];const auto& sprite=out[37+j];
        assert(back.tex_coord0==sprite.tex_coord0 && sprite.tex_coord0==mesh[j].tex_coord0);
        assert(std::abs(back.position[0]-sprite.position[0]-3.f/120)<1e-6f);
        assert(std::abs(back.position[1]-sprite.position[1]-2.f/200)<1e-6f);
        assert(sprite.color[3]==.5f && back.color[3]==.4f);
    }
    std::cout<<"Carrot native guards, early dismount, fades, spend bounce/UV transition, refill and reset checks passed\n";
}
