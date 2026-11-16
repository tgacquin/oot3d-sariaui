// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/world_map.h"
#include "video_core/texture/etc1.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
namespace W=OOT3D::WorldMap;
namespace P=OOT3D::DungeonParchment;
using TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    template<class T> void Floats(U32 a,const T& v) {for(unsigned i=0;i<v.size();++i)words[a+4*i]=std::bit_cast<U32>(v[i]);}
};
std::vector<unsigned char> Read(const char* path) {
    std::ifstream in(path,std::ios::binary);assert(in);return {std::istreambuf_iterator<char>(in),{}};
}
int main(int argc,char** argv) {
    assert(argc==4);
    auto decode=[](std::uint64_t bits,unsigned x,unsigned y) {const auto c=Pica::Texture::SampleETC1Subtile(bits,x,y);return std::array{c.r(),c.g(),c.b()};};
    auto paper=W::ExtractPaper(Read(argv[1]),decode);assert(paper && paper->colors.size()==208*176);
    W::paper_image.store(std::shared_ptr<const W::PaperImage>(paper));
    // Preserve fractional overlap/feather sampling while preparing coordinates
    // once per axis. Bounds intentionally differ from every paper rectangle.
    P::Paper reference;reference.Set(paper->colors,W::PaperWidth,W::PaperHeight);
    const P::Rect bounds{-30.25f,-20.5f,370.75f,260.25f};
    const std::vector<P::Rect> papers{{0,0,208,176},{23.5f,70.25f,231.5f,246.25f},
        {100.3f,10.1f,308.3f,186.1f},{143.5f,60.25f,351.5f,236.25f}};
    const W::PreparedPaper prepared_paper(reference,papers,bounds),empty(reference,{},bounds);
    for(float blend:{0.f,18.f,80.f})for(unsigned y=0;y<P::Height;y+=7)for(unsigned x=0;x<P::Width;x+=11) {
        const auto old=reference.At(std::lerp(bounds.left,bounds.right,(x+.5f)/P::Width),
            std::lerp(bounds.top,bounds.bottom,(y+.5f)/P::Height),papers,blend);
        const auto next=prepared_paper.At(reference,x,y,blend);
        for(unsigned c=0;c<4;++c)assert(std::abs(old[c]-next[c])<.002f);
        assert(empty.At(reference,x,y,blend)==P::Color{});
    }
    std::array<unsigned,P::Height> rows{};
    W::ComposeRows([&](unsigned first,unsigned last){for(unsigned y=first;y<last;++y)++rows[y];});
    for(auto count:rows)assert(count==1);
    W::Outline outline;outline.cut.hole={{10,10},{100,10},{100,100},{10,100}};outline.feather=10;outline.rounding=0;
    assert(W::Valid(outline));assert(W::Alpha(outline,5,50)==0 && W::Alpha(outline,10,50)<1e-6f);
    assert(std::abs(W::Alpha(outline,15,50)-.5f)<1e-6f && W::Alpha(outline,20,50)==1);
    outline.feather=0;assert(W::Alpha(outline,11,50)==1);
    outline.feather=-1;assert(!W::Valid(outline));outline.feather=8;
    outline.rounding=20;outline.feather=0;
    assert(W::Valid(outline) && W::Alpha(outline,11,11)==0 && W::Alpha(outline,30,30)==1);
    const auto rounded=W::Boundary(outline);assert(rounded.size()>4);
    for(const auto p:rounded)assert(p.x>=10 && p.x<=100 && p.y>=10 && p.y<=100);
    outline.cut.hole.push_back(outline.cut.hole.front());
    assert(W::Boundary(outline).size()==rounded.size()); // A duplicate closing point is harmless.
    outline.rounding=-1;assert(!W::Valid(outline));outline.rounding=8;outline.feather=8;
    // Concave outlines retain their notch, and vertices outside the artwork are rejected.
    outline.cut.hole={{0,0},{100,0},{100,40},{40,40},{40,100},{0,100}};
    assert(W::Valid(outline) && W::Alpha(outline,60,60)==0 && W::Alpha(outline,20,60)==1);
    outline.cut.hole[0].x=-1;assert(!W::Valid(outline));
    // Shadow derives from alpha, fades outside the artwork, and stays below it.
    std::vector<W::Color> silhouette(W::MapWidth*W::MapHeight);
    for(unsigned y=60;y<160;++y)for(unsigned x=100;x<200;++x)silhouette[y*W::MapWidth+x]={120,90,60,255};
    W::EdgeShadow shadow;shadow.Build(silhouette,{},12);
    assert(shadow.At(99,100)>shadow.At(94,100) && shadow.At(94,100)>0);
    assert(shadow.At(85,100)==0 && std::abs(shadow.At(150,100)-1)<1e-5f);
    assert(W::Over({120,90,60,255},{0,0,0,200})==W::Color({120,90,60,255}));
    shadow.Build(silhouette,{},0);assert(shadow.At(150,100)==0);
    Memory m;constexpr U32 model=0x8100000,mesh=0x8110000,desc=0x8120000,live=0x14900000,helper=0x8130000,texture=0x14a00000;
    m.words[0x5066f8+0x34]=m.words[0x50446c+0x18]=0;m.words[0x506cb0+0x38]=2;m.words[0x4fda6c+0x50]=0;
    m.words[0x506cb0]=mesh;m.words[0x506cb0+4]=model;m.words[0x506cb0+0x10]=helper;m.words[helper]=30;
    m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[mesh]=desc;m.words[desc+12]=120;
    m.words[mesh+0x124]=0;m.words[mesh+0x1a0]=live;m.words[mesh+0x158]=texture;
    m.words[mesh+0x15c]=512|(256u<<16);m.words[mesh+0x164]=13;
    const std::array<float,12> positions{0,0,0,320,0,0,0,240,0,320,240,0};
    for(unsigned q=0;q<30;++q) {
        auto pos=positions;if(q)for(auto& x:pos)x*=.02f;
        m.Floats(live+q*48,pos);
        m.Floats(live+120*12+q*32,q?std::array<float,8>{.8f,.9f,.9f,.9f,.8f,.8f,.9f,.8f}:
            std::array<float,8>{0,1,320.f/512,1,0,16.f/256,320.f/512,16.f/256});
        std::array<float,16> color;color.fill(1);for(unsigned v=0;v<4;++v)color[4*v+3]=.75f;
        m.Floats(live+120*20+q*64,color);
    }
    const auto atlas=Read(argv[2]);assert(atlas.size()==131144);
    for(unsigned i=0;i<131072;i+=4)m.words[texture+i]=MapFrame::LE32(atlas,i+72);
    PauseMenu::Layout layout;PauseMenu::SetLayout(layout,false);PauseMenu::motion_x=0;
    W::Hook hook;const auto before=m.words;assert(hook.Prepare(m,model,decode));
    assert(m.words==before && hook.foreground.count==29 && W::GetSource());
    // Prewarm actual ROM artwork, then validate first live draw and both UV
    // orientations against the cold compositor byte-for-byte.
    auto source=std::make_shared<W::WarmSource>();source->paper_image=paper;
    source->paper=reference;source->map=hook.map_pixels;
    source->compressed=std::make_shared<const std::vector<U32>>(hook.compressed);
    W::Warmer warmer;
    const auto wait_warm=[&](const PauseMenu::Layout& requested,std::shared_ptr<const W::Outline> cut={}) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        do {
            warmer.Poll(source,requested,cut);
            unsigned index=0;
            if(W::FindWarm(W::WarmRecipe(requested,false).key,*source->compressed,paper,cut,index))return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }while(std::chrono::steady_clock::now()<deadline);
        assert(false);
    };
    wait_warm(layout);
    W::Hook warm;const auto start=std::chrono::steady_clock::now();
    assert(warm.Prepare(m,model,decode) && warm.warm_hits==1 && *warm.composed==*hook.composed);
    std::cout<<"Warm first Prepare (synthetic guest memory): "
        <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms\n";
    m.Floats(live+120*12,std::array<float,8>{320.f/512,1,0,1,320.f/512,16.f/256,0,16.f/256});
    assert(warm.Prepare(m,model,decode) && warm.warm_hits==2);
    auto ready=W::warm_artwork.exchange(std::shared_ptr<const W::WarmPair>{});
    W::Hook cold_mirror;assert(cold_mirror.Prepare(m,model,decode) && cold_mirror.warm_hits==0);
    assert(*warm.composed==*cold_mirror.composed);
    W::warm_artwork.store(ready);
    m.Floats(live+120*12,std::array<float,8>{0,1,320.f/512,1,0,16.f/256,320.f/512,16.f/256});
    // Full source validation rejects edits, including outside the visible crop.
    const auto last=texture+131072-4;m.words[last]^=1;
    W::Hook changed_source;assert(changed_source.Prepare(m,model,decode) && !changed_source.warm_hits);
    m.words[last]^=1;
    auto shifted=positions;for(unsigned i=0;i<4;++i)shifted[3*i]+=1;
    m.Floats(live,shifted);W::Hook native_mismatch;
    assert(native_mismatch.Prepare(m,model,decode) && !native_mismatch.warm_hits);
    m.Floats(live,positions);
    auto changed_layout=layout;changed_layout.world_parchment_blend+=2;
    PauseMenu::SetLayout(changed_layout,false);W::Hook changed_settings;
    assert(changed_settings.Prepare(m,model,decode) && !changed_settings.warm_hits);
    // Change settings while a job is in flight; only the new recipe publishes.
    W::warm_artwork.store(std::shared_ptr<const W::WarmPair>{});
    warmer.Poll(source,layout,{});wait_warm(changed_layout);
    unsigned warm_index=0;
    assert(!W::FindWarm(W::WarmRecipe(layout,false).key,*source->compressed,paper,{},warm_index));
    W::Hook updated;assert(updated.Prepare(m,model,decode) && updated.warm_hits==1);
    assert(*updated.composed==*changed_settings.composed);
    auto cut=std::make_shared<W::Outline>();cut->cut.hole={{20,20},{300,20},{300,220},{20,220}};
    wait_warm(changed_layout,cut);
    assert(!W::FindWarm(W::WarmRecipe(changed_layout,false).key,*source->compressed,paper,{},warm_index));
    auto other_paper=std::make_shared<W::PaperImage>(*paper);
    assert(!W::FindWarm(W::WarmRecipe(changed_layout,false).key,*source->compressed,other_paper,cut,warm_index));
    // User pose and map darkness must produce exactly the live recipe, too.
    auto posed=layout;posed[PauseMenu::Map]={27.5f,-16.25f,.81f};posed.map_darkness=.23f;
    wait_warm(posed);PauseMenu::SetLayout(posed,false);
    auto posed_positions=positions;
    for(unsigned i=0;i<4;++i) {
        const auto xy=PauseMenu::Move(positions[3*i],positions[3*i+1],PauseMenu::Map,posed[PauseMenu::Map]);
        posed_positions[3*i]=xy[0];posed_positions[3*i+1]=xy[1];
    }
    m.Floats(live,posed_positions);
    std::array<float,16> posed_color{};
    for(unsigned i=0;i<4;++i) {
        for(unsigned c=0;c<3;++c)posed_color[4*i+c]=PauseMenu::MapBrightness(posed.map_darkness);
        posed_color[4*i+3]=.75f;
    }
    m.Floats(live+120*20,posed_color);W::Hook posed_hook;
    assert(posed_hook.Prepare(m,model,decode) && posed_hook.warm_hits==1);
    W::warm_artwork.store(std::shared_ptr<const W::WarmPair>{});
    W::Hook posed_cold;assert(posed_cold.Prepare(m,model,decode) && *posed_cold.composed==*posed_hook.composed);
    m.Floats(live,positions);for(unsigned i=0;i<4;++i)for(unsigned c=0;c<3;++c)posed_color[4*i+c]=1;
    m.Floats(live+120*20,posed_color);
    PauseMenu::SetLayout(layout,false);
    assert(W::Pieces(layout).size()==6 && hook.prepared.colors[3]==.75f);
    for(unsigned q=1;q<30;++q) {
        std::array<float,12> p;std::array<float,8> uv;std::array<float,16> c;
        assert(TitleScreen::ReadFloats(m,live+q*48,p));
        assert(TitleScreen::ReadFloats(m,live+120*12+q*32,uv));
        assert(TitleScreen::ReadFloats(m,live+120*20+q*64,c));
        assert(std::equal(p.begin(),p.end(),hook.foreground.positions.begin()+(q-1)*12));
        assert(std::equal(uv.begin(),uv.end(),hook.foreground.uvs.begin()+(q-1)*8));
        assert(std::equal(c.begin(),c.end(),hook.foreground.colors.begin()+(q-1)*16));
    }
    auto first=hook.composed;assert(hook.Prepare(m,model,decode) && hook.composed==first);
    // MQ's reflected UVs retain the same parchment layout and saved outline.
    const auto original_foreground=hook.foreground.positions;
    m.Floats(live+120*12,std::array<float,8>{320.f/512,1,0,1,320.f/512,16.f/256,0,16.f/256});
    const auto reflected_source=m.words;
    assert(hook.Prepare(m,model,decode) && hook.composed!=first && *hook.composed!=*first);
    assert(hook.foreground.positions==original_foreground && m.words==reflected_source);
    const auto reflected=hook.composed;assert(hook.Prepare(m,model,decode) && hook.composed==reflected);
    m.Floats(live+120*12,std::array<float,8>{0,1,320.f/512,1,0,16.f/256,320.f/512,16.f/256});
    assert(hook.Prepare(m,model,decode) && *hook.composed==*first);
    // Restore the baseline pointer used by the cache checks below.
    first=hook.composed;

    // The top fade lives in the backing artwork only. It preserves alpha,
    // foreground labels/icons, and the cached result between frames.
    const auto foreground=hook.foreground.colors;
    layout.world_map_top_fade.enabled=true;
    PauseMenu::SetLayout(layout,false);assert(hook.Prepare(m,model,decode));
    const auto shaded=hook.composed;unsigned changed=0;
    for(unsigned i=0;i<first->size();++i) {
        assert(((*first)[i]&255)==((*shaded)[i]&255));
        for(unsigned c=0;c<3;++c) {
            const auto original=((*first)[i]>>(24-8*c))&255;
            const auto dark=((*shaded)[i]>>(24-8*c))&255;
            assert(dark<=original);changed+=dark<original;
        }
    }
    assert(changed && hook.foreground.colors==foreground);
    assert(hook.Prepare(m,model,decode) && hook.composed==shaded);
    layout.world_map_top_fade.color={1,.2f,.1f};PauseMenu::SetLayout(layout,false);
    assert(hook.Prepare(m,model,decode) && *hook.composed!=*shaded);
    const auto colored=hook.composed;
    for(unsigned i=0;i<first->size();++i)assert(((*first)[i]&255)==((*colored)[i]&255));
    assert(hook.foreground.colors==foreground && hook.Prepare(m,model,decode) && hook.composed==colored);
    layout.world_map_top_fade.height=120;PauseMenu::SetLayout(layout,false);
    assert(hook.Prepare(m,model,decode) && *hook.composed!=*colored);
    layout.world_map_top_fade={};PauseMenu::SetLayout(layout,false);
    assert(hook.Prepare(m,model,decode) && *hook.composed==*first);
    // Restore the original cache pointer used by the following retention checks.
    hook.composed=first;
    // Closing clears the native map controller before gameplay is ready.
    // The retained art uses the same cache and never changes controller memory.
    m.words[0x506cb0+0x38]=0;
    assert(!hook.Prepare(m,model,decode));
    PauseMenu::world_exit_hold=true;
    const auto unpausing=m.words;
    assert(hook.Prepare(m,model,decode) && hook.composed==first && m.words==unpausing);
    PauseMenu::world_exit_hold=false;
    assert(!hook.Prepare(m,model,decode));
    m.words[0x506cb0+0x38]=2;
    const float left=hook.prepared.positions[0];
    for(float motion:{-230.23f,-19.16f,20.f,0.f}) {
        PauseMenu::motion_x=motion;auto moved=positions;
        for(unsigned i=0;i<4;++i)moved[3*i]+=motion/PauseMenu::Scale;
        m.Floats(live,moved);assert(hook.Prepare(m,model,decode));assert(hook.composed==first);
        assert(std::abs(hook.prepared.positions[0]-left-motion/PauseMenu::Scale)<.001f);
    }
    // Existing world-map pose remains independent from both paper stacks.
    const auto sheets=W::Pieces(layout);
    const PauseMenu::Pose pose{31,-12,.7f};auto moved=positions;
    for(unsigned i=0;i<4;++i) {
        const auto p=PauseMenu::Move(moved[3*i],moved[3*i+1],PauseMenu::Map,pose);
        moved[3*i]=p[0];moved[3*i+1]=p[1];
    }
    m.Floats(live,moved);assert(hook.Prepare(m,model,decode));
    assert(std::abs(hook.cached_key[0]-moved[0])<.001f && std::abs(hook.cached_key[2]-moved[3])<.001f);
    const auto after=W::Pieces(layout);assert(after.size()==sheets.size());
    for(unsigned i=0;i<after.size();++i)assert(after[i].left==sheets[i].left && after[i].top==sheets[i].top);
    m.Floats(live,positions);assert(hook.Prepare(m,model,decode));const auto restored=hook.composed;
    // Fade updates geometry only, never regenerates the image.
    std::array<float,16> fade;fade.fill(1);for(unsigned i=0;i<4;++i)fade[4*i+3]=.2f;
    m.Floats(live+120*20,fade);assert(hook.Prepare(m,model,decode) && hook.composed==restored);
    assert(hook.prepared.colors[3]==.2f);
    // The clock colors both backing papers and artwork exactly once, without
    // tinting the separately replayed native labels/icons or changing opacity.
    const auto plain_foreground=hook.foreground.colors;
    m.words[0x4596a0]=0x587958;m.words[0x4594e4]=0xe1d600bc;
    layout.world_map_time_tint=true;PauseMenu::SetLayout(layout,false);
    const auto artwork_builds=hook.lighting.rebuilds;
    for(U32 time:{0u,0xb555u}) {
        m.words[0x587964]=time;
        const auto tint=MapTimeTint::At(time);
        auto tinted=fade;
        // Native CopyWorldMapColors supplies the map tint before compositing.
        for(unsigned v=0;v<4;++v)for(unsigned c=0;c<3;++c)tinted[4*v+c]*=tint[c];
        m.Floats(live+120*20,tinted);
        assert(hook.Prepare(m,model,decode) && hook.composed!=restored);
        assert(hook.lighting.rebuilds==artwork_builds); // Clock must not rebuild the outline/papers/shadow.
        for(unsigned i=0;i<restored->size();++i) {
            assert(((*hook.composed)[i]&255)==((*restored)[i]&255));
            for(unsigned c=0;c<3;++c) {
                const auto actual=((*hook.composed)[i]>>(24-8*c))&255;
                const float expected=float(((*restored)[i]>>(24-8*c))&255)*tint[c];
                assert(std::abs(float(actual)-expected)<=1);
            }
        }
        assert(hook.foreground.colors==plain_foreground && hook.prepared.colors[3]==.2f);
        const auto cached=hook.composed;
        assert(hook.Prepare(m,model,decode) && hook.composed==cached);
    }
    layout.world_map_time_tint=false;PauseMenu::SetLayout(layout,false);
    m.Floats(live+120*20,fade);
    assert(hook.Prepare(m,model,decode) && *hook.composed==*restored);
    // Representative edited outline, for review of compositing with the real atlas.
    outline.cut.hole={{8,20},{84,28},{155,14},{235,8},{310,42},{305,85},{265,104},
        {245,149},{211,175},{141,192},{91,189},{84,158},{111,139},{94,103},{51,82},{13,76}};
    outline.feather=10;assert(W::Valid(outline));
    W::outline.store(std::shared_ptr<const W::Outline>(std::make_shared<W::Outline>(outline)));
    assert(hook.Prepare(m,model,decode) && hook.composed!=first);const auto masked=hook.composed;
    layout.world_map_shadow_opacity=0;PauseMenu::SetLayout(layout,false);
    assert(hook.Prepare(m,model,decode));const auto no_shadow=hook.composed;
    unsigned darker=0;
    for(unsigned i=0;i<masked->size();++i)for(unsigned c=0;c<3;++c) {
        const auto dark=((*masked)[i]>>(24-8*c))&255,plain=((*no_shadow)[i]>>(24-8*c))&255;
        assert(dark<=plain);if(dark<plain)++darker;
    }
    assert(darker>0);
    layout.world_map_shadow_opacity=.35f;layout.world_map_shadow_width=0;PauseMenu::SetLayout(layout,false);
    assert(hook.Prepare(m,model,decode) && *hook.composed==*no_shadow);
    layout.world_map_shadow_width=8;PauseMenu::SetLayout(layout,false);
    assert(hook.Prepare(m,model,decode) && *hook.composed==*masked);
    std::ofstream out(argv[3],std::ios::binary);out<<"P6\n"<<P::Width<<' '<<P::Height<<"\n255\n";
    for(unsigned y=0;y<P::Height;++y)for(unsigned x=0;x<P::Width;++x) {
        const auto pixel=(*masked)[P::TiledOffset(x,P::Height-1-y)];const float a=(pixel&255)/255.f;
        for(unsigned c=0;c<3;++c)out.put(char(std::lround(((pixel>>(24-8*c))&255)*a+35*(1-a))));
    }
    layout.world_parchments[0].pose.x+=10;PauseMenu::SetLayout(layout,false);
    assert(hook.Prepare(m,model,decode) && hook.composed!=masked);
    for(auto& s:layout.world_parchments)s.enabled=false;
    layout.world_map_outline=false;layout.world_map_shadow_opacity=0;PauseMenu::SetLayout(layout,false);assert(!hook.Prepare(m,model,decode));
    layout.world_map_outline=true;PauseMenu::SetLayout(layout,false);assert(hook.Prepare(m,model,decode));
    // Unknown material, a dungeon, and unrelated models retain their native rendering.
    m.words[mesh+0x164]=12;assert(!hook.Prepare(m,model,decode));m.words[mesh+0x164]=13;
    m.words[0x4fda6c+0x50]=1;assert(!hook.Prepare(m,model,decode));m.words[0x4fda6c+0x50]=0;
    assert(!hook.Prepare(m,model+4,decode));
    std::cout<<"World map: six parchments, rounded outline, feathered concave mask, adjustable edge shadow, native labels/markers unchanged, motion/fade cache, toggles and safe fallback passed\n";
}
