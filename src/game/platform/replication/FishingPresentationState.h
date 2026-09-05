#pragma once

#include "../simulation/EntityId.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Game::Replication {

constexpr size_t kFishingPresentationHookCount = 2;
constexpr size_t kFishingPresentationFishLimbCount = 8;

// Disposable render telemetry. This describes how a fishing action should be
// drawn; authoritative lure, fish, player, and weapon state live in simulation.
struct FishingPresentationState {
    int32_t playerId = -1;
    Simulation::EntityId entity{};
    int32_t sceneId = -1;
    uint32_t lifeEpoch = 0;
    uint32_t sequence = 0;
    uint8_t state = 0;
    std::array<float, 3> lureDrawOffset{};
    float rodBendY = 0.0f;
    float rodBendX = 0.0f;
    float rodTwist = 0.0f;
    float rodCastX = 0.0f;
    std::array<float, 3> lureRotation{};
    float lureSpin = 0.0f;
    float lureZOffset = 0.0f;
    std::array<std::array<float, 3>, kFishingPresentationHookCount> lureHookOffsets{};
    std::array<std::array<float, 2>, kFishingPresentationHookCount> lureHookRotations{};
    float lineScale = 0.0f;
    float lineGravity = 0.0f;
    uint8_t lineSpooled = 0;
    uint8_t sinkingLureSegmentIndex = 0;
    uint8_t sinkingLureUnderwater = 0;
    std::array<int16_t, 3> fishRotation{};
    std::array<int16_t, kFishingPresentationFishLimbCount> fishLimbRotation{};
};

// Unbound cosmetic telemetry crossing authenticated server ingress. Transport
// supplies only the life epoch; player/entity/scene fields in presentation are
// deliberately untrusted until ingress replaces them from ServerWorld.
struct FishingPresentationIntent {
    uint32_t lifeEpoch = 0;
    FishingPresentationState presentation{};
};

enum class FishingPresentationUpdateResult : uint8_t {
    Accepted,
    Stale,
    Invalid,
};

} // namespace Game::Replication
