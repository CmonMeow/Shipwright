#pragma once

#include "../../platform/client/RemoteFishingEntityState.h"

#include <array>
#include <cstdint>
#include <optional>

namespace Game::Client {
class RemotePlayerReplicaStore;
}

namespace SoH::Network {

class NativeRemotePlayerRenderer;

struct NativeRemoteFishPresentation {
    std::array<float, 3> position{};
    std::array<int16_t, 3> rotation{};
    std::array<int16_t, 8> limbRotation{};
    float length = 0.0f;
    bool isLoach = false;
};

// Resolves authoritative fish identity and remote-player presentation into one
// Ocarina-facing visual snapshot. Native fish actors never see player IDs,
// entity stores, interpolation state, packets, or transport.
class NativeRemoteFishPresentationController final {
  public:
    NativeRemoteFishPresentationController(
        Game::Client::RemoteFishingEntityState& fishing,
        Game::Client::RemotePlayerReplicaStore& players,
        NativeRemotePlayerRenderer& renderer);

    std::optional<NativeRemoteFishPresentation> Read(
        const Game::Client::RemoteFishIdentity& identity,
        double nowSeconds) const;

  private:
    Game::Client::RemoteFishingEntityState& mFishing;
    Game::Client::RemotePlayerReplicaStore& mPlayers;
    NativeRemotePlayerRenderer& mRenderer;
};

} // namespace SoH::Network
