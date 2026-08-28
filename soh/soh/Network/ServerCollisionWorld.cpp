#include "ServerCollisionWorld.h"

#include <Windows.h>
#include <zip.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace SoH::Network {
namespace {

constexpr size_t kOtrHeaderSize = 64;
constexpr uint32_t kCollisionResourceType = 0x4F434F4C; // OCOL
constexpr uint16_t kVertexIndexMask = 0x1FFF;
constexpr uint16_t kIgnoreProjectiles = 4u << 13;
constexpr int32_t kNormalWildFishParams = 400;
constexpr int32_t kLoachWildFishParams = 401;
constexpr int32_t kNormalWildFishPerWaterBox = 12;
constexpr int32_t kWildLoachesPerWaterBox = 4;
constexpr int32_t kZorasDomainScene = 0x58;

#define DEFINE_SCENE(sceneName, ...) #sceneName,
constexpr const char* kSceneNames[] = {
#include "tables/scene_table.h"
};
#undef DEFINE_SCENE

class BufferReader final {
  public:
    BufferReader(const std::vector<unsigned char>& bytes, bool bigEndian) : mBytes(bytes), mBigEndian(bigEndian) {
    }

    bool Seek(size_t offset) {
        if (offset > mBytes.size()) {
            return false;
        }
        mOffset = offset;
        return true;
    }

    bool ReadU16(uint16_t& value) {
        if (mOffset + 2 > mBytes.size()) {
            return false;
        }
        if (mBigEndian) {
            value = static_cast<uint16_t>((mBytes[mOffset] << 8) | mBytes[mOffset + 1]);
        } else {
            value = static_cast<uint16_t>(mBytes[mOffset] | (mBytes[mOffset + 1] << 8));
        }
        mOffset += 2;
        return true;
    }

    bool ReadS16(int16_t& value) {
        uint16_t raw = 0;
        if (!ReadU16(raw)) {
            return false;
        }
        value = static_cast<int16_t>(raw);
        return true;
    }

    bool ReadU32(uint32_t& value) {
        if (mOffset + 4 > mBytes.size()) {
            return false;
        }
        if (mBigEndian) {
            value = (static_cast<uint32_t>(mBytes[mOffset]) << 24) |
                    (static_cast<uint32_t>(mBytes[mOffset + 1]) << 16) |
                    (static_cast<uint32_t>(mBytes[mOffset + 2]) << 8) | mBytes[mOffset + 3];
        } else {
            value = static_cast<uint32_t>(mBytes[mOffset]) |
                    (static_cast<uint32_t>(mBytes[mOffset + 1]) << 8) |
                    (static_cast<uint32_t>(mBytes[mOffset + 2]) << 16) |
                    (static_cast<uint32_t>(mBytes[mOffset + 3]) << 24);
        }
        mOffset += 4;
        return true;
    }

    bool ReadS32(int32_t& value) {
        uint32_t raw = 0;
        if (!ReadU32(raw)) {
            return false;
        }
        value = static_cast<int32_t>(raw);
        return true;
    }

    bool Skip(size_t bytes) {
        return Seek(mOffset + bytes);
    }

  private:
    const std::vector<unsigned char>& mBytes;
    size_t mOffset = 0;
    bool mBigEndian = false;
};

std::filesystem::path ExecutableDirectory() {
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return {};
    }
    return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
}

int32_t SceneIdFromEntry(const std::string& entryName) {
    for (size_t scene = 0; scene < std::size(kSceneNames); ++scene) {
        const std::string marker = std::string("/") + kSceneNames[scene] + "CollisionHeader_";
        if (entryName.find(marker) != std::string::npos) {
            return static_cast<int32_t>(scene);
        }
    }
    return -1;
}

bool PointInsideTriangle(const ServerCollisionPoint& point, const ServerCollisionPoint& a,
                         const ServerCollisionPoint& b, const ServerCollisionPoint& c) {
    const ServerCollisionPoint v0{ c.x - a.x, c.y - a.y, c.z - a.z };
    const ServerCollisionPoint v1{ b.x - a.x, b.y - a.y, b.z - a.z };
    const ServerCollisionPoint v2{ point.x - a.x, point.y - a.y, point.z - a.z };
    const double dot00 = v0.x * v0.x + v0.y * v0.y + v0.z * v0.z;
    const double dot01 = v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
    const double dot02 = v0.x * v2.x + v0.y * v2.y + v0.z * v2.z;
    const double dot11 = v1.x * v1.x + v1.y * v1.y + v1.z * v1.z;
    const double dot12 = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    const double denominator = dot00 * dot11 - dot01 * dot01;
    if (std::fabs(denominator) < 0.000001) {
        return false;
    }
    const double inverse = 1.0 / denominator;
    const double u = (dot11 * dot02 - dot01 * dot12) * inverse;
    const double v = (dot00 * dot12 - dot01 * dot02) * inverse;
    constexpr double tolerance = 0.002;
    return u >= -tolerance && v >= -tolerance && u + v <= 1.0 + tolerance;
}

