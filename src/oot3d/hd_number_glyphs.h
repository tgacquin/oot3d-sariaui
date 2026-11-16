// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hd_font_data.h"
#include "oot3d/hd_font_thin_data.h"
#include <algorithm>
#include <array>

namespace OOT3D::HDFont {
inline bool NumberGlyph(unsigned code,bool bold,std::array<std::uint8_t,4096>& glyph) {
    const auto decode=[&](const auto& records,const auto& runs) {
        const auto it=std::find_if(std::begin(records),std::end(records),
            [&](const auto& r){return r.code==code;});
        if(it==std::end(records) || it->size%2 || it->offset+it->size>std::size(runs))return false;
        unsigned n=0;
        for(unsigned p=it->offset;p<it->offset+it->size;p+=2) {
            const unsigned count=runs[p];
            if(!count || n+count>glyph.size())return false;
            std::fill_n(glyph.begin()+n,count,runs[p+1]);n+=count;
        }
        return n==glyph.size();
    };
    return bold?decode(Artwork::Records,Artwork::Runs):decode(ThinArtwork::Records,ThinArtwork::Runs);
}
}
