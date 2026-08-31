#pragma once

#include "../simulation/ClientPrediction.h"

#include <algorithm>
#include <cstdint>

namespace Game::Client {

enum class LocalPrimaryActionPresentationState : int8_t {
    Unavailable = -1,
    Idle = 0,
    Active = 1,
};

struct LocalPrimaryActionPresentation {
    LocalPrimaryActionPresentationState state =
        LocalPrimaryActionPresentationState::Unavailable;
    float progress = 0.0f;
};

// Converts prediction into renderer-facing state. This projection carries no
// gameplay authority and can safely lag the simulation by one native frame.
inline LocalPrimaryActionPresentation EvaluateLocalPrimaryActionPresentation(
    const Simulation::ClientPrediction& prediction, bool sessionActive) {
    if (!sessionActive || prediction.LifeEpoch() == 0) {
        return {};
    }

    const auto action = prediction.PredictedActionState();
    if (action != Simulation::PlayerActionState::PrimaryWindup &&
        action != Simulation::PlayerActionState::PrimaryActive &&
        action != Simulation::PlayerActionState::PrimaryRecovery) {
        return { LocalPrimaryActionPresentationState::Idle, 0.0f };
    }

    return { LocalPrimaryActionPresentationState::Active,
             std::clamp(prediction.PredictedActionProgress(), 0.0f, 1.0f) };
}

} // namespace Game::Client
