#include "ServerCommunicationService.h"

#include <utility>

namespace Game::Multiplayer {

namespace {

const NetMsgFlags kReliable =
    static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

} // namespace

ServerCommunicationService::ServerCommunicationService(
    ServerSessionManager& sessions,
    ServerAdministrationService& administration,
    PrivateChatService& privateChat,
    CommunicationInbox& communication,
    Game::Simulation::ServerWorld& world,
    Game::Replication::ServerReplicationCoordinator& replication)
    : mSessions(sessions), mAdministration(administration),
      mPrivateChat(privateChat), mCommunication(communication), mWorld(world),
      mReplication(replication) {
}

void ServerCommunicationService::SetDelivery(
    ServerCommunicationDelivery delivery) {
    mDelivery = std::move(delivery);
}

std::string ServerCommunicationService::PlayerName(int32_t player) const {
    if (player == 0) return "system";
    const NetworkIdentity* identity = mSessions.IdentityFor(player);
    if (identity && !identity->name.empty()) return identity->name;
    return "player " + std::to_string(player);
}

bool ServerCommunicationService::Send(
    int32_t peer, NetAppMessageType type, const NetworkMessageRaw& raw,
    NetMsgFlags flags) const {
    return peer > 0 && mSessions.HasIdentity(peer) && mDelivery.send &&
           mDelivery.send(peer, type, raw, flags);
}

void ServerCommunicationService::Broadcast(
    NetAppMessageType type, const NetworkMessageRaw& raw) const {
    if (mDelivery.broadcast) mDelivery.broadcast(type, raw);
}

bool ServerCommunicationService::SendHostChat(const std::string& message) {
    const std::string text = SanitiseChatText(message);
    if (text.empty()) return false;
    if (text.front() == '/') {
        RunCommand(0, text);
        return true;
    }
    const std::string line = "system: " + text;
    NetworkMessageRaw raw;
    raw.putString(line, CHAT_MAX_LINE_CHARS);
    Broadcast(NAMTChat, raw);
    mCommunication.QueueChat(line, CLKSystem);
    return true;
}

bool ServerCommunicationService::SendHostPrivateChat(
    int32_t targetPlayer, const std::string& message) {
    const std::string text = SanitiseChatText(message);
    std::string cipher;
    if (targetPlayer <= 0 || text.empty() ||
        !mSessions.HasIdentity(targetPlayer) ||
        !mPrivateChat.EncryptFor(targetPlayer, text, cipher)) {
        return false;
    }
    NetworkMessageRaw raw;
    raw.putInt32(0);
    raw.putString("system", 48);
    raw.putString(cipher, 255);
    if (!Send(targetPlayer, NAMTPrivateChat, raw, kReliable)) return false;
    mCommunication.QueueChat(">" + PlayerName(targetPlayer) + ": " + text,
                             CLKPrivate);
    return true;
}

void ServerCommunicationService::HandleChat(
    int32_t sender, const std::string& message) {
    if (!mSessions.HasIdentity(sender)) return;
    const std::string text = SanitiseChatText(message);
    if (text.empty()) return;
    if (text.front() == '/') {
        RunCommand(sender, text);
        return;
    }
    const std::string line = PlayerName(sender) + ": " + text;
    NetworkMessageRaw raw;
    raw.putString(line, CHAT_MAX_LINE_CHARS);
    Broadcast(NAMTChat, raw);
    mCommunication.QueueChat(line);
}

void ServerCommunicationService::HandleChatKey(
    int32_t sender, const DecodedChatKey& key) {
    if (!mSessions.HasIdentity(sender) || key.playerId != 0 ||
        key.playerName != PlayerName(sender)) {
        return;
    }
    const std::string name = PlayerName(sender);
    if (mPrivateChat.SetPeer(sender, name, key.publicKey)) {
        BroadcastChatKey(sender, name, key.publicKey);
    }
}

void ServerCommunicationService::HandlePrivateChat(
    int32_t sender, const DecodedPrivateChatToServer& message) {
    if (!mSessions.HasIdentity(sender)) return;
    if (message.targetPlayerId == 0) {
        std::string text;
        if (mPrivateChat.Decrypt(message.cipherText, text)) {
            mCommunication.QueueChat(
                "(private) " + PlayerName(sender) + ": " + text,
                CLKPrivate);
        }
        return;
    }
    if (!mSessions.HasIdentity(message.targetPlayerId)) return;
    NetworkMessageRaw raw;
    raw.putInt32(sender);
    raw.putString(PlayerName(sender), 48);
    raw.putString(message.cipherText, 255);
    Send(message.targetPlayerId, NAMTPrivateChat, raw, kReliable);
}

void ServerCommunicationService::HandleVoice(
    int32_t sender, NetworkVoiceIntentPacket packet) {
    if (!mSessions.HasIdentity(sender) || !mWorld.PlayerFor(sender) ||
        !CommunicationInbox::IsSaneVoice(packet) ||
        !mCommunication.AdmitVoiceIntent(sender, packet.sequence)) {
        return;
    }
    NetworkVoicePacket state{};
    state.playerId = sender;
    state.sequence = packet.sequence;
    state.codec = packet.codec;
    state.sampleRate = packet.sampleRate;
    state.frameSamples = packet.frameSamples;
    state.data = std::move(packet.data);
    const std::string payload = BuildVoicePayload(state);
    bool hostVisible = false;
    for (const int32_t observer : mReplication.PlayerObservers(sender)) {
        if (observer == 0) {
            hostVisible = true;
        } else if (observer > 0 && mDelivery.sendEncryptedPayload) {
            mDelivery.sendEncryptedPayload(observer, payload);
        }
    }
    if (hostVisible) mCommunication.QueueVoice(std::move(state));
}

void ServerCommunicationService::SendChatKey(
    int32_t peer, int32_t owner, const std::string& name,
    const std::string& publicKey) const {
    NetworkMessageRaw raw;
    raw.putInt32(owner);
    raw.putString(name, 48);
    raw.putString(publicKey, crypto_box_PUBLICKEYBYTES);
    Send(peer, NAMTChatKey, raw, kReliable);
}

void ServerCommunicationService::SendKnownChatKeys(int32_t peer) {
    if (!mSessions.HasIdentity(peer)) return;
    for (const PrivateChatPeer& known : mPrivateChat.Peers()) {
        SendChatKey(peer, known.playerId, known.playerName, known.publicKey);
    }
}

void ServerCommunicationService::BroadcastChatKey(
    int32_t owner, const std::string& name,
    const std::string& publicKey) const {
    for (const int32_t peer : mSessions.AdmittedPeers()) {
        SendChatKey(peer, owner, name, publicKey);
    }
}

void ServerCommunicationService::SendCommandResult(
    int32_t player, const std::string& message) {
    const std::string line = "system: " + message;
    if (player == 0) {
        mCommunication.QueueChat(line, CLKSystem);
        return;
    }
    NetworkMessageRaw raw;
    raw.putString(line, CHAT_MAX_LINE_CHARS);
    Send(player, NAMTChat, raw, kReliable);
}

void ServerCommunicationService::BroadcastSystem(const std::string& message) {
    const std::string line = "system: " + message;
    NetworkMessageRaw raw;
    raw.putString(line, CHAT_MAX_LINE_CHARS);
    Broadcast(NAMTChat, raw);
    mCommunication.QueueChat(line, CLKSystem);
}

void ServerCommunicationService::RunCommand(
    int32_t player, const std::string& command) {
    ServerAdministrationContext context;
    context.players = mDelivery.players;
    context.setTeam = [this](int32_t target, Game::Simulation::TeamId team) {
        return mWorld.SetPlayerTeam(target, team);
    };
    context.sendResult = [this](int32_t target, const std::string& message) {
        SendCommandResult(target, message);
    };
    context.broadcastSystem = [this](const std::string& message) {
        BroadcastSystem(message);
    };
    context.disconnectPlayer = [this](int32_t target, bool ban,
                                      const std::string& reason) {
        mSessions.SetModerationReason(target, reason);
        if (mDelivery.disconnectPlayer) {
            mDelivery.disconnectPlayer(target, ban, reason);
        }
    };
    mAdministration.Execute(player, command, context);
}

} // namespace Game::Multiplayer
