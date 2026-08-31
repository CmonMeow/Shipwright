#pragma once

#include "../simulation/ServerWorld.h"

#include <functional>

namespace Game::Server {

// Administrative mutation boundary for authoritative world configuration and
// durable world objects. Successful spatial mutations refresh replication
// through one callback before control returns to the caller.
class ServerWorldManagement {
  public:
    using SpatialMutationCallback = std::function<void()>;

    ServerWorldManagement(Simulation::ServerWorld& world,
                          SpatialMutationCallback spatialMutation);

    bool ConfigureSceneSpawn(const Simulation::PlayerSpawn& spawn);
    bool AuthorizeSceneTransition(int32_t playerId, int32_t destinationSceneId);

    Simulation::EntityId EnsureObjective(
        const Simulation::ObjectiveDefinition& definition);
    Simulation::EntityId EnsureStrategicSite(
        const Simulation::ObjectiveDefinition& objective,
        Simulation::StrategicSiteKind kind, int32_t influenceRegionKey);
    bool EnsureSupplyRoute(const Simulation::SupplyRouteDefinition& definition);
    bool RemoveSupplyRoute(int32_t routeKey);
    bool EnsureInfluenceAdjacency(
        const Simulation::InfluenceRegionAdjacencyDefinition& definition);
    bool RemoveInfluenceAdjacency(int32_t adjacencyKey);
    bool RemoveObjective(int32_t objectiveKey);
    Simulation::EntityId EnsureStructure(
        const Simulation::StructureDefinition& definition);
    bool RemoveStructure(int32_t structureKey);

  private:
    void PublishSpatialMutation();

    Simulation::ServerWorld& mWorld;
    SpatialMutationCallback mSpatialMutation;
};

} // namespace Game::Server
