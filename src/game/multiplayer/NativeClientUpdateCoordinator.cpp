#include "NativeClientUpdateCoordinator.h"

#include "NativeClientFrameReconciliation.h"
#include "NativeClientInboundReplication.h"
#include "NativeClientOutboundSubmission.h"
#include "NativeClientSessionLifecycle.h"
#include "NetworkRuntime.h"
#include "multiplayer/ui/MultiplayerUI.h"

namespace Game::Multiplayer {

NativeClientUpdateCoordinator::NativeClientUpdateCoordinator(
    NativeClientUpdateDependencies dependencies)
    : mDependencies(dependencies) {
}

void NativeClientUpdateCoordinator::ResetClock(double nowSeconds) {
    mFrameClock.Reset(nowSeconds);
}

void NativeClientUpdateCoordinator::PumpMoveLoop(PlayState* play) {
    mDependencies.runtime.Update();
    mDependencies.inbound.ReceiveSceneAuthority(play);
    mDependencies.inbound.ReceivePlayers(play);
    if (!play) return;
    mDependencies.inbound.EnsureSceneAuthorized(play);
    mDependencies.outbound.SubmitPresentation(play);
}

void NativeClientUpdateCoordinator::UpdateTransport(PlayState* play,
                                                     double nowSeconds) {
    mDependencies.runtime.Update();
    mDependencies.multiplayerUI.Update();
    if (mDependencies.sessionLifecycle.Observe(
            mDependencies.runtime.SessionGeneration())) {
        mFrameClock.Reset(nowSeconds);
    }

    // Scene authority must be committed before destination-scoped lifecycle,
    // pose, or world baselines delivered behind it become visible.
    mDependencies.inbound.ReceiveSceneAuthority(play);
    // Keep only the newest high-rate pose outside PlayState so file selection
    // and scene loads cannot accumulate an unbounded presentation backlog.
    mDependencies.inbound.ReceivePlayers(play);
    mDependencies.inbound.ReceiveCorpses();
    mDependencies.inbound.ReceiveWorld();
    mDependencies.frames.ProcessTransportFrame(play);
}

void NativeClientUpdateCoordinator::UpdateGameplay(PlayState* play,
                                                    double nowSeconds) {
    if (!play) return;
    const float deltaSeconds = mFrameClock.Sample(nowSeconds);
    mDependencies.inbound.ReceiveSceneAuthority(play);
    mDependencies.inbound.EnsureSceneAuthorized(play);
    mDependencies.frames.ReconcileGameplayFrame(play);
    mDependencies.outbound.SubmitPlayerCommand(play, deltaSeconds);
    mDependencies.frames.ProjectLocalPresentation(play);
    mDependencies.outbound.SubmitPresentation(play);
}

} // namespace Game::Multiplayer