uint32_t WildHash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352D;
    value ^= value >> 15;
    value *= 0x846CA68B;
    return value ^ (value >> 16);
}

float WildRandom01(uint32_t seed) {
    return static_cast<float>(WildHash(seed) & 0x00FFFFFF) / 16777216.0f;
}

template <typename TriangleContainer>
bool WildFishSpawnHasDepth(const TriangleContainer& triangles, float x, float fishY, float z) {
    float highestFloor = -std::numeric_limits<float>::infinity();
    for (const auto& triangle : triangles) {
        if (triangle.normal.y <= 0.01f) {
            continue;
        }
        const float floorY = -(triangle.normal.x * x + triangle.normal.z * z + triangle.originDistance) /
                             triangle.normal.y;
        if (floorY > fishY + 30.0f) {
            continue;
        }
        const ServerCollisionPoint point{ x, floorY, z };
        if (PointInsideTriangle(point, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2])) {
            highestFloor = std::max(highestFloor, floorY);
        }
    }
    return std::isfinite(highestFloor) && highestFloor <= fishY - 20.0f;
}

template <typename TriangleContainer>
void AddWildFish(std::vector<ServerWildFishSpawn>& fish, const TriangleContainer& triangles,
                 int32_t sceneId, int32_t waterIndex,
                 int16_t xMinRaw, int16_t ySurfaceRaw, int16_t zMinRaw, int16_t xLengthRaw,
                 int16_t zLengthRaw, int32_t fishIndex, bool isLoach) {
    const float width = std::fabs(static_cast<float>(xLengthRaw));
    const float depth = std::fabs(static_cast<float>(zLengthRaw));
    if (width == 0.0f || depth == 0.0f) {
        return;
    }
    const float minX = xLengthRaw >= 0 ? static_cast<float>(xMinRaw)
                                       : static_cast<float>(xMinRaw + xLengthRaw);
    const float minZ = zLengthRaw >= 0 ? static_cast<float>(zMinRaw)
                                       : static_cast<float>(zMinRaw + zLengthRaw);
    const float marginX = width > 80.0f ? 40.0f : width * 0.2f;
    const float marginZ = depth > 80.0f ? 40.0f : depth * 0.2f;
    const uint32_t seed = static_cast<uint32_t>(sceneId) * 0x9E3779B9U ^
                          static_cast<uint32_t>(waterIndex + 1) * 0x85EBCA6BU ^
                          static_cast<uint32_t>(fishIndex + 1) * 0x165667B1U ^
                          (isLoach ? 0xC2B2AE35U : 0x27D4EB2FU);
    ServerWildFishSpawn spawn{};
    spawn.actorParams = isLoach ? kLoachWildFishParams : kNormalWildFishParams;
    spawn.spawnY = static_cast<float>(ySurfaceRaw) - (isLoach ? 45.0f : 25.0f);
    bool foundSpawn = false;
    for (uint32_t attempt = 0; attempt < 32; ++attempt) {
        const uint32_t candidateSeed = seed ^ (attempt * 0xD3A2646CU);
        spawn.spawnX = minX + marginX + WildRandom01(candidateSeed) * (width - marginX * 2.0f);
        spawn.spawnZ = minZ + marginZ + WildRandom01(candidateSeed ^ 0xA511E9B3U) * (depth - marginZ * 2.0f);
        if (WildFishSpawnHasDepth(triangles, spawn.spawnX, spawn.spawnY, spawn.spawnZ)) {
            foundSpawn = true;
            break;
        }
    }
    if (!foundSpawn) {
        return;
    }
    spawn.homeX = static_cast<int32_t>(std::lround(spawn.spawnX));
    spawn.homeY = static_cast<int32_t>(std::lround(spawn.spawnY));
    spawn.homeZ = static_cast<int32_t>(std::lround(spawn.spawnZ));
    spawn.minX = minX + marginX;
    spawn.maxX = minX + width - marginX;
    spawn.minZ = minZ + marginZ;
    spawn.maxZ = minZ + depth - marginZ;
    spawn.waterSurfaceY = static_cast<float>(ySurfaceRaw);
    spawn.isLoach = isLoach;
    const uint32_t lengthSeed = static_cast<uint32_t>(sceneId) * 0x9E3779B9U ^
                                static_cast<uint32_t>(static_cast<int32_t>(spawn.spawnX)) * 0x85EBCA6BU ^
                                static_cast<uint32_t>(static_cast<int32_t>(spawn.spawnZ)) * 0xC2B2AE35U ^
                                (isLoach ? 0xA511E9B3U : 0x63D83595U);
    spawn.length = (isLoach ? 42.0f : 34.0f) + WildRandom01(lengthSeed) * 18.0f;
    fish.push_back(spawn);
}

} // namespace

