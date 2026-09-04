#pragma once

#include "platform/client/ClientSessionGenerationTracker.h"

#include <cstdint>

namespace Game::Client {
class ClientGameplaySession;
class CorpsePresentationRegistry;
class RemoteFishingEntityState;
class RemotePlayerReplicaStore;
class RemoteProjectileReplicaStore;
}

namespace Game::Multiplayer {

class NativeLocalProjectileController;
class NativeProjectileRenderer;
class NativeRemotePlayerRenderer;

struct NativeClientSessionDependencies {
    NativeRemotePlayerRenderer& players;
    NativeProjectileRenderer& projectiles;
    Game::Client::RemotePlayerReplicaStore& playerReplicas;
    Game::Client::RemoteFishingEntityState& fishing;
    Game::Client::CorpsePresentationRegistry& corpses;
    Game::Client::RemoteProjectileReplicaStore& projectileReplicas;
    Game::Client::ClientGameplaySession& gameplay;
    NativeLocalProjectileController& localProjectiles;
};

// Defines the complete native/semantic state boundary for one authenticated
// transport generation. Reconnect and shutdown cannot reset a partial subset.
class NativeClientSessionLifecycle final {
  public:
    explicit NativeClientSessionLifecycle(
        NativeClientSessionDependencies dependencies);

    // Returns true when a newly established/replaced generation caused a
    // complete session reset and renderer rebind.
    bool Observe(uint64_t generation);
    void ResetTracking();

    // Called after PlayState actor teardown; never dereferences native Actor
    // pointers that the scene already destroyed.
    void DetachAfterSceneShutdown();

  private:
    void ResetEstablishedSession();

    NativeClientSessionDependencies mDependencies;
    Game::Client::ClientSessionGenerationTracker mGeneration;
};

} // namespace Game::Multiplayer
