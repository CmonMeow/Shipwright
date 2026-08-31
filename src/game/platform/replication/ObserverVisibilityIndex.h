#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace Game::Replication {

// Incremental reverse index shared by interest-management systems. Forward
// visibility remains owned by each replication system; this index answers
// entity-to-observer fanout without rebuilding every visible pair each tick.
template <typename EntityKey>
class ObserverVisibilityIndex final {
  public:
    void Add(int32_t observerPlayerId, const EntityKey& entity) {
        if (observerPlayerId < 0) return;
        mObserversByEntity[entity].insert(observerPlayerId);
    }

    void Remove(int32_t observerPlayerId, const EntityKey& entity) {
        const auto observers = mObserversByEntity.find(entity);
        if (observers == mObserversByEntity.end()) return;
        observers->second.erase(observerPlayerId);
        if (observers->second.empty()) mObserversByEntity.erase(observers);
    }

    std::vector<int32_t> ObserversFor(const EntityKey& entity) const {
        std::vector<int32_t> result;
        const auto observers = mObserversByEntity.find(entity);
        if (observers == mObserversByEntity.end()) return result;
        result.assign(observers->second.begin(), observers->second.end());
        return result;
    }

    void Clear() { mObserversByEntity.clear(); }

  private:
    std::map<EntityKey, std::set<int32_t>> mObserversByEntity;
};

} // namespace Game::Replication
