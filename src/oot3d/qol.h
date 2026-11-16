// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <cstdint>

namespace OOT3D::QOL {
inline std::atomic<bool> disable_navi{false};
inline std::atomic<bool> disable_heart_beep{false};
inline std::atomic<bool> fast_ladder{false};
inline std::atomic<bool> fast_loading{true};
inline std::atomic<bool> no_bombchu_first_person{false};
inline std::atomic<bool> hide_sheikah_stones{true};
inline std::atomic<bool> dirty{false};
inline std::atomic<unsigned> reset_epoch{0};
inline void Reset() {++reset_epoch;}

// Owned by the emulation thread. UI/state-load resets use an epoch so they
// never race with the edge detector. All times are emulated microseconds.
struct Taps {
    bool previous=false,pending=false,followup=false;
    std::int64_t polled=0;
    std::uint32_t owner=0;
    unsigned epoch=0;
    void Clear() {pending=followup=false;owner=0;}
    void Poll(bool pressed,bool eligible,std::uint32_t player,std::int64_t now) {
        const auto next=reset_epoch.load();
        if(next!=epoch || !eligible || player!=owner || now<polled || now-polled>100000) {
            Clear();previous=pressed;epoch=next;owner=eligible?player:0;polled=now;
            return;
        }
        const bool edge=pressed&&!previous;previous=pressed;polled=now;
        pending|=edge;
    }
    bool Ready(std::uint32_t player,std::int64_t now) const {
        return fast_ladder.load() && epoch==reset_epoch.load() && owner==player &&
            now>=polled && now-polled<=100000;
    }
    bool Boost(std::uint32_t player,std::int64_t now) {
        if(!Ready(player,now)) {Clear();return false;}
        // MM3D 0x1e5020: B's press edge sets a one-update latch. The
        // following update boosts and clears it, ignoring another edge.
        // Advance here at native climb cadence, not at HID polling cadence.
        const bool boost=followup || pending;
        followup=!followup && pending;
        pending=false;
        return boost;
    }
};
inline Taps taps;
}
