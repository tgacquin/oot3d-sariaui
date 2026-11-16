// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hud_buttons.h"
#include "oot3d/pause_prompt_assets.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
namespace B=OOT3D::HUDButtons;
namespace P=OOT3D::PausePrompts;
using OOT3D::TitleScreen::U32;
std::vector<std::uint8_t> Read(const std::string& path) {
    std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
struct Memory {
    std::map<U32,U32> words;
    unsigned null_counter_reads=0;
    std::optional<U32> Read(U32 at) {if(at==0x10)++null_counter_reads;auto i=words.find(at);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 at,U32 value) {assert(words.contains(at));words[at]=value;}
    void Allocate(U32 at,unsigned bytes) {for(unsigned i=0;i<bytes;i+=4)words[at+i]=0;}
};
int main(int argc,char**argv) {
    assert(argc==2);const std::string root=argv[1];
    const auto atlas=Read(root+"/message/parts.ctxb"),font=Read(root+"/message/us/ltn16.qbf"),
        cursor=Read(root+"/menu/01_US_ENGLISH/menu_cursor00.ctxb");
    const auto assets=P::MakeAssets(atlas,font,cursor);assert(assets && assets->cursor_ready);
    for(unsigned i=0;i<assets->cursor.size();++i)assert(assets->cursor[i]==OOT3D::MapFrame::LE32(cursor,72+i*4));
    auto bad=cursor;bad[44]=0;assert(!P::MakeAssets(atlas,font,bad));
    B::Layout layout;const auto g=B::MakeGeometry(layout,*assets,.4f,false);
    assert(g.circles.count==9 && g.text.count==4);
    for(unsigned i=3;i<g.circles.count*16;i+=4)assert(g.circles.colors[i]==.4f);
    for(unsigned i=0;i<4;++i) {
        const float x=(i==0 || i==2)?64.5f:80.5f,y=i<2?48.5f:32.5f;
        assert(g.text.uvs[i*8]*128==x && (1-g.text.uvs[i*8+1])*128==y);
        assert(g.text.colors[i*16+3]==.4f);
    }
    // Every item bubble mirrors A's lower half; both halves share the seam.
    for(unsigned i=0;i<4;++i) {
        const unsigned top=1+2*i,bottom=top+1;
        for(unsigned q:{top,bottom}) {
            assert(g.circles.uvs[q*8]*256==64.5f);
            assert(g.circles.uvs[q*8+2]*256==101.5f);
        }
        assert((1-g.circles.uvs[top*8+1])*256==37.5f);
        assert((1-g.circles.uvs[top*8+5])*256==19.f);
        assert(g.circles.uvs[top*8+5]==g.circles.uvs[bottom*8+1]);
        assert(g.circles.uvs[top*8+1]==g.circles.uvs[bottom*8+5]);
        assert(g.circles.positions[top*12+7]==g.circles.positions[bottom*12+1]);
        const auto p=*B::ButtonPose(layout,i+B::X);
        assert(g.circles.positions[bottom*12+1]==p.y);
        assert(std::abs(g.circles.positions[bottom*12+7]-g.circles.positions[top*12+1]-40.f*47/48)<.0001f);
    }
    assert((1-g.circles.uvs[1])*256==9.5f);
    assert((1-g.circles.uvs[5])*256==55.5f);
    assert(B::MakeGeometry(layout,*assets,0,false).circles.count==0);
    assert(B::MakeGeometry(layout,*assets,1,true).circles.count==8); // Preserve native contextual B.
    {
        auto small=layout;small.item_scale=.5f;small.item_x=-40;small.item_y=10;
        const auto half=B::MakeGeometry(small,*assets,1,false);
        const auto center=[](const auto& mesh,unsigned q) {
            return std::array{(mesh.positions[q*12]+mesh.positions[q*12+3])/2,
                              mesh.positions[(q+1)*12+1]};
        };
        // Bubble centers converge along with their widths when shrinking
        // the whole diamond; the original B remains independent.
        std::array<std::array<float,2>,4> centers{};
        for(unsigned i=0;i<4;++i) {
            const unsigned q=1+2*i;
            centers[i]=center(half.circles,q);
            assert(half.circles.positions[q*12+3]-half.circles.positions[q*12]==20);
            assert(half.text.positions[i*12+3]-half.text.positions[i*12]==8);
        }
        assert(centers[2][1]-centers[0][1]==34); // X opposite I, formerly 68.
        assert(centers[3][0]-centers[1][0]==34); // Y opposite II.
        assert((centers[0][0]+centers[2][0])/2==264);
        assert((centers[0][1]+centers[2][1])/2==168);
        for(unsigned v=0;v<12;++v)assert(half.circles.positions[v]==g.circles.positions[v]);
    }
    layout.buttons[B::X].pose={100,80,2};const auto moved=B::MakeGeometry(layout,*assets,1,false);
    for(unsigned q=1;q<=2;++q)for(unsigned v=0;v<12;v+=3) {
        assert(moved.circles.positions[q*12+v]==100+2*(g.circles.positions[q*12+v]-304));
        assert(std::abs(moved.circles.positions[q*12+v+1]-(80+2*(g.circles.positions[q*12+v+1]-124)))<.0001f);
    }
    for(unsigned v=0;v<12;v+=3)assert(moved.text.positions[v]==100+2*(g.text.positions[v]-304));
    layout.buttons[B::X].enabled=false;assert(B::MakeGeometry(layout,*assets,1,false).text.count==3);
    Memory m;constexpr U32 helper=0x18000000,vertices=0x18100000;
    m.words[OOT3D::HUD::Action+0x18]=helper;m.words[helper]=8;m.words[helper+0x10]=vertices;
    const auto place=[&](float dx,float dy) {
        const std::array<float,12> v{312+dx,192+dy,0,360+dx,192+dy,0,312+dx,239+dy,0,360+dx,239+dy,0};
        for(unsigned i=0;i<v.size();++i)m.words[vertices+7*48+i*4]=std::bit_cast<U32>(v[i]);
    };
    place(0,0);assert(B::NativeBVisible(m));place(420,240);assert(!B::NativeBVisible(m));
    for(unsigned mode:{1,2,3,4,5}) {
        m.words[OOT3D::HUD::Action+0x48]=mode;
        assert(B::NativeBVisible(m)); // Moving native B offscreen cannot reveal a second sword.
    }
    m.words[OOT3D::HUD::Action+0x48]=0;
    constexpr U32 main_helper=0x18200000,main_vertices=0x18300000,uvs=0x18400000,colors=0x18500000;
    m.words[OOT3D::HUD::Main+4]=main_helper;m.words[main_helper]=92;
    m.words[main_helper+0xc]=main_vertices;m.words[main_helper+0x14]=uvs;m.words[main_helper+0x18]=colors;
    const auto floats=[&](U32 at,const auto& values) {
        for(unsigned i=0;i<values.size();++i)m.words[at+4*i]=std::bit_cast<U32>(values[i]);
    };
    const std::array<float,12> sword_positions{10,20,0,42,20,0,10,52,0,42,52,0};
    const std::array<float,8> sword_uvs{.1f,.2f,.3f,.2f,.1f,.4f,.3f,.4f};
    std::array<float,16> sword_colors;sword_colors.fill(1);
    for(unsigned c=3;c<16;c+=4)sword_colors[c]=.5f;
    floats(main_vertices+35*48,sword_positions);floats(uvs+35*32,sword_uvs);floats(colors+35*64,sword_colors);
    auto sword=B::Sword(m,layout,.4f,false);
    assert(sword.count==1 && sword.uvs==sword_uvs);
    for(unsigned c=3;c<16;c+=4)assert(sword.colors[c]==.2f);
    assert(sword.positions[3]-sword.positions[0]==30);
    assert((sword.positions[0]+sword.positions[3])/2==layout.buttons[B::B].pose.x);
    layout.buttons[B::B].pose={100,80,2};
    sword=B::Sword(m,layout,1,false);
    assert(sword.positions[0]==70 && sword.positions[3]==130);
    assert(sword.positions[1]==60 && sword.positions[7]==120);
    auto new_uvs=sword_uvs;new_uvs[0]=.6f;floats(uvs+35*32,new_uvs);
    assert(B::Sword(m,layout,1,false).uvs==new_uvs); // Follow equipment changes.
    assert(!B::Sword(m,layout,1,true).count && !B::Sword(m,layout,0,false).count);
    layout.buttons[B::B].enabled=false;assert(!B::Sword(m,layout,1,false).count);
    layout.buttons[B::B].enabled=true;
    floats(main_vertices+35*48,std::array<float,12>{});
    assert(!B::Sword(m,layout,1,false).count);
    floats(main_vertices+35*48,sword_positions);
    constexpr U32 controller=0x18600000,item_helper=0x18700000,item_pos=0x18800000,
        item_uvs=0x18900000,item_colors=0x18a00000;
    m.words[OOT3D::HUD::Main+0x14]=controller;m.words[controller+0x10c]=item_helper;
    m.words[item_helper]=5;m.words[item_helper+0xc]=item_pos;
    m.words[item_helper+0x14]=item_uvs;m.words[item_helper+0x18]=item_colors;
    for(unsigned q=0;q<5;++q) {
        floats(item_pos+q*48,sword_positions);
        auto uv=sword_uvs;for(unsigned v=0;v<8;v+=2)uv[v]+=.1f*q;
        floats(item_uvs+q*32,uv);floats(item_colors+q*64,sword_colors);
    }
    layout={};layout.item_scale=.5f;layout.item_x=-40;layout.item_y=10;
    const auto equipped=B::Items(m,layout,.4f);
    assert(equipped.count==4);
    constexpr std::array<unsigned,4> slots{3,4,1,2};
    for(unsigned i=0;i<4;++i) {
        for(unsigned v=0;v<8;++v)
            assert(equipped.uvs[i*8+v]==std::bit_cast<float>(m.words[item_uvs+slots[i]*32+v*4]));
        assert(equipped.colors[i*16+3]==.2f);
        assert(equipped.positions[i*12+3]-equipped.positions[i*12]==13);
    }
    assert((equipped.positions[0]+equipped.positions[3])/2==264);
    assert((equipped.positions[1]+equipped.positions[7])/2==153.5f);
    floats(item_uvs+3*32,std::array<float,8>{}); // Empty X slot.
    assert(B::Items(m,layout,1).count==3);
    floats(item_uvs+3*32,sword_uvs);assert(B::Items(m,layout,1).count==4);
    layout.show_items=false;
    assert(!B::Items(m,layout,1).count && !B::Sword(m,layout,1,false).count);
    assert(B::MakeGeometry(layout,*assets,1,false).circles.count==9);
    layout.show_items=true;layout.buttons[B::Y].enabled=false;
    assert(B::Items(m,layout,1).count==3 && !B::Items(m,layout,0).count);

    // Live ammunition uses the same slot mapping, native digit art and colors.
    // Each counter retains its native sampler, tint and visibility.
    constexpr U32 counters=0x18b00000;
    m.words[controller+0x434]=0x01010100;m.words[controller+0x438]=1;
    for(unsigned slot=1;slot<=4;++slot) {
        const U32 c=counters+slot*0x1000,h=c+0x100;
        m.words[controller+0xc+slot*4]=c;m.words[c+8]=h;
        m.words[h]=2;m.words[h+0xc]=c+0x200;
        m.words[h+0x14]=c+0x300;m.words[h+0x18]=c+0x400;
        P::QuadGeometry<2> digits;
        digits.Add(300,70,8,12,.1f*slot,1,.1f*slot+.05f,.8f);
        digits.Add(292,70,8,12,0,1,.05f,.8f);
        for(unsigned v=0;v<8;++v) {
            digits.colors[v*4]=.3f;digits.colors[v*4+3]=.5f;
        }
        floats(c+0x200,digits.positions);floats(c+0x300,digits.uvs);floats(c+0x400,digits.colors);
    }
    layout={};
    for(unsigned i=B::X;i<B::Count;++i) {
        const auto digits=B::ItemCount(m,layout,i,.4f);
        assert(digits.count==2 && digits.colors[0]==.3f && digits.colors[3]==.2f);
        assert(digits.uvs[0]==.1f*slots[i-B::X]);
        const auto p=*B::ButtonPose(layout,i);
        assert(digits.positions[3]==p.x+18 && digits.positions[7]==p.y+18);
        assert(digits.positions[3]-digits.positions[0]==6.5f);
    }
    const auto digits=B::ItemCount(m,layout,B::X,1);
    // HD ammo transforms are local to the digits, and the master disables
    // them without changing native tint/alpha, UVs or item icons.
    auto hd=OOT3D::HUD::GetLayout();hd.hd_ammo_text={3,4,2};hd.hd_ammo_spacing=1;
    OOT3D::HUD::SetLayout(hd,false);
    const auto hd_digits=B::ItemCount(m,layout,B::X,1);
    assert(hd_digits.positions[3]==digits.positions[3]+3);
    assert(hd_digits.positions[7]==digits.positions[7]+4);
    assert(hd_digits.positions[3]-hd_digits.positions[0]==13);
    assert(hd_digits.uvs==digits.uvs && hd_digits.colors==digits.colors);
    OOT3D::HUD::PublishNumberGeometry(hd_digits,OOT3D::HUD::NumberKind::Ammo);
    auto hash=OOT3D::HUD::HashStart;
    for(float v:hd_digits.positions)hash=OOT3D::HUD::HashWord(hash,std::bit_cast<U32>(v));
    for(float v:hd_digits.uvs)hash=OOT3D::HUD::HashWord(hash,std::bit_cast<U32>(v));
    for(float v:hd_digits.colors)hash=OOT3D::HUD::HashWord(hash,std::bit_cast<U32>(v));
    assert(OOT3D::HUD::FindNumberMesh(hash,4)==OOT3D::HUD::NumberKind::Ammo);
    hd.hd_numbers=false;OOT3D::HUD::SetLayout(hd,false);
    assert(B::ItemCount(m,layout,B::X,1).positions==digits.positions);
    // Native B ammo applies spacing before its button transform. The custom
    // item-button path must give the same advance relative to digit size,
    // including the 26/32 fit and independently edited group/counter scales.
    const auto old_layout=layout;
    for(float button_scale:{.5f,1.f,2.f})for(float count_scale:{.75f,1.5f})
        for(float spacing:{-2.f,0.f,3.f})for(float hd_scale:{.8f,1.5f}) {
            layout=old_layout;layout.item_scale=button_scale;layout.counter_scale=count_scale;
            hd.hd_numbers=true;hd.hd_ammo_text={3,4,hd_scale};hd.hd_ammo_spacing=spacing;
            OOT3D::HUD::SetLayout(hd,false);
            const auto regular=B::ItemCount(m,layout,B::X,1);
            // The fixture has two native 8px digits, at x=300 and x=292.
            P::QuadGeometry<2> native;
            native.Add(300,70,8,12,0,1,.05f,.8f);
            native.Add(292,70,8,12,0,1,.05f,.8f);
            OOT3D::HUD::NumberPositions(native.positions,hd.hd_ammo_text,spacing,true);
            const float regular_width=regular.positions[3]-regular.positions[0];
            const float native_width=native.positions[3]-native.positions[0];
            const float regular_advance=(regular.positions[0]-regular.positions[12])/regular_width;
            const float native_advance=(native.positions[0]-native.positions[12])/native_width;
            assert(std::abs(regular_advance-native_advance)<.0001f);
            hd.hd_ammo_spacing=0;OOT3D::HUD::SetLayout(hd,false);
            const auto unspaced=B::ItemCount(m,layout,B::X,1);
            assert(regular.positions[3]==unspaced.positions[3]); // Right edge.
            assert(regular.positions[7]==unspaced.positions[7]); // Bottom edge.
            assert(regular.uvs==unspaced.uvs && regular.colors==unspaced.colors);
        }
    layout=old_layout;
    OOT3D::HUD::SetLayout({},false);
    const auto icons_before=B::Items(m,layout,1);
    layout.counter_scale=1.5f;
    const auto enlarged=B::ItemCount(m,layout,B::X,1);
    assert(enlarged.positions[3]==digits.positions[3] && enlarged.positions[7]==digits.positions[7]);
    assert(enlarged.positions[3]-enlarged.positions[0]==9.75f);
    assert(enlarged.uvs==digits.uvs && enlarged.colors==digits.colors);
    assert(B::Items(m,layout,1).positions==icons_before.positions);
    layout.counter_x=-3;layout.counter_y=5;
    const auto moved_count=B::ItemCount(m,layout,B::X,1);
    for(unsigned v=0;v<moved_count.count*12;v+=3) {
        assert(moved_count.positions[v]==enlarged.positions[v]-3);
        assert(moved_count.positions[v+1]==enlarged.positions[v+1]+5);
    }
    assert(moved_count.uvs==enlarged.uvs && moved_count.colors==enlarged.colors);
    assert(B::Items(m,layout,1).positions==icons_before.positions);
    layout.counter_x=layout.counter_y=0;
    layout.counter_scale=1;
    layout.item_icons={4,-6,2};layout.item_scale=.5f;
    const auto scaled=B::ItemCount(m,layout,B::X,1);
    const auto bubble=*B::ButtonPose(layout,B::X);
    assert(scaled.positions[3]-scaled.positions[0]==digits.positions[3]-digits.positions[0]);
    assert(scaled.positions[3]==bubble.x+2+18 && scaled.positions[7]==bubble.y-.5f+13);
    const U32 xc=counters+3*0x1000;
    floats(xc+0x300+32,std::array<float,8>{}); // Unused leading digit.
    assert(B::ItemCount(m,layout,B::X,1).count==1);
    const auto count_uv=B::ItemCount(m,layout,B::X,1).uvs[0];
    m.words[xc+0x300]=std::bit_cast<U32>(count_uv+.01f); // Ammo changed this frame.
    assert(B::ItemCount(m,layout,B::X,1).uvs[0]==count_uv+.01f);
    m.words[controller+0x434]=0x00010100; // Non-countable X item, stale counter still allocated.
    assert(!B::ItemCount(m,layout,B::X,1).count);
    m.words[controller+0x434]=0x01010100;
    layout.show_items=false;assert(!B::ItemCount(m,layout,B::X,1).count);
    layout.show_items=true;layout.buttons[B::X].enabled=false;
    assert(!B::ItemCount(m,layout,B::X,1).count && !B::ItemCount(m,layout,B::Y,0).count);
    layout={};

    // Exercise all eight draws. Icon meshes must use A's projection with the
    // live native icon samplers, without writing any original model or atlas.
    constexpr U32 action_model=0x19000000,action_mesh=0x19010000,descriptor=0x19020000,
        sword_model=0x19030000,sword_mesh=0x19040000,item_model=0x19050000,item_mesh=0x19060000;
    m.Allocate(descriptor,0x120);
    for(U32 model:{action_model,sword_model,item_model}) {
        m.Allocate(model,0x1e4);m.words[model]=0x4ebd60;m.words[model+0x1dc]=0x19070000;
    }
    const auto bind=[&](U32 model,U32 mesh,U32 seed) {
        m.Allocate(mesh,0x1c0);m.words[model+4]=mesh;m.words[mesh]=descriptor;
        for(unsigned i=0;i<24;++i)m.words[mesh+0x13c+i*4]=seed+i;
    };
    bind(action_model,action_mesh,100);bind(sword_model,sword_mesh,200);bind(item_model,item_mesh,300);
    for(unsigned slot=1;slot<=4;++slot) {
        const U32 model=0x19100000+slot*0x1000,mesh=0x19200000+slot*0x1000;
        m.Allocate(model,0x1e4);m.words[model]=0x4ebd60;
        bind(model,mesh,400+slot*100);m.words[counters+slot*0x1000+0x10]=model;
    }
    m.words[action_model+0x18]=std::bit_cast<U32>(17.f);
    m.words[OOT3D::HUD::Action+0x20]=action_model;
    m.words[OOT3D::HUD::Main+0xc]=sword_model;m.words[controller+8]=item_model;
    const auto native=m.words;
    std::atomic_store(&P::assets,std::shared_ptr<const P::Assets>(std::make_shared<P::Assets>(*assets)));
    B::SetLayout({},false);
    B::Hook hook;std::array<U32,16> regs{};regs[15]=0x2fea3c;regs[14]=0x2fe000;
    const auto saved=regs;hook.Begin(m,regs,0x2fea38,1);assert(hook.Pending());
    U32 allocation=0x1a000000,created=0x1b000000;unsigned draws=0;
    const auto step=[&](auto& mesh,const std::optional<B::Sampler>& expected) {
        using H=std::remove_reference_t<decltype(mesh)>;
        if(mesh.stage==H::Stage::Allocate) {m.Allocate(allocation,H::AllocationSize);regs[0]=allocation;allocation+=0x10000;}
        if(mesh.stage==H::Stage::TextureAllocate) {m.Allocate(0x14000000,H::TextureBytes);regs[0]=0x14000000;}
        if(mesh.stage==H::Stage::Mesh) {
            m.words[mesh.block]=mesh.block+H::Descriptor;
            m.words[mesh.block+0x124]=0;m.words[mesh.block+0x128]=2;
        }
        if(mesh.stage==H::Stage::Model) {
            m.Allocate(created,0x1e4);m.words[created]=0x4ebd60;m.words[created+8]=created+0x1000;m.words[created+0x1358]=0x19070000;
            regs[0]=created;created+=0x10000;
        }
        if(mesh.Drawing()) {
            ++draws;
            if(expected) {
                for(unsigned i=0;i<24;++i)assert(m.words[mesh.block+0x13c+i*4]==(*expected)[i]);
                assert(m.words[mesh.model+0x18]==m.words[action_model+0x18]);
                assert(mesh.source==action_model);
            }
        }
        hook.Resume(m,regs);
    };
    unsigned steps=0;
    while(hook.Pending()) {
        assert(++steps<100);
        if(hook.circles.Pending())step(hook.circles,{});
        else if(hook.text.Pending())step(hook.text,{});
        else if(hook.sword.Pending())step(hook.sword,hook.sword_sampler);
        else if(hook.items.Pending())step(hook.items,hook.item_sampler);
        else step(hook.counters[hook.counter_index],hook.counter_samplers[hook.counter_index]);
    }
    assert(draws==8 && regs==saved);
    for(const auto [at,word]:native)assert(m.words.at(at)==word);
    // Empty/non-ammunition slots deliberately have no counter. The complete
    // draw hook must not turn that absence into a read of null+0x10.
    m.words[controller+0x434]=m.words[controller+0x438]=0;
    m.null_counter_reads=0;regs=saved;hook.Begin(m,regs,0x2fea38,1);
    assert(m.null_counter_reads==0);
    for(const auto& sampler:hook.counter_samplers)assert(!sampler);
    std::cout<<"HUD buttons: live item mapping, empty slots, group transforms, fades, hide toggle, native samplers and draw lifecycle passed\n";
}
