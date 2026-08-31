#include "StructureSimulation.h"
#include "CombatGeometry.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace Game::Simulation {

EntityId StructureSimulation::EnsureStructure(const StructureDefinition& definition) {
    if (definition.structureKey < 0 || definition.objectiveKey < 0 ||
        definition.sceneId < 0 || !std::isfinite(definition.position.x) ||
        !std::isfinite(definition.position.y) ||
        !std::isfinite(definition.position.z)) {
        return {};
    }
    if (StructureEntity* existing = FindStructure(definition.structureKey)) {
        return existing->id;
    }
    StructureEntity structure{};
    structure.definition = definition;
    structure.definition.maximumHealth = std::max<uint32_t>(1, definition.maximumHealth);
    structure.definition.requiredBuild = std::max<uint32_t>(1, definition.requiredBuild);
    structure.id = mStructures.Create(structure);
    StructureEntity* created = mStructures.Get(structure.id);
    if (!created) return {};
    created->id = structure.id;
    mStructureByKey.insert_or_assign(definition.structureKey, structure.id);
    mStructureSpatialIndex.Update(definition.structureKey, definition.sceneId,
                                  definition.position);
    return structure.id;
}

bool StructureSimulation::RemoveStructure(int32_t structureKey) {
    const auto indexed = mStructureByKey.find(structureKey);
    if (indexed == mStructureByKey.end() ||
        !mStructures.Destroy(indexed->second)) {
        return false;
    }
    mStructureByKey.erase(indexed);
    mStructureSpatialIndex.Remove(structureKey);
    return true;
}

void StructureSimulation::Reset() {
    mStructures.Clear();
    mStructureByKey.clear();
    mStructureSpatialIndex.Reset();
    mEvents.clear();
}

bool StructureSimulation::ContributeBuild(int32_t structureKey, TeamId sourceTeam, TeamId objectiveOwner,
                                          uint32_t amount) {
    StructureEntity* structure = FindStructure(structureKey);
    if (!structure || amount == 0 || sourceTeam == TeamId::Neutral || sourceTeam != objectiveOwner ||
        structure->phase == StructurePhase::Active || structure->phase == StructurePhase::Destroyed ||
        (structure->team != TeamId::Neutral && structure->team != sourceTeam)) {
        return false;
    }
    if (structure->phase == StructurePhase::Planned) {
        structure->phase = StructurePhase::Building;
        structure->team = sourceTeam;
        mEvents.push_back({ structure->id, structureKey, StructureEventKind::BuildStarted, sourceTeam, 0 });
    }
    const uint32_t remaining = structure->definition.requiredBuild - structure->buildProgress;
    const uint32_t accepted = std::min(amount, remaining);
    structure->buildProgress += accepted;
    if (structure->buildProgress == structure->definition.requiredBuild) {
        structure->phase = StructurePhase::Active;
        structure->health = structure->definition.maximumHealth;
        mEvents.push_back({ structure->id, structureKey, StructureEventKind::Built, sourceTeam, accepted });
    }
    return accepted != 0;
}

bool StructureSimulation::ApplyDamage(int32_t structureKey, TeamId sourceTeam, uint32_t amount) {
    StructureEntity* structure = FindStructure(structureKey);
    if (!structure || structure->phase != StructurePhase::Active || amount == 0 ||
        (sourceTeam != TeamId::Neutral && sourceTeam == structure->team)) {
        return false;
    }
    const uint32_t accepted = std::min(amount, structure->health);
    structure->health -= accepted;
    mEvents.push_back({ structure->id, structureKey, StructureEventKind::Damaged, sourceTeam, accepted });
    if (structure->health == 0) {
        structure->phase = StructurePhase::Destroyed;
        mEvents.push_back({ structure->id, structureKey, StructureEventKind::Destroyed, sourceTeam, accepted });
    }
    return accepted != 0;
}

bool StructureSimulation::Repair(int32_t structureKey, TeamId sourceTeam, uint32_t amount) {
    StructureEntity* structure = FindStructure(structureKey);
    if (!structure || structure->phase != StructurePhase::Active || sourceTeam != structure->team || amount == 0 ||
        structure->health == structure->definition.maximumHealth) {
        return false;
    }
    const uint32_t accepted = std::min(amount, structure->definition.maximumHealth - structure->health);
    structure->health += accepted;
    mEvents.push_back({ structure->id, structureKey, StructureEventKind::Repaired, sourceTeam, accepted });
    return true;
}

