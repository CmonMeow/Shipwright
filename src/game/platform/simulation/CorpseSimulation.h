#pragma once

#include "EntityRegistry.h"
#include "PlayerSimulation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <tuple>
#include <vector>

namespace Game::Simulation {

struct CorpsePose {
    int32_t sourcePlayerId = -1;
    EntityId sourcePlayerEntity{};
    uint32_t sourceLifeEpoch = 0;
    int32_t sceneId = -1;
    int32_t roomId = -1;
    Vec3 position{};
    std::array<int16_t, 3> rotation{};
    uint8_t selectedWeapon = 0;
};

struct CorpseSnapshot {
    EntityId entity{};
    CorpsePose pose{};
};

class CorpseSimulation final {
  public:
    EntityId Create(const CorpsePose& pose);
    void Reset();
    std::vector<CorpseSnapshot> Snapshots() const;

  private:
    struct CorpseEntity { EntityId id{}; CorpsePose pose{}; };
    using SourceKey = std::tuple<uint32_t, uint32_t, uint32_t>;

    static SourceKey Source(const CorpsePose& pose);
    static CorpseSnapshot BuildSnapshot(const CorpseEntity& corpse);

    static constexpr size_t kMaximumPerScene = 99;
    EntityRegistry<CorpseEntity> mCorpses;
    std::map<int32_t, std::deque<EntityId>> mSceneOrder;
    std::map<SourceKey, EntityId> mCorpseBySource;
};

} // namespace Game::Simulation
