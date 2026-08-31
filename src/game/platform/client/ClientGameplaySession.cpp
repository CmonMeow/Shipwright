#include "ClientGameplaySession.h"

namespace Game::Client {

void ClientGameplaySession::ResetSession() {
    mWorld.Reset();
    mFishIntents.Reset();
    mFishingUpdates.Reset();
    mCommands.Reset();
    mVitals.Reset();
    mProjectiles.Reset();
    mScene.Reset();
    mPrediction.Reset();
}

void ClientGameplaySession::BeginScene() {
    mFishingUpdates.BeginScene();
    mProjectiles.BeginScene();
}

void ClientGameplaySession::BeginLife(uint32_t lifeEpoch) {
    mFishIntents.Reset();
    mFishingUpdates.Reset();
    mCommands.BeginLife();
    mVitals.Reset();
    mProjectiles.Reset();
    mScene.ObserveLifeEpoch(lifeEpoch);
    mPrediction.Reset(lifeEpoch);
}

} // namespace Game::Client
