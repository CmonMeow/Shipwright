#include "NativeLocalPlayerPresentationController.h"

#include "global.h"

namespace Game::Multiplayer {

NativeLocalPlayerPresentationController::NativeLocalPlayerPresentationController(
    const Game::Simulation::ClientPrediction& prediction,
    const Game::Client::LocalPlayerVitals& vitals,
    const Game::Client::CorpsePresentationRegistry& corpses)
    : mPrediction(prediction), mVitals(vitals), mCorpses(corpses) {
}

void NativeLocalPlayerPresentationController::ProjectBodyOwnership(
    Player* player, int32_t localPlayerId) const {
    if (!player) return;
    player->authoritativeBodyHidden =
        localPlayerId >= 0 && mVitals.HasState() &&
        mCorpses.OwnsSource(mVitals.Entity(), mVitals.LifeEpoch());
}

void NativeLocalPlayerPresentationController::Project(
    Player* player, bool sessionActive) const {
    if (!player) return;

    const auto presentation =
        Game::Client::EvaluateLocalPrimaryActionPresentation(mPrediction,
                                                              sessionActive);
    switch (presentation.state) {
        case Game::Client::LocalPrimaryActionPresentationState::Idle:
            player->primaryActionPresentation =
                PLAYER_PRIMARY_PRESENTATION_IDLE;
            break;
        case Game::Client::LocalPrimaryActionPresentationState::Active:
            player->primaryActionPresentation =
                PLAYER_PRIMARY_PRESENTATION_ACTIVE;
            break;
        case Game::Client::LocalPrimaryActionPresentationState::Unavailable:
        default:
            player->primaryActionPresentation =
                PLAYER_PRIMARY_PRESENTATION_UNAVAILABLE;
            break;
    }
    player->primaryActionProgress = presentation.progress;
}

} // namespace Game::Multiplayer
