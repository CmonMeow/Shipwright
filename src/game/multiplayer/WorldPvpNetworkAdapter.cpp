#include "WorldPvpNetworkAdapter.h"

#include <cmath>

namespace Game::Multiplayer::WorldPvpNetworkAdapter {

static_assert(NET_MAX_STRATEGIC_SITES ==
              Game::Simulation::StrategicWorldTopology::kMaximumSites);
static_assert(NET_MAX_SUPPLY_ROUTES ==
              Game::Simulation::StrategicWorldTopology::kMaximumSupplyRoutes);
static_assert(
    NET_MAX_INFLUENCE_ADJACENCIES ==
    Game::Simulation::StrategicWorldTopology::kMaximumInfluenceAdjacencies);

Game::Simulation::StructureActionCommand ToCommand(
    const NetworkStructureActionPacket& packet) {
    return { -1, packet.sequence, packet.lifeEpoch, packet.structureKey,
             packet.action == NETWORK_STRUCTURE_ACTION_BUILD
                 ? Game::Simulation::StructureActionKind::Build
                 : Game::Simulation::StructureActionKind::Repair };
}
namespace {

bool SaneCoordinate(float value) {
    return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
}

} // namespace

NetworkObjectiveStatePacket ToPacket(
    const Game::Simulation::ObjectiveSnapshot& objective, uint32_t sequence,
    bool active) {
    return { objective.entity.index,
             objective.entity.generation,
             sequence,
             objective.objectiveKey,
             objective.sceneId,
             objective.position.x,
             objective.position.y,
             objective.position.z,
             objective.captureRadius,
             objective.captureProgress,
             static_cast<unsigned char>(active),
             static_cast<unsigned char>(objective.owner),
             static_cast<unsigned char>(objective.capturingTeam),
             static_cast<unsigned char>(objective.contested) };
}

NetworkStructureStatePacket ToPacket(
    const Game::Simulation::StructureSnapshot& structure, uint32_t sequence,
    bool active) {
    return { structure.entity.index,
             structure.entity.generation,
             sequence,
             structure.structureKey,
             structure.objectiveKey,
             structure.sceneId,
             structure.position.x,
             structure.position.y,
             structure.position.z,
             structure.health,
             structure.maximumHealth,
             structure.buildProgress,
             structure.requiredBuild,
             static_cast<unsigned char>(active),
             static_cast<unsigned char>(structure.team),
             static_cast<unsigned char>(structure.phase) };
}

NetworkStrategicTopologyPacket ToPacket(
    const std::vector<Game::Simulation::StrategicSiteDefinition>& sites,
    const std::vector<Game::Simulation::SupplyRouteDefinition>& routes,
    uint32_t revision) {
    return ToPacket(sites, routes, {}, revision);
}

NetworkStrategicTopologyPacket ToPacket(
    const std::vector<Game::Simulation::StrategicSiteDefinition>& sites,
    const std::vector<Game::Simulation::SupplyRouteDefinition>& routes,
    const std::vector<Game::Simulation::InfluenceRegionAdjacencyDefinition>&
        adjacencies,
    uint32_t revision) {
    NetworkStrategicTopologyPacket packet{};
    packet.revision = revision;
    packet.sites.reserve(sites.size());
    for (const auto& site : sites) {
        packet.sites.push_back({ site.objectiveKey, site.influenceRegionKey,
                                 static_cast<unsigned char>(site.kind) });
    }
    packet.supplyRoutes.reserve(routes.size());
    for (const auto& route : routes) {
        packet.supplyRoutes.push_back({ route.routeKey, route.sourceObjectiveKey,
                                        route.destinationObjectiveKey });
    }
    packet.influenceAdjacencies.reserve(adjacencies.size());
    for (const auto& adjacency : adjacencies) {
        packet.influenceAdjacencies.push_back(
            { adjacency.adjacencyKey, adjacency.lowerRegionKey,
              adjacency.upperRegionKey });
    }
    return packet;
}

Game::Simulation::ObjectiveSnapshot ToSnapshot(
    const NetworkObjectiveStatePacket& packet) {
    return { { packet.entityIndex, packet.entityGeneration }, packet.objectiveKey,
             packet.sceneId, { packet.x, packet.y, packet.z }, packet.captureRadius,
             static_cast<Game::Simulation::TeamId>(packet.ownerTeam),
             static_cast<Game::Simulation::TeamId>(packet.capturingTeam),
             packet.captureProgress, packet.contested != 0 };
}

Game::Simulation::StructureSnapshot ToSnapshot(
    const NetworkStructureStatePacket& packet) {
    return { { packet.entityIndex, packet.entityGeneration }, packet.structureKey,
             packet.objectiveKey, packet.sceneId, { packet.x, packet.y, packet.z },
             static_cast<Game::Simulation::TeamId>(packet.team),
             static_cast<Game::Simulation::StructurePhase>(packet.phase), packet.health,
             packet.maximumHealth, packet.buildProgress, packet.requiredBuild };
}

Game::Client::ReplicatedObjectiveState ToClientState(
    const NetworkObjectiveStatePacket& packet) {
    return { ToSnapshot(packet), packet.active != 0 };
}

Game::Client::ReplicatedStructureState ToClientState(
    const NetworkStructureStatePacket& packet) {
    return { ToSnapshot(packet), packet.active != 0 };
}

Game::Client::ReplicatedStrategicTopologyState ToClientState(
    const NetworkStrategicTopologyPacket& packet) {
    Game::Client::ReplicatedStrategicTopologyState state{};
    state.revision = packet.revision;
    state.sites.reserve(packet.sites.size());
    for (const auto& site : packet.sites) {
        state.sites.push_back({
            site.objectiveKey,
            static_cast<Game::Simulation::StrategicSiteKind>(site.kind),
            site.influenceRegionKey
        });
    }
    state.supplyRoutes.reserve(packet.supplyRoutes.size());
    for (const auto& route : packet.supplyRoutes) {
        state.supplyRoutes.push_back({ route.routeKey, route.sourceObjectiveKey,
                                       route.destinationObjectiveKey });
    }
    state.influenceAdjacencies.reserve(packet.influenceAdjacencies.size());
    for (const auto& adjacency : packet.influenceAdjacencies) {
        state.influenceAdjacencies.push_back(
            { adjacency.adjacencyKey, adjacency.lowerRegionKey,
              adjacency.upperRegionKey });
    }
    return state;
}

bool IsSane(const NetworkObjectiveStatePacket& packet) {
    return packet.entityGeneration != 0 && packet.sequence != 0 &&
           packet.objectiveKey >= 0 &&
           packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           SaneCoordinate(packet.x) && SaneCoordinate(packet.y) &&
           SaneCoordinate(packet.z) && std::isfinite(packet.captureRadius) &&
           packet.captureRadius >= 1.0f && packet.captureRadius <= 100000.0f &&
           std::isfinite(packet.captureProgress) && packet.captureProgress >= 0.0f &&
           packet.captureProgress <= 100.0f && packet.active <= 1 &&
           packet.ownerTeam <= NETWORK_TEAM_GREEN &&
           packet.capturingTeam <= NETWORK_TEAM_GREEN && packet.contested <= 1;
}

bool IsSane(const NetworkStructureStatePacket& packet) {
    if (packet.entityGeneration == 0 || packet.sequence == 0 ||
        packet.structureKey < 0 ||
        packet.objectiveKey < 0 || packet.sceneId < 0 ||
        packet.sceneId >= static_cast<int32_t>(NET_MAX_WORLD_LEVELS) ||
        !SaneCoordinate(packet.x) || !SaneCoordinate(packet.y) ||
        !SaneCoordinate(packet.z) || packet.maximumHealth == 0 ||
        packet.requiredBuild == 0 || packet.health > packet.maximumHealth ||
        packet.buildProgress > packet.requiredBuild || packet.active > 1 ||
        packet.team > NETWORK_TEAM_GREEN ||
        packet.phase > static_cast<unsigned char>(Game::Simulation::StructurePhase::Destroyed)) {
        return false;
    }
    const auto phase = static_cast<Game::Simulation::StructurePhase>(packet.phase);
    if (phase == Game::Simulation::StructurePhase::Planned) {
        return packet.team == NETWORK_TEAM_NEUTRAL && packet.health == 0 &&
               packet.buildProgress == 0;
    }
    if (phase == Game::Simulation::StructurePhase::Building) {
        return packet.team != NETWORK_TEAM_NEUTRAL && packet.health == 0 &&
               packet.buildProgress > 0 && packet.buildProgress < packet.requiredBuild;
    }
    if (phase == Game::Simulation::StructurePhase::Active) {
        return packet.team != NETWORK_TEAM_NEUTRAL && packet.health > 0 &&
               packet.buildProgress == packet.requiredBuild;
    }
    return packet.team != NETWORK_TEAM_NEUTRAL && packet.health == 0 &&
           packet.buildProgress == packet.requiredBuild;
}

bool IsSane(const NetworkStructureActionPacket& packet) {
    return packet.sequence != 0 && packet.lifeEpoch != 0 && packet.structureKey >= 0 &&
           (packet.action == NETWORK_STRUCTURE_ACTION_BUILD ||
            packet.action == NETWORK_STRUCTURE_ACTION_REPAIR);
}

bool IsSane(const NetworkStrategicTopologyPacket& packet) {
    if (packet.revision == 0 || packet.sites.size() > NET_MAX_STRATEGIC_SITES ||
        packet.supplyRoutes.size() > NET_MAX_SUPPLY_ROUTES ||
        packet.influenceAdjacencies.size() >
            NET_MAX_INFLUENCE_ADJACENCIES) {
        return false;
    }
    const auto state = ToClientState(packet);
    Game::Simulation::StrategicWorldTopology topology;
    return topology.Restore(state.sites, state.supplyRoutes,
                            state.influenceAdjacencies);
}

} // namespace Game::Multiplayer::WorldPvpNetworkAdapter
