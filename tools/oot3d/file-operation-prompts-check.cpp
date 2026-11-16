// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include <cassert>
#include <map>
#include <iostream>
namespace F=OOT3D::FileSelect;
struct Memory {
    std::map<unsigned,unsigned> words;
    std::optional<unsigned> Read(unsigned a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
};
int main() {
    Memory m;
    for(unsigned state:{39u,42u,44u,24u,25u,26u,27u,28u}) {
        m.words[F::Owner+0x10]=state;m.words[F::Owner+0x2c]=0;
        const auto original=m.words;assert(F::FileOperationBusy(m));
        F::FileOperationPromptTransition fade;
        for(unsigned i=1;i<=6;++i) {
            fade.Frame(F::FileOperationBusy(m));assert(std::abs(fade.opacity-i/6.f)<.00001f);
        }
        assert(fade.opacity==1 && fade.clock_us>0 && m.words==original);
        m.words[F::Owner+0x10]=state<30?32:2;assert(!F::FileOperationBusy(m));
        for(unsigned i=1;i<=6;++i) {
            fade.Frame(F::FileOperationBusy(m));assert(std::abs(fade.opacity-(1-i/6.f))<.00001f);
        }
        assert(fade.opacity==0 && fade.clock_us==0);
    }
    for(unsigned state:{0u,2u,8u,12u,17u,23u,29u,32u,37u,38u,42u}) {
        m.words[F::Owner+0x10]=state;m.words[F::Owner+0x2c]=1;
        assert(!F::FileOperationBusy(m));
    }
    F::FileOperationPromptTransition fade;
    fade.Frame(true);fade.Frame(false);assert(fade.opacity==0);
    std::cout<<"Copy/delete prompt crossfade, busy states, cancellation, completion, short operation and read-only behavior passed\n";
}
