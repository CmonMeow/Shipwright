#include "NativeClientFrameReconciliation.h"

#include "NativeCombatPresentationController.h"
#include "NativeLocalPlayerPresentationController.h"
#include "NativeLocalRespawnController.h"
#include "NativePlayerNameplatePresenter.h"
#include "NativeProjectileRenderer.h"
#include "NativeRemotePlayerRenderer.h"
#include "NetworkRuntime.h"
#include "platform/client/ClientGameplaySession.h"
#include "platform/client/RemoteFishingEntityState.h"

#include "global.h"

extern "C" int32_t Fishing_EnsurePresentedPopulation(PlayState* play);

namespace Game::Multiplayer {

NativeClientFrameReconciliation::NativeClientFrameReconciliation(
    NativeClientFrameDependencies dependencies)
    : mDependencies(dependencies) {
}

void NativeClientFrameReconciliation::ProcessRespawns(PlayState* play) {
    if (!play) return;

    Game::Simulation::PlayerRespawnEvent event{};
    while (mDependencies.runtime.PollPlayerRespawn(event)) {
        mDependencies.respawn.Apply(
            play, event, mDependencies.runtime.LocalPlayerId());
    }
}

void NativeClientFrameReconciliation::QueueNameplates(PlayState* play) {
    if (!play) return;

    for (const auto& player : mDependencies.runtime.Players()) {
        mDependencies.nameplates.Queue(
            play, player.playerId, player.name.c_str());
    }
}

void NativeClientFrameReconciliation::ProcessTransportFrame(PlayState* play) {
    if (!play) return;
    QueueNameplates(play);

    mDependencies.localPresentation.ProjectBodyOwnership(
        GET_PLAYER(play), mDependencies.runtime.LocalPlayerId());

    // Death intentionally freezes the native gameplay update. The transport
    // frame continues, so reliable server respawns must be consumed here.
    ProcessRespawns(play);

    if (Player* player = GET_PLAYER(play)) {
        // BeginFrame runs before Actor_UpdateAll. Apply an authoritative
        // correction here so native floor, wall, water, and ledge collision
        // observes the corrected position during this same gameplay update.
        // Applying a smoothed displacement after Actor_UpdateAll made native
        // Link react one frame late and produced a second movement system at
        // traversal boundaries.
        const Game::Simulation::Vec3 correction =
            mDependencies.gameplay.Prediction().ConsumeCorrection(
                0.0f, 0.0f);
        player->actor.world.pos.x += correction.x;
        player->actor.world.pos.y += correction.y;
        player->actor.world.pos.z += correction.z;
        player->actor.prevPos.x += correction.x;
        player->actor.prevPos.y += correction.y;
        player->actor.prevPos.z += correction.z;
    }
}

void NativeClientFrameReconciliation::ReconcileGameplayFrame(
    PlayState* play) {
    if (!play) return;

    if (mDependencies.players.HasFishingPlayerInScene(play->sceneNum) ||
        mDependencies.fishing.HasEntityInScene(play->sceneNum)) {
        Fishing_EnsurePresentedPopulation(play);
    }

    Game::Simulation::CombatResultEvent combat{};
    while (mDependencies.runtime.PollCombatResult(combat)) {
        mDependencies.combat.Apply(
            play, combat, mDependencies.runtime.LocalPlayerId());
    }
    ProcessRespawns(play);

    mDependencies.players.Reconcile(
        play, mDependencies.runtime.LocalPlayerId());
    mDependencies.projectiles.Reconcile(play);

}

void NativeClientFrameReconciliation::ProjectLocalPresentation(
    PlayState* play) {
    if (!play) return;
    mDependencies.localPresentation.ProjectBodyOwnership(
        GET_PLAYER(play), mDependencies.runtime.LocalPlayerId());
}

} // namespace Game::Multiplayer
