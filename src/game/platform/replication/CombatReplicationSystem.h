#pragma once

#include "PlayerReplicationSystem.h"

#include <cstdint>
#include <vector>

namespace Game::Replication {

struct CombatReplicationBatch {
    Simulation::CombatResultEvent result{};
    std::vector<int32_t> observers;
};

// Resolves reliable combat-event relevance independently of transport. An
// event is deliverable only while its exact source/target lifetimes are still
// active; observer order is deterministic for repeatable network tests.
class CombatReplicationSystem final {
  public:
    std::vector<CombatReplicationBatch> BuildBatches(
        const std::vector<Simulation::CombatResultEvent>& results,
        const std::vector<Simulation::PlayerSnapshot>& players,
        const PlayerReplicationSystem& interest) const;
};

} // namespace Game::Replication
