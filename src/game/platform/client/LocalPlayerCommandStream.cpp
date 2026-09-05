#include "LocalPlayerCommandStream.h"
#include "../SequenceNumber.h"

#include <algorithm>
#include <cmath>

namespace Game::Client {

namespace {

constexpr uint16_t kKnownActions = Simulation::PLAYER_ACTION_PRIMARY |
                                   Simulation::PLAYER_ACTION_BLOCK |
                                   Simulation::PLAYER_ACTION_AIM |
                                   Simulation::PLAYER_ACTION_EVADE;

} // namespace

LocalPlayerCommandStream::LocalPlayerCommandStream(uint32_t nextCommandSequence,
                                                   uint32_t nextActionSequence)
    : mNextCommandSequence(nextCommandSequence == 0 ? 1 : nextCommandSequence),
      mNextActionSequence(nextActionSequence == 0 ? 1 : nextActionSequence) {
}

std::optional<Simulation::PlayerCommand> LocalPlayerCommandStream::Build(
    const LocalPlayerInputSample& sample) {
    if (sample.lifeEpoch == 0 || sample.sceneId < 0 || sample.sceneId >= 4096 ||
        sample.selectedWeapon > 4 ||
        !std::isfinite(sample.moveX) || !std::isfinite(sample.moveY) ||
        !std::isfinite(sample.headingRadians) || !std::isfinite(sample.aimPitchRadians) ||
        (sample.hasPose &&
         (!std::isfinite(sample.position.x) ||
          !std::isfinite(sample.position.y) ||
          !std::isfinite(sample.position.z))) ||
        (sample.heldActions & ~kKnownActions) != 0 ||
        (sample.pressedActions & ~kKnownActions) != 0 ||
        (sample.hasMeleeAttackVariant &&
         ((sample.pressedActions & Simulation::PLAYER_ACTION_PRIMARY) == 0 ||
          sample.selectedWeapon < 1 || sample.selectedWeapon > 2 ||
          sample.meleeAttackVariant >
              Simulation::MeleeAttackVariant::LeftCombo))) {
        return std::nullopt;
    }
    if (mHasLastSample && sample.lifeEpoch == mLastLifeEpoch &&
        sample.sceneId == mLastSceneId &&
        !Sequence::IsNewer(sample.clientTick, mLastClientTick)) {
        // Never turn an old native sample into a new network command. Scene
        // and life changes intentionally establish a new tick namespace;
        // within one namespace ticks are strictly increasing with wraparound.
        return std::nullopt;
    }

    mLastClientTick = sample.clientTick;
    mLastLifeEpoch = sample.lifeEpoch;
    mLastSceneId = sample.sceneId;
    mHasLastSample = true;

    Simulation::PlayerCommand command{};
    command.sequence = TakeNonZeroSequence(mNextCommandSequence);
    command.actionSequence = sample.pressedActions == 0
                                 ? 0
                                 : TakeNonZeroSequence(mNextActionSequence);
    command.lifeEpoch = sample.lifeEpoch;
    command.clientTick = sample.clientTick;
    command.sceneId = sample.sceneId;
    command.moveX = std::clamp(sample.moveX, -1.0f, 1.0f);
    command.moveY = std::clamp(sample.moveY, -1.0f, 1.0f);
    command.headingRadians = sample.headingRadians;
    command.aimPitchRadians = sample.aimPitchRadians;
    command.heldActions = sample.heldActions;
    command.pressedActions = sample.pressedActions;
    command.reportedMeleeAttackVariant = sample.meleeAttackVariant;
    command.hasReportedMeleeAttackVariant = sample.hasMeleeAttackVariant;
    command.reportedPosition = sample.position;
    command.reportedLocomotionMode = sample.locomotionMode;
    command.hasReportedPose = sample.hasPose;
    return command;
}

LocalPlayerCommandSubmission LocalPlayerCommandStream::Submit(
    const LocalPlayerInputSample& sample, float sampleDeltaSeconds,
    const LocalPlayerCommandSender& sender,
    Simulation::ClientPrediction& prediction, bool predictMovement) {
    const std::optional<Simulation::PlayerCommand> command = Build(sample);
    if (!command || !sender) return LocalPlayerCommandSubmission::NoCommand;
    if (!sender(*command)) return LocalPlayerCommandSubmission::TransportRejected;

    // Prediction records exactly the sample accepted by transport. Elapsed
    // time from a rejected sample belongs to no server command and must not be
    // folded into a later command with a different sequence/action window.
    Simulation::PlayerCommand predictionCommand = *command;
    if (!predictMovement) {
        // The native player actor already applies ordinary local locomotion.
        // Prediction still needs elapsed commands for evade/action phases, but
        // replaying WASD here would create a second local movement body.
        predictionCommand.moveX = 0.0f;
        predictionCommand.moveY = 0.0f;
    }
    prediction.RecordCommand(predictionCommand, sampleDeltaSeconds,
                             sample.selectedWeapon);
    return LocalPlayerCommandSubmission::Submitted;
}

std::optional<LocalWeaponSelectionRequest> LocalPlayerCommandStream::PrepareWeaponSelection(
    uint8_t selectedWeapon) {
    if (selectedWeapon > 4 || selectedWeapon == mLastSentWeapon) return std::nullopt;
    if (mOfferedWeapon && mOfferedWeapon->selectedWeapon == selectedWeapon) {
        return mOfferedWeapon;
    }
    mOfferedWeapon = LocalWeaponSelectionRequest{
        TakeNonZeroSequence(mNextWeaponSelectionSequence), selectedWeapon
    };
    mSelectionRequestedAfterTick = mLastAuthoritativeServerTick;
    mAwaitingWeaponConfirmation = true;
    return mOfferedWeapon;
}

void LocalPlayerCommandStream::ResolveWeaponSelection(uint32_t sequence, bool sent) {
    if (!mOfferedWeapon || mOfferedWeapon->sequence != sequence) return;
    if (sent) {
        mLastSentWeapon = mOfferedWeapon->selectedWeapon;
    } else {
        mAwaitingWeaponConfirmation = false;
    }
    mOfferedWeapon.reset();
}

void LocalPlayerCommandStream::ObserveAuthoritativeWeapon(uint8_t selectedWeapon,
                                                           uint32_t serverTick) {
    if (selectedWeapon > 4 || serverTick == 0 ||
        (mLastAuthoritativeServerTick != 0 &&
         !Sequence::IsNewer(serverTick, mLastAuthoritativeServerTick))) {
        return;
    }
    mLastAuthoritativeServerTick = serverTick;
    mAuthoritativeWeapon = selectedWeapon;
    if (mAwaitingWeaponConfirmation &&
        Sequence::IsNewer(serverTick, mSelectionRequestedAfterTick) &&
        selectedWeapon == mLastSentWeapon) {
        mAwaitingWeaponConfirmation = false;
    }
}

bool LocalPlayerCommandStream::WeaponSelectionConfirmed(uint8_t selectedWeapon) const {
    return selectedWeapon <= 4 && !mAwaitingWeaponConfirmation &&
           selectedWeapon == mAuthoritativeWeapon;
}

void LocalPlayerCommandStream::BeginLife() {
    mNextCommandSequence = 1;
    mNextActionSequence = 1;
    mLastClientTick = 0;
    mLastLifeEpoch = 0;
    mLastSceneId = -1;
    mHasLastSample = false;
    mNextWeaponSelectionSequence = 1;
    mLastSentWeapon = 0xFF;
    mAuthoritativeWeapon = 0xFF;
    mLastAuthoritativeServerTick = 0;
    mSelectionRequestedAfterTick = 0;
    mAwaitingWeaponConfirmation = false;
    mOfferedWeapon.reset();
}

void LocalPlayerCommandStream::Reset() {
    BeginLife();
}

uint32_t LocalPlayerCommandStream::TakeNonZeroSequence(uint32_t& next) {
    if (next == 0) next = 1;
    const uint32_t result = next++;
    if (next == 0) next = 1;
    return result;
}

} // namespace Game::Client
