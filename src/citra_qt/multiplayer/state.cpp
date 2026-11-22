// Copyright 2018-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QStandardItemModel>
#include "citra_qt/multiplayer/client_room.h"
#include "citra_qt/multiplayer/direct_connect.h"
#include "citra_qt/multiplayer/host_room.h"
#include "citra_qt/multiplayer/lobby.h"
#include "citra_qt/multiplayer/message.h"
#include "citra_qt/multiplayer/state.h"
#include "citra_qt/multiplayer/oot_room_join.h"
#include "citra_qt/uisettings.h"
#include "citra_qt/util/clickable_label.h"
#include "common/logging/log.h"
#include "core/hle/service/cfg/cfg.h"
#include "oot3d/randomizer/service.h"
#include "oot3d/randomizer/activity.h"
#include "oot3d/input.h"
#include "oot3d/multiplayer_settings.h"

MultiplayerState::MultiplayerState(Core::System& system_, QWidget* parent,
                                   QStandardItemModel* game_list_model, QAction* leave_room,
                                   QAction* show_room)
    : QWidget(parent), system{system_}, game_list_model(game_list_model), leave_room(leave_room),
      show_room(show_room) {
    oot_room_join = std::make_unique<OotRoomJoin>([this] {
        return OotRoomJoin::Identity{Service::CFG::GetConsoleIdHash(system),
                                    Service::CFG::GetConsoleMacAddress(system),Service::CFG::GetUsername(system)};
    }, nullptr, OotRoomJoin::DiscoveryPort, OOT3D::Randomizer::HostedRoomFile,
       OOT3D::Randomizer::ReceiveRoomFile, false, [this] {
           if(const auto room=Network::GetRoom().lock())UISettings::values.ban_list=room->GetBanList();
           announce_multiplayer_session->Stop();
       });
    if (auto member = Network::GetRoomMember().lock()) {
        // register the network structs to use in slots and signals
        state_callback_handle = member->BindOnStateChanged(
            [this](const Network::RoomMember::State& state) { emit NetworkStateChanged(state); });
        connect(this, &MultiplayerState::NetworkStateChanged, this,
                &MultiplayerState::OnNetworkStateChanged);
        error_callback_handle = member->BindOnError(
            [this](const Network::RoomMember::Error& error) {
                // Capture the reason with the event, before Qt queues it. A
                // later reconnect must not rename or erase this departure.
                const auto member=Network::GetRoomMember().lock();
                const auto host=member?member->GetRoomClosureHost():std::nullopt;
                emit NetworkError(error,host?QString::fromStdString(*host):QString{},host.has_value());
            });
        connect(this, &MultiplayerState::NetworkError, this, &MultiplayerState::OnNetworkError);
    }

    qRegisterMetaType<Network::RoomMember::State>();
    qRegisterMetaType<Network::RoomMember::Error>();
    qRegisterMetaType<Common::WebResult>();
    {
        Validation validation;
        QString username = QString::fromStdString(Service::CFG::GetUsername(system));
        int pos = 0;
        announce_multiplayer_session = std::make_shared<Network::AnnounceMultiplayerSession>(
            validation.GetNickname()->validate(username, pos) == QValidator::State::Acceptable
                ? username.toStdString()
                : "Azahar");
    }
    announce_multiplayer_session->BindErrorCallback(
        [this](const Common::WebResult& result) { emit AnnounceFailed(result); });
    connect(this, &MultiplayerState::AnnounceFailed, this, &MultiplayerState::OnAnnounceFailed);

    status_text = new ClickableLabel(this);
    status_icon = new ClickableLabel(this);
    status_text->setToolTip(tr("Current connection status"));
    status_text->setText(tr("Not Connected. Click here to find a room!"));
    status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("disconnected")).pixmap(16));

    connect(status_text, &ClickableLabel::clicked, this, &MultiplayerState::OnOpenNetworkRoom);
    connect(status_icon, &ClickableLabel::clicked, this, &MultiplayerState::OnOpenNetworkRoom);

    connect(static_cast<QApplication*>(QApplication::instance()), &QApplication::focusChanged, this,
            [this](QWidget* /*old*/, QWidget* now) {
                if (client_room && client_room->isAncestorOf(now)) {
                    HideNotification();
                }
            });
}

