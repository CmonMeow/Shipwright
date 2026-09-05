#include "PlayerSimulation.h"
#include "AuthoritativePlayerHitRig.h"
#include "AuthoritativeMeleeWeapon.h"
#include "CombatGeometry.h"
#include "../SequenceNumber.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
#include <utility>

namespace Game::Simulation {
namespace {

constexpr size_t kRecentPlayerCommandCapacity = 64;
constexpr float kWaterEntryTolerance = 2.0f;
constexpr float kMinimumSwimmingDepth = 56.0f;
constexpr float kMaximumFloorProbeDepth = 500.0f;
constexpr float kGroundProbeAbove = 45.0f;
constexpr float kGroundProbeBelow = 100.0f;
constexpr float kGroundSupportProbeAbove = 8.0f;
constexpr float kGroundSupportProbeBelow = 12.0f;
constexpr float kGroundSupportTolerance = 6.0f;
// Native Link applies -1.2 velocity units per 20 Hz frame and clamps at
// -20 units/frame. Express the same acceleration and terminal speed in
// world units per second for the 30 Hz authority loop.
constexpr float kNativeGravity = -1.2f * 20.0f * 20.0f;
constexpr float kNativeTerminalVelocity = -20.0f * 20.0f;

float ClampAxis(float value) {
    return std::clamp(value, -1.0f, 1.0f);
}

bool IsAirborneWeaponState(PlayerActionState state) {
    return state == PlayerActionState::JumpSlashing ||
           state == PlayerActionState::Blocking ||
           state == PlayerActionState::Aiming;
}

// Adult Link settles about 56 units below the water surface during ordinary
// surface swimming (PlayerAgeProperties::unk_2C). Native reported traversal
// may legitimately move above or below this datum while entering, diving, or
// climbing out; kSwimmingRootDepth is only for server-simulated fallback.
constexpr float kSwimmingRootDepth = 56.0f;

} // namespace

Vec3 CalculatePlayerVelocity(const PlayerCommand& command) {
    float moveX = ClampAxis(command.moveX);
    float moveY = ClampAxis(command.moveY);
    const float magnitude = std::sqrt(moveX * moveX + moveY * moveY);
    if (magnitude > 1.0f) {
        moveX /= magnitude;
        moveY /= magnitude;
    }
    const float sinHeading = std::sin(command.headingRadians);
    const float cosHeading = std::cos(command.headingRadians);
    // Native Link is updated at 20 Hz. R_RUN_SPEED_LIMIT is 6 units per
    // native frame, while bow parallel movement is capped at 3.5 and native
    // backward parallel movement at 3. Convert those frame distances to
    // world units per second before the 30 Hz authority integrates them.
    constexpr float runSpeed = 6.0f * 20.0f;
    constexpr float bowAimSpeed = 3.5f * 20.0f;
    constexpr float backwardParallelSpeed = 3.0f * 20.0f;
    float speed = runSpeed;
    if ((command.heldActions & PLAYER_ACTION_AIM) != 0) {
        speed = bowAimSpeed;
    }
    // Native digital parallel movement gives A/D classification priority.
    // Only a pure backward input takes the slower backward cap.
    if ((command.heldActions & (PLAYER_ACTION_BLOCK | PLAYER_ACTION_AIM)) != 0 &&
        std::abs(moveX) <= 0.25f && moveY < -0.25f) {
        speed = backwardParallelSpeed;
    }
    // OoT converts stick input with atan2(-x, y): positive stick X is left,
    // which is negative world X at zero heading. Preserve that transform
    // exactly so authority never pushes against native Link's movement.
    return { (sinHeading * moveY - cosHeading * moveX) * speed,
             0.0f,
             (cosHeading * moveY + sinHeading * moveX) * speed };
}

Vec3 CalculatePlayerEvadeVelocity(const PlayerCommand& command) {
    const float sinHeading = std::sin(command.headingRadians);
    const float cosHeading = std::cos(command.headingRadians);
    if (std::abs(command.moveX) > 0.25f) {
        constexpr float sideHopSpeed = 170.0f;
        const float direction = command.moveX > 0.0f ? -1.0f : 1.0f;
        return { cosHeading * sideHopSpeed * direction, 0.0f,
                 -sinHeading * sideHopSpeed * direction };
    }
    constexpr float backflipSpeed = 120.0f;
    return { -sinHeading * backflipSpeed, 0.0f,
             -cosHeading * backflipSpeed };
}

MeleeAttackVariant BaseMeleeAttackVariantForCommand(
    const PlayerCommand& command) {
    constexpr float kDirectionalThreshold = 0.25f;
    if (command.moveY > kDirectionalThreshold &&
        std::fabs(command.moveY) >= std::fabs(command.moveX)) {
        // OoT converts the forward stab to a forward slash when Link is not
        // Z-targeting. PC sword controls use RMB only for shield/block.
        return MeleeAttackVariant::ForwardSlash;
    }
    if (command.moveX > kDirectionalThreshold) {
        // Native names this for blade travel: right input swings left.
        return MeleeAttackVariant::LeftSlash;
    }
    // Left, backward, and neutral input swing the blade right.
    return MeleeAttackVariant::RightSlash;
}

MeleeAttackVariant MeleeAttackVariantForCommand(const PlayerCommand& command) {
    return command.hasReportedMeleeAttackVariant
               ? command.reportedMeleeAttackVariant
               : BaseMeleeAttackVariantForCommand(command);
}

Vec3 AdvancePlayerPosition(const Vec3& position, const PlayerCommand& command,
                           float deltaSeconds) {
    const Vec3 velocity = CalculatePlayerVelocity(command);
    return { position.x + velocity.x * deltaSeconds,
             position.y,
             position.z + velocity.z * deltaSeconds };
}

bool CanPerformGroundedAction(const PlayerSnapshot& player) {
    return player.health != 0 &&
           player.locomotionMode == PlayerLocomotionMode::Grounded;
}

bool CanPerformFishingAction(const PlayerSnapshot& player) {
    return player.health != 0;
}

void PlayerSimulation::SetCollisionQuery(SegmentCast segmentCast) {
    mSegmentCast = std::move(segmentCast);
}

void PlayerSimulation::SetCollisionSceneQuery(
    CollisionSceneQuery collisionSceneQuery) {
    mCollisionSceneQuery = std::move(collisionSceneQuery);
}

void PlayerSimulation::SetWaterSurfaceQuery(WaterSurfaceQuery waterSurfaceQuery) {
    mWaterSurfaceQuery = std::move(waterSurfaceQuery);
}

void PlayerSimulation::SetClimbSurfaceQuery(
    ClimbSurfaceQuery climbSurfaceQuery) {
    mClimbSurfaceQuery = std::move(climbSurfaceQuery);
}

void PlayerSimulation::ClearNativeClimbAuthorization(PlayerEntity& player) {
    player.nativeClimbAuthorized = false;
    player.nativeClimbLastSurfacePosition = {};
    player.nativeClimbLastSurfaceTick = 0;
}

bool PlayerSimulation::HasAuthoritativeCollisionScene(int32_t sceneId) const {
    return mSegmentCast && mCollisionSceneQuery &&
           mCollisionSceneQuery(sceneId);
}

bool PlayerSimulation::GroundedPoseSupported(
    int32_t sceneId, const Vec3& position) const {
    if (!HasAuthoritativeCollisionScene(sceneId)) return true;
    const Vec3 probeStart{ position.x,
                           position.y + kGroundSupportProbeAbove,
                           position.z };
    const Vec3 probeEnd{ position.x,
                         position.y - kGroundSupportProbeBelow,
                         position.z };
    Vec3 floor{};
    return mSegmentCast(sceneId, probeStart, probeEnd, floor) &&
           std::fabs(position.y - floor.y) <= kGroundSupportTolerance;
}

bool PlayerSimulation::SwimmingPoseSupported(
    int32_t sceneId, const Vec3& position) const {
    float surfaceY = 0.0f;
    if (!mWaterSurfaceQuery ||
        !mWaterSurfaceQuery(sceneId, position, surfaceY) ||
        !std::isfinite(surfaceY) ||
        position.y > surfaceY + kWaterEntryTolerance) {
        return false;
    }
    if (!mSegmentCast) return true;
    const Vec3 floorProbeStart{ position.x, surfaceY + 5.0f, position.z };
    const Vec3 floorProbeEnd{ position.x,
                              surfaceY - kMaximumFloorProbeDepth,
                              position.z };
    Vec3 floor{};
    if (!mSegmentCast(sceneId, floorProbeStart, floorProbeEnd, floor)) {
        return true;
    }

    // PLAYER_STATE1_IN_WATER covers the complete native water traversal:
    // entry, surface swimming, diving and climbing onto shore. Only accepting
    // the surface-swim datum rejected legitimate native poses during every
    // transition, then reconciliation pushed the owning Link back toward the
    // old server pose. Validate that the root remains inside a sufficiently
    // deep water column instead; native Link remains the movement simulation.
    constexpr float kFloorPenetrationTolerance = 6.0f;
    return surfaceY - floor.y >= kMinimumSwimmingDepth &&
           position.y >= floor.y - kFloorPenetrationTolerance;
}

bool PlayerSimulation::LocomotionModeSupported(
    const PlayerEntity& player, PlayerLocomotionMode requestedMode,
    const Vec3& requestedPosition) const {
    switch (requestedMode) {
        case PlayerLocomotionMode::Grounded:
            return GroundedPoseSupported(player.sceneId, requestedPosition);
        case PlayerLocomotionMode::Swimming:
            return SwimmingPoseSupported(player.sceneId, requestedPosition);
        case PlayerLocomotionMode::Airborne:
            if (!HasAuthoritativeCollisionScene(player.sceneId) ||
                player.locomotionMode == PlayerLocomotionMode::Airborne ||
                player.actionState == PlayerActionState::Evading) {
                return true;
            }
            return !GroundedPoseSupported(player.sceneId, requestedPosition);
        case PlayerLocomotionMode::Climbing:
            return true;
    }
    return false;
}

EntityId PlayerSimulation::EnsurePlayer(int32_t ownerPlayerId, const PlayerSpawn& spawn) {
    if (ownerPlayerId < 0 || spawn.sceneId < 0 || spawn.team > TeamId::Green) {
        return {};
    }
    if (PlayerEntity* existing = FindPlayer(ownerPlayerId)) {
        return existing->id;
    }
    if (mPlayers.Size() >= kMaximumPlayers ||
        (spawn.team != TeamId::Neutral &&
         TeamPopulation(spawn.team) >= kMaximumPlayersPerTeam)) {
        return {};
    }
    PlayerEntity player{};
    player.ownerPlayerId = ownerPlayerId;
    player.sceneId = spawn.sceneId;
    player.position = spawn.position;
    player.spawnPosition = spawn.position;
    player.headingRadians = spawn.headingRadians;
    player.spawnHeadingRadians = spawn.headingRadians;
    player.team = spawn.team;
    const EntityId id = mPlayers.Create(std::move(player));
    PlayerEntity* created = mPlayers.Get(id);
    if (!created) return {};
    created->id = id;
    mPlayerByOwner.insert_or_assign(ownerPlayerId, id);
    UpdateLocomotionSurface(*created, 0.0f);
    mPlayerSpatialIndex.Update(ownerPlayerId, created->sceneId, created->position);
    return id;
}

bool PlayerSimulation::ChangeScene(int32_t ownerPlayerId, const PlayerSpawn& spawn) {
    PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player) {
        EnsurePlayer(ownerPlayerId, spawn);
        return true;
    }
    player->sceneId = spawn.sceneId;
    player->position = spawn.position;
    player->spawnPosition = spawn.position;
    player->velocity = {};
    player->evadeVelocity = {};
    player->locomotionPhaseRadians = 0.0f;
    player->headingRadians = spawn.headingRadians;
    player->spawnHeadingRadians = spawn.headingRadians;
    player->heldActions = 0;
    player->aimPitchRadians = 0.0f;
    player->actionState = PlayerActionState::Idle;
    player->actionStartTick = mCurrentTick;
    player->hasCommand = false;
    player->pressedActionsForNextTick = 0;
    player->hasPressedActionSample = false;
    player->commandReceivedTick = 0;
    player->lastAppliedPoseSequence = 0;
    player->lastAdmittedPoseSequence = 0;
    player->lastPoseAdmissionServerTick = 0;
    ClearNativeClimbAuthorization(*player);
    player->nativeClimbReentryTick = 0;
    player->recentCommands.clear();
    player->bowPrimaryHeld = false;
    player->bowShotArmed = false;
    player->swordPrimaryHeld = false;
    player->swordSpinCharged = false;
    player->bowDrawStartTick = 0;
    player->nextBowShotTick = 0;
    player->hitPlayers.clear();
    UpdateLocomotionSurface(*player, 0.0f);
    mPlayerSpatialIndex.Update(ownerPlayerId, player->sceneId, player->position);
    return true;
}

