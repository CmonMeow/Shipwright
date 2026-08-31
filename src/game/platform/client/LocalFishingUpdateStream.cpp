#include "LocalFishingUpdateStream.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Client {

LocalFishingSendDecision LocalFishingUpdateStream::Evaluate(
    const LocalFishingUpdate& update, double nowSeconds) {
    LocalFishingSendDecision decision{};
    if (update.sceneId < 0 || !std::isfinite(nowSeconds)) return decision;

    if (!mInitialized) {
        mInitialized = true;
        mPrevious = update;
        mLastPresentationSeconds = nowSeconds;
        mLastControlSeconds = nowSeconds;
        if (update.visualActive) {
            decision.presentationSequence = TakeSequence(mNextPresentationSequence);
            decision.controlSequence = TakeSequence(mNextControlSequence);
            decision.reliableControl = true;
        }
        return decision;
    }

    // A duplicate message-pump callback can carry the same wall-clock value.
    // Never let clock regression manufacture a refresh or postpone one.
    const double presentationNow = std::max(nowSeconds, mLastPresentationSeconds);
    const double controlNow = std::max(nowSeconds, mLastControlSeconds);
    const bool sceneChanged = update.sceneId != mPrevious.sceneId;
    const bool activationChanged = update.visualActive != mPrevious.visualActive;
    const bool presentationChanged = sceneChanged || activationChanged ||
                                     update.fishingState != mPrevious.fishingState;
    const bool controlChanged = sceneChanged || activationChanged ||
                                update.lureDeployed != mPrevious.lureDeployed ||
                                update.reelHeld != mPrevious.reelHeld;
    const bool lifecycleChanged = sceneChanged || activationChanged ||
                                  update.lureDeployed != mPrevious.lureDeployed;
    const bool presentationDue = update.visualActive &&
        presentationNow - mLastPresentationSeconds + 1e-9 >= kSendIntervalSeconds;
    const bool controlDue = update.visualActive &&
        controlNow - mLastControlSeconds + 1e-9 >= kSendIntervalSeconds;

    if (update.visualActive && (presentationChanged || presentationDue)) {
        decision.presentationSequence = TakeSequence(mNextPresentationSequence);
        mLastPresentationSeconds = presentationNow;
    }
    // Leaving fishing sends one explicit undeploy command. There is no need
    // to continue refreshing an inactive cosmetic stream.
    if ((update.visualActive && (controlChanged || controlDue)) ||
        (!update.visualActive && mPrevious.visualActive)) {
        decision.controlSequence = TakeSequence(mNextControlSequence);
        decision.reliableControl = lifecycleChanged || !update.visualActive;
        mLastControlSeconds = controlNow;
    }

    mPrevious = update;
    return decision;
}

void LocalFishingUpdateStream::BeginScene() {
    mPrevious = {};
    mLastPresentationSeconds = 0.0;
    mLastControlSeconds = 0.0;
    mInitialized = false;
}

void LocalFishingUpdateStream::Reset() {
    BeginScene();
    mNextPresentationSequence = 1;
    mNextControlSequence = 1;
}

uint32_t LocalFishingUpdateStream::TakeSequence(uint32_t& next) {
    const uint32_t sequence = next;
    ++next;
    if (next == 0 || next > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        next = 1;
    }
    return sequence;
}

} // namespace Game::Client
