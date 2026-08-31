#pragma once

#include "ProtocolDispatcher.h"

#include <functional>
#include <string>

namespace SoH::Network {

class SecureTransportChannel;
class ServerCommunicationService;
class ServerGameplayPacketIngress;
class ServerPlayerSessionService;
class ServerSessionManager;

struct ServerProtocolEndpointTransport {
    std::function<void(int32_t, const std::string&)> kick;
};

// Authenticated wire endpoint for server traffic. It owns handshake/identity
// admission and routes decoded semantic requests to communication or gameplay
// ingress; NetworkRuntime only supplies encrypted transport and disconnect.
class ServerProtocolEndpoint final : public ServerProtocolSink {
  public:
    ServerProtocolEndpoint(ServerSessionManager& sessions,
                           SecureTransportChannel& secureTransport,
                           ServerPlayerSessionService& playerSessions,
                           ServerCommunicationService& communication,
                           ServerGameplayPacketIngress& gameplay,
                           ServerProtocolEndpointTransport transport);

    void OnServerKeyHello(int32_t sender,
                          const std::string& keyBytes) override;
    void OnServerIdentity(int32_t sender,
                          const NetworkIdentity& identity) override;
    void OnServerChat(int32_t sender, const std::string& text) override;
    void OnServerChatKey(int32_t sender, const DecodedChatKey& key) override;
    void OnServerPrivateChat(
        int32_t sender, const DecodedPrivateChatToServer& message) override;
    void OnServerSceneEntryIntent(
        int32_t sender, NetworkSceneEntryIntentPacket packet) override;
    void OnServerPlayerCommand(
        int32_t sender, NetworkPlayerCommandPacket packet) override;
    void OnServerWeaponSelection(
        int32_t sender, NetworkWeaponSelectionIntentPacket packet) override;
    void OnServerStructureAction(
        int32_t sender, NetworkStructureActionPacket packet) override;
    void OnServerFishingPresentation(
        int32_t sender,
        NetworkFishingPresentationIntentPacket packet) override;
    void OnServerFishIntent(int32_t sender,
                            NetworkFishIntentPacket packet) override;
    void OnServerLureControlIntent(
        int32_t sender, NetworkLureControlIntentPacket packet) override;
    void OnServerArrowFireIntent(
        int32_t sender, NetworkArrowFireIntentPacket packet) override;
    void OnServerVoice(int32_t sender,
                       NetworkVoiceIntentPacket packet) override;

  private:
    void Kick(int32_t sender, const std::string& reason) const;

    ServerSessionManager& mSessions;
    SecureTransportChannel& mSecureTransport;
    ServerPlayerSessionService& mPlayerSessions;
    ServerCommunicationService& mCommunication;
    ServerGameplayPacketIngress& mGameplay;
    ServerProtocolEndpointTransport mTransport;
};

} // namespace SoH::Network
