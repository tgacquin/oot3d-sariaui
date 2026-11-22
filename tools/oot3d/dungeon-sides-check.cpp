// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/dungeon_sides.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace DungeonSides;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    bool WriteFloats(U32 a,std::span<const float> values) {
        for(unsigned i=0;i<values.size();++i)if(!words.contains(a+4*i))return false;
        Put(a,values);return true;
    }
    template<class T> void Put(U32 a,const T& v) {for(unsigned i=0;i<v.size();++i)words[a+i*4]=std::bit_cast<U32>(v[i]);}
};
constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
int main() {
    constexpr std::array<float,12> quad{0,0,3,40,0,3,0,40,3,40,40,3};
    constexpr std::array<float,8> uv{.2f,.8f,.6f,.8f,.2f,.4f,.6f,.4f};
    std::array<float,16> color; color.fill(1);for(unsigned i=3;i<16;i+=4)color[i]=.6f;
    {
        auto p=quad;auto tex=uv;auto rgba=color;
        ClipFloorRight(p,tex,rgba,30);
        assert(p[3]==30 && p[9]==30 && Near(tex[2],.5f) && Near(tex[6],.5f));
        assert(p[0]==0 && p[6]==0 && rgba==color);
        p=quad;tex=uv;ClipFloorRight(p,tex,rgba,-10);
        assert(p[0]==p[3] && p[6]==p[9]);
        p=quad;tex=uv;ClipFloorRight(p,tex,rgba,50);assert(p==quad && tex==uv);
        PauseMenu::Layout line_layout;
        auto line=DungeonFloorLine::Make(line_layout,.5f,31);
        assert(line.count==33 && Near(line.colors[3],.4f));
        auto& settings=line_layout.dungeon_floor_line;
        settings.roundness=0;line=DungeonFloorLine::Make(line_layout,1,31);
        assert(line.count==1 && Near(line.positions[3]-line.positions[0],4));
        assert(Near(line.positions[7]-line.positions[1],184));
        const auto initial=line;
        settings.pose={10,-20,2};line=DungeonFloorLine::Make(line_layout,1,31);
        assert(Near(line.positions[3]-line.positions[0],8));
        assert(Near((line.positions[3]+line.positions[0]-initial.positions[3]-initial.positions[0])/2,10));
        assert(Near((line.positions[7]+line.positions[1]-initial.positions[7]-initial.positions[1])/2,-20));
        settings.enabled=false;assert(DungeonFloorLine::Make(line_layout,1,31).count==0);
        settings.enabled=true;settings.width=NAN;assert(DungeonFloorLine::Make(line_layout,1,31).count==0);
    }
    PauseMenu::Layout layout;std::array<Rect,2> panels{{{-200,-100,60,300},{340,-100,600,300}}};
    {
        PauseMenu::MapTopFade fade;const Rect paper{40,30,360,220};
        assert(!MapTopFade::Make(fade,paper,1).count);
        fade.enabled=true;fade.opacity=.8f;fade.height=64;fade.color={.2f,.3f,.4f};
        auto g=MapTopFade::Make(fade,paper,.5f);
        assert(g.count==32 && Near(g.colors[3],.4f) && Near(g.colors[0],.2f));
        assert(Near(g.positions[1],30) && Near(g.positions[(g.count-1)*12+7],94));
        assert(Near(g.colors[(g.count-1)*16+15],0));
        assert(Near(MapTopFade::Opacity(62,30,fade),.4f));
        fade.y=20;g=MapTopFade::Make(fade,paper,1);
        assert(g.count==33 && Near(g.positions[7],50));
        fade.y=-32;g=MapTopFade::Make(fade,paper,1);
        assert(g.count==16 && Near(g.positions[1],30) && Near(g.colors[3],.4f));
        fade.y=-100;assert(!MapTopFade::Make(fade,paper,1).count);
        fade.y=300;g=MapTopFade::Make(fade,paper,1);
        assert(g.count==1 && Near(g.positions[7],220));
        assert(!MapTopFade::Make(fade,paper,0).count);
        fade.height=NAN;assert(!MapTopFade::Make(fade,paper,1).count);
    }
    {
        const Rect paper{40,30,360,220};
        auto shadow=TopShadow(layout,paper,.5f);
        assert(shadow.count==1 && Near(shadow.colors[3],.325f));
        assert(shadow.positions[0]==40 && shadow.positions[6]==360);
        assert(shadow.positions[1]==30 && shadow.positions[4]==-2);
        assert(shadow.uvs[0]<shadow.uvs[2]); // Dark against paper, transparent above.
        layout.dungeon_top_shadow.y=-16;
        shadow=TopShadow(layout,paper,1);
        assert(shadow.positions[4]==-18 && shadow.positions[1]==14);
        layout.dungeon_top_shadow.y=16;
        shadow=TopShadow(layout,paper,1);
        assert(shadow.positions[4]==14 && shadow.positions[1]==30);
        assert(Near(shadow.uvs[0],.5f)); // Never shades inside the parchment.
        layout.dungeon_top_shadow.y=200;assert(!TopShadow(layout,paper,1).count);
        layout.dungeon_top_shadow={};layout.dungeon_top_shadow.enabled=false;
        assert(!TopShadow(layout,paper,1).count);
        layout.dungeon_top_shadow={};assert(!TopShadow(layout,paper,0).count);
    }
    layout.dungeon_blocks[0]={true,5,10,90,180,30,.8f};
    layout.dungeon_blocks[1]={true,-5,20,80,170,20,.6f};
    auto blocks=Blocks(layout,panels,.5f);assert(blocks.count==4);
    assert(blocks.positions[0]==65 && blocks.positions[6]==125);
    assert(blocks.positions[12]==125 && blocks.positions[18]==155);
    assert(blocks.positions[24]==255 && blocks.positions[30]==275);
    assert(blocks.positions[36]==275 && blocks.positions[42]==335);
    assert(Near(blocks.colors[3],.4f) && Near(blocks.colors[35],.3f));
    assert(blocks.uvs[8]<blocks.uvs[12] && blocks.uvs[16]>blocks.uvs[20]);
    layout.dungeon_blocks[0].fade=1000;layout.dungeon_blocks[1].enabled=false;
    assert(Blocks(layout,panels,1).count==1);assert(Blocks(layout,panels,0).count==0);
    layout.dungeon_blocks[0].fade=0;assert(Blocks(layout,panels,1).count==1);

    Memory m;
    constexpr U32 root=0x506cb0,model=0x18100000,mesh=0x18110000,desc=0x18120000,live=0x14900000,
        helper=0x18130000,bg=0x18140000,bmesh=0x18150000,bdesc=0x18160000,blive=0x14910000;
    m.words[0x5066f8+0x34]=m.words[0x50446c+0x18]=0;m.words[root+0x38]=2;m.words[0x4fda6c+0x50]=1;
    m.words[root+8]=mesh;m.words[root+0xc]=model;m.words[root+0x14]=helper;m.words[helper]=50;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.Put(model+0xc,identity);
    m.words[model+0x1dc]=0x18170000; // Borrowed native material must be ready before setup.
    m.words[mesh]=desc;m.words[desc+0xc]=200;m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=live;
    m.words[PauseMenu::UI+0x10]=bg;m.words[bg]=0x4ebd60;m.words[bg+4]=bmesh;m.Put(bg+0xc,identity);
    m.words[bmesh]=bdesc;m.words[bdesc+0xc]=4;m.words[bmesh+0x124]=0;m.words[bmesh+0x1a0]=blive;
    m.Put(blive,std::array<float,12>{0,0,0,320,0,0,0,240,0,320,240,0});
    layout={};layout.map_backgrounds[0].pose={-208,0,1};layout.map_backgrounds[1].pose={208,0,1};
    PauseMenu::SetLayout(layout,false);PauseMenu::motion_x=0;
    // Controls cross the parchment edges and must retain their full geometry.
    for(unsigned q=0;q<50;++q) {
        auto p=quad;
        const float x=q>=21?250.f:50.f;
        for(unsigned v=0;v<4;++v)p[v*3]+=x;
        m.Put(live+q*48,p);m.Put(live+200*12+q*32,uv);m.Put(live+200*20+q*64,color);
    }
    m.Put(live,std::array<float,12>{60,0,0,350,0,0,60,240,0,350,240,0});
    const auto before=m.words;Hook hook;std::array<U32,16> r{};r[0]=model;r[14]=0x123456;
    assert(hook.NativeDraw(m,r,true) && hook.used==1 && hook.immediate==0);
    assert(m.words==before); // Source meshes, texture and game state are never patched.
    assert(hook.backs[0].geometry.count==25); // Map/background/hidden native cards.
    assert(hook.geometry[0].count==25); // Six left quads, 17 floors, two temple-banner quads.
    assert(hook.geometry[0].positions[0]==50 && Near(hook.geometry[0].uvs[0],.2f));
    assert(hook.geometry[0].positions[8*12+3]==290);
    assert(hook.backs[0].entry==PauseMenu::Sites[19].pc); // Distinct from the native draw entry.
    assert(hook.sources[0]==model && Near(hook.alpha,.6f));
    assert(hook.paper_bounds && Near(hook.paper_bounds->left,120) && Near(hook.paper_bounds->right,352));
    // Exercise immediate native-draw replacement and the deferred draw path,
    // including their distinct return sites and balanced register restoration.
    constexpr U32 back_block=0x18300000,back_model=0x18310000,front_block=0x18320000,front_model=0x18330000;
    for(U32 a=model;a<model+0x1e4;a+=4)if(!m.words.contains(a))m.words[a]=0;
    for(U32 a=desc;a<desc+0x120;a+=4)if(!m.words.contains(a))m.words[a]=0;
    for(U32 a=mesh+0x13c;a<mesh+0x19c;a+=4)m.words[a]=0;
    m.words[model+0x1dc]=0x18500000;m.words[mesh+0x158]=0x14980000;
    m.words[mesh+0x15c]=512|(256u<<16);
    for(U32 block:{back_block,front_block}) {
        for(U32 a=block;a<block+Layer::AllocationSize;a+=4)m.words[a]=0;
        m.words[block+0x128]=2;
    }
    for(U32 clone:{back_model,front_model}) {
        for(U32 a=clone;a<clone+0x1e4;a+=4)m.words[a]=0;
        m.words[clone]=0x4ebd60;m.words[clone+8]=clone+0x1000;m.words[clone+0x1358]=0x18500000;
    }
    r[0]=back_block;hook.ResumeImmediate(m,r);assert(r[15]==0x348f34);
    hook.ResumeImmediate(m,r);assert(r[15]==0x34897c);
    r[0]=back_model;hook.ResumeImmediate(m,r);
    hook.ResumeImmediate(m,r);hook.ResumeImmediate(m,r);hook.ResumeImmediate(m,r);
    assert(hook.Drawing() && r[0]==back_model && r[15]==0x3f9680);
    assert(!hook.NativeDraw(m,r,true)); // Replacement cannot intercept itself.
    assert(m.words[back_block+0x158]==m.words[mesh+0x158]);
    hook.ResumeImmediate(m,r);assert(!hook.Drawing() && hook.immediate<0 && r[15]==0x123456);
    layout.dungeon_blocks[0].enabled=layout.dungeon_blocks[1].enabled=false;PauseMenu::SetLayout(layout,false);
    r[15]=PauseMenu::Sites[1].pc+4;const auto finish_regs=r;
    hook.Finish(m,r,PauseMenu::Sites[1].pc);assert(hook.Pending() && r[15]==0x2a8bdc);
    r[0]=front_block;hook.Finish(m,r,PauseMenu::Sites[1].pc);assert(r[15]==0x348f34);
    hook.Finish(m,r,PauseMenu::Sites[1].pc);assert(r[15]==0x34897c);
    r[0]=front_model;hook.Finish(m,r,PauseMenu::Sites[1].pc);
    hook.Finish(m,r,PauseMenu::Sites[1].pc);hook.Finish(m,r,PauseMenu::Sites[1].pc);hook.Finish(m,r,PauseMenu::Sites[1].pc);
    assert(hook.Drawing() && !hook.NativeDraw(m,r,true));
    hook.Finish(m,r,PauseMenu::Sites[1].pc);assert(!hook.Pending() && r==finish_regs);
    // A reused layer can shrink then grow on alternate native vertex buffers.
    // The board renderer submits the index count from model +0x174 at 0x3fb968.
    auto& front=hook.fronts[0];
    auto sample=front.geometry;
    for(unsigned count:{1u,25u}) {
        sample.count=count;
        const auto bank=m.words[front_block+0x124];
        front.Begin(m,r,PauseMenu::Sites[1].pc,sample,{},model);
        assert(front.stage==Layer::Stage::Positions && r[1]==count*48);
        front.Resume(m,r);assert(r[1]==count*32);
        front.Resume(m,r);assert(r[1]==count*64);
        front.Resume(m,r);assert(front.Drawing() && m.words[front_block+0x124]!=bank);
        assert(m.words[front_model+0x174]==count*6);
        front.Resume(m,r);assert(!front.Pending());
    }
    hook.Reset();assert(!hook.backgrounds && !hook.used && !hook.Finishing());
    // Actual default backgrounds meet in the middle underneath the parchment.
    // Background panels must never subtract wells or floor selectors.
    layout.map_backgrounds[0].pose={-128,0,1};layout.map_backgrounds[1].pose={128,0,1};
    PauseMenu::SetLayout(layout,false);
    m.Put(live,std::array<float,12>{60,0,0,350,0,0,60,240,0,350,240,0});
    r[0]=model;assert(hook.NativeDraw(m,r,true));
    assert(hook.geometry[0].count==25);
    assert(Near(hook.geometry[0].positions[0],50));
    assert(Near(hook.geometry[0].positions[8*12+3],290));
    // A narrow paper clips the stone artwork, not the left controls. Native
    // hidden quads remain degenerate, and clipping never squeezes textures.
    hook.Reset();
    m.Put(live,std::array<float,12>{3,29,0,207,29,0,3,269,0,207,269,0});
    r[0]=model;assert(hook.NativeDraw(m,r,true));
    assert(hook.geometry[0].count==25);
    for(unsigned q=21;q<=37;++q) {
        const unsigned at=8+q-21;
        assert(hook.geometry[0].positions[at*12]==hook.geometry[0].positions[at*12+3]);
    }
    // A wider/repositioned map exposes both side controls in full; the hidden
    // part of each brown panel must never clip controls lying over the map.
    hook.Reset();
    m.Put(live,std::array<float,12>{-30,0,0,350,0,0,-30,240,0,350,240,0});
    r[0]=model;assert(hook.NativeDraw(m,r,true));
    assert(hook.geometry[0].count==25 && Near(hook.geometry[0].positions[0],50));
    assert(Near(hook.geometry[0].positions[8*12+3],290));
    hook.Reset();
    // An enabled composed parchment draws first, then the original stone
    // atlas (without its paper quad), then the deferred side widgets.
    Hook merged;
    merged.parchment.ready=true;merged.parchment.native_model=model;
    merged.parchment.composed=std::make_shared<DungeonParchment::Pixels>();
    merged.parchment.prepared={std::array<float,12>{-30,0,0,351,0,0,-30,240,0,351,240,0},uv,color,1};
    r={};r[0]=model;r[14]=0x123456;
    assert(merged.NativeDraw(m,r,true) && merged.parchment_first);
    assert(merged.backs[0].geometry.count==24 && merged.geometry[0].count==25);
    assert(merged.geometry[0].positions[8*12+3]==290); // Extended paper edge, not the original piece.
    assert(merged.paper_bounds && Near(merged.paper_bounds->left,48) && Near(merged.paper_bounds->right,352.8f));
    // Failed guest allocation falls back to the complete native draw.
    r[0]=0;merged.ResumeImmediate(m,r);
    assert(merged.immediate<0 && !merged.geometry[0].count && r[15]==0x3f9684);
    merged.Reset();merged.parchment.ready=true;r={};r[0]=model;r[14]=0x123456;
    assert(merged.NativeDraw(m,r,true));
    constexpr U32 paper_block=0x18600000,paper_model=0x18610000,paper_texture=0x14b00000;
    for(U32 a=paper_block;a<paper_block+DungeonParchment::Hook::AllocationSize;a+=4)m.words[a]=0;
    m.words[paper_block+0x128]=2;
    for(U32 a=paper_model;a<paper_model+0x1e4;a+=4)m.words[a]=0;
    m.words[paper_model]=0x4ebd60;m.words[paper_model+8]=0x18620000;m.words[0x18620358]=0x18500000;
    for(U32 a=paper_texture;a<paper_texture+DungeonParchment::Hook::TextureBytes;a+=4)m.words[a]=0;
    for(unsigned step=0;merged.immediate>=0 && step<20;++step) {
        if(merged.parchment_first) {
            using Stage=DungeonParchment::Hook::Stage;
            if(merged.parchment.stage==Stage::Allocate)r[0]=paper_block;
            if(merged.parchment.stage==Stage::TextureAllocate)r[0]=paper_texture;
            if(merged.parchment.stage==Stage::Model)r[0]=paper_model;
        } else {
            if(merged.backs[0].stage==Layer::Stage::Allocate)r[0]=back_block;
            if(merged.backs[0].stage==Layer::Stage::Model)r[0]=back_model;
        }
        merged.ResumeImmediate(m,r);
    }
    assert(merged.immediate<0 && r[15]==0x123456 && merged.geometry[0].count==25);
    assert(m.words[paper_block+0x164]==0 && m.words[paper_block+0x15c]==(1024|(512u<<16)));
    assert(m.words[mesh+0x158]==0x14980000); // Native sampler remains untouched.
    // The cursor is a separate model: even outside a narrow parchment it
    // remains full-sized, while the floor-atlas artwork is clipped above.
    constexpr U32 cursor=0x18700000,cmodel=0x18710000,cmesh=0x18720000,cdesc=0x18730000,clive=0x14920000;
    m.words[root+0x1c]=cursor;m.words[root+0x30]=0;m.words[cursor+4]=cmodel;
    m.words[cmodel]=0x4ebd60;m.words[cmodel+4]=cmesh;m.words[cmesh]=cdesc;
    m.words[cdesc+0xc]=4;m.words[cmesh+0x124]=0;m.words[cmesh+0x1a0]=clive;
    auto cursor_quad=quad;for(unsigned i=0;i<4;++i)cursor_quad[i*3]+=300;
    m.Put(clive,cursor_quad);m.Put(clive+48,uv);m.Put(clive+80,color);
    Hook cursor_hook;r[0]=cmodel;assert(cursor_hook.NativeDraw(m,r,true));
    assert(cursor_hook.geometry[0].count==1);
    for(unsigned i=0;i<12;++i)assert(cursor_hook.geometry[0].positions[i]==cursor_quad[i]);
    // The new line submits after the background pass and before any stones.
    m.words[PauseHeader::Items+0x20]=model;
    for(unsigned row=0;row<5;++row)m.words[0x55b638+4*row]=1;
    Hook line_hook;line_hook.finish_started=true;line_hook.used=1;line_hook.alpha=1;
    line_hook.sources[0]=model;line_hook.geometry[0].Add(quad,uv,color);
    line_hook.paper_bounds=Rect{40,30,360,220};
    layout.dungeon_map_top_fade.enabled=true;PauseMenu::SetLayout(layout,false);
    r={};line_hook.Finish(m,r,PauseMenu::Sites[1].pc);
    assert(line_hook.top_fade.Pending() && !line_hook.top_shadow.Pending() && line_hook.next==0);
    // A failed optional fade allocation must still draw the shadow and controls.
    r[0]=0;line_hook.Finish(m,r,PauseMenu::Sites[1].pc);
    assert(!line_hook.top_fade.Pending());
    assert(line_hook.top_shadow.Pending() && !line_hook.floor_line.Pending() && line_hook.next==0);
    assert(!line_hook.NativeDraw(m,r,true));
    r[0]=0;line_hook.Finish(m,r,PauseMenu::Sites[1].pc);
    assert(!line_hook.top_shadow.Pending()); // Failure still proceeds to the floor line and controls.
    assert(line_hook.floor_line.Pending() && line_hook.next==0 && line_hook.floor_line.geometry.count==33);
    r[0]=0;line_hook.Finish(m,r,PauseMenu::Sites[1].pc); // Allocation failure still draws the controls.
    assert(!line_hook.floor_line.Pending() && line_hook.next==1 && line_hook.fronts[0].Pending());
    m.words[0x4fda6c+0x50]=0;r[0]=model;assert(!hook.NativeDraw(m,r,true));
    std::cout<<"Dungeon side-control visibility, UV preservation, native ownership, layer separation, shadow direction and dimensions passed\n";
}
