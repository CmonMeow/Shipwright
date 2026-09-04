#pragma once

#include "NetworkProtocol.h"
#include "platform/replication/OwnedEntityReplicationSystem.h"
#include "platform/replication/ProjectileLifetimeRegistry.h"
#include "platform/client/RemoteProjectileReplicaStore.h"
#include "platform/client/LocalProjectileIntentStream.h"
#include "platform/simulation/ProjectileSimulation.h"
#include "platform/simulation/ServerWorld.h"

#include <optional>

namespace Game::Multiplayer::ProjectileNetworkAdapter {

enum class LifecycleApplyKind : uint8_t {
    Rejected,
    Established,
    Replaced,
    Retired,
};

struct LifecycleApplyResult {
    LifecycleApplyKind kind = LifecycleApplyKind::Rejected;
    Game::Replication::ProjectileLogicalId logicalId{};
    Game::Simulation::EntityId entity{};
    std::optional<Game::Simulation::EntityId> previousEntity;

    bool Accepted() const { return kind != LifecycleApplyKind::Rejected; }
};

NetworkProjectileStatePacket ToPacket(const Game::Simulation::ArrowSnapshot& arrow);
Game::Client::RemoteProjectileReplicaState ToPresentationState(
    const NetworkProjectileStatePacket& packet);
NetworkProjectileLifecyclePacket ToLifecyclePacket(
    const Game::Replication::ReplicatedOwnedEntity& entity, bool active);
Game::Simulation::ArrowFireCommand ToCommand(
    const NetworkArrowFireIntentPacket& packet);
Game::Client::RemoteProjectileReplicaState ToRetiredPresentationState(
    const NetworkProjectileLifecyclePacket& lifecycle);

bool IsSane(const NetworkProjectileLifecyclePacket& packet);
bool IsSane(const NetworkProjectileStatePacket& packet);
bool IsSane(const NetworkArrowFireIntentPacket& packet);
bool IsSane(const NetworkProjectileIntentResultPacket& packet);
Game::Client::LocalProjectileIntentDecision ToIntentDecision(
    const NetworkProjectileIntentResultPacket& packet);
LifecycleApplyResult ApplyLifecycle(
    const NetworkProjectileLifecyclePacket& packet,
    Game::Replication::ProjectileLifetimeRegistry& lifetimes);
bool MatchesActiveLifetime(
    const NetworkProjectileStatePacket& packet,
    const Game::Replication::ProjectileLifetimeRegistry& lifetimes);

} // namespace Game::Multiplayer::ProjectileNetworkAdapter
