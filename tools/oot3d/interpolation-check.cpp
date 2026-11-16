// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/interpolation.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace Interpolation;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void Put(U32 a,const Matrix& m) {
        for(unsigned i=0;i<12;++i)words[a+4*i]=std::bit_cast<U32>(float(m[i/4][i%4]));
    }
};
bool Near(double a,double b) {return std::abs(a-b)<.00001;}
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
    for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(CodeMatches(m) && Instruction(Site.pc,Site.instruction)==(0xef000000|FirstSVC));
    auto first=Math::Identity(),second=first;
    second[0][3]=20;
    auto half=Math::Blend(first,second,.5);assert(Near(half[0][3],10) && Math::Rigid(half));
    assert(Math::Near(Math::Blend(first,second,0),first,.00001));
    assert(Math::Near(Math::Blend(first,second,1),second,.00001));
    auto rotated=Math::Identity();rotated[0][0]=rotated[2][2]=0;rotated[0][2]=1;rotated[2][0]=-1;
    auto rotation_half=Math::Blend(first,rotated,.5);
    assert(Math::Rigid(rotation_half) && Near(rotation_half[0][0],std::sqrt(.5)));
    History h;
    assert(h.Observe(first,1,1,100000,250,45)==Status::Waiting);
    assert(h.Observe(Math::InverseRigid(second),1,1,133333,250,45)==Status::Ready);
    assert(Near(h.View(.5)[0][3],-10));
    assert(h.Observe(Math::InverseRigid(second),1,1,133334,250,45)==Status::Ready);
    assert(Near(h.View(.5)[0][3],-10)); // Duplicate draw cannot consume the pair.
    assert(h.Observe(Math::InverseRigid(second),1,2,166666,250,45)==Status::Gap);
    assert(h.Observe(Math::InverseRigid(second),2,2,199999,250,45)==Status::Gap);
    assert(h.Observe(first,2,2,400000,250,45)==Status::Gap);
    assert(h.Observe(first,2,2,300000,250,45)==Status::Gap); // State rewind.
    assert(h.Observe(rotated,2,2,333333,250,45)==Status::Cut);
    auto invalid=first;invalid[0][0]=2;
    assert(h.Observe(invalid,2,2,366666,250,45)==Status::Invalid && !h.ready);
    h.Observe(first,1,1,100000,250,45);second[0][3]=300;
    assert(h.Observe(Math::InverseRigid(second),1,1,133333,250,45)==Status::Cut);

    constexpr U32 play=0x8100000,view=play+0x188,temp=0x8200000,renderer=0x5be5b8;
    m.words[PauseMenu::UI+0xc]=play;m.words[PauseMenu::UI+0x24]=0;
    m.words[play+0x100]=0x0203;m.words[play+0x104]=4;
    m.words[play+0x7f24]=0x100;m.words[play+0x5c2c]=m.words[play+0x7f10]=0;
    m.words[HUDFade::Main+0x34]=2;
    m.words[view]=0;m.words[view+4]=400;m.words[view+8]=0;m.words[view+12]=240;
    m.words[renderer+0xfc]=0;
    std::array<U32,16> r{};r[0]=renderer;r[4]=view;
    Hook hook;enabled=true;preview=true;present_60=false;phase=.5f;
    first[2][3]=-100;second=first;second[0][3]=-20;
    m.Put(view+0x114,first);hook.Capture(m,r,100000,true);
    assert(m.Read(renderer+0xfc)==view && status==Status::Waiting);
    m.Put(view+0x114,second);hook.Capture(m,r,133333,true);
    assert(status==Status::Ready);
    assert(!CameraSnapshot().projections_valid);
    for(unsigned lens=0;lens<3;++lens) {
        auto p=Math::Identity();p[0][0]=2.+lens;p[1][1]=3.+lens;
        p[2][3]=5;p[3][2]=-1;p[3][3]=0;
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
            m.words[view+0x54+lens*0x40+(row*4+col)*4]=std::bit_cast<U32>(float(p[row][col]));
    }
    auto attached=Math::Identity();attached[2][3]=-120;m.Put(view+0x144,attached);
    const auto before_capture=m.words;
    hook.Capture(m,r,133334,true);
    assert(m.words==before_capture && CameraSnapshot().projections_valid && CameraSnapshot().attached_valid);
    const auto captured=CameraSnapshot();
    assert(captured.attached_view==attached);
    for(unsigned lens=0;lens<3;++lens) {
        assert(captured.projections[lens][0][1]==3.+lens);
        assert(captured.projections[lens][1][0]==-2.-lens);
        assert(captured.projections[lens][3][2]==-1);
    }
    // Read-only capture classifies only the main camera's first-person modes.
    m.words[play+0xa64]=0;
    for(unsigned mode=0;mode<21;++mode) {
        m.words[play+0x364+0x18c]=mode;
        const auto before=m.words;hook.Capture(m,r,133334,true);
        assert(m.words==before);
        assert(CameraSnapshot().first_person==(mode==6 || mode==7 || mode==11));
    }
    m.words[play+0xa64]=1;m.words[play+0x364+0x18c]=7;
    hook.Capture(m,r,133334,true);assert(!CameraSnapshot().first_person);
    m.words[play+0xa64]=0;m.words[play+0x364+0x18c]=0;
    m.Put(temp,second);const auto native=m.words;
    assert(hook.Apply(m,temp,133334,true));
    assert(Near(std::bit_cast<float>(*m.Read(temp+12)),-10));
    for(const auto& [at,value]:native)if(at<temp || at>=temp+48)assert(m.Read(at)==value);
    // Orthographic/HUD and auxiliary views do not inherit the world preview.
    m.Put(temp,Math::Identity());assert(!hook.Apply(m,temp,133334,true));
    m.Put(temp,second);m.words[renderer+0xfc]=0x8300000;assert(!hook.Apply(m,temp,133334,true));
    m.words[renderer+0xfc]=view;
    assert(!hook.Apply(m,temp,233334,true));
    assert(!hook.Apply(m,temp,133334,false));
    preview=false;assert(!hook.Apply(m,temp,133334,true));preview=true;
    m.words[play+0x7f10]=0x10000;assert(!hook.Apply(m,temp,133334,true));m.words[play+0x7f10]=0;
    ++generation;assert(!hook.Apply(m,temp,133334,true));
    // HUD/input suspension must not disable read-only world replay through a
    // scene fade. The legacy preview keeps its stricter gameplay-only gate.
    present_60=true;preview=false;Hook transition;
    m.words[SaveMenu::Owner+0x28]=0;
    m.Put(view+0x114,first);transition.Capture(m,r,300000,true);
    m.Put(view+0x114,second);transition.Capture(m,r,333333,true);
    assert(status==Status::Ready && CameraSnapshot().valid);
    std::int64_t now=333333;
    for(auto [address,value]:std::array<std::pair<U32,U32>,4>{{
            {play+0x5c2c,0x100},{play+0x7f10,0x10000},
            {play+0x7f24,0},{HUDFade::Main+0x34,0}}}) {
        const auto old=m.words[address];m.words[address]=value;
        assert(!transition.Gameplay(m));
        now+=33333;second[0][3]-=1;m.Put(view+0x114,second);
        const auto untouched=m.words;
        transition.Capture(m,r,now,true);
        assert(status==Status::Ready && CameraSnapshot().valid && m.words==untouched);
        m.words[address]=old;
    }
    // The first frame in a genuinely different scene cannot blend with the
    // outgoing scene; the next adjacent frame can resume normally.
    m.words[play+0x104]=5;now+=33333;transition.Capture(m,r,now,true);
    assert(status==Status::Gap && !transition.history.ready && CameraSnapshot().scene==5);
    now+=33333;transition.Capture(m,r,now,true);assert(status==Status::Ready);
    // Keep menu previews, non-play contexts and unsupported presentation out.
    m.words[PauseMenu::UI+0x14]=1;m.words[PauseMenu::UI+0x20]=0;m.words[PauseMenu::UI+0x24]=1;
    now+=33333;transition.Capture(m,r,now,true);assert(!CameraSnapshot().valid);
    m.words[PauseMenu::UI+0x24]=0;m.words[SaveMenu::Owner+0x28]=1;
    now+=33333;transition.Capture(m,r,now,true);assert(!CameraSnapshot().valid);
    m.words[SaveMenu::Owner+0x28]=0;m.words[play+0x100]=4;
    now+=33333;transition.Capture(m,r,now,true);assert(!CameraSnapshot().valid);
    m.words[play+0x100]=0x0203;
    now+=33333;transition.Capture(m,r,now,false);assert(!CameraSnapshot().valid);
    present_60=false;m.words[play+0x7f10]=0x10000;
    now+=33333;transition.Capture(m,r,now,true);assert(!CameraSnapshot().valid);
    m.words[play+0x7f10]=0;
    enabled=false;hook.Capture(m,r,166666,true);assert(status==Status::Off && !hook.history.seen);
    m.words[play+0x104]=0x55;
    hook.Capture(m,r,199999,true);assert(native_scene==0x55 && !CameraSnapshot().valid);
    hook.Capture(m,r,233332,false);assert(native_scene==~0u);
    std::cout<<"Interpolation: guarded native capture, rigid midpoint/endpoints, cuts, scene changes, rewind, duplicate draws, HUD exclusion and temporary-only preview passed\n";
}
