#pragma once

#include "RemoteFishingPresentationInterpolation.h"
#include "RemotePlayerInterpolation.h"
#include "RemotePlayerPresentationRegistry.h"
#include "../replication/FishingPresentationState.h"
#include "../simulation/PlayerSimulation.h"

#include <cstddef>
#include <cstdint>
#include <map>

namespace Game::Client {

// Complete protocol-independent client replica for one authoritative player.
// Native Actor pointers and render resources deliberately remain outside this
// store; this class owns lifetime, latest state, and interpolation together so
// they cannot become orphaned from one another.
struct RemotePlayerReplica {
    RemotePlayerPresentationState lifetime{};
    Simulation::PlayerSnapshot snapshot{};
    RemotePlayerInterpolation motion;
    RemoteFishingPresentationInterpolation fishing;
    bool hasSnapshot = false;
};

class RemotePlayerReplicaStore final {
  public:
    RemotePlayerPresentationApplyResult ApplyLifecycle(
        const RemotePlayerPresentationState& state);
    bool ApplySnapshot(const Simulation::PlayerSnapshot& snapshot,
                       double receivedSeconds);
    bool ApplyFishing(const Replication::FishingPresentationState& state,
                      double receivedSeconds);

    const RemotePlayerReplica* Find(Simulation::EntityId entity) const;
    RemotePlayerReplica* FindMutable(Simulation::EntityId entity);
    const RemotePlayerReplica* FindPlayer(int32_t playerId) const;
    RemotePlayerReplica* FindPlayerMutable(int32_t playerId);
    const RemotePlayerPresentationState* FindByActorHandle(int16_t actorHandle) const;
    std::optional<int16_t> ActorHandleFor(Simulation::EntityId entity) const;
    std::optional<Simulation::EntityId> EntityForActorHandle(int16_t actorHandle) const;

    void Reset();
    size_t Size() const { return mReplicas.size(); }

  private:
    static uint64_t Key(Simulation::EntityId entity);
    static bool SnapshotIsSane(const Simulation::PlayerSnapshot& snapshot);

    RemotePlayerPresentationRegistry mLifetimes;
    std::map<uint64_t, RemotePlayerReplica> mReplicas;
};

} // namespace Game::Client
