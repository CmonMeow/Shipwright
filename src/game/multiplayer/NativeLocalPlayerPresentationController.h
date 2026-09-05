#pragma once

#include "platform/client/LocalPlayerVitals.h"

struct Player;

namespace Game::Multiplayer {

// Keeps native Link as the owning client's predicted presentation and hides it
// only when the authoritative retained corpse owns the dead presentation.
class NativeLocalPlayerPresentationController final {
  public:
    explicit NativeLocalPlayerPresentationController(
        const Game::Client::LocalPlayerVitals& vitals);

    void ProjectBodyOwnership(Player* player, int32_t localPlayerId) const;

  private:
    const Game::Client::LocalPlayerVitals& mVitals;
};

} // namespace Game::Multiplayer
