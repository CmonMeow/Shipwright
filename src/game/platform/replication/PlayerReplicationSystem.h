#pragma once

#include "ObserverVisibilityIndex.h"
#include "../simulation/PlayerSimulation.h"
#include "../simulation/SpatialGridIndex.h"

#include <cstdint>
#include <cstddef>
#include <map>
#include <vector>

namespace Game::Replication {

struct ReplicatedPlayer {
    int32_t playerId = -1;
    Simulation::EntityId entity{};
    int32_t sceneId = -1;
    Simulation::Vec3 position{};
};

enum class PlayerVisibilityAction : uint8_t {
    Enter,
    Leave,
};

struct PlayerVisibilityTransition {
    int32_t observerPlayerId = -1;
    ReplicatedPlayer subject{};
    PlayerVisibilityAction action = PlayerVisibilityAction::Enter;
};

class PlayerReplicationSystem final {
  public:
    std::vector<PlayerVisibilityTransition> Reconcile(
        const std::vector<Simulation::PlayerSnapshot>& players,
        const std::vector<int32_t>& connectedObservers,
        float visibilityRadius);

    std::vector<PlayerVisibilityTransition> RemovePlayer(int32_t playerId);
    bool IsVisible(int32_t observerPlayerId, int32_t subjectPlayerId) const;
    std::vector<int32_t> VisiblePlayerIds(int32_t observerPlayerId) const;
    std::vector<int32_t> ObserversForPlayer(int32_t subjectPlayerId) const;
    bool HasObserver(int32_t observerPlayerId) const;
    size_t VisibleCount(int32_t observerPlayerId) const;
    size_t TotalVisiblePairCount() const;
    void Reset();

  private:
    using VisiblePlayerMap = std::map<int32_t, ReplicatedPlayer>;

    Simulation::SpatialGridIndex mSpatialIndex;
    std::map<int32_t, VisiblePlayerMap> mVisibleByObserver;
    ObserverVisibilityIndex<int32_t> mObserversBySubject;
};

} // namespace Game::Replication