bool PlayerSimulation::RespawnPlayer(int32_t ownerPlayerId) {
    PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player) {
        return false;
    }
    Respawn(*player, player->health == 0);
    mPlayerSpatialIndex.Update(ownerPlayerId, player->sceneId, player->position);
    return true;
}

bool PlayerSimulation::SelectWeapon(int32_t ownerPlayerId, uint8_t selectedWeapon) {
    PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player || selectedWeapon > 4) return false;
    if (player->selectedWeapon == selectedWeapon) return true;

    player->selectedWeapon = selectedWeapon;
    player->heldActions &= static_cast<uint16_t>(~(PLAYER_ACTION_PRIMARY |
                                                   PLAYER_ACTION_BLOCK |
                                                   PLAYER_ACTION_AIM));
    player->command.heldActions &= static_cast<uint16_t>(~(PLAYER_ACTION_PRIMARY |
                                                           PLAYER_ACTION_BLOCK |
                                                           PLAYER_ACTION_AIM));
    player->command.pressedActions &= static_cast<uint16_t>(~PLAYER_ACTION_PRIMARY);
    player->pressedActionsForNextTick &= static_cast<uint16_t>(~PLAYER_ACTION_PRIMARY);
    player->bowPrimaryHeld = false;
    player->bowShotArmed = false;
    player->swordPrimaryHeld = false;
    player->swordSpinCharged = false;
    player->meleeAttackVariant = MeleeAttackVariant::RightSlash;
    player->repeatedMeleeAttackCount = 0;
    player->bowDrawStartTick = 0;
    if (player->actionState != PlayerActionState::Evading) {
        player->actionState = PlayerActionState::Idle;
        player->actionStartTick = mCurrentTick;
        player->hitPlayers.clear();
    }
    return true;
}

void PlayerSimulation::Respawn(PlayerEntity& player, bool emitEvent) {
    player.position = player.spawnPosition;
    player.velocity = {};
    player.evadeVelocity = {};
    player.locomotionPhaseRadians = 0.0f;
    player.headingRadians = player.spawnHeadingRadians;
    player.health = 48;
    if (emitEvent) {
        ++player.lifeEpoch;
        if (player.lifeEpoch == 0) player.lifeEpoch = 1;

        // Command and action sequences are scoped to lifeEpoch. The client
        // restarts them for a new incarnation, so retain no replay floor or
        // acknowledgement from the player that died.
        player.command = {};
        player.recentCommands.clear();
        player.lastProcessedCommand = 0;
        player.lastReceivedActionSequence = 0;
    }
    player.heldActions = 0;
    player.aimPitchRadians = 0.0f;
    player.actionState = PlayerActionState::Idle;
    player.actionStartTick = mCurrentTick;
    player.respawnTick = 0;
    player.hitPlayers.clear();
    player.hasCommand = false;
    player.pressedActionsForNextTick = 0;
    player.hasPressedActionSample = false;
    player.commandReceivedTick = 0;
    player.lastAppliedPoseSequence = 0;
    player.lastAdmittedPoseSequence = 0;
    player.lastPoseAdmissionServerTick = 0;
    ClearNativeClimbAuthorization(player);
    player.nativeClimbReentryTick = 0;
    player.recentCommands.clear();
    player.bowPrimaryHeld = false;
    player.bowShotArmed = false;
    player.swordPrimaryHeld = false;
    player.swordSpinCharged = false;
    player.meleeAttackVariant = MeleeAttackVariant::RightSlash;
    player.repeatedMeleeAttackCount = 0;
    player.meleeAttackId = 0;
    player.bowDrawStartTick = 0;
    player.nextBowShotTick = 0;
    UpdateLocomotionSurface(player, 0.0f);
    if (emitEvent) {
        mLifeEvents.push_back(BuildLifeEvent(player, PlayerLifeEventKind::Respawned));
    }
}

void PlayerSimulation::RemovePlayer(int32_t ownerPlayerId) {
    const auto indexed = mPlayerByOwner.find(ownerPlayerId);
    if (indexed == mPlayerByOwner.end()) return;
    mPlayers.Destroy(indexed->second);
    mPlayerByOwner.erase(indexed);
    mPlayerSpatialIndex.Remove(ownerPlayerId);
}

void PlayerSimulation::Reset() {
    mPlayers.Clear();
    mPlayerByOwner.clear();
    mPlayerSpatialIndex.Reset();
    mCurrentTick = 0;
    mNextCombatEventId = 1;
    mCombatResults.clear();
    mLifeEvents.clear();
}

PlayerSimulation::PlayerEntity* PlayerSimulation::FindPlayer(int32_t ownerPlayerId) {
    const auto indexed = mPlayerByOwner.find(ownerPlayerId);
    if (indexed == mPlayerByOwner.end()) return nullptr;
    PlayerEntity* player = mPlayers.Get(indexed->second);
    if (player && player->ownerPlayerId == ownerPlayerId) return player;
    // A stale generation indicates an internal lifecycle error. Retire the
    // bad index instead of ever resolving it to a reused registry slot.
    mPlayerByOwner.erase(indexed);
    return nullptr;
}

