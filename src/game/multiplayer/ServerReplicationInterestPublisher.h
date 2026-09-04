#pragma once

#include "ClientReplicationInbox.h"
#include "NetworkProtocol.h"
#include "platform/replication/ServerReplicationCoordinator.h"
#include "platform/simulation/ServerWorld.h"

#include <functional>
#include <vector>

namespace Game::Multiplayer {

struct ServerReplicationDelivery {
    std::function<std::vector<int32_t>()> connectedObservers;
    std::function<void(int32_t, NetAppMessageType, const NetworkMessageRaw&,
                       NetMsgFlags, Game::Replication::ReplicationStreamKey)> send;
    std::function<bool(int32_t)> connectedPlayer;
};

// Owns the conversion from authoritative visibility transitions to exact wire
// lifecycles and baselines. Transport supplies only observer enumeration and a
// delivery callback; it does not decide which entities are visible.
class ServerReplicationInterestPublisher final {
  public:
    ServerReplicationInterestPublisher(
        Game::Simulation::ServerWorld& world,
        Game::Replication::ServerReplicationCoordinator& replication,
        ClientReplicationInbox& clientInbox);

    void SetDelivery(ServerReplicationDelivery delivery);
    void RefreshAll();
    void RefreshPlayers(const std::vector<Game::Simulation::PlayerSnapshot>& players);
    void RefreshOwnedEntities();
    void RefreshSpatialEntities();

  private:
    std::vector<int32_t> ConnectedObservers() const;
    void Deliver(int32_t observer, NetAppMessageType type,
                 const NetworkMessageRaw& raw, NetMsgFlags flags,
                 Game::Replication::ReplicationStreamKey streamKey = {}) const;

    Game::Simulation::ServerWorld& mWorld;
    Game::Replication::ServerReplicationCoordinator& mReplication;
    ClientReplicationInbox& mClientInbox;
    ServerReplicationDelivery mDelivery;
};

} // namespace Game::Multiplayer
