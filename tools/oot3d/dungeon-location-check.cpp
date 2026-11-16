// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/dungeon_location.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace OOT3D;
std::vector<std::uint8_t> Read(const std::string& path) {
    std::ifstream f(path,std::ios::binary);assert(f);return {(std::istreambuf_iterator<char>(f)),{}};
}
int main(int argc,char** argv) {
    assert(argc==2);const std::string root=argv[1];
    namespace D=DungeonLocation;
    auto image=std::make_shared<D::Assets>();
    const auto paper=Read(root+"/menu/01_US_ENGLISH/hud_menu_title00.ctxb");
    assert(D::LoadPaper(*image,paper));
    assert(!D::LoadPaper(*image,std::span(paper).first(72)));
    for(unsigned i=0;i<image->paper.size();++i)assert(image->paper[i]==MapFrame::LE32(paper,72+4*i));
    for(unsigned i=0;i<D::Archives.size();++i) {
        const auto path=D::Archives[i];
        const auto b=Read(root+"/scene/"+std::string(path.begin(),path.end()));
        assert(D::LoadTitle(*image,i,b));assert(!D::LoadTitle(*image,i,std::span(b).first(24)));
        unsigned visible=0;
        for(unsigned y=0;y<32;++y)for(unsigned x=0;x<256;++x)
            visible+=(image->titles[D::Tiled(x,i*32+y)]&255)>0;
        assert(visible>100);
    }
    assert(image->available==4095);
    assert(D::TitleIndex(0)==0 && D::TitleIndex(11)==10 && D::TitleIndex(13)==11);
    assert(D::TitleIndex(17)==0 && D::TitleIndex(23)==7 && D::TitleIndex(24)==6 && !D::TitleIndex(0x55));
    PauseMenu::Layout l;
    const auto p=D::Paper(l,1,0);assert(p.count==3);
    assert(p.positions[0]==56 && p.positions[1]==168 && p.positions[15]==344);
    // The native 288px parchment is made of two mirrored 144px halves.
    assert(p.uvs[0]==p.uvs[10] && p.uvs[2]==p.uvs[8]);
    for(unsigned i=0;i<D::Archives.size();++i) {
        const auto t=D::Title(l,i,1,0);assert(t.count==1);
        assert(unsigned((1-t.uvs[1])*D::Height)==i*32);
        assert(unsigned((1-t.uvs[5])*D::Height)==i*32+31);
        assert(t.positions[0]==72 && t.positions[1]==186);
    }
    auto edited=l;edited[PauseMenu::DungeonLocationPaper]={23,-14,.7f};
    const auto moved=D::Paper(edited,.25f,-8);
    assert(std::abs(moved.positions[0]-(200+(56-200)*.7f+23))<.001f);
    assert(std::abs(moved.positions[1]-(200+(168-200)*.7f-14-8))<.001f);
    assert(moved.colors[3]==.25f);
    assert(D::Title(l,0,1,0).positions==D::Title(edited,0,1,0).positions);
    const auto paper_before=D::Paper(edited,1,0);
    edited[PauseMenu::DungeonLocationTitle]={-7,9,1.2f};
    assert(D::Paper(edited,1,0).positions==paper_before.positions);
    assert(D::Title(edited,0,.3f,-8).colors[3]==.3f);
    assert(D::Paper(l,0,0).colors[3]==0 && D::Title(l,0,0,0).colors[3]==0);
    std::cout<<"Dungeon location: native parchment, all 12 entry titles, bounds, scene mapping, independent poses and fading passed\n";
}
