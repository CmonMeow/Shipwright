#include "SpatialGridIndex.h"

#include <algorithm>
#include <cmath>

namespace Game::Simulation {

void SpatialGridIndex::Update(SpatialIndexId id, int32_t sceneId, const Vec3& position) {
    if (id < 0 || sceneId < 0) return;

    const CellKey nextCell = CellFor(sceneId, position);
    const auto location = mLocations.find(id);
    if (location != mLocations.end()) {
        if (location->second == nextCell) return;
        auto oldCell = mCells.find(location->second);
        if (oldCell != mCells.end()) {
            oldCell->second.erase(id);
            if (oldCell->second.empty()) mCells.erase(oldCell);
        }
        location->second = nextCell;
    } else {
        mLocations.emplace(id, nextCell);
    }
    mCells[nextCell].insert(id);
}

void SpatialGridIndex::Remove(SpatialIndexId id) {
    const auto location = mLocations.find(id);
    if (location == mLocations.end()) return;
    auto cell = mCells.find(location->second);
    if (cell != mCells.end()) {
        cell->second.erase(id);
        if (cell->second.empty()) mCells.erase(cell);
    }
    mLocations.erase(location);
}

std::vector<SpatialIndexId> SpatialGridIndex::CandidatesNear(
    int32_t sceneId, const Vec3& position, float radius) const {
    std::vector<SpatialIndexId> result;
    if (sceneId < 0 || radius < 0.0f) return result;

    const CellKey center = CellFor(sceneId, position);
    const int32_t cellRadius = static_cast<int32_t>(std::ceil(radius / kCellSize));
    for (int32_t x = center.x - cellRadius; x <= center.x + cellRadius; ++x) {
        for (int32_t z = center.z - cellRadius; z <= center.z + cellRadius; ++z) {
            const auto cell = mCells.find({ sceneId, x, z });
            if (cell == mCells.end()) continue;
            result.insert(result.end(), cell->second.begin(), cell->second.end());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void SpatialGridIndex::Reset() {
    mLocations.clear();
    mCells.clear();
}

SpatialGridIndex::CellKey SpatialGridIndex::CellFor(
    int32_t sceneId, const Vec3& position) {
    return { sceneId, static_cast<int32_t>(std::floor(position.x / kCellSize)),
             static_cast<int32_t>(std::floor(position.z / kCellSize)) };
}

} // namespace Game::Simulation
