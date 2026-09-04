#pragma once

#include "NativeProjectileRenderer.h"
#include "platform/client/RemoteProjectileReplicaStore.h"

#include <cstdint>

namespace Game::Multiplayer {

// Owns the semantic replica-to-native transition for remote projectiles.
// Runtime polling and local predicted projectile actors remain outside.
class NativeRemoteProjectilePresentationController final {
  public:
    NativeRemoteProjectilePresentationController(
        Game::Client::RemoteProjectileReplicaStore& replicas,
        NativeProjectileRenderer& renderer);

    void Apply(const Game::Client::RemoteProjectileReplicaState& state,
               int32_t localPlayerId, double receivedSeconds);
    void RetireOwner(int32_t ownerPlayerId);

  private:
    Game::Client::RemoteProjectileReplicaStore& mReplicas;
    NativeProjectileRenderer& mRenderer;
};

} // namespace Game::Multiplayer
