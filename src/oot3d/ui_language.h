// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <string_view>

namespace OOT3D::UILanguage {
// Snapshot at boot; changing the next-boot preference must not mix languages
// in an already-running game. These are CFG IDs, not QM language indices.
inline std::atomic<unsigned> active{1};
inline unsigned Index(unsigned language) {return language==2?1:language==5?2:0;}
inline std::string_view Pick(std::string_view en,std::string_view fr,std::string_view es) {
    return active==2?fr:active==5?es:en;
}
inline unsigned MessageLanguage() {return active==2?5:active==5?7:1;}
inline std::u16string_view Menu(unsigned language) {
    return language==2?u"05_US_FRENCH":language==5?u"07_US_SPANISH":u"01_US_ENGLISH";
}
inline std::string_view Suffix(unsigned language) {return language==2?"usfr":language==5?"ussp":"usen";}
inline auto Join() {return Pick("Join room","Rejoindre","Unirse");}
inline auto Host(bool stop) {return stop?Pick("Stop hosting","Fermer le salon","Cerrar sala"):
    Pick("Host room","Créer un salon","Crear sala");}
inline auto Randomizer(bool on) {return on?Pick("Randomizer on","Aléatoire : oui","Aleatorio: sí"):
    Pick("Randomizer off","Aléatoire : non","Aleatorio: no");}
}
