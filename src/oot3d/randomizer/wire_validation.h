// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/randomizer/service.h"
#include "../../../externals/oot3d-randomizer/shared/s_settings.h"
#include <cmath>
#include <cstring>
#include <set>

namespace OOT3D::Randomizer {
inline bool ValidExpandedPackage(const Package& package) {
    const auto& data=Data();
    const auto bytes=[&](const char* name){const auto s=data.Find(name);return package.payload.mid(s.address-GuestBase,s.size);};
    const auto settings=bytes("gSettingsContext");
    if(settings.size()!=sizeof(SettingsContext))return false;
    SettingsContext rules;std::memcpy(&rules,settings.data(),sizeof(rules));
    if(rules.mqDungeonCount!=(package.mq?12:0) || rules.randomMQDungeons)return false;
    for(auto mode:rules.dungeonModes)if(mode!=(package.mq?1:0))return false;
    const auto read=[](const QByteArray& b,unsigned offset,unsigned count) {
        std::uint32_t v=0;for(unsigned j=0;j<count;++j)v|=std::uint32_t(std::uint8_t(b[offset+j]))<<(8*j);return v;
    };
    const auto entrance=[](unsigned index){return index<0x614 || (index>=0x700 && index<0x721) || (index>=0x800 && index<0x821);};
    const auto entries=bytes("rEntranceOverrides");
    std::set<unsigned> seen;
    for(int i=0;i<entries.size();i+=10) {
        if(entries.mid(i,10)==QByteArray(10,0))break;
        const auto index=read(entries,i,2),warp=read(entries,i+4,2),dest=read(entries,i+6,2);
        if(!entrance(index) || !entrance(dest) || warp>=0x614 || !seen.insert(index).second)return false;
    }
    const auto enemies=bytes("rEnemyOverrides");
    bool end=false;std::int32_t previous=-1;
    for(int i=0;i<enemies.size();i+=8) {
        const auto key=std::int32_t(read(enemies,i,4)),id=int(read(enemies,i+4,1));
        if(!id){end=true;break;}
        if(key<=previous || id>=ENEMY_MAX || read(enemies,i+6,1)>=ENEMY_MAX)return false;
        previous=key;
    }
    if(!end)return false;
    const auto skulls=bytes("rGsLocOverrides");
    for(int i=0;i<skulls.size();i+=28) {
        const auto bit=read(skulls,i+1,1);if(!bit)break;
        if((bit&(bit-1)) || read(skulls,i,1)>=22 || read(skulls,i+5,1)>2 || read(skulls,i+6,1)>1)return false;
        for(unsigned axis=0;axis<3;++axis) {
            const auto word=read(skulls,i+8+4*axis,4);float position;std::memcpy(&position,&word,4);
            if(!std::isfinite(position))return false;
        }
    }
    const auto sound=bytes("rSfxData");
    if(sound.size()!=23352)return false;
    for(unsigned i=0;i<27;++i)if(read(sound,i*2,2)>100)return false;
    if(read(sound,6996,2)>1388)return false;
    bool scrub=false,songs=false,frog=false;
    for(const auto& patch:package.native_patches) {
        if(patch.address==0x52236c)scrub=true;
        if(patch.address==0x54c222) {
            songs=true;
            for(unsigned i=0;i<12;++i) {
                const auto count=read(patch.bytes,9*i,1);
                if(!count || count>8)return false;
                for(unsigned j=0;j<count;++j)if(read(patch.bytes,9*i+1+j,1)>4)return false;
            }
        }
        if(patch.address==0x54c8b0) {frog=true;for(auto b:patch.bytes)if(std::uint8_t(b)>4)return false;}
    }
    return scrub==bool(rules.scrubsanity) && songs==bool(rules.randomSongNotes) && frog==bool(rules.randomSongNotes);
}
}
