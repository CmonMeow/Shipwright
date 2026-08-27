#include "ShipwrightNetworkRuntime.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <sstream>

namespace SoH::Network {

namespace {

const NetMsgFlags kReliable = static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);
std::atomic_bool gCancelConnection = false;

bool CancelPendingConnection() {
    return gCancelConnection.load(std::memory_order_relaxed);
}

bool ReadRaw(const char* message, __int32 size, NetAppMessageType type, NetworkMessageRaw& raw) {
    if (!message || size < static_cast<__int32>(sizeof(NetAppMessageHeader))) {
        return false;
    }
    const auto* header = reinterpret_cast<const NetAppMessageHeader*>(message);
    if (header->type != type || !ValidAppMessageType(header->type)) {
        return false;
    }
    raw = NetworkMessageRaw(message + sizeof(NetAppMessageHeader), size - static_cast<__int32>(sizeof(NetAppMessageHeader)));
    return true;
}

} // namespace

ShipwrightNetworkRuntime::ShipwrightNetworkRuntime() {
    std::memset(mPrivateChatPublicKey, 0, sizeof(mPrivateChatPublicKey));
    std::memset(mPrivateChatSecretKey, 0, sizeof(mPrivateChatSecretKey));
    if (sodium_init() < 0) {
        Error("Network runtime: libsodium initialization failed");
        return;
    }
    EnsurePrivateChatKey();
}

ShipwrightNetworkRuntime::~ShipwrightNetworkRuntime() {
    Disconnect();
    sodium_memzero(mPrivateChatSecretKey, sizeof(mPrivateChatSecretKey));
}

bool ShipwrightNetworkRuntime::Host(uint16_t port, const std::string& sessionName) {
    if (IsActive() || port == 0 || port > 49151) {
        return false;
    }
    SetNetworkPort(port);
    mServer.reset(CreateNetServer());
    if (!mServer || !mServer->Init(sessionName, "", port)) {
        mServer.reset();
        stopUdpListenSend();
        destroyPool();
        return false;
    }
    QueueChat("system: hosting on port " + std::to_string(mServer->GetSessionPort()), CLKSystem);
    return true;
}

bool ShipwrightNetworkRuntime::Connect(const std::string& address) {
    if (IsActive() || address.empty()) {
        return false;
    }
    gCancelConnection.store(false, std::memory_order_relaxed);
    mConnectFuture = std::async(std::launch::async, [address]() {
        ConnectAttempt attempt;
        unsigned short port = DEFAULT_NETWORK_PORT;
        attempt.client.reset(CreateNetClient());
        attempt.result = attempt.client
                             ? attempt.client->Init(address, "", false, port, LocalUserName(), CancelPendingConnection)
                             : CRError;
        if (attempt.result != CROK) {
            attempt.client.reset();
            stopUdpListenSend();
            destroyPool();
        }
        return attempt;
    });
    QueueChat("system: connecting to " + address, CLKSystem);
    return true;
}

void ShipwrightNetworkRuntime::Disconnect() {
    if (mConnectFuture.valid()) {
        gCancelConnection.store(true, std::memory_order_relaxed);
        ConnectAttempt attempt = mConnectFuture.get();
        attempt.client.reset();
        gCancelConnection.store(false, std::memory_order_relaxed);
        stopUdpListenSend();
        destroyPool();
    }
    if (mClient || mServer) {
        sendDisconnectMessages();
        stopUdpListenSend();
    }
    mClient.reset();
    mServer.reset();
    destroyPool();

    mClientCrypto.clear();
    for (auto& [peer, crypto] : mServerCrypto) {
        (void)peer;
        crypto.clear();
    }
    mServerCrypto.clear();
    mPeers.clear();
    mIdentities.clear();
    mPrivateChatKeys.clear();
    mPrivateChatNames.clear();
    mClientIdentitySent = false;
    mLocalPlayerId = -1;
    mLatencyMilliseconds = 0;
    mThroughputBytesPerSecond = 0;
    mInboundBytesPerSecond = 0;
    mOutboundBytesPerSecond = 0;
    mInboundBytesSinceSample = 0;
    mOutboundBytesSinceSample = 0;
    mRateSampleTime = std::chrono::steady_clock::now();
    mPlayerStates.clear();
    mPlayerRemovals.clear();
    mDynamicObjectStates.clear();
    mProjectileStates.clear();
    mPlayerDamage.clear();
    mPersistentDynamicObjectStates.clear();
    mAuthoritativePlayerStates.clear();
    mLastPlayerStateUpdate.clear();
    mServerProjectiles.clear();
    mLastProjectileFire.clear();
    mMeleeWasActive.clear();
    mLastMeleeAttack.clear();
    mNextServerProjectileId = 1;
    mVoice.clear();
    if (mPrivateChatKeyReady) {
        mPrivateChatKeys[0] = PrivateChatPublicKey();
        mPrivateChatNames[0] = "system";
    }
}

void ShipwrightNetworkRuntime::Update() {
    if (mConnectFuture.valid() &&
        mConnectFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        ConnectAttempt attempt = mConnectFuture.get();
        if (attempt.result == CROK && attempt.client) {
            mClient = std::move(attempt.client);
            BeginClientCryptoHandshake();
            QueueChat("system: transport connected; establishing encrypted session", CLKSystem);
        } else {
            QueueChat("system: connection failed", CLKSystem);
        }
    }
    if (mServer) {
        mServer->ProcessPlayers(OnPeerCreated, OnPeerDeleted, this);
        mServer->ProcessUserMessages(OnServerMessage, this);
        UpdateServerProjectiles();
        int64_t totalLatency = 0;
        int64_t totalThroughput = 0;
        int32_t connectedPeers = 0;
        for (const int32_t peer : mPeers) {
            int32_t latency = 0;
            int32_t throughput = 0;
            if (mServer->GetConnectionInfo(peer, latency, throughput)) {
                totalLatency += latency;
                totalThroughput += throughput;
                ++connectedPeers;
            }
        }
        mLatencyMilliseconds = connectedPeers > 0 ? static_cast<int32_t>(totalLatency / connectedPeers) : 0;
        mThroughputBytesPerSecond =
            connectedPeers > 0 ? static_cast<int32_t>(totalThroughput / connectedPeers) : 0;
    }
    if (mClient) {
        mClient->ProcessUserMessages(OnClientMessage, this);
        mClient->GetConnectionInfo(mLatencyMilliseconds, mThroughputBytesPerSecond);
        if (mClient->IsSessionTerminated()) {
            const std::string reason = mClient->GetWhySessionTerminatedStr();
            QueueChat(reason.empty() ? "system: disconnected" : "system: " + reason, CLKSystem);
            Disconnect();
        }
    }

    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - mRateSampleTime).count();
    if (elapsed >= 1.0) {
        mInboundBytesPerSecond = static_cast<int32_t>(mInboundBytesSinceSample / elapsed);
        mOutboundBytesPerSecond = static_cast<int32_t>(mOutboundBytesSinceSample / elapsed);
        mInboundBytesSinceSample = 0;
        mOutboundBytesSinceSample = 0;
        mRateSampleTime = now;
    }
}

