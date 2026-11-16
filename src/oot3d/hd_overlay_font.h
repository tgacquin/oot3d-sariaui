// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>
#include <imgui.h>
#include "oot3d/hd_font_thin_data.h"

namespace OOT3D::HDFont {
// Build once, before the Vulkan backend uploads the ImGui font atlas. This face
// uses our thin artwork independently of the game's selected font style.
inline ImFont* AddThinOverlayFont(ImFontAtlas& atlas) {
    constexpr unsigned scale=4, cell=64;
    static constexpr ImWchar space_range[]{32,32,0};
    ImFontConfig config;
    config.SizePixels=cell;
    config.GlyphRanges=space_range;
    auto* font=atlas.AddFontDefault(&config);
    font->FallbackChar='?';
    atlas.AddCustomRectFontGlyph(font,' ',1,1,4*scale);
    std::vector<int> rectangles;
    for(const auto& glyph:ThinArtwork::Records)
        rectangles.push_back(atlas.AddCustomRectFontGlyph(font,glyph.code,
            glyph.width*scale,cell,(glyph.width+1)*scale));

    unsigned char* pixels=nullptr;
    int width=0,height=0;
    atlas.GetTexDataAsRGBA32(&pixels,&width,&height);
    if(!pixels)throw std::runtime_error("OoT3D HD overlay font atlas failed");
    for(unsigned i=0;i<std::size(ThinArtwork::Records);++i) {
        const auto& glyph=ThinArtwork::Records[i];
        std::array<unsigned char,cell*cell> alpha{};
        unsigned count=0;
        if(glyph.offset+glyph.size>std::size(ThinArtwork::Runs) || glyph.size%2 ||
           glyph.left+glyph.width>cell/scale)
            throw std::runtime_error("Invalid OoT3D HD overlay glyph");
        for(unsigned p=glyph.offset;p<glyph.offset+glyph.size;p+=2) {
            const unsigned length=ThinArtwork::Runs[p];
            if(!length || count+length>alpha.size())
                throw std::runtime_error("Invalid OoT3D HD overlay glyph coverage");
            std::fill_n(alpha.begin()+count,length,ThinArtwork::Runs[p+1]);
            count+=length;
        }
        if(count!=alpha.size())throw std::runtime_error("Incomplete OoT3D HD overlay glyph");
        const auto* rect=atlas.GetCustomRectByIndex(rectangles[i]);
        for(unsigned y=0;y<cell;++y)for(unsigned x=0;x<glyph.width*scale;++x) {
            auto* pixel=pixels+4*((rect->Y+y)*width+rect->X+x);
            pixel[0]=pixel[1]=pixel[2]=255;
            pixel[3]=alpha[y*cell+glyph.left*scale+x];
        }
    }
    return font;
}
} // namespace OOT3D::HDFont
