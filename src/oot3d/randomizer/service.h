// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/randomizer/data.h"
#include "oot3d/randomizer/file_status.h"
#include <atomic>
#include <optional>
#include "oot3d/room_join.h"
class QSettings;

namespace OOT3D::Randomizer {
// USA Rev 1 validates only the format byte at +0x2e. Its reserved next
// byte is included in the native checksum and copied with all three slots.
constexpr unsigned SaveMarkerOffset=0x2f;
constexpr unsigned char SaveMarker=0xa7;
inline std::atomic<bool> dirty{false}, busy{false}, active{false};
// Temporary, session-only reproduction of File 2's incompatible-runtime path.
// Never change the on-disk save, seed package, or persistent settings.
inline std::atomic<bool> simulate_file2_runtime_mismatch{false};
// Guest synchronization commands are consumed by the emulation-thread bridge.
inline std::atomic<unsigned> multiplayer_command{0}, multiplayer_peers{0}, multiplayer_error{0};
inline std::atomic<int> multiplayer_stage{-2};
// Personal cosmetics stay outside the seed/package and copied save data.
inline std::atomic<bool> player_color_enabled{false};
inline std::atomic<std::uint32_t> player_color{0x64ff64};
inline std::uint32_t PlayerColor() {return player_color_enabled.load()?0xa5000000u|player_color.load():0;}
struct Package {
    QJsonObject json;
    QByteArray payload, extended_save;
    Config config;
    std::vector<Patch> native_patches;
    bool mq = false;
};
struct RoomFile {
    QByteArray native_save;
    Package package;
    QByteArray identity;
    std::shared_ptr<const std::vector<std::uint8_t>> wire;
};
bool ValidRoomSave(const QByteArray& bytes);
void PublishRoomFile(unsigned slot,const QByteArray& bytes,const Package* package);
void SelectRoomFile(unsigned slot);
void ClearRoomFiles();
std::shared_ptr<const std::vector<std::uint8_t>> HostedRoomFile(bool new_room);
bool ReceiveRoomFile(const std::shared_ptr<const RoomJoin::Request>& request,
                     const std::vector<std::uint8_t>& wire);
void Initialize(QSettings& settings, const std::string& directory);
void SaveConfig(QSettings& settings);
Config GetConfig();
void SetConfig(const Config& config);
void ToggleEnabled();
std::string Status();
void SetStatus(const std::string& text);
void SetError(FileStatus::Error error,const std::string& detail);
void SetCurrent(const Package* package);
std::optional<Config> CurrentConfig();
// 0: waiting; 1: ready; -1: failure. First call starts generation.
int PrepareNew(unsigned slot, bool mq);
bool PreparationPending();
void CancelPreparation();
bool CanStore(unsigned slot);
std::optional<Package> TakePrepared(unsigned slot, bool mq);
std::optional<Package> DecodePackage(const QJsonObject& object);
std::optional<Package> LoadSlot(unsigned slot, const QByteArray& native_save, bool& invalid,
                              bool notify=true, FileStatus::Error* error=nullptr);
bool RecordSave(unsigned slot, const QByteArray& native_save, const Package* package,
                const QByteArray& extended_save);
void BeginOrdinaryFile(unsigned slot);
void CopySlot(unsigned source, unsigned destination, const QByteArray& native_save);
} // namespace OOT3D::Randomizer
