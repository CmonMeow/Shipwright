#pragma once

#include "RemoteProjectileInterpolation.h"
#include "RemoteProjectilePresentationRegistry.h"
#include "../simulation/ArticulatedPlayerHitRig.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Game::Client {

enum class RemoteProjectilePhase : uint8_t {
    ArrowFlying,
    ArrowStuck,
    ArrowBlocked,
};

struct RemoteProjectileReplicaState {
    Simulation::EntityId entity{};
    Replication::ProjectileLogicalId logicalId{};
    int32_t sceneId = -1;
    uint32_t sequence = 0;
    bool active = false;
    RemoteProjectilePhase phase = RemoteProjectilePhase::ArrowFlying;
    uint8_t projectileType = 0;
    Simulation::Vec3 position{};
    Simulation::Vec3 velocity{};
    int16_t rotationX = 0;
    int16_t rotationY = 0;
    int16_t rotationZ = 0;
    Simulation::ArrowBodyAttachment body{};

    bool Terminal() const {
        return (phase == RemoteProjectilePhase::ArrowStuck && body.playerId < 0) ||
               phase == RemoteProjectilePhase::ArrowBlocked;
    }
};

struct RemoteProjectileReplica {
    RemoteProjectileReplicaState state{};
    RemoteProjectileInterpolation motion;
};

// Owns one complete client-side projectile presentation lifetime: exact server
// entity, logical command correlation, latest semantic state, and interpolation.
// Native Actor pointers remain outside this protocol-independent store.
class RemoteProjectileReplicaStore final {
  public:
    RemoteProjectilePresentationApplyResult Apply(
        const RemoteProjectileReplicaState& state, double receivedSeconds);
    std::vector<Simulation::EntityId> RetireOwner(int32_t ownerPlayerId);

    const RemoteProjectileReplica* Find(Simulation::EntityId entity) const;
    RemoteProjectileReplica* FindMutable(Simulation::EntityId entity);
    const RemoteProjectileReplica* FindLogical(
        Replication::ProjectileLogicalId logicalId) const;
    const RemoteProjectilePresentationState* FindByActorHandle(int16_t actorHandle) const;
    std::optional<int16_t> ActorHandleFor(Simulation::EntityId entity) const;
    std::optional<Simulation::EntityId> EntityForActorHandle(int16_t actorHandle) const;

    void Reset();
    size_t Size() const { return mReplicas.size(); }

  private:
    static uint64_t Key(Simulation::EntityId entity);
    static bool IsSane(const RemoteProjectileReplicaState& state);

    RemoteProjectilePresentationRegistry mLifetimes;
    std::map<uint64_t, RemoteProjectileReplica> mReplicas;
};

} // namespace Game::Client