bool ServerCollisionWorld::LoadDefaultArchive() {
    const std::filesystem::path executableDirectory = ExecutableDirectory();
    const std::filesystem::path currentDirectory = std::filesystem::current_path();
    const std::filesystem::path candidates[] = {
        executableDirectory / "oot.o2r",
        currentDirectory / "oot.o2r",
        currentDirectory / "x64" / "Release" / "oot.o2r",
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate) && LoadArchive(candidate)) {
            return true;
        }
    }
    return false;
}

bool ServerCollisionWorld::LoadArchive(const std::filesystem::path& archivePath) {
    int zipError = 0;
    zip_t* archive = zip_open(archivePath.string().c_str(), ZIP_RDONLY, &zipError);
    if (archive == nullptr) {
        return false;
    }

    std::unordered_map<int32_t, std::vector<Triangle>> scenes;
    std::unordered_map<int32_t, std::vector<ServerWildFishSpawn>> wildFish;
    size_t triangleCount = 0;
    size_t wildFishCount = 0;
    const zip_int64_t entryCount = zip_get_num_entries(archive, 0);
    if (entryCount <= 0) {
        zip_close(archive);
        return false;
    }
    for (zip_uint64_t entryIndex = 0; entryIndex < static_cast<zip_uint64_t>(entryCount); ++entryIndex) {
        const char* entryNameRaw = zip_get_name(archive, entryIndex, ZIP_FL_ENC_GUESS);
        if (entryNameRaw == nullptr) {
            continue;
        }
        const std::string entryName(entryNameRaw);
        if (entryName.find("CollisionHeader_") == std::string::npos ||
            entryName.find("/mq/") != std::string::npos) {
            continue;
        }
        const int32_t sceneId = SceneIdFromEntry(entryName);
        if (sceneId < 0 || scenes.count(sceneId) != 0) {
            continue;
        }

        zip_stat_t stat{};
        if (zip_stat_index(archive, entryIndex, 0, &stat) != 0 || stat.size <= kOtrHeaderSize ||
            stat.size > 64u * 1024u * 1024u) {
            continue;
        }
        zip_file_t* file = zip_fopen_index(archive, entryIndex, 0);
        if (file == nullptr) {
            continue;
        }
        std::vector<unsigned char> bytes(static_cast<size_t>(stat.size));
        const zip_int64_t read = zip_fread(file, bytes.data(), bytes.size());
        zip_fclose(file);
        if (read != static_cast<zip_int64_t>(bytes.size())) {
            continue;
        }

        const bool bigEndian = bytes[0] != 0;
        BufferReader header(bytes, bigEndian);
        uint32_t resourceType = 0;
        if (!header.Seek(4) || !header.ReadU32(resourceType) || resourceType != kCollisionResourceType) {
            continue;
        }
        BufferReader reader(bytes, bigEndian);
        if (!reader.Seek(kOtrHeaderSize) || !reader.Skip(12)) {
            continue;
        }
        uint32_t vertexCount = 0;
        if (!reader.ReadU32(vertexCount) || vertexCount == 0 || vertexCount > 200000) {
            continue;
        }
        std::vector<ServerCollisionPoint> vertices;
        vertices.reserve(vertexCount);
        bool valid = true;
        for (uint32_t vertex = 0; vertex < vertexCount; ++vertex) {
            int16_t x = 0;
            int16_t y = 0;
            int16_t z = 0;
            if (!reader.ReadS16(x) || !reader.ReadS16(y) || !reader.ReadS16(z)) {
                valid = false;
                break;
            }
            vertices.push_back({ static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) });
        }
        uint32_t polygonCount = 0;
        if (!valid || !reader.ReadU32(polygonCount) || polygonCount > 400000) {
            continue;
        }
        std::vector<Triangle> triangles;
        triangles.reserve(polygonCount);
        for (uint32_t polygon = 0; polygon < polygonCount; ++polygon) {
            uint16_t flagsA = 0;
            uint16_t flagsB = 0;
            uint16_t vertexC = 0;
            int16_t normalX = 0;
            int16_t normalY = 0;
            int16_t normalZ = 0;
            int16_t originDistance = 0;
            if (!reader.Skip(sizeof(uint16_t)) || !reader.ReadU16(flagsA) || !reader.ReadU16(flagsB) ||
                !reader.ReadU16(vertexC) || !reader.ReadS16(normalX) || !reader.ReadS16(normalY) ||
                !reader.ReadS16(normalZ) || !reader.ReadS16(originDistance)) {
                valid = false;
                break;
            }
            if ((flagsA & kIgnoreProjectiles) != 0) {
                continue;
            }
            const uint16_t vertexA = flagsA & kVertexIndexMask;
            const uint16_t vertexB = flagsB & kVertexIndexMask;
            if (vertexA >= vertices.size() || vertexB >= vertices.size() || vertexC >= vertices.size()) {
                valid = false;
                break;
            }
            Triangle triangle{};
            triangle.vertices[0] = vertices[vertexA];
            triangle.vertices[1] = vertices[vertexB];
            triangle.vertices[2] = vertices[vertexC];
            constexpr float normalScale = 1.0f / 32767.0f;
            triangle.normal = { normalX * normalScale, normalY * normalScale, normalZ * normalScale };
            triangle.originDistance = static_cast<float>(originDistance);
            triangles.push_back(triangle);
        }
        uint32_t surfaceTypeCount = 0;
        uint32_t cameraDataCount = 0;
        int32_t cameraPositionCount = 0;
        int32_t waterBoxCount = 0;
        if (valid && (!reader.ReadU32(surfaceTypeCount) || surfaceTypeCount > 100000 ||
                      !reader.Skip(static_cast<size_t>(surfaceTypeCount) * 8) ||
                      !reader.ReadU32(cameraDataCount) || cameraDataCount > 100000 ||
                      !reader.Skip(static_cast<size_t>(cameraDataCount) * 8) ||
                      !reader.ReadS32(cameraPositionCount) || cameraPositionCount < 0 ||
                      cameraPositionCount > 100000 ||
                      !reader.Skip(static_cast<size_t>(cameraPositionCount) * 6) ||
                      !reader.ReadS32(waterBoxCount) || waterBoxCount < 0 || waterBoxCount > 10000)) {
            valid = false;
        }
        std::vector<ServerWildFishSpawn> sceneFish;
        for (int32_t waterIndex = 0; valid && waterIndex < waterBoxCount; ++waterIndex) {
            int16_t xMin = 0;
            int16_t ySurface = 0;
            int16_t zMin = 0;
            int16_t xLength = 0;
            int16_t zLength = 0;
            if (!reader.ReadS16(xMin) || !reader.ReadS16(ySurface) || !reader.ReadS16(zMin) ||
                !reader.ReadS16(xLength) || !reader.ReadS16(zLength) || !reader.Skip(sizeof(int32_t))) {
                valid = false;
                break;
            }
            for (int32_t fishIndex = 0; fishIndex < kNormalWildFishPerWaterBox; ++fishIndex) {
                AddWildFish(sceneFish, triangles, sceneId, waterIndex, xMin, ySurface, zMin, xLength, zLength,
                            fishIndex, false);
            }
            for (int32_t fishIndex = 0; fishIndex < kWildLoachesPerWaterBox; ++fishIndex) {
                AddWildFish(sceneFish, triangles, sceneId, waterIndex, xMin, ySurface, zMin, xLength, zLength,
                            fishIndex, true);
            }
        }
        if (valid && sceneId == kZorasDomainScene) {
            for (int32_t fishIndex = 0; fishIndex < kNormalWildFishPerWaterBox; ++fishIndex) {
                AddWildFish(sceneFish, triangles, sceneId, waterBoxCount, -348, 877, -1746, 553, 780,
                            fishIndex, false);
            }
            for (int32_t fishIndex = 0; fishIndex < kWildLoachesPerWaterBox; ++fishIndex) {
                AddWildFish(sceneFish, triangles, sceneId, waterBoxCount, -348, 877, -1746, 553, 780,
                            fishIndex, true);
            }
        }
        if (valid && !triangles.empty()) {
            triangleCount += triangles.size();
            scenes.emplace(sceneId, std::move(triangles));
            if (!sceneFish.empty()) {
                wildFishCount += sceneFish.size();
                wildFish.emplace(sceneId, std::move(sceneFish));
            }
        }
    }
    zip_close(archive);
    if (scenes.empty()) {
        return false;
    }
    for (auto& [sceneId, triangles] : scenes) {
        const auto existingScene = mScenes.find(sceneId);
        if (existingScene != mScenes.end()) {
            mTriangleCount -= existingScene->second.size();
        }
        mScenes[sceneId] = std::move(triangles);
    }
    for (const auto& sceneEntry : scenes) {
        const int32_t sceneId = sceneEntry.first;
        const auto existingFish = mWildFish.find(sceneId);
        if (existingFish != mWildFish.end()) {
            mWildFishCount -= existingFish->second.size();
            mWildFish.erase(existingFish);
        }
    }
    for (auto& [sceneId, fish] : wildFish) {
        mWildFish[sceneId] = std::move(fish);
    }
    mTriangleCount += triangleCount;
    mWildFishCount += wildFishCount;
    return true;
}

