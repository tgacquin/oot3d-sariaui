// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hud_hooks.h"
#include "oot3d/loading_icon_assets.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
using namespace OOT3D::HUD;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {
        auto it=words.find(at);return it==words.end()?std::nullopt:std::optional<U32>{it->second};
    }
    void Write(U32 at,U32 word) {assert(words.contains(at));words[at]=word;}
    float Float(U32 at) {return std::bit_cast<float>(words.at(at));}
    void Float(U32 at,float f) {words[at]=std::bit_cast<U32>(f);}
};
int main(int argc,char** argv) {
    Memory m;
    if(argc>1) {
        std::ifstream f(argv[1],std::ios::binary);assert(f);U32 w;
        for(U32 a=0x100000;f.read(reinterpret_cast<char*>(&w),4);a+=4)m.words[a]=w;
        assert(CodeMatches(m));
        m.words[Sites[0].pc+4]^=1;assert(!CodeMatches(m));m.words.clear();
    }
    constexpr U32 helper=0x10000000,vertices=0x10001000,counter=0x10008000;
    auto setup=[&](unsigned count) {
        m.words.clear();m.words[helper]=count;m.words[helper+0x10]=vertices;
        for(unsigned q=0;q<count;++q) {
            std::array<float,12> v{82,2,7,94,2,7,82,14,7,94,14,7};
            for(unsigned i=0;i<12;++i)m.Float(vertices+q*48+i*4,v[i]);
        }
    };
    std::array<U32,16> regs{};
    Layout layout{};
    RoutingState routing;
    auto apply = [&](unsigned site, const Layout& poses) { Handle(m, regs, site, poses, routing); };
    // Main atlas: only the explicitly identified quads move.
    setup(92);m.words[Main+4]=helper;regs[4]=Main;
    auto original=m.words;
    apply(0,layout);assert(m.Float(vertices+63*48)==12 && regs[0]==helper);
    const auto defaults=m.words; m.words=original;
    layout[Hearts]={10,20,2};layout[Magic]={30,40,.5f};layout[Rupees]={50,60,1};
    apply(0,layout);
    for(unsigned q=0;q<92;++q) {
        const U32 at=vertices+q*48;
        if(q>=63 && q<83) {assert(m.Float(at)==22 && m.Float(at+4)==30);assert(m.Float(at+12)==46);}
        else if(q>=59 && q<=62) {assert(m.Float(at)==44 && m.Float(at+4)==63);}
        else if(q==26) {assert(m.Float(at)==142 && m.Float(at+4)==163);}
        else if(q==27) {assert(m.Float(at)==92 && m.Float(at+4)==43);}
        else if(q>=86) {assert(m.Float(at)==122 && m.Float(at+4)==2);}
        else for(unsigned i=0;i<12;++i)assert(m.words[at+i*4]==original.at(at+i*4));
        for(unsigned i=2;i<12;i+=3)assert(m.Float(at+i*4)==7);
    }
    // Native regeneration makes repeated frames stable, and resetting exact.
    const auto transformed=m.words;m.words=original;apply(0,layout);assert(m.words==transformed);
    // Mounted native code parks hearts and magic at +400 X. Unpark only
    // the fresh upload and obtain the exact on-foot result with edited poses.
    m.words=original;
    constexpr U32 mounted_offsets=0x10009000;
    m.words[helper+0x1c]=mounted_offsets;m.words[Action+0x48]=5;
    for(unsigned q=59;q<83;++q) {
        m.Float(mounted_offsets+q*8,400);m.Float(mounted_offsets+q*8+4,0);
        for(unsigned v=0;v<4;++v) {
            const U32 at=vertices+q*48+v*12;
            m.Float(at,m.Float(at)+400);
        }
    }
    apply(0,layout);
    for(unsigned q=59;q<83;++q) {
        for(unsigned i=0;i<12;++i)
            assert(m.words.at(vertices+q*48+i*4)==transformed.at(vertices+q*48+i*4));
        assert(m.Float(mounted_offsets+q*8)==400); // Never alter native animation state.
    }
    m.words=original;apply(0,{});assert(m.words==defaults);
    // Six horse carrots move/scale as a row, including spacing; the native
    // spent-carrot texture and offscreen visibility remain game-owned.
    m.words=original;layout={};layout[Carrots]={10,30,.5f};
    for(unsigned q=86;q<92;++q) {
        const float x=88+24*(q-86);
        const std::array<float,12> v{x,8,7,x+24,8,7,x,32,7,x+24,32,7};
        for(unsigned i=0;i<12;++i)m.Float(vertices+q*48+i*4,v[i]);
    }
    const auto carrot_source=m.words;
    for(float spacing : {0.f,-20.f,6.f,40.f}) {
        m.words=carrot_source;layout.carrot_spacing=spacing;apply(0,layout);
        for(unsigned q=86;q<92;++q) {
            const auto at=vertices+q*48;
            assert(m.Float(at)==174+12*(q-86)+(float(q-86)-2.5f)*spacing*.5f);
            assert(m.Float(at+4)==44);
            assert(m.Float(at+12)-m.Float(at)==12 && m.Float(at+28)-m.Float(at+4)==12);
        }
        assert((m.Float(vertices+86*48)+m.Float(vertices+91*48+12))*.5f==210);
    }
    SetLayout(layout,false);
    for(float overall:{.5f,1.f,2.f}) {
        OOT3D::UserHUD::Layout user;user.overall_scale=overall;
        OOT3D::UserHUD::SetLayout(user,false);
        m.words=carrot_source;apply(0,GetEffectiveLayout());
        assert(std::abs((m.Float(vertices+86*48)+m.Float(vertices+91*48+12))*.5f-210)<.001f);
    }
    SetLayout({},false);OOT3D::UserHUD::SetLayout({},false);
    m.words=original;layout={};layout[Hearts]={-400,0,1};layout[Rupees]={0,240,1};
    apply(0,layout);
    assert(m.Float(vertices+63*48)==-388); // Routing offsets don't reject extreme edits.
    assert(m.Float(vertices+26*48+4)==343);
    // Spacing translates whole hearts, restarting at each ten-heart row.
    // It must not stretch sprites, affect other HUD quads, or accumulate.
    m.words=original;layout={};layout[Hearts]={10,20,2};layout.heart_spacing=3;
    apply(0,layout);
    for(unsigned q=63;q<83;++q) {
        const auto at=vertices+q*48;
        assert(m.Float(at)==22+((q-63)%10)*6);
        assert(m.Float(at+12)-m.Float(at)==24);
    }
    const auto spaced=m.words;m.words=original;apply(0,layout);assert(m.words==spaced);
    m.words=original;apply(0,{});assert(m.words==defaults);
    // Check single-row clearance and the existing second-row position, including after damage.
    for (const bool custom : {false, true}) for (float heart_scale : {.5f, 1.f, 2.f}) {
        Layout poses;poses[Hearts].scale=heart_scale;poses[Magic]={7,-9,.5f};
        if (custom) {poses.magic_one_row_y=-3.5f;poses.magic_two_row_y=7.5f;}
        m.words=original;apply(0,poses);
        const auto baseline=m.words;
        for (U32 capacity : {0x30u, 0xa0u, 0xa1u, 0xb0u, 0x140u}) {
            for (U32 health : {0x10u, capacity}) {
                m.words=original;
                m.words[0x587958+0x40]=(capacity<<16)|0xffff;
                m.words[0x587958+0x44]=health;
                apply(0,poses);
                const float dy=(capacity>0xa0 ? (custom ? 7.5f : 13.f) : (custom ? -3.5f : 2.f))*heart_scale;
                for(unsigned q=59;q<63;++q)for(unsigned v=0;v<4;++v) {
                    const U32 at=vertices+q*48+v*12;
                    assert(m.words.at(at)==baseline.at(at));
                    assert(m.Float(at+4)==std::bit_cast<float>(baseline.at(at+4))+dy);
                    assert(m.words.at(at+8)==baseline.at(at+8));
                }
            }
        }
    }
    m.words=defaults;
    // A depleted magic fill remains a correctly transformed zero-width edge,
    // allowing render interpolation into/out of zero with an edited HUD pose.
    {
        const auto before=m.words;
        const U32 at=vertices+62*48;
        const std::array<float,12> empty{82,2,7,82,2,7,82,6,7,82,6,7};
        for(unsigned i=0;i<12;++i)m.Float(at+i*4,empty[i]);
        Layout magic_layout;magic_layout[Magic]={30,40,.5f};apply(0,magic_layout);
        assert(m.Float(at)==44 && m.Float(at+4)==63);
        assert(m.Float(at+12)==44 && m.Float(at+28)==65);
        m.words=before;
    }
    // Hidden unused hearts and an unacquired magic meter must remain hidden.
    for(unsigned q: {59u,63u})for(unsigned i=0;i<12;i+=3) {
        m.Float(vertices+q*48+i*4,400);m.Float(vertices+q*48+(i+1)*4,400);
    }
    apply(0,layout);assert(m.Float(vertices+63*48)==400 && m.Float(vertices+59*48)==400);
    setup(8);m.words[Action+0x18]=helper;regs[4]=Action;original=m.words;
    layout[AButton]={15,-20,1};apply(1,layout);
    assert(m.Float(vertices)==97 && m.Float(vertices+4)==-18);
    assert(m.Float(vertices+48)==97);
    for(unsigned q=2;q<8;++q)for(unsigned i=0;i<12;++i)
        assert(m.words[vertices+q*48+i*4]==original.at(vertices+q*48+i*4));
    // Navi text is its own quad: move it without moving A, map arrows,
    // timers, or the D-pad. Native hidden geometry must stay hidden.
    m.words=original;layout={};layout.navi_text={37,-21,1};apply(1,layout);
    for(unsigned q=0;q<8;++q)for(unsigned v=0;v<4;++v) {
        const U32 at=vertices+q*48+v*12;
        assert(m.Float(at)==std::bit_cast<float>(original.at(at))+(q==4?37:0));
        assert(m.Float(at+4)==std::bit_cast<float>(original.at(at+4))-(q==4?21:0));
    }
    m.words=original;apply(1,layout);
    assert(m.Float(vertices+4*48)==119);
    m.words=original;layout={};apply(1,layout);assert(m.words==original);
    // Map image lives in another model; its two arrows share this atlas with A.
    setup(8);m.words[Action+0x18]=helper;regs[4]=Action;original=m.words;
    layout = {};layout[Minimap]={80,-20,1.5f};apply(1,layout);
    for(unsigned q=0;q<8;++q) {
        if(q==2 || q==3) {
            assert(m.Float(vertices+q*48)==183 && m.Float(vertices+q*48+4)==-119);
        } else for(unsigned i=0;i<12;++i)
            assert(m.words[vertices+q*48+i*4]==original.at(vertices+q*48+i*4));
    }
    m.words=original;layout={};apply(1,layout);assert(m.words==original);
    // Minigame offset affects only the native B quad, in screen pixels even
    // with a scaled button. Ordinary gameplay and horseback retain their pose.
    for(unsigned mode=0;mode<=5;++mode) {
        setup(8);m.words[Action+0x18]=helper;regs[4]=Action;m.words[Action+0x48]=mode;
        const auto source=m.words;
        OOT3D::HUDButtons::Layout buttons;buttons.buttons[OOT3D::HUDButtons::B].pose={340,210,1.5f};
        OOT3D::HUDButtons::SetLayout(buttons,false);apply(1,{});const auto baseline=m.words;
        m.words=source;buttons.minigame_b_x=20;OOT3D::HUDButtons::SetLayout(buttons,false);apply(1,{});
        for(unsigned q=0;q<8;++q)for(unsigned v=0;v<12;++v) {
            const auto at=vertices+q*48+v*4;
            const float expected=std::bit_cast<float>(baseline.at(at))+
                (q==7 && v%3==0 && mode>=1 && mode<=4?20.f:0.f);
            assert(m.Float(at)==expected);
        }
    }
    OOT3D::HUDButtons::SetLayout({},false);
    layout[Rupees]={50,60,1};
    // Number renderer also serves timers, keys and ammunition: match owner.
    setup(3);m.words[Main+0x1c]=counter;m.words[counter+8]=helper;regs[4]=counter;
    apply(2,layout);assert(m.Float(vertices)==142);
    setup(3);m.words[Main+0x1c]=counter+4;m.words[counter+8]=helper;original=m.words;
    apply(2,layout);assert(m.words==original);
    // Counter-local controls compose with the shared HUD pose. Native text
    // uses only the icon gap; HD-only offsets, scale and advance are dormant.
    setup(3);m.words[Main+0x1c]=counter;m.words[counter+8]=helper;regs[4]=counter;
    for(unsigned q=0;q<3;++q) {
        const float x=20+10*(2-q);
        const std::array<float,12> v{x,110,7,x+11,110,7,x,124,7,x+11,124,7};
        for(unsigned i=0;i<12;++i)m.Float(vertices+q*48+i*4,v[i]);
    }
    const auto local_source=m.words;
    layout={};layout[Rupees]={50,60,2};layout.rupee_number_gap=5;
    layout.hd_rupee_text={3,-4,1.5f};layout.hd_rupee_spacing=2;
    for(bool hd:{true,false}) {
        layout.hd_rupee_numbers=hd;m.words=local_source;apply(2,layout);
        for(unsigned q=0;q<3;++q) {
            assert(m.Float(vertices+q*48)==(hd?186.f-36*q:148.f-20*q));
            assert(m.Float(vertices+q*48+4)==(hd?262:270));
            assert(m.Float(vertices+q*48+12)-m.Float(vertices+q*48)==(hd?33:22));
            assert(m.Float(vertices+q*48+8)==7);
        }
        const auto adjusted=m.words;m.words=local_source;apply(2,layout);assert(m.words==adjusted);
    }
    layout.hd_rupee_numbers=true;
    setup(92);m.words[Main+4]=helper;regs[4]=Main;
    const auto icon_source=m.words;
    auto group_only=layout;group_only.rupee_number_gap=0;group_only.hd_rupee_text={};group_only.hd_rupee_spacing=0;
    apply(0,group_only);const auto icon_layout=m.words;
    m.words=icon_source;apply(0,layout);assert(m.words==icon_layout);
    // Both counters share the HD text offsets, size, spacing and icon gap.
    setup(3);m.words[Main+0x20]=counter;m.words[counter+8]=helper;regs[4]=counter;
    for(unsigned q=0;q<3;++q) {
        const float x=q==2?400.f:20.f+10*(1-q);
        const std::array<float,12> v{x,150,7,x+11,150,7,x,171,7,x+11,171,7};
        for(unsigned i=0;i<12;++i)m.Float(vertices+q*48+i*4,v[i]);
    }
    const auto key_source=m.words;
    layout={};layout[Keys]={-13.5f,12.2f,.67f};layout[Rupees].scale=.67f;
    layout.hd_rupee_text={-1.2f,-3.6f,1.28f};layout.rupee_number_gap=17;layout.hd_rupee_spacing=-2;
    layout.hd_rupee_numbers=false;apply(2,layout);const auto native_key=m.words;
    const float key_left=m.Float(vertices+48),key_top=m.Float(vertices+4);
    assert(std::abs(key_left-(2+18*.67f+10-13.5f+17*.67f))<1e-4f);
    m.words=key_source;layout.hd_rupee_numbers=true;apply(2,layout);
    assert(std::abs(m.Float(vertices+12)-m.Float(vertices)-11*.67f*1.28f)<1e-4f);
    assert(std::abs(m.Float(vertices+28)-m.Float(vertices+4)-21*.67f*1.28f)<1e-4f);
    assert(std::abs(m.Float(vertices+48)-(key_left-1.2f*.67f))<1e-4f);
    assert(std::abs(m.Float(vertices+4)-(key_top-3.6f*.67f))<1e-4f);
    assert(std::abs(m.Float(vertices)-m.Float(vertices+48)-8*.67f*1.28f)<1e-4f);
    for(unsigned i=0;i<12;++i)assert(m.words.at(vertices+96+i*4)==native_key.at(vertices+96+i*4));
    const auto hd_key=m.words;m.words=key_source;apply(2,layout);assert(m.words==hd_key);
    m.words=key_source;layout.hd_rupee_numbers=false;apply(2,layout);assert(m.words==native_key);
    // The key icon is in the main atlas and ignores the HD text scale.
    setup(92);m.words[Main+4]=helper;regs[4]=Main;const auto key_icon_source=m.words;
    apply(0,layout);const auto key_icon_native=m.words;
    m.words=key_icon_source;layout.hd_rupee_numbers=true;apply(0,layout);assert(m.words==key_icon_native);
    // Hidden counter quads may be parked past only one screen edge. Scaling
    // or moving rupees must never pull those untextured rectangles into view.
    setup(3);m.words[Main+0x1c]=counter;m.words[counter+8]=helper;regs[4]=counter;
    for(unsigned q=1;q<3;++q) {
        const float x=q==1?400.f:320.f;
        const std::array<float,12> v{x,128,7,x+20,128,7,x,148,7,x+20,148,7};
        for(unsigned i=0;i<12;++i)m.Float(vertices+q*48+i*4,v[i]);
    }
    const auto fresh_counter=m.words;
    for(const Pose pose: {Pose{-9.1f,-23.7f,.75f},Pose{100,-60,.5f},Pose{-10,-101,1}}) {
        m.words=fresh_counter;layout[Rupees]=pose;apply(2,layout);
        for(unsigned q=1;q<3;++q) for(unsigned i=0;i<12;i+=3) {
            assert(m.Float(vertices+q*48+i*4)==m.Float(vertices+q*48));
            assert(m.Float(vertices+q*48+(i+1)*4)==m.Float(vertices+q*48+4));
            assert(m.Float(vertices+q*48+(i+2)*4)==7);
        }
        // The visible number remains a nondegenerate quad at the requested pose.
        assert(m.Float(vertices+12)>m.Float(vertices));
        const auto rendered=m.words;m.words=fresh_counter;apply(2,layout);assert(m.words==rendered);
    }
    layout={};
    // The five timer quads are stored right-to-left. Its local controls
    // precede the whole Timer model transform and never touch the clock atlas.
    setup(5);m.words[Action+0x2c]=counter;m.words[counter+8]=helper;regs[4]=counter;
    for(unsigned q=0;q<5;++q)for(unsigned v=0;v<4;++v) {
        m.Float(vertices+q*48+v*12,182+10*(4-q)+12*(v%2));
        m.Float(vertices+q*48+v*12+4,215+21*(v/2));
    }
    const auto native_timer=m.words;
    layout={};layout.hd_timer_text={3,-4,1.5f};layout.hd_timer_spacing=2;layout.hd_rupee_spacing=7;
    for(bool hd:{true,false}) {
        m.words=native_timer;layout.hd_timer_numbers=hd;apply(2,layout);
        for(unsigned q=0;q<5;++q) {
            assert(m.Float(vertices+q*48)==(hd?185+18*(4-q):182+10*(4-q)));
            assert(m.Float(vertices+q*48+4)==(hd?211:215));
            assert(m.Float(vertices+q*48+12)-m.Float(vertices+q*48)==(hd?18:12));
            assert(m.Float(vertices+q*48+8)==7);
        }
        const auto transformed_timer=m.words;
        m.words=native_timer;apply(2,layout);assert(m.words==transformed_timer);
    }
    m.words=native_timer;m.words[Action+0x2c]=counter+4;layout.hd_timer_numbers=true;
    const auto other_counter=m.words;apply(2,layout);assert(m.words==other_counter);
    setup(8);m.words[Action+0x18]=helper;regs[4]=Action;
    const auto clock_source=m.words;apply(1,layout);assert(m.words==clock_source);
    layout={};
    // Timer submission is a direct draw, with no QueueTop model. Register
    // its final mesh from the actual material callback instead of queue entry.
    setup(5);m.words[Action+0x2c]=counter;m.words[counter+8]=helper;
    constexpr U32 timer_model=0x10020000,timer_renderer=0x10021000;
    m.words[counter+0x10]=timer_model;m.words[timer_renderer+0x354]=timer_model;
    m.words[helper+0x14]=vertices+0x1000;m.words[helper+0x18]=vertices+0x2000;
    for(unsigned i=0;i<5*32;i+=4)m.Float(vertices+0x1000+i,float(i)/1024);
    for(unsigned i=0;i<5*64;i+=4)m.Float(vertices+0x2000+i,1);
    const auto timer_mesh=Mesh(m,helper,true);assert(timer_mesh);
    assert(!IsTimerMesh(timer_mesh->hash,5));routing.drawing_top_model=0;
    regs[4]=timer_renderer;apply(8,layout);
    assert(IsTimerMesh(timer_mesh->hash,5) && !IsRouted(timer_mesh->hash,5));
    // Scores and native special-mode ammo register only the matching owner.
    m.words[Action+0x2c]=0;m.words[Action+0x30]=counter;
    apply(8,layout);assert(FindNumberMesh(timer_mesh->hash,5)==NumberKind::Score);
    assert(!FindNumberMesh(timer_mesh->hash,4));
    m.words[Action+0x30]=0;constexpr U32 item_owner=0x10030000;
    m.words[Action+0x40]=item_owner;m.words[item_owner+0x428]=2;m.words[item_owner+0xc]=counter;
    apply(8,layout);assert(FindNumberMesh(timer_mesh->hash,5)==NumberKind::Ammo);
    // Pause grid/slot counters share the ammo override through validated
    // controller ownership; unrelated number models must stay native.
    m.words[Action+0x40]=0;
    constexpr U32 pause_icon=0x10040000;
    m.words[0x5066f8+0x3c]=item_owner;m.words[item_owner+8]=pause_icon;
    m.words[pause_icon]=0x4ebd60;m.words[item_owner+0x224]=1;
    m.words[timer_model]=0x4ebd60;
    assert(FindAmmoCounter(m,[&](U32 c){return c==counter;})==counter);
    apply(8,layout);assert(FindNumberMesh(timer_mesh->hash,5)==NumberKind::Ammo);
    // Reproduce pause routing: submitted positions differ from the helper's
    // source, while the live rotating buffer is the one consumed by Vulkan.
    constexpr U32 submitted=0x10050000,descriptor=0x10051000,live0=0x10052000,live1=0x10053000;
    m.words[timer_model+4]=submitted;m.words[submitted]=descriptor;
    m.words[descriptor+12]=20;m.words[descriptor+0x1c]=0xa0;
    m.words[timer_model+0x174]=30;m.words[submitted+0x124]=0;
    m.words[submitted+0x1a0]=live0;m.words[submitted+0x1a4]=live1;
    for(unsigned buffer=0;buffer<2;++buffer) {
        const U32 live=buffer?live1:live0;
        // Separate contiguous attribute ranges (XYZ, UV, RGBA).
        for(unsigned i=0;i<5*48;i+=4)m.words[live+i]=m.words.at(vertices+i);
        for(unsigned i=0;i<5*32;i+=4)m.words[live+5*48+i]=m.words.at(vertices+0x1000+i);
        for(unsigned i=0;i<5*64;i+=4)m.words[live+5*80+i]=m.words.at(vertices+0x2000+i);
        m.Float(live,40.f+buffer*10); // Pause-specific relocation.
        m.words[submitted+0x124]=buffer;
        const auto actual=SubmittedNumberMesh(m,timer_model);assert(actual && actual->hash!=timer_mesh->hash);
        assert(!FindNumberMesh(actual->hash,5));
        apply(8,layout);assert(FindNumberMesh(actual->hash,5)==NumberKind::Ammo);
        m.Float(vertices,999); // Stale helper must not change submitted identity.
        assert(SubmittedNumberMesh(m,timer_model)->hash==actual->hash);
    }
    m.words[submitted+0x124]=2;assert(!SubmittedNumberMesh(m,timer_model));
    m.words[submitted+0x124]=0;m.words[descriptor+0x1c]=0xa8;
    assert(!SubmittedNumberMesh(m,timer_model));
    m.words[descriptor+0x1c]=0xa0;m.words[descriptor+12]=0;m.words[timer_model+0x174]=6;
    assert(SubmittedNumberMesh(m,timer_model)->quads==1);
    m.words[timer_model+4]=0;
    m.words[pause_icon]=0;
    assert(!FindAmmoCounter(m,[&](U32 c){return c==counter;}));
    m.words[SkulltulaCounter]=counter;
    const auto skull_mesh=Mesh(m,helper,true);assert(skull_mesh);
    apply(8,layout);assert(FindNumberMesh(skull_mesh->hash,5)==NumberKind::Skulltula);
    // HD-only transforms leave native size/Z intact; Skulltulas always center.
    for(unsigned category=0;category<4;++category) {
        const bool score=category==1;
        setup(2);regs[4]=counter;m.words[counter+8]=helper;
        if(score)m.words[Action+0x30]=counter;
        else if(category==3)m.words[SkulltulaCounter]=counter;
        else if(category==2) {
            m.words[0x5066f8+0x40]=item_owner;m.words[item_owner+8]=pause_icon;
            m.words[pause_icon]=0x4ebd60;m.words[item_owner+0x224]=1;
            m.words[item_owner+0xc]=counter;m.words[counter+0x10]=timer_model;
            m.words[timer_model]=0x4ebd60;
        }
        else {m.words[Main+0x14]=item_owner;m.words[item_owner+0xc]=counter;}
        for(unsigned q=0;q<2;++q)for(unsigned v=0;v<4;++v)
            m.Float(vertices+q*48+v*12,100+10*(1-q)+12*(v%2));
        const auto before=m.words;layout={};layout.hd_score_text=layout.hd_ammo_text={3,4,2};layout.hd_score_spacing=layout.hd_ammo_spacing=2;layout.hd_rupee_spacing=7;
        layout.hd_pause_ammo_text={-4,7,.5f};
        layout.hd_skulltula_text={3,4,2};layout.hd_skulltula_spacing=2;
        for(bool master:{false,true}) {
            m.words=before;layout.hd_numbers=master;apply(2,layout);
            assert(m.Float(vertices+8)==7);
            if(!master) {
                if(category==3)assert((m.Float(vertices+48)+m.Float(vertices+12))*.5f==128);
                else assert(m.words==before);
            }
            else {
                assert(m.Float(vertices+12)-m.Float(vertices)==(category==2?6:24));
                assert(m.Float(vertices)==(category==3?131:category==2?112:score?127:101));
                assert(m.Float(vertices+4)==(category==2?15:score?6:-6));
            }
        }
        m.words=before;layout.hd_numbers=true;layout.hd_score_numbers=layout.hd_ammo_numbers=false;
        layout.hd_skulltula_numbers=false;
        apply(2,layout);
        if(category==3)assert((m.Float(vertices+48)+m.Float(vertices+12))*.5f==128);
        else assert(m.words==before);
    }
    // Skulltula's native three slots must share one center for 1/2/3 digits,
    // including edited scale/spacing. Parked slots remain byte-for-byte intact.
    for(bool master:{false,true})for(bool hd:{false,true})
    for(unsigned digits=1;digits<=3;++digits)for(float scale:{.5f,1.f,2.f})for(float spacing:{0.f,2.f}) {
        setup(3);regs[4]=counter;m.words[counter+8]=helper;m.words[SkulltulaCounter]=counter;
        for(unsigned q=0;q<3;++q)for(unsigned v=0;v<4;++v)
            m.Float(vertices+q*48+v*12,q<digits?132.f-10*q+11*(v%2):400.f);
        const auto before=m.words;
        layout={};layout.hd_skulltula_text={3,4,scale};layout.hd_skulltula_spacing=spacing;
        layout.hd_numbers=master;layout.hd_skulltula_numbers=hd;
        apply(2,layout);
        const float left=m.Float(vertices+(digits-1)*48),right=m.Float(vertices+12);
        const bool styled=master && hd;
        assert(std::abs((left+right)*.5f-(styled?131.f:128.f))<.0001f);
        assert(std::abs((right-left)-(11+(digits-1)*(10+(styled?spacing:0)))*(styled?scale:1))<.0001f);
        if(!styled)for(unsigned q=0;q<digits;++q)for(unsigned v=0;v<4;++v)
            for(unsigned axis:{1u,2u})assert(m.words.at(vertices+q*48+v*12+axis*4)==before.at(vertices+q*48+v*12+axis*4));
        for(unsigned q=digits;q<3;++q)for(unsigned i=0;i<12;++i)
            assert(m.words.at(vertices+q*48+i*4)==before.at(vertices+q*48+i*4));
    }
    layout={};
    // B ammunition has its own local pose for native and HD numbers. Other
    // equipped counters retain their placement even while the B HUD is active.
    for(bool hd:{false,true})for(bool special:{false,true}) {
        setup(1);regs[4]=counter;m.words[counter+8]=helper;
        m.words[special?Action+0x40:Main+0x14]=item_owner;
        m.words[item_owner+0x428]=2;m.words[item_owner+0xc]=counter;
        m.words[Action+0x48]=1;
        layout={};layout.hd_numbers=hd;layout.hd_ammo_text={3,4,2};
        const auto source=m.words;
        OOT3D::HUDButtons::SetLayout({},false);apply(2,layout);const auto baseline=m.words;
        auto buttons=OOT3D::HUDButtons::Layout{};buttons.b_ammo={8,-5,1.5f};
        OOT3D::HUDButtons::SetLayout(buttons,false);m.words=source;apply(2,layout);
        if(!special)assert(m.words==baseline);
        else {
            const float right=std::bit_cast<float>(baseline.at(vertices+12));
            const float bottom=std::bit_cast<float>(baseline.at(vertices+28));
            for(unsigned v=0;v<4;++v) {
                const U32 at=vertices+v*12;
                assert(m.Float(at)==right+(std::bit_cast<float>(baseline.at(at))-right)*1.5f+8);
                assert(m.Float(at+4)==bottom+(std::bit_cast<float>(baseline.at(at+4))-bottom)*1.5f-5);
                assert(m.words[at+8]==baseline.at(at+8));
            }
        }
    }
    OOT3D::HUDButtons::SetLayout({},false);layout={};
    // Wrong atlas size / invalid settings fail closed.
    setup(91);m.words[Main+4]=helper;regs[4]=Main;original=m.words;
    apply(0,layout);assert(m.words==original);
    layout[Hearts].scale=std::numeric_limits<float>::quiet_NaN();
    Quads(m,helper,63,20,layout[Hearts],82,2);assert(m.words==original);
    m.words.erase(vertices+4);original=m.words;
    Quads(m,helper,0,1,{10,10,2},82,2);assert(m.words==original);

    // Ocarina return removes native meter translations before offscreen
    // rejection; source pulses/fill, missing hearts and all native offsets stay intact.
    constexpr U32 offsets=0x10010000,source=0x10011000;
    setup(92);m.words[helper+0x1c]=offsets;
    for(unsigned q=0;q<92;++q) {
        m.Float(offsets+q*8,0);m.Float(offsets+q*8+4,0);
    }
    for(unsigned q=59;q<83;++q) {
        m.Float(offsets+q*8+4,-36);
        for(unsigned v=0;v<4;++v)m.Float(vertices+q*48+v*12+4,m.Float(vertices+q*48+v*12+4)-36);
    }
    // One hidden/unowned heart remains parked after removal of the animation.
    for(unsigned v=0;v<4;++v) {
        m.Float(vertices+82*48+v*12,400+(v%2)*12);
        m.Float(vertices+82*48+v*12+4,364+(v/2)*12);
    }
    OOT3D::HUDReturn::Stationary(m,helper,59,24);
    for(unsigned q=59;q<82;++q) {
        assert(m.Float(vertices+q*48+4)==2);
        assert(m.Float(offsets+q*8+4)==-36);
    }
    assert(m.Float(vertices+82*48+4)==400);
    // Native hidden rupee count uses Y=400; restore only its draw positions.
    setup(3);m.words[counter+4]=3;m.words[helper+0xc]=source;
    for(unsigned q=0;q<3;++q)for(unsigned v=0;v<4;++v) {
        const float x=20+10*(2-q)+(v%2)*11,y=400+(v/2)*12;
        for(U32 base:{source,vertices}) {
            m.Float(base+q*48+v*12,x);m.Float(base+q*48+v*12+4,y);m.Float(base+q*48+v*12+8,0);
        }
    }
    // Hidden leading digit retains its separate offscreen offset.
    for(unsigned v=0;v<4;++v)m.Float(vertices+2*48+v*12,m.Float(vertices+2*48+v*12)+400);
    OOT3D::HUDReturn::RupeeDigits(m,counter,helper);
    assert(m.Float(vertices+4)==110 && m.Float(source+4)==400);
    assert(m.Float(vertices+2*48)==420);
    Quads(m,helper,0,3,TopPose({},Rupees),2,111,320);
    assert(m.Float(vertices+4)==211 && m.Float(vertices+12)>m.Float(vertices));
    assert(m.Float(vertices+2*48)==m.Float(vertices+2*48+12));

    // Loading icon: both native sizes and all three animated layers use a
    // fresh bounds rectangle. Preserve controller flags, alpha and UV data.
    constexpr U32 loading = 0x11000000, stack = 0x12000000, rect = stack + 0x44;
    // A top-screen loading draw supplies the real origin and native size.
    // Match its edited rectangle, not just the previous randomizer rectangle.
    for(unsigned size:{16u,32u}) {
        m.words.clear();m.words[loading]=0x4ed274;
        m.words[loading+8]=1|(size==32?0x100:0);m.words[loading+0x20]=0x101;
        m.Float(loading+0x14,360);m.Float(loading+0x18,208);
        const float half=size/2.f;
        for(unsigned i=0;i<4;++i)m.Float(rect+4*i,std::array{208-half,208+half,360-half,360+half}[i]);
        regs[4]=loading;regs[13]=stack;layout[LoadingIcon]={12,-9,1.25f};
        apply(5,layout);
        const auto generated=OOT3D::LoadingIcon::Draw(0,.8f,layout[LoadingIcon]);
        float left=1000,right=-1000,top=1000,bottom=-1000;
        for(unsigned i=0;i<12;i+=3) {
            left=std::min(left,generated.positions[i]);right=std::max(right,generated.positions[i]);
            top=std::min(top,generated.positions[i+1]);bottom=std::max(bottom,generated.positions[i+1]);
        }
        assert(left==m.Float(rect+8) && right==m.Float(rect+12));
        assert(top==m.Float(rect) && bottom==m.Float(rect+4));
        // A duplicate layer must not feed the edited bounds back into the anchor.
        assert(!LoadingRect(m,loading,rect,layout[LoadingIcon]));
        assert(generated.positions==OOT3D::LoadingIcon::Draw(0,.8f,layout[LoadingIcon]).positions);
    }
    const auto generation_before_save=OOT3D::LoadingIcon::Draw(0,.8f,{-80,60,2});
    for (unsigned size : {16u, 32u}) {
        m.words.clear();
        m.words[loading] = 0x4ed274;
        m.words[loading+8] = 1 | (size == 32 ? 0x100 : 0);
        m.words[loading+0x20] = 0x100;
        m.Float(loading+0x14, 284); m.Float(loading+0x18, 24);
        const float half = size/2.f;
        const std::array<float,4> bounds{24-half,24+half,284-half,284+half};
        for(unsigned i=0;i<4;++i)m.Float(rect+i*4,bounds[i]);
        const auto native = m.words;
        regs[4] = loading; regs[13] = stack;
        apply(5, {}); assert(m.words == native && regs[3] == rect);
        layout[LoadingIcon] = {-80, 60, 2};
        for (unsigned layer=0;layer<3;++layer) {
            m.words = native; apply(5, layout);
            assert(m.Float(rect)==84-size && m.Float(rect+4)==84+size);
            assert(m.Float(rect+8)==204-size && m.Float(rect+12)==204+size);
            for (const auto& [at,value] : native)
                if(at<rect || at>=rect+16)assert(m.words.at(at)==value);
        }
        m.words=native;apply(5,{});assert(m.words==native);
        m.words[loading+0x20]=0; auto hidden=m.words;
        apply(5,layout);assert(m.words==hidden);
        m.words=native;m.words[loading]=0;auto unrelated=m.words;
        apply(5,layout);assert(m.words==unrelated);
        m.words=native;m.words.erase(rect+12);auto incomplete=m.words;
        apply(5,layout);assert(m.words==incomplete);
        m.words=native;layout[LoadingIcon].scale=std::numeric_limits<float>::quiet_NaN();
        apply(5,layout);assert(m.words==native);
    }
    // Saving uses (284,24), unlike the bottom-right generation icon. A later
    // creation must retain its position, dimensions, UVs and 80% opacity.
    const auto generation_after_save=OOT3D::LoadingIcon::Draw(0,.8f,{-80,60,2});
    assert(generation_before_save.positions==generation_after_save.positions);
    assert(generation_before_save.uvs==generation_after_save.uvs);
    assert(generation_before_save.colors==generation_after_save.colors);

    // Full native upload + top queue: move only the selected atlas quads.
    setup(92);
    constexpr U32 uv=0x10010000, colors=0x10014000, model=0x10018000;
    m.words[helper+0x14]=uv; m.words[helper+0x18]=colors;
    for(unsigned i=0;i<92*32;i+=4)m.Float(uv+i,.25f);
    for(unsigned i=0;i<92*64;i+=4)m.Float(colors+i,1.f);
    m.words[Main+4]=helper;m.words[Main+0xc]=model;m.words[model]=0x4ebd60;
    m.words[Main+0x34]=1;m.words[Main+0x2c]=0;
    constexpr U32 queue=0x5c0ba8, entries=queue+5*0x60+0x214, priorities=queue+5*0x60+0x4b4;
    for(unsigned i=0;i<24;++i){m.words[entries+i*4]=0;m.words[priorities+i*4]=0;}
    m.words[queue+0x28]=2;m.words[entries]=0x111;m.words[priorities]=0;
    m.words[entries+4]=0x222;m.words[priorities+4]=10;
    const auto mesh=Mesh(m,helper);assert(mesh);
    assert(!IsRouted(mesh->hash,92));
    regs[4]=Main;apply(3,{}); regs[0]=queue; regs[1]=5; apply(4,{});
    assert(m.words[queue+0x28]==3 && m.words[entries+4]==model && m.words[entries+8]==0x222);
    assert(IsRouted(mesh->hash,92));
    // Reproduce the capture: a stationary main HUD published each frame while
    // rupee counts generate fresh meshes must never be evicted by those digits.
    for(unsigned i=0;i<200;++i) {
        auto main=*mesh;main.routed=true;PublishMesh(main);
        PublishMesh({10000+i,3,true,20000+i,true});
        assert(IsRouted(mesh->hash,92) && IsRoutedGeometry(mesh->geometry_hash,92));
    }
    // A newer explicit non-routed publication must still revoke that identity.
    auto revoked=*mesh;revoked.routed=false;PublishMesh(revoked);
    assert(!IsRouted(mesh->hash,92) && !IsRoutedGeometry(mesh->geometry_hash,92));
    revoked.routed=true;PublishMesh(revoked);
    // The native rupee blink/fade may change vertex colors without changing
    // the split atlas. Preserve routing while keeping different UVs excluded.
    m.Float(colors+26*64,.25f);
    const auto blinking=Mesh(m,helper);assert(blinking && blinking->hash!=mesh->hash);
    assert(!IsRouted(blinking->hash,92) && IsRoutedGeometry(blinking->geometry_hash,92));
    m.Float(colors+26*64,1.f);
    unsigned selected=0;
    for(unsigned q=0;q<92;++q) {
        const bool top=KeepTriangle(92,q*4,true);
        assert(top!=KeepTriangle(92,q*4,false));
        selected+=top;
    }
    assert(selected==32); // 20 hearts, four magic quads, rupee/key icons, six carrots.
    for(unsigned q=0;q<3;++q)assert(KeepTriangle(3,q*4,true) && !KeepTriangle(3,q*4,false));
    // During pause/close, the gameplay HUD must not reappear via the top queue.
    m.words[queue+0x28]=0;regs[0]=queue;regs[1]=5;
    Handle(m,regs,4,{},routing,true);assert(m.words[queue+0x28]==0);
    m.Float(uv,.5f);assert(!IsRouted(Mesh(m,helper)->hash,92));
    assert(!IsRoutedGeometry(Mesh(m,helper)->geometry_hash,92));
    m.words[queue+0x28]=24;original=m.words;
    assert(!QueueTop(m,model) && m.words==original); // Never overrun native queue.
    m.words[queue+0x28]=0;m.words[Main+0x1c]=counter;
    m.words[counter+8]=helper;regs[4]=Main;
    apply(3,{});
    assert(routing.preparing_counter && regs[15]==0x2fc950 && regs[14]==Sites[3].pc);
    assert(m.words[queue+0x28]==0); // Queue only after fresh digit upload returns.
    apply(3,{}); regs[0]=queue; regs[1]=5; apply(4,{}); assert(!routing.preparing_counter);
    m.words[Main+0x34]=0x17;m.words[queue+0x28]=0;
    apply(3,{});assert(m.words[queue+0x28]==0 && !routing.preparing_counter);
    // Ocarina return prepares digits in the same frame as its icon even in
    // native states that normally defer counter submission until later.
    for(unsigned state:{13u,15u,17u,19u}) {
        routing={};m.words[Main+0x34]=state;regs[4]=Main;
        apply(3,{});assert(!routing.preparing_counter);
        routing.hud_return.returning=true;
        apply(3,{});assert(routing.preparing_counter && regs[15]==0x2fc950);
        apply(3,{});assert(routing.counter_ready && !routing.preparing_counter);
    }
    std::cout<<"PASS: HUD signatures, transforms, native visibility, top/bottom isolation, queue bounds and fresh counters\n";
}
