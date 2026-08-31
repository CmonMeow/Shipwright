#include "NativeClientFrameReconciliation.h"

#include "NativeCombatPresentationController.h"
#include "NativeLocalPlayerPresentationController.h"
#include "NativeLocalRespawnController.h"
#include "NativePlayerNameplatePresenter.h"
#include "NativeProjectileRenderer.h"
#include "NativeRemotePlayerRenderer.h"
#include "NetworkRuntime.h"
#include "../../platform/client/ClientGameplaySession.h"

#include "global.h"

extern "C" int32_t Fishing_EnsurePresentedPopulation(PlayState* play);

namespace SoH::Network {

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
}

void NativeClientFrameReconciliation::ReconcileGameplayFrame(
    PlayState* play, float deltaSeconds) {
    if (!play) return;

    if (mDependencies.players.HasFishingPlayerInScene(play->sceneNum)) {
        Fishing_EnsurePresentedPopulation(play);
    }

    Game::Simulation::CombatResultEvent combat{};
    while (mDependencies.runtime.PollCombatResult(combat)) {
        mDependencies.combat.Apply(
            play, combat, mDependencies.runtime.LocalPlayerId());
    }
    ProcessRespawns(play);

    mDependencies.players.Reconcile(play);
    mDependencies.projectiles.Reconcile(play);

    if (Player* player = GET_PLAYER(play)) {
        const Game::Simulation::Vec3 correction =
            mDependencies.gameplay.Prediction().ConsumeCorrection(
                deltaSeconds);
        player->actor.world.pos.x += correction.x;
        player->actor.world.pos.y += correction.y;
        player->actor.world.pos.z += correction.z;
        player->actor.prevPos.x += correction.x;
        player->actor.prevPos.y += correction.y;
        player->actor.prevPos.z += correction.z;
    }

}

void NativeClientFrameReconciliation::ProjectLocalPresentation(
    PlayState* play) {
    if (!play) return;
    mDependencies.localPresentation.Project(
        GET_PLAYER(play), mDependencies.runtime.IsActive());
    mDependencies.localPresentation.ProjectBodyOwnership(
        GET_PLAYER(play), mDependencies.runtime.LocalPlayerId());
}

} // namespace SoH::Network
