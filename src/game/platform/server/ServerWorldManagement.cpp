#include "ServerWorldManagement.h"

#include <utility>

namespace Game::Server {

ServerWorldManagement::ServerWorldManagement(
    Simulation::ServerWorld& world, SpatialMutationCallback spatialMutation)
    : mWorld(world), mSpatialMutation(std::move(spatialMutation)) {
}

bool ServerWorldManagement::ConfigureSceneSpawn(
    const Simulation::PlayerSpawn& spawn) {
    return mWorld.ConfigureSceneSpawn(spawn);
}

bool ServerWorldManagement::AuthorizeSceneTransition(
    int32_t playerId, int32_t destinationSceneId) {
    return mWorld.AuthorizeSceneTransition(playerId, destinationSceneId);
}

Simulation::EntityId ServerWorldManagement::EnsureObjective(
    const Simulation::ObjectiveDefinition& definition) {
    const Simulation::EntityId entity = mWorld.EnsureObjective(definition);
    if (entity.Valid()) PublishSpatialMutation();
    return entity;
}

Simulation::EntityId ServerWorldManagement::EnsureStrategicSite(
    const Simulation::ObjectiveDefinition& objective,
    Simulation::StrategicSiteKind kind, int32_t influenceRegionKey) {
    const Simulation::EntityId entity =
        mWorld.EnsureStrategicSite(objective, kind, influenceRegionKey);
    if (entity.Valid()) PublishSpatialMutation();
    return entity;
}

bool ServerWorldManagement::EnsureSupplyRoute(
    const Simulation::SupplyRouteDefinition& definition) {
    if (!mWorld.EnsureSupplyRoute(definition)) return false;
    PublishSpatialMutation();
    return true;
}

bool ServerWorldManagement::RemoveSupplyRoute(int32_t routeKey) {
    if (!mWorld.RemoveSupplyRoute(routeKey)) return false;
    PublishSpatialMutation();
    return true;
}

bool ServerWorldManagement::EnsureInfluenceAdjacency(
    const Simulation::InfluenceRegionAdjacencyDefinition& definition) {
    if (!mWorld.EnsureInfluenceAdjacency(definition)) return false;
    PublishSpatialMutation();
    return true;
}

bool ServerWorldManagement::RemoveInfluenceAdjacency(int32_t adjacencyKey) {
    if (!mWorld.RemoveInfluenceAdjacency(adjacencyKey)) return false;
    PublishSpatialMutation();
    return true;
}

bool ServerWorldManagement::RemoveObjective(int32_t objectiveKey) {
    if (!mWorld.RemoveObjective(objectiveKey)) return false;
    PublishSpatialMutation();
    return true;
}

Simulation::EntityId ServerWorldManagement::EnsureStructure(
    const Simulation::StructureDefinition& definition) {
    const Simulation::EntityId entity = mWorld.EnsureStructure(definition);
    if (entity.Valid()) PublishSpatialMutation();
    return entity;
}

bool ServerWorldManagement::RemoveStructure(int32_t structureKey) {
    if (!mWorld.RemoveStructure(structureKey)) return false;
    PublishSpatialMutation();
    return true;
}

void ServerWorldManagement::PublishSpatialMutation() {
    if (mSpatialMutation) mSpatialMutation();
}

} // namespace Game::Server
