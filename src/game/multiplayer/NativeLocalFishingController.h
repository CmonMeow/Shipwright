#pragma once

#include "platform/client/LocalFishingUpdateStream.h"
#include "platform/client/LocalFishIntentStream.h"
#include "platform/replication/FishingPresentationState.h"
#include "gameplay/FishingGameplay.h"

#include <functional>
#include <optional>

struct PlayState;

namespace Game::Multiplayer {

struct NativeLocalFishingSubmission {
    std::optional<Game::Replication::FishingPresentationState> presentation;
    std::optional<Game::Client::LocalLureControlIntent> control;
};

// Converts native fishing render state and PC input into semantic client
// submissions. It knows nothing about packets, endpoints, or transport.
class NativeLocalFishingController final {
  public:
    explicit NativeLocalFishingController(
        Game::Client::LocalFishingUpdateStream& updates,
        Game::Client::LocalFishIntentStream& intents);

    NativeLocalFishingSubmission Sample(PlayState* play, double nowSeconds);
    bool SubmitAction(FishingGameplayAction action,
                      const std::function<bool(
                          const Game::Client::LocalFishIntent&)>& sender);

  private:
    Game::Client::LocalFishingUpdateStream& mUpdates;
    Game::Client::LocalFishIntentStream& mIntents;
};

} // namespace Game::Multiplayer
