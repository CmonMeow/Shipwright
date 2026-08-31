#include "NetworkRuntime.h"
#include "LocalNetworkIdentity.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace SoH::Network {

namespace {

constexpr int32_t kDefaultGameplayScene = 110;

} // namespace

NetworkRuntime::NetworkRuntime()
    : mConnection({
          [this](char* buffer, int32_t size) {
              mProtocolIngress.ReceiveClient(buffer, size);
          },
          [this](int32_t sender, char* buffer, int32_t size) {
              mProtocolIngress.ReceiveServer(sender, buffer, size);
          },
          [this](int32_t peer) { HandlePeerCreated(peer); },
          [this](int32_t peer) { HandlePeerDeleted(peer); },
      }),
      mClientIngress(mClientInbox, mCommunication, mPrivateChat),
      mSecureTransport(mClientCrypto, mServerSessions, mReplication),
      mClientAdmission(
          mClientCrypto, mPrivateChat,
          {
              [this]() { return mConnection.ClientReady(); },
              [this](NetAppMessageType type, const NetworkMessageRaw& raw) {
                  return mSecureTransport.SendPlainToServer(type, raw);
              },
              [this](NetAppMessageType type, const NetworkMessageRaw& raw,
                     NetMsgFlags flags) {
                  return mSecureTransport.SendToServer(type, raw, flags);
              },
          }),
      mLocalVoiceSubmission({
          [this]() {
              if (mConnection.ClientReady()) return LocalVoiceSubmissionRole::Client;
              if (mConnection.IsHost()) return LocalVoiceSubmissionRole::Host;
              return LocalVoiceSubmissionRole::Inactive;
          },
          [this](const NetworkVoiceIntentPacket& packet) {
              return LocalPlayerId() >= 0 &&
                     mSecureTransport.SendEncryptedPayloadToServer(
                         BuildVoiceIntentPayload(packet), NMFHighPriority);
          },
          [this]() { return mReplication.PlayerObservers(0); },
          [this](int32_t observer, const std::string& payload) {
              return mSecureTransport.SendEncryptedPayloadToPeer(
                  observer, payload, NMFHighPriority);
          },
      }),
      mInterestPublisher(mServerWorld, mReplication, mClientInbox),
      mWorldManagement(mServerWorld,
                       [this]() {
                           mInterestPublisher.RefreshSpatialEntities();
                           mEventPublisher.PublishStrategicTopology();
                       }),
      mEventPublisher(mServerWorld, mReplication, mClientInbox,
                      mInterestPublisher),
      mGameplayCommands(mServerWorld, mClientInbox, mInterestPublisher,
                        mEventPublisher),
      mServerGameplayIngress(mGameplayCommands),
      mLocalGameplayCommands(
          mClientIngress, mSecureTransport, mGameplayCommands, mServerWorld,
          {
              [this]() { return mConnection.ClientReady(); },
              [this]() { return mConnection.IsHost(); },
          }),
      mServerCommunication(mServerSessions, mAdministration, mPrivateChat,
                           mCommunication, mServerWorld, mReplication),
      mLocalTextCommunication(
          mPrivateChat, mCommunication,
          {
              [this]() {
                  if (mConnection.ClientReady()) return LocalTextCommunicationRole::Client;
                  if (mConnection.IsHost()) return LocalTextCommunicationRole::Host;
                  return LocalTextCommunicationRole::Inactive;
              },
              [this](NetAppMessageType type, const NetworkMessageRaw& raw,
                     NetMsgFlags flags) {
                  return mSecureTransport.SendToServer(type, raw, flags);
              },
              [this](const std::string& text) {
                  return mServerCommunication.SendHostChat(text);
              },
              [this](int32_t targetPlayer, const std::string& text) {
                  return mServerCommunication.SendHostPrivateChat(targetPlayer, text);
              },
              [this](int32_t player) { return mPlayerSessions.PlayerName(player); },
          }),
      mPlayerSessions(mServerSessions, mAdministration, mServerWorld,
                      mReplication, mClientInbox, mPrivateChat,
                      mCommunication, mGameplayCommands, mInterestPublisher,
                      mEventPublisher),
      mAuthorityScheduler(mServerWorld, {
          [this]() { mEventPublisher.PublishPlayerSnapshots(); },
          [this]() { mInterestPublisher.RefreshPlayers(mServerWorld.PlayerSnapshots()); },
          [this]() { mEventPublisher.PublishObjectiveSnapshots(); },
          [this]() { mEventPublisher.PublishStructureSnapshots(); },
          [this]() { mEventPublisher.PublishProjectileEvents(); },
          [this]() { mInterestPublisher.RefreshOwnedEntities(); },
          [this]() { mEventPublisher.PublishFishingEvents(); },
          [this]() { mEventPublisher.PublishCombatResults(); },
          [this]() { mEventPublisher.PublishLifeEvents(); },
      }),
      mSessionLifecycle(
          mConnection, mServerSessions, mCommunication, mClientCrypto,
          mClientAdmission, mClientIngress, mServerWorld, mReplication,
          mSecureTransport, mLocalVoiceSubmission, mAuthorityScheduler),
      mClientProtocol(
          mClientIngress, mClientCrypto,
          { [this]() { mClientAdmission.SubmitIdentity(); },
            [this]() { mClientAdmission.SubmitPrivateChatKey(); } }),
      mServerProtocol(
          mServerSessions, mSecureTransport, mPlayerSessions,
          mServerCommunication, mServerGameplayIngress,
          { [this](int32_t peer, const std::string& reason) {
              mConnection.Kick(peer, NTRKicked, reason);
          } }),
      mProtocolIngress(
          mSecureTransport, mServerSessions, mClientProtocol, mServerProtocol,
          { [this](int32_t peer, const std::string& reason) {
              mConnection.Kick(peer, NTRKicked, reason);
          } }) {
    mSecureTransport.SetDelivery({
        [this](const std::string& payload, NetMsgFlags flags) {
            return mConnection.SendToServer(payload, flags);
        },
        [this](int32_t peer, const std::string& payload, NetMsgFlags flags) {
            return mConnection.SendToPeer(peer, payload, flags);
        },
    });
    ServerReplicationDelivery delivery{
        [this]() { return mServerSessions.AdmittedPeers(); },
        [this](int32_t observer, NetAppMessageType type,
               const NetworkMessageRaw& raw, NetMsgFlags flags,
               Game::Replication::ReplicationStreamKey streamKey) {
            mSecureTransport.SendToPeer(observer, type, raw, flags, streamKey);
        },
        [this](int32_t player) { return mServerSessions.HasIdentity(player); },
    };
    mInterestPublisher.SetDelivery(delivery);
    mEventPublisher.SetDelivery(delivery);
    mGameplayCommands.SetDelivery(std::move(delivery));
    mServerCommunication.SetDelivery({
        [this](int32_t peer, NetAppMessageType type,
               const NetworkMessageRaw& raw, NetMsgFlags flags) {
            return mSecureTransport.SendToPeer(peer, type, raw, flags);
        },
        [this](NetAppMessageType type, const NetworkMessageRaw& raw) {
            Broadcast(type, raw);
        },
        [this](int32_t peer, const std::string& payload) {
            return mSecureTransport.SendEncryptedPayloadToPeer(
                peer, payload, NMFHighPriority);
        },
        [this]() {
            std::vector<ServerAdministrationPlayer> players;
            if (mConnection.IsHost()) {
                players.push_back({ 0, LocalIdentityId(), LocalUserName(), 0 });
            }
            for (const int32_t player : mServerSessions.AdmittedPeers()) {
                const NetworkIdentity* identity = mServerSessions.IdentityFor(player);
                if (!identity) continue;
                int32_t latency = 0;
                int32_t throughput = 0;
                mConnection.GetPeerConnectionInfo(player, latency, throughput);
                players.push_back({ player, identity->id, identity->name, latency });
            }
            return players;
        },
        [this](int32_t peer, bool banned, const std::string& reason) {
            mConnection.Kick(peer, banned ? NTRBanned : NTRKicked, reason);
        },
    });
    mPlayerSessions.SetTransport({
        [this](int32_t peer, NetAppMessageType type,
               const NetworkMessageRaw& raw, NetMsgFlags flags) {
            mSecureTransport.SendToPeer(peer, type, raw, flags);
        },
        [this](NetAppMessageType type, const NetworkMessageRaw& raw) {
            Broadcast(type, raw);
        },
        [this](int32_t peer, bool banned, const std::string& reason) {
            mConnection.Kick(peer, banned ? NTRBanned : NTRKicked, reason);
        },
        [this](int32_t peer) { mServerCommunication.SendKnownChatKeys(peer); },
    });
    if (sodium_init() < 0) {
        Error("Network runtime: libsodium initialization failed");
        return;
    }
    if (!mPrivateChat.Initialize()) {
        Error("Network runtime: private chat key initialization failed");
    }
}

