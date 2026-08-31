#pragma once

#include "../../platform/simulation/PlayerSimulation.h"

#include <cstdint>

struct PlayState;

namespace SoH::Network {

// Converts an admitted authoritative combat result into native visual/audio
// feedback. It never computes damage or mutates authoritative health.
class NativeCombatPresentationController final {
  public:
    void Apply(PlayState* play,
               const Game::Simulation::CombatResultEvent& event,
               int32_t localPlayerId) const;
};

} // namespace SoH::Network
