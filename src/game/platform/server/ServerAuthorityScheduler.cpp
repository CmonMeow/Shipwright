#include "ServerAuthorityScheduler.h"

#include <utility>

namespace Game::Server {

ServerAuthorityScheduler::ServerAuthorityScheduler(
    Simulation::ServerWorld& world, AuthorityPublication publication)
    : mWorld(world), mPublication(std::move(publication)) {
}

void ServerAuthorityScheduler::Invoke(const std::function<void()>& callback) {
    if (callback) callback();
}

Simulation::ServerWorldUpdate ServerAuthorityScheduler::Advance(
    Simulation::ServerWorld::Clock::time_point now) {
    const Simulation::ServerWorldUpdate update = mWorld.Advance(now);
    const Replication::ReplicationCadenceDue due =
        mCadence.Advance(update.worldSteps);

    bool combatPublished = false;
    if (update.playerSteps != 0) {
        Invoke(due.players ? mPublication.publishPlayers
                           : mPublication.refreshPlayers);
        Invoke(mPublication.publishCombat);
        combatPublished = true;
    }
    if (due.objectives) Invoke(mPublication.publishObjectives);
    if (due.structures) Invoke(mPublication.publishStructures);
    if (update.worldSteps != 0) {
        Invoke(mPublication.publishProjectiles);
        Invoke(mPublication.refreshOwnedEntities);
        Invoke(mPublication.publishFishing);
    }
    // Odd 60 Hz world ticks can generate projectile combat without a 30 Hz
    // player step. Drain combat once per host update, preserving its earlier
    // ordering on player ticks without the previous redundant second drain.
    if (!combatPublished) Invoke(mPublication.publishCombat);
    Invoke(mPublication.publishLifeEvents);
    return update;
}

void ServerAuthorityScheduler::Reset() {
    mCadence.Reset();
}

} // namespace Game::Server
