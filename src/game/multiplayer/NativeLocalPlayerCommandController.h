#pragma once

#include "platform/client/LocalPlayerCommandStream.h"

#include <cstdint>
#include <functional>

struct PlayState;

namespace Game::Multiplayer {

using LocalWeaponSelectionSender =
    std::function<bool(const Game::Client::LocalWeaponSelectionRequest&)>;

// Samples native player/input state and owns its conversion into semantic
// command-stream submissions. Runtime and protocol types stay outside.
class NativeLocalPlayerCommandController final {
  public:
    NativeLocalPlayerCommandController(
        Game::Client::LocalPlayerCommandStream& commands,
        Game::Simulation::ClientPrediction& prediction);

    void Submit(PlayState* play, uint32_t lifeEpoch, float deltaSeconds,
                const LocalWeaponSelectionSender& sendWeaponSelection,
                const Game::Client::LocalPlayerCommandSender& sendCommand);

  private:
    Game::Client::LocalPlayerCommandStream& mCommands;
    Game::Simulation::ClientPrediction& mPrediction;
};

} // namespace Game::Multiplayer
