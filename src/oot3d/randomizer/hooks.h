// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/randomizer/service.h"
#include "oot3d/randomizer/activity.h"
#include "oot3d/remote_names.h"
#include "oot3d/multiplayer_startup.h"
#include "oot3d/randomizer/room_import.h"
#include "common/logging/log.h"
#include "oot3d/randomizer/difficulty.h"
#include "oot3d/randomizer/resource_diagnostics.h"
#include "oot3d/prices.h"
#include "oot3d/title_hooks.h"
#include <unordered_map>

namespace OOT3D::Randomizer {
constexpr std::uint32_t FirstSVC = 0xfff400;
inline std::atomic<unsigned> process_epoch{0}, code_epoch{0};
inline std::atomic<unsigned> new_slot{3};
inline std::atomic<bool> new_mq{false}, new_randomized{false};
inline bool new_name_seen=false;
constexpr std::array<TitleScreen::Site, 5> Sites{{
    {0x449a04, 0xe5c01035, 0xe8bd8010}, // New native SaveContext, after defaults.
    {0x447380, 0xe0850004, 0xe7911100}, // File load's slot calculation.
    {0x2e613c, 0xe92d47f0, 0xe1a06003}, // Queue native file write, before copying its buffer.
    {0x2eda04, 0xe24ddd09, 0xe3a06000}, // Copy source/destination before native save I/O.
    {0x430068, 0xebfd1503, 0xe59f53f0}, // Select sound: random creation uses the correct-answer chime.
}};
// Observe the native async reader immediately before it reports a failure.
// Keep its error and control flow intact; this is diagnostic only.
constexpr TitleScreen::Site ResourceResultSite{0x42c924,0xe5940000,0xe2101102};
constexpr TitleScreen::Site ObjectArchiveSite{0x2e4fb0,0xeb00d85b,0xe5d80000};
inline std::atomic<bool> creation_chime{false};
inline void ResetProcess() {
    MultiplayerStartup::phase=MultiplayerStartup::Phase::Idle;
    RemoteNames::Reset();
    Activity::SetGame({});
    ClearRoomFiles();
    FileStatus::Publish();
    RestoreDifficulty(); creation_chime=false;
    active = false; process_epoch.fetch_add(1); code_epoch.fetch_add(1);
    new_slot = 3; new_name_seen=false; SetCurrent(nullptr); CancelPreparation();
}
inline const auto& CodePatches() {
    static const auto patches = [] {
        std::unordered_map<std::uint32_t, std::uint32_t> result;
        for (const auto& patch : Data().patches) if (patch.address < 0x4cc000) {
            for (qsizetype i = 0; i + 4 <= patch.bytes.size(); i += 4) {
                std::uint32_t word = 0;
                for (unsigned j = 0; j < 4; ++j) word |= std::uint32_t(std::uint8_t(patch.bytes[i+j])) << (8*j);
                result[patch.address + i] = word;
            }
        }
        return result;
    }();
    return patches;
}
inline const auto& InvalidationRanges() {
    static const auto ranges=[] {
        std::vector<std::pair<std::uint32_t,std::uint32_t>> spans;
        for(const auto& patch:Data().patches)if(patch.address<0x4cc000)
            spans.emplace_back(patch.address & ~3u,(patch.address+patch.bytes.size()+3)&~3u);
        spans.emplace_back(ResourceResultSite.pc,ResourceResultSite.pc+4);
        spans.emplace_back(ObjectArchiveSite.pc,ObjectArchiveSite.pc+4);
        spans.emplace_back(GuestBase,GuestEnd);
        std::sort(spans.begin(),spans.end());
        std::vector<std::pair<std::uint32_t,std::uint32_t>> merged;
        for(const auto& span:spans) {
            if(!merged.empty() && span.first<=merged.back().second)
                merged.back().second=std::max(merged.back().second,span.second);
            else merged.push_back(span);
        }
        return merged;
    }();
    return ranges;
}
inline std::uint32_t Instruction(std::uint32_t pc, std::uint32_t word) {
    for (unsigned i = 0; i < Sites.size(); ++i)
        if (pc == Sites[i].pc && word == Sites[i].instruction) return 0xef000000 | (FirstSVC + 6 + i);
    if (active.load()) {
        for(const auto& hook:Data().reward_hooks)
            if(pc==hook.address && word==hook.instruction)
                return 0xea000000|(((hook.target-pc-8)>>2)&0xffffff);
        static const auto puppet=Data().Find("EnLinkPuppet_Draw");
        if(pc==puppet.address+0x1d8 && word==0xe3a02000)return 0xef000000|(FirstSVC+15);
        static const auto effect=Data().Find("ItemTable_CallEffect");
        if(pc==effect.address && word==0xe1a03000)return 0xef000000|(FirstSVC+14);
        if(pc==ResourceResultSite.pc && word==ResourceResultSite.instruction)
            return 0xef000000 | (FirstSVC+12);
        if(pc==ObjectArchiveSite.pc && word==ObjectArchiveSite.instruction)
            return 0xef000000 | (FirstSVC+13);
        const auto found = CodePatches().find(pc);
        if (found != CodePatches().end()) return found->second;
    }
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for (const auto& site : Sites)
        if (m.Read(site.pc) != site.instruction || m.Read(site.pc + 4) != site.following) return false;
    return m.Read(0x449a0c) == 0x587958 && m.Read(0x447348) == 0x55bec0 &&
        m.Read(ObjectArchiveSite.pc)==ObjectArchiveSite.instruction &&
        m.Read(ObjectArchiveSite.pc+4)==ObjectArchiveSite.following;
}
template<class M> QByteArray ReadBytes(M& m, std::uint32_t address, unsigned length) {
    QByteArray result(length, 0);
    for (std::uint32_t at = address & ~3u; std::uint64_t(at) < std::uint64_t(address) + length; at += 4) {
        const auto word = m.Read(at);
        if (!word) return {};
        for (unsigned j = 0; j < 4; ++j)
            if (at+j >= address && std::uint64_t(at)+j < std::uint64_t(address)+length)
                result[at+j-address] = char(*word >> (8*j));
    }
    return result;
}
template<class M> void WriteBytes(M& m, std::uint32_t address, const QByteArray& bytes) {
    // Preserve neighbors for unaligned patches.
    for (std::uint32_t at = address & ~3u; at < address + bytes.size(); at += 4) {
        auto word = m.Read(at).value_or(0);
        for (unsigned i = 0; i < 4; ++i) if (at+i >= address && at+i < address+bytes.size()) {
            word = (word & ~(255u << (8*i))) | (std::uint32_t(std::uint8_t(bytes[at+i-address])) << (8*i));
        }
        m.Write(at, word);
    }
}
struct Hook {
    RoomImport room_import;
    RoomJoin::FileSession file_session;
    std::array<QByteArray,3> exported;
    unsigned epoch = 0, slot = 3;
    std::optional<Package> package;
    std::vector<Patch> backups;
    QByteArray before_init;
    std::string resource_failure;
    std::optional<Package> staging;
    unsigned staging_slot=3;
    qsizetype staging_offset=0;
    void CancelStaging() {staging.reset();staging_slot=3;staging_offset=0;}
    unsigned copy_destination = 3;
    std::optional<Package> copy_package;
    void CheckEpoch() {
        if (epoch == process_epoch.load()) return;
        epoch = process_epoch.load(); slot = 3; package.reset(); backups.clear(); before_init.clear();
        resource_failure.clear();
        copy_destination = 3; copy_package.reset(); CancelStaging();
        room_import={};file_session={};exported={};
    }
    template<class M> void Deactivate(M& m) {
        CheckEpoch();
        if (!package) return;
        Activity::SetGame({});
        MultiplayerStartup::phase=MultiplayerStartup::Phase::Idle;
        active = false; RestoreDifficulty();
        multiplayer_stage=-2;multiplayer_peers=0;multiplayer_error=0;multiplayer_command=0;
        for (auto it = backups.rbegin(); it != backups.rend(); ++it) WriteBytes(m, it->address, it->bytes);
        backups.clear(); package.reset(); slot = 3; SetCurrent(nullptr); code_epoch.fetch_add(1);
    }
    template<class M> bool Activate(M& m, unsigned selected, Package value, bool payload_ready=false) {
        Deactivate(m);
        resource_failure.clear();
        if (!CodeMatches(m)) {
            SetError(FileStatus::Error::Unsupported,"This game executable does not support the randomizer integration."); return false;
        }
        if (!m.Read(GuestEnd-4)) {
            SetError(FileStatus::Error::Memory,"Randomizer memory was not reserved. Restart the game with the updated app."); return false;
        }
        if(value.json.value("difficulty").toObject().value("prices").toBool() && !Difficulty::price_text_ready.load()) {
            SetError(FileStatus::Error::Prices,"Increased prices require the supported game message archive. Disable increased prices before creating this seed.");return false;
        }
        for (const auto& patch : Data().patches) if (patch.address >= 0x4cc000) {
            const auto previous = ReadBytes(m, patch.address, patch.bytes.size());
            if (previous.size() != patch.bytes.size()) {backups.clear();SetError(FileStatus::Error::Incompatible,"Cannot read the native randomizer patch destination.");return false;}
            backups.push_back({patch.address, previous, patch.name});
        }
        for(const auto& patch:value.native_patches) {
            const auto previous=ReadBytes(m,patch.address,patch.bytes.size());
            if(previous.size()!=patch.bytes.size()) {backups.clear();SetError(FileStatus::Error::Incompatible,"Cannot read the seed patch destination.");return false;}
            backups.push_back({patch.address,previous,patch.name});
        }
        // Scene selection runs after native scene-table copying, outside
        // Randomizer_Init. Preserve the complete affected static records too.
        for(const auto& [address,length] : std::array<std::pair<std::uint32_t,unsigned>,4>{{
            {0x543bb8,0x614*4}, {0x545484,14*0x8c}, {0x539dc4,101*4}, {0x4d47c8,16*4}}}) {
            const auto bytes=ReadBytes(m,address,length);
            if(bytes.size()!=length) {backups.clear();return false;}
            backups.push_back({address,bytes,"late static table"});
        }
        for (const auto& patch : Data().patches)
            if (patch.address >= 0x4cc000) WriteBytes(m, patch.address, patch.bytes);
        for(const auto& patch:value.native_patches)WriteBytes(m,patch.address,patch.bytes);
        if(!payload_ready)WriteBytes(m, GuestBase, value.payload);
        WriteBytes(m,Data().reward_base,Data().rewards);
        slot = selected; package = std::move(value);
        if (!package->extended_save.isEmpty()) WriteBytes(m, Data().Find("gExtSaveData").address, package->extended_save);
        UseDifficulty(package->json.value("difficulty").toObject());
        Activity::SetGame(package->config.advanced.contains("MP_Enabled") && package->config.advanced.at("MP_Enabled")?
            package->json.value("shared_game_id").toString().toStdString():std::string{});
        active = true; SetCurrent(&*package); code_epoch.fetch_add(1);
        MultiplayerStartup::Begin(package->config.advanced.contains("MP_Enabled") && package->config.advanced.at("MP_Enabled"));
        SetStatus("Randomized file: " + package->config.seed);
        return true;
    }
    template<class M> bool Start(M& m, unsigned selected) {
        bool invalid = false;
        auto loaded = LoadSlot(selected, ReadBytes(m, 0x55bec0 + selected*0x15c4, 0x14dc), invalid);
        if (invalid) return false;
        if (!loaded) { Deactivate(m); return true; }
        return Activate(m, selected, std::move(*loaded));
    }
    template<class M> bool CanOpen(M& m, unsigned selected,bool notify=true) {
        bool invalid = false;
        const auto loaded=LoadSlot(selected, ReadBytes(m, 0x55bec0 + selected*0x15c4, 0x14dc), invalid,notify);
        FileStatus::Publish(loaded && !invalid?selected:3,loaded && !invalid?loaded->config.seed:"",
            loaded && !invalid?loaded->json.value("shared_game_id").toString().toStdString():"");
        SelectRoomFile(selected);
        PublishRoomFile(selected,ReadBytes(m,0x55bec0+selected*0x15c4,0x14dc),loaded?&*loaded:nullptr);
        return !invalid;
    }
    template<class M,class R> bool ShutdownBeforeFrame(M& m,R& r) {
        CheckEpoch();
        if(package && m.Read(0x504fb0).value_or(0)==3 && new_slot.load()>=3 &&
           !m.Read(Data().Find("rHostShutdown").address).value_or(0)) {
            r[15]=Data().Find("hook_HostShutdown").address;return true;
        }
        return false;
    }
    template<class M> void Frame(M& m) {
        CheckEpoch();
        const auto state=m.Read(0x504fb0).value_or(0);
        file_session.Frame(state);
        if(!RoomJoin::HideFiles() && (state==3 || state==7 || state==8)) {
            const unsigned selected=m.Read(0x504fb4).value_or(3);
            SelectRoomFile(selected);
            if(selected<3) {
                const auto bytes=m.Read(0x55bea8+4*selected).value_or(0)?
                    ReadBytes(m,0x55bec0+selected*0x15c4,0x14dc):QByteArray{};
                if(bytes!=exported[selected]) {
                    exported[selected]=bytes;bool invalid=false;
                    const auto value=bytes.isEmpty()?std::optional<Package>{}:LoadSlot(selected,bytes,invalid,false);
                    // Newly named files open automatically, bypassing CanOpen.
                    // Refresh their presentation alongside the room profile.
                    FileStatus::Publish(value && !invalid?selected:3,value && !invalid?value->config.seed:"",
                        value && !invalid?value->json.value("shared_game_id").toString().toStdString():"");
                    PublishRoomFile(selected,bytes,value?&*value:nullptr);
                }
            }
        }
        if(state!=3 && new_slot.load()<3)new_name_seen=true;
        if(state==3) {
            if(new_name_seen && new_slot.load()<3) {
                // Naming was cancelled before the native save initializer ran.
                new_slot=3;new_name_seen=false;creation_chime=false;
            }
            if(new_slot.load()>=3)Deactivate(m);
        }
    }
    template<class M, class R> void Handle(M& m, R& r, unsigned site) {
        CheckEpoch();
        if(site==14) {
            const auto table=Data().Find("rItemTable");
            if(package && r[0]>=table.address && r[0]-table.address<table.size && (r[0]-table.address)%24==0)
                Activity::Emit(0,(r[0]-table.address)/24);
            r[3]=r[0];return; // Displaced MOV, leaving the actual reward effect intact.
        }
        if(site==13) {
            const auto check=ResourceDiagnostics::CheckArchive(m,r[1],r[2]);
            if(!check) {
                const auto id=m.Read(r[4]).value_or(0)&0xffff;
                std::string path;
                if(id<0x1a1) {
                    const auto bytes=ReadBytes(m,0x53ccf4+id*0x44,0x40);
                    for(char c:bytes) {if(!c)break;path+=c>=32 && c<127?c:'?';}
                }
                const auto arena=ResourceDiagnostics::InspectArena(m);
                resource_failure=fmt::format(
                    "The game was stopped before loading an invalid object archive.\n"
                    "Restart the game to continue. This check does not repair the damaged data.\n\n"
                    "File slot: {}\nObject: {:04X}\nPath: {}\nReason: {}\n"
                    "Buffer: {:08X}\nSize: {}\nMember: {}\nOffset: {:08X}\n"
                    "Object context: {:08X}\nObject slot: {}\n"
                    "Resource heap free: {}\nLargest block: {}\nHeap valid: {}\n\n"
                    "Details are recorded in azahar_log.txt.",slot+1,id,path,check.error,
                    r[1],r[2],check.index,check.offset,r[8],r[6],arena.free,arena.largest_free,arena.complete);
                LOG_CRITICAL(Core,"OoT invalid object archive: {}",resource_failure);
                for(unsigned i=0;i<8;++i)
                    LOG_CRITICAL(Core,"OoT archive header: offset={:02X} value={:08X}",i*4,m.Read(r[1]+i*4).value_or(0));
                return; // The caller halts the CPU before the displaced BL.
            }
            r[14]=ObjectArchiveSite.pc+4;r[15]=0x31b124;
        } else if(site==12) {
            r[0]=m.Read(r[4]).value_or(0); // Original LDR, before the native error check.
            if(r[0]&0x80000000u) {
                const auto path=QString::fromStdU16String(ResourceDiagnostics::Path(m,r[4]+0x14)).toStdString();
                LOG_ERROR(Core, "OoT randomizer resource read failed: result={:08X} path={} request={:08X} size={} buffer={:08X} allocator={} scene={} slot={}",
                    r[0],path,r[4],m.Read(r[4]+4).value_or(0),m.Read(r[4]+0x10).value_or(0),
                    m.Read(r[4]+0xc).value_or(0),
                    m.Read(m.Read(Data().Find("gGlobalContext").address).value_or(0)+0x104).value_or(0)&0xffff,slot);
                const auto arena=ResourceDiagnostics::InspectArena(m);
                LOG_ERROR(Core, "OoT randomizer resource arena: used={} free={} largest_free={} blocks={} complete={} stopped_at={:08X}",
                    arena.used,arena.free,arena.largest_free,arena.blocks,arena.complete,arena.stopped_at);
                const auto global=m.Read(Data().Find("gGlobalContext").address).value_or(0);
                for(const auto context : {global ? global+0x3a58 : 0u,Data().Find("rExtendedObjectCtx").address}) {
                    if(!context)continue;
                    const auto counts=m.Read(context).value_or(0);
                    const auto limit=context==global+0x3a58?19u:30u;
                    LOG_ERROR(Core, "OoT randomizer resource objects: context={:08X} count={} persistent={}",
                        context,counts&0xff,(counts>>8)&0xff);
                    for(unsigned i=0;i<std::min(counts&0xff,limit);++i) {
                        const auto entry=context+4+i*0x80;
                        LOG_ERROR(Core, "OoT randomizer resource object: slot={} id={:04X} buffer={:08X} size={}",
                            i,m.Read(entry).value_or(0)&0xffff,m.Read(entry+4).value_or(0),m.Read(entry+8).value_or(0));
                    }
                }
            }
        } else if(site==11) {
            multiplayer_stage=std::int32_t(r[0]);multiplayer_peers=r[1];multiplayer_error=r[2];
            if(const auto color=Data().Find("rMultiplayerTunicColor");color.address)m.Write(color.address,PlayerColor());
            const auto session=MultiplayerStartup::GetSession();
            const unsigned role=session?session->role:0;
            unsigned command=multiplayer_command.exchange(0);
            // A saved multiplayer setting permits sharing; it does not mean
            // the player joined a room. With no role, the guest's legacy path
            // otherwise starts blocking UDS scans and eventually hosts a local
            // network. Stop it at this bridge, before any service initialization.
            // Keep the bundled payload/seed ABI unchanged for existing saves.
            if(!role && (std::int32_t(r[0])!=-2 || command==1)) {
                command=2;
                if(std::int32_t(r[0])==0)
                    LOG_INFO(Network,"OoT multiplayer startup skipped: no room connection (slot={})",slot);
            }
            using Phase=MultiplayerStartup::Phase;
            if(!role || command==2)MultiplayerStartup::phase=Phase::Idle;
            else if(command==1 && role)MultiplayerStartup::Begin(true);
            else if((role || MultiplayerStartup::Busy()) && std::int32_t(r[0])>=-1) {
                const auto ready=m.Read(Data().Find("rMultiplayerReady").address).value_or(0);
                const auto next=std::int32_t(r[0])==-1?Phase::Failed:ready?Phase::Ready:r[0]==3?Phase::Syncing:Phase::Connecting;
                if(MultiplayerStartup::phase.exchange(next)!=next) {
                    LOG_INFO(Network,"OoT multiplayer startup: phase={} stage={} peers={} error={:08X}",unsigned(next),r[0],r[1],r[2]);
                    if(next==Phase::Failed)LOG_ERROR(Network,"OoT multiplayer connection failed. Retry synchronization or disconnect in F1 > Multiplayer.");
                }
            }
            const auto username=Data().Find("rMultiplayerUsername");
            if(session && username.size>=33) {
                auto bytes=QString::fromStdString(session->nickname).left(8).toUtf8();bytes.resize(33,'\0');
                WriteBytes(m,username.address,bytes);
            }
            r[0]=command|(role<<8);
        } else if (site == 1 && package) {
            // Only differences made by Randomizer_Init are restored, never live
            // UI or save state. This covers actor, scene, entrance and item tables.
            before_init = ReadBytes(m, 0x4d4000, 0x55a000-0x4d4000);
        } else if (site == 2 && package && !before_init.isEmpty()) {
            const auto after = ReadBytes(m, 0x4d4000, before_init.size());
            if (after.size() == before_init.size()) {
                for (qsizetype i = 0; i < after.size(); i += 4)
                    if (after.mid(i,4) != before_init.mid(i,4)) backups.push_back({std::uint32_t(0x4d4000+i), before_init.mid(i,4), "init"});
                // Object_UpdateEntries caches file size beside each filename.
                // Actor_Init repurposes object slots, but a size cached by an
                // earlier ordinary file still describes the previous archive.
                // Also preserve zero sizes: later lazy loads must not leave a
                // replacement archive's size behind when its name is restored.
                for(unsigned id=0;id<0x1a1;++id) {
                    const auto address=0x53ccf4+id*0x44;
                    const qsizetype offset=address-0x4d4000;
                    if(after.mid(offset,0x40)==before_init.mid(offset,0x40))continue;
                    backups.push_back({address+0x40,before_init.mid(offset+0x40,4),"object size cache"});
                    m.Write(address+0x40,0);
                }
            }
            before_init.clear();
        } else if (site == 3 && package) {
            if (!package->extended_save.isEmpty()) WriteBytes(m, Data().Find("gExtSaveData").address, package->extended_save);
        } else if (site == 4 && package) {
            package->extended_save = ReadBytes(m, Data().Find("gExtSaveData").address, Data().Find("gExtSaveData").size);
        } else if (site == 5) {
            r[0]=Prices::Cost(std::int32_t(r[0]),Prices::Factor());
        } else if (site == 6) {
            const auto selected = new_slot.exchange(3);
            // New-file defaults reset persistent equips, but leave the old
            // temporary B item at +0x156f intact. The first native save restores
            // that byte into an empty B slot before Play_Init can clear it.
            if(selected<3)WriteBytes(m,0x587958+0x156f,QByteArray(1,char(0)));
            if (selected < 3 && new_randomized.load() && package && slot == selected) {
                WriteBytes(m,0x587958+SaveMarkerOffset,QByteArray(1,char(SaveMarker)));
                r[14] = Sites[0].pc+4; r[15] = Data().Find("hook_SaveFile_Init").address; return;
            } else if (selected < 3) {
                Deactivate(m); BeginOrdinaryFile(selected);
                // Native defaults leave this reserved byte untouched when reusing
                // SaveContext. Clear it only for an explicitly ordinary new file;
                // opening/loading a marked save must still require its seed data.
                if(!new_randomized.load())
                    WriteBytes(m,0x587958+SaveMarkerOffset,QByteArray(1,char(0)));
            }
            WriteBytes(m, r[0]+0x35, QByteArray(1, char(r[1])));
        } else if (site == 7) {
            if (package) {
                // Initialize the payload before native savewarp selection, not
                // at Play_Init (which is too late to choose the first entrance).
                r[15] = Data().Find("hook_BeforeLoadGame").address;
                return;
            }
            r[0] = r[4]+r[5];
        } else if (site == 8) {
            // Capture the exact native save buffer before the asynchronous write
            // is queued. Content-addressed records retain the previous save if
            // the native disk operation fails or is interrupted.
            if (r[2] == 0x14dc && TitleScreen::Pointer(r[0], 0x80) && TitleScreen::Pointer(r[1], r[2])) {
                const auto name_bytes = ReadBytes(m, r[0], 0x80);
                std::string name;
                for (int i = 0; i+1 < name_bytes.size(); i += 2) {
                    if (name_bytes[i] == 0 && name_bytes[i+1] == 0) break;
                    name += name_bytes[i];
                }
                unsigned target = 3;
                for (unsigned i = 0; i < 3; ++i)
                    if (name == "data:/save0" + std::to_string(i) + ".bin") target = i;
                if (target < 3) {
                    const auto bytes = ReadBytes(m, r[1], r[2]);
                    if(room_import.request && room_import.file && room_import.request->slot==target) {
                        const auto& value=room_import.file->package;
                        if(!RecordSave(target,bytes,&value,value.extended_save)) {
                            r[0]=0;r[15]=r[14];return;
                        }
                    } else if (target == copy_destination) {
                        if(!RecordSave(target, bytes, copy_package ? &*copy_package : nullptr,
                            copy_package ? copy_package->extended_save : QByteArray{})) {
                            r[0]=0;r[15]=r[14];return; // Native allocation/write failure path.
                        }
                        copy_destination = 3; copy_package.reset();
                    } else {
                        const auto ext = package ? ReadBytes(m, Data().Find("gExtSaveData").address, Data().Find("gExtSaveData").size) : QByteArray{};
                        if(!RecordSave(target, bytes, package && slot == target ? &*package : nullptr, ext)) {
                            r[0]=0;r[15]=r[14];return;
                        }
                    }
                }
            }
            r[13] -= 32;
            for (unsigned i = 0; i < 7; ++i) m.Write(r[13]+i*4, r[4+i]);
            m.Write(r[13]+28, r[14]);
        } else if (site == 9) {
            if (r[0] < 3 && r[1] < 3) {
                bool invalid = false;
                copy_destination = r[1];
                copy_package = LoadSlot(r[0], ReadBytes(m, 0x55bec0+r[0]*0x15c4, 0x14dc), invalid);
                if(invalid) {
                    copy_destination=3;
                    // The original entry PUSH has executed, but no I/O has.
                    for(unsigned i=0;i<5;++i)r[4+i]=m.Read(r[13]+i*4).value_or(0);
                    r[15]=m.Read(r[13]+20).value_or(r[14]);r[13]+=24;return;
                }
                if (!copy_package) BeginOrdinaryFile(copy_destination);
            }
            r[13] -= 0x240;
        } else if (site == 10) {
            const bool joined=RoomJoin::success_chime.exchange(false);
            if (creation_chime.exchange(false) || joined) r[0] = 0x01000487;
            r[14] = Sites[4].pc+4; r[15] = 0x37547c;
        }
    }
};
inline Hook runtime;
struct FileFlow {
    void ToggleRandomizer() {ToggleEnabled();}
    bool RejectChoice(bool mq) {return UnsupportedMasterQuest(GetConfig(),mq);}
    template<class M> int Prepare(M& m, unsigned slot, bool mq) {
        if (!GetConfig().enabled && !busy.load() && !PreparationPending() && !runtime.staging) {
            new_name_seen=false;new_slot=slot; new_randomized=false; new_mq=mq; return 1;
        }
        if (!CodeMatches(m)) {
            SetError(FileStatus::Error::Unsupported,"This executable does not support randomized files."); return -1;
        }
        if (!m.Read(GuestEnd-4)) {
            SetError(FileStatus::Error::Memory,"Randomizer memory was not reserved. Restart the game with the updated app."); return -1;
        }
        runtime.CheckEpoch();
        if(!runtime.staging) {
            const auto result=PrepareNew(slot,mq);
            if(result!=1)return result;
            auto value=TakePrepared(slot,mq);
            if(!value) {SetError(FileStatus::Error::Generation,"Prepared seed is unavailable.");return -1;}
            // No live randomizer may execute while its reserved payload is
            // replaced. File-menu entry already shuts down the prior runtime.
            runtime.Deactivate(m);
            runtime.staging=std::move(value);runtime.staging_slot=slot;runtime.staging_offset=0;
        }
        if(runtime.staging_slot!=slot || runtime.staging->mq!=mq) {
            runtime.CancelStaging();SetError(FileStatus::Error::Incompatible,"Prepared seed does not match the selected file or quest.");return -1;
        }
        // Bound guest-memory writes per menu update. These bytes are unused
        // until the final activation, so B can cancel without touching saves.
        constexpr qsizetype chunk=32*1024;
        const auto amount=std::min(chunk,runtime.staging->payload.size()-runtime.staging_offset);
        WriteBytes(m,GuestBase+runtime.staging_offset,runtime.staging->payload.mid(runtime.staging_offset,amount));
        runtime.staging_offset+=amount;
        if(runtime.staging_offset<runtime.staging->payload.size())return 0;
        auto value=std::move(*runtime.staging);runtime.CancelStaging();
        if(!runtime.Activate(m,slot,std::move(value),true))return -1;
        new_name_seen=false;new_slot=slot;new_randomized=true;new_mq=mq;creation_chime=true;
        return 1;
    }
    template<class M> bool Open(M& m, unsigned slot) {return runtime.CanOpen(m,slot);}
    template<class M> bool OpenForManagement(M& m, unsigned slot) {
        // File management remains available even when gameplay cannot load.
        runtime.CanOpen(m,slot,false);return true;
    }
    template<class M> FileStatus::Error CheckStart(M& m,unsigned slot) {
        bool invalid=false;FileStatus::Error error{};
        LoadSlot(slot,ReadBytes(m,0x55bec0+slot*0x15c4,0x14dc),invalid,false,&error);
        return error;
    }
    template<class M> bool Start(M& m, unsigned slot) {return runtime.Start(m,slot);}
    void Cancel() {CancelPreparation();runtime.CancelStaging();}
};
} // namespace OOT3D::Randomizer
