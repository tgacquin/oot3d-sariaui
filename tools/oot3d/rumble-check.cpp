// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/rumble_hooks.h"
#include "oot3d/difficulty_hooks.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace OOT3D;
using U32 = std::uint32_t;
struct Env : Dynarmic::A32::UserCallbacks {
    std::vector<unsigned char> bytes = std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu{};
    std::uint64_t ticks{};
    bool patched = true;
    U32 single_stop=0, alternate_stop=0;
    float random_value=0;
    unsigned random_calls=0;
    template<class T> T Get(U32 a) {
        assert(a + sizeof(T) <= bytes.size()); T v;
        std::memcpy(&v, &bytes[a], sizeof(v)); return v;
    }
    template<class T> void Put(U32 a, T v) {
        assert(a + sizeof(T) <= bytes.size()); std::memcpy(&bytes[a], &v, sizeof(v));
    }
    std::optional<U32> Read(U32 a) { return Get<U32>(a); }
    void Write(U32 a, U32 v) { Put(a, v); }
    std::optional<U32> MemoryReadCode(U32 a) override {
        if(single_stop) {
            if(a==single_stop || a==alternate_stop)return 0xef000000;
            if(a==0x3759d0)return 0xef000002;
            return patched?Rumble::Instruction(a,Get<U32>(a)):Get<U32>(a);
        }
        if (a == 0xf00000 || a == 0x492cd0) return 0xef000000;
        if (a == 0x36f59c || a == 0x36aeb4 || a == 0x37547c) return 0xef000001;
        U32 word = Difficulty::Instruction(a, Get<U32>(a));
        return patched ? Rumble::Instruction(a, word) : word;
    }
    std::uint8_t MemoryRead8(U32 a) override { return Get<std::uint8_t>(a); }
    std::uint16_t MemoryRead16(U32 a) override { return Get<std::uint16_t>(a); }
    U32 MemoryRead32(U32 a) override { return Get<U32>(a); }
    std::uint64_t MemoryRead64(U32 a) override { return Get<std::uint64_t>(a); }
    void MemoryWrite8(U32 a, std::uint8_t v) override { Put(a,v); }
    void MemoryWrite16(U32 a, std::uint16_t v) override { Put(a,v); }
    void MemoryWrite32(U32 a, U32 v) override { Put(a,v); }
    void MemoryWrite64(U32 a, std::uint64_t v) override { Put(a,v); }
    void InterpreterFallback(U32, std::size_t) override { std::abort(); }
    void ExceptionRaised(U32, Dynarmic::A32::Exception) override { std::abort(); }
    void AddTicks(std::uint64_t n) override { ticks = n > ticks ? 0 : ticks - n; }
    std::uint64_t GetTicksRemaining() override { return ticks; }
    void CallSVC(U32 svc) override {
        if (svc == 0) cpu->HaltExecution();
        else if (svc == 1) cpu->Regs()[15] = cpu->Regs()[14];
        else if (svc == 2) {
            ++random_calls;cpu->ExtRegs()[0]=std::bit_cast<U32>(random_value);
            cpu->Regs()[15]=cpu->Regs()[14];
        }
        else if (svc>=Rumble::FirstSVC && svc<Rumble::FirstSVC+Rumble::Sites.size())
            Rumble::Handle(*this,cpu->Regs(),svc-Rumble::FirstSVC,cpu->ExtRegs()[0]);
        else Difficulty::Handle(*this, cpu->Regs(), svc - Difficulty::FirstSVC);
    }
    void Setup() {
        cpu->Reset(); cpu->ClearCache();
        for (unsigned i = 0; i < 13; ++i) cpu->Regs()[i] = 0x800000 + i * 0x1000;
        for (unsigned i = 0; i < 64; ++i) cpu->ExtRegs()[i] = 0x3f000000 + i;
        cpu->Regs()[13] = 0xe00000; cpu->Regs()[14] = 0xf00000;
        cpu->SetCpsr(0xa0000010); cpu->SetFpscr(0);
        Write(cpu->Regs()[6] + 0x70c, 0x900000);
        Put<std::uint16_t>(0x9000f4, 0x20);
        Rumble::Clear();
        random_calls=0;
    }
    void Run(U32 pc) {
        cpu->Regs()[15] = pc; ticks = 10000; cpu->ClearHalt(); cpu->Run(); assert(ticks);
    }
};
int main(int argc, char** argv) {
    assert(argc == 2);
    using namespace std::chrono_literals;
    const auto start = Rumble::Clock::time_point{};
    assert(Rumble::BombExplosion(0).amplitude==255);
    assert(Rumble::BombExplosion(250).amplitude==192);
    assert(Rumble::BombExplosion(750).amplitude==64);
    assert(Rumble::BombExplosion(500).amplitude==128);
    assert(Rumble::BombExplosion(1000).amplitude==0);
    assert(Rumble::BombExplosion(-1).amplitude==0 && Rumble::BombExplosion(NAN).amplitude==0);
    Rumble::Mixer mixer;
    mixer.Request(120, 20, 10, start);
    assert(mixer.Sample(start + 300ms, 100) == 120 * 257);
    assert(mixer.Sample(start + 500ms, 100) == 20 * 257);
    assert(mixer.Sample(start + 1s, 100) == 0);
    assert(mixer.Sample(start + 300ms, 50) == 120 * 257 / 2);
    assert(mixer.Sample(start, 0) == 0);
    mixer.Request(255, 20, 150, start);
    assert(mixer.Sample(start, 100) == 65535); // Overlap uses max, never wraps.
    mixer.Clear(); assert(mixer.Sample(start, 100) == 0);
    mixer.Request(180,120,5,start);
    mixer.Override({255,90,2},start);
    assert(mixer.Sample(start,100)==65535);
    mixer.Override({1,3,1},start+100ms); // Catch cancels the preceding fight.
    assert(mixer.Sample(start+100ms,100)==257);
    assert(mixer.Sample(start+200ms,100)==180*257); // Ordinary requests age underneath.
    mixer.Clear();assert(mixer.Sample(start+200ms,100)==0);
    mixer.Override({255,120,5},start);mixer.Override({60,90,10},start+100ms);
    assert(mixer.Sample(start+100ms,50)==60*257/2);
    assert(mixer.Sample(start+2s,100)==0); // Old strong override never returns.
    mixer.Clear();
    for(bool sinking:{false,true})for(unsigned timer:{10u,29u,40u,79u}) {
        const auto cue=Rumble::Fishing::BiteCue(sinking,timer);
        assert(cue.amplitude==(sinking?60:180) && cue.hold==int(timer*3) && cue.decay==10);
    }
    assert(Rumble::Fishing::HookCue(200).amplitude==200);
    assert(Rumble::Fishing::HookCue(330).amplitude==255);
    for(const auto [length,expected]:std::array<std::pair<float,int>,10>{{
        {30,140},{39.9f,140},{40,170},{49.9f,170},{50,200},
        {59.9f,200},{60,230},{69.9f,230},{70,255},{100,255}}}) {
        for(bool tired:{false,true})for(U32 seed=0;seed<100;++seed) {
            const auto cue=Rumble::Fishing::StruggleCue(length,tired,seed);
            assert(cue.amplitude==(tired?expected*3/4:expected) && cue.hold>=10 && cue.hold<=14 && cue.decay==5);
        }
        assert(Rumble::Fishing::JumpCue(length).amplitude==(length>=60?255:length>=50?200:180));
    }
    Rumble::ImpactQueue queue;
    queue.Add(11, start);
    queue.Present(10, start + 16ms, mixer);
    assert(mixer.Sample(start + 16ms, 100) == 0); // Still displaying the approach.
    queue.Present(11, start + 33ms, mixer);
    assert(mixer.Sample(start + 33ms, 100) == 65535);
    mixer.Clear(); queue.Present(11, start + 50ms, mixer);
    assert(mixer.Sample(start + 50ms, 100) == 0); // Never replay on duplicate presents.
    queue.Add(12, start); queue.Clear(); queue.Present(12, start + 33ms, mixer);
    assert(mixer.Sample(start + 33ms, 100) == 0);
    queue.Add(13, start); queue.Present(14, start + 600ms, mixer);
    assert(mixer.Sample(start + 600ms, 100) == 0); // Discard stale impacts.
    queue.Add(15, start); queue.Present(16, start + 50ms, mixer);
    assert(mixer.Sample(start + 50ms, 100) == 65535); // Frame skipping still releases.
    mixer.Clear();queue.Add(17,start,{120,20,10});
    queue.Present(16,start+16ms,mixer);assert(mixer.Sample(start+16ms,100)==0);
    queue.Present(17,start+33ms,mixer);assert(mixer.Sample(start+33ms,100)==120*257);
    assert(mixer.Sample(start+333ms,50)==120*257/2);

    Env e;
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate); assert(file);
    const auto size = file.tellg(); file.seekg(0);
    file.read(reinterpret_cast<char*>(e.bytes.data() + 0x100000), size); assert(file);
    assert(Rumble::CodeMatches(e) && Difficulty::CodeMatches(e));
    for (const auto& site : Rumble::Sites) {
        e.Write(site.pc, site.instruction ^ 1); assert(!Rumble::CodeMatches(e));
        assert(Rumble::Instruction(site.pc, site.instruction ^ 1) == (site.instruction ^ 1));
        e.Write(site.pc, site.instruction);
    }
    // The randomizer replaces 492cd0, which must not disable the earlier hook.
    const auto bonk = e.Get<U32>(0x492cd0);
    e.Write(0x492cd0, 0xeb123456); assert(Rumble::CodeMatches(e)); e.Write(0x492cd0, bonk);
    Dynarmic::A32::UserConfig config; config.callbacks = &e;
    Dynarmic::A32::Jit cpu(config); e.cpu = &cpu;
    Rumble::enabled = Rumble::running = true;
    for (unsigned category : {2u, 4u}) {
        e.patched = false; e.Setup(); e.Put<std::uint8_t>(cpu.Regs()[4] + 2, category);
        e.Run(0x492c98);
        const auto regs = cpu.Regs(); const auto fp = cpu.ExtRegs();
        const auto flags = cpu.Cpsr(), fpscr = cpu.Fpscr();
        const auto bytes = e.bytes;
        e.patched = true; e.Setup(); e.Put<std::uint8_t>(cpu.Regs()[4] + 2, category);
        e.Run(0x492c98);
        assert(cpu.Regs() == regs && cpu.ExtRegs() == fp);
        assert(cpu.Cpsr() == flags && cpu.Fpscr() == fpscr && e.bytes == bytes);
        assert((Rumble::Sample() > 0) == (category == 2));
    }
    // Execute each displaced ARM instruction in the native CPU, then the
    // hooked version with identical input. Only controller output may differ.
    for(unsigned site=1;site<12;++site)for(unsigned category:{2u,4u}) {
        const auto setup=[&] {
            e.Setup();e.single_stop=Rumble::Sites[site].pc+4;
            U32 actor=cpu.Regs()[4];
            if(site==1) {actor=0x860000;cpu.Regs()[0]=actor;cpu.Regs()[1]=0x100011a;cpu.Regs()[14]=0x488a58;}
            if(site==5) {
                actor=cpu.Regs()[6];e.Write(cpu.Regs()[4]+0x224,0x870000);
                e.Put<std::uint8_t>(actor+0x1a9,16);
            }
            if(site==7)e.Write(cpu.Regs()[8],0x6428b4f8); // Original short-fall table.
            if(site==8)cpu.Regs()[6]=201;
            e.Put<std::uint8_t>(actor+2,category);
            if(site==11) {
                e.Put<std::uint16_t>(actor,0x10);e.Put<std::uint8_t>(actor+2,category==2?3:4);
                e.Put<std::uint16_t>(actor+0x1c,1);e.Put<float>(actor+0x98,100.f);
                cpu.Regs()[0]=0x3c4438;
            }
        };
        e.patched=false;setup();const auto initial=e.bytes;e.Run(Rumble::Sites[site].pc);
        const auto regs=cpu.Regs();const auto fp=cpu.ExtRegs();
        const auto flags=cpu.Cpsr(),fpscr=cpu.Fpscr();const auto expected=e.bytes;
        e.bytes=initial;e.patched=true;setup();e.Run(Rumble::Sites[site].pc);
        assert(cpu.Regs()==regs && cpu.ExtRegs()==fp && cpu.Cpsr()==flags && cpu.Fpscr()==fpscr);
        assert(e.bytes==expected && (Rumble::Sample()>0)==(category==2));
    }
    for(bool active:{false,true}) {
        e.Setup();Rumble::enabled=active;auto& r=cpu.Regs();
        e.Write(r[5]+0x734,0x876543);const auto before=r;
        Rumble::Handle(e,r,12);
        if(active) {assert(r[15]==0x25259c);r[15]=before[15];assert(r==before);}
        else {assert(r[0]==0x876543);r[0]=before[0];assert(r==before);}
    }
    Rumble::enabled=true;
    // Every fishing instruction must preserve native CPU and memory state,
    // both enabled/disabled and for valid/rejected scene and actor identities.
    for(unsigned site=13;site<Rumble::Sites.size();++site)for(unsigned variant=0;variant<4;++variant) {
        const auto setup=[&] {
            e.Setup();e.single_stop=Rumble::Sites[site].pc+4;
            Rumble::enabled=variant!=1;
            auto& r=cpu.Regs();const U32 actor=r[4],play=0x900000;
            e.Write(0x5043e0,play);e.Write(play+0x104,variant==2?72:73);
            e.Put<std::uint16_t>(actor,variant==3?0xfd:0xfe);
            e.Put<float>(actor+0x204,60);e.Write(actor+0x1b4,32);
            e.Put<std::uint8_t>(Rumble::Fishing::Globals+0x16,0);
            r[0]=25;r[7]=7;cpu.ExtRegs()[0]=std::bit_cast<U32>(240.f);
            e.Write(r[10],play);
            if(site>=20)r[4]=Rumble::Fishing::Globals+(variant==3?4:0);
        };
        e.patched=false;setup();const auto initial=e.bytes;e.Run(Rumble::Sites[site].pc);
        const auto regs=cpu.Regs();const auto fp=cpu.ExtRegs();
        const auto flags=cpu.Cpsr(),fpscr=cpu.Fpscr();const auto expected=e.bytes;
        e.bytes=initial;e.patched=true;setup();e.Run(Rumble::Sites[site].pc);
        assert(cpu.Regs()==regs && cpu.ExtRegs()==fp && cpu.Cpsr()==flags && cpu.Fpscr()==fpscr);
        assert(e.bytes==expected && (Rumble::Sample()>0)==(variant==0) && e.random_calls==0);
    }
    // Execute the original struggle acceptance branches, including guest RNG.
    // The callback only restores the removed rumble request after acceptance.
    Rumble::enabled=true;
    for(U32 mask:{7u,15u})for(U32 timer:{0u,1u})for(float chance:{.2f,.75f,.9f})for(unsigned delay:{0u,1u}) {
        const auto setup=[&] {
            e.Setup();e.single_stop=0x1fbde0;e.alternate_stop=0x1fbe0c;
            auto& r=cpu.Regs();r[0]=timer;r[7]=mask;
            e.Write(0x5043e0,0x900000);e.Write(0x900104,73);
            e.Put<std::uint16_t>(r[4],0xfe);e.Put<float>(r[4]+0x204,60);
            e.Put<std::uint16_t>(Rumble::Fishing::Globals+0x2c,delay);
            e.random_value=chance;
        };
        e.patched=false;setup();e.Run(0x1fbdb0);
        const auto regs=cpu.Regs();const auto fp=cpu.ExtRegs();const auto bytes=e.bytes;
        const auto flags=cpu.Cpsr(),fpscr=cpu.Fpscr(),calls=e.random_calls;
        e.patched=true;setup();e.Run(0x1fbdb0);
        assert(cpu.Regs()==regs && cpu.ExtRegs()==fp && cpu.Cpsr()==flags && cpu.Fpscr()==fpscr);
        assert(e.bytes==bytes && e.random_calls==calls && calls==(timer==0?1u:0u));
        assert((Rumble::Sample()>0)==(timer==0 && chance<.75f && delay==0));
    }
    e.alternate_stop=0;
    e.single_stop=0;
    // Sound reuse or ordinary successful hits must never trigger feedback.
    assert(!Rumble::SoundCue(0x123456,0x100011a));
    assert(!Rumble::SoundCue(0x488a58,0x1000000));
    for(const auto [caller,sound]:std::array<std::pair<U32,U32>,6>{{
        {0x1ebd6c,0x100011a},{0x2093e0,0x100011a},{0x488a58,0x100011a},
        {0x2b9e98,0x10000fc},{0x4c2ffc,0x10000fc},{0x35d448,0x1000075}}})
        assert(Rumble::SoundCue(caller,sound));
    for(unsigned distance:{0u,200u,201u,399u,400u}) {
        e.Setup();const auto actor=cpu.Regs()[4];e.Put<std::uint8_t>(actor+2,2);
        cpu.Regs()[6]=distance;Rumble::Handle(e,cpu.Regs(),8);
        assert((Rumble::Sample()>0)==(distance>200 && distance<400));
    }
    // Execute the actual health-change routine; enabling feedback must not
    // change health, guest registers, flags or floating-point state.
    for (int change : {-32, -16, -4, -1, 0, 16}) {
        const auto setup = [&] {
            e.Setup(); e.Write(0x587998, 320u << 16); e.Write(0x58799c, 160);
            e.Put<std::uint8_t>(0x587966, 0); e.Put<std::uint8_t>(0x5879a9, 0);
            cpu.Regs()[1] = U32(change);
        };
        Rumble::enabled = false; setup(); e.Run(0x352dbc);
        const auto regs = cpu.Regs(); const auto fp = cpu.ExtRegs();
        const auto flags = cpu.Cpsr(), fpscr = cpu.Fpscr();
        const auto health = e.Get<std::uint16_t>(0x58799c);
        Rumble::enabled = true; setup(); e.Run(0x352dbc);
        assert(cpu.Regs() == regs && cpu.ExtRegs() == fp);
        assert(cpu.Cpsr() == flags && cpu.Fpscr() == fpscr);
        assert(e.Get<std::uint16_t>(0x58799c) == health);
        assert((Rumble::Sample() > 0) == (change <= -4));
    }
    Rumble::Bonk(); Rumble::enabled = false; assert(Rumble::Sample() == 0);
    Rumble::enabled = true; assert(Rumble::Sample() == 0);
    Rumble::Override({255,120,5});Rumble::enabled=false;assert(Rumble::Sample()==0);
    Rumble::enabled=true;assert(Rumble::Sample()==0);
    Rumble::Override({255,120,5});Rumble::running=false;assert(Rumble::Sample()==0);
    Rumble::Override({255,120,5});Rumble::running=true;assert(Rumble::Sample()==0);
    Rumble::Bonk(); Rumble::running = false; assert(Rumble::Sample() == 0);
    Rumble::Bonk(); Rumble::running = true; assert(Rumble::Sample() == 0);
    Rumble::sync_impacts_to_present = true;
    Rumble::Bonk(20); assert(Rumble::Sample() == 0);
    Rumble::Presented(Rumble::CaptureFrame(20)); assert(Rumble::Sample() == 0);
    Rumble::Presented(Rumble::CaptureFrame(21)); assert(Rumble::Sample() > 0);
    const auto old_frame = Rumble::CaptureFrame(23);
    Rumble::Clear(); Rumble::Bonk(22);
    Rumble::Presented(old_frame); assert(Rumble::Sample() == 0); // Old session cannot release it.
    Rumble::Presented(Rumble::CaptureFrame(23)); assert(Rumble::Sample() > 0);
    Rumble::Clear(); Rumble::Bonk(24); Rumble::enabled = false;
    assert(Rumble::Sample() == 0);
    Rumble::enabled = true; Rumble::Presented(Rumble::CaptureFrame(25));
    assert(Rumble::Sample() == 0);
    std::cout << "Rumble envelopes/overrides, lifecycle, native hook ABIs, nine fishing events, native struggle gating/RNG, guards and signatures passed\n";
}
