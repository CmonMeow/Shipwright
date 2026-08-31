#pragma once

#include "CombatReplicationSystem.h"
#include "FishingPresentationRelay.h"
#include "OwnedEntityReplicationSystem.h"
#include "PlayerReplicationSystem.h"
#include "ReplicationBudgetSystem.h"
#include "ReplicationQueueSystem.h"
#include "SpatialEntityReplicationSystem.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Game::Replication {

enum class ReplicationSubmission : uint8_t {
    SendNow,
    Queued,
    Rejected,
};

struct ReplicationPlayerDeparture {
    std::vector<PlayerVisibilityTransition> playerLeaves;
};

// Aggregate owner for all per-observer replication state. Networking chooses
// wire formats and performs encryption; this coordinator owns visibility,
// bandwidth isolation, disposable coalescing, and disconnect cleanup.
class ServerReplicationCoordinator final {
  public:
    using Clock = std::chrono::steady_clock;
    using Sender = ReplicationQueueSystem::Sender;

    explicit ServerReplicationCoordinator(ReplicationBudgetConfig budgetConfig = {});

    void UpdateObservers(const std::vector<int32_t>& observers,
                         Clock::time_point now = Clock::now());
    ReplicationSubmission Submit(int32_t observerPlayerId,
                                 const ReplicationStreamKey& streamKey,
                                 ReplicationPriority priority, std::string payload,
                                 bool highTransportPriority, bool reliable,
                                 size_t budgetBytes);
    size_t Flush(const std::vector<int32_t>& observers, const Sender& sender,
                 size_t maximumPacketsPerObserver = 256);

    std::vector<PlayerVisibilityTransition> ReconcilePlayers(
        const std::vector<Simulation::PlayerSnapshot>& players,
        const std::vector<int32_t>& connectedObservers, float visibilityRadius);
    std::vector<OwnedEntityVisibilityTransition> ReconcileOwnedEntities(
        const std::vector<ReplicatedOwnedEntity>& entities,
        const std::vector<Simulation::PlayerSnapshot>& players,
        const std::vector<int32_t>& connectedObservers, float visibilityRadius);
    std::vector<SpatialEntityVisibilityTransition> ReconcileSpatialEntities(
        const std::vector<ReplicatedSpatialEntity>& entities,
        const std::vector<Simulation::PlayerSnapshot>& players,
        const std::vector<int32_t>& connectedObservers, float visibilityRadius);

    FishingPresentationUpdateResult UpdateFishingPresentation(
        const FishingPresentationState& presentation,
        const Simulation::PlayerSnapshot& authoritativePlayer);
    std::optional<FishingPresentationState> FishingPresentationFor(
        int32_t playerId) const;
    std::vector<CombatReplicationBatch> BuildCombatBatches(
        const std::vector<Simulation::CombatResultEvent>& results,
        const std::vector<Simulation::PlayerSnapshot>& players) const;

    ReplicationPlayerDeparture RemovePlayer(int32_t playerId);
    uint32_t NextOwnedEntityStateSequence(int32_t observerPlayerId,
                                          const OwnedEntityKey& entity);
    uint32_t NextSpatialEntityStateSequence(int32_t observerPlayerId,
                                            const SpatialEntityKey& entity);
    size_t RemoveQueuedEntity(int32_t observerPlayerId, int32_t ownerOrKey,
                              Simulation::EntityId entity);

    bool PlayerVisible(int32_t observerPlayerId, int32_t subjectPlayerId) const;
    std::vector<int32_t> PlayerObservers(int32_t subjectPlayerId) const;
    bool OwnedEntityVisible(int32_t observerPlayerId, const OwnedEntityKey& entity) const;
    std::vector<int32_t> OwnedEntityObservers(const OwnedEntityKey& entity) const;
    bool SpatialEntityVisible(int32_t observerPlayerId, const SpatialEntityKey& entity) const;
    std::vector<int32_t> SpatialEntityObservers(const SpatialEntityKey& entity) const;

    size_t ObserverCount() const;
    size_t PendingCount(int32_t observerPlayerId) const;
    size_t TotalPendingCount() const;
    size_t TotalPlayerVisibilityCount() const;
    size_t TotalOwnedVisibilityCount() const;
    size_t TotalSpatialVisibilityCount() const;
    size_t FishingPresentationCount() const;
    ReplicationBudgetStats BudgetStatsFor(int32_t observerPlayerId) const;
    ReplicationQueueStats QueueStatsFor(int32_t observerPlayerId) const;
    void Reset();

  private:
    PlayerReplicationSystem mPlayers;
    CombatReplicationSystem mCombat;
    FishingPresentationRelay mFishingPresentations;
    OwnedEntityReplicationSystem mOwnedEntities;
    ReplicationBudgetSystem mBudgets;
    ReplicationQueueSystem mQueue;
    SpatialEntityReplicationSystem mSpatialEntities;
};

} // namespace Game::Replication
