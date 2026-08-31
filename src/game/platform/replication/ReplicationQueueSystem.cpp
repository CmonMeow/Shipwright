#include "ReplicationQueueSystem.h"

#include <algorithm>
#include <set>

namespace Game::Replication {

bool ReplicationQueueSystem::Enqueue(int32_t observerPlayerId, const ReplicationStreamKey& key,
                                     ReplicationPriority priority, std::string payload,
                                     bool highTransportPriority) {
    if (observerPlayerId < 0 || !key.Valid() || payload.empty() ||
        priority == ReplicationPriority::Critical) {
        return false;
    }
    ObserverQueue& observer = mObservers[observerPlayerId];
    auto [message, inserted] = observer.pending.try_emplace(key);
    if (!inserted) ++observer.stats.coalesced;
    message->second = { priority, std::move(payload), highTransportPriority };
    ++observer.stats.enqueued;
    return true;
}

size_t ReplicationQueueSystem::FlushObserver(int32_t observerPlayerId,
                                             ReplicationBudgetSystem& budgets,
                                             const Sender& sender,
                                             size_t maximumPackets) {
    auto observer = mObservers.find(observerPlayerId);
    if (observer == mObservers.end() || !sender || maximumPackets == 0) return 0;

    size_t sent = 0;
    constexpr std::array<ReplicationPriority, 3> priorities{
        ReplicationPriority::High, ReplicationPriority::Normal, ReplicationPriority::Low
    };
    for (const ReplicationPriority priority : priorities) {
        size_t sentForPriority = 0;
        for (const ReplicationStreamKey& key : OrderedKeys(observer->second, priority)) {
            if (sentForPriority >= maximumPackets) break;
            const auto message = observer->second.pending.find(key);
            if (message == observer->second.pending.end()) continue;
            constexpr size_t kEncryptedEnvelopeOverhead = 64;
            const size_t bytes = message->second.payload.size() + kEncryptedEnvelopeOverhead;
            if (!budgets.TryConsume(observerPlayerId, priority, bytes)) continue;
            if (!sender(observerPlayerId, message->second.payload,
                        message->second.highTransportPriority)) {
                ++observer->second.stats.sendRetries;
                continue;
            }
            const size_t priorityIndex = PriorityIndex(priority);
            observer->second.cursors[priorityIndex] = key;
            observer->second.hasCursor[priorityIndex] = true;
            observer->second.pending.erase(message);
            ++observer->second.stats.sent;
            ++sentForPriority;
            ++sent;
        }
    }
    return sent;
}

size_t ReplicationQueueSystem::Flush(const std::vector<int32_t>& observers,
                                     ReplicationBudgetSystem& budgets,
                                     const Sender& sender,
                                     size_t maximumPacketsPerObserver) {
    size_t sent = 0;
    for (const int32_t observer : observers) {
        sent += FlushObserver(observer, budgets, sender, maximumPacketsPerObserver);
    }
    return sent;
}

void ReplicationQueueSystem::UpdateObservers(const std::vector<int32_t>& observers) {
    const std::set<int32_t> active(observers.begin(), observers.end());
    for (auto observer = mObservers.begin(); observer != mObservers.end();) {
        if (active.count(observer->first) == 0) {
            observer = mObservers.erase(observer);
        } else {
            ++observer;
        }
    }
    for (const int32_t observer : active) {
        if (observer >= 0) mObservers.try_emplace(observer);
    }
}

void ReplicationQueueSystem::RemoveObserver(int32_t observerPlayerId) {
    mObservers.erase(observerPlayerId);
}

size_t ReplicationQueueSystem::RemoveEntity(int32_t observerPlayerId, int32_t ownerOrKey,
                                            Simulation::EntityId entity) {
    const auto observer = mObservers.find(observerPlayerId);
    if (observer == mObservers.end()) return 0;
    size_t removed = 0;
    for (auto message = observer->second.pending.begin(); message != observer->second.pending.end();) {
        if (message->first.ownerOrKey == ownerOrKey && message->first.entity == entity) {
            message = observer->second.pending.erase(message);
            ++removed;
        } else {
            ++message;
        }
    }
    return removed;
}

size_t ReplicationQueueSystem::PendingCount(int32_t observerPlayerId) const {
    const auto observer = mObservers.find(observerPlayerId);
    return observer == mObservers.end() ? 0 : observer->second.pending.size();
}

size_t ReplicationQueueSystem::TotalPendingCount() const {
    size_t total = 0;
    for (const auto& observer : mObservers) total += observer.second.pending.size();
    return total;
}

ReplicationQueueStats ReplicationQueueSystem::StatsFor(int32_t observerPlayerId) const {
    const auto observer = mObservers.find(observerPlayerId);
    return observer == mObservers.end() ? ReplicationQueueStats{} : observer->second.stats;
}

void ReplicationQueueSystem::Reset() {
    mObservers.clear();
}

size_t ReplicationQueueSystem::PriorityIndex(ReplicationPriority priority) {
    switch (priority) {
        case ReplicationPriority::High: return 0;
        case ReplicationPriority::Normal: return 1;
        case ReplicationPriority::Low: return 2;
        case ReplicationPriority::Critical: break;
    }
    return 0;
}

std::vector<ReplicationStreamKey> ReplicationQueueSystem::OrderedKeys(
    const ObserverQueue& observer, ReplicationPriority priority) {
    std::vector<ReplicationStreamKey> keys;
    for (const auto& [key, message] : observer.pending) {
        if (message.priority == priority) keys.push_back(key);
    }
    const size_t priorityIndex = PriorityIndex(priority);
    if (!observer.hasCursor[priorityIndex] || keys.size() < 2) return keys;
    const auto first = std::upper_bound(keys.begin(), keys.end(), observer.cursors[priorityIndex]);
    std::rotate(keys.begin(), first, keys.end());
    return keys;
}

} // namespace Game::Replication
