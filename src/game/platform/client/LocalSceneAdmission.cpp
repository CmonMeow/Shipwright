#include "LocalSceneAdmission.h"
#include "../SequenceNumber.h"

#include <cmath>

namespace Game::Client {

LocalSceneAdmission::LocalSceneAdmission(uint32_t nextSequence)
    : mNextSequence(nextSequence == 0 ? 1 : nextSequence) {
}

std::optional<LocalSceneEntryRequest> LocalSceneAdmission::Prepare(
    int32_t desiredSceneId) {
    if (desiredSceneId < 0 || desiredSceneId >= 4096) return std::nullopt;
    mDesiredSceneId = desiredSceneId;
    if (mAuthorizedSceneId == desiredSceneId || mRejectedSceneId == desiredSceneId ||
        (mOffered && mOffered->sceneId == desiredSceneId) ||
        (mPending && mPending->sceneId == desiredSceneId)) {
        return std::nullopt;
    }

    mOffered = LocalSceneEntryRequest{ TakeSequence(), desiredSceneId };
    return mOffered;
}

bool LocalSceneAdmission::ResolveTransport(uint32_t sequence, bool sent) {
    if (!mOffered || mOffered->sequence != sequence) return false;
    if (sent) {
        mPending = mOffered;
        if (mRejectedSceneId != mOffered->sceneId) mRejectedSceneId = -1;
    }
    mOffered.reset();
    return true;
}

LocalSceneAuthorityResult LocalSceneAdmission::Apply(
    const LocalSceneAuthority& authority) {
    LocalSceneAuthorityResult result{};
    if (!IsSane(authority)) return result;
    if (mLifeEpoch == 0 && authority.requestSequence == 0) {
        mLifeEpoch = authority.lifeEpoch;
    }
    if (authority.lifeEpoch != mLifeEpoch) return result;
    if (mAuthorizedEntity && authority.entity != *mAuthorizedEntity) return result;

    if (authority.requestSequence == 0) {
        // Sequence zero is reserved for the initial server bootstrap. Never
        // let a delayed bootstrap replace an in-flight explicit transition.
        if (!authority.accepted || mBootstrapApplied || mPending || mOffered) return result;
        mAuthorizedEntity = authority.entity;
        mAuthorizedSceneId = authority.sceneId;
        mPendingPlacement = authority;
        mRejectedSceneId = -1;
        mBootstrapApplied = true;
        result.kind = LocalSceneAuthorityKind::Bootstrap;
        result.state = authority;
        return result;
    }

    if (!mPending || authority.requestSequence != mPending->sequence) return result;
    const int32_t requestedSceneId = mPending->sceneId;
    if (authority.accepted &&
        (authority.sceneId != requestedSceneId || mDesiredSceneId != requestedSceneId)) {
        mPending.reset();
        return result;
    }

    mPending.reset();
    mOffered.reset();
    mAuthorizedEntity = authority.entity;
    mAuthorizedSceneId = authority.sceneId;
    mPendingPlacement = authority;
    mBootstrapApplied = true;
    result.state = authority;
    if (authority.accepted) {
        mRejectedSceneId = -1;
        result.kind = LocalSceneAuthorityKind::Accepted;
    } else {
        mRejectedSceneId = requestedSceneId;
        result.kind = LocalSceneAuthorityKind::Rejected;
    }
    return result;
}

bool LocalSceneAdmission::IsAuthorized(int32_t sceneId) const {
    return sceneId >= 0 && sceneId == mAuthorizedSceneId;
}

std::optional<int32_t> LocalSceneAdmission::AuthorizedScene() const {
    return mAuthorizedSceneId < 0 ? std::nullopt
                                  : std::optional<int32_t>(mAuthorizedSceneId);
}

std::optional<Simulation::EntityId> LocalSceneAdmission::AuthorizedEntity() const {
    return mAuthorizedEntity;
}

std::optional<uint32_t> LocalSceneAdmission::LifeEpoch() const {
    return mLifeEpoch == 0 ? std::nullopt : std::optional<uint32_t>(mLifeEpoch);
}

std::optional<int32_t> LocalSceneAdmission::PendingScene() const {
    return mPending ? std::optional<int32_t>(mPending->sceneId) : std::nullopt;
}

std::optional<int32_t> LocalSceneAdmission::PendingPlacementScene() const {
    return mPendingPlacement ? std::optional<int32_t>(mPendingPlacement->sceneId)
                             : std::nullopt;
}

std::optional<LocalSceneAuthority> LocalSceneAdmission::TakePlacement(
    int32_t loadedSceneId) {
    if (!mPendingPlacement || mPendingPlacement->sceneId != loadedSceneId) {
        return std::nullopt;
    }
    const LocalSceneAuthority placement = *mPendingPlacement;
    mPendingPlacement.reset();
    return placement;
}

bool LocalSceneAdmission::ObserveLifeEpoch(uint32_t lifeEpoch) {
    if (lifeEpoch == 0 ||
        (mLifeEpoch != 0 && !Sequence::IsNewer(lifeEpoch, mLifeEpoch))) {
        return false;
    }
    mLifeEpoch = lifeEpoch;
    mOffered.reset();
    mPending.reset();
    mPendingPlacement.reset();
    mRejectedSceneId = -1;
    return true;
}

void LocalSceneAdmission::Reset() {
    mOffered.reset();
    mPending.reset();
    mAuthorizedEntity.reset();
    mPendingPlacement.reset();
    mDesiredSceneId = -1;
    mAuthorizedSceneId = -1;
    mRejectedSceneId = -1;
    mNextSequence = 1;
    mLifeEpoch = 0;
    mBootstrapApplied = false;
}

bool LocalSceneAdmission::IsSane(const LocalSceneAuthority& authority) {
    return authority.playerId >= 0 && authority.entity.Valid() && authority.lifeEpoch != 0 &&
           authority.sceneId >= 0 && authority.sceneId < 4096 &&
           std::isfinite(authority.position.x) &&
           std::isfinite(authority.position.y) &&
           std::isfinite(authority.position.z) &&
           authority.position.x > -1000000.0f && authority.position.x < 1000000.0f &&
           authority.position.y > -1000000.0f && authority.position.y < 1000000.0f &&
           authority.position.z > -1000000.0f && authority.position.z < 1000000.0f;
}

uint32_t LocalSceneAdmission::TakeSequence() {
    const uint32_t sequence = mNextSequence;
    ++mNextSequence;
    if (mNextSequence == 0) mNextSequence = 1;
    return sequence;
}

} // namespace Game::Client
