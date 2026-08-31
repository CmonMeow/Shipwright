#include "NativeRemoteProjectilePresentationController.h"

#include "ClientProjectilePresentationPolicy.h"

namespace SoH::Network {

NativeRemoteProjectilePresentationController::
    NativeRemoteProjectilePresentationController(
        Game::Client::RemoteProjectileReplicaStore& replicas,
        NativeProjectileRenderer& renderer)
    : mReplicas(replicas), mRenderer(renderer) {
}

void NativeRemoteProjectilePresentationController::Apply(
    const Game::Client::RemoteProjectileReplicaState& state,
    int32_t localPlayerId, double receivedSeconds) {
    const bool presentationExists = mReplicas.Find(state.entity) != nullptr;
    const auto action = ClientProjectilePresentationPolicy::Evaluate(
        state, localPlayerId, presentationExists);
    if (action == ClientProjectilePresentationAction::Ignore) return;

    auto replicaState = state;
    replicaState.active =
        action == ClientProjectilePresentationAction::Upsert;
    const auto applied = mReplicas.Apply(replicaState, receivedSeconds);
    if (!applied.Applied()) return;
    if (applied.previousEntity) mRenderer.Retire(*applied.previousEntity);
    if (applied.update ==
        Game::Client::RemoteProjectilePresentationUpdate::Retired) {
        mRenderer.Retire(applied.entity);
        return;
    }
    mRenderer.Track(applied.entity);
}

void NativeRemoteProjectilePresentationController::RetireOwner(
    int32_t ownerPlayerId) {
    for (const auto entity : mReplicas.RetireOwner(ownerPlayerId)) {
        mRenderer.Retire(entity);
    }
}

} // namespace SoH::Network
