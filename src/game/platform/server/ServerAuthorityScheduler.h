#pragma once

#include "../replication/ReplicationCadence.h"
#include "../simulation/ServerWorld.h"

#include <functional>

namespace Game::Server {

struct AuthorityPublication {
    std::function<void()> publishPlayers;
    std::function<void()> refreshPlayers;
    std::function<void()> publishObjectives;
    std::function<void()> publishStructures;
    std::function<void()> publishProjectiles;
    std::function<void()> refreshOwnedEntities;
    std::function<void()> publishFishing;
    std::function<void()> publishCombat;
    std::function<void()> publishLifeEvents;
};

// Protocol-independent fixed-step orchestration. ServerWorld decides gameplay;
// this scheduler preserves publication order and cadence without knowing about
// sockets, packets, encryption, or native presentation.
class ServerAuthorityScheduler final {
  public:
    ServerAuthorityScheduler(Simulation::ServerWorld& world,
                             AuthorityPublication publication);

    Simulation::ServerWorldUpdate Advance(Simulation::ServerWorld::Clock::time_point now);
    void Reset();

  private:
    static void Invoke(const std::function<void()>& callback);

    Simulation::ServerWorld& mWorld;
    AuthorityPublication mPublication;
    Replication::ReplicationCadence mCadence;
};

} // namespace Game::Server
