// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace OOT3D::Randomizer {struct RoomFile;}
namespace OOT3D::RoomJoin {
// File-menu session state. Never use the entered name as a save-file name or
// change the emulated console's identity. Discovery can consume this request
// once the native keyboard has been released and the waiting screen is ready.
struct Profile {std::string game_id;bool mq=false;};
inline std::array<Common::AtomicSharedPtr<const Profile>,3> profiles;
inline std::shared_ptr<const Profile> ProfileAt(unsigned slot) {
    return slot<3?profiles[slot].load():nullptr;
}
inline std::atomic<bool> hosting{false},stopping{false},profile_visible{false};
inline std::atomic<bool> host_opening{false},host_error{false};
inline std::atomic<unsigned> player_count{0}; // Joined room members, including ourselves.
inline std::atomic<std::int64_t> host_hop_started{-1000000};
inline bool HostPrompt() {return profile_visible.load() && !stopping.load() && (hosting.load() || host_opening.load() || host_error.load());}
enum class Intent {New,Resume,Host};
inline Intent intent=Intent::New;
inline std::string game_id;
enum class Phase {Idle,ToName,Naming,NameLeaving,ReturnToFiles,Waiting,Leaving,Reopen,ProfileLeaving,ProfileReturn};
inline std::atomic<Phase> phase{Phase::Idle};
inline std::atomic<float> opacity{0};
inline std::atomic<bool> leave_requested{false};
inline std::atomic<unsigned> entrance_slot{3};
inline std::atomic<bool> success_chime{false};
enum class Guest {None,Waiting,Disconnecting,Returning,Starting};
inline std::atomic<Guest> guest{Guest::None};
inline std::atomic<bool> leave_complete{false},host_started{false},remote_started{false};
inline bool GuestActive() {return guest.load()!=Guest::None;}
inline bool GuestPrompt() {return GuestActive() && guest.load()!=Guest::Starting;}
inline void Disconnect() {
    leave_complete=false;remote_started=false;guest=Guest::Disconnecting;leave_requested=true;
}
enum class Status : unsigned {Searching,Connecting,Connected,Failed,NameUnavailable,IdentityConflict,AlreadyConnected,
    Receiving,ReadyToSave,Saving,HostUnavailable,InvalidFile,SaveFailed,SlotOccupied,SavedDisconnected,WaitingHost,Disconnecting,Hosting,Matching,HostFailed,OpeningHost,Count};
struct Request {
    unsigned slot;bool master_quest;std::u16string name;
    Intent intent;std::string game_id;bool inline_host=false;
    mutable std::atomic<Status> status{Status::Searching};
    mutable std::atomic<bool> cancelled{false},accepted{false};
    mutable std::atomic<bool> committing{false};
    mutable std::atomic<bool> connection_lost{false},saved{false};
    mutable Common::AtomicSharedPtr<const Randomizer::RoomFile> file;
    Request(unsigned s,bool mq,std::u16string n,Intent i=Intent::New,std::string id={},bool h=false):slot(s),master_quest(mq),name(std::move(n)),intent(i),game_id(std::move(id)),inline_host(h) {}
};
inline Common::AtomicSharedPtr<const Request> request;
inline std::atomic<Status> leaving_status{Status::Searching};
inline Status CurrentStatus() {
    if(HostPrompt())return host_error.load()?Status::HostFailed:Status::Hosting;
    if(GuestPrompt())return guest.load()==Guest::Waiting?Status::WaitingHost:Status::Disconnecting;
    const auto r=request.load();
    return r?(r->status==Status::Searching && r->intent==Intent::Resume?Status::Matching:
        r->status==Status::Connecting && r->intent==Intent::Host?Status::OpeningHost:r->status.load()):leaving_status.load();
}
inline bool Searching() {const auto s=CurrentStatus();return s==Status::Searching || s==Status::Connecting ||
    s==Status::Receiving || s==Status::ReadyToSave || s==Status::Saving || s==Status::Connected || s==Status::WaitingHost || s==Status::Hosting || s==Status::Matching || s==Status::OpeningHost;}
inline bool CanCancel() {
    const auto r=request.load();
    return !r || (!r->committing.load() && !(r->saved.load() &&
        (r->status==Status::Connected || r->status==Status::SavedDisconnected)));
}
inline std::string_view Label(Status s,unsigned language=1) {
    if(s==Status::Receiving || s==Status::ReadyToSave || s==Status::Saving || s==Status::Connected)
        s=Status::Connecting; // One continuous joining prompt, without save/success interstitials.
    constexpr std::string_view labels[][3]={
        {"Waiting for rooms...","Recherche de salons...","Esperando salas..."},
        {"Joining room...","Connexion au salon...","Entrando en la sala..."},
        {"Connected!","Connecté !","¡Conectado!"},
        {"Could not join. Please try again.","Connexion impossible. Réessayez.","No se pudo conectar. Inténtalo de nuevo."},
        {"Name unavailable. Please choose another.","Nom indisponible. Choisissez-en un autre.","Nombre no disponible. Elige otro."},
        {"Console identity already in use.","Identité de console déjà utilisée.","Identidad de consola ya en uso."},
        {"Already in a room. Leave it first.","Quittez d'abord le salon actuel.","Sal primero de la sala actual."},
        {"Receiving file...","Réception du fichier...","Recibiendo archivo..."},
        {"Saving file...","Sauvegarde du fichier...","Guardando archivo..."},
        {"Saving file...","Sauvegarde du fichier...","Guardando archivo..."},
        {"Host file unavailable. Please try again.","Fichier hôte indisponible. Réessayez.","Archivo del anfitrión no disponible."},
        {"Incompatible or damaged host file.","Fichier hôte incompatible ou endommagé.","Archivo incompatible o dañado."},
        {"Could not save the received file.","Impossible de sauvegarder le fichier.","No se pudo guardar el archivo."},
        {"This file slot is no longer empty.","Cet emplacement n'est plus vide.","Este espacio ya no está vacío."},
        {"File saved. Room disconnected.","Fichier sauvegardé. Salon déconnecté.","Archivo guardado. Sala desconectada."},
        {"Waiting for host to start...","En attente du départ de l'hôte...","Esperando a que el anfitrión inicie..."},
        {"Disconnecting...","Déconnexion...","Desconectando..."},
        {"Waiting for players to join...","En attente de joueurs...","Esperando a otros jugadores..."},
        {"Looking for matching room ID...","Recherche d'un ID de salon correspondant...","Buscando un ID de sala coincidente..."},
        { "Could not host. Please try again.","Création du salon impossible.","No se pudo crear la sala."},
        {"Opening room...","Ouverture du salon...","Abriendo sala..."},
    };
    const auto i=static_cast<unsigned>(s);
    return labels[i<static_cast<unsigned>(Status::Count)?i:0][language==5?1:language==7?2:0];
}
inline unsigned slot=3,choice=0;
inline std::u16string name;
inline std::int64_t started_us=0;
inline bool back_held=false;
inline float exit_opacity=0;
constexpr std::int64_t FadeUs=150000;
constexpr std::uint32_t NameOwner=0x5077f0;
inline float Fade(std::int64_t now) {
    if(now<started_us)started_us=now;
    const float t=std::clamp(float(now-started_us)/FadeUs,0.f,1.f);
    return t*t*(3-2*t);
}
inline bool Active() {return phase.load()!=Phase::Idle;}
struct FileSession {
    unsigned previous=~0u;
    void Frame(unsigned state) {
        // Native state 9 is the Back animation from Start/Copy/Erase to
        // the file list. Start uses state 10; copying/erasing use other states.
        if(state==9 && previous!=9 && !Active() && !GuestActive())leave_requested=true;
        if(state==3) {
            host_started=false;
            if(guest==Guest::Returning || guest==Guest::Starting)guest=Guest::None;
        }
        previous=state;
    }
};
inline bool WaitingScreen() {
    const auto p=phase.load();return p==Phase::Waiting || p==Phase::Leaving;
}
inline bool StatusPrompt() {return WaitingScreen() || GuestPrompt() || HostPrompt();}
inline bool HideFiles() {
    const auto p=phase.load();
    return entrance_slot.load()<3 || p==Phase::Naming || p==Phase::NameLeaving || p==Phase::ReturnToFiles || p==Phase::ProfileReturn || p==Phase::Reopen || WaitingScreen();
}
inline void Reset() {
    if(const auto r=request.load();r && !r->accepted.load())r->cancelled=true;
    phase=Phase::Idle;opacity=0;slot=3;choice=0;name.clear();started_us=0;back_held=false;exit_opacity=0;
    entrance_slot=3;success_chime=false;
    guest=Guest::None;remote_started=false;profile_visible=false;intent=Intent::New;game_id.clear();
    leaving_status=Status::Searching;request.store(std::shared_ptr<const Request>{});
}
// Called only by the native title UI, never by pause/F1 or gameplay loading.
// Network teardown belongs to the frontend worker; clear presentation now so
// returning to file select cannot revive a room that is being closed.
inline void ReturnToTitle() {
    if(!hosting.load() && !host_opening.load() && !host_started.load() &&
       !GuestActive() && !Active())return;
    Reset();hosting=false;host_opening=false;host_error=false;host_started=false;
    player_count=0;stopping=true;leave_complete=false;leave_requested=true;
}
inline void Begin(unsigned selected,unsigned quest) {
    Reset();slot=selected;choice=quest;phase=Phase::ToName;
}
inline void BeginHost(unsigned selected) {
    const auto profile=ProfileAt(selected);if(!profile)return;
    host_opening=true;host_error=false;host_started=false;
    request.store(std::make_shared<const Request>(selected,profile->mq,std::u16string{},Intent::Host,profile->game_id,true));
}
inline void BeginProfile(unsigned selected,Intent action,std::u16string saved_name,std::int64_t now) {
    const auto profile=ProfileAt(selected);
    if(!profile)return;
    Begin(selected,profile->mq);intent=action;game_id=profile->game_id;
    name=std::move(saved_name);started_us=now;opacity=1;phase=Phase::ProfileLeaving;
    profile_visible=true; // Keep existing Join/Host prompts visible during the departure fade.
}
inline void Wait(std::int64_t now,bool b_down) {
    started_us=now;opacity=0;back_held=b_down;phase=Phase::Waiting;
    request.store(std::make_shared<const Request>(slot,choice!=0,name,intent,game_id));
}
// Run at the native name-state dispatch, before the save initializer can run.
// Keep the normal keyboard validation; its accepted state is 5. While fading,
// run only the geometry tail, then the original keyboard cleanup/return path.
template<class M,class R> bool NameDispatch(M& m,R& r,std::int64_t now) {
    if(!Active())return false;
    r[0]=m.Read(r[6]+4).value_or(0);
    if(phase.load()==Phase::ToName && r[0]>0)phase=Phase::Naming;
    if(phase.load()==Phase::Naming && r[0]==5) {
        const auto length=m.Read(NameOwner+0x24).value_or(0);
        if(length>0 && length<=8) {
            name.clear();
            for(unsigned i=0;i<length;++i) {
                const auto word=m.Read(0x55be4c+(i/2)*4).value_or(0);
                name.push_back(char16_t(word>>((i&1)*16)));
            }
            started_us=now;opacity=0;phase=Phase::NameLeaving;
        }
    }
    if(phase.load()==Phase::NameLeaving) {
        opacity=Fade(now);
        if(opacity.load()>=1) {phase=Phase::ReturnToFiles;r[15]=0x42863c;}
        else r[15]=0x4286fc;
    }
    return true;
}
}