NetworkRuntime::~NetworkRuntime() {
    Disconnect();
}

bool NetworkRuntime::Host(uint16_t port, const std::string& sessionName) {
    if (!mConnection.Host(port, sessionName)) return false;
    mAdministration.Load();
    mWorldManagement.ConfigureSceneSpawn(
        Game::Simulation::PlayerSpawn{ kDefaultGameplayScene, {}, 0.0f });
    mWorldBootstrap.Initialize(mServerWorld);
    mAuthorityScheduler.Reset();
    return true;
}

bool NetworkRuntime::Connect(const std::string& address) {
    return mConnection.Connect(address, LocalUserName());
}

void NetworkRuntime::Disconnect() {
    mSessionLifecycle.Disconnect();
}

void NetworkRuntime::Update() {
    mSessionLifecycle.Update();
}

bool NetworkRuntime::IsHost() const {
    return mConnection.IsHost();
}

bool NetworkRuntime::IsClient() const {
    return mConnection.IsClient();
}

bool NetworkRuntime::IsActive() const {
    return IsHost() || IsClient();
}

bool NetworkRuntime::IsSecure() const {
    return mSessionLifecycle.IsSecure();
}

uint64_t NetworkRuntime::SessionGeneration() const {
    return mSessionLifecycle.SessionGeneration();
}

