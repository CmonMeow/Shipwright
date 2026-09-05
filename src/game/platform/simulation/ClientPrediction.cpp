#include "ClientPrediction.h"
#include "../SequenceNumber.h"

#include <algorithm>
#include <cmath>

namespace Game::Simulation {
namespace {

enum class ReplayActionKind : uint8_t {
    None,
    PrimaryBusy,
    Evading,
    JumpSlashing,
    Held,
};

struct ReplayActionState {
    ReplayActionKind kind = ReplayActionKind::None;
    float remainingSeconds = 0.0f;
    Vec3 evadeVelocity{};
    PlayerActionState heldState = PlayerActionState::Idle;
    PlayerActionState primaryState = PlayerActionState::Idle;
    MeleeAttackTiming meleeTiming{};
};

PlayerActionState PredictedStateFrom(const ReplayActionState& action) {
    switch (action.kind) {
        case ReplayActionKind::PrimaryBusy: return action.primaryState;
        case ReplayActionKind::Evading: return PlayerActionState::Evading;
        case ReplayActionKind::JumpSlashing: return PlayerActionState::JumpSlashing;
        case ReplayActionKind::Held: return action.heldState;
        default: return PlayerActionState::Idle;
    }
}

float RemainingPhaseSeconds(uint32_t durationTicks, uint32_t elapsedTicks) {
    return elapsedTicks >= durationTicks
               ? 0.0f
               : static_cast<float>(durationTicks - elapsedTicks) *
                     kPlayerSimulationTickSeconds;
}

ReplayActionState ActionStateFrom(const PlayerSnapshot& snapshot) {
    const uint32_t elapsedTicks = snapshot.serverTick >= snapshot.actionStartTick
                                      ? snapshot.serverTick - snapshot.actionStartTick
                                      : 0;
    const MeleeAttackTiming meleeTiming = MeleeAttackTimingFor(
        snapshot.meleeAttackVariant, snapshot.selectedWeapon);
    switch (snapshot.actionState) {
        case PlayerActionState::PrimaryWindup:
            return { ReplayActionKind::PrimaryBusy,
                     RemainingPhaseSeconds(meleeTiming.windupTicks, elapsedTicks),
                     {}, PlayerActionState::Idle,
                     PlayerActionState::PrimaryWindup, meleeTiming };
        case PlayerActionState::PrimaryActive:
            return { ReplayActionKind::PrimaryBusy,
                     RemainingPhaseSeconds(meleeTiming.activeTicks, elapsedTicks),
                     {}, PlayerActionState::Idle,
                     PlayerActionState::PrimaryActive, meleeTiming };
        case PlayerActionState::PrimaryRecovery:
            return { ReplayActionKind::PrimaryBusy,
                     RemainingPhaseSeconds(meleeTiming.recoveryTicks, elapsedTicks),
                     {}, PlayerActionState::Idle,
                     PlayerActionState::PrimaryRecovery, meleeTiming };
        case PlayerActionState::Evading:
            return { ReplayActionKind::Evading,
                     RemainingPhaseSeconds(kEvadeDurationTicks, elapsedTicks),
                     snapshot.velocity };
        case PlayerActionState::JumpSlashing:
            return { ReplayActionKind::JumpSlashing };
        case PlayerActionState::Blocking:
        case PlayerActionState::Aiming:
            return { ReplayActionKind::Held, 0.0f, {}, snapshot.actionState };
        default:
            return {};
    }
}

ReplayActionState ActionStateFromPrediction(PlayerActionState state,
                                            float remainingSeconds,
                                            const Vec3& evadeVelocity,
                                            MeleeAttackTiming meleeTiming) {
    if (state == PlayerActionState::JumpSlashing) {
        return { ReplayActionKind::JumpSlashing };
    }
    if (state == PlayerActionState::Blocking || state == PlayerActionState::Aiming) {
        return { ReplayActionKind::Held, 0.0f, {}, state };
    }
    if (remainingSeconds <= 0.0f) return {};
    if (state == PlayerActionState::Evading) {
        return { ReplayActionKind::Evading, remainingSeconds, evadeVelocity };
    }
    if (state == PlayerActionState::PrimaryWindup ||
        state == PlayerActionState::PrimaryActive ||
        state == PlayerActionState::PrimaryRecovery) {
        return { ReplayActionKind::PrimaryBusy, remainingSeconds, {},
                 PlayerActionState::Idle, state, meleeTiming };
    }
    return {};
}

float PrimaryPhaseDuration(PlayerActionState state,
                           const MeleeAttackTiming& timing) {
    switch (state) {
        case PlayerActionState::PrimaryWindup:
            return static_cast<float>(timing.windupTicks) *
                   kPlayerSimulationTickSeconds;
        case PlayerActionState::PrimaryActive:
            return static_cast<float>(timing.activeTicks) *
                   kPlayerSimulationTickSeconds;
        case PlayerActionState::PrimaryRecovery:
            return static_cast<float>(timing.recoveryTicks) *
                   kPlayerSimulationTickSeconds;
        default:
            return 0.0f;
    }
}

PlayerActionState NextPrimaryPhase(PlayerActionState state) {
    switch (state) {
        case PlayerActionState::PrimaryWindup:
            return PlayerActionState::PrimaryActive;
        case PlayerActionState::PrimaryActive:
            return PlayerActionState::PrimaryRecovery;
        default:
            return PlayerActionState::Idle;
    }
}

void AdvancePrimaryAction(ReplayActionState& action, float deltaSeconds) {
    while (action.kind == ReplayActionKind::PrimaryBusy) {
        if (action.remainingSeconds - deltaSeconds > 0.000001f) {
            action.remainingSeconds -= deltaSeconds;
            return;
        }
        deltaSeconds = std::max(0.0f,
                                deltaSeconds - action.remainingSeconds);
        action.primaryState = NextPrimaryPhase(action.primaryState);
        if (action.primaryState == PlayerActionState::Idle) {
            action = {};
            return;
        }
        action.remainingSeconds = PrimaryPhaseDuration(
            action.primaryState, action.meleeTiming);
        if (deltaSeconds <= 0.0f) return;
    }
}

Vec3 AdvanceWithVelocity(const Vec3& position, const Vec3& velocity,
                         float deltaSeconds) {
    return { position.x + velocity.x * deltaSeconds, position.y,
             position.z + velocity.z * deltaSeconds };
}

Vec3 ReplayCommand(const Vec3& position, const PlayerCommand& command,
                   float deltaSeconds, uint8_t predictionWeapon,
                   PlayerLocomotionMode locomotionMode, ReplayActionState& action) {
    if (action.kind == ReplayActionKind::PrimaryBusy &&
        action.remainingSeconds <= 0.0f) {
        AdvancePrimaryAction(action, 0.0f);
    } else if (action.kind == ReplayActionKind::Evading &&
               action.remainingSeconds <= 0.0f) {
        action = {};
    }
    if (locomotionMode == PlayerLocomotionMode::Swimming) {
        action = {};
        return AdvancePlayerPosition(position, command, deltaSeconds);
    }
    const bool swordSelected = predictionWeapon == 1 || predictionWeapon == 2;
    if (locomotionMode == PlayerLocomotionMode::Airborne) {
        if (action.kind == ReplayActionKind::JumpSlashing) {
            return AdvancePlayerPosition(position, command, deltaSeconds);
        }
        action = {};
        if (swordSelected &&
            (command.pressedActions & PLAYER_ACTION_PRIMARY) != 0) {
            action.kind = ReplayActionKind::JumpSlashing;
        } else if (swordSelected &&
                   (command.heldActions & PLAYER_ACTION_BLOCK) != 0) {
            action = { ReplayActionKind::Held, 0.0f, {},
                       PlayerActionState::Blocking };
        } else if (predictionWeapon == 3 &&
                   (command.heldActions &
                    (PLAYER_ACTION_AIM | PLAYER_ACTION_PRIMARY)) != 0) {
            action = { ReplayActionKind::Held, 0.0f, {},
                       PlayerActionState::Aiming };
        }
        return AdvancePlayerPosition(position, command, deltaSeconds);
    }

    if (action.kind == ReplayActionKind::JumpSlashing ||
        action.kind == ReplayActionKind::Held) {
        action = {};
    }

    // An edge received while an authoritative action is busy is consumed on
    // that command tick. Even if the action ends partway through this sample,
    // that edge must not execute against the later idle state.
    if (action.kind == ReplayActionKind::Evading) {
        const float evadeSeconds = std::min(deltaSeconds, action.remainingSeconds);
        Vec3 result = AdvanceWithVelocity(position, action.evadeVelocity, evadeSeconds);
        action.remainingSeconds -= evadeSeconds;
        const float ordinarySeconds = deltaSeconds - evadeSeconds;
        if (action.remainingSeconds <= 0.0f) action = {};
        return ordinarySeconds > 0.0f
                   ? AdvancePlayerPosition(result, command, ordinarySeconds)
                   : result;
    }
    if (action.kind == ReplayActionKind::PrimaryBusy) {
        AdvancePrimaryAction(action, deltaSeconds);
        return AdvancePlayerPosition(position, command, deltaSeconds);
    }

    if ((command.pressedActions & PLAYER_ACTION_EVADE) != 0) {
        action.kind = ReplayActionKind::Evading;
        action.remainingSeconds = static_cast<float>(kEvadeDurationTicks) *
                                  kPlayerSimulationTickSeconds;
        action.evadeVelocity = CalculatePlayerEvadeVelocity(command);
        const float evadeSeconds = std::min(deltaSeconds, action.remainingSeconds);
        action.remainingSeconds -= evadeSeconds;
        return AdvanceWithVelocity(position, action.evadeVelocity, evadeSeconds);
    }
    if ((command.pressedActions & PLAYER_ACTION_PRIMARY) != 0 &&
        swordSelected) {
        const MeleeAttackVariant variant = MeleeAttackVariantForCommand(command);
        const MeleeAttackTiming timing =
            MeleeAttackTimingFor(variant, predictionWeapon);
        const PlayerActionState firstPhase = timing.windupTicks == 0
            ? PlayerActionState::PrimaryActive
            : PlayerActionState::PrimaryWindup;
        action = { ReplayActionKind::PrimaryBusy,
                   PrimaryPhaseDuration(firstPhase, timing), {},
                   PlayerActionState::Idle, firstPhase, timing };
    } else if (swordSelected &&
               (command.heldActions & PLAYER_ACTION_BLOCK) != 0) {
        action = { ReplayActionKind::Held, 0.0f, {}, PlayerActionState::Blocking };
    } else if (predictionWeapon == 3 &&
               (command.heldActions &
                (PLAYER_ACTION_AIM | PLAYER_ACTION_PRIMARY)) != 0) {
        action = { ReplayActionKind::Held, 0.0f, {}, PlayerActionState::Aiming };
    }
    return AdvancePlayerPosition(position, command, deltaSeconds);
}

} // namespace

bool ClientPrediction::SeedAuthoritative(const PlayerSnapshot& authoritative) {
    if (authoritative.lifeEpoch == 0 || authoritative.sceneId < 0 ||
        authoritative.sceneId >= 4096 ||
        !std::isfinite(authoritative.position.x) ||
        !std::isfinite(authoritative.position.y) ||
        !std::isfinite(authoritative.position.z)) {
        return false;
    }
    if (mLifeEpoch != 0 && authoritative.lifeEpoch != mLifeEpoch) return false;

    mSamples.clear();
    mCorrection = {};
    mLifeEpoch = authoritative.lifeEpoch;
    mLastAcknowledgedSequence = authoritative.lastProcessedCommand;
    mLastAcknowledgedScene = authoritative.sceneId;
    mHasAcknowledgement = authoritative.lastProcessedCommand != 0;
    mPredictedPosition = authoritative.position;
    mPredictedScene = authoritative.sceneId;
    mHasPredictedPosition = true;
    const ReplayActionState action = ActionStateFrom(authoritative);
    mPredictedActionState = PredictedStateFrom(action);
    mPredictedActionRemainingSeconds = action.remainingSeconds;
    mPredictedEvadeVelocity = action.evadeVelocity;
    mPredictedMeleeTiming = action.meleeTiming;
    mPredictedLocomotionMode = authoritative.locomotionMode;
    return true;
}

void ClientPrediction::RecordCommand(const PlayerCommand& command,
                                     float deltaSeconds, uint8_t predictionWeapon) {
    if (command.sequence == 0 || command.lifeEpoch == 0 || command.sceneId < 0 ||
        deltaSeconds <= 0.0f || !mHasPredictedPosition ||
        command.lifeEpoch != mLifeEpoch || command.sceneId != mPredictedScene) {
        return;
    }
    deltaSeconds = std::min(deltaSeconds, 0.25f);
    if ((!mSamples.empty() && !Sequence::IsNewer(command.sequence, mSamples.back().sequence)) ||
        (mSamples.empty() && mHasAcknowledgement && command.sceneId == mLastAcknowledgedScene &&
         !Sequence::IsNewer(command.sequence, mLastAcknowledgedSequence))) {
        return;
    }
    ReplayActionState action = ActionStateFromPrediction(
        mPredictedActionState, mPredictedActionRemainingSeconds,
        mPredictedEvadeVelocity, mPredictedMeleeTiming);
    const Vec3 replayedPosition = ReplayCommand(
        mPredictedPosition, command, deltaSeconds, predictionWeapon,
        mPredictedLocomotionMode, action);
    mPredictedPosition = command.hasReportedPose
                             ? command.reportedPosition
                             : replayedPosition;
    if (command.hasReportedPose) {
        mPredictedLocomotionMode = command.reportedLocomotionMode;
    }
    mPredictedActionState = PredictedStateFrom(action);
    mPredictedActionRemainingSeconds = action.remainingSeconds;
    mPredictedEvadeVelocity = action.evadeVelocity;
    mPredictedMeleeTiming = action.meleeTiming;
    mSamples.push_back({ command.sequence, command.lifeEpoch, command.sceneId,
                         mPredictedPosition,
                         command, deltaSeconds, predictionWeapon });
    while (mSamples.size() > 256) mSamples.pop_front();
}

bool ClientPrediction::Reconcile(const PlayerSnapshot& authoritative,
                                 const Vec3& currentPredictedPosition) {
    return ReconcileInternal(authoritative, currentPredictedPosition);
}

bool ClientPrediction::ReconcileInternal(const PlayerSnapshot& authoritative,
                                         const Vec3& currentPredictedPosition) {
    const uint32_t sequence = authoritative.lastProcessedCommand;
    const uint32_t lifeEpoch = authoritative.lifeEpoch;
    const int32_t sceneId = authoritative.sceneId;
    const Vec3& authoritativePosition = authoritative.position;
    if (lifeEpoch == 0 || sceneId < 0) return false;
    if (mLifeEpoch == 0 || lifeEpoch != mLifeEpoch) return false;
    if (sequence != 0 && mHasAcknowledgement &&
        sceneId == mLastAcknowledgedScene &&
        !Sequence::IsNewer(sequence, mLastAcknowledgedSequence)) {
        return false;
    }
    Vec3 acknowledgedPoseError{};
    bool hasAcknowledgedPoseError = false;
    if (sequence != 0) {
        const auto acknowledged = std::find_if(
            mSamples.begin(), mSamples.end(), [&](const Sample& sample) {
                return sample.sequence == sequence && sample.lifeEpoch == lifeEpoch &&
                       sample.sceneId == sceneId;
            });
        if (acknowledged != mSamples.end()) {
            if (acknowledged->command.hasReportedPose) {
                acknowledgedPoseError = {
                    authoritativePosition.x -
                        acknowledged->command.reportedPosition.x,
                    authoritativePosition.y -
                        acknowledged->command.reportedPosition.y,
                    authoritativePosition.z -
                        acknowledged->command.reportedPosition.z
                };
                hasAcknowledgedPoseError = true;
            }
            mSamples.erase(mSamples.begin(), std::next(acknowledged));
        } else {
            // A snapshot can acknowledge a command whose sample was evicted during
            // a long stall or skipped because newer disposable movement arrived.
            std::erase_if(mSamples, [&](const Sample& sample) {
                return sample.lifeEpoch != lifeEpoch || sample.sceneId != sceneId ||
                       !Sequence::IsNewer(sample.sequence, sequence);
            });
        }
    }

    // Replay every unacknowledged local command from the authoritative base.
    // This computes where responsive local presentation should currently be,
    // rather than comparing authority with an old parallel position sample.
    Vec3 reconciledPosition = authoritativePosition;
    ReplayActionState replayAction = ActionStateFrom(authoritative);
    for (const Sample& sample : mSamples) {
        if (sample.lifeEpoch != lifeEpoch || sample.sceneId != sceneId) continue;
        const Vec3 replayedPosition = ReplayCommand(
            reconciledPosition, sample.command, sample.deltaSeconds,
            sample.predictionWeapon, authoritative.locomotionMode,
            replayAction);
        if (sample.command.hasReportedPose) {
            reconciledPosition = sample.command.reportedPosition;
            if (hasAcknowledgedPoseError) {
                reconciledPosition.x += acknowledgedPoseError.x;
                reconciledPosition.y += acknowledgedPoseError.y;
                reconciledPosition.z += acknowledgedPoseError.z;
            }
        } else {
            reconciledPosition = replayedPosition;
        }
    }
    mCorrection = { reconciledPosition.x - currentPredictedPosition.x,
                    reconciledPosition.y - currentPredictedPosition.y,
                    reconciledPosition.z - currentPredictedPosition.z };
    if (sequence != 0) {
        mLastAcknowledgedSequence = sequence;
        mLastAcknowledgedScene = sceneId;
        mHasAcknowledgement = true;
    }
    mPredictedPosition = currentPredictedPosition;
    mPredictedScene = sceneId;
    mHasPredictedPosition = true;
    mPredictedLocomotionMode = authoritative.locomotionMode;
    mPredictedActionState = PredictedStateFrom(replayAction);
    mPredictedActionRemainingSeconds = replayAction.remainingSeconds;
    mPredictedEvadeVelocity = replayAction.evadeVelocity;
    mPredictedMeleeTiming = replayAction.meleeTiming;
    return true;
}

Vec3 ClientPrediction::ConsumeCorrection(float deltaSeconds, float halfLifeSeconds,
                                         float snapDistance) {
    deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.25f);
    halfLifeSeconds = std::max(0.0f, halfLifeSeconds);
    snapDistance = std::max(0.0f, snapDistance);
    const float length = std::sqrt(mCorrection.x * mCorrection.x + mCorrection.y * mCorrection.y +
                                   mCorrection.z * mCorrection.z);
    const float fraction = length > snapDistance
                               ? 1.0f
                               : halfLifeSeconds == 0.0f
                                     ? 1.0f
                                     : 1.0f - std::exp2(-deltaSeconds / halfLifeSeconds);
    Vec3 applied{ mCorrection.x * fraction, mCorrection.y * fraction, mCorrection.z * fraction };
    mCorrection.x -= applied.x;
    mCorrection.y -= applied.y;
    mCorrection.z -= applied.z;
    if (std::abs(mCorrection.x) < 0.01f) mCorrection.x = 0.0f;
    if (std::abs(mCorrection.y) < 0.01f) mCorrection.y = 0.0f;
    if (std::abs(mCorrection.z) < 0.01f) mCorrection.z = 0.0f;