const PlayerSimulation::PlayerEntity* PlayerSimulation::FindPlayer(int32_t ownerPlayerId) const {
    const auto indexed = mPlayerByOwner.find(ownerPlayerId);
    if (indexed == mPlayerByOwner.end()) return nullptr;
    const PlayerEntity* player = mPlayers.Get(indexed->second);
    return player && player->ownerPlayerId == ownerPlayerId ? player : nullptr;
}

bool PlayerSimulation::SubmitCommand(const PlayerCommand& command) {
    PlayerEntity* player = FindPlayer(command.ownerPlayerId);
    if (!player || player->health == 0 || command.lifeEpoch != player->lifeEpoch ||
        command.sceneId != player->sceneId) {
        return false;
    }
    const bool newerMovement = Sequence::IsNewer(command.sequence, player->command.sequence);
    const bool newerAction = command.pressedActions != 0 && command.actionSequence != 0 &&
                             Sequence::IsNewer(command.actionSequence, player->lastReceivedActionSequence);
    if (!newerMovement && !newerAction) return false;

    if (newerAction) {
        player->lastReceivedActionSequence = command.actionSequence;
        // Reliable action packets and disposable movement packets are
        // independent streams even though they share this compact command
        // envelope. A newer movement sample may overtake an older reliable
        // edge; admit that edge against the current authoritative pose while
        // the input stream is live instead of silently losing the action.
        // Once movement has timed out, consume the action sequence without
        // executing it so an old packet cannot wake a disconnected player.
        const bool inputStreamLive =
            newerMovement ||
            (player->hasCommand &&
             mCurrentTick - player->commandReceivedTick <= kCommandTimeoutTicks);
        if (inputStreamLive && player->pressedActionsForNextTick == 0) {
            // Client prediction observes reliable edges in sequence order. If
            // several arrive before one fixed tick, preserve the earliest one
            // so authority makes the same transition. Later edges are still
            // consumed by lastReceivedActionSequence, never merged or queued
            // to execute after the chosen action finishes.
            player->pressedActionsForNextTick = command.pressedActions;
            player->pressedActionSample = command;
            player->pressedActionSample.moveX = ClampAxis(command.moveX);
            player->pressedActionSample.moveY = ClampAxis(command.moveY);
            player->hasPressedActionSample = true;
        }
    }
    if (newerMovement) {
        player->command = command;
        player->command.moveX = ClampAxis(command.moveX);
        player->command.moveY = ClampAxis(command.moveY);
        player->command.pressedActions = 0;
        player->recentCommands.push_back(player->command);
        if (player->recentCommands.size() > kRecentPlayerCommandCapacity) {
            player->recentCommands.pop_front();
        }
        player->commandReceivedTick = mCurrentTick;
        player->hasCommand = true;
    }
    return true;
}

bool PlayerSimulation::ApplyDamage(int32_t sourcePlayerId, int32_t targetPlayerId, uint8_t damage,
                                    int16_t impactHeading, CombatAttackKind attackKind,
                                    const Vec3& impactPosition,
                                    PlayerHitRegion hitRegion,
                                    uint32_t meleeAttackId) {
    PlayerEntity* target = FindPlayer(targetPlayerId);
    const PlayerEntity* source = FindPlayer(sourcePlayerId);
    const uint8_t resolvedDamage = DamageForPlayerHitRegion(damage, hitRegion);
    if (!target || target->health == 0 || resolvedDamage == 0 ||
        (source && sourcePlayerId != targetPlayerId && source->team != TeamId::Neutral &&
         source->team == target->team)) {
        return false;
    }
    target->health = resolvedDamage >= target->health
                         ? 0
                         : static_cast<uint8_t>(target->health - resolvedDamage);
    if (target->health == 0) EnterDeadState(*target);
    mCombatResults.push_back({ NextCombatEventId(), sourcePlayerId, targetPlayerId,
                               source ? source->id : EntityId{},
                                target->id, source ? source->lifeEpoch : 0,
                                target->lifeEpoch, meleeAttackId, target->sceneId,
                                attackKind, CombatResultKind::Damaged,
                                resolvedDamage, impactHeading, impactPosition, hitRegion });
    return true;
}

bool PlayerSimulation::ReportBlockedHit(int32_t sourcePlayerId, int32_t targetPlayerId,
                                        CombatAttackKind attackKind, int16_t impactHeading,
                                        const Vec3& impactPosition,
                                        uint32_t meleeAttackId) {
    const PlayerEntity* source = FindPlayer(sourcePlayerId);
    const PlayerEntity* target = FindPlayer(targetPlayerId);
    if (!source || !target || source == target || source->sceneId != target->sceneId ||
        source->health == 0 || target->health == 0 ||
        target->actionState != PlayerActionState::Blocking ||
        (source->team != TeamId::Neutral && source->team == target->team)) {
        return false;
    }
    mCombatResults.push_back({ NextCombatEventId(), sourcePlayerId, targetPlayerId,
                               source->id, target->id,
                               source->lifeEpoch, target->lifeEpoch,
                               meleeAttackId, target->sceneId,
                               attackKind, CombatResultKind::Blocked, 0,
                               impactHeading, impactPosition });
    return true;
}

bool PlayerSimulation::BowShotReady(int32_t ownerPlayerId) const {
    const PlayerEntity* player = FindPlayer(ownerPlayerId);
    return player && player->health != 0 && player->hasCommand &&
           mCurrentTick - player->commandReceivedTick <= kCommandTimeoutTicks &&
           player->selectedWeapon == 3 &&
           player->locomotionMode != PlayerLocomotionMode::Swimming &&
           player->locomotionMode != PlayerLocomotionMode::Climbing &&
           player->bowShotArmed &&
           Sequence::IsAtOrAfter(mCurrentTick, player->nextBowShotTick);
}

bool PlayerSimulation::CommitBowShot(int32_t ownerPlayerId) {
    PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player || !BowShotReady(ownerPlayerId)) return false;
    player->bowShotArmed = false;
    player->bowDrawStartTick = 0;
    player->nextBowShotTick = mCurrentTick + kBowRefireDurationTicks;
    return true;
}

bool PlayerSimulation::SetPlayerTeam(int32_t ownerPlayerId, TeamId team) {
    PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player || team > TeamId::Green) {
        return false;
    }
    if (player->team == team) return true;
    if (team != TeamId::Neutral &&
        TeamPopulation(team) >= kMaximumPlayersPerTeam) {
        return false;
    }
    player->team = team;
    return true;
}

std::optional<TeamId> PlayerSimulation::TeamForPlayer(int32_t ownerPlayerId) const {
    const PlayerEntity* player = FindPlayer(ownerPlayerId);
    return player ? std::optional<TeamId>(player->team) : std::nullopt;
}

size_t PlayerSimulation::TeamPopulation(TeamId team) const {
    size_t population = 0;
    mPlayers.ForEach([&](const PlayerEntity& player) {
        if (player.team == team) ++population;
    });
    return population;
}

void PlayerSimulation::StepFixed() {
    SimulateTick();
}

void PlayerSimulation::SimulateTick() {
    ++mCurrentTick;
    std::vector<std::pair<int32_t, Vec3>> startPositions;
    startPositions.reserve(mPlayers.Size());
    mPlayers.ForEach([&](const PlayerEntity& player) {
        startPositions.emplace_back(player.ownerPlayerId, player.position);
    });
    std::sort(startPositions.begin(), startPositions.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });
    mPlayers.ForEach([&](PlayerEntity& player) {
        if (player.hasCommand && mCurrentTick - player.commandReceivedTick > kCommandTimeoutTicks) {
            // Packet loss or a blocked window message pump must not repeat a
            // movement, attack, aim, or guard command indefinitely. Sequence
            // state remains intact so delayed older packets still cannot win.
            player.command.moveX = 0.0f;
            player.command.moveY = 0.0f;
            player.command.heldActions = 0;
            // Reported traversal is an observation attached to this command,
            // not durable entity state. Leaving it live after the input stream
            // expires can pin the authoritative player in a client-declared
            // climb indefinitely.
            player.command.hasReportedPose = false;
            ClearNativeClimbAuthorization(player);
        }
        if (player.hasCommand) {
            player.command.pressedActions = player.pressedActionsForNextTick;
            player.pressedActionsForNextTick = 0;
            player.headingRadians = player.hasPressedActionSample
                ? player.pressedActionSample.headingRadians
                : player.command.headingRadians;
            player.aimPitchRadians = player.command.aimPitchRadians;
            player.heldActions = player.command.heldActions;
            player.lastProcessedCommand = player.command.sequence;
        }
        if (player.health == 0) {
            player.velocity = {};
            player.actionState = PlayerActionState::Idle;
            if (player.respawnTick != 0 &&
                Sequence::IsAtOrAfter(mCurrentTick, player.respawnTick)) {
                Respawn(player, true);
            }
            return;
        }
        UpdateAction(player);
        if (player.hasCommand && player.command.hasReportedPose) {
            ApplyNativeTraversalRequest(player);
        } else {
            MovePlayer(player, kPlayerSimulationTickSeconds);
        }
        UpdateBowDrawState(player);
    });
    ResolvePlayerCollisions(startPositions, kPlayerSimulationTickSeconds);
    RefreshSpatialIndex();
    // Resolve combat only after every entity has advanced. This keeps block
    // and hit results independent of registry/insertion order.
    mPlayers.ForEach([&](PlayerEntity& player) { EvaluateCombat(player); });
    mPlayers.ForEach([](PlayerEntity& player) {
        // Pressed actions are edges. They are consumed by exactly one server
        // tick and can never fire later when an animation happens to finish.
        player.command.pressedActions = 0;
        player.hasPressedActionSample = false;
    });
}

