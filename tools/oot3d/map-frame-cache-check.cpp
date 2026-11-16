// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_frame_cache.h"
#include <cassert>
#include <iostream>
using namespace OOT3D::MapFrame;
int main() {
    auto pixels=std::make_shared<Pixels>();
    for(unsigned i=0;i<pixels->size();++i)(*pixels)[i]=i*0x12345u;
    const auto source=Digest("synthetic source"),bytes=EncodeCache(*pixels,source);
    assert(bytes.size()==CacheSize && *DecodeCache(bytes,source)==*pixels);
    assert(!DecodeCache(bytes,Digest("different source")));
    assert(!DecodeCache(bytes.first(bytes.size()-1),source));
    assert(!DecodeCache(bytes+"x",source));
    for(unsigned at:{0u,8u,40u,72u,unsigned(CacheSize-1)}) {
        auto bad=bytes;bad[at]=bad[at]^1;assert(!DecodeCache(bad,source));
    }
    std::cout<<"Map frame cache roundtrip, source changes, corruption and truncation passed\n";
}
