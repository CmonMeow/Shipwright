#include "ObjectiveSimulation.h"

#include <algorithm>
#include <cmath>

namespace Game::Simulation {

EntityId ObjectiveSimulation::EnsureObjective(const ObjectiveDefinition& definition) {
    if (definition.objectiveKey < 0 || definition.sceneId < 0 ||
        !std::isfinite(definition.position.x) ||
        !std::isfinite(definition.position.y) ||
        !std::isfinite(definition.position.z) ||
        !std::isfinite(definition.captureRadius) ||
        definition.captureRadius <= 0.0f) {
        return {};
    }
    if (ObjectiveEntity* existing = FindObjective(definition.objectiveKey)) {
        return existing->id;
    }
    ObjectiveEntity objective{};
    objective.definition = definition;
    objective.definition.captureRadius = std::max(1.0f, definition.captureRadius);
    objective.owner = definition.initialOwner;
    objective.id = mObjectives.Create(objective);
    ObjectiveEntity* created = mObjectives.Get(objective.id);
    if (!created) return {};
    created->id = objective.id;
    mObjectiveByKey.insert_or_assign(definition.objectiveKey, objective.id);
    return objective.id;
}

bool ObjectiveSimulation::RemoveObjective(int32_t objectiveKey) {
    const auto indexed = mObjectiveByKey.find(objectiveKey);
    if (indexed == mObjectiveByKey.end() || !mObjectives.Destroy(indexed->second)) {
        return false;
    }
    mObjectiveByKey.erase(indexed);
    return true;
}

void ObjectiveSimulation::Reset() {
    mObjectives.Clear();
    mObjectiveByKey.clear();
    mCapturedEvents.clear();
}

void ObjectiveSimulation::Update(const PlayerSimulation& players, float deltaSeconds) {
    const float elapsed = std::clamp(deltaSeconds, 0.0f, 0.25f);
    mObjectives.ForEach([&](ObjectiveEntity& objective) {
        uint32_t redPlayers = 0;
        uint32_t bluePlayers = 0;
        const float radiusSquared = objective.definition.captureRadius * objective.definition.captureRadius;
        for (const PlayerSnapshot& player : players.CandidateSnapshotsNear(
                 objective.definition.sceneId, objective.definition.position,
                 objective.definition.captureRadius)) {
            if (player.sceneId != objective.definition.sceneId || player.health == 0 || player.team == TeamId::Neutral) {
                continue;
            }
            const float dx = player.position.x - objective.definition.position.x;
            const float dy = player.position.y - objective.definition.position.y;
            const float dz = player.position.z - objective.definition.position.z;
            if (dx * dx + dy * dy + dz * dz > radiusSquared) {
                continue;
            }
            if (player.team == TeamId::Red) {
                ++redPlayers;
            } else if (player.team == TeamId::Blue) {
                ++bluePlayers;
            }
        }

        objective.contested = redPlayers != 0 && bluePlayers != 0;
        if (objective.contested || (redPlayers == 0 && bluePlayers == 0)) {
            return;
        }

        const TeamId activeTeam = redPlayers != 0 ? TeamId::Red : TeamId::Blue;
        const uint32_t activePlayers = redPlayers != 0 ? redPlayers : bluePlayers;
        if (objective.owner == activeTeam) {
            objective.capturingTeam = TeamId::Neutral;
            objective.captureProgress = 0.0f;
            return;
        }
        if (objective.capturingTeam != activeTeam) {
            objective.capturingTeam = activeTeam;
            objective.captureProgress = 0.0f;
        }
        objective.captureProgress += kCaptureUnitsPerSecond * static_cast<float>(activePlayers) * elapsed;
        if (objective.captureProgress < 100.0f) {
            return;
        }

        const TeamId previousOwner = objective.owner;
        objective.owner = activeTeam;
        objective.capturingTeam = TeamId::Neutral;
        objective.captureProgress = 0.0f;
        mCapturedEvents.push_back({ objective.id, objective.definition.objectiveKey, previousOwner, activeTeam });
    });
}

std::optional<ObjectiveSnapshot> ObjectiveSimulation::SnapshotForObjective(int32_t objectiveKey) const {
    const ObjectiveEntity* objective = FindObjective(objectiveKey);
    return objective ? std::optional<ObjectiveSnapshot>(BuildSnapshot(*objective)) : std::nullopt;
}

std::vector<ObjectiveSnapshot> ObjectiveSimulation::Snapshots() const {
    std::vector<ObjectiveSnapshot> snapshots;
    snapshots.reserve(mObjectives.Size());
    mObjectives.ForEach([&](const ObjectiveEntity& objective) { snapshots.push_back(BuildSnapshot(objective)); });
    return snapshots;
}

void ObjectiveSimulation::Restore(const std::vector<ObjectiveSnapshot>& snapshots) {
    Reset();
    for (const ObjectiveSnapshot& snapshot : snapshots) {
        const EntityId id = EnsureObjective({ snapshot.objectiveKey, snapshot.sceneId, snapshot.position,
                                              snapshot.captureRadius, snapshot.owner });
        ObjectiveEntity* objective = mObjectives.Get(id);
        objective->capturingTeam = snapshot.capturingTeam;
        objective->captureProgress = std::clamp(snapshot.captureProgress, 0.0f, 100.0f);
        objective->contested = snapshot.contested;
    }
}

std::vector<ObjectiveCapturedEvent> ObjectiveSimulation::DrainCapturedEvents() {
    std::vector<ObjectiveCapturedEvent> events;
    events.swap(mCapturedEvents);
    return events;
}

ObjectiveSimulation::ObjectiveEntity* ObjectiveSimulation::FindObjective(int32_t objectiveKey) {
    const auto indexed = mObjectiveByKey.find(objectiveKey);
    if (indexed == mObjectiveByKey.end()) return nullptr;
    ObjectiveEntity* objective = mObjectives.Get(indexed->second);
    if (objective && objective->definition.objectiveKey == objectiveKey) {
        return objective;
    }
    mObjectiveByKey.erase(indexed);
    return nullptr;
}

const ObjectiveSimulation::ObjectiveEntity* ObjectiveSimulation::FindObjective(int32_t objectiveKey) const {
    const auto indexed = mObjectiveByKey.find(objectiveKey);
    if (indexed == mObjectiveByKey.end()) return nullptr;
    const ObjectiveEntity* objective = mObjectives.Get(indexed->second);
    return objective && objective->definition.objectiveKey == objectiveKey
               ? objective
               : nullptr;
}

ObjectiveSnapshot ObjectiveSimulation::BuildSnapshot(const ObjectiveEntity& objective) {
    return { objective.id,
             objective.definition.objectiveKey,
             objective.definition.sceneId,
             objective.definition.position,
             objective.definition.captureRadius,
             objective.owner,
             objective.capturingTeam,
             objective.captureProgress,
             objective.contested };
}

} // namespace Game::Simulation