void PlayerSimulation::EnterDeadState(PlayerEntity& player) {
    if (player.respawnTick != 0) return;
    player.velocity = {};
    player.evadeVelocity = {};
    player.actionState = PlayerActionState::Idle;
    player.actionStartTick = mCurrentTick;
    player.respawnTick = mCurrentTick + kRespawnDelayTicks;
    player.hasCommand = false;
    player.pressedActionsForNextTick = 0;
    player.hasPressedActionSample = false;
    player.commandReceivedTick = 0;
    player.recentCommands.clear();
    player.bowPrimaryHeld = false;
    player.bowShotArmed = false;
    player.swordPrimaryHeld = false;
    player.swordSpinCharged = false;
    player.bowDrawStartTick = 0;
    mLifeEvents.push_back(BuildLifeEvent(player, PlayerLifeEventKind::Died));
}

PlayerLifeEvent PlayerSimulation::BuildLifeEvent(
    const PlayerEntity& player, PlayerLifeEventKind kind) const {
    PlayerLifeEvent event{};
    event.kind = kind;
    event.playerId = player.ownerPlayerId;
    event.entity = player.id;
    event.lifeEpoch = player.lifeEpoch;
    event.sceneId = player.sceneId;
    event.position = player.position;
    event.headingRadians = player.headingRadians;
    event.selectedWeapon = player.selectedWeapon;
    event.serverTick = mCurrentTick;
    return event;
}

void PlayerSimulation::CancelGroundedAction(PlayerEntity& player) const {
    if (player.actionState != PlayerActionState::Idle) {
        player.actionState = PlayerActionState::Idle;
        player.actionStartTick = mCurrentTick;
    }
    player.evadeVelocity = {};
    player.hitPlayers.clear();
}

void PlayerSimulation::UpdateAction(PlayerEntity& player) {
    const PlayerCommand& actionSample = player.hasPressedActionSample
        ? player.pressedActionSample
        : player.command;
    const auto transition = [&](PlayerActionState next) {
        if (player.actionState != next) {
            player.actionState = next;
            player.actionStartTick = mCurrentTick;
            if (next == PlayerActionState::PrimaryWindup ||
                next == PlayerActionState::JumpSlashing ||
                next == PlayerActionState::Evading ||
                next == PlayerActionState::SpinAttacking) {
                player.actionHeadingRadians = actionSample.headingRadians;
                player.hitPlayers.clear();
                if (next != PlayerActionState::Evading) {
                    ++player.meleeAttackId;
                    if (player.meleeAttackId == 0) ++player.meleeAttackId;
                }
            }
        }
    };
    const bool swordSelected = player.selectedWeapon == 1 || player.selectedWeapon == 2;
    const bool canBlock = player.selectedWeapon <= 2;
    const bool inputStreamLive =
        player.hasCommand &&
        mCurrentTick - player.commandReceivedTick <= kCommandTimeoutTicks;
    const bool primaryHeld =
        (player.heldActions & PLAYER_ACTION_PRIMARY) != 0;
    const bool primaryWasHeld = player.swordPrimaryHeld;
    const bool primaryReleased = inputStreamLive && swordSelected &&
                                 primaryWasHeld && !primaryHeld;
    const auto selectMeleeAttack = [&]() {
        const MeleeAttackVariant reported =
            MeleeAttackVariantForCommand(actionSample);
        const MeleeAttackVariant base =
            reported == MeleeAttackVariant::ForwardCombo
                ? MeleeAttackVariant::ForwardSlash
                : reported == MeleeAttackVariant::RightCombo
                    ? MeleeAttackVariant::RightSlash
                    : reported == MeleeAttackVariant::LeftCombo
                        ? MeleeAttackVariant::LeftSlash
                        : reported;
        const bool requestedCombo = reported != base;
        const bool sameBase =
            player.meleeAttackVariant == base ||
            (base == MeleeAttackVariant::ForwardSlash &&
             player.meleeAttackVariant == MeleeAttackVariant::ForwardCombo) ||
            (base == MeleeAttackVariant::RightSlash &&
             player.meleeAttackVariant == MeleeAttackVariant::RightCombo) ||
            (base == MeleeAttackVariant::LeftSlash &&
             player.meleeAttackVariant == MeleeAttackVariant::LeftCombo);
        if (sameBase && player.repeatedMeleeAttackCount < 3) {
            ++player.repeatedMeleeAttackCount;
        } else {
            player.repeatedMeleeAttackCount = 1;
        }
        // Native Link reports the exact slash it actually entered. Authority
        // validates a finisher against the preceding same-direction attacks,
        // but never upgrades a reported base slash into an invented combo.
        player.meleeAttackVariant =
            requestedCombo && player.repeatedMeleeAttackCount >= 3
                ? reported
                : base;
    };
    if (!inputStreamLive || !swordSelected) {
        player.swordPrimaryHeld = false;
        player.swordSpinCharged = false;
    } else {
        player.swordPrimaryHeld = primaryHeld;
    }
    if (player.locomotionMode == PlayerLocomotionMode::Climbing) {
        player.swordSpinCharged = false;
        CancelGroundedAction(player);
        return;
    }
    if (player.locomotionMode == PlayerLocomotionMode::Swimming) {
        player.swordSpinCharged = false;
        CancelGroundedAction(player);
        return;
    }
    if (player.locomotionMode == PlayerLocomotionMode::Airborne) {
        player.swordSpinCharged = false;
        // Airborne weapon input remains live. Sword primary becomes a jump
        // slash, while guard and bow draw retain their normal semantic state.
        // Evade is locomotion rather than weapon use and remains grounded.
        if (player.actionState == PlayerActionState::JumpSlashing) {
            player.headingRadians = player.actionHeadingRadians;
            return;
        }
        if (player.actionState == PlayerActionState::Blocking && canBlock &&
            (player.heldActions & PLAYER_ACTION_BLOCK) != 0) return;
        if (player.actionState == PlayerActionState::Aiming && player.selectedWeapon == 3 &&
            (player.heldActions & (PLAYER_ACTION_AIM | PLAYER_ACTION_PRIMARY)) != 0) return;
        CancelGroundedAction(player);
        if (swordSelected &&
            (player.command.pressedActions & PLAYER_ACTION_PRIMARY) != 0) {
            transition(PlayerActionState::JumpSlashing);
        } else if (canBlock &&
                   (player.heldActions & PLAYER_ACTION_BLOCK) != 0) {
            transition(PlayerActionState::Blocking);
        } else if (player.selectedWeapon == 3 &&
                   (player.heldActions &
                    (PLAYER_ACTION_AIM | PLAYER_ACTION_PRIMARY)) != 0) {
            transition(PlayerActionState::Aiming);
        }
        return;
    }
    if (player.actionState == PlayerActionState::JumpSlashing) {
        CancelGroundedAction(player);
    }
    const uint32_t elapsed = mCurrentTick - player.actionStartTick;
    const MeleeAttackTiming meleeTiming =
        MeleeAttackTimingFor(player.meleeAttackVariant, player.selectedWeapon);
    switch (player.actionState) {
        case PlayerActionState::PrimaryWindup:
            player.headingRadians = player.actionHeadingRadians;
            if (elapsed >= meleeTiming.windupTicks) {
                transition(PlayerActionState::PrimaryActive);
            }
            return;
        case PlayerActionState::PrimaryActive:
            player.headingRadians = player.actionHeadingRadians;
            if (elapsed >= meleeTiming.activeTicks) {
                transition(PlayerActionState::PrimaryRecovery);
            }
            return;
        case PlayerActionState::PrimaryRecovery:
            player.headingRadians = player.actionHeadingRadians;
            if (elapsed < meleeTiming.recoveryTicks) {
                return;
            }
            // Native Link enters his sword-charge hold after the ordinary
            // slash finishes. Releasing from that hold starts the non-magic
            // spin attack; no client-authored hit or animation command is
            // needed.
            player.swordSpinCharged = swordSelected && primaryWasHeld;
            transition(PlayerActionState::Idle);
            break;
        case PlayerActionState::SpinAttacking:
            player.headingRadians = player.actionHeadingRadians;
            if (elapsed < kSpinAttackDurationTicks) {
                return;
            }
            player.swordSpinCharged = false;
            transition(PlayerActionState::Idle);
            break;
        case PlayerActionState::Evading:
            player.headingRadians = player.actionHeadingRadians;
            if (elapsed < Game::Simulation::kEvadeDurationTicks) {
                return;
            }
            player.evadeVelocity = {};
            transition(PlayerActionState::Idle);
            break;
        default:
            break;
    }

    if ((player.command.pressedActions & PLAYER_ACTION_EVADE) != 0) {
        player.swordSpinCharged = false;
        player.evadeVelocity = CalculatePlayerEvadeVelocity(actionSample);
        transition(PlayerActionState::Evading);
    } else if (primaryReleased && player.swordSpinCharged) {
        player.swordSpinCharged = false;
        transition(PlayerActionState::SpinAttacking);
    } else if (swordSelected && (player.command.pressedActions & PLAYER_ACTION_PRIMARY) != 0) {
        player.swordSpinCharged = false;
        selectMeleeAttack();
        transition(PlayerActionState::PrimaryWindup);
        if (MeleeAttackTimingFor(player.meleeAttackVariant,
                                 player.selectedWeapon).windupTicks == 0) {
            transition(PlayerActionState::PrimaryActive);
        }
    } else if (canBlock && (player.heldActions & PLAYER_ACTION_BLOCK) != 0) {
        transition(PlayerActionState::Blocking);
    } else if (player.selectedWeapon == 3 &&
               (player.heldActions & (PLAYER_ACTION_AIM | PLAYER_ACTION_PRIMARY)) != 0) {
        transition(PlayerActionState::Aiming);
    } else {
        transition(PlayerActionState::Idle);
    }
}

