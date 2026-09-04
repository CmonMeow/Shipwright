#pragma once

#include "NetworkProtocol.h"
#include "platform/replication/EntityLifetimeRegistry.h"
#include "platform/simulation/PlayerSimulation.h"

namespace Game::Multiplayer::CombatNetworkAdapter {

NetworkCombatResultPacket ToPacket(const Game::Simulation::CombatResultEvent& result);
Game::Simulation::CombatResultEvent ToEvent(
    const NetworkCombatResultPacket& packet);
bool IsSane(const NetworkCombatResultPacket& packet);
bool MatchesActiveLifetimes(
    const NetworkCombatResultPacket& packet,
    const Game::Replication::EntityLifetimeRegistry& activePlayers);

} // namespace Game::Multiplayer::CombatNetworkAdapter
