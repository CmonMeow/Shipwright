#pragma once

#include "NativeRemotePlayerRenderer.h"

#include "platform/client/RemoteFishingEntityState.h"
#include "platform/client/RemotePlayerReplicaStore.h"

#include <cstdint>
#include <functional>

namespace Game::Multiplayer {

using RemoteOwnerRetirement = std::function<void(int32_t)>;

// Applies admitted semantic state to replica records and remote native actor
// presentation. The owning player's replica remains available for
// acknowledgement/reconciliation, but its visible body and fishing equipment
// are presented by the original native actors.
class NativeRemotePlayerPresentationController final {
  public:
    NativeRemotePlayerPresentationController(
        Game::Client::RemotePlayerReplicaStore& players,
        Game::Client::RemoteFishingEntityState& fishing,
        NativeRemotePlayerRenderer& renderer);

    void ApplyLifecycle(
        const Game::Client::RemotePlayerPresentationState& lifecycle,
        const RemoteOwnerRetirement& retireOwner);
    void ApplySnapshot(const Game::Simulation::PlayerSnapshot& snapshot,
                       double receivedSeconds);
    void ApplyFishingPresentation(
        const Game::Replication::FishingPresentationState& presentation,
        double receivedSeconds);
    void ApplyLure(const Game::Client::RemoteLureEntity& lure);
    void ApplyFish(const Game::Client::RemoteFishEntity& fish);

  private:
    void RefreshFishing(int32_t playerId, int32_t requiredSceneId = -1);

    Game::Client::RemotePlayerReplicaStore& mPlayers;
    Game::Client::RemoteFishingEntityState& mFishing;
    NativeRemotePlayerRenderer& mRenderer;
};

} // namespace Game::Multiplayer
