#pragma once

#include "CommunicationInbox.h"
#include "PrivateChatService.h"
#include "ProtocolDispatcher.h"
#include "ServerAdministrationService.h"
#include "ServerSessionManager.h"
#include "../../platform/replication/ServerReplicationCoordinator.h"
#include "../../platform/simulation/ServerWorld.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SoH::Network {

struct ServerCommunicationDelivery {
    std::function<bool(int32_t, NetAppMessageType, const NetworkMessageRaw&,
                       NetMsgFlags)> send;
    std::function<void(NetAppMessageType, const NetworkMessageRaw&)> broadcast;
    std::function<bool(int32_t, const std::string&)> sendEncryptedPayload;
    std::function<std::vector<ServerAdministrationPlayer>()> players;
    std::function<void(int32_t, bool, const std::string&)> disconnectPlayer;
};

// Owns server-side communication policy. Transport only supplies delivery,
// connection metrics, and disconnect callbacks; it cannot route chat, voice,
// private keys, or administrator commands on its own.
class ServerCommunicationService final {
  public:
    ServerCommunicationService(
        ServerSessionManager& sessions,
        ServerAdministrationService& administration,
        PrivateChatService& privateChat,
        CommunicationInbox& communication,
        Game::Simulation::ServerWorld& world,
        Game::Replication::ServerReplicationCoordinator& replication);

    void SetDelivery(ServerCommunicationDelivery delivery);

    bool SendHostChat(const std::string& text);
    bool SendHostPrivateChat(int32_t targetPlayer, const std::string& text);
    void HandleChat(int32_t sender, const std::string& text);
    void HandleChatKey(int32_t sender, const DecodedChatKey& key);
    void HandlePrivateChat(int32_t sender,
                           const DecodedPrivateChatToServer& message);
    void HandleVoice(int32_t sender, NetworkVoiceIntentPacket packet);
    void SendKnownChatKeys(int32_t peer);

  private:
    std::string PlayerName(int32_t player) const;
    bool Send(int32_t peer, NetAppMessageType type,
              const NetworkMessageRaw& raw, NetMsgFlags flags) const;
    void Broadcast(NetAppMessageType type, const NetworkMessageRaw& raw) const;
    void SendChatKey(int32_t peer, int32_t owner, const std::string& name,
                     const std::string& publicKey) const;
    void BroadcastChatKey(int32_t owner, const std::string& name,
                          const std::string& publicKey) const;
    void SendCommandResult(int32_t player, const std::string& message);
    void BroadcastSystem(const std::string& message);
    void RunCommand(int32_t player, const std::string& command);

    ServerSessionManager& mSessions;
    ServerAdministrationService& mAdministration;
    PrivateChatService& mPrivateChat;
    CommunicationInbox& mCommunication;
    Game::Simulation::ServerWorld& mWorld;
    Game::Replication::ServerReplicationCoordinator& mReplication;
    ServerCommunicationDelivery mDelivery;
};

} // namespace SoH::Network