MultiplayerState::~MultiplayerState() {
    oot_room_join.reset(); // Cancel/join the worker before removing network callbacks.
    if (state_callback_handle) {
        if (auto member = Network::GetRoomMember().lock()) {
            member->Unbind(state_callback_handle);
        }
    }

    if (error_callback_handle) {
        if (auto member = Network::GetRoomMember().lock()) {
            member->Unbind(error_callback_handle);
        }
    }
}

void MultiplayerState::Close() {
    if (host_room)
        host_room->close();
    if (direct_connect)
        direct_connect->close();
    if (client_room)
        client_room->close();
    if (lobby)
        lobby->close();
}

void MultiplayerState::retranslateUi() {
    status_text->setToolTip(tr("Current connection status"));

    if (current_state == Network::RoomMember::State::Uninitialized) {
        status_text->setText(tr("Not Connected. Click here to find a room!"));
    } else if (current_state == Network::RoomMember::State::Joined ||
               current_state == Network::RoomMember::State::Moderator) {

        status_text->setText(tr("Connected"));
    } else {
        status_text->setText(tr("Not Connected"));
    }

    if (lobby)
        lobby->RetranslateUi();
    if (host_room)
        host_room->RetranslateUi();
    if (client_room)
        client_room->RetranslateUi();
    if (direct_connect)
        direct_connect->RetranslateUi();
}

void MultiplayerState::OnNetworkStateChanged(const Network::RoomMember::State& state) {
    LOG_DEBUG(Frontend, "Network State: {}", Network::GetStateStr(state));
    if (state == Network::RoomMember::State::Joined ||
        state == Network::RoomMember::State::Moderator) {

        if (!oot_mode && !oot_room_join->Busy() && !OOT3D::RoomJoin::Active())
            OnOpenNetworkRoom();
        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("connected")).pixmap(16));
        status_text->setText(tr("Connected"));
        leave_room->setEnabled(true);
        show_room->setEnabled(true);
    } else {
        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("disconnected")).pixmap(16));
        status_text->setText(tr("Not Connected"));
        leave_room->setEnabled(false);
        show_room->setEnabled(false);
    }

    current_state = state;
}

void MultiplayerState::OnNetworkError(const Network::RoomMember::Error& error,const QString& host,bool room_closed) {
    LOG_DEBUG(Frontend, "Network Error: {}", Network::GetErrorStr(error));
    // Frontend routing belongs to the running game, not the transient native
    // randomizer runtime (which can deactivate during file/title transitions).
    if (oot_mode || OOT3D::Randomizer::active.load()) {
        using E = Network::RoomMember::Error;
        if(error==E::LostConnection) {
            OOT3D::Randomizer::Activity::ConnectionLost(room_closed?
                std::optional<std::string>{host.toStdString()}:std::nullopt);
            return;
        }
        const char* notice = error == E::HostKicked ? "You were removed from the room." :
                             error == E::HostBanned ? "You were banned from the room." : nullptr;
        if (notice) {
            OOT3D::Randomizer::Activity::Notice(notice);
            return;
        }
    }
    if (oot_room_join->Busy() || OOT3D::RoomJoin::Active())
        return; // The in-game prompt reports errors without interrupting play.
    switch (error) {
    case Network::RoomMember::Error::LostConnection:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::LOST_CONNECTION);
        break;
    case Network::RoomMember::Error::HostKicked:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::HOST_KICKED);
        break;
    case Network::RoomMember::Error::CouldNotConnect:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::UNABLE_TO_CONNECT);
        break;
    case Network::RoomMember::Error::NameCollision:
        NetworkMessage::ErrorManager::ShowError(
            NetworkMessage::ErrorManager::USERNAME_NOT_VALID_SERVER);
        break;
    case Network::RoomMember::Error::MacCollision:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::MAC_COLLISION);
        break;
    case Network::RoomMember::Error::ConsoleIdCollision:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::CONSOLE_ID_COLLISION);
        break;
    case Network::RoomMember::Error::RoomIsFull:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::ROOM_IS_FULL);
        break;
    case Network::RoomMember::Error::WrongPassword:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::WRONG_PASSWORD);
        break;
    case Network::RoomMember::Error::WrongVersion:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::WRONG_VERSION);
        break;
    case Network::RoomMember::Error::HostBanned:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::HOST_BANNED);
        break;
    case Network::RoomMember::Error::UnknownError:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::UNABLE_TO_CONNECT);
        break;
    case Network::RoomMember::Error::PermissionDenied:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::PERMISSION_DENIED);
        break;
    case Network::RoomMember::Error::NoSuchUser:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::NO_SUCH_USER);
        break;
    }
}

