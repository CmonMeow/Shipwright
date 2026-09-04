#include "NativePlayerPresentationState.h"

#include "platform/simulation/AuthoritativePlayerPose.h"
#include "platform/simulation/FishingSimulation.h"
#include "global.h"

#include <cmath>
#include <cstring>

namespace Game::Multiplayer {
namespace {

constexpr float kRadiansToBinaryAngle = 32768.0f / 3.14159265358979323846f;
constexpr float kTau = 6.28318530717958647692f;

} // namespace

void NativePlayerPresentationComposer::ApplyEquipment(
    NativePlayerPresentationState& state, ClientEquipmentPresentation equipment) {
    switch (equipment) {
        case ClientEquipmentPresentation::MasterSwordAndShield:
            state.modelGroup = PLAYER_MODELGROUP_SWORD_AND_SHIELD;
            state.itemAction = PLAYER_IA_SWORD_MASTER;
            break;
        case ClientEquipmentPresentation::BiggoronSword:
            state.modelGroup = PLAYER_MODELGROUP_BGS;
            state.itemAction = PLAYER_IA_SWORD_BIGGORON;
            break;
        case ClientEquipmentPresentation::Bow:
            state.modelGroup = PLAYER_MODELGROUP_BOW;
            state.itemAction = PLAYER_IA_BOW;
            break;
        case ClientEquipmentPresentation::FishingPole:
            state.modelGroup = PLAYER_MODELGROUP_FISHING;
            state.itemAction = PLAYER_IA_FISHING_POLE;
            break;
        default:
            state.modelGroup = PLAYER_MODELGROUP_DEFAULT;
            state.itemAction = PLAYER_IA_NONE;
            break;
    }
}

void NativePlayerPresentationComposer::ApplySnapshot(
    NativePlayerPresentationState& state,
    const Game::Simulation::PlayerSnapshot& snapshot,
    double receivedSeconds) {
    state.x = snapshot.position.x;
    state.y = snapshot.position.y;
    state.z = snapshot.position.z;
    state.rotationY = static_cast<int16_t>(
        std::lround(snapshot.headingRadians * kRadiansToBinaryAngle));
    state.aimPitch = static_cast<int16_t>(
        std::lround(snapshot.aimPitchRadians * kRadiansToBinaryAngle));
    state.speed = std::sqrt(snapshot.velocity.x * snapshot.velocity.x +
                            snapshot.velocity.z * snapshot.velocity.z);
    state.stateFlags &= ~(NATIVE_PLAYER_DEAD | NATIVE_PLAYER_SHIELDING |
                          NATIVE_PLAYER_READY_TO_FIRE);
    const auto action = ClientPlayerActionPresentationPolicy::Evaluate(snapshot);
    ApplyEquipment(state, action.equipment);
    if (action.dead) state.stateFlags |= NATIVE_PLAYER_DEAD;
    if (action.blocking) state.stateFlags |= NATIVE_PLAYER_SHIELDING;
    if (action.bowReady) state.stateFlags |= NATIVE_PLAYER_READY_TO_FIRE;
    std::memset(state.upperLimbRot, 0, sizeof(state.upperLimbRot));
    std::memset(state.headLimbRot, 0, sizeof(state.headLimbRot));
    state.bowStringScale = 0.0f;
    if (action.bowReady) {
        state.upperLimbRot[0] = state.aimPitch;
        state.headLimbRot[0] = static_cast<int16_t>(-state.aimPitch / 2);
        state.bowStringScale = 1.0f;
    }
    state.baseAnimation = action.baseAnimation;
    state.upperAnimation = action.upperAnimation;
    const auto pose =
        Game::Simulation::SampleAuthoritativePlayerPoseState(snapshot);
    const bool synchronizeAction =
        snapshot.health == 0 ||
        snapshot.actionState == Game::Simulation::PlayerActionState::PrimaryWindup ||
        snapshot.actionState == Game::Simulation::PlayerActionState::PrimaryActive ||
        snapshot.actionState == Game::Simulation::PlayerActionState::PrimaryRecovery ||
        snapshot.actionState == Game::Simulation::PlayerActionState::Evading ||
        snapshot.actionState == Game::Simulation::PlayerActionState::JumpSlashing;
    const bool synchronizeLocomotion = !synchronizeAction &&
        pose.direction != Game::Simulation::PlayerPoseDirection::None;
    state.synchronizeBaseAnimation = synchronizeAction || synchronizeLocomotion;
    state.loopBaseAnimationProgress = synchronizeLocomotion;
    if (synchronizeLocomotion) {
        state.baseAnimationProgress = snapshot.locomotionPhaseRadians / kTau;
        state.baseAnimationProgressPerSecond =
            state.speed / Game::Simulation::kPlayerLocomotionCycleDistance;
    } else {
        state.baseAnimationProgress = snapshot.health == 0 ? 1.0f
                                                           : pose.actionProgress;
        state.baseAnimationProgressPerSecond =
            Game::Simulation::PlayerActionProgressPerSecond(snapshot.actionState);
    }
    state.baseAnimationSampleSeconds = receivedSeconds;
}

void NativePlayerPresentationComposer::ApplyFishingPresentation(
    NativePlayerPresentationState& state,
    const Game::Replication::FishingPresentationState& presentation) {
    state.fishingState = presentation.state;
    std::memcpy(state.fishingRodTipOffset, presentation.rodTipOffset.data(),
                sizeof(state.fishingRodTipOffset));
    std::memcpy(state.fishingLureDrawOffset, presentation.lureDrawOffset.data(),
                sizeof(state.fishingLureDrawOffset));
    state.fishingRodBendY = presentation.rodBendY;
    state.fishingRodBendX = presentation.rodBendX;
    state.fishingRodTwist = presentation.rodTwist;
    state.fishingRodCastX = presentation.rodCastX;
    std::memcpy(state.fishingLureRot, presentation.lureRotation.data(),
                sizeof(state.fishingLureRot));
    state.fishingLureSpin = presentation.lureSpin;
    state.fishingLureZOffset = presentation.lureZOffset;
    std::memcpy(state.fishingLureHookOffsets, presentation.lureHookOffsets.data(),
                sizeof(state.fishingLureHookOffsets));
    std::memcpy(state.fishingLureHookRot, presentation.lureHookRotations.data(),
                sizeof(state.fishingLureHookRot));
    state.fishingLineScale = presentation.lineScale;
    state.fishingLineGravity = presentation.lineGravity;
    state.fishingLineSpooled = presentation.lineSpooled;
    state.fishingSinkingLureSegmentIndex = presentation.sinkingLureSegmentIndex;
    state.fishingSinkingLureUnderwater = presentation.sinkingLureUnderwater;
    std::memcpy(state.fishingFishRot, presentation.fishRotation.data(),
                sizeof(state.fishingFishRot));
    std::memcpy(state.fishingFishLimbRot, presentation.fishLimbRotation.data(),
                sizeof(state.fishingFishLimbRot));
}

void NativePlayerPresentationComposer::ApplyAuthoritativeFishing(
    NativePlayerPresentationState& state,
    const Game::Client::RemoteFishingEntityState& entities) {
    const auto* lure = entities.LureForOwner(state.playerId);
    if (lure && lure->sceneId == state.sceneId) {
        state.fishingLureOffset[0] = lure->x - state.x;
        state.fishingLureOffset[1] = lure->y - state.y;
        state.fishingLureOffset[2] = lure->z - state.z;
        state.fishingLureType = lure->lureType;
        state.fishingLineHooked =
            lure->phase == static_cast<uint8_t>(
                               Game::Simulation::FishingLurePhase::Hooked);
    } else {
        std::memset(state.fishingLureOffset, 0, sizeof(state.fishingLureOffset));
        state.fishingLureType = 0;
        state.fishingLineHooked = 0;
    }

    const auto* fish = entities.FishForOwner(state.playerId);
    if (fish && fish->identity.sceneId == state.sceneId) {
        state.fishingFishActive = 1;
        state.fishingFishIsLoach =
            fish->species == Game::Simulation::FishSpecies::HylianLoach;
        state.fishingFishLength = fish->length;
        state.fishingFishOffset[0] = fish->x - state.x;
        state.fishingFishOffset[1] = fish->y - state.y;
        state.fishingFishOffset[2] = fish->z - state.z;
    } else {
        state.fishingFishActive = 0;
        state.fishingFishIsLoach = 0;
        state.fishingFishLength = 0.0f;
        std::memset(state.fishingFishOffset, 0, sizeof(state.fishingFishOffset));
    }
}

} // namespace Game::Multiplayer
