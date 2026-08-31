#pragma once

#include "ArticulatedPlayerHitRig.h"
#include "AuthoritativePlayerPose.h"
#include "CombatGeometry.h"

#include <algorithm>
#include <cmath>

namespace Game::Simulation {

inline constexpr PlayerHitRigDimensions kAdultLinkHitRigDimensions{
    6.5f, 8.5f, 7.5f, 4.25f, 3.75f, 5.25f, 4.25f
};

// Deterministic, headless combat pose. It is derived solely from authoritative
// state and is therefore identical on a dedicated server and every client.
// Rendering may interpolate between snapshots, but never changes this geometry.
inline AuthoritativePlayerSkeletonPose SampleAuthoritativePlayerSkeletonPose(
    const PlayerSnapshot& player, float poseAdvanceSeconds = 0.0f) {
    AuthoritativePlayerSkeletonPose pose{};
    AuthoritativePlayerPoseState state =
        SampleAuthoritativePlayerPoseState(player);
    if (std::isfinite(poseAdvanceSeconds) && poseAdvanceSeconds > 0.0f) {
        constexpr float kTau = 6.28318530717958647692f;
        const float horizontalSpeed =
            std::hypot(player.velocity.x, player.velocity.z);
        state.locomotionPhaseRadians = std::fmod(
            state.locomotionPhaseRadians +
                horizontalSpeed * poseAdvanceSeconds *
                    (kTau / kPlayerLocomotionCycleDistance),
            kTau);
        state.actionProgress = std::clamp(
            state.actionProgress +
                PlayerActionProgressPerSecond(player.actionState) *
                    poseAdvanceSeconds,
            0.0f, 1.0f);
    }
    const float cycle = std::sin(state.locomotionPhaseRadians);
    const float directionSign =
        state.direction == PlayerPoseDirection::Backward ? -1.0f : 1.0f;
    const float legSwing = cycle * 6.0f * state.locomotionAmount * directionSign;
    const float armSwing = cycle * 4.0f * state.locomotionAmount * directionSign;

    auto set = [&](PlayerHitJoint joint, float x, float y, float z) {
        const float cosine = std::cos(player.headingRadians);
        const float sine = std::sin(player.headingRadians);
        pose[joint] = { player.position.x + x * cosine + z * sine,
                        player.position.y + y,
                        player.position.z - x * sine + z * cosine };
    };

    set(PlayerHitJoint::HeadTop, 0.0f, 68.0f, 0.0f);
    set(PlayerHitJoint::HeadBase, 0.0f, 55.0f, 0.0f);
    set(PlayerHitJoint::TorsoTop, 0.0f, 52.0f, 0.0f);
    set(PlayerHitJoint::TorsoBottom, 0.0f, 29.0f, 0.0f);
    set(PlayerHitJoint::WaistBottom, 0.0f, 22.0f, 0.0f);

    float leftElbowX = -13.0f;
    float leftElbowY = 39.0f;
    float leftElbowZ = -armSwing;
    float leftWristX = -12.0f;
    float leftWristY = 29.0f;
    float leftWristZ = -armSwing * 1.5f;
    float rightElbowX = 13.0f;
    float rightElbowY = 39.0f;
    float rightElbowZ = armSwing;
    float rightWristX = 12.0f;
    float rightWristY = 29.0f;
    float rightWristZ = armSwing * 1.5f;

    if (state.direction == PlayerPoseDirection::Left ||
        state.direction == PlayerPoseDirection::Right) {
        const float strafeSign = state.direction == PlayerPoseDirection::Left
                                     ? -1.0f
                                     : 1.0f;
        const float lateralSwing = cycle * 4.0f * state.locomotionAmount;
        leftElbowX += lateralSwing * strafeSign;
        leftWristX += lateralSwing * 1.5f * strafeSign;
        rightElbowX += lateralSwing * strafeSign;
        rightWristX += lateralSwing * 1.5f * strafeSign;
    }

    if (player.actionState == PlayerActionState::Blocking) {
        leftElbowX = -12.0f; leftElbowY = 43.0f; leftElbowZ = 11.0f;
        leftWristX = -8.0f; leftWristY = 39.0f; leftWristZ = 22.0f;
        rightElbowX = 8.0f; rightElbowY = 40.0f; rightElbowZ = 8.0f;
        rightWristX = 5.0f; rightWristY = 35.0f; rightWristZ = 17.0f;
    } else if (player.actionState == PlayerActionState::Aiming) {
        const float pitch = std::clamp(player.aimPitchRadians, -1.2f, 1.2f);
        const float forward = std::cos(pitch);
        const float vertical = -std::sin(pitch);
        leftElbowX = -10.0f; leftElbowY = 48.0f + vertical * 6.0f; leftElbowZ = forward * 10.0f;
        leftWristX = -8.0f; leftWristY = 47.0f + vertical * 16.0f; leftWristZ = forward * 23.0f;
        rightElbowX = 10.0f; rightElbowY = 47.0f + vertical * 5.0f; rightElbowZ = forward * 7.0f;
        rightWristX = 1.0f; rightWristY = 47.0f + vertical * 15.0f; rightWristZ = forward * 20.0f;
    } else if (player.actionState == PlayerActionState::PrimaryWindup ||
               player.actionState == PlayerActionState::PrimaryActive ||
               player.actionState == PlayerActionState::PrimaryRecovery) {
        const float swingAngle = -1.25f + state.actionProgress * 2.5f;
        const float swingX = std::sin(swingAngle);
        const float swingZ = std::cos(swingAngle);
        if (player.selectedWeapon == 2) {
            leftElbowX = -5.0f + swingX * 8.0f;
            leftElbowY = 47.0f;
            leftElbowZ = swingZ * 10.0f;
            leftWristX = swingX * 15.0f;
            leftWristY = 43.0f;
            leftWristZ = swingZ * 20.0f;
            rightElbowX = 5.0f + swingX * 8.0f;
            rightElbowY = 46.0f;
            rightElbowZ = swingZ * 9.0f;
            rightWristX = swingX * 14.0f;
            rightWristY = 42.0f;
            rightWristZ = swingZ * 19.0f;
        } else {
            rightElbowX = 8.0f + swingX * 8.0f;
            rightElbowY = 45.0f;
            rightElbowZ = swingZ * 11.0f;
            rightWristX = 5.0f + swingX * 18.0f;
            rightWristY = 39.0f;
            rightWristZ = swingZ * 22.0f;
        }
    } else if (player.actionState == PlayerActionState::JumpSlashing) {
        const float drop = state.actionProgress * 12.0f;
        leftElbowX = -7.0f; leftElbowY = 50.0f - drop * 0.3f; leftElbowZ = 8.0f;
        leftWristX = -3.0f; leftWristY = 48.0f - drop; leftWristZ = 20.0f;
        rightElbowX = 7.0f; rightElbowY = 50.0f - drop * 0.3f; rightElbowZ = 8.0f;
        rightWristX = 3.0f; rightWristY = 48.0f - drop; rightWristZ = 20.0f;
    } else if (player.actionState == PlayerActionState::Evading) {
        leftElbowX = -11.0f; leftElbowY = 38.0f; leftElbowZ = -5.0f;
        leftWristX = -8.0f; leftWristY = 30.0f; leftWristZ = 2.0f;
        rightElbowX = 11.0f; rightElbowY = 38.0f; rightElbowZ = -5.0f;
        rightWristX = 8.0f; rightWristY = 30.0f; rightWristZ = 2.0f;
    }

    set(PlayerHitJoint::LeftShoulder, -9.0f, 49.0f, 0.0f);
    set(PlayerHitJoint::LeftElbow, leftElbowX, leftElbowY, leftElbowZ);
    set(PlayerHitJoint::LeftWrist, leftWristX, leftWristY, leftWristZ);
    set(PlayerHitJoint::RightShoulder, 9.0f, 49.0f, 0.0f);
    set(PlayerHitJoint::RightElbow, rightElbowX, rightElbowY, rightElbowZ);
    set(PlayerHitJoint::RightWrist, rightWristX, rightWristY, rightWristZ);
    float leftKneeX = -6.0f;
    float leftKneeZ = legSwing;
    float leftAnkleX = -6.0f;
    float leftAnkleZ = -legSwing * 0.35f;
    float rightKneeX = 6.0f;
    float rightKneeZ = -legSwing;
    float rightAnkleX = 6.0f;
    float rightAnkleZ = legSwing * 0.35f;
    if (state.direction == PlayerPoseDirection::Left ||
        state.direction == PlayerPoseDirection::Right) {
        const float strafeSign = state.direction == PlayerPoseDirection::Left
                                     ? -1.0f
                                     : 1.0f;
        const float lateral = cycle * 5.0f * state.locomotionAmount * strafeSign;
        leftKneeX += lateral;
        leftAnkleX -= lateral * 0.35f;
        rightKneeX -= lateral;
        rightAnkleX += lateral * 0.35f;
        leftKneeZ = leftAnkleZ = rightKneeZ = rightAnkleZ = 0.0f;
    }
    if (player.locomotionMode == PlayerLocomotionMode::Airborne) {
        leftKneeZ += 5.0f;
        leftAnkleZ -= 5.0f;
        rightKneeZ += 5.0f;
        rightAnkleZ -= 5.0f;
    }
    set(PlayerHitJoint::LeftHip, -6.0f, 23.0f, 0.0f);
    set(PlayerHitJoint::LeftKnee, leftKneeX, 12.0f, leftKneeZ);
    set(PlayerHitJoint::LeftAnkle, leftAnkleX, 1.5f, leftAnkleZ);
    set(PlayerHitJoint::RightHip, 6.0f, 23.0f, 0.0f);
    set(PlayerHitJoint::RightKnee, rightKneeX, 12.0f, rightKneeZ);
    set(PlayerHitJoint::RightAnkle, rightAnkleX, 1.5f, rightAnkleZ);
    return pose;
}

inline ArticulatedPlayerHitRig BuildAuthoritativePlayerHitRig(
    const PlayerSnapshot& player, float poseAdvanceSeconds = 0.0f) {
    return BuildArticulatedPlayerHitRig(
        SampleAuthoritativePlayerSkeletonPose(player, poseAdvanceSeconds),
        kAdultLinkHitRigDimensions);
}

// The mirror shield follows the blocking hands rather than a broad rectangle
// attached to the actor origin. This same surface resolves arrows and melee.
inline bool SegmentAuthoritativeShieldFirstHit(
    const Vec3& start, const Vec3& end, const PlayerSnapshot& player,
    ShieldHit& hit) {
    if (player.actionState != PlayerActionState::Blocking) return false;
    const AuthoritativePlayerSkeletonPose pose =
        SampleAuthoritativePlayerSkeletonPose(player);
    const Vec3& leftWrist = pose[PlayerHitJoint::LeftWrist];
    const Vec3& rightWrist = pose[PlayerHitJoint::RightWrist];
    const Vec3 center{ (leftWrist.x + rightWrist.x) * 0.5f,
                       (leftWrist.y + rightWrist.y) * 0.5f,
                       (leftWrist.z + rightWrist.z) * 0.5f };
    return SegmentOrientedVerticalRectangleFirstHit(
        start, end, center, player.headingRadians, 14.0f, 18.0f, hit);
}

} // namespace Game::Simulation