bool ServerCollisionWorld::SegmentCast(int32_t sceneId, const ServerCollisionPoint& start,
                                       const ServerCollisionPoint& end, ServerCollisionPoint& impact) const {
    const auto scene = mScenes.find(sceneId);
    if (scene == mScenes.end()) {
        return false;
    }
    float closestRatio = std::numeric_limits<float>::infinity();
    ServerCollisionPoint closest{};
    for (const Triangle& triangle : scene->second) {
        const float distanceA = triangle.normal.x * start.x + triangle.normal.y * start.y +
                                triangle.normal.z * start.z + triangle.originDistance;
        const float distanceB = triangle.normal.x * end.x + triangle.normal.y * end.y +
                                triangle.normal.z * end.z + triangle.originDistance;
        const float delta = distanceA - distanceB;
        if ((distanceA >= 0.0f && distanceB >= 0.0f) || (distanceA < 0.0f && distanceB < 0.0f) ||
            (distanceA < 0.0f && distanceB > 0.0f) || std::fabs(delta) < 0.00001f) {
            continue;
        }
        const float ratio = distanceA / delta;
        if (ratio < 0.0f || ratio > 1.0f || ratio >= closestRatio) {
            continue;
        }
        const ServerCollisionPoint point{ start.x + (end.x - start.x) * ratio,
                                          start.y + (end.y - start.y) * ratio,
                                          start.z + (end.z - start.z) * ratio };
        if (!PointInsideTriangle(point, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2])) {
            continue;
        }
        closestRatio = ratio;
        closest = point;
    }
    if (!std::isfinite(closestRatio)) {
        return false;
    }
    impact = closest;
    return true;
}

