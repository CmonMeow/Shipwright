#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace SoH::Network {

struct ServerCollisionPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ServerWildFishSpawn {
    int32_t actorParams = 0;
    int32_t homeX = 0;
    int32_t homeY = 0;
    int32_t homeZ = 0;
    float spawnX = 0.0f;
    float spawnY = 0.0f;
    float spawnZ = 0.0f;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    float waterSurfaceY = 0.0f;
    float length = 0.0f;
    bool isLoach = false;
};

class ServerCollisionWorld final {
  public:
    bool LoadDefaultArchive();
    bool LoadArchive(const std::filesystem::path& archivePath);
    bool SegmentCast(int32_t sceneId, const ServerCollisionPoint& start, const ServerCollisionPoint& end,
                     ServerCollisionPoint& impact) const;
    const ServerWildFishSpawn* FindWildFish(int32_t sceneId, int32_t actorParams, int32_t homeX,
                                             int32_t homeY, int32_t homeZ) const;
    bool ValidateLoadedGeometry() const;
    bool ValidateSceneGeometry(int32_t sceneId) const;
    size_t SceneCount() const;
    size_t TriangleCount() const;
    size_t WildFishCount() const;

  private:
    struct Triangle {
        ServerCollisionPoint vertices[3];
        ServerCollisionPoint normal;
        float originDistance = 0.0f;
    };

    std::unordered_map<int32_t, std::vector<Triangle>> mScenes;
    std::unordered_map<int32_t, std::vector<ServerWildFishSpawn>> mWildFish;
    size_t mTriangleCount = 0;
    size_t mWildFishCount = 0;
};

} // namespace SoH::Network
