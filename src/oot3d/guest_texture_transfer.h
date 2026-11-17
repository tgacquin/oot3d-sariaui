// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"
#include <span>

namespace OOT3D::GuestUI {
// Preserve full validation while allowing page-wise synchronization and copies.
template<class M> bool ReadTextureWords(M& m, TitleScreen::U32 address,
                                      std::span<TitleScreen::U32> words) {
    if constexpr(requires { m.ReadWords(address, words); }) {
        return m.ReadWords(address, words);
    } else {
        for(unsigned i=0;i<words.size();++i) {
            const auto value=m.Read(address+4*i);
            if(!value)return false;
            words[i]=*value;
        }
        return true;
    }
}
template<class M> bool WriteTextureWords(M& m, TitleScreen::U32 address,
                                       std::span<const TitleScreen::U32> words) {
    if constexpr(requires { m.WriteWords(address, words); }) {
        return m.WriteWords(address, words);
    } else {
        // Reject an incomplete mapping before writing any pixels.
        for(unsigned i=0;i<words.size();++i)if(!m.Read(address+4*i))return false;
        for(unsigned i=0;i<words.size();++i)m.Write(address+4*i,words[i]);
        return true;
    }
}
}
