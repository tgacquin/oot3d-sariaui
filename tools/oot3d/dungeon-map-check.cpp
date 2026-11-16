// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_menu.h"
#include "oot3d/screen_model_hooks.h"
#include "oot3d/pause_header.h"
#include "oot3d/pause_shadow.h"
#include "oot3d/dungeon_sides.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void F(U32 a,float f) {words[a]=std::bit_cast<U32>(f);}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
int main(int argc,char** argv) {
    assert(argc==2);Memory m;
    std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
    for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(CodeMatches(m));
    assert(m.Read(0x480eb4)==0xe3a01032); // Constructor allocates 50 quads.
    assert(m.F(0x506d60)==56 && m.F(0x506d60+8)==0 && m.F(0x506d60+16)==256);
    constexpr U32 root=0x506cb0,play=0x8000000,model=0x8100000,mesh=0x8200000,
        helper=0x8300000,src=0x8400000,dst=0x8500000;
    m.words[UI+0xc]=play;m.words[UI+0x14]=1;m.words[UI+0x20]=0;m.words[UI+0x24]=1;
    m.words[play+0x100]=3;m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=0;
    m.words[root+0x38]=2;m.words[0x4fda6c+0x50]=1;
    m.words[root+8]=mesh;m.words[root+0xc]=model;m.words[root+0x14]=helper;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[helper]=50;
    assert(DungeonPage(m));
    const auto quad=[&](unsigned q,float x,float y,float w,float h) {
        const std::array<float,12> v{x,y,-3,x+w,y,-3,x,y+h,-3,x+w,y+h,-3};
        for(unsigned i=0;i<12;++i) {m.F(src+q*48+i*4,v[i]);m.F(dst+q*48+i*4,0);}
    };
    for(unsigned q=0;q<50;++q)
        quad(q,m.F(root+0xb0+q*8),m.F(root+0xb4+q*8),m.F(root+0x240+q*8),m.F(root+0x244+q*8));
    for(unsigned q=21;q<=35;++q)quad(q,260,4+46*((q-21)%5),48,36);
    Layout poses;poses.dungeon_edge_shadows=false;poses[Map]={300,200,2}; // Overworld controls must not affect dungeon parts.
    poses[DungeonMap]={-13,6,1.1f};poses[DungeonContents]={11,-7,.75f};
    poses[DungeonLeft]={-60,13,1.2f};poses[DungeonRight]={70,-12,.7f};poses[DungeonBanner]={20,16,1.3f};
    poses[DungeonItems]={45,-5,.9f};poses[DungeonFloors]={-52,8,1.1f};
    SetLayout(poses,false);motion_x=motion_slots=motion_y=0;
    const auto original=m.words;Hook hook;
    assert(CopyPositions(m,mesh,dst,src,50*48,&hook.pending));
    {
        const auto visible=m.words;
        poses.hide_dungeon_floor_stones=true;SetLayout(poses,false);
        assert(CopyPositions(m,mesh,dst,src,50*48));
        for(unsigned q=0;q<50;++q)for(unsigned i=0;i<12;i+=3) {
            if(q>=21 && q<=30) {
                assert(m.F(dst+q*48+i*4)==m.F(src+q*48));
                assert(m.F(dst+q*48+i*4+4)==m.F(src+q*48+4));
            } else {
                assert(m.words.at(dst+q*48+i*4)==visible.at(dst+q*48+i*4));
                assert(m.words.at(dst+q*48+i*4+4)==visible.at(dst+q*48+i*4+4));
            }
        }
        poses.hide_dungeon_floor_stones=false;SetLayout(poses,false);m.words=visible;
    }
    // Independent expected native atlas ownership, including bottom cards that
    // remain hidden even if a pose would move them back onto the visible screen.
    const std::array<Part,50> expected{DungeonMap,DungeonLeft,DungeonRight,
        PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,
        DungeonItems,DungeonItems,DungeonItems,DungeonBanner,DungeonItems,DungeonItems,DungeonItems,DungeonBanner,
        DungeonFloors,DungeonFloors,DungeonFloors,DungeonFloors,DungeonFloors,
        DungeonFloors,DungeonFloors,DungeonFloors,DungeonFloors,DungeonFloors,
        DungeonFloors,DungeonFloors,DungeonFloors,DungeonFloors,DungeonFloors,DungeonFloors,DungeonFloors,
        PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount,PartCount};
    for(unsigned q=0;q<50;++q) {
        assert(DungeonQuadPart(m,mesh,q,50)==expected[q]);
        for(unsigned i=0;i<12;i+=3) {
            if(expected[q]==PartCount) {assert(m.F(dst+q*48+i*4)==m.F(src+q*48));continue;}
            const auto p=Move(m.F(src+q*48+i*4),m.F(src+q*48+i*4+4),expected[q],poses[expected[q]]);
            assert(Near(m.F(dst+q*48+i*4),p[0]) && Near(m.F(dst+q*48+i*4+4),p[1]));
            assert(m.F(dst+q*48+i*4+8)==-3);
        }
    }
    {
        const auto saved=m.words;
        const auto saved_poses=poses;
        poses.dungeon_floor_growth={true,1.5f,1};SetLayout(poses,false);
        for(unsigned row=0;row<5;++row)m.words[0x55b638+row*4]=1;
        m.words[root+0x30]=2;
        floor_growth_animation.Update(1000000,true,false,2,31,poses.dungeon_floor_growth);
        floor_growth_animation.Update(1075000,true,false,2,31,poses.dungeon_floor_growth);
        assert(Near(floor_growth_animation.ScaleFor(2,poses.dungeon_floor_growth),1.25f));
        assert(floor_growth_animation.ScaleFor(1,poses.dungeon_floor_growth)==1);
        Hook grown;
        assert(CopyPositions(m,mesh,dst,src,50*48,&grown.pending));
        const auto pending=m.words;
        grown.Animate(m);
        for(unsigned q=21;q<=35;++q) {
            const unsigned row=(q-21)%5;
            const auto center=Move(290,28+46.f*row+DungeonRowShift(DungeonFloors,row,poses,31),DungeonFloors,poses[DungeonFloors]);
            for(unsigned i=0;i<12;i+=3) {
                const auto value=std::bit_cast<float>(pending.at(dst+q*48+4*i));
                assert(Near(m.F(dst+q*48+4*i),center[0]+(value-center[0])*(row==2?1.25f:1.f)));
            }
        }
        const auto deferred=m.words;
        assert(CopyPositions(m,mesh,dst,src,50*48));
        for(unsigned i=0;i<50*48;i+=4)assert(m.words.at(dst+i)==deferred.at(dst+i));
        // Switching smoothly reverses the previous row and grows the new one.
        floor_growth_animation.Update(1150000,true,false,1,31,poses.dungeon_floor_growth);
        assert(floor_growth_animation.ScaleFor(2,poses.dungeon_floor_growth)==1);
        assert(Near(floor_growth_animation.ScaleFor(1,poses.dungeon_floor_growth),1.25f));
        floor_growth_animation.Update(1225000,true,false,1,31,poses.dungeon_floor_growth);
        assert(floor_growth_animation.ScaleFor(1,poses.dungeon_floor_growth)==1.5f);
        // Closing holds the last scale; leaving the map resets it.
        floor_growth_animation.Update(1300000,true,true,3,31,poses.dungeon_floor_growth);
        assert(floor_growth_animation.ScaleFor(1,poses.dungeon_floor_growth)==1.5f);
        floor_growth_animation.Update(1400000,false,false,3,31,poses.dungeon_floor_growth);
        assert(floor_growth_animation.ScaleFor(1,poses.dungeon_floor_growth)==1);
        // 30/60/120 Hz agree after the same elapsed time, and speed is literal.
        for(unsigned hz:{30u,60u,120u})for(float speed:{.5f,1.f,2.f}) {
            FloorGrowthAnimation animation;FloorGrowth settings{true,1.5f,speed};
            animation.Update(1000000,true,false,4,16,settings);
            for(unsigned frame=1;frame<=hz;++frame)
                animation.Update(1000000+1000000ll*frame/hz,true,false,4,16,settings);
            assert(animation.ScaleFor(4,settings)==1.5f);
            assert(animation.ScaleFor(0,settings)==1);
            settings.enabled=false;
            animation.Update(2100000,true,false,4,16,settings);
            assert(animation.ScaleFor(4,settings)==1 && !animation.started);
        }
        for(float speed:{.5f,1.f,2.f}) {
            FloorGrowthAnimation animation;FloorGrowth settings{true,1.5f,speed};
            animation.Update(1000000,true,false,0,1,settings);
            animation.Update(1050000,true,false,0,1,settings);
            const float t=speed/3.f;
            assert(Near(animation.ScaleFor(0,settings),1+.5f*t*t*(3-2*t)));
            // A timeline rewind restarts safely; unavailable floors never grow.
            animation.Update(900000,true,false,0,0,settings);
            assert(animation.ScaleFor(0,settings)==1);
        }
        poses=saved_poses;SetLayout(poses,false);m.words=saved;
    }
    motion_x=-23;hook.Animate(m);
    assert(Near(m.F(dst),Move(56,32,DungeonMap,poses[DungeonMap])[0]-23/Scale));
    for(U32 a=src;a<src+50*48;a+=4)assert(m.words[a]==original.at(a));
    // Stone-only color uploads preserve the other 48 quads and every alpha.
    constexpr U32 color_src=0x8b00000,color_dst=0x8b10000,desc=0x8b20000;
    m.words[helper+0x18]=color_src;
    for(unsigned i=0;i<50*16;++i) {
        m.F(color_src+4*i,i%4==3?.4f:.8f);m.F(color_dst+4*i,0);
    }
    poses.dungeon_brightness=.35f;
    poses.dungeon_left_overlay={{1,0,0},.5f};poses.dungeon_right_overlay={{0,0,1},.75f};
    SetLayout(poses,false);
    const auto colors_before=m.words;
    assert(CopyDungeonStoneColors(m,mesh,color_dst,color_src,50*64));
    for(unsigned i=0;i<50*16;++i) {
        float expected=i%4==3?.4f:.8f;
        if(i%4<3 && i/16==0)expected*=.35f;
        if(i%4<3 && (i/16==1 || i/16==2))
            expected*=OverlayTint(i/16==1?poses.dungeon_left_overlay:poses.dungeon_right_overlay)[i%4];
        assert(Near(m.F(color_dst+i*4),expected));
        assert(m.words.at(color_src+i*4)==colors_before.at(color_src+i*4));
    }
    const auto tinted=m.words;
    assert(CopyDungeonStoneColors(m,mesh,color_dst,color_src,50*64) && m.words==tinted);
    assert(!CopyDungeonStoneColors(m,mesh+4,color_dst,color_src,50*64));
    assert(!CopyDungeonStoneColors(m,mesh,color_dst,color_src,37*64));
    // Observe the actual transformed upload, including movement and native
    // fade; shadows face outward and follow independent column scales.
    m.words[mesh]=desc;m.words[desc+0xc]=200;m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=dst;
    constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
    for(unsigned i=0;i<12;++i)m.F(model+0xc+i*4,identity[i]);
    poses.dungeon_left_shadow={2,.6f};poses.dungeon_right_shadow={3,.7f};
    OOT3D::PauseShadow::MapEdges edges;edges.Observe(m,model);
    const auto shadows=edges.Make(poses);assert(shadows.count==2);
    for(unsigned side=0;side<2;++side) {
        const unsigned q=side+1;
        const auto b=OOT3D::PauseShadow::Bounds(m,dst+q*48,4,model);assert(b);
        const auto box=OOT3D::PauseShadow::ToTop(*b,{});
        const auto part=side?DungeonRight:DungeonLeft;
        const auto style=side?poses.dungeon_right_shadow:poses.dungeon_left_shadow;
        const float depth=8*style.width*Scale*poses[part].scale;
        const unsigned v=side*12;
        assert(Near(shadows.positions[v],side?box.right:box.left-depth));
        assert(Near(shadows.positions[v+6],side?box.right+depth:box.left));
        assert(Near(shadows.positions[v+4],box.top) && Near(shadows.positions[v+1],box.bottom));
        assert(Near(shadows.colors[side*16+3],.4f*style.opacity));
    }
    assert(shadows.uvs[0]>shadows.uvs[4] && shadows.uvs[8]<shadows.uvs[12]);
    auto elevated=poses;elevated.dungeon_edge_shadows=true;elevated.dungeon_edge_shadow={3,.8f};
    const auto elevation=edges.Make(elevated);assert(elevation.count==4);
    assert(Near(elevation.positions[24+6],edges.bounds->left));
    assert(Near(elevation.positions[24],edges.bounds->left-24));
    assert(Near(elevation.positions[36],edges.bounds->right));
    assert(Near(elevation.colors[32+3],.4f*.8f));
    auto no_shadow=poses;no_shadow.dungeon_left_shadow.opacity=0;
    assert(edges.Make(no_shadow).count==1);
    edges.Reset();assert(edges.Make(poses).count==0);
    for(unsigned v=0;v<4;++v)m.F(color_src+64+v*16+12,0);
    edges.Observe(m,model);assert(edges.Make(poses).count==1);
    m.words[0x4fda6c+0x50]=0;
    assert(!CopyDungeonStoneColors(m,mesh,color_dst,color_src,50*64));
    m.words[0x4fda6c+0x50]=1;
    // Actual icon controller, small-key counter and selection cursor owners.
    constexpr U32 icons=0x8600000,iconmodel=0x8610000,iconmesh=0x8620000,
        keys=0x8700000,keymodel=0x8710000,keymesh=0x8720000,
        cursor=0x8800000,cursormodel=0x8810000,cursormesh=0x8820000;
    m.words[root+0x20]=icons;m.words[icons+8]=iconmodel;m.words[icons+0x224]=4;
    m.words[iconmodel]=0x4ebd60;m.words[iconmodel+4]=iconmesh;
    m.words[root+0x18]=keys;m.words[keys+0x10]=keymodel;m.words[keymodel]=0x4ebd60;m.words[keymodel+4]=keymesh;
    m.words[root+0x1c]=cursor;m.words[cursor+4]=cursormodel;m.words[cursormodel]=0x4ebd60;m.words[cursormodel+4]=cursormesh;
    for(unsigned q=0;q<4;++q) {
        assert(DungeonQuadPart(m,iconmesh,q,4)==(q==3?DungeonSkulltula:DungeonItems));
        quad(q,q==3?68:10,q==3?8:112-50*q,24,24);
    }
    assert(CopyPositions(m,iconmesh,dst,src,4*48));
    assert(Near(m.F(dst+3*48),MoveDungeonSkulltula(68,8,poses)[0]));
    // Position is local to the paper; independent title/legacy banner poses
    // cannot detach the icon. Use the same freshly updated rise and opacity.
    const auto before=poses;
    poses[DungeonLocationPaper]={12,-180,.75f};poses[DungeonSkulltula]={17,-8,1.4f};
    SetLayout(poses,false);world_location_y=-6;world_location_alpha=.25f;
    assert(CopyPositions(m,iconmesh,dst,src,4*48,&hook.pending));
    for(unsigned i=0;i<64;++i){m.F(color_src+i*4,i%4==3?.8f:.6f);m.F(color_dst+i*4,0);}
    hook.Animate(m);
    const auto moved=MoveDungeonSkulltula(68,8,poses);
    assert(Near(m.F(dst+3*48),moved[0]) && Near(m.F(dst+3*48+4),moved[1]-6/Scale));
    const auto center=MoveDungeonSkulltula(80,20,poses);
    assert(Near(200+(center[0]-160)*Scale,200+(-92+17)*.75f+12));
    assert(Near(120+(center[1]-120)*Scale+OffsetY,200+(22-8)*.75f-180));
    OOT3D::PauseHeader::Hook header;header.presenting=true;
    assert(header.DungeonIcons(m,iconmodel) && header.ForegroundModel(m,iconmodel));
    std::array<U32,16> draw{};draw[0]=iconmodel;draw[14]=0x1234;
    header.NativeDraw(m,draw,true);
    assert(header.foreground_count==1 && header.foreground[0]==iconmodel && draw[15]==0x1234);
    // Run the earlier dungeon renderer too: it must let the original icon
    // group reach the header, rather than splitting the Skulltula behind it.
    constexpr U32 idesc=0x8900000,ilive=0x14940000;
    m.words[iconmesh]=idesc;m.words[idesc+12]=16;
    m.words[iconmesh+0x124]=0;m.words[iconmesh+0x1a0]=ilive;
    for(unsigned i=0;i<48;++i)m.words[ilive+i*4]=m.words[dst+i*4];
    for(unsigned i=0;i<32;++i)m.F(ilive+16*12+i*4,.5f);
    for(unsigned i=0;i<64;++i)m.F(ilive+16*20+i*4,i%4==3?.8f:.6f);
    for(unsigned i=0;i<12;++i)m.F(iconmodel+12+i*4,identity[i]);
    OOT3D::DungeonSides::Hook sides;draw[0]=iconmodel;
    assert(!sides.NativeDraw(m,draw,true) && sides.used==0);
    for(float fade:{1.f,.5f,0.f,1.f}) {
        world_location_alpha=fade;
        const auto icons=header.DungeonIconGeometry(m,iconmodel);assert(icons && icons->count==4);
        for(unsigned v=0;v<16;++v)assert(Near(icons->colors[v*4+3],.8f*(v>=12?fade:1)));
        // Reused native buffers are untouched; no compounding across frames.
        assert(Near(m.F(ilive+16*20+51*4),.8f));
    }
    // Native cleanup can clear the map controller before pause releases its
    // final image. The header has finished by then, but must still drain its
    // icon queue. Reproduce the exact zero-progress window from the screenshot.
    {
        const auto before_close=m.words;
        m.words[0x54ac4c]=0;dungeon_exit_hold=true;world_location_alpha=0;
        for(U32 map_state:{2u,0u}) {
            m.words[root+0x38]=map_state;
            for(bool owned:{false,true})for(bool material_ready:{false,true}) {
                m.words[iconmodel+0x1dc]=material_ready?0x08a00000:0;
                // Preserve native hidden-item alpha; never invent ownership.
                for(unsigned v=0;v<4;++v)m.F(ilive+16*20+v*16+12,owned?.8f:0.f);
                OOT3D::PauseHeader::Hook closing;closing.presenting=true;closing.slide.progress=0;
                draw={};draw[0]=iconmodel;draw[14]=0x1234;
                closing.NativeDraw(m,draw,true);assert(closing.foreground_count==1);
                const auto geometry=closing.DungeonIconGeometry(m,iconmodel);assert(geometry);
                for(unsigned v=0;v<16;++v)
                    assert(Near(geometry->colors[v*4+3],v>=12?0:v<4 && !owned?0:.8f));
                const auto queued=draw;
                closing.Handle(m,draw,true);
                if(material_ready) {
                    assert(closing.stage==OOT3D::PauseHeader::Hook::Stage::DungeonIconDraw &&
                        closing.dungeon_icons.Pending());
                    assert(closing.dungeon_icons.geometry.colors==geometry->colors);
                    // Even a failed optional mesh allocation must replay the
                    // native model, including on this final closing frame.
                    draw[0]=0;closing.Handle(m,draw,true);
                }
                // Missing optional clone material falls back to the native
                // model draw instead of dropping all three owned-item icons.
                assert(closing.stage==OOT3D::PauseHeader::Hook::Stage::Foreground &&
                    draw[0]==iconmodel && draw[15]==0x3f9680);
                closing.Handle(m,draw,true);
                assert(closing.stage==OOT3D::PauseHeader::Hook::Stage::Idle &&
                    closing.foreground_count==0 && draw==queued);
                assert(DungeonQuadPart(m,keymesh,0,4)==DungeonItems);
                assert(!closing.ForegroundModel(m,keymodel)); // Small keys use the separate side-control pass.
            }
        }
        m.words=before_close;dungeon_exit_hold=false;world_location_alpha=1;
    }
    // Native unavailable/parked Skulltulas remain hidden regardless of pose.
    quad(3,400,8,24,24);assert(CopyPositions(m,iconmesh,dst,src,4*48));
    assert(m.F(dst+3*48)==400 && m.F(dst+3*48+12)==400);
    quad(3,68,8,24,24);poses=before;SetLayout(poses,false);world_location_y=0;world_location_alpha=1;
    assert(DungeonQuadPart(m,keymesh,0,4)==DungeonItems);
    for(unsigned selected=0;selected<9;++selected) {
        m.words[root+0x30]=selected;
        assert(DungeonQuadPart(m,cursormesh,0,4)==(selected<5?DungeonFloors:selected<8?DungeonItems:PartCount));
    }
    quad(0,400,112,42,42); // A hidden/unowned map icon must not be brought back by a pose.
    assert(CopyPositions(m,iconmesh,dst,src,4*48));
    assert(m.F(dst)==400 && m.F(dst+12)==400);
    // Pointer round trips for all floor buttons and dungeon inventory icons.
    for(float width:{400.f,560.f})for(Part part:{DungeonItems,DungeonFloors})for(unsigned i=0;i<(part==DungeonFloors?5u:3u);++i) {
        const float x=part==DungeonFloors?284:31,y=part==DungeonFloors?22+46*i:31+50*i;
        const auto p=AnimatedMove(x,y,part,poses[part]);
        const auto hit=MapPointer(.5f+(p[0]-160)*Scale/width,.5f+((p[1]-120)*Scale+OffsetY)/240,
            width,poses,false,false,true);
        assert(hit && Near((*hit)[0]*320,x) && Near((*hit)[1]*240,y));
    }
    // Spacing moves complete rows, without stretching wells, labels or icons.
    // Offscreen stone backgrounds cannot drag either content group with them.
    poses[DungeonLeft]={-400,0,1};poses[DungeonRight]={400,0,1};
    poses.dungeon_spacing={1.6f,.7f};SetLayout(poses,false);
    for(unsigned row=0;row<4;++row) {
        const float cy=DungeonRowCenter(DungeonItems,row);
        const float dy=(cy-120)*.6f;
        quad(0,13,cy-18,36,36);
        // Test the key counter and its group at the same row anchor as its well.
        assert(CopyPositions(m,keymesh,dst,src,48));
        const auto top=AnimatedMove(13,cy-18+dy,DungeonItems,poses[DungeonItems]);
        assert(Near(m.F(dst),top[0]) && Near(m.F(dst+4),top[1]));
        assert(Near(m.F(dst+12)-m.F(dst),36*poses[DungeonItems].scale));
        assert(Near(m.F(dst+28)-m.F(dst+4),36*poses[DungeonItems].scale));
    }
    for(float width:{400.f,560.f})for(Part part:{DungeonItems,DungeonFloors})
        for(unsigned row=0;row<(part==DungeonFloors?5u:3u);++row) {
            const float cx=part==DungeonFloors?284:31,cy=DungeonRowCenter(part,row);
            const auto p=AnimatedMove(cx,cy+DungeonRowShift(part,row,poses),part,poses[part]);
            const auto hit=MapPointer(.5f+(p[0]-160)*Scale/width,.5f+((p[1]-120)*Scale+OffsetY)/240,
                width,poses,false,false,true);
            assert(hit && Near((*hit)[0]*320,cx) && Near((*hit)[1]*240,cy));
        }
    // Floor cursor fragments share the same row shift as the floor artwork.
    m.words[root+0x30]=2;
    for(unsigned q=0;q<4;++q)quad(q,260+(q%2)*40,27+46*2-16+(q/2)*32,4,4);
    assert(CopyPositions(m,cursormesh,dst,src,4*48));
    const auto cp=AnimatedMove(260,27+46*2-16+(27+46*2-120)*(-.3f),DungeonFloors,poses[DungeonFloors]);
    assert(Near(m.F(dst),cp[0]) && Near(m.F(dst+4),cp[1]));
    {
        const auto saved=m.words;
        poses.dungeon_floor_growth={true,1.5f,1};SetLayout(poses,false);
        floor_growth_animation.progress[2]=1;
        assert(CopyPositions(m,cursormesh,dst,src,4*48));
        const float shift=DungeonRowShift(DungeonFloors,2,poses);
        auto center=AnimatedMove(290,28+46.f*2+shift,DungeonFloors,poses[DungeonFloors]);
        for(unsigned q=0;q<4;++q)for(unsigned i=0;i<12;i+=3) {
            assert(Near(m.F(dst+q*48+4*i),center[0]+(std::bit_cast<float>(saved.at(dst+q*48+4*i))-center[0])*1.5f));
            assert(Near(m.F(dst+q*48+4*i+4),center[1]+(std::bit_cast<float>(saved.at(dst+q*48+4*i+4))-center[1])*1.5f));
        }
        Hook pending_cursor;
        assert(CopyPositions(m,cursormesh,dst,src,4*48,&pending_cursor.pending));
        pending_cursor.Animate(m);
        const auto deferred=m.words;
        assert(CopyPositions(m,cursormesh,dst,src,4*48));
        for(unsigned i=0;i<4*48;i+=4)assert(m.words.at(dst+i)==deferred.at(dst+i));
        poses.dungeon_floor_growth={};SetLayout(poses,false);floor_growth_animation={};m.words=saved;
    }
    constexpr U32 rooms=0x8900000,marker=0x8910000,markermodel=0x8920000,gameplay=0x8930000;
    m.words[0x4fda6c+0x38]=1;m.words[0x55b450+4]=rooms;m.words[rooms]=0x4ebd60;
    m.words[0x4fda6c+0x24]=marker;m.words[marker+4]=markermodel;m.words[markermodel]=0x4ebd60;
    m.words[0x4fda6c+8]=gameplay;m.words[gameplay]=0x4ebd60;
    assert(DungeonContentModel(m,rooms) && DungeonContentModel(m,markermodel));
    assert(!DungeonContentModel(m,gameplay) && !DungeonContentModel(m,model));
    {
        const auto before=m.words;
        constexpr U32 marker_mesh=0x8d00000,marker_helper=0x8d10000,offsets=0x8d20000;
        for(U32 owner_offset:{0x24u,0x2cu}) {
            m.words[0x4fda6c+0x24]=m.words[0x4fda6c+0x2c]=0;
            m.words[0x4fda6c+owner_offset]=marker;
            m.words[markermodel+4]=marker_mesh;m.words[marker+8]=marker_helper;
            m.words[marker_helper]=3;m.words[marker_helper+0x1c]=offsets;
            for(unsigned q=0;q<3;++q) {
                m.F(offsets+q*8,q?400.f:120.f);m.F(offsets+q*8+4,q==2?240.f:0.f);
                quad(q,q?400.f:120.f,q==2?240.f:80.f,16,16);
            }
            const auto fresh=m.words;
            assert(CopyDungeonMarkers(m,marker_mesh,dst,src,3*48));
            for(unsigned q=0;q<3;++q)for(unsigned i=0;i<12;++i) {
                assert(m.words[src+q*48+i*4]==fresh.at(src+q*48+i*4));
                assert(m.words[dst+q*48+i*4]==fresh.at(src+q*48+(q?i%3:i)*4));
            }
            // A fresh update makes the same marker visible on a new floor;
            // there is no cached hidden state or mutation of native offsets.
            m.F(offsets+8,140);m.F(offsets+12,90);quad(1,140,90,16,16);
            assert(CopyDungeonMarkers(m,marker_mesh,dst,src,3*48));
            assert(m.F(dst+48+3*4)==156);
            const auto unchanged=m.words;
            assert(!CopyDungeonMarkers(m,mesh,dst,src,3*48));
            assert(!CopyDungeonMarkers(m,marker_mesh,dst,src,2*48));
            assert(m.words==unchanged);
            m.words[0x4fda6c+0x50]=0;
            assert(!CopyDungeonMarkers(m,marker_mesh,dst,src,3*48));
            m.words[0x4fda6c+0x50]=1;
        }
        m.words=before;
    }
    // Dimming covers the separately drawn floor-plan image as well as the
    // parchment; markers, sidebar art and material alpha remain untouched.
    constexpr U32 rgba=0x8c00000;
    for(unsigned c=0;c<4;++c)m.F(rgba+4*c,.8f);
    assert(!DungeonRoomMaterial(m,markermodel,rgba));
    assert(!DungeonRoomMaterial(m,gameplay,rgba));
    assert(DungeonRoomMaterial(m,rooms,rgba));
    for(unsigned c=0;c<3;++c)assert(Near(m.F(rgba+4*c),.8f*poses.dungeon_brightness));
    assert(m.F(rgba+12)==.8f);
    for(bool wide:{false,true}) {
        auto far=poses;far[DungeonRight]={400,0,3};
        const float edge=(400-(wide?240*OOT3D::Widescreen::Aspect:400))/2;
        const float travel=OOT3D::PauseHeader::PanelTravel(far,wide);
        assert(200+Scale*(288-160+42*3)+400-travel<edge);
    }
    // All available-floor combinations, including a single floor in any native
    // row and gaps without a dungeon map. The native floor order/selection is
    // unchanged; artwork, labels, marker and cursor move as complete rows.
    assert(m.Read(0x442a90)==0x55b638 && m.F(0x4d4e94+40+4)==8.f);
    poses[DungeonFloors]={28.2f,-15.2f,.65f};poses.dungeon_floor_line.pose.y=12.2f;
    motion_x=motion_slots=motion_y=0;
    for(unsigned mask=1;mask<32;++mask)for(float spacing:{.7f,1.f,1.4f}) {
        for(unsigned row=0;row<5;++row)m.words[0x55b638+row*4]=(mask>>row)&1;
        assert(DungeonFloorRows(m)==mask);
        poses.dungeon_spacing[1]=spacing;SetLayout(poses,false);
        for(unsigned q=0;q<50;++q)quad(q,0,0,0,0);
        unsigned first=0,last=0;
        while(!(mask&(1u<<first)))++first;
        for(unsigned row=first;row<5;++row)if(mask&(1u<<row)) {
            last=row;
            quad(21+row,262,6+46*row,56,44);
            quad(26+row,264,8+46*row,52,40);
            quad(31+row,274,17+46*row,32,22);
        }
        quad(36,261,19+46*last,16,16);
        assert(CopyPositions(m,mesh,dst,src,50*48));
        const float center=(m.F(dst+(26+first)*48+4)+m.F(dst+(26+last)*48+28))*.5f;
        const float line_center=120+poses[DungeonFloors].y/Scale+poses.dungeon_floor_line.pose.y/Scale;
        // Fit every available-row mask to the actual transformed floor wells,
        // including sparse lists, custom spacing and the authored Y offset.
        auto bar_layout=poses;bar_layout.dungeon_floor_line.height=600;
        const auto bar=OOT3D::DungeonFloorLine::Make(bar_layout,1,DungeonFloorRows(m));
        float top=1e9f,bottom=-1e9f;
        for(unsigned v=0;v<bar.count*4;++v) {
            top=std::min(top,bar.positions[v*3+1]);bottom=std::max(bottom,bar.positions[v*3+1]);
        }
        const float first_well=120+(m.F(dst+(21+first)*48+4)-120)*Scale+OffsetY;
        const float last_well=120+(m.F(dst+(21+last)*48+28)-120)*Scale+OffsetY;
        assert(Near(first_well-top,8*Scale*poses[DungeonFloors].scale));
        assert(Near(bottom-last_well,8*Scale*poses[DungeonFloors].scale));
        bar_layout.dungeon_floor_growth.enabled=true;bar_layout.dungeon_floor_growth.size=2;
        assert(OOT3D::DungeonFloorLine::Make(bar_layout,1,mask).positions==bar.positions);
        assert(!OOT3D::DungeonFloorLine::Make(bar_layout,1,0).count);
        assert(Near(center,line_center));
        for(unsigned row=0;row<5;++row)if(mask&(1u<<row)) {
            const float native_y=28+46*row;
            const float y=(m.F(dst+(26+row)*48+4)+m.F(dst+(26+row)*48+28))*.5f;
            assert(Near(m.F(dst+(26+row)*48+28)-m.F(dst+(26+row)*48+4),40*.65f));
            const float x=Move(290,native_y,DungeonFloors,poses[DungeonFloors])[0];
            for(float width:{400.f,560.f}) {
                const auto hit=MapPointer(.5f+(x-160)*Scale/width,.5f+((y-120)*Scale+OffsetY)/240,
                    width,poses,false,false,true,mask);
                assert(hit && Near((*hit)[0]*320,290) && Near((*hit)[1]*240,native_y));
            }
        }
        const float shift=DungeonRowShift(DungeonFloors,last,poses,mask);
        const auto marker=Move(261,19+46*last+shift,DungeonFloors,poses[DungeonFloors]);
        assert(Near(m.F(dst+36*48+4),marker[1]));
        m.words[root+0x30]=last;
        for(unsigned q=0;q<4;++q)quad(q,260+(q%2)*40,27+46*last-16+(q/2)*32,4,4);
        assert(CopyPositions(m,cursormesh,dst,src,4*48));
        const auto cursor_position=Move(260,27+46*last-16+shift,DungeonFloors,poses[DungeonFloors]);
        assert(Near(m.F(dst+4),cursor_position[1]));
    }
    // Both floor markers and all three selectable item rows use the same
    // clock, including the independently scaled cursor on either side.
    {
        const auto saved=m.words;const auto saved_poses=poses;
        motion_x=-17;
        for(unsigned row=0;row<5;++row)m.words[0x55b638+4*row]=1;
        const auto verify=[&](U32 target,unsigned count,unsigned selected,bool cursor_draw,
                              bool enabled,float cursor_scale) {
            poses.dungeon_floor_growth={false,1.5f,1};poses.dungeon_cursor_scale=1;
            SetLayout(poses,false);floor_growth_animation={};
            m.words[root+0x30]=selected;
            assert(CopyPositions(m,target,dst,src,count*48));
            const auto baseline=m.words;
            poses.dungeon_floor_growth.enabled=enabled;poses.dungeon_cursor_scale=cursor_scale;
            SetLayout(poses,false);
            floor_growth_animation.Update(1000000,true,false,selected,31,poses.dungeon_floor_growth);
            floor_growth_animation.Update(1075000,true,false,selected,31,poses.dungeon_floor_growth);
            assert(CopyPositions(m,target,dst,src,count*48));
            const auto immediate=m.words;
            for(unsigned q=0;q<count;++q) {
                if(target==mesh && q!=36 && q!=37)continue;
                if(target==iconmesh && q==3) {
                    for(unsigned i=0;i<12;++i)assert(m.words.at(dst+q*48+4*i)==baseline.at(dst+q*48+4*i));
                    continue;
                }
                const bool floor=target==mesh || (cursor_draw && selected<5);
                const unsigned row=cursor_draw?(floor?selected:selected-5):target==mesh?(q==36?2:3):2-q;
                const unsigned selection=floor?row:row+5;
                const auto part=floor?DungeonFloors:DungeonItems;
                const auto center=AnimatedMove(floor?290.f:32.f,
                    (floor?28+46.f*row:DungeonRowCenter(part,row))+DungeonRowShift(part,row,poses,31),part,poses[part]);
                const float factor=(enabled && selected==selection?1.25f:1)*(cursor_draw?cursor_scale:1);
                for(unsigned i=0;i<12;i+=3)for(unsigned axis=0;axis<2;++axis) {
                    const auto address=dst+q*48+4*(i+axis);
                    const float original=std::bit_cast<float>(baseline.at(address));
                    assert(Near(m.F(address),center[axis]+(original-center[axis])*factor));
                }
            }
            Hook deferred;assert(CopyPositions(m,target,dst,src,count*48,&deferred.pending));
            deferred.Animate(m);
            for(unsigned i=0;i<count*48;i+=4)assert(Near(m.F(dst+i),std::bit_cast<float>(immediate.at(dst+i))));
        };
        for(unsigned q=0;q<50;++q)quad(q,0,0,0,0);
        quad(36,261,19+46*2,16,16);quad(37,307,19+46*3,16,16);
        for(unsigned selected:{2u,3u,5u})verify(mesh,50,selected,false,true,1);
        for(unsigned q=0;q<4;++q)quad(q,q==3?68:10,q==3?8:112-50*q,24,24);
        for(unsigned selected=5;selected<8;++selected)verify(iconmesh,4,selected,false,true,1);
        for(unsigned selected:{2u,5u,6u,7u})for(bool enabled:{false,true})for(float scale:{.5f,1.f,2.f}) {
            const bool floor=selected<5;
            const float cx=floor?290.f:32.f,cy=floor?28+46.f*selected:DungeonRowCenter(DungeonItems,selected-5);
            for(unsigned q=0;q<4;++q)quad(q,cx-24+(q%2)*44,cy-24+(q/2)*44,4,4);
            verify(cursormesh,4,selected,true,enabled,scale);
        }
        poses=saved_poses;SetLayout(poses,false);m.words=saved;floor_growth_animation={};motion_x=0;
    }
    m.words[0x4fda6c+0x50]=0;
    assert(!DungeonPage(m) && !DungeonContentModel(m,rooms) && !DungeonQuadPart(m,mesh,0,50));
    m.words[0x4fda6c+0x50]=1;m.words[0x5066f8+0x34]=2;
    assert(!DungeonPage(m));
    std::cout<<"Dungeon atlas, map brightness, independent stones and spaced contents, outward shadows, independent poses, icons, cursor, native hiding, input and model ownership passed\n";
}
