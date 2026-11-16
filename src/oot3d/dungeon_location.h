// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/pause_menu.h"
#include "oot3d/pause_prompts.h"

namespace OOT3D::DungeonLocation {
using PaperGeometry=PausePrompts::QuadGeometry<3>;
using TitleGeometry=PausePrompts::QuadGeometry<1>;
template<class G> G Pose(G g,const PauseMenu::Pose& p,float alpha,float rise) {
    if(!PauseMenu::Valid(p))return {};
    for(unsigned q=0;q<g.count;++q)for(unsigned v=0;v<4;++v) {
        g.positions[q*12+v*3]=200+(g.positions[q*12+v*3]-200)*p.scale+p.x;
        g.positions[q*12+v*3+1]=200+(g.positions[q*12+v*3+1]-200)*p.scale+p.y+rise;
        g.colors[q*16+v*4+3]=std::clamp(alpha,0.f,1.f);
    }
    return g;
}
inline PaperGeometry Paper(const PauseMenu::Layout& l,float alpha,float rise) {
    PaperGeometry g;
    // Exact native overworld parchment/label geometry and atlas crops from
    // 44bd34 (4d5c14..4d5c5c). The second half mirrors the first.
    g.Add(56,168,144,64,.5f/256,63.5f/256,143.5f/256,.5f/256);
    g.Add(200,168,144,64,143.5f/256,63.5f/256,.5f/256,.5f/256);
    g.Add(152,177,96,9,152.5f/256,79.5f/256,247.5f/256,71.5f/256);
    return Pose(g,l[PauseMenu::DungeonLocationPaper],alpha,rise);
}
inline TitleGeometry Title(const PauseMenu::Layout& l,unsigned index,float alpha,float rise) {
    TitleGeometry g;if(index>=Titles.size())return g;
    g.Add(72,186,256,32,.5f/Width,(Height-index*32-.5f)/Height,
          255.5f/Width,(Height-index*32-31.5f)/Height);
    return Pose(g,l[PauseMenu::DungeonLocationTitle],alpha,rise);
}
template<class M> std::optional<unsigned> Current(M& m) {
    if(!PauseMenu::Active(m) || !PauseMenu::MapPage(m) || !PauseMenu::DungeonPage(m))return {};
    const auto play=m.Read(PauseMenu::UI+0xc).value_or(0);
    if(!TitleScreen::Pointer(play,0x108))return {};
    const auto index=TitleIndex(m.Read(play+0x104).value_or(~0u)&0xffff);
    const auto image=GetAssets();
    return image && index && (image->available&(1u<<*index))?index:std::nullopt;
}
}
