#include "SceneNetworkAdapter.h"

#include <cmath>

namespace SoH::Network::SceneNetworkAdapter {

bool IsSane(const NetworkSceneEntryIntentPacket& packet) {
    return packet.sequence != 0 && packet.lifeEpoch != 0;
}

Game::Simulation::SceneEntryCommand ToCommand(
    const NetworkSceneEntryIntentPacket& packet) {
    return { -1, packet.sequence, packet.lifeEpoch };
}

bool IsSane(const NetworkSceneEntryStatePacket& packet) {
    const auto saneCoordinate = [](float value) {
        return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
    };
    return packet.playerId >= 0 && packet.entityGeneration != 0 && packet.lifeEpoch != 0 &&
           packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) && saneCoordinate(packet.x) &&
           saneCoordinate(packet.y) && saneCoordinate(packet.z) && packet.accepted <= 1;
}

Game::Client::LocalSceneAuthority ToAuthority(
    const NetworkSceneEntryStatePacket& packet) {
    Game::Client::LocalSceneAuthority authority{};
    authority.playerId = packet.playerId;
    authority.entity = { packet.entityIndex, packet.entityGeneration };
    authority.requestSequence = packet.requestSequence;
    authority.lifeEpoch = packet.lifeEpoch;
    authority.sceneId = packet.sceneId;
    authority.position = { packet.x, packet.y, packet.z };
    authority.heading = packet.heading;
    authority.accepted = packet.accepted != 0;
    return authority;
}

NetworkSceneEntryStatePacket ToPacket(const Game::Simulation::PlayerSnapshot& snapshot,
                                      uint32_t requestSequence, bool accepted) {
    constexpr float radiansToBinaryAngle = 32768.0f / 3.14159265358979323846f;
    return {
        snapshot.ownerPlayerId,
        snapshot.entity.index,
        snapshot.entity.generation,
        requestSequence,
        snapshot.lifeEpoch,
        snapshot.sceneId,
        snapshot.position.x,
        snapshot.position.y,
        snapshot.position.z,
        static_cast<short>(std::lround(snapshot.headingRadians * radiansToBinaryAngle)),
        static_cast<unsigned char>(accepted ? 1 : 0),
    };
}

} // namespace SoH::Network::SceneNetworkAdapter
