#pragma once

#include "ClientWorldState.h"
#include "LocalFishIntentStream.h"
#include "LocalFishingUpdateStream.h"
#include "LocalPlayerCommandStream.h"
#include "LocalPlayerVitals.h"
#include "LocalProjectileIntentStream.h"
#include "LocalSceneAdmission.h"
#include "../simulation/ClientPrediction.h"

#include <cstdint>

namespace Game::Client {

// Aggregate owner for all local gameplay state whose lifetime is a network
// session, player life, or scene. These boundaries are intentionally explicit
// so native code cannot reset an incomplete hand-maintained subset.
class ClientGameplaySession final {
  public:
    void ResetSession();
    void BeginScene();
    void BeginLife(uint32_t lifeEpoch);

    ClientWorldState& World() { return mWorld; }
    LocalFishIntentStream& FishIntents() { return mFishIntents; }
    LocalFishingUpdateStream& FishingUpdates() { return mFishingUpdates; }
    LocalPlayerCommandStream& Commands() { return mCommands; }
    LocalPlayerVitals& Vitals() { return mVitals; }
    LocalProjectileIntentStream& Projectiles() { return mProjectiles; }
    LocalSceneAdmission& Scene() { return mScene; }
    Simulation::ClientPrediction& Prediction() { return mPrediction; }

    const ClientWorldState& World() const { return mWorld; }
    const LocalPlayerVitals& Vitals() const { return mVitals; }
    const LocalSceneAdmission& Scene() const { return mScene; }
    const Simulation::ClientPrediction& Prediction() const { return mPrediction; }

  private:
    ClientWorldState mWorld;
    LocalFishIntentStream mFishIntents;
    LocalFishingUpdateStream mFishingUpdates;
    LocalPlayerCommandStream mCommands;
    LocalPlayerVitals mVitals;
    LocalProjectileIntentStream mProjectiles;
    LocalSceneAdmission mScene;
    Simulation::ClientPrediction mPrediction;
};

} // namespace Game::Client
