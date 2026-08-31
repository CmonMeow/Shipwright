#include "ServerReplicationCoordinator.h"

#include <utility>

namespace Game::Replication {

ServerReplicationCoordinator::ServerReplicationCoordinator(ReplicationBudgetConfig budgetConfig)
    : mBudgets(budgetConfig) {
}

void ServerReplicationCoordinator::UpdateObservers(const std::vector<int32_t>& observers,
                                                     Clock::time_point now) {
    mBudgets.UpdateObservers(observers, now);
    mQueue.UpdateObservers(observers);
}

ReplicationSubmission ServerReplicationCoordinator::Submit(
    int32_t observerPlayerId, const ReplicationStreamKey& streamKey,
    ReplicationPriority priority, std::string payload, bool highTransportPriority,
    bool reliable, size_t budgetBytes) {
    if (observerPlayerId < 0 || payload.empty() || budgetBytes < payload.size()) {
        return ReplicationSubmission::Rejected;
    }
    if (reliable) {
        mBudgets.TryConsume(observerPlayerId, ReplicationPriority::Critical, budgetBytes);
        return ReplicationSubmission::SendNow;
    }
    if (streamKey.Valid()) {
        return mQueue.Enqueue(observerPlayerId, streamKey, priority, std::move(payload),
                              highTransportPriority)
                   ? ReplicationSubmission::Queued
                   : ReplicationSubmission::Rejected;
    }
    return mBudgets.TryConsume(observerPlayerId, priority, budgetBytes)
               ? ReplicationSubmission::SendNow
               : ReplicationSubmission::Rejected;
}

size_t ServerReplicationCoordinator::Flush(const std::vector<int32_t>& observers,
                                            const Sender& sender,
                                            size_t maximumPacketsPerObserver) {
    return mQueue.Flush(observers, mBudgets, sender, maximumPacketsPerObserver);
}

std::vector<PlayerVisibilityTransition> ServerReplicationCoordinator::ReconcilePlayers(
    const std::vector<Simulation::PlayerSnapshot>& players,
    const std::vector<int32_t>& connectedObservers, float visibilityRadius) {
    mFishingPresentations.Reconcile(players);
    return mPlayers.Reconcile(players, connectedObservers, visibilityRadius);
}

std::vector<OwnedEntityVisibilityTransition>
ServerReplicationCoordinator::ReconcileOwnedEntities(
    const std::vector<ReplicatedOwnedEntity>& entities,
    const std::vector<Simulation::PlayerSnapshot>& players,
    const std::vector<int32_t>& connectedObservers, float visibilityRadius) {
    return mOwnedEntities.Reconcile(entities, connectedObservers, players,
                                    visibilityRadius);
}

std::vector<SpatialEntityVisibilityTransition>
ServerReplicationCoordinator::ReconcileSpatialEntities(
    const std::vector<ReplicatedSpatialEntity>& entities,
    const std::vector<Simulation::PlayerSnapshot>& players,
    const std::vector<int32_t>& connectedObservers, float visibilityRadius) {
    return mSpatialEntities.Reconcile(entities, players, connectedObservers, visibilityRadius);
}

ReplicationPlayerDeparture ServerReplicationCoordinator::RemovePlayer(int32_t playerId) {
    ReplicationPlayerDeparture departure{};
    departure.playerLeaves = mPlayers.RemovePlayer(playerId);
    mFishingPresentations.RemovePlayer(playerId);
    mOwnedEntities.RemoveObserver(playerId);
    mSpatialEntities.RemoveObserver(playerId);
    mBudgets.RemoveObserver(playerId);
    mQueue.RemoveObserver(playerId);
    return departure;
}

FishingPresentationUpdateResult
ServerReplicationCoordinator::UpdateFishingPresentation(
    const FishingPresentationState& presentation,
    const Simulation::PlayerSnapshot& authoritativePlayer) {
    return mFishingPresentations.Update(presentation, authoritativePlayer);
}

std::optional<FishingPresentationState>
ServerReplicationCoordinator::FishingPresentationFor(int32_t playerId) const {
    return mFishingPresentations.ForPlayer(playerId);
}

std::vector<CombatReplicationBatch>
ServerReplicationCoordinator::BuildCombatBatches(
    const std::vector<Simulation::CombatResultEvent>& results,
    const std::vector<Simulation::PlayerSnapshot>& players) const {
    return mCombat.BuildBatches(results, players, mPlayers);
}

uint32_t ServerReplicationCoordinator::NextOwnedEntityStateSequence(
    int32_t observerPlayerId, const OwnedEntityKey& entity) {
    return mOwnedEntities.NextStateSequence(observerPlayerId, entity);
}

uint32_t ServerReplicationCoordinator::NextSpatialEntityStateSequence(
    int32_t observerPlayerId, const SpatialEntityKey& entity) {
    return mSpatialEntities.NextStateSequence(observerPlayerId, entity);
}

size_t ServerReplicationCoordinator::RemoveQueuedEntity(
    int32_t observerPlayerId, int32_t ownerOrKey, Simulation::EntityId entity) {
    return mQueue.RemoveEntity(observerPlayerId, ownerOrKey, entity);
}

bool ServerReplicationCoordinator::PlayerVisible(int32_t observerPlayerId,
                                                  int32_t subjectPlayerId) const {
    return mPlayers.IsVisible(observerPlayerId, subjectPlayerId);
}

std::vector<int32_t> ServerReplicationCoordinator::PlayerObservers(
    int32_t subjectPlayerId) const {
    return mPlayers.ObserversForPlayer(subjectPlayerId);
}

bool ServerReplicationCoordinator::OwnedEntityVisible(
    int32_t observerPlayerId, const OwnedEntityKey& entity) const {
    return mOwnedEntities.IsVisible(observerPlayerId, entity);
}

std::vector<int32_t> ServerReplicationCoordinator::OwnedEntityObservers(
    const OwnedEntityKey& entity) const {
    return mOwnedEntities.ObserversFor(entity);
}

bool ServerReplicationCoordinator::SpatialEntityVisible(
    int32_t observerPlayerId, const SpatialEntityKey& entity) const {
    return mSpatialEntities.IsVisible(observerPlayerId, entity);
}

std::vector<int32_t> ServerReplicationCoordinator::SpatialEntityObservers(
    const SpatialEntityKey& entity) const {
    return mSpatialEntities.ObserversFor(entity);
}

size_t ServerReplicationCoordinator::ObserverCount() const {
    return mBudgets.ObserverCount();
}

size_t ServerReplicationCoordinator::PendingCount(int32_t observerPlayerId) const {
    return mQueue.PendingCount(observerPlayerId);
}

size_t ServerReplicationCoordinator::TotalPendingCount() const {
    return mQueue.TotalPendingCount();
}

size_t ServerReplicationCoordinator::TotalPlayerVisibilityCount() const {
    return mPlayers.TotalVisiblePairCount();
}

size_t ServerReplicationCoordinator::TotalOwnedVisibilityCount() const {
    return mOwnedEntities.TotalVisiblePairCount();
}

size_t ServerReplicationCoordinator::TotalSpatialVisibilityCount() const {
    return mSpatialEntities.TotalVisiblePairCount();
}

size_t ServerReplicationCoordinator::FishingPresentationCount() const {
    return mFishingPresentations.Count();
}

ReplicationBudgetStats ServerReplicationCoordinator::BudgetStatsFor(
    int32_t observerPlayerId) const {
    return mBudgets.StatsFor(observerPlayerId);
}

ReplicationQueueStats ServerReplicationCoordinator::QueueStatsFor(
    int32_t observerPlayerId) const {
    return mQueue.StatsFor(observerPlayerId);
}

void ServerReplicationCoordinator::Reset() {
    mPlayers.Reset();
    mFishingPresentations.Reset();
    mOwnedEntities.Reset();
    mBudgets.Reset();
    mQueue.Reset();
    mSpatialEntities.Reset();
}

} // namespace Game::Replication
