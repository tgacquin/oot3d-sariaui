// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonDocument>

// Execute the actual native continuation used throughout Join Room cancellation.
// Capture the text renderer's position calls before the shared stone/text slide.
inline void CheckRoomReturnText(const char* code_path) {
    namespace M=OOT3D::MasterQuest;
    Env e;
    std::ifstream code(code_path,std::ios::binary);
    code.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),0x4c7000);
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;
    Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    assert(M::CodeMatches(e));
    e.Write(M::Owner+0x10,3);
    std::map<U32,std::array<float,2>> positions;
    e.mocks[0x2e946c]=[&] {
        positions[cpu.Regs()[0]]={std::bit_cast<float>(cpu.ExtRegs()[0]),
                                  std::bit_cast<float>(cpu.ExtRegs()[1])};
    };
    e.mocks[0x2edbe8]=[]{}; // Unrelated card visibility/presentation update.
    e.stop_at=0x4336f8;
    for(unsigned occupied=0;occupied<8;++occupied)for(unsigned selected=0;selected<3;++selected) {
        positions.clear();
        for(unsigned i=0;i<3;++i) {
            e.Write(0x55b53c+4*i,(occupied>>i)&1);
            e.Write(0x55b5b4+4*i,i==selected);
            e.Write(0x55b548+4*i,0xb00000+0x100*i);
            e.Write(0x55b554+4*i,0xb00300+0x100*i);
            for(U32 base:{0x55b5e0u,0x55b5f8u})e.Put<float>(base+4*i,16.f);
            for(U32 base:{0x55b5ecu,0x55b604u})e.Put<float>(base+4*i,48.f+52*i);
            e.Write(0x55b610+4*i,0);e.Write(0x55b61c+4*i,0);
        }
        e.Run(M::FilePresentation);
        for(unsigned i=0;i<3;++i) {
            const bool exists=(occupied>>i)&1;
            const float raised=i==selected?2.f:0.f;
            const auto name=positions.at(0xb00000+0x100*i);
            assert(name[0]==(exists?44.f:32.f));
            assert(name[1]==(exists?54.f:61.f)+52*i+raised);
            if(exists) {
                const auto date=positions.at(0xb00300+0x100*i);
                assert(date[0]==184.f && date[1]==72.f+52*i+raised);
            } else assert(!positions.contains(0xb00300+0x100*i));
            assert(e.Get<float>(0x55b5f8+4*i)==16.f);
            assert(e.Get<float>(0x55b604+4*i)==48.f+52*i);
        }
    }
    std::cout<<"Join return: native name/date positions follow all three stones before rendering\n";
    // The guest gate enters the same native Start/Back blocks as the buttons,
    // preserving the existing load and file-list return animations.
    e.mocks.clear();e.mocks[0x37547c]=[]{};e.stop_at=0x430a54;cpu.ClearCache();
    M::Hook start_gate;
    e.menu_svc=[&](unsigned site){start_gate.Handle(e,cpu.Regs(),site,1000000,R::FileFlow{});};
    e.Write(M::Owner+0x10,8);
    e.setup=[&] {cpu.Regs()[6]=M::Owner;};e.Run(M::GuestStart);
    assert(e.Read(M::Owner+0x10)==10 && e.Read(M::Owner+0x28)==0);
    e.Write(M::Owner+0x10,8);
    e.setup=[&] {cpu.Regs()[6]=M::Owner;cpu.Regs()[7]=1;};e.Run(M::GuestBack);
    assert(e.Read(M::Owner+0x10)==9 && e.Read(M::Owner+0x54)==0);
    std::cout<<"Guest lobby: native Start and completed-disconnect Back continuations passed\n";
    // The actual Start branch must return via the original audio continuation
    // before its native fade/state change. No model cleanup or save load runs.
    e.Write(M::Owner+0x10,8);e.Write(M::Owner+0x14,1);e.Write(M::Owner+0x18,0);
    e.Put<std::uint8_t>(M::Saves+M::Stride+R::SaveMarkerOffset,R::SaveMarker);
    const auto rejected_save=R::ReadBytes(e,M::Saves+M::Stride,M::SaveSize);
    U32 rejection_sound=0;
    e.mocks[0x37547c]=[&]{rejection_sound=cpu.Regs()[0];};
    e.stop_at=0x4336f8;cpu.ClearCache();
    R::simulate_file2_runtime_mismatch=true;
    e.setup=[&]{cpu.Regs()[6]=M::Owner;};e.Run(M::GuestStart);
    assert(e.Read(M::Owner+0x10)==8 && rejection_sound==M::RejectionSound);
    assert(cpu.Regs()[13]==Env::SP && M::start_error==M::FileError::Incompatible);
    assert(R::ReadBytes(e,M::Saves+M::Stride,M::SaveSize)==rejected_save);
    R::simulate_file2_runtime_mismatch=false;start_gate.Reset();
    e.setup=[&]{cpu.Regs()[6]=M::Owner;};e.Run(M::GuestStart);
    assert(M::start_error==M::FileError::Missing && e.Read(M::Owner+0x10)==8);
    e.Put<std::uint8_t>(M::Saves+M::Stride+R::SaveMarkerOffset,0);
    start_gate.Reset();e.menu_svc={};
    std::cout<<"Native Start rejection: incompatible/missing data, original error cue, intact stack/save and no menu departure passed\n";
    namespace J=OOT3D::RoomJoin;
    e.mocks.clear();
    for(U32 pc:{0x2f6944u,0x3525d4u,0x2f43d8u})e.mocks[pc]=[]{};
    for(unsigned selected=0;selected<3;++selected) {
        for(unsigned i=0;i<3;++i) {
            e.Write(M::Exists+4*i,1);e.Write(e.Read(0x42fdfc).value()+4*i,1);
        }
        const auto base=M::Saves+selected*M::Stride;
        const std::u16string saved_name=selected==0?u"Link":selected==1?u"Épona":u"ABCDEFGH";
        e.Put<std::uint8_t>(base+0x2c,saved_name.size());
        for(unsigned i=0;i<saved_name.size();++i)e.Put<std::uint16_t>(base+0x1c+2*i,saved_name[i]);
        const auto native_saves=R::ReadBytes(e,M::Saves,3*M::Stride);
        e.Write(M::Owner+0x14,selected);e.Write(M::Owner+0x10,8);e.Write(J::NameOwner+4,0);
        e.Write(0x50af18,0);e.Write(0x50af14,0);
        M::Hook flow;flow.pending_profile=1;flow.profile_slot=selected;
        J::profiles[selected].store(std::make_shared<const J::Profile>(J::Profile{"fixture",false}));
        assert(flow.Dispatch(e,cpu.Regs(),1000000));
        assert(J::phase==J::Phase::ProfileLeaving && J::name==saved_name && !flow.awaiting_name);
        assert(!J::request.load() && e.Read(J::NameOwner+4)==0);
        assert(flow.Dispatch(e,cpu.Regs(),1075000) && J::opacity==.5f);
        assert(flow.Dispatch(e,cpu.Regs(),1150000));
        const auto regs=cpu.Regs();assert(regs[15]==M::ProfileBack && J::HideFiles());
        // Execute the real action-menu cleanup continuation, without the Back
        // sound or any keyboard initialization, then finish its native slide.
        e.stop_at=0x430a54;cpu.ClearCache();
        e.setup=[&] {cpu.Regs()=regs;};e.Run(regs[15]);
        assert(e.Read(J::NameOwner+4)==0 && e.Read(M::Owner+0x10)==9);
        e.Write(M::Owner+0x10,3);
        assert(flow.Dispatch(e,cpu.Regs(),1500000));
        const auto request=J::request.load();
        assert(request && request->name==saved_name && request->slot==selected && request->game_id=="fixture");
        assert(request->intent==J::Intent::Resume && J::CurrentStatus()==J::Status::Matching);
        assert(R::ReadBytes(e,M::Saves,3*M::Stride)==native_saves);
        for(unsigned i=0;i<3;++i)assert(e.Read(M::Exists+4*i)==1);
        // B follows the existing waiting-screen cancellation back to files.
        e.Write(0x50af18,2);assert(flow.Dispatch(e,cpu.Regs(),1510000));
        assert(J::phase==J::Phase::Leaving && request->cancelled);
        e.Write(0x50af18,0);assert(flow.Dispatch(e,cpu.Regs(),1700000));
        assert(!J::Active() && M::returning && e.Read(J::NameOwner+4)==0);
        assert(R::ReadBytes(e,M::Saves,3*M::Stride)==native_saves);
        J::profiles[selected].store(std::shared_ptr<const J::Profile>{});
    }
    std::cout<<"Existing-file Join: saved UTF-16 name, native action cleanup, direct matching search, B cancellation and unchanged saves passed\n";

}

