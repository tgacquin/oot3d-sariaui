// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include "oot3d/pause_prompt_assets.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
namespace F=OOT3D::FileSelect;
namespace P=OOT3D::PausePrompts;
using OOT3D::TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {
        auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};
    }
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
};
std::vector<std::uint8_t> Read(const std::string& path) {
    std::ifstream file(path,std::ios::binary);assert(file);
    return {std::istreambuf_iterator<char>(file),{}};
}
int main(int argc,char** argv) {
    assert(argc==2);
    const auto assets=P::MakeAssets(Read(std::string(argv[1])+"/message/parts.ctxb"),
                                    Read(std::string(argv[1])+"/message/us/ltn16.qbf"));
    assert(assets);
    static_assert(decltype(F::Hook::text)::TextureBytes==sizeof(P::Assets::text));
    constexpr U32 model=0x18000000,mesh=0x18010000,descriptor=0x18020000,material=0x18030000;
    Memory m;m.words[model]=0x4ebd60;m.words[model+4]=mesh;
    m.words[mesh]=descriptor;m.words[model+0x1dc]=material;
    auto hook=std::make_unique<F::Hook>();
    std::array<U32,16> regs{};
    const auto geometry=F::Prompts(F::Layout{},*assets,1);
    assert(geometry.text.count>0);
    hook->text.Begin(m,regs,0x300568,geometry.text,assets->text,model);
    assert(hook->text.Pending() && regs[15]==0x2a8bdc);
    assert(hook->text.pixels.size()==assets->text.size());
    // No empty label rows: both file-select strings reach visible atlas texels.
    for(unsigned row:{P::FileSelectText,P::FileBackText}) {
        unsigned visible=0;
        for(unsigned y=0;y<20;++y)for(unsigned x=0;x<P::TextWidth;++x)
            visible+=(assets->text[P::Tiled(x,P::TextHeight-1-(row*P::TextRowHeight+y))]&255)!=0;
        assert(visible>0);
    }
    std::cout<<"File-select prompt atlas size, native label pixels and guest text submission passed\n";
}
