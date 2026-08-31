#pragma once

#include "EntityRegistry.h"
#include "PlayerSimulation.h"
#include "StructureSimulation.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Game::Simulation {

enum class ArrowPhase : uint8_t {
    Flying,
    Stuck,
    Blocked,
};

enum class ArrowEventKind : uint8_t {
    Created,
    Snapshot,
    Stuck,
    Blocked,
    HitPlayer,
    HitStructure,
    Removed,
};

struct ArrowSpawn {
    int32_t ownerPlayerId = -1;
    int32_t sceneId = -1;
    uint8_t projectileType = 0;
    Vec3 position{};
    Vec3 velocity{};
    int16_t rotationY = 0;
};

struct ArrowSnapshot {
    EntityId entity{};
    int32_t replicationId = 0;
    int32_t ownerPlayerId = -1;
    int32_t sceneId = -1;
    uint32_t sequence = 0;
    bool active = false;
    ArrowPhase phase = ArrowPhase::Flying;
    uint8_t projectileType = 0;
    Vec3 position{};
    Vec3 velocity{};
    int16_t rotationX = 0;
    int16_t rotationY = 0;
    int16_t rotationZ = 0;
};

struct ArrowEvent {
    ArrowEventKind kind = ArrowEventKind::Snapshot;
    ArrowSnapshot arrow{};
    int32_t hitPlayerId = -1;
    int32_t hitStructureKey = -1;
};

class ProjectileSimulation final {
  public:
    void SetCollisionQuery(SegmentCast segmentCast);
    std::optional<ArrowSnapshot> SpawnArrow(const ArrowSpawn& spawn);
    bool HasArrow(int32_t ownerPlayerId, int32_t replicationId) const;
    bool RemoveArrow(int32_t ownerPlayerId, int32_t replicationId);
    void RemoveOwnedBy(int32_t ownerPlayerId);
    void Reset();
    void StepFixed(PlayerSimulation& players);
    void StepFixed(PlayerSimulation& players, StructureSimulation& structures);
    std::vector<ArrowSnapshot> Snapshots() const;
    std::vector<ArrowEvent> DrainEvents();
    uint32_t CurrentTick() const;

  private:
    struct ArrowEntity {
        EntityId id{};
        int32_t replicationId = 0;
        int32_t ownerPlayerId = -1;
        int32_t sceneId = -1;
        uint32_t sequence = 1;
        bool active = true;
        ArrowPhase phase = ArrowPhase::Flying;
        uint8_t projectileType = 0;
        Vec3 position{};
        Vec3 velocity{};
        int16_t rotationX = 0;
        int16_t rotationY = 0;
        int16_t rotationZ = 0;
        uint32_t spawnTick = 0;
        uint32_t impactTick = 0;
        uint32_t lastSnapshotTick = 0;
        int32_t attachedStructureKey = -1;
    };

    void SimulateTick(PlayerSimulation& players, StructureSimulation* structures);
    void SimulateArrow(ArrowEntity& arrow, PlayerSimulation& players,
                       StructureSimulation* structures, std::vector<EntityId>& remove);
    void StickArrow(ArrowEntity& arrow, const Vec3& position,
                    ArrowEventKind eventKind = ArrowEventKind::Stuck,
                    int32_t hitStructureKey = -1);
    void RetainStuckArrows(const ArrowEntity& current, std::vector<EntityId>& remove);
    void QueueEvent(ArrowEventKind kind, const ArrowEntity& arrow, int32_t hitPlayerId = -1,
                    int32_t hitStructureKey = -1);
    ArrowEntity* FindArrow(int32_t ownerPlayerId, int32_t replicationId);
    const ArrowEntity* FindArrow(int32_t ownerPlayerId,
                                 int32_t replicationId) const;
    bool DestroyArrow(EntityId id);
    ArrowSnapshot BuildSnapshot(const ArrowEntity& arrow) const;
    int32_t TakeReplicationId();

    static constexpr float kTickSeconds = 1.0f / 60.0f;
    static constexpr uint32_t kBroadcastIntervalTicks = 3;
    static constexpr uint32_t kGravityDelayTicks = 15;

    EntityRegistry<ArrowEntity> mArrows;
    std::unordered_map<int32_t, EntityId> mArrowByReplicationId;
    std::unordered_map<int32_t, std::vector<EntityId>> mArrowsByOwner;
    SegmentCast mSegmentCast;
    std::vector<ArrowEvent> mEvents;
    uint32_t mCurrentTick = 0;
    int32_t mNextReplicationId = 1;
};

} // namespace Game::Simulation