int32_t NetworkRuntime::LocalPlayerId() const {
    return mConnection.IsHost() ? 0 : mClientIngress.LocalPlayerId();
}

int32_t NetworkRuntime::LatencyMilliseconds() const {
    return mConnection.LatencyMilliseconds();
}

int32_t NetworkRuntime::ThroughputBytesPerSecond() const {
    return mConnection.ThroughputBytesPerSecond();
}

int32_t NetworkRuntime::InboundBytesPerSecond() const {
    return mSessionLifecycle.InboundBytesPerSecond();
}

int32_t NetworkRuntime::OutboundBytesPerSecond() const {
    return mSessionLifecycle.OutboundBytesPerSecond();
}

std::vector<NetworkPlayerInfo> NetworkRuntime::Players() const {
    std::vector<NetworkPlayerInfo> result;
    if (mConnection.IsHost()) {
        result.push_back({ 0, LocalIdentityId(), LocalUserName(), false });
    }
    for (const int32_t player : mServerSessions.AdmittedPeers()) {
        const NetworkIdentity* identity = mServerSessions.IdentityFor(player);
        result.push_back({ player, identity->id, identity->name, identity->voiceClient });
    }
    // Clients learn peer names with the E2E private-chat public keys. Expose
    // that roster too so name-based /pm and in-world labels work without
    // weakening the server-owned identity exchange.
    for (const auto& peer : mPrivateChat.Peers()) {
        if (peer.playerId == 0 || peer.playerId == mClientIngress.LocalPlayerId() ||
            peer.playerName.empty()) {
            continue;
        }
        const auto existing = std::find_if(result.begin(), result.end(), [&peer](const NetworkPlayerInfo& info) {
            return info.playerId == peer.playerId;
        });
        if (existing == result.end()) {
            result.push_back({ peer.playerId, "", peer.playerName, false });
        }
    }
    return result;
}

