// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/shard_text.h"
#include <cassert>
#include <fstream>
#include <iostream>
struct Memory {
    const std::vector<std::uint8_t>& bytes;
    std::optional<std::uint32_t> Read(std::uint32_t at) {
        if(at==0x800004)return 0x100000;
        if(at<0x100000 || at-0x100000+4>bytes.size())return {};
        return OOT3D::Messages::Read32(bytes,at-0x100000);
    }
};
int main(int argc,char** argv) {
    assert(argc==2);using namespace OOT3D;
    std::ifstream in(argv[1],std::ios::binary);assert(in);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),{}};
    assert(Messages::ApplyAll(bytes));const auto original=bytes;
    assert(ShardText::Prepare(bytes));assert(std::equal(original.begin(),original.end(),bytes.begin()));
    Memory memory{bytes};const auto variants=ShardText::variants.load();
    for(auto id:{0x68u,0x76fu})for(bool rumble:{false,true}) {
        std::array<std::uint32_t,16> r{};r[0]=0x800000;r[1]=id;r[14]=0x900000;
        const auto before=r;Rumble::enabled=rumble;
        assert(ShardText::Select(memory,r)==rumble);
        if(!rumble)assert(r==before);
        else {
            const auto entry=r[0]-0x100000;assert(Messages::Read32(bytes,entry)==id && r[15]==r[14]);
            for(unsigned slot:{1u,5u,7u}) {
                const auto off=Messages::Read32(bytes,entry+16+slot*8),size=Messages::Read32(bytes,entry+20+slot*8);
                const std::string text(bytes.begin()+off,bytes.begin()+off+size);
                assert(text.find(slot==1?"vibrat":slot==5?"vibr":"vibra")!=std::string::npos);
                for(auto sound:{"hear it","résonne","oreille","tintineo","lo oyes","un ruido"})assert(text.find(sound)==std::string::npos);
            }
        }
    }
    Rumble::enabled=true;std::array<std::uint32_t,16> r{};r[0]=0x800000;r[1]=0x4078;
    assert(!ShardText::Select(memory,r));r[1]=0x68;bytes[variants->signature]^=1;assert(!ShardText::Select(memory,r));
    auto invalid=original;invalid[0]=0;const auto copy=invalid;assert(!ShardText::Prepare(invalid) && invalid==copy);
    std::cout<<"Shard text: six localized variants, original text preserved, live toggle, unrelated messages and archive guards passed\n";
}
