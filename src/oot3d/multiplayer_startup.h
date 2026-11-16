// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <atomic>
#include <memory>
#include <string>
namespace OOT3D::MultiplayerStartup {
struct Session {unsigned role=0;std::string nickname;}; // 1 host, 2 guest.
inline Common::AtomicSharedPtr<const Session> session;
inline auto GetSession() {return session.load();}
inline void SetSession(unsigned role,const std::string& nickname={}) {
    const auto previous=GetSession();
    if(previous && previous->role==role && previous->nickname==nickname)return;
    session.store(std::make_shared<const Session>(Session{role,nickname}));
}
enum class Phase {Idle,Connecting,Syncing,Ready,Failed};
inline std::atomic<Phase> phase{Phase::Idle};
inline std::atomic<unsigned> generation{0};
inline void Begin(bool multiplayer) {
    const auto s=GetSession();
    phase=multiplayer && s && s->role?Phase::Connecting:Phase::Idle;
    ++generation;
}
inline bool Busy() {const auto p=phase.load();return p==Phase::Connecting || p==Phase::Syncing || p==Phase::Failed;}
}
