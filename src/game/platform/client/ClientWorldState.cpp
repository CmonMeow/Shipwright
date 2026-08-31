#include "ClientWorldState.h"
#include "../SequenceNumber.h"

#include <utility>

namespace Game::Client {
namespace {

template <typename Snapshot>
bool MayReplace(const Snapshot& current, const Snapshot& candidate) {
    return current.entity.index != candidate.entity.index ||
           Sequence::IsNewer(candidate.entity.generation, current.entity.generation);
}

} // namespace

ClientWorldStateApplyResult ClientWorldState::ApplyObjective(
    const Simulation::ObjectiveSnapshot& state, bool active) {
    ClientWorldStateApplyResult result{};
    if (!state.entity.Valid() || state.objectiveKey < 0 || state.sceneId < 0) return result;
    result.entity = state.entity;
    const uint64_t key = EntityKey(state.entity);
    const auto logical = mObjectiveEntities.find(state.objectiveKey);
    if (!active) {
        if (logical == mObjectiveEntities.end() || logical->second != key) return result;
        mObjectives.erase(key);
        mObjectiveEntities.erase(logical);
        result.update = ClientWorldStateUpdate::Retired;
        return result;
    }
    const auto exact = mObjectives.find(key);
    if (exact != mObjectives.end()) {
        if (exact->second.objectiveKey != state.objectiveKey) return result;
        exact->second = state;
        result.update = ClientWorldStateUpdate::Updated;
        return result;
    }
    if (logical != mObjectiveEntities.end()) {
        const auto current = mObjectives.find(logical->second);
        if (current == mObjectives.end() || !MayReplace(current->second, state)) return result;
        result.previousEntity = current->second.entity;
        mObjectives.erase(current);
        mObjectiveEntities.erase(logical);
    }
    mObjectives[key] = state;
    mObjectiveEntities[state.objectiveKey] = key;
    result.update = result.previousEntity ? ClientWorldStateUpdate::Replaced
                                          : ClientWorldStateUpdate::Established;
    return result;
}

ClientWorldStateApplyResult ClientWorldState::ApplyStructure(
    const Simulation::StructureSnapshot& state, bool active) {
    ClientWorldStateApplyResult result{};
    if (!state.entity.Valid() || state.structureKey < 0 || state.objectiveKey < 0 ||
        state.sceneId < 0) return result;
    result.entity = state.entity;
    const uint64_t key = EntityKey(state.entity);
    const auto logical = mStructureEntities.find(state.structureKey);
    if (!active) {
        if (logical == mStructureEntities.end() || logical->second != key) return result;
        mStructures.erase(key);
        mStructureEntities.erase(logical);
        result.update = ClientWorldStateUpdate::Retired;
        return result;
    }
    const auto exact = mStructures.find(key);
    if (exact != mStructures.end()) {
        if (exact->second.structureKey != state.structureKey) return result;
        exact->second = state;
        result.update = ClientWorldStateUpdate::Updated;
        return result;
    }
    if (logical != mStructureEntities.end()) {
        const auto current = mStructures.find(logical->second);
        if (current == mStructures.end() || !MayReplace(current->second, state)) return result;
        result.previousEntity = current->second.entity;
        mStructures.erase(current);
        mStructureEntities.erase(logical);
    }
    mStructures[key] = state;
    mStructureEntities[state.structureKey] = key;
    result.update = result.previousEntity ? ClientWorldStateUpdate::Replaced
                                          : ClientWorldStateUpdate::Established;
    return result;
}

bool ClientWorldState::ApplyStrategicTopology(
    const ReplicatedStrategicTopologyState& state) {
    if (state.revision == 0 ||
        (mStrategicTopologyRevision != 0 &&
         !Sequence::IsNewer(state.revision, mStrategicTopologyRevision))) {
        return false;
    }
    Simulation::StrategicWorldTopology topology;
    if (!topology.Restore(state.sites, state.supplyRoutes,
                          state.influenceAdjacencies)) return false;
    mStrategicTopology = std::move(topology);
    mStrategicTopologyRevision = state.revision;
    return true;
}

const Simulation::ObjectiveSnapshot* ClientWorldState::FindObjective(
    int32_t objectiveKey) const {
    const auto entity = mObjectiveEntities.find(objectiveKey);
    if (entity == mObjectiveEntities.end()) return nullptr;
    const auto found = mObjectives.find(entity->second);
    return found == mObjectives.end() ? nullptr : &found->second;
}

const Simulation::StructureSnapshot* ClientWorldState::FindStructure(
    int32_t structureKey) const {
    const auto entity = mStructureEntities.find(structureKey);
    if (entity == mStructureEntities.end()) return nullptr;
    const auto found = mStructures.find(entity->second);
    return found == mStructures.end() ? nullptr : &found->second;
}

void ClientWorldState::Reset() {
    mObjectives.clear();
    mObjectiveEntities.clear();
    mStructures.clear();
    mStructureEntities.clear();
    mStrategicTopology.Reset();
    mStrategicTopologyRevision = 0;
}

uint64_t ClientWorldState::EntityKey(Simulation::EntityId entity) {
    return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
}

} // namespace Game::Client
