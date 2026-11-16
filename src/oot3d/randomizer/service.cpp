// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/randomizer/service.h"
#include "oot3d/randomizer/presets.h"
#include "common/logging/log.h"
#include "oot3d/randomizer/difficulty.h"
#include "oot3d/randomizer/wire_validation.h"
#include "oot3d/randomizer/generation_progress.h"
#include <QElapsedTimer>
#include <future>
#include <mutex>
#include <cstring>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QUuid>
#ifdef Q_OS_MACOS
#include <crt_externs.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <thread>
#include <cerrno>
#endif

namespace OOT3D::Randomizer {
namespace {
std::mutex mutex;
Config draft = NewFileConfig();
std::optional<Config> current;
std::string status;
QString directory, executable;
std::future<std::optional<Package>> generation;
std::shared_ptr<std::atomic<bool>> cancel_generation;
// Each std::async generator runs on its own thread. Keep its cancellation token
// with error events, including the completion/B-press race.
thread_local std::shared_ptr<const std::atomic<bool>> reporting_cancellation;
std::optional<Package> prepared;
unsigned preparing_slot = 3;
std::array<std::shared_ptr<const RoomFile>,3> room_files;
std::atomic<unsigned> room_selected{3};
bool preparing_mq = false, generation_failed = false;
QString SlotDirectory(unsigned slot) { return directory + QStringLiteral("/slot%1").arg(slot); }
QString SaveKey(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QString RecordPath(unsigned slot, const QByteArray& bytes) {
    return SlotDirectory(slot) + "/" + SaveKey(bytes) + ".json";
}
QByteArray RoomIdentity(QJsonObject json) {
    json.remove("extended_save");
    return QCryptographicHash::hash(QJsonDocument(json).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
}
void ReadGenerationProgress(const QString& path,const std::shared_ptr<GenerationProgress::Session>& progress) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))return;
    const auto bytes=file.read(64);
    progress->Observe(std::string_view(bytes.constData(),bytes.size()));
}
#ifdef Q_OS_MACOS
// QProcess uses fork on macOS. Fork takes process-wide allocator locks and
// stalls the emulation/audio threads even when launched from a worker. Spawn
// the helper directly: no copy of the emulator and no at-fork lock barrier.
bool SpawnGenerator(const QString& program,const QString& input,bool mq,const QString& output,
                    const QString& errors,const QString& progress_path,
                    const std::shared_ptr<GenerationProgress::Session>& progress,
                    const std::shared_ptr<std::atomic<bool>>& cancelled) {
    auto executable_bytes=QFile::encodeName(program),input_bytes=QFile::encodeName(input),
         output_bytes=QFile::encodeName(output),error_bytes=QFile::encodeName(errors),
         progress_bytes=QFile::encodeName(progress_path);
    char original[]="original",master[]="mq";
    char* arguments[]{executable_bytes.data(),input_bytes.data(),mq?master:original,output_bytes.data(),progress_bytes.data(),nullptr};
    posix_spawn_file_actions_t actions;
    int error=posix_spawn_file_actions_init(&actions);
    if(error) {SetError(FileStatus::Error::Generator,"Could not initialize the seed generator: "+std::string(std::strerror(error)));return false;}
    struct ActionsGuard {
        posix_spawn_file_actions_t& actions;
        ~ActionsGuard() {posix_spawn_file_actions_destroy(&actions);}
    } guard{actions};
    posix_spawnattr_t attributes;
    error=posix_spawnattr_init(&attributes);
    if(error) {SetError(FileStatus::Error::Generator,"Could not initialize seed generator attributes.");return false;}
    struct AttributeGuard {
        posix_spawnattr_t& attributes;
        ~AttributeGuard() {posix_spawnattr_destroy(&attributes);}
    } attribute_guard{attributes};
    error=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_CLOEXEC_DEFAULT);
    if(!error)error=posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    if(!error)error=posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,"/dev/null",O_WRONLY,0);
    if(!error)error=posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,error_bytes.constData(),O_WRONLY|O_CREAT|O_TRUNC,0600);
    pid_t child=0;
    if(!error && !cancelled->load())error=posix_spawn(&child,executable_bytes.constData(),&actions,&attributes,arguments,*_NSGetEnviron());
    if(error) {SetError(FileStatus::Error::Generator,"Could not start the bundled seed generator: "+std::string(std::strerror(error)));return false;}
    if(!child)return false;
    int status=0;
    QElapsedTimer elapsed;elapsed.start();
    qint64 next_progress=0;
    while(true) {
        if(elapsed.elapsed()>=next_progress) {
            ReadGenerationProgress(progress_path,progress);next_progress=elapsed.elapsed()+100;
        }
        const auto result=waitpid(child,&status,WNOHANG);
        if(result==child)break;
        if(result<0 && errno!=EINTR) {SetError(FileStatus::Error::Generator,"Could not wait for the seed generator.");return false;}
        if(cancelled->load() || elapsed.elapsed()>=300000) {
            kill(child,SIGKILL);
            while(waitpid(child,&status,0)<0 && errno==EINTR) {}
            if(!cancelled->load())SetError(FileStatus::Error::Timeout,"Seed generation timed out. Try another seed.");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(cancelled->load())return false;
    if(!WIFEXITED(status) || WEXITSTATUS(status)!=0) {
        QFile file(errors);QByteArray detail;
        if(file.open(QIODevice::ReadOnly)) {file.seek(std::max<qint64>(0,file.size()-1000));detail=file.read(1000);}
        SetError(FileStatus::GeneratorError(detail.toStdString()),"Generation failed: "+detail.toStdString());return false;
    }
    return true;
}
#endif
}
void Initialize(QSettings& settings, const std::string& path) {
    ClearRoomFiles();
    Presets::Load(settings);
    directory = QString::fromStdString(path);
    executable = QCoreApplication::applicationDirPath() + "/oot3d-seed";
#ifdef _WIN32
    executable += ".exe";
#endif
    Config value = NewFileConfig();
    settings.beginGroup("Randomizer");
    player_color_enabled=settings.value("player_color_enabled",false).toBool();
    player_color=settings.value("player_color",0x64ff64u).toUInt()&0xffffffu;
    value.enabled = settings.value("enabled", false).toBool();
    value.seed = settings.value("seed", "").toString().toStdString();
    for (unsigned i = 0; i < Specs.size(); ++i) {
        bool ok = false;
        const int number = settings.value(QLatin1String(Specs[i].key), Specs[i].initial).toInt(&ok);
        value.options[i] = ok ? number : Specs[i].initial;
    }
    settings.beginGroup("advanced");
    for(const auto& option:AdvancedOptions()) {
        const auto key=QString::fromStdString(option.key);
        if(settings.contains(key)) {
            const int number=settings.value(key).toInt();
            if(number>=0 && std::size_t(number)<option.choices.size())value.advanced[option.key]=number;
        }
    }
    settings.endGroup();
    settings.endGroup();
    std::scoped_lock lock(mutex);
    draft = Sanitize(value); dirty = false; FileStatus::enabled=draft.enabled;
    FileStatus::Publish();
}
void SaveConfig(QSettings& settings) {
    const auto config = GetConfig();
    settings.beginGroup("Randomizer");
    settings.setValue("player_color_enabled",player_color_enabled.load());
    settings.setValue("player_color",player_color.load());
    settings.setValue("enabled", config.enabled);
    settings.setValue("seed", QString::fromStdString(config.seed));
    for (unsigned i = 0; i < Specs.size(); ++i)
        settings.setValue(QLatin1String(Specs[i].key), config.options[i]);
    settings.beginGroup("advanced");settings.remove("");
    for(const auto& [key,value]:config.advanced)settings.setValue(QString::fromStdString(key),value);
    settings.endGroup();
    settings.endGroup(); settings.sync();
    if (settings.status() != QSettings::NoError) {
        dirty = true; SetError(FileStatus::Error::Storage,"Could not save randomizer settings.");
    }
}
Config GetConfig() { std::scoped_lock lock(mutex); return draft; }
void SetConfig(const Config& value) {
    std::scoped_lock lock(mutex); draft = Sanitize(value); dirty = true;
    FileStatus::enabled=draft.enabled;
}
void ToggleEnabled() {
    std::scoped_lock lock(mutex);draft.enabled=!draft.enabled;dirty=true;
    FileStatus::enabled=draft.enabled;
}
std::string Status() { std::scoped_lock lock(mutex); return status; }
void SetStatus(const std::string& text) { std::scoped_lock lock(mutex); status = text; }
void SetError(FileStatus::Error error,const std::string& detail) {
    LOG_ERROR(Core,"OoT randomizer: {}",detail);
    std::scoped_lock lock(mutex);status=detail;FileStatus::Report(error,reporting_cancellation);
}
std::optional<Config> CurrentConfig() { std::scoped_lock lock(mutex); return current; }
void SetCurrent(const Package* package) {
    std::scoped_lock lock(mutex);
    current = package ? std::optional(package->config) : std::nullopt;
}
std::optional<Package> DecodePackage(const QJsonObject& input) {
    const auto& data = Data();
    auto object = input;
    // These runtimes share the settings/save structs and item keys. Preserve
    // placements and shop prices; only relocate absolute message pointers.
    // Never regenerate an existing seed or accept an unknown runtime layout.
    const auto runtime = object.value("runtime").toString();
    const bool first_runtime = runtime ==
        QLatin1String("d6d66fb337d9a0a7374785c09625e657662b5e0c2fc286e2b72b88be1573a6af");
    const bool before_time_travel_fix = runtime ==
        QLatin1String("5035dcd1007f8951ff9fc77d25abdbfd617b3a1ed51b80e0c3c27b8acbc82b86");
    const bool before_hook_abi_fix = runtime ==
        QLatin1String("7ec0600623105e620969377dd7733f402c3dbd9a4e2f0f4daee474108b3af7ff");
    const bool before_native_alignment_fix = runtime ==
        QLatin1String("bfd86fb3f38584d079943f109cb9ad005a88441869842fad6c9913b01b3e40ed");
    const bool before_expansion = runtime == QLatin1String("420dc8bca60d1b0ee9ae30f1f76e4b67da092faa655d9085372b03640bcc8440");
    const bool before_player_color = runtime == QLatin1String("2eaf0c69859e7ad7298187b37dcc2f578f1f9c33427eaf92aee0f9b2f20c4101");
    const bool before_new_file_sword_fix = runtime == QLatin1String("35a86e8871a00fc7e29cb480923e99855fa182a4c2b58dcf0a81452445e7b520");
    if ((object.value("format").toInt() == 1 &&
        (first_runtime || before_time_travel_fix || before_hook_abi_fix || before_native_alignment_fix || before_expansion)) ||
        ((before_player_color || before_new_file_sword_fix) &&
         (object.value("format").toInt()==1 || object.value("format").toInt()==2))) {
        const auto decoded = QByteArray::fromBase64Encoding(object.value("message_entries").toString().toLatin1(),
                                                            QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded || decoded.decoded.size() % 0x60) return {};
        auto entries = decoded.decoded;
        const std::uint32_t old_message_at = before_new_file_sword_fix?6668481:before_player_color?6665565:GuestBase +
            (first_runtime ? 600985 : before_time_travel_fix ? 601257 :
             before_hook_abi_fix ? 601273 : 605433);
        const auto new_message_at = data.Find("rCustomMessages").address;
        for (qsizetype at = 0; at < entries.size(); at += 0x60) {
            for (unsigned language = 0; language < 10; ++language) {
                const auto offset_at = at + 16 + language * 8;
                std::uint32_t offset = 0, length = 0;
                for (unsigned j = 0; j < 4; ++j) {
                    offset |= std::uint32_t(std::uint8_t(entries[offset_at+j])) << (8*j);
                    length |= std::uint32_t(std::uint8_t(entries[offset_at+4+j])) << (8*j);
                }
                if (!length) continue;
                if (offset < old_message_at || std::uint64_t(offset-old_message_at)+new_message_at > UINT32_MAX) return {};
                offset = offset - old_message_at + new_message_at;
                for (unsigned j = 0; j < 4; ++j) entries[offset_at+j] = char(offset >> (8*j));
            }
        }
        if (first_runtime) {
            auto fields = object.value("fields").toObject();
            fields["rShopsanityPrices"] = QString::fromLatin1(QByteArray(32*4, 0).toBase64());
            object["fields"] = fields;
        }
        object["message_entries"] = QString::fromLatin1(entries.toBase64());
        object["runtime"] = QString::fromLatin1(data.digest);
    }
    const int format=object.value("format").toInt();
    if ((format != 1 && format != 2) || object.value("runtime").toString() != QString::fromLatin1(data.digest)) return {};
    if (!ValidDifficulty(object.value("difficulty").toObject())) return {};
    Package result;
    // IDs belong to a playthrough, never to its seed or network host.
    if(!object.contains("shared_game_id"))
        object["shared_game_id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto game_id=object.value("shared_game_id").toString();
    if(QUuid(game_id).isNull() || QUuid(game_id).toString(QUuid::WithoutBraces)!=game_id)return {};
    result.json = object; result.payload = data.bytes; result.config = FromJson(object.value("config").toObject());
    result.mq = object.value("mq").toBool();
    for(const auto& [key,value]:result.config.advanced) {
        const auto& options=AdvancedOptions();
        const auto it=std::find_if(options.begin(),options.end(),[&](const auto& o){return o.key==key;});
        if(it==options.end() || value<0 || std::size_t(value)>=it->choices.size())return {};
    }
    if(format==1 && !result.config.advanced.empty())return {};
    const auto native=object.value("native_patches").toArray();
    if(native.size()>3 || (format==1 && !native.isEmpty()))return {};
    std::vector<unsigned> seen;
    for(const auto& entry:native) {
        const auto patch=entry.toObject();
        const auto address=patch.value("address").toInteger();
        const auto bytes=QByteArray::fromBase64Encoding(patch.value("data").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        const unsigned size=address==0x52236c?22:address==0x54c222?108:address==0x54c8b0?14:0;
        if(!bytes || !size || bytes.decoded.size()!=size || std::find(seen.begin(),seen.end(),address)!=seen.end())return {};
        seen.push_back(address);
        result.native_patches.push_back({std::uint32_t(address),bytes.decoded,"seed table"});
    }

    const auto fields = object.value("fields").toObject();
    QStringList allowed{"rItemOverrides", "gSettingsContext", "gSpoilerData", "gSpoilerDataLocs",
                              "rDungeonRewardOverrides", "rDungeonImportance", "rShopsanityPrices"};
    if(format==2)allowed.append({"rEntranceOverrides","rEnemyOverrides","rGsLocOverrides",
        "rBGMOverrides","rSfxData","rScrubRandomItemPrices"});
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (!allowed.contains(it.key())) return {};
        const auto sym = data.Find(it.key().toLatin1().constData());
        const auto decoded = QByteArray::fromBase64Encoding(it.value().toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) return {};
        const auto bytes = decoded.decoded;
        if ((it.key() != "rItemOverrides" && bytes.size() != sym.size) ||
            (it.key() == "rItemOverrides" && (bytes.isEmpty() || bytes.size()%8 || bytes.size() >= sym.size))) return {};
        if (sym.address < GuestBase || bytes.size() > sym.size ||
            std::uint64_t(sym.address - GuestBase) + bytes.size() > std::uint64_t(result.payload.size())) return {};
        std::memcpy(result.payload.data() + sym.address - GuestBase, bytes.data(), bytes.size());
    }
    for (const auto& required : allowed)
        if (required != "rDungeonRewardOverrides" && !fields.contains(required)) return {};
    if (!result.config.options[Rewards] && !fields.contains("rDungeonRewardOverrides")) return {};
    if(format==2 && !ValidExpandedPackage(result))return {};
    const auto message_data = QByteArray::fromBase64(object.value("message_data").toString().toLatin1());
    const auto entries = QByteArray::fromBase64(object.value("message_entries").toString().toLatin1());
    const auto message_at = data.Find("rCustomMessages").address;
    if (message_at != GuestBase + data.bytes.size() || message_data.size() > 1024 * 1024 ||
        entries.size() > 1024 * 1024 || entries.size() != object.value("message_count").toInt() * 0x60) return {};
    const auto read_word = [](const QByteArray& bytes, qsizetype at) {
        std::uint32_t word=0;
        for(unsigned j=0;j<4;++j)word|=std::uint32_t(std::uint8_t(bytes[at+j]))<<(8*j);
        return word;
    };
    std::uint32_t last_id=0;
    for(qsizetype at=0;at<entries.size();at+=0x60) {
        const auto id=read_word(entries,at);
        if(at && id<=last_id)return {};
        last_id=id;
        for(unsigned language=0;language<10;++language) {
            const auto offset=read_word(entries,at+16+language*8);
            const auto length=read_word(entries,at+20+language*8);
            if(length && (offset<message_at || std::uint64_t(offset)+length>std::uint64_t(message_at)+message_data.size()))return {};
        }
    }
    result.payload += message_data;
    while (result.payload.size() & 3) result.payload += char(0);
    const std::uint32_t entries_at = GuestBase + result.payload.size();
    result.payload += entries;
    if (result.payload.size() > GuestEnd - GuestBase - 0x1000) return {};
    const auto write_word = [&](const char* name, std::uint32_t word) {
        const auto sym = data.Find(name);
        if (sym.address < GuestBase || sym.address + 4 > GuestBase + data.bytes.size()) return false;
        for (unsigned i = 0; i < 4; ++i) result.payload[sym.address - GuestBase + i] = char(word >> (8 * i));
        return true;
    };
    if (!write_word("ptrCustomMessageEntries", entries_at) ||
        !write_word("numCustomMessageEntries", object.value("message_count").toInt())) return {};
    result.extended_save = QByteArray::fromBase64(object.value("extended_save").toString().toLatin1());
    if (!result.extended_save.isEmpty() && result.extended_save.size() != data.Find("gExtSaveData").size) return {};
    return result;
}
bool PreparationPending() {return preparing_slot<3;}
int PrepareNew(unsigned slot, bool mq) {
    if (slot >= 3) return -1;
    if (preparing_slot == slot && preparing_mq == mq && prepared) return 1;
    if (generation.valid()) {
        if (generation.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return 0;
        auto result = generation.get(); busy = false;
        if (preparing_slot != slot || preparing_mq != mq) return 0;
        prepared = std::move(result);
        generation_failed = !prepared;
        if (prepared) SetStatus("Seed ready. Finish naming your file to save it.");
        return prepared ? 1 : -1;
    }
    if (generation_failed && preparing_slot == slot && preparing_mq == mq) return -1;
    const auto config = GetConfig();
    preparing_slot = slot; preparing_mq = mq; generation_failed = false;
    prepared.reset(); busy = true; SetStatus("Generating a beatable seed...");
    const auto program = executable;
    const auto difficulty = CaptureDifficulty();
    cancel_generation = std::make_shared<std::atomic<bool>>(false);
    const auto cancelled = cancel_generation;
    const auto progress=GenerationProgress::Begin();
    generation = std::async(std::launch::async, [config, mq, program, difficulty, cancelled, progress, slot]() -> std::optional<Package> {
        reporting_cancellation=cancelled;
        QTemporaryDir temp;
        auto input = ToJson(config); input["difficulty"] = difficulty;
        if (!temp.isValid() || !WriteJson(temp.filePath("config.json"), input)) {
            SetError(FileStatus::Error::Storage,"Could not create temporary seed files."); return {};
        }
#ifdef Q_OS_MACOS
        if(!SpawnGenerator(program,temp.filePath("config.json"),mq,temp.filePath("seed.json"),
                           temp.filePath("errors.txt"),temp.filePath("progress.txt"),progress,cancelled))return {};
#else
        QProcess process;
        process.setStandardOutputFile(QProcess::nullDevice());
        process.start(program, {temp.filePath("config.json"), mq ? "mq" : "original", temp.filePath("seed.json"),temp.filePath("progress.txt")});
        if (!process.waitForStarted(10000)) { SetError(FileStatus::Error::Generator,"Could not start the bundled seed generator."); return {}; }
        QElapsedTimer elapsed; elapsed.start();
        while (!process.waitForFinished(100)) {
            ReadGenerationProgress(temp.filePath("progress.txt"),progress);
            if (cancelled->load() || elapsed.elapsed() >= 300000) {
                process.kill(); process.waitForFinished(3000);
                if (!cancelled->load()) SetError(FileStatus::Error::Timeout,"Seed generation timed out. Try another seed.");
                return {};
            }
        }
        if (cancelled->load()) return {};
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            const auto detail=process.readAllStandardError().right(1000).toStdString();
            SetError(FileStatus::GeneratorError(detail),"Generation failed: " + detail);
            return {};
        }
#endif
        // Decode/validate the payload and probe save-folder permissions on the
        // worker too. Neither JSON expansion nor filesystem sync belongs in a
        // file-menu update on the emulation thread.
        auto result = DecodePackage(ReadJson(temp.filePath("seed.json")));
        if (cancelled->load()) return {};
        if (!result) { SetError(FileStatus::Error::Incompatible,"Generated seed did not match the bundled runtime."); return {}; }
        if (!CanStore(slot) || cancelled->load()) return {};
        return result;
    });
    return 0;
}
void CancelPreparation() {
    if (cancel_generation) cancel_generation->store(true);
    GenerationProgress::Reset();
    preparing_slot = 3; prepared.reset(); generation_failed = false; busy = false;
}
bool CanStore(unsigned slot) {
    if (slot >= 3 || directory.isEmpty() || !QDir().mkpath(SlotDirectory(slot))) {
        SetError(FileStatus::Error::Storage,"Cannot create randomizer save data. Check the user folder permissions."); return false;
    }
    const auto path = SlotDirectory(slot) + "/write-check.json";
    if (!WriteJson(path, QJsonObject{{"check", true}})) {
        SetError(FileStatus::Error::Storage,"Cannot write randomizer save data. Check disk space and folder permissions."); return false;
    }
    QFile::remove(path); return true;
}
std::optional<Package> TakePrepared(unsigned slot, bool mq) {
    if (slot != preparing_slot || mq != preparing_mq) return {};
    auto result = std::move(prepared); prepared.reset(); preparing_slot = 3; return result;
}
std::optional<Package> LoadSlot(unsigned slot, const QByteArray& bytes, bool& invalid,
                              bool notify, FileStatus::Error* error) {
    invalid = false;
    if(error)*error=FileStatus::Error::None;
    const auto reject=[&](FileStatus::Error reason,const char* message) {
        invalid=true;if(error)*error=reason;
        if(notify)SetError(reason,message);
    };
    if (slot >= 3 || bytes.size() != 0x14dc) {
        reject(FileStatus::Error::Incompatible,"This randomized file needs the matching runtime version and seed data.");
        return {};
    }
    const bool randomized=std::uint8_t(bytes[SaveMarkerOffset])==SaveMarker;
    if(!randomized)return {};
    if(slot==1 && simulate_file2_runtime_mismatch.load()) {
        reject(FileStatus::Error::Incompatible,"This randomized file needs the matching runtime version and seed data.");
        return {};
    }
    const auto path = RecordPath(slot, bytes);
    if (!QFile::exists(path)) {
        reject(FileStatus::Error::Missing,"This file's randomizer data is missing or does not match its save.");
        return {};
    }
    const auto object = ReadJson(path);
    auto result = DecodePackage(object);
    invalid = !result || ((bytes[0xe]!=0)!=result->mq);
    // Migrate once, atomically, before advertising this existing file. Never
    // infer a shared playthrough merely because two independent seeds match.
    if(!invalid && !object.contains("shared_game_id") && !WriteJson(path,result->json))invalid=true;
    if(invalid)result.reset();
    if (invalid) reject(FileStatus::Error::Incompatible,"This randomized file needs the matching runtime version and seed data.");
    return result;
}
bool RecordSave(unsigned slot, const QByteArray& bytes, const Package* package, const QByteArray& ext) {
    if (slot >= 3 || bytes.size() != 0x14dc) return false;
    if(!package) {
        if(std::uint8_t(bytes[SaveMarkerOffset])==SaveMarker) {
            SetError(FileStatus::Error::Missing,"Randomizer seed data is unavailable; the save was not overwritten.");return false;
        }
        BeginOrdinaryFile(slot);return true;
    }
    if(directory.isEmpty())return false;
    QDir().mkpath(SlotDirectory(slot));
    QJsonObject object = package ? package->json : QJsonObject{{"ordinary", true}};
    if (package) {
        object["extended_save"] = QString::fromLatin1(ext.toBase64());
        QFile marker(SlotDirectory(slot) + "/randomized");
        if (!marker.open(QIODevice::WriteOnly)) return false;
        marker.close();
    }
    const bool saved = WriteJson(RecordPath(slot, bytes), object);
    if(saved && package) {
        auto snapshot=*package;snapshot.json=object;snapshot.extended_save=ext;
        PublishRoomFile(slot,bytes,&snapshot);
    }
    if (!saved) SetError(FileStatus::Error::Storage,"Could not save randomizer data. Check available disk space before continuing.");
    return saved;
}
void BeginOrdinaryFile(unsigned slot) {
    if (slot < 3) {
        QFile::remove(SlotDirectory(slot) + "/randomized");
        std::atomic_store(&RoomJoin::profiles[slot],std::shared_ptr<const RoomJoin::Profile>{});
        std::atomic_store(&room_files[slot],std::shared_ptr<const RoomFile>{});
    }
}
void CopySlot(unsigned source, unsigned destination, const QByteArray& bytes) {
    bool invalid = false;
    const auto package = LoadSlot(source, bytes, invalid);
    if (invalid) return;
    if (!package) BeginOrdinaryFile(destination);
    RecordSave(destination, bytes, package ? &*package : nullptr, package ? package->extended_save : QByteArray{});
}

bool ValidRoomSave(const QByteArray& bytes) {
    if(bytes.size()!=0x14dc || std::uint8_t(bytes[0x2e])!=2 ||
       std::uint8_t(bytes[SaveMarkerOffset])!=SaveMarker || bytes.mid(0x30,6)!="ZELDAZ" ||
       std::uint8_t(bytes[0xe])>1)return false;
    std::uint16_t crc=0;
    for(qsizetype i=0;i<bytes.size();++i) {
        crc^=i==0x14d8 || i==0x14d9?0:std::uint8_t(bytes[i]);
        for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xa001:0);
    }
    return crc==(std::uint8_t(bytes[0x14d8])|(std::uint16_t(std::uint8_t(bytes[0x14d9]))<<8));
}
void ClearRoomFiles() {
    room_selected=3;
    for(auto& file:room_files)std::atomic_store(&file,std::shared_ptr<const RoomFile>{});
    for(auto& profile:RoomJoin::profiles)std::atomic_store(&profile,std::shared_ptr<const RoomJoin::Profile>{});
}
void SelectRoomFile(unsigned slot) {room_selected=slot;}
void PublishRoomFile(unsigned slot,const QByteArray& bytes,const Package* package) {
    if(slot>=3)return;
    std::atomic_store(&RoomJoin::profiles[slot],std::shared_ptr<const RoomJoin::Profile>{});
    if(!package || !ValidRoomSave(bytes) || (bytes[0xe]!=0)!=package->mq) {
        std::atomic_store(&room_files[slot],std::shared_ptr<const RoomFile>{});return;
    }
    auto file=std::make_shared<RoomFile>();file->native_save=bytes;file->package=*package;
    file->package.json["extended_save"]=QString::fromLatin1(package->extended_save.toBase64());
    file->identity=RoomIdentity(file->package.json);
    const auto id=file->package.json.value("shared_game_id").toString().toStdString();
    if(!id.empty() && package->config.advanced.contains("MP_Enabled") && package->config.advanced.at("MP_Enabled"))
        std::atomic_store(&RoomJoin::profiles[slot],std::make_shared<const RoomJoin::Profile>(RoomJoin::Profile{id,package->mq}));
    const auto wire=QJsonDocument(QJsonObject{{"format","oot3d-room-file-1"},
        {"save",QString::fromLatin1(bytes.toBase64())},{"package",file->package.json}}).toJson(QJsonDocument::Compact);
    if(wire.size()>8*1024*1024)return;
    file->wire=std::make_shared<const std::vector<std::uint8_t>>(wire.begin(),wire.end());
    std::atomic_store(&room_files[slot],std::shared_ptr<const RoomFile>(std::move(file)));
}
std::shared_ptr<const std::vector<std::uint8_t>> HostedRoomFile(bool new_room) {
    // Called only by the frontend. A room stays tied to the file/seed selected
    // when hosting began; moving the cursor cannot publish a different file.
    static unsigned slot=3;static QByteArray identity;
    if(new_room){slot=3;identity.clear();}
    if(slot>=3) {
        const auto selected=room_selected.load();if(selected>=3)return {};
        const auto file=std::atomic_load(&room_files[selected]);if(!file)return {};
        slot=selected;identity=file->identity;
    }
    const auto file=std::atomic_load(&room_files[slot]);
    return file && file->identity==identity?file->wire:nullptr;
}
bool ReceiveRoomFile(const std::shared_ptr<const RoomJoin::Request>& request,
                     const std::vector<std::uint8_t>& wire) {
    if(!request || request->cancelled.load() || wire.empty() || wire.size()>8*1024*1024)return false;
    const auto object=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(wire.data()),wire.size())).object();
    if(object.value("format").toString()!="oot3d-room-file-1")return false;
    const auto raw=QByteArray::fromBase64Encoding(object.value("save").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!raw || !ValidRoomSave(raw.decoded))return false;
    if(!object.value("package").toObject().contains("shared_game_id"))return false;
    auto package=DecodePackage(object.value("package").toObject());
    if(!package || package->mq!=(raw.decoded[0xe]!=0) ||
       package->extended_save.size()!=Data().Find("gExtSaveData").size ||
       !package->config.advanced.contains("MP_Enabled") ||
       package->config.advanced.at("MP_Enabled")==0)return false;
    auto file=std::make_shared<RoomFile>();file->native_save=raw.decoded;file->package=std::move(*package);
    file->identity=RoomIdentity(file->package.json);
    if(request->cancelled.load())return false;
    if(request->intent==RoomJoin::Intent::Resume) {
        const auto local=request->slot<3?std::atomic_load(&room_files[request->slot]):nullptr;
        if(!local || request->game_id!=file->package.json.value("shared_game_id").toString().toStdString() ||
           local->identity!=file->identity)return false;
        // Existing progress is reconciled by ordinary multiplayer at game start.
        // Do not run the import writer on an occupied slot.
        request->saved=true;request->status=RoomJoin::Status::Connected;return true;
    }
    std::atomic_store(&request->file,std::shared_ptr<const RoomFile>(std::move(file)));
    request->status=RoomJoin::Status::ReadyToSave;
    return true;
}
} // namespace OOT3D::Randomizer
