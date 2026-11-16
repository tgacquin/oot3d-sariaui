// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/screen_model_hooks.h"
#include <cassert>
#include <iostream>
#include <map>
#include <fstream>
#include <cstring>
#include <filesystem>
using namespace OOT3D;
using TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    template<std::size_t N> void Floats(U32 a,const std::array<float,N>& v) {
        for(unsigned i=0;i<N;++i)words[a+4*i]=std::bit_cast<U32>(v[i]);
    }
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
bool Near(float a,float b) {return std::abs(a-b)<.001f;}
int main(int argc,char** argv) {
    assert(argc==1 || argc==2);
    Memory m;
    constexpr U32 model=0x8000000,helper=0x8100000,mesh=0x8200000,source=0x8300000,
        offset=0x8400000,arrow_helper=0x8500000,arrow_vertices=0x8600000,
        marker=0x8700000,marker_model=0x8800000,view=0x8900000;
    constexpr std::array<float,12> identity{1,0,0,0,0,1,0,0,0,0,1,0};
    m.words[MinimapControl::Owner+8]=model;m.words[MinimapControl::Owner+0x18]=helper;
    m.words[MinimapControl::Owner+4]=mesh;m.words[model]=0x4ebd60;m.words[model+4]=mesh;
    m.words[helper]=1;m.words[helper+0xc]=source;m.words[helper+0x1c]=offset;
    m.words[MinimapControl::Owner+0x20]=marker;m.words[marker+4]=marker_model;
    m.words[marker_model]=0x4ebd60;
    m.words[HUD::Action+0x18]=arrow_helper;m.words[arrow_helper]=8;m.words[arrow_helper+0x10]=arrow_vertices;
    m.Floats(model+0xc,identity);m.Floats(marker_model+0xc,identity);
    const auto quad=[&](U32 at,float x,float y,float w,float h) {
        m.Floats(at,std::array{x,y,0.f,x+w,y,0.f,x,y+h,0.f,x+w,y+h,0.f});
    };
    HUD::Layout l;HUD::RoutingState state;std::array<U32,16> regs{};
    // Simulate different room/overworld bounds and native per-area offsets.
    // Reuse the owner to catch stale bounds after a room change or state load.
    for(const auto b:std::array<std::array<float,6>,4>{{
        {0,0,128,128,-16,104},{0,0,128,128,4,108},
        {-20,90,64,96,10,8},{5,120,96,80,-12,12}}})
    for(float scale:{.1f,.9f,1.f,3.f})for(float x:{-400.f,324.7f,400.f}) {
        quad(source,b[0],b[1],b[2],b[3]);m.Floats(offset,std::array{b[4],b[5]});
        const float right=b[0]+b[2]+b[4],bottom=b[1]+b[3]+b[5];
        const auto corner=MinimapControl::NativeCorner(m);assert(corner);
        assert(Near((*corner)[0],right) && Near((*corner)[1],bottom));
        l[HUD::Minimap]={x,-11,scale};HUD::SetLayout(l,false);
        const auto before=m.words;
        for(U32 owner:{model,marker_model}) {
            const auto t=ScreenModels::Select(m,owner,{},l[HUD::Minimap]);assert(t);
            m.Floats(view,identity);assert(ScreenModels::Apply(m,view,owner,*t));
            const float actual_x=m.F(view)*right+m.F(view+12);
            const float actual_y=m.F(view+20)*bottom+m.F(view+28);
            assert(Near(actual_x,128+x) && Near(actual_y,232-11));
            // A marker's relative distance to the corner scales uniformly.
            assert(Near(m.F(view)*(right-13)+m.F(view+12),128+x-13*scale));
        }
        for(unsigned i=0;i<48;i+=4)assert(m.words.at(source+i)==before.at(source+i));
        for(unsigned q=0;q<8;++q)quad(arrow_vertices+q*48,right-20,bottom-20,8,8);
        const auto original=m.words;
        regs[4]=HUD::Action;HUD::Handle(m,regs,1,l,state);
        for(unsigned q:{2u,3u}) {
            assert(Near(m.F(arrow_vertices+q*48),128+x-20*scale));
            assert(Near(m.F(arrow_vertices+q*48+4),232-11-20*scale));
        }
        // Adjacent A button and special B bubble remain independent.
        for(unsigned q:{0u,1u,7u})for(unsigned i=0;i<48;i+=4)
            assert(m.words.at(arrow_vertices+q*48+i)==original.at(arrow_vertices+q*48+i));
        l.minimap_lock_corner=false;HUD::SetLayout(l,false);
        const auto unlocked=ScreenModels::Select(m,model,{},l[HUD::Minimap]);assert(unlocked);
        assert(unlocked->x==x && unlocked->y==-11 && unlocked->ax==40 && unlocked->ay==204);
        l.minimap_lock_corner=true;
    }
    // Missing/unloaded/parked/degenerate maps do not produce a new anchor.
    m.words[helper]=0;assert(!MinimapControl::NativeCorner(m));m.words[helper]=1;
    m.Floats(offset,std::array{0.f,0.f});
    for(auto b:std::array<std::array<float,4>,4>{{{400,0,64,64},{0,240,64,64},{0,0,0,64},{0,0,64,0}}}) {
        quad(source,b[0],b[1],b[2],b[3]);assert(!MinimapControl::NativeCorner(m));
    }
    // A 128px quad is not the visible map. Two equally sized native textures
    // with different transparent margins must land on the SAME visible edge.
    constexpr U32 texture=0x14000000,coords=0x8a00000;
    m.words[mesh+0x158]=texture;m.words[mesh+0x15c]=128|(128u<<16);
    m.words[mesh+0x164]=13;m.words[helper+0x14]=coords;
    quad(source,0,0,128,128);m.Floats(offset,std::array{-16.f,104.f});
    const auto upload=[&](const MinimapControl::AlphaBlocks& alpha) {
        for(unsigned i=0;i<alpha.size();++i) {
            m.words[texture+16*i]=U32(alpha[i]);m.words[texture+16*i+4]=U32(alpha[i]>>32);
        }
    };
    const auto check_edges=[&](MinimapControl::TextureBounds pixels) {
        for(bool mirror:{false,true})for(float scale:{.1f,.9f,3.f}) {
            m.Floats(coords,std::array{mirror?1.f:0.f,1.f,mirror?0.f:1.f,1.f,
                                      mirror?1.f:0.f,0.f,mirror?0.f:1.f,0.f});
            const float right=-16+(mirror?128-pixels.left:pixels.right),bottom=104+pixels.top;
            const auto corner=MinimapControl::NativeCorner(m);assert(corner);
            assert(Near((*corner)[0],right) && Near((*corner)[1],bottom));
            l[HUD::Minimap]={220,-11,scale};l.minimap_lock_corner=true;HUD::SetLayout(l,false);
            const auto original=m.words;
            for(U32 owner:{model,marker_model}) {
                const auto t=ScreenModels::Select(m,owner,{},l[HUD::Minimap]);assert(t);
                m.Floats(view,identity);assert(ScreenModels::Apply(m,view,owner,*t));
                assert(Near(m.F(view)*right+m.F(view+12),348));
                assert(Near(m.F(view+20)*bottom+m.F(view+28),221));
            }
            for(unsigned q=0;q<8;++q)quad(arrow_vertices+q*48,right-20,bottom-20,8,8);
            regs[4]=HUD::Action;HUD::Handle(m,regs,1,l,state);
            for(unsigned q:{2u,3u}) {
                assert(Near(m.F(arrow_vertices+q*48),348-20*scale));
                assert(Near(m.F(arrow_vertices+q*48+4),221-20*scale));
            }
            for(unsigned i=0;i<48;i+=4)assert(m.words.at(source+i)==original.at(source+i));
            for(unsigned i=0;i<1024;++i)for(unsigned word:{0u,4u})
                assert(m.words.at(texture+16*i+word)==original.at(texture+16*i+word));
        }
    };
    for(auto box:std::array<std::array<unsigned,4>,3>{{{3,4,95,62},{6,0,64,56},{11,3,80,71}}}) {
        MinimapControl::AlphaBlocks alpha{};
        for(unsigned y=box[1];y<box[3];++y)for(unsigned x=box[0];x<box[2];++x) {
            const auto block=((y/8)*16+x/8)*4+(y%8)/4*2+(x%8)/4;
            alpha[block]|=std::uint64_t(7)<<(4*(4*(x%4)+y%4));
        }
        upload(alpha);check_edges({float(box[0]),float(box[1]),float(box[2]),float(box[3])});
    }
    // Cropped UVs anchor only the visible portion being sampled. PICA flips
    // encoded rows when sampling UVs; encoded row zero appears at screen top.
    m.Floats(coords,std::array{0.f,1.f,.5f,1.f,0.f,0.f,.5f,0.f});
    auto cropped=MinimapControl::NativeCorner(m);assert(cropped);
    assert(Near((*cropped)[0],112) && Near((*cropped)[1],175));
    // Vertical crop: sample just the top half of the texture, stretching that
    // half over the full geometry. This also catches flipping only the output
    // bounds without flipping the crop before scanning the alpha blocks.
    m.Floats(coords,std::array{0.f,1.f,1.f,1.f,0.f,.5f,1.f,.5f});
    cropped=MinimapControl::NativeCorner(m);assert(cropped);
    assert(Near((*cropped)[0],64) && Near((*cropped)[1],232));
    // Real game textures reproduce the padding difference, not just synthetic
    // quads. Read only the user's own local assets; no assets enter the mod.
    if(argc==2)for(auto entry:std::array<std::pair<const char*,MinimapControl::TextureBounds>,3>{{
            {"h_fm_04_txt.ctxb",{3,0,95,62}},{"h_fm_10_txt.ctxb",{6,0,64,56}},
            {"h_fm_00_txt.ctxb",{11,3,80,71}}}}) {
        std::ifstream file(std::string(argv[1])+"/menu/map/fm/"+entry.first,std::ios::binary);assert(file);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
        assert(bytes.size()==72+16384);
        const auto word=[&](unsigned at){U32 v;std::memcpy(&v,bytes.data()+at,4);return v;};
        assert(word(0)==0x62787463 && word(20)==72 && word(44)==(128|(128u<<16)) && word(48)==0x675b);
        MinimapControl::AlphaBlocks alpha;
        for(unsigned i=0;i<alpha.size();++i)alpha[i]=word(72+16*i)|(std::uint64_t(word(76+16*i))<<32);
        upload(alpha);check_edges(entry.second);
    }
    // Dungeon rooms use RGBA4 as well as ETC1A4. Exercise the real Forest
    // Temple room art, including the faint rectangular panel baked into it.
    if(argc==2)for(auto entry:std::array<std::pair<const char*,MinimapControl::TextureBounds>,3>{{
            {"bmori_07_1_txt.ctxb",{32,21,96,85}},
            {"bmori_07_2_txt.ctxb",{32,21,96,85}},
            {"bmori_10_txt.ctxb",{58,47,96,85}}}}) {
        std::ifstream file(std::string(argv[1])+"/menu/map/Bmori/Bmori_i/"+entry.first,std::ios::binary);assert(file);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
        assert(bytes.size()==72+32768);
        U32 format;std::memcpy(&format,bytes.data()+48,4);assert(format==0x80336752);
        m.words[mesh+0x164]=4;
        for(unsigned i=0;i<32768;i+=4)std::memcpy(&m.words[texture+i],bytes.data()+72+i,4);
        check_edges(entry.second);
    }
    // The reported Water Temple basement corridor has a stray opaque red
    // corner pixel. Its panel must match the upstairs variant's placement.
    if(argc==2)for(const char* name:{"mizusin_03_b1_txt.ctxb","mizusin_03_1_txt.ctxb"}) {
        std::ifstream input(std::string(argv[1])+"/menu/map/MIZUsin/mizusin_i/"+name,std::ios::binary);assert(input);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
        assert(bytes.size()==72+32768);
        m.words[mesh+0x164]=4;
        for(unsigned i=0;i<32768;i+=4)std::memcpy(&m.words[texture+i],bytes.data()+72+i,4);
        check_edges({23,53,96,85});
    }
    // Only isolated opaque red corner markers are excluded. Thin lines,
    // other colors, and red pixels connected to the map remain eligible.
    {
        MinimapControl::RgbaWords words{};
        words.back()=0xf00f0000u;
        assert(MinimapControl::IsCornerMarker(words,127,127));
        MinimapControl::RgbaCache cache;
        assert(!MinimapControl::VisibleBounds(cache.Observe(words),{0,0,1,1}));
        words.back()=0xffff0000u;
        assert(!MinimapControl::IsCornerMarker(words,127,127));
        assert(MinimapControl::VisibleBounds(cache.Observe(words),{0,0,1,1}));
        words.back()=0xf00f0001u; // Neighbor at (126,127) joins the artwork.
        assert(!MinimapControl::IsCornerMarker(words,127,127));
        assert(MinimapControl::VisibleBounds(cache.Observe(words),{0,0,1,1}));
    }
    // Broaden the real-asset regression to every 128x128 RGBA4 room map.
    // Independently decode its Morton coordinates from linear texel indices,
    // then check the final corner under native offsets and a model transform.
    unsigned dungeon_maps=0;
    if(argc==2)for(const auto& file:std::filesystem::recursive_directory_iterator(
            std::filesystem::path(argv[1])/"menu/map")) {
        if(!file.is_regular_file() || !file.path().filename().string().ends_with("_txt.ctxb"))continue;
        std::ifstream input(file.path(),std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
        if(bytes.size()!=72+32768)continue;
        U32 dimensions,format;
        std::memcpy(&dimensions,bytes.data()+44,4);std::memcpy(&format,bytes.data()+48,4);
        if(dimensions!=(128|(128u<<16)) || format!=0x80336752)continue;
        int right=0,bottom=0;
        for(unsigned pixel=0;pixel<16384;++pixel) {
            if(!(bytes[72+pixel*2]&15))continue;
            unsigned x=(pixel/64)%16*8,y=(pixel/64)/16*8;
            for(unsigned bit=0;bit<3;++bit) {
                x|=((pixel>>(bit*2))&1)<<bit;y|=((pixel>>(bit*2+1))&1)<<bit;
            }
            right=std::max(right,int(x)+1);bottom=std::max(bottom,int(y)+1);
        }
        if(file.path().filename()=="mizusin_03_b1_txt.ctxb") {
            // Independently measured panel extent; the raw alpha scan above
            // deliberately includes the extra red pixel and reaches 128x128.
            assert(right==128 && bottom==128);
            right=96;bottom=85;
        }
        if(!right || !bottom)continue;
        m.words[mesh+0x164]=4;
        for(unsigned i=0;i<32768;i+=4)std::memcpy(&m.words[texture+i],bytes.data()+72+i,4);
        m.Floats(coords,std::array{0.f,1.f,1.f,1.f,0.f,0.f,1.f,0.f});
        quad(source,-51,150,128,128);m.Floats(offset,std::array{3.f,-4.f});
        const std::array<float,12> model_matrix{.8f,0,0,10,0,.8f,0,20,0,0,1,0};
        m.Floats(model+0xc,model_matrix);
        const float cx=.8f*(right-48)+10,cy=.8f*(bottom+146)+20;
        const auto corner=MinimapControl::NativeCorner(m);assert(corner);
        assert(Near((*corner)[0],cx) && Near((*corner)[1],cy));
        l[HUD::Minimap]={264,0,.9f};HUD::SetLayout(l,false);
        const auto t=ScreenModels::Select(m,model,{},l[HUD::Minimap]);assert(t);
        m.Floats(view,identity);assert(ScreenModels::Apply(m,view,model,*t));
        assert(Near(m.F(view)*cx+m.F(view+12),392));
        assert(Near(m.F(view+20)*cy+m.F(view+28),232));
        ++dungeon_maps;
    }
    if(argc==2)assert(dungeon_maps>100);
    // A missing RGBA4 word cannot reuse the previous room's alpha bounds.
    m.words[mesh+0x164]=4;m.words.erase(texture);
    assert(!MinimapControl::TextureEdges(m,mesh,{0,1,1,1,0,0,1,0}));
    m.words[mesh+0x164]=13;
    m.Floats(model+0xc,identity);quad(source,0,0,128,128);
    m.Floats(offset,std::array{-16.f,104.f});
    upload({});
    // Switch between a dungeon and the field without sharing offsets or scale.
    // Model art, markers and action-atlas arrows must pick the same profile.
    l[HUD::Minimap]={120,-24,.5f};l.outside_minimap={240,-35,1.4f};HUD::SetLayout(l,false);
    for(U32 kind:{1u,0u,1u,0u}) {
        m.words[MinimapControl::Owner+0x50]=kind;
        const auto pose=MinimapControl::LayoutPose(m,l);
        const auto expected=kind?l[HUD::Minimap]:l.outside_minimap;
        assert(pose.x==expected.x && pose.y==expected.y && pose.scale==expected.scale);
        const auto corner=MinimapControl::NativeCorner(m);assert(corner);
        for(U32 owner:{model,marker_model}) {
            const auto t=ScreenModels::Select(m,owner,{},pose);assert(t);
            m.Floats(view,identity);assert(ScreenModels::Apply(m,view,owner,*t));
            assert(Near(m.F(view)*(*corner)[0]+m.F(view+12),128+pose.x));
            assert(Near(m.F(view+20)*(*corner)[1]+m.F(view+28),232+pose.y));
        }
        for(unsigned q=0;q<8;++q)quad(arrow_vertices+q*48,(*corner)[0]-20,(*corner)[1]-20,8,8);
        regs[4]=HUD::Action;HUD::Handle(m,regs,1,l,state);
        for(unsigned q:{2u,3u}) {
            assert(Near(m.F(arrow_vertices+q*48),128+pose.x-20*pose.scale));
            assert(Near(m.F(arrow_vertices+q*48+4),232+pose.y-20*pose.scale));
        }
    }
    // The user's pre-trim position (432.6,271.2) put narrow field maps off-screen.
    HUD::Pose legacy{304.6f,39.2f,.9f};HUD::RecoverLegacyMinimap(legacy,true);
    assert(legacy.x==264 && legacy.y==0 && legacy.scale==.9f);
    HUD::Pose existing{250,-15,1.3f};HUD::RecoverLegacyMinimap(existing,true);
    assert(existing.x==250 && existing.y==-15 && existing.scale==1.3f);
    HUD::Pose unlocked{304.6f,39.2f,.9f};HUD::RecoverLegacyMinimap(unlocked,false);
    assert(unlocked.x==304.6f && unlocked.y==39.2f);
    HUD::MinimapBottomRight(existing);assert(existing.x==264 && existing.y==0 && existing.scale==1.3f);
    // Empty/replaced/unreadable textures cannot reuse the previous area's edge.
    upload({});auto empty=MinimapControl::NativeCorner(m);assert(empty);
    assert(Near((*empty)[0],112) && Near((*empty)[1],232));
    m.words[mesh+0x164]=0;assert(MinimapControl::NativeCorner(m)==empty);
    m.words[mesh+0x164]=13;m.words.erase(texture);assert(MinimapControl::NativeCorner(m)==empty);
    m.words[MinimapControl::Owner+8]=0;
    const auto fallback=MinimapControl::Place(m,{13,21,.8f},true);
    assert(fallback.x==13 && fallback.y==21 && fallback.ax==40 && fallback.ay==204);
    HUD::SetLayout({},false);
    std::cout<<"Minimap corner: ETC1A4/RGBA4 edges, "<<dungeon_maps<<" real dungeon maps, mirrored/cropped UVs, area changes, native offsets, scale, markers, arrows, source preservation and unavailable-map fallback passed\n";
}
