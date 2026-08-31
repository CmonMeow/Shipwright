#pragma once

#include "../simulation/ObjectiveSimulation.h"
#include "../simulation/StrategicWorldTopology.h"
#include "../simulation/StructureSimulation.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Game::Client {

struct ReplicatedObjectiveState {
    Simulation::ObjectiveSnapshot snapshot{};
    bool active = false;
};

struct ReplicatedStructureState {
    Simulation::StructureSnapshot snapshot{};
    bool active = false;
};

struct ReplicatedStrategicTopologyState {
    uint32_t revision = 0;
    std::vector<Simulation::StrategicSiteDefinition> sites;
    std::vector<Simulation::SupplyRouteDefinition> supplyRoutes;
    std::vector<Simulation::InfluenceRegionAdjacencyDefinition>
        influenceAdjacencies;
};

enum class ClientWorldStateUpdate : uint8_t {
    Ignored,
    Established,
    Updated,
    Replaced,
    Retired,
};

struct ClientWorldStateApplyResult {
    ClientWorldStateUpdate update = ClientWorldStateUpdate::Ignored;
    Simulation::EntityId entity{};
    std::optional<Simulation::EntityId> previousEntity;

    bool Applied() const { return update != ClientWorldStateUpdate::Ignored; }
};

// Semantic client mirror for server-owned WvW entities.
// Reliable interest enter/leave controls exact lifetimes; snapshots only
// update the matching generation and cannot resurrect a retired entity.
class ClientWorldState final {
  public:
    ClientWorldStateApplyResult ApplyObjective(
        const Simulation::ObjectiveSnapshot& state, bool active);
    ClientWorldStateApplyResult ApplyStructure(
        const Simulation::StructureSnapshot& state, bool active);
    bool ApplyStrategicTopology(const ReplicatedStrategicTopologyState& state);
    const Simulation::ObjectiveSnapshot* FindObjective(int32_t objectiveKey) const;
    const Simulation::StructureSnapshot* FindStructure(int32_t structureKey) const;
    size_t ObjectiveCount() const { return mObjectives.size(); }
    size_t StructureCount() const { return mStructures.size(); }
    uint32_t StrategicTopologyRevision() const { return mStrategicTopologyRevision; }
    const Simulation::StrategicWorldTopology& StrategicTopology() const {
        return mStrategicTopology;
    }
    void Reset();

  private:
    static uint64_t EntityKey(Simulation::EntityId entity);

    std::map<uint64_t, Simulation::ObjectiveSnapshot> mObjectives;
    std::map<int32_t, uint64_t> mObjectiveEntities;
    std::map<uint64_t, Simulation::StructureSnapshot> mStructures;
    std::map<int32_t, uint64_t> mStructureEntities;
    Simulation::StrategicWorldTopology mStrategicTopology;
    uint32_t mStrategicTopologyRevision = 0;
};

} // namespace Game::Client
