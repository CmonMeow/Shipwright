#pragma once

#include "PlayerSimulation.h"

#include <cmath>

namespace Game::Simulation {

struct ShieldHit {
    float segmentRatio = 0.0f;
    Vec3 position{};
};

inline bool SegmentVerticalCylinderFirstHit(const Vec3& start, const Vec3& end,
                                            const Vec3& base, float radius, float height,
                                            float& hitRatio) {
    const float deltaX = end.x - start.x;
    const float deltaY = end.y - start.y;
    const float deltaZ = end.z - start.z;
    const float relativeX = start.x - base.x;
    const float relativeZ = start.z - base.z;
    const float radiusSquared = radius * radius;
    const float top = base.y + height;
    float closestRatio = 2.0f;

    const auto inside = [&](float ratio) {
        const float y = start.y + deltaY * ratio;
        const float x = relativeX + deltaX * ratio;
        const float z = relativeZ + deltaZ * ratio;
        return y >= base.y && y <= top && x * x + z * z <= radiusSquared;
    };
    if (inside(0.0f)) {
        hitRatio = 0.0f;
        return true;
    }
    const auto accept = [&](float ratio) {
        if (ratio >= 0.0f && ratio <= 1.0f && ratio < closestRatio && inside(ratio)) {
            closestRatio = ratio;
        }
    };
    const float quadraticA = deltaX * deltaX + deltaZ * deltaZ;
    if (quadraticA > 0.00001f) {
        const float quadraticB = 2.0f * (relativeX * deltaX + relativeZ * deltaZ);
        const float quadraticC = relativeX * relativeX + relativeZ * relativeZ - radiusSquared;
        const float discriminant = quadraticB * quadraticB - 4.0f * quadraticA * quadraticC;
        if (discriminant >= 0.0f) {
            const float root = std::sqrt(discriminant);
            accept((-quadraticB - root) / (2.0f * quadraticA));
            accept((-quadraticB + root) / (2.0f * quadraticA));
        }
    }
    if (std::fabs(deltaY) > 0.00001f) {
        accept((base.y - start.y) / deltaY);
        accept((top - start.y) / deltaY);
    }
    if (closestRatio > 1.0f) return false;
    hitRatio = closestRatio;
    return true;
}

inline bool SegmentOrientedVerticalRectangleFirstHit(
    const Vec3& start, const Vec3& end, const Vec3& center,
    float headingRadians, float halfWidth, float halfHeight,
    ShieldHit& hit) {
    constexpr float epsilon = 0.00001f;

    const Vec3 forward{ std::sin(headingRadians), 0.0f,
                        std::cos(headingRadians) };
    const Vec3 right{ std::cos(headingRadians), 0.0f,
                     -std::sin(headingRadians) };
    const Vec3 direction{ end.x - start.x, end.y - start.y, end.z - start.z };
    const float denominator = direction.x * forward.x + direction.z * forward.z;
    if (std::fabs(denominator) < epsilon) return false;

    const Vec3 centerOffset{ center.x - start.x, center.y - start.y, center.z - start.z };
    const float ratio = (centerOffset.x * forward.x + centerOffset.z * forward.z) /
                        denominator;
    if (ratio < 0.0f || ratio > 1.0f) return false;

    const Vec3 impact{ start.x + direction.x * ratio,
                       start.y + direction.y * ratio,
                       start.z + direction.z * ratio };
    const Vec3 offset{ impact.x - center.x, impact.y - center.y, impact.z - center.z };
    const float horizontal = offset.x * right.x + offset.z * right.z;
    if (std::fabs(horizontal) > halfWidth || std::fabs(offset.y) > halfHeight) return false;

    hit = { ratio, impact };
    return true;
}

} // namespace Game::Simulation
