// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include "network/room.h"
#include "network/room_member.h"
#include "oot3d/room_join.h"
#include "oot3d/multiplayer_settings.h"

// Local discovery is separate from the room transport: only an explicit in-game
// request connects, and the existing ENet room still validates every join.
class OotRoomJoin final : public QObject {
public:
    struct Identity {
        std::string console_id;
        Network::MacAddress mac;
        std::string nickname;
    };
    using IdentityProvider = std::function<Identity()>;
    using FileProvider = std::function<std::shared_ptr<const std::vector<u8>>(bool)>;
    using FileReceiver = std::function<bool(const std::shared_ptr<const OOT3D::RoomJoin::Request>&,
                                          const std::vector<u8>&)>;
    explicit OotRoomJoin(IdentityProvider identity, QObject* parent = nullptr,
                         quint16 discovery_port = DiscoveryPort,
                         FileProvider files = {}, FileReceiver receive = {}, bool loopback_only = false,
                         std::function<void()> before_host_close = {});
    ~OotRoomJoin() override;
    bool Busy() const;
    bool Configure(OOT3D::MultiplayerSettings::Config settings);
    bool CanLeave() const;
    bool Leave();
    static constexpr quint16 DiscoveryPort = 24873;
    static constexpr quint64 GameId = 0x0004000000033500ULL;

private:
    void Poll();
    void Search();
    void Answer();
    void Receive();
    void Join(const QHostAddress& address, quint16 port);
    void Host();
    QString shared_game_id;
    std::shared_ptr<const std::vector<u8>> advertised_file;
    IdentityProvider identity;
    FileProvider files;
    FileReceiver receive;
    u64 room_generation=0;
    quint16 discovery_port;
    OOT3D::MultiplayerSettings::Config settings;
    bool loopback_only;
    bool leaving_room=false;
    std::function<void()> before_host_close;
    Network::RoomMember::CallbackHandle<Network::ActivityEvent> activity_callback;
    Network::RoomMember::CallbackHandle<Network::StatusMessageEntry> status_callback;
    QTimer timer;
    QUdpSocket advertiser, browser;
    QFutureWatcher<void> worker;
    std::shared_ptr<const OOT3D::RoomJoin::Request> current;
    QString nonce;
    qint64 next_search = 0;
};
