#include "NativeClientNetworkSession.h"

#include "Network/NetworkRuntime.h"
#include "Network/NativeClientInboundReplication.h"
#include "Network/NativeClientOutboundSubmission.h"
#include "Network/NativeClientFrameReconciliation.h"
#include "Network/NativeClientSessionLifecycle.h"
#include "Network/NativeClientUpdateCoordinator.h"
#include "Network/NativeMultiplayerInteractionController.h"
#include "Network/NativeProjectileRenderer.h"
#include "Network/NativeLocalFishingController.h"
#include "Network/NativeLocalPlayerCommandController.h"
#include "Network/NativeLocalPlayerPresentationController.h"
#include "Network/NativeLocalProjectileController.h"
#include "Network/NativeLocalRespawnController.h"
#include "Network/NativeCombatPresentationController.h"
#include "Network/NativePlayerPresentationState.h"
#include "Network/NativeRemotePlayerRenderer.h"
#include "Network/NativeRemotePlayerPresentationController.h"
#include "Network/NativeRemoteFishPresentationController.h"
#include "Network/NativePlayerNameplatePresenter.h"
#include "Network/NativeRemoteProjectilePresentationController.h"
#include "Network/NativeCorpsePresentationController.h"
#include "Network/FishingNetworkAdapter.h"
#include "Network/PlayerSimulationNetworkAdapter.h"
#include "Network/ProjectileNetworkAdapter.h"
#include "Network/WorldPvpNetworkAdapter.h"
#include "Gameplay/FishingGameplay.h"
#include "Gameplay/FishPresentation.h"
#include "Gameplay/GameplayNotification.h"
#include "Gameplay/ProjectileGameplay.h"
#include "../platform/client/ClientGameplaySession.h"
#include "../platform/client/CorpsePresentationRegistry.h"
#include "../platform/client/RemoteFishingEntityState.h"
#include "../platform/client/RemotePlayerReplicaStore.h"
#include "../platform/client/RemoteProjectileReplicaStore.h"
#include "engine/window/Overlay.h"
#include "MultiplayerUI.h"
#include "Enhancements/debugger/colViewer.h"
#include "../platform/simulation/AuthoritativePlayerHitRig.h"
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

