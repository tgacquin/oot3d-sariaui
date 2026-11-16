// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <iostream>
#include "core/hle/service/nwm/uds_room_membership.h"

int main() {
    using namespace Service::NWM;
    struct Node {bool connected=false,spec=false;u16 node_id=0;};
    const MacAddress host{0,1,2,3,4,1},leaving{0,1,2,3,4,2},staying{0,1,2,3,4,3},
        spectator{0,1,2,3,4,4},pending{0,1,2,3,4,5};
    std::map<MacAddress,Node> nodes{{leaving,{true,false,2}},{staying,{true,false,3}},
        {spectator,{true,true,0}},{pending,{false,false,0}}};
    ConnectionStatus status{};NetworkInfo network{};NodeList info(8);
    status.status=NetworkStatus::ConnectedAsHost;status.network_node_id=1;
    status.max_nodes=network.max_nodes=8;status.total_nodes=network.total_nodes=3;
    status.node_bitmask=7;
    for(unsigned i=0;i<3;++i) {
        status.nodes[i]=i+1;info[i].network_node_id=i+1;info[i].username[0]='A'+i;
    }
    Network::RoomMember::MemberList members(2);
    members[0].mac_address=host;members[1].mac_address=staying;
    // Client/spectator services never prune the host's wireless state.
    status.status=NetworkStatus::ConnectedAsClient;
    assert(!RetireMissingRoomNodes(nodes,status,network,info,members) && nodes.size()==4);
    status.status=NetworkStatus::ConnectedAsHost;
    assert(RetireMissingRoomNodes(nodes,status,network,info,members)==3);
    assert(nodes.size()==1 && nodes.contains(staying) && !nodes.contains(leaving));
    assert(status.total_nodes==2 && network.total_nodes==2 && status.node_bitmask==5);
    assert(status.changed_nodes==2 && !status.nodes[1] && !info[1].network_node_id && !info[1].username[0]);
    assert(status.nodes[0]==1 && status.nodes[2]==3 && info[0].username[0]=='A' && info[2].username[0]=='C');
    assert(!RetireMissingRoomNodes(nodes,status,network,info,members));
    assert(status.total_nodes==2 && network.total_nodes==2);
    // The same MAC now passes authentication's duplicate check. Native slot
    // selection can reuse node 2 without disturbing node 3's connection.
    unsigned free_id=0;
    for(unsigned i=0;i<status.max_nodes;++i)if(!(status.node_bitmask&(1u<<i))) {free_id=i+1;break;}
    assert(free_id==2);
    nodes.emplace(leaving,Node{false,false,0});
    members.push_back({});members.back().mac_address=leaving;
    assert(!RetireMissingRoomNodes(nodes,status,network,info,members));
    nodes[leaving]={true,false,2};status.node_bitmask|=2;
    ++status.total_nodes;++network.total_nodes;status.nodes[1]=2;info[1].network_node_id=2;
    assert(!RetireMissingRoomNodes(nodes,status,network,info,members));
    assert(nodes.size()==2 && status.total_nodes==3 && network.total_nodes==3);
    std::cout<<"UDS rejoin: missing/pending/spectator retirement, live-peer preservation, idempotence and same-MAC slot reuse passed\n";
}
