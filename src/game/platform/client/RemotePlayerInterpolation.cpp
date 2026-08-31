#include "RemotePlayerInterpolation.h"
#include "../SequenceNumber.h"

#include <algorithm>
#include <cmath>

namespace Game::Client {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

bool IsFinite(const Simulation::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Simulation::Vec3 Lerp(const Simulation::Vec3& from, const Simulation::Vec3& to, float fraction) {
    return { from.x + (to.x - from.x) * fraction,
             from.y + (to.y - from.y) * fraction,
             from.z + (to.z - from.z) * fraction };
}

float LerpAngle(float from, float to, float fraction) {
    const float difference = std::remainder(to - from, kTwoPi);
    return std::remainder(from + difference * fraction, kTwoPi);
}

} // namespace

bool RemotePlayerInterpolation::Push(const RemoteMotionSample& sample, double receivedSeconds) {
    if (sample.sceneId < 0 || sample.serverTick == 0 || sample.lifeEpoch == 0 ||
        !IsFinite(sample.position) || !IsFinite(sample.velocity) ||
        !std::isfinite(sample.headingRadians) || !std::isfinite(receivedSeconds)) {
        return false;
    }

    if (!mSamples.empty() &&
        (mSamples.back().motion.sceneId != sample.sceneId ||
         mSamples.back().motion.lifeEpoch != sample.lifeEpoch)) {
        Reset();
    }

    uint64_t unwrappedTick = sample.serverTick;
    if (!mSamples.empty()) {
        if (!Sequence::IsNewer(sample.serverTick,
                               mSamples.back().motion.serverTick)) return false;
        const uint32_t tickDelta = sample.serverTick -
                                   mSamples.back().motion.serverTick;
        unwrappedTick = mSamples.back().unwrappedTick + tickDelta;
        receivedSeconds = std::max(receivedSeconds, mSamples.back().receivedSeconds);
    }

    mSamples.push_back({ sample, unwrappedTick, receivedSeconds });
    while (mSamples.size() > kMaximumSamples) mSamples.pop_front();
    return true;
}

std::optional<RemoteMotionPose> RemotePlayerInterpolation::Evaluate(double nowSeconds) {
    if (mSamples.empty() || !std::isfinite(nowSeconds)) return std::nullopt;
    if (mSamples.size() == 1) {
        return RemoteMotionPose{ mSamples.back().motion.position,
                                 mSamples.back().motion.headingRadians, false };
    }

    const BufferedSample& newest = mSamples.back();
    const double elapsedTicks = std::clamp(
        (nowSeconds - newest.receivedSeconds) * kServerTicksPerSecond,
        0.0, kInterpolationDelayTicks + kMaximumExtrapolationTicks);
    double renderTick = static_cast<double>(newest.unwrappedTick) + elapsedTicks -
                        kInterpolationDelayTicks;
    if (mHasRenderTick) renderTick = std::max(renderTick, mLastRenderTick);
    renderTick = std::min(renderTick,
                          static_cast<double>(newest.unwrappedTick) + kMaximumExtrapolationTicks);
    mLastRenderTick = renderTick;
    mHasRenderTick = true;

    if (renderTick <= static_cast<double>(mSamples.front().unwrappedTick)) {
        const auto& sample = mSamples.front().motion;
        return RemoteMotionPose{ sample.position, sample.headingRadians, false };
    }

    for (size_t index = 1; index < mSamples.size(); ++index) {
        const BufferedSample& to = mSamples[index];
        if (renderTick > static_cast<double>(to.unwrappedTick)) continue;
        const BufferedSample& from = mSamples[index - 1];
        const double span = static_cast<double>(to.unwrappedTick - from.unwrappedTick);
        const float fraction = span <= 0.0
                                   ? 1.0f
                                   : static_cast<float>((renderTick - from.unwrappedTick) / span);
        return RemoteMotionPose{ Lerp(from.motion.position, to.motion.position, fraction),
                                 LerpAngle(from.motion.headingRadians,
                                           to.motion.headingRadians, fraction),
                                 false };
    }

    const double extrapolationTicks = std::clamp(
        renderTick - static_cast<double>(newest.unwrappedTick),
        0.0, kMaximumExtrapolationTicks);
    const float extrapolationSeconds =
        static_cast<float>(extrapolationTicks / kServerTicksPerSecond);
    const Simulation::Vec3 position{
        newest.motion.position.x + newest.motion.velocity.x * extrapolationSeconds,
        newest.motion.position.y + newest.motion.velocity.y * extrapolationSeconds,
        newest.motion.position.z + newest.motion.velocity.z * extrapolationSeconds
    };
    return RemoteMotionPose{ position, newest.motion.headingRadians,
                             extrapolationTicks > 0.0 };
}

void RemotePlayerInterpolation::Reset() {
    mSamples.clear();
    mLastRenderTick = 0.0;
    mHasRenderTick = false;
}

} // namespace Game::Client
