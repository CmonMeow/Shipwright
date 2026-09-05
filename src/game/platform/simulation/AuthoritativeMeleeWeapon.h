#pragma once

#include "AuthoritativePlayerHitRig.h"

#include <cmath>

namespace Game::Simulation {

struct AuthoritativeMeleeWeaponSegment {
    Vec3 base{};
    Vec3 tip{};
};

// Ocarina's adult weapon model lengths are 4000 and 5500 model units. Link is
// rendered at 0.01 scale, yielding 40 and 55 world units respectively.
inline constexpr float kMasterSwordWorldLength = 40.0f;
inline constexpr float kBiggoronSwordWorldLength = 55.0f;

inline bool SampleAuthoritativeMeleeWeaponSegment(
    const PlayerSnapshot& player, AuthoritativeMeleeWeaponSegment& segment) {
    const bool attacking =
        player.actionState == PlayerActionState::PrimaryActive ||
        player.actionState == PlayerActionState::JumpSlashing ||
        player.actionState == PlayerActionState::SpinAttacking;
    if (!attacking || (player.selectedWeapon != 1 && player.selectedWeapon != 2)) {
        return false;
    }

    const AuthoritativePlayerSkeletonPose pose =
        SampleAuthoritativePlayerSkeletonPose(player);
    Vec3 elbow = pose[PlayerHitJoint::RightElbow];
    Vec3 wrist = pose[PlayerHitJoint::RightWrist];
    float length = kMasterSwordWorldLength;
    if (player.selectedWeapon == 2) {
        const Vec3& leftElbow = pose[PlayerHitJoint::LeftElbow];
        const Vec3& leftWrist = pose[PlayerHitJoint::LeftWrist];
        elbow = { (elbow.x + leftElbow.x) * 0.5f,
                  (elbow.y + leftElbow.y) * 0.5f,
                  (elbow.z + leftElbow.z) * 0.5f };
        wrist = { (wrist.x + leftWrist.x) * 0.5f,
                  (wrist.y + leftWrist.y) * 0.5f,
                  (wrist.z + leftWrist.z) * 0.5f };
        length = kBiggoronSwordWorldLength;
    }

    Vec3 direction{ wrist.x - elbow.x, wrist.y - elbow.y,
                    wrist.z - elbow.z };
    const float directionLength = std::sqrt(
        direction.x * direction.x + direction.y * direction.y +
        direction.z * direction.z);
    if (directionLength <= 0.0001f) return false;
    const float scale = length / directionLength;
    segment.base = wrist;
    segment.tip = { wrist.x + direction.x * scale,
                    wrist.y + direction.y * scale,
                    wrist.z + direction.z * scale };
    return true;
}

} // namespace Game::Simulation
