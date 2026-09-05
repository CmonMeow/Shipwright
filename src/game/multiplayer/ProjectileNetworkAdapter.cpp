#include "ProjectileNetworkAdapter.h"

#include <cmath>
#include <climits>

namespace Game::Multiplayer::ProjectileNetworkAdapter {

Game::Simulation::ArrowFireCommand ToCommand(
    const NetworkArrowFireIntentPacket& packet) {
    return { -1, packet.sequence, packet.lifeEpoch, packet.clientTick,
             packet.heading, packet.aimPitch };
}

NetworkProjectileStatePacket ToPacket(const Game::Simulation::ArrowSnapshot& arrow) {
    unsigned char phase = NETWORK_ARROW_FLYING;
    if (arrow.phase == Game::Simulation::ArrowPhase::Stuck) {
        phase = NETWORK_ARROW_STUCK;
    } else if (arrow.phase == Game::Simulation::ArrowPhase::Blocked) {
        phase = NETWORK_ARROW_BLOCKED;
    }
    return { arrow.ownerPlayerId, arrow.replicationId, arrow.entity.index,
             arrow.entity.generation, arrow.sceneId, arrow.sequence,
             static_cast<unsigned char>(arrow.active), NETWORK_PROJECTILE_ARROW, phase,
             arrow.projectileType, arrow.position.x, arrow.position.y, arrow.position.z,
             arrow.rotationX, arrow.rotationY, arrow.rotationZ,
             arrow.velocity.x, arrow.velocity.y, arrow.velocity.z, arrow.body.playerId,
             arrow.body.lifeEpoch, static_cast<uint8_t>(arrow.body.region),
             arrow.body.offset.x, arrow.body.offset.y, arrow.body.offset.z,
             arrow.body.direction.x, arrow.body.direction.y, arrow.body.direction.z };
}

Game::Client::RemoteProjectileReplicaState ToPresentationState(
    const NetworkProjectileStatePacket& packet) {
    Game::Client::RemoteProjectilePhase phase =
        Game::Client::RemoteProjectilePhase::ArrowFlying;
    if (packet.phase == NETWORK_ARROW_STUCK) {
        phase = Game::Client::RemoteProjectilePhase::ArrowStuck;
    } else if (packet.phase == NETWORK_ARROW_BLOCKED) {
        phase = Game::Client::RemoteProjectilePhase::ArrowBlocked;
    }
    return {
        { packet.entityIndex, packet.entityGeneration },
        { packet.playerId, packet.projectileId, packet.projectileKind },
        packet.sceneId,
        packet.sequence,
        packet.active != 0,
        phase,
        packet.projectileType,
        { packet.x, packet.y, packet.z },
        { packet.velocityX, packet.velocityY, packet.velocityZ },
        packet.rotationX,
        packet.rotationY,
        packet.rotationZ,
        { packet.bodyPlayerId, packet.bodyLifeEpoch, static_cast<Game::Simulation::PlayerHitRegion>(packet.bodyRegion),
          { packet.bodyOffsetX, packet.bodyOffsetY, packet.bodyOffsetZ },
          { packet.bodyDirectionX, packet.bodyDirectionY, packet.bodyDirectionZ } },
    };
}

NetworkProjectileLifecyclePacket ToLifecyclePacket(
    const Game::Replication::ReplicatedOwnedEntity& entity, bool active) {
    return { entity.key.ownerPlayerId, entity.key.replicationId, entity.entity.index,
             entity.entity.generation, entity.sceneId, NETWORK_PROJECTILE_ARROW,
             static_cast<unsigned char>(active ? 1 : 0) };
}

Game::Client::RemoteProjectileReplicaState ToRetiredPresentationState(
    const NetworkProjectileLifecyclePacket& lifecycle) {
    Game::Client::RemoteProjectileReplicaState retired{};
    retired.entity = { lifecycle.entityIndex, lifecycle.entityGeneration };
    retired.logicalId = {
        lifecycle.playerId, lifecycle.projectileId, lifecycle.projectileKind
    };
    retired.sceneId = lifecycle.sceneId;
    retired.active = false;
    return retired;
}

bool IsSane(const NetworkProjectileLifecyclePacket& packet) {
    return packet.playerId >= 0 && packet.projectileId > 0 &&
           packet.projectileId < INT32_MAX && packet.entityGeneration != 0 &&
           packet.sceneId >= 0 && packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.projectileKind == NETWORK_PROJECTILE_ARROW && packet.active <= 1;
}

bool IsSane(const NetworkProjectileStatePacket& packet) {
    const auto saneFloat = [](float value) {
        return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
    };
    return packet.playerId >= 0 && packet.projectileId > 0 &&
           packet.projectileId < INT32_MAX && packet.entityGeneration != 0 &&
           packet.sceneId >= 0 && packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.active <= 1 && packet.projectileKind == NETWORK_PROJECTILE_ARROW &&
           packet.phase <= NETWORK_ARROW_BLOCKED && packet.projectileType <= 8 &&
           saneFloat(packet.x) && saneFloat(packet.y) && saneFloat(packet.z) &&
           saneFloat(packet.velocityX) && saneFloat(packet.velocityY) &&
           saneFloat(packet.velocityZ) && packet.bodyPlayerId >= -1 &&
           packet.bodyRegion < Game::Simulation::kPlayerHitRegionCount &&
           saneFloat(packet.bodyOffsetX) && saneFloat(packet.bodyOffsetY) && saneFloat(packet.bodyOffsetZ) &&
           saneFloat(packet.bodyDirectionX) && saneFloat(packet.bodyDirectionY) && saneFloat(packet.bodyDirectionZ) &&
           (packet.bodyPlayerId < 0 || (packet.bodyLifeEpoch != 0 && packet.phase == NETWORK_ARROW_STUCK && packet.bodyRegion != 0));
}

bool IsSane(const NetworkArrowFireIntentPacket& packet) {
    return packet.sequence != 0 && packet.lifeEpoch != 0;
}

bool IsSane(const NetworkProjectileIntentResultPacket& packet) {
    return packet.sequence != 0 && packet.lifeEpoch != 0 &&
           (packet.accepted ? packet.projectileId > 0 : packet.projectileId == 0) &&
           packet.intentKind == NETWORK_PROJECTILE_INTENT_ARROW_FIRE &&
           packet.accepted <= 1;
}

Game::Client::LocalProjectileIntentDecision ToIntentDecision(
    const NetworkProjectileIntentResultPacket& packet) {
    return { packet.sequence, packet.projectileId,
             Game::Client::LocalProjectileIntentKind::FireArrow,
             packet.accepted != 0 };
}

LifecycleApplyResult ApplyLifecycle(
    const NetworkProjectileLifecyclePacket& packet,
    Game::Replication::ProjectileLifetimeRegistry& lifetimes) {
    LifecycleApplyResult result{};
    if (!IsSane(packet)) return result;
    result.logicalId = { packet.playerId, packet.projectileId, packet.projectileKind };
    result.entity = { packet.entityIndex, packet.entityGeneration };
    if (packet.active) {
        result.previousEntity = lifetimes.ActiveEntity(result.logicalId);
        if (!lifetimes.Establish(result.logicalId, result.entity)) return {};
        result.kind = result.previousEntity && *result.previousEntity != result.entity
                          ? LifecycleApplyKind::Replaced
                          : LifecycleApplyKind::Established;
        return result;
    }
    if (!lifetimes.Retire(result.logicalId, result.entity)) return {};
    result.kind = LifecycleApplyKind::Retired;
    return result;
}

bool MatchesActiveLifetime(
    const NetworkProjectileStatePacket& packet,
    const Game::Replication::ProjectileLifetimeRegistry& lifetimes) {
    return IsSane(packet) && lifetimes.Matches(
        { packet.playerId, packet.projectileId, packet.projectileKind },
        { packet.entityIndex, packet.entityGeneration });
}

} // namespace Game::Multiplayer::ProjectileNetworkAdapter