bool ShipwrightNetworkRuntime::IsHost() const {
    return mServer != nullptr;
}

bool ShipwrightNetworkRuntime::IsClient() const {
    return mClient != nullptr || mConnectFuture.valid();
}

bool ShipwrightNetworkRuntime::IsActive() const {
    return IsHost() || IsClient();
}

bool ShipwrightNetworkRuntime::IsSecure() const {
    if (mClient) {
        return mClientCrypto.ready() && mClientIdentitySent;
    }
    if (mServer) {
        for (const int32_t peer : mPeers) {
            const auto crypto = mServerCrypto.find(peer);
            if (crypto == mServerCrypto.end() || !crypto->second.ready() || mIdentities.find(peer) == mIdentities.end()) {
                return false;
            }
        }
        return true;
    }
    return false;
}

int32_t ShipwrightNetworkRuntime::LocalPlayerId() const {
    return mServer ? 0 : mLocalPlayerId;
}

int32_t ShipwrightNetworkRuntime::LatencyMilliseconds() const {
    return mLatencyMilliseconds;
}

int32_t ShipwrightNetworkRuntime::ThroughputBytesPerSecond() const {
    return mThroughputBytesPerSecond;
}

int32_t ShipwrightNetworkRuntime::InboundBytesPerSecond() const {
    return mInboundBytesPerSecond;
}

int32_t ShipwrightNetworkRuntime::OutboundBytesPerSecond() const {
    return mOutboundBytesPerSecond;
}

std::vector<NetworkPlayerInfo> ShipwrightNetworkRuntime::Players() const {
    std::vector<NetworkPlayerInfo> result;
    if (mServer) {
        result.push_back({ 0, LocalIdentityId(), LocalUserName(), false });
    }
    for (const auto& [player, identity] : mIdentities) {
        result.push_back({ player, identity.id, identity.name, identity.voiceClient });
    }
    return result;
}

bool ShipwrightNetworkRuntime::SendChat(const std::string& message) {
    const std::string text = SanitiseChatText(message);
    if (text.empty()) {
        return false;
    }
    if (mClient) {
        NetworkMessageRaw raw;
        raw.putString(text, CHAT_MAX_MESSAGE_CHARS);
        return SendToServer(NAMTChat, raw, kReliable);
    }
    if (mServer) {
        const std::string line = "system: " + text;
        NetworkMessageRaw raw;
        raw.putString(line, CHAT_MAX_LINE_CHARS);
        Broadcast(NAMTChat, raw);
        QueueChat(line, CLKSystem);
        return true;
    }
    return false;
}

bool ShipwrightNetworkRuntime::SendPrivateChat(int32_t targetPlayer, const std::string& message) {
    const std::string text = SanitiseChatText(message);
    std::string cipher;
    if (text.empty() || !EncryptPrivateText(targetPlayer, text, cipher)) {
        return false;
    }

    if (mClient) {
        NetworkMessageRaw raw;
        raw.putInt32(targetPlayer);
        raw.putString(cipher, 255);
        if (!SendToServer(NAMTPrivateChat, raw, kReliable)) {
            return false;
        }
    } else if (mServer && targetPlayer != 0) {
        NetworkMessageRaw raw;
        raw.putInt32(0);
        raw.putString("system", 48);
        raw.putString(cipher, 255);
        if (!SendToPeer(targetPlayer, NAMTPrivateChat, raw, kReliable)) {
            return false;
        }
    } else {
        return false;
    }

    QueueChat(">" + PlayerName(targetPlayer) + ": " + text, CLKPrivate);
    return true;
}

bool ShipwrightNetworkRuntime::SendPlayerState(NetworkPlayerStatePacket packet) {
    if (!SanePlayerState(packet)) {
        return false;
    }
    packet.playerId = LocalPlayerId();
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, packet);
    if (mClient) {
        return mLocalPlayerId >= 0 && SendToServer(NAMTPlayerState, raw, NMFHighPriority);
    }
    if (mServer) {
        packet.playerId = 0;
        mAuthoritativePlayerStates[0] = packet;
        EvaluateMeleeAttack(0, packet);
        Broadcast(NAMTPlayerState, raw, -1, NMFHighPriority);
        return true;
    }
    return false;
}

bool ShipwrightNetworkRuntime::SendDynamicObjectState(const NetworkDynamicObjectStatePacket& packet) {
    if (!SaneDynamicObjectState(packet)) {
        return false;
    }
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, packet);
    if (mClient) {
        return SendToServer(NAMTDynamicObjectState, raw, kReliable);
    }
    if (mServer) {
        const auto key = std::make_tuple(packet.sceneId, packet.roomId, packet.actorId, packet.actorParams,
                                         packet.homeX, packet.homeY, packet.homeZ);
        if (packet.destroyed <= 1) {
            mPersistentDynamicObjectStates[key] = packet;
        }
        Broadcast(NAMTDynamicObjectState, raw);
        mDynamicObjectStates.push_back(packet);
        return true;
    }
    return false;
}

bool ShipwrightNetworkRuntime::SendProjectileState(NetworkProjectileStatePacket packet) {
    if (!SaneProjectileState(packet)) {
        return false;
    }
    packet.playerId = LocalPlayerId();
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, packet);
    if (mClient) {
        return mLocalPlayerId >= 0 && SendToServer(NAMTDynamicObjectStateRaw, raw, NMFHighPriority);
    }
    if (mServer) {
        return AcceptServerProjectile(0, packet);
    }
    return false;
}

bool ShipwrightNetworkRuntime::SendVoice(NetworkVoicePacket packet) {
    if (!SaneVoice(packet)) {
        return false;
    }
    packet.playerId = LocalPlayerId();
    const std::string payload = BuildVoicePayload(packet);
    if (mClient) {
        return SendEncryptedPayloadToServer(payload, NMFHighPriority);
    }
    if (mServer) {
        bool sent = true;
        for (const int32_t peer : mPeers) {
            sent = SendEncryptedPayloadToPeer(peer, payload, NMFHighPriority) && sent;
        }
        return sent;
    }
    return false;
}

