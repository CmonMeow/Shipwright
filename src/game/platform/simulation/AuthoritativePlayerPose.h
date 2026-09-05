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
    // Player-local lateral velocity: positive is left.
    float leftSpeed = 0.0f;
    float locomotionAmount = 0.0f;
    float locomotionPhaseRadians = 0.0f;
    float actionProgress = 0.0f;
};

inline float PlayerActionProgress(const PlayerSnapshot& player) {
    const float elapsed = static_cast<float>(player.serverTick -
                                             std::min(player.serverTick,
                                                      player.actionStartTick));
    switch (player.actionState) {
        case PlayerActionState::PrimaryWindup: {
            const MeleeAttackTiming timing = MeleeAttackTimingFor(
                player.meleeAttackVariant, player.selectedWeapon);
            if (timing.windupTicks == 0) return 0.0f;
            const float mainTicks = static_cast<float>(
                timing.windupTicks + timing.activeTicks);
            const float activeStart =
                static_cast<float>(timing.windupTicks) / mainTicks;
            return activeStart * std::clamp(
                elapsed / static_cast<float>(timing.windupTicks), 0.0f, 1.0f);
        }
        case PlayerActionState::PrimaryActive: {
            const MeleeAttackTiming timing = MeleeAttackTimingFor(
                player.meleeAttackVariant, player.selectedWeapon);
            const float mainTicks = static_cast<float>(
                timing.windupTicks + timing.activeTicks);
            const float activeStart =
                static_cast<float>(timing.windupTicks) / mainTicks;
            return activeStart + (1.0f - activeStart) * std::clamp(
                elapsed / static_cast<float>(timing.activeTicks), 0.0f, 1.0f);
        }
        case PlayerActionState::PrimaryRecovery:
            return 1.0f;
        case PlayerActionState::Evading:
            return std::clamp(elapsed / static_cast<float>(kEvadeDurationTicks),
                              0.0f, 1.0f);
        case PlayerActionState::SpinAttacking:
            return std::clamp(elapsed /
                                  static_cast<float>(kSpinAttackDurationTicks),
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

inline float PlayerActionProgressPerSecond(const PlayerSnapshot& player) {
    switch (player.actionState) {
        case PlayerActionState::PrimaryWindup:
        case PlayerActionState::PrimaryActive: {
            const MeleeAttackTiming timing = MeleeAttackTimingFor(
                player.meleeAttackVariant, player.selectedWeapon);
            return 30.0f / static_cast<float>(
                timing.windupTicks + timing.activeTicks);
        }
        case PlayerActionState::PrimaryRecovery:
            return 0.0f;
        case PlayerActionState::Evading:
            return 30.0f / static_cast<float>(kEvadeDurationTicks);
        case PlayerActionState::SpinAttacking:
            return 30.0f / static_cast<float>(kSpinAttackDurationTicks);
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
    pose.leftSpeed =
        player.velocity.x * std::cos(player.headingRadians) -
        player.velocity.z * std::sin(player.headingRadians);
    const float speed = std::hypot(pose.forwardSpeed, pose.leftSpeed);
    pose.locomotionAmount = std::clamp(speed / 120.0f, 0.0f, 1.0f);
    pose.locomotionPhaseRadians = player.locomotionPhaseRadians;
    pose.actionProgress = PlayerActionProgress(player);

    if (std::fabs(pose.forwardSpeed) <= kMovementThreshold &&
        std::fabs(pose.leftSpeed) <= kMovementThreshold) {
        return pose;
    }
    // Native PC parallel movement classifies any digital A/D component as
    // lateral before considering W/S. Preserve that tie-break for normalized
    // diagonals so observers select the same side-step animation as local
    // Link instead of a forward run with a sideways body lean.
    if (std::fabs(pose.leftSpeed) >= std::fabs(pose.forwardSpeed)) {
        pose.direction = pose.leftSpeed > 0.0f ? PlayerPoseDirection::Left
                                              : PlayerPoseDirection::Right;
    } else {
        pose.direction = pose.forwardSpeed < 0.0f ? PlayerPoseDirection::Backward
                                                 : PlayerPoseDirection::Forward;
    }
    return pose;
}

} // namespace Game::Simulation