    // Unacknowledged samples are expressed in the same corrected world frame
    // as the local actor, preventing the next snapshot from reintroducing an
    // error that has already been applied visually.
    for (Sample& sample : mSamples) {
        sample.position.x += applied.x;
        sample.position.y += applied.y;
        sample.position.z += applied.z;
    }
    if (mHasPredictedPosition) {
        mPredictedPosition.x += applied.x;
        mPredictedPosition.y += applied.y;
        mPredictedPosition.z += applied.z;
    }
    return applied;
}

void ClientPrediction::Reset(uint32_t lifeEpoch) {
    mSamples.clear();
    mCorrection = {};
    mLastAcknowledgedSequence = 0;
    mLastAcknowledgedScene = -1;
    mHasAcknowledgement = false;
    mPredictedPosition = {};
    mPredictedScene = -1;
    mHasPredictedPosition = false;
    mPredictedActionState = PlayerActionState::Idle;
    mPredictedActionRemainingSeconds = 0.0f;
    mPredictedEvadeVelocity = {};
    mPredictedMeleeTiming = {};
    mPredictedLocomotionMode = PlayerLocomotionMode::Grounded;
    mLifeEpoch = lifeEpoch;
}

const Vec3& ClientPrediction::PendingCorrection() const {
    return mCorrection;
}

size_t ClientPrediction::PendingCommandCount() const {
    return mSamples.size();
}

} // namespace Game::Simulation
