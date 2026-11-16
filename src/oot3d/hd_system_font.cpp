// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hd_font.h"
#include "oot3d/hd_system_font_data.h"

namespace OOT3D::HDFont {
namespace {
constexpr unsigned W=8,H=16,HDW=W*Scale,HDH=H*Scale;
struct Glyph {
    std::array<std::uint8_t,W*H> native;
    std::array<std::uint8_t,HDW*HDH*4> rgba;
};
using Face=std::vector<Glyph>;
std::shared_ptr<const Face> face;
std::atomic<std::uint64_t> system_generation{0};
std::uint64_t Hash(std::span<const std::uint8_t> bytes) {
    std::uint64_t hash=14695981039346656037ULL;
    for(auto b:bytes)hash=(hash^b)*1099511628211ULL;
    return hash;
}
}
std::uint64_t SystemArtworkGeneration() {return system_generation.load();}
void ClearSystemArtwork() {std::atomic_store(&face,std::shared_ptr<const Face>{});++system_generation;}
bool LoadSystemArtwork(std::span<const std::uint8_t> qbf) {
    ClearSystemArtwork();
    if(qbf.size()<16 || qbf[0]!='Q' || qbf[1]!='B' || qbf[2]!='F' || qbf[3]!='1' ||
       qbf[12]!=8 || qbf[13]!=W || qbf[14]!=H || qbf[15]!=4)return false;
    const auto word=[&](unsigned at) {return unsigned(qbf[at])|(unsigned(qbf[at+1])<<8);};
    const unsigned count=word(4),sheets=word(6),bitmap=16+count*8;
    if(!count || count>4096 || !sheets || sheets>4096 || qbf.size()!=bitmap+sheets*W*H)return false;
    auto result=std::make_shared<Face>();
    for(unsigned i=0;i<count;++i) {
        const unsigned at=16+i*8,index=word(at+2);
        if(index>=sheets || qbf[at+4]+qbf[at+5]>W)return false;
        for(const auto& record:SystemArtwork::Records) {
            if(record.code!=word(at) || record.left!=qbf[at+4] || record.width!=qbf[at+5])continue;
            const auto pixels=qbf.subspan(bitmap+index*W*H,W*H);
            if(Hash(pixels)!=record.native_hash)continue; // Modified fonts stay native.
            Glyph glyph{};std::copy(pixels.begin(),pixels.end(),glyph.native.begin());
            if(record.offset+record.size>std::size(SystemArtwork::Runs) || record.size%5)return false;
            unsigned pen=0;
            for(unsigned p=record.offset;p<record.offset+record.size;p+=5) {
                const unsigned n=SystemArtwork::Runs[p];
                if(!n || pen+n>HDW*HDH)return false;
                for(unsigned j=0;j<n;++j,++pen)
                    std::copy_n(SystemArtwork::Runs+p+1,4,glyph.rgba.begin()+pen*4);
            }
            if(pen!=HDW*HDH)return false;
            result->push_back(std::move(glyph));break;
        }
    }
    if(result->empty())return false;
    std::atomic_store(&face,std::shared_ptr<const Face>(std::move(result)));
    ++system_generation;
    return true;
}

std::vector<std::uint8_t> RedrawSystem(std::span<const std::uint8_t> ia4,unsigned width,unsigned height) {
    const auto current=std::atomic_load(&face);
    if(!current || width<W || height<H || width>512 || height>512 ||
       width%8 || height%8 || ia4.size()!=width*height)return {};
    struct Match {unsigned x,y,index;};
    std::vector<Match> matches;
    std::array<std::uint8_t,W*H> cell;
    for(unsigned y=0;y+H<=height;y+=8)for(unsigned x=0;x+W<=width;x+=8) {
        for(unsigned row=0;row<H;++row)
            std::copy_n(ia4.begin()+(y+H-1-row)*width+x,W,cell.begin()+row*W);
        for(unsigned i=0;i<current->size();++i)if((*current)[i].native==cell) {
            matches.push_back({x,y,i});break;
        }
    }
    if(matches.empty())return {};
    const unsigned wide=width*Scale,high=height*Scale;
    std::vector<std::uint8_t> out(wide*high*4);
    // Preserve unsupported symbols with bilinear, premultiplied interpolation.
    // Both luminance and alpha matter: IA4 contains the native dark outline.
    const auto sample=[&](int x,int y) {
        const auto b=ia4[std::clamp(y,0,int(height)-1)*width+std::clamp(x,0,int(width)-1)];
        const float a=(b&15)*17.f;
        return std::array<float,2>{(b>>4)*17.f*a/255.f,a};
    };
    for(unsigned y=0;y<high;++y)for(unsigned x=0;x<wide;++x) {
        const float sx=(x+.5f)/Scale-.5f,sy=(y+.5f)/Scale-.5f;
        const int ix=int(std::floor(sx)),iy=int(std::floor(sy));
        const auto a=sample(ix,iy),b=sample(ix+1,iy),c=sample(ix,iy+1),d=sample(ix+1,iy+1);
        std::array<float,2> value;
        for(unsigned k=0;k<2;++k)value[k]=std::lerp(std::lerp(a[k],b[k],sx-ix),std::lerp(c[k],d[k],sx-ix),sy-iy);
        const unsigned p=(y*wide+x)*4;
        out[p]=out[p+1]=out[p+2]=value[1]>0?std::uint8_t(std::clamp(value[0]*255/value[1],0.f,255.f)+.5f):255;
        out[p+3]=std::uint8_t(value[1]+.5f);
    }
    for(const auto& match:matches) {
        const auto& rgba=(*current)[match.index].rgba;
        for(unsigned y=0;y<HDH;++y)
            std::copy_n(rgba.begin()+(HDH-1-y)*HDW*4,HDW*4,
                        out.begin()+((match.y*Scale+y)*wide+match.x*Scale)*4);
    }
    redrawn_glyphs+=unsigned(matches.size());
    return out;
}
}
