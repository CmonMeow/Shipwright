#pragma once

#include "../../platform/client/ClientFrameClock.h"

struct PlayState;
class MultiplayerUI;

namespace SoH::Network {

class NetworkRuntime;
class NativeClientFrameReconciliation;
class NativeClientInboundReplication;
class NativeClientOutboundSubmission;
class NativeClientSessionLifecycle;

struct NativeClientUpdateDependencies {
    NetworkRuntime& runtime;
    NativeClientInboundReplication& inbound;
    NativeClientOutboundSubmission& outbound;
    NativeClientFrameReconciliation& frames;
    NativeClientSessionLifecycle& sessionLifecycle;
    MultiplayerUI& multiplayerUI;
};

// Owns the ordering contract between transport admission, native projection,
// command submission, and reconnect lifecycle. It contains no entity or combat
// policy; those remain in the injected semantic/native coordinators.
class NativeClientUpdateCoordinator {
  public:
    explicit NativeClientUpdateCoordinator(
        NativeClientUpdateDependencies dependencies);

    void ResetClock(double nowSeconds);
    void PumpMoveLoop(PlayState* play);
    void UpdateTransport(PlayState* play, double nowSeconds);
    void UpdateGameplay(PlayState* play, double nowSeconds);

  private:
    NativeClientUpdateDependencies mDependencies;
    Game::Client::ClientFrameClock mFrameClock;
};

} // namespace SoH::Network
