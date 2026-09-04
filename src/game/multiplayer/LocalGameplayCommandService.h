#pragma once

#include "platform/client/LocalFishIntentStream.h"
#include "platform/client/LocalFishingUpdateStream.h"
#include "platform/client/LocalPlayerCommandStream.h"
#include "platform/client/LocalProjectileIntentStream.h"
#include "platform/client/LocalSceneAdmission.h"
#include "platform/client/LocalStructureActionStream.h"
#include "platform/replication/FishingPresentationState.h"

#include <functional>

namespace Game::Simulation {
class ServerWorld;
}

namespace Game::Multiplayer {

class ClientSessionIngress;
class SecureTransportChannel;
class ServerGameplayCommandService;

struct LocalGameplayEndpointState {
    std::function<bool()> remoteClientActive;
    std::function<bool()> listenServerActive;
};

// Converts semantic local-player requests into either encrypted client
// transport messages or direct listen-server authority calls. This is the only
// boundary allowed to stamp the current life epoch and choose gameplay command
// reliability for locally originated actions.
class LocalGameplayCommandService final {
  public:
    LocalGameplayCommandService(
        ClientSessionIngress& clientIngress,
        SecureTransportChannel& transport,
        ServerGameplayCommandService& serverCommands,
        Game::Simulation::ServerWorld& serverWorld,
        LocalGameplayEndpointState endpointState);

    bool SubmitPlayerCommand(Game::Simulation::PlayerCommand command,
                             uint32_t expectedLifeEpoch = 0);
    bool SelectWeapon(const Game::Client::LocalWeaponSelectionRequest& request);
    bool EnterScene(const Game::Client::LocalSceneEntryRequest& request);
    bool SubmitFishingPresentation(
        const Game::Replication::FishingPresentationState& presentation);
    bool SubmitFishAction(const Game::Client::LocalFishIntent& intent);
    bool SubmitLureControl(const Game::Client::LocalLureControlIntent& intent);
    bool FireProjectile(const Game::Client::LocalProjectileIntent& intent);
    bool SubmitStructureAction(const Game::Client::LocalStructureAction& action);

  private:
    bool IsRemoteClient() const;
    bool IsListenServer() const;
    bool LocalPlayerAdmitted() const;
    uint32_t CurrentLifeEpoch() const;

    ClientSessionIngress& mClientIngress;
    SecureTransportChannel& mTransport;
    ServerGameplayCommandService& mServerCommands;
    Game::Simulation::ServerWorld& mServerWorld;
    LocalGameplayEndpointState mEndpointState;
};

} // namespace Game::Multiplayer
