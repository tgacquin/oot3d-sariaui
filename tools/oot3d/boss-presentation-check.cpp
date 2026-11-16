// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/boss_challenge.h"
#include "oot3d/pause_prompt_assets.h"
#include <cassert>
#include <fstream>
#include <iostream>
std::vector<std::uint8_t> Read(const std::string& path) {
    std::ifstream f(path,std::ios::binary);assert(f);return {(std::istreambuf_iterator<char>(f)),{}};
}
int main(int argc,char** argv) {
    assert(argc==2);const std::string root=argv[1];
    namespace B=OOT3D::BossChallenge;
    namespace S=OOT3D::ShopHint;
    const auto archive=Read(root+"/actor/zelda_keep.zar");
    const auto art=S::MakePixels(archive);assert(art);
    unsigned opaque=0;for(auto p:*art)opaque+=(p&255)>0;assert(opaque>100);
    assert(!S::MakePixels(std::span(archive).first(100)));
    const auto font=Read(root+"/message/us/ltn16.qbf");
    const auto text=OOT3D::FileStatusText::MakeAssets(font);assert(text);
    const auto prompts=OOT3D::PausePrompts::MakeAssets(Read(root+"/message/parts.ctxb"),font);assert(prompts);
    auto titles=std::make_unique<OOT3D::BossTitle::Pixels>();
    for(unsigned i=0;i<8;++i) {
        const auto path=OOT3D::BossTitle::Archives[i];
        const auto data=Read(root+"/actor/"+std::string(path.begin(),path.end()));
        assert(OOT3D::BossTitle::LoadTitle(*titles,i,data));
        assert(!OOT3D::BossTitle::LoadTitle(*titles,i,std::span(data).first(100)));
        unsigned visible=0;
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<256;++x)
            visible+=((*titles)[S::Tiled(x,i*64+y,256)]&255)>0;
        assert(visible>100);
    }
    B::Layout l;
    for(unsigned i=0;i<9;++i) {
        const auto name=B::NativeNameGeometry(l,i);assert(name.count==(i<8?1:0));
        if(i<8) {
            // PICA V runs opposite to the tiled texture's row order.
            const unsigned top=unsigned((1-name.uvs[1])*OOT3D::BossTitle::Height);
            const unsigned bottom=unsigned((1-name.uvs[5])*OOT3D::BossTitle::Height);
            assert(top==i*64 && bottom==i*64+63);
        }
        assert(B::NameGeometry(l,i,*text).count>0 == (i==8));
        auto moved=l;moved[B::BossName].x+=12;moved[B::BossName].y-=7;
        const auto shifted=B::NativeNameGeometry(moved,i);assert(shifted.count==name.count);
        for(unsigned j=0;j<name.count*12;j+=3) {
            assert(std::abs(shifted.positions[j]-name.positions[j]-12)<.001);
            assert(std::abs(shifted.positions[j+1]-name.positions[j+1]+7)<.001);
        }
        for(bool two:{false,true}) {
            assert(B::Neighbor(i,0,two)==((i+1)%9));
            assert(B::Neighbor(i,1,two)==((i+8)%9));
        }
    }
    assert(!B::NameGeometry(l,9,*text).count);
    const auto hints=B::NavigationHints(l,0),moving=B::NavigationHints(l,.25);
    assert(hints.count==4 && moving.count==4);
    assert(moving.positions[0]==hints.positions[0] && moving.positions[1]==hints.positions[1]+.25f);
    assert(hints.uvs[8]==hints.uvs[10] && hints.uvs[9]<hints.uvs[11]);
    assert(hints.uvs[24]==hints.uvs[26] && hints.uvs[25]>hints.uvs[27]);
    assert(hints.colors[17]==1 && hints.colors[18]==1); // Preserve native orange arrow colors.
    auto scaled=l;scaled[B::LeftStick].scale=2;
    const auto large=B::NavigationHints(scaled,0);
    assert(large.positions[3]-large.positions[0]==2*(hints.positions[3]-hints.positions[0]));
    assert(large.positions[15]-large.positions[12]==2*(hints.positions[15]-hints.positions[12]));
    auto moved=l;moved[B::LeftStick].x+=13;moved[B::LeftStick].y+=9;
    const auto together=B::NavigationHints(moved,0);
    for(unsigned q=0;q<2;++q)for(unsigned v=0;v<4;++v) {
        assert(together.positions[q*12+v*3]==hints.positions[q*12+v*3]+13);
        assert(together.positions[q*12+v*3+1]==hints.positions[q*12+v*3+1]+9);
    }
    // Border follows the preview, preserving the saved cut rectangle offset.
    namespace Border=OOT3D::BossBorder;
    auto source=std::make_unique<OOT3D::MapFrame::SourcePixels>();
    for(unsigned y=0;y<240;++y)for(unsigned x=0;x<400;++x)(*source)[y*400+x]=(x<<20)|(y<<8)|255;
    OOT3D::MapFrame::Cutout selection;selection.left=2;selection.top=3;
    selection.hole={{30,30},{370,30},{370,200},{30,200}};
    const auto cut=OOT3D::MapFrame::Cut(*source,selection);assert(cut && cut->left==2 && cut->top==3);
    const auto border=Border::MakePixels(*source,*cut);
    assert((*border)[Border::Tile(2,508)]==(*source)[3*400+2]);
    assert((*border)[Border::Tile(100,411)]==0); // Boss artwork removed.
    assert((*border)[Border::Tile(100,271)]==(*source)[239*400+100]);
    // Record areas remain transparent; the brown below them is included.
    assert((*border)[Border::Tile(100,511-228)]==0);
    assert((*border)[Border::Tile(300,511-228)]==0);
    assert((*border)[Border::Tile(10,511-228)]==(*source)[228*400+10]);
    assert((*border)[Border::Tile(100,511-239)]==(*source)[239*400+100]);
    const auto feather=Border::MakePixels(*source,*cut,0,true,8,.5f);
    const auto alpha=[&](unsigned x,unsigned y){return (*feather)[Border::Tile(x,511-y)]&255;};
    assert(alpha(30,100)>alpha(34,100) && alpha(34,100)>alpha(37,100));
    assert(alpha(39,100)==0 && alpha(100,228)==0);
    assert((*feather)[Border::Tile(29,411)]==(*border)[Border::Tile(29,411)]);
    const auto edge=Border::Make(l);assert(edge.count==2);
    // Evaluate the actual draw UVs against guest texture storage, not only the
    // CPU composite: the old inverted UVs sampled empty space for the frame.
    assert(unsigned((1-edge.uvs[1])*Border::Height)==511);
    assert(unsigned((1-edge.uvs[5])*Border::Height)==272);
    assert(unsigned((1-edge.uvs[9])*Border::Height)==271);
    assert(unsigned((1-edge.uvs[13])*Border::Height)==252);
    assert(std::abs(edge.positions[12+1]-(120+l[B::Preview].y+120*l[B::Preview].scale))<.001f);
    auto dim=l;dim.panel_darkness=.25f;dim.border_darkness=.5f;
    const auto dark=Border::Make(dim);assert(dark.count==2 && dark.colors[3]==1);
    assert(Border::Brightness(dim)==.375f);
    assert(dark.colors[0]==1 && dark.colors[16]==1); // Material tint, once for both quads.
    dim.border_enabled=dim.border_bottom=false;assert(Border::Make(dim).count==0);
    auto footer=l;footer[B::Footer].x=13;footer[B::Footer].y=7;
    const auto shifted_footer=Border::Make(footer);
    assert(shifted_footer.positions[0]==edge.positions[0]);
    assert(std::abs(shifted_footer.positions[12]-edge.positions[12]-13*l[B::Preview].scale)<.001f);
    assert(std::abs(shifted_footer.positions[13]-edge.positions[13]-7*l[B::Preview].scale)<.001f);
    footer[B::Footer].y=-12;
    const auto overlap=Border::Make(footer);
    assert(overlap.positions[13]==edge.positions[13]); // Never cover Victories/Record.
    assert(std::abs(overlap.positions[19]-overlap.positions[13]-8*l[B::Preview].scale)<.001f);
    const auto aligned=Border::Make(l,{0,-12,400,228});
    assert(std::abs(aligned.positions[13]-(120+l[B::Preview].y+108*l[B::Preview].scale))<.001f);
    for(unsigned x:{0u,12u,388u,399u}) {
        assert((*border)[Border::Tile(x,271)]==(*source)[239*400+x]);
    }
    // Brown record trim is tinted; only the green interiors are transparent.
    assert((*border)[Border::Tile(22,511-218)]==(*source)[218*400+22]);
    assert((*border)[Border::Tile(100,511-237)]==(*source)[237*400+100]);
    const auto rounded=Border::MakePixels(*source,*cut,16,true);
    assert(((*rounded)[Border::Tile(2,508)]&255)==0);
    assert((*rounded)[Border::Tile(100,511-228)]==0);
    const std::array<float,12> panel{0,0,0,400,0,0,0,240,0,400,240,0};
    const std::array<float,8> tex{0,1,1,1,0,0,1,0};
    const auto curved=OOT3D::BossRounding::Make(panel,tex,16,true);
    assert(curved.count==65 && curved.positions[0]==16);
    assert(curved.positions[(curved.count-1)*12+6]==16);
    for(unsigned q=0;q<curved.count;++q)for(unsigned v=0;v<4;++v) {
        const float x=curved.positions[q*12+v*3],y=curved.positions[q*12+v*3+1];
        assert(x>=0 && x<=400 && y>=0 && y<=240);
        assert(std::abs(curved.uvs[q*8+v*2]-x/400)<.0001f);
        assert(std::abs(curved.uvs[q*8+v*2+1]-(1-y/240))<.0001f);
    }
    const auto buttons=B::ButtonPrompts(l,*prompts);
    assert(buttons.icons.count==2 && buttons.text.count==10); // Select / Back.
    assert(buttons.icons.positions[0]==120 && buttons.icons.positions[12]==220);
    static_assert(decltype(B::Hook{}.text)::TextureBytes==sizeof(prompts->text));
    std::cout<<"Boss presentation: native shop art, animated mirrored hints, all boss labels, positioning/scaling, Select/Back atlas and sequential navigation passed\n";
}
