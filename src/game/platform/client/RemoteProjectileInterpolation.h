#pragma once

#include "../simulation/PlayerSimulation.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace Game::Client {

struct RemoteProjectileSample {
    int32_t sceneId = -1;
    uint32_t sequence = 0;
    uint8_t phase = 0;
    bool terminal = false;
    Simulation::Vec3 position{};
    Simulation::Vec3 velocity{};
    int16_t rotationX = 0;
    int16_t rotationY = 0;
    int16_t rotationZ = 0;
};

struct RemoteProjectilePose {
    Simulation::Vec3 position{};
    int16_t rotationX = 0;
    int16_t rotationY = 0;
    int16_t rotationZ = 0;
    bool extrapolated = false;
    bool terminal = false;
};

// Smooths disposable 20 Hz projectile snapshots for rendering only. The
// server remains the sole collision and terminal-state authority: terminal
// samples bypass the delay and are presented at their exact impact position.
class RemoteProjectileInterpolation final {
  public:
    bool Push(const RemoteProjectileSample& sample, double receivedSeconds);
    std::optional<RemoteProjectilePose> Evaluate(double nowSeconds);
    void Reset();

    size_t SampleCount() const { return mSamples.size(); }

  private:
    struct BufferedSample {
        RemoteProjectileSample motion{};
        uint64_t unwrappedSequence = 0;
        double receivedSeconds = 0.0;
    };

    static constexpr double kSnapshotsPerSecond = 20.0;
    static constexpr double kInterpolationDelaySnapshots = 1.0;
    static constexpr double kMaximumExtrapolationSnapshots = 2.0;
    static constexpr size_t kMaximumSamples = 12;

    std::deque<BufferedSample> mSamples;
    double mLastRenderSequence = 0.0;
    bool mHasRenderSequence = false;
    bool mTerminal = false;
};

} // namespace Game::Client
