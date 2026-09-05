#include "NativeLocalPlayerCommandController.h"

#include "platform/win32/App.h"
#include "platform/win32/Input.h"
#include "gameplay/Controls.h"
#include "global.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>

namespace Game::Multiplayer {
namespace {

void* AnimationAsset(const char* asset) {
    return const_cast<char*>(asset);
}

bool IsNativeSwordAttackStartAnimation(const Player& player) {
    const void* animation = player.skelAnime.animation;
    switch (player.meleeWeaponAnimation) {
        case PLAYER_MWA_FORWARD_SLASH_1H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_normal_kiru);
        case PLAYER_MWA_FORWARD_SLASH_2H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_Lnormal_kiru);
        case PLAYER_MWA_FORWARD_COMBO_1H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_normal_kiru_finsh);
        case PLAYER_MWA_FORWARD_COMBO_2H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_Lnormal_kiru_finsh);
        case PLAYER_MWA_RIGHT_SLASH_1H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_Lside_kiru);
        case PLAYER_MWA_RIGHT_SLASH_2H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_LLside_kiru);
        case PLAYER_MWA_RIGHT_COMBO_1H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_Lside_kiru_finsh);
        case PLAYER_MWA_RIGHT_COMBO_2H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_LLside_kiru_finsh);
        case PLAYER_MWA_LEFT_SLASH_1H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_Rside_kiru);
        case PLAYER_MWA_LEFT_SLASH_2H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_LRside_kiru);
        case PLAYER_MWA_LEFT_COMBO_1H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_Rside_kiru_finsh);
        case PLAYER_MWA_LEFT_COMBO_2H:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_LRside_kiru_finsh);
        case PLAYER_MWA_FLIPSLASH_START:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_jump_rollkiru);
        case PLAYER_MWA_JUMPSLASH_START:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_Lpower_jump_kiru);
        case PLAYER_MWA_FLIPSLASH_FINISH:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_jump_kiru_finsh);
        case PLAYER_MWA_JUMPSLASH_FINISH:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_Lpower_jump_kiru_hit);
        case PLAYER_MWA_BACKSLASH_RIGHT:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_turn_kiruR);
        case PLAYER_MWA_BACKSLASH_LEFT:
            return animation == AnimationAsset(gPlayerAnim_link_fighter_turn_kiruL);
        default:
            return false;
    }
}

} // namespace

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
    if (mObservedLifeEpoch != lifeEpoch) {
        mObservedLifeEpoch = lifeEpoch;
        mLastNativeAnimation = nullptr;
        mLastNativeAnimationFrame = 0.0f;
        mLastAnimationWasSwordAttack = false;
    }
    const bool nativeSwordAttack = IsNativeSwordAttackStartAnimation(*player);
    const bool nativeSwordAttackStarted =
        nativeSwordAttack &&
        (!mLastAnimationWasSwordAttack ||
         mLastNativeAnimation != player->skelAnime.animation ||
         player->skelAnime.curFrame + 0.01f < mLastNativeAnimationFrame);
    const bool rightMouse = !App.suppressWorldMouse && input.key[VK_RBUTTON];

    // The selected slot is player intent. Deriving it from heldItemAction
    // creates a circular dependency: a sheathed weapon is reported as slot 0,
    // so authority never confirms the slot and its input can never be sent.
    const int32_t selectedWeapon = std::clamp<int32_t>(controls.weapon, 1, 4);
    constexpr float kBinaryAngleToRadians =
        3.14159265358979323846f / 32768.0f;
    Game::Client::LocalPlayerInputSample sample{};
    sample.clientTick = play->gameplayFrames;
    sample.lifeEpoch = lifeEpoch;
    sample.sceneId = play->sceneNum;
    const bool fishingOwnsBody =
        player->heldItemAction == PLAYER_IA_FISHING_POLE && player->unk_860 != 0;
    if (!App.suppressWorldMouse && !fishingOwnsBody) {
        // Submit exactly the axes consumed by native Link. Keeping a second
        // key mapping here previously inverted A/D for server authority and
        // discarded forward movement whenever strafe was also held.
        sample.moveX = static_cast<float>(controls.move.x);
        sample.moveY = static_cast<float>(controls.move.y);
    }
    sample.headingRadians =
        static_cast<float>(player->actor.shape.rot.y) * kBinaryAngleToRadians;
    sample.aimPitchRadians =
        static_cast<float>(player->actor.focus.rot.x) * kBinaryAngleToRadians;
    sample.position = { player->actor.world.pos.x, player->actor.world.pos.y,
                        player->actor.world.pos.z };
    sample.hasPose = true;
    if (player->stateFlags1 &
        (PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
         PLAYER_STATE1_CLIMBING_LADDER)) {
        sample.locomotionMode =
            Game::Simulation::PlayerLocomotionMode::Climbing;
    } else if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        // This native flag spans entry, surface strokes, dives and shore
        // transitions. Authority validates the full water column; it must not
        // force every one of these native states onto the surface-swim datum.
        sample.locomotionMode =
            Game::Simulation::PlayerLocomotionMode::Swimming;
    } else if ((player->actor.bgCheckFlags & 1) == 0) {
        sample.locomotionMode =
            Game::Simulation::PlayerLocomotionMode::Airborne;
    }
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
    if (weaponConfirmed &&
        ((selectedWeapon <= 2 && nativeSwordAttackStarted) ||
         (selectedWeapon == 3 && !App.suppressWorldMouse &&
          input.framePress[VK_LBUTTON]))) {
        sample.pressedActions |= Game::Simulation::PLAYER_ACTION_PRIMARY;
        if (selectedWeapon <= 2 && nativeSwordAttackStarted &&
            player->meleeWeaponAnimation >= PLAYER_MWA_FORWARD_SLASH_1H &&
            player->meleeWeaponAnimation <= PLAYER_MWA_LEFT_COMBO_2H) {
            // Native uses adjacent one-/two-handed ids for each semantic
            // slash. Report the action it actually selected after this
            // gameplay update; authority validates only the six supported
            // slash/combo identities and owns timing, collision, and damage.
            switch (player->meleeWeaponAnimation / 2) {
                case 0:
                    sample.meleeAttackVariant =
                        Game::Simulation::MeleeAttackVariant::ForwardSlash;
                    sample.hasMeleeAttackVariant = true;
                    break;
                case 1:
                    sample.meleeAttackVariant =
                        Game::Simulation::MeleeAttackVariant::ForwardCombo;
                    sample.hasMeleeAttackVariant = true;
                    break;
                case 2:
                    sample.meleeAttackVariant =
                        Game::Simulation::MeleeAttackVariant::RightSlash;
                    sample.hasMeleeAttackVariant = true;
                    break;
                case 3:
                    sample.meleeAttackVariant =
                        Game::Simulation::MeleeAttackVariant::RightCombo;
                    sample.hasMeleeAttackVariant = true;
                    break;
                case 4:
                    sample.meleeAttackVariant =
                        Game::Simulation::MeleeAttackVariant::LeftSlash;
                    sample.hasMeleeAttackVariant = true;
                    break;
                case 5:
                    sample.meleeAttackVariant =
                        Game::Simulation::MeleeAttackVariant::LeftCombo;
                    sample.hasMeleeAttackVariant = true;
                    break;
                default:
                    break;
            }
        }
    }
    const bool nativeShielding =
        (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0;
    if (weaponConfirmed && selectedWeapon <= 2 &&
        (rightMouse || nativeShielding)) {
        sample.heldActions |= Game::Simulation::PLAYER_ACTION_BLOCK;
    }
    if (weaponConfirmed && selectedWeapon == 3 && rightMouse) {
        sample.heldActions |= Game::Simulation::PLAYER_ACTION_AIM;
    }
    if (!App.suppressWorldMouse && input.framePress[VK_SPACE]) {
        sample.pressedActions |= Game::Simulation::PLAYER_ACTION_EVADE;
    }

    mCommands.Submit(sample, std::max(deltaSeconds, 0.0f), sendCommand,
                     mPrediction, false);
    mLastNativeAnimation = player->skelAnime.animation;
    mLastNativeAnimationFrame = player->skelAnime.curFrame;
    mLastAnimationWasSwordAttack = nativeSwordAttack;
}

} // namespace Game::Multiplayer
