#pragma once

#include "NetworkProtocol.h"
#include "../../platform/simulation/PlayerSimulation.h"
#include "../../platform/simulation/ServerWorld.h"

namespace SoH::Network::PlayerSimulationNetworkAdapter {

Game::Simulation::PlayerCommand ToCommand(const NetworkPlayerCommandPacket& packet);
Game::Simulation::WeaponSelectionCommand ToCommand(
    const NetworkWeaponSelectionIntentPacket& packet);
Game::Simulation::PlayerSnapshot ToSnapshot(const NetworkPlayerSnapshotPacket& packet);
NetworkPlayerCommandPacket ToPacket(const Game::Simulation::PlayerCommand& command);
NetworkPlayerSnapshotPacket ToPacket(const Game::Simulation::PlayerSnapshot& snapshot);

bool IsSane(const NetworkPlayerCommandPacket& packet);
bool IsSane(const NetworkWeaponSelectionIntentPacket& packet);
bool IsSane(const Game::Simulation::PlayerCommand& command);
bool IsSane(const NetworkPlayerSnapshotPacket& packet);

} // namespace SoH::Network::PlayerSimulationNetworkAdapter
