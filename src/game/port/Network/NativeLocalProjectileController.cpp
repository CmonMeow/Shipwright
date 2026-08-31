#include "NativeLocalProjectileController.h"

#include "global.h"

namespace SoH::Network {

NativeLocalProjectileController::NativeLocalProjectileController(
    Game::Client::LocalProjectileIntentStream& intents)
    : mIntents(intents) {
}

void NativeLocalProjectileController::ResetBindings() {
    mBindings.Reset();
}

void NativeLocalProjectileController::BindPredictedArrow(Actor* actor,
                                                          int32_t sceneId) {
    if (!actor) return;
    const auto presentationId = mBindings.Observe(actor);
    mIntents.BindPresentation({ presentationId, sceneId });
}

bool NativeLocalProjectileController::CommitArrowFire(Actor* actor,
                                                       int32_t sceneId) {
    if (!actor) return false;
    const auto presentationId = mBindings.Find(actor);
    return presentationId &&
           mIntents.RequestArrowFire(*presentationId, sceneId);
}

void NativeLocalProjectileController::UnbindPredictedArrow(Actor* actor) {
    if (!actor) return;
    if (const auto presentationId = mBindings.Find(actor)) {
        mIntents.Retire(*presentationId);
    }
    mBindings.Forget(actor);
}

void NativeLocalProjectileController::ApplyAuthorityResult(
    const Game::Client::LocalProjectileIntentDecision& decision) {
    const auto resolved = mIntents.ApplyAuthorityResult(
        decision.sequence, decision.projectileId, decision.kind,
        decision.accepted);
    if (!resolved || resolved->accepted) return;
    RetirePresentation(resolved->presentationId, true);
}

void NativeLocalProjectileController::ApplyAuthoritativeState(
    const Game::Client::RemoteProjectileReplicaState& state,
    int32_t localPlayerId) {
    if (state.logicalId.ownerPlayerId != localPlayerId ||
        (state.active && !state.Terminal())) {
        return;
    }
    const auto presentationId =
        mIntents.PresentationForProjectile(state.logicalId.projectileId);
    if (presentationId) RetirePresentation(*presentationId, true);
}

void NativeLocalProjectileController::RetirePresentation(
    Game::Client::LocalProjectilePresentationId presentationId,
    bool killActor) {
    Actor* actor = mBindings.Resolve(presentationId);
    mIntents.Retire(presentationId);
    if (actor) mBindings.Forget(actor);
    if (killActor && actor && actor->update) Actor_Kill(actor);
}

} // namespace SoH::Network
