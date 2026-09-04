#include "NativeLocalPlayerCommandController.h"

#include "platform/win32/App.h"
#include "platform/win32/Input.h"
#include "gameplay/Controls.h"
#include "global.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>

namespace Game::Multiplayer {

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

    const int32_t selectedWeapon = std::clamp<int32_t>(controls.weapon, 1, 4);
    constexpr float kBinaryAngleToRadians =
        3.14159265358979323846f / 32768.0f;
    Game::Client::LocalPlayerInputSample sample{};
    sample.clientTick = play->gameplayFrames;
    sample.lifeEpoch = lifeEpoch;
    sample.sceneId = play->sceneNum;
    if (!App.suppressWorldMouse) {
        sample.moveX = static_cast<float>((input.key['D'] ? 1 : 0) - (input.key['A'] ? 1 : 0));
        sample.moveY = static_cast<float>((input.key['W'] ? 1 : 0) - (input.key['S'] ? 1 : 0));
        if (sample.moveX != 0.0f && sample.moveY != 0.0f) {
            sample.moveY = 0.0f;
        }
    }
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
    if (weaponConfirmed && !App.suppressWorldMouse && input.key[VK_LBUTTON]) {
        sample.heldActions |= Game::Simulation::PLAYER_ACTION_PRIMARY;
    }
    if (weaponConfirmed && !App.suppressWorldMouse && input.framePress[VK_LBUTTON]) {
        sample.pressedActions |= Game::Simulation::PLAYER_ACTION_PRIMARY;
    }
    const bool rightMouse = !App.suppressWorldMouse && input.key[VK_RBUTTON];
    if (weaponConfirmed && selectedWeapon <= 2 && rightMouse) {
        sample.heldActions |= Game::Simulation::PLAYER_ACTION_BLOCK;
    }
    if (weaponConfirmed && selectedWeapon == 3 && rightMouse) {
        sample.heldActions |= Game::Simulation::PLAYER_ACTION_AIM;
    }
    if (!App.suppressWorldMouse && input.framePress[VK_SPACE]) {
        sample.pressedActions |= Game::Simulation::PLAYER_ACTION_EVADE;
    }

    mCommands.Submit(
        sample, std::max(deltaSeconds, 0.0f), sendCommand, mPrediction);
}

} // namespace Game::Multiplayer
