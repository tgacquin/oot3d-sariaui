// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_prompts.h"
#include "oot3d/pause_prompt_assets.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <limits>
using namespace OOT3D;
using namespace OOT3D::PausePrompts;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {
        auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};
    }
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
};
std::vector<std::uint8_t> Read(const std::string& path) {
    std::ifstream file(path,std::ios::binary);assert(file);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
// Verify the editor's requested geometry independently of the viewport guard.
// localized-prompts-check covers the default fitted presentation at the edges.
Pair UnboundedGeometry(const Layout& l,const Assets& a,Page p=Page::Items,float y=0,
                       bool gear=true,bool navigation=true,bool arrows=false) {
    return MakeGeometry(l,a,p,y,gear,navigation,arrows,false);
}
int main(int argc,char** argv) {
    assert(argc>1);
    auto atlas=Read(std::string(argv[1])+"/message/parts.ctxb");
    auto font=Read(std::string(argv[1])+"/message/us/ltn16.qbf");
    const auto image=MakeAssets(atlas,font);assert(image);
    // Full payload, dimensions and format stay native: both raw-byte and
    // decoded-pixel texture-pack hash schemes see the original texture.
    for(unsigned i=0;i<image->icons.size();++i)
        assert(image->icons[i]==MapFrame::LE32(atlas,72+i*4));
    assert(image->widths[0]==64 && image->widths[1]==55);
    assert(image->text[Tiled(0,0)]==0);
    assert(std::any_of(image->text.begin(),image->text.end(),[](U32 p){return (p>>24)>200 && (p&255)>0;}));
    assert(std::any_of(image->text.begin(),image->text.end(),[](U32 p){return (p>>8)==0 && (p&255)>0;}));
    auto bad=atlas;bad[48]^=1;assert(!MakeAssets(bad,font));
    bad=font;bad.pop_back();assert(!MakeAssets(atlas,bad));
    bad=font;bad[12]=8;assert(!MakeAssets(atlas,bad));
    bad=font;bad[16+35*8+2]=255;bad[16+35*8+3]=255;assert(!MakeAssets(atlas,bad));
    const auto near=[](float a,float b){return std::abs(a-b)<.0001f;};
    SaveMenu::Layout save;
    const auto prompts=SavePrompts(*image,save,.5f);
    assert(prompts.icons.count==2 && prompts.text.count==10);
    assert(prompts.icons.colors[3]==.5f && prompts.text.colors[3]==.5f);
    save.prompts[0]={10,-20,2};
    const auto save_moved=SavePrompts(*image,save,1);
    assert(save_moved.icons.positions[0]==38 && save_moved.icons.positions[1]==200);
    assert(save_moved.icons.positions[3]-save_moved.icons.positions[0]==32);
    assert(save_moved.icons.positions[12]==prompts.icons.positions[12]);
    assert(save_moved.text.positions[0]==38+(prompts.text.positions[0]-28)*2);
    save.prompts[1]={-11,-7,.8f};
    const auto death=SavePrompts(*image,save,.4f,false,true);
    const auto boss=SavePrompts(*image,save,.4f);
    assert(death.icons.count==1 && death.text.count==6 && boss.icons.count==2);
    assert(death.icons.positions[0]==boss.icons.positions[12] &&
           death.icons.positions[1]==boss.icons.positions[13]);
    assert(death.icons.uvs[0]==prompts.icons.uvs[0]); // A artwork, in B's saved pose.
    assert(death.text.uvs[1]==prompts.text.uvs[1]); // Select, not Back.
    assert(death.icons.colors[3]==.4f && death.text.colors[3]==.4f);
    Memory save_memory;SaveMenu::on_top=true;
    save_memory.words[SaveMenu::Owner+0x28]=2;
    for(unsigned state:{0u,1u,7u,8u,9u,10u,11u,14u}) {
        save_memory.words[SaveMenu::Owner+0x24]=state;
        assert(SavePromptsVisible(save_memory));
    }
    save_memory.words[SaveMenu::Owner+0x28]=3;assert(SavePromptsVisible(save_memory));
    const auto options_prompts=SavePrompts(*image,SaveMenu::Layout{},1,true);
    assert(options_prompts.icons.count==2 && options_prompts.text.count==8);
    assert(options_prompts.text.uvs[1]==(OkayText*TextRowHeight+.5f)/TextHeight);
    SaveMenu::on_top=false;
    Layout settings;
    const auto original=UnboundedGeometry(settings,*image);
    assert(original.icons.count==6 && original.text.count==55);
    ControlSwap::shoulders=true;
    const auto swapped=UnboundedGeometry(settings,*image);
    assert(swapped.icons.count==5 && swapped.text.count==57);
    // A/B retain their icon identity; I/II now use native L/R icons and
    // Change tab/navigation use the existing HD-capable ZL/ZR label rows.
    assert(std::equal(swapped.icons.uvs.begin(),swapped.icons.uvs.begin()+16,original.icons.uvs.begin()));
    assert(near(swapped.icons.uvs[16],192.5f/IconWidth));
    assert(near(swapped.icons.uvs[24],208.5f/IconWidth));
    const auto swapped_map=UnboundedGeometry(settings,*image,Page::Map);
    assert(swapped_map.icons.count==3);
    ControlSwap::shoulders=false;
    const auto arrow=UnboundedGeometry(settings,*image,Page::Items,0,true,true,true);
    assert(arrow.icons.positions==original.icons.positions && arrow.text.positions==original.text.positions);
    unsigned dimmed_icons=0,dimmed_text=0;
    for(unsigned q=0;q<arrow.icons.count;++q) {
        const float alpha=arrow.icons.colors[q*16+3];
        assert(alpha==1 || alpha==.6f);dimmed_icons+=alpha==.6f;
        if(q<2)assert(alpha==1); // A/B retain their full opacity.
    }
    for(unsigned q=0;q<arrow.text.count;++q)dimmed_text+=arrow.text.colors[q*16+3]==.6f;
    assert(dimmed_icons==2 && dimmed_text>0);
    const auto challenge=UnboundedGeometry(settings,*image,Page::Items,0,true,false);
    assert(challenge.icons.count==4 && challenge.text.count<original.text.count);
    assert(std::equal(challenge.icons.positions.begin(),challenge.icons.positions.begin()+challenge.icons.count*12,
        original.icons.positions.begin())); // Only the navigation hint disappears.
    assert(std::equal(challenge.text.positions.begin(),challenge.text.positions.begin()+challenge.text.count*12,
        original.text.positions.begin()));
    // Follow the native UI material's (1-v) sampling, not a direct atlas
    // preview. These must reach raw rows 32..47 (A/B), never 80..95 (X slots).
    for(unsigned i=0;i<2;++i) {
        assert(original.icons.uvs[i*8]==(128.5f+16*i)/256);
        assert((1-original.icons.uvs[i*8+1])*128==32.5f);
        assert((1-original.icons.uvs[i*8+5])*128==47.5f);
        for(unsigned j=0;j<6;++j) {
            const unsigned q=i*6+j;
            assert((1-original.text.uvs[q*8+1])*TextHeight==TextHeight-.5f-TextRowHeight*i);
            assert((1-original.text.uvs[q*8+5])*TextHeight==TextHeight-19.5f-TextRowHeight*i);
            const auto glyph=image->glyphs[i][j];
            assert(original.text.uvs[q*8]*TextWidth==glyph.x-1);
            assert(original.text.uvs[q*8+2]*TextWidth==glyph.x+glyph.width+1);
        }
    }
    const auto map_prompts=UnboundedGeometry(settings,*image,Page::Map);
    assert(map_prompts.icons.count==4 && map_prompts.text.count==22);
    assert((1-map_prompts.icons.uvs[1])*128==.5f); // Neutral Circle Pad.
    assert((1-map_prompts.icons.uvs[9])*128==32.5f); // B.
    assert(map_prompts.icons.uvs[8]==144.5f/256);
    assert(image->widths[2]>image->widths[0] && image->widths[2]<=TextWidth);
    {
        const auto backdrop=MakeNavigationShadow(settings,*image,Page::Map);
        assert(backdrop.count==1 && backdrop.colors[3]==.45f);
        assert(MakeNavigationShadow(settings,*image,Page::Items).count==0);
        assert(MakeNavigationShadow(settings,*image,Page::Gear).count==0);
        const auto pixels=MakeNavigationShadowTexture(.7f);
        const auto a=[&](unsigned x,unsigned y){return pixels[NavigationShadowTexel(x,127-y)]&255;};
        assert(a(0,64)>a(64,64) && a(64,64)>a(120,64));
        assert(a(100,0)==0 && a(100,127)==0 && a(0,64)>200);
        for(auto pixel:pixels)assert((pixel>>8)==0); // Black tint; alpha only.
        auto custom=settings;custom.prompts[MapToItems].group={30,-12,.5f};
        const auto moved=MakeNavigationShadow(custom,*image,Page::Map);
        assert(near(moved.positions[3]-moved.positions[0],32));
        assert(near(moved.positions[7]-moved.positions[1],50));
        custom.map_navigation_shadow.opacity=0;
        assert(MakeNavigationShadow(custom,*image,Page::Map).count==0);
        custom.map_navigation_shadow.opacity=.8f;custom.map_navigation_shadow.enabled=false;
        assert(MakeNavigationShadow(custom,*image,Page::Map).count==0);
        const auto unchanged=UnboundedGeometry(custom,*image,Page::Map);
        custom.map_navigation_shadow.enabled=true;custom.map_navigation_shadow.width=190;
        const auto still=UnboundedGeometry(custom,*image,Page::Map);
        assert(unchanged.icons.positions==still.icons.positions && unchanged.text.positions==still.text.positions);
    }
    const auto gear_prompts=UnboundedGeometry(settings,*image,Page::Gear);
    assert(gear_prompts.icons.count==4 && gear_prompts.text.count==52);
    {
        Memory gear;
        gear.words[0x587958+4]=1;
        for(unsigned i=0;i<9;++i)gear.words[0x4d47c8+i*4]=9;
        for(unsigned selected=0;selected<28;++selected) {
            gear.words[0x50446c+0x20]=selected;
            gear.words[0x587958+0xb4]=0xffff0000;
            assert(GearEquippable(gear)==(selected<9));
            gear.words[0x587958+0xb4]=0;assert(!GearEquippable(gear));
            if(selected<9) {
                gear.words[0x587958+0xb4]=1u<<(16+(selected/3)*4+selected%3);
                assert(GearEquippable(gear));
            }
        }
        // Native equip age table: child=1, adult=0, unrestricted=9.
        constexpr std::array<unsigned,9> ages{1,0,0,1,9,0,9,0,0};
        for(unsigned age:{0u,1u})for(unsigned selected=0;selected<9;++selected) {
            gear.words[0x587958+4]=age;gear.words[0x50446c+0x20]=selected;
            gear.words[0x4d47c8+selected*4]=ages[selected];
            gear.words[0x587958+0xb4]=0xffff0000;
            assert(GearEquippable(gear)==(ages[selected]==9 || ages[selected]==age));
            gear.words[0x587958+0xb4]=0;assert(!GearEquippable(gear));
        }
        gear.words.clear();assert(!GearEquippable(gear));
        for(auto page:{Page::Gear,Page::Items,Page::Map}) {
            const auto bright=UnboundedGeometry(settings,*image,page,0,true);
            const auto dim=UnboundedGeometry(settings,*image,page,0,false);
            assert(bright.icons.positions==dim.icons.positions && bright.text.positions==dim.text.positions);
            for(unsigned q=0;q<bright.icons.count;++q)for(unsigned v=0;v<4;++v)
                assert(near(dim.icons.colors[q*16+v*4+3],page==Page::Gear && q==0?.6f:1.f));
            for(unsigned q=0;q<bright.text.count;++q)for(unsigned v=0;v<4;++v)
                assert(near(dim.text.colors[q*16+v*4+3],page==Page::Gear && (q<6 || (q>=12 && q<41))?.6f:1.f));
            // A stays bright for owned gear, while both trigger labels and
            // their descriptions remain disabled regardless of selection.
            for(unsigned q=0;q<bright.icons.count;++q)for(unsigned v=0;v<4;++v)
                assert(near(bright.icons.colors[q*16+v*4+3],1.f));
            for(unsigned q=0;q<bright.text.count;++q)for(unsigned v=0;v<4;++v)
                assert(near(bright.text.colors[q*16+v*4+3],page==Page::Gear && q>=12 && q<41?.6f:1.f));
        }
    }
    {
        // Measure complete rendered prompts independently of the row arranger.
        const auto bounds=[](const Pair& g,unsigned ib,unsigned ie,unsigned tb,unsigned te) {
            std::array<float,2> b{10000,-10000};
            const auto add=[&](const auto& mesh,unsigned first,unsigned end) {
                for(unsigned q=first;q<end;++q)for(unsigned v=0;v<12;v+=3) {
                    b[0]=std::min(b[0],mesh.positions[q*12+v]);b[1]=std::max(b[1],mesh.positions[q*12+v]);
                }
            };
            add(g.icons,ib,ie);add(g.text,tb,te);return b;
        };
        constexpr std::array<std::array<unsigned,4>,5> parts{{{2,4,41,52},{0,1,0,6},{1,2,6,12},{4,4,12,26},{4,4,26,41}}};
        for(auto page:{Page::Items,Page::Gear})for(float spacing:{-3.f,0.f,12.f,35.f})
        for(float scale:{.5f,1.f,1.5f}) {
            Layout l;l.lower.spacing=spacing;l.lower.pose={17,-9,scale};
            const auto g=UnboundedGeometry(l,*image,page);
            const unsigned count=5;
            float left=0,right=0;
            for(unsigned i=0;i<count;++i) {
                const auto p=parts[i];
                const auto b=bounds(g,p[0],p[1],p[2],p[3]);
                if(i==0)left=b[0];else assert(near(b[0]-right,spacing*scale));
                right=b[1];
                assert(near(g.text.positions[p[2]*12+1],228-9-7.5f*scale));
            }
            assert(near((left+right)/2,217));
            const auto base=UnboundedGeometry(Layout{},*image,page);
            if(page!=Page::Gear) {
                for(unsigned q=g.icons.count-2;q<g.icons.count;++q)for(unsigned v=0;v<12;++v)
                    assert(g.icons.positions[q*12+v]==base.icons.positions[q*12+v]);
                const unsigned nav=page==Page::Map?5:3;
                for(unsigned q=g.text.count-nav;q<g.text.count;++q)for(unsigned v=0;v<12;++v)
                    assert(g.text.positions[q*12+v]==base.text.positions[q*12+v]);
            }
        }
        Layout old;
        for(unsigned i=0;i<ItemsToMap;++i) {old.prompts[i].group.x=300-i*40;old.prompts[i].group.y=-100.f+i*20;}
        const auto aligned=UnboundedGeometry(old,*image);
        assert(aligned.icons.positions==original.icons.positions && aligned.text.positions==original.text.positions);
    }
    // Row movement, scale and spacing belong exclusively to Items/Gear.
    {
        Layout edit;edit.lower.pose={120,-70,2};edit.lower.spacing=35;
        const auto map=UnboundedGeometry(edit,*image,Page::Map);
        assert(map.icons.positions==map_prompts.icons.positions && map.text.positions==map_prompts.text.positions);
        for(unsigned index:{MapMove,MapReturn}) {
            edit=Layout{};edit.prompts[index].group={-23,17,1.5f};
            const auto moved=UnboundedGeometry(edit,*image,Page::Map);
            const unsigned q=index-MapMove,other=1-q;
            const float x=index==MapMove?48.f:280.f;
            assert(near(moved.icons.positions[q*12],x-23+1.5f*(map_prompts.icons.positions[q*12]-x)));
            assert(near(moved.icons.positions[q*12+1],228+17-12));
            assert(near(moved.icons.positions[q*12+3]-moved.icons.positions[q*12],24));
            for(unsigned v=0;v<12;++v)
                assert(moved.icons.positions[other*12+v]==map_prompts.icons.positions[other*12+v]);
            const unsigned first=q?11:0,end=q?17:11;
            for(unsigned glyph=first;glyph<end;++glyph) {
                assert(near(moved.text.positions[glyph*12],x-23+1.5f*(map_prompts.text.positions[glyph*12]-x)));
                assert(near(moved.text.positions[glyph*12+1],228+17+1.5f*(map_prompts.text.positions[glyph*12+1]-228)));
            }
            const auto items=UnboundedGeometry(edit,*image,Page::Items);
            assert(items.icons.positions==original.icons.positions && items.text.positions==original.text.positions);
        }
    }
    // The side navigation is Items -> Map (R, arrow right) or Map -> Items
    // (arrow left, L), with a centered caption below. Never add it to Gear.
    const auto check_navigation=[&](const Pair& g,bool map) {
        const unsigned button=g.icons.count-2,arrow=button+1;
        const float bx=g.icons.positions[button*12],ax=g.icons.positions[arrow*12];
        assert(map?ax<bx:bx<ax);
        assert(g.icons.uvs[button*8]==(map?192.5f:208.5f)/IconWidth);
        assert((1-g.icons.uvs[button*8+1])*IconHeight==32.5f);
        // Locate the raw downward arrow tip after the UV rotation: the
        // destination tip must reach the bottom center of the atlas tile.
        const unsigned tip0=map?0:2,tip1=map?4:6;
        const auto at=g.icons.uvs.begin()+arrow*8;
        assert((at[tip0]+at[tip1])/2*IconWidth==200.f);
        assert((1-(at[tip0+1]+at[tip1+1])/2)*IconHeight==95.5f);
        const unsigned first=g.text.count-(map?5:3);
        assert(g.text.positions[first*12+1]>g.icons.positions[arrow*12+7]);
    };
    check_navigation(original,false);check_navigation(map_prompts,true);
    for(const auto page:{Page::Items,Page::Map}) {
        const unsigned prompt=page==Page::Map?MapToItems:ItemsToMap;
        const auto base=UnboundedGeometry(settings,*image,page);
        auto edit=settings;edit.prompts[prompt].group={-17,9,1.5f};
        const auto moved=UnboundedGeometry(edit,*image,page);
        const unsigned first=base.icons.count-2;
        assert(moved.icons.positions[first*12+3]-moved.icons.positions[first*12]==24);
        assert(moved.icons.positions!=base.icons.positions && moved.text.positions!=base.text.positions);
        assert(UnboundedGeometry(edit,*image,Page::Gear).icons.positions==gear_prompts.icons.positions);
        const auto other=page==Page::Map?Page::Items:Page::Map;
        assert(UnboundedGeometry(edit,*image,other).icons.positions==UnboundedGeometry(settings,*image,other).icons.positions);
        edit=settings;edit.prompts[prompt].button={5,-3,1.25f};
        const auto button_moved=UnboundedGeometry(edit,*image,page);
        assert(button_moved.text.positions==base.text.positions);
        for(unsigned q=first;q<base.icons.count;++q)
            assert(button_moved.icons.positions[q*12+3]-button_moved.icons.positions[q*12]==20);
    }
    // New controls are Items/Gear-only. L and R sample the native row 32,
    // and the slash and trigger labels share the existing font and shadow.
    assert(Text[EquipI]=="- Equip to I" && Text[EquipII]=="- Equip to II");
    for(const auto [row,first]:{std::pair{EquipI,12u},std::pair{EquipII,26u}}) {
        const auto& glyphs=image->glyphs[row];
        const float advance=original.text.positions[(first+2)*12]-original.text.positions[first*12];
        const float native_advance=glyphs[2].x-glyphs[0].x+
            2*(settings.letter_spacing-float(image->native_spacing));
        assert(near(advance,native_advance*.75f));
        // The dash has exactly one normal font space on both sides,
        // including the boundary between the separate ZL/ZR and text runs.
        const unsigned trigger_row=row==EquipI?ZLText:ZRText;
        const unsigned trigger_last=first+Text[row].size()+1;
        const float trigger_right=original.text.positions[trigger_last*12]+
            (image->glyphs[trigger_row][1].width+1)*.75f;
        const float dash_left=original.text.positions[first*12]+.75f;
        const float dash_right=dash_left+glyphs[0].width*.75f;
        const float equip_left=original.text.positions[(first+2)*12]+.75f;
        assert(near(dash_left-trigger_right,equip_left-dash_right));
        for(unsigned j=3;j<Text[row].size();++j) {
            const float expected=(glyphs[j].x-glyphs[j-1].x+
                settings.letter_spacing-float(image->native_spacing))*.75f;
            assert(near(original.text.positions[(first+j)*12]-
                        original.text.positions[(first+j-1)*12],expected));
        }
    }
    for(unsigned q=2;q<4;++q) {
        assert(original.icons.uvs[q*8]==(192.5f+16*(q-2))/256);
        assert((1-original.icons.uvs[q*8+1])*128==32.5f);
        assert((1-original.icons.uvs[q*8+5])*128==47.5f);
    }
    for(unsigned row:{EquipI,EquipII,ChangeTab}) {
        auto edit=settings;edit.prompts[row].group={-25,11,1.25f};
        const auto map=UnboundedGeometry(edit,*image,Page::Map);
        assert(map.icons.positions==map_prompts.icons.positions && map.text.positions==map_prompts.text.positions);
        const auto all=UnboundedGeometry(edit,*image);
        for(unsigned q=0;q<12;++q)
            assert(near(all.text.positions[q*12+3]-all.text.positions[q*12],
                        original.text.positions[q*12+3]-original.text.positions[q*12]));
        assert(all.text.positions!=original.text.positions);
    }
    {
        auto edit=settings;edit.prompts[ChangeTab].button={7,-4,1};
        const auto all=UnboundedGeometry(edit,*image);
        const float reflow=all.text.positions[41*12]-original.text.positions[41*12];
        for(unsigned q=2;q<4;++q)for(unsigned v=0;v<12;v+=3) {
            assert(near(all.icons.positions[q*12+v],original.icons.positions[q*12+v]+7+reflow));
            assert(all.icons.positions[q*12+v+1]==original.icons.positions[q*12+v+1]-4);
        }
        for(unsigned v=0;v<12;v+=3) {
            assert(near(all.text.positions[51*12+v],original.text.positions[51*12+v]+7+reflow));
            assert(all.text.positions[51*12+v+1]==original.text.positions[51*12+v+1]-4);
        }
        for(unsigned q=41;q<51;++q)for(unsigned v=0;v<12;v+=3) {
            assert(near(all.text.positions[q*12+v],original.text.positions[q*12+v]+reflow));
            assert(all.text.positions[q*12+v+1]==original.text.positions[q*12+v+1]);
        }
    }
    // Compare every displayed glyph/shadow pixel to the source font using
    // the same vertical sampling convention as the native material.
    for(unsigned row=0;row<Text.size();++row) {
        std::array<unsigned,TextWidth*20> alpha{};
        unsigned pen=2;
        for(char c:Text[row]) {
            unsigned record=16;
            while((font[record]|unsigned(font[record+1])<<8)!=unsigned(c))record+=8;
            const unsigned glyph=font[record+2]|unsigned(font[record+3])<<8;
            const unsigned left=font[record+4],width=font[record+5];
            const unsigned bitmap=16+(font[4]|unsigned(font[5])<<8)*8;
            for(unsigned y=0;y<16;++y)for(unsigned x=0;x<width;++x) {
                const unsigned pixel=y*16+left+x;
                alpha[(y+2)*TextWidth+pen+x]=
                    ((font[bitmap+glyph*128+pixel/2]>>(pixel%2?0:4))&15)*17;
            }
            pen+=width+font[15];
        }
        for(unsigned y=0;y<20;++y)for(unsigned x=0;x<image->widths[row];++x) {
            const auto p=image->text[Tiled(x,TextHeight-1-(row*TextRowHeight+y))];
            const unsigned a=alpha[y*TextWidth+x];
            const unsigned shadow=(x && y)?alpha[(y-1)*TextWidth+x-1]*4/5:0;
            assert((p&255)==a+shadow*(255-a)/255);
            if(a==255)assert(p==0xffffffff);
            if(!a)assert((p>>8)==0);
        }
    }
    settings.prompts[0].group={-40,20,2};
    auto moved=UnboundedGeometry(settings,*image);
    assert(moved.icons.positions[1]==212); // Scale around the common row center, Y=228.
    assert(moved.icons.positions[3]-moved.icons.positions[0]==32);
    assert(near(moved.text.positions[0]-moved.icons.positions[0],41.5f) && moved.text.positions[1]==213);
    assert(moved.text.positions[3]-moved.text.positions[0]==(image->glyphs[0][0].width+2)*1.5f);
    settings.prompts[0].button={5,-3,1.5f};
    const auto adjusted=UnboundedGeometry(settings,*image);
    assert(near(adjusted.icons.positions[0]-adjusted.text.positions[0],-39.5f));
    assert(adjusted.icons.positions[1]==198 && adjusted.text.positions[1]==213);
    assert(adjusted.icons.positions[3]-adjusted.icons.positions[0]==48);
    assert(near(adjusted.icons.positions[15]-adjusted.icons.positions[12],16));
    assert(adjusted.icons.positions[13]==original.icons.positions[13]);
    settings.enabled=false;assert(!UnboundedGeometry(settings,*image).icons.count);
    settings.enabled=true;settings.prompts[0].button.scale=std::numeric_limits<float>::quiet_NaN();
    assert(UnboundedGeometry(settings,*image).icons.count==5);
    {
        Layout bob_settings;NavigationBob bob;
        constexpr std::array<float,9> offsets{0,-1,-2,-1,0,1,2,1,0};
        for(unsigned frame=0;frame<offsets.size();++frame) {
            const auto time=std::int64_t(frame)*125000;
            assert(near(bob.Advance(time,bob_settings,true),offsets[frame]));
            assert(near(bob.Advance(time,bob_settings,true),offsets[frame])); // Paused clock.
        }
        for(unsigned fps:{30u,60u,120u}) {
            NavigationBob sampled;sampled.Advance(0,bob_settings,true);
            for(unsigned frame=1;frame<=fps;++frame)
                sampled.Advance(std::int64_t(frame)*1000000/fps,bob_settings,true);
            assert(near(sampled.Advance(1125000,bob_settings,true),-1));
        }
        bob={};bob.Advance(0,bob_settings,true);
        assert(near(bob.Advance(125000,bob_settings,true),-1));
        bob_settings.navigation_speed=2;
        assert(near(bob.Advance(125000,bob_settings,true),-1)); // No jump when speed changes.
        assert(near(bob.Advance(187500,bob_settings,true),-2));
        assert(bob.Advance(100,bob_settings,true)==0); // Save-state time rollback.
        assert(bob.Advance(200,bob_settings,false)==0 && !bob.last_time);
        assert(bob.Advance(9000000,bob_settings,true)==0); // Fresh menu starts at its set Y.
        bob_settings.navigation_speed=0;assert(bob.Advance(9500000,bob_settings,true)==0);
        bob_settings.navigation_speed=1;bob_settings.navigation_distance=0;
        assert(bob.Advance(9600000,bob_settings,true)==0);
        bob_settings.navigation_distance=std::numeric_limits<float>::quiet_NaN();
        assert(bob.Advance(9700000,bob_settings,true)==0);
        for(auto page:{Page::Items,Page::Map,Page::Gear})for(float scale:{.5f,1.f,2.f}) {
            Layout pose;pose.prompts[ItemsToMap].group.scale=pose.prompts[MapToItems].group.scale=scale;
            const auto base=UnboundedGeometry(pose,*image,page);
            const auto moved=UnboundedGeometry(pose,*image,page,-3);
            assert(moved.text.positions==base.text.positions && moved.icons.uvs==base.icons.uvs);
            for(unsigned q=0;q<base.icons.count;++q)for(unsigned v=0;v<12;v+=3) {
                const bool navigation=page!=Page::Gear && q>=base.icons.count-2;
                assert(moved.icons.positions[q*12+v]==base.icons.positions[q*12+v]);
                assert(near(moved.icons.positions[q*12+v+1],base.icons.positions[q*12+v+1]-(navigation?3:0)));
            }
        }
    }
    settings={};
    settings.letter_spacing=0;
    auto tight=UnboundedGeometry(settings,*image);
    assert(tight.icons.uvs==original.icons.uvs && tight.text.uvs==original.text.uvs);
    for(unsigned q=0;q<12;++q) {
        const unsigned first=(q/6)*6;
        assert(near((original.text.positions[q*12]-original.text.positions[first*12])-
                    (tight.text.positions[q*12]-tight.text.positions[first*12]),(q%6)*.75f));
        assert(near(tight.text.positions[q*12+3]-tight.text.positions[q*12],
                    original.text.positions[q*12+3]-original.text.positions[q*12]));
    }
    settings={};settings.icon_scale=1.5f;
    const auto big_icons=UnboundedGeometry(settings,*image);
    // Icon sizes can reflow the row; main glyph sizes and baselines stay fixed.
    for(unsigned q=0;q<original.text.count;++q) {
        if(q==24 || q==25 || q==39 || q==40 || q==51)continue;
        assert(big_icons.text.positions[q*12+1]==original.text.positions[q*12+1]);
        assert(near(big_icons.text.positions[q*12+3]-big_icons.text.positions[q*12],
                    original.text.positions[q*12+3]-original.text.positions[q*12]));
    }
    for(unsigned q=0;q<2;++q) {
        assert(big_icons.icons.positions[q*12+3]-big_icons.icons.positions[q*12]==24);
        assert(near(big_icons.icons.positions[q*12]+12-big_icons.text.positions[q*6*12],
                    original.icons.positions[q*12]+8-original.text.positions[q*6*12]));
        assert(big_icons.icons.positions[q*12+1]+12==original.icons.positions[q*12+1]+8);
    }
    settings={};settings.scale=2;
    auto scaled=UnboundedGeometry(settings,*image);
    for(unsigned q=0;q<12;++q)
        assert(near(scaled.text.positions[q*12+3]-scaled.text.positions[q*12],
                    2*(original.text.positions[q*12+3]-original.text.positions[q*12])));
    for(unsigned q=0;q<2;++q)
        assert(scaled.icons.positions[q*12+3]-scaled.icons.positions[q*12]==32);
    settings={};settings.prompts[0].group.y=.05f;settings.prompts[1].group.y=-19.95f;
    const auto aligned=UnboundedGeometry(settings,*image);
    assert(near(aligned.text.positions[1],aligned.text.positions[6*12+1]));
    settings.letter_spacing=std::numeric_limits<float>::quiet_NaN();
    assert(UnboundedGeometry(settings,*image).text.count==0);
    Memory m;
    constexpr U32 play=0x18000000,source=0x18100000,mesh=0x18110000,desc=0x18120000,
        material=0x18130000,block=0x18200000,model=0x18300000,texture=0x14000000;
    m.words[PauseMenu::UI+0xc]=play;m.words[play+0x100]=3;
    m.words[PauseMenu::UI+0x14]=1;m.words[PauseMenu::UI+0x20]=0;m.words[PauseMenu::UI+0x24]=1;
    m.words[0x54ac4c]=0x01000000;
    for(bool items:{false,true}) {
        m.words[0x5066f8+0x34]=items?2:0;m.words[0x50446c+0x18]=items?0:2;
        assert(Visible(m));
        assert(CurrentPage(m)==(items?Page::Items:Page::Gear));
        m.words[0x54ac4c]=0;assert(!Visible(m));m.words[0x54ac4c]=0x01000000;
        m.words[SaveMenu::Owner+0x28]=3;assert(!Visible(m));m.words[SaveMenu::Owner+0x28]=0;
    }
    m.words[0x5066f8+0x34]=m.words[0x50446c+0x18]=0;assert(!Visible(m));
    m.words[0x506cb0+0x38]=2;assert(Visible(m)); // Both world and dungeon map pages.
    for(unsigned dungeon:{0u,1u}) {
        m.words[0x4fda6c+0x50]=dungeon;
        assert(CurrentPage(m)==Page::Map);
    }
    m.words[SaveMenu::Owner+0x28]=3;assert(!Visible(m));
    m.words[SaveMenu::Owner+0x28]=0;m.words[0x54ac4c]=0;assert(!Visible(m));
    m.words[0x54ac4c]=0x01000000;m.words[0x506cb0+0x38]=0;
    m.words[0x5066f8+0x34]=2;m.words[PauseMenu::UI+0x14]=0;assert(!Visible(m));
    m.words[PauseMenu::UI+0x14]=1;
    for(U32 i=0;i<0x1e4;i+=4)m.words[source+i]=m.words[model+i]=0;
    m.words[source]=m.words[model]=0x4ebd60;m.words[source+4]=mesh;
    m.words[source+0x1dc]=material;m.words[mesh]=desc;m.words[model+8]=0x18400000;m.words[0x18400358]=material;
    for(U32 i=0;i<0x120;i+=4)m.words[desc+i]=0;
    Hook hook;
    using IconMesh=decltype(hook.icons);
    static_assert(IconMesh::TextureBytes==65536);
    for(U32 i=0;i<IconMesh::AllocationSize;i+=4)m.words[block+i]=0;
    for(U32 i=0;i<IconMesh::TextureBytes;i+=4)m.words[texture+i]=0;
    std::array<U32,16> regs{};regs[15]=0x123456;const auto saved=regs;
    hook.icons.Begin(m,regs,PauseMenu::Sites[19].pc,original.icons,image->icons,source);
    assert(regs[15]==0x2a8bdc);
    regs[0]=block;hook.icons.Resume(m,regs);
    assert(regs[15]==0x2a8bdc && regs[3]==65536);
    regs[0]=texture;hook.icons.Resume(m,regs);
    for(unsigned i=0;i<image->icons.size();++i)assert(m.Read(texture+4*i)==image->icons[i]);
    hook.icons.Resume(m,regs);assert(m.Read(block+0x164)==4);
    assert(m.Read(block+0x15c)==(256|(128<<16)));
    regs[0]=model;hook.icons.Resume(m,regs);
    m.words[block+0x128]=2;m.words[block+0x124]=0;
    for(unsigned i=0;i<4;++i)hook.icons.Resume(m,regs);
    assert(!hook.icons.Pending() && regs==saved && hook.icons.Owned(m));
    // Reusing the mesh preserves the exact atlas; no CPU-side replacement
    // cache can keep stale buttons after the renderer's custom-texture toggle.
    const auto before=m.words;
    hook.icons.Begin(m,regs,PauseMenu::Sites[19].pc,original.icons,image->icons,source);
    assert(regs[15]==0x36759c);
    for(U32 i=0;i<65536;i+=4)assert(m.words.at(texture+i)==before.at(texture+i));
    // The per-letter mesh reserves room for both map labels;
    // exercise its larger descriptor/buffers through the native draw lifecycle.
    using TextMesh=decltype(hook.text);
    for(U32 i=0;i<TextMesh::AllocationSize;i+=4)m.words[block+i]=0;
    for(U32 i=0;i<TextMesh::TextureBytes;i+=4)m.words[texture+i]=0;
    regs=saved;
    hook.text.Begin(m,regs,PauseMenu::Sites[19].pc,original.text,image->text,source);
    assert(regs[15]==0x2a8bdc && regs[3]==TextMesh::AllocationSize);
    regs[0]=block;hook.text.Resume(m,regs);
    assert(regs[3]==TextWidth*TextHeight*4);
    regs[0]=texture;hook.text.Resume(m,regs);
    hook.text.Resume(m,regs);assert(m.Read(block+0x164)==0);
    regs[0]=model;hook.text.Resume(m,regs);
    assert(m.Read(model+0x174)==TextMesh::Quads*6);
    for(unsigned i=0;i<original.text.positions.size();++i)
        assert(m.Read(block+TextMesh::Positions+4*i)==std::bit_cast<U32>(original.text.positions[i]));
    m.words[block+0x128]=2;m.words[block+0x124]=0;
    for(unsigned i=0;i<4;++i)hook.text.Resume(m,regs);
    assert(!hook.text.Pending() && hook.text.Owned(m) && regs==saved);
    {
        // Shadow, dungeon corner, then icons/text. Allocation failure must still
        // continue with the original prompt sequence.
        std::atomic_store(&assets,std::shared_ptr<const Assets>(std::make_shared<Assets>(*image)));
        SetLayout(Layout{},false);
        m.words[0x5066f8+0x20]=source;m.words[0x5066f8+0x34]=0;m.words[0x506cb0+0x38]=2;
        Hook navigation_hook;regs=saved;
        navigation_hook.Begin(m,regs,PauseMenu::Sites[19].pc,true,1000);
        assert(navigation_hook.shadow.Pending() && !navigation_hook.icons.Pending());
        regs[0]=block;navigation_hook.Resume(m,regs);
        assert(regs[3]==NavigationShadowSize*NavigationShadowSize*4);
        regs[0]=texture;navigation_hook.Resume(m,regs);navigation_hook.Resume(m,regs);
        regs[0]=model;navigation_hook.Resume(m,regs);
        m.words[block+0x128]=2;m.words[block+0x124]=0;
        for(unsigned i=0;i<3;++i)navigation_hook.Resume(m,regs);
        assert(navigation_hook.shadow.Drawing() && !navigation_hook.icons.Pending());
        navigation_hook.Resume(m,regs);
        assert(!navigation_hook.shadow.Pending() && navigation_hook.corner.Pending() && !navigation_hook.icons.Pending());
        regs[0]=0;navigation_hook.Resume(m,regs); // Optional corner allocation fails safely.
        assert(!navigation_hook.shadow.Pending() && navigation_hook.icons.Pending());
        Hook failed;regs=saved;failed.Begin(m,regs,PauseMenu::Sites[19].pc,true,1000);
        regs[0]=0;failed.Resume(m,regs);
        assert(!failed.shadow.Pending() && failed.corner.Pending() && !failed.icons.Pending());
        regs[0]=0;failed.Resume(m,regs);
        assert(!failed.shadow.Pending() && failed.icons.Pending());
        // Overworld vignette is independent and disabled by default.
        m.words[0x4fda6c+0x50]=0;
        Hook overworld;regs=saved;overworld.Begin(m,regs,PauseMenu::Sites[19].pc,true,1000);
        regs[0]=0;overworld.Resume(m,regs);
        assert(!overworld.corner.Pending() && overworld.icons.Pending());
        auto map_layout=PauseMenu::GetLayout();
        map_layout.world_prompt_vignette.enabled=true;
        map_layout.world_prompt_vignette.color={.2f,.4f,.7f};
        PauseMenu::SetLayout(map_layout,false);
        Hook tinted_world;regs=saved;
        tinted_world.Begin(m,regs,PauseMenu::Sites[19].pc,true,1000);
        regs[0]=0;tinted_world.Resume(m,regs);
        assert(tinted_world.corner.Pending() && !tinted_world.icons.Pending());
        const auto& tint=tinted_world.corner_geometry;
        assert(tint.count==144 && CornerPixels[0]==0xffffffff);
        assert(near(tint.colors[0],.2f) && near(tint.colors[1],.4f) && near(tint.colors[2],.7f));
        assert(tint.colors[3]==0 && near(tint.colors[144*16-1],.55f));
        // The existing dungeon corner retains its black appearance.
        const auto dungeon=MapPromptCorner(map_layout.dungeon_prompt_vignette,400);
        assert(dungeon.colors[0]==0 && dungeon.colors[1]==0 && dungeon.colors[2]==0);
        PauseMenu::SetLayout(PauseMenu::Layout{},false);
    }
    PauseMenu::Hook pause;regs[15]=PauseMenu::Sites[19].pc+4;
    pause.Handle(m,regs,19);assert(regs[14]==0x30056c && regs[15]==0x2ffe40);
    // Optional local preview of the generated lettering; no game assets enter
    // the source tree or distribution.
    if(argc>2) {
        std::ofstream ppm(argv[2],std::ios::binary);
        ppm<<"P6\n"<<TextWidth<<" "<<TextHeight<<"\n255\n";
        for(unsigned y=0;y<TextHeight;++y)for(unsigned x=0;x<TextWidth;++x) {
            const U32 p=image->text[Tiled(x,TextHeight-1-y)],a=p&255;
            for(unsigned shift:{24u,16u,8u})ppm.put(char((((p>>shift)&255)*a+40*(255-a))/255));
        }
    }
    std::cout<<"Pause prompt native assets, pack-compatible atlas, orientation, independent poses, visibility and guest draw lifecycle passed\n";
}
