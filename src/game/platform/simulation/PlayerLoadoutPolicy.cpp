#include "PlayerLoadoutPolicy.h"

namespace Game::Simulation {
namespace {

constexpr uint8_t kKnownWeaponMask = 0x1F;
constexpr uint16_t kWeaponActions = PLAYER_ACTION_PRIMARY | PLAYER_ACTION_BLOCK |
                                    PLAYER_ACTION_AIM;

uint16_t ActionsForWeapon(uint8_t selectedWeapon) {
    switch (static_cast<PlayerWeaponSlot>(selectedWeapon)) {
        case PlayerWeaponSlot::OneHandedSword:
        case PlayerWeaponSlot::TwoHandedSword:
            return PLAYER_ACTION_PRIMARY | PLAYER_ACTION_BLOCK;
        case PlayerWeaponSlot::Bow:
            return PLAYER_ACTION_PRIMARY | PLAYER_ACTION_AIM;
        default:
            return 0;
    }
}

} // namespace

bool PlayerLoadout::Allows(uint8_t weapon) const {
    return weapon <= static_cast<uint8_t>(PlayerWeaponSlot::FishingPole) &&
           (allowedWeaponMask & static_cast<uint8_t>(1U << weapon)) != 0;
}

uint8_t PlayerLoadout::FallbackWeapon(uint8_t currentWeapon) const {
    if (Allows(currentWeapon)) return currentWeapon;
    for (uint8_t weapon = 0;
         weapon <= static_cast<uint8_t>(PlayerWeaponSlot::FishingPole); ++weapon) {
        if (Allows(weapon)) return weapon;
    }
    return static_cast<uint8_t>(PlayerWeaponSlot::None);
}

bool PlayerLoadoutPolicy::EnsurePlayer(int32_t playerId) {
    if (playerId < 0) return false;
    mLoadouts.try_emplace(playerId, PlayerLoadout{});
    return true;
}

bool PlayerLoadoutPolicy::ConfigurePlayer(int32_t playerId,
                                          const PlayerLoadout& loadout) {
    if (playerId < 0 || !IsValid(loadout) || mLoadouts.count(playerId) == 0) {
        return false;
    }
    mLoadouts[playerId] = loadout;
    return true;
}

std::optional<PlayerLoadoutDecision> PlayerLoadoutPolicy::Evaluate(
    const PlayerSnapshot& player, const PlayerCommand& command) const {
    const auto found = mLoadouts.find(player.ownerPlayerId);
    if (found == mLoadouts.end() || command.ownerPlayerId != player.ownerPlayerId) {
        return std::nullopt;
    }

    PlayerLoadoutDecision decision{ command, found->second.Allows(player.selectedWeapon) };
    const uint8_t authoritativeWeapon =
        found->second.FallbackWeapon(player.selectedWeapon);
    const uint16_t allowedActions = decision.authoritativeWeaponAllowed
                                        ? ActionsForWeapon(authoritativeWeapon)
                                        : 0;
    decision.command.heldActions &= static_cast<uint16_t>(~(kWeaponActions & ~allowedActions));
    decision.command.pressedActions &= static_cast<uint16_t>(~(kWeaponActions & ~allowedActions));
    if (decision.command.pressedActions == 0) decision.command.actionSequence = 0;
    return decision;
}

bool PlayerLoadoutPolicy::AllowsSelection(int32_t playerId, uint8_t selectedWeapon) const {
    const auto found = mLoadouts.find(playerId);
    return found != mLoadouts.end() && found->second.Allows(selectedWeapon);
}

std::optional<PlayerLoadout> PlayerLoadoutPolicy::LoadoutForPlayer(int32_t playerId) const {
    const auto found = mLoadouts.find(playerId);
    return found == mLoadouts.end() ? std::nullopt
                                    : std::optional<PlayerLoadout>(found->second);
}

void PlayerLoadoutPolicy::RemovePlayer(int32_t playerId) {
    mLoadouts.erase(playerId);
}

void PlayerLoadoutPolicy::Reset() {
    mLoadouts.clear();
}

bool PlayerLoadoutPolicy::IsValid(const PlayerLoadout& loadout) {
    return loadout.allowedWeaponMask != 0 &&
           (loadout.allowedWeaponMask & static_cast<uint8_t>(~kKnownWeaponMask)) == 0;
}

} // namespace Game::Simulation
