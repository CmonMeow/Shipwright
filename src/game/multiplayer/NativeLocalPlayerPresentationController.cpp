#include "NativeLocalPlayerPresentationController.h"

#include "global.h"

namespace Game::Multiplayer {

NativeLocalPlayerPresentationController::NativeLocalPlayerPresentationController(
    const Game::Client::LocalPlayerVitals& vitals)
    : mVitals(vitals) {
}

void NativeLocalPlayerPresentationController::ProjectBodyOwnership(
    Player* player, int32_t localPlayerId) const {
    if (!player) return;
    // The owning client renders native Link exactly as offline play does.
    // Server snapshots reconcile that actor's transform and vitals, but a
    // delayed reconstructed network body must never replace its movement,
    // equipment, water, climb, or combat animation. The retained corpse owns
    // presentation only after authoritative death.
    player->authoritativeBodyHidden = localPlayerId >= 0 && mVitals.HasState() &&
                                      mVitals.Health() == 0;
}

} // namespace Game::Multiplayer