bool NetworkRuntime::SendChat(const std::string& message) {
    return mLocalTextCommunication.SendChat(message);
}

bool NetworkRuntime::SendPrivateChat(int32_t targetPlayer, const std::string& message) {
    return mLocalTextCommunication.SendPrivateChat(targetPlayer, message);
}

bool NetworkRuntime::SendPlayerCommand(Game::Simulation::PlayerCommand command,
                                       uint32_t expectedLifeEpoch) {
    return mLocalGameplayCommands.SubmitPlayerCommand(
        std::move(command), expectedLifeEpoch);
}

bool NetworkRuntime::SendWeaponSelection(
    const Game::Client::LocalWeaponSelectionRequest& request) {
    return mLocalGameplayCommands.SelectWeapon(request);
}

bool NetworkRuntime::SendSceneEntryIntent(
    const Game::Client::LocalSceneEntryRequest& request) {
    return mLocalGameplayCommands.EnterScene(request);
}

bool NetworkRuntime::ConfigureSceneSpawn(const Game::Simulation::PlayerSpawn& spawn) {
    return mConnection.IsHost() && mWorldManagement.ConfigureSceneSpawn(spawn);
}

bool NetworkRuntime::AuthorizeSceneTransition(int32_t playerId,
                                              int32_t destinationSceneId) {
    return mConnection.IsHost() && mWorldManagement.AuthorizeSceneTransition(
                          playerId, destinationSceneId);
}

bool NetworkRuntime::SendFishingPresentation(
    const Game::Replication::FishingPresentationState& presentationState) {
    return mLocalGameplayCommands.SubmitFishingPresentation(presentationState);
}

bool NetworkRuntime::SendFishIntent(const Game::Client::LocalFishIntent& intent) {
    return mLocalGameplayCommands.SubmitFishAction(intent);
}

bool NetworkRuntime::SendLureControlIntent(
    const Game::Client::LocalLureControlIntent& intent) {
    return mLocalGameplayCommands.SubmitLureControl(intent);
}

bool NetworkRuntime::SendArrowFireIntent(
    const Game::Client::LocalProjectileIntent& intent) {
    return mLocalGameplayCommands.FireProjectile(intent);
}

bool NetworkRuntime::SendVoiceFrame(std::vector<uint8_t> opusData) {
    return mLocalVoiceSubmission.Submit(std::move(opusData));
}

bool NetworkRuntime::SendStructureAction(
    const Game::Client::LocalStructureAction& action) {
    return mLocalGameplayCommands.SubmitStructureAction(action);
}

Game::Simulation::EntityId NetworkRuntime::EnsureObjective(
    const Game::Simulation::ObjectiveDefinition& definition) {
    if (!mConnection.IsHost()) return {};
    return mWorldManagement.EnsureObjective(definition);
}

Game::Simulation::EntityId NetworkRuntime::EnsureStrategicSite(
    const Game::Simulation::ObjectiveDefinition& objective,
    Game::Simulation::StrategicSiteKind kind, int32_t influenceRegionKey) {
    if (!mConnection.IsHost()) return {};
    return mWorldManagement.EnsureStrategicSite(objective, kind,
                                                influenceRegionKey);
}

bool NetworkRuntime::EnsureSupplyRoute(
    const Game::Simulation::SupplyRouteDefinition& definition) {
    return mConnection.IsHost() && mWorldManagement.EnsureSupplyRoute(definition);
}

