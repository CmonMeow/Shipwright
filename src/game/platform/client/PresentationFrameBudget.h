#pragma once

#include <algorithm>
#include <cstdint>

namespace Game::Client {

// Keeps optional presentation work inside one fixed native simulation step.
// Durations are supplied by the caller so the policy remains deterministic
// and independently testable.
class PresentationFrameBudget {
  public:
    static uint32_t FrameCount(uint32_t presentationFps, uint32_t simulationFps) {
        presentationFps = std::max(presentationFps, 1U);
        simulationFps = std::max(simulationFps, 1U);
        return presentationFps / simulationFps +
               (presentationFps % simulationFps != 0U ? 1U : 0U);
    }

    void BeginBatch(int presentationFps, int simulationFps) {
        presentationFps = std::max(presentationFps, 1);
        mSimulationSeconds = 1.0 / std::max(simulationFps, 1);
        if (mPresentationFps != presentationFps || mEstimatedPresentSeconds <= 0.0) {
            mPresentationFps = presentationFps;
            mEstimatedPresentSeconds = 1.0 / presentationFps;
        }
    }

    bool CanPresentIntermediate(double elapsedSeconds) const {
        // Reserve one optional sample plus the mandatory newest native state.
        return std::max(elapsedSeconds, 0.0) + mEstimatedPresentSeconds * 1.9 <= mSimulationSeconds;
    }

    void ObservePresent(double presentSeconds) {
        if (presentSeconds <= 0.0) return;
        const double weight = presentSeconds > mEstimatedPresentSeconds ? 0.5 : 0.1;
        mEstimatedPresentSeconds += (presentSeconds - mEstimatedPresentSeconds) * weight;
    }

    double EstimatedPresentSeconds() const { return mEstimatedPresentSeconds; }

  private:
    int mPresentationFps = 0;
    double mSimulationSeconds = 1.0 / 30.0;
    double mEstimatedPresentSeconds = 0.0;
};

} // namespace Game::Client
