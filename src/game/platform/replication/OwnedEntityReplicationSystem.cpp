#include "OwnedEntityReplicationSystem.h"
#include "InterestRelevance.h"

#include <algorithm>
#include <set>

namespace Game::Replication {

namespace {

bool SameLifetime(const ReplicatedOwnedEntity& left, const ReplicatedOwnedEntity& right) {
    return left.entity == right.entity;
}

float DistanceSquared(const Simulation::Vec3& left,
                      const Simulation::Vec3& right) {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return x * x + y * y + z * z;
}

} // namespace

std::vector<OwnedEntityVisibilityTransition> OwnedEntityReplicationSystem::Reconcile(
    const std::vector<ReplicatedOwnedEntity>& entities,
    const std::vector<int32_t>& connectedObservers,
    const std::vector<Simulation::PlayerSnapshot>& players,
    float visibilityRadius) {
    std::vector<OwnedEntityVisibilityTransition> transitions;
    std::map<OwnedEntityKey, ReplicatedOwnedEntity> current;
    for (const ReplicatedOwnedEntity& entity : entities) {
        if (entity.key.ownerPlayerId < 0 || entity.key.replicationId <= 0 ||
            !entity.entity.Valid() || entity.sceneId < 0) {
            continue;
        }
        current[entity.key] = entity;
    }
    mSpatialIndex.Reset();
    std::vector<const ReplicatedOwnedEntity*> indexedEntities;
    std::map<int32_t, std::vector<const ReplicatedOwnedEntity*>> entitiesByOwner;
    indexedEntities.reserve(current.size());
    for (const auto& [key, entity] : current) {
        const auto index = static_cast<Simulation::SpatialIndexId>(indexedEntities.size());
        indexedEntities.push_back(&entity);
        entitiesByOwner[key.ownerPlayerId].push_back(&entity);
        mSpatialIndex.Update(index, entity.sceneId, entity.position);
    }
    std::map<int32_t, const Simulation::PlayerSnapshot*> observerStates;
    for (const Simulation::PlayerSnapshot& player : players) {
        if (player.ownerPlayerId >= 0 && player.entity.Valid() && player.sceneId >= 0) {
            observerStates[player.ownerPlayerId] = &player;
        }
    }
    mLastCandidateCount = 0;

    const std::set<int32_t> observers(connectedObservers.begin(), connectedObservers.end());
    for (auto visible = mVisibleByObserver.begin(); visible != mVisibleByObserver.end();) {
        if (observers.count(visible->first) == 0) {
            for (const auto& entity : visible->second) {
                mObserversByEntity.Remove(visible->first, entity.first);
            }
            visible = mVisibleByObserver.erase(visible);
        } else {
            ++visible;
        }
    }
    for (auto sequences = mLatestStateSequences.begin();
         sequences != mLatestStateSequences.end();) {
        if (observers.count(sequences->first) == 0) {
            sequences = mLatestStateSequences.erase(sequences);
        } else {
            ++sequences;
        }
    }

    const InterestRadii radii = MakeInterestRadii(visibilityRadius);
    for (const int32_t observerId : observers) {
        VisibleEntities desired;
        const auto previousVisible = mVisibleByObserver.find(observerId);
        // The predicting owner receives only entities explicitly marked for
        // owner reconciliation, even if temporary divergence places them
        // outside the ordinary interest radius.
        if (const auto owned = entitiesByOwner.find(observerId);
            owned != entitiesByOwner.end()) {
            for (const ReplicatedOwnedEntity* entity : owned->second) {
                ++mLastCandidateCount;
                if (entity->includeOwner) desired.emplace(entity->key, *entity);
            }
        }
        const auto observer = observerStates.find(observerId);
        if (observer != observerStates.end()) {
            for (const Simulation::SpatialIndexId candidateId :
                 mSpatialIndex.CandidatesNear(observer->second->sceneId,
                                              observer->second->position,
                                              radii.leave)) {
                ++mLastCandidateCount;
                const ReplicatedOwnedEntity& entity =
                    *indexedEntities[static_cast<size_t>(candidateId)];
                bool alreadyVisible = false;
                if (previousVisible != mVisibleByObserver.end()) {
                    const auto previous = previousVisible->second.find(entity.key);
                    alreadyVisible = previous != previousVisible->second.end() &&
                                     SameLifetime(previous->second, entity);
                }
                if (entity.sceneId != observer->second->sceneId ||
                    entity.key.ownerPlayerId == observerId ||
                    !WithinInterest(DistanceSquared(entity.position,
                                                     observer->second->position),
                                    alreadyVisible, radii)) {
                    continue;
                }
                desired.emplace(entity.key, entity);
            }
        }

        VisibleEntities& visible = mVisibleByObserver[observerId];
        for (const auto& [key, previous] : visible) {
            const auto next = desired.find(key);
            if (next == desired.end() || !SameLifetime(previous, next->second)) {
                const auto authoritative = current.find(key);
                const bool lifetimeEnded = authoritative == current.end() ||
                    !SameLifetime(previous, authoritative->second);
                transitions.push_back({ observerId, previous,
                                        OwnedEntityVisibilityAction::Leave,
                                        lifetimeEnded });
                mObserversByEntity.Remove(observerId, key);
            }
        }
        for (const auto& [key, next] : desired) {
            const auto previous = visible.find(key);
            if (previous == visible.end() || !SameLifetime(previous->second, next)) {
                transitions.push_back({ observerId, next, OwnedEntityVisibilityAction::Enter });
                mObserversByEntity.Add(observerId, key);
            }
        }
        visible = std::move(desired);
    }
    return transitions;
}

std::vector<OwnedEntityVisibilityTransition> OwnedEntityReplicationSystem::RemoveObserver(
    int32_t observerPlayerId) {
    std::vector<OwnedEntityVisibilityTransition> transitions;
    const auto observer = mVisibleByObserver.find(observerPlayerId);
    if (observer == mVisibleByObserver.end()) {
        mLatestStateSequences.erase(observerPlayerId);
        return transitions;
    }
    for (const auto& [key, entity] : observer->second) {
        transitions.push_back({ observerPlayerId, entity, OwnedEntityVisibilityAction::Leave });
        mObserversByEntity.Remove(observerPlayerId, key);
    }
    mVisibleByObserver.erase(observer);
    mLatestStateSequences.erase(observerPlayerId);
    return transitions;
}

uint32_t OwnedEntityReplicationSystem::NextStateSequence(
    int32_t observerPlayerId, const OwnedEntityKey& entity) {
    if (observerPlayerId < 0 || entity.ownerPlayerId < 0 || entity.replicationId <= 0) {
        return 0;
    }
    uint32_t& sequence = mLatestStateSequences[observerPlayerId][entity];
    ++sequence;
    if (sequence == 0) ++sequence;
    return sequence;
}

bool OwnedEntityReplicationSystem::IsVisible(int32_t observerPlayerId,
                                             const OwnedEntityKey& entity) const {
    const auto observer = mVisibleByObserver.find(observerPlayerId);
    return observer != mVisibleByObserver.end() && observer->second.count(entity) != 0;
}

std::vector<int32_t> OwnedEntityReplicationSystem::ObserversFor(
    const OwnedEntityKey& entity) const {
    return mObserversByEntity.ObserversFor(entity);
}

size_t OwnedEntityReplicationSystem::VisibleCount(int32_t observerPlayerId) const {
    const auto observer = mVisibleByObserver.find(observerPlayerId);
    return observer == mVisibleByObserver.end() ? 0 : observer->second.size();
}

size_t OwnedEntityReplicationSystem::TotalVisiblePairCount() const {
    size_t total = 0;
    for (const auto& observer : mVisibleByObserver) total += observer.second.size();
    return total;
}

void OwnedEntityReplicationSystem::Reset() {
    mSpatialIndex.Reset();
    mVisibleByObserver.clear();
    mObserversByEntity.Clear();
    mLatestStateSequences.clear();
    mLastCandidateCount = 0;
}

} // namespace Game::Replication
