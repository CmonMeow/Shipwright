#pragma once

#include "../../platform/client/RemoteProjectileReplicaStore.h"

#include <cstdint>

namespace SoH::Network {

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

} // namespace SoH::Network
