// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/minimap_control.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
using namespace OOT3D::MinimapControl;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {words[a]=v;}
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream in(argv[1],std::ios::binary);assert(in);
    U32 word;for(U32 a=0x100000;in.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(CodeMatches(m) && Ocarina::CodeMatches(m));
    for(unsigned i=0;i<FadeSites.size();++i) {
        auto s=FadeSites[i];assert(Instruction(s.pc,s.instruction)==(0xef000000|FirstSVC+1+i));
    }
    constexpr U32 play=0x10000000,model=0x10010000;
    m.words[HUDFade::UI+0xc]=play;m.words[play+0x100]=0x203;
    m.words[play+0x104]=1;m.words[play+0x7f24]=0x100;
    m.words[play+0x5c2c]=m.words[play+0x7f10]=0;m.words[HUDFade::Main+0x34]=2;
    m.words[Owner+8]=model;m.words[Owner+0x60]=0;m.words[Action+0x68]=1;
    m.words[SaveMenu::Owner+0x28]=0;m.words[Ocarina::Owner+0x14]=0;
    for(int interval:{8333,16667,33333}) {
        InstrumentFade fade;std::array<U32,16> r{};r[4]=Owner;
        std::int64_t now=1000000;
        r[0]=play;fade.Handle(m,r,3,false,now);fade.Handle(m,r,2,false,now);
        assert(fade.opacity==1 && r[0]==model);
        m.words[Ocarina::Owner+0x14]=12;
        // Instrument/modal HUD state fails GameplayReady. It must NOT reset
        // the map envelope, and the old late hook is never called here.
        m.words[HUDFade::Main+0x34]=0x17;m.words[play+0x7f24]=0;
        for(int i=0;i<25;++i) {
            now+=interval;r[15]=FadeSites[0].pc+4;
            r[0]=play;fade.Handle(m,r,3,false,now);
            const float expected=std::max(0.f,1.f-float(interval*(i+1))/HUDFade::FadeUs);
            assert(std::abs(fade.opacity-expected)<.00001f);
            const float before=fade.opacity;
            r[0]=play;fade.Handle(m,r,3,false,now);assert(fade.opacity==before);
            if(fade.opacity>0) {
                assert(r[15]==0x441f08 && r[14]==0x42b9d8);
                fade.Handle(m,r,2,false,now);assert(fade.submitted==1000000);
                // Both possibilities: native early-return, or native code
                // reaching the old map block. The latter must not double draw.
                r[15]=FadeSites[0].pc+4;fade.Handle(m,r,1,false,now);
                assert(r[15]==0x42bb94);
            }
        }
        assert(fade.opacity==0);
        m.words[Ocarina::Owner+0x14]=0;
        m.words[HUDFade::Main+0x34]=2;m.words[play+0x7f24]=0x100;
        now+=200000;r[0]=play;fade.Handle(m,r,3,false,now);
        assert(fade.opacity==0); // Native gates can delay resubmission.
        fade.Handle(m,r,2,false,now);
        const auto start=now;
        for(int i=0;i<25;++i) {
            now+=interval;r[0]=play;fade.Handle(m,r,3,false,now);fade.Handle(m,r,2,false,now);
            assert(std::abs(fade.opacity-std::min(1.f,float(now-start)/HUDFade::FadeUs))<.00001f);
            assert(!fade.retain);
        }
        assert(fade.opacity==1);
        // A map absent before instrument use must never be forced into view.
        fade={};r[0]=play;fade.Handle(m,r,3,false,now);
        m.words[Ocarina::Owner+0x14]=12;r[15]=FadeSites[0].pc+4;
        r[0]=play;fade.Handle(m,r,3,false,now+interval);assert(!fade.retain && r[15]==FadeSites[0].pc+4);
        r[0]=play;fade.Handle(m,r,3,true,now+interval);assert(!fade.initialized);
        m.words[Ocarina::Owner+0x14]=0;
        r[0]=play;fade.Handle(m,r,3,false,now+interval);
        fade.Handle(m,r,2,false,now+interval);
        m.words[Ocarina::Owner+0x14]=12;
        m.words[play+0x7f10]=0x10000; // Scene transition must cancel retention.
        r[0]=play;r[15]=FadeSites[2].pc+4;
        fade.Handle(m,r,3,false,now+interval*2);
        assert(!fade.initialized && r[15]==FadeSites[2].pc+4);
        m.words[play+0x7f10]=0;m.words[Ocarina::Owner+0x14]=0;
    }
    std::cout<<"Native map hook guards, instrument fades at 30/60/120 Hz, stereo, submission retention and pause reset passed\n";
}
