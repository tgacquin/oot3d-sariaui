// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
inline void CheckRewards(Env& original) {
    Env e;e.bytes=original.bytes;
    Dynarmic::A32::UserConfig config;config.callbacks=&e;
    Dynarmic::A32::Jit cpu(config);e.cpu=&cpu;
    const auto sym=[](const char* name){return R::Data().Find(name).address;};
    const auto ext=[](const char* name){return U32(R::Data().reward_symbols[name].toInteger());};
    R::WriteBytes(e,R::Data().reward_base,R::Data().rewards);
    constexpr U32 play=0xa00000,player=0xa10000,packet=0xa20000,actor=0xa30000,
        ghost=0xa40000,local=0xa50000,local2=0xa50200,factory=0xa60000,
        factory_vtable=0xa60100,create=0xa60200,destroy=0xa60300,vtable=0xa60400;
    e.Write(sym("gGlobalContext"),play);e.Write(play+0x20ac,player);
    e.Put<std::uint16_t>(play+0x104,4);e.Write(0x58795c,1);
    e.Write(sym("rGameplayFrames"),100);e.Write(sym("rActiveItemRow"),0);
    e.Write(player+0x244,local);e.Write(player+0x248,local2);
    e.Put<std::int16_t>(player+0x224e,99); // Native two-model reward.
    for(unsigned i=0;i<3;++i)e.Put<float>(player+0x28+4*i,float(10+10*i));
    for(U32 model:{local,local2}) {
        e.Write(model+0xc,model+0xb0);e.Put<float>(model+0xb8,3.5f);
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
            e.Put<float>(model+0x7c+row*16+col*4,col==3?float(row==1?80:10+10*row):float(row==col));
    }
    e.mocks[ext("Original_Matrix")]=[]{};
    e.mocks[sym("PauseContext_GetState")]=[&]{cpu.Regs()[0]=0;};
    e.mocks[ext("Original_Receive")]=[]{};
    unsigned puppet_draws=0;
    e.mocks[ext("Original_Draw")]=[&]{++puppet_draws;};
    e.mocks[ext("Original_Destroy")]=[]{};
    e.mocks[ext("Original_Shutdown")]=[]{};
    unsigned sent=0;
    e.mocks[ext("Original_Send")]=[&]{sent=e.Get<U32>(cpu.Regs()[13]);cpu.Regs()[0]=0;};
    e.Write(sym("mBuffer"),packet);e.Write(sym("mBufSize"),0x1000);e.Write(packet,1);
    const auto send=[&] {
        e.setup=[&]{cpu.Regs()[3]=packet;e.Write(Env::SP,48);};
        e.Run(ext("Rewards_Send"),0xffff,1,1);
        assert(sent==164);e.Write(sym("hostPacketBytes"),sent);
    };
    e.Run(ext("Rewards_Matrix"),local);e.Run(ext("Rewards_Matrix"),local2);send();
    assert(e.Get<std::uint8_t>(packet+58)==3);
    assert(e.Get<float>(packet+60+12)==0 && e.Get<float>(packet+60+28)==60);
    const auto valid=R::ReadBytes(e,packet+48,116);
    e.Run(ext("Rewards_Valid"),packet+48);assert(cpu.Regs()[0]);
    // Invalid identity, nonfinite matrix and invalid second-model mask.
    for(const auto [offset,value]:std::array<std::pair<U32,U32>,3>{{{4,0xffffffff},{12,0x7fc00000},{8,0x00040062}}}) {
        R::WriteBytes(e,packet+48,valid);e.Write(packet+48+offset,value);
        e.Run(ext("Rewards_Valid"),packet+48);assert(!cpu.Regs()[0]);
    }
    R::WriteBytes(e,packet+48,valid);
    e.Run(ext("Rewards_Receive"),2);
    e.Write(actor+0x1a4,ghost);e.Write(actor+0x178,0x12345678);
    e.Put<std::uint8_t>(ghost,1);e.Put<std::uint16_t>(ghost+0x10,2);
    e.Put<std::uint8_t>(ghost+0x12,1);e.Write(ghost+0x14,4);e.Write(ghost+0x18,1);
    e.Put<float>(ghost+0x1c,110);e.Put<float>(ghost+0x20,20);e.Put<float>(ghost+0x24,30);
    e.Write(0x5be5b8+0x17c,factory);e.Write(factory,factory_vtable);e.Write(factory+8,0x42);
    e.Write(factory_vtable+8,create);e.Write(vtable+4,destroy);
    unsigned created=0,drawn=0,cleared=0;bool budget=true;
    std::set<U32> live;std::map<U32,unsigned> meshes;
    e.mocks[ext("Rewards_Headroom")]=[&]{cpu.Regs()[0]=budget;};
    e.mocks[sym("Object_Spawn")]=[&] {
        auto& r=cpu.Regs();const U32 context=r[0];
        e.Put<std::uint8_t>(context,1);e.Write(context+8,0xb00000);e.Write(context+12,0x10000);r[0]=0;
    };
    e.mocks[sym("Object_Clear")]=[&] {++cleared;};
    e.mocks[sym("ZAR_GetCMBByIndex")]=[&]{cpu.Regs()[0]=0x1234;};
    e.mocks[sym("ZAR_GetCMABByIndex")]=[&]{cpu.Regs()[0]=0x5678;};
    e.mocks[sym("MatAnim_Init")]=[]{};
    e.mocks[sym("SkeletonAnimationModel_SetMesh")]=[&]{meshes[cpu.Regs()[0]]=cpu.Regs()[1];};
    e.mocks[create]=[&] {
        assert(e.Get<U32>(factory+8)==0x12345678);
        const U32 model=0xc00000+created++*0x200;
        e.Write(model,vtable);e.Write(model+0xc,model+0xb0);live.insert(model);cpu.Regs()[0]=model;
    };
    e.mocks[destroy]=[&]{assert(live.erase(cpu.Regs()[0])==1);};
    e.mocks[sym("SkeletonAnimationModel_MatrixCopy")]=[&] {
        const auto& r=cpu.Regs();R::WriteBytes(e,r[0]+0x7c,R::ReadBytes(e,r[1],48));
    };
    e.mocks[sym("SkeletonAnimationModel_Draw")]=[&] {
        const U32 model=cpu.Regs()[0];assert(live.contains(model));++drawn;
        assert(e.Get<float>(model+0x88)==110 && e.Get<float>(model+0x98)==80);
        assert(e.Get<float>(model+0xb8)==3.5f && e.Get<float>(model+0xbc)==0);
    };
    cpu.ClearCache();e.Run(ext("Rewards_Draw"),actor,play);
    assert(created==2 && drawn==2 && live.size()==2 && e.Get<U32>(factory+8)==0x42);
    e.Run(ext("Rewards_Draw"),actor,play);assert(created==2 && drawn==4);
    // Clear transmitted display immediately; returning to an old peer cannot retain it.
    e.Write(sym("hostPacketBytes"),48);e.Run(ext("Rewards_Receive"),2);
    e.Run(ext("Rewards_Draw"),actor,play);assert(live.empty() && cleared==1);
    e.Write(sym("hostPacketBytes"),164);e.Run(ext("Rewards_Receive"),2);
    budget=false;e.Run(ext("Rewards_Draw"),actor,play);assert(created==2 && live.empty());
    budget=true;e.Run(ext("Rewards_Draw"),actor,play);assert(live.size()==2);
    e.Write(sym("rGameplayFrames"),116);e.Run(ext("Rewards_Draw"),actor,play);assert(live.empty());
    send();assert(e.Get<std::uint8_t>(packet+58)==0); // Local draw expired.
    // Rupee/custom variant keeps the actual row and locally validated appearance.
    const U32 rows=sym("rItemTable");unsigned rupee=0;
    for(unsigned i=1;i<277;++i)if(e.Get<std::uint16_t>(rows+i*24+4)==0x17f && e.Get<std::uint8_t>(rows+i*24+10)>0) {rupee=i;break;}
    assert(rupee);
    e.Write(sym("rActiveItemRow"),rows+rupee*24);
    R::WriteBytes(e,sym("rActiveDrawItem"),R::ReadBytes(e,rows+rupee*24+4,6));
    e.Put<std::uint8_t>(player+0x1ac,0);
    e.Run(ext("Rewards_Matrix"),local);send();
    assert(e.Get<std::uint16_t>(packet+54)==rupee);
    assert(e.Get<std::uint16_t>(packet+56)==e.Get<std::uint8_t>(rows+rupee*24+10));
    e.Run(ext("Rewards_Valid"),packet+48);assert(cpu.Regs()[0]);
    R::WriteBytes(e,packet+48,valid);e.Run(ext("Rewards_Receive"),2);
    e.Run(ext("Rewards_Draw"),actor,play);assert(live.size()==2);
    e.Run(ext("Rewards_Destroy"),actor,play);assert(live.empty());
    // A room loss must retire ghosts even though their network timeout no
    // longer runs. Exercise the native puppet update/spawner after shutdown.
    const auto ghost_table=R::Data().Find("ghosts");
    assert(ghost_table.size==16*1368);
    R::WriteBytes(e,ghost_table.address,R::ReadBytes(e,ghost,1368));
    for(unsigned i=0;i<16;++i) {
        e.Put<std::uint8_t>(ghost_table.address+i*1368,1);
        e.Put<std::uint8_t>(ghost_table.address+i*1368+0x12,1);
    }
    e.Write(actor+0x1a4,ghost_table.address);
    e.Run(ext("Rewards_Draw"),actor,play);assert(live.size()==2);
    const auto local_player=R::ReadBytes(e,player,0x2300);
    const auto draws_before_loss=puppet_draws;
    e.Run(ext("Rewards_Shutdown"));assert(live.empty());
    for(unsigned i=0;i<16;++i) {
        assert(!e.Get<std::uint8_t>(ghost_table.address+i*1368));
        assert(!e.Get<std::uint8_t>(ghost_table.address+i*1368+0x12));
    }
    e.Run(ext("Rewards_Draw"),actor,play);assert(puppet_draws==draws_before_loss);
    unsigned killed=0,spawned=0;
    e.mocks[sym("Actor_Kill")]=[&]{assert(cpu.Regs()[0]==actor);++killed;};
    e.mocks[sym("Actor_Spawn")]=[&]{++spawned;cpu.Regs()[0]=0;};
    cpu.ClearCache();e.Run("EnLinkPuppet_Update",actor,play);assert(killed==1);
    e.Run("Multiplayer_Ghosts_SpawnPuppets");assert(spawned==0);
    e.Run("Multiplayer_Ghosts_GetGhostData",2);assert(cpu.Regs()[0]==0);
    assert(R::ReadBytes(e,player,0x2300)==local_player);
    // A fresh peer packet can activate a retired slot without old rewards.
    e.mocks[sym("svcGetSystemTick")]=[&]{cpu.Regs()[0]=1;cpu.Regs()[1]=0;};
    cpu.ClearCache();e.Run("Multiplayer_Ghosts_UpdateGhostData",2,ghost+0x14,1);
    e.Run("Multiplayer_Ghosts_GetGhostData",2);assert(cpu.Regs()[0]==ghost_table.address+0x14);
    e.Run(ext("Rewards_Draw"),actor,play);assert(live.empty());
    e.Run(ext("Rewards_Receive"),2);e.Run(ext("Rewards_Draw"),actor,play);assert(live.size()==2);
    e.Run(ext("Rewards_Shutdown"));assert(live.empty());
    assert(e.Get<U32>(sym("rActiveItemRow"))==rows+rupee*24); // Never touches local reward globals.
    std::cout<<"Remote rewards: ARM capture/transport, two models, variants, stale/legacy/malformed packets, memory refusal and cleanup passed\n";
}
