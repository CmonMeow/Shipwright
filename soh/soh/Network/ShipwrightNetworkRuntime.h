#pragma once

#include "NetworkProtocol.h"
#include "ServerCollisionWorld.h"

#include <cstdint>
#include <chrono>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace SoH::Network {

struct NetworkPlayerInfo {
    int32_t playerId = -1;
    std::string identity;
    std::string name;
    bool voiceClient = false;
};

class ShipwrightNetworkRuntime final {
  public:
    ShipwrightNetworkRuntime();
    ~ShipwrightNetworkRuntime();

    ShipwrightNetworkRuntime(const ShipwrightNetworkRuntime&) = delete;
    ShipwrightNetworkRuntime& operator=(const ShipwrightNetworkRuntime&) = delete;

    bool Host(uint16_t port = DEFAULT_NETWORK_PORT, const std::string& sessionName = "Ship of Harkinian");
    bool Connect(const std::string& address = DEFAULT_NETWORK_ADDRESS);
    void Disconnect();
    void Update();

    bool IsHost() const;
    bool IsClient() const;
    bool IsActive() const;
    bool IsSecure() const;
    int32_t LocalPlayerId() const;
    int32_t LatencyMilliseconds() const;
    int32_t ThroughputBytesPerSecond() const;
    int32_t InboundBytesPerSecond() const;
    int32_t OutboundBytesPerSecond() const;

    std::vector<NetworkPlayerInfo> Players() const;
    bool SendChat(const std::string& message);
    bool SendPrivateChat(int32_t targetPlayer, const std::string& message);
    bool SendPlayerState(NetworkPlayerStatePacket packet);
    bool SendActorEvent(NetworkActorEventPacket packet);
    bool SendProjectileState(NetworkProjectileStatePacket packet, bool reliableTransition = false);
    bool SendProjectileImpact(NetworkProjectileImpactPacket packet);
    bool SendVoice(NetworkVoicePacket packet);

    bool PollChat(NetworkChatLine& line);
    bool PollPlayerState(NetworkPlayerStatePacket& packet);
    bool PollPlayerRemove(NetworkPlayerRemovePacket& packet);
    bool PollDynamicObjectState(NetworkDynamicObjectStatePacket& packet);
    bool PollActorEvent(NetworkActorEventPacket& packet);
    bool PollProjectileState(NetworkProjectileStatePacket& packet);
    bool PollPlayerDamage(NetworkPlayerDamagePacket& packet);
    bool PollPlayerRespawn(NetworkPlayerRespawnPacket& packet);
    bool PollVoice(NetworkVoicePacket& packet);

  private:
    struct ConnectAttempt {
        std::unique_ptr<NetTranspClient> client;
        ConnectResult result = CRError;
    };

    static void OnClientMessage(char* buffer, __int32 size, void* context);
    static void OnServerMessage(__int32 sender, char* buffer, __int32 size, void* context);
    static void OnPeerCreated(__int32 peer, bool botClient, const char* name, unsigned long address, void* context);
    static void OnPeerDeleted(__int32 peer, void* context);

    void HandleClientMessage(char* buffer, __int32 size);
    void HandleServerMessage(int32_t sender, char* buffer, __int32 size);
    void HandlePeerCreated(int32_t peer);
    void HandlePeerDeleted(int32_t peer);

    bool PrepareClientMessage(char* buffer, __int32 size, std::string& decrypted, const char*& message,
                              __int32& messageSize);
    bool PrepareServerMessage(int32_t sender, char* buffer, __int32 size, std::string& decrypted,
                              const char*& message, __int32& messageSize);

