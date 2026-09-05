#include "FishingNetworkAdapter.h"
#include "platform/replication/FishingPresentationValidation.h"

#include <cmath>

namespace Game::Multiplayer::FishingNetworkAdapter {

Game::Simulation::LureControlCommand ToCommand(
    const NetworkLureControlIntentPacket& packet) {
    return { -1, packet.sequence, packet.lifeEpoch,
             (packet.controlFlags & NETWORK_LURE_DEPLOYED) != 0,
             (packet.controlFlags & NETWORK_LURE_REEL_HELD) != 0 };
}

Game::Simulation::FishActionCommand ToCommand(
    const NetworkFishIntentPacket& packet) {
    return { -1, packet.sequence, packet.lifeEpoch,
             packet.action == NETWORK_FISH_INTENT_RELEASE
                 ? Game::Simulation::FishActionKind::Release
                 : Game::Simulation::FishActionKind::Hook };
}
namespace {

bool SaneCoordinate(float value) {
    return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
}

bool IsSaneFishIdentity(int32_t sceneId, uint32_t spawnKey) {
    return sceneId >= 0 && sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           spawnKey != 0;
}

LifetimeApplyResult Apply(int32_t ownerPlayerId, Game::Simulation::EntityId entity,
                          bool active,
                          Game::Replication::EntityLifetimeRegistry& lifetimes) {
    LifetimeApplyResult result{};
    result.ownerPlayerId = ownerPlayerId;
    result.entity = entity;
    if (ownerPlayerId < 0 || !entity.Valid()) return result;
    if (active) {
        result.previousEntity = lifetimes.ActiveEntity(ownerPlayerId);
        if (!lifetimes.Establish(ownerPlayerId, entity)) return {};
        result.kind = result.previousEntity && *result.previousEntity != entity
                          ? LifetimeApplyKind::Replaced
                          : LifetimeApplyKind::Established;
        return result;
    }
    if (!lifetimes.Retire(ownerPlayerId, entity)) return {};
    result.kind = LifetimeApplyKind::Retired;
    return result;
}

template <typename Packet>
Game::Replication::FishingPresentationState PresentationStateFrom(const Packet& packet) {
    Game::Replication::FishingPresentationState state{};
    state.sequence = static_cast<uint32_t>(packet.sequence);
    state.state = packet.fishingState;
    state.rodBendY = packet.fishingRodBendY;
    state.rodBendX = packet.fishingRodBendX;
    state.rodTwist = packet.fishingRodTwist;
    state.rodCastX = packet.fishingRodCastX;
    state.lureSpin = packet.fishingLureSpin;
    state.lureZOffset = packet.fishingLureZOffset;
    state.lineScale = packet.fishingLineScale;
    state.lineGravity = packet.fishingLineGravity;
    state.lineSpooled = packet.fishingLineSpooled;
    state.sinkingLureSegmentIndex = packet.fishingSinkingLureSegmentIndex;
    state.sinkingLureUnderwater = packet.fishingSinkingLureUnderwater;
    for (size_t axis = 0; axis < 3; ++axis) {
        state.lureDrawOffset[axis] = packet.fishingLureDrawOffset[axis];
        state.lureRotation[axis] = packet.fishingLureRot[axis];
        state.fishRotation[axis] = packet.fishingFishRot[axis];
    }
    static_assert(Game::Replication::kFishingPresentationHookCount == 2);
    for (size_t hook = 0; hook < Game::Replication::kFishingPresentationHookCount; ++hook) {
        for (size_t axis = 0; axis < 3; ++axis) {
            state.lureHookOffsets[hook][axis] = packet.fishingLureHookOffsets[hook][axis];
        }
        for (size_t axis = 0; axis < 2; ++axis) {
            state.lureHookRotations[hook][axis] = packet.fishingLureHookRot[hook][axis];
        }
    }
    static_assert(Game::Replication::kFishingPresentationFishLimbCount == 8);
    for (size_t limb = 0; limb < Game::Replication::kFishingPresentationFishLimbCount; ++limb) {
        state.fishLimbRotation[limb] = packet.fishingFishLimbRot[limb];
    }
    return state;
}

template <typename Packet>
bool IsSanePresentationBody(const Packet& packet) {
    if (packet.sequence <= 0 || packet.fishingState > 5 ||
        packet.fishingLineSpooled >= NETWORK_FISHING_LINE_POINT_COUNT ||
        packet.fishingSinkingLureSegmentIndex >= 20 ||
        packet.fishingSinkingLureUnderwater > 1) {
        return false;
    }
    return Game::Replication::FishingPresentationPoseIsBounded(
        PresentationStateFrom(packet));
}

} // namespace

NetworkFishStatePacket ToPacket(const Game::Simulation::FishSnapshot& fish,
                                uint32_t sequence, bool active) {
    const auto& identity = fish.identity;
    return { fish.ownerPlayerId, fish.ownerLifeEpoch, fish.entity.index,
             fish.entity.generation, sequence,
             identity.sceneId, identity.spawnKey,
             fish.position.x, fish.position.y, fish.position.z,
             static_cast<unsigned char>(fish.species), fish.length,
             static_cast<unsigned char>(active) };
}

NetworkLureStatePacket ToPacket(const Game::Simulation::FishingLureSnapshot& lure,
                                uint32_t sequence, bool active) {
    return { lure.ownerPlayerId, lure.ownerLifeEpoch, lure.entity.index,
             lure.entity.generation, sequence,
             lure.sceneId, lure.position.x, lure.position.y, lure.position.z,
             static_cast<unsigned char>(lure.phase), lure.lureType,
             static_cast<unsigned char>(active) };
}

Game::Client::RemoteFishEntity ToRemoteEntity(
    const NetworkFishStatePacket& packet) {
    return {
        packet.ownerPlayerId,
        { packet.entityIndex, packet.entityGeneration },
        { packet.sceneId, packet.spawnKey },
        packet.x,
        packet.y,
        packet.z,
        packet.length,
        static_cast<Game::Simulation::FishSpecies>(packet.species),
        packet.ownerLifeEpoch,
        packet.active != 0,
    };
}

Game::Client::RemoteLureEntity ToRemoteEntity(
    const NetworkLureStatePacket& packet) {
    return {
        packet.ownerPlayerId,
        { packet.entityIndex, packet.entityGeneration },
        packet.sceneId,
        packet.x,
        packet.y,
        packet.z,
        packet.phase,
        packet.lureType,
        packet.ownerLifeEpoch,
        packet.active != 0,
    };
}

Game::Replication::FishingPresentationState ToState(
    const NetworkFishingPresentationPacket& packet) {
    Game::Replication::FishingPresentationState state = PresentationStateFrom(packet);
    state.playerId = packet.playerId;
    state.entity = { packet.entityIndex, packet.entityGeneration };
    state.sceneId = packet.sceneId;
    state.lifeEpoch = packet.lifeEpoch;
    return state;
}

Game::Replication::FishingPresentationIntent ToIntent(
    const NetworkFishingPresentationIntentPacket& packet) {
    return { packet.lifeEpoch, PresentationStateFrom(packet) };
}

NetworkFishingPresentationPacket ToPacket(
    const Game::Replication::FishingPresentationState& state) {
    NetworkFishingPresentationPacket packet{};
    packet.playerId = state.playerId;
    packet.entityIndex = state.entity.index;
    packet.entityGeneration = state.entity.generation;
    packet.sceneId = state.sceneId;
    packet.lifeEpoch = state.lifeEpoch;
    packet.sequence = state.sequence;
    packet.fishingState = state.state;
    packet.fishingRodBendY = state.rodBendY;
    packet.fishingRodBendX = state.rodBendX;
    packet.fishingRodTwist = state.rodTwist;
    packet.fishingRodCastX = state.rodCastX;
    packet.fishingLureSpin = state.lureSpin;
    packet.fishingLureZOffset = state.lureZOffset;
    packet.fishingLineScale = state.lineScale;
    packet.fishingLineGravity = state.lineGravity;
    packet.fishingLineSpooled = state.lineSpooled;
    packet.fishingSinkingLureSegmentIndex = state.sinkingLureSegmentIndex;
    packet.fishingSinkingLureUnderwater = state.sinkingLureUnderwater;
    for (size_t axis = 0; axis < 3; ++axis) {
        packet.fishingLureDrawOffset[axis] = state.lureDrawOffset[axis];
        packet.fishingLureRot[axis] = state.lureRotation[axis];
        packet.fishingFishRot[axis] = state.fishRotation[axis];
    }
    for (size_t hook = 0; hook < Game::Replication::kFishingPresentationHookCount; ++hook) {
        for (size_t axis = 0; axis < 3; ++axis) {
            packet.fishingLureHookOffsets[hook][axis] = state.lureHookOffsets[hook][axis];
        }
        for (size_t axis = 0; axis < 2; ++axis) {
            packet.fishingLureHookRot[hook][axis] = state.lureHookRotations[hook][axis];
        }
    }
    for (size_t limb = 0; limb < Game::Replication::kFishingPresentationFishLimbCount; ++limb) {
        packet.fishingFishLimbRot[limb] = state.fishLimbRotation[limb];
    }
    return packet;
}

NetworkFishingPresentationIntentPacket ToIntentPacket(
    const Game::Replication::FishingPresentationState& state) {
    NetworkFishingPresentationIntentPacket packet{};
    packet.sequence = static_cast<int32_t>(state.sequence);
    packet.fishingState = state.state;
    packet.fishingRodBendY = state.rodBendY;
    packet.fishingRodBendX = state.rodBendX;
    packet.fishingRodTwist = state.rodTwist;
    packet.fishingRodCastX = state.rodCastX;
    packet.fishingLureSpin = state.lureSpin;
    packet.fishingLureZOffset = state.lureZOffset;
    packet.fishingLineScale = state.lineScale;
    packet.fishingLineGravity = state.lineGravity;
    packet.fishingLineSpooled = state.lineSpooled;
    packet.fishingSinkingLureSegmentIndex = state.sinkingLureSegmentIndex;
    packet.fishingSinkingLureUnderwater = state.sinkingLureUnderwater;
    for (size_t axis = 0; axis < 3; ++axis) {
        packet.fishingLureDrawOffset[axis] = state.lureDrawOffset[axis];
        packet.fishingLureRot[axis] = state.lureRotation[axis];
        packet.fishingFishRot[axis] = state.fishRotation[axis];
    }
    for (size_t hook = 0; hook < Game::Replication::kFishingPresentationHookCount; ++hook) {
        for (size_t axis = 0; axis < 3; ++axis) {
            packet.fishingLureHookOffsets[hook][axis] = state.lureHookOffsets[hook][axis];
        }
        for (size_t axis = 0; axis < 2; ++axis) {
            packet.fishingLureHookRot[hook][axis] = state.lureHookRotations[hook][axis];
        }
    }
    for (size_t limb = 0; limb < Game::Replication::kFishingPresentationFishLimbCount; ++limb) {
        packet.fishingFishLimbRot[limb] = state.fishLimbRotation[limb];
    }
    return packet;
}

bool IsSane(const NetworkFishingPresentationPacket& packet) {
    return packet.playerId >= 0 && packet.entityGeneration != 0 && packet.lifeEpoch != 0 &&
           packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           IsSanePresentationBody(packet);
}

bool IsSane(const NetworkFishingPresentationIntentPacket& packet) {
    return packet.lifeEpoch != 0 && IsSanePresentationBody(packet);
}

bool IsSane(const NetworkFishIntentPacket& packet) {
    return packet.sequence != 0 && packet.lifeEpoch != 0 &&
           (packet.action == NETWORK_FISH_INTENT_HOOK ||
            packet.action == NETWORK_FISH_INTENT_RELEASE);
}

bool IsSane(const NetworkFishStatePacket& packet) {
    return packet.ownerPlayerId >= 0 && packet.ownerLifeEpoch != 0 &&
           packet.entityGeneration != 0 &&
           packet.sequence != 0 && packet.active <= 1 &&
           packet.species <= static_cast<unsigned char>(
                                 Game::Simulation::FishSpecies::HylianLoach) &&
           IsSaneFishIdentity(packet.sceneId, packet.spawnKey) &&
           SaneCoordinate(packet.x) && SaneCoordinate(packet.y) &&
           SaneCoordinate(packet.z) &&
           SaneCoordinate(packet.length) && packet.length >= 0.0f && packet.length <= 100.0f;
}

bool IsSane(const NetworkLureControlIntentPacket& packet) {
    return packet.sequence != 0 && packet.lifeEpoch != 0 &&
           (packet.controlFlags & ~NETWORK_LURE_CONTROL_MASK) == 0;
}

bool IsSane(const NetworkLureStatePacket& packet) {
    return packet.ownerPlayerId >= 0 && packet.ownerLifeEpoch != 0 &&
           packet.entityGeneration != 0 &&
           packet.sequence != 0 && packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.phase <= static_cast<unsigned char>(Game::Simulation::FishingLurePhase::Hooked) &&
           packet.lureType <= 2 && packet.active <= 1 &&
           SaneCoordinate(packet.x) && SaneCoordinate(packet.y) &&
           SaneCoordinate(packet.z);
}

LifetimeApplyResult ApplyLifetime(
    const NetworkFishStatePacket& packet,
    Game::Replication::EntityLifetimeRegistry& lifetimes) {
    if (!IsSane(packet)) return {};
    return Apply(packet.ownerPlayerId,
                 { packet.entityIndex, packet.entityGeneration }, packet.active != 0,
                 lifetimes);
}

LifetimeApplyResult ApplyLifetime(
    const NetworkLureStatePacket& packet,
    Game::Replication::EntityLifetimeRegistry& lifetimes) {
    if (!IsSane(packet)) return {};
    return Apply(packet.ownerPlayerId,
                 { packet.entityIndex, packet.entityGeneration }, packet.active != 0,
                 lifetimes);
}

} // namespace Game::Multiplayer::FishingNetworkAdapter
