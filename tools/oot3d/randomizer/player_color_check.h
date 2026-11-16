// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

inline void CheckPlayerColor(Env& e,Dynarmic::A32::Jit& cpu) {
    constexpr U32 animation=0xcc0000,manager=0xcc0100,chunk=0xcc0200,local=0xcc0400,remote=0xcc0500;
    e.Write(animation+4,manager);e.Write(manager,chunk);
    for(unsigned i=0;i<0x100;i+=4)e.Put<float>(chunk+i,float(i)/256);
    const auto original=R::ReadBytes(e,chunk,0x100);
    for(unsigned child:{0u,1u}) {
        const auto begin=[&](U32 scope,U32 color) {
            e.setup=[&]{cpu.Regs()[3]=child;};e.Run("MultiplayerTunic_Begin",scope,animation,color);
        };
        const auto check=[&](U32 color) {
            for(unsigned c=0;c<3;++c)for(unsigned k=0;k<(child?1:3);++k)
                assert(std::abs(e.Get<float>(chunk+0x70+c*(child?0x18:0x28)+8*k)-
                    float((color>>(16-8*c))&255)/255)<.00001f);
        };
        begin(local,0xa5e01020);check(0xe01020);
        begin(remote,0xa52040f0);check(0x2040f0);
        e.Run("MultiplayerTunic_End",remote);check(0xe01020);
        e.Run("MultiplayerTunic_End",local);
        assert(R::ReadBytes(e,chunk,0x100)==original);
        for(U32 disabled:{0u,0xff123456u}) {
            begin(local,disabled);e.Run("MultiplayerTunic_End",local);
            assert(R::ReadBytes(e,chunk,0x100)==original);
        }
    }
    const auto settings=R::Data().Find("gSettingsContext").address;
    const auto saved=e.Get<SettingsContext>(settings);
    e.Put<std::uint8_t>(settings+offsetof(SettingsContext,mp_Enabled),1);
    e.Put<std::uint8_t>(settings+offsetof(SettingsContext,customTunicColors),0);
    for(unsigned tunic=0;tunic<3;++tunic) {
        e.Run("MultiplayerTunic_ResolveColor",0,tunic);
        assert(cpu.Regs()[0]==(0xa5000000u|std::array<U32,3>{0x168a0e,0x96000e,0x002a8e}[tunic]));
        e.Run("MultiplayerTunic_ResolveColor",0xa5123456,tunic);assert(cpu.Regs()[0]==0xa5123456);
    }
    e.Put<std::uint8_t>(settings+offsetof(SettingsContext,customTunicColors),1);
    e.Run("MultiplayerTunic_ResolveColor",0,0);assert(cpu.Regs()[0]==0);
    e.Put(settings,saved);
    std::cout<<"Player color: native child/adult draw scopes keep per-player palettes isolated and restore all original keys\n";
}
