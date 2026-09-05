#include "PlayerSimulationNetworkAdapter.h"

#include <algorithm>
#include <cmath>

namespace Game::Multiplayer::PlayerSimulationNetworkAdapter {
namespace {

constexpr float kBinaryAngleToRadians = 3.14159265358979323846f / 32768.0f;
constexpr float kRadiansToBinaryAngle = 32768.0f / 3.14159265358979323846f;

int16_t ToBinaryAngle(float radians) {
    return static_cast<int16_t>(std::lround(radians * kRadiansToBinaryAngle));
}

} // namespace

Game::Simulation::PlayerCommand ToCommand(const NetworkPlayerCommandPacket& packet) {
    Game::Simulation::PlayerCommand command{
        -1, packet.sequence, packet.actionSequence, packet.lifeEpoch,
        packet.clientTick, -1,
        std::clamp(static_cast<float>(packet.moveX) / 85.0f, -1.0f, 1.0f),
        std::clamp(static_cast<float>(packet.moveY) / 85.0f, -1.0f, 1.0f),
        static_cast<float>(packet.heading) * kBinaryAngleToRadians,
        static_cast<float>(packet.aimPitch) * kBinaryAngleToRadians,
        packet.heldActions, packet.pressedActions,
        static_cast<Game::Simulation::MeleeAttackVariant>(
            packet.meleeAttackVariant),
        packet.hasMeleeAttackVariant != 0,
        { packet.x, packet.y, packet.z },
        static_cast<Game::Simulation::PlayerLocomotionMode>(
            packet.locomotionMode),
        packet.hasPose != 0
    };
    return command;
}

Game::Simulation::WeaponSelectionCommand ToCommand(
    const NetworkWeaponSelectionIntentPacket& packet) {
    return { -1, packet.sequence, packet.lifeEpoch, packet.selectedWeapon };
}

Game::Simulation::PlayerSnapshot ToSnapshot(
    const NetworkPlayerSnapshotPacket& packet) {
    Game::Simulation::PlayerSnapshot snapshot{
             { packet.entityIndex, packet.entityGeneration },
             packet.playerId,
             packet.sceneId,
             packet.serverTick,
             packet.lastProcessedCommand,
             packet.lifeEpoch,
             { packet.x, packet.y, packet.z },
             { packet.velocityX, packet.velocityY, packet.velocityZ },
             static_cast<float>(packet.heading) * kBinaryAngleToRadians,
             static_cast<float>(packet.aimPitch) * kBinaryAngleToRadians,
              packet.heldActions,
              packet.selectedWeapon,
              static_cast<Game::Simulation::PlayerLocomotionMode>(packet.locomotionMode),
              static_cast<Game::Simulation::PlayerActionState>(packet.actionState),
              static_cast<Game::Simulation::MeleeAttackVariant>(packet.meleeAttackVariant),
             packet.meleeAttackId,
             packet.actionStartTick,
             packet.health,
             static_cast<Game::Simulation::TeamId>(packet.team),
             packet.locomotionPhaseRadians };
    return snapshot;
}

NetworkPlayerCommandPacket ToPacket(const Game::Simulation::PlayerCommand& command) {
    NetworkPlayerCommandPacket packet{ command.sequence,
             command.actionSequence,
             command.lifeEpoch,
             command.clientTick,
             static_cast<int8_t>(std::lround(std::clamp(command.moveX, -1.0f, 1.0f) * 85.0f)),
             static_cast<int8_t>(std::lround(std::clamp(command.moveY, -1.0f, 1.0f) * 85.0f)),
             ToBinaryAngle(command.headingRadians),
             ToBinaryAngle(command.aimPitchRadians),
             command.heldActions,
             command.pressedActions,
             static_cast<unsigned char>(command.reportedMeleeAttackVariant),
             static_cast<unsigned char>(command.hasReportedMeleeAttackVariant),
             command.reportedPosition.x,
             command.reportedPosition.y,
             command.reportedPosition.z,
             static_cast<unsigned char>(command.reportedLocomotionMode),
             static_cast<unsigned char>(command.hasReportedPose) };
    return packet;
}

NetworkPlayerSnapshotPacket ToPacket(const Game::Simulation::PlayerSnapshot& snapshot) {
    NetworkPlayerSnapshotPacket packet{ snapshot.ownerPlayerId,
             snapshot.entity.index,
             snapshot.entity.generation,
             snapshot.sceneId,
             snapshot.serverTick,
             snapshot.lastProcessedCommand,
             snapshot.lifeEpoch,
             snapshot.position.x,
             snapshot.position.y,
             snapshot.position.z,
             snapshot.velocity.x,
             snapshot.velocity.y,
             snapshot.velocity.z,
             ToBinaryAngle(snapshot.headingRadians),
             ToBinaryAngle(snapshot.aimPitchRadians),
             snapshot.heldActions,
             snapshot.selectedWeapon,
             static_cast<unsigned char>(snapshot.actionState),
             static_cast<unsigned char>(snapshot.meleeAttackVariant),
             snapshot.meleeAttackId,
             snapshot.actionStartTick,
              snapshot.health,
              static_cast<unsigned char>(snapshot.team),
              static_cast<unsigned char>(snapshot.locomotionMode),
              snapshot.locomotionPhaseRadians };
    return packet;
}

bool IsSane(const NetworkPlayerCommandPacket& packet) {
    constexpr uint16_t knownActions = NETWORK_ACTION_PRIMARY | NETWORK_ACTION_BLOCK |
                                      NETWORK_ACTION_AIM | NETWORK_ACTION_EVADE;
    return packet.sequence != 0 && packet.lifeEpoch != 0 && packet.moveX >= -85 &&
           packet.moveX <= 85 && packet.moveY >= -85 && packet.moveY <= 85 &&
           (packet.heldActions & ~knownActions) == 0 &&
           (packet.pressedActions & ~knownActions) == 0 &&
           packet.hasMeleeAttackVariant <= 1 &&
           packet.meleeAttackVariant <= static_cast<unsigned char>(
               Game::Simulation::MeleeAttackVariant::LeftCombo) &&
           (!packet.hasMeleeAttackVariant ||
            ((packet.pressedActions & NETWORK_ACTION_PRIMARY) != 0)) &&
           packet.hasPose <= 1 &&
           packet.locomotionMode <= static_cast<unsigned char>(
               Game::Simulation::PlayerLocomotionMode::Climbing) &&
           (!packet.hasPose ||
            (std::isfinite(packet.x) && std::isfinite(packet.y) &&
             std::isfinite(packet.z) && std::abs(packet.x) < 32000.0f &&
             std::abs(packet.y) < 32000.0f && std::abs(packet.z) < 32000.0f)) &&
           ((packet.pressedActions == 0 && packet.actionSequence == 0) ||
           (packet.pressedActions != 0 && packet.actionSequence != 0));
}

bool IsSane(const NetworkWeaponSelectionIntentPacket& packet) {
    return packet.sequence != 0 && packet.lifeEpoch != 0 &&
           packet.selectedWeapon <= 4;
}

bool IsSane(const Game::Simulation::PlayerCommand& command) {
    constexpr uint16_t knownActions = Game::Simulation::PLAYER_ACTION_PRIMARY |
                                      Game::Simulation::PLAYER_ACTION_BLOCK |
                                      Game::Simulation::PLAYER_ACTION_AIM |
                                      Game::Simulation::PLAYER_ACTION_EVADE;
    constexpr float kMaximumAngleMagnitude = 4.0f * 3.14159265358979323846f;
    return command.sequence != 0 && command.lifeEpoch != 0 &&
           std::isfinite(command.moveX) && std::isfinite(command.moveY) &&
           command.moveX >= -1.0f && command.moveX <= 1.0f &&
           command.moveY >= -1.0f && command.moveY <= 1.0f &&
           std::isfinite(command.headingRadians) &&
           std::abs(command.headingRadians) <= kMaximumAngleMagnitude &&
           std::isfinite(command.aimPitchRadians) &&
           std::abs(command.aimPitchRadians) <= kMaximumAngleMagnitude &&
           (!command.hasReportedPose ||
            (std::isfinite(command.reportedPosition.x) &&
             std::isfinite(command.reportedPosition.y) &&
             std::isfinite(command.reportedPosition.z) &&
             std::abs(command.reportedPosition.x) < 32000.0f &&
             std::abs(command.reportedPosition.y) < 32000.0f &&
             std::abs(command.reportedPosition.z) < 32000.0f &&
             command.reportedLocomotionMode <=
                 Game::Simulation::PlayerLocomotionMode::Climbing)) &&
           (command.heldActions & ~knownActions) == 0 &&
           (command.pressedActions & ~knownActions) == 0 &&
           command.reportedMeleeAttackVariant <=
               Game::Simulation::MeleeAttackVariant::LeftCombo &&
           (!command.hasReportedMeleeAttackVariant ||
            ((command.pressedActions &
              Game::Simulation::PLAYER_ACTION_PRIMARY) != 0)) &&
           ((command.pressedActions == 0 && command.actionSequence == 0) ||
            (command.pressedActions != 0 && command.actionSequence != 0));
}

bool IsSane(const NetworkPlayerSnapshotPacket& packet) {
    constexpr uint16_t knownActions = NETWORK_ACTION_PRIMARY | NETWORK_ACTION_BLOCK |
                                      NETWORK_ACTION_AIM | NETWORK_ACTION_EVADE;
    const auto saneFloat = [](float value) {
        return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
    };
    return packet.playerId >= 0 && packet.entityGeneration != 0 &&
           packet.lifeEpoch != 0 && packet.sceneId >= 0 && packet.sceneId < 4096 &&
           packet.serverTick != 0 && saneFloat(packet.x) && saneFloat(packet.y) &&
           saneFloat(packet.z) && saneFloat(packet.velocityX) &&
           saneFloat(packet.velocityY) && saneFloat(packet.velocityZ) &&
           (packet.heldActions & ~knownActions) == 0 && packet.selectedWeapon <= 4 &&
           packet.actionState <=
               static_cast<unsigned char>(Game::Simulation::PlayerActionState::SpinAttacking) &&
           packet.meleeAttackVariant <= static_cast<unsigned char>(
               Game::Simulation::MeleeAttackVariant::LeftCombo) &&
            packet.actionStartTick <= packet.serverTick && packet.health <= 48 &&
            packet.team <= NETWORK_TEAM_GREEN &&
            packet.locomotionMode <= static_cast<unsigned char>(
                Game::Simulation::PlayerLocomotionMode::Climbing) &&
            std::isfinite(packet.locomotionPhaseRadians) &&
            packet.locomotionPhaseRadians >= 0.0f &&
            packet.locomotionPhaseRadians < 6.28318530717958647692f;
}

} // namespace Game::Multiplayer::PlayerSimulationNetworkAdapter