bool ShipwrightNetworkRuntime::PollChat(NetworkChatLine& line) {
    if (mChat.empty()) {
        return false;
    }
    line = std::move(mChat.front());
    mChat.pop_front();
    return true;
}

bool ShipwrightNetworkRuntime::PollPlayerState(NetworkPlayerStatePacket& packet) {
    if (mPlayerStates.empty()) {
        return false;
    }
    packet = mPlayerStates.front();
    mPlayerStates.pop_front();
    return true;
}

bool ShipwrightNetworkRuntime::PollPlayerRemove(NetworkPlayerRemovePacket& packet) {
    if (mPlayerRemovals.empty()) {
        return false;
    }
    packet = mPlayerRemovals.front();
    mPlayerRemovals.pop_front();
    return true;
}

bool ShipwrightNetworkRuntime::PollDynamicObjectState(NetworkDynamicObjectStatePacket& packet) {
    if (mDynamicObjectStates.empty()) {
        return false;
    }
    packet = mDynamicObjectStates.front();
    mDynamicObjectStates.pop_front();
    return true;
}

bool ShipwrightNetworkRuntime::PollProjectileState(NetworkProjectileStatePacket& packet) {
    if (mProjectileStates.empty()) {
        return false;
    }
    packet = mProjectileStates.front();
    mProjectileStates.pop_front();
    return true;
}

bool ShipwrightNetworkRuntime::PollPlayerDamage(NetworkPlayerDamagePacket& packet) {
    if (mPlayerDamage.empty()) {
        return false;
    }
    packet = mPlayerDamage.front();
    mPlayerDamage.pop_front();
    return true;
}

bool ShipwrightNetworkRuntime::PollVoice(NetworkVoicePacket& packet) {
    if (mVoice.empty()) {
        return false;
    }
    packet = std::move(mVoice.front());
    mVoice.pop_front();
    return true;
}

void ShipwrightNetworkRuntime::OnClientMessage(char* buffer, __int32 size, void* context) {
    static_cast<ShipwrightNetworkRuntime*>(context)->HandleClientMessage(buffer, size);
}

void ShipwrightNetworkRuntime::OnServerMessage(__int32 sender, char* buffer, __int32 size, void* context) {
    static_cast<ShipwrightNetworkRuntime*>(context)->HandleServerMessage(sender, buffer, size);
}

void ShipwrightNetworkRuntime::OnPeerCreated(__int32 peer, bool, const char*, unsigned long, void* context) {
    static_cast<ShipwrightNetworkRuntime*>(context)->HandlePeerCreated(peer);
}

void ShipwrightNetworkRuntime::OnPeerDeleted(__int32 peer, void* context) {
    static_cast<ShipwrightNetworkRuntime*>(context)->HandlePeerDeleted(peer);
}

void ShipwrightNetworkRuntime::HandleClientMessage(char* buffer, __int32 size) {
    if (size > 0) {
        mInboundBytesSinceSample += static_cast<uint64_t>(size);
    }
    std::string decrypted;
    const char* message = nullptr;
    __int32 messageSize = 0;
    if (!PrepareClientMessage(buffer, size, decrypted, message, messageSize)) {
        return;
    }

    std::string keyBytes;
    if (ParseAppRawBytes(message, messageSize, NAMTKeyAccept, keyBytes, crypto_kx_PUBLICKEYBYTES)) {
        if (mClientCrypto.acceptServerKey(keyBytes)) {
            Error("Network runtime: client accepted server encryption key");
            SendClientIdentity();
            SendPrivateChatKeyFromClient();
        }
        return;
    }

    NetworkPlayerAssignPacket assignment{};
    if (ParseAppPacket(message, messageSize, NAMTPlayerAssign, assignment)) {
        mLocalPlayerId = assignment.playerId;
        return;
    }

    std::string text;
    if (ParseAppRawString(message, messageSize, NAMTChat, text, CHAT_MAX_LINE_CHARS)) {
        text = SanitiseChatLine(text);
        QueueChat(text, text.rfind("system:", 0) == 0 ? CLKSystem : CLKNormal);
        return;
    }

    int32_t keyPlayer = -1;
    std::string keyName;
    std::string publicKey;
    if (DecodeChatKey(message, messageSize, keyPlayer, keyName, publicKey)) {
        mPrivateChatKeys[keyPlayer] = publicKey;
        mPrivateChatNames[keyPlayer] = keyName;
        return;
    }

    int32_t privateSender = -1;
    std::string privateName;
    std::string privateCipher;
    if (DecodePrivateForClient(message, messageSize, privateSender, privateName, privateCipher)) {
        std::string privateText;
        if (DecryptPrivateText(privateCipher, privateText)) {
            QueueChat("(private) " + privateName + ": " + privateText, CLKPrivate);
        }
        return;
    }

    NetworkPlayerStatePacket state{};
    if (ParseAppPacket(message, messageSize, NAMTPlayerState, state) && SanePlayerState(state) &&
        state.playerId != mLocalPlayerId) {
        mPlayerStates.push_back(state);
        return;
    }

    NetworkPlayerRemovePacket removal{};
    if (ParseAppPacket(message, messageSize, NAMTPlayerRemove, removal)) {
        mPlayerRemovals.push_back(removal);
        return;
    }

    NetworkDynamicObjectStatePacket objectState{};
    if (ParseAppPacket(message, messageSize, NAMTDynamicObjectState, objectState) &&
        SaneDynamicObjectState(objectState)) {
        mDynamicObjectStates.push_back(objectState);
        return;
    }

    NetworkProjectileStatePacket projectile{};
    if (ParseAppPacket(message, messageSize, NAMTDynamicObjectStateRaw, projectile) &&
        SaneProjectileState(projectile) && projectile.playerId != mLocalPlayerId) {
        mProjectileStates.push_back(projectile);
        return;
    }

    NetworkPlayerDamagePacket damage{};
    if (ParseAppPacket(message, messageSize, NAMTPlayerDamage, damage) && damage.targetPlayerId == mLocalPlayerId &&
        damage.damage > 0 && damage.damage <= 64) {
        mPlayerDamage.push_back(damage);
        return;
    }

    NetworkVoicePacket voice;
    if (ParseVoicePacket(message, messageSize, voice) && SaneVoice(voice) && voice.playerId != mLocalPlayerId) {
        mVoice.push_back(std::move(voice));
        while (mVoice.size() > 64) {
            mVoice.pop_front();
        }
        return;
    }

}

