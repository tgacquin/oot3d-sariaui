// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include <cassert>
#include <fstream>
#include <iostream>

namespace F=OOT3D::FileSelect;
using Pixel=std::array<float,4>;
using Bytes=std::vector<unsigned char>;
Bytes Read(const std::string& path) {
    std::ifstream f(path,std::ios::binary);assert(f);
    return {std::istreambuf_iterator<char>(f),{}};
}
Pixel Texel(const Bytes& b,int x,int y) {
    x=std::clamp(x,0,511);y=std::clamp(y,0,511);
    unsigned morton=0;
    for(unsigned bit=0;bit<3;++bit)morton|=((x>>bit)&1)<<(2*bit)|((y>>bit)&1)<<(2*bit+1);
    const auto at=72+(((y/8)*64+x/8)*64+morton)*2;
    const unsigned word=b.at(at)|(unsigned(b.at(at+1))<<8);
    Pixel p;for(unsigned c=0;c<4;++c)p[c]=float((word>>(12-4*c))&15)*17;
    return p;
}
Pixel Sample(const Bytes& b,float u,float v) {
    const float x=u*512-.5f,y=v*512-.5f;
    const int ix=int(std::floor(x)),iy=int(std::floor(y));
    const float fx=x-ix,fy=y-iy;
    Pixel p{};
    for(unsigned k=0;k<4;++k) {
        const auto t=Texel(b,ix+(k&1),iy+((k>>1)&1));
        const float weight=(k&1?fx:1-fx)*(k&2?fy:1-fy);
        for(unsigned c=0;c<4;++c)p[c]+=t[c]*weight;
    }
    return p;
}
std::array<float,2> UV(const F::NameGeometry& g,unsigned q,float x,float y) {
    std::array<float,2> uv{};
    for(unsigned k=0;k<4;++k) {
        const float weight=(k&1?x:1-x)*(k&2?y:1-y);
        for(unsigned c=0;c<2;++c)uv[c]+=g.uvs[q*8+k*2+c]*weight;
    }
    return uv;
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    F::NameGeometry preview;
    Bytes preview_texture;
    // Verify all three QWERTY pages and the accented-letter page against the
    // game's real pixels, including bilinear filtering at every tile edge.
    for(unsigned page=0;page<4;++page) {
        const auto texture=Read(std::string(argv[1])+"/menu/01_US_ENGLISH/name_entry00_parts0"+
            (page==3?"1":"0")+".ctxb");
        assert(texture.size()==72+512*512*2);
        const float y=page==3?0.f:166.f*page;
        F::NameGeometry native;
        native.Add(-2,34,324,166,0,y/512,324.f/512,(y+166)/512);
        for(auto part:{F::Keyboard,F::KeyboardBackground})for(float scale:{.5f,.99f,1.01f,1.5f,3.f}) {
            F::Layout layout;layout.name_parts[part].scale=scale;
            F::NameGeometry g;F::NameKeyboard(g,native,layout,1,page==3);
            assert(g.count>98);
            for(unsigned q=0;q<98;++q) {
                for(float v:{0.f,.25f,.5f,.75f,1.f})for(float u:{0.f,.25f,.5f,.75f,1.f}) {
                    const auto uv=UV(g,q,u,v);const auto p=Sample(texture,uv[0],uv[1]);
                    // Background samples cannot include bevels, letters, alpha
                    // fringes or neighboring keys. Grain stays native-sized.
                    assert(uv[0]>=4.49f/512 && uv[0]<=27.51f/512);
                    assert(uv[1]>=(y+4.49f)/512 && uv[1]<=(y+27.51f)/512);
                    assert(p[0]<=69 && p[1]<=86 && p[2]<=103 && p[3]>254.9f);
                }
                for(unsigned c=0;c<2;++c) {
                    if(q%14!=13)for(unsigned k=0;k<2;++k)
                        assert(std::abs(g.uvs[q*8+(1+k*2)*2+c]-g.uvs[(q+1)*8+k*4+c])<1e-6f);
                    if(q/14!=6)for(unsigned k=0;k<2;++k)
                        assert(std::abs(g.uvs[q*8+4+k*2+c]-g.uvs[(q+14)*8+k*2+c])<1e-6f);
                }
                const float density=std::abs(g.uvs[q*8+2]-g.uvs[q*8])*512/
                    (g.positions[q*12+3]-g.positions[q*12]);
                const float backing_scale=part==F::KeyboardBackground?scale:1;
                assert(std::abs(density*backing_scale-23.f/24)<1e-4f);
            }
            if(page==0 && part==F::KeyboardBackground && scale==1.5f) {preview=g;preview_texture=texture;}
        }
    }
    if(argc==3) {
        constexpr unsigned width=1080,height=660;
        std::vector<Pixel> pixels(width*height,Pixel{45,35,20,255});
        for(unsigned q=0;q<preview.count;++q) {
            const auto b=F::Bounds(preview,q,q);
            for(unsigned py=0;py<height;++py)for(unsigned px=0;px<width;++px) {
                const float x=(px+.5f)/2-110,y=(py+.5f)/2-40;
                if(!b.Contains(x,y))continue;
                const auto uv=UV(preview,q,(x-b.left)/(b.right-b.left),(y-b.top)/(b.bottom-b.top));
                const auto color=Sample(preview_texture,uv[0],uv[1]);auto& dst=pixels[py*width+px];
                for(unsigned c=0;c<3;++c)dst[c]=color[c]*color[3]/255+dst[c]*(1-color[3]/255);
            }
        }
        std::ofstream f(argv[2],std::ios::binary);assert(f);f<<"P6\n"<<width<<' '<<height<<"\n255\n";
        for(const auto& p:pixels)for(unsigned c=0;c<3;++c)f.put(char(std::clamp(p[c],0.f,255.f)));
    }
    std::cout<<"Keyboard background: real atlas pixels, fractional/large scales, all pages, seamless UVs and native texture density passed\n";
}
