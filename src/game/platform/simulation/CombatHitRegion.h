#pragma once

#include <cstdint>

namespace Game::Simulation {

enum class PlayerHitRegion : uint8_t {
    None,
    Head,
    Torso,
    Waist,
    LeftUpperArm,
    LeftForearm,
    RightUpperArm,
    RightForearm,
    LeftThigh,
    LeftShin,
    RightThigh,
    RightShin,
};

inline constexpr uint8_t kPlayerHitRegionCount =
    static_cast<uint8_t>(PlayerHitRegion::RightShin) + 1;

} // namespace Game::Simulation
