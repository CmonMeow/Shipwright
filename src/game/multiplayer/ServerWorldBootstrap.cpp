#include "ServerWorldBootstrap.h"

#include "platform/simulation/FishCatalog.h"

#include <runtime/log/Log.h>

namespace Game::Multiplayer {
namespace {

constexpr int32_t kFishingPondScene = 0x49;
constexpr float kFishingPondFallbackSurfaceY = -20.0f;

Game::Simulation::Vec3 SimulationPoint(const ServerCollisionPoint& point) {
    return { point.x, point.y, point.z };
}

ServerCollisionPoint CollisionPoint(const Game::Simulation::Vec3& point) {
    return { point.x, point.y, point.z };
}

} // namespace

bool ServerWorldBootstrap::Initialize(Game::Simulation::ServerWorld& world) {
    // A host can be stopped and started again in the same process. Rebuild
    // archive state transactionally instead of retaining a previous session's
    // collision/fish catalog through transport lifetime changes.
    mCollisionWorld = {};
    const bool collisionLoaded = mCollisionWorld.LoadDefaultArchive();
    if (collisionLoaded) {
        Error("Dedicated collision loaded: %zu scenes, %zu triangles, %zu authoritative wild fish",
              mCollisionWorld.SceneCount(), mCollisionWorld.TriangleCount(),
              mCollisionWorld.WildFishCount());
    } else {
        Error("Dedicated collision unavailable: authoritative arrows cannot collide with static geometry");
    }

    for (const Game::Simulation::FishDefinition& fish :
         Game::Simulation::BuildFishingPondCatalog()) {
        world.RegisterFish(fish);
    }
    for (const ServerWildFishSpawn& fish : mCollisionWorld.WildFishSpawns()) {
        Game::Simulation::FishDefinition definition{};
        definition.identity = {
            fish.sceneId,
            Game::Simulation::MakeFishSpawnKey(
                fish.sceneId, 0, fish.homeX, fish.homeY, fish.homeZ),
        };
        definition.spawnPosition = { static_cast<float>(fish.homeX),
                                     static_cast<float>(fish.homeY),
                                     static_cast<float>(fish.homeZ) };
        definition.species = fish.isLoach
                                 ? Game::Simulation::FishSpecies::HylianLoach
                                 : Game::Simulation::FishSpecies::HylianBass;
        definition.length = fish.length;
        definition.bounded = true;
        definition.minX = fish.minX;
        definition.maxX = fish.maxX;
        definition.minY = fish.waterSurfaceY - 250.0f;
        definition.maxY = fish.waterSurfaceY + 80.0f;
        definition.minZ = fish.minZ;
        definition.maxZ = fish.maxZ;
        world.RegisterFish(definition);
    }

    const auto segmentCast = [this](int32_t sceneId,
                                    const Game::Simulation::Vec3& start,
                                    const Game::Simulation::Vec3& end,
                                    Game::Simulation::Vec3& impact) {
        ServerCollisionPoint serverImpact{};
        const bool hit = mCollisionWorld.SegmentCast(
            sceneId, CollisionPoint(start), CollisionPoint(end), serverImpact);
        if (hit) impact = SimulationPoint(serverImpact);
        return hit;
    };
    world.SetPlayerCollisionQuery(segmentCast);
    world.SetPlayerCollisionSceneQuery(
        [this](int32_t sceneId) { return mCollisionWorld.HasScene(sceneId); });
    world.SetProjectileCollisionQuery(segmentCast);
    world.SetFishingCollisionQuery(segmentCast);
    const auto waterSurfaceQuery =
        [this](int32_t sceneId, const Game::Simulation::Vec3& position,
               float& surfaceY) {
            if (mCollisionWorld.FindWaterSurface(
                    sceneId, CollisionPoint(position), surfaceY)) {
                return true;
            }
            // The canonical pond catalog remains available in stripped server
            // packages. Keep that one known pond functional without inventing
            // water for any other scene.
            if (sceneId == kFishingPondScene) {
                surfaceY = kFishingPondFallbackSurfaceY;
                return true;
            }
            return false;
        };
    world.SetPlayerWaterSurfaceQuery(waterSurfaceQuery);
    world.SetFishingWaterSurfaceQuery(waterSurfaceQuery);
    return collisionLoaded;
}

} // namespace Game::Multiplayer
