#pragma once

#include "NativeRemotePlayerRenderer.h"
#include "platform/client/CorpsePresentationRegistry.h"

namespace Game::Multiplayer {

// Owns admitted corpse lifetime transitions and their native Link actors.
class NativeCorpsePresentationController final {
  public:
    NativeCorpsePresentationController(
        Game::Client::CorpsePresentationRegistry& corpses,
        NativeRemotePlayerRenderer& renderer);

    void Apply(const Game::Client::CorpsePresentationState& state);

  private:
    Game::Client::CorpsePresentationRegistry& mCorpses;
    NativeRemotePlayerRenderer& mRenderer;
};

} // namespace Game::Multiplayer
