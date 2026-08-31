#pragma once

#include "../simulation/PlayerSimulation.h"

#include <cstdint>

namespace Game::Client {

enum class LocalPlayerVitalsUpdate : uint8_t {
    Applied,
    Stale,
    Invalid,
};

// Projects authoritative health into the local native player without giving
// Ocarina's hit routines ownership of PvP damage. Identity, incarnation, and
// server-tick ordering are retained even if transport admission changes.
class LocalPlayerVitals final {
  public:
    LocalPlayerVitalsUpdate Apply(const Simulation::PlayerSnapshot& snapshot,
                                  int32_t localPlayerId);
    void Reset();

    uint8_t Health() const { return mHealth; }
    Simulation::EntityId Entity() const { return mEntity; }
    uint32_t LifeEpoch() const { return mLifeEpoch; }
    bool HasState() const { return mHasState; }

  private:
    Simulation::EntityId mEntity{};
    uint32_t mLifeEpoch = 0;
    uint32_t mServerTick = 0;
    uint8_t mHealth = 0;
    bool mHasState = false;
};

} // namespace Game::Client
