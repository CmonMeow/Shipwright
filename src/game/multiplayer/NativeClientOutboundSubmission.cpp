#include "NativeClientOutboundSubmission.h"

#include "NativeLocalFishingController.h"
#include "NativeLocalPlayerCommandController.h"
#include "NativeLocalProjectileController.h"
#include "NetworkRuntime.h"
#include "platform/client/ClientGameplaySession.h"

#include "global.h"

#include <chrono>

namespace Game::Multiplayer {
namespace {

double NowSeconds() {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

NativeClientOutboundSubmission::NativeClientOutboundSubmission(
    NativeClientOutboundDependencies dependencies)
    : mDependencies(dependencies) {
}

bool NativeClientOutboundSubmission::SubmitFishingAction(
    FishingGameplayAction action) {
    if (!mDependencies.runtime.IsActive()) return false;
    return mDependencies.fishing.SubmitAction(
        action, [this](const Game::Client::LocalFishIntent& intent) {
            return mDependencies.runtime.SendFishIntent(intent);
        });
}

void NativeClientOutboundSubmission::SubmitPlayerCommand(PlayState* play,
                                                          float deltaSeconds) {
    if (!play || !mDependencies.runtime.IsActive() ||
        !mDependencies.gameplay.Scene().IsAuthorized(play->sceneNum)) {
        return;
    }
    mDependencies.playerCommands.Submit(
        play, mDependencies.gameplay.Scene().LifeEpoch().value_or(0),
        deltaSeconds,
        [this](const Game::Client::LocalWeaponSelectionRequest& selection) {
            return mDependencies.runtime.SendWeaponSelection(selection);
        },
        [this](const Game::Simulation::PlayerCommand& command) {
            return mDependencies.runtime.SendPlayerCommand(
                command, command.lifeEpoch);
        });
}

void NativeClientOutboundSubmission::FlushProjectileIntents() {
    if (!mDependencies.runtime.IsActive()) {
        mDependencies.gameplay.Projectiles().Reset();
        mDependencies.projectiles.ResetBindings();
        return;
    }
    while (const auto intent =
               mDependencies.gameplay.Projectiles().NextIntent()) {
        const bool sent = mDependencies.runtime.SendArrowFireIntent(*intent);
        mDependencies.gameplay.Projectiles().Resolve(intent->sequence, sent);
        if (!sent) break;
    }
}

void NativeClientOutboundSubmission::SubmitPresentation(PlayState* play) {
    if (!play || !mDependencies.runtime.IsActive() ||
        !mDependencies.gameplay.Scene().IsAuthorized(play->sceneNum) ||
        !GET_PLAYER(play)) {
        return;
    }
    const auto fishing = mDependencies.fishing.Sample(play, NowSeconds());
    if (fishing.presentation) {
        mDependencies.runtime.SendFishingPresentation(*fishing.presentation);
    }
    if (fishing.control) {
        mDependencies.runtime.SendLureControlIntent(*fishing.control);
    }
    FlushProjectileIntents();
}

} // namespace Game::Multiplayer
