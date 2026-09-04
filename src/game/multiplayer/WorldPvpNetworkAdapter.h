#pragma once

#include "NetworkProtocol.h"
#include "platform/client/ClientWorldState.h"
#include "platform/simulation/ObjectiveSimulation.h"
#include "platform/simulation/StructureSimulation.h"
#include "platform/simulation/ServerWorld.h"

namespace Game::Multiplayer::WorldPvpNetworkAdapter {

NetworkObjectiveStatePacket ToPacket(
    const Game::Simulation::ObjectiveSnapshot& objective, uint32_t sequence,
    bool active = true);
NetworkStructureStatePacket ToPacket(
    const Game::Simulation::StructureSnapshot& structure, uint32_t sequence,
    bool active = true);
NetworkStrategicTopologyPacket ToPacket(
    const std::vector<Game::Simulation::StrategicSiteDefinition>& sites,
    const std::vector<Game::Simulation::SupplyRouteDefinition>& routes,
    uint32_t revision);
NetworkStrategicTopologyPacket ToPacket(
    const std::vector<Game::Simulation::StrategicSiteDefinition>& sites,
    const std::vector<Game::Simulation::SupplyRouteDefinition>& routes,
    const std::vector<Game::Simulation::InfluenceRegionAdjacencyDefinition>&
        adjacencies,
    uint32_t revision);
Game::Simulation::ObjectiveSnapshot ToSnapshot(
    const NetworkObjectiveStatePacket& packet);
Game::Simulation::StructureSnapshot ToSnapshot(
    const NetworkStructureStatePacket& packet);
Game::Client::ReplicatedObjectiveState ToClientState(
    const NetworkObjectiveStatePacket& packet);
Game::Client::ReplicatedStructureState ToClientState(
    const NetworkStructureStatePacket& packet);
Game::Client::ReplicatedStrategicTopologyState ToClientState(
    const NetworkStrategicTopologyPacket& packet);
Game::Simulation::StructureActionCommand ToCommand(
    const NetworkStructureActionPacket& packet);

bool IsSane(const NetworkObjectiveStatePacket& packet);
bool IsSane(const NetworkStructureStatePacket& packet);
bool IsSane(const NetworkStructureActionPacket& packet);
bool IsSane(const NetworkStrategicTopologyPacket& packet);

} // namespace Game::Multiplayer::WorldPvpNetworkAdapter
