#pragma once

#include "NetworkProtocol.h"
#include "NetworkConnectionTransport.h"
#include "NetworkProtocolIngress.h"
#include "NetworkSessionLifecycleService.h"
#include "ClientReplicationInbox.h"
#include "ClientSessionIngress.h"
#include "ClientProtocolEndpoint.h"
#include "CommunicationInbox.h"
#include "PrivateChatService.h"
#include "LocalClientAdmissionService.h"
#include "LocalGameplayCommandService.h"
#include "LocalTextCommunicationService.h"
#include "LocalVoiceSubmissionService.h"
#include "ServerAdministrationService.h"
#include "ServerCommunicationService.h"
#include "ServerReplicationInterestPublisher.h"
#include "ServerReplicationEventPublisher.h"
#include "ServerGameplayCommandService.h"
#include "ServerGameplayPacketIngress.h"
#include "ServerPlayerSessionService.h"
#include "SecureTransportChannel.h"
#include "ServerWorldBootstrap.h"
#include "ServerSessionManager.h"
#include "ServerProtocolEndpoint.h"
#include "platform/client/LocalPlayerCommandStream.h"
#include "platform/client/LocalFishIntentStream.h"
#include "platform/client/LocalFishingUpdateStream.h"
#include "platform/client/LocalStructureActionStream.h"
#include "platform/replication/ServerReplicationCoordinator.h"
#include "platform/server/ServerAuthorityScheduler.h"
#include "platform/server/ServerWorldManagement.h"
#include "platform/simulation/ServerGameplayIngress.h"
#include "platform/simulation/ServerWorld.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Game::Multiplayer {

struct NetworkPlayerInfo {
    int32_t playerId = -1;
    std::string identity;
    std::string name;
    bool voiceClient = false;
};

class NetworkRuntime final {
  public:
    NetworkRuntime();
    explicit NetworkRuntime(int32_t defaultGameplayScene);
    ~NetworkRuntime();

    NetworkRuntime(const NetworkRuntime&) = delete;
    NetworkRuntime& operator=(const NetworkRuntime&) = delete;

    bool Host(uint16_t port = DEFAULT_NETWORK_PORT, const std::string& sessionName = "Ocarina of Time");
    bool Connect(const std::string& address = DEFAULT_NETWORK_ADDRESS, uint16_t port = DEFAULT_NETWORK_PORT);
    void Disconnect();
    void Update();

    bool IsHost() const;
    bool IsClient() const;
    bool IsActive() const;
    bool IsSecure() const;
    uint64_t SessionGeneration() const;
    int32_t LocalPlayerId() const;
    int32_t LatencyMilliseconds() const;
    int32_t ThroughputBytesPerSecond() const;
    int32_t InboundBytesPerSecond() const;
    int32_t OutboundBytesPerSecond() const;

