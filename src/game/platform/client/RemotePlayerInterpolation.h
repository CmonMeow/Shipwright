#pragma once

#include "../simulation/PlayerSimulation.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace Game::Client {

struct RemoteMotionSample {
    int32_t sceneId = -1;
    uint32_t serverTick = 0;
    uint32_t lifeEpoch = 0;
    Simulation::Vec3 position{};
    Simulation::Vec3 velocity{};
    float headingRadians = 0.0f;
};

struct RemoteMotionPose {
    Simulation::Vec3 position{};
    float headingRadians = 0.0f;
    bool extrapolated = false;
};

// Converts authoritative 30 Hz player snapshots into a frame-rate-independent
// render pose. Gameplay and collision continue to use server state; this class
// only delays presentation briefly to absorb packet timing variation.
class RemotePlayerInterpolation final {
  public:
    bool Push(const RemoteMotionSample& sample, double receivedSeconds);
    std::optional<RemoteMotionPose> Evaluate(double nowSeconds);
    void Reset();

    size_t SampleCount() const { return mSamples.size(); }

  private:
    struct BufferedSample {
        RemoteMotionSample motion{};
        uint64_t unwrappedTick = 0;
        double receivedSeconds = 0.0;
    };

    static constexpr double kServerTicksPerSecond = 30.0;
    static constexpr double kInterpolationDelayTicks = 2.0;
    static constexpr double kMaximumExtrapolationTicks = 3.0;
    static constexpr size_t kMaximumSamples = 16;

    std::deque<BufferedSample> mSamples;
    double mLastRenderTick = 0.0;
    bool mHasRenderTick = false;
};

} // namespace Game::Client
