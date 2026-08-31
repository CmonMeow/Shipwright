#include "ClientCombatPresentationPolicy.h"

namespace SoH::Network {

ClientCombatPresentationAction ClientCombatPresentationPolicy::Evaluate(
    const Game::Simulation::CombatResultEvent& event, int32_t localPlayerId,
    int32_t currentSceneId) {
    if (event.sceneId != currentSceneId) {
        return ClientCombatPresentationAction::Ignore;
    }
    if (event.result == Game::Simulation::CombatResultKind::Blocked) {
        return ClientCombatPresentationAction::BlockedImpact;
    }
    if (event.targetPlayerId != localPlayerId) {
        return ClientCombatPresentationAction::ObservedDamageImpact;
    }
    return ClientCombatPresentationAction::LocalHitReaction;
}

} // namespace SoH::Network