bool NetworkRuntime::RemoveSupplyRoute(int32_t routeKey) {
    return mConnection.IsHost() && mWorldManagement.RemoveSupplyRoute(routeKey);
}

bool NetworkRuntime::EnsureInfluenceAdjacency(
    const Game::Simulation::InfluenceRegionAdjacencyDefinition& definition) {
    return mConnection.IsHost() && mWorldManagement.EnsureInfluenceAdjacency(definition);
}

bool NetworkRuntime::RemoveInfluenceAdjacency(int32_t adjacencyKey) {
    return mConnection.IsHost() && mWorldManagement.RemoveInfluenceAdjacency(adjacencyKey);
}

bool NetworkRuntime::RemoveObjective(int32_t objectiveKey) {
    return mConnection.IsHost() && mWorldManagement.RemoveObjective(objectiveKey);
}

Game::Simulation::EntityId NetworkRuntime::EnsureStructure(
    const Game::Simulation::StructureDefinition& definition) {
    if (!mConnection.IsHost()) return {};
    return mWorldManagement.EnsureStructure(definition);
}

bool NetworkRuntime::RemoveStructure(int32_t structureKey) {
    return mConnection.IsHost() && mWorldManagement.RemoveStructure(structureKey);
}

bool NetworkRuntime::PollChat(NetworkChatLine& line) {
    return mCommunication.PollChat(line);
}

bool NetworkRuntime::PollPlayerSnapshot(
    Game::Simulation::PlayerSnapshot& snapshot) {
    return mClientInbox.Poll(snapshot);
}

bool NetworkRuntime::PollSceneEntryState(
    Game::Client::LocalSceneAuthority& authority) {
    return mClientInbox.Poll(authority);
}

bool NetworkRuntime::PollObjectiveState(
    Game::Client::ReplicatedObjectiveState& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollStrategicTopology(
    Game::Client::ReplicatedStrategicTopologyState& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollStructureState(
    Game::Client::ReplicatedStructureState& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollCorpseState(Game::Client::CorpsePresentationState& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollFishingPresentation(
    Game::Replication::FishingPresentationState& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollPlayerLifecycle(
    Game::Client::RemotePlayerPresentationState& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollFishState(Game::Client::RemoteFishEntity& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollLureState(Game::Client::RemoteLureEntity& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollProjectileState(
    Game::Client::RemoteProjectileReplicaState& state) {
    return mClientInbox.Poll(state);
}

bool NetworkRuntime::PollProjectileIntentResult(
    Game::Client::LocalProjectileIntentDecision& decision) {
    return mClientInbox.Poll(decision);
}

bool NetworkRuntime::PollCombatResult(Game::Simulation::CombatResultEvent& event) {
    return mClientInbox.Poll(event);
}

bool NetworkRuntime::PollPlayerRespawn(Game::Simulation::PlayerRespawnEvent& event) {
    return mClientInbox.Poll(event);
}

bool NetworkRuntime::PollVoice(NetworkVoicePacket& packet) {
    return mCommunication.PollVoice(packet);
}

void NetworkRuntime::HandlePeerCreated(int32_t peer) {
    mPlayerSessions.ConnectPeer(peer);
}

void NetworkRuntime::HandlePeerDeleted(int32_t peer) {
    mPlayerSessions.DisconnectPeer(peer);
}











void NetworkRuntime::Broadcast(NetAppMessageType type, const NetworkMessageRaw& raw, int32_t exceptPlayer,
                               NetMsgFlags flags,
                               Game::Replication::ReplicationStreamKey streamKey) {
    for (const int32_t peer : mServerSessions.AdmittedPeers()) {
        if (peer != exceptPlayer) {
            mSecureTransport.SendToPeer(peer, type, raw, flags, streamKey);
        }
    }
}


} // namespace SoH::Network
