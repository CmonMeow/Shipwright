#pragma once

#include <algorithm>
#include <cmath>

namespace Game::Client {

// Converts a monotonic wall-clock sample into bounded presentation/simulation
// delta time. Session replacement resets the clock so elapsed disconnect or
// loading time can never become one oversized gameplay frame.
class ClientFrameClock {
  public:
    static constexpr float kDefaultDeltaSeconds = 1.0f / 30.0f;
    static constexpr float kMaximumDeltaSeconds = 0.25f;

    void Reset(double nowSeconds) {
        mPreviousSeconds = nowSeconds;
        mValid = std::isfinite(nowSeconds);
    }

    float Sample(double nowSeconds) {
        if (!std::isfinite(nowSeconds)) return kDefaultDeltaSeconds;
        if (!mValid) {
            Reset(nowSeconds);
            return kDefaultDeltaSeconds;
        }
        const double elapsed = nowSeconds - mPreviousSeconds;
        mPreviousSeconds = nowSeconds;
        return static_cast<float>(
            std::clamp(elapsed, 0.0,
                       static_cast<double>(kMaximumDeltaSeconds)));
    }

  private:
    double mPreviousSeconds = 0.0;
    bool mValid = false;
};

} // namespace Game::Client
