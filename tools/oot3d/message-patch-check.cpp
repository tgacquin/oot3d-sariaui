// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/message_patch.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <set>
using namespace OOT3D::Messages;
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    std::ifstream f(argv[1],std::ios::binary);assert(f);
    const std::vector<std::uint8_t> original{std::istreambuf_iterator<char>(f),{}};
    auto patched=original;assert(Apply(patched)==78);
    const auto count=Read32(original,8);
    std::set<unsigned> changed;
    for(unsigned i=0;i<count;++i) {
        const auto entry=16+i*96;
        assert(std::equal(original.begin()+entry,original.begin()+entry+16,patched.begin()+entry));
        for(unsigned slot=0;slot<10;++slot) {
            const auto at=entry+16+slot*8;
            const auto old_off=Read32(original,at),old_size=Read32(original,at+4);
            const auto off=Read32(patched,at),size=Read32(patched,at+4);
            if(off==old_off) {assert(size==old_size);continue;}
            assert(slot==unsigned(Language::English));assert(off>=original.size() && size>2);
            assert(off+size<=patched.size() && patched[off+size-2]==127 && patched[off+size-1]==0);
            changed.insert(Read32(original,entry));
        }
    }
    assert(changed.size()==78);
    assert(changed.contains(0x1017) && changed.contains(0x103f) && changed.contains(0x10c7));
    // Every original message payload remains byte-identical, including all
    // French/Spanish strings, unedited English dialogue and alternate modes.
    const auto payload=16+count*96;
    assert(std::equal(original.begin()+payload,original.end(),patched.begin()+payload));
    assert(!changed.contains(0x010f) && !changed.contains(0x103b)); // Correct START/save instructions.
    const auto once=patched;assert(Apply(patched)==0 && patched==once);
    for(auto language:{Language::French,Language::Spanish}) {
        auto other=original;assert(Apply(other,language)==78);
        for(unsigned i=0;i<count;++i)for(unsigned slot=0;slot<10;++slot) {
            const auto at=16+i*96+16+slot*8;
            if(slot!=unsigned(language))assert(Read32(other,at)==Read32(original,at));
        }
        const auto saved=other;assert(!Apply(other,language) && other==saved);
    }
    auto localized=original;assert(ApplyAll(localized)==234);
    for(unsigned i=0;i<count;++i)for(unsigned slot=0;slot<10;++slot) {
        const auto at=16+i*96+16+slot*8;
        if(Read32(localized,at)!=Read32(original,at)) {
            const auto id=Read32(original,16+i*96);
            if(slot!=1) {
                assert(slot==5 || slot==7);
                assert(changed.contains(id));
            }
        }
    }
    const auto all_once=localized;assert(!ApplyAll(localized) && localized==all_once);
    // A changed French/Spanish source must reject the entire transaction,
    // without leaving only the English portion installed.
    for(unsigned slot:{5u,7u}) {
        auto custom=original;
        for(unsigned i=0;i<count;++i)if(Read32(custom,16+i*96)==0x103f)
            custom[Read32(custom,16+i*96+16+slot*8)]^=1;
        const auto saved=custom;assert(!ApplyAll(custom) && custom==saved);
    }
    for(unsigned length:{0u,12u,16u,200u}) {
        std::vector<std::uint8_t> short_file(original.begin(),original.begin()+length);
        auto before=short_file;assert(!Apply(short_file) && short_file==before);
    }
    auto malformed=original;Write32(malformed,8,0xffffffff);auto before=malformed;
    assert(!Apply(malformed) && malformed==before);
    malformed=original;Write32(malformed,16+16+5*8,0xffffffff);
    Write32(malformed,16+20+5*8,128);before=malformed;
    assert(!Apply(malformed) && malformed==before);
    // A modified audited message rejects the entire patch transaction.
    malformed=original;
    for(unsigned i=0;i<count;++i)if(Read32(malformed,16+i*96)==0x1036) {
        const auto off=Read32(malformed,16+i*96+24);malformed[off]^=1;
    }
    before=malformed;assert(!Apply(malformed) && malformed==before);
    if(argc==3) {
        std::ofstream out(argv[2],std::ios::binary);assert(out);
        out.write(reinterpret_cast<const char*>(all_once.data()),all_once.size());assert(out);
    }
    std::cout<<"78 messages in each of English, French and Spanish patched; locale isolation and atomic installation verified; malformed and modified archives rejected\n";
}
