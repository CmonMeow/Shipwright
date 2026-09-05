#pragma once

#include "FishingPresentationState.h"

#include <cmath>

namespace Game::Replication {

inline constexpr float kFishingLureOffsetLimit = 5000.0f;
inline constexpr float kFishingHookOffsetFromLureLimit = 64.0f;
inline constexpr float kFishingRodBendLimit = 4.0f;
inline constexpr float kFishingRodTwistLimit = 8.0f;
inline constexpr float kFishingRodCastLimit = 64.0f;
inline constexpr float kFishingRotationLimit = 8.0f;
inline constexpr float kFishingLureSpinLimit = 8.0f;
inline constexpr float kFishingLureZOffsetLimit = 2000.0f;
inline constexpr float kFishingLineScaleMinimum = 0.0001f;
inline constexpr float kFishingLineScaleMaximum = 0.01f;
inline constexpr float kFishingLineGravityMaximum = 520.0f;
inline constexpr uint8_t kFishingLinePointCount = 200;

inline bool FishingPresentationPoseIsBounded(
    const FishingPresentationState& presentation) {
    const auto finite = [](float value) { return std::isfinite(value); };
    const auto insideRadius = [&finite](const std::array<float, 3>& value,
                                       float radius) {
        if (!finite(value[0]) || !finite(value[1]) || !finite(value[2])) {
            return false;
        }
        return value[0] * value[0] + value[1] * value[1] +
                   value[2] * value[2] <=
               radius * radius;
    };

    if (presentation.state > 5 ||
        presentation.lineSpooled >= kFishingLinePointCount ||
        presentation.sinkingLureSegmentIndex >= 20 ||
        presentation.sinkingLureUnderwater > 1 ||
        !insideRadius(presentation.lureDrawOffset,
                      kFishingLureOffsetLimit) ||
        !finite(presentation.rodBendY) ||
        std::abs(presentation.rodBendY) > kFishingRodBendLimit ||
        !finite(presentation.rodBendX) ||
        std::abs(presentation.rodBendX) > kFishingRodBendLimit ||
        !finite(presentation.rodTwist) ||
        std::abs(presentation.rodTwist) > kFishingRodTwistLimit ||
        !finite(presentation.rodCastX) ||
        std::abs(presentation.rodCastX) > kFishingRodCastLimit ||
        !finite(presentation.lureSpin) ||
        std::abs(presentation.lureSpin) > kFishingLureSpinLimit ||
        !finite(presentation.lureZOffset) ||
        std::abs(presentation.lureZOffset) > kFishingLureZOffsetLimit ||
        !finite(presentation.lineScale) ||
        !finite(presentation.lineGravity) ||
        presentation.lineGravity < 0.0f ||
        presentation.lineGravity > kFishingLineGravityMaximum ||
        (presentation.state != 0 &&
         (presentation.lineScale < kFishingLineScaleMinimum ||
          presentation.lineScale > kFishingLineScaleMaximum))) {
        return false;
    }

    for (const float rotation : presentation.lureRotation) {
        if (!finite(rotation) ||
            std::abs(rotation) > kFishingRotationLimit) {
            return false;
        }
    }
    for (size_t hook = 0; hook < presentation.lureHookOffsets.size(); ++hook) {
        if (!insideRadius(presentation.lureHookOffsets[hook],
                          kFishingLureOffsetLimit)) {
            return false;
        }
        for (const float rotation : presentation.lureHookRotations[hook]) {
            if (!finite(rotation) ||
                std::abs(rotation) > kFishingRotationLimit) {
                return false;
            }
        }
    }
    return true;
}

} // namespace Game::Replication
