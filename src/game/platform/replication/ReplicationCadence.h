#pragma once

#include <cstdint>

namespace Game::Replication {

struct ReplicationCadenceDue {
    bool players = false;
    bool objectives = false;
    bool structures = false;
};

// Converts authoritative fixed-world progress into publication deadlines.
// Rendering and transport polling frequency must not alter snapshot cadence.
class ReplicationCadence final {
  public:
    ReplicationCadenceDue Advance(uint32_t worldSteps);
    void Reset();

  private:
    static constexpr uint32_t kPlayerInterval = 3;    // 20 Hz at 60 world ticks/s
    static constexpr uint32_t kWorldStateInterval = 6; // 10 Hz at 60 world ticks/s

    uint32_t mPlayerSteps = 0;
    uint32_t mObjectiveSteps = 0;
    uint32_t mStructureSteps = 0;
};

} // namespace Game::Replication
