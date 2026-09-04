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
    return { -1,
             packet.sequence,
             packet.actionSequence,
             packet.lifeEpoch,
             -1,
             std::clamp(static_cast<float>(packet.moveX) / 85.0f, -1.0f, 1.0f),
             std::clamp(static_cast<float>(packet.moveY) / 85.0f, -1.0f, 1.0f),
             static_cast<float>(packet.heading) * kBinaryAngleToRadians,
             static_cast<float>(packet.aimPitch) * kBinaryAngleToRadians,
             packet.heldActions,
             packet.pressedActions };
}

Game::Simulation::WeaponSelectionCommand ToCommand(
    const NetworkWeaponSelectionIntentPacket& packet) {
    return { -1, packet.sequence, packet.lifeEpoch, packet.selectedWeapon };
}

Game::Simulation::PlayerSnapshot ToSnapshot(
    const NetworkPlayerSnapshotPacket& packet) {
    return { { packet.entityIndex, packet.entityGeneration },
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
             packet.actionStartTick,
             packet.health,
             static_cast<Game::Simulation::TeamId>(packet.team),
             packet.locomotionPhaseRadians };
}

NetworkPlayerCommandPacket ToPacket(const Game::Simulation::PlayerCommand& command) {
    return { command.sequence,
             command.actionSequence,
             command.lifeEpoch,
             static_cast<int8_t>(std::lround(std::clamp(command.moveX, -1.0f, 1.0f) * 85.0f)),
             static_cast<int8_t>(std::lround(std::clamp(command.moveY, -1.0f, 1.0f) * 85.0f)),
             ToBinaryAngle(command.headingRadians),
             ToBinaryAngle(command.aimPitchRadians),
             command.heldActions,
             command.pressedActions };
}

NetworkPlayerSnapshotPacket ToPacket(const Game::Simulation::PlayerSnapshot& snapshot) {
    return { snapshot.ownerPlayerId,
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
             snapshot.actionStartTick,
              snapshot.health,
              static_cast<unsigned char>(snapshot.team),
              static_cast<unsigned char>(snapshot.locomotionMode),
              snapshot.locomotionPhaseRadians };
}

bool IsSane(const NetworkPlayerCommandPacket& packet) {
    constexpr uint16_t knownActions = NETWORK_ACTION_PRIMARY | NETWORK_ACTION_BLOCK |
                                      NETWORK_ACTION_AIM | NETWORK_ACTION_EVADE;
    return packet.sequence != 0 && packet.lifeEpoch != 0 && packet.moveX >= -85 &&
           packet.moveX <= 85 && packet.moveY >= -85 && packet.moveY <= 85 &&
           (packet.heldActions & ~knownActions) == 0 &&
           (packet.pressedActions & ~knownActions) == 0 &&
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
           (command.heldActions & ~knownActions) == 0 &&
           (command.pressedActions & ~knownActions) == 0 &&
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
               static_cast<unsigned char>(Game::Simulation::PlayerActionState::JumpSlashing) &&
            packet.actionStartTick <= packet.serverTick && packet.health <= 48 &&
            packet.team <= NETWORK_TEAM_BLUE &&
            packet.locomotionMode <= static_cast<unsigned char>(
                Game::Simulation::PlayerLocomotionMode::Swimming) &&
            std::isfinite(packet.locomotionPhaseRadians) &&
            packet.locomotionPhaseRadians >= 0.0f &&
            packet.locomotionPhaseRadians < 6.28318530717958647692f;
}

} // namespace Game::Multiplayer::PlayerSimulationNetworkAdapter
