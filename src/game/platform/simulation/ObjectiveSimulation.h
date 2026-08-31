#pragma once

#include "EntityRegistry.h"
#include "PlayerSimulation.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Game::Simulation {

struct ObjectiveDefinition {
    int32_t objectiveKey = -1;
    int32_t sceneId = -1;
    Vec3 position{};
    float captureRadius = 300.0f;
    TeamId initialOwner = TeamId::Neutral;
};

struct ObjectiveSnapshot {
    EntityId entity{};
    int32_t objectiveKey = -1;
    int32_t sceneId = -1;
    Vec3 position{};
    float captureRadius = 0.0f;
    TeamId owner = TeamId::Neutral;
    TeamId capturingTeam = TeamId::Neutral;
    float captureProgress = 0.0f;
    bool contested = false;
};

struct ObjectiveCapturedEvent {
    EntityId entity{};
    int32_t objectiveKey = -1;
    TeamId previousOwner = TeamId::Neutral;
    TeamId newOwner = TeamId::Neutral;
};

class ObjectiveSimulation final {
  public:
    EntityId EnsureObjective(const ObjectiveDefinition& definition);
    bool RemoveObjective(int32_t objectiveKey);
    void Reset();

    void Update(const PlayerSimulation& players, float deltaSeconds);
    std::optional<ObjectiveSnapshot> SnapshotForObjective(int32_t objectiveKey) const;
    std::vector<ObjectiveSnapshot> Snapshots() const;
    void Restore(const std::vector<ObjectiveSnapshot>& snapshots);
    std::vector<ObjectiveCapturedEvent> DrainCapturedEvents();

  private:
    struct ObjectiveEntity {
        EntityId id{};
        ObjectiveDefinition definition{};
        TeamId owner = TeamId::Neutral;
        TeamId capturingTeam = TeamId::Neutral;
        float captureProgress = 0.0f;
        bool contested = false;
    };

    ObjectiveEntity* FindObjective(int32_t objectiveKey);
    const ObjectiveEntity* FindObjective(int32_t objectiveKey) const;
    static ObjectiveSnapshot BuildSnapshot(const ObjectiveEntity& objective);

    static constexpr float kCaptureUnitsPerSecond = 20.0f;

    EntityRegistry<ObjectiveEntity> mObjectives;
    std::unordered_map<int32_t, EntityId> mObjectiveByKey;
    std::vector<ObjectiveCapturedEvent> mCapturedEvents;
};

} // namespace Game::Simulation
