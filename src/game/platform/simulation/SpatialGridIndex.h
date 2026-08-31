#pragma once

#include "Vec3.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace Game::Simulation {

using SpatialIndexId = int64_t;

// Coarse scene/XZ partition shared by replication systems. Callers retain
// ownership of entity state and apply their own exact distance policy to the
// deterministic candidate list returned by this index.
class SpatialGridIndex final {
  public:
    void Update(SpatialIndexId id, int32_t sceneId, const Vec3& position);
    void Remove(SpatialIndexId id);
    std::vector<SpatialIndexId> CandidatesNear(
        int32_t sceneId, const Vec3& position, float radius) const;
    void Reset();

  private:
    struct CellKey {
        int32_t sceneId = -1;
        int32_t x = 0;
        int32_t z = 0;

        constexpr auto operator<=>(const CellKey&) const = default;
    };

    static CellKey CellFor(int32_t sceneId, const Vec3& position);

    static constexpr float kCellSize = 1000.0f;
    std::map<SpatialIndexId, CellKey> mLocations;
    std::map<CellKey, std::set<SpatialIndexId>> mCells;
};

} // namespace Game::Simulation
