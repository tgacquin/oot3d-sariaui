// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace OOT3D::MultiplayerSettings {
struct Config {
    std::string room_name,host_address;
    int room_port=24872,max_players=8,discovery_port=24873;
    bool search_lan=true;
    bool operator==(const Config&) const = default;
};
struct Status {
    unsigned role=0,players=0,port=0;
    bool busy=false,can_disconnect=false;
    std::string nickname,error;
};
// UI snapshots and queued edits only. Qt's existing multiplayer configuration
// remains the persistent source of truth; the render thread never accesses it.
inline std::mutex mutex;
inline Config config;
inline Status status;
inline std::optional<Config> pending;
inline std::atomic<bool> select_tab{false},disconnect_requested{false};
inline Config Normalize(Config value) {
    value.room_port=std::clamp(value.room_port,1,65535);
    value.discovery_port=std::clamp(value.discovery_port,1,65535);
    value.max_players=std::clamp(value.max_players,2,16);
    return value;
}
inline void Publish(Config value,Status state) {
    std::scoped_lock lock(mutex);config=std::move(value);status=std::move(state);
}
inline auto Get() {
    std::scoped_lock lock(mutex);
    return std::pair{pending.value_or(config),status};
}
inline void Edit(Config value) {
    std::scoped_lock lock(mutex);pending=Normalize(std::move(value));
}
inline std::optional<Config> TakeEdit() {
    std::scoped_lock lock(mutex);auto value=std::move(pending);pending.reset();return value;
}
void Draw();
// Authored name layout controls, exposed only in the F1 HUD editor.
void DrawNameSettings();
}