void ShipwrightNetworkRuntime::HandleServerMessage(int32_t sender, char* buffer, __int32 size) {
    if (size > 0) {
        mInboundBytesSinceSample += static_cast<uint64_t>(size);
    }
    std::string decrypted;
    const char* message = nullptr;
    __int32 messageSize = 0;
    if (!PrepareServerMessage(sender, buffer, size, decrypted, message, messageSize)) {
        return;
    }

    std::string keyBytes;
    if (ParseAppRawBytes(message, messageSize, NAMTKeyHello, keyBytes, crypto_kx_PUBLICKEYBYTES)) {
        std::string response;
        cCryptoSession& crypto = mServerCrypto[sender];
        if (crypto.acceptClientHello(keyBytes, response)) {
            NetworkMessageRaw raw;
            raw.put(response.data(), static_cast<__int32>(response.size()));
            SendPlainToPeer(sender, NAMTKeyAccept, raw);
            Error("Network runtime: server accepted encryption key from %d", sender);
        }
        return;
    }

    if (mIdentities.find(sender) == mIdentities.end()) {
        NetworkIdentity identity;
        if (!ParseIdentityRaw(message, messageSize, identity)) {
            mServer->KickOff(sender, NTRKicked, "invalid or incompatible identity");
            return;
        }
        mIdentities[sender] = identity;
        NetworkPlayerAssignPacket assignment{ sender };
        NetworkMessageRaw assignmentRaw;
        EncodeAppPacketRaw(assignmentRaw, assignment);
        SendToPeer(sender, NAMTPlayerAssign, assignmentRaw, kReliable);
        SendKnownChatKeysTo(sender);
        for (const auto& [key, objectState] : mPersistentDynamicObjectStates) {
            (void)key;
            NetworkMessageRaw objectRaw;
            EncodeAppPacketRaw(objectRaw, objectState);
            SendToPeer(sender, NAMTDynamicObjectState, objectRaw, kReliable);
        }

        const std::string line = "system: " + identity.name + " joined";
        NetworkMessageRaw chatRaw;
        chatRaw.putString(line, CHAT_MAX_LINE_CHARS);
        Broadcast(NAMTChat, chatRaw);
        QueueChat(line, CLKSystem);
        return;
    }

    if (ParseAppRawControl(message, messageSize, NAMTDisconnect)) {
        mServer->KickOff(sender, NTRDisconnected, "disconnected");
        return;
    }

    std::string text;
    if (ParseAppRawString(message, messageSize, NAMTChat, text, CHAT_MAX_MESSAGE_CHARS)) {
        text = SanitiseChatText(text);
        if (!text.empty()) {
            const std::string line = PlayerName(sender) + ": " + text;
            NetworkMessageRaw raw;
            raw.putString(line, CHAT_MAX_LINE_CHARS);
            Broadcast(NAMTChat, raw);
            QueueChat(line);
        }
        return;
    }

    int32_t keyOwner = -1;
    std::string keyName;
    std::string publicKey;
    if (DecodeChatKey(message, messageSize, keyOwner, keyName, publicKey)) {
        (void)keyOwner;
        mPrivateChatKeys[sender] = publicKey;
        mPrivateChatNames[sender] = PlayerName(sender);
        BroadcastChatKey(sender, PlayerName(sender), publicKey);
        return;
    }

    int32_t privateTarget = -1;
    std::string privateCipher;
    if (DecodePrivateForServer(message, messageSize, privateTarget, privateCipher)) {
        if (privateTarget == 0) {
            std::string privateText;
            if (DecryptPrivateText(privateCipher, privateText)) {
                QueueChat("(private) " + PlayerName(sender) + ": " + privateText, CLKPrivate);
            }
        } else if (std::find(mPeers.begin(), mPeers.end(), privateTarget) != mPeers.end()) {
            NetworkMessageRaw raw;
            raw.putInt32(sender);
            raw.putString(PlayerName(sender), 48);
            raw.putString(privateCipher, 255);
            SendToPeer(privateTarget, NAMTPrivateChat, raw, kReliable);
        }
        return;
    }

    NetworkPlayerStatePacket state{};
    if (ParseAppPacket(message, messageSize, NAMTPlayerState, state)) {
        state.playerId = sender;
        if (SanePlayerState(state)) {
            const auto now = std::chrono::steady_clock::now();
            const auto previous = mAuthoritativePlayerStates.find(sender);
            if (previous != mAuthoritativePlayerStates.end() && previous->second.sceneId == state.sceneId) {
                const auto previousTime = mLastPlayerStateUpdate.find(sender);
                const float elapsed = previousTime == mLastPlayerStateUpdate.end()
                                          ? 0.05f
                                          : std::min(0.25f, std::chrono::duration<float>(now - previousTime->second).count());
                const float dx = state.x - previous->second.x;
                const float dy = state.y - previous->second.y;
                const float dz = state.z - previous->second.z;
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                const float maximumDistance = 25.0f + 800.0f * elapsed;
                if (distance > maximumDistance && distance > 0.0f) {
                    const float scale = maximumDistance / distance;
                    state.x = previous->second.x + dx * scale;
                    state.y = previous->second.y + dy * scale;
                    state.z = previous->second.z + dz * scale;
                }
                state.sequence = previous->second.sequence + 1;
            }
            mAuthoritativePlayerStates[sender] = state;
            mLastPlayerStateUpdate[sender] = now;
            EvaluateMeleeAttack(sender, state);
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, state);
            Broadcast(NAMTPlayerState, raw, sender, NMFHighPriority);
            mPlayerStates.push_back(state);
        }
        return;
    }

    NetworkDynamicObjectStatePacket objectState{};
    if (ParseAppPacket(message, messageSize, NAMTDynamicObjectState, objectState) &&
        SaneDynamicObjectState(objectState) && PlayerIsNearObject(sender, objectState)) {
        const auto key = std::make_tuple(objectState.sceneId, objectState.roomId, objectState.actorId,
                                         objectState.actorParams, objectState.homeX, objectState.homeY,
                                         objectState.homeZ);
        if (objectState.destroyed <= 1) {
            mPersistentDynamicObjectStates[key] = objectState;
        }
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, objectState);
        Broadcast(NAMTDynamicObjectState, raw, sender, kReliable);
        mDynamicObjectStates.push_back(objectState);
        return;
    }

    NetworkProjectileStatePacket projectile{};
    if (ParseAppPacket(message, messageSize, NAMTDynamicObjectStateRaw, projectile)) {
        AcceptServerProjectile(sender, projectile);
        return;
    }

    NetworkVoicePacket voice;
    if (ParseVoicePacket(message, messageSize, voice)) {
        voice.playerId = sender;
        if (SaneVoice(voice)) {
            const std::string payload = BuildVoicePayload(voice);
            for (const int32_t peer : mPeers) {
                if (peer != sender) {
                    SendEncryptedPayloadToPeer(peer, payload, NMFHighPriority);
                }
            }
            mVoice.push_back(std::move(voice));
        }
        return;
    }

}

