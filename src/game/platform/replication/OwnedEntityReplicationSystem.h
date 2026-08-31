#pragma once

#include "ObserverVisibilityIndex.h"
#include "../simulation/FishingSimulation.h"
#include "../simulation/ProjectileSimulation.h"
#include "../simulation/SpatialGridIndex.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <variant>
#include <vector>

namespace Game::Replication {

enum class OwnedEntityKind : uint8_t {
    Arrow,
    Fish,
    Lure,
};

struct OwnedEntityKey {
    OwnedEntityKind kind = OwnedEntityKind::Arrow;
    int32_t ownerPlayerId = -1;
    int32_t replicationId = 0;

    constexpr auto operator<=>(const OwnedEntityKey&) const = default;
};

using OwnedEntityLifecyclePayload =
    std::variant<std::monostate, Simulation::ArrowSnapshot,
                 Simulation::FishSnapshot, Simulation::FishingLureSnapshot>;

struct ReplicatedOwnedEntity {
    OwnedEntityKey key{};
    Simulation::EntityId entity{};
    int32_t sceneId = -1;
    Simulation::Vec3 position{};
    bool includeOwner = false;
    OwnedEntityLifecyclePayload payload{};
};

enum class OwnedEntityVisibilityAction : uint8_t {
    Enter,
    Leave,
};

struct OwnedEntityVisibilityTransition {
    int32_t observerPlayerId = -1;
    ReplicatedOwnedEntity subject{};
    OwnedEntityVisibilityAction action = OwnedEntityVisibilityAction::Enter;
    bool lifetimeEnded = false;
};

// Tracks exact server lifetimes for player-owned entities. Relevance follows
// each entity's authoritative scene and position, not its owner's visibility;
// this lets projectiles cross interest boundaries independently of shooters.
// The transport consumes deterministic enter/leave transitions and never
// infers lifecycle from disposable transform packets.
class OwnedEntityReplicationSystem final {
  public:
    std::vector<OwnedEntityVisibilityTransition> Reconcile(
        const std::vector<ReplicatedOwnedEntity>& entities,
        const std::vector<int32_t>& connectedObservers,
        const std::vector<Simulation::PlayerSnapshot>& players,
        float visibilityRadius);

    std::vector<OwnedEntityVisibilityTransition> RemoveObserver(int32_t observerPlayerId);
    uint32_t NextStateSequence(int32_t observerPlayerId, const OwnedEntityKey& entity);
    bool IsVisible(int32_t observerPlayerId, const OwnedEntityKey& entity) const;
    std::vector<int32_t> ObserversFor(const OwnedEntityKey& entity) const;
    size_t VisibleCount(int32_t observerPlayerId) const;
    size_t TotalVisiblePairCount() const;
    size_t LastCandidateCount() const { return mLastCandidateCount; }
    void Reset();

  private:
    using VisibleEntities = std::map<OwnedEntityKey, ReplicatedOwnedEntity>;

    std::map<int32_t, VisibleEntities> mVisibleByObserver;
    ObserverVisibilityIndex<OwnedEntityKey> mObserversByEntity;
    std::map<int32_t, std::map<OwnedEntityKey, uint32_t>> mLatestStateSequences;
    Simulation::SpatialGridIndex mSpatialIndex;
    size_t mLastCandidateCount = 0;
};

} // namespace Game::Replication
