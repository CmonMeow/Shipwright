#include "CorpseNetworkAdapter.h"

#include <cmath>

namespace Game::Multiplayer::CorpseNetworkAdapter {
NetworkCorpseStatePacket ToPacket(const Game::Simulation::CorpseSnapshot& corpse,
                                  uint32_t sequence, bool active) {
    NetworkCorpseStatePacket packet{};
    packet.entityIndex = corpse.entity.index;
    packet.entityGeneration = corpse.entity.generation;
    packet.sequence = sequence;
    packet.sourcePlayerId = corpse.pose.sourcePlayerId;
    packet.sourcePlayerEntityIndex = corpse.pose.sourcePlayerEntity.index;
    packet.sourcePlayerEntityGeneration = corpse.pose.sourcePlayerEntity.generation;
    packet.sourceLifeEpoch = corpse.pose.sourceLifeEpoch;
    packet.sceneId = corpse.pose.sceneId;
    packet.roomId = corpse.pose.roomId;
    packet.x = corpse.pose.position.x;
    packet.y = corpse.pose.position.y;
    packet.z = corpse.pose.position.z;
    for (size_t axis = 0; axis < 3; ++axis) {
        packet.rotation[axis] = corpse.pose.rotation[axis];
    }
    packet.selectedWeapon = corpse.pose.selectedWeapon;
    packet.active = active ? 1 : 0;
    return packet;
}

Game::Client::CorpsePresentationState ToPresentationState(
    const NetworkCorpseStatePacket& packet) {
    Game::Client::CorpsePresentationState state{};
    state.entity = { packet.entityIndex, packet.entityGeneration };
    state.sourcePlayerId = packet.sourcePlayerId;
    state.sourcePlayerEntity = { packet.sourcePlayerEntityIndex,
                                 packet.sourcePlayerEntityGeneration };
    state.sourceLifeEpoch = packet.sourceLifeEpoch;
    state.sceneId = packet.sceneId;
    state.roomId = packet.roomId;
    state.x = packet.x;
    state.y = packet.y;
    state.z = packet.z;
    for (size_t axis = 0; axis < 3; ++axis) {
        state.rotation[axis] = packet.rotation[axis];
    }
    state.selectedWeapon = packet.selectedWeapon;
    state.active = packet.active != 0;
    return state;
}

bool IsSane(const NetworkCorpseStatePacket& packet) {
    const auto saneCoordinate = [](float value) {
        return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
    };
    return packet.entityGeneration != 0 && packet.sequence != 0 &&
           packet.sourcePlayerId >= 0 &&
           packet.sourcePlayerEntityGeneration != 0 &&
           packet.sourceLifeEpoch != 0 &&
           packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.roomId >= -1 && packet.roomId < 256 && saneCoordinate(packet.x) &&
           saneCoordinate(packet.y) && saneCoordinate(packet.z) && packet.selectedWeapon <= 4 &&
           packet.active <= 1;
}

} // namespace Game::Multiplayer::CorpseNetworkAdapter
