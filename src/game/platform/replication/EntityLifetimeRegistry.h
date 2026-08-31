#pragma once

#include "../simulation/EntityId.h"

#include <cstdint>
#include <map>
#include <optional>

namespace Game::Replication {

// Client-side registry of exact server entity lifetimes. Reliable lifecycle
// messages establish/retire entries; snapshots and semantic events may only
// reference a currently matching generation.
class EntityLifetimeRegistry final {
  public:
    bool Establish(int32_t ownerId, Simulation::EntityId entity);
    bool Retire(int32_t ownerId, Simulation::EntityId entity);
    bool Matches(int32_t ownerId, Simulation::EntityId entity) const;
    std::optional<Simulation::EntityId> ActiveEntity(int32_t ownerId) const;
    void RemoveOwner(int32_t ownerId);
    void Reset();
    size_t Size() const;

  private:
    std::map<int32_t, Simulation::EntityId> mActive;
};

} // namespace Game::Replication