void PlayerSimulation::UpdateBowDrawState(PlayerEntity& player) {
    const bool inputStreamLive =
        player.hasCommand &&
        mCurrentTick - player.commandReceivedTick <= kCommandTimeoutTicks;
    const bool canDraw = inputStreamLive && player.health != 0 &&
                         player.selectedWeapon == 3 &&
                         player.locomotionMode != PlayerLocomotionMode::Swimming &&
                         player.locomotionMode != PlayerLocomotionMode::Climbing;
    if (!canDraw) {
        player.bowPrimaryHeld = false;
        player.bowShotArmed = false;
        player.bowDrawStartTick = 0;
        return;
    }

    const bool primaryHeld =
        (player.heldActions & PLAYER_ACTION_PRIMARY) != 0;
    if (primaryHeld && !player.bowPrimaryHeld) {
        // A fresh physical press starts one draw cycle. Holding the button
        // after a shot cannot manufacture another permit; release and draw
        // again are required, matching native Link's bow lifecycle.
        player.bowDrawStartTick = mCurrentTick;
        player.bowShotArmed = false;
    }
    if (primaryHeld && player.bowDrawStartTick != 0 &&
        mCurrentTick - player.bowDrawStartTick >= kBowRefireDurationTicks &&
        Sequence::IsAtOrAfter(mCurrentTick, player.nextBowShotTick)) {
        player.bowShotArmed = true;
    }
    // Keep an armed permit across the release edge because the reliable fire
    // intent and disposable movement sample can arrive in either order. The
    // permit is consumed by CommitBowShot or cleared by invalid lifecycle.
    player.bowPrimaryHeld = primaryHeld;
}

void PlayerSimulation::ApplyNativeTraversalRequest(PlayerEntity& player) {
    if (player.command.sequence == player.lastAppliedPoseSequence) {
        // The latest command remains held between incoming 20 Hz native
        // samples. Its controls remain available to actions and animation,
        // but its already-consumed root pose is not another movement tick.
        return;
    }

    const Vec3 requested = player.command.reportedPosition;
    const uint32_t elapsedServerTicks =
        player.lastPoseAdmissionServerTick == 0
            ? 1
            : std::clamp<uint32_t>(
                  mCurrentTick - player.lastPoseAdmissionServerTick, 1, 12);
    player.lastAppliedPoseSequence = player.command.sequence;
    player.lastPoseAdmissionServerTick = mCurrentTick;

    Vec3 delta{ requested.x - player.position.x,
                requested.y - player.position.y,
                requested.z - player.position.z };
    const float horizontalDistance = std::hypot(delta.x, delta.z);

    // Native Link is the prediction implementation, including acceleration,
    // floor following, water entry and animation root motion. Authority admits
    // each resulting pose only once and bounds it before publishing it. This
    // avoids running a second, different walk/swim simulation on top of Link.
    const PlayerLocomotionMode requestedMode =
        player.command.reportedLocomotionMode;
    const bool locomotionModeRejected =
        !LocomotionModeSupported(player, requestedMode, requested);
    const bool climbTraversal = player.nativeClimbAuthorized ||
                                requestedMode == PlayerLocomotionMode::Climbing;
    float maximumHorizontalSpeed = 360.0f;
    float maximumVerticalSpeed = 600.0f;
    if (climbTraversal) {
        // Native ledge animation root motion can advance roughly thirty world
        // units in one 20 Hz gameplay sample. Initial ledge setup can move the
        // root by the full adult ledge height (79.4 units), notably when Link
        // climbs out of water; continuing movement remains on the lower cap.
        maximumHorizontalSpeed = 900.0f;
        maximumVerticalSpeed = requestedMode == PlayerLocomotionMode::Climbing &&
                                       !player.nativeClimbAuthorized
                                   ? 2400.0f
                                   : 900.0f;
    } else if (requestedMode == PlayerLocomotionMode::Swimming) {
        // Swimming uses ordinary native input locomotion. Keep packet and
        // collision slack without granting the sword/climb root-motion cap.
        maximumHorizontalSpeed = 180.0f;
        maximumVerticalSpeed = 300.0f;
    } else if (requestedMode == PlayerLocomotionMode::Grounded) {
        const bool actionRootMotion =
            player.actionState == PlayerActionState::PrimaryWindup ||
            player.actionState == PlayerActionState::PrimaryActive ||
            player.actionState == PlayerActionState::PrimaryRecovery ||
            player.actionState == PlayerActionState::SpinAttacking;
        if (player.actionState == PlayerActionState::Evading) {
            maximumHorizontalSpeed = 220.0f;
        } else {
            // Ordinary Link movement tops out at 120 u/s. Sword animation
            // root motion can reach fifteen units per 20 Hz native frame.
            maximumHorizontalSpeed = actionRootMotion ? 360.0f : 180.0f;
            if (!actionRootMotion &&
                std::hypot(player.command.moveX,
                           player.command.moveY) <= 0.01f) {
                // Native parallel movement decelerates after key release and
                // environmental pushes can move Link without WASD. Preserve
                // that short tail, but do not grant an idle client the full
                // active-locomotion envelope.
                maximumHorizontalSpeed = 90.0f;
            }
        }
        maximumVerticalSpeed = 300.0f;
    }
    const float elapsedSeconds =
        static_cast<float>(elapsedServerTicks) * kPlayerSimulationTickSeconds;
    constexpr float kNativePoseAdmissionSlack = 4.0f;
    bool directionRejected = false;
    const bool ordinaryDirectionalMovement =
        !climbTraversal &&
        (requestedMode == PlayerLocomotionMode::Grounded ||
         requestedMode == PlayerLocomotionMode::Swimming) &&
        (player.actionState == PlayerActionState::Idle ||
         player.actionState == PlayerActionState::Blocking ||
         player.actionState == PlayerActionState::Aiming);
    if (ordinaryDirectionalMovement &&
        horizontalDistance > kNativePoseAdmissionSlack) {
        bool hasDirectionEvidence = false;
        bool alignedWithDirectionEvidence = false;
        const auto evaluateDirection = [&](const PlayerCommand& command) {
            const Vec3 intended = CalculatePlayerVelocity(command);
            const float intendedSpeed = std::hypot(intended.x, intended.z);
            if (intendedSpeed <= 0.25f) return;
            hasDirectionEvidence = true;
            const float along =
                (delta.x * intended.x + delta.z * intended.z) /
                intendedSpeed;
            constexpr float kMinimumUnblockedDirectionCosine = 0.25f;
            const float directionCosine = along / horizontalDistance;
            if (directionCosine >= kMinimumUnblockedDirectionCosine) {
                alignedWithDirectionEvidence = true;
                return;
            }

            // A collision-free native displacement cannot turn sharply away
            // from both causal input headings. If the intended path is
            // actually blocked in the authoritative world, however, native
            // wall sliding may legitimately redirect most of that motion.
            const float intendedX = intended.x / intendedSpeed;
            const float intendedZ = intended.z / intendedSpeed;
            const Vec3 intendedEnd{
                player.position.x + intendedX * horizontalDistance,
                player.position.y,
                player.position.z + intendedZ * horizontalDistance
            };
            if (MovementBlocked(player, player.position, intendedEnd)) {
                alignedWithDirectionEvidence = true;
            }
        };
        evaluateDirection(player.command);
        // A native frame can turn before its post-update pose is sampled.
        // Accept alignment with the immediately preceding input heading too;
        // this preserves legitimate 180-degree mouse turns without allowing
        // arbitrary reverse movement against both causal samples.
        if (player.recentCommands.size() >= 2) {
            evaluateDirection(player.recentCommands[
                player.recentCommands.size() - 2]);
        }
        directionRejected = hasDirectionEvidence &&
                            !alignedWithDirectionEvidence;
    }
    if (locomotionModeRejected || directionRejected || horizontalDistance >
            maximumHorizontalSpeed * elapsedSeconds +
                kNativePoseAdmissionSlack ||
        std::fabs(delta.y) >
            maximumVerticalSpeed * elapsedSeconds +
                kNativePoseAdmissionSlack) {
        player.velocity = {};
        if (requestedMode != PlayerLocomotionMode::Climbing) {
            const bool retiringClimb = player.nativeClimbAuthorized ||
                                       player.locomotionMode ==
                                           PlayerLocomotionMode::Climbing;
            ClearNativeClimbAuthorization(player);
            if (retiringClimb) {
                // A rejected climb-completion pose still retires the native
                // traversal session, but its untrusted mode cannot replace
                // authority. Reclassify the retained server position using
                // collision and water instead.
                UpdateLocomotionSurface(player, 0.0f);
            }
        }
        return;
    }

    const bool continuingClimb =
        player.nativeClimbAuthorized &&
        player.locomotionMode == PlayerLocomotionMode::Climbing;
    if (requestedMode == PlayerLocomotionMode::Climbing) {
        // A backflip/side-hop can leave Link close to the surface he just
        // departed. Do not let the following native ledge probe immediately
        // convert that authoritative evade into a climb back onto the same lip.
        if (player.actionState == PlayerActionState::Evading) {
            player.velocity = {};
            ClearNativeClimbAuthorization(player);
            return;
        }
        if (!continuingClimb &&
            !Sequence::IsAtOrAfter(mCurrentTick,
                                   player.nativeClimbReentryTick)) {
            player.velocity = {};
            return;
        }
        const bool surfaceConfirmed =
            mClimbSurfaceQuery &&
            mClimbSurfaceQuery(player.sceneId, player.position, requested,
                               player.headingRadians);
        if (!continuingClimb && !surfaceConfirmed) {
            player.velocity = {};
            return;
        }
        if (surfaceConfirmed) {
            player.nativeClimbAuthorized = true;
            player.nativeClimbLastSurfacePosition = player.position;
            player.nativeClimbLastSurfaceTick = mCurrentTick;
        } else {
            // Above the lip of a ledge, Link's root legitimately loses the
            // wall before the native climb animation reports Grounded. Keep
            // only a short, spatially bounded completion corridor from the
            // last position independently confirmed against server geometry.
            constexpr uint32_t kClimbSurfaceGraceTicks = 18;
            constexpr float kClimbCompletionHorizontal = 60.0f;
            constexpr float kClimbCompletionVertical = 100.0f;
            const Vec3 fromSurface{
                requested.x - player.nativeClimbLastSurfacePosition.x,
                requested.y - player.nativeClimbLastSurfacePosition.y,
                requested.z - player.nativeClimbLastSurfacePosition.z
            };
            if (player.nativeClimbLastSurfaceTick == 0 ||
                mCurrentTick - player.nativeClimbLastSurfaceTick >
                    kClimbSurfaceGraceTicks ||
                std::hypot(fromSurface.x, fromSurface.z) >
                    kClimbCompletionHorizontal ||
                std::fabs(fromSurface.y) > kClimbCompletionVertical) {
                player.velocity = {};
                return;
            }
        }
    } else {
        // The completion frame of an authorized climb legitimately moves the
        // native root across the wall plane and then reports Grounded. Admit
        // that one transition without interpreting it as an ordinary walk
        // through a wall; subsequent grounded samples use the normal sweep.
        const bool finishingClimb = player.nativeClimbAuthorized;
        ClearNativeClimbAuthorization(player);
        if (finishingClimb) {
            // Native ledge flags can briefly return after Link jumps away or
            // after the completion frame clears them. Do not re-authorize the
            // same lip from that stale transition on following samples.
            player.nativeClimbReentryTick = mCurrentTick + 10;
        }
        if (!finishingClimb &&
            MovementBlocked(player, player.position, requested)) {
            player.velocity = {};
            return;
        }
    }

    player.lastAdmittedPoseSequence = player.command.sequence;
    player.position = requested;
    player.velocity = { delta.x / elapsedSeconds, delta.y / elapsedSeconds,
                        delta.z / elapsedSeconds };
    player.locomotionMode = requestedMode;
}

