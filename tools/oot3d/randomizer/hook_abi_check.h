// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise real ARM wrappers with a deliberately hostile, but ABI-conforming,
// C callee. A compiler may change all caller-saved registers and flags.
static void CheckHookABI(Env& env, Dynarmic::A32::Jit& jit) {
    const auto poison = [&] {
        for(unsigned i : {0u,1u,2u,3u,12u})jit.Regs()[i]=0xdead0000+i;
        for(unsigned i=0;i<16;++i)jit.ExtRegs()[i]=0xdead1000+i;
        jit.SetCpsr(0x10);
        jit.SetFpscr(0);
    };
    const auto intercept = [&](const char* name) {
        env.mock_at=R::Data().Find(name).address;assert(env.mock_at);
        jit.ClearCache();
    };

    // This hook retains a live s0 across its call and returns speed in s0.
    intercept("Player_GetSpeedMultiplier");
    env.Put<float>(0xc00000+0x21c,4.0f);
    env.setup=[&] {jit.Regs()[6]=0xc00000;};
    env.mock=[&] {poison();jit.Regs()[0]=0x3fc00000;}; // multiplier 1.5
    env.Run("hook_MultiplyPlayerSpeed");
    assert(jit.ExtRegs()[0]==0x40c00000); // 4 * 1.5 = 6, not clobbered s0

    // The conditional C call must not affect whether the following CMP runs.
    intercept("ObjKibako_CheckRuto");
    env.mock=[&] {poison();jit.Regs()[0]=0;};
    env.Run("hook_JabuBoxCheckRuto",0);
    assert(jit.Cpsr()&0x40000000);

    // The ghost-race patch sits between r1=sp and a native call using r1.
    intercept("EnPoRelay_CheckChestFlag");
    env.mock=poison;
    env.setup=[&] {jit.Regs()[4]=0xc00000;};
    env.stop_at=0x367358;
    env.Run(0x19a41c);
    assert(jit.Regs()[0]==0xc00000 && jit.Regs()[1]==Env::SP);
    env.stop_at=Env::Stop;

    // This wrapper also calls the original native function. Saving just LR
    // left that call misaligned even after fixing its separate C helper.
    const auto isg_option=R::Data().Find("gSettingsContext").address+0xea;
    const auto saved_isg=env.Get<std::uint8_t>(isg_option);
    env.Put<std::uint8_t>(isg_option,0);
    env.mock_at=0x34bbfc;jit.ClearCache();
    unsigned native_called=0;
    env.mock=[&] {
        ++native_called;
        assert((jit.Regs()[13]&7)==0);
        assert(jit.Regs()[0]==0xc00000);
        poison();
    };
    env.Run("hook_RestoreISG",0xc00000);
    assert(native_called==1 && jit.Regs()[13]==Env::SP);
    env.Put<std::uint8_t>(isg_option,saved_isg);

    // Every declared bridge, both possible incoming stack alignments, and
    // multiple flag states. Return values and all four arguments must survive.
    unsigned count=0;
    for(auto it=R::Data().symbols.begin();it!=R::Data().symbols.end();++it) {
        const auto name=it.key();
        const bool returns_value=name.startsWith("hookabi_value_");
        const bool returns_void=name.startsWith("hookabi_void_");
        if(!returns_value && !returns_void)continue;
        const auto target=name.mid(returns_value?14:13).toLatin1();
        intercept(target.constData());
        for(U32 misalignment:{0u,4u})for(U32 flags:{0u,0x40000000u,0xb80f0000u}) {
            const U32 sp=Env::SP-misalignment;
            constexpr U32 fpscr=0x01c00010;
            std::array<U32,16> registers{};
            std::array<U32,32> floats{};
            for(unsigned i=0;i<13;++i)registers[i]=0x12000000+i;
            registers[13]=sp;registers[14]=Env::Stop;
            for(unsigned i=0;i<32;++i)floats[i]=0x3f000000+i;
            env.setup=[&] {
                for(unsigned i=0;i<15;++i)jit.Regs()[i]=registers[i];
                for(unsigned i=0;i<32;++i)jit.ExtRegs()[i]=floats[i];
                jit.SetCpsr(0x10|flags);jit.SetFpscr(fpscr);
            };
            unsigned called=0;
            env.mock=[&] {
                ++called;
                assert((jit.Regs()[13]&7)==0);
                for(unsigned i=0;i<4;++i)assert(jit.Regs()[i]==registers[i]);
                poison();jit.Regs()[0]=0x76543210;
            };
            env.Run(name.toLatin1().constData());
            assert(called==1);
            for(unsigned i=0;i<14;++i)
                assert(jit.Regs()[i]==((returns_value && i==0)?0x76543210:registers[i]));
            for(unsigned i=0;i<32;++i)assert(jit.ExtRegs()[i]==floats[i]);
            assert((jit.Cpsr()&0xf80f0000)==flags);
            assert(jit.Fpscr()==fpscr);
        }
        ++count;
    }
    assert(count>200);
    env.mock={};env.mock_at=0;jit.ClearCache();
    std::cout<<"Hook ABI: "<<count<<" bridges preserved state, aligned stacks and returned results; native ghost-race, Ruto and speed continuations passed\n";
}
