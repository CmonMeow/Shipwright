#pragma once

#include <cstdint>

namespace Game::Client {

struct LocalFishingUpdate {
    int32_t sceneId = -1;
    bool visualActive = false;
    uint8_t fishingState = 0;
    bool lureDeployed = false;
    bool reelHeld = false;
};

struct LocalLureControlIntent {
    uint32_t sequence = 0;
    bool deployed = false;
    bool reelHeld = false;
    bool lifecycleTransition = false;
};

struct LocalFishingSendDecision {
    uint32_t presentationSequence = 0;
    uint32_t controlSequence = 0;
    bool reliableControl = false;

    bool SendPresentation() const { return presentationSequence != 0; }
    bool SendControl() const { return controlSequence != 0; }
};

// Coalesces high-volume cosmetic fishing telemetry independently from the
// render and Win32 message-pump rates. Discrete deploy/reel/state changes
// are emitted immediately; unchanged active state is refreshed at 20 Hz.
class LocalFishingUpdateStream final {
  public:
    LocalFishingSendDecision Evaluate(const LocalFishingUpdate& update,
                                      double nowSeconds);
    // Clears scene-local cadence/control memory while preserving monotonic
    // request sequences for the current player life.
    void BeginScene();
    void Reset();

  private:
    static uint32_t TakeSequence(uint32_t& next);

    static constexpr double kSendIntervalSeconds = 1.0 / 20.0;
    LocalFishingUpdate mPrevious{};
    double mLastPresentationSeconds = 0.0;
    double mLastControlSeconds = 0.0;
    uint32_t mNextPresentationSequence = 1;
    uint32_t mNextControlSequence = 1;
    bool mInitialized = false;
};

} // namespace Game::Client
