// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
namespace F=OOT3D::FileSelect;
using U32=std::uint32_t;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) const {
        const auto it=words.find(at);
        return it==words.end()?std::nullopt:std::optional<U32>{it->second};
    }
    template<class T> void Floats(U32 at,const T& a) {
        for(unsigned i=0;i<a.size();++i)words[at+4*i]=std::bit_cast<U32>(a[i]);
    }
};
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream input(argv[1],std::ios::binary);assert(input);
    const std::vector<std::uint8_t> code{std::istreambuf_iterator<char>(input),{}};
    const auto value=[&](U32 at) {return std::bit_cast<float>(OOT3D::MapFrame::LE32(code,at-0x100000));};
    // Exercise the actual ROM rectangles, including mirrored sampling. The
    // five faces must sample only their own interior without moving a vertex.
    for(unsigned q:{125u,129u,133u,137u,144u})for(bool mirror:{false,true}) {
        const float x=value(0x506138+q*8),y=value(0x506138+q*8+4);
        const float w=value(0x505b78+q*8),h=value(0x505b78+q*8+4);
        F::Geometry g;g.Add(10,20,100,40,mirror?(x+w)/512:x/512,y/256,
            mirror?x/512:(x+w)/512,(y+h)/256);
        const auto positions=g.positions;const auto colors=g.colors;
        F::StoneFaceUVs(g,0);assert(g.positions==positions && g.colors==colors);
        for(unsigned v=0;v<4;++v) {
            assert(g.uvs[v*2]>=(x+.5f)/512 && g.uvs[v*2]<=(x+w-.5f)/512);
            assert(g.uvs[v*2+1]>=(y+.5f)/256 && g.uvs[v*2+1]<=(y+h-.5f)/256);
        }
    }
    F::Vignette settings;
    const auto base=F::CornerVignette(settings,true);
    settings.bottom_right_boost=4;
    const auto boosted=F::CornerVignette(settings,true);
    assert(base.count==boosted.count && base.positions==boosted.positions);
    bool darker=false;
    for(unsigned i=3;i<base.count*16;i+=4) {
        if(i<3*144*16)assert(base.colors[i]==boosted.colors[i]);
        else {assert(boosted.colors[i]>=base.colors[i] && boosted.colors[i]<=1);darker|=boosted.colors[i]>base.colors[i];}
    }
    assert(darker);
    settings.enabled=false;assert(!F::CornerVignette(settings,true).count);
    Memory m;
    constexpr U32 helper=0x8000000,positions=0x8010000,uvs=0x8020000,colors=0x8030000;
    m.words[F::NameOwner+0x70]=helper;m.words[helper]=62;
    m.words[helper+0x10]=positions;m.words[helper+0x14]=uvs;m.words[helper+0x18]=colors;
    m.words[F::NameOwner+4]=4;m.words[F::NameOwner+0x40]=1;
    for(unsigned mode:{2u,3u})for(unsigned slot=0;slot<8;++slot) {
        m.words[F::NameOwner+8]=mode;
        F::NameGeometry raw;
        for(unsigned q=0;q<62;++q) {
            const bool field=q==7 || q==25,cursor=q==46;
            raw.Add(field?73.f:cursor?73.f+22*slot:0,field||cursor?10:0,
                field?176:cursor?20:0,field||cursor?20:0,0,0,1,1);
            for(unsigned v=0;v<4;++v)raw.colors[q*16+v*4+3]=.7f;
        }
        m.Floats(positions,raw.positions);m.Floats(uvs,raw.uvs);m.Floats(colors,raw.colors);
        const auto untouched=m.words;
        bool valid=false;const auto g=F::NameAtlas(m,1,{},1,&valid);
        assert(valid && g.count==1 && m.words==untouched);
        const auto b=F::Bounds(g,0,0);
        assert(b.left==73+22*slot && b.right-b.left==20 && std::abs(b.bottom-b.top-2)<.001f);
        assert(g.colors[3]==.7f); // Preserve the native cursor pulse.
    }
    std::cout<<"Native stone face UVs, isolated bottom-right vignette boost and single typing underline passed\n";
}
