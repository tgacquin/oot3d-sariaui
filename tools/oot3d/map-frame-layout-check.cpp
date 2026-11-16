// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_frame.h"
#include <cassert>
#include <iostream>
using namespace OOT3D;
int main() {
    PauseMenu::Layout layout;
    auto a=MapFrame::Make(layout,300,200);assert(a.count==1);
    assert(a.positions[3]-a.positions[0]==300 && a.positions[7]-a.positions[1]==200);
    layout.map_frame.cut_x=1200;layout.map_frame.cut_y=-900;layout.map_frame.cut_scale=3;
    auto b=MapFrame::Make(layout,300,200);assert(b.count==1);
    assert(b.positions[3]-b.positions[0]==900 && b.positions[7]-b.positions[1]==600);
    assert((b.positions[0]+b.positions[3])/2==160+1200/PauseMenu::Scale);
    assert((b.positions[1]+b.positions[7])/2==120-900/PauseMenu::Scale);
    assert(a.uvs==b.uvs);
    // Old piece/edge settings cannot distort the cutout.
    layout.map_frame.left=500;layout.map_frame.width=7;layout.map_frame.pieces[0].x=200;
    assert(MapFrame::Make(layout,300,200).positions==b.positions);
    PauseMenu::motion_x=0;
    const auto projection=MapFrame::DrawTransform();
    layout[PauseMenu::Items]={100,-50,2};layout[PauseMenu::Map]={-90,30,.5f};
    PauseMenu::SetLayout(layout,false);
    assert(MapFrame::Make(layout,300,200).positions==b.positions);
    const auto independent=MapFrame::DrawTransform();
    assert(independent.x==projection.x && independent.y==projection.y &&
        independent.sx==projection.sx && independent.sy==projection.sy);
    PauseMenu::motion_x=-70;
    const auto animated=MapFrame::DrawTransform();
    assert(animated.x==-.8f*70 && animated.y==projection.y &&
        animated.sx==projection.sx && animated.sy==projection.sy);
    PauseMenu::motion_x=0;PauseMenu::SetLayout({},false);
    layout.map_frame.enabled=false;assert(MapFrame::Make(layout,300,200).count==0);
    layout.map_frame={};assert(MapFrame::Make(layout,0,200).count==0);
    std::cout<<"Single cutout quad, aspect ratio, movement, scale and legacy isolation passed\n";
}
