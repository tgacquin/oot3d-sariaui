// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <cstdint>

namespace OOT3D::Randomizer::FileStatus {
enum class Error {None,Missing,Incompatible,Placement,Generation,Timeout,Settings,Generator,
                  Storage,Memory,Prices,Unsupported,Connection,Count};
inline std::string_view ErrorLabel(Error error,unsigned language=1) {
    constexpr std::string_view labels[][3]={
        {"","",""},
        {"Randomizer data missing.","Données aléatoires manquantes.","Faltan datos del randomizador."},
        {"Randomizer data incompatible.","Données aléatoires incompatibles.","Datos del randomizador incompatibles."},
        {"No valid placement. Try another seed.","Placement impossible. Changez de graine.","Sin reparto válido. Cambia de semilla."},
        {"Generation failed. Check the log.","Échec de génération. Voir le journal.","Falló la generación. Consulta el registro."},
        {"Generation timed out. Try again.","Délai de génération dépassé. Réessayez.","Se agotó el tiempo. Inténtalo de nuevo."},
        {"Conflicting settings. Review options.","Réglages incompatibles. Vérifiez-les.","Ajustes incompatibles. Revísalos."},
        {"Generator unavailable. Check the log.","Générateur indisponible. Voir le journal.","Generador no disponible. Ver registro."},
        {"Cannot save. Check disk access.","Sauvegarde impossible. Voir le journal.","No se pudo guardar. Revisa el disco."},
        {"Memory unavailable. Restart the game.","Mémoire indisponible. Relancez le jeu.","Memoria no disponible. Reinicia el juego."},
        {"Disable custom prices.","Désactivez les prix modifiés.","Desactiva los precios modificados."},
        {"Unsupported game version.","Version du jeu non compatible.","Versión del juego no compatible."},
        {"Connection failed. Retry via F1.","Échec de connexion. Réessayez via F1.","Falló la conexión. Reintenta desde F1."},
    };
    static_assert(std::size(labels)==static_cast<unsigned>(Error::Count));
    const auto i=static_cast<unsigned>(error);
    return labels[i<std::size(labels)?i:0][language==4 || language==5?1:language==6 || language==7?2:0];
}
// Immutable error events cross from worker threads to the native file menu.
// Repeated failures get new events even when their text is identical.
struct Notice {
    Error error;
    std::shared_ptr<const std::atomic<bool>> cancellation;
    bool Cancelled() const {return cancellation && cancellation->load();}
};
inline Common::AtomicSharedPtr<const Notice> notice;
inline void Report(Error error,std::shared_ptr<const std::atomic<bool>> cancellation={}) {
    notice.store(std::make_shared<const Notice>(Notice{error,std::move(cancellation)}));
}
inline auto LatestNotice() {return notice.load();}
inline Error GeneratorError(std::string_view detail) {
    if(detail.find("No beatable placement")!=detail.npos)return Error::Placement;
    if(detail.find("Triforce pieces required")!=detail.npos || detail.find("Adult start requires")!=detail.npos ||
       detail.find("cannot be excluded")!=detail.npos || detail.find("Conflicting goals")!=detail.npos ||
       detail.find("Master Quest logic")!=detail.npos || detail.find("Invalid difficulty")!=detail.npos)return Error::Settings;
    return Error::Generation;
}
// Presentation snapshots: no disk access or Qt dependency in the native draw.
inline std::atomic<bool> enabled{false};
struct Selection {unsigned slot=3;std::string seed,game_id;};
inline Common::AtomicSharedPtr<const Selection> selection;
inline void Publish(unsigned slot=3,std::string seed={},std::string game_id={}) {
    selection.store(std::make_shared<const Selection>(Selection{slot,std::move(seed),std::move(game_id)}));
}
inline std::string ShortRoomId(unsigned slot) {
    const auto value=selection.load();
    if(!value || value->slot!=slot || value->game_id.size()!=36)return {};
    auto id=value->game_id.substr(0,8);
    for(auto& c:id)if(c>='a' && c<='f')c-=('a'-'A');
    return id;
}
inline std::string Seed(unsigned slot) {
    const auto value=selection.load();
    return value && value->slot==slot?value->seed:std::string{};
}
}
