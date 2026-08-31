#pragma once

#include "MultiplayerInteractionPort.h"

#include <string>

namespace Game::Client {

struct MultiplayerCommandResult {
    std::string notice;
    bool clearHistory = false;
};

// Converts user-facing slash commands into semantic multiplayer requests.
// It deliberately has no window, renderer, transport, packet, or audio
// dependencies, so command behavior can be verified without launching a game.
class MultiplayerCommandProcessor {
  public:
    explicit MultiplayerCommandProcessor(MultiplayerInteractionPort& interaction);

    MultiplayerCommandResult Execute(const std::string& command) const;

  private:
    int32_t FindPlayer(const std::string& reference) const;

    MultiplayerInteractionPort& mInteraction;
};

} // namespace Game::Client
