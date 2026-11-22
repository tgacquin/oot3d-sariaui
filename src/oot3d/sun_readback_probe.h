// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

namespace OOT3D::SunReadbackProbe {
using U32 = std::uint32_t;
using I64 = std::int64_t;
constexpr I64 Delay = 3'000'000, Duration = 10'000'000;
inline I64 Now() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
constexpr const char* Names[] = {"native-before", "bypass", "native-after"};
struct Stats {
    std::atomic<std::uint64_t> queries{}, bypasses{}, reads{}, read_us{};
    std::atomic<std::uint64_t> cached{}, pending{}, clear{}, submitted{}, skipped{};
    std::atomic<std::uint64_t> depth_downloads{}, depth_us{}, sun_downloads{}, sun_us{};
    std::atomic<std::uint64_t> other_downloads{}, other_us{};
    // Written only by the main presentation thread; read after reported=true.
    I64 first_host{}, last_host{}, first_guest{}, last_guest{};
    unsigned presentations{};
    int min_cap = 120, max_cap = 0, requested{};
    bool fresh_attempt{};
    int last_cap{};
    I64 target_host_us{}, target_guest_us{};
};
struct Session {
    const I64 begins;
    const bool optimized;
    std::array<Stats, 3> phases;
    std::atomic<bool> cancelled{}, reported{};
    explicit Session(I64 now, bool use_optimized) : begins(now + Delay), optimized(use_optimized) {}
    const char* Name(int phase) const {return phase == 1 && optimized ? "asynchronous" : Names[phase];}
    int Phase(I64 now) const {
        if(cancelled.load() || now < begins || now >= begins + 3 * Duration) return -1;
        return int((now - begins) / Duration);
    }
    bool Running(I64 now) const {return !cancelled.load() && now < begins + 3 * Duration;}
};
using SessionSlot = Common::AtomicSharedPtr<Session>;
inline SessionSlot current;
inline std::atomic<unsigned> installed{}, rejected{};
inline std::shared_ptr<Session> Start(I64 now = Now(), bool optimized = false) {
    auto session = std::make_shared<Session>(now, optimized);
    current.store(session);
    return session;
}
struct Ticket {
    std::shared_ptr<Session> session;
    int phase = -1;
    Stats* Get() const {return session && phase >= 0 ? &session->phases[phase] : nullptr;}
};
inline Ticket Take(I64 now = Now()) {
    auto session = current.load();
    return {session, session ? session->Phase(now) : -1};
}
// Actual submission to the presentation queue, independently of the cap and
// of replay samples that were dropped before acquiring a presentation frame.
inline void Submitted() {const auto ticket = Take();if(auto* s = ticket.Get()) ++s->submitted;}
// Renderer-thread only: report each phase once. The user's selected frame
// target stays fixed throughout; the probe does not change rate settings.
inline int BeginPhase(I64 now = Now()) {
    const auto ticket = Take(now);
    if(auto* stats = ticket.Get(); stats && !stats->fresh_attempt) {
        stats->fresh_attempt = true;
        return ticket.phase;
    }
    return -1;
}
// Only set around the exact guest LDR. Ordinary downloads remain attributable
// separately; no memory cache, scheduler or synchronization behavior is changed.
// Keep the owning ticket on the stack. Only the active scope's address needs TLS.
inline constinit thread_local const Ticket* sun_query = nullptr;
class ReadScope {
    const Ticket ticket;
    const Ticket* previous;

public:
    explicit ReadScope(const Ticket& query) : ticket(query), previous(sun_query) {
        sun_query = &ticket;
    }
    ~ReadScope() { sun_query = previous; }

