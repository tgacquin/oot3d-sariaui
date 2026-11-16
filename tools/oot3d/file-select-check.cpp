// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include "oot3d/pause_prompt_assets.h"
#include "oot3d/screen_model_hooks.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
namespace F=OOT3D::FileSelect;
namespace P=OOT3D::PausePrompts;
using OOT3D::TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void Allocate(U32 a,U32 bytes) {for(U32 off=0;off<bytes;off+=4)words[a+off]=0;}
    void Float(U32 at,float value) {words[at]=std::bit_cast<U32>(value);}
    template<class T> void Floats(U32 at,const T& v) {for(unsigned i=0;i<v.size();++i)words[at+i*4]=std::bit_cast<U32>(v[i]);}
};
std::vector<std::uint8_t> Read(const std::string& path) {
    std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char** argv) {
    assert(argc==3);Memory m;
    const auto code=Read(argv[1]);
    for(unsigned i=0;i+3<code.size();i+=4)m.words[0x100000+i]=OOT3D::MapFrame::LE32(code,i);
    assert(F::CodeMatches(m));
    {
        F::FileOperationPromptTransition fade;
        m.words[F::Owner+0x10]=37;m.words[F::Owner+0x2c]=0;
        assert(!F::DeletingFile(m));fade.Frame(false);assert(fade.opacity==0);
        for(unsigned state:{39u,42u,44u}) {
            m.words[F::Owner+0x10]=state;assert(F::DeletingFile(m));
        }
        const auto native=m.words;
        for(unsigned frame=1;frame<=6;++frame) {
            fade.Frame(F::DeletingFile(m));assert(std::abs(fade.opacity-frame/6.f)<.00001f);
        }
        assert(fade.opacity==1 && fade.clock_us>0 && m.words==native);
        m.words[F::Owner+0x10]=2;
        for(unsigned frame=1;frame<=6;++frame) {
            fade.Frame(F::DeletingFile(m));assert(std::abs(fade.opacity-(1-frame/6.f))<.00001f);
        }
        assert(fade.opacity==0 && fade.clock_us==0);
        m.words[F::Owner+0x10]=42;m.words[F::Owner+0x2c]=1;
        assert(!F::DeletingFile(m)); // Cancellation never becomes a loading operation.
        fade.Frame(true);fade.Frame(false);assert(fade.opacity==0); // Fast completion.
        for(unsigned state=24;state<=28;++state) {
            m.words[F::Owner+0x10]=state;assert(F::FileOperationBusy(m));
            fade.Frame(F::FileOperationBusy(m));
        }
        m.words[F::Owner+0x10]=32;assert(!F::FileOperationBusy(m));
        for(unsigned i=0;i<6;++i)fade.Frame(F::FileOperationBusy(m));
        assert(fade.opacity==0);
        m.words[F::Owner+0x10]=0;m.words[F::Owner+0x2c]=0;
    }
    // Native validation has already accepted the name. Skip only state 6;
    // save/cancel handling and every other register remain game-owned.
    {
        std::array<U32,16> r{};for(unsigned i=0;i<r.size();++i)r[i]=0x12340000+i;
        m.words[F::NameOwner+4]=5;m.words[F::NameOwner+0x24]=4;
        const auto before=m.words;const auto original_r=r;
        F::AcceptName(m,r);assert(r[0]==10 && m.words==before);
        for(unsigned i=1;i<r.size();++i)assert(r[i]==original_r[i]);
        for(auto length:{0u,9u}) {
            m.words[F::NameOwner+0x24]=length;F::AcceptName(m,r);assert(r[0]==6);
        }
        m.words[F::NameOwner+4]=4;m.words[F::NameOwner+0x24]=4;
        F::AcceptName(m,r);assert(r[0]==6);
        m.words.erase(F::NameOwner+4);m.words.erase(F::NameOwner+0x24);
    }
    {
        // Presentation is driven by native save state, never by a guessed delay.
        F::NameSaveFlow flow;std::array<U32,16> r{};r[6]=F::NameOwner;r[15]=0x427cfc;
        m.words[F::NameOwner+4]=5;m.words[F::NameOwner+8]=2;
        m.words[F::NameOwner+0x24]=0;
        assert(m.words[0x43047c]==0x588958);
        m.words[m.words[0x43047c]+0x4dc]=1;
        m.words[OOT3D::MasterQuest::System+0x4dc]=0; // Wrong context must not select slot 1.
        flow.NameDispatch(m,r,true);assert(!flow.active);
        m.words[F::NameOwner+0x24]=4;flow.NameDispatch(m,r,true);assert(flow.active);
        float previous=0;
        for(unsigned i=0;i<10;++i) {flow.Frame();assert(flow.opacity>=previous);previous=flow.opacity;}
        assert(flow.opacity==1);
        m.words[F::NameOwner+4]=4;m.words[F::NameOwner+8]=5;
        flow.NameDispatch(m,r,true);assert(flow.active && r[15]==0x427cfc);
        m.words[F::NameOwner+8]=6;flow.NameDispatch(m,r,true);
        assert(!flow.active && flow.returning && r[15]==0x42863c);
        m.words[F::Owner+0x10]=3;m.words[m.words[0x42fdfc]+4]=0;
        assert(!flow.FileDispatch(m,r) && !flow.returning); // Missing slot: normal list fallback.
        for(unsigned i=0;i<10;++i)flow.Frame();assert(flow.opacity==0);
        m.words.erase(F::NameOwner+4);m.words.erase(F::NameOwner+8);
        m.words.erase(F::NameOwner+0x24);
    }
    assert(std::abs(OOT3D::LoadingPrompt::Opacity(0)-1)<.0001f);
    assert(std::abs(OOT3D::LoadingPrompt::Opacity(1000000)-.45f)<.0001f);
    assert(std::abs(OOT3D::LoadingPrompt::Opacity(2000000)-1)<.0001f);
    {
        // Pulse all native glyph/shadow models without fading the prompt plate,
        // changing native model alpha, or leaking into the normal file prompt.
        Memory pulse;const U32 controller=0x18600000,model=0x18610000,alpha=0x18500000;
        pulse.words[0x55b548+6*4]=controller;pulse.words[controller+0x40]=2;
        for(unsigned i=0;i<9;++i)pulse.words[controller+0x10+4*i]=model+0x200*i;
        const auto entry=std::atomic_load(&OOT3D::LoadingPrompt::entry);
        std::atomic_store(&OOT3D::LoadingPrompt::entry,
            std::make_shared<const OOT3D::LoadingPrompt::Entry>(OOT3D::LoadingPrompt::Entry{0,0}));
        OOT3D::MasterQuest::loading_label=true;OOT3D::MasterQuest::cursor_us=1000000;
        auto h=std::make_unique<F::Hook>();h->stage=F::Hook::Stage::Native;h->quest_frame=true;
        for(unsigned i=0;i<9;++i) {
            pulse.Float(alpha,.8f);h->Material(pulse,alpha,model+0x200*i);
            assert(std::abs(std::bit_cast<float>(pulse.words[alpha])-.36f)<.0001f);
        }
        pulse.Float(alpha,.8f);h->Material(pulse,alpha,model+0x4000);
        assert(std::bit_cast<float>(pulse.words[alpha])==.8f);
        OOT3D::MasterQuest::loading_label=false;h->Material(pulse,alpha,model);
        assert(std::bit_cast<float>(pulse.words[alpha])==.8f);
        OOT3D::MasterQuest::cursor_us=0;std::atomic_store(&OOT3D::LoadingPrompt::entry,entry);
    }
    // Seed backing follows actual glyph bounds, including wrapped seeds, and
    // contains no crossed quads or opacity compounded by overlapping fills.
    F::SeedShadow seed_shadow;
    OOT3D::FileStatusText::Geometry seed_text;
    seed_text.Add(12,200,140,15,0,0,1,1);
    const auto seed_back=F::SeedBackdrop(seed_text,seed_shadow);
    assert(seed_back.count==324);
    assert(std::abs(F::Bounds(seed_back,0,seed_back.count-1).X()-82)<.001f);
    for(unsigned q=0;q<seed_back.count;++q) {
        for(auto triangle:{std::array{0u,1u,2u},std::array{2u,1u,3u}}) {
            const auto at=q*12;
            const auto a=at+triangle[0]*3,b=at+triangle[1]*3,c=at+triangle[2]*3;
            const float cross=(seed_back.positions[b]-seed_back.positions[a])*(seed_back.positions[c+1]-seed_back.positions[a+1])-
                (seed_back.positions[b+1]-seed_back.positions[a+1])*(seed_back.positions[c]-seed_back.positions[a]);
            assert(cross>=-.001f);
        }
    }
    seed_shadow.x=30;seed_shadow.y=-8;seed_shadow.width=2;seed_shadow.height=1.5f;
    const auto extended_seed=F::SeedBackdrop(seed_text,seed_shadow);
    const auto extended_bounds=F::Bounds(extended_seed,0,extended_seed.count-1);
    assert(std::abs(extended_bounds.X()-112)<.001f && std::abs(extended_bounds.Y()-199.5f)<.001f);
    seed_shadow.enabled=false;assert(F::SeedBackdrop(seed_text,seed_shadow).count==0);
    assert(F::SeedBackdrop({},{}).count==0);
    // Verify the real prompt-cap rectangles, including mirrored caps and the
    // lower confirmation box. Only the interior U edge changes; saved layout,
    // alpha, V slicing, and unrelated sprite coordinates stay untouched.
    for(bool naming:{false,true}) {
        const U32 sizes=naming?0x507c44:0x505b78,origins=naming?0x507e34:0x506138;
        const std::vector<unsigned> quads=naming?
            std::vector<unsigned>{1,3,4,6,16,18,19,21,22,24,48,50,51,53,54,56}:
            std::vector<unsigned>{0,2,3,5,162,164,165,167,168,170};
        const float edge=naming?360:288;
        for(unsigned q:quads) {
            const float u=std::bit_cast<float>(*m.Read(origins+q*8));
            const float w=std::bit_cast<float>(*m.Read(sizes+q*8));
            assert(std::abs(w)==24 && std::max(u,u+w)==edge);
            F::Geometry cap;cap.Add(12,4,24,20,u/512,.2f,(u+w)/512,.3f);
            const auto before=cap;
            F::PromptCapUVs(cap,0,edge);
            assert(cap.positions==before.positions && cap.colors==before.colors);
            for(unsigned v=0;v<4;++v) {
                const auto original=before.uvs[v*2];
                assert(cap.uvs[v*2]==(original==edge/512?(edge-.5f)/512:original));
                assert(cap.uvs[v*2+1]==before.uvs[v*2+1]);
            }
            const auto fixed=cap;F::PromptCapUVs(cap,0,edge);assert(fixed.uvs==cap.uvs);
        }
    }
    // Corner shading is symmetric, clears toward the center, and uses the
    // actual viewport edges in both native and widescreen layouts.
    for(bool wide:{false,true}) {
        const auto vg=F::CornerVignette({},wide);
        assert(vg.count==576);
        const float width=wide?240*OOT3D::Widescreen::Aspect:400;
        assert(std::abs(F::Bounds(vg,0,575).left-(400-width)/2)<.001f);
        assert(std::abs(F::Bounds(vg,0,575).right-(400+width)/2)<.001f);
        for(unsigned corner=0;corner<4;++corner) {
            const unsigned x=corner&1?11:0,y=corner&2?11:0;
            assert(std::abs(vg.colors[(corner*144+y*12+x)*16+corner*4+3]-.55f)<.001f);
        }
        for(unsigned q=0;q<vg.count;++q)for(unsigned v=0;v<4;++v) {
            const float a=vg.colors[q*16+v*4+3];
            assert(std::isfinite(a) && a>=0 && a<=.55f);
        }
    }
    F::Vignette no_vignette;no_vignette.enabled=false;
    assert(F::CornerVignette(no_vignette,true).count==0);
    no_vignette.enabled=true;no_vignette.strength=0;
    assert(F::CornerVignette(no_vignette,true).count==0);
    for(unsigned i=0;i<F::Sites.size();++i) {
        const auto s=F::Sites[i];assert(F::Instruction(s.pc,s.instruction)==(0xef000000|F::FirstSVC+i));
        assert(F::Instruction(s.pc,s.instruction^1)==(s.instruction^1));
        m.words[s.pc]^=1;assert(!F::CodeMatches(m));m.words[s.pc]^=1;
    }
    // Verify the native table addresses and Back classification against the ROM.
    for(unsigned q:{118u,119u,120u,173u,174u}) {
        assert(F::BackQuad(q));
        assert(std::bit_cast<float>(*m.Read(0x504ff8+q*8))<=50);
        assert(std::bit_cast<float>(*m.Read(0x504ff8+q*8+4))>=202);
    }
    assert(*m.Read(0x506138+120*8)==0); // BACK label at raw atlas (0,112).
    assert(std::bit_cast<float>(*m.Read(0x506138+120*8+4))==112);
    for(unsigned q:{0u,6u,40u,77u,114u,122u,125u,129u,133u,183u})assert(!F::BackQuad(q));
    constexpr U32 model=0x18000000,mesh=0x18010000,helper=0x18020000,pos=0x18030000,uv=0x18040000,
        color=0x18050000,desc=0x18060000,material=0x18070000;
    m.Allocate(model,0x1e4);m.Allocate(mesh,0x1b8);m.Allocate(desc,0x120);
    m.words[F::Owner+8]=model;m.words[F::Owner+4]=mesh;m.words[F::Owner+0x30]=helper;
    m.words[F::Owner+0x10]=2;m.words[model]=0x4ebd60;m.words[model+4]=mesh;
    m.words[model+0x1dc]=material;m.words[mesh]=desc;
    m.words[helper]=184;m.words[helper+0x10]=pos;m.words[helper+0x14]=uv;m.words[helper+0x18]=color;
    const std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};m.Floats(model+0xc,identity);
    for(unsigned i=0;i<24;++i)m.words[mesh+0x13c+i*4]=100+i;
    F::Geometry original;
    for(unsigned q=0;q<184;++q) {
        original.Add(20+q%10,50+q%5,16,24,0,1,1,0);
        for(unsigned v=3;v<16;v+=4)original.colors[q*16+v]=.65f;
    }
    m.Floats(pos,original.positions);m.Floats(uv,original.uvs);m.Floats(color,original.colors);
    const auto before=m.words;
    const auto top=F::Atlas(m,false),bottom=F::Atlas(m,true);
    assert(top.count==184 && bottom.count==184 && before==m.words);
    for(unsigned q=0;q<184;++q) {
        const auto& visible=F::BackQuad(q)?bottom:top;
        const auto& hidden=F::BackQuad(q)?top:bottom;
        for(unsigned v=0;v<12;++v) {
            assert(visible.positions[q*12+v]==original.positions[q*12+v]);
            assert(hidden.positions[q*12+v]==0);
        }
    }
    assert(top.colors==original.colors && top.uvs==original.uvs);
    // All four edges follow the transformed card and its opacity. Parked or
    // collapsed stones produce no shadows over unrelated UI.
    F::Layout shadow_layout;shadow_layout.stones={13,-7,1.6f};
    const auto shadow_atlas=F::Atlas(m,false,shadow_layout);
    const auto shadow_geo=F::StoneShadows(shadow_atlas);assert(shadow_geo.count==72);
    for(unsigned stone=0;stone<3;++stone) {
        const auto box=F::Bounds(shadow_atlas,6+stone*37,8+stone*37);
        const auto* v=shadow_geo.positions.data()+stone*9*12;
        assert(v[3]==box.left && v[12]==box.right);
        assert(std::abs(v[2*12+7]-box.top)<.0001f && std::abs(v[3*12+1]-box.bottom)<.0001f);
        assert(shadow_geo.colors[stone*9*16+3]==0);
        const float inner_alpha=shadow_geo.colors[stone*9*16+7];
        assert(std::abs(inner_alpha-.65f*.55f)<.0001f);
        const auto* fill=v+8*12;
        assert(fill[0]==box.left && fill[1]==box.top);
        assert(std::abs(fill[9]-box.right)<.0001f && std::abs(fill[10]-box.bottom)<.0001f);
        for(unsigned corner=0;corner<4;++corner)
            assert(shadow_geo.colors[(stone*9+8)*16+corner*4+3]==inner_alpha);
    }
    assert(!F::StoneShadows(F::Geometry{}).count);
    assert(!F::StoneShadows(shadow_atlas,0).count);
    const auto dim=F::StoneShadows(shadow_atlas,.275f);
    for(unsigned i=3;i<dim.count*16;i+=4)
        assert(std::abs(dim.colors[i]-shadow_geo.colors[i]*.5f)<.0001f);
    // Action cards and the two quest stones share the same opacity, including
    // their filled corners; the original quest shadow is not double drawn.
    const auto cards=F::QuestStones(0);
    for(unsigned q:{0u,2u})for(unsigned v=0;v<4;++v)assert(cards.colors[q*16+v*4+3]==0);
    const auto quest_shadow=F::QuestShadows(cards,.275f);assert(quest_shadow.count==18);
    assert(!F::QuestShadows(cards,0).count);
    for(unsigned stone=0;stone<2;++stone) {
        const auto card=F::Bounds(cards,stone*2+1,stone*2+1);
        const auto* center=quest_shadow.positions.data()+(stone*9+8)*12;
        assert(std::abs(center[0]+40-card.left)<.0001f);
        assert(quest_shadow.colors[(stone*9+8)*16+3]==.275f);
    }
    // Every editable part moves all its atlas details with a uniform scale.
    const auto bounds=F::PartBounds(original);
    for(unsigned part=0;part<F::Count;++part) {
        F::Layout edit;edit.parts[part]={17,-9,1.5f};
        const auto changed=F::Atlas(m,false,edit);
        for(unsigned q=0;q<184;++q)if(!F::BackQuad(q))for(unsigned v=0;v<4;++v) {
            const float x=original.positions[q*12+v*3],y=original.positions[q*12+v*3+1];
            const bool moved=F::QuadPart(q)==part;
            assert(changed.positions[q*12+v*3]==(moved?17+bounds[part].X()+1.5f*(x-bounds[part].X()):x));
            assert(changed.positions[q*12+v*3+1]==(moved?-9+bounds[part].Y()+1.5f*(y-bounds[part].Y()):y));
        }
    }
    // Confirmation faces/text retain complete geometry across the old 320px
    // edge, with independent poses, the cursor and inverse touch mapping.
    auto confirm=original;
    for(unsigned q=134;q<=147;++q)for(unsigned v=0;v<4;++v)confirm.positions[q*12+v*3]+=290;
    m.Floats(pos,confirm.positions);m.words[F::Owner+0x10]=36;
    const auto uncut=F::Atlas(m,false);
    for(unsigned q=162;q<=170;++q)for(unsigned v=0;v<12;++v)assert(uncut.positions[q*12+v]==0);
    for(unsigned q=0;q<6;++q)for(unsigned v=0;v<12;++v)assert(uncut.positions[q*12+v]==confirm.positions[q*12+v]);
    for(unsigned q=134;q<=147;++q)for(unsigned v=0;v<4;++v) {
        assert(uncut.positions[q*12+v*3]==confirm.positions[q*12+v*3]);
        assert(uncut.uvs[q*8+v*2]==confirm.uvs[q*8+v*2]);
    }
    for(unsigned part:{F::DeleteCancel,F::DeleteErase}) {
        for(float offset:{-500.f,500.f})for(float scale:{.1f,1.f,4.f})for(float x:{-400.f,0.f,400.f}) {
            std::array<F::Rect,F::Count> bs{};const float left=part==F::DeleteCancel?34.f:162.f;
            bs[part]={left+offset,116,left+124+offset,176,true};
            F::Layout layout;layout.parts[part]={x,0,scale};const auto t=F::PartTransform(layout,part,bs);
            const float l=t.x+t.ax+(bs[part].left-t.ax)*t.sx,r=t.x+t.ax+(bs[part].right-t.ax)*t.sx;
            assert(offset>0?l>=423.999f:r<=-23.999f); // Entire enlarged/shifted stone clears screen.
        }
        F::touch_bounds={};F::touch_layout={};
        const float left=part==F::DeleteCancel?34.f:162.f;
        F::touch_bounds[part]={left,116,left+124,176};F::touch_layout.parts[part]={-15,-25,.8f};
        const auto t=F::PartTransform(F::touch_layout,part,F::touch_bounds);
        const auto point=F::MapPointer((t.x+left+62)/400,(t.y+146)/240,400);
        assert(point && std::abs((*point)[0]-(left+62)/320)<.0001f && std::abs((*point)[1]-146.f/240)<.0001f);
        constexpr U32 cursor=0x18080000,cm=0x18090000;
        m.words[F::Owner+0x34]=cursor;m.words[cursor+4]=cm;m.words[F::Owner+0x10]=37;
        m.words[F::Owner+0x24]=part==F::DeleteCancel?0:1;
        F::Hook h;h.stage=F::Hook::Stage::Native;h.frame_layout=F::touch_layout;h.bounds=F::touch_bounds;
        const auto ct=h.ModelTransform(m,cm);assert(ct.x==t.x && ct.y==t.y && ct.sx==t.sx);
    }
    F::touch_bounds={};F::touch_layout={};m.words[F::Owner+0x10]=2;m.words[F::Owner+0x34]=0;
    m.Floats(pos,original.positions);
    auto pressed=original;
    for(unsigned q:{138u,139u,140u,145u,146u,147u})
        for(unsigned v=0;v<4;++v)pressed.positions[q*12+v*3]+=500;
    m.Floats(pos,pressed.positions);m.words[F::Owner+0x10]=37;
    F::Layout extreme;extreme.parts[F::DeleteCancel]={-400,0,4};extreme.parts[F::DeleteErase]={-400,0,4};
    const auto clean=F::Atlas(m,false,extreme);
    for(unsigned q:{138u,139u,140u,145u,146u,147u})for(unsigned v=0;v<12;++v)assert(clean.positions[q*12+v]==0);
    m.words[F::Owner+0x10]=2;m.Floats(pos,original.positions);
    // A parked normal frame can leave just its left cap within the old
    // per-slice distance threshold. Reject the entire unused face together.
    {
        auto raw=original;
        for(unsigned first:{134u,141u})for(unsigned q=first;q<first+7;++q) {
            const float x=std::bit_cast<float>(*m.Read(0x504ff8+q*8))+(q<first+3?400:0);
            const float y=std::bit_cast<float>(*m.Read(0x504ff8+q*8+4));
            const float w=std::bit_cast<float>(*m.Read(0x505b78+q*8));
            const float h=std::bit_cast<float>(*m.Read(0x505b78+q*8+4));
            for(unsigned v=0;v<4;++v) {
                raw.positions[q*12+v*3]=x+(v&1?w:0);
                raw.positions[q*12+v*3+1]=y+(v&2?h:0);
            }
        }
        m.words[F::Owner+0x10]=37;m.Floats(pos,raw.positions);
        const auto selected=m.words[F::Owner+0x14];m.words[F::Owner+0x14]=1;
        const auto out=F::Atlas(m,false);
        for(unsigned first:{134u,141u}) {
            for(unsigned q=first;q<first+3;++q)assert(!F::Bounds(out,q,q).Valid());
            for(unsigned q=first+3;q<first+7;++q)assert(F::Bounds(out,q,q).Valid());
        }
        for(unsigned q=148;q<=161;++q)assert(!F::Bounds(out,q,q).Valid());
        for(unsigned q:{6u,80u,122u,126u,130u,171u})assert(!F::Bounds(out,q,q).Valid());
        assert(F::Bounds(out,43,45).Valid());
        m.words[F::Owner+0x14]=selected;
        m.words[F::Owner+0x10]=2;m.Floats(pos,original.positions);
    }
    // Both pages move on the acceptance frame and every subsequent 60 Hz frame,
    // even when the native atlas is still parked or updates on alternate frames.
    const auto exchange_memory=m.words;
    for(auto mode:{F::Exchange::Erase,F::Exchange::Copy,F::Exchange::Start,
                   F::Exchange::BackErase,F::Exchange::BackCopy}) {
        const bool back=mode==F::Exchange::BackErase || mode==F::Exchange::BackCopy;
        const bool copy=mode==F::Exchange::Copy || mode==F::Exchange::BackCopy;
        const bool erase=mode==F::Exchange::Erase || mode==F::Exchange::BackErase;
        m.words[F::Owner+0x10]=mode==F::Exchange::Erase?34:mode==F::Exchange::Copy?12:
            mode==F::Exchange::Start?10:mode==F::Exchange::BackErase?38:15;
        m.words[F::Owner+0x14]=1;F::BeginExchange(mode);
        float previous_action=back?-500.f:0.f,previous_other=back?0.f:500.f;
        for(unsigned frame=1;frame<=10;++frame) {
            auto native=original;
            for(unsigned q=0;q<F::QuadCount;++q) {
                const auto raw=F::Bounds(native,q,q);
                const float x=std::bit_cast<float>(*m.Read(0x504ff8+q*8));
                const float y=std::bit_cast<float>(*m.Read(0x504ff8+q*8+4));
                for(unsigned v=0;v<4;++v) {
                    native.positions[q*12+v*3]+=x-raw.left+400-40*(frame/2);
                    native.positions[q*12+v*3+1]+=y-raw.top;
                }
            }
            m.Floats(pos,native.positions);F::exchange_frames=frame;
            std::array<F::Rect,F::Count> bs{};
            const auto animated=F::Atlas(m,false,F::Layout{},&bs);
            const float action=F::Bounds(animated,122,122).left-98;
            assert(back?action>previous_action:action<previous_action);
            assert(frame!=1 || action<0);
            previous_action=action;
            if(erase) {
                const float other=F::Bounds(animated,134,134).left-34;
                assert(back?other>previous_other:other<previous_other);
                previous_other=other;
                if(frame==10)assert(std::abs(other-(back?500:0))<.001f);
            }
            if(copy) {
                const float other=F::Bounds(animated,6,8).left-16;
                assert(back?other>previous_other:other<previous_other);
                previous_other=other;
                for(unsigned i=0;i<3;++i) {
                    const auto raw=F::Bounds(native,6+37*i,8+37*i);
                    const auto actual=F::Bounds(animated,6+37*i,8+37*i);
                    const auto t=F::PartTransform(F::Layout{},i,bs);
                    // Native names/dates use the identical translation as their card.
                    assert(std::abs(actual.left-(raw.left+t.x-40))<.001f);
                    if(frame==10)assert(std::abs(actual.left-(i==1?(back?16:-484):(back?516:16)))<.001f);
                }
            }
            if(frame==10)assert(std::abs(action-(back?0:-500))<.001f);
        }
        for(unsigned part:{F::Start,F::Copy,F::Erase}) {
            const float left=part==F::Start?98:part==F::Copy?70:166;
            for(float offset:{-500.f,500.f})for(float scale:{.1f,4.f})for(float x:{-400.f,400.f}) {
                std::array<F::Rect,F::Count> bs{};bs[part]={left+offset,106,left+124+offset,166,true};
                F::Layout layout;layout.parts[part]={x,0,scale};const auto t=F::PartTransform(layout,part,bs);
                const float l=t.x+t.ax+(bs[part].left-t.ax)*t.sx,r=t.x+t.ax+(bs[part].right-t.ax)*t.sx;
                assert(offset>0?l>=423.999f:r<=-23.999f);
            }
        }
    }
    F::exchange=F::Exchange::None;F::exchange_frames=0;m.words=exchange_memory;
    std::cout<<"Simultaneous action-page exchanges: immediate motion, ten distinct frames, return directions, shared lettering and complete offscreen travel passed\n";
    // Group scaling includes distances; spacing affects centers but not art.
    auto group_bounds=bounds;
    for(unsigned i=0;i<3;++i)group_bounds[i]={16,40.f+60*i,304,84.f+60*i};
    F::Layout grouped;grouped.stones={13,-7,.5f};grouped.stone_spacing=1.5f;
    for(unsigned i=0;i<3;++i) {
        const auto t=F::PartTransform(grouped,i,group_bounds);
        assert(t.sx==.5f && t.sy==.5f);
        assert(t.x+group_bounds[i].X()==213);
        assert(t.y+group_bounds[i].Y()==115+45*(int(i)-1));
        F::touch_bounds=group_bounds;F::touch_layout=grouped;
        const auto point=F::MapPointer((t.x+group_bounds[i].X())/400,
            (t.y+group_bounds[i].Y())/240,400);
        assert(point && std::abs((*point)[0]-.5f)<.0001f);
        assert(std::abs((*point)[1]-group_bounds[i].Y()/240)<.0001f);
    }
    F::touch_bounds={};F::touch_layout={};
    // Parked cards never come back into view when scaled down or moved left.
    auto parked=original;
    for(unsigned q:{122u,123u,124u,125u,175u,176u,177u})
        for(unsigned v=0;v<4;++v)parked.positions[q*12+v*3]+=400;
    m.Floats(pos,parked.positions);
    F::Layout shrunk;shrunk.parts[F::Start]={-300,0,.1f};
    const auto hidden_cards=F::Atlas(m,false,shrunk);
    for(unsigned q:{122u,123u,124u,125u,175u,176u,177u})
        for(unsigned v=0;v<12;++v)assert(hidden_cards.positions[q*12+v]==0);
    m.Floats(pos,original.positions);
    // A stone crossing the old x=320 edge retains every atlas quad/UV, just
    // like its native date/name. Only the final top viewport may clip it.
    auto entering=original;
    for(unsigned q=6;q<43;++q)for(unsigned v=0;v<4;++v)entering.positions[q*12+v*3]+=290;
    m.Floats(pos,entering.positions);
    const auto full_stone=F::Atlas(m,false);
    for(unsigned q=6;q<43;++q)for(unsigned v=0;v<4;++v) {
        assert(full_stone.positions[q*12+v*3]==entering.positions[q*12+v*3]);
        assert(full_stone.uvs[q*8+v*2]==entering.uvs[q*8+v*2]);
    }
    // A still-moving stone entirely outside the old canvas also remains
    // drawable after group repositioning/scaling; a parked stone stays hidden.
    for(unsigned q=6;q<43;++q)for(unsigned v=0;v<4;++v)entering.positions[q*12+v*3]+=100;
    m.Floats(pos,entering.positions);
    m.words[0x55b610]=std::bit_cast<U32>(-80.f);m.words[0x55b61c]=0;
    std::array<F::Rect,F::Count> entrance_bounds;
    F::Layout entrance_layout;entrance_layout.stones={-150,0,.5f};
    const auto complete=F::Atlas(m,false,entrance_layout,&entrance_bounds);
    const auto shared=F::PartTransform(entrance_layout,0,entrance_bounds);
    assert(shared.sx==.5f && entrance_bounds[0].moving);
    for(unsigned q=6;q<43;++q)for(unsigned v=0;v<4;++v)
        assert(complete.positions[q*12+v*3]==shared.x-40+shared.ax+
            (entering.positions[q*12+v*3]-shared.ax)*shared.sx);
    // The animated middle stone must never move the group's scaling pivot.
    const auto stable=shared;entrance_bounds[1].left+=240;entrance_bounds[1].right+=240;
    assert(F::PartTransform(entrance_layout,0,entrance_bounds).x==stable.x);
    m.words[0x55b610]=0;
    const auto parked_stone=F::Atlas(m,false,entrance_layout);
    for(unsigned v=0;v<12;++v)assert(parked_stone.positions[6*12+v]==0);
    m.Floats(pos,original.positions);
    for(float strength:{0.f,1.f,2.f,4.f}) {
        assert(F::EntranceX(16,strength)==16 && F::EntranceX(336,strength)==336);
        assert(F::EntranceX(576,strength)==576);
        float last=336,previous_step=INFINITY;
        for(unsigned n=1;n<=8;++n) {
            const float x=F::EntranceX(336-40*n,strength),step=last-x;
            assert(step>0 && step<=previous_step+.001f);
            if(strength==0)assert(step==40);
            previous_step=step;last=x;
        }
        for(const auto endpoints:{std::array<float,2>{16,-384},
                                  std::array<float,2>{48,56},
                                  std::array<float,2>{100,56},
                                  std::array<float,2>{152,56}}) {
            const auto [start,end]=endpoints;
            float previous=start,previous_step=INFINITY;
            assert(F::EasePosition(start,start,end,strength)==start);
            for(unsigned n=1;n<=10;++n) {
                const float value=F::EasePosition(start+(end-start)*n/10,start,end,strength);
                const float step=std::abs(value-previous);
                assert(step>0 && step<=previous_step+.001f);
                previous=value;previous_step=step;
            }
            assert(previous==end);
        }
    }
    // File stone movement shares its eased offsets with native text/cursors
    // and the shadow geometry, including the selected stone's vertical slide.
    m.words[F::Owner+0x10]=4;m.words[F::Owner+0x14]=1;
    m.words[0x55b5e0]=std::bit_cast<U32>(-384.f);
    m.words[0x55b5f8]=std::bit_cast<U32>(-184.f);
    m.words[0x55b5f0]=std::bit_cast<U32>(56.f);
    m.words[0x55b608]=std::bit_cast<U32>(78.f);
    std::array<F::Rect,F::Count> motion_bounds;
    F::Layout motion_layout;F::Atlas(m,false,motion_layout,&motion_bounds);
    assert(motion_bounds[0].slide_x==0 && motion_bounds[1].slide_y==-16.5f);
    const auto selected_transform=F::PartTransform(motion_layout,1,motion_bounds);
    assert(selected_transform.y==-16.5f);
    m.words[F::Owner+0x10]=7;
    F::Atlas(m,false,motion_layout,&motion_bounds);
    for(unsigned i=F::Start;i<=F::Erase;++i)assert(motion_bounds[i].moving);
    m.words[F::Owner+0x10]=9;
    m.words[0x55b610]=std::bit_cast<U32>(80.f);m.words[0x55b61c]=0;
    F::Atlas(m,false,motion_layout,&motion_bounds);
    assert(motion_bounds[0].slide_x==150 && motion_bounds[1].slide_y==16.5f);
    assert(F::PartTransform(motion_layout,1,motion_bounds).y==16.5f);
    for(unsigned i=F::Start;i<=F::Erase;++i)assert(motion_bounds[i].moving);
    m.words[0x55b610]=0;
    m.words[F::Owner+0x10]=2;
    // Selection exits stay linear through the screen edge at 60 Hz. Include
    // the user's 0.78 group scale and extreme edits: the complete card clears
    // before native parking, while names and shadows share the same motion.
    {
        const auto memory=m.words;
        for(bool opening:{false,true})for(unsigned selected=0;selected<3;++selected)for(float scale:{.4f,.78f,1.f,2.f}) {
            F::action_opening=opening;
            F::Layout l;l.stones={120,9,scale};l.parts[0].scale=1.3f;
            m.words[F::Owner+0x10]=4;m.words[F::Owner+0x14]=selected;
            std::array<float,3> last{},step{};
            for(unsigned frame=0;frame<=10;++frame) {
                F::action_frames=frame;
                auto raw=original;
                for(unsigned i=0;i<3;++i) {
                    const float x=i==selected?16.f:16.f-40*frame,y=48.f+52*i;
                    m.words[0x55b5f8+4*i]=std::bit_cast<U32>(x);
                    m.words[0x55b5e0+4*i]=std::bit_cast<U32>(i==selected?16.f:-384.f);
                    m.words[0x55b610+4*i]=std::bit_cast<U32>(i==selected?0.f:-80.f);
                    m.words[0x55b61c+4*i]=0;
                    const unsigned first=6+37*i;
                    for(unsigned q=first;q<first+3;++q)for(unsigned v=0;v<4;++v) {
                        raw.positions[q*12+v*3]=x-2+96*(q-first)+(v&1?96:0);
                        raw.positions[q*12+v*3+1]=y+(v&2?44:0);
                    }
                }
                m.Floats(pos,raw.positions);std::array<F::Rect,F::Count> bs{};
                const auto out=F::Atlas(m,false,l,&bs);
                for(unsigned i=0;i<3;++i)if(i!=selected) {
                    const auto b=F::Bounds(out,6+37*i,8+37*i);
                    if(frame && (!opening || frame<=9)) {
                        const float delta=last[i]-b.right;
                        assert(delta>0);
                        if(frame>1)assert(std::abs(delta-step[i])<.001f);
                        step[i]=delta;
                    }
                    last[i]=b.right;
                    const auto t=F::PartTransform(l,i,bs);
                    assert(std::abs(b.right-(t.x-40+t.ax+(bs[i].right-t.ax)*t.sx))<.001f);
                    if(frame>=(opening?9u:10u))assert(b.right+40<=-23.99f);
                }
            }
        }
        F::action_opening=false;
        m.words=memory;
    }
    // A newly saved slot shares one screen-space slide with its action cards,
    // even with different editor scales. Hiding slot 2 must not lose the group pivot.
    {
        const auto memory=m.words;
        m.words[F::Owner+0x10]=8;
        F::Layout l;l.stones={-35,9,1.35f};l.stone_spacing=.8f;
        l.parts[F::Stone2]={7,-4,.9f};
        l.parts[F::Start]={-17,6,1.2f};l.parts[F::Copy]={22,-9,.75f};
        for(unsigned selected=0;selected<3;++selected) {
            auto raw=original;
            for(unsigned q=0;q<F::QuadCount;++q) {
                const auto part=F::QuadPart(q);
                const auto b=F::Bounds(raw,q,q);
                float x=std::bit_cast<float>(*m.Read(0x504ff8+q*8));
                float y=std::bit_cast<float>(*m.Read(0x504ff8+q*8+4));
                if(part<=F::Stone3) {x+=part==selected?0:-400;y+=part==selected?8.f-52.f*part:0;}
                for(unsigned v=0;v<4;++v) {
                    raw.positions[q*12+v*3]+=x-b.left;
                    raw.positions[q*12+v*3+1]+=y-b.top;
                }
            }
            const auto faces=F::PartBounds(raw);
            for(unsigned q=0;q<F::QuadCount;++q) {
                const auto part=F::QuadPart(q);if(part>F::Stone3)continue;
                const float dx=(part==selected?16.f:-384.f)-faces[part].left;
                const float dy=(part==selected?56.f:48.f+52*part)-faces[part].top;
                for(unsigned v=0;v<4;++v) {raw.positions[q*12+v*3]+=dx;raw.positions[q*12+v*3+1]+=dy;}
            }
            m.Floats(pos,raw.positions);
            for(unsigned i=0;i<3;++i) {m.Float(0x55b610+4*i,0);m.Float(0x55b61c+4*i,0);}
            l.saved_file_slot=3;
            std::array<F::Rect,F::Count> normal{};F::Atlas(m,false,l,&normal);
            l.saved_file_slot=selected;l.saved_file_remaining=0;
            std::array<F::Rect,F::Count> settled{};F::Atlas(m,false,l,&settled);
            const std::array parts{selected,unsigned(F::Start),unsigned(F::Copy),unsigned(F::Erase)};
            const auto left=[&](unsigned part,const auto& bounds) {
                const auto t=F::PartTransform(l,part,bounds);const auto b=bounds[part];
                return t.x+t.ax+(b.left-t.ax)*t.sx;
            };
            for(auto part:parts)assert(std::abs(left(part,settled)-left(part,normal))<.001f);
            const auto before_slide=m.words;
            float previous=INFINITY;
            for(unsigned frame=0;frame<=12;++frame) {
                l.saved_file_remaining=F::EasePosition(1-frame/12.f,1,0,l.stone_ease_out);
                std::array<F::Rect,F::Count> moving{};
                const auto out=F::Atlas(m,false,l,&moving);
                const float dx=left(selected,moving)-left(selected,settled);
                assert(dx<=previous && (frame==12 || dx>0));previous=dx;
                for(auto part:parts) {
                    assert(std::abs(left(part,moving)-left(part,settled)-dx)<.001f);
                    if(frame==0)assert(left(part,moving)>=423.99f);
                }
                for(unsigned i=0;i<3;++i)if(i!=selected) {
                    assert(!moving[i].Valid());
                    assert(!F::Bounds(out,6+37*i,8+37*i).Valid());
                }
                assert(m.words==before_slide);
            }
        }
        m.words=memory;
    }
    // Existing-file action cards enter concurrently with the selected file,
    // using the native per-piece resting positions, with 60 Hz ease-out.
    m.words[F::Owner+0x10]=4;F::action_opening=true;
    for(unsigned n=0;n<=10;++n) {
        F::action_frames=n;
        const auto actions=F::Atlas(m,false);
        const float offset=F::EasePosition(400*(1-n/10.f),400,0,2);
        for(unsigned q:{122u,126u,130u}) {
            const auto b=F::Bounds(actions,q,q);
            assert(std::abs(b.left-(std::bit_cast<float>(*m.Read(0x504ff8+q*8))+offset))<.001f);
            assert(std::abs(b.top-std::bit_cast<float>(*m.Read(0x504ff8+q*8+4)))<.001f);
        }
        for(unsigned q=175;q<184;++q)assert(!F::Bounds(actions,q,q).Valid());
    }
    F::action_opening=false;m.words[F::Owner+0x10]=2;
    // Clipping preserves the texel scale and interpolates vertex opacity.
    F::Geometry partial;partial.Add(310,20,20,20,0,1,1,0);
    partial.colors[3]=partial.colors[11]=0;
    assert(F::Clip(partial,0));
    assert(partial.positions[3]==320 && partial.uvs[2]==.5f && partial.colors[7]==.5f);
    F::Geometry outside;outside.Add(400,20,20,20,0,1,1,0);
    assert(!F::Clip(outside,0));
    m.words[helper]=183;assert(!F::Active(m) && F::Atlas(m,false).count==0);m.words[helper]=184;
    m.words[F::Owner+0x10]=0;assert(!F::Active(m));m.words[F::Owner+0x10]=2;
    const auto atlas=Read(std::string(argv[2])+"/message/parts.ctxb");
    const auto font=Read(std::string(argv[2])+"/message/us/ltn16.qbf");
    {
        const auto badge_font=OOT3D::FileStatusText::MakeAssets(font);assert(badge_font);
        Memory bm;F::Geometry cards;std::array<F::Rect,F::Count> card_bounds{};
        for(unsigned slot=0;slot<3;++slot) {
            bm.words[OOT3D::MasterQuest::Exists+slot*4]=1;
            bm.words[(OOT3D::MasterQuest::Saves+slot*OOT3D::MasterQuest::Stride+0x2f)&~3u]=slot==1?0:0xa7000000;
            card_bounds[slot]={16,56.f+60*slot,304,100.f+60*slot};
            cards.count=6+37*slot;
            for(unsigned cap=0;cap<3;++cap)cards.Add(16+96*cap,56+60*slot,96,44,0,0,1,1);
            cards.count=11+37*slot;cards.Add(184,62+60*slot,10,10,0,0,1,1);
        }
        cards.count=F::QuadCount;
        OOT3D::FileStatusText::Geometry marks;
        const auto before=bm.words;
        F::RandomizerBadges(bm,marks,*badge_font,cards,{},card_bounds);
        assert(marks.count==90 && bm.words==before); // Two RANDO labels; ordinary file has none.
        assert(std::abs(F::Bounds(marks,44,44).right-219)<.001f); // Five-pixel gap before first heart.
        assert(std::abs(F::Bounds(marks,89,89).right-219)<.001f); // Same alignment on every card.
        assert(std::abs(F::Bounds(marks,44,44).bottom-F::Bounds(marks,44,44).top-14)<.001f);
        assert(marks.colors[8*16]==1 && marks.colors[8*16+1]==.18f);
        card_bounds[0].moving=true;card_bounds[0].slide_x=-100;
        F::Layout shifted;shifted.stones.scale=.8f;
        OOT3D::FileStatusText::Geometry moved;
        auto posed_cards=cards;
        for(unsigned slot=0;slot<3;++slot) {
            const auto t=F::PartTransform(shifted,slot,card_bounds);
            for(unsigned q=6+slot*37;q<43+slot*37;++q)for(unsigned v=0;v<4;++v) {
                auto& x=posed_cards.positions[q*12+v*3];auto& y=posed_cards.positions[q*12+v*3+1];
                x=t.x-40+t.ax+(x-t.ax)*t.sx;y=t.y+t.ay+(y-t.ay)*t.sy;
            }
        }
        F::RandomizerBadges(bm,moved,*badge_font,posed_cards,shifted,card_bounds);
        const auto heart=F::Bounds(posed_cards,11,11);
        assert(std::abs(F::Bounds(moved,44,44).right-(heart.left+40-5*.8f))<.001f);
        bm.words[OOT3D::MasterQuest::Exists]=0;
        OOT3D::FileStatusText::Geometry erased;
        F::RandomizerBadges(bm,erased,*badge_font,cards,{},card_bounds);
        assert(erased.count==45); // Old marker bytes cannot label an empty/erased slot.
    }
    {
        F::Layout aligned;
        aligned.prompts={{{360.6f,211.2f,.74f},{360.6f,224.4f,.74f}}};
        for(unsigned i=0;i<2;++i) {
            const auto p=F::NamePromptPose(aligned,i),base=aligned.prompts[i];
            assert(p.x==base.x && p.y==base.y && p.scale==base.scale);
        }
        aligned.name_prompts[0]={-20,5,2};
        const auto p=F::NamePromptPose(aligned,0);
        assert(p.x==340.6f && p.y==216.2f && p.scale==1.48f);
        Memory pm;
        assert(F::StandardPromptsVisible(pm));
        OOT3D::MasterQuest::choosing=true;OOT3D::MasterQuest::leaving=true;
        for(float a:{0.f,.5f,1.f}) {
            OOT3D::MasterQuest::loading_alpha=a;
            assert(!F::StandardPromptsVisible(pm));
        }
        OOT3D::MasterQuest::generation_return=true;
        assert(F::StandardPromptsVisible(pm));
        OOT3D::MasterQuest::generation_return=false;OOT3D::MasterQuest::cancelling=true;
        assert(F::StandardPromptsVisible(pm));
        OOT3D::MasterQuest::choosing=false;OOT3D::MasterQuest::leaving=false;
        OOT3D::MasterQuest::cancelling=false;OOT3D::MasterQuest::loading_alpha=0;
        pm.words[F::Owner+0x10]=5;assert(!F::StandardPromptsVisible(pm));
        pm.words[F::Owner+0x10]=0;
        for(unsigned state=1;state<=10;++state) {
            pm.words[F::NameOwner+4]=state;
            assert(!F::StandardPromptsVisible(pm)); // Includes model allocation/teardown.
        }
        pm.words[F::NameOwner+4]=0;assert(F::StandardPromptsVisible(pm));
    }
    {
        Memory selection;
        OOT3D::MasterQuest::choosing=false;OOT3D::MasterQuest::cancelling=false;
        for(unsigned state:{3u,14u,8u,10u})for(unsigned index=0;index<=3;++index) {
            selection.words[F::Owner+0x10]=state;
            selection.words[F::Owner+(state==3?0x14:state==14?0x1c:0x18)]=index;
            assert(F::FocusedStone(selection,false)==(index>=3?F::FocusCount:state==3 || state==14?index:F::Start+index));
        }
        selection.words[F::Owner+0x10]=37;
        for(unsigned index=0;index<2;++index) {
            selection.words[F::Owner+0x24]=index;
            assert(F::FocusedStone(selection,false)==(index?F::DeleteErase:F::DeleteCancel));
            OOT3D::MasterQuest::choosing=true;OOT3D::MasterQuest::highlighted=index;
            assert(F::FocusedStone(selection,false)==F::QuestFocus+index);
            OOT3D::MasterQuest::choosing=false;
            selection.words[F::NameOwner+4]=4;selection.words[F::NameOwner+8]=4;
            selection.words[F::NameOwner+0x2c]=index;
            assert(F::FocusedStone(selection,true)==F::NameFocus+(index?F::ConfirmYes:F::ConfirmNo));
        }
        selection.words[F::NameOwner+8]=2;
        assert(F::FocusedStone(selection,true)==F::FocusCount);
        F::Layout focus;focus.stone_focus_grow=true;focus.stone_focus_seconds=0;
        focus.parts[0]={7,9,4};focus.stones={3,-2,2};
        std::array<F::Rect,F::Count> bounds{};
        for(unsigned i=0;i<3;++i)bounds[i]={16,48.f+52*i,304,92.f+52*i};
        const auto base=F::PartTransform(focus,0,bounds);
        F::StoneFocusAnimation animation;animation.Apply(focus,0);
        const auto grown=F::PartTransform(focus,0,bounds);
        assert(std::abs(grown.sx/base.sx-1.05f)<.00001f);
        assert(grown.x==base.x && grown.y==base.y && grown.ax==base.ax && grown.ay==base.ay);
        assert(focus.parts[0].scale==4 && focus.stones.scale==2);
        animation.Apply(focus,2);assert(focus.stone_focus_amount[0]==0 && focus.stone_focus_amount[2]==1);
        animation={};focus.stone_focus_seconds=.1f;
        float previous=0;
        for(unsigned frame=0;frame<6;++frame) {
            animation.Apply(focus,0);
            assert(focus.stone_focus_amount[0]>previous);previous=focus.stone_focus_amount[0];
        }
        assert(std::abs(previous-1)<.00001f);
        animation.Apply(focus,1);
        assert(focus.stone_focus_amount[0]<1 && focus.stone_focus_amount[1]>0);
        for(unsigned frame=0;frame<6;++frame)animation.Apply(focus,1);
        assert(focus.stone_focus_amount[0]==0 && focus.stone_focus_amount[1]==1);
        focus.stone_focus_seconds=0;
        for(unsigned part:{F::Start,F::Copy,F::Erase,F::DeleteCancel,F::DeleteErase}) {
            bounds[part]={34,94,158,146};focus.parts[part]={7,-3,.8f};
            animation.Apply(focus,F::FocusCount);
            const auto base=F::PartTransform(focus,part,bounds);
            animation.Apply(focus,part);
            const auto grown=F::PartTransform(focus,part,bounds);
            assert(std::abs(grown.sx/base.sx-1.05f)<.00001f);
            assert(base.x==grown.x && base.y==grown.y && base.ax==grown.ax && base.ay==grown.ay);
        }
        for(unsigned mode=0;mode<2;++mode) {
            animation.Apply(focus,F::FocusCount);
            const auto base=F::QuestStones(mode,{},focus);
            animation.Apply(focus,F::QuestFocus+mode);
            const auto grown=F::QuestStones(mode,{},focus);
            for(unsigned q=0;q<grown.count;++q) {
                const auto a=F::Bounds(base,q,q),b=F::Bounds(grown,q,q);
                const bool selected=q>=4 || q/2==mode;
                assert(std::abs((b.right-b.left)/(a.right-a.left)-(selected?1.05f:1.f))<.0001f);
                if(q==mode*2+1)assert(std::abs(a.X()-b.X())<.0001f && std::abs(a.Y()-b.Y())<.0001f);
            }
        }
        for(unsigned part:{F::ConfirmNo,F::ConfirmYes}) {
            animation.Apply(focus,F::FocusCount);const auto base=F::NameTransform(focus,part);
            animation.Apply(focus,F::NameFocus+part);const auto grown=F::NameTransform(focus,part);
            assert(std::abs(grown.sx/base.sx-1.05f)<.00001f);
            assert(base.x==grown.x && base.y==grown.y && base.ax==grown.ax && base.ay==grown.ay);
        }
        focus.stone_focus_grow=false;animation.Apply(focus,1);
        assert((focus.stone_focus_amount==std::array<float,F::FocusCount>{}));
    }
    const auto assets=P::MakeAssets(atlas,font);assert(assets);
    // Every consumer must accept the complete shared atlas after new labels
    // are added. A stale size silently skips text while icons still draw.
    static_assert(decltype(F::Hook::text)::TextureBytes==sizeof(P::Assets::text));
    F::Layout l;const auto prompts=F::Prompts(l,*assets,.4f);
    assert(prompts.icons.count==2 && prompts.text.count==10);
    assert(prompts.icons.uvs[0]==128.5f/256 && prompts.icons.uvs[8]==144.5f/256);
    assert(prompts.icons.uvs[1]==95.5f/128 && prompts.icons.colors[3]==.4f);
    l.prompts[0]={50,80,2};const auto moved=F::Prompts(l,*assets,1);
    for(unsigned v=0;v<12;v+=3) {
        assert(moved.icons.positions[v]==50+2*(prompts.icons.positions[v]-120));
        assert(moved.icons.positions[v+1]==80+2*(prompts.icons.positions[v+1]-220));
        assert(moved.icons.positions[12+v]==prompts.icons.positions[12+v]);
    }
    for(unsigned v=0;v<6*12;v+=3) {
        assert(std::abs(moved.text.positions[v]-(50+2*(prompts.text.positions[v]-120)))<.0001f);
        assert(std::abs(moved.text.positions[v+1]-(80+2*(prompts.text.positions[v+1]-220)))<.0001f);
    }
    for(float width:{400.f,426.6667f}) {
        const auto touch=F::MapPointer(.5f,.5f,width);assert(touch && (*touch)[0]==.5f && (*touch)[1]==.5f);
        assert(!F::MapPointer(0,.5f,width));
        assert(!F::MapPointer((width-320)/2/width,220.f/240,width));
    }
    F::touch_bounds={};F::touch_layout={};
    F::touch_bounds[F::Stone1]={16,48,304,92};
    F::touch_layout.parts[F::Stone1]={100,60,.5f};
    const auto touched=F::MapPointer(300.f/400,130.f/240,400);
    assert(touched && (*touched)[0]==.5f && std::abs((*touched)[1]-70.f/240)<.0001f);
    assert(!F::MapPointer(200.f/400,70.f/240,400)); // Old location is inactive.
    F::touch_bounds={};F::touch_layout={};
    // Erase's destination prompt exists on the accepted-input frame, before
    // native state 36 finishes the entrance. B immediately restores slot 7.
    // Use native controllers so translations, font and saved prompt pose match.
    {
        const auto memory=m.words;
        constexpr U32 old_text=0x19290000,new_text=0x19291000,old_model=0x19292000,new_model=0x19293000;
        m.Allocate(old_text,0x4c);m.Allocate(new_text,0x4c);
        m.words[old_text+0x40]=3;m.words[old_text+0x10]=old_model;
        m.words[new_text+0x40]=3;m.words[new_text+0x10]=new_model;
        for(U32 state:{34u,36u,38u}) {
            const unsigned desired=state==38?7:19,previous=state==38?19:7;
            m.words[F::Owner+0x10]=state;
            m.words[0x55b548+previous*4]=old_text;m.words[0x55b548+desired*4]=0;
            F::Hook h;std::array<U32,16> r{};h.Begin(m,r,0x300568,true);
            assert(h.stage==F::Hook::Stage::PromptInit && r[0]==desired && r[15]==0x2ee864);
            m.words[0x55b548+desired*4]=new_text;
            // After construction, destination text uses the prompt transform;
            // the outgoing text cannot overlap it on this first frame.
            h.stage=F::Hook::Stage::Native;h.frame_layout.parts[F::Prompt]={12,-3,1.25f};
            assert(h.ModelTransform(m,old_model).sx==0);
            const auto t=h.ModelTransform(m,new_model);
            assert(t.sx==1.25f && t.x==52 && t.y==-3);
            F::Hook next;next.Begin(m,r,0x300568,true);
            assert(next.prompt_slot==desired && next.stage!=F::Hook::Stage::PromptInit);
        }
        m.words=memory;
    }
    // Exercise the injected guest-call chain through allocation, uploads, native
    // text/cursor drawing and return. No borrowed texture allocation is permitted.
    F::Hook hook;std::array<U32,16> regs{};regs[15]=0x2ffe40;regs[14]=0x30056c;
    const auto saved=regs;hook.Begin(m,regs,0x300568,true);
    assert(hook.stage==F::Hook::Stage::PromptInit && regs[0]==6 && regs[15]==0x2ee864);
    // The native constructor completes before this same frame draws the box.
    constexpr U32 native_prompt=0x19280000;
    m.Allocate(native_prompt,0x4c);m.words[0x55b560]=native_prompt;
    OOT3D::MasterQuest::prompt_controller=native_prompt;OOT3D::MasterQuest::prompt_message=0x950;
    hook.Resume(m,regs);assert(hook.Pending());
    auto drive=[&](auto& mesh_hook,U32 block,U32 created,auto resume) {
        using H=std::remove_reference_t<decltype(mesh_hook)>;
        unsigned steps=0;
        while(mesh_hook.Pending()) {
            assert(++steps<12);
            if(mesh_hook.stage==H::Stage::Allocate) {m.Allocate(block,H::AllocationSize);regs[0]=block;}
            if(mesh_hook.stage==H::Stage::Mesh) {
                m.words[block]=block+H::Descriptor;m.words[block+0x124]=0;m.words[block+0x128]=2;
            }
            if(mesh_hook.stage==H::Stage::Model) {
                m.Allocate(created,0x1e4);m.words[created]=0x4ebd60;m.words[created+8]=created+0x1000;m.words[created+0x1358]=material;regs[0]=created;
            }
            assert(mesh_hook.stage!=H::Stage::TextureAllocate);
            resume();
        }
        assert(mesh_hook.Owned(m));
        for(unsigned i=0;i<24;++i)assert(m.words[block+0x13c+i*4]==m.words[mesh+0x13c+i*4]);
    };
    unsigned vignette_allocations=0;
    auto drive_vignette=[&](F::Hook& h) {
        auto& vg=h.vignette;using VH=std::remove_reference_t<decltype(vg)>;
        const U32 block=vg.block?vg.block:0x19400000+(vignette_allocations++)*0x20000;
        const U32 model=block+0x18000,texture=0x14700000+(block-0x19400000)/32;
        unsigned steps=0;
        while(vg.Pending()) {
            assert(h.stage==F::Hook::Stage::Vignette && ++steps<12);
            if(vg.stage==VH::Stage::Allocate){m.Allocate(block,VH::AllocationSize);regs[0]=block;}
            if(vg.stage==VH::Stage::TextureAllocate){m.Allocate(texture,VH::TextureBytes);regs[0]=texture;}
            if(vg.stage==VH::Stage::Mesh){m.words[block]=block+VH::Descriptor;m.words[block+0x128]=2;}
            if(vg.stage==VH::Stage::Model){m.Allocate(model,0x1e4);m.words[model]=0x4ebd60;m.words[model+8]=model+0x1000;m.words[model+0x1358]=material;regs[0]=model;}
            if(vg.Drawing()) {
                assert(h.Drawing() && !h.backgrounds.Pending() && !h.shadows.Pending() && !h.top.Pending());
                assert(h.ModelTransform(m,model).x==0 && vg.geometry.count==576);
                m.words[0x18500000]=std::bit_cast<U32>(.1f);h.Material(m,0x18500000);
                assert(std::bit_cast<float>(*m.Read(0x18500000))==1.f);
            }
            h.Resume(m,regs);
        }
    };
    auto drive_shadows=[&](F::Hook& h,U32 block,U32 model,U32 texture) {
        drive_vignette(h);
        auto& sh=h.shadows;using SH=std::remove_reference_t<decltype(sh)>;
        unsigned steps=0;
        assert(h.stage==F::Hook::Stage::Shadows);
        while(sh.Pending()) {
            assert(++steps<12);
            if(sh.stage==SH::Stage::Allocate){m.Allocate(block,SH::AllocationSize);regs[0]=block;}
            if(sh.stage==SH::Stage::TextureAllocate){m.Allocate(texture,SH::TextureBytes);regs[0]=texture;}
            if(sh.stage==SH::Stage::Mesh){m.words[block]=block+SH::Descriptor;m.words[block+0x128]=2;}
            if(sh.stage==SH::Stage::Model){m.Allocate(model,0x1e4);m.words[model]=0x4ebd60;m.words[model+8]=model+0x1000;m.words[model+0x1358]=material;regs[0]=model;}
            if(sh.Drawing()) {
                assert(!h.top.Pending() && !h.backgrounds.Pending());
                assert(h.ModelTransform(m,model).x==40);
                assert(sh.geometry.count==(h.name_frame?9u:h.return_frame?45u:h.quest_frame?18u:72u));
            }
            h.Resume(m,regs);
        }
        assert(h.stage==(h.name_frame?F::Hook::Stage::Name:h.quest_frame?F::Hook::Stage::Quest:F::Hook::Stage::Atlas) && sh.Owned(m));
    };
    drive_shadows(hook,0x19000000,0x19010000,0x14500000);
    drive(hook.top,0x18100000,0x18200000,[&]{hook.Resume(m,regs);});
    assert(hook.NativeDrawing() && regs[15]==0x41f2e4 && F::on_top.load());
    // Native lettering, outlines and cursor share their stone's pivot and pose.
    constexpr U32 controller=0x18600000,name=0x18610000,cursor=0x18620000,cursor_model=0x18630000;
    m.Allocate(controller,0x4c);m.words[0x55b548]=controller;
    m.words[controller+0x40]=2;
    for(unsigned j=0;j<9;++j)m.words[controller+0x10+j*4]=name+j*0x200;
    hook.frame_layout.parts[F::Stone1]={11,13,2};
    for(unsigned j=0;j<9;++j) {
        const auto t=hook.ModelTransform(m,name+j*0x200);
        assert(t.x==51 && t.y==13 && t.sx==2 && t.sy==2);
        assert(t.ax==hook.bounds[0].X() && t.ay==hook.bounds[0].Y());
    }
    m.words[F::Owner+0x34]=cursor;m.words[cursor+4]=cursor_model;
    m.words[F::Owner+0x10]=3;m.words[F::Owner+0x14]=0;
    const auto cursor_t=hook.ModelTransform(m,cursor_model);
    assert(cursor_t.x==51 && cursor_t.y==13 && cursor_t.sx==2 && cursor_t.sy==2);
    m.words[F::Owner+0x10]=2;
    assert(hook.ModelTransform(m,cursor_model).sx==0); // Parked native cursor.
    hook.bounds[0]={400,48,688,92};
    assert(hook.ModelTransform(m,name).sx==0); // Hidden stone hides its lettering.
    hook.stage=F::Hook::Stage::Atlas;
    const auto atlas_t=hook.ModelTransform(m,model);
    assert(atlas_t.x==40 && atlas_t.sx==1 && atlas_t.sy==1);
    hook.stage=F::Hook::Stage::Native;
    m.words[0x18500000]=std::bit_cast<U32>(.8f);
    hook.Material(m,0x18500000);assert(std::bit_cast<float>(m.words[0x18500000])==.8f);
    hook.Resume(m,regs);assert(!hook.Pending() && regs==saved);
    hook.Material(m,0x18500000);assert(std::bit_cast<float>(m.words[0x18500000])==.8f);
    assert(m.words[pos]==before.at(pos));
    hook.Bottom(m,regs);assert(hook.bottom.Pending());
    drive(hook.bottom,0x18300000,0x18400000,[&]{hook.Bottom(m,regs);});
    assert(regs[15]==F::Sites[0].pc+4);
    // A recreated donor does not invalidate our independently owned mesh.
    // Its current material and sampler are rebound on the next draw.
    m.words[model+0x1dc]+=4;assert(hook.top.Owned(m));m.words[model+0x1dc]-=4;
    hook.Begin(m,regs,0x300568,false);assert(!F::on_top.load() && !hook.Pending());
    hook.Bottom(m,regs);assert(regs[15]==0x41f2e4 && !hook.bottom.Pending());
    // Four original brown panels: two touching panels per independently editable stack.
    namespace BG=OOT3D::FileBackground;
    const auto background_bytes=Read(std::string(argv[2])+"/menu/01_US_ENGLISH/common_bg01.ctxb");
    const auto background=BG::MakePixels(background_bytes);assert(background);
    assert(!BG::MakePixels({}));
    for(unsigned i=0;i<background->size();++i)
        assert((*background)[i]==OOT3D::MapFrame::LE32(background_bytes,72+4*i));
    F::Layout panels;
    const auto normal=F::Backgrounds(panels);assert(normal.count==4);
    // The player toggle suppresses only the decorations, without overwriting
    // the editor layout. Turning it off restores the exact authored geometry.
    {
        F::SetLayout(panels,false);
        OOT3D::UserHUD::classic_file_select=true;
        const auto classic=F::PresentationLayout(F::GetLayout());
        assert(F::Backgrounds(classic).count==0);
        assert(F::CornerVignette(classic.vignette,true).count==0);
        assert(F::GetLayout().backgrounds_enabled && F::GetLayout().vignette.enabled);
        assert(classic.stone_shadow_opacity==panels.stone_shadow_opacity);
        assert(classic.keyboard_shadow.enabled==panels.keyboard_shadow.enabled);
        OOT3D::UserHUD::classic_file_select=false;
        const auto restored=F::Backgrounds(F::PresentationLayout(F::GetLayout()));
        assert(restored.positions==normal.positions && restored.colors==normal.colors);
    }
    for(unsigned side=0;side<2;++side) {
        const unsigned q=2*side;
        assert(normal.positions[q*12+7]==normal.positions[(q+1)*12+1]);
        assert(normal.positions[q*12+3]-normal.positions[q*12]==256);
        assert(normal.positions[q*12+7]-normal.positions[q*12+1]==192);
    }
    panels.backgrounds[0]={{-80,10,.5f},.65f};
    panels.backgrounds[1].darkness=.2f;
    const auto edited=F::Backgrounds(panels);
    assert(edited.positions[3]-edited.positions[0]==128);
    assert(edited.positions[7]==130 && edited.positions[13]==130);
    for(unsigned q=0;q<4;++q)for(unsigned v=0;v<4;++v) {
        assert(std::abs(edited.colors[q*16+v*4]-(q<2?.35f:.8f))<.0001f);
        assert(edited.colors[q*16+v*4+3]==1);
    }
    for(unsigned i=24;i<48;++i)assert(edited.positions[i]==normal.positions[i]);
    panels.backgrounds_enabled=false;assert(!F::Backgrounds(panels).count);
    std::atomic_store(&BG::pixels,std::shared_ptr<const BG::Pixels>(std::make_shared<BG::Pixels>(*background)));
    F::Hook decorated;F::SetLayout({},false);
    regs=saved;decorated.Begin(m,regs,0x300568,true);
    assert(decorated.stage==F::Hook::Stage::Background && !decorated.top.Pending());
    auto& bg=decorated.backgrounds;using BH=std::remove_reference_t<decltype(bg)>;
    unsigned steps=0,draws=0;
    while(bg.Pending()) {
        assert(++steps<12);
        if(bg.stage==BH::Stage::Allocate) {m.Allocate(0x18700000,BH::AllocationSize);regs[0]=0x18700000;}
        if(bg.stage==BH::Stage::TextureAllocate) {
            assert(regs[3]==512*256/2);
            m.Allocate(0x14000000,BH::TextureBytes);regs[0]=0x14000000;
        }
        if(bg.stage==BH::Stage::Mesh) {
            m.words[bg.block]=bg.block+BH::Descriptor;
            m.words[bg.block+0x124]=0;m.words[bg.block+0x128]=2;
        }
        if(bg.stage==BH::Stage::Model) {
            m.Allocate(0x18800000,0x1e4);m.words[0x18800000]=0x4ebd60;
            m.words[0x18800008]=0x18801000;m.words[0x18801000+0x358]=material;regs[0]=0x18800000;
        }
        if(bg.Drawing()) {
            ++draws;assert(!decorated.top.Pending() && !decorated.NativeDrawing());
            assert(decorated.ModelTransform(m,bg.model).x==0);
            assert(m.words[bg.block+0x164]==12 && m.words[bg.block+0x15c]==(512u|(256u<<16)));
            assert(bg.geometry.count==4);
            for(unsigned i=0;i<background->size();++i)assert(m.words[bg.texture+4*i]==(*background)[i]);
            m.words[0x18500000]=std::bit_cast<U32>(.1f);
            decorated.Material(m,0x18500000);assert(std::bit_cast<float>(m.words[0x18500000])==1.f);
        }
        decorated.Resume(m,regs);
    }
    assert(draws==1 && decorated.stage==F::Hook::Stage::Vignette && !decorated.top.Pending());
    drive_shadows(decorated,0x19020000,0x19030000,0x14501000);
    drive(decorated.top,0x18900000,0x18a00000,[&]{decorated.Resume(m,regs);});
    assert(decorated.NativeDrawing()); // Stones and prompt box are drawn after the background.
    decorated.Resume(m,regs);assert(!decorated.Pending() && regs==saved);
    // An MQ badge draws after the file atlas and before its native lettering.
    // It owns only its overlay texture/model and returns the guest registers.
    {
        const auto before=m.words;const auto before_layout=F::GetLayout();
        auto badge_layout=before_layout;badge_layout.backgrounds_enabled=false;badge_layout.stone_shadow_opacity=0;
        badge_layout.vignette.enabled=false;
        F::SetLayout(badge_layout,false);
        m.words[OOT3D::MasterQuest::Exists]=1;
        m.words[OOT3D::MasterQuest::Exists+4]=m.words[OOT3D::MasterQuest::Exists+8]=0;
        m.words[(OOT3D::MasterQuest::Saves+OOT3D::MasterQuest::ModeOffset)&~3u]=0x10000;
        std::atomic_store(&OOT3D::DPad::textures[1],std::shared_ptr<const OOT3D::DPad::Pixels>(
            std::make_shared<OOT3D::DPad::Pixels>()));
        F::Hook badged;regs=saved;badged.Begin(m,regs,0x300568,true);
        assert(badged.stage==F::Hook::Stage::Atlas && badged.badge_geometry.count==1);
        drive(badged.top,0x19200000,0x19210000,[&]{badged.Resume(m,regs);});
        assert(badged.stage==F::Hook::Stage::Badges);
        auto& bh=badged.badges;using BH=std::remove_reference_t<decltype(bh)>;
        unsigned steps=0;
        while(bh.Pending()) {
            assert(++steps<12);
            if(bh.stage==BH::Stage::Allocate) {m.Allocate(0x19220000,BH::AllocationSize);regs[0]=0x19220000;}
            if(bh.stage==BH::Stage::TextureAllocate) {m.Allocate(0x14600000,BH::TextureBytes);regs[0]=0x14600000;}
            if(bh.stage==BH::Stage::Mesh) {
                m.words[bh.block]=bh.block+BH::Descriptor;m.words[bh.block+0x124]=0;m.words[bh.block+0x128]=2;
            }
            if(bh.stage==BH::Stage::Model) {
                m.Allocate(0x19230000,0x1e4);m.words[0x19230000]=0x4ebd60;
                m.words[0x19230008]=0x19231000;m.words[0x19231000+0x358]=material;regs[0]=0x19230000;
            }
            if(bh.Drawing())assert(badged.Drawing() && badged.ModelTransform(m,bh.model).x==40);
            badged.Resume(m,regs);
        }
        assert(badged.NativeDrawing() && regs[15]==0x41f2e4 && bh.Owned(m));
        badged.Resume(m,regs);assert(!badged.Pending() && regs==saved);
        std::atomic_store(&OOT3D::DPad::textures[1],std::shared_ptr<const OOT3D::DPad::Pixels>{});
        m.words=before;F::SetLayout(before_layout,false);
    }
    // The quest dialog replaces the file cards, uses the original atlas, and
    // returns to the same top pass without drawing native names/cursor over it.
    const auto quest_pixels=OOT3D::QuestChoice::MakePixels(
        Read(std::string(argv[2])+"/misc/us/english/ura.ctxb"));assert(quest_pixels);
    std::atomic_store(&OOT3D::QuestChoice::pixels,
        std::shared_ptr<const OOT3D::QuestChoice::Pixels>(
            std::make_shared<OOT3D::QuestChoice::Pixels>(*quest_pixels)));
    auto dialog_layout=F::GetLayout();dialog_layout.backgrounds_enabled=false;
    F::SetLayout(dialog_layout,false);OOT3D::MasterQuest::choosing=true;
    OOT3D::MasterQuest::ready=true;
    OOT3D::MasterQuest::highlighted=1;
    F::Hook dialog;regs=saved;dialog.Begin(m,regs,0x300568,true);
    assert(dialog.stage==F::Hook::Stage::PromptDestroy && regs[0]==native_prompt && regs[15]==0x2f6944);
    dialog.Resume(m,regs);assert(dialog.stage==F::Hook::Stage::PromptFree && regs[15]==0x3525d4);
    dialog.Resume(m,regs);assert(dialog.stage==F::Hook::Stage::PromptInit && m.words[0x55b560]==0 && regs[0]==6);
    m.words[0x55b560]=native_prompt;OOT3D::MasterQuest::prompt_message=0x900;
    dialog.Resume(m,regs);
    drive_shadows(dialog,0x19040000,0x19050000,0x14502000);
    assert(dialog.stage==F::Hook::Stage::Quest && !dialog.top.Pending());
    auto& qm=dialog.quest;using QH=std::remove_reference_t<decltype(qm)>;
    steps=draws=0;
    while(qm.Pending()) {
        assert(++steps<12);
        if(qm.stage==QH::Stage::Allocate) {m.Allocate(0x18b00000,QH::AllocationSize);regs[0]=0x18b00000;}
        if(qm.stage==QH::Stage::TextureAllocate) {
            assert(regs[3]==512*256*2);m.Allocate(0x14100000,QH::TextureBytes);regs[0]=0x14100000;
        }
        if(qm.stage==QH::Stage::Mesh) {
            m.words[qm.block]=qm.block+QH::Descriptor;
            m.words[qm.block+0x124]=0;m.words[qm.block+0x128]=2;
        }
        if(qm.stage==QH::Stage::Model) {
            m.Allocate(0x18c00000,0x1e4);m.words[0x18c00000]=0x4ebd60;
            m.words[0x18c00008]=0x18c01000;m.words[0x18c01000+0x358]=material;regs[0]=0x18c00000;
        }
        if(qm.Drawing()) {
            ++draws;assert(!dialog.NativeDrawing() && dialog.Drawing());
            assert(dialog.ModelTransform(m,qm.model).x==0);
            assert(qm.geometry.count==12 && m.words[qm.block+0x164]==4);
        }
        dialog.Resume(m,regs);
    }
    assert(draws==1 && dialog.stage==F::Hook::Stage::Atlas);
    // The exact live header pieces and borrowed sampler remain; only the
    // native text controller changed. File cards are hidden on the mode page.
    for(unsigned q=0;q<6;++q)for(unsigned v=0;v<12;++v)
        assert(dialog.top.geometry.positions[q*12+v]==dialog.atlas_geometry.positions[q*12+v]);
    for(unsigned q=6;q<F::QuadCount;++q)for(unsigned v=0;v<12;++v)
        assert(dialog.top.geometry.positions[q*12+v]==0);
    drive(dialog.top,0x19260000,0x19270000,[&]{dialog.Resume(m,regs);});
    assert(dialog.NativeDrawing() && regs[15]==0x41f2e4);
    dialog.Resume(m,regs);assert(!dialog.Pending() && regs==saved);
    OOT3D::MasterQuest::highlighted=0;
    dialog.Begin(m,regs,0x300568,true);
    drive_vignette(dialog);
    while(dialog.shadows.Pending())dialog.Resume(m,regs);
    while(dialog.quest.Pending())dialog.Resume(m,regs);
    while(dialog.top.Pending())dialog.Resume(m,regs);
    assert(dialog.NativeDrawing());dialog.Resume(m,regs);
    assert(!dialog.Pending() && regs==saved);
    // Native file-select top decorations are suppressed only on the routed path.
    for(unsigned site:{4u,5u}) {
        auto rr=saved;dialog.NativeDecoration(m,rr,site,true);
        assert(rr[15]==F::Sites[site].pc+4);
        rr=saved;dialog.NativeDecoration(m,rr,site,false);assert(rr[15]==0x2d7354);
        const auto old=m.words[F::Owner+0x10];m.words[F::Owner+0x10]=0;
        dialog.NativeDecoration(m,rr,site,true);assert(rr[15]==0x2d7354);
        m.words[F::Owner+0x10]=old;
    }
    for(float width:{400.f,426.6667f}) {
        assert(!F::MapPointer((250+(width-400)/2)/width,100.f/240,width));
        assert(OOT3D::MasterQuest::pointer_action.exchange(0)==2);
        assert(!F::MapPointer(.5f,.02f,width));
        assert(OOT3D::MasterQuest::pointer_action.load()==0);
    }
    auto moved_quest=F::GetLayout();moved_quest.quest_stones={{{-90,0,.75f},{80,-40,.4f}}};
    F::SetLayout(moved_quest,false);dialog.Begin(m,regs,0x300568,true);
    drive_vignette(dialog);
    while(dialog.shadows.Pending())dialog.Resume(m,regs);
    while(dialog.quest.Pending())dialog.Resume(m,regs);
    while(dialog.top.Pending())dialog.Resume(m,regs);
    if(dialog.NativeDrawing())dialog.Resume(m,regs);
    assert(!dialog.Pending());
    F::MapPointer(250.f/400,100.f/240,400);assert(OOT3D::MasterQuest::pointer_action.load()==0);
    F::MapPointer(358.1f/400,82.8f/240,400);assert(OOT3D::MasterQuest::pointer_action.exchange(0)==2);
    OOT3D::MasterQuest::ready=false;
    F::MapPointer(358.1f/400,82.8f/240,400);assert(OOT3D::MasterQuest::pointer_action.load()==0);
    // B draws returning file cards and outgoing mode cards in the SAME frame.
    OOT3D::MasterQuest::cancelling=true;OOT3D::MasterQuest::returning=true;
    OOT3D::MasterQuest::leaving=true;OOT3D::MasterQuest::motion_us=100000;
    m.words[F::Owner+0x10]=3;
    dialog.Begin(m,regs,0x300568,true);
    assert(dialog.stage==F::Hook::Stage::PromptDestroy);
    dialog.Resume(m,regs);dialog.Resume(m,regs);
    m.words[0x55b560]=native_prompt;OOT3D::MasterQuest::prompt_message=0x950;
    dialog.Resume(m,regs);
    assert(dialog.return_frame && dialog.bounds[0].slide_x<0);
    assert(OOT3D::MasterQuest::Offsets(2)[0]>0);
    drive_shadows(dialog,0x19040000,0x19050000,0x14502000);
    while(dialog.quest.Pending())dialog.Resume(m,regs);
    assert(dialog.stage==F::Hook::Stage::Atlas);
    assert(dialog.top.geometry.positions==dialog.atlas_geometry.positions);
    while(dialog.top.Pending())dialog.Resume(m,regs);
    assert(dialog.NativeDrawing());dialog.Resume(m,regs);assert(!dialog.Pending());
    // Opening a new file keeps rendering the file page while its stones exit.
    OOT3D::MasterQuest::cancelling=false;OOT3D::MasterQuest::returning=false;
    OOT3D::MasterQuest::departing_files=true;OOT3D::MasterQuest::leaving=false;
    OOT3D::MasterQuest::prompt_message=0x900;
    dialog.Begin(m,regs,0x300568,true);
    assert(!dialog.quest_frame && dialog.bounds[0].slide_x<0);
    assert(F::Bounds(dialog.atlas_geometry,6,8).Valid());
    // The return uses the same exact saved pose when its offset reaches zero.
    OOT3D::MasterQuest::motion_us=OOT3D::MasterQuest::SlideUs+2*OOT3D::MasterQuest::StaggerUs;
    const auto away=F::Atlas(m,false,F::GetLayout());
    for(unsigned first:{6u,43u,80u})assert(F::Bounds(away,first,first+2).right+40<0);
    OOT3D::MasterQuest::departing_files=false;
    OOT3D::MasterQuest::choosing=false;
    // Naming disables file selection, but keeps the same background stacks.
    // The entire native draw (two atlases, twelve text controllers, cursor)
    // runs once on top, centered without applying file-stone editor offsets.
    F::SetLayout({},false);
    m.words[F::Owner+0x10]=0;
    m.words[F::NameOwner+4]=4;
    m.words[F::NameOwner+0x64]=model;m.words[F::NameOwner+0x68]=model;
    assert(!F::Active(m) && F::NameActive(m));
    regs=saved;decorated.Top(m,regs,true);
    assert(decorated.stage==F::Hook::Stage::Background);
    const auto naming_return=decorated.backgrounds.saved;
    assert(naming_return[15]==0x4228c0 && naming_return[14]==F::Sites[3].pc+4);
    unsigned name_steps=0;
    while(decorated.backgrounds.Pending()) {
        assert(++name_steps<12);
        assert(decorated.backgrounds.stage!=BH::Stage::Allocate);
        if(decorated.backgrounds.Drawing()) {
            m.words[0x18500000]=std::bit_cast<U32>(.1f);
            decorated.Material(m,0x18500000);
            assert(std::bit_cast<float>(*m.Read(0x18500000))==1.f);
        }
        decorated.Top(m,regs,true);
    }
    drive_vignette(decorated);
    assert(decorated.stage==F::Hook::Stage::Name && decorated.Drawing());
    assert(regs[15]==F::NameDraw && regs[14]==F::Sites[3].pc);
    assert(F::on_top.load() && F::name_on_top.load());
    // Atlas helpers are not ready yet: no stationary native piece may leak
    // into the viewport while the shared entrance is waiting to start.
    for(U32 native_model:{model,name,cursor_model}) {
        const auto t=decorated.ModelTransform(m,native_model);
        assert(t.sx==0 && t.sy==0);
    }
    std::array<U32,16> lower_regs=saved;
    decorated.NameBottom(lower_regs);assert(lower_regs[15]==F::Sites[2].pc+4);
    for(float width:{400.f,426.6667f}) {
        // Cancel, OK and keyboard edge keys must all remain touchable.
        for(auto point:{std::array{10.f,220.f},std::array{310.f,220.f},std::array{25.f,70.f}}) {
            const auto touch=F::MapPointer((point[0]+(width-320)/2)/width,point[1]/240,width);
            assert(touch && std::abs((*touch)[0]-point[0]/320)<.00001f &&
                   std::abs((*touch)[1]-point[1]/240)<.00001f);
        }
        assert(!F::MapPointer(0,.5f,width));
    }
    decorated.Top(m,regs,true);
    assert(!decorated.Pending() && regs==naming_return); // Original fade executes next.
    m.words[F::NameOwner+4]=0;
    decorated.Top(m,regs,true);
    assert(!decorated.Pending() && !F::on_top.load() && !F::name_on_top.load());
    decorated.NameBottom(lower_regs);assert(lower_regs[15]==F::NameDraw);
    m.words[F::NameOwner+4]=4;
    m.words[F::NameOwner+0x68]=0;assert(!F::NameActive(m));
    m.words[F::NameOwner+0x68]=model;
    decorated.Top(m,regs,false);
    assert(!decorated.Pending() && !F::on_top.load() && !F::name_on_top.load());
    m.words[F::NameOwner+4]=0;
    // Actual name-entry atlas layout; saved poses never mutate native vertices.
    constexpr U32 nh=0x19100000,np=0x19110000,nu=0x19120000,nc=0x19130000;
    F::NameGeometry native_names,native_keys;
    for(unsigned q=0;q<62;++q) {
        const float x=std::bit_cast<float>(*m.Read(0x507864+q*8));
        const float y=std::bit_cast<float>(*m.Read(0x507868+q*8));
        const float w=std::bit_cast<float>(*m.Read(0x507a54+q*8));
        const float h=std::bit_cast<float>(*m.Read(0x507a58+q*8));
        native_names.Add(x+(q==0 || q>=16?400:0),y,w,h,.1f,.8f,.6f,.2f);
    }
    native_keys.Add(-2,34,324,166,.1f,.8f,.6f,.2f);
    for(unsigned a=0;a<2;++a) {
        m.words[F::NameOwner+0x6c+a*4]=nh+a*0x100;
        m.words[nh+a*0x100]=a?62:1;
        m.words[nh+a*0x100+0x10]=np+a*0x1000;
        m.words[nh+a*0x100+0x14]=nu+a*0x1000;
        m.words[nh+a*0x100+0x18]=nc+a*0x1000;
        const auto& g=a?native_names:native_keys;
        m.Floats(np+a*0x1000,g.positions);m.Floats(nu+a*0x1000,g.uvs);m.Floats(nc+a*0x1000,g.colors);
    }
    m.words[F::NameOwner+4]=4;m.words[F::NameOwner+8]=2;m.words[F::NameOwner+0x40]=1;
    const auto name_memory=m.words;
    auto plain_keys=F::NameAtlas(m,0,{},1);
    assert(plain_keys.count==1 && F::Bounds(plain_keys,0,0).left==-2);
    for(unsigned k=0;k<12;++k)assert(plain_keys.positions[k]==native_keys.positions[k]);
    F::Layout nl;nl.backgrounds_enabled=false;
    nl.name_box_edge_fade=0; // Legacy frame geometry for the placement checks below.
    nl.name_parts[F::Keyboard]={-45,12,1.25f};
    nl.name_parts[F::NameBox]={17,24,.8f};
    nl.name_parts[F::Cancel]={-25,-20,1.4f};
    nl.name_parts[F::Okay]={32,-12,.75f};
    nl.name_parts[F::KeyboardBackground]=nl.name_parts[F::Keyboard];
    const auto moved_keys=F::NameAtlas(m,0,nl,1);
    assert(moved_keys.count==1);
    assert(std::abs(F::Bounds(moved_keys,0,0).X()-115)<.001f);
    assert(std::abs(F::Bounds(moved_keys,0,0).Y()-129)<.001f);
    const auto shadow=F::NameShadow(m,nl,1);
    assert(shadow.count==9);
    const auto center=F::Bounds(shadow,8,8),keys=F::Bounds(moved_keys,0,0);
    assert(std::abs(center.left-(keys.left-2*1.25f))<.001f);
    assert(std::abs(center.right-(keys.right+2*1.25f))<.001f);
    assert(std::abs(center.top-(keys.top+(3-2)*1.25f))<.001f);
    auto keyboard_shadow_layout=nl;keyboard_shadow_layout.keyboard_shadow.softness=24;
    const auto softer=F::NameShadow(m,keyboard_shadow_layout,1);
    assert(F::Bounds(softer,8,8).left==center.left);
    assert(F::Bounds(softer,0,8).left<F::Bounds(shadow,0,8).left);
    keyboard_shadow_layout.keyboard_shadow.enabled=false;
    assert(F::NameShadow(m,keyboard_shadow_layout,1).count==0);
    keyboard_shadow_layout.keyboard_shadow.enabled=true;keyboard_shadow_layout.keyboard_shadow.opacity=0;
    assert(F::NameShadow(m,keyboard_shadow_layout,1).count==0);
    const auto entering_shadow=F::NameShadow(m,nl,.5f);
    const auto entering_transform=F::NameTransform(nl,F::Keyboard,.5f);
    assert(std::abs(F::Bounds(entering_shadow,8,8).left-center.left-
        (entering_transform.x-F::NameTransform(nl,F::Keyboard,1).x))<.001f);
    const auto moved_names=F::NameAtlas(m,1,nl,1);
    assert(moved_names.count==15); // Hidden confirmations and pressed keys stay hidden.
    assert(std::abs(F::Bounds(moved_names,7,9).X()-27)<.001f);
    assert(std::abs(F::Bounds(moved_names,11,13).X()-300)<.001f);
    nl.name_parts[F::KeyboardBackground]={};
    const auto split=F::NameAtlas(m,0,nl,1);
    assert(split.count>100 && split.count<256);
    const auto split_again=F::NameAtlas(m,0,nl,.5f);
    assert(split_again.count==split.count);
    assert(name_memory==m.words);
    // Independent backing motion leaves the transformed key rectangles alone.
    nl.name_parts[F::KeyboardBackground]={60,-9,.7f};
    assert(F::NameShadow(m,nl,1).positions==shadow.positions);
    const auto split_back=F::NameAtlas(m,0,nl,1);
    for(unsigned k=98*12;k<split.count*12;++k)assert(split.positions[k]==split_back.positions[k]);
    assert(split.positions[0]!=split_back.positions[0]);
    F::SetLayout(nl,false);F::Hook naming;
    m.words[F::NameOwner+4]=2;
    float last_x=INFINITY;
    float last_step=INFINITY;
    for(unsigned frame=0;frame<=7+F::NameSlideFrames;++frame) {
        if(frame==7)m.words[F::NameOwner+4]=4;
        naming.Begin(m,regs,0x300520,true);
        drive_vignette(naming);
        if(naming.stage==F::Hook::Stage::Shadows)
            drive_shadows(naming,0x19300000,0x19310000,0x14504000);
        assert(naming.stage==F::Hook::Stage::Name && naming.name_editable);
        const float progress=frame<7?0.f:float(frame-7)/F::NameSlideFrames;
        assert(std::abs(naming.name_progress-progress)<.0001f);
        const auto t=F::NameTransform(nl,F::Keyboard,naming.name_progress);
        const auto box=F::NameTransform(nl,F::NameBox,naming.name_progress);
        assert(std::abs((t.x-F::NameTransform(nl,F::Keyboard).x)-
                        (box.x-F::NameTransform(nl,F::NameBox).x))<.001f);
        const auto moving_shadow=F::NameShadow(m,nl,naming.name_progress);
        const auto resting_shadow=F::NameShadow(m,nl,1);
        if(progress==0) {
            assert(!moving_shadow.count && !F::NameAtlas(m,0,nl,0).count && !F::NameAtlas(m,1,nl,0).count);
            const auto hidden=naming.ModelTransform(m,model);
            assert(hidden.sx==0 && hidden.sy==0);
        } else {
            assert(moving_shadow.count==resting_shadow.count && moving_shadow.count);
            assert(std::abs((moving_shadow.positions[0]-resting_shadow.positions[0])-
                            (t.x-F::NameTransform(nl,F::Keyboard).x))<.001f);
        }
        if(frame>7) {
            const float step=last_x-t.x;
            assert(step>0 && step<last_step); // Every 60 Hz frame moves, then slows.
            last_step=step;
        }
        assert(t.x<=last_x);last_x=t.x;
        naming.Resume(m,regs);
    }
    assert(F::NameEase(0)==0 && F::NameEase(1)==1);
    assert(F::NameEase(.1f)>.1f && F::NameEase(.9f)>.9f);
    // The name's glyph controllers and each cursor target follow their owner.
    naming.stage=F::Hook::Stage::Name;
    m.words[0x55be60]=controller;m.words[controller+0x10]=name;
    m.words[controller+0x40]=3;
    m.Float(name+0x3c,75);m.Float(name+0x40,10);
    auto nt=naming.ModelTransform(m,name);
    assert(nt.x==57 && nt.y==24 && nt.sx==.8f);
    // Parked or sliding native glyphs stay fixed in the visible name slots.
    for(float native_x:{400.f,275.f,175.f,75.f}) {
        m.Float(name+0x3c,native_x);
        nt=naming.ModelTransform(m,name);
        const auto stable=F::NameTransform(nl,F::NameBox);
        assert(std::abs(nt.x+nt.ax+(native_x-nt.ax)*nt.sx-
            (stable.x+stable.ax+(75-stable.ax)*stable.sx))<.001f);
    }
    // Keep the keyboard/name pose frozen while native saving parks and fades
    // the live geometry. Capturing must not write to guest buffers or config.
    {
        const auto memory=m.words;
        auto saving=std::make_unique<F::Hook>();
        m.words[F::NameOwner+4]=5;m.words[F::NameOwner+8]=2;
        m.words[F::NameOwner+0x24]=4;m.words[m.words[0x43047c]+0x4dc]=2;
        std::array<U32,16> r{};r[6]=F::NameOwner;
        const auto before=m.words;saving->NameDispatch(m,r,true);
        assert(m.words==before && saving->name_save.slot==2 && saving->saving_keyboard);
        const auto& frozen=*saving->saving_keyboard;
        assert(frozen.atlases[1].count && frozen.shadow.count && frozen.field.Valid());
        const auto expected=F::NameGlyphTransform(m,nl,controller,name,0,1);
        auto parked=native_names;
        for(unsigned i=0;i<parked.count*12;i+=3)parked.positions[i]+=400;
        for(unsigned i=3;i<parked.count*16;i+=4)parked.colors[i]=0;
        m.Floats(np+0x1000,parked.positions);m.Floats(nc+0x1000,parked.colors);
        m.words[F::NameOwner+4]=4;m.words[F::NameOwner+8]=5;
        const auto pose=F::NameGlyphTransform(m,frozen.layout,controller,name,0,1,&frozen.field);
        assert(std::abs(pose.x-expected.x)<.001f && std::abs(pose.y-expected.y)<.001f);
        saving->stage=F::Hook::Stage::Name;m.Float(material,.0f);
        saving->Material(m,material,name);assert(std::bit_cast<float>(*m.Read(material))==1);
        for(float progress:{0.f,.5f,1.f}) {
            saving->name_save.opacity=progress;
            auto atlas=frozen.atlases[1];saving->FadeSavingKeyboard(atlas);
            for(unsigned i=3;i<atlas.count*16;i+=4)
                assert(atlas.colors[i]==frozen.atlases[1].colors[i]*(1-progress));
            assert(atlas.positions==frozen.atlases[1].positions);
            saving->Material(m,material,name);
            assert(std::bit_cast<float>(*m.Read(material))==1-progress);
        }
        m.words[F::NameOwner+4]=1;saving->NameDispatch(m,r,true);
        assert(!saving->saving_keyboard && !saving->name_save.active);
        m.words=memory;
    }
    // Outlined letters use their central model as the common reference, so
    // all nine pieces retain their original one-pixel outline offsets.
    m.words[controller+0x40]=2;
    m.Float(name+8*0x200+0x3c,400);m.Float(name+8*0x200+0x40,10);
    for(unsigned j=0;j<9;++j) {
        const auto glyph=naming.ModelTransform(m,name+j*0x200);
        assert(std::abs(glyph.x-(57+(75-400)*.8f))<.001f && glyph.y==24);
    }
    // Kind 1 (the actual name-entry glyph path) stores screen positions in
    // streamed vertices, not model translation. Exercise all eight slots,
    // both buffers and both the foreground and one-pixel shadow.
    m.words[controller+0x40]=1;
    constexpr U32 glyph_mesh=0x19400000,glyph_vertices=0x19410000,glyph_view=0x19420000;
    m.Allocate(glyph_view,48);
    for(unsigned index=0;index<8;++index) {
        if(index)m.words[0x55be60+(index-1)*4]=0;
        m.words[0x55be60+index*4]=controller;
        for(float start:{75.f,175.f,400.f})for(unsigned buffer=0;buffer<2;++buffer)for(unsigned layer=0;layer<2;++layer) {
            const U32 glyph=name+layer*0x200,mesh=glyph_mesh+layer*0x1000;
            const U32 live=glyph_vertices+layer*0x1000+buffer*0x100;
            m.words[glyph+4]=mesh;m.words[mesh+0x124]=buffer;
            m.words[mesh+0x1a0+buffer*4]=live;
            m.Float(glyph+0x3c,0);m.Float(glyph+0x40,0);
            const float offset=layer==0?1.f:0.f,x=start+22*index+offset,y=10+offset;
            m.Float(live,x);m.Float(live+4,y);
            const auto glyph_transform=naming.ModelTransform(m,glyph);
            const std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
            m.Floats(glyph+0xc,identity);m.Floats(glyph_view,identity);
            assert(OOT3D::ScreenModels::Apply(m,glyph_view,glyph,glyph_transform));
            const auto read_float=[&](U32 address){return std::bit_cast<float>(*m.Read(address));};
            const float draw_x=read_float(glyph_view)*x+read_float(glyph_view+12);
            const float draw_y=read_float(glyph_view+20)*y+read_float(glyph_view+28);
            const auto field=F::Bounds(moved_names,6,6);
            assert(std::abs(draw_x-(field.left+40+(2+22*index+offset)*.8f))<.001f);
            // Only the slot backing is cropped to an underline; glyphs retain
            // their original placement within the full twenty-pixel field.
            assert(std::abs(field.bottom-field.top-2*.8f)<.001f);
            assert(std::abs(draw_y-(field.bottom-(20-2-offset)*.8f))<.001f);
        }
    }
    m.words[0x55be60+7*4]=0;m.words[0x55be60]=controller;
    m.words[controller+0x40]=3;
    m.words[F::NameOwner]=cursor;m.words[cursor+4]=cursor_model;
    m.words[F::NameOwner+0x14]=5;m.words[F::NameOwner+0x28]=1;
    nt=naming.ModelTransform(m,cursor_model);assert(nt.x==72 && nt.y==-12 && nt.sx==.75f);
    m.words[F::NameOwner+0x28]=0;
    nt=naming.ModelTransform(m,cursor_model);assert(nt.x==15 && nt.y==-20 && nt.sx==1.4f);
    m.words[F::NameOwner+0x14]=1;
    nt=naming.ModelTransform(m,cursor_model);assert(nt.x==-5 && nt.y==12 && nt.sx==1.25f);
    m.words[F::NameOwner+4]=5;
    assert(naming.ModelTransform(m,cursor_model).sx==0); // Native parked cursor cannot peek onscreen.
    m.words[F::NameOwner+4]=4;
    for(float width:{400.f,426.6667f})for(unsigned part:{F::Cancel,F::Okay,F::NameBox,F::Keyboard}) {
        const auto b=F::NameBounds[part];const auto t=F::NameTransform(nl,part);
        const float x=b.X()+t.x-40,y=b.Y()+t.y;
        const auto hit=F::MapPointer((x+(width-320)/2)/width,y/240,width);
        assert(hit && std::abs((*hit)[0]-b.X()/320)<.0001f && std::abs((*hit)[1]-b.Y()/240)<.0001f);
    }
    // Confirmation stones, text overlays and cursor share the new poses.
    nl.name_parts[F::ConfirmNo]={-12,5,.7f};nl.name_parts[F::ConfirmYes]={18,-7,1.3f};
    for(unsigned q=0;q<62;++q)for(unsigned v=0;v<4;++v) {
        const float native_x=std::bit_cast<float>(*m.Read(0x507864+q*8));
        const float native_y=std::bit_cast<float>(*m.Read(0x507868+q*8));
        const float w=std::bit_cast<float>(*m.Read(0x507a54+q*8));
        const float h=std::bit_cast<float>(*m.Read(0x507a58+q*8));
        m.Float(np+0x1000+q*48+v*12,native_x+(v&1?w:0)+
            ((q>=16 && q<=33) || (q>=40 && q<=45)?0:400));
        m.Float(np+0x1000+q*48+v*12+4,native_y+(v&2?h:0));
    }
    m.words[F::NameOwner+8]=4;m.Float(name+0x3c,400);m.Float(name+0x40,40);
    naming.frame_layout=nl;
    nt=naming.ModelTransform(m,name);
    assert(std::abs(nt.x-(57+(75-400)*.8f))<.001f && nt.y==24);
    const auto name_confirm=F::NameAtlas(m,1,nl,1);
    assert(name_confirm.count==24); // 10 prompt pieces + 7 pieces per stone.
    assert(std::abs(F::Bounds(name_confirm,10,12).X()-(96-12))<.001f);
    assert(std::abs(F::Bounds(name_confirm,10,12).right-F::Bounds(name_confirm,10,12).left-124*.7f)<.001f);
    m.words[F::NameOwner+0x2c]=0;
    assert(naming.ModelTransform(m,cursor_model).sx==.7f);
    m.words[F::NameOwner+0x2c]=1;
    assert(naming.ModelTransform(m,cursor_model).sx==1.3f);
    F::touch_layout=nl;F::name_touch_confirmation=true;
    for(float width:{400.f,426.6667f})for(unsigned part:{F::ConfirmNo,F::ConfirmYes}) {
        const auto b=F::NameBounds[part];const auto t=F::NameTransform(nl,part);
        const auto hit=F::MapPointer((b.X()+t.x-40+(width-320)/2)/width,(b.Y()+t.y)/240,width);
        assert(hit && std::abs((*hit)[0]-b.X()/320)<.0001f && std::abs((*hit)[1]-b.Y()/240)<.0001f);
    }
    for(unsigned q:{7u,25u})for(unsigned v=0;v<4;++v)m.Float(np+0x1000+q*48+v*12,500.f+(v&1?174:0));
    assert(naming.ModelTransform(m,name).sx==0); // Neither name field is on screen.
    // Borrowed native atlas call preserves the original BLX fallback.
    F::Hook fallback;regs[4]=0;regs[1]=0x3f9680;
    fallback.NameSubmit(m,regs);
    assert(regs[15]==0x3f9680 && regs[14]==F::Sites[6].pc+4);
    m.words[nh+0x10]=0;
    regs[4]=0;regs[1]=0x3f9680;
    naming.NameSubmit(m,regs);assert(regs[15]==0x3f9680);
    std::cout<<"File-select routing, panels, compressed texture, layer order, prompts and guest-call checks passed\n";
}