void ShipwrightNetworkRuntime::HandlePeerCreated(int32_t peer) {
    if (std::find(mPeers.begin(), mPeers.end(), peer) == mPeers.end()) {
        mPeers.push_back(peer);
    }
    mServerCrypto.try_emplace(peer);
}

void ShipwrightNetworkRuntime::HandlePeerDeleted(int32_t peer) {
    const std::string name = PlayerName(peer);
    mPeers.erase(std::remove(mPeers.begin(), mPeers.end(), peer), mPeers.end());
    mIdentities.erase(peer);
    mServerCrypto.erase(peer);
    mPrivateChatKeys.erase(peer);
    mPrivateChatNames.erase(peer);
    mAuthoritativePlayerStates.erase(peer);
    mLastPlayerStateUpdate.erase(peer);
    mLastProjectileFire.erase(peer);
    mMeleeWasActive.erase(peer);
    mLastMeleeAttack.erase(peer);
    for (auto it = mServerProjectiles.begin(); it != mServerProjectiles.end();) {
        if (it->first.first == peer) {
            it = mServerProjectiles.erase(it);
        } else {
            ++it;
        }
    }
    NetworkPlayerRemovePacket removal{ peer, -1 };
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, removal);
    Broadcast(NAMTPlayerRemove, raw, peer, kReliable);
    mPlayerRemovals.push_back(removal);
    QueueChat("system: " + name + " left", CLKSystem);
}

bool ShipwrightNetworkRuntime::PrepareClientMessage(char* buffer, __int32 size, std::string& decrypted,
                                                     const char*& message, __int32& messageSize) {
    message = buffer;
    messageSize = size;
    if (!buffer || size < static_cast<__int32>(sizeof(NetAppMessageHeader)) ||
        static_cast<size_t>(size) > NET_MAX_ENCRYPTED_BYTES) {
        return false;
    }
    const auto* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (!ValidAppMessageType(header->type)) {
        return false;
    }
    if (header->type == NAMTEncrypted) {
        if (!mClientCrypto.decrypt(buffer, size, decrypted)) {
            return false;
        }
        message = decrypted.data();
        messageSize = static_cast<__int32>(decrypted.size());
        return true;
    }
    return !mClientCrypto.ready() || header->type == NAMTKeyAccept;
}

bool ShipwrightNetworkRuntime::PrepareServerMessage(int32_t sender, char* buffer, __int32 size,
                                                     std::string& decrypted, const char*& message,
                                                     __int32& messageSize) {
    message = buffer;
    messageSize = size;
    if (!buffer || size < static_cast<__int32>(sizeof(NetAppMessageHeader)) ||
        static_cast<size_t>(size) > NET_MAX_ENCRYPTED_BYTES) {
        return false;
    }
    const auto* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (!ValidAppMessageType(header->type)) {
        return false;
    }
    if (header->type == NAMTEncrypted) {
        auto crypto = mServerCrypto.find(sender);
        if (crypto == mServerCrypto.end() || !crypto->second.decrypt(buffer, size, decrypted)) {
            return false;
        }
        message = decrypted.data();
        messageSize = static_cast<__int32>(decrypted.size());
        return true;
    }
    return header->type == NAMTKeyHello;
}

bool ShipwrightNetworkRuntime::SendPlainToClient(NetAppMessageType type, const NetworkMessageRaw& raw) {
    return mClient && SendPayloadToServer(BuildAppRawMessage(type, raw), kReliable);
}

bool ShipwrightNetworkRuntime::SendPlainToPeer(int32_t peer, NetAppMessageType type, const NetworkMessageRaw& raw) {
    return mServer && SendPayloadToPeer(peer, BuildAppRawMessage(type, raw), kReliable);
}

bool ShipwrightNetworkRuntime::SendToServer(NetAppMessageType type, const NetworkMessageRaw& raw, NetMsgFlags flags) {
    return SendEncryptedPayloadToServer(BuildAppRawMessage(type, raw), flags);
}

bool ShipwrightNetworkRuntime::SendToPeer(int32_t peer, NetAppMessageType type, const NetworkMessageRaw& raw,
                                          NetMsgFlags flags) {
    return SendEncryptedPayloadToPeer(peer, BuildAppRawMessage(type, raw), flags);
}

bool ShipwrightNetworkRuntime::SendEncryptedPayloadToServer(const std::string& payload, NetMsgFlags flags) {
    if (!mClient || !mClientCrypto.ready()) {
        return false;
    }
    std::string encrypted;
    return mClientCrypto.encrypt(payload, encrypted) && SendPayloadToServer(encrypted, flags);
}

bool ShipwrightNetworkRuntime::SendEncryptedPayloadToPeer(int32_t peer, const std::string& payload,
                                                           NetMsgFlags flags) {
    auto crypto = mServerCrypto.find(peer);
    if (!mServer || crypto == mServerCrypto.end() || !crypto->second.ready()) {
        return false;
    }
    std::string encrypted;
    return crypto->second.encrypt(payload, encrypted) && SendPayloadToPeer(peer, encrypted, flags);
}

bool ShipwrightNetworkRuntime::SendPayloadToServer(const std::string& payload, NetMsgFlags flags) {
    if (!mClient || payload.empty()) {
        return false;
    }
    DWORD messageId = 0;
    const bool sent = static_cast<bool>(mClient->SendMsg(reinterpret_cast<BYTE*>(const_cast<char*>(payload.data())),
                                                         static_cast<__int32>(payload.size()), messageId, flags,
                                                         nullptr));
    if (sent) {
        mOutboundBytesSinceSample += payload.size();
    }
    return sent;
}

bool ShipwrightNetworkRuntime::SendPayloadToPeer(int32_t peer, const std::string& payload, NetMsgFlags flags) {
    if (!mServer || payload.empty()) {
        return false;
    }
    DWORD messageId = 0;
    const bool sent = static_cast<bool>(mServer->SendMsg(peer, reinterpret_cast<BYTE*>(const_cast<char*>(payload.data())),
                                                         static_cast<__int32>(payload.size()), messageId, flags,
                                                         nullptr));
    if (sent) {
        mOutboundBytesSinceSample += payload.size();
    }
    return sent;
}

