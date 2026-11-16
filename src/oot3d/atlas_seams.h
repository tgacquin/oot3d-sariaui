// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include <vector>

namespace OOT3D::AtlasSeams {
using TitleScreen::U32;
// Gear's heart frame/fill, Triforce surround and item-well rails are separate
// atlas cells. Their native edge UVs sample adjacent art/transparent white.
inline bool GearDecoration(unsigned q) {
    return q==1 || q==2 || q==3 || q==6 || q==7 ||
        (q>=9 && q<=14) || q==16 || q==17 || q==28;
}
// The meter's cap ends at atlas column 144, immediately beside unrelated
// artwork in menu_top_parts00 (512x256). Its stretched middle is only ONE texel wide (column 142).
// Sample the cap's last texel center and the middle's single column. Keep
// vertical UVs intact: the meter's height/border slicing depends on its rows.
inline U32 MagicWord(unsigned quad,unsigned component,U32 word) {
    if(component%2 || quad<59 || quad>61)return word;
    const float u=std::bit_cast<float>(word);
    if((quad==59 || quad==61) && u==144.f/512)return std::bit_cast<U32>(143.5f/512);
    if(quad==60 && (u==142.f/512 || u==143.f/512))return std::bit_cast<U32>(142.5f/512);
    return word;
}
inline U32 SavePromptWord(unsigned q,unsigned component,U32 word) {
    const bool top=q<=2 || (q>=22 && q<=24);
    const bool bottom=(q>=3 && q<=5) || (q>=28 && q<=30);
    const float uv=std::bit_cast<float>(word);
    if(component%2) {
        if(top && uv==1)return std::bit_cast<U32>(1-.5f/256);
        if(bottom && uv==1-40.f/256)return std::bit_cast<U32>(1-39.5f/256);
    } else {
        const bool cap=q==0 || q==2 || q==3 || q==5 || q==22 || q==24 ||
                       q==25 || q==27 || q==28 || q==30;
        if(cap && uv==120.f/256)return std::bit_cast<U32>(119.5f/256);
        if((q==1 || q==4 || q==23 || q==29) && (uv==118.f/256 || uv==119.f/256))
            return std::bit_cast<U32>(118.5f/256);
    }
    return word;
}
// Each song-list note is a separate 16x16 cell in menu_okarina_parts00.
// Clamp the current crop, since the selected song changes which button it uses.
inline void SongNoteUVs(std::span<float,8> uv) {
    for(unsigned axis=0;axis<2;++axis) {
        float lo=1,hi=0;
        for(unsigned v=0;v<4;++v) {
            if(!std::isfinite(uv[v*2+axis]))return;
            lo=std::min(lo,uv[v*2+axis]);hi=std::max(hi,uv[v*2+axis]);
        }
        const float half=.5f/(axis?256:512);
        if(lo<0 || hi>1 || hi-lo<2*half)continue;
        const float dimension=axis?256.f:512.f;
        const float first=(std::floor(lo*dimension)+.5f)/dimension;
        const float last=(std::ceil(hi*dimension)-.5f)/dimension;
        for(unsigned v=0;v<4;++v)uv[v*2+axis]=std::clamp(uv[v*2+axis],first,last);
    }
}
inline U32 SongPanelWord(unsigned q,unsigned component,U32 word) {
    const float uv=std::bit_cast<float>(word);
    if(q<=2) {
        if(component%2) {
            if(uv==1-192.f/256)return std::bit_cast<U32>(1-192.5f/256);
            if(uv==0)return std::bit_cast<U32>(.5f/256);
        } else {
            if(q==0 && uv==56.f/512)return std::bit_cast<U32>(56.5f/512);
            if(q==2 && uv==136.f/512)return std::bit_cast<U32>(135.5f/512);
            if(q==1 && (uv==103.f/512 || uv==104.f/512))return std::bit_cast<U32>(103.5f/512);
        }
    } else if(component%2==0) {
        if((q==35 || q==37) && uv==224.f/512)return std::bit_cast<U32>(223.5f/512);
        if(q==36 && (uv==222.f/512 || uv==223.f/512))return std::bit_cast<U32>(222.5f/512);
    }
    return word;
}
// Dungeon floors pack room rectangles into a shared texture. Native 2f1d78
// stores their model/helper pairs in these eight slots; 2fc40c uses exact crop
// boundaries, so linear filtering can pull a neighboring room's colored edge.
// Inset only the uploaded UVs, using each floor's real atlas dimensions.
template<class M> bool CopyDungeonRooms(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    if(!bytes || bytes%32 || bytes>256*32 || !TitleScreen::Pointer(src,bytes) ||
       !TitleScreen::Pointer(dst,bytes) || (src<dst+bytes && dst<src+bytes))return false;
    U32 helper=0;
    for(unsigned floor=0;floor<8;++floor) {
        const auto model=m.Read(0x55b450+floor*4).value_or(0);
        const auto h=m.Read(0x55b470+floor*4).value_or(0);
        if(TitleScreen::Pointer(model,0x1e4) && m.Read(model)==0x4ebd60 &&
           m.Read(model+4)==mesh && TitleScreen::Pointer(h,0x20) &&
           m.Read(h)==bytes/32 && m.Read(h+0x14)==src) {helper=h;break;}
    }
    if(!helper)return false;
    std::array<float,2> dimensions;
    if(!TitleScreen::ReadFloats(m,helper+4,dimensions))return false;
    for(float d:dimensions)if(d<8 || d>2048 || std::floor(d)!=d)return false;
    std::vector<U32> uv(bytes/4);
    for(unsigned i=0;i<uv.size();++i) {
        const auto word=m.Read(src+4*i);
        if(!word || !m.Read(dst+4*i) || !std::isfinite(std::bit_cast<float>(*word)))return false;
        uv[i]=*word;
    }
    for(unsigned q=0;q<bytes/32;++q)for(unsigned axis=0;axis<2;++axis) {
        float lo=1,hi=0;
        for(unsigned v=0;v<4;++v) {
            const float value=std::bit_cast<float>(uv[q*8+v*2+axis]);
            lo=std::min(lo,value);hi=std::max(hi,value);
        }
        if(lo<0 || hi>1 || hi<=lo)return false;
        const float inset=std::min((hi-lo)*.5f,.5f/dimensions[axis]);
        for(unsigned v=0;v<4;++v) {
            auto& word=uv[q*8+v*2+axis];
            word=std::bit_cast<U32>(std::clamp(std::bit_cast<float>(word),lo+inset,hi-inset));
        }
    }
    for(unsigned i=0;i<uv.size();++i)m.Write(dst+4*i,uv[i]);
    return true;
}
// Work on the new GPU upload, never on the native atlas or helper UVs. This
// remains compatible with replacements using the original texture identity.
template<class M> bool Copy(M& m,U32 mesh,U32 dst,U32 src,U32 bytes) {
    if(CopyDungeonRooms(m,mesh,dst,src,bytes))return true;
    if(!bytes || bytes%32 || bytes>108*32 || !TitleScreen::Pointer(src,bytes) ||
       !TitleScreen::Pointer(dst,bytes) || (src<dst+bytes && dst<src+bytes))return false;
    const auto owned=[&](U32 model,U32 helper,unsigned count) {
        return bytes==count*32 && TitleScreen::Pointer(model,0x1e4) &&
            m.Read(model)==std::optional<U32>{0x4ebd60} && m.Read(model+4)==std::optional<U32>{mesh} &&
            m.Read(helper)==std::optional<U32>{count} && m.Read(helper+0x14)==std::optional<U32>{src};
    };
    const bool magic=owned(m.Read(0x50af34+0xc).value_or(0),m.Read(0x50af34+4).value_or(0),92);
    const bool world=owned(m.Read(0x506cb0+4).value_or(0),m.Read(0x506cb0+0x10).value_or(0),30);
    const bool dungeon=owned(m.Read(0x506cb0+0xc).value_or(0),m.Read(0x506cb0+0x14).value_or(0),50);
    const bool save=owned(m.Read(0x50a508+4).value_or(0),m.Read(0x50a508+8).value_or(0),78);
    const bool song=owned(m.Read(0x5093e4+4).value_or(0),m.Read(0x5093e4+8).value_or(0),108);
    const bool gear=owned(m.Read(0x50446c+4).value_or(0),m.Read(0x50446c+0xc).value_or(0),41);
    // Separate world-map controllers share menu_cursor00 (128x128):
    // 2d1af8 builds the eight-piece blue frame; 2d1e30 builds 25 markers.
    const auto map_owned=[&](U32 offset,unsigned count) {
        const U32 owner=m.Read(0x506cb0+offset).value_or(0);
        return TitleScreen::Pointer(owner,0x10) && m.Read(owner)==std::optional<U32>{mesh} &&
            m.Read(owner+0xc)==std::optional<U32>{0} &&
            owned(m.Read(owner+4).value_or(0),m.Read(owner+8).value_or(0),count);
    };
    // Native 2f2448 mode 0 builds eight yellow cursor/glow corners from
    // the same 128x128 atlas. Scope this to the dungeon-map controller.
    const U32 cursor=m.Read(0x506cb0+0x1c).value_or(0);
    const bool dungeon_cursor=TitleScreen::Pointer(cursor,0x38) &&
        m.Read(cursor)==std::optional<U32>{mesh} && m.Read(cursor+0xc)==8 &&
        m.Read(cursor+0x24)==0 &&
        owned(m.Read(cursor+4).value_or(0),m.Read(cursor+8).value_or(0),8);
    const bool map_sprites=map_owned(0x50,8) || map_owned(0x2c,25) || dungeon_cursor;
    const U32 banner=m.Read(0x4fc648+0x38).value_or(0);
    const bool parchment=TitleScreen::Pointer(banner,0x18) &&
        owned(m.Read(banner+0x10).value_or(0),m.Read(banner+0x14).value_or(0),3);
    if(!magic && !world && !parchment && !dungeon && !map_sprites && !save && !song && !gear)return false;
    std::vector<U32> uv(bytes/4);
    for(unsigned i=0;i<uv.size();++i) {
        const auto word=m.Read(src+4*i);if(!word || !m.Read(dst+4*i))return false;uv[i]=*word;
        if(!std::isfinite(std::bit_cast<float>(*word)))return false;
    }
    bool changed=false;
    for(unsigned q=0;q<bytes/32;++q) {
        if(song && q>=39 && q<=46) {
            std::array<float,8> note;
            for(unsigned c=0;c<8;++c)note[c]=std::bit_cast<float>(uv[q*8+c]);
            SongNoteUVs(note);
            for(unsigned c=0;c<8;++c) {
                const auto word=std::bit_cast<U32>(note[c]);changed|=uv[q*8+c]!=word;uv[q*8+c]=word;
            }
        } else if(save && q>=43 && q<64) {
            // Options tiles and labels share a tightly packed 256x256 atlas.
            // Keep bilinear sampling inside each crop, including stretched
            // one-texel borders, instead of pulling in neighboring artwork.
            for(unsigned axis=0;axis<2;++axis) {
                float lo=1,hi=0;
                for(unsigned v=0;v<4;++v) {
                    const float f=std::bit_cast<float>(uv[q*8+v*2+axis]);
                    lo=std::min(lo,f);hi=std::max(hi,f);
                }
                if(lo<0 || hi>1 || hi<=lo)continue;
                const float inset=std::min((hi-lo)*.5f,.5f/256);
                for(unsigned v=0;v<4;++v) {
                    auto& u=uv[q*8+v*2+axis];
                    const auto clamped=std::bit_cast<U32>(std::clamp(std::bit_cast<float>(u),lo+inset,hi-inset));
                    changed|=u!=clamped;u=clamped;
                }
            }
        } else if(save || song) {
            // Native save caps border transparent white padding. The staff's
            // outer edges border other sprites; its middle is one texel wide.
            // Apply to native uploads too, not only the song-list replacement.
            for(unsigned c=0;c<8;++c) {
                auto& u=uv[q*8+c];const auto v=save?SavePromptWord(q,c,u):SongPanelWord(q,c,u);
                changed|=v!=u;u=v;
            }
        } else if(magic) {
            for(unsigned c=0;c<8;++c) {auto& u=uv[q*8+c];const auto v=MagicWord(q,c,u);changed|=v!=u;u=v;}
        } else if(parchment && q<2) {
            // 44bd34 builds two mirrored 144x64 parchment halves from the
            // left of the atlas. Column 144 belongs to the next sprite.
            for(unsigned c=0;c<8;c+=2)if(uv[q*8+c]==std::bit_cast<U32>(144.f/256)) {
                uv[q*8+c]=std::bit_cast<U32>(143.5f/256);changed=true;
            }
        } else if(map_sprites || (world && q>=12 && q<=17) || (dungeon && (q==19 || (q>=21 && q<=35))) ||
                  (gear && GearDecoration(q))) {
            // 480bb8: rounded location plate (12..14), name (15), and
            // Skulltula icons (16/17). Clamp each sprite to its own texels;
            // the shared atlas has parchment/white frame pixels right beside them.
            // 480e98: dungeon small key (19) occupies [488,512) x [48,72).
            // Its transparent top/bottom rows border opaque sprites. Keep
            // filtering inside those rows; leave the counter and name alone.
            // Floor stones/labels (21..35) also border unrelated art. Native
            // 44281c changes label rows per dungeon: clamp the current crop.
            // Gear uses the same 512x256 dimensions. Read the current crop,
            // including the heart-piece fill's changing atlas column.
            for(unsigned axis=0;axis<2;++axis) {
                float lo=1,hi=0;
                for(unsigned v=0;v<4;++v) {const float f=std::bit_cast<float>(uv[q*8+v*2+axis]);lo=std::min(lo,f);hi=std::max(hi,f);}
                // Also collapse the frame's one-texel stretched strips to
                // their texel center; never sample adjacent corners/markers.
                const float half=.5f/(map_sprites?128:(axis?256:512));
                if(lo<0 || hi>1 || hi-lo<2*half)continue;
                for(unsigned v=0;v<4;++v) {
                    auto& u=uv[q*8+v*2+axis];const float f=std::bit_cast<float>(u);
                    const auto clamped=std::bit_cast<U32>(std::clamp(f,lo+half,hi-half));changed|=u!=clamped;u=clamped;
                }
            }
        }
    }
    if(!changed)return false;
    for(unsigned i=0;i<uv.size();++i)m.Write(dst+4*i,uv[i]);
    return true;
}
}
