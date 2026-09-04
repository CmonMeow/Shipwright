#include "NativeClientNetworkSession.h"

#include "multiplayer/NetworkRuntime.h"
#include "multiplayer/NativeClientInboundReplication.h"
#include "multiplayer/NativeClientOutboundSubmission.h"
#include "multiplayer/NativeClientFrameReconciliation.h"
#include "multiplayer/NativeClientSessionLifecycle.h"
#include "multiplayer/NativeClientUpdateCoordinator.h"
#include "multiplayer/NativeMultiplayerInteractionController.h"
#include "multiplayer/NativeProjectileRenderer.h"
#include "multiplayer/NativeLocalFishingController.h"
#include "multiplayer/NativeLocalPlayerCommandController.h"
#include "multiplayer/NativeLocalPlayerPresentationController.h"
#include "multiplayer/NativeLocalProjectileController.h"
#include "multiplayer/NativeLocalRespawnController.h"
#include "multiplayer/NativeCombatPresentationController.h"
#include "multiplayer/NativePlayerPresentationState.h"
#include "multiplayer/NativeRemotePlayerRenderer.h"
#include "multiplayer/NativeRemotePlayerPresentationController.h"
#include "multiplayer/NativeRemoteFishPresentationController.h"
#include "multiplayer/NativePlayerNameplatePresenter.h"
#include "multiplayer/NativeRemoteProjectilePresentationController.h"
#include "multiplayer/NativeCorpsePresentationController.h"
#include "multiplayer/FishingNetworkAdapter.h"
#include "multiplayer/PlayerSimulationNetworkAdapter.h"
#include "multiplayer/ProjectileNetworkAdapter.h"
#include "multiplayer/WorldPvpNetworkAdapter.h"
#include "gameplay/FishingGameplay.h"
#include "gameplay/FishPresentation.h"
#include "gameplay/GameplayNotification.h"
#include "gameplay/ProjectileGameplay.h"
#include "platform/client/ClientGameplaySession.h"
#include "platform/client/CorpsePresentationRegistry.h"
#include "platform/client/RemoteFishingEntityState.h"
#include "platform/client/RemotePlayerReplicaStore.h"
#include "platform/client/RemoteProjectileReplicaStore.h"
#include "multiplayer/ui/MultiplayerUI.h"
#include "debug/collision/colViewer.h"
#include "platform/simulation/AuthoritativePlayerHitRig.h"
#include "global.h"
#include "variables.h"

#include <runtime/log/Log.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <type_traits>

namespace Game::Multiplayer {

struct NativeClientNetworkSession::State {
    bool initialized = false;
    std::unique_ptr<NetworkRuntime> runtime;
    std::unique_ptr<NativeClientInboundReplication> inbound;
    std::unique_ptr<NativeClientOutboundSubmission> outbound;
    std::unique_ptr<NativeClientFrameReconciliation> frames;
    std::unique_ptr<NativeMultiplayerInteractionController> interactions;
    std::unique_ptr<MultiplayerUI> multiplayerUI;
    std::unique_ptr<NativeClientUpdateCoordinator> updates;
    Game::Client::RemotePlayerReplicaStore playerReplicas;
    NativeRemotePlayerRenderer playerRenderer;
    Game::Client::RemoteFishingEntityState fishingEntities;
    NativeRemoteFishPresentationController remoteFish{
        fishingEntities, playerReplicas, playerRenderer };
    NativeRemotePlayerPresentationController remotePlayers{
        playerReplicas, fishingEntities, playerRenderer };
    NativePlayerNameplatePresenter nameplates{ playerRenderer };
    Game::Client::CorpsePresentationRegistry corpseRegistry;
    NativeCorpsePresentationController corpses{
        corpseRegistry, playerRenderer };
    Game::Client::RemoteProjectileReplicaStore projectileReplicas;
    NativeProjectileRenderer projectileRenderer;
    NativeRemoteProjectilePresentationController remoteProjectiles{
        projectileReplicas, projectileRenderer };
    Game::Client::ClientGameplaySession gameplay;
    NativeLocalFishingController localFishing{
        gameplay.FishingUpdates(), gameplay.FishIntents() };
    NativeLocalPlayerCommandController localPlayerCommands{
        gameplay.Commands(), gameplay.Prediction() };
    NativeLocalPlayerPresentationController localPlayerPresentation{
        gameplay.Prediction(), gameplay.Vitals(), corpseRegistry };
    NativeLocalProjectileController localProjectiles{
        gameplay.Projectiles() };
    NativeLocalRespawnController localRespawn{
        gameplay, localProjectiles };
    NativeCombatPresentationController combatPresentation;
    NativeClientSessionLifecycle sessionLifecycle{
        { playerRenderer, projectileRenderer, playerReplicas, fishingEntities,
          corpseRegistry, projectileReplicas, gameplay, localProjectiles } };
};

} // namespace Game::Multiplayer

