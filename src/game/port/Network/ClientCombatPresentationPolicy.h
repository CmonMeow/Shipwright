#pragma once

#include "../../platform/simulation/PlayerSimulation.h"

#include <cstdint>

namespace SoH::Network {

enum class ClientCombatPresentationAction : uint8_t {
    Ignore,
    BlockedImpact,
    LocalHitReaction,
    ObservedDamageImpact,
};

class ClientCombatPresentationPolicy final {
  public:
    static ClientCombatPresentationAction Evaluate(
        const Game::Simulation::CombatResultEvent& event, int32_t localPlayerId,
        int32_t currentSceneId);
};

} // namespace SoH::Network
