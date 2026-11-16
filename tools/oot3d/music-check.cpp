// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/music.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::Music;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {
        const auto i=words.find(at);return i==words.end()?std::nullopt:std::optional<U32>{i->second};
    }
    void Write(U32 at,U32 word) {words.at(at)=word;}
};
int main(int argc,char** argv) {
    Memory m;
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
        for(U32 at=0x100000;file.read(reinterpret_cast<char*>(&word),4);at+=4)m.words[at]=word;
        assert(CodeMatches(m));
        assert(Instruction(PC,Word)==(0xef000000|SVC));
        m.words[0x30ceec]^=1;assert(!CodeMatches(m));
        assert(Instruction(PC,Word^1)==(Word^1));
        m.words.clear();
    }
    constexpr U32 sound=0x8000000,command=0x8010000;
    constexpr U32 gain=std::bit_cast<U32>(.37f);
    m.words={{sound,SequenceVTable},{sound+0x9c,0x010005c4},
             {sound+0xb0,0x3f000000},{command+0x14,0},{command+0x18,0x3f800000}};
    for(U32 id:{0x01000585u,0x01000588u,0x0100058cu,0x010005c4u,0x010005dau,0x010005efu}) {
        m.words[sound+0x9c]=id;const auto before=m.words;
        AudioSettings::mute_music=false;Store(m,sound,command,gain);
        assert(m.words.at(command+0x14)==gain);
        AudioSettings::mute_music=true;Store(m,sound,command,gain);
        assert(m.words.at(command+0x14)==0);
        AudioSettings::mute_music=false;Store(m,sound,command,gain);
        assert(m.words.at(command+0x14)==gain);
        for(auto [at,value]:before)if(at!=command+0x14)assert(m.words.at(at)==value);
    }
    AudioSettings::mute_music=true;
    for(U32 id:{0x01000498u,0x01000575u,0x01000590u,0x01000593u,0x010005a0u,
               0x010005b1u,0x010005b6u,0x010005aeu,0x010005c0u,0xffffffffu}) {
        m.words[sound+0x9c]=id;Store(m,sound,command,gain);
        assert(m.words.at(command+0x14)==gain);
    }
    m.words[sound+0x9c]=0x01000000;
    Store(m,sound,command,gain);assert(m.words.at(command+0x14)==gain);
    m.words[sound]=StreamVTable;
    Store(m,sound,command,gain);assert(m.words.at(command+0x14)==0);
    m.words[sound]=0x4eca20; // WaveSound: overlapping IDs must not silence effects.
    m.words[sound+0x9c]=0x010005c4;
    Store(m,sound,command,gain);assert(m.words.at(command+0x14)==gain);
    assert(!IsMusicSound(m,0));assert(!IsMusicSound(m,sound+1));
    AudioSettings::mute_music=false;
    std::cout<<"Native music gain signature, BGM/Field/credits classification, preserved songs/effects and live unmute passed\n";
}
