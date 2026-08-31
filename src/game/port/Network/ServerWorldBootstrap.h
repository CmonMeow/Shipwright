#pragma once

#include "ServerCollisionWorld.h"
#include "../../platform/simulation/ServerWorld.h"

namespace SoH::Network {

// Owns dedicated-world static data and binds its read-only collision/water
// queries to authoritative simulation. Transport/session code does not parse
// archives or know how native world resources become server entities.
class ServerWorldBootstrap final {
  public:
    bool Initialize(Game::Simulation::ServerWorld& world);

    const ServerCollisionWorld& CollisionWorld() const { return mCollisionWorld; }

  private:
    ServerCollisionWorld mCollisionWorld;
};

} // namespace SoH::Network
