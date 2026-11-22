// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace OOT3D::GyroAim {
inline std::atomic<bool> enabled{false};

// Sample the configured motion device on the existing pad poll. Intermediate
// views can follow the timing of the player's turn while both ends of the
// interval still agree with the game's aim, including its sensitivity/clamps.
class History {
    struct Reading {
        std::int64_t time{};
        std::array<float,3> rate{};
    };
    std::array<Reading,32> readings{};
    unsigned next{},size{},epoch{};
    std::mutex mutex;
public:
    void Reset() {std::scoped_lock lock{mutex};size=next=0;}
    void Add(unsigned generation,std::int64_t time,std::array<float,3> rate) {
        std::scoped_lock lock{mutex};
        if(epoch!=generation || (size && time<=readings[(next+31)%32].time))size=next=0;
        epoch=generation;
        for(float value:rate)if(!std::isfinite(value) || std::abs(value)>2100){size=next=0;return;}
        readings[next]={time,rate};
        next=(next+1)%readings.size();size=std::min<unsigned>(size+1,readings.size());
    }
    float RotationPhase(unsigned generation,std::int64_t start,std::int64_t end,float phase) {
        if(phase<=0 || phase>=1 || end-start<20000 || end-start>50000)return phase;
        std::scoped_lock lock{mutex};
        if(generation!=epoch || size<2)return phase;
        // Read up to one 120 Hz display interval ahead of ordinary replay.
        // Taper to the exact native endpoint, where arrows and hits resolve.
        const double cursor=start+(end-start)*phase+8333.*(1-phase);
        std::array<double,3> part{},whole{};
        std::int64_t covered=start;
        double path=0;
        for(unsigned i=0;i<size && covered<end;++i) {
            const unsigned slot=(next+readings.size()-size+i)%readings.size();
            const auto& sample=readings[slot];
            const auto until=i+1<size?readings[(slot+1)%readings.size()].time:end;
            if(until<=start)continue;
            if(sample.time>covered || covered-sample.time>10000)return phase;
            const auto stop=std::min(end,until);
            if(stop-sample.time>10000)return phase;
            const double dt=(stop-covered)*1e-6;
            const double early=std::clamp(cursor-double(covered),0.,double(stop-covered))*1e-6;
            for(unsigned axis=0;axis<3;++axis) {
                whole[axis]+=dt*sample.rate[axis];
                part[axis]+=early*sample.rate[axis];
            }
            path+=dt*std::hypot(sample.rate[0],sample.rate[1],sample.rate[2]);
            covered=stop;
        }
        const double angle=std::hypot(whole[0],whole[1],whole[2]);
        if(covered!=end || angle<.07 || angle<.85*path)return phase;
        double progress=0;
        for(unsigned i=0;i<3;++i)progress+=part[i]*whole[i];
        return static_cast<float>(std::clamp(progress/(angle*angle),0.,1.));
    }
};
inline History history;
}
