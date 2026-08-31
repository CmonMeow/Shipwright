#pragma once

struct PlayState;

namespace Game::Client {
class ClientGameplaySession;
}

namespace SoH::Network {

class NativeCombatPresentationController;
class NativeLocalPlayerPresentationController;
class NativeLocalRespawnController;
class NativePlayerNameplatePresenter;
class NativeProjectileRenderer;
class NativeRemotePlayerRenderer;
class NetworkRuntime;

struct NativeClientFrameDependencies {
    NetworkRuntime& runtime;
    Game::Client::ClientGameplaySession& gameplay;
    NativeLocalRespawnController& respawn;
    NativeCombatPresentationController& combat;
    NativeRemotePlayerRenderer& players;
    NativeProjectileRenderer& projectiles;
    NativeLocalPlayerPresentationController& localPresentation;
    NativePlayerNameplatePresenter& nameplates;
};

// Owns native per-frame projection of admitted authoritative state. Transport
// draining and command submission remain separate; this boundary only turns
// already-admitted state into native actors, feedback, corrections and HUD.
class NativeClientFrameReconciliation final {
  public:
    explicit NativeClientFrameReconciliation(
        NativeClientFrameDependencies dependencies);

    // Runs from the transport frame hook, including while PlayState gameplay
    // simulation is frozen by death or scene transitions.
    void ProcessTransportFrame(PlayState* play);

    // Runs once per gameplay update before the next local command is sampled.
    void ReconcileGameplayFrame(PlayState* play, float deltaSeconds);

    // Projects the prediction state after this frame's command sample so the
    // native animation observes the same action edge that was just submitted.
    void ProjectLocalPresentation(PlayState* play);

  private:
    void ProcessRespawns(PlayState* play);
    void QueueNameplates(PlayState* play);

    NativeClientFrameDependencies mDependencies;
};

} // namespace SoH::Network
