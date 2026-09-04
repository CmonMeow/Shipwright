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

inline constexpr uint8_t DamageForPlayerHitRegion(
    uint8_t baseDamage, PlayerHitRegion region) {
    switch (region) {
        case PlayerHitRegion::Head:
            return baseDamage > 127
                       ? static_cast<uint8_t>(255)
                       : static_cast<uint8_t>(baseDamage * 2);
        case PlayerHitRegion::LeftUpperArm:
        case PlayerHitRegion::LeftForearm:
        case PlayerHitRegion::RightUpperArm:
        case PlayerHitRegion::RightForearm:
        case PlayerHitRegion::LeftThigh:
        case PlayerHitRegion::LeftShin:
        case PlayerHitRegion::RightThigh:
        case PlayerHitRegion::RightShin:
            return static_cast<uint8_t>((baseDamage + 1) / 2);
        case PlayerHitRegion::None:
        case PlayerHitRegion::Torso:
        case PlayerHitRegion::Waist:
        default:
            return baseDamage;
    }
}

} // namespace Game::Simulation
