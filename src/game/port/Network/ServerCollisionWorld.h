#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace SoH::Network {

struct ServerCollisionPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ServerWildFishSpawn {
    int32_t sceneId = -1;
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
    bool FindWaterSurface(int32_t sceneId, const ServerCollisionPoint& position, float& surfaceY) const;
    bool HasScene(int32_t sceneId) const;
    std::vector<ServerWildFishSpawn> WildFishSpawns() const;
    bool ValidateLoadedGeometry() const;
    bool ValidateSceneGeometry(int32_t sceneId) const;
    bool ValidateSpatialIndex(int32_t sceneId) const;
    size_t SceneCount() const;
    size_t TriangleCount() const;
    size_t SpatialCellCount() const;
    size_t UnindexedTriangleCount() const;
    size_t WildFishCount() const;

  private:
    struct Triangle {
        ServerCollisionPoint vertices[3];
        ServerCollisionPoint normal;
        float originDistance = 0.0f;
    };

    using SpatialCell = std::tuple<int32_t, int32_t, int32_t>;
    struct SpatialIndex {
        std::map<SpatialCell, std::vector<uint32_t>> cells;
        std::vector<uint32_t> unindexedTriangles;
    };

    static int32_t SpatialCoordinate(float value);
    static SpatialIndex BuildSpatialIndex(const std::vector<Triangle>& triangles);
    std::vector<uint32_t> SegmentCandidates(int32_t sceneId,
                                            const ServerCollisionPoint& start,
                                            const ServerCollisionPoint& end) const;
    bool SegmentCastBruteForce(int32_t sceneId, const ServerCollisionPoint& start,
                               const ServerCollisionPoint& end,
                               ServerCollisionPoint& impact) const;
    static bool SegmentCastTriangles(const std::vector<Triangle>& triangles,
                                     const std::vector<uint32_t>& candidates,
                                     const ServerCollisionPoint& start,
                                     const ServerCollisionPoint& end,
                                     ServerCollisionPoint& impact);

    std::unordered_map<int32_t, std::vector<Triangle>> mScenes;
    std::unordered_map<int32_t, SpatialIndex> mSpatialIndices;
    std::unordered_map<int32_t, std::vector<ServerWildFishSpawn>> mWildFish;
    size_t mTriangleCount = 0;
    size_t mWildFishCount = 0;
};

} // namespace SoH::Network
