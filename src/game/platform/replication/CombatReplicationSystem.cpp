#include "CombatReplicationSystem.h"

#include <map>
#include <set>

namespace Game::Replication {

std::vector<CombatReplicationBatch> CombatReplicationSystem::BuildBatches(
    const std::vector<Simulation::CombatResultEvent>& results,
    const std::vector<Simulation::PlayerSnapshot>& players,
    const PlayerReplicationSystem& interest) const {
    std::vector<CombatReplicationBatch> batches;

    std::map<int32_t, const Simulation::PlayerSnapshot*> playerByOwner;
    for (const Simulation::PlayerSnapshot& player : players) {
        if (player.ownerPlayerId >= 0 && player.entity.Valid() && player.sceneId >= 0) {
            playerByOwner[player.ownerPlayerId] = &player;
        }
    }
    for (const Simulation::CombatResultEvent& result : results) {
        const auto target = playerByOwner.find(result.targetPlayerId);
        if (target == playerByOwner.end() || target->second->entity != result.targetEntity ||
            target->second->lifeEpoch != result.targetLifeEpoch ||
            target->second->sceneId != result.sceneId) {
            continue;
        }
        if (result.sourcePlayerId >= 0) {
            const auto source = playerByOwner.find(result.sourcePlayerId);
            if (source == playerByOwner.end() || source->second->entity != result.sourceEntity ||
                source->second->lifeEpoch != result.sourceLifeEpoch ||
                source->second->sceneId != result.sceneId) {
                continue;
            }
        }
        const bool validOutcome = result.eventId != 0 &&
            ((result.result == Simulation::CombatResultKind::Blocked && result.damage == 0) ||
             (result.result == Simulation::CombatResultKind::Damaged && result.damage > 0));
        if (!validOutcome) continue;

        CombatReplicationBatch batch{};
        batch.result = result;
        std::set<int32_t> observers;
        if (interest.HasObserver(result.targetPlayerId)) {
            observers.insert(result.targetPlayerId);
        }
        if (result.sourcePlayerId >= 0 &&
            interest.HasObserver(result.sourcePlayerId)) {
            observers.insert(result.sourcePlayerId);
        }
        const std::vector<int32_t> witnesses =
            interest.ObserversForPlayer(result.targetPlayerId);
        observers.insert(witnesses.begin(), witnesses.end());
        for (const int32_t observerId : observers) {
            const auto observer = playerByOwner.find(observerId);
            if (observer == playerByOwner.end() || observer->second->sceneId != result.sceneId) continue;
            batch.observers.push_back(observerId);
        }
        if (!batch.observers.empty()) batches.push_back(std::move(batch));
    }
    return batches;
}

} // namespace Game::Replication
