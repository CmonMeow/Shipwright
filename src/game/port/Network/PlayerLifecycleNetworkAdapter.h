#pragma once

#include "NetworkProtocol.h"
#include "../../platform/client/RemotePlayerPresentationRegistry.h"
#include "../../platform/replication/EntityLifetimeRegistry.h"
#include "../../platform/replication/PlayerReplicationSystem.h"

namespace SoH::Network::PlayerLifecycleNetworkAdapter {

NetworkPlayerLifecyclePacket ToPacket(
    const Game::Replication::ReplicatedPlayer& player, bool active);
NetworkPlayerRespawnPacket ToRespawnPacket(
    const Game::Simulation::PlayerSnapshot& snapshot);
bool IsSane(const NetworkPlayerLifecyclePacket& packet);
bool IsSane(const NetworkPlayerRespawnPacket& packet);
bool Apply(const NetworkPlayerLifecyclePacket& packet,
           Game::Replication::EntityLifetimeRegistry& lifetimes);
Game::Client::RemotePlayerPresentationState ToPresentationState(
    const NetworkPlayerLifecyclePacket& packet);
bool MatchesActiveLifetime(
    const NetworkPlayerRespawnPacket& packet,
    const Game::Replication::EntityLifetimeRegistry& lifetimes);
Game::Simulation::PlayerRespawnEvent ToRespawnEvent(
    const NetworkPlayerRespawnPacket& packet);

} // namespace SoH::Network::PlayerLifecycleNetworkAdapter
