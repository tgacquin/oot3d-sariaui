// SPDX-License-Identifier: GPL-2.0-or-later
// c++ -std=c++20 -Isrc tools/oot3d/lens-layout-check.cpp -o /tmp/oot-lens-check
// /tmp/oot-lens-check [path/to/romfs/actor/zelda_keep.zar]
#include "oot3d/lens.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

using namespace OOT3D::Lens;
constexpr std::array<unsigned,18> Indices{0,1,2,1,3,2,4,0,5,0,2,5,6,4,7,4,5,7};

int main(int argc,char** argv) {
    assert(Candidate(18,1) && Candidate(18,3));
    assert(!Candidate(6,1) && !Candidate(18,0) && !Candidate(18,7));
    assert(Geometry(Vertices));
    auto mesh=Vertices;
    std::reverse(mesh.begin(),mesh.end());
    assert(Geometry(mesh));
    mesh[0]=mesh[1]; assert(!Geometry(mesh));
    mesh=Vertices; mesh[0][0]+=1; assert(!Geometry(mesh));
    mesh=Vertices; mesh[0][2]=1; assert(!Geometry(mesh));
    mesh=Vertices; mesh[0][0]=std::numeric_limits<float>::quiet_NaN();
    assert(!Geometry(mesh));

    const std::array<Row,4> projection{{{0,-2.f/240,0,1},{-2.f/400,0,0,1},
                                      {0,0,1.f/1800,1.f/3},{0,0,0,1}}};
    for(float scale:{.25f,.3f,.5f,1.f,1.5f}) {
        const std::array<Row,3> model{{{-scale,0,0,200},{0,-scale,0,120},{0,0,scale,-1200}}};
        assert(Transform(projection,model));
        std::array<Row,18> clip{};
        for(unsigned i=0;i<Indices.size();++i) {
            const auto& v=Vertices[Indices[i]];
            Row world{};
            for(unsigned r=0;r<3;++r)
                for(unsigned c=0;c<4;++c) world[r]+=model[r][c]*v[c];
            world[3]=1;
            for(unsigned r=0;r<4;++r)
                for(unsigned c=0;c<4;++c) clip[i][r]+=projection[r][c]*world[c];
        }
        assert(ClipGeometry(clip,projection,model));
        auto bad=clip; bad[0][3]=2; assert(!ClipGeometry(bad,projection,model));
        bad=clip; bad[0][0]+=.01f; assert(!ClipGeometry(bad,projection,model));
        bad=clip; bad[0][2]+=.01f; assert(!ClipGeometry(bad,projection,model));
        bad=clip; bad.fill(clip[0]); assert(!ClipGeometry(bad,projection,model));
        bad=clip; bad[0][1]=std::numeric_limits<float>::infinity();
        assert(!ClipGeometry(bad,projection,model));
        auto moved=model; moved[0][3]+=1; assert(!Transform(projection,moved));
        moved=model; moved[1][1]*=2; assert(!Transform(projection,moved));
        moved=model; moved[2][3]=std::numeric_limits<float>::quiet_NaN();
        assert(!Transform(projection,moved));
        auto other_projection=projection; other_projection[3]={0,0,-1,0};
        assert(!Transform(other_projection,model)); // Perspective/world draw.
        other_projection=projection; other_projection[1][0]=-2.f/320;
        assert(!Transform(other_projection,model)); // Bottom screen.
        other_projection=projection; other_projection[2][2]=std::numeric_limits<float>::infinity();
        assert(!Transform(other_projection,model));
        // The ordinary widescreen squeeze leaves a gap at the native side
        // edge. Bypassing it places both cover and mask on the viewport edge.
        if(scale==.25f) {
            assert(Near(clip[1][1],1));
            assert(clip[1][1]*OOT3D::Widescreen::HorizontalScale<1);
        }
    }
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary); assert(file);
        const std::string bytes((std::istreambuf_iterator<char>(file)),{});
        const auto u32=[&](std::size_t at) {
            assert(at+4<=bytes.size()); std::uint32_t n;
            std::memcpy(&n,bytes.data()+at,4); return n;
        };
        unsigned masks=0,covers=0;
        for(std::size_t at=0;(at=bytes.find("cmb ",at))!=std::string::npos;++at) {
            if(at+32>bytes.size())continue;
            const std::string name(bytes.data()+at+16,strnlen(bytes.data()+at+16,16));
            if(name!="wipe_makoto_alp" && name!="wipe_makoto")continue;
            const auto end=at+u32(at+4); assert(end<=bytes.size());
            const auto vatr=bytes.find("vatr",at); assert(vatr<end);
            assert(u32(vatr+12)==8*3*sizeof(float));
            const auto positions=vatr+u32(vatr+16);
            assert(positions+8*3*sizeof(float)<=end);
            std::array<Row,8> native{};
            for(unsigned i=0;i<native.size();++i) {
                std::memcpy(native[i].data(),bytes.data()+positions+i*12,12);
                native[i][3]=1;
            }
            assert(Geometry(native));
            const auto indices=at+u32(at+0x3c); assert(indices+36<=end);
            for(unsigned i=0;i<Indices.size();++i) {
                std::uint16_t n; std::memcpy(&n,bytes.data()+indices+2*i,2);
                assert(n==Indices[i]);
            }
            if(name=="wipe_makoto_alp")++masks; else ++covers;
        }
        assert(masks==2 && covers==1);
    }
    std::cout<<"Lens widescreen mesh/transform checks passed\n";
}
