// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "oot3d/randomizer/activity_items.h"
#include "oot3d/randomizer/activity_phrases.h"
namespace OOT3D::Randomizer::Activity {
struct Event {std::string game;unsigned kind=0,item=0;}; // 0 item; 1 death.
struct Line {std::string text;std::chrono::steady_clock::time_point born;bool local=false;};
inline std::mutex mutex;
inline std::string game;
inline std::deque<Event> outgoing;
inline std::deque<Line> lines;
inline std::atomic<bool> connected{false};
inline std::atomic<bool> enabled{true},settings_dirty{false};
inline bool dead=false;
inline unsigned join_chimes=0;
inline bool TakeJoinChime() {
    std::scoped_lock lock(mutex);
    if(!connected.load() || !join_chimes)return false;
    --join_chimes;return true;
}
inline void SetGame(std::string id) {
    std::scoped_lock lock(mutex);
    if(id!=game) {game=std::move(id);outgoing.clear();lines.clear();dead=false;join_chimes=0;}
}
inline void Connection(bool value) {
    std::scoped_lock lock(mutex);
    connected=value;
    if(!value) {
        join_chimes=0;
        outgoing.clear();
        std::erase_if(lines,[](const Line& line){return !line.local;});
    }
}
// Local connection notices must survive the frontend's repeated disconnected
// polls, but expire with the ordinary feed and never travel over the network.
inline void Notice(std::string text) {
    const auto now=std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex);
    if(std::any_of(lines.begin(),lines.end(),[&](const Line& line) {
        return line.local && line.text==text && now-line.born<std::chrono::seconds(6);
    }))return;
    if(lines.size()>=5)lines.pop_front();
    lines.push_back({std::move(text),now,true});
}
inline void ConnectionLost(std::optional<std::string> host={}) {
    if(!host) {Notice("Connection to room lost.");return;}
    for(auto& c:*host)if(static_cast<unsigned char>(c)<32 || c==127)c=' ';
    if(host->empty() || host->size()>64)*host="The host";
    Notice(*host+" stopped hosting. You've been disconnected.");
}
inline void Emit(unsigned kind,unsigned item=0) {
    if(!connected.load() || kind>1 || (!kind && ItemName(item).empty()))return;
    std::scoped_lock lock(mutex);if(!connected.load() || game.empty())return;
    if(outgoing.size()>=32)outgoing.pop_front();
    outgoing.push_back({game,kind,item});
}
inline void ObserveDeath(bool value) {
    const bool rising=value && !dead;dead=value;
    if(rising)Emit(1);
}
inline std::vector<Event> TakeOutgoing() {
    std::scoped_lock lock(mutex);std::vector<Event> result(outgoing.begin(),outgoing.end());outgoing.clear();return result;
}
inline void Receive(const Event& event,std::string nickname) {
    if(event.kind>1 || (!event.kind && ItemName(event.item).empty()))return;
    for(auto& c:nickname)if(static_cast<unsigned char>(c)<32 || c==127)c=' ';
    if(nickname.empty() || nickname.size()>64)return;
    std::scoped_lock lock(mutex);if(!connected.load() || game.empty() || event.game!=game)return;
    const auto text=event.kind?nickname+std::string(UILanguage::Pick(" died."," a péri."," murió.")):
        nickname+std::string(UILanguage::Pick(" got "," a obtenu "," obtuvo "))+
        std::string(ItemPhrase(event.item))+std::string(UILanguage::Pick("!"," !","!"));
    if(lines.size()>=5)lines.pop_front();
    lines.push_back({text,std::chrono::steady_clock::now()});
}
inline void Presence(std::string nickname,bool joined) {
    for(auto& c:nickname)if(static_cast<unsigned char>(c)<32 || c==127)c=' ';
    if(nickname.empty() || nickname.size()>64)return;
    std::scoped_lock lock(mutex);
    if(!connected.load())return;
    if(joined)join_chimes=std::min(join_chimes+1,16u);
    if(lines.size()>=5)lines.pop_front();
    lines.push_back({nickname+(joined?" has joined!":" has left!"),std::chrono::steady_clock::now()});
}
inline void Joined(std::string nickname) {Presence(std::move(nickname),true);}
inline void Left(std::string nickname) {Presence(std::move(nickname),false);}
inline std::vector<Line> Visible() {
    const auto now=std::chrono::steady_clock::now();std::scoped_lock lock(mutex);
    while(!lines.empty() && now-lines.front().born>=std::chrono::seconds(6))lines.pop_front();
    return {lines.begin(),lines.end()};
}
}
