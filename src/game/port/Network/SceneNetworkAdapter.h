#pragma once

#include "NetworkProtocol.h"
#include "../../platform/client/LocalSceneAdmission.h"
#include "../../platform/simulation/PlayerSimulation.h"
#include "../../platform/simulation/ServerWorld.h"

namespace SoH::Network::SceneNetworkAdapter {

bool IsSane(const NetworkSceneEntryIntentPacket& packet);
bool IsSane(const NetworkSceneEntryStatePacket& packet);
Game::Simulation::SceneEntryCommand ToCommand(
    const NetworkSceneEntryIntentPacket& packet);
Game::Client::LocalSceneAuthority ToAuthority(
    const NetworkSceneEntryStatePacket& packet);
NetworkSceneEntryStatePacket ToPacket(const Game::Simulation::PlayerSnapshot& snapshot,
                                      uint32_t requestSequence, bool accepted);

} // namespace SoH::Network::SceneNetworkAdapter
