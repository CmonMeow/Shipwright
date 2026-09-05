#include "RemoteFishingPresentationInterpolation.h"
#include "../SequenceNumber.h"

#include <algorithm>
#include <cmath>

namespace Game::Client {
namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

float Lerp(float from, float to, float fraction) {
    return from + (to - from) * fraction;
}

float LerpAngle(float from, float to, float fraction) {
    return from + std::remainder(to - from, kTwoPi) * fraction;
}

int16_t LerpBinaryAngle(int16_t from, int16_t to, float fraction) {
    const int16_t difference = static_cast<int16_t>(
        static_cast<uint16_t>(to) - static_cast<uint16_t>(from));
    const int32_t value = static_cast<int32_t>(from) +
                          static_cast<int32_t>(std::lround(difference * fraction));
    return static_cast<int16_t>(static_cast<uint16_t>(value));
}

bool IsFinite(const Replication::FishingPresentationState& state) {
    const auto finite = [](float value) { return std::isfinite(value); };
    for (size_t axis = 0; axis < 3; ++axis) {
        if (!finite(state.lureDrawOffset[axis]) ||
            !finite(state.lureRotation[axis])) return false;
    }
    for (const auto& hook : state.lureHookOffsets) {
        for (float value : hook) if (!finite(value)) return false;
    }
    for (const auto& hook : state.lureHookRotations) {
        for (float value : hook) if (!finite(value)) return false;
    }
    return finite(state.rodBendY) && finite(state.rodBendX) &&
           finite(state.rodTwist) && finite(state.rodCastX) &&
           finite(state.lureSpin) && finite(state.lureZOffset) &&
           finite(state.lineScale) && finite(state.lineGravity);
}

Replication::FishingPresentationState Interpolate(
    const Replication::FishingPresentationState& from,
    const Replication::FishingPresentationState& to, float fraction) {
    Replication::FishingPresentationState result = to;
    for (size_t axis = 0; axis < 3; ++axis) {
        result.lureDrawOffset[axis] = Lerp(from.lureDrawOffset[axis], to.lureDrawOffset[axis], fraction);
        result.lureRotation[axis] = LerpAngle(from.lureRotation[axis], to.lureRotation[axis], fraction);
        result.fishRotation[axis] = LerpBinaryAngle(from.fishRotation[axis], to.fishRotation[axis], fraction);
    }
    result.rodBendY = Lerp(from.rodBendY, to.rodBendY, fraction);
    result.rodBendX = Lerp(from.rodBendX, to.rodBendX, fraction);
    result.rodTwist = LerpAngle(from.rodTwist, to.rodTwist, fraction);
    result.rodCastX = LerpAngle(from.rodCastX, to.rodCastX, fraction);
    result.lureSpin = LerpAngle(from.lureSpin, to.lureSpin, fraction);
    result.lureZOffset = Lerp(from.lureZOffset, to.lureZOffset, fraction);
    for (size_t hook = 0; hook < result.lureHookOffsets.size(); ++hook) {
        for (size_t axis = 0; axis < result.lureHookOffsets[hook].size(); ++axis) {
            result.lureHookOffsets[hook][axis] = Lerp(
                from.lureHookOffsets[hook][axis], to.lureHookOffsets[hook][axis], fraction);
        }
        for (size_t axis = 0; axis < result.lureHookRotations[hook].size(); ++axis) {
            result.lureHookRotations[hook][axis] = LerpAngle(
                from.lureHookRotations[hook][axis], to.lureHookRotations[hook][axis], fraction);
        }
    }
    result.lineScale = Lerp(from.lineScale, to.lineScale, fraction);
    result.lineGravity = Lerp(from.lineGravity, to.lineGravity, fraction);
    for (size_t limb = 0; limb < result.fishLimbRotation.size(); ++limb) {
        result.fishLimbRotation[limb] = LerpBinaryAngle(
            from.fishLimbRotation[limb], to.fishLimbRotation[limb], fraction);
    }
    return result;
}

} // namespace

bool RemoteFishingPresentationInterpolation::Push(
    const Replication::FishingPresentationState& state, double receivedSeconds) {
    if (state.playerId < 0 || !state.entity.Valid() || state.sceneId < 0 ||
        state.sequence == 0 || !IsFinite(state) || !std::isfinite(receivedSeconds)) {
        return false;
    }
    if (!mSamples.empty() &&
        (mSamples.back().state.playerId != state.playerId ||
         mSamples.back().state.entity != state.entity ||
         mSamples.back().state.sceneId != state.sceneId ||
         mSamples.back().state.state != state.state)) {
        Reset();
    }

    uint64_t unwrappedSequence = state.sequence;
    if (!mSamples.empty()) {
        if (!Sequence::IsNewer(state.sequence,
                               mSamples.back().state.sequence)) return false;
        const uint32_t delta = state.sequence - mSamples.back().state.sequence;
        unwrappedSequence = mSamples.back().unwrappedSequence +
                            delta;
        receivedSeconds = std::max(receivedSeconds, mSamples.back().receivedSeconds);
    }
    mSamples.push_back({ state, unwrappedSequence, receivedSeconds });
    while (mSamples.size() > kMaximumSamples) mSamples.pop_front();
    return true;
}

std::optional<Replication::FishingPresentationState>
RemoteFishingPresentationInterpolation::Evaluate(double nowSeconds) {
    if (mSamples.empty() || !std::isfinite(nowSeconds)) return std::nullopt;
    if (mSamples.size() == 1) return mSamples.back().state;

    const BufferedSample& newest = mSamples.back();
    const double elapsed = std::clamp(
        (nowSeconds - newest.receivedSeconds) * kSnapshotsPerSecond,
        0.0, kInterpolationDelaySnapshots);
    double renderSequence = static_cast<double>(newest.unwrappedSequence) +
                            elapsed - kInterpolationDelaySnapshots;
    if (mHasRenderSequence) renderSequence = std::max(renderSequence, mLastRenderSequence);
    renderSequence = std::min(renderSequence, static_cast<double>(newest.unwrappedSequence));
    mLastRenderSequence = renderSequence;
    mHasRenderSequence = true;

    if (renderSequence <= static_cast<double>(mSamples.front().unwrappedSequence)) {
        return mSamples.front().state;
    }
    for (size_t index = 1; index < mSamples.size(); ++index) {
        const BufferedSample& to = mSamples[index];
        if (renderSequence > static_cast<double>(to.unwrappedSequence)) continue;
        const BufferedSample& from = mSamples[index - 1];
        const double span = static_cast<double>(to.unwrappedSequence -
                                                from.unwrappedSequence);
        const float fraction = span <= 0.0
                                   ? 1.0f
                                   : static_cast<float>((renderSequence -
                                                         from.unwrappedSequence) / span);
        return Interpolate(from.state, to.state, fraction);
    }
    return newest.state;
}

void RemoteFishingPresentationInterpolation::Reset() {
    mSamples.clear();
    mLastRenderSequence = 0.0;
    mHasRenderSequence = false;
}

} // namespace Game::Client
