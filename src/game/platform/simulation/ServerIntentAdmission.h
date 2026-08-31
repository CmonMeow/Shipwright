#pragma once

#include <cstdint>
#include <compare>
#include <map>

namespace Game::Simulation {

enum class ServerIntentKind : uint8_t {
    Projectile,
    Fish,
    Lure,
    WeaponSelection,
    SceneEntry,
    Structure,
};

enum class ServerIntentResult : uint8_t {
    Fresh,
    Duplicate,
    Stale,
    Invalid,
};

// Owns transport-independent replay floors and deterministic action cooldowns.
// Admission is incarnation-aware: the authoritative life epoch must match the
// intent epoch, and a new authoritative life starts with fresh sequence floors
// and cooldowns. A sane current-life intent is consumed before other world-state
// validation so a rejected packet cannot execute later after equipment or scene
// state changes.
class ServerIntentAdmission final {
  public:
    ServerIntentResult Admit(int32_t playerId, uint32_t authoritativeLifeEpoch,
                             uint32_t intentLifeEpoch, ServerIntentKind kind,
                             uint32_t sequence);
    bool CooldownReady(int32_t playerId, ServerIntentKind kind,
                       uint64_t serverTick) const;
    void RecordAccepted(int32_t playerId, ServerIntentKind kind,
                        uint64_t serverTick);
    void RemovePlayer(int32_t playerId);
    void Reset();

    static constexpr uint64_t CooldownTicks(ServerIntentKind kind) {
        switch (kind) {
            case ServerIntentKind::Projectile:
                return 9;
            case ServerIntentKind::Structure:
                return 15;
            default:
                return 0;
        }
    }

  private:
    struct IntentKey {
        int32_t playerId = -1;
        ServerIntentKind kind = ServerIntentKind::Projectile;

        constexpr auto operator<=>(const IntentKey&) const = default;
    };

    std::map<IntentKey, uint32_t> mLatestSequences;
    std::map<int32_t, uint32_t> mLifeEpochs;
    std::map<IntentKey, uint64_t> mLastAcceptedTicks;
};

} // namespace Game::Simulation
