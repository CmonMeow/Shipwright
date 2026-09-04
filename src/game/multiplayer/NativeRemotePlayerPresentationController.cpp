#include "NativeRemotePlayerPresentationController.h"

namespace Game::Multiplayer {

namespace {

constexpr uint8_t kFishingPoleItemAction = 2;

} // namespace

NativeRemotePlayerPresentationController::NativeRemotePlayerPresentationController(
    Game::Client::RemotePlayerReplicaStore& players,
    Game::Client::RemoteFishingEntityState& fishing,
    NativeRemotePlayerRenderer& renderer)
    : mPlayers(players), mFishing(fishing), mRenderer(renderer) {
}

void NativeRemotePlayerPresentationController::ApplyLifecycle(
    const Game::Client::RemotePlayerPresentationState& lifecycle,
    int32_t localPlayerId, const RemoteOwnerRetirement& retireOwner) {
    if (lifecycle.playerId == localPlayerId) return;
    const auto* previous = mPlayers.FindPlayer(lifecycle.playerId);
    const bool sceneChanged = previous && previous->lifetime.sceneId != lifecycle.sceneId;
    const auto applied = mPlayers.ApplyLifecycle(lifecycle);
    if (!applied.Applied()) return;

    const auto retirePresentations = [&]() {
        mFishing.RemoveOwner(lifecycle.playerId);
        if (retireOwner) retireOwner(lifecycle.playerId);
    };
    if (applied.previousEntity) {
        mRenderer.RetirePlayer(*applied.previousEntity);
    }
    if (applied.previousEntity || sceneChanged) retirePresentations();
    if (applied.update ==
        Game::Client::RemotePlayerPresentationUpdate::Retired) {
        mRenderer.RetirePlayer(applied.entity);
        retirePresentations();
        return;
    }

    if (sceneChanged) mRenderer.ResetFishingVisuals(applied.entity);

    NativePlayerPresentationState initial{};
    initial.playerId = lifecycle.playerId;
    initial.sceneId = lifecycle.sceneId;
    initial.roomId = -1;
    initial.stateFlags = NATIVE_PLAYER_VISIBLE;
    mRenderer.UpsertPlayer(applied.entity, initial);
}

void NativeRemotePlayerPresentationController::ApplySnapshot(
    const Game::Simulation::PlayerSnapshot& snapshot,
    double receivedSeconds) {
    if (!mPlayers.ApplySnapshot(snapshot, receivedSeconds)) return;
    NativePlayerPresentationState* state = mRenderer.FindPlayer(snapshot.entity);
    if (!state) return;

    const bool fishingPoleWasActive =
        state->itemAction == kFishingPoleItemAction;
    state->playerId = snapshot.ownerPlayerId;
    state->sceneId = snapshot.sceneId;
    state->roomId = -1;
    state->stateFlags |= NATIVE_PLAYER_VISIBLE;
    NativePlayerPresentationComposer::ApplySnapshot(*state, snapshot,
                                                     receivedSeconds);
    NativePlayerPresentationComposer::ApplyAuthoritativeFishing(
        *state, mFishing);
    if (!fishingPoleWasActive ||
        state->itemAction != kFishingPoleItemAction) {
        mRenderer.ResetFishingVisuals(snapshot.entity);
    }
    mRenderer.MarkPlayerReady(snapshot.entity);
}

void NativeRemotePlayerPresentationController::ApplyFishingPresentation(
    const Game::Replication::FishingPresentationState& presentation,
    double receivedSeconds) {
    mPlayers.ApplyFishing(presentation, receivedSeconds);
}

void NativeRemotePlayerPresentationController::ApplyLure(
    const Game::Client::RemoteLureEntity& lure) {
    mFishing.ApplyLure(lure);
    RefreshFishing(lure.ownerPlayerId, lure.sceneId);
}

void NativeRemotePlayerPresentationController::ApplyFish(
    const Game::Client::RemoteFishEntity& fish) {
    mFishing.ApplyFish(fish);
    RefreshFishing(fish.ownerPlayerId);
}

void NativeRemotePlayerPresentationController::RefreshFishing(
    int32_t playerId, int32_t requiredSceneId) {
    NativePlayerPresentationState* state = mRenderer.FindPlayer(playerId);
    if (!state || !mRenderer.IsPlayerReady(playerId) ||
        (requiredSceneId >= 0 && state->sceneId != requiredSceneId)) {
        return;
    }
    NativePlayerPresentationComposer::ApplyAuthoritativeFishing(
        *state, mFishing);
}

} // namespace Game::Multiplayer
