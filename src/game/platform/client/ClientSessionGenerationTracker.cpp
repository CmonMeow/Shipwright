#include "ClientSessionGenerationTracker.h"

namespace Game::Client {

ClientSessionGenerationUpdate ClientSessionGenerationTracker::Observe(
    uint64_t generation) {
    if (generation == 0) return ClientSessionGenerationUpdate::Invalid;
    if (!mCurrent) {
        mCurrent = generation;
        return ClientSessionGenerationUpdate::Established;
    }
    if (*mCurrent == generation) {
        return ClientSessionGenerationUpdate::Unchanged;
    }
    mCurrent = generation;
    return ClientSessionGenerationUpdate::Replaced;
}

void ClientSessionGenerationTracker::Reset() {
    mCurrent.reset();
}

bool ClientSessionGenerationTracker::RequiresStateReset(
    ClientSessionGenerationUpdate update) {
    return update == ClientSessionGenerationUpdate::Established ||
           update == ClientSessionGenerationUpdate::Replaced;
}

} // namespace Game::Client
