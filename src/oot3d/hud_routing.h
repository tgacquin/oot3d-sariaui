// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <bit>
#include <atomic>
#include <chrono>

namespace OOT3D::HUD {
// Identify the actual native upload, including UVs and colors, not merely an
// atlas shared by other menus. Keep a bounded history for queued GPU frames.
struct RoutedMesh {
    std::uint64_t hash{}; unsigned quads{}; bool routed{};
    std::uint64_t geometry_hash{}; bool hd_counter{};
    std::array<std::uint64_t,3> attributes{};
};
namespace RoutingCapture {
inline std::atomic<std::int64_t> begins{},ends{};
inline std::atomic<unsigned> lines{};
inline std::int64_t Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void Start() {lines=0;begins=Now()+3000;ends=begins.load()+20000;}
inline bool Active() {const auto t=Now();return t>=begins.load() && t<ends.load();}
inline bool Take() {return Active() && lines.fetch_add(1)<6000;}
}
inline std::mutex routing_mutex;
inline std::array<RoutedMesh, 32> routed_meshes{};
inline RoutedMesh latest_main_mesh{};
inline RoutedMesh LatestMainMesh() {std::scoped_lock lock{routing_mutex};return latest_main_mesh;}
inline std::array<RoutedMesh,32> timer_meshes{};
inline unsigned timer_cursor{};
enum class NumberKind { Ammo, Score, Skulltula };
struct NumberMesh {RoutedMesh mesh;NumberKind kind;};
inline std::array<NumberMesh,64> number_meshes{};
inline unsigned number_cursor{};
inline void PublishNumberMesh(RoutedMesh mesh,NumberKind kind) {
    if(!mesh.hash || !mesh.quads || mesh.quads>8)return;
    std::scoped_lock lock{routing_mutex};
    for(auto& old:number_meshes)if(old.mesh.hash==mesh.hash && old.mesh.quads==mesh.quads) {
        old={mesh,kind};return;
    }
    number_meshes[number_cursor++%number_meshes.size()]={mesh,kind};
}
inline std::optional<NumberKind> FindNumberMesh(std::uint64_t hash,unsigned quads) {
    std::scoped_lock lock{routing_mutex};
    for(const auto& entry:number_meshes)
        if(entry.mesh.hash==hash && entry.mesh.quads==quads && quads)return entry.kind;
    return {};
}
inline void PublishTimerMesh(RoutedMesh mesh) {
    std::scoped_lock lock{routing_mutex};
    for(auto& old:timer_meshes)if(old.hash==mesh.hash && old.quads==mesh.quads){old=mesh;return;}
    timer_meshes[timer_cursor++%timer_meshes.size()]=mesh;
}
inline bool IsTimerMesh(std::uint64_t hash,unsigned quads) {
    std::scoped_lock lock{routing_mutex};
    for(const auto& mesh:timer_meshes)if(mesh.hash==hash && mesh.quads==quads)return true;
    return false;
}
inline std::uint64_t HashWord(std::uint64_t hash, std::uint32_t word) {
    return (hash ^ word) * 1099511628211ULL;
}
constexpr std::uint64_t HashStart = 14695981039346656037ULL;
template<class G> void PublishNumberGeometry(const G& g,NumberKind kind) {
    if(!g.count)return;
    auto hash=HashStart;
    const auto add=[&](const auto& a){for(float f:a)hash=HashWord(hash,std::bit_cast<std::uint32_t>(f));};
    // GuestUI uploads its full fixed-capacity arrays, including empty quads.
    add(g.positions);add(g.uvs);const auto geometry=hash;add(g.colors);
    PublishNumberMesh({hash,unsigned(g.positions.size()/12),false,geometry},kind);
}
inline void PublishMesh(RoutedMesh mesh) {
    std::scoped_lock lock{routing_mutex};
    if(mesh.quads==92)latest_main_mesh=mesh;
    // Refresh recency even when the main HUD is unchanged. Otherwise changing
    // rupee digits can evict the main atlas immediately after it was published,
    // allowing its bottom-screen quads through the next top-screen draw.
    auto end=std::find_if(routed_meshes.begin(),routed_meshes.end(),[&](const auto& old) {
        return old.hash==mesh.hash && old.quads==mesh.quads;
    });
    if(end==routed_meshes.end())--end;
    std::move_backward(routed_meshes.begin(),end,end+1);
    routed_meshes.front()=mesh;
}
inline bool IsRouted(std::uint64_t hash, unsigned quads) {
    std::scoped_lock lock{routing_mutex};
    for (const auto& mesh : routed_meshes)
        if (mesh.hash == hash && mesh.quads == quads) return mesh.routed;
    return false;
}
inline bool IsHDCounterMesh(std::uint64_t hash,std::uint64_t geometry_hash,unsigned quads) {
    if(!geometry_hash || !quads || quads>4)return false;
    std::scoped_lock lock{routing_mutex};
    for(const auto& mesh:routed_meshes)
        if(mesh.hash==hash && mesh.quads==quads)return mesh.routed && mesh.hd_counter;
    bool found=false;
    for(const auto& mesh:routed_meshes)if(mesh.geometry_hash==geometry_hash && mesh.quads==quads) {
        if(!mesh.routed || !mesh.hd_counter)return false;
        found=true;
    }
    return found;
}
// Blinking rupees and menu fades can change RGBA independently of geometry.
// A color-only difference must not expose all bottom-atlas triangles in the
// top draw. Still require an exact XYZ+UV match to an owned, queued HUD mesh;
// triangle counts or shared textures alone are never sufficient.
inline bool IsRoutedGeometry(std::uint64_t geometry_hash,unsigned quads) {
    if(!geometry_hash)return false;
    std::scoped_lock lock{routing_mutex};
    bool routed=false;
    for(const auto& mesh:routed_meshes)if(mesh.geometry_hash==geometry_hash && mesh.quads==quads) {
        if(!mesh.routed)return false;
        routed=true;
    }
    return routed;
}
inline bool TopQuad(unsigned quads, unsigned quad) {
    return quads <= 4 || quad == 26 || quad == 27 || (quad >= 59 && quad < 83) ||
           (quad >= 86 && quad < 92);
}
inline bool KeepTriangle(unsigned quads, unsigned vertex, bool top) {
    return TopQuad(quads, vertex / 4) == top;
}
} // namespace OOT3D::HUD
