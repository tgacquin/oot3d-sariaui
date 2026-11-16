// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include "core/hle/service/nwm/nwm_uds.h"

namespace Service::NWM {
// The room transport can disappear before a client's wireless deauth arrives.
// Reconcile its authoritative member list while retaining every live peer.
template<class Nodes>
unsigned RetireMissingRoomNodes(Nodes& nodes,ConnectionStatus& status,NetworkInfo& network,
                               NodeList& info,const Network::RoomMember::MemberList& members) {
    if(status.status!=NetworkStatus::ConnectedAsHost)return 0;
    unsigned removed=0;
    for(auto it=nodes.begin();it!=nodes.end();) {
        if(std::any_of(members.begin(),members.end(),[&](const auto& peer) {
            return peer.mac_address==it->first;
        })) {++it;continue;}
        const auto node=it->second;
        if(node.connected && !node.spec && node.node_id>=1 && node.node_id<=UDSMaxNodes &&
           node.node_id!=status.network_node_id) {
            const unsigned bit=1u<<(node.node_id-1);
            if(status.node_bitmask&bit) {
                status.node_bitmask&=~bit;status.changed_nodes|=bit;
                if(status.total_nodes>1)--status.total_nodes;
                if(network.total_nodes>1)--network.total_nodes;
            }
            status.nodes[node.node_id-1]=0;
            for(auto& entry:info)if(entry.network_node_id==node.node_id)entry.Reset();
        }
        it=nodes.erase(it);++removed;
    }
    return removed;
}
}
