// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/dungeon_parchment.h"
#include <cassert>
#include <iostream>
using namespace OOT3D::DungeonParchment;
inline std::shared_ptr<Pixels> ReferenceCompose(const Paper& paper,std::span<const Rect> pieces,float blend,Rect bounds) {
    auto out=std::make_shared<Pixels>();
    // A transparent texel around the union prevents linear-filter edge streaks.
    for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x) {
        const auto c=paper.At(std::lerp(bounds.left,bounds.right,(x+.5f)/Width),
            std::lerp(bounds.top,bounds.bottom,(y+.5f)/Height),pieces,blend);
        U32 pixel=0;for(unsigned k=0;k<4;++k)pixel|=U32(std::clamp(std::lround(c[k]),0l,255l))<<(24-8*k);
        (*out)[TiledOffset(x,Height-1-y)]=pixel;
    }
    return out;
}
int main() {
    Paper paper;std::vector<Color> source(208*176);
    for(unsigned y=0;y<176;++y)for(unsigned x=0;x<208;++x)
        source[y*208+x]={float(50+(x*7+y*3)%170),float(40+(x+y)%150),float(30+(x*3+y)%100),float((x+y)%13?255:0)};
    paper.Set(std::move(source),208,176);
    std::array<Rect,4> boxes{{{56,32,264,208},{-24,32,184,208},{136,32,344,208},{82,102,238,234}}};
    double old_ms=0,new_ms=0;
    for(unsigned n:{1u,2u,4u})for(float blend:{0.f,18.f,80.f}) {
        const std::span<const Rect> pieces{boxes.data(),n};auto bounds=Union(pieces);
        bounds.left-=1;bounds.top-=1;bounds.right+=1;bounds.bottom+=1;
        auto start=Clock::now();auto expected=ReferenceCompose(paper,pieces,blend,bounds);old_ms+=Milliseconds(start);
        start=Clock::now();auto result=Compose(paper,pieces,blend,bounds);new_ms+=Milliseconds(start);
        assert(*expected==*result); // Every RGBA texel, including tiled storage.
    }
    std::cout<<"9 full-size composites, exact pixel equality: reference_ms="<<old_ms
        <<" optimized_ms="<<new_ms<<" speedup="<<old_ms/new_ms<<"\n";
}