void ShipwrightNetworkRuntime::Broadcast(NetAppMessageType type, const NetworkMessageRaw& raw, int32_t exceptPlayer,
                                         NetMsgFlags flags) {
    for (const int32_t peer : mPeers) {
        if (peer != exceptPlayer && mIdentities.find(peer) != mIdentities.end()) {
            SendToPeer(peer, type, raw, flags);
        }
    }
}

void ShipwrightNetworkRuntime::BeginClientCryptoHandshake() {
    std::string hello;
    if (!mClientCrypto.buildClientHello(hello)) {
        return;
    }
    NetworkMessageRaw raw;
    raw.put(hello.data(), static_cast<__int32>(hello.size()));
    SendPlainToClient(NAMTKeyHello, raw);
}

void ShipwrightNetworkRuntime::SendClientIdentity() {
    NetworkMessageRaw raw;
    EncodeLocalIdentityRaw(raw);
    mClientIdentitySent = SendToServer(NAMTConnect, raw, kReliable);
}

void ShipwrightNetworkRuntime::SendPrivateChatKeyFromClient() {
    if (!mClient || !EnsurePrivateChatKey()) {
        return;
    }
    NetworkMessageRaw raw;
    raw.putInt32(0);
    raw.putString(LocalUserName(), 48);
    raw.putString(PrivateChatPublicKey(), crypto_box_PUBLICKEYBYTES);
    SendToServer(NAMTChatKey, raw, kReliable);
}

void ShipwrightNetworkRuntime::SendChatKeyTo(int32_t peer, int32_t owner, const std::string& name,
                                             const std::string& publicKey) {
    NetworkMessageRaw raw;
    raw.putInt32(owner);
    raw.putString(name, 48);
    raw.putString(publicKey, crypto_box_PUBLICKEYBYTES);
    SendToPeer(peer, NAMTChatKey, raw, kReliable);
}

void ShipwrightNetworkRuntime::SendKnownChatKeysTo(int32_t peer) {
    for (const auto& [owner, key] : mPrivateChatKeys) {
        const auto name = mPrivateChatNames.find(owner);
        SendChatKeyTo(peer, owner, name != mPrivateChatNames.end() ? name->second : PlayerName(owner), key);
    }
}

void ShipwrightNetworkRuntime::BroadcastChatKey(int32_t owner, const std::string& name,
                                                const std::string& publicKey) {
    for (const int32_t peer : mPeers) {
        SendChatKeyTo(peer, owner, name, publicKey);
    }
}

bool ShipwrightNetworkRuntime::EnsurePrivateChatKey() {
    if (mPrivateChatKeyReady) {
        return true;
    }
    if (crypto_box_keypair(mPrivateChatPublicKey, mPrivateChatSecretKey) != 0) {
        return false;
    }
    mPrivateChatKeyReady = true;
    mPrivateChatKeys[0] = PrivateChatPublicKey();
    mPrivateChatNames[0] = "system";
    return true;
}

std::string ShipwrightNetworkRuntime::PrivateChatPublicKey() const {
    return std::string(reinterpret_cast<const char*>(mPrivateChatPublicKey), crypto_box_PUBLICKEYBYTES);
}

bool ShipwrightNetworkRuntime::EncryptPrivateText(int32_t target, const std::string& message,
                                                  std::string& cipher) const {
    const auto key = mPrivateChatKeys.find(target);
    if (key == mPrivateChatKeys.end() || key->second.size() != crypto_box_PUBLICKEYBYTES) {
        return false;
    }
    cipher.resize(message.size() + crypto_box_SEALBYTES);
    if (crypto_box_seal(reinterpret_cast<unsigned char*>(cipher.data()),
                        reinterpret_cast<const unsigned char*>(message.data()), message.size(),
                        reinterpret_cast<const unsigned char*>(key->second.data())) != 0) {
        cipher.clear();
        return false;
    }
    return true;
}

bool ShipwrightNetworkRuntime::DecryptPrivateText(const std::string& cipher, std::string& message) const {
    message.clear();
    if (!mPrivateChatKeyReady || cipher.size() < crypto_box_SEALBYTES) {
        return false;
    }
    message.resize(cipher.size() - crypto_box_SEALBYTES);
    if (crypto_box_seal_open(reinterpret_cast<unsigned char*>(message.data()),
                             reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size(),
                             mPrivateChatPublicKey, mPrivateChatSecretKey) != 0) {
        message.clear();
        return false;
    }
    message = SanitiseChatText(message);
    return !message.empty();
}

std::string ShipwrightNetworkRuntime::PlayerName(int32_t player) const {
    if (player == 0) {
        return "system";
    }
    const auto identity = mIdentities.find(player);
    if (identity != mIdentities.end() && !identity->second.name.empty()) {
        return identity->second.name;
    }
    const auto privateName = mPrivateChatNames.find(player);
    if (privateName != mPrivateChatNames.end() && !privateName->second.empty()) {
        return privateName->second;
    }
    return "player " + std::to_string(player);
}

void ShipwrightNetworkRuntime::QueueChat(const std::string& text, ChatLineKind kind) {
    const std::string clean = SanitiseChatLine(text);
    if (clean.empty()) {
        return;
    }
    mChat.push_back({ clean, kind });
    while (mChat.size() > CHAT_MAX_HISTORY_LINES) {
        mChat.pop_front();
    }
}

bool ShipwrightNetworkRuntime::DecodeChatKey(const char* message, __int32 size, int32_t& player,
                                             std::string& name, std::string& publicKey) {
    NetworkMessageRaw raw;
    return ReadRaw(message, size, NAMTChatKey, raw) && raw.getInt32(player) && raw.getString(name, 48) &&
           raw.getString(publicKey, crypto_box_PUBLICKEYBYTES) && publicKey.size() == crypto_box_PUBLICKEYBYTES &&
           raw.fullyRead();
}

bool ShipwrightNetworkRuntime::DecodePrivateForServer(const char* message, __int32 size, int32_t& target,
                                                      std::string& cipher) {
    NetworkMessageRaw raw;
    return ReadRaw(message, size, NAMTPrivateChat, raw) && raw.getInt32(target) && raw.getString(cipher, 255) &&
           raw.fullyRead();
}