void MultiplayerState::OnAnnounceFailed(const Common::WebResult& result) {
    announce_multiplayer_session->Stop();
    QMessageBox::warning(this, tr("Error"),
                         tr("Failed to update the room information. Please check your internet "
                            "connection and try hosting the room again.\nDebug Message: ") +
                             QString::fromStdString(result.result_string),
                         QMessageBox::Ok);
}

void MultiplayerState::UpdateThemedIcons() {
    if (show_notification) {
        status_icon->setPixmap(
            QIcon::fromTheme(QStringLiteral("connected_notification")).pixmap(16));
    } else if (current_state == Network::RoomMember::State::Joined ||
               current_state == Network::RoomMember::State::Moderator) {

        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("connected")).pixmap(16));
    } else {
        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("disconnected")).pixmap(16));
    }
    if (client_room)
        client_room->UpdateIconDisplay();
}

static void BringWidgetToFront(QWidget* widget) {
    widget->show();
    widget->activateWindow();
    widget->raise();
}

bool MultiplayerState::UpdateOotSettings(bool running_oot) {
    namespace S=OOT3D::MultiplayerSettings;
    if(oot_mode!=running_oot) {
        oot_mode=running_oot;
        if(oot_mode)Close();
        oot_settings_error.clear();
        status_text->setToolTip(oot_mode?tr("Multiplayer settings (F1)"):tr("Current connection status"));
        if(!oot_mode) {
            const auto member=Network::GetRoomMember().lock();
            leave_room->setEnabled(member && member->IsConnected());
        }
    }
    const auto read_port=[](const QString& text,int fallback) {
        bool ok=false;const int value=text.toInt(&ok);
        return ok && value>=1 && value<=65535?value:fallback;
    };
    auto& ui=UISettings::values;
    S::Config options{ui.room_name.toStdString(),ui.ip.toStdString(),
        read_port(ui.room_port,24872),int(ui.max_player),ui.oot_discovery_port,ui.oot_search_lan};
    options=S::Normalize(options);
    bool changed=false;
    if(const auto edit=S::TakeEdit();edit && oot_mode) {
        if(oot_room_join->Configure(*edit)) {
            ui.room_name=QString::fromStdString(edit->room_name);
            ui.ip=QString::fromStdString(edit->host_address);
            ui.room_port=QString::number(edit->room_port);ui.max_player=edit->max_players;
            ui.oot_discovery_port=edit->discovery_port;ui.oot_search_lan=edit->search_lan;
            options=*edit;changed=true;oot_settings_error.clear();
        } else oot_settings_error="Settings were not changed. Disconnect first and use a valid IPv4 address.";
    }
    oot_room_join->Configure(options);
    if(S::disconnect_requested.exchange(false) && oot_mode)oot_room_join->Leave();
    S::Status state;
    const auto member=Network::GetRoomMember().lock();
    const auto room=Network::GetRoom().lock();
    if(member && member->IsConnected()) {
        state.role=room && room->GetState()==Network::Room::State::Open?1:2;
        state.players=member->GetMemberCount();state.nickname=member->GetNickname();
        if(state.role==1)state.port=room->GetRoomInformation().port;
    }
    state.busy=oot_room_join->Busy() || OOT3D::RoomJoin::Active();
    state.can_disconnect=oot_room_join->CanLeave();state.error=oot_settings_error;
    S::Publish(std::move(options),std::move(state));
    if(oot_mode)leave_room->setEnabled(oot_room_join->CanLeave());
    return changed;
}

