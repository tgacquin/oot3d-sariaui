// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/audio_settings.h"
#include "oot3d/title_hooks.h"

namespace OOT3D::Music {
using TitleScreen::U32;
// USA 1.0 BasicSound::UpdateParam: final gain command, VSTR s19,[r5,#0x14].
// Only the outgoing command changes. Native playback and fades keep advancing,
// so unmuting restores the current gain without restarting the track.
constexpr U32 PC=0x404274, Word=0xedc59a05, SVC=0xfff110;
constexpr U32 SequenceVTable=0x4ec72c, StreamVTable=0x4ec6e0;
constexpr std::array<TitleScreen::Site,7> Signatures{{
    {0x404114,0xe92d41f0,0xe1a04000},
    {0x404268,0xe1a00004,0xe12fff31},
    {0x404270,0xe5850010,Word},
    {PC,Word,0xed85ba06},
    // SoundArchivePlayer assigns the archive ID to BasicSound +9c.
    {0x30cee8,0xe1a00009,0xe580109c},
    {0x30d030,0xe1a00007,0xe580109c},
    {0x30b03c,0xe3e00000,0xe584009c},
}};
inline U32 Instruction(U32 pc,U32 word) {
    return pc==PC && word==Word ? 0xef000000|SVC : word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Signatures)
        if(m.Read(s.pc)!=std::optional<U32>{s.instruction} ||
           m.Read(s.pc+4)!=std::optional<U32>{s.following})return false;
    return m.Read(0x48a858)==std::optional<U32>{StreamVTable} &&
           m.Read(SequenceVTable+0x2c)==std::optional<U32>{0x404114};
}
// QueenSound archive IDs. Preserve demo sound effects, item/goal fanfares,
// song demonstrations and playable ocarina notes. Include all Field layers.
inline bool IsSequenceMusic(U32 id) {
    if(id<0x01000585 || id>0x010005ef)return false;
    switch(id & 0xffff) {
    case 0x58e: case 0x58f: case 0x590: case 0x592: case 0x593:
    case 0x599: case 0x59f: case 0x5a0: case 0x5a1: case 0x5a2:
    case 0x5a3: case 0x5a4: case 0x5a6: case 0x5a8: case 0x5aa:
    case 0x5ae: case 0x5b0: case 0x5b1: case 0x5b2: case 0x5b3:
    case 0x5b4: case 0x5b5: case 0x5b6: case 0x5c0: case 0x5c6:
    case 0x5ca:
        return false;
    default:return true;
    }
}
template<class M> bool IsMusicSound(M& m,U32 sound) {
    if(!TitleScreen::Pointer(sound,0xa0))return false;
    const auto type=m.Read(sound),id=m.Read(sound+0x9c);
    if(!type || !id)return false;
    if(*type==SequenceVTable)return IsSequenceMusic(*id);
    // QueenStream contains only the credits music. Its ID overlaps a sound
    // effect in QueenSound, so the stream type must be checked separately.
    return *type==StreamVTable && *id==0x01000000;
}
template<class M> void Store(M& m,U32 sound,U32 command,U32 gain) {
    m.Write(command+0x14,(AudioSettings::mute_music.load() && IsMusicSound(m,sound))?0:gain);
}
}
