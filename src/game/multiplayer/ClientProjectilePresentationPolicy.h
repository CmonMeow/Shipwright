#pragma once

#include "platform/client/RemoteProjectileReplicaStore.h"

#include <cstdint>

namespace Game::Multiplayer {

enum class ClientProjectilePresentationAction : uint8_t {
    Ignore,
    Upsert,
    Retire,
};

class ClientProjectilePresentationPolicy final {
  public:
    static ClientProjectilePresentationAction Evaluate(
        const Game::Client::RemoteProjectileReplicaState& state,
        int32_t localPlayerId, bool presentationExists);
};

} // namespace Game::Multiplayer
