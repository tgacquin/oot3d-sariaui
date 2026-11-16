// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_shadow.h"
#include "oot3d/gear_panel.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::PauseShadow;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {
        auto i=words.find(at);return i==words.end()?std::nullopt:std::optional<U32>{i->second};
    }
    void Write(U32 at,U32 word) {assert(words.contains(at));words[at]=word;}
};
int main(int argc,char** argv) {
    Memory m;
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
        for(U32 at=0x100000;file.read(reinterpret_cast<char*>(&word),4);at+=4)m.words[at]=word;
        assert(PauseHeader::CodeMatches(m));m.words.clear();
    }
    // Texture is black, with an opaque near edge, smooth monotonic falloff,
    // and a completely transparent far edge. Every tiled row is identical.
    for(unsigned y=0;y<8;++y) {
        U32 last=255;
        for(unsigned x=0;x<64;++x) {
            unsigned morton=0;
            for(unsigned bit=0;bit<3;++bit)morton|=((x>>bit)&1)<<(bit*2)|((y>>bit)&1)<<(bit*2+1);
            const auto a=Assets::PanelShadow[(x/8)*64+morton];
            assert(a<=last && a<=255);if(x==0)assert(a==255);if(x==63)assert(a==0);last=a;
        }
    }
    // Stone shadows borrow exact native UVs, and follow the already-uploaded
    // small/selected geometry on both pages (including editor transforms).
    for(bool items:{true,false})for(float size:{1.f,1.5f}) {
        Memory tabs;
        constexpr U32 play=0x8100000,model=0x8200000,mesh=0x8300000,
            helper=0x8400000,desc=0x8500000,live=0x14900000;
        const U32 root=items?PauseHeader::Items:0x50446c,count=items?37:41;
        tabs.words[PauseMenu::UI+0xc]=play;tabs.words[PauseMenu::UI+0x14]=1;
        tabs.words[PauseMenu::UI+0x20]=0;tabs.words[PauseMenu::UI+0x24]=1;tabs.words[play+0x100]=3;
        tabs.words[0x5066f8+0x34]=items?2:0;tabs.words[0x50446c+0x18]=items?0:2;
        tabs.words[root+(items?0x20:4)]=model;tabs.words[root+(items?0x1c:0)]=mesh;
        tabs.words[root+(items?0x24:0xc)]=helper;tabs.words[helper]=count;
        tabs.words[model]=0x4ebd60;tabs.words[model+4]=mesh;tabs.words[mesh]=desc;
        tabs.words[desc+12]=count*4;tabs.words[mesh+0x124]=1;tabs.words[mesh+0x1a4]=live;
        const auto floats=[&](U32 at,const auto& values) {
            for(unsigned i=0;i<values.size();++i)tabs.words[at+i*4]=std::bit_cast<U32>(values[i]);
        };
        floats(model+12,std::array<float,12>{1,0,0,7,0,1,0,-9,0,0,1,0});
        const auto indices=items?std::array{9u,11u}:std::array{21u,23u};
        const std::array<float,8> uv{.2f,.8f,.4f,.8f,.2f,.6f,.4f,.6f};
        for(unsigned q:indices) {
            floats(live+q*48,std::array<float,12>{80,190,0,80+54*size,190,0,
                80,190+34*size,0,80+54*size,190+34*size,0});
            floats(live+count*48+q*32,uv);
            std::array<float,16> color;color.fill(.6f);floats(live+count*80+q*64,color);
        }
        const auto original=tabs.words;
        auto shadow=PauseMenu::Layout{}.tab_shadow;
        const auto stone=TabHook::Make(tabs,model,shadow);assert(stone.count==32);
        for(unsigned q=0;q<32;++q) {
            assert(std::abs(stone.positions[q*12+3]-stone.positions[q*12]-54*size*PauseMenu::Scale)<.001f);
            assert(std::abs(stone.positions[q*12+7]-stone.positions[q*12+1]-34*size*PauseMenu::Scale)<.001f);
            for(unsigned i=0;i<8;++i)assert(stone.uvs[q*8+i]==uv[i]);
            for(unsigned i=0;i<16;++i)assert(i%4==3?stone.colors[q*16+i]>0 && stone.colors[q*16+i]<.6f:stone.colors[q*16+i]==0);
        }
        assert(std::abs(stone.positions[0]-(200+(87-160)*PauseMenu::Scale+1.5f))<.001f);
        assert(tabs.words==original);
        shadow.enabled=false;assert(TabHook::Make(tabs,model,shadow).count==0);
        shadow.enabled=true;shadow.strength=0;assert(TabHook::Make(tabs,model,shadow).count==0);
        shadow.strength=.5f;assert(TabHook::Make(tabs,model+4,shadow).count==0);
        for(unsigned q:indices)floats(live+q*48,std::array<float,12>{});
        assert(TabHook::Make(tabs,model,shadow).count==0);
    }
    PauseMenu::Layout l;l.header_y=19;l.header_shadow={2,.3f};
    const auto g=Header(l,-10,true);
    assert(g.count==1 && g.positions[4]==49 && g.positions[1]==65);
    assert(std::abs(g.positions[6]-g.positions[0]-240*Widescreen::Aspect)<.001f);
    assert(g.colors[3]==.3f && g.uvs[0]>g.uvs[2]);
    l.header_shadow.opacity=0;assert(Header(l,0,false).count==0);
    l.header_shadow={4,1};PauseHeader::Slide slide;
    assert(Header(l,slide.Y(PauseHeader::Travel(l)),true).positions[1]<=0);
    {
        Memory panels;
        constexpr U32 bg=0x18100000,bmesh=0x18110000,desc=0x18120000,live=0x14900000;
        constexpr U32 item=0x18130000,helper=0x18140000,vertices=0x18150000,colors=0x18160000;
        const auto floats=[&](U32 at,const auto& values) {
            for(unsigned i=0;i<values.size();++i)panels.words[at+4*i]=std::bit_cast<U32>(values[i]);
        };
        constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
        floats(bg+12,identity);floats(item+12,identity);
        panels.words[PauseMenu::UI+0x10]=bg;panels.words[bg+4]=bmesh;
        panels.words[bmesh]=desc;panels.words[desc]=0; // Expired descriptor positions must be ignored.
        panels.words[desc+12]=4;panels.words[bmesh+0x124]=1;panels.words[bmesh+0x1a4]=live;
        floats(live,std::array<float,12>{0,0,0,320,0,0,0,240,0,320,240,0});
        panels.words[PauseHeader::Items+0x20]=item;panels.words[PauseHeader::Items+0x24]=helper;
        panels.words[PauseHeader::Items+0x34]=2;panels.words[helper]=37;
        panels.words[helper+0x10]=vertices;panels.words[helper+0x18]=colors;
        floats(vertices+12*48,std::array<float,12>{256,0,0,320,0,0,256,240,0,320,240,0});
        std::array<float,16> rgba;rgba.fill(1);for(unsigned i=3;i<16;i+=4)rgba[i]=.5f;
        floats(colors+12*64,rgba);
        PauseMenu::Layout layout;layout[PauseMenu::Background]={10,20,.5f};
        layout[PauseMenu::Slots]={30,-40,1.5f};layout.stack_background=true;
        layout.horizontal=true;layout.background_darkness=.6f;
        const auto original=panels.words;const auto p=Panels(panels,layout);
        assert(p.count==3 && panels.words==original);
        assert(p.positions[0]==274 && p.positions[6]==278 && p.positions[4]==14 && p.positions[1]==206);
        assert(p.colors[19]==.6f); // One dimmer across both stacked backgrounds.
        assert(std::abs(p.positions[30]-p.positions[24]-288)<.001f);
        assert(std::abs(p.positions[25]-p.positions[28]-9.6f)<.001f);
        assert(p.colors[35]==.225f); // Slot shadow inherits the native fade.
        panels.words[vertices+12*48]=std::bit_cast<U32>(320.f);
        panels.words[vertices+12*48+24]=std::bit_cast<U32>(320.f);
        assert(Panels(panels,layout).count==2); // Hidden native rail cannot leave a shadow behind.
        layout.background_shadow.opacity=0;layout.background_darkness=0;
        assert(Panels(panels,layout).count==0);
        floats(vertices,std::array<float,12>{4,0,0,128,0,0,4,200,0,128,200,0});
        floats(vertices+48,std::array<float,12>{128,0,0,252,0,0,128,200,0,252,200,0});
        floats(colors,rgba);floats(colors+64,rgba);
        layout[PauseMenu::Items]={17,-23,.65f};layout.items_shadow={2,.8f};
        const auto panel=Panels(panels,layout);assert(panel.count==8);
        const auto left=PauseMenu::Move(4,200,PauseMenu::Items,layout[PauseMenu::Items]);
        const auto right=PauseMenu::Move(252,200,PauseMenu::Items,layout[PauseMenu::Items]);
        const auto edge=ToTop({left[0],left[1],right[0],right[1]},{});
        assert(std::abs(panel.positions[0]-edge.left)<.001f && std::abs(panel.positions[6]-edge.right)<.001f);
        assert(std::abs(panel.positions[4]-edge.bottom)<.001f);
        assert(std::abs(panel.positions[1]-edge.bottom-8*2*.8f*.65f)<.001f);
        assert(panel.colors[3]==.4f);
        assert(panel.uvs[8]>panel.uvs[12]); // Left fades toward decreasing X.
        assert(panel.uvs[16]<panel.uvs[20]); // Right fades toward increasing X.
        assert(panel.uvs[24]<panel.uvs[26]); // Top fades toward decreasing Y.
        assert(panel.colors[4*16+3]==.4f && panel.colors[4*16+7]==0);
        // Full perimeter must not crowd out the background or slot shadow.
        floats(vertices+12*48,std::array<float,12>{256,0,0,320,0,0,256,240,0,320,240,0});
        layout.background_shadow.opacity=.45f;layout.background_darkness=.6f;
        const auto all=Panels(panels,layout);assert(all.count==11 && all.colors[10*16+3]==.225f);
        layout.background_shadow.opacity=0;layout.background_darkness=0;
        panels.words[vertices+12*48]=std::bit_cast<U32>(320.f);
        panels.words[vertices+12*48+24]=std::bit_cast<U32>(320.f);
        layout.items_shadow.opacity=0;assert(Panels(panels,layout).count==0);
        layout.items_shadow.opacity=1;panels.words[PauseHeader::Items+0x34]=0;
        assert(Panels(panels,layout).count==0); // Other pages have no inventory shadow.
    }
    // Shadows follow both independently edited green panels, and no emitted
    // rectangle covers either panel even when their positions overlap.
    for(float dx:{0.f,70.f,-90.f})for(float dy:{0.f,-15.f,30.f}) {
        PauseMenu::Layout gear;
        gear.gear_panel={-80,-35,.7f,.45f};
        gear.gear_panel_lower={-80+dx,35+dy,.65f,.4f};
        gear.gear_shadows={PauseMenu::Shadow{3,.7f},PauseMenu::Shadow{2,.6f}};
        Geometry shadows;GearPanels(shadows,gear,.5f,-11);
        assert(shadows.count>0 && shadows.count<=Quads);
        for(const auto panel:{gear.gear_panel,gear.gear_panel_lower}) {
            const auto art=GearPanel::Make(panel,{1,1,1,.5f},-11);
            const auto b=ToTop({art.positions[0],art.positions[1],art.positions[8*12+9],art.positions[8*12+10]},{});
            for(unsigned q=0;q<shadows.count;++q) {
                const auto* p=shadows.positions.data()+q*12;
                assert(std::min(p[6],b.right)-std::max(p[0],b.left)<.001f ||
                       std::min(p[1],b.bottom)-std::max(p[4],b.top)<.001f);
                for(unsigned c=0;c<8;++c)assert(shadows.uvs[q*8+c]>=0 && shadows.uvs[q*8+c]<=1);
                for(unsigned v=0;v<4;++v)assert(shadows.colors[q*16+v*4+3]<=.35001f);
            }
        }
        Geometry hidden;GearPanels(hidden,gear,0,0);assert(hidden.count==0);
    }
    constexpr U32 source=0x18000000,mesh=0x18010000,material=0x18020000;
    constexpr U32 block=0x18030000,model=0x18040000,node=0x18050000,texture=0x14900000;
    for(U32 i=0;i<0x1e4;i+=4)m.words[source+i]=0;
    for(U32 i=0;i<0x1b8;i+=4)m.words[mesh+i]=0;
    for(U32 i=0;i<0x1e4;i+=4)m.words[model+i]=0;
    for(U32 i=0;i<AllocationSize;i+=4)m.words[block+i]=0;
    for(U32 i=0;i<sizeof(Assets::PanelShadow);i+=4)m.words[texture+i]=0;
    m.words[source]=0x4ebd60;m.words[source+4]=mesh;m.words[source+0x1dc]=material;
    m.words[mesh]=mesh+4;m.words[PauseHeader::Items+0x20]=source;
    std::array<U32,16> r{};r[15]=PauseHeader::Entry+4;const auto saved=r;const auto original=m.words;
    Hook h;h.Begin(m,r,PauseHeader::Entry,g);
    assert(h.Pending() && r[15]==0x2a8bdc && r[3]==AllocationSize);
    r[0]=block;h.Resume(m,r);assert(r[15]==0x2a8bdc && r[3]==sizeof(Assets::PanelShadow));
    r[0]=texture;h.Resume(m,r);assert(r[15]==0x348f34);
    assert(h.TextureAddress()%128==0 && h.TextureAddress()==texture);
    assert(m.words.at(block+Descriptor+12)==Quads*4 && m.words.at(block+Descriptor+20)==Quads*6);
    m.words[block+0x128]=2;h.Resume(m,r);assert(r[15]==0x34897c && r[3]==0);
    assert(m.words.at(block+0x158)==h.TextureAddress());
    assert(m.words.at(block+0x15c)==(64|(8<<16)));
    m.words[model]=0x4ebd60;m.words[model+8]=node;m.words[node+0x358]=material;r[0]=model;h.Resume(m,r);
    assert(h.Owned(m) && r[15]==0x36759c);
    h.Resume(m,r);assert(r[15]==0x317d1c);
    h.Resume(m,r);assert(r[15]==0x2f9934);
    h.Resume(m,r);assert(h.Drawing() && r[15]==0x3f9680 && m.words.at(block+0x124)==1);
    h.Resume(m,r);assert(r==saved && !h.Pending());
    for(const auto& [at,word]:original)
        if((at<block || at>=block+AllocationSize) && (at<model || at>=model+0x1e4) && (at<texture || at>=texture+sizeof(Assets::PanelShadow)))assert(m.words.at(at)==word);
    h.Begin(m,r,PauseHeader::Entry,g);assert(r[15]==0x36759c); // Reuse, no per-frame allocation.
    for(int i=0;i<4;++i)h.Resume(m,r);
    assert(!h.Pending() && r==saved);
    m.words[block+Cookie]=0;h.Begin(m,r,PauseHeader::Entry,g);assert(r[15]==0x2a8bdc);
    r[0]=0;h.Resume(m,r);assert(!h.Pending() && r==saved); // Safe state-load/allocation fallback.
    {
        constexpr U32 owner=0x18080000,popup=0x18090000,popup_mesh=0x180a0000,
            popup_desc=0x180b0000,live=0x14910000;
        m.words[PauseHeader::Items+0x4c]=owner;m.words[owner+4]=popup;
        m.words[PauseHeader::Items+0x28]=owner+0x100;m.words[owner+0x104]=source;
        m.words[PauseHeader::Items+0x34]=9;m.words[PauseHeader::Items+0x38]=2;
        m.words[PauseHeader::Items+0x60]=0;
        m.words[popup+4]=popup_mesh;m.words[popup_mesh]=popup_desc;
        m.words[popup_desc+12]=4;m.words[popup_mesh+0x124]=0;m.words[popup_mesh+0x1a0]=live;
        constexpr std::array<float,12> matrix{1,0,0,0,0,1,0,0,0,0,1,0};
        constexpr std::array<float,12> vertices{80,60,0,80,150,0,170,60,0,170,150,0};
        for(unsigned i=0;i<12;++i) {
            m.words[popup+12+4*i]=std::bit_cast<U32>(matrix[i]);
            m.words[live+4*i]=std::bit_cast<U32>(vertices[i]);
        }
        PauseMenu::SetLayout(PauseMenu::Layout{},false);
        ArrowHook arrow;r=saved;r[0]=popup;const auto native=r;
        const auto before=m.words;
        assert(arrow.NativeDraw(m,r,true) && arrow.geometry.count==8);
        assert(m.words==before); // Geometry uses published positions without mutating native meshes.
        assert(arrow.geometry.uvs[24]<arrow.geometry.uvs[26]); // Top gradient points outward.
        assert(arrow.geometry.colors[4*16+3]>0 && arrow.geometry.colors[4*16+7]==0);
        r[0]=0;arrow.ResumeNative(m,r);
        assert(!arrow.Pending() && r[0]==popup && r[1]==0x54c8c8 && r[15]==0x3f9684);
        r=native;m.words[PauseHeader::Items+0x38]=1;
        assert(!arrow.NativeDraw(m,r,true) && r==native);
        m.words[PauseHeader::Items+0x38]=2;r[0]=source;
        assert(!arrow.NativeDraw(m,r,true)); // Never shadow the items atlas or cursor.
        auto disabled=PauseMenu::GetLayout();disabled.arrow_shadow.enabled=false;
        PauseMenu::SetLayout(disabled,false);r=native;assert(!arrow.NativeDraw(m,r,true));
    }
    std::cout<<"Embedded shadow, width/opacity, clipping, buffer ownership, uploads and state fallback passed\n";
}