    bool SendPlainToClient(NetAppMessageType type, const NetworkMessageRaw& raw);
    bool SendPlainToPeer(int32_t peer, NetAppMessageType type, const NetworkMessageRaw& raw);
    bool SendToServer(NetAppMessageType type, const NetworkMessageRaw& raw, NetMsgFlags flags);
    bool SendToPeer(int32_t peer, NetAppMessageType type, const NetworkMessageRaw& raw, NetMsgFlags flags);
    bool SendEncryptedPayloadToServer(const std::string& payload, NetMsgFlags flags);
    bool SendEncryptedPayloadToPeer(int32_t peer, const std::string& payload, NetMsgFlags flags);
    bool SendPayloadToServer(const std::string& payload, NetMsgFlags flags);
    bool SendPayloadToPeer(int32_t peer, const std::string& payload, NetMsgFlags flags);
    void Broadcast(NetAppMessageType type, const NetworkMessageRaw& raw, int32_t exceptPlayer = -1,
                   NetMsgFlags flags = static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority));

    void BeginClientCryptoHandshake();
    void SendClientIdentity();
    void SendPrivateChatKeyFromClient();
    void SendChatKeyTo(int32_t peer, int32_t owner, const std::string& name, const std::string& publicKey);
    void SendKnownChatKeysTo(int32_t peer);
    void BroadcastChatKey(int32_t owner, const std::string& name, const std::string& publicKey);
    bool EnsurePrivateChatKey();
    std::string PrivateChatPublicKey() const;
    bool EncryptPrivateText(int32_t target, const std::string& message, std::string& cipher) const;
    bool DecryptPrivateText(const std::string& cipher, std::string& message) const;
    std::string PlayerName(int32_t player) const;
    void QueueChat(const std::string& text, ChatLineKind kind = CLKNormal);
    bool AcceptServerProjectile(int32_t player, const NetworkProjectileStatePacket& request);
    bool AcceptServerProjectileImpact(int32_t witness, const NetworkProjectileImpactPacket& impact);
    void RetainServerStuckArrow(const std::pair<int32_t, int32_t>& currentKey);
    void SanitizeServerFishingState(int32_t player, NetworkPlayerStatePacket& state,
                                    const NetworkPlayerStatePacket* previous, float elapsedSeconds);
    bool AcceptServerActorEvent(int32_t player, NetworkActorEventPacket packet);
    void ProcessPendingActorEvents();
    void UpdateServerDynamicObjects();
    void ReleaseFishOwnedBy(int32_t player);
    void UpdateServerProjectiles();
    void UpdateServerRespawns();
    void ProcessServerDeathTransition(int32_t player, const NetworkPlayerStatePacket& state);
    void CreateServerCorpse(const NetworkPlayerStatePacket& finalPose);
    void EvaluateMeleeAttack(int32_t player, const NetworkPlayerStatePacket& state);
    bool PlayerIsNearObject(int32_t player, const NetworkDynamicObjectStatePacket& objectState) const;

    static bool DecodeChatKey(const char* message, __int32 size, int32_t& player, std::string& name,
                              std::string& publicKey);
    static bool DecodePrivateForServer(const char* message, __int32 size, int32_t& target, std::string& cipher);
    static bool DecodePrivateForClient(const char* message, __int32 size, int32_t& sender, std::string& name,
                                       std::string& cipher);
    static bool SanePlayerState(const NetworkPlayerStatePacket& packet);
    static bool SaneDynamicObjectState(const NetworkDynamicObjectStatePacket& packet);
    static bool SaneActorEvent(const NetworkActorEventPacket& packet);
    static bool SaneProjectileState(const NetworkProjectileStatePacket& packet);
    static bool SaneProjectileImpact(const NetworkProjectileImpactPacket& packet);
    static bool SaneVoice(const NetworkVoicePacket& packet);

    std::unique_ptr<NetTranspServer> mServer;
    std::unique_ptr<NetTranspClient> mClient;
    std::future<ConnectAttempt> mConnectFuture;
    std::vector<int32_t> mPeers;
    std::map<int32_t, NetworkIdentity> mIdentities;
    std::map<int32_t, cCryptoSession> mServerCrypto;
    std::map<int32_t, std::string> mPrivateChatKeys;
    std::map<int32_t, std::string> mPrivateChatNames;
    cCryptoSession mClientCrypto;
    unsigned char mPrivateChatPublicKey[crypto_box_PUBLICKEYBYTES];
    unsigned char mPrivateChatSecretKey[crypto_box_SECRETKEYBYTES];
    bool mPrivateChatKeyReady = false;
    bool mClientIdentitySent = false;
    int32_t mLocalPlayerId = -1;
    int32_t mLatencyMilliseconds = 0;
    int32_t mThroughputBytesPerSecond = 0;
    int32_t mInboundBytesPerSecond = 0;
    int32_t mOutboundBytesPerSecond = 0;
    uint64_t mInboundBytesSinceSample = 0;
    uint64_t mOutboundBytesSinceSample = 0;
    std::chrono::steady_clock::time_point mRateSampleTime = std::chrono::steady_clock::now();
    std::deque<NetworkChatLine> mChat;
    std::deque<NetworkPlayerStatePacket> mPlayerStates;
    std::deque<NetworkPlayerRemovePacket> mPlayerRemovals;
    std::deque<NetworkDynamicObjectStatePacket> mDynamicObjectStates;
    std::deque<NetworkActorEventPacket> mActorEvents;
    std::deque<NetworkProjectileStatePacket> mProjectileStates;
    std::map<std::pair<int32_t, int32_t>, unsigned __int32> mLatestProjectileSequences;
    std::deque<NetworkPlayerDamagePacket> mPlayerDamage;
    std::deque<NetworkPlayerRespawnPacket> mPlayerRespawns;
    struct ServerProjectile {
        NetworkProjectileStatePacket state{};
        float velocityX = 0.0f;
        float velocityY = 0.0f;
        float velocityZ = 0.0f;
        float heldOffsetX = 0.0f;
        float heldOffsetY = 0.0f;
        float heldOffsetZ = 0.0f;
        float groundY = 0.0f;
        std::chrono::steady_clock::time_point spawned = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point explodingSince = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point impactedSince = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point lastBroadcast = std::chrono::steady_clock::now();
    };
    std::map<int32_t, NetworkPlayerStatePacket> mAuthoritativePlayerStates;
    std::map<int32_t, bool> mPlayerWasDead;
    std::map<int32_t, NetworkPlayerStatePacket> mLastDeadPlayerStates;
    std::map<int32_t, std::chrono::steady_clock::time_point> mRespawnDeadlines;
    std::map<int32_t, NetworkPlayerStatePacket> mServerCorpses;
    std::map<int32_t, std::deque<int32_t>> mSceneCorpses;
    std::map<int32_t, std::chrono::steady_clock::time_point> mLastPlayerStateUpdate;
    std::map<std::pair<int32_t, int32_t>, ServerProjectile> mServerProjectiles;
    ServerCollisionWorld mCollisionWorld;
    std::map<int32_t, std::chrono::steady_clock::time_point> mLastProjectileFire;
    std::map<int32_t, bool> mMeleeWasActive;
    std::map<int32_t, std::chrono::steady_clock::time_point> mLastMeleeAttack;
    std::set<std::pair<int32_t, int32_t>> mMeleeHits;
    std::set<std::pair<int32_t, int32_t>> mSeenActorEvents;
    struct PendingActorEvent {
        int32_t player = -1;
        NetworkActorEventPacket packet{};
        std::chrono::steady_clock::time_point received = std::chrono::steady_clock::now();
    };
    std::deque<PendingActorEvent> mPendingActorEvents;
    int32_t mNextServerProjectileId = 1;
    int32_t mNextServerCorpseId = -1000;
    int32_t mNextServerActorEventId = 1;
    std::map<std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t>,
             NetworkDynamicObjectStatePacket> mPersistentDynamicObjectStates;
    std::map<std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t>,
             std::chrono::steady_clock::time_point> mGrassRestoreDeadlines;
    std::map<std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t>, int32_t> mFishOwners;
    std::deque<NetworkVoicePacket> mVoice;
    bool mLastLocalPlayerDead = false;
};

} // namespace SoH::Network