void MultiplayerState::OnViewLobby() {
    if (oot_mode || oot_room_join->Busy())return;
    if (lobby == nullptr) {
        lobby = new Lobby(system, this, game_list_model, announce_multiplayer_session);
    }
    BringWidgetToFront(lobby);
}

void MultiplayerState::OnCreateRoom() {
    if (oot_mode || oot_room_join->Busy())return;
    if (host_room == nullptr) {
        host_room = new HostRoomWindow(system, this, game_list_model, announce_multiplayer_session);
    }
    BringWidgetToFront(host_room);
}

bool MultiplayerState::OnCloseRoom() {
    if(oot_mode)return oot_room_join->Leave();
    if (oot_room_join->Busy())return false;
    if (!NetworkMessage::WarnCloseRoom())
        return false;
    if (auto room = Network::GetRoom().lock()) {
        // if you are in a room, leave it
        if (auto member = Network::GetRoomMember().lock()) {
            member->Leave();
            LOG_DEBUG(Frontend, "Left the room (as a client)");
        }

        // if you are hosting a room, also stop hosting
        if (room->GetState() != Network::Room::State::Open) {
            return true;
        }

        // Save ban list
        UISettings::values.ban_list = room->GetBanList();

        room->Destroy();
        announce_multiplayer_session->Stop();
        LOG_DEBUG(Frontend, "Closed the room (as a server)");
    }
    return true;
}

void MultiplayerState::ShowNotification() {
    if(oot_mode)return;
    if (client_room && client_room->isAncestorOf(QApplication::focusWidget()))
        return; // Do not show notification if the chat window currently has focus
    show_notification = true;
    QApplication::alert(nullptr);
    status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("connected_notification")).pixmap(16));
    status_text->setText(tr("New Messages Received"));
}

void MultiplayerState::HideNotification() {
    show_notification = false;
    status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("connected")).pixmap(16));
    status_text->setText(tr("Connected"));
}

void MultiplayerState::OnOpenNetworkRoom() {
    if(oot_mode) {
        OOT3D::MultiplayerSettings::select_tab=true;
        OOT3D::settings_view=OOT3D::SettingsView::Editor;OOT3D::visible=true;
        return;
    }
    if (auto member = Network::GetRoomMember().lock()) {
        if (member->IsConnected()) {
            if (client_room == nullptr) {
                client_room = new ClientRoomWindow(this);
                connect(client_room, &ClientRoomWindow::ShowNotification, this,
                        &MultiplayerState::ShowNotification);
            }
            BringWidgetToFront(client_room);
            return;
        }
    }
    // If the user is not a member of a room, show the lobby instead.
    // This is currently only used on the clickable label in the status bar
    OnViewLobby();
}

void MultiplayerState::OnDirectConnectToRoom() {
    if (oot_mode || oot_room_join->Busy())return;
    if (direct_connect == nullptr) {
        direct_connect = new DirectConnectWindow(system, this);
    }
    BringWidgetToFront(direct_connect);
}

bool MultiplayerState::IsHostingPublicRoom() const {
    return announce_multiplayer_session->IsRunning();
}

void MultiplayerState::UpdateCredentials() {
    Validation validation;
    QString username = QString::fromStdString(Service::CFG::GetUsername(system));
    int pos = 0;
    announce_multiplayer_session->UpdateCredentials(
        validation.GetNickname()->validate(username, pos) == QValidator::State::Acceptable
            ? username.toStdString()
            : "Azahar");
}

void MultiplayerState::UpdateGameList(QStandardItemModel* game_list) {
    game_list_model = game_list;
    if (lobby) {
        lobby->UpdateGameList(game_list);
    }
    if (host_room) {
        host_room->UpdateGameList(game_list);
    }
}
