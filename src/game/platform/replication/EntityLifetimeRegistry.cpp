#include "EntityLifetimeRegistry.h"

namespace Game::Replication {

bool EntityLifetimeRegistry::Establish(int32_t ownerId, Simulation::EntityId entity) {
    if (ownerId < 0 || !entity.Valid()) return false;
    mActive[ownerId] = entity;
    return true;
}

bool EntityLifetimeRegistry::Retire(int32_t ownerId, Simulation::EntityId entity) {
    const auto active = mActive.find(ownerId);
    if (active == mActive.end() || active->second != entity) return false;
    mActive.erase(active);
    return true;
}

bool EntityLifetimeRegistry::Matches(int32_t ownerId, Simulation::EntityId entity) const {
    const auto active = mActive.find(ownerId);
    return entity.Valid() && active != mActive.end() && active->second == entity;
}

std::optional<Simulation::EntityId> EntityLifetimeRegistry::ActiveEntity(int32_t ownerId) const {
    const auto active = mActive.find(ownerId);
    return active == mActive.end() ? std::nullopt
                                   : std::optional<Simulation::EntityId>(active->second);
}

void EntityLifetimeRegistry::RemoveOwner(int32_t ownerId) {
    mActive.erase(ownerId);
}

void EntityLifetimeRegistry::Reset() {
    mActive.clear();
}

size_t EntityLifetimeRegistry::Size() const {
    return mActive.size();
}

} // namespace Game::Replication
