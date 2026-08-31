#include "LocalPlayerVitals.h"
#include "../SequenceNumber.h"

namespace Game::Client {
LocalPlayerVitalsUpdate LocalPlayerVitals::Apply(
    const Simulation::PlayerSnapshot& snapshot, int32_t localPlayerId) {
    if (localPlayerId < 0 || snapshot.ownerPlayerId != localPlayerId ||
        !snapshot.entity.Valid() || snapshot.lifeEpoch == 0 ||
        snapshot.serverTick == 0 || snapshot.health > 48) {
        return LocalPlayerVitalsUpdate::Invalid;
    }
    if (mHasState) {
        if (snapshot.entity != mEntity) {
            return LocalPlayerVitalsUpdate::Invalid;
        }
        if (!Sequence::IsNewer(snapshot.lifeEpoch, mLifeEpoch) &&
            snapshot.lifeEpoch != mLifeEpoch) {
            return LocalPlayerVitalsUpdate::Stale;
        }
        if (snapshot.lifeEpoch == mLifeEpoch &&
            !Sequence::IsNewer(snapshot.serverTick, mServerTick)) {
            return LocalPlayerVitalsUpdate::Stale;
        }
    }
    mEntity = snapshot.entity;
    mLifeEpoch = snapshot.lifeEpoch;
    mServerTick = snapshot.serverTick;
    mHealth = snapshot.health;
    mHasState = true;
    return LocalPlayerVitalsUpdate::Applied;
}

void LocalPlayerVitals::Reset() {
    mEntity = {};
    mLifeEpoch = 0;
    mServerTick = 0;
    mHealth = 0;
    mHasState = false;
}

} // namespace Game::Client
