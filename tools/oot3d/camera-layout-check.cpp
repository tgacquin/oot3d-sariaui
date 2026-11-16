// SPDX-License-Identifier: GPL-2.0-or-later
// c++ -std=c++20 -Isrc tools/oot3d/camera-layout-check.cpp -o /tmp/oot-camera-check
// /tmp/oot-camera-check [path/to/code-decompressed.bin]
#include "oot3d/camera.h"
#include "oot3d/widescreen.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
using namespace OOT3D::Camera;
struct Memory {
    std::map<U32, U32> words;
    std::optional<U32> Read(U32 at) {
        auto it = words.find(at);
        return it == words.end() ? std::nullopt : std::optional<U32>{it->second};
    }
};
bool Near(float a, float b) { return std::abs(a - b) < .0001f; }
int main(int argc, char** argv) {
    Memory memory;
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        assert(file);
        U32 word;
        for (U32 at = 0x100000; file.read(reinterpret_cast<char*>(&word), 4); at += 4)
            memory.words[at] = word;
        assert(CodeMatches(memory));
        // Native letterbox routine's horizontal extent and screen height.
        assert(memory.Read(0x2eab68)==std::optional<U32>{std::bit_cast<U32>(400.f)});
        assert(memory.Read(0x2eab88)==std::optional<U32>{std::bit_cast<U32>(240.f)});
        memory.words[0x478f64] ^= 1;
        assert(!CodeMatches(memory));
        memory.words.clear();
    }
    for (unsigned i = 0; i < Sites.size(); ++i) {
        assert(Instruction(Sites[i].pc, Sites[i].instruction) == (0xef000000 | (FirstSVC + i)));
        assert(Instruction(Sites[i].pc + 4, Sites[i].instruction) == Sites[i].instruction);
        assert(Instruction(Sites[i].pc, 0) == 0);
    }
    assert(Widen(52, 52) == 52 && Near(Widen(52, 70), 70));
    assert(Near(Widen(52, 77), 77));
    assert(Widen(30, 70) > 30 && Widen(30, 70) < 45);
    assert(Widen(45, 52) == 45);
    assert(Widen(0, 70) == 0 && Widen(180, 70) == 180);
    assert(Widen(52, std::numeric_limits<float>::quiet_NaN()) == 52);
    assert(Sanitize(-1) == 52 && Sanitize(100) == 77);
    constexpr U32 play = 0x10000000, view = play + 0x188;
    memory.words = {{0x5043e0, play}, {view, 0}, {view+4, 400}, {view+8, 0}, {view+12, 240}};
    std::array<U32, 64> fp{};
    fp[16] = std::bit_cast<U32>(52.f); fp[23] = 0x3c8efa35;
    const auto original = fp;
    const auto original_memory = memory.words;
    for (unsigned i = 0; i < 10; ++i) {
        Handle(memory, view, fp, 70);
        assert(Near(std::bit_cast<float>(fp[0]), 70.f * std::bit_cast<float>(fp[23])));
        for (unsigned j = 1; j < fp.size(); ++j) assert(fp[j] == original[j]);
        assert(memory.words == original_memory); // No accumulated primary/secondary widening.
    }
    // A distinct secondary FOV receives the same relative zoom, independently.
    fp[16] = std::bit_cast<U32>(30.f);
    Handle(memory, view, fp, 70);
    assert(Near(std::bit_cast<float>(fp[0]), Widen(30.f, 70.f) * std::bit_cast<float>(fp[23])));
    // Targeting and its return to orbit share the configured lens, without
    // modifying the native transition FOV or accumulating across projections.
    for(float configured:{52.f,65.f,77.f})for(float native_fov:{30.f,45.f,52.f,60.f}) {
        fp[16]=std::bit_cast<U32>(native_fov);
        const auto before=fp;
        for(unsigned frame=0;frame<10;++frame) {
            Handle(memory,view,fp,configured,true);
            assert(Near(std::bit_cast<float>(fp[0]),
                configured*std::bit_cast<float>(fp[23])));
            for(unsigned j=1;j<fp.size();++j)assert(fp[j]==before[j]);
            assert(memory.words==original_memory);
        }
        Handle(memory,view,fp,configured,false); // Dialogue still zooms.
        assert(Near(std::bit_cast<float>(fp[0]),Widen(native_fov,configured)*std::bit_cast<float>(fp[23])));
        Handle(memory,view+0x200,fp,configured,true); // Model previews are excluded.
        assert(Near(std::bit_cast<float>(fp[0]),native_fov*std::bit_cast<float>(fp[23])));
    }
    // House cap affects both projections without changing native camera state.
    memory.words[play+0x104]=0xabcd0034;
    for(float configured:{52.f,54.f,55.f,65.f,77.f}) {
        assert(Near(EffectiveFov(memory,view,52.f,configured),std::min(configured,55.f)));
        assert(Near(EffectiveFov(memory,view,30.f,configured),Widen(30.f,std::min(configured,55.f))));
        assert(EffectiveFov(memory,view,60.f,configured)==55.f);
        fp[16]=std::bit_cast<U32>(52.f);
        const auto before_memory=memory.words;
        for(const auto& site:Sites) {
            (void)site;
            Handle(memory,view,fp,configured);
            assert(Near(std::bit_cast<float>(fp[0]),std::min(configured,55.f)*std::bit_cast<float>(fp[23])));
            assert(fp[16]==std::bit_cast<U32>(52.f) && memory.words==before_memory);
        }
        Handle(memory,view+0x200,fp,configured,true);
        assert(Near(std::bit_cast<float>(fp[0]),52.f*std::bit_cast<float>(fp[23])));
    }
    memory.words[play+0x104]=0xabcd0025; // Temple of Time exterior, adult ruins.
    for(float configured:{52.f,54.f,56.f,65.f,77.f}) {
        assert(Near(EffectiveFov(memory,view,52.f,configured),std::min(configured,56.f)));
        assert(Near(EffectiveFov(memory,view,30.f,configured),Widen(30.f,std::min(configured,56.f))));
        assert(EffectiveFov(memory,view,80.f,configured)==56.f);
        fp[16]=std::bit_cast<U32>(52.f);
        const auto before=memory.words;
        for(unsigned projection=0;projection<Sites.size();++projection) {
            Handle(memory,view,fp,configured,true);
            assert(Near(std::bit_cast<float>(fp[0]),std::min(configured,56.f)*std::bit_cast<float>(fp[23])));
            assert(fp[16]==std::bit_cast<U32>(52.f) && memory.words==before);
        }
        assert(EffectiveFov(memory,view+0x200,80.f,configured)==80.f);
    }
    for(U32 scene:{0x23u,0x24u,0x43u,0x64u}) {
        memory.words[play+0x104]=scene;
        assert(Near(EffectiveFov(memory,view,52.f,77.f),77.f));
    }
    memory.words[play+0x104]=0; // Dungeon remains unrestricted.
    assert(Near(EffectiveFov(memory,view,52.f,77.f),77.f));
    // First-person look/aim uses a 62-degree ceiling in both world projections.
    // Keep zooms, tighter scene caps, targeting and non-gameplay views intact.
    {
        const auto saved=memory.words;
        constexpr U32 camera=play+0x364,player=0x11000000;
        memory.words[play+0xa64]=0;memory.words[camera+0x188]=7;
        memory.words[camera+0xd4]=play;memory.words[camera+0xd8]=player;
        for(U32 mode:{6u,7u,11u})for(U32 flags:{0u,0x18208u,0x100208u}) {
            memory.words[camera+0x18c]=mode;memory.words[player+0x1710]=flags;
            assert(FirstPersonView(memory,view));
            for(float configured:{52.f,60.f,62.f,70.f,77.f})for(float lens:{30.f,52.f,80.f}) {
                const float expected=std::min(Widen(lens,std::min(configured,62.f)),62.f);
                fp[16]=std::bit_cast<U32>(lens);
                const auto before=memory.words;
                for(unsigned projection=0;projection<Sites.size();++projection) {
                    Handle(memory,view,fp,configured);
                    assert(Near(std::bit_cast<float>(fp[0]),expected*std::bit_cast<float>(fp[23])));
                    assert(fp[16]==std::bit_cast<U32>(lens) && memory.words==before);
                }
                assert(EffectiveFov(memory,view+0x200,lens,configured)==lens);
            }
            for(auto [scene,cap]:std::array<std::pair<U32,float>,3>{{{0x34,55},{0x25,56},{0x2d,58}}}) {
                memory.words[play+0x104]=scene;
                assert(Near(EffectiveFov(memory,view,52,77),cap));
            }
            memory.words[play+0x104]=0;
        }
        memory.words[camera+0x18c]=0;memory.words[player+0x1710]=0x100000;
        assert(FirstPersonView(memory,view));
        assert(Near(EffectiveFov(memory,view,52,77),62));
        memory.words[player+0x1710]=0x18208;
        for(U32 mode:{0u,1u,2u,3u,4u,9u}) {
            memory.words[camera+0x18c]=mode;
            assert(!FirstPersonView(memory,view));
            assert(Near(EffectiveFov(memory,view,52,77),77));
        }
        memory.words[camera+0x18c]=6;
        memory.words[play+0xa64]=1;assert(!FirstPersonView(memory,view));
        memory.words[play+0xa64]=0;memory.words[camera+0x188]=0;
        assert(!FirstPersonView(memory,view));
        memory.words=saved;
    }
    memory.words.erase(play+0x104);
    fp[16] = original[16];
    Handle(memory, view, fp, 52);
    const auto native = fp[0];
    Handle(memory, view + 0x200, fp, 70); assert(fp[0] == native);
    memory.words[view+4] = 240; memory.words[view+12] = 400;
    Handle(memory, view, fp, 70); assert(fp[0] == native);
    memory.words[view+4] = 400;
    memory.words[view+12] = 320;
    Handle(memory, view, fp, 70); assert(fp[0] == native);
    memory.words.erase(0x5043e0);
    Handle(memory, view, fp, 70); assert(fp[0] == native);

    using namespace OOT3D::Widescreen;
    // Recorded native clear must stay full width; similar HUD/world draws must not.
    constexpr std::uint64_t shader = 0x6352a3be33cb99c6ULL, swizzle = 0xba817d0ef27276d0ULL;
    assert(ClearState(shader, swizzle, 4, 1, 0x1f11, 0, false, false));
    assert(!ClearState(shader, swizzle, 4, 1, 0x1f41, 0, false, false));
    assert(!ClearState(shader, swizzle, 4, 1, 0x1f11, 1, false, false));
    assert(!ClearState(shader, swizzle, 4, 1, 0x1f11, 0, true, false));
    assert(!ClearState(shader, swizzle, 4, 1, 0x1f11, 0, false, true));
    assert(!ClearState(shader ^ 1, swizzle, 4, 1, 0x1f11, 0, false, false));
    std::array<Row, 4> projection{{{0,-2.f/240,0,1}, {-2.f/400,0,0,1},
                                 {0,0,1.f/9,1.f/9}, {0,0,0,1}}};
    std::array<Row, 3> model{{{1,0,0,0}, {0,1,0,0}, {0,0,1,-10}}};
    std::array<Row, 4> vertices{{{0,0,0,1}, {400,0,0,1}, {0,240,0,1}, {400,240,0,1}}};
    assert(ClearGeometry(projection, model, vertices));
    vertices[1][0] = 100; assert(!ClearGeometry(projection, model, vertices));
    vertices[1][0] = 400;
    model[0][3] = 20; assert(!ClearGeometry(projection, model, vertices));
    model[0][3] = 0;
    projection[3] = {0,0,-1,0}; assert(!ClearGeometry(projection, model, vertices));
    std::array<Row, 6> clipped{{{1,1,-1,1}, {1,-1,-1,1}, {-1,1,-1,1},
                              {1,-1,-1,1}, {-1,-1,-1,1}, {-1,1,-1,1}}};
    assert(ClearClipGeometry(clipped));
    clipped[0][0] = .9375f; assert(!ClearClipGeometry(clipped));
    // Captured title-to-file-select fade. Keep coverage independent of opacity.
    assert(FadeState(shader, swizzle, 4, 1, 0xf10, 0, false, false, {1,0,6,7}));
    assert(!FadeState(shader, swizzle, 4, 1, 0xf10, 1, false, false, {1,0,6,7}));
    assert(!FadeState(shader, swizzle, 4, 1, 0x1f11, 0, false, false, {1,0,6,7}));
    assert(!FadeState(shader, swizzle, 4, 1, 0xf10, 0, false, false, {1,0,1,1}));
    assert(!FadeState(shader, swizzle, 4, 1, 0xf10, 0, false, true, {1,0,6,7}));
    projection = {{{0,-2.f/240,0,1}, {-2.f/400,0,0,1}, {0,0,1.f/3,-1.f/3}, {0,0,0,1}}};
    model = {{{400,0,0,200}, {0,240,0,120}, {0,0,1,-1}}};
    vertices = {{{-.5f,-.5f,0,1}, {.5f,-.5f,0,1}, {-.5f,.5f,0,1}, {.5f,.5f,0,1}}};
    assert(FadeGeometry(projection, model, vertices));
    assert(!ClearGeometry(projection, model, vertices));
    model[0][0] = 100; assert(!FadeGeometry(projection, model, vertices)); // Small UI panel.
    model[0][0] = 400; model[0][3] = 180;
    assert(!FadeGeometry(projection, model, vertices)); // Offset quad.
    model[0][3] = 200; projection[3] = {0,0,-1,0};
    assert(!FadeGeometry(projection, model, vertices)); // World geometry.
    clipped[0][0] = 1;
    for (auto& v : clipped) v[2] = -2.f/3;
    assert(FadeClipGeometry(clipped));
    clipped[1][1] = -.9375f; assert(!FadeClipGeometry(clipped));
    // Native Z-target/first-person bars: every opening/closing height and
    // both edges must expand horizontally in GPU- and CPU-shaded paths.
    projection={{{0,-2.f/240,0,1},{-2.f/400,0,0,1},
                 {0,0,1.f/1800,1.f/3},{0,0,0,1}}};
    const auto project_bar=[&] {
        std::array<Row,6> result{};
        constexpr std::array<unsigned,6> indices{0,2,1,1,2,3};
        for(unsigned i=0;i<6;++i) {
            Row world{0,0,0,1};
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                world[r]+=model[r][c]*vertices[indices[i]][c];
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                result[i][r]+=projection[r][c]*world[c];
        }
        return result;
    };
    for(unsigned side=0;side<2;++side)for(unsigned h=1;h<120;++h) {
        const float height=float(h);
        model={{{400,0,0,200},{0,height,0,side?240-height/2:height/2},{0,0,1,-1200}}};
        assert(LetterboxGeometry(projection,model,vertices));
        const auto bar=project_bar();assert(LetterboxClipGeometry(bar));
        assert(!FadeGeometry(projection,model,vertices) && !FadeClipGeometry(bar));
    }
    model[0][0]=320; // Bottom-screen panel cannot match.
    assert(!LetterboxGeometry(projection,model,vertices));
    assert(!LetterboxClipGeometry(project_bar()));
    model[0][0]=400;model[0][3]=195; // Shifted horizontal edges.
    assert(!LetterboxGeometry(projection,model,vertices));
    assert(!LetterboxClipGeometry(project_bar()));
    model[0][3]=200;model[1]={0,20,0,120}; // Centered solid UI panel.
    assert(!LetterboxGeometry(projection,model,vertices));
    assert(!LetterboxClipGeometry(project_bar()));
    model[1]={0,240,0,120}; // Full-screen fade is handled separately.
    assert(!LetterboxGeometry(projection,model,vertices));
    assert(!LetterboxClipGeometry(project_bar()));
    model[1]={0,0,0,0}; // Closed/degenerate bar.
    assert(!LetterboxGeometry(projection,model,vertices));
    assert(!LetterboxClipGeometry(project_bar()));
    model[1]={0,20,0,10};projection[3]={0,0,-1,0};
    assert(!LetterboxGeometry(projection,model,vertices)); // Perspective world draw.
    projection[3]={0,0,0,1};model[0][0]=std::numeric_limits<float>::quiet_NaN();
    assert(!LetterboxGeometry(projection,model,vertices));
    assert(!LetterboxClipGeometry(project_bar()));
    assert(IsTopViewport(480, 400) && IsTopViewport(240, 400));
    assert(!IsTopViewport(240, 320) && !IsTopViewport(1024, 1024));
    assert(Near(HorizontalScale * Aspect / NativeAspect, 1.f)); // HUD aspect cancellation.
    const Common::Rectangle<unsigned> bottom{80, 480, 720, 960};
    const auto top = FitTop({0, 0, 800, 480}, bottom, 800, true);
    assert(top.left == 0 && top.right == 800 && top.GetHeight() == 450);
    assert(top.bottom <= bottom.top);
    const auto side = FitTop({0, 0, 800, 480}, {800, 0, 1440, 480}, 1440, true);
    assert(side.right <= 800 && side.GetHeight() == 450);
    const auto inset = FitTop({40, 0, 840, 480}, {600, 300, 800, 450}, 880, true);
    assert(inset.GetHeight() == 480 && inset.GetWidth() == 853);
    const auto tiny = FitTop({0, 0, 1, 1}, {}, 1, false);
    assert(tiny.GetWidth() == 1 && tiny.GetHeight() == 1);
    enabled.store(true); supported_layout.store(false); assert(!Active());
    supported_layout.store(true); assert(Active());
    enabled.store(false); assert(!Active());
    std::cout << "OoT3D camera/widescreen checks passed\n";
}
