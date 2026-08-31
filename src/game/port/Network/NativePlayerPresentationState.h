#pragma once

#include "ClientPlayerActionPresentationPolicy.h"
#include "../../platform/client/RemoteFishingEntityState.h"
#include "../../platform/replication/FishingPresentationState.h"
#include "../../platform/simulation/PlayerSimulation.h"

#include <cstdint>

namespace SoH::Network {

inline constexpr uint32_t NATIVE_PLAYER_VISIBLE = 1U << 0;
inline constexpr uint32_t NATIVE_PLAYER_READY_TO_FIRE = 1U << 3;
inline constexpr uint32_t NATIVE_PLAYER_DEAD = 1U << 4;
inline constexpr uint32_t NATIVE_PLAYER_SHIELDING = 1U << 5;

// Ocarina-facing presentation model composed from protocol-independent,
// authoritative client replicas. It contains no packet types or Actor pointer.
struct NativePlayerPresentationState {
    int32_t playerId = 0;
    int32_t sceneId = 0;
    int32_t roomId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int16_t rotationX = 0;
    int16_t rotationY = 0;
    int16_t rotationZ = 0;
    int16_t aimPitch = 0;
    int16_t aimYaw = 0;
    float speed = 0.0f;
    uint32_t stateFlags = 0;
    uint8_t modelGroup = 0;
    uint8_t itemAction = 0;
    uint8_t fishingState = 0;
    int16_t upperLimbRot[3]{};
    int16_t headLimbRot[3]{};
    float bowStringScale = 0.0f;
    ClientPlayerBaseAnimation baseAnimation = ClientPlayerBaseAnimation::Idle;
    ClientPlayerUpperAnimation upperAnimation = ClientPlayerUpperAnimation::None;
    bool synchronizeBaseAnimation = false;
    bool loopBaseAnimationProgress = false;
    float baseAnimationProgress = 0.0f;
    float baseAnimationProgressPerSecond = 0.0f;
    double baseAnimationSampleSeconds = 0.0;
    float fishingRodTipOffset[3]{};
    float fishingLureOffset[3]{};
    float fishingLureDrawOffset[3]{};
    float fishingRodBendY = 0.0f;
    float fishingRodBendX = 0.0f;
    float fishingRodTwist = 0.0f;
    float fishingRodCastX = 0.0f;
    float fishingLureRot[3]{};
    float fishingLureSpin = 0.0f;
    float fishingLureZOffset = 0.0f;
    float fishingLureHookOffsets[2][3]{};
    float fishingLureHookRot[2][2]{};
    float fishingLineScale = 0.0f;
    float fishingLineGravity = 0.0f;
    uint8_t fishingLureType = 0;
    uint8_t fishingLineSpooled = 0;
    uint8_t fishingLineHooked = 0;
    uint8_t fishingSinkingLureSegmentIndex = 0;
    uint8_t fishingSinkingLureUnderwater = 0;
    uint8_t fishingFishActive = 0;
    uint8_t fishingFishIsLoach = 0;
    float fishingFishOffset[3]{};
    int16_t fishingFishRot[3]{};
    int16_t fishingFishLimbRot[8]{};
    float fishingFishLength = 0.0f;
};

class NativePlayerPresentationComposer final {
  public:
    static void ApplyEquipment(NativePlayerPresentationState& state,
                               ClientEquipmentPresentation equipment);
    static void ApplySnapshot(NativePlayerPresentationState& state,
                              const Game::Simulation::PlayerSnapshot& snapshot,
                              double receivedSeconds);
    static void ApplyFishingPresentation(
        NativePlayerPresentationState& state,
        const Game::Replication::FishingPresentationState& presentation);
    static void ApplyAuthoritativeFishing(
        NativePlayerPresentationState& state,
        const Game::Client::RemoteFishingEntityState& entities);
};

} // namespace SoH::Network
