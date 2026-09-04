#include "multiplayer/ServerCollisionWorld.h"
#include "multiplayer/ServerWorldBootstrap.h"

#include "multiplayer/Win32NetworkPlatform.h"

namespace {
constexpr int32_t kTest01SceneId = 0x65;
}

int main() {
    Game::Multiplayer::ServerCollisionWorld collision;
    if (!collision.LoadDefaultArchive()) {
        Error("Dedicated collision self-test: oot.o2r could not be loaded");
        return 1;
    }
    if (collision.SceneCount() != 1 || collision.TriangleCount() < 700 ||
        collision.SpatialCellCount() == 0 || collision.WildFishCount() < 4 ||
        !collision.ValidateLoadedGeometry() || !collision.ValidateSceneGeometry(kTest01SceneId) ||
        !collision.ValidateSpatialIndex(kTest01SceneId)) {
        Error("Dedicated collision self-test failed: scenes=%zu triangles=%zu cells=%zu unindexed=%zu wildFish=%zu",
              collision.SceneCount(), collision.TriangleCount(), collision.SpatialCellCount(),
              collision.UnindexedTriangleCount(), collision.WildFishCount());
        return 1;
    }
    Error("Dedicated collision self-test passed: scenes=%zu triangles=%zu cells=%zu unindexed=%zu wildFish=%zu",
          collision.SceneCount(), collision.TriangleCount(), collision.SpatialCellCount(),
          collision.UnindexedTriangleCount(), collision.WildFishCount());

    Game::Multiplayer::ServerWorldBootstrap bootstrap;
    Game::Simulation::ServerWorld world;
    constexpr size_t canonicalPondFish = 17;
    if (!bootstrap.Initialize(world) ||
        world.RegisteredFishCount() !=
            canonicalPondFish + bootstrap.CollisionWorld().WildFishCount()) {
        Error("Dedicated world bootstrap self-test failed: fish=%zu wildFish=%zu",
              world.RegisteredFishCount(),
              bootstrap.CollisionWorld().WildFishCount());
        return 2;
    }
    const size_t firstFishCount = world.RegisteredFishCount();
    world.Reset();
    if (!bootstrap.Initialize(world) ||
        world.RegisteredFishCount() != firstFishCount) {
        Error("Dedicated world restart bootstrap failed: first=%zu restarted=%zu",
              firstFishCount, world.RegisteredFishCount());
        return 3;
    }
    return 0;
}
