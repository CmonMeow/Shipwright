#include "LocalFishIntentStream.h"

namespace Game::Client {

std::optional<LocalFishIntent> LocalFishIntentStream::BeginHook() {
    if (mHookActive || mPending) return std::nullopt;
    const uint32_t sequence = TakeSequence();
    mPending = PendingIntent{ sequence, mHookActive };
    mHookActive = true;
    return LocalFishIntent{
        sequence, { LocalFishIntentAction::Hook }
    };
}

std::optional<LocalFishIntent> LocalFishIntentStream::EndHook() {
    if (!mHookActive || mPending) return std::nullopt;
    const uint32_t sequence = TakeSequence();
    mPending = PendingIntent{ sequence, mHookActive };
    mHookActive = false;
    return LocalFishIntent{
        sequence, { LocalFishIntentAction::Release }
    };
}

bool LocalFishIntentStream::Resolve(uint32_t sequence, bool submitted) {
    if (!mPending || mPending->sequence != sequence) return false;
    if (!submitted) mHookActive = mPending->previousHookActive;
    mPending.reset();
    return true;
}

void LocalFishIntentStream::Reset() {
    mNextSequence = 1;
    mHookActive = false;
    mPending.reset();
}

uint32_t LocalFishIntentStream::TakeSequence() {
    const uint32_t sequence = mNextSequence++;
    if (mNextSequence == 0) mNextSequence = 1;
    return sequence;
}

} // namespace Game::Client
