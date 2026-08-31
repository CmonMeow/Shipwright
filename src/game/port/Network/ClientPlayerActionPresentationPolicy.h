#pragma once

#include "../../platform/simulation/PlayerSimulation.h"

#include <cstdint>

namespace SoH::Network {

enum class ClientEquipmentPresentation : uint8_t {
    None,
    MasterSwordAndShield,
    BiggoronSword,
    Bow,
    FishingPole,
};

enum class ClientPlayerBaseAnimation : uint8_t {
    Idle,
    RunForward,
    WalkBackward,
    StrafeLeft,
    StrafeRight,
    SwordAttack,
    BiggoronAttack,
    EvadeBackward,
    EvadeLeft,
    EvadeRight,
    Fishing,
    SwimIdle,
    SwimForward,
    SwimBackward,
    SwimLeft,
    SwimRight,
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
    ClientPlayerBaseAnimation baseAnimation = ClientPlayerBaseAnimation::Idle;
    ClientPlayerUpperAnimation upperAnimation = ClientPlayerUpperAnimation::None;
};

class ClientPlayerActionPresentationPolicy final {
  public:
    static ClientEquipmentPresentation EquipmentForWeapon(uint8_t selectedWeapon);
    static ClientPlayerActionPresentation Evaluate(
        const Game::Simulation::PlayerSnapshot& snapshot);
};

} // namespace SoH::Network
