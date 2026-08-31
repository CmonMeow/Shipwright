#pragma once

#include "../replication/ProjectileLifetimeRegistry.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Game::Client {

struct RemoteProjectilePresentationState {
    Simulation::EntityId entity{};
    Replication::ProjectileLogicalId logicalId{};
    int32_t sceneId = -1;
    bool active = false;
};

enum class RemoteProjectilePresentationUpdate : uint8_t {
    Ignored,
    Established,
    Updated,
    Replaced,
    Retired,
};

struct RemoteProjectilePresentationApplyResult {
    RemoteProjectilePresentationUpdate update = RemoteProjectilePresentationUpdate::Ignored;
    Simulation::EntityId entity{};
    std::optional<Simulation::EntityId> previousEntity;
    int16_t actorHandle = 0;

    bool Applied() const { return update != RemoteProjectilePresentationUpdate::Ignored; }
};

// Owns exact authoritative projectile render lifetimes. Logical projectile
// identity remains metadata used to correlate commands; native Actor::params
// carries only a private bounded handle resolved during Actor initialization.
class RemoteProjectilePresentationRegistry final {
  public:
    RemoteProjectilePresentationApplyResult Apply(
        const RemoteProjectilePresentationState& state);

    const RemoteProjectilePresentationState* Find(Simulation::EntityId entity) const;
    const RemoteProjectilePresentationState* FindLogical(
        Replication::ProjectileLogicalId logicalId) const;
    const RemoteProjectilePresentationState* FindByActorHandle(int16_t actorHandle) const;
    std::optional<int16_t> ActorHandleFor(Simulation::EntityId entity) const;
    std::optional<Simulation::EntityId> EntityForActorHandle(int16_t actorHandle) const;
    std::vector<Simulation::EntityId> RetireOwner(int32_t ownerPlayerId);

    void Reset();
    size_t Size() const { return mEntries.size(); }

  private:
    struct Entry {
        RemoteProjectilePresentationState state{};
        int16_t actorHandle = 0;
    };

    static uint64_t Key(Simulation::EntityId entity);
    static bool IsSane(const RemoteProjectilePresentationState& state);
    int16_t AllocateActorHandle();

    std::map<uint64_t, Entry> mEntries;
    std::map<Replication::ProjectileLogicalId, uint64_t> mEntitiesByLogicalId;
    std::map<int16_t, uint64_t> mEntitiesByActorHandle;
    int32_t mNextActorHandle = 1;
};

} // namespace Game::Client
