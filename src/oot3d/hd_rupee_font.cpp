// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hd_rupee_font.h"
#include "oot3d/hd_number_glyphs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>

namespace OOT3D::HDFont {
std::shared_ptr<const RupeeAtlas> StyledRupeeAtlas(RupeeShadow shadow,bool timer,bool backdrop,bool bold) {
    const auto source=rupee_atlas.load();
    if(!source || (!bold && !timer && !backdrop && shadow==RupeeShadow{}))return source;
    static std::mutex mutex;
    static std::array<std::shared_ptr<const RupeeAtlas>,4> previous,result;
    static std::array<RupeeShadow,4> style;
    static std::array<bool,4> weight{};
    static std::uint64_t variant=0;
    const unsigned slot=unsigned(timer)+2*unsigned(backdrop);
    std::scoped_lock lock{mutex};
    if(previous[slot]==source && style[slot]==shadow && weight[slot]==bold)return result[slot];
    auto next=std::make_shared<RupeeAtlas>();
    next->rgba=MakeRupeeAtlas(source->native,shadow,timer,backdrop,bold);
    if(next->rgba.empty())return source;
    next->hash=source->hash;next->variant=++variant;
    previous[slot]=source;style[slot]=shadow;weight[slot]=bold;result[slot]=std::move(next);
    return result[slot];
}
std::vector<std::uint8_t> MakeRupeeAtlas(std::span<const std::uint8_t> bytes,RupeeShadow style,bool timer,bool backdrop,bool bold) {
    if(!std::isfinite(style.x) || !std::isfinite(style.y) || !std::isfinite(style.blur) ||
       !std::isfinite(style.strength) || std::abs(style.x)>3 || std::abs(style.y)>3 ||
       style.blur<0 || style.blur>2 || style.strength<0 || style.strength>1)return {};
    constexpr unsigned W=256,H=128,S=4;
    const auto word=[&](unsigned p) {
        return unsigned(bytes[p])|unsigned(bytes[p+1])<<8|unsigned(bytes[p+2])<<16|unsigned(bytes[p+3])<<24;
    };
    if(bytes.size()!=72+W*H*2 || word(0)!=0x62787463 || word(20)!=72 ||
       word(44)!=(W|(H<<16)) || word(48)!=0x80336752)return {};
    std::array<std::array<unsigned char,64*64>,11> glyphs{};
    int gx0=64,gy0=64,gx1=0,gy1=0;
    for(unsigned d=0;d<11;++d) {
        if(!NumberGlyph(d==10?':':'0'+d,bold,glyphs[d]))return {};
        for(int y=0;y<64;++y)for(int x=0;x<64;++x)if(d<10 && glyphs[d][y*64+x]) {
            gx0=std::min(gx0,x);gy0=std::min(gy0,y);gx1=std::max(gx1,x+1);gy1=std::max(gy1,y+1);
        }
    }
    if(gx1<=gx0 || gy1<=gy0)return {};
    std::vector<std::uint8_t> native(W*H*4),out(W*H*S*S*4);
    for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x) {
        unsigned morton=0;
        for(unsigned bit=0;bit<3;++bit)morton|=((x>>bit)&1)<<(bit*2)|((y>>bit)&1)<<(bit*2+1);
        const auto p=72+2*((y/8*(W/8)+x/8)*64+morton);
        const unsigned v=bytes[p]|unsigned(bytes[p+1])<<8;
        for(unsigned c=0;c<4;++c)native[(y*W+x)*4+c]=((v>>(12-c*4))&15)*17;
    }
    // Vulkan's decoded texture is bottom-up. Preserve every unused cell.
    for(unsigned y=0;y<H*S;++y)for(unsigned x=0;x<W*S;++x)
        for(unsigned c=0;c<4;++c)out[((H*S-1-y)*W*S+x)*4+c]=native[((y/S)*W+x/S)*4+c];
    constexpr std::array rows{0u,12u,24u,42u,60u,84u,108u};
    constexpr std::array heights{12u,12u,14u,14u,21u,21u,20u};
    for(unsigned row=0;row<rows.size();++row) {
        int x0=12,y0=heights[row],x1=0,y1=0;
        std::array<unsigned,3> color{};
        for(unsigned d=0;d<10;++d)for(unsigned y=0;y<heights[row];++y)for(unsigned x=0;x<12;++x) {
            const auto p=((rows[row]+y)*W+d*12+x)*4;
            if(native[p+3]<128 || std::max({native[p],native[p+1],native[p+2]})<128)continue;
            x0=std::min(x0,int(x));x1=std::max(x1,int(x+1));
            y0=std::min(y0,int(y));y1=std::max(y1,int(y+1));
            for(unsigned c=0;c<3;++c)color[c]=std::max(color[c],unsigned(native[p+c]));
        }
        if(x1<=x0 || y1<=y0)return {};
        const float scale=std::min(float((x1-x0)*S)/(gx1-gx0),float((y1-y0)*S)/(gy1-gy0));
        const float oy=y0*S+((y1-y0)*S-(gy1-gy0)*scale)*.5f;
        std::array<unsigned,3> red{};
        if(timer && row==4)for(unsigned y=0;y<heights[row];++y)for(unsigned x=128;x<248;++x) {
            const auto at=((rows[row]+y)*W+x)*4;
            if(native[at+3]>=128)for(unsigned c=0;c<3;++c)red[c]=std::max(red[c],unsigned(native[at+c]));
        }
        const unsigned cells=timer && row>=4?(row==4?22:11):10;
        for(unsigned d=0;d<cells;++d) {
            const unsigned glyph=d%11;
            const unsigned cell_x=(d/11)*128+glyph*12,cell_width=glyph==10?8:12;
            const auto ink=d>=11?red:color;
            int left=64,right=0;
            for(int y=gy0;y<gy1;++y)for(int x=0;x<64;++x)if(glyphs[glyph][y*64+x]) {
                left=std::min(left,x);right=std::max(right,x+1);
            }
            // Native 424cb4 uses 12-pixel digits and an 8-pixel colon;
            // the red countdown bank begins at X=128, not X=132.
            const float ox=glyph==10?(cell_width*S-(right-left)*scale)*.5f:
                x0*S+((x1-x0)*S-(right-left)*scale)*.5f;
            const auto alpha=[&](float x,float y) {
                const float sx=(x-ox)/scale+left-.5f,sy=(y-oy)/scale+gy0-.5f;
                const int ix=int(std::floor(sx)),iy=int(std::floor(sy));
                const auto sample=[&](int a,int b)->float{return a<0 || a>=64 || b<0 || b>=64?0:glyphs[glyph][b*64+a];};
                return std::lerp(std::lerp(sample(ix,iy),sample(ix+1,iy),sx-ix),
                    std::lerp(sample(ix,iy+1),sample(ix+1,iy+1),sx-ix),sy-iy)/255.f;
            };
            for(unsigned y=0;y<heights[row]*S;++y)for(unsigned x=0;x<cell_width*S;++x) {
                const float a=alpha(x+.5f,y+.5f);
                float shadow=0;
                if(style.enabled && style.strength>0) {
                    const float sx=x+.5f-S*style.x,sy=y+.5f-S*style.y;
                    if(style.blur==0)shadow=alpha(sx,sy);
                    else {
                        // Separable 1-2-1 sampling softens the glyph silhouette.
                        constexpr std::array weights{1.f,2.f,1.f};
                        for(int by=-1;by<=1;++by)for(int bx=-1;bx<=1;++bx)
                            shadow+=alpha(sx+bx*S*style.blur,sy+by*S*style.blur)*weights[bx+1]*weights[by+1]/16;
                    }
                    shadow*=style.strength;
                }
                const float combined=a+shadow*(1-a);
                const auto p=((H*S-1-(rows[row]*S+y))*W*S+cell_x*S+x)*4;
                for(unsigned c=0;c<3;++c)out[p+c]=std::uint8_t(combined?ink[c]*a/combined+.5f:0);
                out[p+3]=std::uint8_t(combined*255+.5f);
            }
        }
    }
    // Only backdrop variants reserve this white patch. Backdrop triangles
    // sample its center; native digits and all other atlas variants are intact.
    if(backdrop)for(unsigned y=0;y<8*S;++y)for(unsigned x=248*S;x<W*S;++x)
        for(unsigned c=0;c<4;++c)out[((H*S-1-y)*W*S+x)*4+c]=255;
    return out;
}
}