namespace {

static_assert(std::is_standard_layout_v<FishingLocalVisual>);
static_assert(std::is_standard_layout_v<FishPresentationIdentity>);
static_assert(std::is_standard_layout_v<FishPresentationState>);

int32_t SubmitNativeFishingAction(FishingGameplayAction action, void* context) {
    auto* outbound =
        static_cast<Game::Multiplayer::NativeClientOutboundSubmission*>(context);
    return outbound && outbound->SubmitFishingAction(action);
}

void BindNativePredictedArrow(const void* presentation, int32_t sceneId,
                              void* context) {
    auto* controller =
        static_cast<Game::Multiplayer::NativeLocalProjectileController*>(context);
    if (!controller) return;
    controller->BindPredictedArrow(
        const_cast<Actor*>(static_cast<const Actor*>(presentation)), sceneId);
}

int32_t CommitNativeArrowFire(const void* presentation, int32_t sceneId,
                              void* context) {
    auto* controller =
        static_cast<Game::Multiplayer::NativeLocalProjectileController*>(context);
    return controller && controller->CommitArrowFire(
        const_cast<Actor*>(static_cast<const Actor*>(presentation)), sceneId);
}

void UnbindNativePredictedArrow(const void* presentation, void* context) {
    auto* controller =
        static_cast<Game::Multiplayer::NativeLocalProjectileController*>(context);
    if (!controller) return;
    controller->UnbindPredictedArrow(
        const_cast<Actor*>(static_cast<const Actor*>(presentation)));
}

void ShowGameplayNotification(const char* text, void* context) {
    auto* ui = static_cast<MultiplayerUI*>(context);
    if (ui) ui->ShowNotification(text);
}

void ClearGameplayNotification(void* context) {
    auto* ui = static_cast<MultiplayerUI*>(context);
    if (ui) ui->ClearNotification();
}

double NowSeconds();

int32_t ReadNativeFishPresentation(
    const FishPresentationIdentity* identity, FishPresentationState* output,
    void* context) {
    if (!identity || !output || identity->spawnKey == 0 || !context) {
        return false;
    }
    auto* controller = static_cast<
        Game::Multiplayer::NativeRemoteFishPresentationController*>(context);
    const auto state = controller->Read(
        { identity->sceneId, identity->spawnKey }, NowSeconds());
    if (!state) return false;
    for (size_t axis = 0; axis < state->position.size(); ++axis) {
        output->position[axis] = state->position[axis];
        output->rotation[axis] = state->rotation[axis];
    }
    for (size_t limb = 0; limb < state->limbRotation.size(); ++limb) {
        output->limbRotation[limb] = state->limbRotation[limb];
    }
    output->length = state->length;
    output->isLoach = state->isLoach;
    return true;
}

double NowSeconds() {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

extern "C" PlayState* gPlayState;

namespace Game::Multiplayer {

NativeClientNetworkSession::NativeClientNetworkSession(Engine::ConsoleVariable& variables,
                                                       Engine::Rendering::GameRenderer& renderer,
                                                       Input& input)
    : mState(std::make_unique<State>()), mVariables(variables), mRenderer(renderer), mInput(input) {
}

NativeClientNetworkSession::~NativeClientNetworkSession() {
    Shutdown();
}

void NativeClientNetworkSession::PumpMoveLoop() {
    if (mState->updates) mState->updates->PumpMoveLoop(gPlayState);
}

Game::Client::MultiplayerInteractionPort& NativeClientNetworkSession::Interaction() {
    return *mState->interactions;
}

void NativeClientNetworkSession::RegisterActors() {
    mState->playerRenderer.RegisterActorType();
    mState->projectileRenderer.RegisterActorType();
}

void NativeClientNetworkSession::Initialize() {
    State& state = *mState;
    if (state.initialized) {
        return;
    }
    state.runtime = std::make_unique<NetworkRuntime>();
    state.inbound =
        std::make_unique<NativeClientInboundReplication>(
            NativeClientInboundDependencies{
                *state.runtime, state.gameplay,
                state.remotePlayers, state.remoteProjectiles,
                state.localProjectiles, state.corpses });
    state.outbound =
        std::make_unique<NativeClientOutboundSubmission>(
            NativeClientOutboundDependencies{
                *state.runtime, state.gameplay,
                state.localFishing, state.localPlayerCommands,
                state.localProjectiles });
    state.frames =
        std::make_unique<NativeClientFrameReconciliation>(
            NativeClientFrameDependencies{
                *state.runtime, state.gameplay,
                state.localRespawn, state.combatPresentation,
                state.playerRenderer, state.projectileRenderer,
                state.localPlayerPresentation,
                state.nameplates });
    FishingGameplay_SetActionSink(SubmitNativeFishingAction,
                                  state.outbound.get());
    const ProjectileGameplaySink projectileSink{
        BindNativePredictedArrow, CommitNativeArrowFire,
        UnbindNativePredictedArrow, &state.localProjectiles
    };
    ProjectileGameplay_SetSink(&projectileSink);
    state.sessionLifecycle.ResetTracking();
    state.sessionLifecycle.Observe(state.runtime->SessionGeneration());
    state.interactions = std::make_unique<NativeMultiplayerInteractionController>(
        *state.runtime, mVariables, mInput);
    state.multiplayerUI =
        std::make_unique<MultiplayerUI>(*state.interactions, mVariables, mRenderer, mInput);
    state.updates = std::make_unique<NativeClientUpdateCoordinator>(
        NativeClientUpdateDependencies{
            *state.runtime, *state.inbound,
            *state.outbound, *state.frames,
            state.sessionLifecycle, *state.multiplayerUI });
    state.updates->ResetClock(NowSeconds());
    const GameplayNotificationSink notificationSink{
        ShowGameplayNotification, ClearGameplayNotification,
        state.multiplayerUI.get()
    };
    GameplayNotification_SetSink(&notificationSink);
    const FishPresentationSink fishPresentationSink{
        ReadNativeFishPresentation, &state.remoteFish
    };
    FishPresentation_SetSink(&fishPresentationSink);

    state.initialized = true;
}

void NativeClientNetworkSession::Shutdown() {
    State& state = *mState;
    if (!state.initialized) {
        return;
    }
    state.initialized = false;
    FishingGameplay_ClearActionSink(state.outbound.get());
    ProjectileGameplay_ClearSink(&state.localProjectiles);
    GameplayNotification_ClearSink(state.multiplayerUI.get());
    FishPresentation_ClearSink(&state.remoteFish);
    state.updates.reset();
    if (state.multiplayerUI) {
        state.multiplayerUI->Shutdown();
        state.multiplayerUI.reset();
    }
    if (state.interactions) {
        state.interactions->Shutdown();
        state.interactions.reset();
    }
    if (state.runtime) {
        state.runtime->Disconnect();
        state.frames.reset();
        state.outbound.reset();
        state.inbound.reset();
        state.runtime.reset();
    }
    state.sessionLifecycle.DetachAfterSceneShutdown();
}

void NativeClientNetworkSession::UpdateTransport() {
    if (mState->updates) {
        mState->updates->UpdateTransport(gPlayState, NowSeconds());
    }
}

void NativeClientNetworkSession::UpdateGameplay(PlayState* play) {
    if (mState->updates) {
        mState->updates->UpdateGameplay(play, NowSeconds());
    }
}

} // namespace Game::Multiplayer
