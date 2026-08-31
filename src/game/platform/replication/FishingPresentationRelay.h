#pragma once

#include "FishingPresentationState.h"
#include "../simulation/PlayerLoadoutPolicy.h"
#include "../simulation/PlayerSimulation.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Game::Replication {

// Stores untrusted, cosmetic fishing pose samples outside authoritative world
// simulation. Every sample is bound to an exact live player entity and is
// discarded when that player dies, changes scene, or unequips the pole.
class FishingPresentationRelay final {
  public:
    FishingPresentationUpdateResult Update(
        const FishingPresentationState& presentation,
        const Simulation::PlayerSnapshot& authoritativePlayer);
    void Reconcile(const std::vector<Simulation::PlayerSnapshot>& players);
    void RemovePlayer(int32_t playerId);
    std::optional<FishingPresentationState> ForPlayer(
        int32_t playerId) const;
    size_t Count() const;
    void Reset();

  private:
    std::map<int32_t, FishingPresentationState> mPresentations;
};

} // namespace Game::Replication
