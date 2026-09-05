#include "ClientPlayerActionPresentationPolicy.h"
#include "platform/simulation/AuthoritativePlayerPose.h"

#include <cmath>

namespace Game::Multiplayer {

ClientEquipmentPresentation ClientPlayerActionPresentationPolicy::EquipmentForWeapon(
    uint8_t selectedWeapon) {
    switch (selectedWeapon) {
        case 1: return ClientEquipmentPresentation::MasterSwordAndShield;
        case 2: return ClientEquipmentPresentation::BiggoronSword;
        case 3: return ClientEquipmentPresentation::Bow;
        case 4: return ClientEquipmentPresentation::FishingPole;
        default: return ClientEquipmentPresentation::None;
    }
}

ClientPlayerActionPresentation ClientPlayerActionPresentationPolicy::Evaluate(
    const Game::Simulation::PlayerSnapshot& snapshot) {
    using Game::Simulation::PlayerPoseDirection;
    ClientPlayerActionPresentation presentation{};
    const auto pose =
        Game::Simulation::SampleAuthoritativePlayerPoseState(snapshot);
    presentation.equipment = EquipmentForWeapon(snapshot.selectedWeapon);
    if (snapshot.selectedWeapon == 1) {
        presentation.baseAnimation = ClientPlayerBaseAnimation::IdleSword;
    } else if (snapshot.selectedWeapon == 2) {
        presentation.baseAnimation = ClientPlayerBaseAnimation::IdleBiggoron;
    }
    presentation.dead = snapshot.health == 0;
    presentation.blocking = !presentation.dead &&
        snapshot.actionState == Game::Simulation::PlayerActionState::Blocking &&
        snapshot.selectedWeapon <= 2;
    presentation.bowReady = !presentation.dead && snapshot.selectedWeapon == 3 &&
        snapshot.actionState == Game::Simulation::PlayerActionState::Aiming;
    if (presentation.dead) {
        presentation.baseAnimation = ClientPlayerBaseAnimation::Dead;
        return presentation;
    }
    if (snapshot.locomotionMode ==
        Game::Simulation::PlayerLocomotionMode::Climbing) {
        presentation.baseAnimation = ClientPlayerBaseAnimation::Climb;
        return presentation;
    }
    if (snapshot.locomotionMode ==
        Game::Simulation::PlayerLocomotionMode::Swimming) {
        if (pose.direction == PlayerPoseDirection::None) {
            presentation.baseAnimation = ClientPlayerBaseAnimation::SwimIdle;
        } else if (pose.direction == PlayerPoseDirection::Left ||
                   pose.direction == PlayerPoseDirection::Right) {
            presentation.baseAnimation = pose.direction == PlayerPoseDirection::Left
                ? ClientPlayerBaseAnimation::SwimLeft
                : ClientPlayerBaseAnimation::SwimRight;
        } else {
            presentation.baseAnimation = pose.direction == PlayerPoseDirection::Backward
                ? ClientPlayerBaseAnimation::SwimBackward
                : ClientPlayerBaseAnimation::SwimForward;
        }
        return presentation;
    }
    if (snapshot.locomotionMode ==
        Game::Simulation::PlayerLocomotionMode::Airborne) {
        presentation.baseAnimation =
            snapshot.actionState == Game::Simulation::PlayerActionState::JumpSlashing
                ? ClientPlayerBaseAnimation::JumpSlash
                : ClientPlayerBaseAnimation::Falling;
        if (presentation.blocking) {
            presentation.upperAnimation = ClientPlayerUpperAnimation::Blocking;
        } else if (presentation.bowReady) {
            presentation.upperAnimation = ClientPlayerUpperAnimation::BowAiming;
        } else if (snapshot.selectedWeapon == 4) {
            presentation.upperAnimation = ClientPlayerUpperAnimation::Fishing;
        }
        return presentation;
    }
    if (snapshot.actionState >= Game::Simulation::PlayerActionState::PrimaryWindup &&
        snapshot.actionState <= Game::Simulation::PlayerActionState::PrimaryRecovery) {
        if (snapshot.selectedWeapon == 1 || snapshot.selectedWeapon == 2) {
            switch (snapshot.meleeAttackVariant) {
                case Game::Simulation::MeleeAttackVariant::ForwardSlash:
                    presentation.baseAnimation = ClientPlayerBaseAnimation::MeleeForwardSlash;
                    break;
                case Game::Simulation::MeleeAttackVariant::ForwardCombo:
                    presentation.baseAnimation = ClientPlayerBaseAnimation::MeleeForwardCombo;
                    break;
                case Game::Simulation::MeleeAttackVariant::RightSlash:
                    presentation.baseAnimation = ClientPlayerBaseAnimation::MeleeRightSlash;
                    break;
                case Game::Simulation::MeleeAttackVariant::RightCombo:
                    presentation.baseAnimation = ClientPlayerBaseAnimation::MeleeRightCombo;
                    break;
                case Game::Simulation::MeleeAttackVariant::LeftSlash:
                    presentation.baseAnimation = ClientPlayerBaseAnimation::MeleeLeftSlash;
                    break;
                case Game::Simulation::MeleeAttackVariant::LeftCombo:
                    presentation.baseAnimation = ClientPlayerBaseAnimation::MeleeLeftCombo;
                    break;
            }
            return presentation;
        }
    }
    if (snapshot.actionState ==
        Game::Simulation::PlayerActionState::SpinAttacking) {
        presentation.baseAnimation = snapshot.selectedWeapon == 2
            ? ClientPlayerBaseAnimation::BiggoronSpinAttack
            : ClientPlayerBaseAnimation::SwordSpinAttack;
        return presentation;
    }

    if (snapshot.actionState == Game::Simulation::PlayerActionState::Evading) {
        if (pose.direction == PlayerPoseDirection::Left ||
            pose.direction == PlayerPoseDirection::Right) {
            presentation.baseAnimation = pose.direction == PlayerPoseDirection::Left
                                             ? ClientPlayerBaseAnimation::EvadeLeft
                                             : ClientPlayerBaseAnimation::EvadeRight;
        } else {
            presentation.baseAnimation = ClientPlayerBaseAnimation::EvadeBackward;
        }
        return presentation;
    }

    // Holding primary after the swing leaves native Link in the charged
    // weapon-ready pose. It is presentation-only; combat still comes solely
    // from the authoritative attack phases above.
    if ((snapshot.heldActions & Game::Simulation::PLAYER_ACTION_PRIMARY) != 0) {
        if (snapshot.selectedWeapon == 1) {
            presentation.baseAnimation = ClientPlayerBaseAnimation::SwordHeld;
            return presentation;
        }
        if (snapshot.selectedWeapon == 2) {
            presentation.baseAnimation = ClientPlayerBaseAnimation::BiggoronHeld;
            return presentation;
        }
    }

    if (presentation.blocking && pose.direction == PlayerPoseDirection::None) {
        presentation.baseAnimation = snapshot.selectedWeapon == 1
            ? ClientPlayerBaseAnimation::BlockingSword
            : snapshot.selectedWeapon == 2
                ? ClientPlayerBaseAnimation::BlockingBiggoron
                : ClientPlayerBaseAnimation::BlockingFree;
    } else if (pose.direction != PlayerPoseDirection::None) {
        if (pose.direction == PlayerPoseDirection::Left ||
            pose.direction == PlayerPoseDirection::Right) {
            presentation.baseAnimation = pose.direction == PlayerPoseDirection::Left
                                              ? ClientPlayerBaseAnimation::StrafeLeft
                                              : ClientPlayerBaseAnimation::StrafeRight;
        } else {
            presentation.baseAnimation = pose.direction == PlayerPoseDirection::Backward
                                              ? ClientPlayerBaseAnimation::WalkBackward
                                              : ClientPlayerBaseAnimation::RunForward;
        }
    } else if (snapshot.selectedWeapon == 4) {
        presentation.baseAnimation = ClientPlayerBaseAnimation::Fishing;
    }

    if (presentation.blocking && pose.direction != PlayerPoseDirection::None) {
        presentation.upperAnimation = ClientPlayerUpperAnimation::Blocking;
    } else if (presentation.bowReady) {
        presentation.upperAnimation = ClientPlayerUpperAnimation::BowAiming;
    } else if (snapshot.selectedWeapon == 4) {
        presentation.upperAnimation = ClientPlayerUpperAnimation::Fishing;
    }
    return presentation;
}

} // namespace Game::Multiplayer