bool ServerCollisionWorld::ValidateLoadedGeometry() const {
    for (const auto& sceneEntry : mScenes) {
        const int32_t sceneId = sceneEntry.first;
        if (ValidateSceneGeometry(sceneId)) {
            return true;
        }
    }
    return false;
}

bool ServerCollisionWorld::ValidateSceneGeometry(int32_t sceneId) const {
    const auto scene = mScenes.find(sceneId);
    if (scene == mScenes.end()) {
        return false;
    }
    for (const Triangle& triangle : scene->second) {
        const ServerCollisionPoint center{
            (triangle.vertices[0].x + triangle.vertices[1].x + triangle.vertices[2].x) / 3.0f,
            (triangle.vertices[0].y + triangle.vertices[1].y + triangle.vertices[2].y) / 3.0f,
            (triangle.vertices[0].z + triangle.vertices[1].z + triangle.vertices[2].z) / 3.0f,
        };
        const ServerCollisionPoint start{ center.x + triangle.normal.x * 20.0f,
                                          center.y + triangle.normal.y * 20.0f,
                                          center.z + triangle.normal.z * 20.0f };
        const ServerCollisionPoint end{ center.x - triangle.normal.x * 20.0f,
                                        center.y - triangle.normal.y * 20.0f,
                                        center.z - triangle.normal.z * 20.0f };
        ServerCollisionPoint impact{};
        if (SegmentCast(sceneId, start, end, impact)) {
            return true;
        }
    }
    return false;
}

const ServerWildFishSpawn* ServerCollisionWorld::FindWildFish(int32_t sceneId, int32_t actorParams,
                                                               int32_t homeX, int32_t homeY,
                                                               int32_t homeZ) const {
    const auto scene = mWildFish.find(sceneId);
    if (scene == mWildFish.end()) {
        return nullptr;
    }
    for (const ServerWildFishSpawn& fish : scene->second) {
        if (fish.actorParams == actorParams && fish.homeX == homeX && fish.homeY == homeY && fish.homeZ == homeZ) {
            return &fish;
        }
    }
    return nullptr;
}

size_t ServerCollisionWorld::SceneCount() const {
    return mScenes.size();
}

size_t ServerCollisionWorld::TriangleCount() const {
    return mTriangleCount;
}

size_t ServerCollisionWorld::WildFishCount() const {
    return mWildFishCount;
}

} // namespace SoH::Network
