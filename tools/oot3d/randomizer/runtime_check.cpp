// SPDX-License-Identifier: GPL-2.0-or-later
// Execute the actual ARM randomizer payload and native save routines, without a window.
#include "oot3d/randomizer/hooks.h"
#include "oot3d/master_quest.h"
#include "oot3d/death_menu.h"
#include "oot3d/price_runtime.h"
#include "dynarmic/interface/A32/a32.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <cassert>
#include <chrono>
#include <thread>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <map>
#include "../../../externals/oot3d-randomizer/shared/s_settings.h"
#include "../../../externals/oot3d-randomizer/shared/s_seq_id.h"

namespace R=OOT3D::Randomizer;
using U32=std::uint32_t;
struct Env : Dynarmic::A32::UserCallbacks {
    static constexpr U32 SP=0xe00000,Stop=0xf00000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;
    std::uint64_t remaining=0;
    bool stopped=false;
    unsigned host_writes=0;
    U32 stop_at=Stop;
    std::function<void()> setup;
    std::function<void(unsigned)> menu_svc;
    std::map<U32,std::function<void()>> mocks;
    U32 mock_at=0;
    std::function<void()> mock;
    template<class T> T Get(U32 at) {assert(std::uint64_t(at)+sizeof(T)<=bytes.size());T v;std::memcpy(&v,&bytes[at],sizeof v);return v;}
    template<class T> void Put(U32 at,T v) {assert(std::uint64_t(at)+sizeof(T)<=bytes.size());std::memcpy(&bytes[at],&v,sizeof v);}
    std::optional<U32> Read(U32 at) {if(std::uint64_t(at)+4>bytes.size())return {};return Get<U32>(at);}
    void Write(U32 at,U32 v) {++host_writes;Put(at,v);}
    std::optional<U32> MemoryReadCode(U32 at) override {
        if(mocks.contains(at))return 0xef000003;
        if(at==stop_at)return 0xef000000;
        if(mock_at && at==mock_at)return 0xef000002;
        if(at==0x35010c || at==0x306e40 || at==0x324f44)return 0xef000001;
        if(at==OOT3D::Prices::TextSite.pc)return 0xef000000|OOT3D::Prices::TextSVC;
        const auto word=R::Instruction(at,Get<U32>(at));
        return OOT3D::MasterQuest::Instruction(at,word);
    }
    std::uint8_t MemoryRead8(U32 at) override {return Get<std::uint8_t>(at);}
    std::uint16_t MemoryRead16(U32 at) override {return Get<std::uint16_t>(at);}
    U32 MemoryRead32(U32 at) override {return Get<U32>(at);}
    std::uint64_t MemoryRead64(U32 at) override {return Get<std::uint64_t>(at);}
    void MemoryWrite8(U32 at,std::uint8_t v) override {Put(at,v);}
    void MemoryWrite16(U32 at,std::uint16_t v) override {Put(at,v);}
    void MemoryWrite32(U32 at,U32 v) override {Put(at,v);}
    void MemoryWrite64(U32 at,std::uint64_t v) override {Put(at,v);}
    void InterpreterFallback(U32 pc,std::size_t) override {std::cerr<<"fallback "<<std::hex<<pc<<'\n';std::abort();}
    void ExceptionRaised(U32 pc,Dynarmic::A32::Exception) override {std::cerr<<"exception "<<std::hex<<pc<<'\n';std::abort();}
    void AddTicks(std::uint64_t n) override {remaining=n>remaining?0:remaining-n;}
    std::uint64_t GetTicksRemaining() override {return remaining;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();
        if(!svc) {stopped=true;cpu->HaltExecution();return;}
        if(svc==3) {mocks.at(r[15]-4)();r[15]=r[14];return;}
        if(svc==2) {assert(mock);mock();r[15]=r[14];return;}
        if(svc==1) {
            if(r[15]-4==0x35010c)r[0]=0xb80000;
            if(r[15]-4==0x324f44) {
                // The native C locale is initialized by CRT startup, not this
                // isolated harness. Save filenames are strictly ASCII.
                unsigned i=0;
                for(;i<r[2];++i) {const auto c=Get<std::uint8_t>(r[1]+i);Put<std::uint16_t>(r[0]+i*2,c);if(!c)break;}
                r[0]=i;
            }
            r[15]=r[14];return;
        }
        if(svc==OOT3D::Prices::TextSVC) {OOT3D::Prices::SelectText(*this,r);return;}
        if(svc>=R::FirstSVC && svc<R::FirstSVC+15) {
            R::runtime.Handle(*this,r,svc-R::FirstSVC);
            if(!R::runtime.resource_failure.empty()) {stopped=true;cpu->HaltExecution();}
            return;
        }
        if(menu_svc && svc>=OOT3D::MasterQuest::FirstSVC && svc<OOT3D::MasterQuest::FirstSVC+OOT3D::MasterQuest::Sites.size()) {
            menu_svc(svc-OOT3D::MasterQuest::FirstSVC);return;
        }
        if(svc==OOT3D::MasterQuest::FirstSVC+1) {r[0]=0;r[15]=r[14];return;}
        if(svc==0x2a) {r[0]=0;r[1]=1;r[2]=0;return;} // svcGetSystemInfo: emulator.
        std::cerr<<"unexpected svc "<<std::hex<<svc<<" pc="<<r[15]<<'\n';std::abort();
    }
    void Run(U32 pc,U32 a=0,U32 b=0,U32 c=0) {
        cpu->Reset();cpu->SetCpsr(0x10);
        auto& r=cpu->Regs();r[0]=a;r[1]=b;r[2]=c;r[13]=SP;r[14]=Stop;r[15]=pc;
        if(setup) {auto prepare=std::move(setup);setup={};prepare();}
        remaining=10000000;stopped=false;cpu->Run();
        if(!stopped)std::cerr<<"did not return pc="<<std::hex<<r[15]<<'\n';
        assert(stopped);
    }
    void Run(const char* name,U32 a=0,U32 b=0,U32 c=0) {
        auto symbol=R::Data().Find(name);assert(symbol.address);Run(symbol.address,a,b,c);
    }
};
#include "hook_abi_check.h"
#include "expanded_runtime_check.h"
#include "room_import_check.h"
#include "player_color_check.h"
#include "rewards_check.h"

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    assert(argc>=5);
    QTemporaryDir temp;assert(temp.isValid());
    QSettings settings(temp.filePath("settings.ini"),QSettings::IniFormat);
    R::Initialize(settings,temp.path().toStdString());
    const auto draft=R::GetConfig();
    R::FileFlow{}.ToggleRandomizer();
    assert(R::GetConfig().enabled!=draft.enabled && R::FileStatus::enabled==R::GetConfig().enabled && R::dirty);
    assert(R::GetConfig().seed==draft.seed && R::GetConfig().options==draft.options);
    R::SaveConfig(settings);R::Initialize(settings,temp.path().toStdString());
    assert(R::GetConfig().enabled!=draft.enabled && R::FileStatus::enabled==R::GetConfig().enabled);
    R::SetConfig(draft);assert(R::FileStatus::enabled==draft.enabled);
    R::player_color=0x12abef;R::player_color_enabled=true;
    R::SaveConfig(settings);R::player_color=0;R::player_color_enabled=false;
    R::Initialize(settings,temp.path().toStdString());
    assert(R::PlayerColor()==0xa512abef && R::GetConfig()==draft);
    auto object=R::ReadJson(QString::fromLocal8Bit(argv[2]));
    auto package=R::DecodePackage(object);assert(package);
    assert(package->json["fields"]==object["fields"] && package->json["message_data"]==object["message_data"]);
    assert(package->json["runtime"].toString()==QString::fromLatin1(R::Data().digest));
    auto mq=R::DecodePackage(R::ReadJson(QString::fromLocal8Bit(argv[3])));assert(mq && mq->mq);
    std::vector<R::Package> seeds{*package,*mq};
    for(int i=5;i<argc;++i) {
        const auto input=R::ReadJson(QString::fromLocal8Bit(argv[i]));
        auto extra=R::DecodePackage(input);assert(extra);
        assert(extra->json["fields"].toObject()["rItemOverrides"]==input["fields"].toObject()["rItemOverrides"]);
        for(auto fields=input["fields"].toObject();auto key:fields.keys())
            assert(extra->json["fields"].toObject()[key]==fields[key]);
        assert(extra->json["message_data"]==input["message_data"]);
        assert(extra->json["difficulty"]==input["difficulty"]);
        assert(extra->extended_save==QByteArray::fromBase64(input["extended_save"].toString().toLatin1()));
        assert(extra->config==R::FromJson(input["config"].toObject()));
        assert(extra->json["runtime"].toString()==QString::fromLatin1(R::Data().digest));
        seeds.push_back(*extra);
    }
    auto corrupt=object;corrupt["runtime"]="wrong";assert(!R::DecodePackage(corrupt));
    corrupt=object;auto fields=corrupt["fields"].toObject();fields["gSettingsContext"]="AA==";corrupt["fields"]=fields;
    assert(!R::DecodePackage(corrupt));
    corrupt=object;corrupt["message_count"]=-1;assert(!R::DecodePackage(corrupt));
    Env env;
    // USA Rev 1 exheader: native BSS ends at 0x5c6608, while the
    // emulator's conservative page-rounded allocation ends at 0x5c8000.
    // That padding must not prevent reserving the injected payload.
    static_assert(R::CanReserveGuestMemory(0x4f9000, 397856, 443368, 847872));
    static_assert(!R::CanReserveGuestMemory(0x4f9000, 397856, 447457, 851968));
    static_assert(!R::CanReserveGuestMemory(0x4f9000, 397856, 443368, 401408));
    static_assert(!R::CanReserveGuestMemory(0x4f9000, 397856, 443368, 0x600000));
    std::ifstream code(argv[1],std::ios::binary);assert(code);code.read(reinterpret_cast<char*>(env.bytes.data()+0x100000),0x4c7000);
    assert(R::CodeMatches(env));
    Dynarmic::A32::UserConfig config;config.callbacks=&env;
    Dynarmic::A32::Jit jit(config);env.cpu=&jit;
    // The archive guard is a read-only check before the original BL. Valid
    // input must arrive at ZAR_Init with identical CPU/memory state; invalid
    // input must halt before the constructor can allocate or write anything.
    {
        constexpr U32 buffer=0xa01000,owner=0xa02000;
        env.Write(buffer,0x0152415a);env.Write(buffer+4,32);env.Write(buffer+8,0);
        for(unsigned off:{12u,16u,20u})env.Write(buffer+off,32);
        env.stop_at=0x31b124;
        const auto init=[&] {
            for(unsigned i=3;i<13;++i)jit.Regs()[i]=0x12340000+i;
            jit.Regs()[4]=owner;jit.SetCpsr(0xa0000010);
        };
        R::active=false;env.setup=init;
        env.Run(R::ObjectArchiveSite.pc,owner+16,buffer,32);
        const auto native=jit.Regs();const auto flags=jit.Cpsr();const auto bytes=env.bytes;
        R::active=true;jit.ClearCache();env.setup=init;
        env.Run(R::ObjectArchiveSite.pc,owner+16,buffer,32);
        assert(jit.Regs()==native && jit.Cpsr()==flags && env.bytes==bytes);
        env.Write(buffer+8,0xffffffff);env.Write(owner,0);const auto invalid=env.bytes;
        env.setup=init;
        env.Run(R::ObjectArchiveSite.pc,owner+16,buffer,32);
        assert(!R::runtime.resource_failure.empty() && jit.Regs()[15]==R::ObjectArchiveSite.pc+4);
        assert(env.bytes==invalid); // Diagnostic logging must not mutate guest data.
        R::runtime.resource_failure.clear();R::active=false;env.stop_at=Env::Stop;jit.ClearCache();
    }
    // Native initialization legitimately resets menu cursor globals; establish
    // those native defaults before checking randomizer table restoration.
    env.Run(0x4499b0);
    // The native initializer reuses the live SaveContext. A previous randomized
    // file's reserved marker must not classify a newly created ordinary file.
    for(unsigned target=0;target<3;++target)for(bool master:{false,true}) {
        env.Put<std::uint8_t>(0x587958+R::SaveMarkerOffset,R::SaveMarker);
        const auto existing_slots=R::ReadBytes(env,0x55bec0,3*0x15c4);
        assert(R::FileFlow{}.Prepare(env,target,master)==1);
        assert(!R::new_randomized && R::new_slot==target);
        // Merely opening/cancelling naming must not alter a previous save.
        assert(env.Get<std::uint8_t>(0x587958+R::SaveMarkerOffset)==R::SaveMarker);
        OOT3D::MasterQuest::SetMode(env,master);
        env.Put<std::uint8_t>(0x587958+0x156f,0x3d);
        env.Run(0x4499b0);
        assert(env.Get<std::uint8_t>(0x587958+0x156f)==0);
        const auto ordinary=R::ReadBytes(env,0x587958,0x14dc);
        assert(std::uint8_t(ordinary[R::SaveMarkerOffset])==0);
        assert(ordinary.mid(0x30,6)=="ZELDAZ" && (ordinary[0xe]!=0)==master);
        R::WriteBytes(env,0xc10000,ordinary);
        QByteArray filename;
        for(char c:std::string("data:/save0")+std::to_string(target)+".bin") {
            filename+=c;filename+=char(0);
        }
        filename+=QByteArray(2,0);R::WriteBytes(env,0xc20000,filename);
        env.Run(0x2e613c,0xc20000,0xc10000,ordinary.size());
        assert(jit.Regs()[0]==0xb80000); // Native write queue accepted the file.
        bool invalid=false;
        assert(!R::LoadSlot(target,ordinary,invalid) && !invalid);
        assert(R::ReadBytes(env,0x55bec0,3*0x15c4)==existing_slots);
        auto marked=ordinary;marked[R::SaveMarkerOffset]=char(R::SaveMarker);
        assert(!R::RecordSave(target,marked,nullptr,{})); // Missing seed still blocks real rando saves.
    }
    OOT3D::MasterQuest::SetMode(env,false);env.Run(0x4499b0);
    // Model an ordinary session which already loaded the melody archive.
    // Its object slot is repurposed for the larger randomizer asset archive.
    constexpr U32 melody=0x53ccf4+0xb6*0x44;
    env.Write(melody+0x40,7276);
    const auto before=R::ReadBytes(env,0x4cc000,0x590000-0x4cc000);
    // Price variants use real English/French/Spanish QM text and the actual
    // native message lookup, with ordinary startup prices preserved.
    std::ifstream qm(argv[4],std::ios::binary);assert(qm);
    const std::vector<unsigned char> original_qm{std::istreambuf_iterator<char>(qm),{}};
    auto archive=original_qm;
    assert(OOT3D::Prices::PrepareTextVariants(archive,2));
    const auto before_loading_prompt=archive;
    assert(OOT3D::LoadingPrompt::Prepare(archive));
    assert(std::equal(before_loading_prompt.begin(),before_loading_prompt.end(),archive.begin()));
    assert(archive.size()<0x300000);
    const U32 archive_at=0xa00000,owner=0xd80000;
    std::memcpy(env.bytes.data()+archive_at,archive.data(),archive.size());
    env.Write(owner+4,archive_at);env.Write(owner+8,archive_at+16);
    OOT3D::Difficulty::price_text_ready=true;
    const auto variants=std::atomic_load(&OOT3D::Prices::text_variants);
    for(unsigned factor:{1u,2u,3u}) {
        OOT3D::Difficulty::file_rules=true;OOT3D::Difficulty::increase_prices=factor>1;
        OOT3D::Difficulty::price_multiplier=std::max(2u,factor);
        for(const auto& [id,offsets]:variants->entries) {
            env.Run(0x2df4c4,owner,id);assert(jit.Regs()[0]==archive_at+offsets[factor-1]);
        }
        assert(OOT3D::Prices::Factor()==factor);
    }
    OOT3D::Difficulty::file_rules=false;
    // The loading label goes through the real native message lookup with the
    // same prompt formatting commands, including when no seed is active yet.
    env.Run(0x2df4c4,owner,OOT3D::LoadingPrompt::MessageId);
    const auto loading_entry=jit.Regs()[0];
    assert(env.Read(loading_entry)==OOT3D::LoadingPrompt::MessageId);
    for(unsigned language:{1u,5u,7u}) {
        const auto offset=env.Read(loading_entry+16+language*8).value();
        const auto size=env.Read(loading_entry+20+language*8).value();
        const auto text=R::ReadBytes(env,archive_at+offset,size);
        assert(text.startsWith(QByteArray::fromHex("7f047f1e")) && text.endsWith(QByteArray::fromHex("7f057f00")));
        assert(text.contains(language==5?"Randomisation...":language==7?"Aleatorizando...":"Randomizing..."));
    }
    OOT3D::MasterQuest::choosing=true;OOT3D::MasterQuest::loading_label=true;
    assert(OOT3D::MasterQuest::PromptMessage()==OOT3D::LoadingPrompt::MessageId);
    OOT3D::MasterQuest::loading_label=false;
    assert(OOT3D::MasterQuest::PromptMessage()==0x900);
    OOT3D::MasterQuest::choosing=false;
    assert(OOT3D::MasterQuest::PromptMessage()==0x950);
    for(const auto& [id,offsets]:variants->entries) {
        env.Run(0x2df4c4,owner,id);
        const auto ordinary=R::ReadBytes(env,jit.Regs()[0],96);
        assert(ordinary==R::ReadBytes(env,archive_at+offsets[1],96));
    }
    OOT3D::Difficulty::increase_prices=false;OOT3D::Difficulty::price_multiplier=2;
    const auto difficulty=R::CaptureDifficulty();
    // Only substituted instructions and injected code need invalidation;
    // unrelated menu/renderer code must remain translated.
    std::size_t invalidated=0;
    for(const auto& [first,end]:R::InvalidationRanges()) {assert(first<end);invalidated+=end-first;}
    for(const auto& [pc,word]:R::CodePatches()) {
        bool found=false;for(const auto& [first,end]:R::InvalidationRanges())found|=pc>=first && pc+4<=end;
        assert(found);
    }
    assert(invalidated<0x500000);
    // Stage a real seed in bounded slices, and prove cancellation has not
    // touched native tables/save bytes or enabled partially uploaded code.
    R::runtime.CheckEpoch();
    const auto untouched=R::ReadBytes(env,0x4cc000,R::GuestBase-0x4cc000);
    R::runtime.staging=*package;R::runtime.staging_slot=0;
    env.host_writes=0;
    assert(R::FileFlow{}.Prepare(env,0,false)==0 && !R::active);
    assert(env.host_writes<=8192 && R::runtime.staging_offset==32768);
    R::FileFlow{}.Cancel();
    assert(!R::runtime.staging && !R::active && R::new_slot==3);
    assert(R::ReadBytes(env,0x4cc000,R::GuestBase-0x4cc000)==untouched);
    R::runtime.staging=*package;R::runtime.staging_slot=0;
    unsigned steps=0;
    while(true) {
        env.host_writes=0;
        const int result=R::FileFlow{}.Prepare(env,0,false);++steps;
        assert(result>=0 && steps<200);
        if(result==1)break;
        assert(env.host_writes<=8192 && !R::active);
    }
    assert(steps>1 && R::active && R::new_slot==0);
    assert(R::ReadBytes(env,R::GuestBase,package->payload.size())==package->payload);
    R::runtime.Deactivate(env);R::new_slot=3;R::new_randomized=false;R::creation_chime=false;
    assert(R::ReadBytes(env,0x4cc000,R::GuestBase-0x4cc000)==untouched);
    // The real subprocess, package decoding and permission probe complete on
    // the worker. Polling does not install any game memory or create a save.
    auto async_config=draft;async_config.enabled=true;async_config.seed="native-loading-check";
    auto cancelled_config=async_config;cancelled_config.seed="cancelled-loading-check";
    R::SetConfig(cancelled_config);
    assert(R::PrepareNew(0,false)==0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    R::CancelPreparation();
    assert(!R::PreparationPending() && !R::busy && !R::TakePrepared(0,false));
    R::SetConfig(async_config);
    const auto generation_start=std::chrono::steady_clock::now();
    double max_poll_ms=0;int generation_result=0;
    do {
        const auto start=std::chrono::steady_clock::now();
        generation_result=R::PrepareNew(0,false);
        max_poll_ms=std::max(max_poll_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        if(generation_result<0)std::cerr<<R::Status()<<std::endl;
        assert(generation_result>=0);
        assert(std::chrono::steady_clock::now()-generation_start<std::chrono::seconds(60));
        if(!generation_result)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while(!generation_result);
    const auto generated=R::TakePrepared(0,false);
    assert(generated && generated->config.seed==async_config.seed && !R::active && R::new_slot==3);
    std::cout<<"Native loading: "<<steps<<" upload slices; maximum generation poll "<<max_poll_ms<<"ms"<<std::endl;
    R::CancelPreparation();R::SetConfig(draft);
    bool checked_abi=false;
    for(auto& seed_value:seeds) {
        auto* seed=&seed_value;
        std::cout<<"Runtime seed: "<<seed->config.seed<<" advanced="<<seed->config.advanced.size()<<" mq="<<seed->mq<<std::endl;
        assert(R::runtime.Activate(env,0,*seed));jit.ClearCache();
        if(!checked_abi) {CheckHookABI(env,jit);checked_abi=true;}
        // Follow the real order: create/load a file, choose its savewarp
        // entrance, then enter gameplay. Previously the test initialized the
        // runtime first, hiding an all-zero entrance table at file load.
        R::new_slot=0;R::new_randomized=true;R::new_mq=seed->mq;
        OOT3D::MasterQuest::SetMode(env,seed->mq);
        // Reproduce a previous adult file's temporary B item surviving the
        // file-select initializer, then execute the real native save fallback.
        env.Put<std::uint8_t>(0x587958+0x156f,0x3d);
        env.Run(0x4499b0);
        const auto before_save_b=env.Get<std::uint8_t>(0x587958+0x80);
        env.setup=[&] {jit.Regs()[8]=0x587958;jit.Regs()[4]=0x588958;};
        env.stop_at=0x2fdd08;env.Run(0x2fdce0);env.stop_at=Env::Stop;
        const auto after_save_b=env.Get<std::uint8_t>(0x587958+0x80);
        std::cout<<"New-file B restore: before="<<unsigned(before_save_b)
                 <<" after="<<unsigned(after_save_b)<<std::endl;
        assert(after_save_b==before_save_b);
        const auto raw=R::ReadBytes(env,0x587958,0x14dc);
        // A previous file's inventory, equipment, health, quest/dungeon/scene
        // progress and other persistent fields must all reset to the same
        // new-file defaults. Names/preferences before +0x30 are intentional.
        for(unsigned pattern:{0x5au,0xa5u}) {
            R::WriteBytes(env,0x587958+0x30,QByteArray(0x14dc-0x30,char(pattern)));
            R::new_slot=0;R::new_randomized=true;
            env.Run(0x4499b0);
            assert(R::ReadBytes(env,0x587958+0x30,0x14dc-0x30)==raw.mid(0x30));
        }
        std::cout<<"New-file persistent inventory/progress reset passed"<<std::endl;
        // Repair the already-saved stale B assignment without touching any
        // inventory/progress bytes, and preserve owned or genuinely equipped
        // swords. Exercise the actual ARM load helper for both quest modes.
        for(unsigned sword:{0x3bu,0x3cu,0x3du,0x55u}) {
            const unsigned mask=sword==0x3b?1:sword==0x3c?2:4;
            for(unsigned state=0;state<3;++state) {
                R::WriteBytes(env,0x587958,raw);
                env.Put<U32>(0x587958+4,1);
                env.Put<std::uint16_t>(0x587958+0xb6,0x1100|(state==1?mask:0));
                env.Put<std::uint16_t>(0x587958+0x8a,0x1100|(state==2?1:0));
                env.Put<std::uint8_t>(0x587958+0x80,sword);
                auto expected=R::ReadBytes(env,0x587958,0x14dc);
                if(state==0)expected[0x80]=char(0xff);
                env.Run("SaveFile_LoadFileSwordless");
                assert(R::ReadBytes(env,0x587958,0x14dc)==expected);
            }
        }
        R::WriteBytes(env,0x587958,raw);
        const auto rules=env.Get<SettingsContext>(R::Data().Find("gSettingsContext").address);
        const bool adult=rules.resolvedStartingAge==AGE_ADULT;
        assert(env.Get<std::uint16_t>(0x587958+0x7e)==(adult?0x43:0x34));
        // An uninitialized table really does map Link's House to Deku Tree.
        env.Run("Entrance_GetOverride",0xbb);assert(jit.Regs()[0]==0);
        env.stop_at=0x447384;env.Run(0x447380);env.stop_at=Env::Stop;
        // Execute the native size-cache branch, with only the filesystem size
        // query stubbed. It must query the replacement, not reuse 7276 bytes.
        bool queried_archive=false;
        env.mocks[0x324fd0]=[&] {
            assert(jit.Regs()[0]==melody);
            assert(R::ReadBytes(env,melody,64).startsWith("rom:/actor/zelda_ootr_assets.zar"));
            queried_archive=true;jit.Regs()[0]=255060;
        };
        env.setup=[&] {jit.Regs()[7]=melody;};
        env.stop_at=0x2e4f08;jit.ClearCache();env.Run(0x2e4ee8);env.stop_at=Env::Stop;
        assert(queried_archive && jit.Regs()[0]==255060);
        env.mocks.erase(0x324fd0);jit.ClearCache();
        env.Run("Entrance_SetSavewarpEntrance");
        if(!rules.shuffleOverworldSpawns)assert(env.Get<U32>(0x587958)==(adult?0x282u:0xbbu));
        else assert(env.Get<U32>(0x587958)<0x614);
        // Play_Init must not initialize again and reset per-file runtime state.
        const auto entrance_table=R::Data().Find("entranceOverrideTable").address;assert(entrance_table);
        const auto original_house=env.Get<std::uint16_t>(entrance_table+0xbb*2);
        env.Put<std::uint16_t>(entrance_table+0xbb*2,0x123);
        env.Run("before_Play_Init",0xd00000);
        env.Run("Entrance_GetOverride",0xbb);assert(jit.Regs()[0]==0x123);
        env.Put<std::uint16_t>(entrance_table+0xbb*2,original_house);
        env.Run("Scene_Init");
        assert(R::CodeMatches(env));
        CheckExpandedRuntime(env,jit,*seed);
        CheckPlayerColor(env,jit);
        CheckRewards(env);
        const U32 stored_fw=R::Data().Find("gExtSaveData").address+0x378;
        for(U32 word=0;word<10;++word) {
            env.Write(0x587958+0xe7c+word*4,0x12340000+word);
            env.Write(stored_fw+word*4,0x56780000+word);
        }
        bool fw_swapped=false;
        // Both directions of the native pedestal transition, including the
        // alternate scene setup used by a fresh adult-start file.
        for(U32 age:{0u,1u})for(U32 layer:{0u,4u,6u}) {
            constexpr U32 play=0xd00000,save=0x587958;
            env.Write(0x587958+4,age);
            env.Write(R::Data().Find("gGlobalContext").address,play);
            env.Put<std::uint16_t>(play+0x22b8,0);
            env.Run("TimeTravelAdvanceCutsceneTimer");
            if(seed->config.options[R::DoorOfTime]==0)
                assert(env.Get<std::uint16_t>(play+0x22b8)==(age?340:310));
            env.Write(save+0x14e8,layer);
            const auto story_flags=env.Get<std::uint16_t>(save+0xef4);
            assert(story_flags&0x20);
            env.Put<std::uint8_t>(play+0x5c00,age);
            env.setup=[&] {
                auto& r=jit.Regs();r[4]=save+0x1000;r[5]=play;r[8]=0;r[9]=1;
            };
            env.stop_at=0x3716f0;env.Run(0x491864);env.stop_at=Env::Stop;
            if(jit.Regs()[1]!=0x324)std::cerr<<"Time travel selected story entrance "<<std::hex<<jit.Regs()[1]<<" instead of Temple of Time\n";
            assert(jit.Regs()[1]==0x324);
            assert(env.Get<std::uint8_t>(play+0x5c00)==1-age);
            assert(env.Get<std::uint16_t>(save+0xef4)==story_flags);
            fw_swapped=!fw_swapped;
            for(U32 word=0;word<10;++word) {
                assert(env.Get<U32>(save+0xe7c+word*4)==(fw_swapped?0x56780000u:0x12340000u)+word);
                assert(env.Get<U32>(stored_fw+word*4)==(fw_swapped?0x12340000u:0x56780000u)+word);
            }
        }
        assert(!R::runtime.backups.empty());
        // The generated ARM item table must resolve every key exactly, including
        // the first/last entries of the payload's binary search.
        const auto overrides=QByteArray::fromBase64(seed->json["fields"].toObject()["rItemOverrides"].toString().toLatin1());
        for(int i=0;i<overrides.size();i+=8) {
            U32 key,value;std::memcpy(&key,overrides.constData()+i,4);std::memcpy(&value,overrides.constData()+i+4,4);
            // ARM returns an eight-byte struct through the hidden r0 pointer.
            env.Run("ItemOverride_LookupByKey",0xc00000,key);
            assert(env.Get<U32>(0xc00000)==key && env.Get<U32>(0xc00004)==value);
            if(((key>>16)&255)==3) {
                // Resolve the actual skull-token actor's scene/flag key too.
                env.Put<std::uint16_t>(0xc01000,0x19c);
                env.Put<std::uint16_t>(0xc01000+0x1c,((key>>8)&255)*256+(key>>24));
                env.Run("ItemOverride_Lookup",0xc00000,0xc01000);
                assert(env.Get<U32>(0xc00000)==key && env.Get<U32>(0xc00004)==value);
            }
        }
        const auto prices=QByteArray::fromBase64(seed->json["fields"].toObject()["rShopsanityPrices"].toString().toLatin1());
        assert(prices.size()==128);
        // Resolve all 32 price slots through the game's actual shop mapping.
        constexpr U32 shop=0xc02000,row=0xc03000,context=0xd00000;
        constexpr std::array shop_indices{4,0,7,6,1,3,2,5};
        for(unsigned number=0;number<8;++number)for(unsigned position=0;position<4;++position) {
            env.Put<std::uint16_t>(context+0x104,number==7?44:44+number);
            env.Write(0x587958,number==7?0xb7:0xbb);
            env.Write(shop+0x200,position);
            env.Run("ShopsanityItem_GetPrice",shop);
            std::int32_t price;std::memcpy(&price,prices.constData()+(shop_indices[number]*4+position)*4,4);
            assert(jit.Regs()[0]==U32(price));
        }
        // Actual affordability and payment use the same per-save multiplier.
        env.Write(shop+0x208,row);env.Put<std::uint8_t>(row+1,2); // Bombs: repeatable.
        env.Write(0x587958+0xb8,env.Get<U32>(0x587958+0xb8)|8); // Bomb bag.
        for(unsigned factor:{1u,2u,3u})for(int price:{10,95,200,500,995}) {
            OOT3D::Difficulty::active_price_multiplier=factor;
            const int cost=OOT3D::Prices::Cost(price,factor);
            env.Put<std::int16_t>(shop+0x1dc,price);
            env.Put<std::int16_t>(0x587958+0x48,cost-1);
            env.Run("ShopsanityItem_CanBuy",context,shop);assert(jit.Regs()[0]==4);
            env.Put<std::int16_t>(0x587958+0x48,cost);
            env.Run("ShopsanityItem_CanBuy",context,shop);assert(jit.Regs()[0]==0);
            env.Put<std::int16_t>(0x587958+0x155c,0);
            env.Run("ShopsanityItem_BuyEventFunc",context,shop);
            assert(env.Get<std::int16_t>(0x587958+0x155c)==-cost);
        }
        R::UseDifficulty(seed->json["difficulty"].toObject());
        assert(raw.mid(0x30,6)=="ZELDAZ");
        assert((raw[0xe]!=0)==seed->mq);
        const auto ext=R::ReadBytes(env,R::Data().Find("gExtSaveData").address,R::Data().Find("gExtSaveData").size);
        R::WriteBytes(env,0xc10000,raw);
        QByteArray filename;
        for(char c:std::string("data:/save00.bin")) {filename+=c;filename+=char(0);}
        filename+=QByteArray(2,0);R::WriteBytes(env,0xc20000,filename);
        env.Run(0x2e613c,0xc20000,0xc10000,raw.size());
        assert(jit.Regs()[0]==0xb80000);
        bool invalid=false;auto loaded=R::LoadSlot(0,raw,invalid);
        assert(loaded && !invalid && loaded->extended_save==ext && loaded->config==seed->config);
        R::WriteBytes(env,0x55bec0,raw);env.Write(0x55bea8,1);
        // Automatic new-file selection skips CanOpen: seed and room ID must
        // already be available for the details and their shared backdrop.
        const auto previous_state=env.Get<U32>(0x504fb0),previous_selection=env.Get<U32>(0x504fb4);
        const bool previous_name_seen=R::new_name_seen;
        env.Write(0x504fb0,7);env.Write(0x504fb4,0);
        R::runtime.exported[0].clear();R::FileStatus::Publish();
        R::runtime.Frame(env);
        const auto short_id=loaded->json["shared_game_id"].toString().left(8).toUpper().toStdString();
        assert(R::FileStatus::Seed(0)==seed->config.seed && R::FileStatus::ShortRoomId(0)==short_id);
        R::runtime.Frame(env);
        assert(R::FileStatus::Seed(0)==seed->config.seed && R::FileStatus::ShortRoomId(0)==short_id);
        auto plain=raw;plain[R::SaveMarkerOffset]=0;
        R::WriteBytes(env,0x55bec0,plain);R::runtime.Frame(env);
        assert(R::FileStatus::Seed(0).empty() && R::FileStatus::ShortRoomId(0).empty());
        R::WriteBytes(env,0x55bec0,raw);R::runtime.Frame(env);
        assert(R::FileStatus::Seed(0)==seed->config.seed && R::FileStatus::ShortRoomId(0)==short_id);
        env.Write(0x504fb0,previous_state);env.Write(0x504fb4,previous_selection);
        R::new_name_seen=previous_name_seen;
        assert(R::runtime.CanOpen(env,0) && R::FileStatus::Seed(0)==seed->config.seed);
        auto other_draft=R::GetConfig();other_draft.seed="different-new-file-seed";
        R::SetConfig(other_draft);assert(R::FileStatus::Seed(0)==seed->config.seed);
        env.Run(0x2eda00,0,1);
        auto copied=R::LoadSlot(1,raw,invalid);assert(copied && !invalid && copied->config==seed->config);
        R::simulate_file2_runtime_mismatch=true;
        assert(!R::LoadSlot(1,raw,invalid) && invalid);
        assert(R::Status()=="This randomized file needs the matching runtime version and seed data.");
        R::WriteBytes(env,0x55bec0+0x15c4,raw);
        R::SetStatus("");
        assert(R::FileFlow{}.OpenForManagement(env,1) && R::Status().empty());
        assert(R::FileFlow{}.CheckStart(env,1)==R::FileStatus::Error::Incompatible && R::Status().empty());
        assert(R::LoadSlot(0,raw,invalid) && !invalid);
        auto diagnostic_ordinary=raw;diagnostic_ordinary[R::SaveMarkerOffset]=0;
        assert(!R::LoadSlot(1,diagnostic_ordinary,invalid) && !invalid);
        R::simulate_file2_runtime_mismatch=false;
        const auto restored_package=R::LoadSlot(1,raw,invalid);
        assert(restored_package && !invalid && restored_package->json==copied->json && restored_package->extended_save==copied->extended_save);
        assert(!loaded->json["shared_game_id"].toString().isEmpty());
        assert(copied->json["shared_game_id"]==loaded->json["shared_game_id"]);
        const auto reload=R::LoadSlot(0,raw,invalid);
        assert(reload && !invalid && reload->json["shared_game_id"]==loaded->json["shared_game_id"]);
        auto legacy=seed->json;legacy.remove("shared_game_id");
        const auto a=R::DecodePackage(legacy),b=R::DecodePackage(legacy);
        assert(a && b && a->json["shared_game_id"]!=b->json["shared_game_id"]);
        auto changed=raw;changed[0]^=1;R::LoadSlot(0,changed,invalid);assert(invalid);
        assert(!R::LoadSlot(2,raw,invalid) && invalid); // Missing seed data cannot silently become vanilla.
        R::WriteBytes(env,0x55bec0+2*0x15c4,raw);
        R::SetStatus("");
        assert(R::FileFlow{}.OpenForManagement(env,2) && R::Status().empty());
        assert(R::FileFlow{}.CheckStart(env,2)==R::FileStatus::Error::Missing && R::Status().empty());
        auto ordinary=raw;ordinary[R::SaveMarkerOffset]=0;
        R::WriteBytes(env,0x55bec0+2*0x15c4,ordinary);
        assert(R::runtime.CanOpen(env,2) && R::FileStatus::Seed(0).empty() && R::FileStatus::Seed(2).empty());
        R::WriteBytes(env,0x55bec0+2*0x15c4,raw);
        assert(!R::runtime.CanOpen(env,2) && R::FileStatus::Seed(2).empty());
        R::BeginOrdinaryFile(2);assert(R::RecordSave(2,ordinary,nullptr,{}));assert(!R::LoadSlot(2,ordinary,invalid) && !invalid);
        CheckMultiplayerRuntime(env,jit);
        {
            namespace A=R::Activity;
            const std::string game="ba7ab566-c1e4-4933-9e6d-efdc6251a7bb";
            A::SetGame(game);A::Connection(true);jit.ClearCache();
            // Execute the actual item-effect function with its displaced MOV.
            env.Run("ItemTable_CallEffect",R::Data().Find("rItemTable").address+24);
            auto events=A::TakeOutgoing();assert(events.size()==1 && events[0].kind==0 && events[0].item==1);
            A::Receive(events[0],"Guest");assert(A::Visible().back().text=="Guest got some Bombs (5)!");
            assert(A::TakeOutgoing().empty()); // Received messages never rebroadcast as pickups.
            A::Receive({"different-game",0,1},"Other");assert(A::Visible().size()==1);
            const auto old_owner=env.Read(OOT3D::DeathMenu::ControllerPointer).value();
            const auto old_health=env.Read(0x58799c).value();
            env.Write(OOT3D::DeathMenu::ControllerPointer,0xc60000);
            env.Write(0xc60008,0x10003);env.Write(0x58799c,0);
            OOT3D::DeathMenu::Hook death;
            death.ControlUpdate(env,jit.Regs());death.ControlUpdate(env,jit.Regs());
            events=A::TakeOutgoing();assert(events.size()==1 && events[0].kind==1);
            env.Write(0x58799c,0x30);death.ControlUpdate(env,jit.Regs());
            assert(A::TakeOutgoing().empty()); // Living Boss Rush result isn't a death.
            env.Write(0x58799c,0);death.ControlUpdate(env,jit.Regs());assert(A::TakeOutgoing().size()==1);
            env.Write(OOT3D::DeathMenu::ControllerPointer,old_owner);env.Write(0x58799c,old_health);
            A::Connection(false);env.Run("ItemTable_CallEffect",R::Data().Find("rItemTable").address+24);
            A::Receive({game,0,1},"Guest"); // An in-flight packet cannot repopulate a disconnected feed.
            assert(A::TakeOutgoing().empty() && A::Visible().empty());
            A::Notice("Connection to room lost.");
            for(unsigned i=0;i<10;++i)A::Connection(false);
            A::Notice("Connection to room lost.");
            A::Receive({game,0,1},"Late packet");
            assert(A::Visible().size()==1 && A::Visible()[0].text=="Connection to room lost.");
            assert(A::TakeOutgoing().empty());
            {std::scoped_lock lock(A::mutex);A::lines.front().born-=std::chrono::seconds(7);}
            assert(A::Visible().empty());
            A::Notice("Connection to room lost.");A::SetGame({});assert(A::Visible().empty());
            std::cout<<"Activity: real reward hook, death edge, no replay, wrong-game filtering and disconnect cleanup passed\n";
        }
        R::runtime.Deactivate(env);jit.ClearCache();assert(!R::active && R::CaptureDifficulty()==difficulty);
        // Live save legitimately changed; immutable native tables must return.
        const auto restored=R::ReadBytes(env,0x4cc000,0x55a000-0x4cc000);
        unsigned differences=0;
        for(int at=0;at<restored.size();at+=4) if(at+0x4cc000!=0x50bb58 && restored.mid(at,4)!=before.mid(at,4)) {
            if(differences++<30)std::cerr<<"not restored "<<std::hex<<0x4cc000+at<<" before "<<before.mid(at,4).toHex().toStdString()<<" after "<<restored.mid(at,4).toHex().toStdString()<<'\n';
        }
        assert(differences==0);
    }
    // The reverse case matters too: a previously uncached ordinary archive
    // must not inherit the replacement size learned later during gameplay.
    env.Write(melody+0x40,0);
    assert(R::runtime.Activate(env,0,*package));jit.ClearCache();
    env.stop_at=0x447384;env.Run(0x447380);env.stop_at=Env::Stop;
    assert(env.Read(melody+0x40)==0);
    env.Write(melody+0x40,255060);
    R::runtime.Deactivate(env);jit.ClearCache();
    assert(env.Read(melody+0x40)==0);
    assert(R::ReadBytes(env,melody,64).startsWith("rom:/actor/zelda_gi_melody.zar"));
    std::cout<<"Native archive size cache refreshed on replacement and restored for cached/uncached files\n";
    std::cout<<"Randomizer ARM time travel in both directions, Farore's Wind swap, spawn/load order, shop prices/payment, Skulltula lookup, per-save persistence, copies and restoration passed\n";
    CheckRoomReturnText(argv[1]);
    CheckRoomImport(argv[1],*package);
    CheckRoomImport(argv[1],*mq);
}
