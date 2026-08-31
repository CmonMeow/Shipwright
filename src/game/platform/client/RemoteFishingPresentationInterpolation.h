#pragma once

#include "../replication/FishingPresentationState.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace Game::Client {

// Converts the bounded 20 Hz cosmetic fishing stream into a stable render
// pose. Fish/lure identity and world position remain separate authoritative
// entities and are deliberately not represented here.
class RemoteFishingPresentationInterpolation final {
  public:
    bool Push(const Replication::FishingPresentationState& state,
              double receivedSeconds);
    std::optional<Replication::FishingPresentationState> Evaluate(double nowSeconds);
    void Reset();

    size_t SampleCount() const { return mSamples.size(); }

  private:
    struct BufferedSample {
        Replication::FishingPresentationState state{};
        uint64_t unwrappedSequence = 0;
        double receivedSeconds = 0.0;
    };

    static constexpr double kSnapshotsPerSecond = 20.0;
    static constexpr double kInterpolationDelaySnapshots = 1.0;
    static constexpr size_t kMaximumSamples = 8;

    std::deque<BufferedSample> mSamples;
    double mLastRenderSequence = 0.0;
    bool mHasRenderSequence = false;
};

} // namespace Game::Client
