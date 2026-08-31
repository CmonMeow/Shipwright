#include "PlayerLifecycleNetworkAdapter.h"

#include <cmath>

namespace SoH::Network::PlayerLifecycleNetworkAdapter {
namespace {

constexpr float kBinaryAngleToRadians = 3.14159265358979323846f / 32768.0f;
constexpr float kRadiansToBinaryAngle = 32768.0f / 3.14159265358979323846f;

int16_t ToBinaryAngle(float radians) {
    return static_cast<int16_t>(std::lround(radians * kRadiansToBinaryAngle));
}

bool IsSaneCoordinate(float value) {
    return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
}

} // namespace

NetworkPlayerLifecyclePacket ToPacket(
    const Game::Replication::ReplicatedPlayer& player, bool active) {
    return { player.playerId, player.entity.index, player.entity.generation,
             player.sceneId, static_cast<unsigned char>(active ? 1 : 0) };
}

NetworkPlayerRespawnPacket ToRespawnPacket(
    const Game::Simulation::PlayerSnapshot& snapshot) {
    return { snapshot.ownerPlayerId, snapshot.entity.index,
             snapshot.entity.generation, snapshot.lifeEpoch,
             snapshot.sceneId, snapshot.serverTick,
             snapshot.position.x, snapshot.position.y, snapshot.position.z,
             ToBinaryAngle(snapshot.headingRadians), snapshot.selectedWeapon };
}

bool IsSane(const NetworkPlayerLifecyclePacket& packet) {
    return packet.playerId >= 0 && packet.entityGeneration != 0 && packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) && packet.active <= 1;
}

bool IsSane(const NetworkPlayerRespawnPacket& packet) {
    return packet.playerId >= 0 && packet.entityGeneration != 0 &&
           packet.lifeEpoch != 0 && packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.serverTick != 0 && IsSaneCoordinate(packet.x) &&
           IsSaneCoordinate(packet.y) && IsSaneCoordinate(packet.z) &&
           packet.selectedWeapon <= 4;
}

bool Apply(const NetworkPlayerLifecyclePacket& packet,
           Game::Replication::EntityLifetimeRegistry& lifetimes) {
    if (!IsSane(packet)) return false;
    const Game::Simulation::EntityId entity{ packet.entityIndex, packet.entityGeneration };
    return packet.active ? lifetimes.Establish(packet.playerId, entity)
                         : lifetimes.Retire(packet.playerId, entity);
}

Game::Client::RemotePlayerPresentationState ToPresentationState(
    const NetworkPlayerLifecyclePacket& packet) {
    return { { packet.entityIndex, packet.entityGeneration }, packet.playerId,
             packet.sceneId, packet.active != 0 };
}

bool MatchesActiveLifetime(
    const NetworkPlayerRespawnPacket& packet,
    const Game::Replication::EntityLifetimeRegistry& lifetimes) {
    return IsSane(packet) &&
           lifetimes.Matches(packet.playerId,
                             { packet.entityIndex, packet.entityGeneration });
}

Game::Simulation::PlayerRespawnEvent ToRespawnEvent(
    const NetworkPlayerRespawnPacket& packet) {
    return { packet.playerId,
             { packet.entityIndex, packet.entityGeneration },
             packet.lifeEpoch,
             packet.sceneId,
             packet.serverTick,
             { packet.x, packet.y, packet.z },
             static_cast<float>(packet.heading) * kBinaryAngleToRadians,
             packet.selectedWeapon };
}

} // namespace SoH::Network::PlayerLifecycleNetworkAdapter
