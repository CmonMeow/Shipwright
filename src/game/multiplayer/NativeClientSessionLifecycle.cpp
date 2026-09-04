#include "NativeClientSessionLifecycle.h"

#include "NativeLocalProjectileController.h"
#include "NativeProjectileRenderer.h"
#include "NativeRemotePlayerRenderer.h"
#include "debug/collision/colViewer.h"
#include "platform/client/ClientGameplaySession.h"
#include "platform/client/CorpsePresentationRegistry.h"
#include "platform/client/RemoteFishingEntityState.h"
#include "platform/client/RemotePlayerReplicaStore.h"
#include "platform/client/RemoteProjectileReplicaStore.h"

namespace Game::Multiplayer {

NativeClientSessionLifecycle::NativeClientSessionLifecycle(
    NativeClientSessionDependencies dependencies)
    : mDependencies(dependencies) {
}

void NativeClientSessionLifecycle::ResetEstablishedSession() {
    // Retire native actors before invalidating the semantic lifetimes they
    // present. This is the sole live-session reset boundary used by reconnect.
    mDependencies.players.Reset();
    mDependencies.projectiles.Reset();
    mDependencies.playerReplicas.Reset();
    mDependencies.fishing.Reset();
    mDependencies.corpses.Reset();
    mDependencies.projectileReplicas.Reset();
    mDependencies.gameplay.ResetSession();
    mDependencies.localProjectiles.ResetBindings();
    ClearAuthoritativePlayerCollision();
    mDependencies.players.Bind(
        &mDependencies.playerReplicas, &mDependencies.fishing,
        &mDependencies.corpses);
    mDependencies.projectiles.Bind(&mDependencies.projectileReplicas);
}

bool NativeClientSessionLifecycle::Observe(uint64_t generation) {
    const auto update = mGeneration.Observe(generation);
    if (!Game::Client::ClientSessionGenerationTracker::RequiresStateReset(
            update)) {
        return false;
    }
    ResetEstablishedSession();
    return true;
}

void NativeClientSessionLifecycle::ResetTracking() {
    mGeneration.Reset();
}

void NativeClientSessionLifecycle::DetachAfterSceneShutdown() {
    mDependencies.players.DetachAfterSceneShutdown();
    mDependencies.playerReplicas.Reset();
    mDependencies.fishing.Reset();
    mDependencies.corpses.Reset();
    mDependencies.gameplay.ResetSession();
    mDependencies.projectiles.DetachAfterSceneShutdown();
    mDependencies.projectileReplicas.Reset();
    mDependencies.localProjectiles.ResetBindings();
    ClearAuthoritativePlayerCollision();
    mGeneration.Reset();
}

} // namespace Game::Multiplayer
