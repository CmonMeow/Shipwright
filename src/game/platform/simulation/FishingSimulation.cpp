#include "FishingSimulation.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace Game::Simulation {
namespace {

constexpr float kCastHorizontalSpeed = 600.0f;
constexpr float kCastVerticalSpeed = 300.0f;
constexpr float kGravity = 400.0f;
constexpr float kReelSpeed = 420.0f;
constexpr float kSinkingLureSpeed = 18.0f;
constexpr float kMaximumHookDistance = 300.0f;
constexpr uint8_t kSinkingLureType = 2;

float Length(const Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3 MoveTowards(const Vec3& from, const Vec3& to, float maximumDistance) {
    const Vec3 delta{ to.x - from.x, to.y - from.y, to.z - from.z };
    const float distance = Length(delta);
    if (distance <= maximumDistance || distance <= 0.00001f) return to;
    const float scale = maximumDistance / distance;
    return { from.x + delta.x * scale, from.y + delta.y * scale, from.z + delta.z * scale };
}

Vec3 BackOffImpact(const Vec3& from, const Vec3& impact) {
    const Vec3 delta{ impact.x - from.x, impact.y - from.y, impact.z - from.z };
    const float distance = Length(delta);
    if (distance <= 2.0f) return from;
    const float scale = (distance - 2.0f) / distance;
    return { from.x + delta.x * scale, from.y + delta.y * scale, from.z + delta.z * scale };
}

} // namespace

uint32_t MakeFishSpawnKey(int32_t sceneId, int32_t roomId, int32_t homeX,
                          int32_t homeY, int32_t homeZ) {
    uint32_t hash = 2166136261U;
    const auto combine = [&hash](int32_t value) {
        hash ^= static_cast<uint32_t>(value);
        hash *= 16777619U;
    };
    combine(sceneId);
    combine(roomId);
    combine(homeX);
    combine(homeY);
    combine(homeZ);
    return hash == 0 ? 1U : hash;
}

size_t FishingSimulation::FishIdentityHash::operator()(const FishIdentity& identity) const noexcept {
    size_t hash = 1469598103934665603ULL;
    const auto combine = [&hash](int32_t value) {
        hash ^= static_cast<uint32_t>(value);
        hash *= 1099511628211ULL;
    };
    combine(identity.sceneId);
    combine(static_cast<int32_t>(identity.spawnKey));
    return hash;
}

void FishingSimulation::SetCollisionQuery(SegmentCast segmentCast) {
    mSegmentCast = std::move(segmentCast);
}

void FishingSimulation::SetWaterSurfaceQuery(WaterSurfaceQuery waterSurfaceQuery) {
    mWaterSurfaceQuery = std::move(waterSurfaceQuery);
}

bool FishingSimulation::RegisterFish(const FishDefinition& definition) {
    const bool finiteBounds = std::isfinite(definition.minX) && std::isfinite(definition.maxX) &&
                              std::isfinite(definition.minY) && std::isfinite(definition.maxY) &&
                              std::isfinite(definition.minZ) && std::isfinite(definition.maxZ);
    if (definition.identity.sceneId < 0 || definition.identity.spawnKey == 0 ||
        !std::isfinite(definition.spawnPosition.x) ||
        !std::isfinite(definition.spawnPosition.y) ||
        !std::isfinite(definition.spawnPosition.z) ||
        definition.species > FishSpecies::HylianLoach ||
        !std::isfinite(definition.length) || definition.length <= 0.0f ||
        (definition.bounded && (!finiteBounds || definition.minX > definition.maxX ||
                                definition.minY > definition.maxY || definition.minZ > definition.maxZ)) ||
        IsFishRegistered(definition.identity)) {
        return false;
    }
    return mCatalog.emplace(definition.identity, definition).second;
}

bool FishingSimulation::IsFishRegistered(const FishIdentity& identity) const {
    return mCatalog.find(identity) != mCatalog.end();
}

size_t FishingSimulation::RegisteredFishCount() const {
    return mCatalog.size();
}

bool FishingSimulation::ApplyLureControl(int32_t playerId, int32_t sceneId, bool deployed,
                                         bool reelHeld, uint8_t lureType, const PlayerSnapshot& owner) {
    if (playerId < 0 || sceneId < 0 || owner.ownerPlayerId != playerId || owner.sceneId != sceneId ||
        owner.health == 0) {
        return false;
    }
    LureEntity* lure = FindLure(playerId);
    if (!deployed) return lure == nullptr || RemoveLure(playerId);
    if (lure != nullptr && lure->sceneId != sceneId) {
        RemoveLure(playerId);
        lure = nullptr;
    }
    if (lure != nullptr && lure->ownerLifeEpoch != owner.lifeEpoch) {
        RemoveLure(playerId);
        lure = nullptr;
    }
    if (lure == nullptr) {
        LureEntity created{};
        created.ownerPlayerId = playerId;
        created.ownerLifeEpoch = owner.lifeEpoch;
        created.sceneId = sceneId;
        created.position = { owner.position.x, owner.position.y + 42.0f, owner.position.z };
        created.velocity = { std::sin(owner.headingRadians) * kCastHorizontalSpeed, kCastVerticalSpeed,
                             std::cos(owner.headingRadians) * kCastHorizontalSpeed };
        created.lureType = lureType;
        created.reelHeld = reelHeld;
        created.lastSnapshotTick = mCurrentTick;
        const EntityId id = mLures.Create(std::move(created));
        lure = mLures.Get(id);
        lure->id = id;
        mLureByOwner[playerId] = id;
        QueueLureEvent(FishingLureEventKind::Created, *lure);
        return true;
    }
    lure->reelHeld = reelHeld;
    lure->lureType = lureType;
    return true;
}

void FishingSimulation::StepFixed(PlayerSimulation& players) {
    SimulateTick(players);
}

void FishingSimulation::SimulateTick(PlayerSimulation& players) {
    ++mCurrentTick;
    std::vector<EntityId> remove;
    mLures.ForEach([&](LureEntity& lure) {
        const auto owner = players.SnapshotForPlayer(lure.ownerPlayerId);
        if (!owner || owner->health == 0 || owner->lifeEpoch != lure.ownerLifeEpoch ||
            owner->sceneId != lure.sceneId) {
            QueueLureEvent(FishingLureEventKind::Removed, lure);
            remove.push_back(lure.id);
            return;
        }
        SimulateLure(lure, *owner, remove);
    });
    for (EntityId id : remove) DestroyLure(id);
}

void FishingSimulation::SimulateLure(LureEntity& lure, const PlayerSnapshot& owner,
                                     std::vector<EntityId>& remove) {
    if (const auto fish = FishOwnedBy(lure.ownerPlayerId)) {
        lure.phase = FishingLurePhase::Hooked;
        lure.position = fish->position;
    } else if (lure.phase == FishingLurePhase::Hooked) {
        lure.phase = FishingLurePhase::Settled;
    }

    const Vec3 previous = lure.position;
    if (lure.phase == FishingLurePhase::Flying) {
        lure.velocity.y -= kGravity * kTickSeconds;
        lure.position.x += lure.velocity.x * kTickSeconds;
        lure.position.y += lure.velocity.y * kTickSeconds;
        lure.position.z += lure.velocity.z * kTickSeconds;
        Vec3 impact{};
        if (mSegmentCast && mSegmentCast(lure.sceneId, previous, lure.position, impact)) {
            lure.position = BackOffImpact(previous, impact);
            lure.velocity = {};
            lure.phase = FishingLurePhase::Settled;
        } else if (mWaterSurfaceQuery) {
            float surfaceY = 0.0f;
            if (mWaterSurfaceQuery(lure.sceneId, lure.position, surfaceY) && previous.y >= surfaceY &&
                lure.position.y <= surfaceY) {
                lure.position.y = surfaceY - 2.0f;
                lure.velocity = {};
                lure.phase = FishingLurePhase::Settled;
            }
        }
    } else if (lure.phase == FishingLurePhase::Settled) {
        const Vec3 forward{ std::sin(owner.headingRadians), 0.0f, std::cos(owner.headingRadians) };
        const Vec3 rodTarget{ owner.position.x + forward.x * 12.0f, owner.position.y + 42.0f,
                              owner.position.z + forward.z * 12.0f };
        if (lure.reelHeld) {
            const Vec3 requested = MoveTowards(lure.position, rodTarget, kReelSpeed * kTickSeconds);
            Vec3 impact{};
            lure.position = mSegmentCast && mSegmentCast(lure.sceneId, lure.position, requested, impact)
                                ? BackOffImpact(lure.position, impact)
                                : requested;
        } else if (mWaterSurfaceQuery) {
            float surfaceY = 0.0f;
            if (mWaterSurfaceQuery(lure.sceneId, lure.position, surfaceY)) {
                if (lure.lureType == kSinkingLureType) {
                    const Vec3 requested{ lure.position.x,
                                          std::max(lure.position.y - kSinkingLureSpeed * kTickSeconds,
                                                   surfaceY - 250.0f),
                                          lure.position.z };
                    Vec3 impact{};
                    lure.position = mSegmentCast && mSegmentCast(lure.sceneId, lure.position, requested, impact)
                                        ? BackOffImpact(lure.position, impact)
                                        : requested;
                } else {
                    lure.position.y = surfaceY - 2.0f;
                }
            }
        }
    }

    if (std::fabs(lure.position.x) >= 32000.0f || std::fabs(lure.position.y) >= 32000.0f ||
        std::fabs(lure.position.z) >= 32000.0f) {
        QueueLureEvent(FishingLureEventKind::Removed, lure);
        remove.push_back(lure.id);
        return;
    }
    const Vec3 motion{ lure.position.x - previous.x, lure.position.y - previous.y,
                       lure.position.z - previous.z };
    if (Length(motion) > 0.001f && mCurrentTick - lure.lastSnapshotTick >= kBroadcastIntervalTicks) {
        lure.lastSnapshotTick = mCurrentTick;
        QueueLureEvent(FishingLureEventKind::Snapshot, lure);
    }
}

std::optional<FishingLureSnapshot> FishingSimulation::LureForPlayer(int32_t playerId) const {
    const LureEntity* lure = FindLure(playerId);
    return lure ? std::optional<FishingLureSnapshot>(BuildLureSnapshot(*lure)) : std::nullopt;
}

std::vector<FishingLureSnapshot> FishingSimulation::LureSnapshots() const {
    std::vector<FishingLureSnapshot> snapshots;
    snapshots.reserve(mLures.Size());
    mLures.ForEach([&](const LureEntity& lure) {
        snapshots.push_back(BuildLureSnapshot(lure));
    });
    return snapshots;
}

bool FishingSimulation::RemoveLure(int32_t playerId) {
    LureEntity* lure = FindLure(playerId);
    return lure != nullptr && DestroyLure(lure->id);
}

std::vector<FishingLureEvent> FishingSimulation::DrainLureEvents() {
    std::vector<FishingLureEvent> events;
    events.swap(mLureEvents);
    return events;
}

bool FishingSimulation::HookNearestRegistered(int32_t playerId) {
    const auto lure = LureForPlayer(playerId);
    // Hooking is an intent, not a client-reported collision. The server only
    // permits a bite once its own lure has landed and only for fish close
    // enough to that authoritative lure position.
    if (!lure || lure->phase != FishingLurePhase::Settled ||
        FishOwnedBy(playerId)) {
        return false;
    }

    const FishDefinition* nearest = nullptr;
    float nearestDistanceSquared = 0.0f;
    std::tuple<int32_t, uint32_t> nearestKey{};
    for (const auto& entry : mCatalog) {
        const FishDefinition& definition = entry.second;
        if (definition.identity.sceneId != lure->sceneId) continue;
        const FishEntity* hooked = FindFish(definition.identity);
        if (hooked) continue;
        if (definition.bounded) {
            constexpr float margin = 20.0f;
            const bool lureInside = lure->position.x >= definition.minX - margin &&
                                    lure->position.x <= definition.maxX + margin &&
                                    lure->position.y >= definition.minY &&
                                    lure->position.y <= definition.maxY &&
                                    lure->position.z >= definition.minZ - margin &&
                                    lure->position.z <= definition.maxZ + margin;
            if (!lureInside) continue;
        }
        const float deltaX = lure->position.x - definition.spawnPosition.x;
        const float deltaY = lure->position.y - definition.spawnPosition.y;
        const float deltaZ = lure->position.z - definition.spawnPosition.z;
        const float distanceSquared = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
        if (distanceSquared > kMaximumHookDistance * kMaximumHookDistance) continue;
        const auto key = std::tie(definition.identity.sceneId,
                                  definition.identity.spawnKey);
        if (!nearest || distanceSquared < nearestDistanceSquared ||
            (distanceSquared == nearestDistanceSquared && key < nearestKey)) {
            nearest = &definition;
            nearestDistanceSquared = distanceSquared;
            nearestKey = key;
        }
    }
    if (!nearest) return false;
    return Hook(*nearest, playerId, lure->position);
}

bool FishingSimulation::Hook(const FishDefinition& definition, int32_t playerId, const Vec3& position) {
    const FishIdentity& identity = definition.identity;
    LureEntity* lure = FindLure(playerId);
    if (playerId < 0 || lure == nullptr) return false;
    if (const FishEntity* owned = FindFishOwnedBy(playerId);
        owned != nullptr && !(owned->identity == identity)) {
        return false;
    }
    FishEntity* fish = FindFish(identity);
    if (fish != nullptr) {
        if (fish->ownerPlayerId != -1 && fish->ownerPlayerId != playerId) return false;
        fish->ownerPlayerId = playerId;
        fish->ownerLifeEpoch = lure->ownerLifeEpoch;
        mFishByOwner[playerId] = fish->id;
        fish->position = position;
        fish->species = definition.species;
        fish->length = definition.length;
    } else {
        FishEntity created{};
        created.identity = identity;
        created.ownerPlayerId = playerId;
        created.ownerLifeEpoch = lure->ownerLifeEpoch;
        created.position = position;
        created.species = definition.species;
        created.length = definition.length;
        const EntityId id = mFish.Create(std::move(created));
        FishEntity* added = mFish.Get(id);
        added->id = id;
        mFishByIdentity[identity] = id;
        mFishByOwner[playerId] = id;
    }
    lure->phase = FishingLurePhase::Hooked;
    lure->position = position;
    lure->velocity = {};
    QueueLureEvent(FishingLureEventKind::Snapshot, *lure);
    return true;
}

bool FishingSimulation::Release(const FishIdentity& identity, int32_t playerId) {
    FishEntity* fish = FindFish(identity);
    if (fish == nullptr || fish->ownerPlayerId != playerId) return false;
    if (LureEntity* lure = FindLure(playerId)) {
        lure->phase = FishingLurePhase::Settled;
    }
    return DestroyFish(fish->id);
}

std::optional<FishSnapshot> FishingSimulation::FishOwnedBy(int32_t playerId) const {
    const FishEntity* fish = FindFishOwnedBy(playerId);
    return fish ? std::optional<FishSnapshot>(BuildSnapshot(*fish)) : std::nullopt;
}

std::optional<int32_t> FishingSimulation::OwnerOf(const FishIdentity& identity) const {
    const FishEntity* fish = FindFish(identity);
    return fish ? std::optional<int32_t>(fish->ownerPlayerId) : std::nullopt;
}

std::vector<FishSnapshot> FishingSimulation::ReleaseOwnedBy(int32_t playerId) {
    std::vector<FishSnapshot> released;
    std::vector<EntityId> remove;
    mFish.ForEach([&](const FishEntity& fish) {
        if (fish.ownerPlayerId == playerId) {
            released.push_back(BuildSnapshot(fish));
            remove.push_back(fish.id);
        }
    });
    for (EntityId id : remove) DestroyFish(id);
    if (!released.empty()) {
        if (LureEntity* lure = FindLure(playerId)) {
            lure->phase = FishingLurePhase::Settled;
        }
    }
    return released;
}

void FishingSimulation::RemoveIneligibleOwners(
    const std::vector<PlayerSnapshot>& players) {
    std::unordered_map<int32_t, uint32_t> eligibleOwners;
    eligibleOwners.reserve(players.size());
    for (const PlayerSnapshot& player : players) {
        if (CanPerformFishingAction(player) && player.selectedWeapon == 4) {
            eligibleOwners.insert_or_assign(player.ownerPlayerId, player.lifeEpoch);
        }
    }

    std::vector<EntityId> removeFish;
    mFish.ForEach([&](const FishEntity& fish) {
        const auto owner = eligibleOwners.find(fish.ownerPlayerId);
        if (owner != eligibleOwners.end() && owner->second == fish.ownerLifeEpoch) return;
        removeFish.push_back(fish.id);
    });
    for (const EntityId id : removeFish) DestroyFish(id);

    std::vector<EntityId> removeLures;
    mLures.ForEach([&](const LureEntity& lure) {
        const auto owner = eligibleOwners.find(lure.ownerPlayerId);
        if (owner != eligibleOwners.end() && owner->second == lure.ownerLifeEpoch) return;
        QueueLureEvent(FishingLureEventKind::Removed, lure);
        removeLures.push_back(lure.id);
    });
    for (const EntityId id : removeLures) DestroyLure(id);
}

std::vector<FishSnapshot> FishingSimulation::Snapshots() const {
    std::vector<FishSnapshot> snapshots;
    snapshots.reserve(mFish.Size());
    mFish.ForEach([&](const FishEntity& fish) { snapshots.push_back(BuildSnapshot(fish)); });
    return snapshots;
}

void FishingSimulation::Reset() {
    mFish.Clear();
    mLures.Clear();
    mFishByIdentity.clear();
    mFishByOwner.clear();
    mLureByOwner.clear();
    mCatalog.clear();
    mLureEvents.clear();
    mCurrentTick = 0;
}

void FishingSimulation::QueueLureEvent(FishingLureEventKind kind, const LureEntity& lure) {
    mLureEvents.push_back({ kind, BuildLureSnapshot(lure) });
}

FishSnapshot FishingSimulation::BuildSnapshot(const FishEntity& fish) const {
    return { fish.id, fish.identity, fish.ownerPlayerId, fish.ownerLifeEpoch,
             fish.position, fish.species, fish.length };
}

FishingLureSnapshot FishingSimulation::BuildLureSnapshot(const LureEntity& lure) const {
    return { lure.id, lure.ownerPlayerId, lure.ownerLifeEpoch, lure.sceneId,
             lure.position, lure.phase, lure.lureType };
}

FishingSimulation::FishEntity* FishingSimulation::FindFish(
    const FishIdentity& identity) {
    const auto found = mFishByIdentity.find(identity);
    return found == mFishByIdentity.end() ? nullptr : mFish.Get(found->second);
}

const FishingSimulation::FishEntity* FishingSimulation::FindFish(
    const FishIdentity& identity) const {
    const auto found = mFishByIdentity.find(identity);
    return found == mFishByIdentity.end() ? nullptr : mFish.Get(found->second);
}

FishingSimulation::FishEntity* FishingSimulation::FindFishOwnedBy(
    int32_t playerId) {
    const auto found = mFishByOwner.find(playerId);
    return found == mFishByOwner.end() ? nullptr : mFish.Get(found->second);
}

const FishingSimulation::FishEntity* FishingSimulation::FindFishOwnedBy(
    int32_t playerId) const {
    const auto found = mFishByOwner.find(playerId);
    return found == mFishByOwner.end() ? nullptr : mFish.Get(found->second);
}

FishingSimulation::LureEntity* FishingSimulation::FindLure(int32_t playerId) {
    const auto found = mLureByOwner.find(playerId);
    return found == mLureByOwner.end() ? nullptr : mLures.Get(found->second);
}

const FishingSimulation::LureEntity* FishingSimulation::FindLure(
    int32_t playerId) const {
    const auto found = mLureByOwner.find(playerId);
    return found == mLureByOwner.end() ? nullptr : mLures.Get(found->second);
}

bool FishingSimulation::DestroyFish(EntityId id) {
    FishEntity* fish = mFish.Get(id);
    if (fish == nullptr) return false;
    mFishByIdentity.erase(fish->identity);
    if (fish->ownerPlayerId >= 0) mFishByOwner.erase(fish->ownerPlayerId);
    return mFish.Destroy(id);
}

bool FishingSimulation::DestroyLure(EntityId id) {
    LureEntity* lure = mLures.Get(id);
    if (lure == nullptr) return false;
    mLureByOwner.erase(lure->ownerPlayerId);
    return mLures.Destroy(id);
}

} // namespace Game::Simulation