void PlayerSimulation::EvaluateCombat(PlayerEntity& attacker) {
    const bool jumpSlash = attacker.actionState == PlayerActionState::JumpSlashing;
    const bool spinAttack = attacker.actionState == PlayerActionState::SpinAttacking;
    if ((!jumpSlash && !spinAttack &&
         attacker.actionState != PlayerActionState::PrimaryActive) ||
        attacker.health == 0) {
        return;
    }
    constexpr float pi = 3.14159265358979323846f;
    AuthoritativeMeleeWeaponSegment blade{};
    if (!SampleAuthoritativeMeleeWeaponSegment(BuildSnapshot(attacker), blade)) {
        return;
    }
    const Vec3 bladeDelta{ blade.tip.x - blade.base.x,
                           blade.tip.y - blade.base.y,
                           blade.tip.z - blade.base.z };
    const float bladeLengthSquared = bladeDelta.x * bladeDelta.x +
                                     bladeDelta.y * bladeDelta.y +
                                     bladeDelta.z * bladeDelta.z;
    float worldHitRatio = 2.0f;
    if (mSegmentCast && bladeLengthSquared > 0.00001f) {
        Vec3 worldImpact{};
        if (mSegmentCast(attacker.sceneId, blade.base, blade.tip,
                         worldImpact)) {
            worldHitRatio = std::clamp(
                ((worldImpact.x - blade.base.x) * bladeDelta.x +
                 (worldImpact.y - blade.base.y) * bladeDelta.y +
                 (worldImpact.z - blade.base.z) * bladeDelta.z) /
                    bladeLengthSquared,
                0.0f, 1.0f);
        }
    }
    constexpr float kPlayerRigBroadPhaseRadius = 80.0f;
    for (const PlayerSnapshot& candidate : SnapshotsNearSegment(
             attacker.sceneId, blade.base, blade.tip,
             kPlayerRigBroadPhaseRadius)) {
        PlayerEntity* target = FindPlayer(candidate.ownerPlayerId);
        if (!target || target->ownerPlayerId == attacker.ownerPlayerId ||
            target->sceneId != attacker.sceneId || target->health == 0 ||
            attacker.hitPlayers.contains(target->ownerPlayerId) ||
            (attacker.team != TeamId::Neutral && attacker.team == target->team)) {
            continue;
        }
        const PlayerSnapshot targetSnapshot = BuildSnapshot(*target);
        PlayerRigHit bodyHit{};
        const bool hitBody = SegmentArticulatedPlayerHitRigFirstHit(
            blade.base, blade.tip,
            BuildAuthoritativePlayerHitRig(targetSnapshot), bodyHit);
        ShieldHit shieldHit{};
        const bool hitShield = SegmentAuthoritativeShieldFirstHit(
            blade.base, blade.tip, targetSnapshot, shieldHit);
        const bool shieldFirst = hitShield &&
                                 shieldHit.segmentRatio < worldHitRatio &&
                                 (!hitBody || shieldHit.segmentRatio <=
                                                 bodyHit.segmentRatio);
        const bool bodyFirst = hitBody && bodyHit.segmentRatio < worldHitRatio &&
                               (!hitShield || bodyHit.segmentRatio <
                                                  shieldHit.segmentRatio);
        if (!shieldFirst && !bodyFirst) continue;

        const uint8_t baseDamage = attacker.selectedWeapon == 2 ? 16 : 8;
        const float impactRadians = std::atan2(target->position.x - attacker.position.x,
                                               target->position.z - attacker.position.z);
        const int16_t impactHeading = static_cast<int16_t>(std::lround(impactRadians * (32768.0f / pi)));
        attacker.hitPlayers.insert(target->ownerPlayerId);
        if (shieldFirst) {
            ReportBlockedHit(attacker.ownerPlayerId, target->ownerPlayerId,
                             CombatAttackKind::Melee, impactHeading,
                             shieldHit.position, attacker.meleeAttackId);
        } else {
            ApplyDamage(attacker.ownerPlayerId, target->ownerPlayerId,
                        baseDamage, impactHeading, CombatAttackKind::Melee,
                        bodyHit.position, bodyHit.region,
                        attacker.meleeAttackId);
        }
    }
}

void PlayerSimulation::RefreshSpatialIndex() {
    mPlayers.ForEach([this](const PlayerEntity& player) {
        mPlayerSpatialIndex.Update(player.ownerPlayerId, player.sceneId,
                                   player.position);
    });
}

void PlayerSimulation::MovePlayer(PlayerEntity& player, float deltaSeconds) {
    const float verticalVelocity = player.velocity.y;
    if (!player.hasCommand) {
        player.velocity = {};
        player.velocity.y = verticalVelocity;
        UpdateLocomotionSurface(player, deltaSeconds);
        return;
    }
    player.velocity = player.actionState == PlayerActionState::Evading
                          ? player.evadeVelocity
                          : CalculatePlayerVelocity(player.command);
    player.velocity.y = verticalVelocity;

    Vec3 candidate = player.position;
    candidate.x += player.velocity.x * deltaSeconds;
    if (!MovementBlocked(player, player.position, candidate)) {
        player.position.x = candidate.x;
    } else {
        player.velocity.x = 0.0f;
    }
    candidate = player.position;
    candidate.z += player.velocity.z * deltaSeconds;
    if (!MovementBlocked(player, player.position, candidate)) {
        player.position.z = candidate.z;
    } else {
        player.velocity.z = 0.0f;
    }
    UpdateLocomotionSurface(player, deltaSeconds);
}

