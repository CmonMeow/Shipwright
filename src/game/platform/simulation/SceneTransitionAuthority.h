#pragma once

#include "PlayerSimulation.h"

#include <cstdint>
#include <map>
#include <optional>

namespace Game::Simulation {

enum class SceneEntryResult : uint8_t {
    Accepted,
    Rejected,
};

struct SceneEntryDecision {
    int32_t playerId = -1;
    uint32_t requestSequence = 0;
    int32_t destinationSceneId = -1;
    SceneEntryResult result = SceneEntryResult::Rejected;
    std::optional<PlayerSpawn> spawn;
    std::optional<PlayerSpawn> fallbackSpawn;
};

// Owns pure scene admission policy. Replay and incarnation admission are owned
// once by ServerIntentAdmission before ServerWorld evaluates this policy.
class SceneTransitionAuthority final {
  public:
    explicit SceneTransitionAuthority(int32_t maximumSceneCount = 4096,
                                      int32_t defaultSceneId = 110);

    bool ConfigureSpawn(const PlayerSpawn& spawn);
    bool Grant(int32_t playerId, uint32_t lifeEpoch, int32_t destinationSceneId);
    void RevokePlayer(int32_t playerId);
    std::optional<PlayerSpawn> SpawnForScene(int32_t sceneId) const;
    std::optional<PlayerSpawn> DefaultSpawn() const;
    std::optional<SceneEntryDecision> Evaluate(int32_t playerId,
                                               uint32_t requestSequence,
                                               uint32_t lifeEpoch,
                                               bool bootstrap);
    void Reset();

  private:
    int32_t mMaximumSceneCount = 4096;
    int32_t mDefaultSceneId = 110;
    std::map<int32_t, PlayerSpawn> mSpawns;
    struct TransitionGrant {
        uint32_t lifeEpoch = 0;
        int32_t destinationSceneId = -1;
    };
    std::map<int32_t, TransitionGrant> mGrants;
};

} // namespace Game::Simulation
