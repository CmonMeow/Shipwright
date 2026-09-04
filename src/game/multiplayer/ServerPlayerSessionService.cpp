#include "ServerPlayerSessionService.h"

#include "PlayerLifecycleNetworkAdapter.h"

#include <utility>

namespace Game::Multiplayer {

namespace {

const NetMsgFlags kReliable =
    static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

} // namespace

ServerPlayerSessionService::ServerPlayerSessionService(
    ServerSessionManager& sessions,
    ServerAdministrationService& administration,
    Game::Simulation::ServerWorld& world,
    Game::Replication::ServerReplicationCoordinator& replication,
    ClientReplicationInbox& clientInbox,
    PrivateChatService& privateChat,
    CommunicationInbox& communication,
    ServerGameplayCommandService& gameplayCommands,
    ServerReplicationInterestPublisher& interestPublisher,
    ServerReplicationEventPublisher& eventPublisher)
    : mSessions(sessions), mAdministration(administration), mWorld(world),
      mReplication(replication), mClientInbox(clientInbox),
      mPrivateChat(privateChat), mCommunication(communication),
      mGameplayCommands(gameplayCommands), mInterestPublisher(interestPublisher),
      mEventPublisher(eventPublisher) {
}

void ServerPlayerSessionService::SetTransport(
    ServerPlayerSessionTransport transport) {
    mTransport = std::move(transport);
}

void ServerPlayerSessionService::Send(
    int32_t peer, NetAppMessageType type, const NetworkMessageRaw& raw,
    NetMsgFlags flags) const {
    if (peer > 0 && mTransport.send) mTransport.send(peer, type, raw, flags);
}

void ServerPlayerSessionService::Kick(
    int32_t peer, bool banned, const std::string& reason) const {
    if (mTransport.kick) mTransport.kick(peer, banned, reason);
}

bool ServerPlayerSessionService::ConnectPeer(int32_t peer) {
    return mSessions.ConnectPeer(peer);
}

std::string ServerPlayerSessionService::PlayerName(int32_t player) const {
    if (player == 0) return "system";
    const NetworkIdentity* identity = mSessions.IdentityFor(player);
    if (identity && !identity->name.empty()) return identity->name;
    const std::string privateName = mPrivateChat.PeerName(player);
    if (!privateName.empty()) return privateName;
    return "player " + std::to_string(player);
}

bool ServerPlayerSessionService::AdmitIdentity(
    int32_t peer, const NetworkIdentity& identity) {
    if (!mSessions.HasPeer(peer) || mSessions.HasIdentity(peer) ||
        !identity.authenticated || identity.id.empty()) {
        Kick(peer, false, "identity admission failed");
        return false;
    }
    if (mAdministration.IsBanned(identity.id)) {
        Kick(peer, true, "banned");
        return false;
    }
    const auto initial = mWorld.AdmitPlayerAtDefaultSpawn(peer);
    if (!initial) {
        Kick(peer, false, "server spawn unavailable");
        return false;
    }
    if (!mSessions.AdmitIdentity(peer, identity)) {
        mWorld.RemovePlayer(peer);
        Kick(peer, false, "identity admission failed");
        return false;
    }

    mCommunication.ActivateVoicePlayer(peer);
    NetworkMessageRaw assignmentRaw;
    EncodeAppPacketRaw(assignmentRaw, NetworkPlayerAssignPacket{ peer });
    Send(peer, NAMTPlayerAssign, assignmentRaw, kReliable);

    const NetworkPlayerLifecyclePacket lifecycle{
        peer, initial->entity.index, initial->entity.generation,
        initial->sceneId, 1
    };
    NetworkMessageRaw lifecycleRaw;
    EncodeAppPacketRaw(lifecycleRaw, lifecycle);
    Send(peer, NAMTPlayerLifecycle, lifecycleRaw, kReliable);
    mGameplayCommands.SendSceneEntryState(peer, 0, true);
    mEventPublisher.PublishStrategicTopologyTo(peer);
    if (mTransport.sendKnownChatKeys) mTransport.sendKnownChatKeys(peer);

    mInterestPublisher.RefreshAll();
    return true;
}

bool ServerPlayerSessionService::DisconnectPeer(int32_t peer) {
    if (!mSessions.HasPeer(peer)) return false;
    const Game::Replication::ReplicationPlayerDeparture replicationDeparture =
        mReplication.RemovePlayer(peer);
    const auto departure = mSessions.DisconnectPeer(peer);
    if (!departure) return false;

    mWorld.RemovePlayer(peer);
    mPrivateChat.RemovePeer(peer);
    mCommunication.ForgetVoicePlayer(peer);
    mEventPublisher.PublishProjectileEvents();
    mInterestPublisher.RefreshOwnedEntities();
    for (const auto& leave : replicationDeparture.playerLeaves) {
        mReplication.RemoveQueuedEntity(leave.observerPlayerId,
                                        leave.subject.playerId,
                                        leave.subject.entity);
        const NetworkPlayerLifecyclePacket lifecycle =
            PlayerLifecycleNetworkAdapter::ToPacket(leave.subject, false);
        if (leave.observerPlayerId == 0) {
            mClientInbox.AcceptPlayerLifecycle(lifecycle);
        } else {
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, lifecycle);
            Send(leave.observerPlayerId, NAMTPlayerLifecycle, raw, kReliable);
        }
    }
    return true;
}

} // namespace Game::Multiplayer
