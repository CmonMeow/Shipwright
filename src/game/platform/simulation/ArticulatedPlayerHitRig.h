#pragma once

#include "CombatHitRegion.h"
#include "PlayerSimulation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <limits>

namespace Game::Simulation {

// These anchors are sampled from the authoritative Link skeleton. Cosmetic
// joints (hands, feet, hair, hat and sheath) intentionally do not participate
// in combat collision.
enum class PlayerHitJoint : uint8_t {
    HeadTop,
    HeadBase,
    TorsoTop,
    TorsoBottom,
    WaistBottom,
    LeftShoulder,
    LeftElbow,
    LeftWrist,
    RightShoulder,
    RightElbow,
    RightWrist,
    LeftHip,
    LeftKnee,
    LeftAnkle,
    RightHip,
    RightKnee,
    RightAnkle,
    Count,
};

struct AuthoritativePlayerSkeletonPose {
    std::array<Vec3, static_cast<std::size_t>(PlayerHitJoint::Count)> joints{};

    Vec3& operator[](PlayerHitJoint joint) {
        return joints[static_cast<std::size_t>(joint)];
    }
    const Vec3& operator[](PlayerHitJoint joint) const {
        return joints[static_cast<std::size_t>(joint)];
    }
};

// Outer radii are measured against the visible model. Keeping them separate
// from the pose lets model calibration change without changing collision code.
struct PlayerHitRigDimensions {
    float head = 0.0f;
    float torso = 0.0f;
    float waist = 0.0f;
    float upperArm = 0.0f;
    float forearm = 0.0f;
    float thigh = 0.0f;
    float shin = 0.0f;
};

struct PlayerHitPrism {
    PlayerHitRegion region = PlayerHitRegion::None;
    Vec3 start{};
    Vec3 end{};
    float outerRadius = 0.0f;
};

struct ArticulatedPlayerHitRig {
    std::array<PlayerHitPrism, 11> prisms{};
};

struct PlayerRigHit {
    PlayerHitRegion region = PlayerHitRegion::None;
    float segmentRatio = 0.0f;
    Vec3 position{};
};

struct ArrowBodyAttachment {
    int32_t playerId = -1;
    uint32_t lifeEpoch = 0;
    PlayerHitRegion region = PlayerHitRegion::None;
    Vec3 offset{};
    Vec3 direction{};
};

namespace HitRigDetail {

inline float Dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline Vec3 Subtract(const Vec3& left, const Vec3& right) {
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

inline Vec3 Cross(const Vec3& left, const Vec3& right) {
    return { left.y * right.z - left.z * right.y,
             left.z * right.x - left.x * right.z,
             left.x * right.y - left.y * right.x };
}

inline Vec3 Scale(const Vec3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

inline Vec3 Add(const Vec3& left, const Vec3& right) {
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

inline bool ClipHalfSpace(const Vec3& segmentStart, const Vec3& segmentDirection,
                          const Vec3& normal, float planeOffset,
                          float& enterRatio, float& exitRatio) {
    constexpr float kParallelEpsilon = 0.000001f;
    const float startDistance = Dot(normal, segmentStart) - planeOffset;
    const float directionDistance = Dot(normal, segmentDirection);
    if (std::abs(directionDistance) <= kParallelEpsilon) {
        return startDistance <= 0.0f;
    }
    const float boundaryRatio = -startDistance / directionDistance;
    if (directionDistance > 0.0f) {
        exitRatio = std::min(exitRatio, boundaryRatio);
    } else {
        enterRatio = std::max(enterRatio, boundaryRatio);
    }
    return enterRatio <= exitRatio;
}

inline bool SegmentHexagonalPrismFirstHit(const Vec3& segmentStart,
                                           const Vec3& segmentEnd,
                                           const PlayerHitPrism& prism,
                                           float& ratio) {
    constexpr float kMinimumLengthSquared = 0.000001f;
    constexpr float kHexagonApothemScale = 0.8660254037844386f;
    constexpr float kPiOverThree = 1.0471975511965977f;
    if (prism.outerRadius <= 0.0f) return false;

    const Vec3 axisVector = Subtract(prism.end, prism.start);
    const float axisLengthSquared = Dot(axisVector, axisVector);
    if (axisLengthSquared <= kMinimumLengthSquared) return false;
    const float axisLength = std::sqrt(axisLengthSquared);
    const Vec3 axis = Scale(axisVector, 1.0f / axisLength);
    const Vec3 reference = std::abs(axis.y) < 0.9f ? Vec3{ 0.0f, 1.0f, 0.0f }
                                                   : Vec3{ 1.0f, 0.0f, 0.0f };
    const Vec3 firstCross = Cross(axis, reference);
    const float firstCrossLength = std::sqrt(Dot(firstCross, firstCross));
    const Vec3 radialX = Scale(firstCross, 1.0f / firstCrossLength);
    const Vec3 radialY = Cross(axis, radialX);
    const Vec3 segmentDirection = Subtract(segmentEnd, segmentStart);
    float enterRatio = 0.0f;
    float exitRatio = 1.0f;

    if (!ClipHalfSpace(segmentStart, segmentDirection, axis,
                       Dot(axis, prism.end), enterRatio, exitRatio) ||
        !ClipHalfSpace(segmentStart, segmentDirection, Scale(axis, -1.0f),
                       -Dot(axis, prism.start), enterRatio, exitRatio)) {
        return false;
    }

    const float apothem = prism.outerRadius * kHexagonApothemScale;
    for (int side = 0; side < 6; ++side) {
        const float angle = static_cast<float>(side) * kPiOverThree;
        const Vec3 normal = Add(Scale(radialX, std::cos(angle)),
                                Scale(radialY, std::sin(angle)));
        if (!ClipHalfSpace(segmentStart, segmentDirection, normal,
                           Dot(normal, prism.start) + apothem,
                           enterRatio, exitRatio)) {
            return false;
        }
    }

    if (exitRatio < 0.0f || enterRatio > 1.0f) return false;
    ratio = std::max(0.0f, enterRatio);
    return true;
}

} // namespace HitRigDetail

inline ArticulatedPlayerHitRig BuildArticulatedPlayerHitRig(
    const AuthoritativePlayerSkeletonPose& pose,
    const PlayerHitRigDimensions& dimensions) {
    const auto prism = [&](PlayerHitRegion region, PlayerHitJoint start,
                           PlayerHitJoint end, float radius) {
        return PlayerHitPrism{ region, pose[start], pose[end], radius };
    };
    return { {
        prism(PlayerHitRegion::Head, PlayerHitJoint::HeadBase,
              PlayerHitJoint::HeadTop, dimensions.head),
        prism(PlayerHitRegion::Torso, PlayerHitJoint::TorsoBottom,
              PlayerHitJoint::TorsoTop, dimensions.torso),
        prism(PlayerHitRegion::Waist, PlayerHitJoint::WaistBottom,
              PlayerHitJoint::TorsoBottom, dimensions.waist),
        prism(PlayerHitRegion::LeftUpperArm, PlayerHitJoint::LeftShoulder,
              PlayerHitJoint::LeftElbow, dimensions.upperArm),
        prism(PlayerHitRegion::LeftForearm, PlayerHitJoint::LeftElbow,
              PlayerHitJoint::LeftWrist, dimensions.forearm),
        prism(PlayerHitRegion::RightUpperArm, PlayerHitJoint::RightShoulder,
              PlayerHitJoint::RightElbow, dimensions.upperArm),
        prism(PlayerHitRegion::RightForearm, PlayerHitJoint::RightElbow,
              PlayerHitJoint::RightWrist, dimensions.forearm),
        prism(PlayerHitRegion::LeftThigh, PlayerHitJoint::LeftHip,
              PlayerHitJoint::LeftKnee, dimensions.thigh),
        prism(PlayerHitRegion::LeftShin, PlayerHitJoint::LeftKnee,
              PlayerHitJoint::LeftAnkle, dimensions.shin),
        prism(PlayerHitRegion::RightThigh, PlayerHitJoint::RightHip,
              PlayerHitJoint::RightKnee, dimensions.thigh),
        prism(PlayerHitRegion::RightShin, PlayerHitJoint::RightKnee,
              PlayerHitJoint::RightAnkle, dimensions.shin),
    } };
}

inline bool SegmentArticulatedPlayerHitRigFirstHit(
    const Vec3& start, const Vec3& end, const ArticulatedPlayerHitRig& rig,
    PlayerRigHit& hit) {
    float closest = std::numeric_limits<float>::infinity();
    PlayerHitRegion region = PlayerHitRegion::None;
    for (const PlayerHitPrism& prism : rig.prisms) {
        float candidate = 0.0f;
        if (HitRigDetail::SegmentHexagonalPrismFirstHit(start, end, prism,
                                                        candidate) &&
            candidate < closest) {
            closest = candidate;
            region = prism.region;
        }
    }
    if (region == PlayerHitRegion::None) return false;
    hit = { region, closest,
            { start.x + (end.x - start.x) * closest,
              start.y + (end.y - start.y) * closest,
              start.z + (end.z - start.z) * closest } };
    return true;
}

// Coordinates are relative to the struck limb, including its length/radius.
// This lets the rendered skeleton resolve the same attachment as the server rig.
inline bool ArrowAttachmentFrame(const ArticulatedPlayerHitRig& rig, PlayerHitRegion region,
                                 float heading, Vec3& origin, Vec3& x, Vec3& y, Vec3& z,
                                 float& radius, float& length) {
    using namespace HitRigDetail;
    for (const auto& prism : rig.prisms) {
        if (prism.region != region) continue;
        origin = prism.start;
        y = Subtract(prism.end, prism.start);
        length = std::sqrt(Dot(y, y));
        radius = prism.outerRadius;
        if (length < 0.001f || radius <= 0.0f) return false;
        y = Scale(y, 1.0f / length);
        Vec3 right{ std::cos(heading), 0.0f, -std::sin(heading) };
        x = Subtract(right, Scale(y, Dot(right, y)));
        if (Dot(x, x) < 0.01f) {
            right = { std::sin(heading), 0.0f, std::cos(heading) };
            x = Subtract(right, Scale(y, Dot(right, y)));
        }
        x = Scale(x, 1.0f / std::sqrt(Dot(x, x)));
        z = Cross(x, y);
        return true;
    }
    return false;
}

inline bool BindArrowToBody(ArrowBodyAttachment& attachment, const ArticulatedPlayerHitRig& rig,
                            float heading, const Vec3& position, const Vec3& direction) {
    using namespace HitRigDetail;
    Vec3 origin, x, y, z;
    float radius, length;
    if (!ArrowAttachmentFrame(rig, attachment.region, heading, origin, x, y, z, radius, length)) return false;
    const Vec3 offset = Subtract(position, origin);
    attachment.offset = { Dot(offset, x) / radius, Dot(offset, y) / length, Dot(offset, z) / radius };
    attachment.direction = { Dot(direction, x), Dot(direction, y), Dot(direction, z) };
    return true;
}

inline bool ResolveArrowOnBody(const ArrowBodyAttachment& attachment, const ArticulatedPlayerHitRig& rig,
                               float heading, Vec3& position, Vec3& direction) {
    using namespace HitRigDetail;
    Vec3 origin, x, y, z;
    float radius, length;
    if (!ArrowAttachmentFrame(rig, attachment.region, heading, origin, x, y, z, radius, length)) return false;
    position = Add(origin, Add(Scale(x, attachment.offset.x * radius),
                               Add(Scale(y, attachment.offset.y * length), Scale(z, attachment.offset.z * radius))));
    direction = Add(Scale(x, attachment.direction.x),
                     Add(Scale(y, attachment.direction.y), Scale(z, attachment.direction.z)));
    return true;
}

} // namespace Game::Simulation
