// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../../../externals/oot3d-randomizer/shared/s_sfx.h"

void CheckExpandedRuntime(Env& env,Dynarmic::A32::Jit& jit,const R::Package& seed) {
    const auto symbol=[](const char* name){const auto at=R::Data().Find(name).address;assert(at);return at;};
    const auto rules=env.Get<SettingsContext>(symbol("gSettingsContext"));
    assert(sizeof(SettingsContext)==R::Data().Find("gSettingsContext").size);
    assert(rules.mqDungeonCount==(seed.mq?12:0));
    for(auto mode:rules.dungeonModes)assert(mode==(seed.mq?1:0));
    if(seed.json["format"].toInt()<2)return;
    const auto field=[&](const char* name){return QByteArray::fromBase64(seed.json["fields"].toObject()[name].toString().toLatin1());};
    const auto entrances=field("rEntranceOverrides");
    for(int i=0;i<entrances.size();i+=10) {
        const auto at=symbol("rEntranceOverrides")+i;
        const auto index=env.Get<std::uint16_t>(at),dest=env.Get<std::uint16_t>(at+6);
        if(!index && !dest)break;
        if(index<0x614 && dest<0x614) {
            env.Run("Entrance_GetOverride",index);
            assert(jit.Regs()[0]==dest);
        }
    }
    if(rules.enemizer) {
        const auto count=env.Get<unsigned>(symbol("rEnemyOverrides_Count"));
        assert(count>0 && count<800);
        for(unsigned i=0;i<count;++i) {
            const auto key=env.Get<U32>(symbol("rEnemyOverrides")+i*8);
            env.setup=[&]{jit.Regs()[3]=key&255;};
            env.Run("Enemizer_FindOverrideIndex",key>>24,(key>>16)&255,(key>>8)&255);
            assert(jit.Regs()[0]==i);
        }
    }
    if(rules.randomGsLocations) {
        unsigned count=0;
        for(unsigned i=0;i<100;++i) {
            const auto at=symbol("rGsLocOverrides")+i*28;
            const unsigned array=env.Get<std::uint8_t>(at),bit=env.Get<std::uint8_t>(at+1);
            if(!bit)break;
            env.Put<std::uint16_t>(0xc01000+0x1c,(array<<8)|bit);
            env.Run("Gs_HasAltLoc",0xc01000,1,0);assert(jit.Regs()[0]==1);++count;
        }
        assert(count>0);
    }
    for(unsigned i=0;i<85;++i) {
        env.Run("SetBGM",NA_BGM_FIELD+i);
        assert(jit.Regs()[0]==env.Get<U32>(symbol("rBGMOverrides")+4*i));
    }
    for(unsigned i=0;i<SFX_COUNT;++i) {
        const auto type=env.Get<std::uint8_t>(symbol("rSfxData")+54+i);
        const bool movement=type<=SEQ_CRAWL;
        // Keep the native child/adult voice adjustment separate from shuffle verification.
        if(NA_SE_PL_WALK_GROUND+i==NA_SE_VO_LI_LASH_KID)continue;
        env.Run("SetSFX",NA_SE_PL_WALK_GROUND+i);
        if(rules.shuffleSFX==SHUFFLESFX_ALL && type<SEQTYPE_COUNT &&
           (rules.shuffleSFXFootsteps || !movement) && (rules.shuffleSFXLinkVoice || i<1258 || i>1321))
            assert(jit.Regs()[0]==env.Get<U32>(symbol("rSfxData")+1444+i*4));
        else if(!rules.shuffleSFX)assert(jit.Regs()[0]==NA_SE_PL_WALK_GROUND+i);
    }
    for(U32 invalid:std::array<U32,4>{0u,NA_SE_PL_WALK_GROUND-1,NA_SE_PL_WALK_GROUND+SFX_COUNT,0xffffffffu}) {
        env.Run("SetSFX",invalid);assert(jit.Regs()[0]==invalid);
    }
    if(rules.shuffleMagicBeans) {
        for(unsigned factor:{1u,2u,3u}) {
            OOT3D::Difficulty::active_price_multiplier=factor;
            env.Put<std::int16_t>(0x587958+0x155c,0);
            env.Run("EnMs_Purchase",U32(-10));
            assert(env.Get<std::int16_t>(0x587958+0x155c)==-60*int(factor));
        }
    }
    R::UseDifficulty(seed.json["difficulty"].toObject());
    if(rules.randomSongNotes) {
        for(unsigned i=0;i<12;++i) {
            const auto count=env.Get<std::uint8_t>(0x54c222+9*i);assert(count>0 && count<=8);
            for(unsigned j=0;j<count;++j)
                assert(env.Get<U32>(symbol("rMenuSongsOverrides")+i*32+j*4)==env.Get<std::uint8_t>(0x54c223+i*9+j));
        }
    }
}

