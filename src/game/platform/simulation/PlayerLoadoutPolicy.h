#pragma once

#include "PlayerSimulation.h"

#include <cstdint>
#include <map>
#include <optional>

namespace Game::Simulation {

enum class PlayerWeaponSlot : uint8_t {
    None = 0,
    OneHandedSword = 1,
    TwoHandedSword = 2,
    Bow = 3,
    FishingPole = 4,
};

constexpr uint8_t WeaponSlotMask(PlayerWeaponSlot slot) {
    return static_cast<uint8_t>(1U << static_cast<uint8_t>(slot));
}

struct PlayerLoadout {
    uint8_t allowedWeaponMask = 0x1F;

    bool Allows(uint8_t weapon) const;
    uint8_t FallbackWeapon(uint8_t currentWeapon) const;
};

struct PlayerLoadoutDecision {
    PlayerCommand command{};
    bool authoritativeWeaponAllowed = false;
};

// Server-owned equipment policy. Movement commands never select equipment.
// They are normalized against the already-authoritative slot, with action bits
// that the equipped weapon cannot perform removed before simulation.
class PlayerLoadoutPolicy final {
  public:
    bool EnsurePlayer(int32_t playerId);
    bool ConfigurePlayer(int32_t playerId, const PlayerLoadout& loadout);
    std::optional<PlayerLoadoutDecision> Evaluate(
        const PlayerSnapshot& player, const PlayerCommand& command) const;
    bool AllowsSelection(int32_t playerId, uint8_t selectedWeapon) const;
    std::optional<PlayerLoadout> LoadoutForPlayer(int32_t playerId) const;
    void RemovePlayer(int32_t playerId);
    void Reset();

  private:
    static bool IsValid(const PlayerLoadout& loadout);

    std::map<int32_t, PlayerLoadout> mLoadouts;
};

} // namespace Game::Simulation
