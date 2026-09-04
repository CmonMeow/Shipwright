#pragma once

#include "ClientSessionIngress.h"
#include "ProtocolDispatcher.h"

#include <functional>

namespace Game::Multiplayer {

struct ClientProtocolHandshakeActions {
    std::function<void()> sendIdentity;
    std::function<void()> sendPrivateChatKey;
};

// Admits decoded server messages into one client session. Transport framing,
// decryption, and protocol decoding happen before this endpoint; native game
// presentation happens after ClientSessionIngress accepts the result.
class ClientProtocolEndpoint final : public ClientProtocolSink {
  public:
    ClientProtocolEndpoint(ClientSessionIngress& ingress,
                           cCryptoSession& crypto,
                           ClientProtocolHandshakeActions handshake);

    void OnClientKeyAccept(const std::string& keyBytes) override;
    void OnClientPlayerAssign(
        const NetworkPlayerAssignPacket& assignment) override;
    void OnClientChat(const std::string& text) override;
    void OnClientChatKey(const DecodedChatKey& key) override;
    void OnClientPrivateChat(
        const DecodedPrivateChatToClient& message) override;
    void OnClientPlayerSnapshot(
        const NetworkPlayerSnapshotPacket& snapshot) override;
    void OnClientSceneEntryState(
        const NetworkSceneEntryStatePacket& state) override;
    void OnClientObjectiveState(
        const NetworkObjectiveStatePacket& state) override;
    void OnClientStrategicTopology(
        const NetworkStrategicTopologyPacket& topology) override;
    void OnClientStructureState(
        const NetworkStructureStatePacket& state) override;
    void OnClientCorpseState(const NetworkCorpseStatePacket& state) override;
    void OnClientFishingPresentation(
        const NetworkFishingPresentationPacket& state) override;
    void OnClientPlayerLifecycle(
        const NetworkPlayerLifecyclePacket& lifecycle) override;
    void OnClientFishState(const NetworkFishStatePacket& state) override;
    void OnClientLureState(const NetworkLureStatePacket& state) override;
    void OnClientProjectileLifecycle(
        const NetworkProjectileLifecyclePacket& lifecycle) override;
    void OnClientProjectileIntentResult(
        const NetworkProjectileIntentResultPacket& result) override;
    void OnClientProjectileState(
        const NetworkProjectileStatePacket& state) override;
    void OnClientCombatResult(
        const NetworkCombatResultPacket& result) override;
    void OnClientPlayerRespawn(
        const NetworkPlayerRespawnPacket& respawn) override;
    void OnClientVoice(NetworkVoicePacket packet) override;

  private:
    ClientSessionIngress& mIngress;
    cCryptoSession& mCrypto;
    ClientProtocolHandshakeActions mHandshake;
};

} // namespace Game::Multiplayer
