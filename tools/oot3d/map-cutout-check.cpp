// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_cutout.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace OOT3D::MapFrame;
int main() {
    SourcePixels source;source.fill(0x123456ff);
    Cutout c{10,20,110,120,{{30,40},{80,40},{80,60},{60,60},{60,100},{30,100}}};
    auto cut=Cut(source,c);assert(cut && cut->width==100 && cut->height==100);
    const auto pixel=[&](int x,int y){return (*cut->pixels)[TiledOffset(x,Height-1-y)];};
    assert(pixel(0,0)==0x123456ff); // Outer corner is retained, not resynthesized.
    assert(pixel(30,30)==0); // Artwork inside polygon removed.
    assert(pixel(60,60)==0x123456ff); // Concave notch retained.
    assert(pixel(100,0)==0 && pixel(0,100)==0); // Exclusive crop bounds.
    auto reversed=c;std::reverse(reversed.hole.begin(),reversed.hole.end());
    assert(*Cut(source,reversed)->pixels==*cut->pixels);
    c.hole.clear();assert(!Cut(source,c));
    c.hole={{1,1},{3,3},{5,5}};assert(!Valid(c));
    c.hole={{30,40},{80,40},{80,100}};c.right=401;assert(!Valid(c));
    c.right=110;c.hole[0].x=std::numeric_limits<float>::quiet_NaN();assert(!Valid(c));
    std::cout<<"Coordinate cutout: concave polygon, exact crop, transparent hole, orientation and invalid input passed\n";
}
