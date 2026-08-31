#pragma once

#include "EntityRegistry.h"
#include "PlayerSimulation.h"
#include "SpatialGridIndex.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Game::Simulation {

enum class StructurePhase : uint8_t {
    Planned,
    Building,
    Active,
    Destroyed,
};

enum class StructureEventKind : uint8_t {
    BuildStarted,
    Built,
    Damaged,
    Repaired,
    Destroyed,
    Reset,
};

struct StructureDefinition {
    int32_t structureKey = -1;
    int32_t objectiveKey = -1;
    int32_t sceneId = -1;
    Vec3 position{};
    uint32_t maximumHealth = 1000;
    uint32_t requiredBuild = 100;
};

struct StructureSnapshot {
    EntityId entity{};
    int32_t structureKey = -1;
    int32_t objectiveKey = -1;
    int32_t sceneId = -1;
    Vec3 position{};
    TeamId team = TeamId::Neutral;
    StructurePhase phase = StructurePhase::Planned;
    uint32_t health = 0;
    uint32_t maximumHealth = 0;
    uint32_t buildProgress = 0;
    uint32_t requiredBuild = 0;
};

struct StructureEvent {
    EntityId entity{};
    int32_t structureKey = -1;
    StructureEventKind kind = StructureEventKind::BuildStarted;
    TeamId sourceTeam = TeamId::Neutral;
    uint32_t amount = 0;
};

struct StructureHit {
    EntityId entity{};
    int32_t structureKey = -1;
    float segmentRatio = 0.0f;
    Vec3 position{};
};

class StructureSimulation final {
  public:
    EntityId EnsureStructure(const StructureDefinition& definition);
    bool RemoveStructure(int32_t structureKey);
    void Reset();

    bool ContributeBuild(int32_t structureKey, TeamId sourceTeam, TeamId objectiveOwner, uint32_t amount);
    bool ApplyDamage(int32_t structureKey, TeamId sourceTeam, uint32_t amount);
    bool Repair(int32_t structureKey, TeamId sourceTeam, uint32_t amount);
    bool ResetStructure(int32_t structureKey);
    bool FirstSegmentHit(int32_t sceneId, const Vec3& start, const Vec3& end,
                         StructureHit& hit) const;

    static constexpr float CollisionRadius() { return 60.0f; }
    static constexpr float CollisionHeight() { return 180.0f; }

    std::optional<StructureSnapshot> SnapshotForStructure(int32_t structureKey) const;
    std::vector<StructureSnapshot> Snapshots() const;
    void Restore(const std::vector<StructureSnapshot>& snapshots);
    std::vector<StructureEvent> DrainEvents();

  private:
    struct StructureEntity {
        EntityId id{};
        StructureDefinition definition{};
        TeamId team = TeamId::Neutral;
        StructurePhase phase = StructurePhase::Planned;
        uint32_t health = 0;
        uint32_t buildProgress = 0;
    };

    StructureEntity* FindStructure(int32_t structureKey);
    const StructureEntity* FindStructure(int32_t structureKey) const;
    static StructureSnapshot BuildSnapshot(const StructureEntity& structure);

    EntityRegistry<StructureEntity> mStructures;
    std::unordered_map<int32_t, EntityId> mStructureByKey;
    SpatialGridIndex mStructureSpatialIndex;
    std::vector<StructureEvent> mEvents;
};

} // namespace Game::Simulation