namespace {

static_assert(std::is_standard_layout_v<FishingLocalVisual>);
static_assert(std::is_standard_layout_v<FishPresentationIdentity>);
static_assert(std::is_standard_layout_v<FishPresentationState>);

using SoH::Network::NetworkRuntime;

struct NetworkGameState {
    std::unique_ptr<NetworkRuntime> runtime;
    std::unique_ptr<SoH::Network::NativeClientInboundReplication> inbound;
    std::unique_ptr<SoH::Network::NativeClientOutboundSubmission> outbound;
    std::unique_ptr<SoH::Network::NativeClientFrameReconciliation> frames;
    std::unique_ptr<SoH::Network::NativeMultiplayerInteractionController>
        interactions;
    std::unique_ptr<MultiplayerUI> multiplayerUI;
    std::unique_ptr<SoH::Network::NativeClientUpdateCoordinator> updates;
    Game::Client::RemotePlayerReplicaStore playerReplicas;
    SoH::Network::NativeRemotePlayerRenderer playerRenderer;
    Game::Client::RemoteFishingEntityState fishingEntities;
    SoH::Network::NativeRemoteFishPresentationController remoteFish{
        fishingEntities, playerReplicas, playerRenderer };
    SoH::Network::NativeRemotePlayerPresentationController remotePlayers{
        playerReplicas, fishingEntities, playerRenderer };
    SoH::Network::NativePlayerNameplatePresenter nameplates{ playerRenderer };
    Game::Client::CorpsePresentationRegistry corpseRegistry;
    SoH::Network::NativeCorpsePresentationController corpses{
        corpseRegistry, playerRenderer };
    Game::Client::RemoteProjectileReplicaStore projectileReplicas;
    SoH::Network::NativeProjectileRenderer projectileRenderer;
    SoH::Network::NativeRemoteProjectilePresentationController remoteProjectiles{
        projectileReplicas, projectileRenderer };
    Game::Client::ClientGameplaySession gameplay;
    SoH::Network::NativeLocalFishingController localFishing{
        gameplay.FishingUpdates(), gameplay.FishIntents() };
    SoH::Network::NativeLocalPlayerCommandController localPlayerCommands{
        gameplay.Commands(), gameplay.Prediction() };
    SoH::Network::NativeLocalPlayerPresentationController localPlayerPresentation{
        gameplay.Prediction(), gameplay.Vitals(), corpseRegistry };
    SoH::Network::NativeLocalProjectileController localProjectiles{
        gameplay.Projectiles() };
    SoH::Network::NativeLocalRespawnController localRespawn{
        gameplay, localProjectiles };
    SoH::Network::NativeCombatPresentationController combatPresentation;
    SoH::Network::NativeClientSessionLifecycle sessionLifecycle{
        { playerRenderer, projectileRenderer, playerReplicas, fishingEntities,
          corpseRegistry, projectileReplicas, gameplay, localProjectiles } };
};

NetworkGameState gNetworkGame;

int32_t SubmitNativeFishingAction(FishingGameplayAction action, void* context) {
    auto* outbound =
        static_cast<SoH::Network::NativeClientOutboundSubmission*>(context);
    return outbound && outbound->SubmitFishingAction(action);
}

void BindNativePredictedArrow(const void* presentation, int32_t sceneId,
                              void* context) {
    auto* controller =
        static_cast<SoH::Network::NativeLocalProjectileController*>(context);
    if (!controller) return;
    controller->BindPredictedArrow(
        const_cast<Actor*>(static_cast<const Actor*>(presentation)), sceneId);
}

int32_t CommitNativeArrowFire(const void* presentation, int32_t sceneId,
                              void* context) {
    auto* controller =
        static_cast<SoH::Network::NativeLocalProjectileController*>(context);
    return controller && controller->CommitArrowFire(
        const_cast<Actor*>(static_cast<const Actor*>(presentation)), sceneId);
}

void UnbindNativePredictedArrow(const void* presentation, void* context) {
    auto* controller =
        static_cast<SoH::Network::NativeLocalProjectileController*>(context);
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
        SoH::Network::NativeRemoteFishPresentationController*>(context);
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

namespace SoH::Network::NativeClientNetworkSession {

void PumpMoveLoop() {
    if (gNetworkGame.updates) gNetworkGame.updates->PumpMoveLoop(gPlayState);
}

void RegisterActors() {
    gNetworkGame.playerRenderer.RegisterActorType();
    gNetworkGame.projectileRenderer.RegisterActorType();
}

void Initialize() {
    if (gNetworkGame.runtime) {
        return;
    }
    gNetworkGame.runtime = std::make_unique<NetworkRuntime>();
    gNetworkGame.inbound =
        std::make_unique<SoH::Network::NativeClientInboundReplication>(
            SoH::Network::NativeClientInboundDependencies{
                *gNetworkGame.runtime, gNetworkGame.gameplay,
                gNetworkGame.remotePlayers, gNetworkGame.remoteProjectiles,
                gNetworkGame.localProjectiles, gNetworkGame.corpses });
    gNetworkGame.outbound =
        std::make_unique<SoH::Network::NativeClientOutboundSubmission>(
            SoH::Network::NativeClientOutboundDependencies{
                *gNetworkGame.runtime, gNetworkGame.gameplay,
                gNetworkGame.localFishing, gNetworkGame.localPlayerCommands,
                gNetworkGame.localProjectiles });
    gNetworkGame.frames =
        std::make_unique<SoH::Network::NativeClientFrameReconciliation>(
            SoH::Network::NativeClientFrameDependencies{
                *gNetworkGame.runtime, gNetworkGame.gameplay,
                gNetworkGame.localRespawn, gNetworkGame.combatPresentation,
                gNetworkGame.playerRenderer, gNetworkGame.projectileRenderer,
                gNetworkGame.localPlayerPresentation,
                gNetworkGame.nameplates });
    FishingGameplay_SetActionSink(SubmitNativeFishingAction,
                                  gNetworkGame.outbound.get());
    const ProjectileGameplaySink projectileSink{
        BindNativePredictedArrow, CommitNativeArrowFire,
        UnbindNativePredictedArrow, &gNetworkGame.localProjectiles
    };
    ProjectileGameplay_SetSink(&projectileSink);
    gNetworkGame.sessionLifecycle.ResetTracking();
    gNetworkGame.sessionLifecycle.Observe(
        gNetworkGame.runtime->SessionGeneration());
    gNetworkGame.interactions = std::make_unique<
        SoH::Network::NativeMultiplayerInteractionController>(
        *gNetworkGame.runtime);
    gNetworkGame.multiplayerUI =
        std::make_unique<MultiplayerUI>(*gNetworkGame.interactions);
    gNetworkGame.updates = std::make_unique<
        SoH::Network::NativeClientUpdateCoordinator>(
        SoH::Network::NativeClientUpdateDependencies{
            *gNetworkGame.runtime, *gNetworkGame.inbound,
            *gNetworkGame.outbound, *gNetworkGame.frames,
            gNetworkGame.sessionLifecycle, *gNetworkGame.multiplayerUI });
    gNetworkGame.updates->ResetClock(NowSeconds());
    const GameplayNotificationSink notificationSink{
        ShowGameplayNotification, ClearGameplayNotification,
        gNetworkGame.multiplayerUI.get()
    };
    GameplayNotification_SetSink(&notificationSink);
    const FishPresentationSink fishPresentationSink{
        ReadNativeFishPresentation, &gNetworkGame.remoteFish
    };
    FishPresentation_SetSink(&fishPresentationSink);
    
    Engine::Overlay::SetMoveLoopCallback(PumpMoveLoop);
}

void Shutdown() {
    FishingGameplay_ClearActionSink(gNetworkGame.outbound.get());
    ProjectileGameplay_ClearSink(&gNetworkGame.localProjectiles);
    GameplayNotification_ClearSink(gNetworkGame.multiplayerUI.get());
    FishPresentation_ClearSink(&gNetworkGame.remoteFish);
    Engine::Overlay::SetMoveLoopCallback(nullptr);
    gNetworkGame.updates.reset();
    if (gNetworkGame.multiplayerUI) {
        gNetworkGame.multiplayerUI->Shutdown();
        gNetworkGame.multiplayerUI.reset();
    }
    if (gNetworkGame.interactions) {
        gNetworkGame.interactions->Shutdown();
        gNetworkGame.interactions.reset();
    }
    if (gNetworkGame.runtime) {
        gNetworkGame.runtime->Disconnect();
        gNetworkGame.frames.reset();
        gNetworkGame.outbound.reset();
        gNetworkGame.inbound.reset();
        gNetworkGame.runtime.reset();
    }
    gNetworkGame.sessionLifecycle.DetachAfterSceneShutdown();
}

void UpdateTransport() {
    if (gNetworkGame.updates) {
        gNetworkGame.updates->UpdateTransport(gPlayState, NowSeconds());
    }
}

void UpdateGameplay(PlayState* play) {
    if (gNetworkGame.updates) {
        gNetworkGame.updates->UpdateGameplay(play, NowSeconds());
    }
}

} // namespace SoH::Network::NativeClientNetworkSession