bool ShipwrightNetworkRuntime::DecodePrivateForClient(const char* message, __int32 size, int32_t& sender,
                                                      std::string& name, std::string& cipher) {
    NetworkMessageRaw raw;
    return ReadRaw(message, size, NAMTPrivateChat, raw) && raw.getInt32(sender) && raw.getString(name, 48) &&
           raw.getString(cipher, 255) && raw.fullyRead();
}

bool ShipwrightNetworkRuntime::SanePlayerState(const NetworkPlayerStatePacket& packet) {
    const auto saneFloat = [](float value) { return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f; };
    const bool saneFishing = packet.fishingState <= 5 && saneFloat(packet.fishingRodTipOffset[0]) &&
                             saneFloat(packet.fishingRodTipOffset[1]) && saneFloat(packet.fishingRodTipOffset[2]) &&
                             saneFloat(packet.fishingLureOffset[0]) && saneFloat(packet.fishingLureOffset[1]) &&
                             saneFloat(packet.fishingLureOffset[2]);
    for (unsigned char point = 0; point < NETWORK_FISHING_LINE_POINT_COUNT; ++point) {
        for (unsigned char axis = 0; axis < 3; ++axis) {
            if (!saneFloat(packet.fishingLineOffsets[point][axis])) {
                return false;
            }
        }
    }
    return packet.sceneId >= 0 && packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.roomId >= -1 && packet.roomId < 256 && saneFloat(packet.x) && saneFloat(packet.y) &&
           saneFloat(packet.z) && saneFloat(packet.speed) &&
           (packet.stateFlags & ~(NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_GROUNDED | NETWORK_PLAYER_SWIMMING)) == 0 &&
           packet.modelGroup < 16 && packet.itemAction < 0x43 && packet.meleeWeaponState >= 0 &&
           packet.meleeWeaponState <= 2 && saneFishing;
}

void ShipwrightNetworkRuntime::EvaluateMeleeAttack(int32_t player, const NetworkPlayerStatePacket& state) {
    if (!mServer) {
        return;
    }
    const bool active = state.meleeWeaponState > 0;
    const bool wasActive = mMeleeWasActive[player];
    mMeleeWasActive[player] = active;
    if (!active || wasActive) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto previousAttack = mLastMeleeAttack.find(player);
    if (previousAttack != mLastMeleeAttack.end() && now - previousAttack->second < std::chrono::milliseconds(250)) {
        return;
    }
    mLastMeleeAttack[player] = now;
    const float yaw = state.rotationY * (3.14159265358979323846f / 32768.0f);
    const float facingX = std::sin(yaw);
    const float facingZ = std::cos(yaw);
    for (const auto& [targetId, target] : mAuthoritativePlayerStates) {
        if (targetId == player || target.sceneId != state.sceneId) {
            continue;
        }
        const float dx = target.x - state.x;
        const float dy = target.y - state.y;
        const float dz = target.z - state.z;
        const float horizontalSquared = dx * dx + dz * dz;
        if (horizontalSquared > 120.0f * 120.0f || std::fabs(dy) > 80.0f ||
            facingX * dx + facingZ * dz < -10.0f) {
            continue;
        }
        const short impactYaw = static_cast<short>(std::atan2(dx, dz) *
                                                   (32768.0f / 3.14159265358979323846f));
        NetworkPlayerDamagePacket damage{ player, targetId, 16, impactYaw };
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, damage);
        if (targetId == 0) {
            mPlayerDamage.push_back(damage);
        } else {
            SendToPeer(targetId, NAMTPlayerDamage, raw, kReliable);
        }
    }
}

bool ShipwrightNetworkRuntime::SaneDynamicObjectState(const NetworkDynamicObjectStatePacket& packet) {
    constexpr int32_t coordinateLimit = 1000000;
    return packet.sceneId >= 0 && packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.roomId >= -1 && packet.roomId < 256 && packet.actorId >= 0 && packet.actorId < 0x1000 &&
           packet.actorParams >= INT16_MIN && packet.actorParams <= INT16_MAX && packet.homeX > -coordinateLimit &&
           packet.homeX < coordinateLimit && packet.homeY > -coordinateLimit && packet.homeY < coordinateLimit &&
           packet.homeZ > -coordinateLimit && packet.homeZ < coordinateLimit && packet.destroyed <= 2;
}

bool ShipwrightNetworkRuntime::SaneProjectileState(const NetworkProjectileStatePacket& packet) {
    const auto saneFloat = [](float value) { return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f; };
    return packet.projectileId > 0 && packet.projectileId < INT32_MAX && packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) && packet.active <= 1 &&
           packet.projectileKind <= NETWORK_PROJECTILE_BOMB && packet.phase <= NETWORK_BOMB_EXPLODING &&
           packet.projectileType <= 8 && saneFloat(packet.x) && saneFloat(packet.y) && saneFloat(packet.z) &&
           saneFloat(packet.velocityX) && saneFloat(packet.velocityY) && saneFloat(packet.velocityZ);
}

bool ShipwrightNetworkRuntime::PlayerIsNearObject(
    int32_t player, const NetworkDynamicObjectStatePacket& objectState) const {
    const auto found = mAuthoritativePlayerStates.find(player);
    if (found == mAuthoritativePlayerStates.end() || found->second.sceneId != objectState.sceneId) {
        return false;
    }
    const float dx = found->second.x - static_cast<float>(objectState.homeX);
    const float dy = found->second.y - static_cast<float>(objectState.homeY);
    const float dz = found->second.z - static_cast<float>(objectState.homeZ);
    // Permit canonical-pose interpolation lag while keeping the decision on
    // the server and bounded to the player's immediate area.
    return dx * dx + dy * dy + dz * dz <= 800.0f * 800.0f;
}