void PlayerSimulation::ResolvePlayerCollisions(
    const std::vector<std::pair<int32_t, Vec3>>& startPositions, float deltaSeconds) {
    std::vector<PlayerEntity*> bodies;
    bodies.reserve(mPlayers.Size());
    mPlayers.ForEach([&](PlayerEntity& player) {
        if (player.health != 0) bodies.push_back(&player);
    });
    std::sort(bodies.begin(), bodies.end(), [](const PlayerEntity* left, const PlayerEntity* right) {
        return left->ownerPlayerId < right->ownerPlayerId;
    });

    constexpr float minimumDistance = kBodyRadius * 2.0f;
    constexpr float minimumDistanceSquared = minimumDistance * minimumDistance;
    constexpr float epsilon = 0.0001f;
    for (int pass = 0; pass < kBodySolverPasses; ++pass) {
        using BodyCell = std::tuple<int32_t, int32_t, int32_t>;
        std::map<BodyCell, std::vector<PlayerEntity*>> grid;
        for (PlayerEntity* body : bodies) {
            const int32_t cellX = static_cast<int32_t>(std::floor(body->position.x / minimumDistance));
            const int32_t cellZ = static_cast<int32_t>(std::floor(body->position.z / minimumDistance));
            grid[{ body->sceneId, cellX, cellZ }].push_back(body);
        }
        std::vector<std::pair<PlayerEntity*, PlayerEntity*>> candidatePairs;
        for (PlayerEntity* left : bodies) {
            const int32_t cellX = static_cast<int32_t>(std::floor(left->position.x / minimumDistance));
            const int32_t cellZ = static_cast<int32_t>(std::floor(left->position.z / minimumDistance));
            for (int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
                for (int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                    const auto cell = grid.find({ left->sceneId, cellX + offsetX, cellZ + offsetZ });
                    if (cell == grid.end()) continue;
                    for (PlayerEntity* right : cell->second) {
                        if (right->ownerPlayerId > left->ownerPlayerId) {
                            candidatePairs.emplace_back(left, right);
                        }
                    }
                }
            }
        }
        std::sort(candidatePairs.begin(), candidatePairs.end(), [](const auto& left, const auto& right) {
            return std::tie(left.first->ownerPlayerId, left.second->ownerPlayerId) <
                   std::tie(right.first->ownerPlayerId, right.second->ownerPlayerId);
        });

        bool resolvedAny = false;
        for (const auto& pair : candidatePairs) {
            PlayerEntity& left = *pair.first;
            PlayerEntity& right = *pair.second;
            if (std::abs(left.position.y - right.position.y) >= kBodyHeight) continue;
            const float dx = right.position.x - left.position.x;
            const float dz = right.position.z - left.position.z;
            const float distanceSquared = dx * dx + dz * dz;
            if (distanceSquared >= minimumDistanceSquared) continue;

            float normalX = 1.0f;
            float normalZ = 0.0f;
            float distance = 0.0f;
            if (distanceSquared > epsilon) {
                distance = std::sqrt(distanceSquared);
                normalX = dx / distance;
                normalZ = dz / distance;
            }
            const float penetration = minimumDistance - distance;
            const bool leftMoving = std::abs(left.velocity.x) > epsilon ||
                                    std::abs(left.velocity.z) > epsilon;
            const bool rightMoving = std::abs(right.velocity.x) > epsilon ||
                                     std::abs(right.velocity.z) > epsilon;
            float leftShare = 0.5f;
            float rightShare = 0.5f;
            if (leftMoving != rightMoving) {
                leftShare = leftMoving ? 1.0f : 0.0f;
                rightShare = rightMoving ? 1.0f : 0.0f;
            }

            const Vec3 leftCandidate{ left.position.x - normalX * penetration * leftShare,
                                      left.position.y,
                                      left.position.z - normalZ * penetration * leftShare };
            const Vec3 rightCandidate{ right.position.x + normalX * penetration * rightShare,
                                       right.position.y,
                                       right.position.z + normalZ * penetration * rightShare };
            bool moved = false;
            if (leftShare > 0.0f && !MovementBlocked(left, left.position, leftCandidate)) {
                left.position.x = leftCandidate.x;
                left.position.z = leftCandidate.z;
                moved = true;
            }
            if (rightShare > 0.0f && !MovementBlocked(right, right.position, rightCandidate)) {
                right.position.x = rightCandidate.x;
                right.position.z = rightCandidate.z;
                moved = true;
            }
            resolvedAny = resolvedAny || moved;
        }
        if (!resolvedAny) break;
    }

    for (PlayerEntity* player : bodies) {
        const auto start = std::lower_bound(
            startPositions.begin(), startPositions.end(), player->ownerPlayerId,
            [](const auto& entry, int32_t ownerPlayerId) { return entry.first < ownerPlayerId; });
        if (start != startPositions.end() && start->first == player->ownerPlayerId && deltaSeconds > 0.0f) {
            constexpr float kTau = 6.28318530717958647692f;
            const float distanceX = player->position.x - start->second.x;
            const float distanceZ = player->position.z - start->second.z;
            const float actualDistance = std::hypot(distanceX, distanceZ);
            float animationDistance = actualDistance;
            const bool groundedReportedMovement =
                player->hasCommand && player->command.hasReportedPose &&
                player->command.sequence ==
                    player->lastAdmittedPoseSequence &&
                player->command.reportedLocomotionMode ==
                    PlayerLocomotionMode::Grounded &&
                (std::abs(player->command.moveX) > 0.01f ||
                 std::abs(player->command.moveY) > 0.01f);
            if (groundedReportedMovement) {
                // Native OoT advances gameplay at 20 Hz while the server runs
                // at 30 Hz. Consecutive commands can therefore carry the same
                // root position even though movement remains held. Preserve
                // the continuous movement intent for presentation instead of
                // alternating moving/idle snapshots and restarting the bow
                // locomotion animation every other server tick.
                const Vec3 intended = CalculatePlayerVelocity(player->command);
                player->velocity.x = intended.x;
                player->velocity.z = intended.z;
                animationDistance =
                    std::hypot(intended.x, intended.z) * deltaSeconds;
            } else {
                player->velocity.x = distanceX / deltaSeconds;
                player->velocity.z = distanceZ / deltaSeconds;
            }
            if (animationDistance > 0.0001f) {
                player->locomotionPhaseRadians = std::fmod(
                    player->locomotionPhaseRadians +
                        animationDistance *
                            (kTau / kPlayerLocomotionCycleDistance),
                    kTau);
            }
        }
        const bool liveNativePose =
            player->hasCommand && player->command.hasReportedPose &&
            mCurrentTick - player->commandReceivedTick <= kCommandTimeoutTicks;
        if (!liveNativePose &&
            player->locomotionMode != PlayerLocomotionMode::Climbing) {
            UpdateLocomotionSurface(*player, 0.0f);
        }
    }
}

bool PlayerSimulation::MovementBlocked(const PlayerEntity& player, const Vec3& start, const Vec3& end) const {
    if (!mSegmentCast || (start.x == end.x && start.z == end.z)) {
        return false;
    }
    const float deltaX = end.x - start.x;
    const float deltaZ = end.z - start.z;
    const float distance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    if (distance <= 0.0001f) return false;
    const float forwardX = deltaX / distance;
    const float forwardZ = deltaZ / distance;
    const float sideX = -forwardZ;
    const float sideZ = forwardX;

    // Sweep five points across the leading semicircle at three capsule
    // heights. Center-only rays let the 24-unit player body enter a wall until
    // its origin crossed the polygon; these probes stop the visible cylinder
    // at its actual radius while preserving axis-separated wall sliding.
    constexpr float lateralFactors[] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
    constexpr float heights[] = { 8.0f, 32.0f, 56.0f };
    for (float height : heights) {
        for (float lateralFactor : lateralFactors) {
            const float lateral = lateralFactor * kBodyRadius;
            const float forward = std::sqrt(std::max(
                0.0f, kBodyRadius * kBodyRadius - lateral * lateral));
            const float offsetX = sideX * lateral + forwardX * forward;
            const float offsetZ = sideZ * lateral + forwardZ * forward;
            const Vec3 probeStart{ start.x + offsetX, start.y + height,
                                   start.z + offsetZ };
            const Vec3 probeEnd{ end.x + offsetX, end.y + height,
                                 end.z + offsetZ };
            Vec3 impact{};
            if (mSegmentCast(player.sceneId, probeStart, probeEnd, impact)) {
                return true;
            }
        }
    }
    return false;
}

