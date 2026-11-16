// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hd_boss_timer_font.h"
#include "oot3d/hd_number_glyphs.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace OOT3D::HDFont {
std::shared_ptr<const RupeeAtlas> StyledBossTimerAtlas(std::uint64_t hash,RupeeShadow shadow,bool bold) {
    std::shared_ptr<const RupeeAtlas> source;
    for(const auto& entry:boss_timer_atlases) {
        auto candidate=std::atomic_load(&entry);
        if(candidate && candidate->hash==hash) {source=std::move(candidate);break;}
    }
    if(!source)return {};
    static std::mutex mutex;
    static std::shared_ptr<const RupeeAtlas> previous,result;
    static RupeeShadow style;
    static bool weight=false;
    static std::uint64_t variant=0;
    std::scoped_lock lock{mutex};
    if(previous==source && style==shadow && weight==bold)return result;
    auto next=std::make_shared<RupeeAtlas>();
    next->rgba=MakeBossTimerAtlas(source->native,shadow,bold);
    if(next->rgba.empty())return {};
    next->hash=hash;next->variant=++variant;
    previous=source;style=shadow;weight=bold;result=std::move(next);
    return result;
}
std::vector<std::uint8_t> MakeBossTimerAtlas(std::span<const std::uint8_t> native,RupeeShadow style,bool bold) {
    constexpr unsigned W=512,H=256,S=4;
    if(native.size()!=W*H*4 || !std::isfinite(style.x) || !std::isfinite(style.y) ||
       !std::isfinite(style.blur) || !std::isfinite(style.strength) ||
       std::abs(style.x)>3 || std::abs(style.y)>3 || style.blur<0 || style.blur>2 ||
       style.strength<0 || style.strength>1)return {};
    std::array<std::array<unsigned char,4096>,12> glyphs{};
    int gx0=64,gy0=64,gx1=0,gy1=0;
    for(unsigned d=0;d<12;++d) {
        const unsigned code=d<10?'0'+d:d==10?':':'.';
        if(!NumberGlyph(code,bold,glyphs[d]))return {};
        for(int y=0;y<64;++y)for(int x=0;x<64;++x)if(d<10 && glyphs[d][y*64+x]) {
            gx0=std::min(gx0,x);gx1=std::max(gx1,x+1);gy0=std::min(gy0,y);gy1=std::max(gy1,y+1);
        }
    }
    int x0=12,y0=21,x1=0,y1=0;
    std::array<unsigned,3> color{};
    for(unsigned d=0;d<10;++d)for(unsigned y=0;y<21;++y)for(unsigned x=0;x<12;++x) {
        const auto p=((208+y)*W+336+d*12+x)*4;
        if(native[p+3]<128 || std::max({native[p],native[p+1],native[p+2]})<128)continue;
        x0=std::min(x0,int(x));x1=std::max(x1,int(x+1));y0=std::min(y0,int(y));y1=std::max(y1,int(y+1));
        for(unsigned c=0;c<3;++c)color[c]=std::max(color[c],unsigned(native[p+c]));
    }
    if(x1<=x0 || y1<=y0 || gx1<=gx0 || gy1<=gy0)return {};
    const float scale=std::min(float((x1-x0)*S)/(gx1-gx0),float((y1-y0)*S)/(gy1-gy0));
    const float oy=y0*S+((y1-y0)*S-(gy1-gy0)*scale)*.5f;
    std::vector<std::uint8_t> out(W*H*S*S*4);
    for(unsigned y=0;y<H*S;++y)for(unsigned x=0;x<W*S;++x)
        for(unsigned c=0;c<4;++c)out[((H*S-1-y)*W*S+x)*4+c]=native[((y/S)*W+x/S)*4+c];
    for(unsigned d=0;d<12;++d) {
        const unsigned cell_x=336+(d<10?d:d-10)*12,cell_y=d<10?208:232;
        int left=64,right=0;
        for(int y=0;y<64;++y)for(int x=0;x<64;++x)if(glyphs[d][y*64+x]) {
            left=std::min(left,x);right=std::max(right,x+1);
        }
        const float ox=x0*S+((x1-x0)*S-(right-left)*scale)*.5f;
        const auto alpha=[&](float x,float y) {
            const float sx=(x-ox)/scale+left-.5f,sy=(y-oy)/scale+gy0-.5f;
            const int ix=int(std::floor(sx)),iy=int(std::floor(sy));
            const auto sample=[&](int a,int b)->float{return a<0 || a>=64 || b<0 || b>=64?0:glyphs[d][b*64+a];};
            return std::lerp(std::lerp(sample(ix,iy),sample(ix+1,iy),sx-ix),
                std::lerp(sample(ix,iy+1),sample(ix+1,iy+1),sx-ix),sy-iy)/255.f;
        };
        for(unsigned y=0;y<21*S;++y)for(unsigned x=0;x<12*S;++x) {
            const float a=alpha(x+.5f,y+.5f);float shadow=0;
            if(style.enabled && style.strength>0) {
                const float sx=x+.5f-S*style.x,sy=y+.5f-S*style.y;
                if(style.blur==0)shadow=alpha(sx,sy);
                else {
                    constexpr std::array weights{1.f,2.f,1.f};
                    for(int by=-1;by<=1;++by)for(int bx=-1;bx<=1;++bx)
                        shadow+=alpha(sx+bx*S*style.blur,sy+by*S*style.blur)*weights[bx+1]*weights[by+1]/16;
                }
                shadow*=style.strength;
            }
            const float combined=a+shadow*(1-a);
            const auto p=((H*S-1-(cell_y*S+y))*W*S+cell_x*S+x)*4;
            for(unsigned c=0;c<3;++c)out[p+c]=std::uint8_t(combined?color[c]*a/combined+.5f:0);
            out[p+3]=std::uint8_t(combined*255+.5f);
        }
    }
    return out;
}
}