bool StructureSimulation::ResetStructure(int32_t structureKey) {
    StructureEntity* structure = FindStructure(structureKey);
    if (!structure || (structure->phase == StructurePhase::Planned && structure->team == TeamId::Neutral)) {
        return false;
    }
    structure->team = TeamId::Neutral;
    structure->phase = StructurePhase::Planned;
    structure->health = 0;
    structure->buildProgress = 0;
    mEvents.push_back({ structure->id, structureKey, StructureEventKind::Reset, TeamId::Neutral, 0 });
    return true;
}

bool StructureSimulation::FirstSegmentHit(int32_t sceneId, const Vec3& start,
                                          const Vec3& end, StructureHit& hit) const {
    bool found = false;
    float closestRatio = 2.0f;
    const Vec3 midpoint{ (start.x + end.x) * 0.5f,
                         (start.y + end.y) * 0.5f,
                         (start.z + end.z) * 0.5f };
    const float queryRadius =
        std::hypot(end.x - start.x, end.z - start.z) * 0.5f +
        CollisionRadius();
    for (const SpatialIndexId candidate :
         mStructureSpatialIndex.CandidatesNear(sceneId, midpoint, queryRadius)) {
        if (candidate < 0 || candidate > INT32_MAX) continue;
        const StructureEntity* structure =
            FindStructure(static_cast<int32_t>(candidate));
        if (!structure || structure->definition.sceneId != sceneId ||
            structure->phase != StructurePhase::Active) {
            continue;
        }
        float ratio = 0.0f;
        if (!SegmentVerticalCylinderFirstHit(start, end, structure->definition.position,
                                             CollisionRadius(), CollisionHeight(), ratio) ||
            ratio >= closestRatio) {
            continue;
        }
        closestRatio = ratio;
        found = true;
        hit = { structure->id, structure->definition.structureKey, ratio,
                { start.x + (end.x - start.x) * ratio,
                  start.y + (end.y - start.y) * ratio,
                  start.z + (end.z - start.z) * ratio } };
    }
    return found;
}

std::optional<StructureSnapshot> StructureSimulation::SnapshotForStructure(int32_t structureKey) const {
    const StructureEntity* structure = FindStructure(structureKey);
    return structure ? std::optional<StructureSnapshot>(BuildSnapshot(*structure)) : std::nullopt;
}

std::vector<StructureSnapshot> StructureSimulation::Snapshots() const {
    std::vector<StructureSnapshot> snapshots;
    snapshots.reserve(mStructures.Size());
    mStructures.ForEach([&](const StructureEntity& structure) { snapshots.push_back(BuildSnapshot(structure)); });
    return snapshots;
}

void StructureSimulation::Restore(const std::vector<StructureSnapshot>& snapshots) {
    Reset();
    for (const StructureSnapshot& snapshot : snapshots) {
        const EntityId id = EnsureStructure({ snapshot.structureKey, snapshot.objectiveKey, snapshot.sceneId,
                                              snapshot.position, snapshot.maximumHealth, snapshot.requiredBuild });
        StructureEntity* structure = mStructures.Get(id);
        structure->team = snapshot.team;
        structure->phase = snapshot.phase;
        structure->health = std::min(snapshot.health, structure->definition.maximumHealth);
        structure->buildProgress = std::min(snapshot.buildProgress, structure->definition.requiredBuild);
    }
}

std::vector<StructureEvent> StructureSimulation::DrainEvents() {
    std::vector<StructureEvent> events;
    events.swap(mEvents);
    return events;
}

StructureSimulation::StructureEntity* StructureSimulation::FindStructure(int32_t structureKey) {
    const auto indexed = mStructureByKey.find(structureKey);
    if (indexed == mStructureByKey.end()) return nullptr;
    StructureEntity* structure = mStructures.Get(indexed->second);
    if (structure && structure->definition.structureKey == structureKey) {
        return structure;
    }
    mStructureByKey.erase(indexed);
    return nullptr;
}

const StructureSimulation::StructureEntity* StructureSimulation::FindStructure(int32_t structureKey) const {
    const auto indexed = mStructureByKey.find(structureKey);
    if (indexed == mStructureByKey.end()) return nullptr;
    const StructureEntity* structure = mStructures.Get(indexed->second);
    return structure && structure->definition.structureKey == structureKey
               ? structure
               : nullptr;
}

StructureSnapshot StructureSimulation::BuildSnapshot(const StructureEntity& structure) {
    return { structure.id,
             structure.definition.structureKey,
             structure.definition.objectiveKey,
             structure.definition.sceneId,
             structure.definition.position,
             structure.team,
             structure.phase,
             structure.health,
             structure.definition.maximumHealth,
             structure.buildProgress,
             structure.definition.requiredBuild };
}

} // namespace Game::Simulation
