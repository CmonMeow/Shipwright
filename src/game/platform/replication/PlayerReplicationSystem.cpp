#include "PlayerReplicationSystem.h"
#include "InterestRelevance.h"

#include <algorithm>
#include <set>

namespace Game::Replication {

namespace {

ReplicatedPlayer ToReplicatedPlayer(const Simulation::PlayerSnapshot& snapshot) {
    return { snapshot.ownerPlayerId, snapshot.entity, snapshot.sceneId, snapshot.position };
}

bool SameLifetime(const ReplicatedPlayer& left, const ReplicatedPlayer& right) {
    return left.entity == right.entity;
}

float HorizontalDistanceSquared(const Simulation::Vec3& left,
                                const Simulation::Vec3& right) {
    const float x = left.x - right.x;
    const float z = left.z - right.z;
    return x * x + z * z;
}

} // namespace

std::vector<PlayerVisibilityTransition> PlayerReplicationSystem::Reconcile(
    const std::vector<Simulation::PlayerSnapshot>& players,
    const std::vector<int32_t>& connectedObservers,
    float visibilityRadius) {
    std::vector<PlayerVisibilityTransition> transitions;
    std::map<int32_t, ReplicatedPlayer> currentPlayers;
    mSpatialIndex.Reset();
    for (const Simulation::PlayerSnapshot& snapshot : players) {
        if (snapshot.ownerPlayerId < 0 || !snapshot.entity.Valid() || snapshot.sceneId < 0) continue;
        currentPlayers[snapshot.ownerPlayerId] = ToReplicatedPlayer(snapshot);
        mSpatialIndex.Update(snapshot.ownerPlayerId, snapshot.sceneId, snapshot.position);
    }

    const std::set<int32_t> observers(connectedObservers.begin(), connectedObservers.end());
    for (auto visible = mVisibleByObserver.begin(); visible != mVisibleByObserver.end();) {
        if (observers.count(visible->first) == 0) {
            for (const auto& subject : visible->second) {
                mObserversBySubject.Remove(visible->first, subject.first);
            }
            visible = mVisibleByObserver.erase(visible);
        } else {
            ++visible;
        }
    }

    for (const int32_t observerId : observers) {
        VisiblePlayerMap desired;
        const auto observer = currentPlayers.find(observerId);
        if (observer != currentPlayers.end()) {
            const InterestRadii radii = MakeInterestRadii(visibilityRadius);
            const auto previousVisible = mVisibleByObserver.find(observerId);
            for (const Simulation::SpatialIndexId candidateId : mSpatialIndex.CandidatesNear(
                     observer->second.sceneId, observer->second.position, radii.leave)) {
                const int32_t subjectId = static_cast<int32_t>(candidateId);
                if (subjectId == observerId) continue;
                const auto subject = currentPlayers.find(subjectId);
                bool alreadyVisible = false;
                if (subject != currentPlayers.end() &&
                    previousVisible != mVisibleByObserver.end()) {
                    const auto previous = previousVisible->second.find(subjectId);
                    alreadyVisible = previous != previousVisible->second.end() &&
                                     SameLifetime(previous->second, subject->second);
                }
                if (subject != currentPlayers.end() && WithinInterest(
                        HorizontalDistanceSquared(subject->second.position,
                                                  observer->second.position),
                        alreadyVisible, radii)) {
                    desired.emplace(subjectId, subject->second);
                }
            }
        }

        VisiblePlayerMap& visible = mVisibleByObserver[observerId];
        for (const auto& [subjectId, previous] : visible) {
            const auto next = desired.find(subjectId);
            if (next == desired.end() || !SameLifetime(previous, next->second)) {
                transitions.push_back({ observerId, previous, PlayerVisibilityAction::Leave });
                mObserversBySubject.Remove(observerId, subjectId);
            }
        }
        for (const auto& [subjectId, next] : desired) {
            const auto previous = visible.find(subjectId);
            if (previous == visible.end() || !SameLifetime(previous->second, next)) {
                transitions.push_back({ observerId, next, PlayerVisibilityAction::Enter });
                mObserversBySubject.Add(observerId, subjectId);
            }
        }
        visible = std::move(desired);
    }
    return transitions;
}

std::vector<PlayerVisibilityTransition> PlayerReplicationSystem::RemovePlayer(int32_t playerId) {
    std::vector<PlayerVisibilityTransition> transitions;
    const auto departingObserver = mVisibleByObserver.find(playerId);
    if (departingObserver != mVisibleByObserver.end()) {
        for (const auto& subject : departingObserver->second) {
            mObserversBySubject.Remove(playerId, subject.first);
        }
        mVisibleByObserver.erase(departingObserver);
    }
    for (auto& [observerId, visible] : mVisibleByObserver) {
        const auto subject = visible.find(playerId);
        if (subject == visible.end()) continue;
        transitions.push_back({ observerId, subject->second, PlayerVisibilityAction::Leave });
        mObserversBySubject.Remove(observerId, playerId);
        visible.erase(subject);
    }
    mSpatialIndex.Remove(playerId);
    return transitions;
}

bool PlayerReplicationSystem::IsVisible(int32_t observerPlayerId, int32_t subjectPlayerId) const {
    const auto observer = mVisibleByObserver.find(observerPlayerId);
    return observer != mVisibleByObserver.end() && observer->second.count(subjectPlayerId) != 0;
}

std::vector<int32_t> PlayerReplicationSystem::VisiblePlayerIds(int32_t observerPlayerId) const {
    std::vector<int32_t> result;
    const auto observer = mVisibleByObserver.find(observerPlayerId);
    if (observer == mVisibleByObserver.end()) return result;
    result.reserve(observer->second.size());
    for (const auto& visible : observer->second) result.push_back(visible.first);
    return result;
}

std::vector<int32_t> PlayerReplicationSystem::ObserversForPlayer(
    int32_t subjectPlayerId) const {
    return mObserversBySubject.ObserversFor(subjectPlayerId);
}

bool PlayerReplicationSystem::HasObserver(int32_t observerPlayerId) const {
    return mVisibleByObserver.count(observerPlayerId) != 0;
}

size_t PlayerReplicationSystem::VisibleCount(int32_t observerPlayerId) const {
    const auto observer = mVisibleByObserver.find(observerPlayerId);
    return observer == mVisibleByObserver.end() ? 0 : observer->second.size();
}

size_t PlayerReplicationSystem::TotalVisiblePairCount() const {
    size_t total = 0;
    for (const auto& observer : mVisibleByObserver) {
        total += observer.second.size();
    }
    return total;
}

void PlayerReplicationSystem::Reset() {
    mSpatialIndex.Reset();
    mVisibleByObserver.clear();
    mObserversBySubject.Clear();
}

} // namespace Game::Replication
