// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Core { class System; }
namespace OOT3D::MapFrame {
using Bytes = std::vector<std::uint8_t>;
constexpr unsigned Width = 512, Height = 256, FrameWidth = 400, FrameHeight = 216;
constexpr unsigned MaskVersion = 2;
using Pixels = std::array<std::uint32_t, Width * Height>;
inline Common::AtomicSharedPtr<const Pixels> texture;
inline auto GetTexture() { return texture.load(); }
inline void SetTexture(std::shared_ptr<const Pixels> value) { texture.store(std::move(value)); }
enum class Status { Unavailable, Extracted, Cached };
inline std::atomic<Status> status{Status::Unavailable};
void Load(Core::System& system);

inline std::uint32_t LE32(std::span<const std::uint8_t> b, std::size_t p) {
    return std::uint32_t(b[p]) | (std::uint32_t(b[p+1])<<8) |
        (std::uint32_t(b[p+2])<<16) | (std::uint32_t(b[p+3])<<24);
}
inline std::uint64_t LE64(std::span<const std::uint8_t> b, std::size_t p) {
    return LE32(b,p) | (std::uint64_t(LE32(b,p+4))<<32);
}
// Bounded, incremental level-3 RomFS traversal. Never loads the whole cartridge.
// Reader has the same GetSize/ReadFile interface as FileSys::RomFSReader.
template<class Reader> Bytes ReadSource(Reader& reader) {
    const auto read = [&](std::uint64_t at, std::size_t size) -> Bytes {
        if (at > reader.GetSize() || size > reader.GetSize()-at) return {};
        Bytes bytes(size);
        if (reader.ReadFile(at,size,bytes.data()) != size) return {};
        return bytes;
    };
    const auto h=read(0,40);
    if (h.size()!=40 || LE32(h,0)!=40) return {};
    const auto dirs=LE32(h,12), dir_size=LE32(h,16), files=LE32(h,28), file_size=LE32(h,32);
    const auto data=LE32(h,36);
    if (dirs>reader.GetSize() || dir_size>reader.GetSize()-dirs ||
        files>reader.GetSize() || file_size>reader.GetSize()-files || data>reader.GetSize()) return {};
    const auto entry = [&](std::uint32_t table,std::uint32_t length,std::uint32_t off,
                           unsigned fixed,std::u16string_view name) -> Bytes {
        if (off>length || fixed>length-off) return {};
        auto b=read(std::uint64_t(table)+off,fixed);
        if (b.size()!=fixed) return {};
        const auto n=LE32(b,fixed-4);
        if (n>512 || n%2 || n>length-off-fixed) return {};
        auto text=read(std::uint64_t(table)+off+fixed,n);
        if (text.size()!=n) return {};
        bool match=n==name.size()*2;
        for (unsigned i=0;match && i<name.size();++i)
            match=(text[2*i] | (unsigned(text[2*i+1])<<8))==name[i];
        b.push_back(match);return b;
    };
    auto root=entry(dirs,dir_size,0,24,u"");
    if (root.empty()) return {};
    auto off=LE32(root,8);
    for (unsigned visited=0;off!=0xffffffff && visited<std::min(dir_size/24,4096u);++visited) {
        auto dir=entry(dirs,dir_size,off,24,u"misc");
        if (dir.empty()) return {};
        if (dir.back()) {
            off=LE32(dir,12);
            for (unsigned n=0;off!=0xffffffff && n<std::min(file_size/32,4096u);++n) {
                auto file=entry(files,file_size,off,32,u"boss_goma.ctxb");
                if (file.empty()) return {};
                if (file.back()) {
                    const auto start=LE64(file,8), size=LE64(file,16);
                    if (size!=131144 || start>reader.GetSize()-data) return {};
                    return read(data+start,size);
                }
                off=LE32(file,4);
            }
            return {};
        }
        off=LE32(dir,4);
    }
    return {};
}
// Accept only the verified single-texture ETC1 layout. Unknown/replaced layouts
// fail closed instead of sampling arbitrary data or borrowing another game's art.
inline bool ValidSource(std::span<const std::uint8_t> b) {
    return b.size()==131144 && LE32(b,0)==0x62787463 && LE32(b,4)==b.size() &&
        LE32(b,8)==1 && LE32(b,16)==24 && LE32(b,20)==72 &&
        LE32(b,24)==0x20786574 && LE32(b,28)==48 && LE32(b,32)==1 &&
        LE32(b,36)==131072 && LE32(b,44)==(1024 | (256u<<16)) &&
        LE32(b,48)==0x675a && LE32(b,52)==0;
}
constexpr unsigned TiledOffset(unsigned x,unsigned y) {
    unsigned m=0;
    for (unsigned b=0;b<3;++b) m|=((x>>b)&1)<<(2*b) | ((y>>b)&1)<<(2*b+1);
    return ((y/8)*(Width/8)+x/8)*64+m;
}
// Boundary of the baked illustration, in source-atlas pixels. Include its
// antialias fringe in the cutout. The exterior brown/green frame and central
// Triforce remain; the two unrelated caption boxes above it are excluded.
inline bool Artwork(float x,float y) {
    constexpr std::array<std::array<float,2>,22> polygon{{
        {25,46},{375,46},{375,71},{384,79},{384,194},{375,203},
        {375,231},{345,231},{336,240},{216,240},{209,232},{189,232},
        {180,240},{62,240},{53,232},{25,232},{25,203},{16,194},
        {16,79},{25,71},{25,47},{25,46}}};
    bool inside=false;
    for (unsigned i=0,j=polygon.size()-1;i<polygon.size();j=i++) {
        const auto a=polygon[i], b=polygon[j];
        if ((a[1]>y)!=(b[1]>y) && x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]) inside=!inside;
    }
    return inside;
}
// Decode callback returns RGB for an ETC1 4x4 subtile. Reuse the emulator's
// decoder; this file contains geometry and parsing only, never game pixels.
template<class Decode> std::shared_ptr<Pixels> Extract(std::span<const std::uint8_t> b, Decode decode) {
    if (!ValidSource(b)) return {};
    auto pixels=std::make_shared<Pixels>();
    for (unsigned y=0;y<FrameHeight;++y) for (unsigned x=0;x<FrameWidth;++x) {
        // Native boss card places the decorated edge and Triforce at the top.
        if (Artwork(x+.5f,255.5f-y)) continue;
        const unsigned sy=y, sx=x;
        const auto at=72+((sy/8)*128+sx/8)*32+((sy%8)/4*2+(sx%8)/4)*8;
        const auto rgb=decode(LE64(b,at),sx%4,sy%4);
        // Native tiled RGBA8 stores A,B,G,R in ascending byte addresses.
        (*pixels)[TiledOffset(x,Height-1-y)]=255u | (unsigned(rgb[2])<<8) |
            (unsigned(rgb[1])<<16) | (unsigned(rgb[0])<<24);
    }
    return pixels;
}
} // namespace OOT3D::MapFrame
