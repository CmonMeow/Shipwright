#pragma once

#include "NativeLocalProjectileController.h"
#include "../../platform/client/ClientGameplaySession.h"
#include "../../platform/simulation/PlayerSimulation.h"

#include <cstdint>

struct PlayState;

namespace SoH::Network {

// Applies a server-authorized local respawn to native game presentation and
// rebases every local command producer at the new authoritative life epoch.
class NativeLocalRespawnController final {
  public:
    NativeLocalRespawnController(
        Game::Client::ClientGameplaySession& gameplay,
        NativeLocalProjectileController& projectiles);

    bool Apply(PlayState* play,
               const Game::Simulation::PlayerRespawnEvent& event,
               int32_t localPlayerId);

  private:
    Game::Client::ClientGameplaySession& mGameplay;
    NativeLocalProjectileController& mProjectiles;
};

} // namespace SoH::Network
