// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/randomizer/resource_diagnostics.h"
#include <cassert>
#include <map>
#include <optional>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

struct Memory {
    std::map<std::uint32_t,std::uint32_t> words;
    std::optional<std::uint32_t> Read(std::uint32_t at) const {
        const auto it=words.find(at);
        if(it==words.end())return {};
        return it->second;
    }
};
int main(int argc,char** argv) {
    namespace D=OOT3D::Randomizer::ResourceDiagnostics;
    Memory m;
    const std::u16string path=u"rom:/actor/zelda_spo04_objects.zar";
    for(unsigned i=0;i<=path.size();++i)
        m.words[0x1000+(i*2&~3u)] |= std::uint32_t(path[i])<<((i&1)*16);
    assert(D::Path(m,0x1000)==path);
    assert(D::Path(m,0x1002)==path.substr(1));
    assert(D::Path(m,0x2000).empty());

    auto block=[&](std::uint32_t at,bool free,std::uint32_t size,std::uint32_t next,std::uint32_t prev) {
        m.words[at]=0x7373|(std::uint32_t(free)<<16);
        m.words[at+4]=size;m.words[at+8]=next;m.words[at+12]=prev;
    };
    m.words[0x565500]=0x3000;
    block(0x3000,false,0x100,0x3110,0);
    block(0x3110,true,0x200,0x3320,0x3000);
    block(0x3320,true,0x100,0,0x3110);
    auto a=D::InspectArena(m);
    assert(a.complete && a.blocks==3 && a.used==0x100 && a.free==0x300 && a.largest_free==0x200);
    m.words[0x3328]=0x3000; // Cycle must stop, even with apparently valid block flags.
    assert(!D::InspectArena(m).complete);
    m.words[0x3328]=0;
    m.words[0x3110]=0; // Corrupted block magic.
    a=D::InspectArena(m);
    assert(!a.complete && a.stopped_at==0x3110);
    m.words[0x3110]=0x17373;
    m.words.erase(0x3114); // Unmapped header field.
    assert(!D::InspectArena(m).complete);
    m.words[0x565500]=0;
    assert(D::InspectArena(m).complete);
    if(argc==3) {
        struct Buffer {
            std::vector<unsigned char> bytes;
            std::optional<std::uint32_t> Read(std::uint32_t address) const {
                if(address<0x800000 || std::uint64_t(address-0x800000)+4>bytes.size())return {};
                std::uint32_t value;std::memcpy(&value,bytes.data()+address-0x800000,4);return value;
            }
        } buffer;
        unsigned archives=0,mutations=0;
        const auto check=[&](const std::filesystem::path& path) {
            std::ifstream in(path,std::ios::binary);assert(in);
            buffer.bytes={std::istreambuf_iterator<char>(in),{}};
            const auto size=std::uint32_t(buffer.bytes.size());
            const auto result=D::CheckArchive(buffer,0x800000,size);
            if(!result) {
                std::cerr<<path<<": "<<result.error<<" offset="<<std::hex<<result.offset
                         <<" member="<<result.index<<'\n';std::abort();
            }
            ++archives;
            const auto read=[&](unsigned off){return *buffer.Read(0x800000+off);};
            const auto set=[&](unsigned off,std::uint32_t v){std::memcpy(buffer.bytes.data()+off,&v,4);};
            const auto saved=read(0);set(0,0);assert(!D::CheckArchive(buffer,0x800000,size));set(0,saved);
            assert(!D::CheckArchive(buffer,0x800000,31));
            const unsigned files=read(8)>>16,offsets=read(20);
            for(unsigned i=0;i<files;++i) {
                const auto at=read(offsets+i*4);
                if(read(at)!=0x62616d63 || !read(at+16))continue;
                const auto section=at+read(at+20),relative=read(section+16);
                if(!relative)continue;
                const auto count_at=section+relative+4,original=read(count_at);
                set(count_at,0x330393a3); // Impossible texture count seen in the crash.
                const auto invalid=D::CheckArchive(buffer,0x800000,size);
                assert(!invalid && invalid.offset==count_at);
                set(count_at,original);++mutations;
            }
        };
        for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[1]))
            if(entry.is_regular_file() && entry.path().extension()==".zar")check(entry.path());
        check(argv[2]);
        assert(archives>100 && mutations>0);
        std::cout<<archives<<" original/custom archives validated; "<<mutations
                 <<" corrupted CMAB texture counts rejected before native allocation.\n";
    }
}
