// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/price_text.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::Prices;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
};
std::vector<std::uint8_t> Load(const char* path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);assert(f);
    std::vector<std::uint8_t> b(f.tellg());f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),b.size());assert(f);return b;
}
std::span<const std::uint8_t> Text(const std::vector<std::uint8_t>& b,unsigned i,unsigned slot) {
    const auto at=16+i*96+16+slot*8;
    return std::span(b).subspan(Messages::Read32(b,at),Messages::Read32(b,at+4));
}
int main(int argc,char**argv) {
    assert(argc==3);Memory m;const auto code=Load(argv[1]);
    for(unsigned i=0;i+4<=code.size();i+=4)m.words[0x100000+i]=Messages::Read32(code,i);
    assert(CodeMatches(m));
    for(const auto& site:Sites) {
        m.words[site.pc]^=1;assert(!CodeMatches(m));m.words[site.pc]^=1;
        assert(Instruction(site.pc,site.instruction^1)==(site.instruction^1));
    }
    // Exhaustive affordability boundaries, including capped purchases.
    for(unsigned factor:{1u,2u,3u})for(int cost=1;cost<=500;++cost)for(int wallet=0;wallet<=500;++wallet)
        assert((Funds(wallet,factor)>=cost)==(wallet>=Cost(cost,factor)));
    // Keep only sparse runtime memory after the ROM guards have been checked.
    for(U32 pc:{0x16d380u,0x16d3b8u,0x130184u,0x19090cu,0x1b7b18u,0x25a448u})
        assert(Instruction(pc,*m.Read(pc))==*m.Read(pc));
    m.words.clear();
    Difficulty::price_text_ready=true;
    for(unsigned factor:{1u,2u,3u}) {
        Difficulty::active_price_multiplier=factor;
        for(unsigned i=0;i<Sites.size();++i) {
            const auto& site=Sites[i];
            assert(Instruction(site.pc,site.instruction)==(0xef000000|(FirstSVC+i)));
            for(int money:{0,9,10,19,20,29,30,79,80,99,100,149,150,179,180,199,200,299,300,399,400,499,500}) {
                std::array<U32,16> r{};
                for(unsigned k=0;k<16;++k)r[k]=0x8000000+4*k;
                if(i<FundsCount) {
                    r[(site.instruction>>16)&15]=Wallet-0x48;m.words[Wallet]=0xabcd0000|money;
                    auto expected=r;expected[(site.instruction>>12)&15]=Funds(money,factor);
                    const auto before=m.words;Handle(m,r,i);assert(r==expected && m.words==before);
                    // The same instruction with an unrelated base is never treated as the wallet.
                    r[(site.instruction>>16)&15]=0x9000000;m.words[0x9000048]=0x12340064;
                    Handle(m,r,i);assert(r[(site.instruction>>12)&15]==100);
                } else {
                    for(int sign:{-1,1}) {
                        r[0]=U32(sign*money);r[14]=0x8888888;
                        auto expected=r;expected[0]=sign<0?U32(-Cost(money,factor)):U32(money);
                        expected[15]=ChangeRupees;if(site.instruction&0x01000000)expected[14]=site.pc+4;
                        const auto before=m.words;Handle(m,r,i);assert(r==expected && m.words==before);
                    }
                }
            }
        }
    }
    Difficulty::price_text_ready=false;assert(Factor()==1);
    auto original=Load(argv[2]);const auto table_count=Messages::Read32(original,8);
    for(unsigned factor:{2u,3u})for(bool controls:{false,true}) {
        auto base=original;if(controls)assert(Messages::ApplyAll(base)==90);
        auto bytes=base;assert(ApplyText(bytes,factor)==PriceMessageCount);
        unsigned changed=0;
        for(unsigned i=0;i<table_count;++i)for(unsigned slot=0;slot<10;++slot) {
            const auto a=Text(base,i,slot),b=Text(bytes,i,slot);
            if(std::equal(a.begin(),a.end(),b.begin(),b.end()))continue;
            ++changed;assert(slot==1 || slot==5 || slot==7);
            const auto id=Messages::Read32(base,16+i*96);
            const auto it=std::find_if(std::begin(PriceMessages),std::end(PriceMessages),[&](const auto& p) {
                return p.id==id && p.slot==slot && p.fingerprint==Messages::Fingerprint(a);
            });assert(it!=std::end(PriceMessages));
            // Check exact numeric substitutions and every untouched byte/command.
            std::vector<std::uint8_t> expected;unsigned cursor=0;
            for(const auto& edit:it->edits) {
                expected.insert(expected.end(),a.begin()+cursor,a.begin()+edit.offset);
                auto price=std::to_string(std::min(500,int(edit.cost*factor)));
                expected.insert(expected.end(),price.begin(),price.end());cursor=edit.offset+edit.remove;
            }
            expected.insert(expected.end(),a.begin()+cursor,a.end());
            assert(std::equal(expected.begin(),expected.end(),b.begin(),b.end()));
        }
        assert(changed==PriceMessageCount);
        auto once=bytes;assert(ApplyText(bytes,factor)==0 && bytes==once); // No double scaling.
        auto corrupt=base;
        for(unsigned i=0;i<table_count;++i)if(Messages::Read32(corrupt,16+i*96)==6) {
            const auto at=Messages::Read32(corrupt,16+i*96+24);corrupt[at+2]^=1;break;
        }
        auto untouched=corrupt;assert(ApplyText(corrupt,factor)==0 && corrupt==untouched);
    }
    auto off=original;assert(ApplyText(off,1)==0 && off==original);
    std::cout<<"Prices: all guarded native sites, exact affordability boundaries, capped costs, reward/debt exclusions, and 324 EN/FR/ES messages passed\n";
}
