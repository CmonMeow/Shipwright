#pragma once

#include "ObserverVisibilityIndex.h"
#include "../simulation/PlayerSimulation.h"
#include "../simulation/SpatialGridIndex.h"
#include "../simulation/CorpseSimulation.h"
#include "../simulation/ObjectiveSimulation.h"
#include "../simulation/StructureSimulation.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <variant>
#include <vector>

namespace Game::Replication {

enum class SpatialEntityKind : uint8_t {
    Corpse,
    Objective,
    Structure,
};

struct SpatialEntityKey {
    SpatialEntityKind kind = SpatialEntityKind::Corpse;
    int32_t logicalKey = 0;

    constexpr auto operator<=>(const SpatialEntityKey&) const = default;
};

using SpatialEntityLifecyclePayload =
    std::variant<std::monostate, Simulation::CorpseSnapshot,
                 Simulation::ObjectiveSnapshot, Simulation::StructureSnapshot>;

struct ReplicatedSpatialEntity {
    SpatialEntityKey key{};
    Simulation::EntityId entity{};
    int32_t ownerOrKey = -1;
    int32_t subKey = -1;
    int32_t sceneId = -1;
    Simulation::Vec3 position{};
    SpatialEntityLifecyclePayload payload{};
};

enum class SpatialEntityVisibilityAction : uint8_t {
    Enter,
    Leave,
};

struct SpatialEntityVisibilityTransition {
    int32_t observerPlayerId = -1;
    ReplicatedSpatialEntity subject{};
    SpatialEntityVisibilityAction action = SpatialEntityVisibilityAction::Enter;
    bool lifetimeEnded = false;
};

class SpatialEntityReplicationSystem final {
  public:
    std::vector<SpatialEntityVisibilityTransition> Reconcile(
        const std::vector<ReplicatedSpatialEntity>& entities,
        const std::vector<Simulation::PlayerSnapshot>& players,
        const std::vector<int32_t>& connectedObservers,
        float visibilityRadius);

    void RemoveObserver(int32_t observerPlayerId);
    uint32_t NextStateSequence(int32_t observerPlayerId,
                               const SpatialEntityKey& entity);
    bool IsVisible(int32_t observerPlayerId, const SpatialEntityKey& entity) const;
    std::vector<int32_t> ObserversFor(const SpatialEntityKey& entity) const;
    size_t VisibleCount(int32_t observerPlayerId) const;
    size_t TotalVisiblePairCount() const;
    size_t LastCandidateCount() const { return mLastCandidateCount; }
    void Reset();

  private:
    using VisibleEntities = std::map<SpatialEntityKey, ReplicatedSpatialEntity>;

    Simulation::SpatialGridIndex mSpatialIndex;
    std::map<int32_t, VisibleEntities> mVisibleByObserver;
    ObserverVisibilityIndex<SpatialEntityKey> mObserversByEntity;
    std::map<int32_t, std::map<SpatialEntityKey, uint32_t>> mLatestStateSequences;
    size_t mLastCandidateCount = 0;
};

} // namespace Game::Replication