    std::vector<NetworkPlayerInfo> Players() const;
    bool SendChat(const std::string& message);
    bool SendPrivateChat(int32_t targetPlayer, const std::string& message);
    bool SendPlayerCommand(Game::Simulation::PlayerCommand command,
                           uint32_t expectedLifeEpoch = 0);
    bool SendWeaponSelection(const Game::Client::LocalWeaponSelectionRequest& request);
    bool SendSceneEntryIntent(const Game::Client::LocalSceneEntryRequest& request);
    bool ConfigureSceneSpawn(const Game::Simulation::PlayerSpawn& spawn);
    bool AuthorizeSceneTransition(int32_t playerId, int32_t destinationSceneId);
    bool SendFishingPresentation(
        const Game::Replication::FishingPresentationState& presentation);
    bool SendFishIntent(const Game::Client::LocalFishIntent& intent);
    bool SendLureControlIntent(const Game::Client::LocalLureControlIntent& intent);
    bool SendArrowFireIntent(const Game::Client::LocalProjectileIntent& intent);
    bool SendVoiceFrame(std::vector<uint8_t> opusData);
    bool SendStructureAction(const Game::Client::LocalStructureAction& action);
    Game::Simulation::EntityId EnsureObjective(const Game::Simulation::ObjectiveDefinition& definition);
    Game::Simulation::EntityId EnsureStrategicSite(
        const Game::Simulation::ObjectiveDefinition& objective,
        Game::Simulation::StrategicSiteKind kind, int32_t influenceRegionKey);
    bool EnsureSupplyRoute(const Game::Simulation::SupplyRouteDefinition& definition);
    bool RemoveSupplyRoute(int32_t routeKey);
    bool EnsureInfluenceAdjacency(
        const Game::Simulation::InfluenceRegionAdjacencyDefinition& definition);
    bool RemoveInfluenceAdjacency(int32_t adjacencyKey);
    bool RemoveObjective(int32_t objectiveKey);
    Game::Simulation::EntityId EnsureStructure(const Game::Simulation::StructureDefinition& definition);
    bool RemoveStructure(int32_t structureKey);
    bool PollChat(NetworkChatLine& line);
    bool PollPlayerSnapshot(Game::Simulation::PlayerSnapshot& snapshot);
    bool PollSceneEntryState(Game::Client::LocalSceneAuthority& authority);
    bool PollFishingPresentation(
        Game::Replication::FishingPresentationState& state);
    bool PollPlayerLifecycle(Game::Client::RemotePlayerPresentationState& state);
    bool PollFishState(Game::Client::RemoteFishEntity& state);
    bool PollLureState(Game::Client::RemoteLureEntity& state);
    bool PollProjectileState(Game::Client::RemoteProjectileReplicaState& state);
    bool PollProjectileIntentResult(
        Game::Client::LocalProjectileIntentDecision& decision);
    bool PollCombatResult(Game::Simulation::CombatResultEvent& event);
    bool PollPlayerRespawn(Game::Simulation::PlayerRespawnEvent& event);
    bool PollVoice(NetworkVoicePacket& packet);
    bool PollObjectiveState(Game::Client::ReplicatedObjectiveState& state);
    bool PollStrategicTopology(
        Game::Client::ReplicatedStrategicTopologyState& state);
    bool PollStructureState(Game::Client::ReplicatedStructureState& state);
    bool PollCorpseState(Game::Client::CorpsePresentationState& state);

  private:
    void HandlePeerCreated(int32_t peer);
    void HandlePeerDeleted(int32_t peer);

    void Broadcast(NetAppMessageType type, const NetworkMessageRaw& raw, int32_t exceptPlayer = -1,
                   NetMsgFlags flags = static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority),
                   Game::Replication::ReplicationStreamKey streamKey = {});

    NetworkConnectionTransport mConnection;
    int32_t mDefaultGameplayScene;
    ServerSessionManager mServerSessions;
    ServerAdministrationService mAdministration;
    PrivateChatService mPrivateChat;
    CommunicationInbox mCommunication;
    cCryptoSession mClientCrypto;
    ClientReplicationInbox mClientInbox;
    ClientSessionIngress mClientIngress;
    Game::Simulation::ServerWorld mServerWorld;
    Game::Replication::ServerReplicationCoordinator mReplication;
    SecureTransportChannel mSecureTransport;
    LocalClientAdmissionService mClientAdmission;
    LocalVoiceSubmissionService mLocalVoiceSubmission;
    ServerReplicationInterestPublisher mInterestPublisher;
    Game::Server::ServerWorldManagement mWorldManagement;
    ServerReplicationEventPublisher mEventPublisher;
    ServerGameplayCommandService mGameplayCommands;
    ServerGameplayPacketIngress mServerGameplayIngress;
    LocalGameplayCommandService mLocalGameplayCommands;
    ServerCommunicationService mServerCommunication;
    LocalTextCommunicationService mLocalTextCommunication;
    ServerPlayerSessionService mPlayerSessions;
    Game::Server::ServerAuthorityScheduler mAuthorityScheduler;
    NetworkSessionLifecycleService mSessionLifecycle;
    ServerWorldBootstrap mWorldBootstrap;
    ClientProtocolEndpoint mClientProtocol;
    ServerProtocolEndpoint mServerProtocol;
    NetworkProtocolIngress mProtocolIngress;
};

} // namespace Game::Multiplayer
