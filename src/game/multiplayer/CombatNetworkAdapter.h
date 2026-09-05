#pragma once

#include "NetworkProtocol.h"
#include "platform/replication/EntityLifetimeRegistry.h"
#include "platform/simulation/PlayerSimulation.h"

#include <map>

namespace Game::Multiplayer::CombatNetworkAdapter {

NetworkCombatResultPacket ToPacket(const Game::Simulation::CombatResultEvent& result);
Game::Simulation::CombatResultEvent ToEvent(
    const NetworkCombatResultPacket& packet);
bool IsSane(const NetworkCombatResultPacket& packet);
bool MatchesActiveLifetimes(
    const NetworkCombatResultPacket& packet,
    const Game::Replication::EntityLifetimeRegistry& activePlayers);
bool MatchesActiveIncarnations(
    const NetworkCombatResultPacket& packet,
    const std::map<int32_t, uint32_t>& activeLifeEpochs);

} // namespace Game::Multiplayer::CombatNetworkAdapter
