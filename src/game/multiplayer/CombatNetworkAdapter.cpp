#include "CombatNetworkAdapter.h"

#include <cmath>

namespace Game::Multiplayer::CombatNetworkAdapter {

NetworkCombatResultPacket ToPacket(const Game::Simulation::CombatResultEvent& result) {
    return {
        result.eventId,
        result.sourcePlayerId,
        result.targetPlayerId,
        result.sourceEntity.index,
        result.sourceEntity.generation,
        result.targetEntity.index,
        result.targetEntity.generation,
        result.sceneId,
        static_cast<unsigned char>(result.attackKind),
        static_cast<unsigned char>(result.result),
        result.damage,
        static_cast<unsigned char>(result.hitRegion),
        result.impactHeading,
        result.impactPosition.x,
        result.impactPosition.y,
        result.impactPosition.z,
    };
}

Game::Simulation::CombatResultEvent ToEvent(
    const NetworkCombatResultPacket& packet) {
    return {
        packet.eventId,
        packet.sourcePlayerId,
        packet.targetPlayerId,
        { packet.sourceEntityIndex, packet.sourceEntityGeneration },
        { packet.targetEntityIndex, packet.targetEntityGeneration },
        packet.sceneId,
        static_cast<Game::Simulation::CombatAttackKind>(packet.attackKind),
        static_cast<Game::Simulation::CombatResultKind>(packet.result),
        packet.damage,
        packet.impactYaw,
        { packet.impactX, packet.impactY, packet.impactZ },
        static_cast<Game::Simulation::PlayerHitRegion>(packet.hitRegion),
    };
}

bool IsSane(const NetworkCombatResultPacket& packet) {
    const auto saneCoordinate = [](float value) {
        return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
    };
    if (packet.eventId == 0 || packet.sourcePlayerId < -1 || packet.targetPlayerId < 0 ||
        packet.targetEntityGeneration == 0 || packet.sceneId < 0 || packet.sceneId >= 4096 ||
        packet.attackKind > NETWORK_COMBAT_ENVIRONMENT || packet.result > NETWORK_COMBAT_BLOCKED ||
        packet.hitRegion >= Game::Simulation::kPlayerHitRegionCount ||
        !saneCoordinate(packet.impactX) || !saneCoordinate(packet.impactY) ||
        !saneCoordinate(packet.impactZ)) {
        return false;
    }
    if (packet.sourcePlayerId >= 0 && packet.sourceEntityGeneration == 0) return false;
    if (packet.result == NETWORK_COMBAT_BLOCKED &&
        packet.hitRegion != static_cast<unsigned char>(Game::Simulation::PlayerHitRegion::None)) {
        return false;
    }
    if (packet.result == NETWORK_COMBAT_DAMAGED &&
        (packet.attackKind == NETWORK_COMBAT_MELEE || packet.attackKind == NETWORK_COMBAT_ARROW) &&
        packet.hitRegion == static_cast<unsigned char>(Game::Simulation::PlayerHitRegion::None)) {
        return false;
    }
    return packet.result == NETWORK_COMBAT_BLOCKED ? packet.damage == 0
                                                    : packet.damage > 0 && packet.damage <= 64;
}

bool MatchesActiveLifetimes(
    const NetworkCombatResultPacket& packet,
    const Game::Replication::EntityLifetimeRegistry& activePlayers) {
    if (!activePlayers.Matches(packet.targetPlayerId,
                               { packet.targetEntityIndex, packet.targetEntityGeneration })) {
        return false;
    }
    if (packet.sourcePlayerId < 0) return true;
    return activePlayers.Matches(packet.sourcePlayerId,
                                 { packet.sourceEntityIndex, packet.sourceEntityGeneration });
}

} // namespace Game::Multiplayer::CombatNetworkAdapter