void PlayerSimulation::UpdateLocomotionSurface(PlayerEntity& player,
                                                float deltaSeconds) const {
    // A water box can extend beneath nearby dry geometry. Link must actually
    // reach its surface before entering swim locomotion; the old 30-unit
    // allowance classified test01's land spawn (15 units above its water)
    // as swimming.
    float surfaceY = 0.0f;
    if (mWaterSurfaceQuery &&
        mWaterSurfaceQuery(player.sceneId, player.position, surfaceY) &&
        std::isfinite(surfaceY) &&
        player.position.y <= surfaceY + kWaterEntryTolerance) {
        bool deepEnough = true;
        if (mSegmentCast) {
            const Vec3 floorProbeStart{ player.position.x, surfaceY + 5.0f,
                                        player.position.z };
            const Vec3 floorProbeEnd{ player.position.x,
                                      surfaceY - kMaximumFloorProbeDepth,
                                      player.position.z };
            Vec3 floor{};
            if (mSegmentCast(player.sceneId, floorProbeStart, floorProbeEnd,
                             floor)) {
                deepEnough = surfaceY - floor.y >= kMinimumSwimmingDepth;
            }
        }
        if (deepEnough) {
            player.locomotionMode = PlayerLocomotionMode::Swimming;
            player.position.y = surfaceY - kSwimmingRootDepth;
            player.velocity.y = 0.0f;
            CancelGroundedAction(player);
            return;
        }
    }

    if (!mSegmentCast ||
        (mCollisionSceneQuery && !mCollisionSceneQuery(player.sceneId))) {
        player.locomotionMode = PlayerLocomotionMode::Grounded;
        player.velocity.y = 0.0f;
        return;
    }

    if (player.locomotionMode != PlayerLocomotionMode::Airborne) {
        const Vec3 floorProbeStart{ player.position.x,
                                    player.position.y + kGroundProbeAbove,
                                    player.position.z };
        const Vec3 floorProbeEnd{ player.position.x,
                                  player.position.y - kGroundProbeBelow,
                                  player.position.z };
        Vec3 floor{};
        if (mSegmentCast(player.sceneId, floorProbeStart, floorProbeEnd, floor)) {
            player.locomotionMode = PlayerLocomotionMode::Grounded;
            player.position.y = floor.y;
            player.velocity.y = 0.0f;
            return;
        }
        player.locomotionMode = PlayerLocomotionMode::Airborne;
        player.velocity.y = 0.0f;
        if (!IsAirborneWeaponState(player.actionState)) {
            CancelGroundedAction(player);
        }
    }

    if (player.locomotionMode == PlayerLocomotionMode::Airborne &&
        !IsAirborneWeaponState(player.actionState)) {
        CancelGroundedAction(player);
    }
    if (deltaSeconds <= 0.0f) return;

    player.velocity.y = std::max(
        kNativeTerminalVelocity,
        player.velocity.y + kNativeGravity * deltaSeconds);
    const Vec3 previous = player.position;
    const Vec3 candidate{ previous.x,
                          previous.y + player.velocity.y * deltaSeconds,
                          previous.z };
    const Vec3 castStart{ previous.x, previous.y + 5.0f, previous.z };
    const Vec3 castEnd{ candidate.x, candidate.y - 5.0f, candidate.z };
    Vec3 impact{};
    if (mSegmentCast(player.sceneId, castStart, castEnd, impact) &&
        impact.y <= castStart.y && impact.y >= castEnd.y) {
        player.position.y = impact.y;
        player.velocity.y = 0.0f;
        player.locomotionMode = PlayerLocomotionMode::Grounded;
        if (player.actionState == PlayerActionState::JumpSlashing) {
            CancelGroundedAction(player);
        }
        return;
    }
    player.position.y = candidate.y;
}

void PlayerSimulation::SnapToFloor(PlayerEntity& player) const {
    if (!mSegmentCast) {
        return;
    }
    const Vec3 start{ player.position.x, player.position.y + 45.0f, player.position.z };
    const Vec3 end{ player.position.x, player.position.y - 100.0f, player.position.z };
    Vec3 floor{};
    if (mSegmentCast(player.sceneId, start, end, floor)) {
        player.position.y = floor.y;
    }
}

PlayerSnapshot PlayerSimulation::BuildSnapshot(const PlayerEntity& player) const {
    PlayerSnapshot snapshot{ player.id,
             player.ownerPlayerId,
             player.sceneId,
             mCurrentTick,
             player.lastProcessedCommand,
             player.lifeEpoch,
             player.position,
             player.velocity,
             player.headingRadians,
             player.aimPitchRadians,
              player.heldActions,
              player.selectedWeapon,
              player.locomotionMode,
              player.actionState,
              player.meleeAttackVariant,
              player.meleeAttackId,
             player.actionStartTick,
             player.health,
             player.team,
             player.locomotionPhaseRadians };
    return snapshot;
}

std::optional<PlayerSnapshot> PlayerSimulation::SnapshotForPlayer(int32_t ownerPlayerId) const {
    const PlayerEntity* player = FindPlayer(ownerPlayerId);
    return player ? std::optional<PlayerSnapshot>(BuildSnapshot(*player)) : std::nullopt;
}

std::vector<PlayerSnapshot> PlayerSimulation::CandidateSnapshotsNear(
    int32_t sceneId, const Vec3& position, float radius) const {
    if (sceneId < 0 || !std::isfinite(radius) || radius < 0.0f) return {};
    return SnapshotsForSpatialCandidates(
        sceneId, mPlayerSpatialIndex.CandidatesNear(sceneId, position, radius));
}

std::vector<PlayerSnapshot> PlayerSimulation::SnapshotsNearSegment(
    int32_t sceneId, const Vec3& start, const Vec3& end,
    float paddingRadius) const {
    std::vector<PlayerSnapshot> snapshots;
    if (sceneId < 0 || !std::isfinite(paddingRadius) || paddingRadius < 0.0f) {
        return snapshots;
    }
    const Vec3 midpoint{ (start.x + end.x) * 0.5f,
                         (start.y + end.y) * 0.5f,
                         (start.z + end.z) * 0.5f };
    const float dx = end.x - start.x;
    const float dz = end.z - start.z;
    const float queryRadius = std::hypot(dx, dz) * 0.5f + paddingRadius;
    return SnapshotsForSpatialCandidates(
        sceneId,
        mPlayerSpatialIndex.CandidatesNear(sceneId, midpoint, queryRadius));
}

std::vector<PlayerSnapshot> PlayerSimulation::SnapshotsForSpatialCandidates(
    int32_t sceneId, const std::vector<SpatialIndexId>& candidates) const {
    std::vector<PlayerSnapshot> snapshots;
    snapshots.reserve(candidates.size());
    for (const SpatialIndexId candidate : candidates) {
        if (candidate < 0 || candidate > INT32_MAX) continue;
        const PlayerEntity* player = FindPlayer(static_cast<int32_t>(candidate));
        if (player && player->sceneId == sceneId) {
            snapshots.push_back(BuildSnapshot(*player));
        }
    }
    return snapshots;
}

std::optional<PlayerCommand> PlayerSimulation::SubmittedCommandForPlayer(
    int32_t ownerPlayerId) const {
    const PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player || !player->hasCommand || player->health == 0 ||
        player->command.lifeEpoch != player->lifeEpoch ||
        player->command.sceneId != player->sceneId ||
        mCurrentTick - player->commandReceivedTick > kCommandTimeoutTicks) {
        return std::nullopt;
    }
    return player->command;
}

std::optional<PlayerCommand>
PlayerSimulation::SubmittedCommandForPlayerAtClientTick(
    int32_t ownerPlayerId, uint32_t clientTick) const {
    const PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player || !player->hasCommand || player->health == 0 ||
        mCurrentTick - player->commandReceivedTick > kCommandTimeoutTicks) {
        return std::nullopt;
    }
    for (auto command = player->recentCommands.rbegin();
         command != player->recentCommands.rend(); ++command) {
        if (command->clientTick == clientTick &&
            command->lifeEpoch == player->lifeEpoch &&
            command->sceneId == player->sceneId) {
            return *command;
        }
    }
    return std::nullopt;
}

std::vector<PlayerSnapshot> PlayerSimulation::Snapshots() const {
    std::vector<PlayerSnapshot> snapshots;
    snapshots.reserve(mPlayers.Size());
    mPlayers.ForEach([&](const PlayerEntity& player) { snapshots.push_back(BuildSnapshot(player)); });
    return snapshots;
}

std::vector<CombatResultEvent> PlayerSimulation::DrainCombatResults() {
    std::vector<CombatResultEvent> events;
    events.swap(mCombatResults);
    return events;
}

uint32_t PlayerSimulation::NextCombatEventId() {
    if (mNextCombatEventId == 0) mNextCombatEventId = 1;
    const uint32_t result = mNextCombatEventId++;
    if (mNextCombatEventId == 0) mNextCombatEventId = 1;
    return result;
}

std::vector<PlayerLifeEvent> PlayerSimulation::DrainLifeEvents() {
    std::vector<PlayerLifeEvent> events;
    events.swap(mLifeEvents);
    return events;
}

uint32_t PlayerSimulation::CurrentTick() const {
    return mCurrentTick;
}

} // namespace Game::Simulation
