// SPDX-License-Identifier: GPL-2.0-or-later
// c++ -std=c++20 -Isrc tools/oot3d/title-layout-check.cpp -o /tmp/oot-title-check
// /tmp/oot-title-check [path/to/code-decompressed.bin]
#include "oot3d/title_hooks.h"
#include "oot3d/title_prompt.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

using namespace OOT3D::TitleScreen;
struct Memory {
    std::map<U32, U32> words;
    unsigned writes = 0;
    std::optional<U32> Read(U32 at) {
        const auto it = words.find(at);
        return it == words.end() ? std::nullopt : std::optional<U32>{it->second};
    }
    void Write(U32 at, U32 value) { assert(words.contains(at)); words[at] = value; ++writes; }
    void Allocate(U32 at,unsigned bytes) {for(unsigned i=0;i<bytes;i+=4)words[at+i]=0;}
    void Float(U32 at, float value) { words[at] = std::bit_cast<U32>(value); }
    float Float(U32 at) { return std::bit_cast<float>(words.at(at)); }
};
int main(int argc, char** argv) {
    Memory memory;
    FadeOverride fade;
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        assert(file);
        U32 word;
        for (U32 at = 0x100000; file.read(reinterpret_cast<char*>(&word), 4); at += 4)
            memory.words[at] = word;
        assert(CodeMatches(memory));
        memory.words[0x1da890] ^= 1;
        assert(!CodeMatches(memory));
        memory.words.clear();
    }
    for (unsigned i = 0; i < Sites.size(); ++i) {
        assert(Instruction(Sites[i].pc, Sites[i].instruction) == (0xef000000 | (FirstSVC + i)));
        assert(Instruction(Sites[i].pc + 4, Sites[i].instruction) == Sites[i].instruction);
        assert(Instruction(Sites[i].pc, 0) == 0);
    }
    constexpr U32 matrix = 0x10000000;
    const std::array<float, 12> original{1, 0, 0, 0, 0, 1, 0, -11, 0, 0, 1, -34};
    const auto reset = [&] {
        for (unsigned i = 0; i < original.size(); ++i) memory.Float(matrix + i * 4, original[i]);
    };
    reset();
    const auto before = memory.words;
    assert(Matrix(memory, matrix, {}));
    assert(memory.words == before);
    for (unsigned frame = 0; frame < 10; ++frame) {
        reset(); // Game generates the source matrix afresh, no accumulated scaling.
        assert(Matrix(memory, matrix, {20, 30, 2}));
        assert(memory.Float(matrix) == 2 && memory.Float(matrix + 20) == 2);
        assert(memory.Float(matrix + 12) == 2 && memory.Float(matrix + 28) == -14);
        assert(memory.Float(matrix + 44) == -34);
    }
    auto writes = memory.writes;
    assert(!Matrix(memory, matrix, {0, 0, std::numeric_limits<float>::quiet_NaN()}));
    memory.words.erase(matrix + 44);
    assert(!Matrix(memory, matrix, {}));
    assert(memory.writes == writes); // No partially written invalid matrices.

    constexpr U32 controller = 0x11000000, rect = 0x12000000;
    memory.words[0x50bb50] = controller;
    memory.Float(controller + 0x114, 208);
    memory.Float(controller + 0x118, 32);
    memory.Float(rect, 104); memory.Float(rect + 4, 136);
    memory.Float(rect + 8, 56); memory.Float(rect + 12, 264);
    std::array<U32, 16> regs{};
    regs[3] = rect;
    regs[4] = controller + 0x1c; // Background: must stay untouched on bottom.
    writes = memory.writes;
    Handle(memory, regs, 3, Layout{}, fade);
    assert(regs[1] == 6 && memory.writes == writes);
    regs[4] = controller + 0x10c;
    Handle(memory, regs, 3, Layout{}, fade);
    assert(regs[1] == 5);
    assert(memory.Float(rect) == 174 && memory.Float(rect + 4) == 206);
    assert(memory.Float(rect + 8) == 96 && memory.Float(rect + 12) == 304);
    memory.words.erase(rect + 12);
    writes = memory.writes;
    Handle(memory, regs, 3, Layout{}, fade);
    assert(regs[1] == 6 && memory.writes == writes);

    reset();
    regs[13] = matrix - 0x58;
    Layout poses{};
    poses[1] = {10, 20, .5f};
    Handle(memory, regs, 2, poses, fade);
    assert(regs[1] == matrix);
    assert(memory.Float(matrix) == .5f && memory.Float(matrix + 28) == -13);
    constexpr U32 play = 0x13000000, actor = 0x14000000;
    memory.words[controller] = play;
    memory.words[play + 0x20cc] = actor;
    memory.words[actor] = 0x00060171;
    memory.words[actor + 0x140] = 0x1da4f4;
    memory.words[actor + 0x1ac] = 0x15000000;
    memory.Float(controller + 0x144, .25f);
    // Intro fade-in, full opacity, fade-out, fully hidden: same alpha each time.
    for (float alpha : {0.f, 64.f, 128.f, 255.f, 128.f, 64.f, 0.f}) {
        memory.Float(actor + 0x1d8, alpha);
        for (unsigned return_site : {5u, 6u}) {
            regs[0] = controller;
            regs[13] = 0x16000000;
            Handle(memory, regs, 4, Layout{}, fade);
            assert(regs[4] == controller);
            assert(std::abs(memory.Float(controller + 0x144) - .25f * alpha / 255.f) < .000001f);
            Handle(memory, regs, return_site, Layout{}, fade);
            assert(regs[13] == 0x16000050);
            assert(memory.Float(controller + 0x144) == .25f && fade.controller == 0);
        }
    }
    memory.Float(actor + 0x1d8, 255.f);
    for (float pulse : {0.f, .2f, .8f, 1.f, 0.f, 1.f, 0.f}) {
        memory.Float(controller + 0x144, pulse);
        fade.Begin(memory, controller);
        assert(std::abs(memory.Float(controller + 0x144) - pulse) < .000001f);
        fade.End(memory, controller);
        assert(memory.Float(controller + 0x144) == pulse);
    }
    // Reproduce the native unbounded 1/30-step pulse across several cycles.
    // Hidden/fading copyright must never admit an unmodulated overshoot frame.
    float pulse = 0.f, step = 1.f / 30.f;
    bool saw_upper_overshoot = false, saw_lower_overshoot = false;
    for (unsigned frame = 0; frame < 240; ++frame) {
        pulse += step;
        if (pulse >= 1.f) step = -1.f / 30.f;
        else if (pulse <= 0.f) step = 1.f / 30.f;
        saw_upper_overshoot |= pulse > 1.f;
        saw_lower_overshoot |= pulse < 0.f;
        for (float envelope : {0.f, .1f, .5f, 1.f}) {
            memory.Float(actor + 0x1d8, envelope * 255.f);
            memory.Float(controller + 0x144, pulse);
            const auto original = memory.words.at(controller + 0x144);
            fade.Begin(memory, controller);
            const float displayed = memory.Float(controller + 0x144);
            assert(displayed >= 0 && displayed <= envelope + .000001f);
            assert(std::abs(displayed - envelope * std::clamp(pulse, 0.f, 1.f)) < .000001f);
            fade.End(memory, controller);
            assert(memory.words.at(controller + 0x144) == original);
        }
    }
    assert(saw_upper_overshoot && saw_lower_overshoot);
    writes = memory.writes;
    memory.Float(actor + 0x1d8, std::numeric_limits<float>::quiet_NaN());
    fade.Begin(memory, controller);
    assert(memory.writes == writes && fade.controller == 0);
    memory.words[play + 0x20cc] = 0; // Title actor gone: no stale opacity.
    assert(!CopyrightAlpha(memory, controller));
    memory.words[play + 0x20cc] = actor;
    memory.words[actor] = 0x00060001; // Different actor, circular list: bounded fallback.
    memory.words[actor + 0x130] = actor;
    assert(!CopyrightAlpha(memory, controller));
    if(argc>2) {
        namespace T=OOT3D::TitlePrompt;
        std::ifstream file(argv[2],std::ios::binary);assert(file);
        const std::vector<std::uint8_t> font{std::istreambuf_iterator<char>(file),{}};
        const auto image=T::MakeAssets(font);assert(image && image->width>20);
        assert(!T::MakeAssets({}));
        bool white=false,shadow=false;
        for(auto pixel:image->pixels) {
            white|=(pixel>>8)==0xffffff && (pixel&255);
            shadow|=(pixel>>8)==0 && (pixel&255);
        }
        assert(white && shadow);
        T::assets.store(std::shared_ptr<const T::Assets>(std::make_shared<T::Assets>(*image)));
        T::Hook hook;
        memory.words[actor]=0x00060171;
        memory.words[actor+0x140]=0x1da4f4;
        memory.Float(actor+0x1d8,128);
        memory.Float(controller+0x144,.75f);
        memory.Float(rect,104);memory.Float(rect+4,136);
        memory.Float(rect+8,56);memory.Float(rect+12,264);
        for(unsigned i=0;i<3;++i)memory.Float(controller+0x138+4*i,1);
        regs={};regs[2]=controller+0x138;regs[3]=rect;regs[4]=controller+0x10c;
        regs[13]=0x17000000;regs[15]=Sites[3].pc+4;
        fade.Begin(memory,controller);
        const float opacity=memory.Float(controller+0x144);
        Layout text_layout;text_layout[3]={10,-20,2};
        hook.Observe(memory,regs,text_layout);assert(hook.next.count==T::Text.size());
        const auto geometry=hook.next;
        const unsigned last=(geometry.count-1)*12;
        assert(std::abs((geometry.positions[0]+geometry.positions[last+3])/2-210)<.001f);
        assert((geometry.positions[1]+geometry.positions[7])/2==170);
        assert(geometry.positions[7]-geometry.positions[1]==30);
        for(unsigned i=3;i<16;i+=4)assert(geometry.colors[i]==opacity);
        // Existing prompt controls must not affect the extra prompt's captured pose.
        assert(Prompt(memory,regs[4],rect,{-50,30,.5f}));
        assert(hook.next.positions==geometry.positions);
        constexpr U32 source=0x19000000,mesh=0x19010000,descriptor=0x19020000;
        memory.Allocate(source,0x1e4);memory.Allocate(mesh,0x1c0);memory.Allocate(descriptor,0x120);
        memory.words[T::Queue+0x154]=source;
        memory.words[source]=0x4ebd60;memory.words[source+4]=mesh;
        memory.words[source+0x1dc]=0x19030000;memory.words[mesh]=descriptor;
        // Native sprite: triangle strip with vertex color and UV attributes disabled.
        memory.words[descriptor+0x1c]=0x98;
        memory.Float(source+0xfc,.2f);memory.Float(source+0x48,208);
        const auto native=memory.words;
        regs[15]=Sites[7].pc+4;const auto saved=regs;
        assert(hook.Return(memory,regs));
        unsigned steps=0,queued=0;
        while(hook.Pending()) {
            assert(++steps<16);
            using H=T::Hook;
            if(hook.stage==H::Stage::Allocate) {
                memory.Allocate(0x1a000000,H::AllocationSize);regs[0]=0x1a000000;
            }
            if(hook.stage==H::Stage::TextureAllocate) {
                assert((memory.words[hook.block+H::Descriptor+0x1c]&0x138)==0x120);
                memory.Allocate(0x18000000,H::TextureBytes);regs[0]=0x18000000;
            }
            if(hook.stage==H::Stage::Mesh) {
                memory.words[hook.block]=hook.block+H::Descriptor;
                memory.words[hook.block+0x124]=0;memory.words[hook.block+0x128]=2;
            }
            if(hook.stage==H::Stage::Model) {
                memory.Allocate(0x1b000000,0x1e4);memory.words[0x1b000000]=0x4ebd60;
                memory.words[0x1b000008]=0x1b001000;memory.words[0x1b001358]=0x19030000;regs[0]=0x1b000000;
            }
            if(hook.stage==H::Stage::Draw) {
                ++queued;
                assert(regs[15]==0x328350 && regs[0]==T::Queue && regs[1]==5 && regs[2]==hook.model);
                assert(memory.Float(hook.model+0xfc)==1 && memory.Float(hook.model+0x48)==1);
                assert(memory.Float(hook.model+0x110)==1 && memory.Float(hook.model+0x11c)==0);
                assert(hook.geometry.colors[3]==opacity);
                assert(memory.words[hook.block+0x124]==0); // Queue advances once, not twice.
                memory.words[hook.block+0x124]=1; // Native model update before rendering.
                // Each independently indexed glyph has two nondegenerate,
                // equally wound triangles (not a strip culled halfway across).
                for(unsigned q=0;q<T::Text.size();++q) {
                    const auto index=[&](unsigned i) {
                        return (memory.words[hook.block+H::Indices+q*12+(i/2)*4]>>(16*(i%2)))&65535;
                    };
                    const auto area=[&](unsigned a,unsigned b,unsigned c) {
                        const auto& v=hook.geometry.positions;
                        return (v[3*b]-v[3*a])*(v[3*c+1]-v[3*a+1])-
                               (v[3*b+1]-v[3*a+1])*(v[3*c]-v[3*a]);
                    };
                    assert(area(index(0),index(1),index(2))<0);
                    assert(area(index(3),index(4),index(5))<0);
                }
            }
            hook.Return(memory,regs);
        }
        assert(queued==1 && regs==saved);
        for(const auto& [at,word]:native)assert(memory.words.at(at)==word);
        assert(!hook.Return(memory,regs)); // No duplicate on the same return.
        fade.End(memory,controller);
        assert(memory.Float(controller+0x144)==.75f);
        // Color and tracking change only the new text; glyph shapes and alpha stay intact.
        const std::array<float,4> bounds{104,136,56,264},rgba{1,1,1,.45f};
        const auto baseline=T::MakeGeometry(*image,text_layout,bounds,rgba);
        text_layout.text_color={.2f,.6f,.8f};text_layout.letter_spacing+=1.5f;
        const auto tracked=T::MakeGeometry(*image,text_layout,bounds,rgba);
        assert(tracked.count==baseline.count && tracked.uvs==baseline.uvs);
        for(unsigned q=0;q<tracked.count;++q) {
            assert(tracked.colors[q*16]==.2f && tracked.colors[q*16+1]==.6f);
            assert(tracked.colors[q*16+2]==.8f && tracked.colors[q*16+3]==.45f);
            assert(std::abs((tracked.positions[q*12+3]-tracked.positions[q*12])-
                (baseline.positions[q*12+3]-baseline.positions[q*12]))<.001f);
            assert(std::abs(tracked.positions[q*12]-baseline.positions[q*12]-
                (float(q)-5)*1.5f*.75f*2)<.001f);
        }
        // Several complete unbounded pulse cycles, including endpoints and
        // negative/positive overshoot: no alpha threshold or stale opaque frame.
        float native_pulse=0,delta=1.f/30;
        for(unsigned frame=0;frame<240;++frame) {
            native_pulse+=delta;
            if(native_pulse>=1)delta=-1.f/30;
            else if(native_pulse<=0)delta=1.f/30;
            memory.Float(controller+0x144,native_pulse);
            fade.Begin(memory,controller);
            const float displayed=memory.Float(controller+0x144);
            const auto pulsing=T::MakeGeometry(*image,text_layout,bounds,{1,1,1,displayed});
            if(displayed>0) {
                assert(pulsing.count==T::Text.size());
                for(unsigned i=3;i<pulsing.count*16;i+=4)assert(pulsing.colors[i]==displayed);
            } else assert(!pulsing.count);
            fade.End(memory,controller);
            assert(memory.Float(controller+0x144)==native_pulse);
        }
        // Reuse the allocated model across both buffer slots. A queued update
        // must advance exactly once and retain this frame's geometry and fade.
        for(float alpha:{.01f,.7f,.03f}) {
            memory.Float(rect,104);memory.Float(rect+4,136);
            memory.Float(rect+8,56);memory.Float(rect+12,264);
            memory.Float(controller+0x144,alpha);
            regs=saved;regs[2]=controller+0x138;regs[3]=rect;
            hook.Observe(memory,regs,text_layout);
            const auto before_buffer=memory.words[hook.block+0x124];
            assert(hook.Return(memory,regs));
            assert(hook.stage==T::Hook::Stage::Positions); // Reuse; no allocation.
            unsigned resumes=0;
            while(hook.Pending()) {
                assert(++resumes<6);
                if(hook.stage==T::Hook::Stage::Draw) {
                    assert(memory.words[hook.block+0x124]==before_buffer);
                    assert(hook.geometry.colors[3]==alpha);
                    memory.words[hook.block+0x124]=(before_buffer+1)%2;
                }
                hook.Return(memory,regs);
            }
            assert(memory.words[hook.block+0x124]==(before_buffer+1)%2);
        }
        // Other sprites and a zero-opacity native prompt never emit extra text.
        regs[4]=controller+0x1c;hook.Observe(memory,regs,{});assert(!hook.next.count);
        regs[4]=controller+0x10c;memory.Float(controller+0x144,0);
        hook.Observe(memory,regs,{});assert(!hook.next.count);
    }
    std::cout << "PASS: title signatures, transforms, synchronized fades, textbox font, and extra prompt queue lifecycle\n";
}
