#include "RemoteProjectileInterpolation.h"

#include <algorithm>
#include <cmath>

namespace Game::Client {
namespace {

bool IsFinite(const Simulation::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Simulation::Vec3 Lerp(const Simulation::Vec3& from, const Simulation::Vec3& to, float fraction) {
    return { from.x + (to.x - from.x) * fraction,
             from.y + (to.y - from.y) * fraction,
             from.z + (to.z - from.z) * fraction };
}

int16_t LerpBinaryAngle(int16_t from, int16_t to, float fraction) {
    const int16_t difference = static_cast<int16_t>(
        static_cast<uint16_t>(to) - static_cast<uint16_t>(from));
    const int32_t value = static_cast<int32_t>(from) +
                          static_cast<int32_t>(std::lround(difference * fraction));
    return static_cast<int16_t>(static_cast<uint16_t>(value));
}

RemoteProjectilePose PoseFrom(const RemoteProjectileSample& sample, bool extrapolated = false) {
    return { sample.position, sample.rotationX, sample.rotationY, sample.rotationZ,
             extrapolated, sample.terminal };
}

} // namespace

bool RemoteProjectileInterpolation::Push(const RemoteProjectileSample& sample,
                                         double receivedSeconds) {
    if (sample.sceneId < 0 || sample.sequence == 0 || !IsFinite(sample.position) ||
        !IsFinite(sample.velocity) || !std::isfinite(receivedSeconds)) {
        return false;
    }

    if (!mSamples.empty() &&
        (mSamples.back().motion.sceneId != sample.sceneId ||
         mSamples.back().motion.phase != sample.phase || mTerminal)) {
        Reset();
    }

    uint64_t unwrappedSequence = sample.sequence;
    if (!mSamples.empty()) {
        const int32_t sequenceDelta = static_cast<int32_t>(
            sample.sequence - mSamples.back().motion.sequence);
        if (sequenceDelta <= 0) return false;
        unwrappedSequence = mSamples.back().unwrappedSequence +
                            static_cast<uint32_t>(sequenceDelta);
        receivedSeconds = std::max(receivedSeconds, mSamples.back().receivedSeconds);
    }

    if (sample.terminal) {
        Reset();
        mTerminal = true;
        mSamples.push_back({ sample, unwrappedSequence, receivedSeconds });
        return true;
    }

    mSamples.push_back({ sample, unwrappedSequence, receivedSeconds });
    while (mSamples.size() > kMaximumSamples) mSamples.pop_front();
    return true;
}

std::optional<RemoteProjectilePose> RemoteProjectileInterpolation::Evaluate(double nowSeconds) {
    if (mSamples.empty() || !std::isfinite(nowSeconds)) return std::nullopt;
    if (mTerminal || mSamples.size() == 1) return PoseFrom(mSamples.back().motion);

    const BufferedSample& newest = mSamples.back();
    const double elapsedSnapshots = std::clamp(
        (nowSeconds - newest.receivedSeconds) * kSnapshotsPerSecond,
        0.0, kInterpolationDelaySnapshots + kMaximumExtrapolationSnapshots);
    double renderSequence = static_cast<double>(newest.unwrappedSequence) +
                            elapsedSnapshots - kInterpolationDelaySnapshots;
    if (mHasRenderSequence) renderSequence = std::max(renderSequence, mLastRenderSequence);
    renderSequence = std::min(
        renderSequence,
        static_cast<double>(newest.unwrappedSequence) + kMaximumExtrapolationSnapshots);
    mLastRenderSequence = renderSequence;
    mHasRenderSequence = true;

    if (renderSequence <= static_cast<double>(mSamples.front().unwrappedSequence)) {
        return PoseFrom(mSamples.front().motion);
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
        return RemoteProjectilePose{
            Lerp(from.motion.position, to.motion.position, fraction),
            LerpBinaryAngle(from.motion.rotationX, to.motion.rotationX, fraction),
            LerpBinaryAngle(from.motion.rotationY, to.motion.rotationY, fraction),
            LerpBinaryAngle(from.motion.rotationZ, to.motion.rotationZ, fraction),
            false,
            false
        };
    }

    const double extrapolationSnapshots = std::clamp(
        renderSequence - static_cast<double>(newest.unwrappedSequence),
        0.0, kMaximumExtrapolationSnapshots);
    const float extrapolationSeconds = static_cast<float>(
        extrapolationSnapshots / kSnapshotsPerSecond);
    RemoteProjectilePose pose = PoseFrom(newest.motion, extrapolationSnapshots > 0.0);
    pose.position = { newest.motion.position.x + newest.motion.velocity.x * extrapolationSeconds,
                      newest.motion.position.y + newest.motion.velocity.y * extrapolationSeconds,
                      newest.motion.position.z + newest.motion.velocity.z * extrapolationSeconds };
    return pose;
}

void RemoteProjectileInterpolation::Reset() {
    mSamples.clear();
    mLastRenderSequence = 0.0;
    mHasRenderSequence = false;
    mTerminal = false;
}

} // namespace Game::Client
