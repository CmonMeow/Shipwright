#pragma once

#include "../simulation/EntityId.h"

#include <cstdint>
#include <map>
#include <optional>

namespace Game::Replication {

struct ProjectileLogicalId {
    int32_t ownerPlayerId = -1;
    int32_t projectileId = 0;
    uint8_t projectileKind = 0;

    bool Valid() const;
    bool operator==(const ProjectileLogicalId& other) const = default;
    bool operator<(const ProjectileLogicalId& other) const;
};

// Tracks the exact server entity generation currently assigned to each
// logical projectile. A delayed retirement can never erase its successor.
class ProjectileLifetimeRegistry final {
  public:
    bool Establish(ProjectileLogicalId logicalId, Simulation::EntityId entity);
    bool Retire(ProjectileLogicalId logicalId, Simulation::EntityId entity);
    bool Matches(ProjectileLogicalId logicalId, Simulation::EntityId entity) const;
    std::optional<Simulation::EntityId> ActiveEntity(ProjectileLogicalId logicalId) const;
    void Reset();
    size_t Size() const;

  private:
    std::map<ProjectileLogicalId, Simulation::EntityId> mActive;
};

} // namespace Game::Replication
