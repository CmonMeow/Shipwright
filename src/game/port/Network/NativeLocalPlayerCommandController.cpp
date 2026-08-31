#include "NativeLocalPlayerCommandController.h"

#include "engine/input/PCInput.h"
#include "global.h"

#include <algorithm>

namespace SoH::Network {

NativeLocalPlayerCommandController::NativeLocalPlayerCommandController(
    Game::Client::LocalPlayerCommandStream& commands,
    Game::Simulation::ClientPrediction& prediction)
    : mCommands(commands), mPrediction(prediction) {
}

void NativeLocalPlayerCommandController::Submit(
    PlayState* play, uint32_t lifeEpoch, float deltaSeconds,
    const LocalWeaponSelectionSender& sendWeaponSelection,
    const Game::Client::LocalPlayerCommandSender& sendCommand) {
    if (!play || lifeEpoch == 0) return;
    Player* player = GET_PLAYER(play);
    if (!player) return;

    const int32_t selectedWeapon =
        std::clamp(PCInput_GetSelectedWeaponSlot(), 1, 4);
    static constexpr uint16_t kWeaponButtons[] = {
        BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, BTN_CUP
    };
    const uint16_t selectedButton = kWeaponButtons[selectedWeapon - 1];

    constexpr float kBinaryAngleToRadians =
        3.14159265358979323846f / 32768.0f;
    Game::Client::LocalPlayerInputSample sample{};
    sample.clientTick = play->gameplayFrames;
    sample.lifeEpoch = lifeEpoch;
    sample.sceneId = play->sceneNum;
    sample.moveX = std::clamp(
        static_cast<float>(play->state.input[0].cur.stick_x) / 85.0f,
        -1.0f, 1.0f);
    sample.moveY = std::clamp(
        static_cast<float>(play->state.input[0].cur.stick_y) / 85.0f,
        -1.0f, 1.0f);
    sample.headingRadians =
        static_cast<float>(player->actor.shape.rot.y) * kBinaryAngleToRadians;
    sample.aimPitchRadians =
        static_cast<float>(player->actor.focus.rot.x) * kBinaryAngleToRadians;
    sample.selectedWeapon = static_cast<uint8_t>(selectedWeapon);

    if (const auto selection =
            mCommands.PrepareWeaponSelection(sample.selectedWeapon)) {
        const bool sent = sendWeaponSelection && sendWeaponSelection(*selection);
        mCommands.ResolveWeaponSelection(selection->sequence, sent);
    }

    const bool weaponConfirmed =
        mCommands.WeaponSelectionConfirmed(sample.selectedWeapon);
    if (weaponConfirmed &&
        CHECK_BTN_ALL(play->state.input[0].cur.button, selectedButton)) {
        sample.heldActions |= Game::Simulation::PLAYER_ACTION_PRIMARY;
    }
    if (weaponConfirmed &&
        CHECK_BTN_ALL(play->state.input[0].press.button, selectedButton)) {
        sample.pressedActions |= Game::Simulation::PLAYER_ACTION_PRIMARY;
    }
    if (weaponConfirmed && PCInput_IsShieldHeld()) {
        sample.heldActions |= Game::Simulation::PLAYER_ACTION_BLOCK;
    }
    if (weaponConfirmed && PCInput_IsBowAimHeld()) {
        sample.heldActions |= Game::Simulation::PLAYER_ACTION_AIM;
    }
    if (PCInput_EvadeRequestedThisSample()) {
        sample.pressedActions |= Game::Simulation::PLAYER_ACTION_EVADE;
    }

    mCommands.Submit(
        sample, std::max(deltaSeconds, 0.0f), sendCommand, mPrediction);
}

} // namespace SoH::Network
