#include "PlayerSimulation.h"
#include "AuthoritativePlayerHitRig.h"
#include "AuthoritativeMeleeWeapon.h"
#include "CombatGeometry.h"
#include "../SequenceNumber.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>

namespace Game::Simulation {
namespace {

float ClampAxis(float value) {
    return std::clamp(value, -1.0f, 1.0f);
}

bool IsAirborneWeaponState(PlayerActionState state) {
    return state == PlayerActionState::JumpSlashing ||
           state == PlayerActionState::Blocking ||
           state == PlayerActionState::Aiming;
}

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
    constexpr float runSpeed = 180.0f;
    constexpr float guardSpeed = 80.0f;
    const float speed = (command.heldActions & (PLAYER_ACTION_BLOCK | PLAYER_ACTION_AIM)) != 0
                            ? guardSpeed
                            : runSpeed;
    return { (sinHeading * moveY + cosHeading * moveX) * speed,
             0.0f,
             (cosHeading * moveY - sinHeading * moveX) * speed };
}

Vec3 CalculatePlayerEvadeVelocity(const PlayerCommand& command) {
    const float sinHeading = std::sin(command.headingRadians);
    const float cosHeading = std::cos(command.headingRadians);
    if (std::abs(command.moveX) > 0.25f) {
        constexpr float sideHopSpeed = 170.0f;
        const float direction = command.moveX < 0.0f ? -1.0f : 1.0f;
        return { cosHeading * sideHopSpeed * direction, 0.0f,
                 -sinHeading * sideHopSpeed * direction };
    }
    constexpr float backflipSpeed = 120.0f;
    return { -sinHeading * backflipSpeed, 0.0f,
             -cosHeading * backflipSpeed };
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

EntityId PlayerSimulation::EnsurePlayer(int32_t ownerPlayerId, const PlayerSpawn& spawn) {
    if (ownerPlayerId < 0 || spawn.sceneId < 0) return {};
    if (PlayerEntity* existing = FindPlayer(ownerPlayerId)) {
        return existing->id;
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
    player->commandReceivedTick = 0;
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
    player.commandReceivedTick = 0;
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
        }
    }
    if (newerMovement) {
        player->command = command;
        player->command.moveX = ClampAxis(command.moveX);
        player->command.moveY = ClampAxis(command.moveY);
        player->command.pressedActions = 0;
        player->commandReceivedTick = mCurrentTick;
        player->hasCommand = true;
    }
    return true;
}

bool PlayerSimulation::ApplyDamage(int32_t sourcePlayerId, int32_t targetPlayerId, uint8_t damage,
                                    int16_t impactHeading, CombatAttackKind attackKind,
                                    const Vec3& impactPosition,
                                    PlayerHitRegion hitRegion) {
    PlayerEntity* target = FindPlayer(targetPlayerId);
    const PlayerEntity* source = FindPlayer(sourcePlayerId);
    if (!target || target->health == 0 || damage == 0 ||
        (source && sourcePlayerId != targetPlayerId && source->team != TeamId::Neutral &&
         source->team == target->team)) {
        return false;
    }
    target->health = damage >= target->health ? 0 : static_cast<uint8_t>(target->health - damage);
    if (target->health == 0) EnterDeadState(*target);
    mCombatResults.push_back({ NextCombatEventId(), sourcePlayerId, targetPlayerId,
                               source ? source->id : EntityId{},
                                target->id, target->sceneId, attackKind, CombatResultKind::Damaged,
                                damage, impactHeading, impactPosition, hitRegion });
    return true;
}

bool PlayerSimulation::ReportBlockedHit(int32_t sourcePlayerId, int32_t targetPlayerId,
                                        CombatAttackKind attackKind, int16_t impactHeading,
                                        const Vec3& impactPosition) {
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
                               target->sceneId, attackKind, CombatResultKind::Blocked, 0,
                               impactHeading, impactPosition });
    return true;
}

