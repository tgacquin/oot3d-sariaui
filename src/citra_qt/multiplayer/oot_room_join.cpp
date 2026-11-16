// SPDX-License-Identifier: GPL-2.0-or-later
#include "citra_qt/multiplayer/oot_room_join.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSet>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>
#include "common/logging/log.h"
#include "network/network.h"
#include "oot3d/randomizer/activity.h"
#include "oot3d/multiplayer_startup.h"

namespace {
namespace J = OOT3D::RoomJoin;
// Version 3 advertises persistent playthrough IDs for safe existing-file joins.
const auto Protocol = QStringLiteral("oot3d-room-discovery-3");
QJsonObject Decode(const QNetworkDatagram& packet) {
    if (packet.data().size() > 1024)
        return {};
    const auto object = QJsonDocument::fromJson(packet.data()).object();
    return object.value(QStringLiteral("protocol")).toString() == Protocol ? object : QJsonObject{};
}
QByteArray Encode(QJsonObject object) {
    object.insert(QStringLiteral("protocol"), Protocol);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
bool Advertise(const std::shared_ptr<Network::Room>& room) {
    if (!room || room->GetState() != Network::Room::State::Open || room->HasPassword())
        return false;
    const auto& info = room->GetRoomInformation();
    const auto members = room->GetRoomMemberList();
    if (members.size() >= info.member_slots)
        return false;
    if (info.preferred_game_id == OotRoomJoin::GameId)
        return true;
    // "No preference" uses UINT64_MAX in Qt (zero in some other frontends).
    // Infer from the host, not an unrelated guest or a room for another game.
    return (info.preferred_game_id == 0 || info.preferred_game_id == ~u64{0}) &&
        std::any_of(members.begin(), members.end(), [&](const auto& member) {
            return member.nickname == info.host_username && member.game_info.id == OotRoomJoin::GameId;
        });
}
J::Status ErrorStatus(Network::RoomMember::Error error) {
    using E = Network::RoomMember::Error;
    if (error == E::NameCollision)
        return J::Status::NameUnavailable;
    if (error == E::ConsoleIdCollision || error == E::MacCollision)
        return J::Status::IdentityConflict;
    return J::Status::Failed;
}
}

OotRoomJoin::OotRoomJoin(IdentityProvider identity_, QObject* parent, quint16 discovery_port_,
                       FileProvider files_, FileReceiver receive_, bool loopback_only_,
                       std::function<void()> before_host_close_)
    : QObject(parent), identity(std::move(identity_)), files(std::move(files_)),
      receive(std::move(receive_)), discovery_port(discovery_port_), loopback_only(loopback_only_),
      before_host_close(std::move(before_host_close_)) {
    settings.discovery_port=discovery_port;
    connect(&timer, &QTimer::timeout, this, [this] { Poll(); });
    connect(&advertiser, &QUdpSocket::readyRead, this, [this] { Answer(); });
    connect(&browser, &QUdpSocket::readyRead, this, [this] { Receive(); });
    if(const auto member=Network::GetRoomMember().lock())
        activity_callback=member->BindOnActivity([](const Network::ActivityEvent& event) {
            OOT3D::Randomizer::Activity::Receive({event.game,event.kind,event.item},event.nickname);
        });
    if(const auto member=Network::GetRoomMember().lock())
        status_callback=member->BindOnStatusMessageReceived([](const Network::StatusMessageEntry& event) {
            const auto member=Network::GetRoomMember().lock();
            const bool joined=event.type==Network::IdMemberJoin;
            const bool left=event.type==Network::IdMemberLeave || event.type==Network::IdMemberKicked ||
                event.type==Network::IdMemberBanned;
            if(member && member->IsConnected() && (joined || left) && event.nickname!=member->GetNickname()) {
                // Managed hosting leaves its local member before closing the
                // server. The explicit close packet supplies one final notice.
                if(event.type==Network::IdMemberLeave &&
                   event.nickname==member->GetRoomInformation().host_username)return;
                OOT3D::Randomizer::Activity::Connection(true);
                OOT3D::Randomizer::Activity::Presence(event.nickname,joined);
            }
        });
    timer.start(100);
}

OotRoomJoin::~OotRoomJoin() {
    timer.stop();
    J::player_count=0;
    OOT3D::MultiplayerStartup::SetSession(0);
    if(const auto member=Network::GetRoomMember().lock();member && activity_callback)member->Unbind(activity_callback);
    if(const auto member=Network::GetRoomMember().lock();member && status_callback)member->Unbind(status_callback);
    OOT3D::Randomizer::Activity::Connection(false);
    if (current && !current->accepted.load())
        current->cancelled = true;
    worker.waitForFinished();
}

bool OotRoomJoin::Busy() const {
    return current || worker.isRunning() || leaving_room || J::leave_requested.load();
}

bool OotRoomJoin::Configure(OOT3D::MultiplayerSettings::Config value) {
    value=OOT3D::MultiplayerSettings::Normalize(std::move(value));
    if(value==settings)return true;
    const auto member=Network::GetRoomMember().lock();
    const auto room=Network::GetRoom().lock();
    if(Busy() || (member && member->IsConnected()) ||
       (room && room->GetState()==Network::Room::State::Open))return false;
    QHostAddress address;
    if(!value.host_address.empty() && (!address.setAddress(QString::fromStdString(value.host_address)) ||
        address.protocol()!=QAbstractSocket::IPv4Protocol || address.isNull()))return false;
    if(discovery_port!=value.discovery_port) {advertiser.close();browser.close();}
    discovery_port=value.discovery_port;settings=std::move(value);return true;
}

bool OotRoomJoin::CanLeave() const {
    const auto member=Network::GetRoomMember().lock();
    const auto room=Network::GetRoom().lock();
    return !Busy() && !J::Active() && ((member && member->IsConnected()) ||
        (room && room->GetState()==Network::Room::State::Open));
}

bool OotRoomJoin::Leave() {
    if(!CanLeave())return false;
    if(J::GuestPrompt())J::Disconnect(); // Preserve the native guest-lobby return animation.
    else {
        J::hosting=false;J::host_opening=false;J::host_error=false;J::host_started=false;
        J::stopping=true;J::leave_complete=false;J::leave_requested=true;
    }
    return true;
}

void OotRoomJoin::Poll() {
    const auto room = Network::GetRoom().lock();
    const auto activity_member=Network::GetRoomMember().lock();
    const bool activity_connected=activity_member &&
        (activity_member->GetState()==Network::RoomMember::State::Joined || activity_member->GetState()==Network::RoomMember::State::Moderator) &&
        !leaving_room && !J::leave_requested.load();
    OOT3D::Randomizer::Activity::Connection(activity_connected);
    J::player_count=activity_connected?activity_member->GetMemberCount():0;
    OOT3D::MultiplayerStartup::SetSession(activity_connected?
        (room && room->GetState()==Network::Room::State::Open?1:2):0,
        activity_connected?activity_member->GetNickname():std::string{});
    for(const auto& event:OOT3D::Randomizer::Activity::TakeOutgoing())if(activity_connected)
        activity_member->SendActivity(event.game,static_cast<u8>(event.kind),static_cast<u16>(event.item));
    if(leaving_room) {
        if(worker.isRunning())return;
        leaving_room=false;
        J::leave_complete=true;J::hosting=false;J::stopping=false;J::host_opening=false;J::host_error=false;
    }
    if(J::leave_requested.load()) {
        if(current && !current->accepted.load())current->cancelled=true;
        if(worker.isRunning())return; // Serialize teardown with the join/import worker.
        J::leave_requested=false;
        J::leave_complete=false;
        advertiser.close();browser.close();current.reset();
        const auto member=Network::GetRoomMember().lock();
        const bool hosting=room && room->GetState()==Network::Room::State::Open;
        if(hosting && before_host_close)before_host_close();
        leaving_room=true;
        worker.setFuture(QtConcurrent::run([member,room,hosting] {
            if(member)member->Leave();
            if(hosting)room->Destroy();
            LOG_INFO(Network,"OoT file menu: room departure completed");
        }));
        return;
    }
    if(J::hosting.load() && !worker.isRunning() && (!room || room->GetState()!=Network::Room::State::Open))J::hosting=false;
    if(room && room->GetState()==Network::Room::State::Open)
        room->SetSessionStarted(J::host_started.load());
    if(J::guest==J::Guest::Waiting) {
        const auto member=Network::GetRoomMember().lock();
        if(!member || !member->IsConnected()) {J::Disconnect();return;}
        member->RequestSessionState();
        J::remote_started=member->SessionStarted();
    }
    if(room && room->GetState()==Network::Room::State::Open && files) {
        const auto generation=room->GetGeneration();
        const auto snapshot=files(generation!=room_generation);
        room->SetSessionData(snapshot);
        if(snapshot!=advertised_file) {
            advertised_file=snapshot;shared_game_id.clear();
            if(snapshot)shared_game_id=QJsonDocument::fromJson(QByteArray(
                reinterpret_cast<const char*>(snapshot->data()),snapshot->size())).object()
                .value(QStringLiteral("package")).toObject().value(QStringLiteral("shared_game_id")).toString();
        }
        room_generation=generation;
    }
    if (Advertise(room)) {
        if (advertiser.state() != QAbstractSocket::BoundState &&
            advertiser.bind(QHostAddress::AnyIPv4, discovery_port,
                            QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
            LOG_INFO(Network, "OoT Join Room: advertising local room on port {}", room->GetRoomInformation().port);
    } else {
        advertiser.close();
    }

    const auto request = J::request.load();
    if (request != current) {
        if (current && !current->accepted.load())
            current->cancelled = true;
        browser.close();
        if (worker.isRunning())
            return; // Never overlap Join/Leave on the same RoomMember.
        current = request;
        if (!current)
            return;
        const auto member = Network::GetRoomMember().lock();
        if (!member) {
            current->status = J::Status::Failed;
            return;
        }
        if (member->IsConnected() || (room && room->GetState() == Network::Room::State::Open)) {
            current->status = J::Status::AlreadyConnected;
            return; // Never replace a manual connection or close a hosted room.
        }
        if(current->intent==J::Intent::Host) {Host();return;}
        nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!browser.bind(QHostAddress::AnyIPv4, 0)) {
            current->status = J::Status::Failed;
            return;
        }
        next_search = 0;
        LOG_INFO(Network, "OoT Join Room: searching local network");
    }
    if (current && !current->cancelled.load() && current->status == J::Status::Searching &&
        QDateTime::currentMSecsSinceEpoch() >= next_search)
        Search();
}

void OotRoomJoin::Search() {
    next_search = QDateTime::currentMSecsSinceEpoch() + 1000;
    const auto query = Encode({{QStringLiteral("type"), QStringLiteral("query")}, {QStringLiteral("nonce"), nonce}});
    QSet<QHostAddress> targets{QHostAddress::LocalHost};
    if(!loopback_only && settings.search_lan)targets.insert(QHostAddress::Broadcast);
    if(!loopback_only && settings.search_lan)for (const auto& interface : QNetworkInterface::allInterfaces()) {
        if (!interface.flags().testFlag(QNetworkInterface::IsUp) ||
            !interface.flags().testFlag(QNetworkInterface::CanBroadcast))
            continue;
        for (const auto& entry : interface.addressEntries())
            if (entry.broadcast().protocol() == QAbstractSocket::IPv4Protocol)
                targets.insert(entry.broadcast());
    }
    if(!settings.host_address.empty())targets.insert(QHostAddress(QString::fromStdString(settings.host_address)));
    for (const auto& target : targets)
        browser.writeDatagram(query, target, discovery_port);
}

void OotRoomJoin::Answer() {
    while (advertiser.hasPendingDatagrams()) {
        const auto packet = advertiser.receiveDatagram(1025);
        const auto query = Decode(packet);
        const auto token = query.value(QStringLiteral("nonce")).toString();
        const auto room = Network::GetRoom().lock();
        if (query.value(QStringLiteral("type")) != QStringLiteral("query") || token.size() != 36 || !Advertise(room))
            continue;
        const auto reply = Encode({{QStringLiteral("type"), QStringLiteral("room")}, {QStringLiteral("nonce"), token},
                                   {QStringLiteral("version"), int(Network::network_version)},
                                   {QStringLiteral("game"), QString::number(GameId, 16)},
                                   {QStringLiteral("port"), int(room->GetRoomInformation().port)},
                                   {QStringLiteral("shared_game_id"), shared_game_id}});
        advertiser.writeDatagram(reply, packet.senderAddress(), packet.senderPort());
    }
}

void OotRoomJoin::Receive() {
    while (browser.hasPendingDatagrams()) {
        const auto packet = browser.receiveDatagram(1025);
        const auto reply = Decode(packet);
        const int port = reply.value(QStringLiteral("port")).toInt();
        if (!current || current != J::request.load() || current->cancelled.load() ||
            current->status != J::Status::Searching || reply.value(QStringLiteral("type")) != QStringLiteral("room") ||
            reply.value(QStringLiteral("nonce")) != nonce ||
            reply.value(QStringLiteral("version")).toInt(-1) != int(Network::network_version) ||
            reply.value(QStringLiteral("game")).toString() != QString::number(GameId, 16) || port < 1 || port > 65535)
            continue;
        if(current->intent==J::Intent::Resume &&
           reply.value(QStringLiteral("shared_game_id")).toString().toStdString()!=current->game_id)continue;
        Join(packet.senderAddress(), quint16(port));
        return; // The first eligible response wins. No concurrent attempts.
    }
}

void OotRoomJoin::Join(const QHostAddress& address, quint16 port) {
    const auto request = current;
    const auto member = Network::GetRoomMember().lock();
    if (!member || member->IsConnected()) {
        request->status = J::Status::AlreadyConnected;
        return;
    }
    const auto credentials = identity(); // Read CFG only on the frontend thread.
    const auto nickname = QString::fromStdU16String(request->name).toUtf8().toStdString();
    const auto host = address.toString().toStdString();
    request->status = J::Status::Connecting;
    browser.close();
    LOG_INFO(Network, "OoT Join Room: connecting to {}:{}", host, port);
    worker.setFuture(QtConcurrent::run([request, member, credentials, nickname, host, port, receive=receive] {
        auto collision=std::make_shared<std::atomic<bool>>(false);
        const auto error = member->BindOnError([request,collision](const auto& e) {
            if(e==Network::RoomMember::Error::NameCollision && request->status==J::Status::Connecting) {
                *collision=true;return;
            }
            request->connection_lost=true;
            if (!request->cancelled.load() && !request->committing.load())
                request->status = request->saved.load()?J::Status::SavedDisconnected:ErrorStatus(e);
        });
        if (!request->cancelled.load()) {
            for(unsigned suffix=0;suffix<=99 && !request->cancelled.load();++suffix) {
                *collision=false;
                const auto candidate=nickname+(suffix?" ("+std::to_string(suffix)+")":"");
                member->Join(candidate, credentials.console_id, host.c_str(), port, 0,
                             credentials.mac, "", "", [request] { return request->cancelled.load(); });
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                while (!request->cancelled.load() &&
                       member->GetState() == Network::RoomMember::State::Joining &&
                       std::chrono::steady_clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if(member->IsConnected())break;
                // Join the packet thread before inspecting its error callback:
                // Idle is published just before NameCollision is delivered.
                member->Leave();
                if(!collision->load())break;
                if(suffix==99)request->status=J::Status::NameUnavailable;
            }
            const auto state = member->GetState();
            if (!request->cancelled.load() &&
                (state == Network::RoomMember::State::Joined || state == Network::RoomMember::State::Moderator)) {
                LOG_INFO(Network, "OoT Join Room: connected");
                request->status=J::Status::Receiving;
                static std::atomic<u64> next_token{0};
                const auto token=++next_token;
                auto reply=std::make_shared<Common::AtomicSharedPtr<const Network::SessionData>>();
                const auto callback=member->BindOnSessionData([reply,token](const auto& data) {
                    if(data.token==token)reply->store(std::make_shared<const Network::SessionData>(data));
                });
                member->RequestSessionData(token);
                const auto transfer_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
                while(!request->cancelled.load() && member->IsConnected() &&
                      !reply->load() && std::chrono::steady_clock::now()<transfer_deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                member->Unbind(callback);
                const auto data=reply->load();
                if(!request->cancelled.load() && request->status==J::Status::Receiving) {
                    if(!data || data->bytes.empty())request->status=J::Status::HostUnavailable;
                    else if(!receive || !receive(request,data->bytes))request->status=J::Status::InvalidFile;
                }
                while(!request->cancelled.load() && !request->accepted.load() &&
                      (request->status==J::Status::ReadyToSave || request->status==J::Status::Saving ||
                       request->status==J::Status::Connected)) {
                    if(!member->IsConnected()) {
                        request->connection_lost=true;
                        if(!request->committing.load())
                            request->status=request->saved.load()?J::Status::SavedDisconnected:J::Status::Failed;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            if (request->status == J::Status::Connecting)
                request->status = J::Status::Failed;
            // Acceptance transfers connection ownership to ordinary multiplayer.
            if (!request->accepted.load())
                member->Leave();
        }
        member->Unbind(error);
    }));
}

void OotRoomJoin::Host() {
    const auto request=current;
    const auto room=Network::GetRoom().lock();
    const auto member=Network::GetRoomMember().lock();
    const auto snapshot=files?files(true):nullptr;
    if(!room || !member || !snapshot || request->game_id.empty()) {
        request->status=J::Status::HostFailed;return;
    }
    const auto id=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(snapshot->data()),snapshot->size()))
        .object().value(QStringLiteral("package")).toObject().value(QStringLiteral("shared_game_id")).toString().toStdString();
    if(id!=request->game_id) {request->status=J::Status::HostFailed;return;}
    const auto credentials=identity();
    auto name=request->name;
    if(request->inline_host) {
        const auto object=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(snapshot->data()),snapshot->size())).object();
        const auto save=QByteArray::fromBase64(object.value(QStringLiteral("save")).toString().toLatin1());
        if(save.size()>0x2c)for(unsigned i=0;i<std::min(8u,unsigned(static_cast<unsigned char>(save[0x2c])));++i) {
            const auto c=static_cast<unsigned char>(save[0x1c+2*i])|(unsigned(static_cast<unsigned char>(save[0x1d+2*i]))<<8);
            if(c>=32 && c!=0xffff)name.push_back(char16_t(c));
        }
    }
    const auto nickname=name.empty()?std::string("Link"):QString::fromStdU16String(name).toUtf8().toStdString();
    request->status=J::Status::Connecting;
    J::host_started=false;
    const auto options=settings;
    worker.setFuture(QtConcurrent::run([request,room,member,snapshot,credentials,nickname,options] {
        bool created=false;
        // A second local emulator may already own the default room port.
        u16 port=options.room_port;
        const unsigned attempts=port==Network::DefaultRoomPort?10:1;
        for(unsigned attempt=0;attempt<attempts && !request->cancelled.load();++attempt,++port) {
            if(room->Create(options.room_name.empty()?"Ocarina of Time 3D":options.room_name, "", "", port, "", options.max_players, nickname,
                            "Ocarina of Time 3D", GameId,
                            std::make_unique<Network::VerifyUser::NullBackend>())) {created=true;break;}
        }
        if(created && !request->cancelled.load()) {
            room->SetSessionData(snapshot);
            member->Join(nickname,credentials.console_id,"127.0.0.1",port,0,credentials.mac,"","",
                         [request] {return request->cancelled.load();});
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while(!request->cancelled.load() && member->GetState()==Network::RoomMember::State::Joining &&
                  std::chrono::steady_clock::now()<deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if(!request->cancelled.load() && member->IsConnected()) {
                J::hosting=true;request->saved=true;request->status=J::Status::Connected;
                while(!request->cancelled.load() && !request->accepted.load() && request->status==J::Status::Connected &&
                      member->IsConnected() && room->GetState()==Network::Room::State::Open)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if(request->accepted.load())return;
                J::hosting=false;request->saved=false;
            }
        }
        if(created) {member->Leave();room->Destroy();}
        request->status=J::Status::HostFailed;
    }));
}
