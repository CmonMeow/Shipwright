#pragma once

#include "NetworkProtocol.h"
#include "platform/client/CorpsePresentationRegistry.h"
#include "platform/simulation/CorpseSimulation.h"

namespace Game::Multiplayer::CorpseNetworkAdapter {

NetworkCorpseStatePacket ToPacket(const Game::Simulation::CorpseSnapshot& corpse,
                                  uint32_t sequence, bool active = true);
Game::Client::CorpsePresentationState ToPresentationState(
    const NetworkCorpseStatePacket& packet);
bool IsSane(const NetworkCorpseStatePacket& packet);

} // namespace Game::Multiplayer::CorpseNetworkAdapter
