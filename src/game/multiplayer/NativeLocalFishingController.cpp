#include "NativeLocalFishingController.h"

#include "platform/win32/App.h"
#include "platform/win32/Input.h"

#include "gameplay/FishingGameplay.h"
#include "global.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstddef>

namespace Game::Multiplayer {

NativeLocalFishingController::NativeLocalFishingController(
    Game::Client::LocalFishingUpdateStream& updates,
    Game::Client::LocalFishIntentStream& intents)
    : mUpdates(updates), mIntents(intents) {
}

NativeLocalFishingSubmission NativeLocalFishingController::Sample(
    PlayState* play, double nowSeconds) {
    NativeLocalFishingSubmission submission{};
    FishingLocalVisual visual{};
    const bool visualActive =
        FishingGameplay_ReadLocalVisual(play, &visual) != 0;

    Game::Replication::FishingPresentationState presentation{};
    if (visualActive) {
        presentation.state = visual.state;
        presentation.rodTipOffset = { visual.rodTipOffset[0], visual.rodTipOffset[1],
                                      visual.rodTipOffset[2] };
        presentation.lureDrawOffset = {
            visual.lureDrawOffset[0], visual.lureDrawOffset[1],
            visual.lureDrawOffset[2]
        };
        presentation.rodBendY = visual.rodBendY;
        presentation.rodBendX = visual.rodBendX;
        presentation.rodTwist = visual.rodTwist;
        presentation.rodCastX = visual.rodCastX;
        presentation.lureRotation = { visual.lureRotation[0], visual.lureRotation[1],
                                      visual.lureRotation[2] };
        presentation.lureSpin = visual.lureSpin;
        presentation.lureZOffset = visual.lureZOffset;
        for (size_t hook = 0; hook < presentation.lureHookOffsets.size(); ++hook) {
            presentation.lureHookOffsets[hook] = {
                visual.lureHookOffsets[hook][0], visual.lureHookOffsets[hook][1],
                visual.lureHookOffsets[hook][2]
            };
            presentation.lureHookRotations[hook] = {
                visual.lureHookRotations[hook][0],
                visual.lureHookRotations[hook][1]
            };
        }
        presentation.lineScale = visual.lineScale;
        presentation.lineGravity = visual.lineGravity;
        presentation.lineSpooled = visual.lineSpooled;
        presentation.sinkingLureSegmentIndex = visual.sinkingLureSegmentIndex;
        presentation.sinkingLureUnderwater = visual.sinkingLureUnderwater;
        presentation.fishRotation = { visual.fishRotation[0], visual.fishRotation[1],
                                      visual.fishRotation[2] };
        for (size_t limb = 0; limb < presentation.fishLimbRotation.size(); ++limb) {
            presentation.fishLimbRotation[limb] = visual.fishLimbRotation[limb];
        }
    }

    const bool deployed = visualActive && presentation.state != 0;
    const bool reelHeld = visualActive && !App.suppressWorldMouse && input.key[VK_RBUTTON];
    const auto decision = mUpdates.Evaluate(
        { play ? play->sceneNum : -1, visualActive, presentation.state,
          deployed, reelHeld },
        nowSeconds);
    if (decision.SendPresentation()) {
        presentation.sequence = decision.presentationSequence;
        submission.presentation = presentation;
    }
    if (decision.SendControl()) {
        submission.control = Game::Client::LocalLureControlIntent{
            decision.controlSequence, deployed, reelHeld,
            decision.reliableControl
        };
    }
    return submission;
}

bool NativeLocalFishingController::SubmitAction(
    FishingGameplayAction action,
    const std::function<bool(const Game::Client::LocalFishIntent&)>& sender) {
    const auto intent = action == FISHING_GAMEPLAY_ACTION_HOOK
                            ? mIntents.BeginHook()
                            : mIntents.EndHook();
    if (!intent) return false;
    const bool submitted = sender && sender(*intent);
    mIntents.Resolve(intent->sequence, submitted);
    return submitted;
}

} // namespace Game::Multiplayer