    ReadScope(const ReadScope&) = delete;
    ReadScope& operator=(const ReadScope&) = delete;
};
struct DownloadScope {
    // Attribute all reads to the query's starting phase, even if a query
    // crosses the boundary while its first GPU download is in flight.
    Ticket ticket;
    bool depth{}, sun = sun_query != nullptr;
    I64 start = ticket.Get() ? Now() : 0;
    explicit DownloadScope(bool is_depth, I64 now = Now())
        : ticket(sun_query ? *sun_query : Take(now)), depth(is_depth) {}
    ~DownloadScope() {
        if(auto* stats = ticket.Get()) {
            const auto elapsed = Now() - start;
            if(depth) {++stats->depth_downloads;stats->depth_us += elapsed;}
            else {++stats->other_downloads;stats->other_us += elapsed;}
            if(sun) {++stats->sun_downloads;stats->sun_us += elapsed;}
        }
    }
};
inline void Present(I64 guest, int requested, int cap, unsigned passes, unsigned skipped = 0) {
    const auto now = Now();
    const auto ticket = Take(now);
    if(auto* stats = ticket.Get()) {
        stats->skipped += skipped;
        if(!stats->presentations) {stats->first_host = now;stats->first_guest = guest;}
        else if(stats->last_cap >= requested && cap >= requested && guest >= stats->last_guest) {
            stats->target_host_us += now - stats->last_host;
            stats->target_guest_us += guest - stats->last_guest;
        }
        stats->last_host = now;stats->last_guest = guest;
        stats->last_cap = cap;
        stats->presentations += passes;stats->requested = requested;
        stats->min_cap = std::min(stats->min_cap, cap);
        stats->max_cap = std::max(stats->max_cap, cap);
    }
}
// Called by the main presentation thread once after the bounded experiment.
// UI reads only atomics and never races the presentation fields above.
inline std::string Finish(I64 now = Now()) {
    const auto session = current.load();
    if(!session || session->Running(now) || session->reported.load()) return {};
    std::ostringstream out;
    out << "OoT sun-readback verification v3: installed=" << installed.load()
        << "/3 rejected=" << rejected.load() << " cancelled=" << session->cancelled.load();
    const auto& middle = session->phases[1];
    const bool verified = installed.load() == 3 && middle.bypasses.load() > 0 &&
        middle.queries.load() == middle.bypasses.load() && middle.reads.load() == 0 &&
        middle.sun_downloads.load() == 0;
    out << " bypass_verified=" << verified;
    if(session->optimized) out << " async_verified=" << (installed.load() == 3 &&
        middle.queries.load() > 0 && middle.cached.load() > 0 && middle.reads.load() == 0 &&
        middle.sun_downloads.load() == 0);
    for(unsigned i = 0; i < 3; ++i) {
        const auto& s = session->phases[i];
        const double seconds = (s.last_host - s.first_host) / 1e6;
        out << "\n" << session->Name(i) << ": queries=" << s.queries.load()
            << " bypassed=" << s.bypasses.load() << " native_reads=" << s.reads.load()
            << " native_read_ms=" << s.read_us.load() / 1000.
            << " cached_samples=" << s.cached.load() << " pending_samples=" << s.pending.load()
            << " clear_cached_samples=" << s.clear.load()
            << " submitted=" << s.submitted.load() << " submitted_fps_approx=" << s.submitted.load() / (Duration / 1e6)
            << " skipped_samples=" << s.skipped.load()
            << " sun_downloads=" << s.sun_downloads.load() << " sun_download_ms=" << s.sun_us.load() / 1000.
            << " all_depth_downloads=" << s.depth_downloads.load() << " all_depth_ms=" << s.depth_us.load() / 1000.
            << " other_downloads=" << s.other_downloads.load() << " other_download_ms=" << s.other_us.load() / 1000.
            << " observed_seconds=" << seconds
            << " guest_speed=" << (seconds > 0 ? (s.last_guest - s.first_guest) / 1e6 / seconds : 0)
            << " requested=" << s.requested << " cap=" << s.min_cap << ".." << s.max_cap
            << " fresh_attempt=" << s.fresh_attempt
            << " target_seconds=" << s.target_host_us / 1e6
            << " target_guest_speed=" << (s.target_host_us > 0 ? double(s.target_guest_us) / s.target_host_us : 0);
    }
    session->reported = true;
    return out.str();
}
}
