#pragma once

#include "platform/simulation/PlayerSimulation.h"

#include <cstdint>

namespace Game::Multiplayer {

enum class ClientEquipmentPresentation : uint8_t {
    None,
    MasterSwordAndShield,
    BiggoronSword,
    Bow,
    FishingPole,
};

enum class ClientPlayerBaseAnimation : uint8_t {
    IdleFree,
    IdleSword,
    IdleBiggoron,
    BlockingFree,
    BlockingSword,
    BlockingBiggoron,
    RunForward,
    WalkBackward,
    StrafeLeft,
    StrafeRight,
    SwordHeld,
    BiggoronHeld,
    MeleeForwardSlash,
    MeleeForwardCombo,
    MeleeRightSlash,
    MeleeRightCombo,
    MeleeLeftSlash,
    MeleeLeftCombo,
    SwordSpinAttack,
    BiggoronSpinAttack,
    EvadeBackward,
    EvadeLeft,
    EvadeRight,
    Fishing,
    SwimIdle,
    SwimForward,
    SwimBackward,
    SwimLeft,
    SwimRight,
    Climb,
    Falling,
    JumpSlash,
    Dead,
};

enum class ClientPlayerUpperAnimation : uint8_t {
    None,
    Blocking,
    BowAiming,
    Fishing,
};

struct ClientPlayerActionPresentation {
    ClientEquipmentPresentation equipment = ClientEquipmentPresentation::None;
    bool dead = false;
    bool blocking = false;
    bool bowReady = false;
    ClientPlayerBaseAnimation baseAnimation = ClientPlayerBaseAnimation::IdleFree;
    ClientPlayerUpperAnimation upperAnimation = ClientPlayerUpperAnimation::None;
};

class ClientPlayerActionPresentationPolicy final {
  public:
    static ClientEquipmentPresentation EquipmentForWeapon(uint8_t selectedWeapon);
    static ClientPlayerActionPresentation Evaluate(
        const Game::Simulation::PlayerSnapshot& snapshot);
};

} // namespace Game::Multiplayer