bool PlayerSimulation::BowShotReady(int32_t ownerPlayerId) const {
    const PlayerEntity* player = FindPlayer(ownerPlayerId);
    return player && player->health != 0 && player->selectedWeapon == 3 &&
           player->actionState == PlayerActionState::Aiming &&
           mCurrentTick - player->actionStartTick >=
               kBowMinimumDrawDurationTicks;
}

bool PlayerSimulation::CommitBowShot(int32_t ownerPlayerId) {
    PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player || !BowShotReady(ownerPlayerId)) return false;
    // A shot consumes the current draw. Keeping the semantic Aiming state
    // preserves the held bow pose while restarting its authoritative draw
    // clock; another packet cannot manufacture an immediately re-notched
    // arrow even if its intent sequence and transport cooldown are valid.
    player->actionStartTick = mCurrentTick;
    return true;
}

bool PlayerSimulation::SetPlayerTeam(int32_t ownerPlayerId, TeamId team) {
    PlayerEntity* player = FindPlayer(ownerPlayerId);
    if (!player) {
        return false;
    }
    player->team = team;
    return true;
}

std::optional<TeamId> PlayerSimulation::TeamForPlayer(int32_t ownerPlayerId) const {
    const PlayerEntity* player = FindPlayer(ownerPlayerId);
    return player ? std::optional<TeamId>(player->team) : std::nullopt;
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
        }
        if (player.hasCommand) {
            player.command.pressedActions = player.pressedActionsForNextTick;
            player.pressedActionsForNextTick = 0;
            player.headingRadians = player.command.headingRadians;
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
        MovePlayer(player, kPlayerSimulationTickSeconds);
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
    player.commandReceivedTick = 0;
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
    const auto transition = [&](PlayerActionState next) {
        if (player.actionState != next) {
            player.actionState = next;
            player.actionStartTick = mCurrentTick;
            if (next == PlayerActionState::PrimaryWindup ||
                next == PlayerActionState::JumpSlashing) {
                player.hitPlayers.clear();
            }
        }
    };
    const bool swordSelected = player.selectedWeapon == 1 || player.selectedWeapon == 2;
    if (player.locomotionMode == PlayerLocomotionMode::Swimming) {
        CancelGroundedAction(player);
        return;
    }
    if (player.locomotionMode == PlayerLocomotionMode::Airborne) {
        // Airborne weapon input remains live. Sword primary becomes a jump
        // slash, while guard and bow draw retain their normal semantic state.
        // Evade is locomotion rather than weapon use and remains grounded.
        if (player.actionState == PlayerActionState::JumpSlashing) return;
        if (player.actionState == PlayerActionState::Blocking && swordSelected &&
            (player.heldActions & PLAYER_ACTION_BLOCK) != 0) return;
        if (player.actionState == PlayerActionState::Aiming && player.selectedWeapon == 3 &&
            (player.heldActions & (PLAYER_ACTION_AIM | PLAYER_ACTION_PRIMARY)) != 0) return;
        CancelGroundedAction(player);
        if (swordSelected &&
            (player.command.pressedActions & PLAYER_ACTION_PRIMARY) != 0) {
            transition(PlayerActionState::JumpSlashing);
        } else if (swordSelected &&
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
    switch (player.actionState) {
        case PlayerActionState::PrimaryWindup:
            if (elapsed >= kPrimaryWindupDurationTicks) {
                transition(PlayerActionState::PrimaryActive);
            }
            return;
        case PlayerActionState::PrimaryActive:
            if (elapsed >= kPrimaryActiveDurationTicks) {
                transition(PlayerActionState::PrimaryRecovery);
            }
            return;
        case PlayerActionState::PrimaryRecovery:
            if (elapsed < kPrimaryRecoveryDurationTicks) {
                return;
            }
            transition(PlayerActionState::Idle);
            break;
        case PlayerActionState::Evading:
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
        player.evadeVelocity = CalculatePlayerEvadeVelocity(player.command);
        transition(PlayerActionState::Evading);
    } else if (swordSelected && (player.command.pressedActions & PLAYER_ACTION_PRIMARY) != 0) {
        transition(PlayerActionState::PrimaryWindup);
    } else if (swordSelected && (player.heldActions & PLAYER_ACTION_BLOCK) != 0) {
        transition(PlayerActionState::Blocking);
    } else if (player.selectedWeapon == 3 &&
               (player.heldActions & (PLAYER_ACTION_AIM | PLAYER_ACTION_PRIMARY)) != 0) {
        transition(PlayerActionState::Aiming);
    } else {
        transition(PlayerActionState::Idle);
    }
}

void PlayerSimulation::EvaluateCombat(PlayerEntity& attacker) {
    const bool jumpSlash = attacker.actionState == PlayerActionState::JumpSlashing;
    if ((!jumpSlash && attacker.actionState != PlayerActionState::PrimaryActive) ||
        attacker.health == 0) {
        return;
    }
    constexpr float pi = 3.14159265358979323846f;
    AuthoritativeMeleeWeaponSegment blade{};
    if (!SampleAuthoritativeMeleeWeaponSegment(BuildSnapshot(attacker), blade)) {
        return;
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
        if (!SegmentArticulatedPlayerHitRigFirstHit(
                blade.base, blade.tip,
                BuildAuthoritativePlayerHitRig(targetSnapshot), bodyHit)) continue;

        ShieldHit shieldHit{};
        const bool blocked = SegmentAuthoritativeShieldFirstHit(
                                 blade.base, blade.tip, targetSnapshot, shieldHit) &&
                             shieldHit.segmentRatio <= bodyHit.segmentRatio;
        const uint8_t damage = attacker.selectedWeapon == 2 ? 16 : 8;
        const float impactRadians = std::atan2(target->position.x - attacker.position.x,
                                               target->position.z - attacker.position.z);
        const int16_t impactHeading = static_cast<int16_t>(std::lround(impactRadians * (32768.0f / pi)));
        attacker.hitPlayers.insert(target->ownerPlayerId);
        if (!blocked) {
            target->health = damage >= target->health
                                 ? 0
                                 : static_cast<uint8_t>(target->health - damage);
            if (target->health == 0) EnterDeadState(*target);
        }
        mCombatResults.push_back({ NextCombatEventId(), attacker.ownerPlayerId,
                                   target->ownerPlayerId, attacker.id,
                                   target->id, target->sceneId, CombatAttackKind::Melee,
                                   blocked ? CombatResultKind::Blocked : CombatResultKind::Damaged,
                                   blocked ? uint8_t{ 0 } : damage, impactHeading,
                                   blocked ? shieldHit.position
                                           : bodyHit.position,
                                   blocked ? PlayerHitRegion::None : bodyHit.region });
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
            const float distance = std::hypot(distanceX, distanceZ);
            if (distance > 0.0001f) {
                player->locomotionPhaseRadians = std::fmod(
                    player->locomotionPhaseRadians +
                        distance * (kTau / kPlayerLocomotionCycleDistance),
                    kTau);
            }
            player->velocity.x = distanceX / deltaSeconds;
            player->velocity.z = distanceZ / deltaSeconds;
        }
        UpdateLocomotionSurface(*player, 0.0f);
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
    constexpr float kWaterEntryTolerance = 30.0f;
    constexpr float kMinimumSwimmingDepth = 40.0f;
    constexpr float kMaximumFloorProbeDepth = 500.0f;
    constexpr float kGroundProbeAbove = 45.0f;
    constexpr float kGroundProbeBelow = 100.0f;
    constexpr float kGravity = -900.0f;
    constexpr float kTerminalVelocity = -700.0f;

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
            player.position.y = surfaceY;
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

    player.velocity.y = std::max(kTerminalVelocity,
                                 player.velocity.y + kGravity * deltaSeconds);
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
    return { player.id,
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
             player.actionStartTick,
             player.health,
             player.team,
             player.locomotionPhaseRadians };
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
