#pragma once

#include "EntityRegistry.h"
#include "PlayerSimulation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
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
    static CorpseSnapshot BuildSnapshot(const CorpseEntity& corpse);

    static constexpr size_t kMaximumPerScene = 99;
    EntityRegistry<CorpseEntity> mCorpses;
    std::map<int32_t, std::deque<EntityId>> mSceneOrder;
};

} // namespace Game::Simulation
