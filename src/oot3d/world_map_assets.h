// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_cutout.h"

namespace OOT3D::WorldMap {
constexpr unsigned MapWidth=320,MapHeight=240,PaperWidth=208,PaperHeight=176;
struct PaperImage { std::vector<std::array<float,4>> colors; };
inline std::shared_ptr<const PaperImage> paper_image;
inline std::shared_ptr<const MapFrame::SourcePixels> source_image;
inline auto GetSource() {return std::atomic_load(&source_image);}
inline auto GetPaper() {return std::atomic_load(&paper_image);}
struct Outline {
    MapFrame::Cutout cut{0,0,320,240,{}};
    float feather=8,rounding=8;
};
inline std::shared_ptr<const Outline> outline;
inline auto GetOutline() {return std::atomic_load(&outline);}
inline bool Valid(const Outline& o) {
    return MapFrame::Valid(o.cut) && o.cut.right<=int(MapWidth) &&
        o.cut.bottom<=int(MapHeight) && std::isfinite(o.feather) && o.feather>=0 && o.feather<=64 &&
        std::isfinite(o.rounding) && o.rounding>=0 && o.rounding<=64;
}
// Round the editable polygon with tangent quadratic corners. Clamp each trim to
// half its neighboring edges so short segments cannot cross over one another.
// Keep control points unchanged; both preview and rendering use this boundary.
inline std::vector<MapFrame::Point> Boundary(const Outline& o) {
    if(o.rounding<=0 || !std::isfinite(o.rounding))return o.cut.hole;
    std::vector<MapFrame::Point> points,out;
    for(const auto p:o.cut.hole)
        if(points.empty() || std::hypot(p.x-points.back().x,p.y-points.back().y)>.0001f)points.push_back(p);
    if(points.size()>1 && std::hypot(points.front().x-points.back().x,points.front().y-points.back().y)<.0001f)points.pop_back();
    if(points.size()<3)return points;
    for(unsigned i=0;i<points.size();++i) {
        const auto a=points[(i+points.size()-1)%points.size()],b=points[i],c=points[(i+1)%points.size()];
        const float incoming=std::hypot(a.x-b.x,a.y-b.y),outgoing=std::hypot(c.x-b.x,c.y-b.y);
        const float trim=std::min({o.rounding,incoming*.5f,outgoing*.5f});
        const MapFrame::Point start{b.x+(a.x-b.x)*trim/incoming,b.y+(a.y-b.y)*trim/incoming};
        const MapFrame::Point end{b.x+(c.x-b.x)*trim/outgoing,b.y+(c.y-b.y)*trim/outgoing};
        const unsigned steps=std::clamp(unsigned(std::ceil(std::sqrt(trim)*2)),2u,16u);
        for(unsigned j=0;j<=steps;++j) {
            const float t=float(j)/steps,u=1-t;
            out.push_back({u*u*start.x+2*u*t*b.x+t*t*end.x,u*u*start.y+2*u*t*b.y+t*t*end.y});
        }
    }
    return out;
}
// Feather inward: the drawn boundary is the outside edge of the retained map.
inline float Alpha(const std::vector<MapFrame::Point>& boundary,float feather,float x,float y) {
    if(!MapFrame::Inside(boundary,x,y))return 0;
    if(feather<=0)return 1;
    float distance=1e9f;
    for(unsigned i=0;i<boundary.size();++i) {
        const auto a=boundary[i],b=boundary[(i+1)%boundary.size()];
        const float dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
        const float t=length>0?std::clamp(((x-a.x)*dx+(y-a.y)*dy)/length,0.f,1.f):0;
        const float rx=x-a.x-t*dx,ry=y-a.y-t*dy;
        distance=std::min(distance,rx*rx+ry*ry);
    }
    const float t=std::clamp(std::sqrt(distance)/feather,0.f,1.f);
    return t*t*(3-2*t);
}
inline float Alpha(const Outline& o,float x,float y) {return Alpha(Boundary(o),o.feather,x,y);}
bool ApplyOutline(const Outline& value);
void LoadOutline();
void Load(Core::System& system);
void Warmup();

// The same 208x176 parchment used by quad 0 of the dungeon map atlas.
// Extract from the loaded game, so it is ready before visiting any dungeon.
template<class Decode> std::shared_ptr<PaperImage> ExtractPaper(std::span<const std::uint8_t> b,Decode decode) {
    using MapFrame::LE32;using MapFrame::LE64;
    if(b.size()!=131144 || LE32(b,0)!=0x62787463 || LE32(b,4)!=b.size() ||
       LE32(b,8)!=1 || LE32(b,20)!=72 || LE32(b,44)!=(512|(256u<<16)) ||
       LE32(b,48)!=0x675b)return {};
    auto out=std::make_shared<PaperImage>();out->colors.resize(PaperWidth*PaperHeight);
    for(unsigned y=0;y<PaperHeight;++y)for(unsigned x=0;x<PaperWidth;++x) {
        const unsigned tx=256+x,ty=y;
        const unsigned at=72+((ty/8)*64+tx/8)*64+((ty%8)/4*2+(tx%8)/4)*16;
        const auto rgb=decode(LE64(b,at+8),tx%4,ty%4);
        out->colors[y*PaperWidth+x]={float(rgb[0]),float(rgb[1]),float(rgb[2]),
            float(((LE64(b,at)>>(4*(4*(tx%4)+ty%4)))&15)*17)};
    }
    return out;
}
}
