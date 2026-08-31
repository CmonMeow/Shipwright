#include "ProjectileLifetimeRegistry.h"

#include <tuple>

namespace Game::Replication {

bool ProjectileLogicalId::Valid() const {
    return ownerPlayerId >= 0 && projectileId > 0;
}

bool ProjectileLogicalId::operator<(const ProjectileLogicalId& other) const {
    return std::tie(ownerPlayerId, projectileId, projectileKind) <
           std::tie(other.ownerPlayerId, other.projectileId, other.projectileKind);
}

bool ProjectileLifetimeRegistry::Establish(ProjectileLogicalId logicalId,
                                           Simulation::EntityId entity) {
    if (!logicalId.Valid() || !entity.Valid()) return false;
    mActive[logicalId] = entity;
    return true;
}

bool ProjectileLifetimeRegistry::Retire(ProjectileLogicalId logicalId,
                                        Simulation::EntityId entity) {
    const auto active = mActive.find(logicalId);
    if (active == mActive.end() || active->second != entity) return false;
    mActive.erase(active);
    return true;
}

bool ProjectileLifetimeRegistry::Matches(ProjectileLogicalId logicalId,
                                         Simulation::EntityId entity) const {
    const auto active = mActive.find(logicalId);
    return logicalId.Valid() && entity.Valid() && active != mActive.end() &&
           active->second == entity;
}

std::optional<Simulation::EntityId> ProjectileLifetimeRegistry::ActiveEntity(
    ProjectileLogicalId logicalId) const {
    const auto active = mActive.find(logicalId);
    return active == mActive.end() ? std::nullopt
                                   : std::optional<Simulation::EntityId>(active->second);
}

void ProjectileLifetimeRegistry::Reset() {
    mActive.clear();
}

size_t ProjectileLifetimeRegistry::Size() const {
    return mActive.size();
}

} // namespace Game::Replication
