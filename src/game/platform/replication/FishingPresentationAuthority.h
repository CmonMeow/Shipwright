#pragma once

#include "FishingPresentationState.h"
#include "../simulation/FishingSimulation.h"
#include "../simulation/PlayerSimulation.h"

#include <optional>

namespace Game::Replication {

// Constrains disposable native fishing animation telemetry to the exact
// server-owned player, lure, and fish lifetime. Clients may describe rod bend
// and hook animation, but may not choose whether a lure/fish exists, its phase,
// or the line endpoint used by other clients.
class FishingPresentationAuthority final {
  public:
    static bool Constrain(
        FishingPresentationState& presentation,
        const Simulation::PlayerSnapshot& player,
        const std::optional<Simulation::FishingLureSnapshot>& lure,
        const std::optional<Simulation::FishSnapshot>& fish);
};

} // namespace Game::Replication
