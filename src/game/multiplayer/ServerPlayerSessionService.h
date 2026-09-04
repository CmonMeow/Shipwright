#pragma once

#include "CommunicationInbox.h"
#include "PrivateChatService.h"
#include "ServerAdministrationService.h"
#include "ServerGameplayCommandService.h"
#include "ServerSessionManager.h"

#include <functional>
#include <string>

namespace Game::Multiplayer {

struct ServerPlayerSessionTransport {
    std::function<void(int32_t, NetAppMessageType, const NetworkMessageRaw&,
                       NetMsgFlags)> send;
    std::function<void(NetAppMessageType, const NetworkMessageRaw&)> broadcast;
    std::function<void(int32_t, bool, const std::string&)> kick;
    std::function<void(int32_t)> sendKnownChatKeys;
};

class ServerPlayerSessionService final {
  public:
    ServerPlayerSessionService(
        ServerSessionManager& sessions,
        ServerAdministrationService& administration,
        Game::Simulation::ServerWorld& world,
        Game::Replication::ServerReplicationCoordinator& replication,
        ClientReplicationInbox& clientInbox,
        PrivateChatService& privateChat,
        CommunicationInbox& communication,
        ServerGameplayCommandService& gameplayCommands,
        ServerReplicationInterestPublisher& interestPublisher,
        ServerReplicationEventPublisher& eventPublisher);

    void SetTransport(ServerPlayerSessionTransport transport);
    bool ConnectPeer(int32_t peer);
    bool AdmitIdentity(int32_t peer, const NetworkIdentity& identity);
    bool DisconnectPeer(int32_t peer);
    std::string PlayerName(int32_t player) const;

  private:
    void Send(int32_t peer, NetAppMessageType type,
              const NetworkMessageRaw& raw, NetMsgFlags flags) const;
    void Kick(int32_t peer, bool banned, const std::string& reason) const;

    ServerSessionManager& mSessions;
    ServerAdministrationService& mAdministration;
    Game::Simulation::ServerWorld& mWorld;
    Game::Replication::ServerReplicationCoordinator& mReplication;
    ClientReplicationInbox& mClientInbox;
    PrivateChatService& mPrivateChat;
    CommunicationInbox& mCommunication;
    ServerGameplayCommandService& mGameplayCommands;
    ServerReplicationInterestPublisher& mInterestPublisher;
    ServerReplicationEventPublisher& mEventPublisher;
    ServerPlayerSessionTransport mTransport;
};

} // namespace Game::Multiplayer
