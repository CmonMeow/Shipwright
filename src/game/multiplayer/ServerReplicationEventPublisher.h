#pragma once

#include "ServerReplicationInterestPublisher.h"

namespace Game::Multiplayer {

// Publishes authoritative simulation snapshots and semantic events after
// interest lifecycles have established exact entity generations.
class ServerReplicationEventPublisher final {
  public:
    ServerReplicationEventPublisher(
        Game::Simulation::ServerWorld& world,
        Game::Replication::ServerReplicationCoordinator& replication,
        ClientReplicationInbox& clientInbox,
        ServerReplicationInterestPublisher& interestPublisher);

    void SetDelivery(ServerReplicationDelivery delivery);
    void PublishPlayerSnapshots();
    void PublishObjectiveSnapshots();
    void PublishStrategicTopology();
    void PublishStrategicTopologyTo(int32_t observer);
    void PublishStructureSnapshots();
    void PublishStructureState(const Game::Simulation::StructureSnapshot& structure);
    bool PublishFishingPresentation(
        const Game::Replication::FishingPresentationState& presentation,
        const Game::Simulation::PlayerSnapshot& authoritativePlayer);
    void PublishProjectileEvents();
    void PublishFishingEvents();
    void PublishCombatResults();
    void PublishLifeEvents();

  private:
    void Deliver(int32_t observer, NetAppMessageType type,
                 const NetworkMessageRaw& raw, NetMsgFlags flags,
                 Game::Replication::ReplicationStreamKey streamKey = {}) const;
    void DeliverOwnedEntity(NetAppMessageType type, const NetworkMessageRaw& raw,
                            const Game::Replication::OwnedEntityKey& entity,
                            bool includeOwner, NetMsgFlags flags,
                            Game::Replication::ReplicationStreamKey streamKey = {}) const;

    Game::Simulation::ServerWorld& mWorld;
    Game::Replication::ServerReplicationCoordinator& mReplication;
    ClientReplicationInbox& mClientInbox;
    ServerReplicationInterestPublisher& mInterestPublisher;
    ServerReplicationDelivery mDelivery;
    std::vector<Game::Simulation::StrategicSiteDefinition>
        mPublishedStrategicSites;
    std::vector<Game::Simulation::SupplyRouteDefinition>
        mPublishedSupplyRoutes;
    std::vector<Game::Simulation::InfluenceRegionAdjacencyDefinition>
        mPublishedInfluenceAdjacencies;
    uint32_t mStrategicTopologyRevision = 1;
};

} // namespace Game::Multiplayer
