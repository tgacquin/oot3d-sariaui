// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hd_boss_timer_font.h"
#include "video_core/texture/etc1.h"
#include <cassert>
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    using namespace OOT3D::HDFont;
    unsigned language=0;
    for(const auto name:{"english","french","spanish"}) {
        const auto base=std::string(argv[1])+"/misc/us/"+name+"/";
        std::ifstream layout(base+"challenge.qsp",std::ios::binary);assert(layout);
        const std::vector<std::uint8_t> qsp((std::istreambuf_iterator<char>(layout)),{});
        assert(qsp.size()>=211*40);
        const auto value=[&](unsigned p){return std::bit_cast<float>(OOT3D::MapFrame::LE32(qsp,210*40+p));};
        assert(value(20)==336 && value(24)==208 && value(28)==12 && value(32)==21);
        std::ifstream input(base+"challenge00.ctxb",std::ios::binary);assert(input);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        const auto decode=[](std::uint64_t v,unsigned x,unsigned y){return Pica::Texture::SampleETC1Subtile(v,x,y);};
        auto source=DecodeBossTimerAtlas(bytes,decode);assert(source);
        assert(!DecodeBossTimerAtlas({},decode));
        bytes[48]^=1;assert(!DecodeBossTimerAtlas(bytes,decode));
        source->hash=++language;
        boss_timer_atlases[language-1].store(std::shared_ptr<const RupeeAtlas>(source));
        const auto atlas=StyledBossTimerAtlas(source->hash,{});assert(atlas);
        assert(atlas==StyledBossTimerAtlas(source->hash,{}));
        const auto bold=StyledBossTimerAtlas(source->hash,{},true);
        assert(bold && bold->rgba.size()==atlas->rgba.size() && bold->rgba!=atlas->rgba);
        assert(bold->variant!=atlas->variant && bold==StyledBossTimerAtlas(source->hash,{},true));
        assert(StyledBossTimerAtlas(source->hash,{},false)->rgba==atlas->rgba);
        const auto& out=atlas->rgba;assert(out.size()==512*256*64);
        // Every pixel outside the twelve timer cells retains its exact color,
        // alpha and orientation, including labels and the challenge panel.
        for(unsigned y=0;y<1024;++y)for(unsigned x=0;x<2048;++x) {
            const unsigned nx=x/4,ny=y/4;
            if((ny>=208 && ny<229 && nx>=336 && nx<456) ||
               (ny>=232 && ny<253 && nx>=336 && nx<360))continue;
            for(unsigned c=0;c<4;++c)
                assert(out[((1023-y)*2048+x)*4+c]==source->native[(ny*512+nx)*4+c]);
        }
        for(unsigned d=0;d<12;++d) {
            unsigned coverage=0,antialias=0;
            const unsigned cx=336+(d<10?d:d-10)*12,cy=d<10?208:232;
            for(unsigned y=0;y<84;++y)for(unsigned x=0;x<48;++x) {
                const auto p=((1023-(cy*4+y))*2048+cx*4+x)*4;
                if(out[p]>128 && out[p+3]) {++coverage;antialias+=out[p+3]<255;}
            }
            assert(coverage>20 && antialias>10); // Includes both punctuation marks.
        }
        RupeeShadow shadow;shadow.enabled=false;
        const auto plain=StyledBossTimerAtlas(source->hash,shadow);
        assert(plain && plain->rgba!=out && plain->variant!=atlas->variant);
        assert(!StyledBossTimerAtlas(0,{}));
        shadow.blur=3;assert(MakeBossTimerAtlas(source->native,shadow).empty());
        assert(MakeBossTimerAtlas({}).empty());
        if(argc==3 && language==1) {
            std::ofstream image(argv[2],std::ios::binary);
            image.write(reinterpret_cast<const char*>(out.data()),out.size());assert(image);
        }
    }
    std::cout<<"HD boss timer: all languages, digits/punctuation, untouched art, shadows, hash isolation and invalid assets passed\n";
}
