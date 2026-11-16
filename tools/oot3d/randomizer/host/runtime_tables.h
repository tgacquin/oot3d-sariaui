// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <bit>
#include "entrance.hpp"
#include "gold_skulltulas.hpp"
#include "music.hpp"
#include "sound_effects.hpp"
#include "ocarina_notes.hpp"

// Host compilers use different enum sizes/padding from ARM. Write the wire
// format explicitly; never copy host structs with unspecified padding.
struct ArmBytes {
    QByteArray bytes;
    void u8(unsigned v) { bytes += char(v); }
    void u16(unsigned v) { u8(v); u8(v>>8); }
    void u32(std::uint32_t v) { u16(v); u16(v>>16); }
    void f32(float v) { u32(std::bit_cast<std::uint32_t>(v)); }
    void align(unsigned n) { while (bytes.size()%n) u8(0); }
};
template<class Put> QJsonArray RuntimeTables(Put put) {
    const auto write = [&](const char* name, ArmBytes& buffer) {
        const auto symbol=OOT3D::Randomizer::Data().Find(name);
        if (buffer.bytes.size()>symbol.size) throw std::runtime_error(std::string("Too much runtime data: ")+name);
        // Empty tails provide the native table sentinels, including unshuffled seeds.
        buffer.bytes.append(QByteArray(symbol.size-buffer.bytes.size(),0));
        put(name,buffer.bytes.data(),buffer.bytes.size());
    };
    ArmBytes entrances;
    for(const auto& e:entranceOverrides) {
        entrances.u16(e.index);entrances.u16(e.destination);entrances.u16(e.blueWarp);
        entrances.u16(e.override);entrances.u16(e.overrideDestination);
    }
    write("rEntranceOverrides",entrances);
    ArmBytes enemies;
    std::vector<EnemyOverride> enemyOverrides;
    if(Settings::Enemizer)Enemizer::FillPatchOverrides(enemyOverrides);
    for(const auto& e:enemyOverrides) {
        enemies.u32(e.key);enemies.u8(e.enemyId);enemies.u8(e.paramsIdx);enemies.u8(e.vanillaId);enemies.u8(0);
    }
    write("rEnemyOverrides",enemies);
    ArmBytes skulltulas;
    for(const auto& g:*Gs_GetOverrideData()) {
        skulltulas.u8(g.arrayIndex);skulltulas.u8(g.bitFlag);skulltulas.u16(g.scene);skulltulas.u8(g.room);
        skulltulas.u8(g.ageCondition);skulltulas.u8(g.timeCondition);skulltulas.u8(0);
        skulltulas.f32(g.posRot.pos.x);skulltulas.f32(g.posRot.pos.y);skulltulas.f32(g.posRot.pos.z);
        skulltulas.u16(g.posRot.rot.x);skulltulas.u16(g.posRot.rot.y);skulltulas.u16(g.posRot.rot.z);skulltulas.u16(0);
    }
    write("rGsLocOverrides",skulltulas);
    ArmBytes music;
    for(unsigned i=0;i<128;++i)music.u32(i<Music::seqOverridesMusic.size()?Music::seqOverridesMusic[i]:Music::BGM_BASE+i);
    write("rBGMOverrides",music);
    ArmBytes sfx;
    const auto& sound=SFX::GetSFXData();
    for(auto v:sound.rSeqCounts)sfx.u16(v);
    for(auto v:sound.rSeqTypesSFX)sfx.u8(v);
    sfx.align(4);
    for(auto v:sound.rSFXOverrides_All)sfx.u32(v);
    sfx.u16(sound.rSFXOverrides_TrimmedCount);sfx.align(4);
    for(auto v:sound.rSFXOverrides_AllTrimmed)sfx.u32(v);
    for(const auto& group:sound.rSFXOverrides_Types)for(auto v:group)sfx.u32(v);
    write("rSfxData",sfx);
    QJsonArray native;
    const auto patch=[&](unsigned address,const QByteArray& bytes) {
        native.append(QJsonObject{{"address",int(address)},{"data",QString::fromLatin1(bytes.toBase64())}});
    };
    ArmBytes prices, texts;
    const std::array<unsigned,11> textIds{0x9014,0x900f,0x900a,0x9028,0x9032,0x9028,0x9046,0x9028,0x9028,0x9028,0x9028};
    for(auto id:textIds) {
        unsigned price=0;
        if(Settings::Scrubsanity.Is(SCRUBSANITY_RANDOM_PRICES)) {price=GetRandomScrubPrice();id=0x9000+price;}
        else if(Settings::Scrubsanity.Is(SCRUBSANITY_AFFORDABLE))id=0x900a;
        prices.u16(price);texts.u16(id);
    }
    write("rScrubRandomItemPrices",prices);
    if(Settings::Scrubsanity.IsNot(SCRUBSANITY_OFF))patch(0x52236c,texts.bytes);
    if(Settings::RandomSongNotes) {
        patch(0x54c222,QByteArray(reinterpret_cast<const char*>(OcarinaNotes::SongData),sizeof(OcarinaNotes::SongData)));
        patch(0x54c8b0,QByteArray(reinterpret_cast<const char*>(OcarinaNotes::FrogSongNotes),sizeof(OcarinaNotes::FrogSongNotes)));
    }
    return native;
}
