#pragma once

#include "../platform/simulation/ServerWorld.h"

namespace Game::Simulation {

// Integration and load tests occasionally need to inject low-level state or
// inspect subsystem clocks. Production code cannot use these handles and must
// cross ServerWorld's command, snapshot, and lifecycle boundary.
class ServerWorldTestAccess final {
  public:
    static PlayerSimulation& Players(ServerWorld& world) { return world.mPlayers; }
    static ProjectileSimulation& Projectiles(ServerWorld& world) { return world.mProjectiles; }
    static FishingSimulation& Fishing(ServerWorld& world) { return world.mFishing; }
    static ObjectiveSimulation& Objectives(ServerWorld& world) { return world.mObjectives; }
    static StrategicWorldTopology& StrategicTopology(ServerWorld& world) {
        return world.mStrategicTopology;
    }
    static StructureSimulation& Structures(ServerWorld& world) { return world.mStructures; }
    static CorpseSimulation& Corpses(ServerWorld& world) { return world.mCorpses; }
};

} // namespace Game::Simulation
