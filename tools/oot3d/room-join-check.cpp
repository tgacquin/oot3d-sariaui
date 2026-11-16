// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QProcess>
#include <QThread>
#include <QSocketNotifier>
#include <unistd.h>
#include "citra_qt/multiplayer/oot_room_join.h"
#include "common/logging/backend.h"
#include "network/network.h"
#include "oot3d/randomizer/activity.h"
#include "oot3d/multiplayer_startup.h"

namespace J = OOT3D::RoomJoin;
std::shared_ptr<const std::vector<u8>> FixtureFile() {
    static const auto bytes=[] {
        QByteArray save(0x14dc,0);
        const std::u16string name=u"Épona";save[0x2c]=char(name.size());
        for(unsigned i=0;i<name.size();++i){save[0x1c+i*2]=char(name[i]);save[0x1d+i*2]=char(name[i]>>8);}
        const auto data=QJsonDocument(QJsonObject{{"package",QJsonObject{
            {"shared_game_id","ba7ab566-c1e4-4933-9e6d-efdc6251a7bb"}}},
            {"save",QString::fromLatin1(save.toBase64())},
            {"padding",QString(256*1024,'x')}}).toJson(QJsonDocument::Compact);
        return std::make_shared<std::vector<u8>>(data.begin(),data.end());
    }();return bytes;
}
template<class F> bool Spin(F done, int ms = 6000) {
    QElapsedTimer clock;clock.start();
    do {
        QCoreApplication::processEvents();
        if(done())return true;
        QThread::msleep(5);
    } while(clock.elapsed()<ms);
    const bool result=done();
    if(!result && ms>=6000) {
        if(const auto request=J::request.load())
            std::cerr<<"Timed out: "<<J::Label(request->status.load())<<" cancelled="<<request->cancelled.load()
                <<" lost="<<request->connection_lost.load()<<" member-state="
                <<int(Network::GetRoomMember().lock()->GetState())<<"\n";
    }
    return result;
}
quint16 FreePort() {
    QUdpSocket socket;assert(socket.bind(QHostAddress::LocalHost,0));return socket.localPort();
}
std::shared_ptr<const J::Request> Begin(std::u16string name,J::Intent intent=J::Intent::New,
    std::string id={}) {
    J::Begin(1,0);J::name=std::move(name);J::intent=intent;J::game_id=std::move(id);J::Wait(1,false);return J::request.load();
}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    Common::Log::DisableLoggingInTests();
    assert(Network::Init());
    const auto args=app.arguments();
    if(args.size()>1 && args[1]=="--host") {
        const auto room=Network::GetRoom().lock();
        const auto mode=args[4];
        const auto game=mode=="other"?u64{1}:
            mode=="no-preference" || mode=="no-game"?~u64{0}:
            mode=="no-preference-zero"?u64{0}:OotRoomJoin::GameId;
        assert(room->Create("Fixture","","127.0.0.1",args[3].toUShort(),mode=="locked"?"secret":"",
            mode=="full"?1:4,"Host","OOT",game,
            std::make_unique<Network::VerifyUser::NullBackend>()));
        if(mode!="no-file")room->SetSessionData(FixtureFile());
        const auto member=Network::GetRoomMember().lock();
        if(mode!="no-game")member->SendGameInfo({"Ocarina of Time 3D",OotRoomJoin::GameId});
        member->Join("Host","host-id","127.0.0.1",args[3].toUShort(),0,
            Network::NoPreferredMac,mode=="locked"?"secret":"");
        assert(Spin([&]{return member->GetState()==Network::RoomMember::State::Moderator;}));
        OotRoomJoin service([] {return OotRoomJoin::Identity{"unused",Network::NoPreferredMac};},nullptr,args[2].toUShort(),[mode](bool) {return mode=="no-file"?nullptr:FixtureFile();});
        QSocketNotifier commands(STDIN_FILENO,QSocketNotifier::Read);
        QObject::connect(&commands,&QSocketNotifier::activated,&app,[&] {
            char command=0;
            if(::read(STDIN_FILENO,&command,1)==1) {
                if(command=='Q')app.quit();
                else if(command=='P' || command=='D')member->SendActivity("ba7ab566-c1e4-4933-9e6d-efdc6251a7bb",command=='D'?1:0,command=='D'?0:4);
                else J::host_started=command=='S';
            }
            else commands.setEnabled(false);
        });
        std::cout<<"READY"<<std::endl;
        QTimer stop;
        QObject::connect(&stop,&QTimer::timeout,&app,[&]{
            // Test process lifetime is bounded even if the parent fails.
            app.quit();
        });
        stop.start(60000);
        app.exec();
        Network::Shutdown();return 0;
    }
    const auto discovery=FreePort();
    std::string console_id="client-id";
    {
        OotRoomJoin service([&]{return OotRoomJoin::Identity{console_id,Network::NoPreferredMac};},nullptr,discovery,[](bool) {return FixtureFile();},
            [](const auto& request,const auto& data) {
                if(data!=*FixtureFile())return false;
                request->status=J::Status::Connected;return true;
            },true); // Keep discovery on the same interface as the loopback-only fixture room.
        const auto member=Network::GetRoomMember().lock();
        const auto cancel=[&] {
            J::Reset();assert(Spin([&]{return !service.Busy();}));
            assert(!member->IsConnected());
        };
        // No room: search remains cancellable and never opens a connection.
        auto request=Begin(u"Link");
        assert(Spin([&]{return service.Busy();}));
        Spin([]{return false;},150);
        assert(request->status==J::Status::Searching);cancel();

        // A responder advertising an unreachable ENet room exercises cancellation
        // inside the real connect handshake, and rejects stale/mismatched replies.
        QUdpSocket responder,sink;
        assert(responder.bind(QHostAddress::LocalHost,discovery));
        assert(sink.bind(QHostAddress::LocalHost,0));
        bool reply_valid=false;
        QObject::connect(&responder,&QUdpSocket::readyRead,&app,[&] {
            while(responder.hasPendingDatagrams()) {
                const auto packet=responder.receiveDatagram();
                auto json=QJsonDocument::fromJson(packet.data()).object();
                json["type"]="room";json["port"]=int(sink.localPort());
                json["game"]=QString::number(OotRoomJoin::GameId,16);
                json["version"]=int(Network::network_version);
                if(!reply_valid)json["nonce"]="wrong";
                responder.writeDatagram(QJsonDocument(json).toJson(QJsonDocument::Compact),packet.senderAddress(),packet.senderPort());
            }
        });
        request=Begin(u"Link");
        Spin([]{return false;},1200);
        assert(request->status==J::Status::Searching);
        reply_valid=true;
        assert(Spin([&]{return request->status==J::Status::Connecting;}));
        QElapsedTimer cancellation;cancellation.start();cancel();assert(cancellation.elapsed()<1500);
        responder.close();sink.close();

        const auto start_host=[&](QProcess& host,const char* mode) {
            host.setProcessChannelMode(QProcess::MergedChannels);
            host.start(app.applicationFilePath(),{"--host",QString::number(discovery),QString::number(FreePort()),mode});
            assert(host.waitForStarted());
            QByteArray output;
            const bool ready=Spin([&]{output+=host.readAll();return output.contains("READY");});
            if(!ready)std::cerr<<output.toStdString()<<std::endl;
            assert(ready);
        };
        const auto stop_host=[](QProcess& host) {host.terminate();assert(host.waitForFinished(3000));};
        for(const auto mode:{"locked","full","other","no-game"}) {
            QProcess host;start_host(host,mode);
            request=Begin(u"Link");Spin([]{return false;},1200);
            assert(request->status==J::Status::Searching);cancel();stop_host(host);
        }
        QProcess host;start_host(host,"normal");
        request=Begin(u"Link",J::Intent::Resume,"different-game");
        Spin([]{return false;},1400);
        assert(request->status==J::Status::Searching && !member->IsConnected());cancel();
        request=Begin(u"Link",J::Intent::Resume,"ba7ab566-c1e4-4933-9e6d-efdc6251a7bb");
        assert(Spin([&]{return request->status==J::Status::Connected;}));cancel();
        // A duplicate display name retries with a suffix; console identity conflicts still fail.
        request=Begin(u"Host");
        assert(Spin([&]{return request->status==J::Status::Connected;}));
        assert(member->GetNickname()=="Host (1)");cancel();
        Network::RoomMember occupied;
        const auto port=host.arguments()[2].toUShort();
        occupied.Join("Host (1)","occupied-id","127.0.0.1",port,0,Network::NoPreferredMac);
        assert(Spin([&]{return occupied.IsConnected();}));
        request=Begin(u"Host");
        assert(Spin([&]{return request->status==J::Status::Connected;}));
        assert(member->GetNickname()=="Host (2)");cancel();occupied.Leave();
        occupied.Join("Épona123", "accent-id", "127.0.0.1", port, 0, Network::NoPreferredMac);
        assert(Spin([&]{return occupied.IsConnected();}));
        request=Begin(u"Épona123");
        assert(Spin([&]{return request->status==J::Status::Connected;}));
        assert(member->GetNickname()=="Épona123 (1)");cancel();occupied.Leave();
        console_id="host-id";request=Begin(u"Link");
        assert(Spin([&]{return request->status==J::Status::IdentityConflict;}));cancel();
        console_id="client-id";
        // Native one-character accented name; real discovery and ENet handshake.
        request=Begin(u"É");
        assert(Spin([&]{return request->status==J::Status::Connected;}));
        assert(member->GetNickname()=="É");
        // B during the success display must still leave the just-joined room.
        cancel();
        request=Begin(u"Link");
        assert(Spin([&]{return request->status==J::Status::Connected;}));
        request->accepted=true;J::Reset();
        assert(Spin([&]{return !service.Busy();}));assert(member->IsConnected());
        assert(Spin([&]{return J::player_count==2 && member->GetMemberCount()==2;}));
        J::guest=J::Guest::Waiting;
        Spin([]{return false;},350);assert(!J::remote_started);
        host.write("S",1);assert(host.waitForBytesWritten(1000));
        assert(Spin([&]{return J::remote_started.load();}));
        J::guest=J::Guest::Starting;
        namespace A=OOT3D::Randomizer::Activity;
        A::SetGame("ba7ab566-c1e4-4933-9e6d-efdc6251a7bb");
        host.write("P",1);assert(host.waitForBytesWritten(1000));
        assert(Spin([&]{return A::Visible().size()==1;}));
        assert(A::Visible()[0].text=="Host got the Fairy Bow!");
        host.write("D",1);assert(host.waitForBytesWritten(1000));
        assert(Spin([&]{return A::Visible().size()==2;}));assert(A::Visible()[1].text=="Host died.");
        A::Emit(0,8);assert(Spin([&]{return A::Visible().size()==3;}));
        assert(A::Visible()[2].text=="Link got the Hookshot!");
        Spin([]{return false;},250);assert(A::Visible().size()==3); // One server echo per event.
        occupied.Join("Visitor", "visitor-id", "127.0.0.1", port, 0, Network::NoPreferredMac);
        assert(Spin([&]{return A::Visible().size()==4;}));
        assert(A::Visible().back().text=="Visitor has joined!");
        assert(A::TakeJoinChime() && !A::TakeJoinChime());
        occupied.Leave();
        assert(Spin([&]{return A::Visible().size()==5;}));
        assert(A::Visible().back().text=="Visitor has left!");
        assert(!A::TakeJoinChime());
        Spin([]{return false;},250);assert(A::Visible().size()==5 && A::TakeOutgoing().empty());
        // A pre-existing connection must be preserved, not replaced or renamed.
        request=Begin(u"Other");
        assert(Spin([&]{return request->status==J::Status::AlreadyConnected;}));
        J::Reset();assert(Spin([&]{return !service.Busy();}));
        assert(member->IsConnected() && member->GetNickname()=="Link");
        // Entering gameplay keeps the room, but returning to the file list
        // leaves it automatically and permits a fresh join without F1 cleanup.
        J::FileSession file_session;
        file_session.Frame(8);file_session.Frame(10);file_session.Frame(11);
        assert(!J::leave_requested && member->IsConnected());
        file_session.Frame(8);file_session.Frame(9);
        assert(Spin([&]{return !service.Busy() && !member->IsConnected();}));
        file_session.Frame(9);assert(!J::leave_requested); // One request per Back transition.
        request=Begin(u"Link");
        assert(Spin([&]{return request->status==J::Status::Connected;}));
        request->accepted=true;J::Reset();J::guest=J::Guest::Waiting;
        assert(Spin([&]{return J::remote_started.load();})); // Joining an already-started host.
        assert(Spin([&]{return service.CanLeave();}));
        assert(service.Leave() && J::guest==J::Guest::Disconnecting && !J::leave_complete);
        assert(Spin([&]{return J::leave_complete.load() && !service.Busy();}));
        assert(!member->IsConnected() && !member->SessionStarted());J::Reset();
        assert(J::player_count==0 && member->GetMemberCount()==0);
        // Losing the host while waiting completes the same teardown path.
        host.write("W",1);assert(host.waitForBytesWritten(1000));
        request=Begin(u"Link");assert(Spin([&]{return request->status==J::Status::Connected;}));
        request->accepted=true;J::Reset();J::guest=J::Guest::Waiting;
        assert(Spin([&]{return !service.Busy();}));
        // The existing explicit close packet carries a different meaning from
        // a timeout. Capture it before frontend polling clears the connection.
        std::atomic<unsigned> losses{0};
        const auto closure=member->BindOnError([&](const auto& error) {
            if(error!=Network::RoomMember::Error::LostConnection)return;
            A::ConnectionLost(member->GetRoomClosureHost());++losses;
        });
        host.write("Q",1);assert(host.waitForBytesWritten(1000));assert(host.waitForFinished(3000));
        assert(Spin([&]{return losses==1;}));
        assert(member->GetRoomClosureHost()==std::optional<std::string>{"Host"});
        for(unsigned i=0;i<10;++i)A::Connection(false);
        assert(A::Visible().size()==1 && A::Visible()[0].text=="Host stopped hosting. You've been disconnected.");
        assert(Spin([&]{return J::guest==J::Guest::Disconnecting && J::leave_complete.load();},15000));
        J::Reset();
        A::SetGame({});A::SetGame("ba7ab566-c1e4-4933-9e6d-efdc6251a7bb");
        QProcess crashed;start_host(crashed,"normal");
        request=Begin(u"Link");assert(Spin([&]{return request->status==J::Status::Connected;}));
        assert(!member->GetRoomClosureHost()); // A new room cannot inherit the old reason.
        request->accepted=true;J::Reset();J::guest=J::Guest::Waiting;
        assert(Spin([&]{return !service.Busy();}));
        crashed.kill();assert(crashed.waitForFinished(3000)); // Only our isolated fixture process.
        assert(Spin([&]{return losses==2;},45000));
        assert(!member->GetRoomClosureHost());
        assert(A::Visible().size()==1 && A::Visible()[0].text=="Connection to room lost.");
        assert(Spin([&]{return J::leave_complete.load() && !service.Busy();},15000));
        member->Unbind(closure);J::Reset();A::SetGame({});
        for(const auto mode:{"no-preference","no-preference-zero"}) {
            QProcess inferred;start_host(inferred,mode);
            request=Begin(u"É");
            const bool connected=Spin([&]{return request->status==J::Status::Connected;});
            if(!connected)std::cerr<<mode<<": "<<inferred.readAll().toStdString()<<"\n";
            assert(connected);
            assert(member->GetNickname()=="É");cancel();stop_host(inferred);
        }
        QProcess missing;start_host(missing,"no-file");request=Begin(u"Link");
        const bool unavailable=Spin([&]{return request->status==J::Status::HostUnavailable;});
        if(!unavailable)std::cerr<<"Missing-file status: "<<J::Label(request->status.load())<<"\n";
        assert(unavailable);cancel();stop_host(missing);
        // The same menu exit closes a locally hosted room, not just its member.
        const auto room=Network::GetRoom().lock();
        assert(room->Create("Host exit fixture","","127.0.0.1",FreePort(),"",4,"Host","OOT",
            OotRoomJoin::GameId,std::make_unique<Network::VerifyUser::NullBackend>()));
        member->Join("Host","host-exit-id","127.0.0.1",room->GetRoomInformation().port,0,
            Network::NoPreferredMac,"");
        assert(Spin([&]{return member->GetState()==Network::RoomMember::State::Moderator;}));
        file_session.Frame(8);file_session.Frame(9);
        assert(Spin([&]{return !service.Busy() && !member->IsConnected() &&
            room->GetState()!=Network::Room::State::Open;}));
    }
    // In-game hosting uses the entered nickname and supports stop/rehost.
    {
        OotRoomJoin service([] {return OotRoomJoin::Identity{"menu-host",Network::NoPreferredMac,"MenuHost"};},nullptr,
            discovery,[](bool) {return FixtureFile();},{},true);
        const auto room=Network::GetRoom().lock();const auto member=Network::GetRoomMember().lock();
        OOT3D::MultiplayerSettings::Config options;
        options.room_name="F1 room settings";options.room_port=FreePort();options.max_players=3;
        options.discovery_port=discovery;options.search_lan=false;
        assert(service.Configure(options));
        auto invalid=options;invalid.host_address="not an IP";assert(!service.Configure(invalid));
        unsigned iteration=0;
        for(bool accept:{false,true,true}) {
            J::profiles[1].store(std::make_shared<const J::Profile>(J::Profile{"ba7ab566-c1e4-4933-9e6d-efdc6251a7bb",false}));
            J::BeginHost(1);auto request=J::request.load();
            assert(!J::Active() && request->inline_host && request->name.empty());
            assert(Spin([&]{return request->status==J::Status::Connected;}));
            assert(J::hosting && room->GetState()==Network::Room::State::Open && member->GetNickname()=="Épona");
            assert(room->GetRoomInformation().port==options.room_port);
            assert(room->GetRoomInformation().member_slots==3 && room->GetRoomInformation().name==options.room_name);
            auto changed=options;changed.room_port=FreePort();assert(!service.Configure(changed));
            assert(!service.CanLeave() && !service.Leave()); // Transfer/acceptance cannot be interrupted from F1.
            assert(Spin([&]{return J::player_count==1 && member->GetMemberCount()==1;}));
            const auto session=OOT3D::MultiplayerStartup::GetSession();
            assert(session && session->role==1 && session->nickname=="Épona");
            if(accept)request->accepted=true;
            J::host_opening=false;J::Reset();assert(Spin([&]{return !service.Busy();}));
            if(accept) {
                assert(J::hosting && member->IsConnected());
                J::host_started=true;
                if(iteration==2)assert(service.Leave()); // F1 and legacy Leave share this teardown.
                else J::ReturnToTitle();
                assert(!J::hosting && !J::host_started && J::stopping && J::leave_requested);
                J::ReturnToTitle(); // Repeated title draws do not restart teardown.
                assert(Spin([&]{return J::leave_complete.load() && !service.Busy();}));
                J::ReturnToTitle();
                assert(!J::leave_requested && J::leave_complete);
            }
            assert(!J::hosting && !J::stopping && !member->IsConnected() && room->GetState()!=Network::Room::State::Open);
            ++iteration;
        }
    }
    // Configured discovery reaches an explicit address/port, with LAN broadcast
    // disabled, without bypassing the existing room validation/join flow.
    {
        QUdpSocket responder;
        assert(responder.bind(QHostAddress::LocalHost,0));
        OotRoomJoin service([] {return OotRoomJoin::Identity{"settings-check",Network::NoPreferredMac};},nullptr,FreePort(),{},{},true);
        OOT3D::MultiplayerSettings::Config options;
        options.host_address="127.0.0.1";options.discovery_port=responder.localPort();options.search_lan=false;
        assert(service.Configure(options));
        const auto request=Begin(u"Link");
        assert(Spin([&]{return responder.hasPendingDatagrams();}));
        const auto query=QJsonDocument::fromJson(responder.receiveDatagram().data()).object();
        assert(query["protocol"]=="oot3d-room-discovery-3" && query["type"]=="query");
        assert(request->status==J::Status::Searching && !Network::GetRoomMember().lock()->IsConnected());
        J::Reset();assert(Spin([&]{return !service.Busy();}));
    }
    Network::Shutdown();
    std::cout<<"Room join: isolated discovery/transfer, errors, cancellation, connection preservation on Start, automatic guest/host departure on Back, and rejoining passed\n";
}
