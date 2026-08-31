#pragma once

#include "PlayerSimulation.h"

#include <cstddef>
#include <cstdint>
#include <deque>

namespace Game::Simulation {

class ClientPrediction final {
  public:
    bool SeedAuthoritative(const PlayerSnapshot& authoritative);
    void RecordCommand(const PlayerCommand& command,
                       float deltaSeconds = 1.0f / 30.0f,
                       uint8_t predictionWeapon = 0);
    bool Reconcile(const PlayerSnapshot& authoritative,
                   const Vec3& currentPredictedPosition);
    Vec3 ConsumeCorrection(float deltaSeconds, float halfLifeSeconds = 0.15f,
                           float snapDistance = 200.0f);
    void Reset(uint32_t lifeEpoch = 0);

    const Vec3& PendingCorrection() const;
    size_t PendingCommandCount() const;
    PlayerActionState PredictedActionState() const { return mPredictedActionState; }
    float PredictedActionProgress() const;
    PlayerLocomotionMode PredictedLocomotionMode() const {
        return mPredictedLocomotionMode;
    }
    bool HasAuthoritativeSeed() const { return mHasPredictedPosition; }
    uint32_t LifeEpoch() const { return mLifeEpoch; }

  private:
    struct Sample {
        uint32_t sequence = 0;
        uint32_t lifeEpoch = 0;
        int32_t sceneId = -1;
        Vec3 position{};
        PlayerCommand command{};
        float deltaSeconds = 0.0f;
        uint8_t predictionWeapon = 0;
    };

    bool ReconcileInternal(const PlayerSnapshot& authoritative,
                           const Vec3& currentPredictedPosition);

    std::deque<Sample> mSamples;
    uint32_t mLifeEpoch = 0;
    Vec3 mCorrection{};
    uint32_t mLastAcknowledgedSequence = 0;
    int32_t mLastAcknowledgedScene = -1;
    bool mHasAcknowledgement = false;
    Vec3 mPredictedPosition{};
    int32_t mPredictedScene = -1;
    bool mHasPredictedPosition = false;
    PlayerActionState mPredictedActionState = PlayerActionState::Idle;
    float mPredictedActionRemainingSeconds = 0.0f;
    Vec3 mPredictedEvadeVelocity{};
    PlayerLocomotionMode mPredictedLocomotionMode =
        PlayerLocomotionMode::Grounded;
};

} // namespace Game::Simulation
