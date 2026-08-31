#pragma once

#include "PlayerSimulation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Game::Simulation {

enum class PlayerPoseDirection : uint8_t {
    None,
    Forward,
    Backward,
    Left,
    Right,
};

// Rendering and collision consume this same semantic pose clock. It contains
// no client animation state and can therefore be reconstructed by a dedicated
// server from an authoritative snapshot alone.
struct AuthoritativePlayerPoseState {
    PlayerPoseDirection direction = PlayerPoseDirection::None;
    float forwardSpeed = 0.0f;
    float rightSpeed = 0.0f;
    float locomotionAmount = 0.0f;
    float locomotionPhaseRadians = 0.0f;
    float actionProgress = 0.0f;
};

inline float PlayerActionProgress(const PlayerSnapshot& player) {
    const float elapsed = static_cast<float>(player.serverTick -
                                             std::min(player.serverTick,
                                                      player.actionStartTick));
    switch (player.actionState) {
        case PlayerActionState::PrimaryWindup:
            return 0.2f * std::clamp(
                elapsed / static_cast<float>(kPrimaryWindupDurationTicks), 0.0f, 1.0f);
        case PlayerActionState::PrimaryActive:
            return 0.2f + 0.45f * std::clamp(
                elapsed / static_cast<float>(kPrimaryActiveDurationTicks), 0.0f, 1.0f);
        case PlayerActionState::PrimaryRecovery:
            return 0.65f + 0.35f * std::clamp(
                elapsed / static_cast<float>(kPrimaryRecoveryDurationTicks), 0.0f, 1.0f);
        case PlayerActionState::Evading:
            return std::clamp(elapsed / static_cast<float>(kEvadeDurationTicks),
                              0.0f, 1.0f);
        case PlayerActionState::JumpSlashing:
            return std::clamp(elapsed / 12.0f, 0.0f, 1.0f);
        case PlayerActionState::Blocking:
        case PlayerActionState::Aiming:
            return 1.0f;
        default:
            return 0.0f;
    }
}

inline float PlayerActionProgressPerSecond(PlayerActionState state) {
    switch (state) {
        case PlayerActionState::PrimaryWindup:
            return 0.2f * 30.0f /
                   static_cast<float>(kPrimaryWindupDurationTicks);
        case PlayerActionState::PrimaryActive:
            return 0.45f * 30.0f /
                   static_cast<float>(kPrimaryActiveDurationTicks);
        case PlayerActionState::PrimaryRecovery:
            return 0.35f * 30.0f /
                   static_cast<float>(kPrimaryRecoveryDurationTicks);
        case PlayerActionState::Evading:
            return 30.0f / static_cast<float>(kEvadeDurationTicks);
        case PlayerActionState::JumpSlashing:
            return 30.0f / 12.0f;
        default:
            return 0.0f;
    }
}

inline AuthoritativePlayerPoseState SampleAuthoritativePlayerPoseState(
    const PlayerSnapshot& player) {
    constexpr float kMovementThreshold = 0.25f;
    AuthoritativePlayerPoseState pose{};
    pose.forwardSpeed =
        player.velocity.x * std::sin(player.headingRadians) +
        player.velocity.z * std::cos(player.headingRadians);
    pose.rightSpeed =
        player.velocity.x * std::cos(player.headingRadians) -
        player.velocity.z * std::sin(player.headingRadians);
    const float speed = std::hypot(pose.forwardSpeed, pose.rightSpeed);
    pose.locomotionAmount = std::clamp(speed / 180.0f, 0.0f, 1.0f);
    pose.locomotionPhaseRadians = player.locomotionPhaseRadians;
    pose.actionProgress = PlayerActionProgress(player);

    if (std::fabs(pose.forwardSpeed) <= kMovementThreshold &&
        std::fabs(pose.rightSpeed) <= kMovementThreshold) {
        return pose;
    }
    if (std::fabs(pose.rightSpeed) > std::fabs(pose.forwardSpeed)) {
        pose.direction = pose.rightSpeed < 0.0f ? PlayerPoseDirection::Left
                                               : PlayerPoseDirection::Right;
    } else {
        pose.direction = pose.forwardSpeed < 0.0f ? PlayerPoseDirection::Backward
                                                 : PlayerPoseDirection::Forward;
    }
    return pose;
}

} // namespace Game::Simulation
