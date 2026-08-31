#pragma once

#include "ReplicationBudgetSystem.h"
#include "../simulation/EntityId.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace Game::Replication {

struct ReplicationStreamKey {
    uint16_t stream = 0;
    int32_t ownerOrKey = -1;
    Simulation::EntityId entity{};
    int32_t subId = 0;

    constexpr bool Valid() const { return stream != 0 && entity.Valid(); }
    constexpr bool operator==(const ReplicationStreamKey&) const = default;
    constexpr bool operator<(const ReplicationStreamKey& other) const {
        return std::tie(stream, ownerOrKey, entity.index, entity.generation, subId) <
               std::tie(other.stream, other.ownerOrKey, other.entity.index,
                        other.entity.generation, other.subId);
    }
};

struct ReplicationQueueStats {
    uint64_t enqueued = 0;
    uint64_t coalesced = 0;
    uint64_t sent = 0;
    uint64_t sendRetries = 0;
};

class ReplicationQueueSystem final {
  public:
    using Sender = std::function<bool(int32_t observerPlayerId, const std::string& payload,
                                      bool highTransportPriority)>;

    bool Enqueue(int32_t observerPlayerId, const ReplicationStreamKey& key,
                 ReplicationPriority priority, std::string payload,
                 bool highTransportPriority);
    size_t FlushObserver(int32_t observerPlayerId, ReplicationBudgetSystem& budgets,
                         const Sender& sender, size_t maximumPackets = 256);
    size_t Flush(const std::vector<int32_t>& observers, ReplicationBudgetSystem& budgets,
                 const Sender& sender, size_t maximumPacketsPerObserver = 256);
    void UpdateObservers(const std::vector<int32_t>& observers);
    void RemoveObserver(int32_t observerPlayerId);
    size_t RemoveEntity(int32_t observerPlayerId, int32_t ownerOrKey,
                        Simulation::EntityId entity);
    size_t PendingCount(int32_t observerPlayerId) const;
    size_t TotalPendingCount() const;
    ReplicationQueueStats StatsFor(int32_t observerPlayerId) const;
    void Reset();

  private:
    struct PendingMessage {
        ReplicationPriority priority = ReplicationPriority::Normal;
        std::string payload;
        bool highTransportPriority = true;
    };

    struct ObserverQueue {
        std::map<ReplicationStreamKey, PendingMessage> pending;
        std::array<ReplicationStreamKey, 3> cursors{};
        std::array<bool, 3> hasCursor{};
        ReplicationQueueStats stats{};
    };

    static size_t PriorityIndex(ReplicationPriority priority);
    static std::vector<ReplicationStreamKey> OrderedKeys(const ObserverQueue& observer,
                                                          ReplicationPriority priority);

    std::map<int32_t, ObserverQueue> mObservers;
};

} // namespace Game::Replication
