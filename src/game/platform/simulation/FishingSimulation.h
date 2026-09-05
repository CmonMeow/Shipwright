#pragma once

#include "EntityRegistry.h"
#include "PlayerSimulation.h"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Game::Simulation {

struct FishIdentity {
    int32_t sceneId = -1;
    uint32_t spawnKey = 0;

    bool operator==(const FishIdentity&) const = default;
};

enum class FishSpecies : uint8_t {
    HylianBass,
    HylianLoach,
};

uint32_t MakeFishSpawnKey(int32_t sceneId, int32_t roomId, int32_t homeX,
                          int32_t homeY, int32_t homeZ);

struct FishSnapshot {
    EntityId entity{};
    FishIdentity identity{};
    int32_t ownerPlayerId = -1;
    uint32_t ownerLifeEpoch = 0;
    Vec3 position{};
    FishSpecies species = FishSpecies::HylianBass;
    float length = 0.0f;
};

struct FishDefinition {
    FishIdentity identity{};
    Vec3 spawnPosition{};
    FishSpecies species = FishSpecies::HylianBass;
    float length = 0.0f;
    bool bounded = false;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
};

enum class FishingLurePhase : uint8_t { Flying, Settled, Hooked };

struct FishingLureSnapshot {
    EntityId entity{};
    int32_t ownerPlayerId = -1;
    uint32_t ownerLifeEpoch = 0;
    int32_t sceneId = -1;
    Vec3 position{};
    FishingLurePhase phase = FishingLurePhase::Flying;
    uint8_t lureType = 0;
};

enum class FishingLureEventKind : uint8_t { Created, Snapshot, Removed };

struct FishingLureEvent {
    FishingLureEventKind kind = FishingLureEventKind::Snapshot;
    FishingLureSnapshot lure{};
};

class FishingSimulation final {
  public:
    using WaterSurfaceQuery = std::function<bool(int32_t sceneId, const Vec3& position, float& surfaceY)>;

    void SetCollisionQuery(SegmentCast segmentCast);
    void SetWaterSurfaceQuery(WaterSurfaceQuery waterSurfaceQuery);
    bool RegisterFish(const FishDefinition& definition);
    bool IsFishRegistered(const FishIdentity& identity) const;
    size_t RegisteredFishCount() const;
    bool ApplyLureControl(int32_t playerId, int32_t sceneId, bool deployed, bool reelHeld,
                          uint8_t lureType, const PlayerSnapshot& owner);
    void StepFixed(PlayerSimulation& players);
    std::optional<FishingLureSnapshot> LureForPlayer(int32_t playerId) const;
    std::vector<FishingLureSnapshot> LureSnapshots() const;
    bool RemoveLure(int32_t playerId);
    std::vector<FishingLureEvent> DrainLureEvents();
    bool HookNearestRegistered(int32_t playerId);
    bool Release(const FishIdentity& identity, int32_t playerId);
    std::optional<FishSnapshot> FishOwnedBy(int32_t playerId) const;
    std::optional<int32_t> OwnerOf(const FishIdentity& identity) const;
    std::vector<FishSnapshot> ReleaseOwnedBy(int32_t playerId);
    void RemoveIneligibleOwners(const std::vector<PlayerSnapshot>& players);
    std::vector<FishSnapshot> Snapshots() const;
    void Reset();

  private:
    struct FishIdentityHash {
        size_t operator()(const FishIdentity& identity) const noexcept;
    };

    struct FishEntity {
        EntityId id{};
        FishIdentity identity{};
        int32_t ownerPlayerId = -1;
        uint32_t ownerLifeEpoch = 0;
        Vec3 position{};
        FishSpecies species = FishSpecies::HylianBass;
        float length = 0.0f;
    };

    struct LureEntity {
        EntityId id{};
        int32_t ownerPlayerId = -1;
        uint32_t ownerLifeEpoch = 0;
        int32_t sceneId = -1;
        Vec3 position{};
        Vec3 velocity{};
        FishingLurePhase phase = FishingLurePhase::Flying;
        uint8_t lureType = 0;
        bool reelHeld = false;
        uint32_t lastSnapshotTick = 0;
    };

    void SimulateTick(PlayerSimulation& players);
    void SimulateLure(LureEntity& lure, const PlayerSnapshot& owner, std::vector<EntityId>& remove);
    void QueueLureEvent(FishingLureEventKind kind, const LureEntity& lure);
    bool Hook(const FishDefinition& definition, int32_t playerId, const Vec3& position);
    FishSnapshot BuildSnapshot(const FishEntity& fish) const;
    FishingLureSnapshot BuildLureSnapshot(const LureEntity& lure) const;
    FishEntity* FindFish(const FishIdentity& identity);
    const FishEntity* FindFish(const FishIdentity& identity) const;
    FishEntity* FindFishOwnedBy(int32_t playerId);
    const FishEntity* FindFishOwnedBy(int32_t playerId) const;
    LureEntity* FindLure(int32_t playerId);
    const LureEntity* FindLure(int32_t playerId) const;
    bool DestroyFish(EntityId id);
    bool DestroyLure(EntityId id);

    static constexpr float kTickSeconds = 1.0f / 60.0f;
    static constexpr uint32_t kBroadcastIntervalTicks = 3;

    EntityRegistry<FishEntity> mFish;
    EntityRegistry<LureEntity> mLures;
    std::unordered_map<FishIdentity, EntityId, FishIdentityHash> mFishByIdentity;
    std::map<int32_t, EntityId> mFishByOwner;
    std::map<int32_t, EntityId> mLureByOwner;
    std::unordered_map<FishIdentity, FishDefinition, FishIdentityHash> mCatalog;
    SegmentCast mSegmentCast;
    WaterSurfaceQuery mWaterSurfaceQuery;
    std::vector<FishingLureEvent> mLureEvents;
    uint32_t mCurrentTick = 0;
};

} // namespace Game::Simulation
