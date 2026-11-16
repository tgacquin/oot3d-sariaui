// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_frame_asset.h"
#include <cmath>
#include <mutex>

namespace OOT3D::MapFrame {
constexpr int SourceWidth=400, SourceHeight=240;
using SourcePixels=std::array<std::uint32_t,SourceWidth*SourceHeight>;
struct Point { float x=0,y=0; };
struct Cutout {
    int left=0,top=0,right=400,bottom=216;
    std::vector<Point> hole;
};
struct CutImage { std::shared_ptr<const Pixels> pixels; int width=0,height=0,left=0,top=0; };
inline std::shared_ptr<const SourcePixels> source_image;
inline std::shared_ptr<const CutImage> cut_image;
inline auto GetSource() { return std::atomic_load(&source_image); }
inline auto GetCutImage() { return std::atomic_load(&cut_image); }
inline std::mutex cutout_mutex;
inline Cutout saved_cutout;
inline bool has_saved_cutout=false;
inline bool Inside(const std::vector<Point>& p,float x,float y) {
    bool inside=false;
    for(unsigned i=0,j=p.size()-1;i<p.size();j=i++) {
        const auto a=p[i],b=p[j];
        if((a.y>y)!=(b.y>y) && x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x)inside=!inside;
    }
    return inside;
}
inline bool Valid(const Cutout& c) {
    if(c.left<0 || c.top<0 || c.right>SourceWidth || c.bottom>SourceHeight ||
       c.right<=c.left || c.bottom<=c.top || c.hole.size()<3 || c.hole.size()>128)return false;
    double area=0;
    for(unsigned i=0;i<c.hole.size();++i) {
        const auto a=c.hole[i],b=c.hole[(i+1)%c.hole.size()];
        if(!std::isfinite(a.x) || !std::isfinite(a.y) || a.x<c.left || a.x>c.right ||
           a.y<c.top || a.y>c.bottom)return false;
        area+=a.x*b.y-b.x*a.y;
    }
    return std::abs(area)>1;
}
inline std::shared_ptr<CutImage> Cut(const SourcePixels& source,const Cutout& c) {
    if(!Valid(c))return {};
    auto pixels=std::make_shared<Pixels>();
    for(int y=c.top;y<c.bottom;++y)for(int x=c.left;x<c.right;++x) {
        if(Inside(c.hole,x+.5f,y+.5f))continue;
        (*pixels)[TiledOffset(x-c.left,Height-1-(y-c.top))]=source[y*SourceWidth+x];
    }
    return std::make_shared<CutImage>(CutImage{pixels,c.right-c.left,c.bottom-c.top,c.left,c.top});
}
// Persist coordinates only in the editor configuration directory. Pixel data
// always comes from the current loaded game; no extracted artwork is shipped.
bool ApplyCutout(const Cutout& cutout);
void LoadCutout();
}
