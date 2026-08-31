#include "SceneTransitionAuthority.h"

#include <cmath>

namespace Game::Simulation {

SceneTransitionAuthority::SceneTransitionAuthority(int32_t maximumSceneCount,
                                                   int32_t defaultSceneId)
    : mMaximumSceneCount(maximumSceneCount), mDefaultSceneId(defaultSceneId) {
}

bool SceneTransitionAuthority::ConfigureSpawn(const PlayerSpawn& spawn) {
    if (mMaximumSceneCount <= 0 || spawn.sceneId < 0 || spawn.sceneId >= mMaximumSceneCount ||
        !std::isfinite(spawn.position.x) || !std::isfinite(spawn.position.y) ||
        !std::isfinite(spawn.position.z) || !std::isfinite(spawn.headingRadians)) {
        return false;
    }
    mSpawns[spawn.sceneId] = spawn;
    return true;
}

std::optional<PlayerSpawn> SceneTransitionAuthority::SpawnForScene(int32_t sceneId) const {
    const auto spawn = mSpawns.find(sceneId);
    return spawn == mSpawns.end() ? std::nullopt : std::optional<PlayerSpawn>(spawn->second);
}

std::optional<PlayerSpawn> SceneTransitionAuthority::DefaultSpawn() const {
    return SpawnForScene(mDefaultSceneId);
}

bool SceneTransitionAuthority::Grant(int32_t playerId, uint32_t lifeEpoch,
                                     int32_t destinationSceneId) {
    if (playerId < 0 || lifeEpoch == 0 || !SpawnForScene(destinationSceneId)) return false;
    mGrants.insert_or_assign(playerId,
                             TransitionGrant{ lifeEpoch, destinationSceneId });
    return true;
}

void SceneTransitionAuthority::RevokePlayer(int32_t playerId) {
    mGrants.erase(playerId);
}

std::optional<SceneEntryDecision> SceneTransitionAuthority::Evaluate(
    int32_t playerId, uint32_t requestSequence, uint32_t lifeEpoch,
    bool bootstrap) {
    if (playerId < 0 || requestSequence == 0 || lifeEpoch == 0) return std::nullopt;

    SceneEntryDecision decision{};
    decision.playerId = playerId;
    decision.requestSequence = requestSequence;
    decision.fallbackSpawn = DefaultSpawn();
    if (bootstrap) {
        decision.spawn = decision.fallbackSpawn;
    } else {
        const auto grant = mGrants.find(playerId);
        if (grant != mGrants.end()) {
            if (grant->second.lifeEpoch == lifeEpoch) {
                decision.destinationSceneId = grant->second.destinationSceneId;
                decision.spawn = SpawnForScene(decision.destinationSceneId);
            }
            mGrants.erase(grant);
        }
    }
    if (decision.spawn) decision.destinationSceneId = decision.spawn->sceneId;
    decision.result = decision.spawn ? SceneEntryResult::Accepted : SceneEntryResult::Rejected;
    return decision;
}

void SceneTransitionAuthority::Reset() {
    mSpawns.clear();
    mGrants.clear();
}

} // namespace Game::Simulation
