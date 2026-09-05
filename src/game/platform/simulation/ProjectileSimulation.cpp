#include "ProjectileSimulation.h"
#include "AuthoritativePlayerHitRig.h"
#include "CombatGeometry.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

namespace Game::Simulation {
namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

void ProjectileSimulation::SetCollisionQuery(SegmentCast segmentCast) {
    mSegmentCast = std::move(segmentCast);
}

std::optional<ArrowSnapshot> ProjectileSimulation::SpawnArrow(const ArrowSpawn& spawn) {
    if (spawn.ownerPlayerId < 0 || spawn.sceneId < 0) {
        return std::nullopt;
    }
    ArrowEntity arrow{};
    arrow.replicationId = TakeReplicationId();
    if (arrow.replicationId <= 0) return std::nullopt;
    arrow.ownerPlayerId = spawn.ownerPlayerId;
    arrow.sceneId = spawn.sceneId;
    arrow.projectileType = spawn.projectileType;
    arrow.position = spawn.position;
    arrow.velocity = spawn.velocity;
    const float horizontalSpeed = std::hypot(spawn.velocity.x, spawn.velocity.z);
    arrow.rotationX = static_cast<int16_t>(
        std::atan2(-spawn.velocity.y, horizontalSpeed) * (32768.0f / kPi));
    arrow.rotationY = spawn.rotationY;
    arrow.spawnTick = mCurrentTick;
    arrow.lastSnapshotTick = mCurrentTick;
    const EntityId id = mArrows.Create(std::move(arrow));
    ArrowEntity* created = mArrows.Get(id);
    if (!created) return std::nullopt;
    created->id = id;
    mArrowByReplicationId.insert_or_assign(created->replicationId, id);
    mArrowsByOwner[created->ownerPlayerId].push_back(id);
    QueueEvent(ArrowEventKind::Created, *created);
    return BuildSnapshot(*created);
}

bool ProjectileSimulation::HasArrow(int32_t ownerPlayerId, int32_t replicationId) const {
    return FindArrow(ownerPlayerId, replicationId) != nullptr;
}

bool ProjectileSimulation::RemoveArrow(int32_t ownerPlayerId,
                                       int32_t replicationId) {
    ArrowEntity* arrow = FindArrow(ownerPlayerId, replicationId);
    if (!arrow) return false;
    arrow->active = false;
    ++arrow->sequence;
    QueueEvent(ArrowEventKind::Removed, *arrow);
    return DestroyArrow(arrow->id);
}

void ProjectileSimulation::RemoveOwnedBy(int32_t ownerPlayerId) {
    const auto owned = mArrowsByOwner.find(ownerPlayerId);
    if (owned == mArrowsByOwner.end()) return;
    const std::vector<EntityId> removeArrows = owned->second;
    for (const EntityId id : removeArrows) {
        ArrowEntity* arrow = mArrows.Get(id);
        if (!arrow || arrow->ownerPlayerId != ownerPlayerId) continue;
        arrow->active = false;
        ++arrow->sequence;
        QueueEvent(ArrowEventKind::Removed, *arrow);
        DestroyArrow(id);
    }
}

void ProjectileSimulation::DetachFromPlayerLife(int32_t playerId,
                                                uint32_t lifeEpoch) {
    if (playerId < 0 || lifeEpoch == 0) return;
    mArrows.ForEach([&](ArrowEntity& arrow) {
        if (!arrow.active || arrow.phase != ArrowPhase::Stuck ||
            arrow.body.playerId != playerId ||
            arrow.body.lifeEpoch != lifeEpoch) {
            return;
        }
        // The retained corpse is a distinct entity. Keep the arrow at the
        // authoritative impact location instead of allowing the persistent
        // live-player slot to carry it into the next life epoch.
        arrow.body = {};
        arrow.lastSnapshotTick = mCurrentTick;
        ++arrow.sequence;
        // This is a persistent semantic state change, not an expendable
        // movement sample. Observers must learn that the old player life no
        // longer owns the attachment even if subsequent UDP snapshots drop.
        QueueEvent(ArrowEventKind::BodyDetached, arrow);
    });
}

void ProjectileSimulation::Reset() {
    mArrows.Clear();
    mArrowByReplicationId.clear();
    mArrowsByOwner.clear();
    mEvents.clear();
    mCurrentTick = 0;
    mNextReplicationId = 1;
}

void ProjectileSimulation::StepFixed(PlayerSimulation& players) {
    SimulateTick(players, nullptr);
}

void ProjectileSimulation::StepFixed(PlayerSimulation& players, StructureSimulation& structures) {
    SimulateTick(players, &structures);
}

void ProjectileSimulation::SimulateTick(PlayerSimulation& players, StructureSimulation* structures) {
    ++mCurrentTick;
    std::vector<EntityId> removeArrows;
    mArrows.ForEach([&](ArrowEntity& arrow) {
        SimulateArrow(arrow, players, structures, removeArrows);
    });
    for (EntityId id : removeArrows) {
        DestroyArrow(id);
    }
}

void ProjectileSimulation::SimulateArrow(ArrowEntity& arrow, PlayerSimulation& players,
                                         StructureSimulation* structures,
                                         std::vector<EntityId>& remove) {
    if (!arrow.active) {
        return;
    }
    if (arrow.phase == ArrowPhase::Stuck) {
        if (arrow.body.playerId >= 0) {
            const auto player = players.SnapshotForPlayer(arrow.body.playerId);
            if (!player || player->sceneId != arrow.sceneId || player->lifeEpoch != arrow.body.lifeEpoch) {
                arrow.active = false;
                ++arrow.sequence;
                QueueEvent(ArrowEventKind::Removed, arrow);
                remove.push_back(arrow.id);
                return;
            }
            if (player->health == 0) {
                // DrainPlayerLifeEvents transfers this attachment to static
                // retained-world presentation. Do not first snap it from the
                // impact pose onto the standing semantic fallback skeleton.
                return;
            }
            Vec3 direction{};
            ResolveArrowOnBody(arrow.body, BuildAuthoritativePlayerHitRig(*player),
                               player->headingRadians, arrow.position, direction);
            arrow.rotationX = static_cast<int16_t>(std::atan2(-direction.y, std::hypot(direction.x, direction.z)) * (32768.0f / kPi));
            arrow.rotationY = static_cast<int16_t>(std::atan2(direction.x, direction.z) * (32768.0f / kPi));
            if (mCurrentTick - arrow.lastSnapshotTick >= kBroadcastIntervalTicks) {
                arrow.lastSnapshotTick = mCurrentTick;
                ++arrow.sequence;
                QueueEvent(ArrowEventKind::Snapshot, arrow);
            }
        }
        if (structures && arrow.attachedStructureKey >= 0) {
            const auto attached = structures->SnapshotForStructure(arrow.attachedStructureKey);
            if (!attached || attached->phase != StructurePhase::Active) {
                arrow.active = false;
                ++arrow.sequence;
                QueueEvent(ArrowEventKind::Removed, arrow);
                remove.push_back(arrow.id);
            }
        }
        return;
    }
    if (arrow.phase != ArrowPhase::Flying) return;
    const Vec3 previous = arrow.position;
    if (mCurrentTick - arrow.spawnTick >= kGravityDelayTicks) {
        arrow.velocity.y -= 160.0f * kTickSeconds;
    }
    arrow.position.x += arrow.velocity.x * kTickSeconds;
    arrow.position.y += arrow.velocity.y * kTickSeconds;
    arrow.position.z += arrow.velocity.z * kTickSeconds;
    const float horizontalSpeed = std::hypot(arrow.velocity.x, arrow.velocity.z);
    arrow.rotationX = static_cast<int16_t>(
        std::atan2(-arrow.velocity.y, horizontalSpeed) * (32768.0f / kPi));

    if (std::fabs(arrow.position.x) >= 32000.0f || std::fabs(arrow.position.y) >= 32000.0f ||
        std::fabs(arrow.position.z) >= 32000.0f) {
        arrow.active = false;
        ++arrow.sequence;
        QueueEvent(ArrowEventKind::Removed, arrow);
        remove.push_back(arrow.id);
        return;
    }

    const Vec3 segment{ arrow.position.x - previous.x, arrow.position.y - previous.y,
                        arrow.position.z - previous.z };
    const float lengthSquared = segment.x * segment.x + segment.y * segment.y + segment.z * segment.z;
    Vec3 staticImpact{};
    const bool hitStatic = mSegmentCast && mSegmentCast(arrow.sceneId, previous, arrow.position, staticImpact);
    float staticRatio = 2.0f;
    if (hitStatic && lengthSquared > 0.00001f) {
        staticRatio = std::clamp(((staticImpact.x - previous.x) * segment.x +
                                  (staticImpact.y - previous.y) * segment.y +
                                  (staticImpact.z - previous.z) * segment.z) /
                                     lengthSquared,
                                 0.0f, 1.0f);
    }

    int32_t bodyPlayer = -1;
    PlayerHitRegion bodyRegion = PlayerHitRegion::None;
    int32_t shieldPlayer = -1;
    float bodyRatio = 2.0f;
    float shieldRatio = 2.0f;
    const auto sourceTeam = players.TeamForPlayer(arrow.ownerPlayerId);
    constexpr float kPlayerRigBroadPhaseRadius = 80.0f;
    for (const PlayerSnapshot& player : players.SnapshotsNearSegment(
             arrow.sceneId, previous, arrow.position,
             kPlayerRigBroadPhaseRadius)) {
        if (player.ownerPlayerId == arrow.ownerPlayerId || player.sceneId != arrow.sceneId || player.health == 0) {
            continue;
        }
        if (sourceTeam && *sourceTeam != TeamId::Neutral && player.team == *sourceTeam) {
            continue;
        }
        PlayerRigHit candidate{};
        if (SegmentArticulatedPlayerHitRigFirstHit(
                previous, arrow.position, BuildAuthoritativePlayerHitRig(player), candidate) &&
            candidate.segmentRatio < bodyRatio) {
            bodyRatio = candidate.segmentRatio;
            bodyPlayer = player.ownerPlayerId;
            bodyRegion = candidate.region;
        }
        ShieldHit shieldHit{};
        if (SegmentAuthoritativeShieldFirstHit(previous, arrow.position, player,
                                               shieldHit) &&
            shieldHit.segmentRatio < shieldRatio) {
            shieldRatio = shieldHit.segmentRatio;
            shieldPlayer = player.ownerPlayerId;
        }
    }

    StructureHit structureHit{};
    const bool hitStructure = structures &&
                              structures->FirstSegmentHit(arrow.sceneId, previous,
                                                          arrow.position, structureHit);
    const float structureRatio = hitStructure ? structureHit.segmentRatio : 2.0f;

    if (hitStatic && staticRatio <= bodyRatio && staticRatio <= shieldRatio &&
        staticRatio <= structureRatio) {
        StickArrow(arrow, staticImpact);
        RetainStuckArrows(arrow, remove);
        return;
    }
    if (shieldPlayer >= 0 && shieldRatio <= bodyRatio && shieldRatio < staticRatio &&
        shieldRatio <= structureRatio) {
        arrow.position = { previous.x + segment.x * shieldRatio, previous.y + segment.y * shieldRatio,
                           previous.z + segment.z * shieldRatio };
        const int16_t impactHeading = static_cast<int16_t>(
            std::atan2(arrow.velocity.x, arrow.velocity.z) * (32768.0f / kPi));
        players.ReportBlockedHit(arrow.ownerPlayerId, shieldPlayer, CombatAttackKind::Arrow,
                                 impactHeading, arrow.position);
        arrow.phase = ArrowPhase::Blocked;
        arrow.active = false;
        ++arrow.sequence;
        QueueEvent(ArrowEventKind::Blocked, arrow, shieldPlayer);
        remove.push_back(arrow.id);
        return;
    }
    if (bodyPlayer >= 0 && bodyRatio < staticRatio && bodyRatio < shieldRatio &&
        bodyRatio <= structureRatio) {
        arrow.position = { previous.x + segment.x * bodyRatio, previous.y + segment.y * bodyRatio,
                           previous.z + segment.z * bodyRatio };
        const int16_t impactHeading = static_cast<int16_t>(
            std::atan2(arrow.velocity.x, arrow.velocity.z) * (32768.0f / kPi));
        const auto target = players.SnapshotForPlayer(bodyPlayer);
        arrow.body.playerId = bodyPlayer;
        arrow.body.lifeEpoch = target->lifeEpoch;
        arrow.body.region = bodyRegion;
        const float speed = std::sqrt(lengthSquared);
        const Vec3 direction = speed > 0.00001f
            ? Vec3{ segment.x / speed, segment.y / speed, segment.z / speed }
            : Vec3{ std::sin(arrow.rotationY * (kPi / 32768.0f)), 0.0f,
                    std::cos(arrow.rotationY * (kPi / 32768.0f)) };
        BindArrowToBody(arrow.body, BuildAuthoritativePlayerHitRig(*target), target->headingRadians,
                        arrow.position, direction);
        players.ApplyDamage(arrow.ownerPlayerId, bodyPlayer, 8, impactHeading,
                            CombatAttackKind::Arrow, arrow.position, bodyRegion);
        arrow.phase = ArrowPhase::Stuck;
        arrow.velocity = {};
        arrow.impactTick = mCurrentTick;
        ++arrow.sequence;
        QueueEvent(ArrowEventKind::HitPlayer, arrow, bodyPlayer);
        RetainStuckArrows(arrow, remove);
        return;
    }
    if (hitStructure && structureRatio < staticRatio && structureRatio < shieldRatio &&
        structureRatio < bodyRatio) {
        if (sourceTeam) {
            structures->ApplyDamage(structureHit.structureKey, *sourceTeam, 8);
        }
        StickArrow(arrow, structureHit.position, ArrowEventKind::HitStructure,
                   structureHit.structureKey);
        RetainStuckArrows(arrow, remove);
        return;
    }
    if (mCurrentTick - arrow.lastSnapshotTick >= kBroadcastIntervalTicks) {
        arrow.lastSnapshotTick = mCurrentTick;
        ++arrow.sequence;
        QueueEvent(ArrowEventKind::Snapshot, arrow);
    }
}

void ProjectileSimulation::StickArrow(ArrowEntity& arrow, const Vec3& position,
                                      ArrowEventKind eventKind, int32_t hitStructureKey) {
    arrow.position = position;
    arrow.velocity = {};
    arrow.phase = ArrowPhase::Stuck;
    arrow.attachedStructureKey = hitStructureKey;
    arrow.impactTick = mCurrentTick;
    ++arrow.sequence;
    QueueEvent(eventKind, arrow, -1, hitStructureKey);
}

void ProjectileSimulation::RetainStuckArrows(const ArrowEntity& current, std::vector<EntityId>& remove) {
    constexpr size_t maximum = 99;
    std::vector<const ArrowEntity*> stuck;
    const auto owned = mArrowsByOwner.find(current.ownerPlayerId);
    if (owned == mArrowsByOwner.end()) return;
    stuck.reserve(owned->second.size());
    for (const EntityId id : owned->second) {
        const ArrowEntity* arrow = mArrows.Get(id);
        if (arrow && arrow->sceneId == current.sceneId &&
            arrow->phase == ArrowPhase::Stuck) {
            stuck.push_back(arrow);
        }
    }
    if (stuck.size() <= maximum) {
        return;
    }
    std::sort(stuck.begin(), stuck.end(), [](const ArrowEntity* left, const ArrowEntity* right) {
        return left->impactTick < right->impactTick;
    });
    for (size_t index = 0; index < stuck.size() - maximum; ++index) {
        ArrowEntity* oldest = mArrows.Get(stuck[index]->id);
        if (!oldest || oldest->id == current.id) {
            continue;
        }
        oldest->active = false;
        ++oldest->sequence;
        QueueEvent(ArrowEventKind::Removed, *oldest);
        remove.push_back(oldest->id);
    }
}

void ProjectileSimulation::QueueEvent(ArrowEventKind kind, const ArrowEntity& arrow,
                                      int32_t hitPlayerId, int32_t hitStructureKey) {
    mEvents.push_back({ kind, BuildSnapshot(arrow), hitPlayerId, hitStructureKey });
}

ProjectileSimulation::ArrowEntity* ProjectileSimulation::FindArrow(
    int32_t ownerPlayerId, int32_t replicationId) {
    const auto indexed = mArrowByReplicationId.find(replicationId);
    if (indexed == mArrowByReplicationId.end()) return nullptr;
    ArrowEntity* arrow = mArrows.Get(indexed->second);
    if (arrow && arrow->ownerPlayerId == ownerPlayerId &&
        arrow->replicationId == replicationId) {
        return arrow;
    }
    if (!arrow || arrow->replicationId != replicationId) {
        mArrowByReplicationId.erase(indexed);
    }
    return nullptr;
}

const ProjectileSimulation::ArrowEntity* ProjectileSimulation::FindArrow(
    int32_t ownerPlayerId, int32_t replicationId) const {
    const auto indexed = mArrowByReplicationId.find(replicationId);
    if (indexed == mArrowByReplicationId.end()) return nullptr;
    const ArrowEntity* arrow = mArrows.Get(indexed->second);
    return arrow && arrow->ownerPlayerId == ownerPlayerId &&
                   arrow->replicationId == replicationId
               ? arrow
               : nullptr;
}

bool ProjectileSimulation::DestroyArrow(EntityId id) {
    const ArrowEntity* arrow = mArrows.Get(id);
    if (!arrow) return false;
    const int32_t replicationId = arrow->replicationId;
    const int32_t ownerPlayerId = arrow->ownerPlayerId;
    if (!mArrows.Destroy(id)) return false;
    mArrowByReplicationId.erase(replicationId);
    const auto owned = mArrowsByOwner.find(ownerPlayerId);
    if (owned != mArrowsByOwner.end()) {
        std::erase(owned->second, id);
        if (owned->second.empty()) mArrowsByOwner.erase(owned);
    }
    return true;
}

ArrowSnapshot ProjectileSimulation::BuildSnapshot(const ArrowEntity& arrow) const {
    return { arrow.id,          arrow.replicationId, arrow.ownerPlayerId,
             arrow.sceneId,     arrow.sequence,      arrow.active,        arrow.phase,
             arrow.projectileType, arrow.position,  arrow.velocity,      arrow.rotationX,
             arrow.rotationY,   arrow.rotationZ, arrow.body };
}

int32_t ProjectileSimulation::TakeReplicationId() {
    const int32_t first = mNextReplicationId;
    do {
        const int32_t candidate = mNextReplicationId++;
        if (mNextReplicationId <= 0 || mNextReplicationId == INT32_MAX) {
            mNextReplicationId = 1;
        }
        const bool used = mArrowByReplicationId.contains(candidate);
        if (!used) return candidate;
    } while (mNextReplicationId != first);
    return 0;
}

std::vector<ArrowSnapshot> ProjectileSimulation::Snapshots() const {
    std::vector<ArrowSnapshot> snapshots;
    snapshots.reserve(mArrows.Size());
    mArrows.ForEach([&](const ArrowEntity& arrow) { snapshots.push_back(BuildSnapshot(arrow)); });
    return snapshots;
}

std::vector<ArrowEvent> ProjectileSimulation::DrainEvents() {
    std::vector<ArrowEvent> events;
    events.swap(mEvents);
    return events;
}

uint32_t ProjectileSimulation::CurrentTick() const {
    return mCurrentTick;
}

} // namespace Game::Simulation