bool ShipwrightNetworkRuntime::AcceptServerProjectile(int32_t player,
                                                       const NetworkProjectileStatePacket& request) {
    if (!mServer || !SaneProjectileState(request)) {
        return false;
    }
    const auto key = std::make_pair(player, request.projectileId);
    auto existing = mServerProjectiles.find(key);
    if (!request.active) {
        if (existing == mServerProjectiles.end()) {
            return false;
        }
        // Bomb lifetime and detonation are owned by the server. A client may
        // retire its own arrow after collision, but cannot silently erase a bomb.
        if (existing->second.state.projectileKind == NETWORK_PROJECTILE_BOMB) {
            return true;
        }
        existing->second.state.active = 0;
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, existing->second.state);
        Broadcast(NAMTDynamicObjectStateRaw, raw, player, NMFHighPriority);
        mProjectileStates.push_back(existing->second.state);
        mServerProjectiles.erase(existing);
        return true;
    }
    if (existing != mServerProjectiles.end()) {
        if (existing->second.state.projectileKind == NETWORK_PROJECTILE_BOMB &&
            existing->second.state.phase == NETWORK_BOMB_HELD && request.phase == NETWORK_BOMB_RELEASED) {
            existing->second.state.phase = NETWORK_BOMB_RELEASED;
            constexpr float maximumBombSpeed = 500.0f;
            const float speed = std::sqrt(request.velocityX * request.velocityX + request.velocityY * request.velocityY +
                                          request.velocityZ * request.velocityZ);
            const float scale = speed > maximumBombSpeed ? maximumBombSpeed / speed : 1.0f;
            existing->second.velocityX = request.velocityX * scale;
            existing->second.velocityY = request.velocityY * scale;
            existing->second.velocityZ = request.velocityZ * scale;
        }
        return true;
    }

    const auto shooter = mAuthoritativePlayerStates.find(player);
    if (shooter == mAuthoritativePlayerStates.end() || shooter->second.sceneId != request.sceneId) {
        return false;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto lastFire = mLastProjectileFire.find(player);
    if (lastFire != mLastProjectileFire.end() &&
        now - lastFire->second < std::chrono::milliseconds(150)) {
        return false;
    }
    mLastProjectileFire[player] = now;

    ServerProjectile projectile;
    projectile.state = request;
    projectile.state.playerId = player;
    projectile.state.projectileId = mNextServerProjectileId++;
    projectile.state.x = shooter->second.x;
    projectile.state.y = shooter->second.y + 42.0f;
    projectile.state.z = shooter->second.z;
    projectile.groundY = shooter->second.y;
    projectile.spawned = projectile.lastUpdate = projectile.lastBroadcast = now;
    if (request.projectileKind == NETWORK_PROJECTILE_BOMB) {
        projectile.state.phase = NETWORK_BOMB_HELD;
        projectile.state.y = shooter->second.y + 38.0f;
        projectile.velocityX = projectile.velocityY = projectile.velocityZ = 0.0f;
    } else {
    constexpr float arrowSpeedPerSecond = 3000.0f;
    const float yaw = shooter->second.aimYaw * (3.14159265358979323846f / 32768.0f);
    const float pitch = shooter->second.aimPitch * (3.14159265358979323846f / 32768.0f);
    const float horizontal = std::cos(pitch) * arrowSpeedPerSecond;
    projectile.velocityX = std::sin(yaw) * horizontal;
    projectile.velocityY = -std::sin(pitch) * arrowSpeedPerSecond;
    projectile.velocityZ = std::cos(yaw) * horizontal;
    projectile.state.rotationX = static_cast<short>(
        std::atan2(horizontal, -projectile.velocityY) * (32768.0f / 3.14159265358979323846f));
    projectile.state.rotationY = shooter->second.aimYaw;
    projectile.state.rotationZ = request.rotationZ;
    }
    mServerProjectiles.emplace(key, projectile);

    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, projectile.state);
    Broadcast(NAMTDynamicObjectStateRaw, raw, player, NMFHighPriority);
    mProjectileStates.push_back(projectile.state);
    return true;
}

void ShipwrightNetworkRuntime::UpdateServerProjectiles() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = mServerProjectiles.begin(); it != mServerProjectiles.end();) {
        ServerProjectile& projectile = it->second;
        const float deltaSeconds =
            std::min(0.1f, std::chrono::duration<float>(now - projectile.lastUpdate).count());
        projectile.lastUpdate = now;
        if (projectile.state.projectileKind == NETWORK_PROJECTILE_BOMB) {
            const auto owner = mAuthoritativePlayerStates.find(projectile.state.playerId);
            if (projectile.state.phase == NETWORK_BOMB_HELD && owner != mAuthoritativePlayerStates.end()) {
                projectile.state.x = owner->second.x;
                projectile.state.y = owner->second.y + 38.0f;
                projectile.state.z = owner->second.z;
                projectile.groundY = owner->second.y;
            } else if (projectile.state.phase == NETWORK_BOMB_RELEASED) {
                projectile.velocityY -= 480.0f * deltaSeconds;
                projectile.state.x += projectile.velocityX * deltaSeconds;
                projectile.state.y += projectile.velocityY * deltaSeconds;
                projectile.state.z += projectile.velocityZ * deltaSeconds;
                if (projectile.state.y < projectile.groundY) {
                    projectile.state.y = projectile.groundY;
                    projectile.velocityY = std::fabs(projectile.velocityY) > 70.0f ? -projectile.velocityY * 0.3f : 0.0f;
                    projectile.velocityX *= 0.7f;
                    projectile.velocityZ *= 0.7f;
                }
            }
            if (projectile.state.phase != NETWORK_BOMB_EXPLODING &&
                now - projectile.spawned >= std::chrono::milliseconds(3500)) {
                projectile.state.phase = NETWORK_BOMB_EXPLODING;
                projectile.explodingSince = now;
            }
            if (projectile.state.phase == NETWORK_BOMB_EXPLODING &&
                now - projectile.explodingSince >= std::chrono::milliseconds(500)) {
                projectile.state.active = 0;
                NetworkMessageRaw raw;
                EncodeAppPacketRaw(raw, projectile.state);
                Broadcast(NAMTDynamicObjectStateRaw, raw, projectile.state.playerId, NMFHighPriority);
                mProjectileStates.push_back(projectile.state);
                it = mServerProjectiles.erase(it);
                continue;
            }
        } else {
            projectile.state.x += projectile.velocityX * deltaSeconds;
            projectile.state.y += projectile.velocityY * deltaSeconds;
            projectile.state.z += projectile.velocityZ * deltaSeconds;
        }
        if (now - projectile.lastBroadcast < std::chrono::milliseconds(50)) {
            ++it;
            continue;
        }
        projectile.lastBroadcast = now;
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, projectile.state);
        Broadcast(NAMTDynamicObjectStateRaw, raw, projectile.state.playerId, NMFHighPriority);
        mProjectileStates.push_back(projectile.state);
        ++it;
    }
}

bool ShipwrightNetworkRuntime::SaneVoice(const NetworkVoicePacket& packet) {
    return (packet.codec == VOICE_CODEC_OPUS || packet.codec == VOICE_CODEC_ADPCM) &&
           packet.sampleRate == VOICE_SAMPLE_RATE && packet.frameSamples == VOICE_SAMPLES_PER_PACKET &&
           !packet.data.empty() && packet.data.size() <= VOICE_MAX_OPUS_BYTES;
}

} // namespace SoH::Network