// Exercise the upstream ARM transport boundary and receive functions without
// opening game windows or joining an external room. Real room connectivity
// remains a separate two-instance playtest.
void CheckMultiplayerRuntime(Env& env,Dynarmic::A32::Jit& jit) {
    const auto symbol=[](const char* name){const auto at=R::Data().Find(name).address;assert(at);return at;};
    const U32 settings=symbol("gSettingsContext"),buffer=0xc40000;
    if(!env.Get<std::uint8_t>(settings+offsetof(SettingsContext,mp_Enabled)))return;
    env.Run("Multiplayer_OnFileLoad");
    env.Write(symbol("mBuffer"),buffer);env.Write(symbol("mBufSize"),0x1000);
    env.Put<std::uint8_t>(symbol("netStage"),3);
    QByteArray packet;
    env.mocks[symbol("udsSendTo")]=[&]{
        packet=R::ReadBytes(env,jit.Regs()[3],env.Get<U32>(jit.Regs()[13]));jit.Regs()[0]=0;
    };
    jit.ClearCache();
    env.Run("Multiplayer_Send_Item",0,0); // Deku Stick in its inventory slot.
    assert(packet.size()==36);
    env.mocks.clear();jit.ClearCache();
    const auto receive=[&](const QByteArray& incoming) {
        bool delivered=false;
        env.mocks[symbol("IsInGame")]=[&]{jit.Regs()[0]=0;};
        env.mocks[symbol("udsPullPacket")]=[&]{
            auto& r=jit.Regs();
            if(!delivered) {R::WriteBytes(env,r[1],incoming);env.Write(r[3],incoming.size());delivered=true;}
            else env.Write(r[3],0);
            env.Put<std::uint16_t>(env.Get<U32>(r[13]),2);r[0]=0;
        };
        jit.ClearCache();env.Run("Multiplayer_ReceivePackets");
        env.mocks.clear();jit.ClearCache();
    };
    const auto shared=symbol("mSaveContext");
    const auto before=R::ReadBytes(env,shared,R::Data().Find("mSaveContext").size);
    env.Put<std::uint8_t>(settings+offsetof(SettingsContext,mp_SyncId),7);
    receive(packet);
    assert(before==R::ReadBytes(env,shared,before.size()));
    env.Put<std::uint8_t>(settings+offsetof(SettingsContext,mp_SyncId),std::uint8_t(packet[4]));
    receive(packet.left(8));assert(before==R::ReadBytes(env,shared,before.size()));
    auto unknown=packet;unknown[0]=char(255);receive(unknown);assert(before==R::ReadBytes(env,shared,before.size()));
    receive(packet);
    assert(before!=R::ReadBytes(env,shared,before.size()));
    // Send/receive the real ARM ghost packet: color is an optional trailer,
    // leaving the old pose and joint-table formats intact.
    const auto global_pointer=symbol("gGlobalContext"),old_global=env.Get<U32>(global_pointer);
    constexpr U32 play=0xc50000,player=0xc58000;
    env.Write(global_pointer,play);env.Write(play+0x20ac,player);
    env.Put<std::int16_t>(play+0x104,7);
    env.mocks[symbol("Model_GetMeshGroupCount")]=[&]{jit.Regs()[0]=0;};
    env.mocks[symbol("Model_IsMeshGroupUsed")]=[&]{jit.Regs()[0]=0;};
    env.mocks[symbol("udsSendTo")]=[&]{
        packet=R::ReadBytes(env,jit.Regs()[3],env.Get<U32>(jit.Regs()[13]));jit.Regs()[0]=0;
    };
    env.Write(symbol("rMultiplayerTunicColor"),0xa512abef);jit.ClearCache();
    env.Run("Multiplayer_Send_GhostData");
    assert(packet.size()==48 && std::uint8_t(packet[44])==0xef && std::uint8_t(packet[47])==0xa5);
    env.mocks.clear();jit.ClearCache();
    const auto receive_color=[&](QByteArray bytes,U32 expected) {
        env.mocks[symbol("svcGetSystemTick")]=[&]{jit.Regs()[0]=1;jit.Regs()[1]=0;};
        receive(bytes);
        env.Run("Multiplayer_Ghosts_GetGhostData",2);const U32 data=jit.Regs()[0];assert(data);
        assert(env.Get<U32>(data)==7 && env.Get<U32>(data+40+25*52)==expected);
    };
    receive_color(packet,0xa512abef);
    auto other=packet;other[44]=char(0x30);other[45]=char(0x20);other[46]=char(0x10);
    receive_color(other,0xa5102030);
    receive_color(packet.left(44),0); // Legacy peers have no override.
    other[47]=char(0xff);receive_color(other,0); // Invalid tags cannot become colors.
    env.Write(global_pointer,old_global);
    std::cout<<"Multiplayer color: native wire roundtrip, live changes and legacy packet fallback passed\n";
    // A shutdown releases its allocation once, and the trampoline preserves
    // the native file-menu state while returning to the intercepted PC.
    unsigned frees=0;
    env.mocks[symbol("SystemArena_Free")]=[&]{assert(jit.Regs()[0]==buffer);++frees;};
    jit.ClearCache();env.Run("Multiplayer_Shutdown");
    assert(frees==1 && env.Get<U32>(symbol("mBuffer"))==0 && env.Get<U32>(symbol("rHostShutdown"))==1);
    env.Run("Multiplayer_Shutdown");assert(frees==1);
    env.mocks.clear();jit.ClearCache();
    unsigned srvStarts=0,udsStarts=0,srvStops=0,udsStops=0,hosts=0,unbinds=0,destroys=0,scans=0;
    std::uint64_t ticks=1;
    const auto mock=[&](const char* name,std::function<void()> fn){env.mocks[symbol(name)]=std::move(fn);};
    mock("svcGetSystemTick",[&]{jit.Regs()[0]=U32(ticks);jit.Regs()[1]=U32(ticks>>32);});
    mock("srvInit",[&]{++srvStarts;jit.Regs()[0]=0;});
    mock("udsInit",[&]{++udsStarts;jit.Regs()[0]=0;});
    mock("srvExit",[&]{++srvStops;});mock("udsExit",[&]{++udsStops;});
    mock("SystemArena_Malloc",[&]{jit.Regs()[0]=buffer;});
    mock("SystemArena_Free",[&]{assert(jit.Regs()[0]==buffer);++frees;});
    mock("udsScanBeacons",[&]{++scans;env.Write(jit.Regs()[3],0);jit.Regs()[0]=0;});
    mock("udsGenerateDefaultNetworkStruct",[&]{});
    mock("udsCreateNetwork",[&]{++hosts;env.Write(jit.Regs()[3]+4,1);jit.Regs()[0]=0;});
    mock("udsUnbind",[&]{++unbinds;env.Write(jit.Regs()[0]+4,0);jit.Regs()[0]=0;});
    mock("udsDestroyNetwork",[&]{++destroys;jit.Regs()[0]=0;});
    mock("udsDisconnectNetwork",[&]{jit.Regs()[0]=0;});
    mock("udsGetConnectionStatus",[&]{jit.Regs()[0]=0xffffffff;});
    mock("IsInGameOrBossChallenge",[&]{jit.Regs()[0]=0;});
    namespace Startup=OOT3D::MultiplayerStartup;
    Startup::SetSession(0);Startup::Begin(true);
    // Match an existing MP-enabled save's fresh payload: stage zero, no room.
    // It must never reach even srvInit, let alone the old blocking scan loop.
    env.Put<std::uint8_t>(symbol("netStage"),0);
    R::multiplayer_command=0;jit.ClearCache();
    for(unsigned i=0;i<90;++i)env.Run("Multiplayer_Run");
    assert(srvStarts==0 && udsStarts==0 && scans==0 && hosts==0);
    assert(env.Get<std::int8_t>(symbol("netStage"))==-2 && Startup::phase==Startup::Phase::Idle);
    assert(!env.Get<U32>(symbol("mBuffer")) && !env.Get<U32>(symbol("rMultiplayerHold")));
    // A stale/manual reconnect request cannot opt an offline player into UDS.
    R::multiplayer_command=1;env.Run("Multiplayer_Run");
    assert(srvStarts==0 && udsStarts==0 && scans==0 && hosts==0);
    Startup::SetSession(1,"FileName");R::multiplayer_command=1;
    const unsigned old_scans=scans,old_hosts=hosts;
    for(unsigned i=0;i<4;++i)env.Run("Multiplayer_Run");
    assert(hosts==old_hosts+1 && scans==old_scans);
    assert(env.Get<U32>(symbol("rMultiplayerHold"))==0 && env.Get<U32>(symbol("rMultiplayerReady"))==1);
    assert(Startup::phase==Startup::Phase::Ready);
    assert(R::ReadBytes(env,symbol("rMultiplayerUsername"),8)=="FileName");
    assert(env.Get<std::uint8_t>(symbol("hostNonblocking"))==1);
    // Produce a full snapshot through the actual ARM serializers.
    const auto old_progress=env.Get<std::uint8_t>(settings+offsetof(SettingsContext,mp_SharedProgress));
    env.Put<std::uint8_t>(settings+offsetof(SettingsContext,mp_SharedProgress),1);
    std::vector<QByteArray> fullsync;
    mock("udsSendTo",[&]{fullsync.push_back(R::ReadBytes(env,jit.Regs()[3],env.Get<U32>(jit.Regs()[13])));jit.Regs()[0]=0;});
    jit.ClearCache();
    env.Run("Multiplayer_Send_BaseSync",2);
    env.Run("Multiplayer_Send_FullEntranceSync",2);
    for(unsigned i=0;i<4;++i)env.Run("Multiplayer_Send_FullSceneFlagSync",2,i);
    for(unsigned i=0;i<R::Data().Find("mp_completeSyncs").size-6;++i)env.Run("Multiplayer_Send_FullExtInfSync",2,i);
    assert(fullsync.size()==R::Data().Find("mp_completeSyncs").size);
    R::multiplayer_command=2;env.Run("Multiplayer_Run");
    assert(srvStops==1 && udsStops==1 && destroys==1 && unbinds==1);
    Startup::SetSession(2,"Guest");R::multiplayer_command=1;
    unsigned connects=0;bool found=false,associated=false;
    mock("udsScanBeacons",[&]{++scans;env.Write(jit.Regs()[2],0xc70000);env.Write(jit.Regs()[3],found?1:0);jit.Regs()[0]=0;});
    mock("SystemArena_Free",[&]{assert(jit.Regs()[0]==buffer || jit.Regs()[0]==0xc70000);++frees;});
    mock("udsConnectNetwork",[&]{++connects;jit.Regs()[0]=0;});
    mock("udsGetConnectionStatus",[&]{const auto at=jit.Regs()[0];env.Write(at,associated?9:7);env.Put<std::uint8_t>(at+0x28,2);jit.Regs()[0]=0;});
    jit.ClearCache();env.Run("Multiplayer_Run");env.Run("Multiplayer_Run");
    const auto waiting_scans=scans;
    for(unsigned i=0;i<60;++i)env.Run("Multiplayer_Run");
    assert(scans==waiting_scans && hosts==old_hosts+1); // No spin or accidental guest host.
    found=true;ticks+=268123480/10+1;env.Run("Multiplayer_Run");
    assert(connects==1 && env.Get<U32>(symbol("rMultiplayerHold"))==1);
    for(unsigned i=0;i<30;++i)env.Run("Multiplayer_Run");
    assert(connects==1 && env.Get<std::uint8_t>(symbol("netStage"))==1);
    associated=true;env.Run("Multiplayer_Run");env.Run("Multiplayer_Run");
    assert(env.Get<std::uint8_t>(symbol("netStage"))==3 && env.Get<std::uint8_t>(symbol("mp_isSyncing"))==1);
    assert(env.Get<U32>(symbol("rMultiplayerHold"))==1);
    // Each update receives a single section, keeping partial data private.
    env.Write(global_pointer,play);
    mock("IsInGame",[&]{jit.Regs()[0]=1;});
    mock("IsInGameOrBossChallenge",[&]{jit.Regs()[0]=1;});
    mock("Multiplayer_Send_GhostData",[&]{});
    mock("Multiplayer_Send_GhostData_JointTable",[&]{});
    mock("Multiplayer_Ghosts_Tick",[&]{});
    mock("Multiplayer_Ghosts_SpawnPuppets",[&]{});
    mock("udsSendTo",[&]{jit.Regs()[0]=0;});
    const auto health=env.Get<std::uint16_t>(0x587958+0x42);
    env.Put<std::uint16_t>(0x587958+0x42,health+0x10);
    for(unsigned i=0;i<fullsync.size();++i) {
        bool delivered=false;
        mock("udsPullPacket",[&]{auto& r=jit.Regs();
            if(!delivered){R::WriteBytes(env,r[1],fullsync[i]);env.Write(r[3],fullsync[i].size());delivered=true;}
            else env.Write(r[3],0);
            env.Put<std::uint16_t>(env.Get<U32>(r[13]),1);r[0]=0;
        });
        jit.ClearCache();env.Run("Multiplayer_Run");
        if(i+1<fullsync.size()) {
            assert(env.Get<U32>(symbol("rMultiplayerHold"))==1);
            assert(env.Get<std::uint16_t>(0x587958+0x42)==health+0x10);
        }
    }
    assert(env.Get<U32>(symbol("rMultiplayerReady"))==1 && env.Get<U32>(symbol("rMultiplayerHold"))==0);
    assert(env.Get<std::uint16_t>(0x587958+0x42)==health);
    // Losing the room shuts down the guest connection and releases gameplay,
    // without falling back to the old automatic wireless discovery path.
    const auto scans_before_leave=scans,starts_before_leave=srvStarts;
    const auto ghosts=R::Data().Find("ghosts");
    assert(ghosts.size==16*1368);
    for(unsigned i=0;i<16;++i) {
        env.Put<std::uint8_t>(ghosts.address+i*1368,1);
        env.Put<std::uint8_t>(ghosts.address+i*1368+0x12,1);
    }
    Startup::SetSession(0);env.Run("Multiplayer_Run");
    for(unsigned i=0;i<16;++i) {
        assert(!env.Get<std::uint8_t>(ghosts.address+i*1368));
        assert(!env.Get<std::uint8_t>(ghosts.address+i*1368+0x12));
    }
    for(unsigned i=0;i<40;++i)env.Run("Multiplayer_Run");
    assert(env.Get<std::int8_t>(symbol("netStage"))==-2 && Startup::phase==Startup::Phase::Idle);
    assert(!env.Get<U32>(symbol("rMultiplayerHold")) && !env.Get<U32>(symbol("mBuffer")));
    assert(scans==scans_before_leave && srvStarts==starts_before_leave);
    Startup::SetSession(2,"Guest");
    // Failure has a bounded wait; explicit Disconnect always releases gameplay.
    R::multiplayer_command=2;env.Run("Multiplayer_Run");
    found=false;R::multiplayer_command=1;env.Run("Multiplayer_Run");
    ticks+=21ULL*268123480;env.Run("Multiplayer_Run");env.Run("Multiplayer_Run");
    assert(Startup::phase==Startup::Phase::Failed && env.Get<U32>(symbol("rMultiplayerHold"))==1);
    R::multiplayer_command=2;env.Run("Multiplayer_Run");
    assert(Startup::phase==Startup::Phase::Idle && !env.Get<U32>(symbol("rMultiplayerHold")));
    mock("SystemArena_Malloc",[&]{jit.Regs()[0]=0;});
    jit.ClearCache();R::multiplayer_command=1;env.Run("Multiplayer_Run");env.Run("Multiplayer_Run");
    assert(Startup::phase==Startup::Phase::Failed && env.Get<U32>(symbol("rMultiplayerHold"))==1);
    R::multiplayer_command=2;env.Run("Multiplayer_Run");
    assert(!env.Get<U32>(symbol("rMultiplayerHold")));
    // Execute the native update prologue and assembly bridge with the hold set.
    // It must return through its own saved stack, without entering simulation.
    mock("before_GlobalContext_Update",[&]{env.Write(symbol("rMultiplayerHold"),1);});
    mock("after_GlobalContext_Update",[&]{});
    bool simulated=false,rendered=false;
    const auto old_update_owner=env.Read(0x539f98).value();
    for(U32 at:{0x2e4514u,0x2e2e60u})env.mocks[at]=[&]{simulated=true;};
    env.mocks[0x2e25f0]=[&]{rendered=true;};
    jit.ClearCache();env.Run(0x45238c,play);assert(!simulated && rendered);
    assert(env.Read(0x539f98)==play+0x14);
    mock("before_GlobalContext_Update",[&]{env.Write(symbol("rMultiplayerHold"),0);});
    jit.ClearCache();env.Run(0x45238c,play);assert(simulated);
    env.Write(0x539f98,old_update_owner);
    env.Write(global_pointer,old_global);
    env.Put<std::uint8_t>(settings+offsetof(SettingsContext,mp_SharedProgress),old_progress);
    Startup::SetSession(0);Startup::phase=Startup::Phase::Idle;
    env.mocks.clear();jit.ClearCache();
    std::cout<<"Multiplayer startup: offline isolation, direct host, polled guest, atomic full sync, room loss, bounded failure and native gameplay hold passed\n";
}
