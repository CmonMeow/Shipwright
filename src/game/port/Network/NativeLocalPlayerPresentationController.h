#pragma once

#include "../../platform/client/LocalPrimaryActionPresentation.h"
#include "../../platform/client/LocalPlayerVitals.h"
#include "../../platform/client/CorpsePresentationRegistry.h"

struct Player;

namespace SoH::Network {

// Projects client prediction into native Player presentation fields. Native
// actor code consumes only those fields and remains independent of networking.
class NativeLocalPlayerPresentationController final {
  public:
    explicit NativeLocalPlayerPresentationController(
        const Game::Simulation::ClientPrediction& prediction,
        const Game::Client::LocalPlayerVitals& vitals,
        const Game::Client::CorpsePresentationRegistry& corpses);

    void Project(Player* player, bool sessionActive) const;
    void ProjectBodyOwnership(Player* player, int32_t localPlayerId) const;

  private:
    const Game::Simulation::ClientPrediction& mPrediction;
    const Game::Client::LocalPlayerVitals& mVitals;
    const Game::Client::CorpsePresentationRegistry& mCorpses;
};

} // namespace SoH::Network
