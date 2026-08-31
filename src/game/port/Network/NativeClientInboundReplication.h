#pragma once

struct PlayState;

namespace Game::Client {
class ClientGameplaySession;
}

namespace SoH::Network {

class NativeCorpsePresentationController;
class NativeLocalProjectileController;
class NativeRemotePlayerPresentationController;
class NativeRemoteProjectilePresentationController;
class NetworkRuntime;

struct NativeClientInboundDependencies {
    NetworkRuntime& runtime;
    Game::Client::ClientGameplaySession& gameplay;
    NativeRemotePlayerPresentationController& remotePlayers;
    NativeRemoteProjectilePresentationController& remoteProjectiles;
    NativeLocalProjectileController& localProjectiles;
    NativeCorpsePresentationController& corpses;
};

// Drains already-decoded, admitted runtime state into semantic client replicas.
// It owns no transport and creates no native actors; render reconciliation
// remains in the frame coordinator/composition root.
class NativeClientInboundReplication final {
  public:
    explicit NativeClientInboundReplication(
        NativeClientInboundDependencies dependencies);

    void ReceivePlayers(PlayState* play);
    void ReceiveCorpses();
    void ReceiveWorld();
    void ReceiveSceneAuthority(PlayState* play);
    bool EnsureSceneAuthorized(PlayState* play);

  private:
    NativeClientInboundDependencies mDependencies;
};

} // namespace SoH::Network
