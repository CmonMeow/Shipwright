#include "ClientProjectilePresentationPolicy.h"

namespace SoH::Network {

ClientProjectilePresentationAction ClientProjectilePresentationPolicy::Evaluate(
    const Game::Client::RemoteProjectileReplicaState& state, int32_t localPlayerId,
    bool presentationExists) {
    if (!state.active) {
        return presentationExists ? ClientProjectilePresentationAction::Retire
                                  : ClientProjectilePresentationAction::Ignore;
    }
    const bool nativeLocalPrediction = state.logicalId.ownerPlayerId == localPlayerId &&
        state.phase == Game::Client::RemoteProjectilePhase::ArrowFlying;
    if (nativeLocalPrediction) {
        return presentationExists ? ClientProjectilePresentationAction::Retire
                                  : ClientProjectilePresentationAction::Ignore;
    }
    return ClientProjectilePresentationAction::Upsert;
}

} // namespace SoH::Network
