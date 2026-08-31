#include "LocalProjectileIntentStream.h"

namespace Game::Client {

LocalProjectileIntentStream::LocalProjectileIntentStream(
    uint32_t nextSequence)
    : mNextSequence(nextSequence != 0 ? nextSequence : 1) {
}

bool LocalProjectileIntentStream::BindPresentation(
    const LocalProjectilePresentation& presentation) {
    if (!IsSane(presentation)) return false;
    auto found = mRecords.find(presentation.presentationId);
    if (found == mRecords.end() ||
        found->second.presentation.sceneId != presentation.sceneId) {
        ReplaceRecord(presentation.presentationId, presentation);
    } else {
        found->second.presentation = presentation;
    }
    return true;
}

bool LocalProjectileIntentStream::RequestArrowFire(
    LocalProjectilePresentationId presentationId, int32_t sceneId) {
    const auto found = mRecords.find(presentationId);
    if (found == mRecords.end() || sceneId < 0 || sceneId >= 4096 ||
        found->second.presentation.sceneId != sceneId ||
        found->second.arrowFireRequested || found->second.arrowFireSubmitted) {
        return false;
    }
    found->second.arrowFireRequested = true;
    return true;
}

bool LocalProjectileIntentStream::Retire(
    LocalProjectilePresentationId presentationId) {
    const auto found = mRecords.find(presentationId);
    if (found == mRecords.end()) return false;
    if (mPending && mPending->presentationId == presentationId) {
        mPending.reset();
    }
    for (auto awaiting = mAwaitingResults.begin(); awaiting != mAwaitingResults.end();) {
        if (awaiting->second.presentationId == presentationId) {
            awaiting = mAwaitingResults.erase(awaiting);
        } else {
            ++awaiting;
        }
    }
    mRecords.erase(found);
    return true;
}

std::optional<LocalProjectileIntent> LocalProjectileIntentStream::NextIntent() {
    if (mPending) return mPending->intent;

    for (auto& [presentationId, record] : mRecords) {

        LocalProjectileIntent intent{};
        intent.sceneId = record.presentation.sceneId;
        if (!record.arrowFireRequested || record.arrowFireSubmitted) continue;
        intent.kind = LocalProjectileIntentKind::FireArrow;

        intent.sequence = TakeSequence();
        mPending = PendingIntent{ presentationId, intent };
        return intent;
    }
    return std::nullopt;
}

bool LocalProjectileIntentStream::Resolve(uint32_t sequence, bool sent) {
    if (!mPending || mPending->intent.sequence != sequence) return false;

    const auto found = mRecords.find(mPending->presentationId);
    if (sent && found != mRecords.end()) {
        found->second.arrowFireSubmitted = true;
        mAwaitingResults.insert_or_assign(sequence, *mPending);
    }
    mPending.reset();
    return true;
}

std::optional<LocalProjectileAuthorityResult>
LocalProjectileIntentStream::ApplyAuthorityResult(
    uint32_t sequence, int32_t projectileId,
    LocalProjectileIntentKind kind, bool accepted) {
    const auto awaiting = mAwaitingResults.find(sequence);
    if (awaiting == mAwaitingResults.end() ||
        awaiting->second.intent.kind != kind ||
        (accepted ? projectileId <= 0 : projectileId != 0)) {
        return std::nullopt;
    }
    const LocalProjectilePresentationId presentationId = awaiting->second.presentationId;
    mAwaitingResults.erase(awaiting);
    const auto record = mRecords.find(presentationId);
    if (record == mRecords.end()) return std::nullopt;
    record->second.authoritativeProjectileId = accepted ? projectileId : 0;
    return LocalProjectileAuthorityResult{ presentationId, kind, accepted };
}

void LocalProjectileIntentStream::BeginScene() {
    mRecords.clear();
    mPending.reset();
    mAwaitingResults.clear();
}

void LocalProjectileIntentStream::Reset() {
    BeginScene();
    mNextSequence = 1;
}

std::optional<LocalProjectilePresentationId>
LocalProjectileIntentStream::PresentationForProjectile(
    int32_t projectileId) const {
    for (const auto& [presentationId, record] : mRecords) {
        if (record.authoritativeProjectileId == projectileId) return presentationId;
    }
    return std::nullopt;
}

bool LocalProjectileIntentStream::IsSane(
    const LocalProjectilePresentation& presentation) {
    return presentation.presentationId != 0 && presentation.sceneId >= 0 &&
           presentation.sceneId < 4096;
}

uint32_t LocalProjectileIntentStream::TakeSequence() {
    const uint32_t sequence = mNextSequence;
    ++mNextSequence;
    if (mNextSequence == 0) mNextSequence = 1;
    return sequence;
}

LocalProjectileIntentStream::Record& LocalProjectileIntentStream::ReplaceRecord(
    LocalProjectilePresentationId presentationId,
    const LocalProjectilePresentation& presentation) {
    if (mPending && mPending->presentationId == presentationId) {
        mPending.reset();
    }
    for (auto awaiting = mAwaitingResults.begin(); awaiting != mAwaitingResults.end();) {
        if (awaiting->second.presentationId == presentationId) {
            awaiting = mAwaitingResults.erase(awaiting);
        } else {
            ++awaiting;
        }
    }
    Record record{};
    record.presentation = presentation;
    return mRecords.insert_or_assign(presentationId, record).first->second;
}

} // namespace Game::Client
