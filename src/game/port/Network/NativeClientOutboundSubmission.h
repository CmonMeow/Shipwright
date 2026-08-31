#pragma once

#include "../Gameplay/FishingGameplay.h"

struct PlayState;

namespace Game::Client {
class ClientGameplaySession;
}

namespace SoH::Network {

class NativeLocalFishingController;
class NativeLocalPlayerCommandController;
class NativeLocalProjectileController;
class NetworkRuntime;

struct NativeClientOutboundDependencies {
    NetworkRuntime& runtime;
    Game::Client::ClientGameplaySession& gameplay;
    NativeLocalFishingController& fishing;
    NativeLocalPlayerCommandController& playerCommands;
    NativeLocalProjectileController& projectiles;
};

// Converts local semantic streams into runtime submissions. Every reliable
// stream resolves transport acceptance here so failed sends roll provisional
// state back instead of leaving native actions wedged.
class NativeClientOutboundSubmission final {
  public:
    explicit NativeClientOutboundSubmission(
        NativeClientOutboundDependencies dependencies);

    bool SubmitFishingAction(FishingGameplayAction action);
    void SubmitPlayerCommand(PlayState* play, float deltaSeconds);
    void SubmitPresentation(PlayState* play);

  private:
    void FlushProjectileIntents();

    NativeClientOutboundDependencies mDependencies;
};

} // namespace SoH::Network
