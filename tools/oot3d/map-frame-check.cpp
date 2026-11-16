// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_frame_asset.h"
#include "video_core/texture/etc1.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace OOT3D::MapFrame;
struct Reader {
    Bytes bytes;
    std::size_t reads=0;
    std::size_t GetSize() const {return bytes.size();}
    std::size_t ReadFile(std::size_t at,std::size_t size,std::uint8_t* out) {
        ++reads;
        if(at>bytes.size() || size>bytes.size()-at)return 0;
        std::copy_n(bytes.data()+at,size,out);return size;
    }
};
void Put(Bytes& b,unsigned p,std::uint32_t v) {for(unsigned i=0;i<4;++i)b[p+i]=v>>(8*i);}
Bytes Fixture() {
    Bytes b(131144);
    for(auto [p,v]:std::array<std::pair<unsigned,unsigned>,12>{{
        {0,0x62787463},{4,131144},{8,1},{16,24},{20,72},{24,0x20786574},
        {28,48},{32,1},{36,131072},{44,1024|(256<<16)},{48,0x675a},{52,0}}})Put(b,p,v);
    return b;
}
int main(int argc,char** argv) {
    const auto source=Fixture();assert(ValidSource(source));
    for(unsigned size:{0,20,71,131143})assert(!ValidSource(std::span(source).first(size)));
    for(unsigned off:{0,4,8,16,20,24,28,32,36,44,48,52}) {
        auto bad=source;bad[off]^=1;assert(!ValidSource(bad));
    }
    auto white=Extract(source,[](auto,unsigned,unsigned){return std::array{255,255,255};});
    assert(white);
    const auto pixel=[&](unsigned x,unsigned y){return (*white)[TiledOffset(x,255-y)];};
    assert(pixel(200,80)==0 && pixel(399,215)==0xffffffff && pixel(400,215)==0);
    assert(pixel(200,10)==0xffffffff); // Triforce retained.
    assert(pixel(200,215)==0xffffffff && pixel(200,216)==0);
    assert(pixel(30,195)==0 && pixel(380,135)==0);
    // Every tile location is unique, including row and tile boundaries.
    std::array<bool,Width*Height> seen{};
    for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x) {
        auto at=TiledOffset(x,y);assert(at<seen.size() && !seen[at]);seen[at]=true;
    }
    Reader r{Bytes(256+source.size())};
    Put(r.bytes,0,40);Put(r.bytes,12,40);Put(r.bytes,16,64);
    Put(r.bytes,28,104);Put(r.bytes,32,64);Put(r.bytes,36,256);
    Put(r.bytes,48,24); // Root's first child.
    Put(r.bytes,68,0xffffffff);Put(r.bytes,76,0);Put(r.bytes,84,8);
    for(unsigned i=0;i<4;++i)r.bytes[88+2*i]=std::string_view("misc")[i];
    Put(r.bytes,108,0xffffffff);Put(r.bytes,120,source.size());Put(r.bytes,132,28);
    for(unsigned i=0;i<14;++i)r.bytes[136+2*i]=std::string_view("boss_goma.ctxb")[i];
    std::copy(source.begin(),source.end(),r.bytes.begin()+256);
    assert(ReadSource(r)==source && r.reads<12);
    auto invalid=r;Put(invalid.bytes,112,0xfffffff0);Put(invalid.bytes,116,0xffffffff);
    assert(ReadSource(invalid).empty());
    invalid=r;Put(invalid.bytes,132,0xfffffff0);assert(ReadSource(invalid).empty());
    invalid=r;invalid.bytes[88]='x';Put(invalid.bytes,68,24);invalid.reads=0;
    assert(ReadSource(invalid).empty() && invalid.reads<16); // Cyclic directory list.
    invalid=r;Put(invalid.bytes,28,0xfffffff0);assert(ReadSource(invalid).empty());
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary);
        Bytes bytes((std::istreambuf_iterator<char>(file)),{});
        auto pixels=Extract(bytes,[](auto value,unsigned x,unsigned y) {
            auto p=Pica::Texture::SampleETC1Subtile(value,x,y);return std::array{p.r(),p.g(),p.b()};
        });
        assert(pixels);
        if(argc>3) {
            struct DiskReader {
                std::ifstream file;
                std::size_t size=0,read_bytes=0;
                explicit DiskReader(const char* path):file(path,std::ios::binary|std::ios::ate) {
                    assert(file);size=std::size_t(file.tellg())-0x1000;
                }
                std::size_t GetSize() const {return size;}
                std::size_t ReadFile(std::size_t at,std::size_t count,std::uint8_t* out) {
                    file.seekg(at+0x1000);file.read(reinterpret_cast<char*>(out),count);
                    read_bytes+=file.gcount();return file.gcount();
                }
            } disk(argv[3]);
            assert(ReadSource(disk)==bytes);
            std::cout<<"Loaded source directly from RomFS using "<<disk.read_bytes<<" bytes of reads\n";
        }
        if(argc>2) {
            std::ofstream out(argv[2],std::ios::binary);
            out<<"P7\nWIDTH "<<Width<<"\nHEIGHT "<<Height<<"\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
            for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x) {
                auto p=(*pixels)[TiledOffset(x,255-y)];
                for(int c=3;c>=0;--c)out.put(p>>(8*c));
            }
        }
    }
    std::cout<<"Map frame parser, mask, tiling and bounded RomFS checks passed\n";
}
