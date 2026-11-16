// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/name_keyboard.h"
#include <atomic>
#include <deque>
#include <mutex>

namespace OOT3D::NameTyping {
constexpr unsigned Backspace=0x110000,Confirm=0x110001;
inline std::atomic<bool> available{false};
inline std::mutex mutex;
inline std::deque<unsigned> input;
inline void Publish(bool active) {
    std::scoped_lock lock(mutex);available=active;
    if(!active)input.clear();
}
inline void Clear() {std::scoped_lock lock(mutex);input.clear();}
inline void Push(unsigned code) {
    std::scoped_lock lock(mutex);
    if(available.load() && input.size()<64)input.push_back(code);
}
inline std::optional<unsigned> Pop() {
    std::scoped_lock lock(mutex);
    if(input.empty())return {};
    const auto code=input.front();input.pop_front();return code;
}
struct Hook {
    std::optional<unsigned> buttons;
    template<class M> void Frame(M& m,bool supported) {
        using namespace NameKeyboard;
        buttons.reset();
        const auto mode=m.Read(Owner+8).value_or(0);
        const bool active=supported && m.Read(Owner+4)==4 && (mode==2 || mode==3) &&
            m.Read(Owner+0x40).value_or(0) && !Keys(m).empty();
        Publish(active);if(!active)return;
        const auto code=Pop();if(!code)return;
        if(*code==Backspace) {
            // Native B cancels when empty. A text-field Backspace must not.
            buttons=m.Read(Owner+0x24).value_or(0)>0?2:0;
        } else if(*code==Confirm) {
            m.Write(Owner+0x14,5);m.Write(Owner+0x10,1);m.Write(Owner+0x28,1);
            buttons=1; // Native OK validation, sound and fade/save flow.
        } else {
            const auto caps=m.Read(Owner+0x20).value_or(0);
            const std::array<std::array<unsigned,2>,5> pages{{{mode,caps},{2,0},{2,1},{2,2},{3,0}}};
            for(const auto& page:pages) {
                for(const auto& key:Keys(m,{},page[0],page[1])) {
                    if(key.command || key.text.size()!=1 || key.text[0]!=*code)continue;
                    m.Write(Owner+8,page[0]);m.Write(Owner+0x20,page[1]);
                    m.Write(Owner+0x14,static_cast<unsigned>(key.row));
                    m.Write(Owner+0x10,static_cast<unsigned>(key.column));
                    buttons=1;break;
                }
                if(buttons)break;
            }
        }
        if(buttons)m.Write(Owner+0x30,~0u); // Discard an older touch action.
    }
};
}
