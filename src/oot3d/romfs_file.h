// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_frame_asset.h"

namespace OOT3D {
// Small bounded level-3 RomFS lookup, for assets read from the loaded cartridge.
// Never depends on the development extraction directory or a scene being loaded.
template<class Reader>
MapFrame::Bytes ReadRomFSFile(Reader& reader,std::span<const std::u16string_view> path) {
    using namespace MapFrame;
    const auto read=[&](std::uint64_t at,std::uint64_t size)->Bytes {
        if(size>4*1024*1024 || at>reader.GetSize() || size>reader.GetSize()-at)return {};
        Bytes b(size);if(reader.ReadFile(at,size,b.data())!=size)return {};return b;
    };
    auto h=read(0,40);if(h.size()!=40 || LE32(h,0)!=40 || path.empty())return {};
    const auto dirs=LE32(h,12),ds=LE32(h,16),files=LE32(h,28),fs=LE32(h,32),data=LE32(h,36);
    const auto entry=[&](unsigned table,unsigned size,unsigned off,unsigned fixed,
                         std::u16string_view name)->Bytes {
        if(off>size || fixed>size-off)return {};
        auto b=read(std::uint64_t(table)+off,fixed);if(b.size()!=fixed)return {};
        const auto n=LE32(b,fixed-4);if(n>512 || n%2 || n>size-off-fixed)return {};
        auto text=read(std::uint64_t(table)+off+fixed,n);if(text.size()!=n)return {};
        bool match=n==name.size()*2;
        for(unsigned i=0;match && i<name.size();++i)
            match=(text[2*i]|(unsigned(text[2*i+1])<<8))==name[i];
        b.push_back(match);return b;
    };
    auto dir=entry(dirs,ds,0,24,u"");if(dir.empty())return {};
    for(unsigned level=0;level+1<path.size();++level) {
        auto off=LE32(dir,8);bool found=false;
        for(unsigned n=0;off!=0xffffffff && n<std::min(ds/24,4096u);++n) {
            auto next=entry(dirs,ds,off,24,path[level]);if(next.empty())return {};
            if(next.back()){dir=std::move(next);found=true;break;}
            off=LE32(next,4);
        }
        if(!found)return {};
    }
    auto off=LE32(dir,12);
    for(unsigned n=0;off!=0xffffffff && n<std::min(fs/32,4096u);++n) {
        auto file=entry(files,fs,off,32,path.back());if(file.empty())return {};
        if(file.back()) {
            const auto start=LE64(file,8),size=LE64(file,16);
            if(data>reader.GetSize() || start>reader.GetSize()-data)return {};
            return read(data+start,size);
        }
        off=LE32(file,4);
    }
    return {};
}
}
