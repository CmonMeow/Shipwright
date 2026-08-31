#include "ReplicationBudgetSystem.h"

#include <algorithm>
#include <set>

namespace Game::Replication {

ReplicationBudgetSystem::ReplicationBudgetSystem(ReplicationBudgetConfig config)
    : mConfig(config) {
    if (mConfig.bytesPerSecond == 0) mConfig.bytesPerSecond = 1;
    if (mConfig.burstDuration <= std::chrono::milliseconds::zero()) {
        mConfig.burstDuration = std::chrono::milliseconds(1);
    }
    const uint32_t shareTotal = static_cast<uint32_t>(mConfig.unreliableShares[0]) +
                                static_cast<uint32_t>(mConfig.unreliableShares[1]) +
                                static_cast<uint32_t>(mConfig.unreliableShares[2]);
    if (shareTotal != 100 || mConfig.unreliableShares[0] == 0 ||
        mConfig.unreliableShares[1] == 0 || mConfig.unreliableShares[2] == 0) {
        mConfig.unreliableShares = { 65, 25, 10 };
    }
}

void ReplicationBudgetSystem::UpdateObservers(const std::vector<int32_t>& observers,
                                              Clock::time_point now) {
    const std::set<int32_t> active(observers.begin(), observers.end());
    for (auto observer = mObservers.begin(); observer != mObservers.end();) {
        if (active.count(observer->first) == 0) {
            observer = mObservers.erase(observer);
        } else {
            Refill(observer->second, now);
            ++observer;
        }
    }
    for (const int32_t observerId : active) {
        if (observerId < 0) continue;
        mObservers.try_emplace(observerId, MakeObserver(now));
    }
}

bool ReplicationBudgetSystem::TryConsume(int32_t observerPlayerId,
                                         ReplicationPriority priority, size_t bytes) {
    if (observerPlayerId < 0 || bytes == 0) return false;
    auto observer = mObservers.find(observerPlayerId);
    if (observer == mObservers.end()) {
        observer = mObservers.emplace(observerPlayerId, MakeObserver(Clock::now())).first;
    }
    if (priority == ReplicationPriority::Critical) {
        ++observer->second.stats.acceptedPackets;
        observer->second.stats.acceptedBytes += bytes;
        return true;
    }

    double& tokens = observer->second.tokens[PoolIndex(priority)];
    if (tokens + 0.0001 < static_cast<double>(bytes)) {
        ++observer->second.stats.deferredPackets;
        observer->second.stats.deferredBytes += bytes;
        return false;
    }
    tokens -= static_cast<double>(bytes);
    ++observer->second.stats.acceptedPackets;
    observer->second.stats.acceptedBytes += bytes;
    return true;
}

void ReplicationBudgetSystem::RemoveObserver(int32_t observerPlayerId) {
    mObservers.erase(observerPlayerId);
}

ReplicationBudgetStats ReplicationBudgetSystem::StatsFor(int32_t observerPlayerId) const {
    const auto observer = mObservers.find(observerPlayerId);
    return observer == mObservers.end() ? ReplicationBudgetStats{} : observer->second.stats;
}

size_t ReplicationBudgetSystem::ObserverCount() const {
    return mObservers.size();
}

void ReplicationBudgetSystem::Reset() {
    mObservers.clear();
}

size_t ReplicationBudgetSystem::PoolIndex(ReplicationPriority priority) {
    switch (priority) {
        case ReplicationPriority::High: return 0;
        case ReplicationPriority::Normal: return 1;
        case ReplicationPriority::Low: return 2;
        case ReplicationPriority::Critical: break;
    }
    return 0;
}

double ReplicationBudgetSystem::PoolRate(size_t pool) const {
    return static_cast<double>(mConfig.bytesPerSecond) *
           static_cast<double>(mConfig.unreliableShares[pool]) / 100.0;
}

double ReplicationBudgetSystem::PoolCapacity(size_t pool) const {
    const double burstSeconds =
        static_cast<double>(mConfig.burstDuration.count()) / 1000.0;
    return (std::max)(1.0, PoolRate(pool) * burstSeconds);
}

ReplicationBudgetSystem::ObserverBudget ReplicationBudgetSystem::MakeObserver(
    Clock::time_point now) const {
    ObserverBudget observer{};
    for (size_t pool = 0; pool < observer.tokens.size(); ++pool) {
        observer.tokens[pool] = PoolCapacity(pool);
    }
    observer.lastRefill = now;
    return observer;
}

void ReplicationBudgetSystem::Refill(ObserverBudget& observer, Clock::time_point now) const {
    if (now <= observer.lastRefill) return;
    const double elapsed = std::chrono::duration<double>(now - observer.lastRefill).count();
    for (size_t pool = 0; pool < observer.tokens.size(); ++pool) {
        observer.tokens[pool] = (std::min)(PoolCapacity(pool),
                                           observer.tokens[pool] + PoolRate(pool) * elapsed);
    }
    observer.lastRefill = now;
}

} // namespace Game::Replication
