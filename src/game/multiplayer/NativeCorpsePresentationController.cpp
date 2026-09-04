#include "NativeCorpsePresentationController.h"

#include "ClientPlayerActionPresentationPolicy.h"

namespace Game::Multiplayer {

NativeCorpsePresentationController::NativeCorpsePresentationController(
    Game::Client::CorpsePresentationRegistry& corpses,
    NativeRemotePlayerRenderer& renderer)
    : mCorpses(corpses), mRenderer(renderer) {
}

void NativeCorpsePresentationController::Apply(
    const Game::Client::CorpsePresentationState& corpse) {
    const auto applied = mCorpses.Apply(corpse);
    if (!applied.Applied()) return;
    if (applied.previousEntity) mRenderer.RetireCorpse(*applied.previousEntity);
    if (applied.update == Game::Client::CorpsePresentationUpdate::Retired) {
        mRenderer.RetireCorpse(applied.entity);
        return;
    }

    NativePlayerPresentationState state{};
    state.playerId = corpse.sourcePlayerId;
    state.sceneId = corpse.sceneId;
    state.roomId = corpse.roomId;
    state.x = corpse.x;
    state.y = corpse.y;
    state.z = corpse.z;
    state.rotationX = corpse.rotation[0];
    state.rotationY = corpse.rotation[1];
    state.rotationZ = corpse.rotation[2];
    NativePlayerPresentationComposer::ApplyEquipment(
        state, ClientPlayerActionPresentationPolicy::EquipmentForWeapon(
                   corpse.selectedWeapon));
    state.stateFlags = NATIVE_PLAYER_VISIBLE | NATIVE_PLAYER_DEAD;
    state.baseAnimation = ClientPlayerBaseAnimation::Dead;
    mRenderer.UpsertCorpse(applied.entity, state);
}

} // namespace Game::Multiplayer
