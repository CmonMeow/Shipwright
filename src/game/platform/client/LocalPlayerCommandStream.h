#pragma once

#include "../simulation/ClientPrediction.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace Game::Client {

struct LocalPlayerInputSample {
    uint32_t clientTick = 0;
    uint32_t lifeEpoch = 0;
    int32_t sceneId = -1;
    // Native OoT stick axis: positive is left, negative is right.
    float moveX = 0.0f;
    float moveY = 0.0f;
    float headingRadians = 0.0f;
    float aimPitchRadians = 0.0f;
    uint16_t heldActions = 0;
    uint16_t pressedActions = 0;
    Simulation::MeleeAttackVariant meleeAttackVariant =
        Simulation::MeleeAttackVariant::RightSlash;
    bool hasMeleeAttackVariant = false;
    uint8_t selectedWeapon = 0;
    Simulation::Vec3 position{};
    Simulation::PlayerLocomotionMode locomotionMode =
        Simulation::PlayerLocomotionMode::Grounded;
    bool hasPose = false;
};

struct LocalWeaponSelectionRequest {
    uint32_t sequence = 0;
    uint8_t selectedWeapon = 0;
};

enum class LocalPlayerCommandSubmission : uint8_t {
    NoCommand,
    TransportRejected,
    Submitted,
};

using LocalPlayerCommandSender =
    std::function<bool(const Simulation::PlayerCommand&)>;

// Owns the ordered local command stream before prediction or serialization.
// A native input sample can create at most one movement command and at most
// one independently sequenced action edge.
class LocalPlayerCommandStream final {
  public:
    explicit LocalPlayerCommandStream(uint32_t nextCommandSequence = 1,
                                      uint32_t nextActionSequence = 1);

    std::optional<Simulation::PlayerCommand> Build(const LocalPlayerInputSample& sample);
    LocalPlayerCommandSubmission Submit(
        const LocalPlayerInputSample& sample, float sampleDeltaSeconds,
        const LocalPlayerCommandSender& sender,
        Simulation::ClientPrediction& prediction,
        bool predictMovement = true);
    std::optional<LocalWeaponSelectionRequest> PrepareWeaponSelection(uint8_t selectedWeapon);
    void ResolveWeaponSelection(uint32_t sequence, bool sent);
    void ObserveAuthoritativeWeapon(uint8_t selectedWeapon, uint32_t serverTick);
    bool WeaponSelectionConfirmed(uint8_t selectedWeapon) const;
    void BeginLife();
    void Reset();

  private:
    static uint32_t TakeNonZeroSequence(uint32_t& next);

    uint32_t mNextCommandSequence = 1;
    uint32_t mNextActionSequence = 1;
    uint32_t mLastClientTick = 0;
    uint32_t mLastLifeEpoch = 0;
    int32_t mLastSceneId = -1;
    bool mHasLastSample = false;
    uint32_t mNextWeaponSelectionSequence = 1;
    uint8_t mLastSentWeapon = 0xFF;
    uint8_t mAuthoritativeWeapon = 0xFF;
    uint32_t mLastAuthoritativeServerTick = 0;
    uint32_t mSelectionRequestedAfterTick = 0;
    bool mAwaitingWeaponConfirmation = false;
    std::optional<LocalWeaponSelectionRequest> mOfferedWeapon;
};

} // namespace Game::Client
