// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/control_swap_text.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
struct Memory {
    std::map<std::uint32_t,std::uint32_t> words;
    std::optional<std::uint32_t> Read(std::uint32_t at) {const auto it=words.find(at);return it==words.end()?std::nullopt:std::optional{it->second};}
};
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);assert(f);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),{}};
    assert(Messages::ApplyAll(bytes)==234);const auto original=bytes;
    assert(ControlSwap::PrepareText(bytes));
    const auto v=ControlSwap::variants.load();assert(v && v->entries.size()>10);
    assert(std::equal(original.begin(),original.end(),bytes.begin()));
    const auto count=Messages::Read32(bytes,8);
    unsigned replacements=0;
    for(unsigned i=0;i<count;++i) {
        const auto at=16+i*96,id=Messages::Read32(bytes,at);
        const auto it=v->entries.find(id);if(it==v->entries.end())continue;
        for(unsigned slot=0;slot<10;++slot) {
            const auto oldoff=Messages::Read32(bytes,at+16+slot*8),oldsize=Messages::Read32(bytes,at+20+slot*8);
            const auto newoff=Messages::Read32(bytes,it->second+16+slot*8),newsize=Messages::Read32(bytes,it->second+20+slot*8);
            if(!oldsize) {assert(!newsize);continue;}
            const auto back=ControlSwap::SwapText(std::span(bytes).subspan(newoff,newsize));
            assert(back && std::equal(back->begin(),back->end(),bytes.begin()+oldoff,bytes.begin()+oldoff+oldsize));
            replacements+=oldoff!=newoff;
            if(id==0x103b && (slot==1 || slot==5 || slot==7)) {
                const std::string text(bytes.begin()+newoff,bytes.begin()+newoff+newsize);
                assert(text.find("SELECT")!=text.npos && text.find("START")==text.npos);
            }
        }
    }
    // A binary command payload resembling START must remain untouched.
    const std::vector<std::uint8_t> command{127,14,'S','T','A','R','T',127,0};
    assert(ControlSwap::SwapText(command)==command);
    assert(!ControlSwap::SwapText(std::array<std::uint8_t,2>{127,14}));
    Memory m;constexpr unsigned engine=0x800000,base=0x900000;
    m.words[engine+4]=base;m.words[base]=0x4d51;
    m.words[base+v->signature]=0x53574150;m.words[base+v->signature+4]=0x53544152;
    std::array<std::uint32_t,16> r{};r[0]=engine;r[1]=0x103b;r[14]=0x1234;
    ControlSwap::enabled=false;assert(!ControlSwap::SelectText(m,r));
    ControlSwap::enabled=true;assert(ControlSwap::SelectText(m,r));
    assert(r[0]==base+v->entries.at(0x103b) && r[15]==0x1234);
    for(unsigned keys=0;keys<4096;++keys) {
        assert(ControlSwap::Remap(ControlSwap::Remap(keys,true),true)==keys);
        assert(ControlSwap::Remap(keys,false)==keys);
    }
    ControlSwap::Input input;unsigned keys=8;input.Filter(keys,true);assert(!keys); // Don't synthesize a toggle-time press.
    keys=0;input.Filter(keys,true);keys=8;input.Filter(keys,true);assert(keys==4);
    keys=4;input.Filter(keys,true);assert(keys==8);
    keys=0;input.Filter(keys,false);keys=8;input.Filter(keys,false);assert(keys==8); // Title/file select unaffected.
    for(bool swapped:{false,true}) {
        ControlSwap::enabled=swapped;input={};
        keys=0;input.Filter(keys,true,true);
        keys=4;input.Filter(keys,true,true);assert(keys==4); // Select never becomes Songs' logical Start.
        keys=0;input.Filter(keys,true,true);
        keys=8;input.Filter(keys,true,true);assert(keys==8); // Physical Start always opens Songs.
        keys=0;input.Filter(keys,true,false);
        keys=8;input.Filter(keys,true,false);assert(keys==(swapped?4:8)); // Restore gameplay preference.
    }
    ControlSwap::enabled=false;input={};
    keys=4;input.Filter(keys,true,false);assert(keys==4);
    // Both shoulder directions, every button combination, and held-toggle
    // suppression. Other buttons must survive the remap unchanged.
    for(unsigned packed=0;packed<16;++packed) {
        ControlSwap::Shoulders s;ControlSwap::shoulders=true;
        unsigned b=0,t=0;s.Filter(b,t,true);
        b=0x401|((packed&1)?0x200:0)|((packed&2)?0x100:0);t=packed>>2;
        s.Filter(b,t,true);
        assert((b&~0x300u)==0x401 && t==(packed&3));
        assert(bool(b&0x200)==bool(packed&4) && bool(b&0x100)==bool(packed&8));
    }
    ControlSwap::Shoulders s;ControlSwap::shoulders=false;
    unsigned b=0x200,t=0;s.Filter(b,t,true);ControlSwap::shoulders=true;
    b=0x200;s.Filter(b,t,true);assert(b==0 && t==0);
    b=0;s.Filter(b,t,true);b=0x200;s.Filter(b,t,true);assert(b==0 && t==1);
    // Icon commands and plain button names in every localized table have
    // shoulder-only and combined variants; unrelated command bytes survive.
    assert(ControlSwap::SwapText(command,false,true)==command);
    const std::vector<std::uint8_t> controls{127,0x24,0x0a,' ',127,0x24,0x0b,' ','Z','L',' ','Z','R',127,0};
    const auto swapped=ControlSwap::SwapText(controls,false,true);assert(swapped);
    assert(std::string(swapped->begin(),swapped->end()-2)=="ZL ZR L R");
    for(unsigned slot:{1u,5u,7u}) {
        unsigned changed=0;
        for(unsigned i=0;i<count;++i) {
            const auto at=16+i*96,id=Messages::Read32(bytes,at);
            const auto it=v->shoulder_entries.find(id);if(it==v->shoulder_entries.end())continue;
            const auto off=Messages::Read32(bytes,at+16+slot*8),size=Messages::Read32(bytes,at+20+slot*8);
            if(!size)continue;
            const auto expected=ControlSwap::SwapText(std::span(bytes).subspan(off,size),false,true);
            const auto dest=Messages::Read32(bytes,it->second+16+slot*8),length=Messages::Read32(bytes,it->second+20+slot*8);
            assert(expected && expected->size()==length && std::equal(expected->begin(),expected->end(),bytes.begin()+dest));
            changed+=off!=dest;
        }
        assert(changed>10);
    }
    for(unsigned mode=1;mode<=3;++mode) {
        ControlSwap::enabled=mode&1;ControlSwap::shoulders=mode&2;r[0]=engine;r[1]=0x103b;
        const auto& map=mode==1?v->entries:mode==2?v->shoulder_entries:v->both_entries;
        assert(ControlSwap::SelectText(m,r)==map.contains(0x103b));
        if(map.contains(0x103b))assert(r[0]==base+map.at(0x103b));
    }
    ControlSwap::enabled=true;ControlSwap::shoulders=false;input={};
    keys=0;input.Filter(keys,true,false);
    keys=8;input.Filter(keys,true,false);assert(keys==4);
    keys=8;input.Filter(keys,true,true);assert(keys==0); // Held Start cannot synthesize a Songs press.
    keys=0;input.Filter(keys,true,true);
    keys=8;input.Filter(keys,true,true);assert(keys==8);
    keys=8;input.Filter(keys,true,false);assert(keys==0); // Nor open Items on ocarina exit.
    keys=0;input.Filter(keys,true,false);
    keys=8;input.Filter(keys,true,false);assert(keys==4);
    ControlSwap::enabled=false;
    std::cout<<"Control swaps: "<<replacements<<" Start/Select strings, English/French/Spanish shoulder references, combined toggles, command integrity and input edges passed\n";
}