// Real ARM initialization/checksum/queue/poll/reap; filesystem completion and
// OS locks are supplied by the isolated harness, never by a running profile.
inline void CheckRoomImport(const char* code_path, R::Package package) {
    namespace J=OOT3D::RoomJoin;
    namespace M=OOT3D::MasterQuest;
    QTemporaryDir storage;assert(storage.isValid());
    QSettings local_settings(storage.filePath("settings.ini"),QSettings::IniFormat);
    R::Initialize(local_settings,storage.path().toStdString());
    // Reuse any seed fixture, explicitly enabling its multiplayer setting.
    package.config.advanced["MP_Enabled"]=1;
    package.json["config"]=R::ToJson(package.config);
    auto fields=package.json["fields"].toObject();
    auto context=QByteArray::fromBase64(fields["gSettingsContext"].toString().toLatin1());
    context[offsetof(SettingsContext,mp_Enabled)]=1;
    fields["gSettingsContext"]=QString::fromLatin1(context.toBase64());package.json["fields"]=fields;
    auto decoded=R::DecodePackage(package.json);assert(decoded);package=std::move(*decoded);
    Env e;
    std::ifstream code(code_path,std::ios::binary);
    code.read(reinterpret_cast<char*>(e.bytes.data()+0x100000),0x4c7000);
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&e;
    Dynarmic::A32::Jit cpu(cfg);e.cpu=&cpu;
    assert(R::runtime.Activate(e,0,package));cpu.ClearCache();M::SetMode(e,package.mq);
    R::new_slot=0;R::new_randomized=true;R::new_mq=package.mq;
    e.Run(0x4499b0);
    constexpr U32 live=0x587958,bank=M::Saves+2*M::Stride;
    e.Put<std::uint16_t>(live+0x14d8,0);
    e.Put<std::uint8_t>(live+0x2e,2);
    e.Run(0x2faf90,live,M::SaveSize,0);
    e.Put<std::uint16_t>(live+0x14d8,cpu.Regs()[0]);
    const auto raw=R::ReadBytes(e,live,M::SaveSize);
    assert(R::ValidRoomSave(raw));
    R::runtime.Deactivate(e);cpu.ClearCache();
    auto damaged=raw;damaged[0]^=1;assert(!R::ValidRoomSave(damaged));
    package.extended_save=QByteArray(R::Data().Find("gExtSaveData").size,'\x35');
    package.json["extended_save"]=QString::fromLatin1(package.extended_save.toBase64());
    R::ClearRoomFiles();R::PublishRoomFile(0,raw,&package);R::SelectRoomFile(0);
    const auto wire=R::HostedRoomFile(true);assert(wire && !wire->empty());
    R::SelectRoomFile(1);assert(R::HostedRoomFile(false)==wire);
    auto different=package;different.json["seed"]="other-identity";
    R::PublishRoomFile(0,raw,&different);assert(!R::HostedRoomFile(false));
    R::PublishRoomFile(0,raw,&package);assert(R::HostedRoomFile(false));
    // A returning guest keeps its own native and extended progress. Only the
    // persistent ID and immutable gameplay package determine compatibility.
    const auto id=package.json["shared_game_id"].toString().toStdString();assert(!id.empty());
    auto resumed=std::make_shared<const J::Request>(0,package.mq,u"Guest",J::Intent::Resume,id);
    assert(R::ReceiveRoomFile(resumed,*wire) && resumed->saved && resumed->status==J::Status::Connected);
    assert(!resumed->file.load()); // No native save import queued.
    auto wrong=std::make_shared<const J::Request>(0,package.mq,u"Guest",J::Intent::Resume,"wrong");
    assert(!R::ReceiveRoomFile(wrong,*wire));
    auto old_progress=package;old_progress.extended_save.fill(char(9));
    R::PublishRoomFile(0,raw,&old_progress);
    assert(R::ReceiveRoomFile(resumed,*wire));
    R::PublishRoomFile(0,raw,&different);assert(!R::ReceiveRoomFile(resumed,*wire));
    R::PublishRoomFile(0,raw,&package);
    const auto encoded=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(wire->data()),wire->size())).object();
    const auto receive=[&](QJsonObject obj) {
        const auto bytes=QJsonDocument(obj).toJson(QJsonDocument::Compact);
        auto request=std::make_shared<const J::Request>(2,!package.mq,u"Guest");
        return R::ReceiveRoomFile(request,{bytes.begin(),bytes.end()});
    };
    auto bad=encoded;bad["save"]=QString::fromLatin1(damaged.toBase64());assert(!receive(bad));
    bad=encoded;auto bad_package=bad["package"].toObject();bad_package["runtime"]="unknown";
    bad["package"]=bad_package;assert(!receive(bad));
    bad=encoded;bad_package=bad["package"].toObject();bad_package["mq"]=!package.mq;
    bad["package"]=bad_package;assert(!receive(bad));
    bad=encoded;bad_package=bad["package"].toObject();bad_package["extended_save"]="AA==";
    bad["package"]=bad_package;assert(!receive(bad));
    bad=encoded;bad_package=bad["package"].toObject();auto config=bad_package["config"].toObject();
    auto advanced=config["advanced"].toObject();advanced["MP_Enabled"]=0;config["advanced"]=advanced;
    bad_package["config"]=config;bad["package"]=bad_package;assert(!receive(bad));
    auto cancelled=std::make_shared<const J::Request>(2,false,u"Guest");cancelled->cancelled=true;
    assert(!R::ReceiveRoomFile(cancelled,*wire));
    const auto before=e.bytes;
    const auto begin=[&] {
        e.bytes=before;R::runtime.CheckEpoch();R::runtime.room_import={};R::new_slot=3;
        e.Write(M::Owner+0x10,3);e.Write(0x50bb58,0);
        for(unsigned i=0;i<3;++i)e.Write(M::Exists+4*i,0);
        J::Reset();J::Begin(2,0);J::name=u"Guest";J::Wait(1,false);
        auto request=J::request.load();assert(R::ReceiveRoomFile(request,*wire));
        const auto file=request->file.load();
        assert(file && file->native_save==raw && file->package.config==package.config &&
               file->package.extended_save==package.extended_save);
        e.ResetGuestRegisters();
        for(unsigned i=0;i<16;++i)cpu.Regs()[i]=0x12340000+i;
        cpu.Regs()[13]=Env::SP;cpu.Regs()[15]=R::RoomImport::Dispatch+4;
        return request;
    };
    auto request=begin();e.Write(M::Exists+8,1);auto memory=e.bytes;
    assert(!R::runtime.room_import.Frame(e,cpu.Regs()));
    assert(request->status==J::Status::SlotOccupied && e.bytes==memory);
    request=begin();request->cancelled=true;memory=e.bytes;
    assert(!R::runtime.room_import.Frame(e,cpu.Regs()) && e.bytes==memory);
    request=begin();request->connection_lost=true;
    const auto empty_after_disconnect=R::ReadBytes(e,bank,M::SaveSize);
    assert(!R::runtime.room_import.Frame(e,cpu.Regs()));
    assert(request->status==J::Status::Failed && R::ReadBytes(e,bank,M::SaveSize)==empty_after_disconnect);

    // Execute the injected native call with its original stack and return PC.
    const auto native_call=[&] {
        const auto regs=cpu.Regs();e.stop_at=R::RoomImport::Dispatch;
        e.setup=[&]{cpu.Regs()=regs;};e.Run(regs[15]);e.stop_at=Env::Stop;
    };
    for(unsigned outcome=0;outcome<3;++outcome) {
        const bool succeeds=outcome!=0;
        request=begin();const auto regs=cpu.Regs();
        const auto untouched=R::ReadBytes(e,M::Saves,2*M::Stride);
        const auto empty=R::ReadBytes(e,bank,M::SaveSize);
        assert(R::runtime.room_import.Frame(e,cpu.Regs()));
        assert(!J::CanCancel() && request->status==J::Status::Saving);
        assert(cpu.Regs()[15]==R::RoomImport::Queue && cpu.Regs()[1]==bank);
        native_call();assert(cpu.Regs()[0]==0xb80000);
        const U32 job=cpu.Regs()[0];assert(e.Read(job+0x10)==bank);
        assert(!R::runtime.room_import.Frame(e,cpu.Regs()) && cpu.Regs()==regs);
        bool invalid=false;auto loaded=R::LoadSlot(2,raw,invalid);
        assert(loaded && !invalid && loaded->extended_save==package.extended_save && loaded->config==package.config);
        // Supply initialized native locks and mock only the OS lock operations.
        e.Write(e.Read(0x31bb68).value(),1);e.Write(e.Read(0x31bb6c).value(),1);
        for(U32 pc:{0x31fe84u,0x31bebcu,0x31bd30u,0x31bb84u})e.mocks[pc]=[]{};
        bool freed=false;e.mocks[0x34fc6c]=[&]{assert(cpu.Regs()[0]==job);freed=true;};cpu.ClearCache();
        for(unsigned i=0;i<3;++i) {
            assert(R::runtime.room_import.Frame(e,cpu.Regs()));native_call();
            assert(!R::runtime.room_import.Frame(e,cpu.Regs()) && cpu.Regs()==regs);
            assert(R::ReadBytes(e,bank,M::SaveSize)==raw && !freed);
        }
        e.Put<std::uint8_t>(job+8,1);e.Write(job,succeeds?0:0x80000000);
        if(outcome==2)request->connection_lost=true;
        assert(R::runtime.room_import.Frame(e,cpu.Regs()));native_call();
        assert(R::runtime.room_import.Frame(e,cpu.Regs()));native_call();assert(freed);
        assert(!R::runtime.room_import.Frame(e,cpu.Regs()) && cpu.Regs()==regs);
        assert(request->status==(outcome==2?J::Status::SavedDisconnected:
            succeeds?J::Status::Connected:J::Status::SaveFailed));
        assert(J::CanCancel()==!succeeds && request->saved.load()==succeeds);
        assert(e.Read(M::Exists+8)==(succeeds?1u:0u));
        assert(R::ReadBytes(e,bank,M::SaveSize)==(succeeds?raw:empty));
        assert(R::ReadBytes(e,live,M::SaveSize)==raw);
        assert(R::ReadBytes(e,M::Saves,2*M::Stride)==untouched);
        e.mocks.clear();cpu.ClearCache();
    }
    // A sidecar write failure must prevent even scheduling the native write.
    request=begin();const auto empty=R::ReadBytes(e,bank,M::SaveSize);
    QTemporaryDir blocked;QFile blocker(blocked.filePath("not-a-directory"));
    assert(blocker.open(QIODevice::WriteOnly));blocker.close();
    QSettings settings(blocked.filePath("settings.ini"),QSettings::IniFormat);
    R::Initialize(settings,blocker.fileName().toStdString());
    assert(R::runtime.room_import.Frame(e,cpu.Regs()));native_call();assert(cpu.Regs()[0]==0);
    assert(!R::runtime.room_import.Frame(e,cpu.Regs()));
    assert(request->status==J::Status::SaveFailed && J::CanCancel());
    assert(R::ReadBytes(e,bank,M::SaveSize)==empty);
    J::Reset();R::ClearRoomFiles();
    // Completion substitutes the ordinary Select sound exactly once.
    J::success_chime=true;R::creation_chime=false;
    auto& registers=cpu.Regs();registers[0]=0x010004c0;
    R::runtime.Handle(e,registers,10);
    assert(registers[0]==0x01000487 && registers[15]==0x37547c && !J::success_chime);
    registers[0]=0x010004c0;R::runtime.Handle(e,registers,10);
    assert(registers[0]==0x010004c0);
    std::cout<<"Room file ("<<(package.mq?"MQ":"Original")<<"): checksum/seed validation, host identity lock, cancellation, occupied-slot protection, ARM queue/poll/reap, pending buffer lifetime, disconnects, save failures and exact native/extended save roundtrip passed\n";
}
