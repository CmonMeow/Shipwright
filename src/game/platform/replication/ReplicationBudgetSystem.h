#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace Game::Replication {

enum class ReplicationPriority : uint8_t {
    Critical,
    High,
    Normal,
    Low,
};

struct ReplicationBudgetConfig {
    size_t bytesPerSecond = 512 * 1024;
    std::chrono::milliseconds burstDuration{ 250 };
    std::array<uint8_t, 3> unreliableShares{ 65, 25, 10 };
};

struct ReplicationBudgetStats {
    uint64_t acceptedPackets = 0;
    uint64_t acceptedBytes = 0;
    uint64_t deferredPackets = 0;
    uint64_t deferredBytes = 0;
};

// Per-observer bandwidth isolation for disposable replication. Each priority
// owns a refillable pool, preventing a movement flood from consuming the share
// reserved for projectiles or low-rate world state. Critical/reliable traffic
// is recorded but deliberately bypasses these budgets.
class ReplicationBudgetSystem final {
  public:
    using Clock = std::chrono::steady_clock;

    explicit ReplicationBudgetSystem(ReplicationBudgetConfig config = {});

    void UpdateObservers(const std::vector<int32_t>& observers, Clock::time_point now);
    bool TryConsume(int32_t observerPlayerId, ReplicationPriority priority, size_t bytes);
    void RemoveObserver(int32_t observerPlayerId);
    ReplicationBudgetStats StatsFor(int32_t observerPlayerId) const;
    size_t ObserverCount() const;
    void Reset();

  private:
    struct ObserverBudget {
        std::array<double, 3> tokens{};
        Clock::time_point lastRefill{};
        ReplicationBudgetStats stats{};
    };

    static size_t PoolIndex(ReplicationPriority priority);
    double PoolRate(size_t pool) const;
    double PoolCapacity(size_t pool) const;
    ObserverBudget MakeObserver(Clock::time_point now) const;
    void Refill(ObserverBudget& observer, Clock::time_point now) const;

    ReplicationBudgetConfig mConfig;
    std::map<int32_t, ObserverBudget> mObservers;
};

} // namespace Game::Replication
