#include "ShipwrightNetworkRuntime.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace SoH::Network {

namespace {

const NetMsgFlags kReliable = static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);
std::atomic_bool gCancelConnection = false;

bool SequenceIsNewer(int32_t candidate, int32_t current) {
    return static_cast<int32_t>(static_cast<uint32_t>(candidate) - static_cast<uint32_t>(current)) > 0;
}

struct ServerVec3 {
    float x;
    float y;
    float z;
};

float SegmentDistanceSquared(const ServerVec3& a0, const ServerVec3& a1,
                             const ServerVec3& b0, const ServerVec3& b1) {
    const ServerVec3 u{ a1.x - a0.x, a1.y - a0.y, a1.z - a0.z };
    const ServerVec3 v{ b1.x - b0.x, b1.y - b0.y, b1.z - b0.z };
    const ServerVec3 w{ a0.x - b0.x, a0.y - b0.y, a0.z - b0.z };
    const auto dot = [](const ServerVec3& left, const ServerVec3& right) {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    };
    const float uu = dot(u, u);
    const float uv = dot(u, v);
    const float vv = dot(v, v);
    const float uw = dot(u, w);
    const float vw = dot(v, w);
    const float denominator = uu * vv - uv * uv;
    float numeratorA = denominator;
    float numeratorB = denominator;
    float denominatorA = denominator;
    float denominatorB = denominator;
    if (denominator < 0.00001f) {
        numeratorA = 0.0f;
        denominatorA = 1.0f;
        numeratorB = vw;
        denominatorB = vv;
    } else {
        numeratorA = uv * vw - vv * uw;
        numeratorB = uu * vw - uv * uw;
        if (numeratorA < 0.0f) {
            numeratorA = 0.0f;
            numeratorB = vw;
            denominatorB = vv;
        } else if (numeratorA > denominatorA) {
            numeratorA = denominatorA;
            numeratorB = vw + uv;
            denominatorB = vv;
        }
    }
    if (numeratorB < 0.0f) {
        numeratorB = 0.0f;
        if (-uw < 0.0f) {
            numeratorA = 0.0f;
        } else if (-uw > uu) {
            numeratorA = denominatorA;
        } else {
            numeratorA = -uw;
            denominatorA = uu;
        }
    } else if (numeratorB > denominatorB) {
        numeratorB = denominatorB;
        if ((-uw + uv) < 0.0f) {
            numeratorA = 0.0f;
        } else if ((-uw + uv) > uu) {
            numeratorA = denominatorA;
        } else {
            numeratorA = -uw + uv;
            denominatorA = uu;
        }
    }
    const float scaleA = std::fabs(numeratorA) < 0.00001f ? 0.0f : numeratorA / denominatorA;
    const float scaleB = std::fabs(numeratorB) < 0.00001f ? 0.0f : numeratorB / denominatorB;
    const ServerVec3 delta{ w.x + scaleA * u.x - scaleB * v.x,
                            w.y + scaleA * u.y - scaleB * v.y,
                            w.z + scaleA * u.z - scaleB * v.z };
    return dot(delta, delta);
}

ServerVec3 Subtract(const ServerVec3& left, const ServerVec3& right) {
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

float Dot(const ServerVec3& left, const ServerVec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

ServerVec3 Cross(const ServerVec3& left, const ServerVec3& right) {
    return { left.y * right.z - left.z * right.y,
             left.z * right.x - left.x * right.z,
             left.x * right.y - left.y * right.x };
}

float PointTriangleDistanceSquared(const ServerVec3& point, const ServerVec3& a,
                                   const ServerVec3& b, const ServerVec3& c) {
    const ServerVec3 ab = Subtract(b, a);
    const ServerVec3 ac = Subtract(c, a);
    const ServerVec3 ap = Subtract(point, a);
    const float d1 = Dot(ab, ap);
    const float d2 = Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return Dot(ap, ap);
    }

    const ServerVec3 bp = Subtract(point, b);
    const float d3 = Dot(ab, bp);
    const float d4 = Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        return Dot(bp, bp);
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float amount = d1 / (d1 - d3);
        const ServerVec3 delta{ ap.x - amount * ab.x, ap.y - amount * ab.y, ap.z - amount * ab.z };
        return Dot(delta, delta);
    }

    const ServerVec3 cp = Subtract(point, c);
    const float d5 = Dot(ab, cp);
    const float d6 = Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        return Dot(cp, cp);
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float amount = d2 / (d2 - d6);
        const ServerVec3 delta{ ap.x - amount * ac.x, ap.y - amount * ac.y, ap.z - amount * ac.z };
        return Dot(delta, delta);
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const ServerVec3 bc = Subtract(c, b);
        const float amount = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        const ServerVec3 delta{ bp.x - amount * bc.x, bp.y - amount * bc.y, bp.z - amount * bc.z };
        return Dot(delta, delta);
    }

    const float denominator = 1.0f / (va + vb + vc);
    const float v = vb * denominator;
    const float w = vc * denominator;
    const ServerVec3 delta{ ap.x - ab.x * v - ac.x * w,
                            ap.y - ab.y * v - ac.y * w,
                            ap.z - ab.z * v - ac.z * w };
    return Dot(delta, delta);
}

bool SegmentIntersectsTriangle(const ServerVec3& start, const ServerVec3& end,
                               const ServerVec3& a, const ServerVec3& b, const ServerVec3& c) {
    const ServerVec3 direction = Subtract(end, start);
    const ServerVec3 edge1 = Subtract(b, a);
    const ServerVec3 edge2 = Subtract(c, a);
    const ServerVec3 p = Cross(direction, edge2);
    const float determinant = Dot(edge1, p);
    if (std::fabs(determinant) < 0.00001f) {
        return false;
    }
    const float inverse = 1.0f / determinant;
    const ServerVec3 offset = Subtract(start, a);
    const float u = Dot(offset, p) * inverse;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    const ServerVec3 q = Cross(offset, edge1);
    const float v = Dot(direction, q) * inverse;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    const float amount = Dot(edge2, q) * inverse;
    return amount >= 0.0f && amount <= 1.0f;
}

float SegmentTriangleDistanceSquared(const ServerVec3& start, const ServerVec3& end,
                                     const ServerVec3& a, const ServerVec3& b, const ServerVec3& c) {
    const ServerVec3 normal = Cross(Subtract(b, a), Subtract(c, a));
    if (Dot(normal, normal) < 0.00001f) {
        return std::min({ SegmentDistanceSquared(start, end, a, b),
                          SegmentDistanceSquared(start, end, b, c),
                          SegmentDistanceSquared(start, end, c, a) });
    }
    if (SegmentIntersectsTriangle(start, end, a, b, c)) {
        return 0.0f;
    }
    return std::min({ PointTriangleDistanceSquared(start, a, b, c),
                      PointTriangleDistanceSquared(end, a, b, c),
                      SegmentDistanceSquared(start, end, a, b),
                      SegmentDistanceSquared(start, end, b, c),
                      SegmentDistanceSquared(start, end, c, a) });
}

bool SegmentVerticalCylinderFirstHit(const ServerVec3& start, const ServerVec3& end,
                                     const ServerVec3& cylinderBase, float radius, float height,
                                     float& hitRatio) {
    const float deltaX = end.x - start.x;
    const float deltaY = end.y - start.y;
    const float deltaZ = end.z - start.z;
    const float relativeX = start.x - cylinderBase.x;
    const float relativeZ = start.z - cylinderBase.z;
    const float radiusSquared = radius * radius;
    const float cylinderTop = cylinderBase.y + height;
    float closestRatio = 2.0f;

    const auto insideCylinder = [&](float ratio) {
        const float y = start.y + deltaY * ratio;
        const float x = relativeX + deltaX * ratio;
        const float z = relativeZ + deltaZ * ratio;
        return y >= cylinderBase.y && y <= cylinderTop && x * x + z * z <= radiusSquared;
    };
    if (insideCylinder(0.0f)) {
        hitRatio = 0.0f;
        return true;
    }

    const auto acceptRatio = [&](float ratio) {
        if (ratio >= 0.0f && ratio <= 1.0f && ratio < closestRatio && insideCylinder(ratio)) {
            closestRatio = ratio;
        }
    };

    const float quadraticA = deltaX * deltaX + deltaZ * deltaZ;
    if (quadraticA > 0.00001f) {
        const float quadraticB = 2.0f * (relativeX * deltaX + relativeZ * deltaZ);
        const float quadraticC = relativeX * relativeX + relativeZ * relativeZ - radiusSquared;
        const float discriminant = quadraticB * quadraticB - 4.0f * quadraticA * quadraticC;
        if (discriminant >= 0.0f) {
            const float root = std::sqrt(discriminant);
            acceptRatio((-quadraticB - root) / (2.0f * quadraticA));
            acceptRatio((-quadraticB + root) / (2.0f * quadraticA));
        }
    }
    if (std::fabs(deltaY) > 0.00001f) {
        acceptRatio((cylinderBase.y - start.y) / deltaY);
        acceptRatio((cylinderTop - start.y) / deltaY);
    }
    if (closestRatio > 1.0f) {
        return false;
    }
    hitRatio = closestRatio;
    return true;
}

bool PointNearDirectedPath(const ServerVec3& origin, const ServerVec3& velocity, const ServerVec3& point,
                           float maximumBehind, float maximumAhead, float radius) {
    const float speedSquared = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    const float offsetX = point.x - origin.x;
    const float offsetY = point.y - origin.y;
    const float offsetZ = point.z - origin.z;
    const float offsetSquared = offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ;
    if (speedSquared < 0.00001f) {
        return offsetSquared <= radius * radius;
    }
    const float speed = std::sqrt(speedSquared);
    const float along = (offsetX * velocity.x + offsetY * velocity.y + offsetZ * velocity.z) / speed;
    if (along < -maximumBehind || along > maximumAhead) {
        return false;
    }
    const float perpendicularSquared = std::max(0.0f, offsetSquared - along * along);
    return perpendicularSquared <= radius * radius;
}

uint32_t FishingIdentityHash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352D;
    value ^= value >> 15;
    value *= 0x846CA68B;
    return value ^ (value >> 16);
}

float FishingIdentityRandom01(uint32_t seed) {
    return static_cast<float>(FishingIdentityHash(seed) & 0x00FFFFFF) / 16777216.0f;
}

struct PondFishIdentity {
    int32_t homeX;
    int32_t homeY;
    int32_t homeZ;
    float baseLength;
    bool isLoach;
};

const PondFishIdentity* FindPondFishIdentity(int32_t actorParams) {
    static constexpr PondFishIdentity fish[] = {
        { 666, -45, 354, 38.0f, false },  { 681, -45, 240, 36.0f, false },
        { 670, -45, 90, 41.0f, false },   { 615, -45, -450, 35.0f, false },
        { 500, -45, -420, 39.0f, false }, { 420, -45, -550, 44.0f, false },
        { -264, -45, -640, 40.0f, false }, { -470, -45, -540, 34.0f, false },
        { -557, -45, -430, 54.0f, false }, { -260, -60, -330, 47.0f, false },
        { -500, -60, 330, 42.0f, false }, { 428, -40, -283, 33.0f, false },
        { 409, -70, -230, 57.0f, false }, { 450, -67, -300, 63.0f, false },
        { -136, -65, -196, 71.0f, false }, { -561, -35, -547, 45.0f, true },
        { 667, -35, 317, 43.0f, true },
    };
    const int32_t index = actorParams - 100;
    return index >= 0 && index < static_cast<int32_t>(sizeof(fish) / sizeof(fish[0])) ? &fish[index] : nullptr;
}

float DeterministicPondFishLength(int32_t sceneId, int32_t actorParams,
                                  const PondFishIdentity& fish) {
    const uint32_t seed = static_cast<uint32_t>(sceneId) * 0x9E3779B9U ^
                          static_cast<uint32_t>(actorParams) * 0x85EBCA6BU ^
                          static_cast<uint32_t>(fish.homeX) * 0xC2B2AE35U ^
                          static_cast<uint32_t>(fish.homeZ) * 0x27D4EB2FU;
    float length = fish.baseLength + FishingIdentityRandom01(seed) * 4.99999f;
    if (length >= 65.0f && FishingIdentityRandom01(seed ^ 0x63D83595U) < 0.05f) {
        length += FishingIdentityRandom01(seed ^ 0xA511E9B3U) * 7.99999f;
    }
    return length;
}

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
    LoadBanList(mBannedIdentities);
    LoadGameMasterList(mGameMasterIdentities);
    if (mCollisionWorld.LoadDefaultArchive()) {
        Error("Dedicated collision loaded: %zu scenes, %zu triangles, %zu authoritative wild fish",
              mCollisionWorld.SceneCount(), mCollisionWorld.TriangleCount(), mCollisionWorld.WildFishCount());
    } else {
        Error("Dedicated collision unavailable: using validated client collision witnesses");
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
    mPendingLeaveMessages.clear();
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
    mFishingStates.clear();
    mPlayerRemovals.clear();
    mDynamicObjectStates.clear();
    mActorEvents.clear();
    mProjectileStates.clear();
    mLatestProjectileSequences.clear();
    mPlayerDamage.clear();
    mPlayerRespawns.clear();
    mPersistentDynamicObjectStates.clear();
    mGrassRestoreDeadlines.clear();
    mAuthoritativePlayerStates.clear();
    mPlayerWasDead.clear();
    mLastDeadPlayerStates.clear();
    mRespawnDeadlines.clear();
    mServerCorpses.clear();
    mSceneCorpses.clear();
    mLastPlayerStateUpdate.clear();
    mServerProjectiles.clear();
    mLastProjectileFire.clear();
    mMeleeWasActive.clear();
    mLastMeleeAttack.clear();
    mMeleeHits.clear();
    mSeenActorEvents.clear();
    mPendingActorEvents.clear();
    mFishOwners.clear();
    mNextServerProjectileId = 1;
    mNextServerCorpseId = -1000;
    mNextServerActorEventId = 1;
    mVoice.clear();
    mLastLocalPlayerDead = false;
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
        ProcessPendingActorEvents();
        UpdateServerDynamicObjects();
        UpdateServerProjectiles();
        UpdateServerRespawns();
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
    // Clients learn peer names with the E2E private-chat public keys. Expose
    // that roster too so name-based /pm and in-world labels work without
    // weakening the server-owned identity exchange.
    for (const auto& [player, name] : mPrivateChatNames) {
        if (player == 0 || player == mLocalPlayerId || name.empty()) {
            continue;
        }
        const auto existing = std::find_if(result.begin(), result.end(), [player](const NetworkPlayerInfo& info) {
            return info.playerId == player;
        });
        if (existing == result.end()) {
            result.push_back({ player, "", name, false });
        }
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
        if (text[0] == '/') {
            RunServerCommand(0, text);
            return true;
        }
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
    const bool meleeItem = packet.itemAction >= 3 && packet.itemAction <= 5;
    if (!meleeItem) {
        packet.meleeWeaponState = 0;
        std::fill(std::begin(packet.meleeBase), std::end(packet.meleeBase), 0.0f);
        std::fill(std::begin(packet.meleeTip), std::end(packet.meleeTip), 0.0f);
    }
    // Validate the compact pose independently. Fishing telemetry now travels
    // in its own packet and must never be erased merely because a lure offset
    // is outside the pose validator's assumptions.
    NetworkPlayerStatePacket posePacket = packet;
    posePacket.fishingState = 0;
    std::memset(posePacket.fishingRodTipOffset, 0,
                offsetof(NetworkPlayerStatePacket, jointTable) -
                    offsetof(NetworkPlayerStatePacket, fishingRodTipOffset));
    if (!SanePlayerState(posePacket)) {
        return false;
    }
    packet.playerId = LocalPlayerId();
    posePacket.playerId = packet.playerId;
    const bool dead = (packet.stateFlags & NETWORK_PLAYER_DEAD) != 0;
    const bool reliableTransition = dead != mLastLocalPlayerDead;
    mLastLocalPlayerDead = dead;
    const NetMsgFlags stateFlags = reliableTransition ? kReliable : NMFHighPriority;
    if (mClient) {
        NetworkMessageRaw poseRaw;
        EncodeAppPacketRaw(poseRaw, posePacket);
        const bool poseSent = mLocalPlayerId >= 0 && SendToServer(NAMTPlayerState, poseRaw, stateFlags);
        if (poseSent && packet.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE) {
            NetworkMessageRaw fishingRaw;
            EncodeFishingStateRaw(fishingRaw, packet);
            SendToServer(NAMTFishingState, fishingRaw, NMFHighPriority);
        }
        return poseSent;
    }
    if (mServer) {
        packet.playerId = 0;
        const auto previous = mAuthoritativePlayerStates.find(0);
        ProcessServerDeathTransition(0, packet);
        if (packet.itemAction != NETWORK_PLAYER_ITEM_FISHING_POLE || packet.fishingState < 4 ||
            !packet.fishingFishActive) {
            ReleaseFishOwnedBy(0);
        }
        SanitizeServerFishingState(0, packet,
                                   previous == mAuthoritativePlayerStates.end() ? nullptr : &previous->second,
                                   0.05f);
        EvaluateMeleeAttack(0, packet);
        mAuthoritativePlayerStates[0] = packet;
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        Broadcast(NAMTPlayerState, raw, -1, stateFlags);
        if (packet.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE) {
            NetworkMessageRaw fishingRaw;
            EncodeFishingStateRaw(fishingRaw, packet);
            Broadcast(NAMTFishingState, fishingRaw, -1, NMFHighPriority);
        }
        return true;
    }
    return false;
}

bool ShipwrightNetworkRuntime::SendActorEvent(NetworkActorEventPacket packet) {
    if (!SaneActorEvent(packet)) {
        return false;
    }
    packet.sourcePlayerId = LocalPlayerId();
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, packet);
    if (mClient) {
        return mLocalPlayerId >= 0 && SendToServer(NAMTActorEvent, raw, kReliable);
    }
    if (mServer) {
        return AcceptServerActorEvent(0, packet);
    }
    return false;
}

bool ShipwrightNetworkRuntime::SendProjectileState(NetworkProjectileStatePacket packet, bool reliableTransition) {
    if (!SaneProjectileState(packet)) {
        return false;
    }
    packet.playerId = LocalPlayerId();
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, packet);
    if (mClient) {
        const NetMsgFlags flags = reliableTransition ? kReliable : NMFHighPriority;
        return mLocalPlayerId >= 0 && SendToServer(NAMTDynamicObjectStateRaw, raw, flags);
    }
    if (mServer) {
        return AcceptServerProjectile(0, packet);
    }
    return false;
}

bool ShipwrightNetworkRuntime::SendProjectileImpact(NetworkProjectileImpactPacket packet) {
    if (!SaneProjectileImpact(packet)) {
        return false;
    }
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, packet);
    if (mClient) {
        return mLocalPlayerId >= 0 && SendToServer(NAMTProjectileImpact, raw, kReliable);
    }
    if (mServer) {
        return AcceptServerProjectileImpact(0, packet);
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

bool ShipwrightNetworkRuntime::PollFishingState(NetworkPlayerStatePacket& packet) {
    if (mFishingStates.empty()) {
        return false;
    }
    packet = mFishingStates.front();
    mFishingStates.pop_front();
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

bool ShipwrightNetworkRuntime::PollActorEvent(NetworkActorEventPacket& packet) {
    if (mActorEvents.empty()) {
        return false;
    }
    packet = mActorEvents.front();
    mActorEvents.pop_front();
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

bool ShipwrightNetworkRuntime::PollPlayerRespawn(NetworkPlayerRespawnPacket& packet) {
    if (mPlayerRespawns.empty()) {
        return false;
    }
    packet = mPlayerRespawns.front();
    mPlayerRespawns.pop_front();
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

    NetworkMessageRaw fishingRaw;
    NetworkPlayerStatePacket fishingState{};
    if (ReadRaw(message, messageSize, NAMTFishingState, fishingRaw) &&
        DecodeFishingStateRaw(fishingRaw, fishingState) && fishingState.playerId != mLocalPlayerId) {
        mFishingStates.push_back(fishingState);
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

    NetworkActorEventPacket actorEvent{};
    if (ParseAppPacket(message, messageSize, NAMTActorEvent, actorEvent) && SaneActorEvent(actorEvent) &&
        actorEvent.sourcePlayerId != mLocalPlayerId) {
        mActorEvents.push_back(actorEvent);
        return;
    }

    NetworkProjectileStatePacket projectile{};
    if (ParseAppPacket(message, messageSize, NAMTDynamicObjectStateRaw, projectile) &&
        SaneProjectileState(projectile) && projectile.playerId != mLocalPlayerId) {
        const auto key = std::make_pair(projectile.playerId, projectile.projectileId);
        const auto latest = mLatestProjectileSequences.find(key);
        if (latest != mLatestProjectileSequences.end() &&
            !SequenceIsNewer(projectile.sequence, latest->second)) {
            return;
        }
        mLatestProjectileSequences[key] = projectile.sequence;
        mProjectileStates.push_back(projectile);
        return;
    }

    NetworkPlayerDamagePacket damage{};
    if (ParseAppPacket(message, messageSize, NAMTPlayerDamage, damage) && damage.targetPlayerId == mLocalPlayerId &&
        damage.damage > 0 && damage.damage <= 64) {
        mPlayerDamage.push_back(damage);
        return;
    }

    NetworkPlayerRespawnPacket respawn{};
    if (ParseAppPacket(message, messageSize, NAMTPlayerRespawn, respawn) &&
        respawn.playerId == mLocalPlayerId) {
        mPlayerRespawns.push_back(respawn);
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
        if (std::find(mBannedIdentities.begin(), mBannedIdentities.end(), identity.id) !=
            mBannedIdentities.end()) {
            mServer->KickOff(sender, NTRBanned, "banned");
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
        for (const auto& [corpseId, corpse] : mServerCorpses) {
            (void)corpseId;
            NetworkMessageRaw corpseRaw;
            EncodeAppPacketRaw(corpseRaw, corpse);
            SendToPeer(sender, NAMTPlayerState, corpseRaw, kReliable);
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
            if (text[0] == '/') {
                RunServerCommand(sender, text);
                return;
            }
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
        const bool meleeItem = state.itemAction >= 3 && state.itemAction <= 5;
        if (!meleeItem) {
            state.meleeWeaponState = 0;
            std::fill(std::begin(state.meleeBase), std::end(state.meleeBase), 0.0f);
            std::fill(std::begin(state.meleeTip), std::end(state.meleeTip), 0.0f);
        }
        if (!SanePlayerState(state)) {
            // Never let an invalid optional fishing sample discard the core
            // movement packet. The cleared packet is validated again below,
            // so this does not weaken pose, scene, or animation validation.
            state.fishingState = 0;
            std::memset(state.fishingRodTipOffset, 0,
                        offsetof(NetworkPlayerStatePacket, jointTable) -
                            offsetof(NetworkPlayerStatePacket, fishingRodTipOffset));
        }
        if (SanePlayerState(state)) {
            const auto now = std::chrono::steady_clock::now();
            const auto previous = mAuthoritativePlayerStates.find(sender);
            if (previous != mAuthoritativePlayerStates.end() &&
                !SequenceIsNewer(state.sequence, previous->second.sequence)) {
                return;
            }
            if (previous != mAuthoritativePlayerStates.end() &&
                state.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE &&
                previous->second.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE &&
                previous->second.sceneId == state.sceneId) {
                CopyNetworkFishingState(state, previous->second);
            }
            const auto respawnDeadline = mRespawnDeadlines.find(sender);
            if (respawnDeadline != mRespawnDeadlines.end() && now < respawnDeadline->second &&
                (state.stateFlags & NETWORK_PLAYER_DEAD) == 0) {
                const auto deadState = mLastDeadPlayerStates.find(sender);
                if (deadState != mLastDeadPlayerStates.end()) {
                    const int32_t sequence = state.sequence;
                    state = deadState->second;
                    state.sequence = sequence;
                } else {
                    return;
                }
            }
            if (previous == mAuthoritativePlayerStates.end() || previous->second.sceneId != state.sceneId ||
                previous->second.roomId != state.roomId) {
                Error("Network runtime: player %d entered scene %d room %d at %.1f %.1f %.1f", sender,
                      state.sceneId, state.roomId, state.x, state.y, state.z);
            }
            if (previous != mAuthoritativePlayerStates.end() && previous->second.sceneId != state.sceneId) {
                ReleaseFishOwnedBy(sender);
            }
            float elapsed = 0.05f;
            if (previous != mAuthoritativePlayerStates.end() && previous->second.sceneId == state.sceneId) {
                const auto previousTime = mLastPlayerStateUpdate.find(sender);
                elapsed = previousTime == mLastPlayerStateUpdate.end()
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
            }
            if (state.itemAction != NETWORK_PLAYER_ITEM_FISHING_POLE || state.fishingState < 4 ||
                !state.fishingFishActive) {
                ReleaseFishOwnedBy(sender);
            }
            SanitizeServerFishingState(sender, state,
                                       previous == mAuthoritativePlayerStates.end() ? nullptr : &previous->second,
                                       elapsed);
            ProcessServerDeathTransition(sender, state);
            EvaluateMeleeAttack(sender, state);
            mAuthoritativePlayerStates[sender] = state;
            mLastPlayerStateUpdate[sender] = now;
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, state);
            const NetMsgFlags stateFlags = NMFHighPriority;
            Broadcast(NAMTPlayerState, raw, sender, stateFlags);
            mPlayerStates.push_back(state);
        }
        return;
    }

    NetworkMessageRaw fishingRaw;
    NetworkPlayerStatePacket fishingState{};
    if (ReadRaw(message, messageSize, NAMTFishingState, fishingRaw) &&
        DecodeFishingStateRaw(fishingRaw, fishingState)) {
        const auto current = mAuthoritativePlayerStates.find(sender);
        if (current == mAuthoritativePlayerStates.end() ||
            current->second.itemAction != NETWORK_PLAYER_ITEM_FISHING_POLE ||
            current->second.sceneId != fishingState.sceneId ||
            static_cast<int32_t>(current->second.sequence - fishingState.sequence) > 8) {
            return;
        }
        NetworkPlayerStatePacket merged = current->second;
        CopyNetworkFishingState(merged, fishingState);
        if (!SanePlayerState(merged)) {
            return;
        }
        SanitizeServerFishingState(sender, merged, &current->second, 0.05f);
        CopyNetworkFishingState(current->second, merged);
        merged.playerId = sender;
        merged.sequence = current->second.sequence;
        NetworkMessageRaw forwarded;
        EncodeFishingStateRaw(forwarded, merged);
        Broadcast(NAMTFishingState, forwarded, sender, NMFHighPriority);
        mFishingStates.push_back(merged);
        return;
    }

    NetworkActorEventPacket actorEvent{};
    if (ParseAppPacket(message, messageSize, NAMTActorEvent, actorEvent) && SaneActorEvent(actorEvent)) {
        if (!AcceptServerActorEvent(sender, actorEvent)) {
            if (mPendingActorEvents.size() < 64) {
                mPendingActorEvents.push_back({ sender, actorEvent, std::chrono::steady_clock::now() });
            }
        }
        return;
    }

    NetworkProjectileStatePacket projectile{};
    if (ParseAppPacket(message, messageSize, NAMTDynamicObjectStateRaw, projectile)) {
        AcceptServerProjectile(sender, projectile);
        return;
    }

    NetworkProjectileImpactPacket impact{};
    if (ParseAppPacket(message, messageSize, NAMTProjectileImpact, impact)) {
        AcceptServerProjectileImpact(sender, impact);
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
    const auto pendingLeave = mPendingLeaveMessages.find(peer);
    const bool hasModerationReason = pendingLeave != mPendingLeaveMessages.end();
    ReleaseFishOwnedBy(peer);
    mPeers.erase(std::remove(mPeers.begin(), mPeers.end(), peer), mPeers.end());
    mIdentities.erase(peer);
    mServerCrypto.erase(peer);
    mPrivateChatKeys.erase(peer);
    mPrivateChatNames.erase(peer);
    if (hasModerationReason) {
        mPendingLeaveMessages.erase(pendingLeave);
    }
    mAuthoritativePlayerStates.erase(peer);
    mPlayerWasDead.erase(peer);
    mLastDeadPlayerStates.erase(peer);
    mRespawnDeadlines.erase(peer);
    mLastPlayerStateUpdate.erase(peer);
    mLastProjectileFire.erase(peer);
    mMeleeWasActive.erase(peer);
    mLastMeleeAttack.erase(peer);
    for (auto event = mSeenActorEvents.begin(); event != mSeenActorEvents.end();) {
        if (event->first == peer) {
            event = mSeenActorEvents.erase(event);
        } else {
            ++event;
        }
    }
    for (auto event = mPendingActorEvents.begin(); event != mPendingActorEvents.end();) {
        if (event->player == peer) {
            event = mPendingActorEvents.erase(event);
        } else {
            ++event;
        }
    }
    for (auto hit = mMeleeHits.begin(); hit != mMeleeHits.end();) {
        if (hit->first == peer || hit->second == peer) {
            hit = mMeleeHits.erase(hit);
        } else {
            ++hit;
        }
    }
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
    if (!hasModerationReason) {
        QueueChat("system: " + name + " left", CLKSystem);
    }
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

bool ShipwrightNetworkRuntime::IsGameMaster(int32_t player) const {
    const auto identity = mIdentities.find(player);
    return identity != mIdentities.end() &&
           std::find(mGameMasterIdentities.begin(), mGameMasterIdentities.end(), identity->second.id) !=
               mGameMasterIdentities.end();
}

bool ShipwrightNetworkRuntime::ResolvePlayerReference(const std::string& reference, int32_t& player) const {
    const std::string value = TrimWhitespace(reference);
    if (value.empty()) {
        return false;
    }

    char* end = nullptr;
    const long numeric = std::strtol(value.c_str(), &end, 10);
    if (end != value.c_str() && *end == '\0' && numeric > 0 &&
        std::find(mPeers.begin(), mPeers.end(), static_cast<int32_t>(numeric)) != mPeers.end()) {
        player = static_cast<int32_t>(numeric);
        return true;
    }

    for (const auto& [candidate, identity] : mIdentities) {
        if (_stricmp(identity.id.c_str(), value.c_str()) == 0) {
            player = candidate;
            return true;
        }
    }
    for (const auto& [candidate, identity] : mIdentities) {
        if (_stricmp(identity.name.c_str(), value.c_str()) == 0) {
            player = candidate;
            return true;
        }
    }
    return false;
}

void ShipwrightNetworkRuntime::SendCommandResult(int32_t player, const std::string& message) {
    const std::string line = "system: " + message;
    if (player == 0) {
        QueueChat(line, CLKSystem);
        return;
    }
    NetworkMessageRaw raw;
    raw.putString(line, CHAT_MAX_LINE_CHARS);
    SendToPeer(player, NAMTChat, raw, kReliable);
}

void ShipwrightNetworkRuntime::BroadcastSystem(const std::string& message) {
    const std::string line = "system: " + message;
    NetworkMessageRaw raw;
    raw.putString(line, CHAT_MAX_LINE_CHARS);
    Broadcast(NAMTChat, raw);
    QueueChat(line, CLKSystem);
}

bool ShipwrightNetworkRuntime::KickPlayer(const std::string& reference, bool ban, std::string& result) {
    int32_t player = -1;
    if (!mServer || !ResolvePlayerReference(reference, player)) {
        result = mServer ? "player not found" : "host only command";
        return false;
    }

    const auto identity = mIdentities.find(player);
    if (ban && (identity == mIdentities.end() || identity->second.id.empty())) {
        result = "player has no identity yet";
        return false;
    }
    if (ban && AddUniqueString(mBannedIdentities, identity->second.id)) {
        SaveBanList(mBannedIdentities);
    }

    result = PlayerName(player) + (ban ? " was banned" : " was kicked");
    mPendingLeaveMessages[player] = result;
    BroadcastSystem(result);
    mServer->KickOff(player, ban ? NTRBanned : NTRKicked, result.c_str());
    return true;
}

bool ShipwrightNetworkRuntime::GrantGameMaster(const std::string& reference, std::string& result) {
    int32_t player = -1;
    if (!mServer || !ResolvePlayerReference(reference, player)) {
        result = mServer ? "player not found" : "host only command";
        return false;
    }
    const auto identity = mIdentities.find(player);
    if (identity == mIdentities.end() || identity->second.id.empty()) {
        result = "player has no identity yet";
        return false;
    }
    if (AddUniqueString(mGameMasterIdentities, identity->second.id)) {
        SaveGameMasterList(mGameMasterIdentities);
    }
    result = PlayerName(player) + " is now an admin";
    BroadcastSystem(result);
    return true;
}

bool ShipwrightNetworkRuntime::RevokeGameMaster(const std::string& reference, std::string& result) {
    std::string identityValue = TrimWhitespace(reference);
    int32_t player = -1;
    if (ResolvePlayerReference(identityValue, player)) {
        const auto identity = mIdentities.find(player);
        if (identity != mIdentities.end()) {
            identityValue = identity->second.id;
        }
    }
    const auto found = std::find_if(mGameMasterIdentities.begin(), mGameMasterIdentities.end(),
                                    [&identityValue](const std::string& value) {
                                        return _stricmp(value.c_str(), identityValue.c_str()) == 0;
                                    });
    if (identityValue.empty() || found == mGameMasterIdentities.end()) {
        result = "admin identity not found";
        return false;
    }
    const std::string removed = *found;
    mGameMasterIdentities.erase(found);
    SaveGameMasterList(mGameMasterIdentities);
    result = removed + " is no longer an admin";
    BroadcastSystem(result);
    return true;
}

bool ShipwrightNetworkRuntime::UnbanIdentity(const std::string& identity, std::string& result) {
    const std::string value = TrimWhitespace(identity);
    const auto found = std::find_if(mBannedIdentities.begin(), mBannedIdentities.end(),
                                    [&value](const std::string& candidate) {
                                        return _stricmp(candidate.c_str(), value.c_str()) == 0;
                                    });
    if (value.empty() || found == mBannedIdentities.end()) {
        result = "banned identity not found";
        return false;
    }
    const std::string removed = *found;
    mBannedIdentities.erase(found);
    SaveBanList(mBannedIdentities);
    result = removed + " was unbanned";
    BroadcastSystem(result);
    return true;
}

void ShipwrightNetworkRuntime::SendUsersList(int32_t player) {
    const auto players = Players();
    SendCommandResult(player, "users online: " + std::to_string(players.size()));
    for (const auto& info : players) {
        int32_t latency = 0;
        int32_t throughput = 0;
        if (info.playerId > 0 && mServer) {
            mServer->GetConnectionInfo(info.playerId, latency, throughput);
        }
        std::ostringstream line;
        line << "#" << info.playerId << " " << info.name;
        if (!info.identity.empty()) {
            line << " [" << info.identity << "]";
        }
        line << " " << latency << " ms";
        SendCommandResult(player, line.str());
    }
}

void ShipwrightNetworkRuntime::SendIdentityList(int32_t player, const char* label,
                                                 const std::vector<std::string>& identities) {
    SendCommandResult(player, std::string(label) + ": " + std::to_string(identities.size()));
    for (const std::string& identity : identities) {
        SendCommandResult(player, identity);
    }
}

void ShipwrightNetworkRuntime::RunServerCommand(int32_t player, const std::string& command) {
    if (player != 0 && !IsGameMaster(player)) {
        SendCommandResult(player, "admin only command");
        return;
    }

    const size_t split = command.find(' ');
    const std::string name = command.substr(0, split);
    const std::string argument = split == std::string::npos ? std::string() : TrimWhitespace(command.substr(split + 1));
    std::string result;

    if (_stricmp(name.c_str(), "/kick") == 0 || _stricmp(name.c_str(), "/ban") == 0) {
        const bool ban = _stricmp(name.c_str(), "/ban") == 0;
        if (argument.empty()) {
            SendCommandResult(player, std::string("usage: ") + name + " name|identity|netId");
            return;
        }
        if (!KickPlayer(argument, ban, result)) {
            SendCommandResult(player, result);
        }
        return;
    }
    if (_stricmp(name.c_str(), "/gm") == 0 || _stricmp(name.c_str(), "/admin") == 0) {
        if (argument.empty()) {
            SendCommandResult(player, "usage: /admin name|identity|netId");
            return;
        }
        if (!GrantGameMaster(argument, result)) {
            SendCommandResult(player, result);
        }
        return;
    }
    if (_stricmp(name.c_str(), "/ungm") == 0 || _stricmp(name.c_str(), "/unadmin") == 0) {
        if (argument.empty()) {
            SendCommandResult(player, "usage: /unadmin name|identity|netId");
            return;
        }
        if (!RevokeGameMaster(argument, result)) {
            SendCommandResult(player, result);
        }
        return;
    }
    if (_stricmp(name.c_str(), "/unban") == 0) {
        if (argument.empty()) {
            SendCommandResult(player, "usage: /unban identity");
            return;
        }
        if (!UnbanIdentity(argument, result)) {
            SendCommandResult(player, result);
        }
        return;
    }
    if (_stricmp(name.c_str(), "/users") == 0) {
        SendUsersList(player);
        return;
    }
    if (_stricmp(name.c_str(), "/admins") == 0 || _stricmp(name.c_str(), "/gms") == 0) {
        SendIdentityList(player, "admins", mGameMasterIdentities);
        return;
    }
    if (_stricmp(name.c_str(), "/bans") == 0) {
        SendIdentityList(player, "bans", mBannedIdentities);
        return;
    }
    if (_stricmp(name.c_str(), "/help") == 0) {
        SendCommandResult(player, "admin commands: /users /kick /ban /unban /admin /unadmin /admins /bans");
        return;
    }
    SendCommandResult(player, "unknown command: " + name);
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
    const auto offsetInside = [&](const float value[3], float radius) {
        return saneFloat(value[0]) && saneFloat(value[1]) && saneFloat(value[2]) &&
               value[0] * value[0] + value[1] * value[1] + value[2] * value[2] <= radius * radius;
    };
    const bool fishingPoleActive = packet.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE;
    const bool fishingVisualActive = fishingPoleActive && packet.fishingState != 0;
    const bool saneFishing = packet.fishingState <= 5 &&
                             packet.fishingLineSpooled < NETWORK_FISHING_LINE_POINT_COUNT &&
                             saneFloat(packet.fishingRodTipOffset[0]) &&
                             saneFloat(packet.fishingRodTipOffset[1]) && saneFloat(packet.fishingRodTipOffset[2]) &&
                             saneFloat(packet.fishingLureOffset[0]) && saneFloat(packet.fishingLureOffset[1]) &&
                             saneFloat(packet.fishingLureOffset[2]) && saneFloat(packet.fishingRodBendY) &&
                              saneFloat(packet.fishingRodBendX) && saneFloat(packet.fishingRodTwist) &&
                              saneFloat(packet.fishingRodCastX) && saneFloat(packet.fishingLureSpin) &&
                              std::abs(packet.fishingLureSpin) <= 8.0f &&
                              saneFloat(packet.fishingLureZOffset) &&
                              std::abs(packet.fishingLureZOffset) <= 2000.0f &&
                               saneFloat(packet.fishingLineScale) &&
                               saneFloat(packet.fishingLineGravity) && packet.fishingLineGravity >= 0.0f &&
                               packet.fishingLineGravity <= 520.0f &&
                             (!fishingVisualActive ||
                              (packet.fishingLineScale >= 0.0001f && packet.fishingLineScale <= 0.01f)) &&
                              packet.fishingLureType <= 2 &&
                              packet.fishingLineHooked <= 1 && packet.fishingSinkingLureSegmentIndex < 20 &&
                              packet.fishingSinkingLureUnderwater <= 1 &&
                              packet.fishingFishActive <= 1 &&
                             packet.fishingFishIsLoach <= 1 && saneFloat(packet.fishingFishLength) &&
                             packet.fishingFishLength >= 0.0f && packet.fishingFishLength <= 100.0f &&
                             offsetInside(packet.fishingRodTipOffset, 5000.0f) &&
                             offsetInside(packet.fishingLureOffset, 5000.0f) &&
                             offsetInside(packet.fishingLureDrawOffset, 5000.0f) &&
                             offsetInside(packet.fishingFishOffset, 5000.0f) &&
                             (fishingPoleActive ||
                              (packet.fishingState == 0 && packet.fishingFishActive == 0)) &&
                             (!packet.fishingFishActive ||
                              (fishingPoleActive && packet.fishingState >= 4));
    for (unsigned char axis = 0; axis < 3; ++axis) {
        if (!saneFloat(packet.fishingLureRot[axis]) || !saneFloat(packet.fishingFishOffset[axis])) {
            return false;
        }
    }
    for (unsigned char hook = 0; hook < 2; ++hook) {
        for (unsigned char axis = 0; axis < 3; ++axis) {
            if (!saneFloat(packet.fishingLureHookOffsets[hook][axis])) {
                return false;
            }
        }
        for (unsigned char axis = 0; axis < 2; ++axis) {
            if (!saneFloat(packet.fishingLureHookRot[hook][axis])) {
                return false;
            }
        }
    }
    for (unsigned char axis = 0; axis < 3; ++axis) {
        if (!saneFloat(packet.meleeBase[axis]) || !saneFloat(packet.meleeTip[axis])) {
            return false;
        }
    }
    return packet.sceneId >= 0 && packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.roomId >= -1 && packet.roomId < 256 && saneFloat(packet.x) && saneFloat(packet.y) &&
           saneFloat(packet.z) && saneFloat(packet.speed) && saneFloat(packet.bowStringScale) &&
           packet.bowStringScale >= 0.0f && packet.bowStringScale <= 1.0f &&
           (packet.stateFlags & ~(NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_GROUNDED | NETWORK_PLAYER_SWIMMING |
                                  NETWORK_PLAYER_READY_TO_FIRE | NETWORK_PLAYER_DEAD)) == 0 &&
           packet.modelGroup < 16 && packet.itemAction < 0x43 && packet.meleeWeaponState >= 0 &&
           packet.meleeWeaponState <= 2 && saneFishing;
}

void ShipwrightNetworkRuntime::EvaluateMeleeAttack(int32_t player, const NetworkPlayerStatePacket& state) {
    if (!mServer) {
        return;
    }
    const bool active = (state.stateFlags & NETWORK_PLAYER_DEAD) == 0 && state.meleeWeaponState > 0 &&
                        state.itemAction >= 3 && state.itemAction <= 5;
    mMeleeWasActive[player] = active;
    if (!active) {
        for (auto hit = mMeleeHits.begin(); hit != mMeleeHits.end();) {
            if (hit->first == player) {
                hit = mMeleeHits.erase(hit);
            } else {
                ++hit;
            }
        }
        return;
    }
    const float baseX = state.meleeBase[0];
    const float baseY = state.meleeBase[1];
    const float baseZ = state.meleeBase[2];
    const float tipX = state.meleeTip[0];
    const float tipY = state.meleeTip[1];
    const float tipZ = state.meleeTip[2];
    const auto endpointNearPlayer = [&](float x, float y, float z) {
        const float dx = x - state.x;
        const float dy = y - (state.y + 35.0f);
        const float dz = z - state.z;
        return dx * dx + dy * dy + dz * dz <= 110.0f * 110.0f;
    };
    if (!endpointNearPlayer(baseX, baseY, baseZ) || !endpointNearPlayer(tipX, tipY, tipZ)) {
        return;
    }
    const float segmentX = tipX - baseX;
    const float segmentY = tipY - baseY;
    const float segmentZ = tipZ - baseZ;
    const float segmentLengthSquared = segmentX * segmentX + segmentY * segmentY + segmentZ * segmentZ;
    if (segmentLengthSquared < 1.0f || segmentLengthSquared > 100.0f * 100.0f) {
        return;
    }
    const ServerVec3 newBase{ baseX, baseY, baseZ };
    const ServerVec3 newTip{ tipX, tipY, tipZ };
    bool hasPreviousBlade = false;
    ServerVec3 oldBase{};
    ServerVec3 oldTip{};
    const auto previous = mAuthoritativePlayerStates.find(player);
    if (previous != mAuthoritativePlayerStates.end() && previous->second.sceneId == state.sceneId &&
        previous->second.meleeWeaponState > 0) {
        const NetworkPlayerStatePacket& old = previous->second;
        oldBase = { old.meleeBase[0], old.meleeBase[1], old.meleeBase[2] };
        oldTip = { old.meleeTip[0], old.meleeTip[1], old.meleeTip[2] };
        const float oldLengthX = oldTip.x - oldBase.x;
        const float oldLengthY = oldTip.y - oldBase.y;
        const float oldLengthZ = oldTip.z - oldBase.z;
        const float oldLengthSquared = oldLengthX * oldLengthX + oldLengthY * oldLengthY + oldLengthZ * oldLengthZ;
        const auto oldEndpointNearPlayer = [&](const ServerVec3& endpoint) {
            const float dx = endpoint.x - old.x;
            const float dy = endpoint.y - (old.y + 35.0f);
            const float dz = endpoint.z - old.z;
            return dx * dx + dy * dy + dz * dz <= 110.0f * 110.0f;
        };
        if (oldLengthSquared >= 1.0f && oldLengthSquared <= 100.0f * 100.0f &&
            oldEndpointNearPlayer(oldBase) && oldEndpointNearPlayer(oldTip)) {
            hasPreviousBlade = true;
        }
    }
    for (const auto& [targetId, target] : mAuthoritativePlayerStates) {
        const auto hitKey = std::make_pair(player, targetId);
        if (targetId == player || target.sceneId != state.sceneId ||
            (target.stateFlags & (NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_DEAD)) != NETWORK_PLAYER_VISIBLE ||
            mMeleeHits.count(hitKey) != 0) {
            continue;
        }
        // Match Link's native 12-unit-radius, 60-unit-high cylinder and test
        // the swept visible blade between the two accepted pose samples.
        const ServerVec3 targetBase{ target.x, target.y, target.z };
        const ServerVec3 targetTop{ target.x, target.y + 60.0f, target.z };
        float closestDistanceSquared = SegmentDistanceSquared(newBase, newTip, targetBase, targetTop);
        if (hasPreviousBlade) {
            // The visible blade between two accepted poses sweeps a ruled
            // quadrilateral. Test its two triangles, not only its four edges;
            // otherwise the middle of a fast sword arc can pass through Link
            // without registering while widening an endpoint would create a
            // phantom hit beyond the rendered sword.
            closestDistanceSquared = std::min(
                closestDistanceSquared,
                SegmentTriangleDistanceSquared(targetBase, targetTop, oldBase, oldTip, newTip));
            closestDistanceSquared = std::min(
                closestDistanceSquared,
                SegmentTriangleDistanceSquared(targetBase, targetTop, oldBase, newTip, newBase));
        }
        if (closestDistanceSquared > 12.0f * 12.0f) {
            continue;
        }
        mMeleeHits.insert(hitKey);
        const short impactYaw = static_cast<short>(std::atan2(target.x - state.x, target.z - state.z) *
                                                   (32768.0f / 3.14159265358979323846f));
        const short swordDamage = state.itemAction == 5 ? 16 : (state.itemAction == 3 ? 8 : 4);
        NetworkPlayerDamagePacket damage{ player, targetId, swordDamage, impactYaw };
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, damage);
        if (targetId == 0) {
            mPlayerDamage.push_back(damage);
        } else {
            SendToPeer(targetId, NAMTPlayerDamage, raw, kReliable);
        }
    }
}

void ShipwrightNetworkRuntime::SanitizeServerFishingState(
    int32_t player, NetworkPlayerStatePacket& state, const NetworkPlayerStatePacket* previous,
    float elapsedSeconds) {
    const auto clearFish = [&]() {
        state.fishingFishActive = 0;
        state.fishingFishIsLoach = 0;
        state.fishingFishLength = 0.0f;
        state.fishingFishRoomId = 0;
        state.fishingFishActorParams = 0;
        state.fishingFishHomeX = 0;
        state.fishingFishHomeY = 0;
        state.fishingFishHomeZ = 0;
        std::fill(std::begin(state.fishingFishOffset), std::end(state.fishingFishOffset), 0.0f);
        std::fill(std::begin(state.fishingFishRot), std::end(state.fishingFishRot), 0);
        for (auto& limb : state.fishingFishLimbRot) {
            limb = 0;
        }
    };

    if (state.itemAction != NETWORK_PLAYER_ITEM_FISHING_POLE) {
        state.fishingState = 0;
        state.fishingLineHooked = 0;
        clearFish();
        return;
    }

    // The client runs Link's native rod animation and lure integrator, while
    // the server bounds that telemetry to a physically reachable path and its
    // own static collision world before forwarding it to other players.
    ServerVec3 lure{ state.x + state.fishingLureOffset[0],
                     state.y + state.fishingLureOffset[1],
                     state.z + state.fishingLureOffset[2] };
    if (previous != nullptr && previous->sceneId == state.sceneId &&
        previous->itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE) {
        const ServerVec3 oldLure{ previous->x + previous->fishingLureOffset[0],
                                  previous->y + previous->fishingLureOffset[1],
                                  previous->z + previous->fishingLureOffset[2] };
        const float dx = lure.x - oldLure.x;
        const float dy = lure.y - oldLure.y;
        const float dz = lure.z - oldLure.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float maximumDistance = 80.0f + 4000.0f * std::clamp(elapsedSeconds, 0.0f, 0.25f);
        if (distance > maximumDistance && distance > 0.0f) {
            const float scale = maximumDistance / distance;
            lure = { oldLure.x + dx * scale, oldLure.y + dy * scale, oldLure.z + dz * scale };
        }

        ServerCollisionPoint impact{};
        if (mCollisionWorld.SegmentCast(state.sceneId, { oldLure.x, oldLure.y, oldLure.z },
                                        { lure.x, lure.y, lure.z }, impact)) {
            const float hitDx = impact.x - oldLure.x;
            const float hitDy = impact.y - oldLure.y;
            const float hitDz = impact.z - oldLure.z;
            const float hitDistance = std::sqrt(hitDx * hitDx + hitDy * hitDy + hitDz * hitDz);
            const float backoff = hitDistance > 2.0f ? (hitDistance - 2.0f) / hitDistance : 0.0f;
            lure = { oldLure.x + hitDx * backoff, oldLure.y + hitDy * backoff,
                     oldLure.z + hitDz * backoff };
        }
    }
    state.fishingLureOffset[0] = lure.x - state.x;
    state.fishingLureOffset[1] = lure.y - state.y;
    state.fishingLureOffset[2] = lure.z - state.z;

    auto owned = std::find_if(mFishOwners.begin(), mFishOwners.end(),
                              [player](const auto& entry) { return entry.second == player; });
    if (owned == mFishOwners.end() && state.fishingFishActive && state.fishingState >= 4) {
        const auto requestedKey = std::make_tuple(
            state.sceneId, state.fishingFishRoomId, 0xFE, state.fishingFishActorParams,
            state.fishingFishHomeX, state.fishingFishHomeY, state.fishingFishHomeZ);
        const bool requestedLoach = state.fishingFishActorParams == 401 ||
                                    state.fishingFishActorParams == 115 ||
                                    state.fishingFishActorParams == 116;
        bool validIdentity = false;
        if (state.fishingFishActorParams == 400 || state.fishingFishActorParams == 401) {
            const ServerWildFishSpawn* requestedFish = mCollisionWorld.FindWildFish(
                state.sceneId, state.fishingFishActorParams, state.fishingFishHomeX,
                state.fishingFishHomeY, state.fishingFishHomeZ);
            validIdentity = requestedFish != nullptr && requestedFish->isLoach == requestedLoach;
        } else {
            constexpr int32_t fishingPondScene = 0x49;
            const PondFishIdentity* requestedFish = FindPondFishIdentity(state.fishingFishActorParams);
            validIdentity = state.sceneId == fishingPondScene && requestedFish != nullptr &&
                            state.fishingFishHomeX == requestedFish->homeX &&
                            state.fishingFishHomeY == requestedFish->homeY &&
                            state.fishingFishHomeZ == requestedFish->homeZ &&
                            requestedFish->isLoach == requestedLoach;
        }

        const ServerVec3 requestedFishPos{ state.x + state.fishingFishOffset[0],
                                           state.y + state.fishingFishOffset[1],
                                           state.z + state.fishingFishOffset[2] };
        const float fishDx = requestedFishPos.x - lure.x;
        const float fishDy = requestedFishPos.y - lure.y;
        const float fishDz = requestedFishPos.z - lure.z;
        const auto existingOwner = mFishOwners.find(requestedKey);
        if (validIdentity && fishDx * fishDx + fishDy * fishDy + fishDz * fishDz <= 250.0f * 250.0f &&
            (existingOwner == mFishOwners.end() || existingOwner->second == player)) {
            mFishOwners[requestedKey] = player;
            owned = mFishOwners.find(requestedKey);
        }
    }
    if (owned == mFishOwners.end() || !state.fishingFishActive || state.fishingState < 4) {
        clearFish();
        return;
    }

    const auto& [sceneId, roomId, actorId, actorParams, homeX, homeY, homeZ] = owned->first;
    (void)roomId;
    (void)actorId;
    if (sceneId != state.sceneId) {
        clearFish();
        return;
    }

    const bool isLoach = actorParams == 401 || actorParams == 115 || actorParams == 116;
    float canonicalLength = 0.0f;
    const ServerWildFishSpawn* wildFish = nullptr;
    if (actorParams == 400 || actorParams == 401) {
        wildFish = mCollisionWorld.FindWildFish(sceneId, actorParams, homeX, homeY, homeZ);
        if (wildFish == nullptr) {
            clearFish();
            return;
        }
        canonicalLength = wildFish->length;
    } else {
        const PondFishIdentity* pondFish = FindPondFishIdentity(actorParams);
        if (pondFish == nullptr) {
            clearFish();
            return;
        }
        canonicalLength = DeterministicPondFishLength(sceneId, actorParams, *pondFish);
    }

    ServerVec3 fish{ state.x + state.fishingFishOffset[0],
                     state.y + state.fishingFishOffset[1],
                     state.z + state.fishingFishOffset[2] };
    const float fishDx = fish.x - lure.x;
    const float fishDy = fish.y - lure.y;
    const float fishDz = fish.z - lure.z;
    const float fishDistance = std::sqrt(fishDx * fishDx + fishDy * fishDy + fishDz * fishDz);
    constexpr float maximumFishMouthDistance = 120.0f;
    if (fishDistance > maximumFishMouthDistance && fishDistance > 0.0f) {
        const float scale = maximumFishMouthDistance / fishDistance;
        fish = { lure.x + fishDx * scale, lure.y + fishDy * scale, lure.z + fishDz * scale };
    }
    if (wildFish != nullptr) {
        fish.x = std::clamp(fish.x, wildFish->minX - 20.0f, wildFish->maxX + 20.0f);
        fish.y = std::clamp(fish.y, wildFish->waterSurfaceY - 250.0f, wildFish->waterSurfaceY + 80.0f);
        fish.z = std::clamp(fish.z, wildFish->minZ - 20.0f, wildFish->maxZ + 20.0f);
    }
    state.fishingFishOffset[0] = fish.x - state.x;
    state.fishingFishOffset[1] = fish.y - state.y;
    state.fishingFishOffset[2] = fish.z - state.z;
    state.fishingFishActive = 1;
    state.fishingFishIsLoach = static_cast<unsigned char>(isLoach);
    state.fishingFishLength = canonicalLength;
    state.fishingFishRoomId = roomId;
    state.fishingFishActorParams = actorParams;
    state.fishingFishHomeX = homeX;
    state.fishingFishHomeY = homeY;
    state.fishingFishHomeZ = homeZ;
}

bool ShipwrightNetworkRuntime::AcceptServerActorEvent(int32_t player, NetworkActorEventPacket packet) {
    if (!mServer || !SaneActorEvent(packet)) {
        return false;
    }
    packet.sourcePlayerId = player;
    const auto acceptedPlayer = mAuthoritativePlayerStates.find(player);
    if (acceptedPlayer == mAuthoritativePlayerStates.end() ||
        (acceptedPlayer->second.stateFlags & NETWORK_PLAYER_DEAD) != 0) {
        return false;
    }
    const auto eventKey = std::make_pair(player, packet.eventId);
    if (mSeenActorEvents.count(eventKey) != 0) {
        return true;
    }
    NetworkDynamicObjectStatePacket objectState{ packet.sceneId, packet.roomId, packet.actorId,
                                                  packet.actorParams, packet.homeX, packet.homeY,
                                                  packet.homeZ, 0 };
    const auto objectKey = std::make_tuple(objectState.sceneId, objectState.roomId, objectState.actorId,
                                           objectState.actorParams, objectState.homeX, objectState.homeY,
                                           objectState.homeZ);
    if (packet.eventType == NETWORK_ACTOR_EVENT_FISH_HOOK ||
        packet.eventType == NETWORK_ACTOR_EVENT_FISH_RELEASE) {
        const auto state = mAuthoritativePlayerStates.find(player);
        if (state == mAuthoritativePlayerStates.end() || state->second.sceneId != packet.sceneId ||
            state->second.itemAction != NETWORK_PLAYER_ITEM_FISHING_POLE) {
            return false;
        }
        if (packet.eventType == NETWORK_ACTOR_EVENT_FISH_HOOK) {
            const float lureX = state->second.x + state->second.fishingLureOffset[0];
            const float lureY = state->second.y + state->second.fishingLureOffset[1];
            const float lureZ = state->second.z + state->second.fishingLureOffset[2];
            const float dx = packet.x - lureX;
            const float dy = packet.y - lureY;
            const float dz = packet.z - lureZ;
            const bool actorIsLoach = packet.actorParams == 401 || packet.actorParams == 115 ||
                                      packet.actorParams == 116;
            float canonicalLength = 0.0f;
            if (packet.actorParams == 400 || packet.actorParams == 401) {
                const ServerWildFishSpawn* fish = mCollisionWorld.FindWildFish(
                    packet.sceneId, packet.actorParams, packet.homeX, packet.homeY, packet.homeZ);
                if (fish == nullptr || fish->isLoach != actorIsLoach ||
                    packet.x < fish->minX - 20.0f || packet.x > fish->maxX + 20.0f ||
                    packet.z < fish->minZ - 20.0f || packet.z > fish->maxZ + 20.0f ||
                    packet.y < fish->waterSurfaceY - 250.0f || packet.y > fish->waterSurfaceY + 80.0f ||
                    lureX < fish->minX - 20.0f || lureX > fish->maxX + 20.0f ||
                    lureZ < fish->minZ - 20.0f || lureZ > fish->maxZ + 20.0f) {
                    return false;
                }
                canonicalLength = fish->length;
            } else {
                constexpr int32_t fishingPondScene = 0x49;
                const PondFishIdentity* fish = FindPondFishIdentity(packet.actorParams);
                if (packet.sceneId != fishingPondScene || fish == nullptr ||
                    packet.homeX != fish->homeX || packet.homeY != fish->homeY || packet.homeZ != fish->homeZ ||
                    actorIsLoach != fish->isLoach) {
                    return false;
                }
                canonicalLength = DeterministicPondFishLength(packet.sceneId, packet.actorParams, *fish);
            }
            if (state->second.fishingState < 4 || dx * dx + dy * dy + dz * dz > 250.0f * 250.0f ||
                mFishOwners.count(objectKey) != 0) {
                return false;
            }
            mFishOwners[objectKey] = player;
            state->second.fishingFishActive = 1;
            state->second.fishingFishIsLoach = static_cast<unsigned char>(actorIsLoach);
            state->second.fishingFishLength = canonicalLength;
            state->second.fishingFishOffset[0] = packet.x - state->second.x;
            state->second.fishingFishOffset[1] = packet.y - state->second.y;
            state->second.fishingFishOffset[2] = packet.z - state->second.z;
        } else {
            const auto owner = mFishOwners.find(objectKey);
            if (owner == mFishOwners.end() || owner->second != player) {
                return false;
            }
            mFishOwners.erase(owner);
        }
    } else {
        if (packet.eventType == NETWORK_ACTOR_EVENT_GRASS_THROWN_BREAK) {
            const float dx = acceptedPlayer->second.x - packet.x;
            const float dy = (acceptedPlayer->second.y + 35.0f) - packet.y;
            const float dz = acceptedPlayer->second.z - packet.z;
            if (acceptedPlayer->second.sceneId != packet.sceneId ||
                dx * dx + dy * dy + dz * dz > 250.0f * 250.0f) {
                return false;
            }
        } else if (!PlayerIsNearObject(player, objectState)) {
            return false;
        }
        if (packet.eventType == NETWORK_ACTOR_EVENT_GRASS_CUT) {
            bool authoritativeCut = false;
            const NetworkPlayerStatePacket& playerState = acceptedPlayer->second;
            if (playerState.meleeWeaponState > 0 && playerState.itemAction >= 3 && playerState.itemAction <= 5) {
                float hitRatio = 0.0f;
                authoritativeCut = SegmentVerticalCylinderFirstHit(
                    { playerState.meleeBase[0], playerState.meleeBase[1], playerState.meleeBase[2] },
                    { playerState.meleeTip[0], playerState.meleeTip[1], playerState.meleeTip[2] },
                    { packet.x, packet.y, packet.z }, 12.0f, 44.0f, hitRatio);
            }
            if (!authoritativeCut) {
                for (const auto& [projectileKey, projectile] : mServerProjectiles) {
                    (void)projectileKey;
                    if (projectile.state.sceneId != packet.sceneId) {
                        continue;
                    }
                    const float dx = projectile.state.x - packet.x;
                    const float dy = projectile.state.y - (packet.y + 22.0f);
                    const float dz = projectile.state.z - packet.z;
                    const float distanceSquared = dx * dx + dy * dy + dz * dz;
                    if ((projectile.state.projectileKind == NETWORK_PROJECTILE_BOMB &&
                         projectile.state.phase == NETWORK_BOMB_EXPLODING &&
                         distanceSquared <= 150.0f * 150.0f) ||
                        (projectile.state.projectileKind == NETWORK_PROJECTILE_ARROW &&
                         projectile.state.phase == NETWORK_ARROW_FLYING &&
                         distanceSquared <= 180.0f * 180.0f)) {
                        authoritativeCut = true;
                        break;
                    }
                }
            }
            if (!authoritativeCut) {
                return false;
            }
        }
        if (packet.eventType == NETWORK_ACTOR_EVENT_OWL_DEPART) {
            const auto playerState = mAuthoritativePlayerStates.find(player);
            if (playerState == mAuthoritativePlayerStates.end()) {
                return false;
            }
            int32_t owlType = (packet.actorParams & 0xFC0) >> 6;
            if (packet.actorParams == 0xFFF) {
                owlType = 1;
            }
            float triggerDistance = 360.0f;
            if (owlType == 2) {
                triggerDistance = 540.0f;
            } else if (owlType == 3) {
                triggerDistance = 480.0f;
            } else if (owlType == 7 || owlType == 8 || owlType == 9) {
                triggerDistance = 120.0f;
            } else if (owlType == 11) {
                triggerDistance = 190.0f;
            }
            const float owlDx = playerState->second.x - static_cast<float>(packet.homeX);
            const float owlDz = playerState->second.z - static_cast<float>(packet.homeZ);
            const float allowedDistance = triggerDistance + 25.0f;
            if (owlDx * owlDx + owlDz * owlDz > allowedDistance * allowedDistance) {
                return false;
            }
        }
        if (packet.eventType == NETWORK_ACTOR_EVENT_BOULDER_BREAK) {
            bool explosionNear = false;
            for (const auto& [projectileKey, projectile] : mServerProjectiles) {
                (void)projectileKey;
                if (projectile.state.projectileKind != NETWORK_PROJECTILE_BOMB ||
                    projectile.state.phase != NETWORK_BOMB_EXPLODING ||
                    projectile.state.sceneId != packet.sceneId) {
                    continue;
                }
                const float dx = projectile.state.x - packet.x;
                const float dy = projectile.state.y - packet.y;
                const float dz = projectile.state.z - packet.z;
                if (dx * dx + dy * dy + dz * dz <= 150.0f * 150.0f) {
                    explosionNear = true;
                    break;
                }
            }
            if (!explosionNear) {
                return false;
            }
        }
    }
    mSeenActorEvents.insert(eventKey);
    if (packet.eventType == NETWORK_ACTOR_EVENT_GRASS_CUT ||
        packet.eventType == NETWORK_ACTOR_EVENT_GRASS_THROWN_BREAK ||
        packet.eventType == NETWORK_ACTOR_EVENT_BOULDER_BREAK) {
        objectState.destroyed = 1;
        mPersistentDynamicObjectStates[objectKey] = objectState;
        if ((packet.eventType == NETWORK_ACTOR_EVENT_GRASS_CUT ||
             packet.eventType == NETWORK_ACTOR_EVENT_GRASS_THROWN_BREAK) &&
            (packet.actorParams & 3) == 1) {
            // EnKusa_CutWaitRegrow uses 120 native gameplay frames at 20 Hz.
            mGrassRestoreDeadlines[objectKey] = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        }
        NetworkMessageRaw stateRaw;
        EncodeAppPacketRaw(stateRaw, objectState);
        Broadcast(NAMTDynamicObjectState, stateRaw, player, kReliable);
        mDynamicObjectStates.push_back(objectState);
    }
    NetworkMessageRaw eventRaw;
    EncodeAppPacketRaw(eventRaw, packet);
    Broadcast(NAMTActorEvent, eventRaw, player, kReliable);
    mActorEvents.push_back(packet);
    return true;
}

void ShipwrightNetworkRuntime::ProcessPendingActorEvents() {
    const auto now = std::chrono::steady_clock::now();
    for (auto event = mPendingActorEvents.begin(); event != mPendingActorEvents.end();) {
        if (AcceptServerActorEvent(event->player, event->packet)) {
            event = mPendingActorEvents.erase(event);
        } else if (now - event->received >= std::chrono::seconds(1)) {
            event = mPendingActorEvents.erase(event);
        } else {
            ++event;
        }
    }
}

void ShipwrightNetworkRuntime::UpdateServerDynamicObjects() {
    const auto now = std::chrono::steady_clock::now();
    for (auto deadline = mGrassRestoreDeadlines.begin(); deadline != mGrassRestoreDeadlines.end();) {
        if (now < deadline->second) {
            ++deadline;
            continue;
        }
        const auto object = mPersistentDynamicObjectStates.find(deadline->first);
        if (object != mPersistentDynamicObjectStates.end()) {
            NetworkDynamicObjectStatePacket restored = object->second;
            restored.destroyed = 0;
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, restored);
            Broadcast(NAMTDynamicObjectState, raw, -1, kReliable);
            mDynamicObjectStates.push_back(restored);
            mPersistentDynamicObjectStates.erase(object);
        }
        deadline = mGrassRestoreDeadlines.erase(deadline);
    }
}

void ShipwrightNetworkRuntime::ReleaseFishOwnedBy(int32_t player) {
    for (auto owner = mFishOwners.begin(); owner != mFishOwners.end();) {
        if (owner->second != player) {
            ++owner;
            continue;
        }
        const auto& [sceneId, roomId, actorId, actorParams, homeX, homeY, homeZ] = owner->first;
        NetworkActorEventPacket packet{};
        packet.sourcePlayerId = player;
        packet.eventId = mNextServerActorEventId++;
        packet.sceneId = sceneId;
        packet.roomId = roomId;
        packet.actorId = actorId;
        packet.actorParams = actorParams;
        packet.homeX = homeX;
        packet.homeY = homeY;
        packet.homeZ = homeZ;
        packet.x = static_cast<float>(homeX);
        packet.y = static_cast<float>(homeY);
        packet.z = static_cast<float>(homeZ);
        packet.eventType = NETWORK_ACTOR_EVENT_FISH_RELEASE;
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        Broadcast(NAMTActorEvent, raw, player, kReliable);
        mActorEvents.push_back(packet);
        owner = mFishOwners.erase(owner);
    }
}

bool ShipwrightNetworkRuntime::SaneDynamicObjectState(const NetworkDynamicObjectStatePacket& packet) {
    constexpr int32_t coordinateLimit = 1000000;
    return packet.sceneId >= 0 && packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           packet.roomId >= -1 && packet.roomId < 256 && packet.actorId >= 0 && packet.actorId < 0x1000 &&
           packet.actorParams >= INT16_MIN && packet.actorParams <= INT16_MAX && packet.homeX > -coordinateLimit &&
           packet.homeX < coordinateLimit && packet.homeY > -coordinateLimit && packet.homeY < coordinateLimit &&
           packet.homeZ > -coordinateLimit && packet.homeZ < coordinateLimit && packet.destroyed <= 1;
}

bool ShipwrightNetworkRuntime::SaneActorEvent(const NetworkActorEventPacket& packet) {
    NetworkDynamicObjectStatePacket objectState{ packet.sceneId, packet.roomId, packet.actorId,
                                                  packet.actorParams, packet.homeX, packet.homeY,
                                                  packet.homeZ, 0 };
    bool eventMatchesActor = false;
    switch (packet.eventType) {
        case NETWORK_ACTOR_EVENT_GRASS_CUT:
        case NETWORK_ACTOR_EVENT_GRASS_THROWN_BREAK:
            eventMatchesActor = packet.actorId == 0x125; // ACTOR_EN_KUSA
            break;
        case NETWORK_ACTOR_EVENT_BOULDER_BREAK:
            eventMatchesActor = packet.actorId == 0x127; // ACTOR_OBJ_BOMBIWA
            break;
        case NETWORK_ACTOR_EVENT_OWL_DEPART:
            eventMatchesActor = packet.actorId == 0x14D; // ACTOR_EN_OWL
            break;
        case NETWORK_ACTOR_EVENT_FISH_HOOK:
        case NETWORK_ACTOR_EVENT_FISH_RELEASE:
            eventMatchesActor = packet.actorId == 0xFE &&
                                ((packet.actorParams >= 100 && packet.actorParams <= 116) ||
                                 packet.actorParams == 400 || packet.actorParams == 401); // ACTOR_FISHING
            break;
    }
    const bool sanePosition = std::isfinite(packet.x) && std::isfinite(packet.y) && std::isfinite(packet.z) &&
                              packet.x > -1000000.0f && packet.x < 1000000.0f &&
                              packet.y > -1000000.0f && packet.y < 1000000.0f &&
                              packet.z > -1000000.0f && packet.z < 1000000.0f;
    return packet.eventId > 0 && packet.eventId < INT32_MAX && eventMatchesActor &&
           sanePosition && SaneDynamicObjectState(objectState);
}

void ShipwrightNetworkRuntime::ProcessServerDeathTransition(int32_t player,
                                                             const NetworkPlayerStatePacket& state) {
    const bool dead = (state.stateFlags & NETWORK_PLAYER_DEAD) != 0;
    const auto previous = mPlayerWasDead.find(player);
    const bool wasDead = previous != mPlayerWasDead.end() && previous->second;
    if (dead != wasDead) {
        Error("Network runtime: player %d state sequence %d changed %s -> %s", player, state.sequence,
              wasDead ? "dead" : "alive", dead ? "dead" : "alive");
    }
    if (dead) {
        // Keep replacing this with the newest unreliable pose. The reliable
        // dead transition guarantees that the server enters this state, while
        // later disposable snapshots let the final native death animation pose
        // settle before respawn.
        mLastDeadPlayerStates[player] = state;
        if (!wasDead) {
            mRespawnDeadlines[player] =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(NET_RESPAWN_MS);
        }
    }
    mPlayerWasDead[player] = dead;
}

void ShipwrightNetworkRuntime::UpdateServerRespawns() {
    const auto now = std::chrono::steady_clock::now();
    for (auto deadline = mRespawnDeadlines.begin(); deadline != mRespawnDeadlines.end();) {
        if (now < deadline->second) {
            ++deadline;
            continue;
        }

        const int32_t player = deadline->first;
        const auto finalPose = mLastDeadPlayerStates.find(player);
        if (finalPose != mLastDeadPlayerStates.end()) {
            CreateServerCorpse(finalPose->second);
            mLastDeadPlayerStates.erase(finalPose);
        }

        NetworkPlayerRespawnPacket respawn{ player };
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, respawn);
        if (player == 0) {
            mPlayerRespawns.push_back(respawn);
        } else {
            SendToPeer(player, NAMTPlayerRespawn, raw, kReliable);
        }
        Error("Network runtime: five-second respawn sent to player %d", player);
        // Stay dead until the client acknowledges this command with its first
        // living state. Late unreliable death snapshots therefore cannot open
        // a second respawn deadline after the reliable command is sent.
        mPlayerWasDead[player] = true;
        deadline = mRespawnDeadlines.erase(deadline);
    }
}

void ShipwrightNetworkRuntime::CreateServerCorpse(const NetworkPlayerStatePacket& finalPose) {
    int32_t corpseId = 0;
    for (int32_t attempt = 0; attempt < 31769; ++attempt) {
        const int32_t candidate = mNextServerCorpseId;
        --mNextServerCorpseId;
        if (mNextServerCorpseId < INT16_MIN) {
            mNextServerCorpseId = -1000;
        }
        if (mServerCorpses.count(candidate) == 0) {
            corpseId = candidate;
            break;
        }
    }
    if (corpseId == 0) {
        return;
    }

    NetworkPlayerStatePacket corpse = finalPose;
    corpse.playerId = corpseId;
    corpse.sequence = 1;
    corpse.speed = 0.0f;
    corpse.meleeWeaponState = 0;
    corpse.fishingState = 0;
    corpse.fishingFishActive = 0;
    corpse.stateFlags |= NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_DEAD;
    corpse.stateFlags &= ~NETWORK_PLAYER_READY_TO_FIRE;
    mServerCorpses[corpseId] = corpse;

    auto& sceneCorpses = mSceneCorpses[corpse.sceneId];
    sceneCorpses.push_back(corpseId);
    NetworkMessageRaw corpseRaw;
    EncodeAppPacketRaw(corpseRaw, corpse);
    Broadcast(NAMTPlayerState, corpseRaw, -1, kReliable);
    mPlayerStates.push_back(corpse);

    constexpr size_t maxCorpsesPerScene = 99;
    while (sceneCorpses.size() > maxCorpsesPerScene) {
        const int32_t oldestId = sceneCorpses.front();
        sceneCorpses.pop_front();
        mServerCorpses.erase(oldestId);
        NetworkPlayerRemovePacket removal{ oldestId, corpse.sceneId };
        NetworkMessageRaw removalRaw;
        EncodeAppPacketRaw(removalRaw, removal);
        Broadcast(NAMTPlayerRemove, removalRaw, -1, kReliable);
        mPlayerRemovals.push_back(removal);
    }
}

bool ShipwrightNetworkRuntime::SaneProjectileState(const NetworkProjectileStatePacket& packet) {
    const auto saneFloat = [](float value) { return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f; };
    return packet.projectileId > 0 && packet.projectileId < INT32_MAX && packet.sceneId >= 0 &&
           packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) && packet.active <= 1 &&
           packet.projectileKind <= NETWORK_PROJECTILE_BOMB && packet.phase <= NETWORK_BOMB_EXPLODING &&
           packet.projectileType <= 8 && saneFloat(packet.x) && saneFloat(packet.y) && saneFloat(packet.z) &&
           saneFloat(packet.velocityX) && saneFloat(packet.velocityY) && saneFloat(packet.velocityZ);
}

bool ShipwrightNetworkRuntime::SaneProjectileImpact(const NetworkProjectileImpactPacket& packet) {
    const auto saneFloat = [](float value) { return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f; };
    return packet.ownerPlayerId >= 0 && packet.projectileId > 0 && packet.projectileId < INT32_MAX &&
           packet.sceneId >= 0 && packet.sceneId < static_cast<int32_t>(NET_MAX_WORLD_LEVELS) &&
           saneFloat(packet.x) && saneFloat(packet.y) && saneFloat(packet.z);
}

bool ShipwrightNetworkRuntime::AcceptServerProjectileImpact(int32_t witness,
                                                             const NetworkProjectileImpactPacket& impact) {
    if (!mServer || !SaneProjectileImpact(impact) || witness == impact.ownerPlayerId) {
        return false;
    }
    const auto witnessState = mAuthoritativePlayerStates.find(witness);
    if (witnessState == mAuthoritativePlayerStates.end() || witnessState->second.sceneId != impact.sceneId ||
        (witnessState->second.stateFlags & NETWORK_PLAYER_DEAD) != 0) {
        return false;
    }
    auto projectile = std::find_if(mServerProjectiles.begin(), mServerProjectiles.end(), [&](const auto& entry) {
        return entry.second.state.playerId == impact.ownerPlayerId &&
               entry.second.state.projectileId == impact.projectileId;
    });
    if (projectile == mServerProjectiles.end() ||
        projectile->second.state.projectileKind != NETWORK_PROJECTILE_ARROW ||
        projectile->second.state.phase != NETWORK_ARROW_FLYING ||
        projectile->second.state.sceneId != impact.sceneId) {
        return false;
    }
    // Reports arrive behind the server simulation after snapshot delivery and
    // reliable-UDP processing. Permit that along-path latency, but never accept
    // an off-axis impact that the authoritative trajectory cannot reach.
    if (!PointNearDirectedPath({ projectile->second.state.x, projectile->second.state.y,
                                 projectile->second.state.z },
                               { projectile->second.velocityX, projectile->second.velocityY,
                                 projectile->second.velocityZ },
                               { impact.x, impact.y, impact.z }, 700.0f, 80.0f, 20.0f)) {
        return false;
    }
    projectile->second.state.x = impact.x;
    projectile->second.state.y = impact.y;
    projectile->second.state.z = impact.z;
    projectile->second.state.phase = NETWORK_ARROW_STUCK;
    projectile->second.velocityX = projectile->second.velocityY = projectile->second.velocityZ = 0.0f;
    projectile->second.impactedSince = std::chrono::steady_clock::now();
    NetworkMessageRaw raw;
    ++projectile->second.state.sequence;
    EncodeAppPacketRaw(raw, projectile->second.state);
    Broadcast(NAMTDynamicObjectStateRaw, raw, impact.ownerPlayerId, kReliable);
    mProjectileStates.push_back(projectile->second.state);
    RetainServerStuckArrow(projectile->first);
    return true;
}

bool ShipwrightNetworkRuntime::PlayerIsNearObject(
    int32_t player, const NetworkDynamicObjectStatePacket& objectState) const {
    const auto found = mAuthoritativePlayerStates.find(player);
    if (found == mAuthoritativePlayerStates.end() || found->second.sceneId != objectState.sceneId ||
        (found->second.stateFlags & NETWORK_PLAYER_DEAD) != 0) {
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
        if (!PointNearDirectedPath({ existing->second.state.x, existing->second.state.y,
                                     existing->second.state.z },
                                   { existing->second.velocityX, existing->second.velocityY,
                                     existing->second.velocityZ },
                                   { request.x, request.y, request.z }, 450.0f, 100.0f, 20.0f)) {
            return false;
        }
        existing->second.state.x = request.x;
        existing->second.state.y = request.y;
        existing->second.state.z = request.z;
        existing->second.state.active = 0;
        NetworkMessageRaw raw;
        ++existing->second.state.sequence;
        EncodeAppPacketRaw(raw, existing->second.state);
        Broadcast(NAMTDynamicObjectStateRaw, raw, player, kReliable);
        mProjectileStates.push_back(existing->second.state);
        mServerProjectiles.erase(existing);
        return true;
    }
    if (existing != mServerProjectiles.end()) {
        if (existing->second.state.projectileKind == NETWORK_PROJECTILE_ARROW &&
            existing->second.state.phase == NETWORK_ARROW_FLYING && request.phase == NETWORK_ARROW_STUCK) {
            if (PointNearDirectedPath({ existing->second.state.x, existing->second.state.y,
                                        existing->second.state.z },
                                      { existing->second.velocityX, existing->second.velocityY,
                                        existing->second.velocityZ },
                                      { request.x, request.y, request.z }, 450.0f, 100.0f, 20.0f)) {
                existing->second.state.x = request.x;
                existing->second.state.y = request.y;
                existing->second.state.z = request.z;
                existing->second.state.phase = NETWORK_ARROW_STUCK;
                existing->second.velocityX = existing->second.velocityY = existing->second.velocityZ = 0.0f;
                existing->second.impactedSince = std::chrono::steady_clock::now();
                NetworkMessageRaw impactRaw;
                ++existing->second.state.sequence;
                EncodeAppPacketRaw(impactRaw, existing->second.state);
                Broadcast(NAMTDynamicObjectStateRaw, impactRaw, player, kReliable);
                mProjectileStates.push_back(existing->second.state);
                RetainServerStuckArrow(key);
            }
            return true;
        }
        if (existing->second.state.projectileKind == NETWORK_PROJECTILE_BOMB &&
            existing->second.state.phase == NETWORK_BOMB_HELD &&
            (request.phase == NETWORK_BOMB_HELD || request.phase == NETWORK_BOMB_RELEASED)) {
            const auto owner = mAuthoritativePlayerStates.find(player);
            if (owner == mAuthoritativePlayerStates.end() || owner->second.sceneId != request.sceneId ||
                (owner->second.stateFlags & NETWORK_PLAYER_DEAD) != 0) {
                return false;
            }
            const float heldDx = request.x - owner->second.x;
            const float heldDy = request.y - (owner->second.y + 35.0f);
            const float heldDz = request.z - owner->second.z;
            if (heldDx * heldDx + heldDy * heldDy + heldDz * heldDz > 90.0f * 90.0f) {
                return false;
            }
            existing->second.heldOffsetX = request.x - owner->second.x;
            existing->second.heldOffsetY = request.y - owner->second.y;
            existing->second.heldOffsetZ = request.z - owner->second.z;
            existing->second.state.x = request.x;
            existing->second.state.y = request.y;
            existing->second.state.z = request.z;
            existing->second.groundY = owner->second.y;
            if (request.phase == NETWORK_BOMB_HELD) {
                return true;
            }
            existing->second.state.phase = NETWORK_BOMB_RELEASED;
            constexpr float maximumBombSpeed = 500.0f;
            const float speed = std::sqrt(request.velocityX * request.velocityX + request.velocityY * request.velocityY +
                                          request.velocityZ * request.velocityZ);
            const float scale = speed > maximumBombSpeed ? maximumBombSpeed / speed : 1.0f;
            existing->second.velocityX = request.velocityX * scale;
            existing->second.velocityY = request.velocityY * scale;
            existing->second.velocityZ = request.velocityZ * scale;
            ++existing->second.state.sequence;
            NetworkMessageRaw releaseRaw;
            EncodeAppPacketRaw(releaseRaw, existing->second.state);
            Broadcast(NAMTDynamicObjectStateRaw, releaseRaw, player, kReliable);
            mProjectileStates.push_back(existing->second.state);
        }
        return true;
    }

    const auto shooter = mAuthoritativePlayerStates.find(player);
    if (shooter == mAuthoritativePlayerStates.end() || shooter->second.sceneId != request.sceneId ||
        (shooter->second.stateFlags & NETWORK_PLAYER_DEAD) != 0) {
        return false;
    }
    if ((request.projectileKind == NETWORK_PROJECTILE_ARROW &&
         (shooter->second.itemAction < 8 || shooter->second.itemAction > 14)) ||
        (request.projectileKind == NETWORK_PROJECTILE_BOMB && shooter->second.itemAction != 18)) {
        return false;
    }
    if ((request.projectileKind == NETWORK_PROJECTILE_ARROW && request.phase != NETWORK_ARROW_FLYING) ||
        (request.projectileKind == NETWORK_PROJECTILE_BOMB && request.phase != NETWORK_BOMB_HELD)) {
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
    projectile.state.sequence = 1;
    projectile.state.playerId = player;
    projectile.state.projectileId = mNextServerProjectileId++;
    projectile.state.x = shooter->second.x;
    projectile.state.y = shooter->second.y + 42.0f;
    projectile.state.z = shooter->second.z;
    projectile.groundY = shooter->second.y;
    projectile.spawned = projectile.lastUpdate = projectile.lastBroadcast = now;
    if (request.projectileKind == NETWORK_PROJECTILE_BOMB) {
        const float heldDx = request.x - shooter->second.x;
        const float heldDy = request.y - (shooter->second.y + 35.0f);
        const float heldDz = request.z - shooter->second.z;
        if (heldDx * heldDx + heldDy * heldDy + heldDz * heldDz > 90.0f * 90.0f) {
            return false;
        }
        projectile.state.phase = NETWORK_BOMB_HELD;
        projectile.state.x = request.x;
        projectile.state.y = request.y;
        projectile.state.z = request.z;
        projectile.heldOffsetX = request.x - shooter->second.x;
        projectile.heldOffsetY = request.y - shooter->second.y;
        projectile.heldOffsetZ = request.z - shooter->second.z;
        projectile.velocityX = projectile.velocityY = projectile.velocityZ = 0.0f;
    } else {
        // The native arrow actor supplies the exact nock/release origin. It is
        // accepted only inside the player's immediate pose envelope; direction
        // and speed remain server-derived from the accepted aim state.
        const float originDx = request.x - shooter->second.x;
        const float originDy = request.y - (shooter->second.y + 42.0f);
        const float originDz = request.z - shooter->second.z;
        if (originDx * originDx + originDy * originDy + originDz * originDz > 90.0f * 90.0f) {
            return false;
        }
        projectile.state.x = request.x;
        projectile.state.y = request.y;
        projectile.state.z = request.z;
        // Direction is derived exclusively from the latest server-accepted
        // Link aim. The client supplies the native hand origin and arrow type,
        // but cannot steer a projectile independently of its visible pose.
        const short launchYaw = shooter->second.aimYaw;
        const short launchPitch = shooter->second.aimPitch;
        constexpr float arrowSpeedPerSecond = 3000.0f;
        const float yaw = launchYaw * (3.14159265358979323846f / 32768.0f);
        const float pitch = launchPitch * (3.14159265358979323846f / 32768.0f);
        const float horizontal = std::cos(pitch) * arrowSpeedPerSecond;
        projectile.velocityX = std::sin(yaw) * horizontal;
        projectile.velocityY = -std::sin(pitch) * arrowSpeedPerSecond;
        projectile.velocityZ = std::cos(yaw) * horizontal;
        // Match Math_Atan2S(speedXZ, -velocity.y): despite its parameter
        // names, Math_Atan2S(x, y) computes atan2(y, x). A level native arrow
        // therefore has a display pitch of zero, not 0x4000.
        projectile.state.rotationX = static_cast<short>(
            std::atan2(-projectile.velocityY, horizontal) * (32768.0f / 3.14159265358979323846f));
        projectile.state.rotationY = launchYaw;
        projectile.state.rotationZ = 0;
    }
    mServerProjectiles.emplace(key, projectile);

    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, projectile.state);
    Broadcast(NAMTDynamicObjectStateRaw, raw, player, kReliable);
    mProjectileStates.push_back(projectile.state);
    return true;
}

void ShipwrightNetworkRuntime::RetainServerStuckArrow(
    const std::pair<int32_t, int32_t>& currentKey) {
    constexpr size_t maxStuckArrowsPerPlayerPerScene = 99;
    const auto current = mServerProjectiles.find(currentKey);
    if (current == mServerProjectiles.end() ||
        current->second.state.projectileKind != NETWORK_PROJECTILE_ARROW ||
        current->second.state.phase != NETWORK_ARROW_STUCK) {
        return;
    }

    const int32_t owner = current->second.state.playerId;
    const int32_t scene = current->second.state.sceneId;
    size_t count = 0;
    for (const auto& [key, projectile] : mServerProjectiles) {
        (void)key;
        if (projectile.state.playerId == owner && projectile.state.sceneId == scene &&
            projectile.state.projectileKind == NETWORK_PROJECTILE_ARROW &&
            projectile.state.phase == NETWORK_ARROW_STUCK) {
            ++count;
        }
    }

    while (count > maxStuckArrowsPerPlayerPerScene) {
        auto oldest = mServerProjectiles.end();
        for (auto candidate = mServerProjectiles.begin(); candidate != mServerProjectiles.end(); ++candidate) {
            const ServerProjectile& projectile = candidate->second;
            if (candidate->first == currentKey || projectile.state.playerId != owner ||
                projectile.state.sceneId != scene ||
                projectile.state.projectileKind != NETWORK_PROJECTILE_ARROW ||
                projectile.state.phase != NETWORK_ARROW_STUCK) {
                continue;
            }
            if (oldest == mServerProjectiles.end() ||
                projectile.impactedSince < oldest->second.impactedSince) {
                oldest = candidate;
            }
        }
        if (oldest == mServerProjectiles.end()) {
            break;
        }

        oldest->second.state.active = 0;
        ++oldest->second.state.sequence;
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, oldest->second.state);
        Broadcast(NAMTDynamicObjectStateRaw, raw, owner, kReliable);
        mProjectileStates.push_back(oldest->second.state);
        mServerProjectiles.erase(oldest);
        --count;
    }
}

void ShipwrightNetworkRuntime::UpdateServerProjectiles() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = mServerProjectiles.begin(); it != mServerProjectiles.end();) {
        ServerProjectile& projectile = it->second;
        const float previousX = projectile.state.x;
        const float previousY = projectile.state.y;
        const float previousZ = projectile.state.z;
        const float deltaSeconds =
            std::min(0.1f, std::chrono::duration<float>(now - projectile.lastUpdate).count());
        projectile.lastUpdate = now;
        if (projectile.state.projectileKind == NETWORK_PROJECTILE_BOMB) {
            const auto owner = mAuthoritativePlayerStates.find(projectile.state.playerId);
            if (projectile.state.phase == NETWORK_BOMB_HELD && owner != mAuthoritativePlayerStates.end()) {
                projectile.state.x = owner->second.x + projectile.heldOffsetX;
                projectile.state.y = owner->second.y + projectile.heldOffsetY;
                projectile.state.z = owner->second.z + projectile.heldOffsetZ;
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
                ++projectile.state.sequence;
                NetworkMessageRaw explosionRaw;
                EncodeAppPacketRaw(explosionRaw, projectile.state);
                Broadcast(NAMTDynamicObjectStateRaw, explosionRaw, projectile.state.playerId, kReliable);
                mProjectileStates.push_back(projectile.state);
            }
            if (projectile.state.phase == NETWORK_BOMB_EXPLODING &&
                now - projectile.explodingSince >= std::chrono::milliseconds(500)) {
                projectile.state.active = 0;
                NetworkMessageRaw raw;
                ++projectile.state.sequence;
                EncodeAppPacketRaw(raw, projectile.state);
                Broadcast(NAMTDynamicObjectStateRaw, raw, projectile.state.playerId, kReliable);
                mProjectileStates.push_back(projectile.state);
                it = mServerProjectiles.erase(it);
                continue;
            }
        } else {
            if (projectile.state.phase == NETWORK_ARROW_STUCK) {
                ++it;
                continue;
            }
            // EnArrow starts at timer 12 and enables -0.4 units/frame^2
            // gravity once the timer drops below 7.2. At the native 20 Hz
            // gameplay rate that is five frames (250 ms) and -160 units/s^2.
            if (now - projectile.spawned >= std::chrono::milliseconds(250)) {
                projectile.velocityY -= 160.0f * deltaSeconds;
            }
            projectile.state.x += projectile.velocityX * deltaSeconds;
            projectile.state.y += projectile.velocityY * deltaSeconds;
            projectile.state.z += projectile.velocityZ * deltaSeconds;
            const float horizontalSpeed = std::sqrt(projectile.velocityX * projectile.velocityX +
                                                    projectile.velocityZ * projectile.velocityZ);
            projectile.state.rotationX = static_cast<short>(
                std::atan2(-projectile.velocityY, horizontalSpeed) *
                (32768.0f / 3.14159265358979323846f));

            const float segmentX = projectile.state.x - previousX;
            const float segmentY = projectile.state.y - previousY;
            const float segmentZ = projectile.state.z - previousZ;
            const float segmentLengthSquared = segmentX * segmentX + segmentY * segmentY + segmentZ * segmentZ;
            ServerCollisionPoint staticImpact{};
            const bool hitStatic = mCollisionWorld.SegmentCast(
                projectile.state.sceneId, { previousX, previousY, previousZ },
                { projectile.state.x, projectile.state.y, projectile.state.z }, staticImpact);
            float staticHitRatio = 2.0f;
            if (hitStatic && segmentLengthSquared > 0.00001f) {
                staticHitRatio = std::clamp(((staticImpact.x - previousX) * segmentX +
                                             (staticImpact.y - previousY) * segmentY +
                                             (staticImpact.z - previousZ) * segmentZ) /
                                                segmentLengthSquared,
                                            0.0f, 1.0f);
            }

            int32_t hitPlayerId = -1;
            float playerHitRatio = 2.0f;
            for (const auto& [targetId, target] : mAuthoritativePlayerStates) {
                if (targetId == projectile.state.playerId || target.sceneId != projectile.state.sceneId ||
                    (target.stateFlags & (NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_DEAD)) != NETWORK_PLAYER_VISIBLE) {
                    continue;
                }
                float candidateRatio = 0.0f;
                // The native Link cylinder is radius 12, height 60. Server
                // target poses can be one 50 ms snapshot behind; at Link's
                // maximum normal movement that is roughly six world units.
                // Add exactly that bounded envelope so a visibly intersecting
                // swept arrow is not lost between target snapshots.
                if (SegmentVerticalCylinderFirstHit({ previousX, previousY, previousZ },
                                                    { projectile.state.x, projectile.state.y, projectile.state.z },
                                                    { target.x, target.y - 3.0f, target.z }, 18.0f, 66.0f,
                                                    candidateRatio) &&
                    candidateRatio < playerHitRatio) {
                    playerHitRatio = candidateRatio;
                    hitPlayerId = targetId;
                }
            }

            // Resolve the first contact along the swept arrow path. A player in
            // front of a wall is hit; a wall in front of a player blocks it.
            if (hitStatic && staticHitRatio <= playerHitRatio) {
                projectile.state.x = staticImpact.x;
                projectile.state.y = staticImpact.y;
                projectile.state.z = staticImpact.z;
                projectile.state.phase = NETWORK_ARROW_STUCK;
                projectile.velocityX = projectile.velocityY = projectile.velocityZ = 0.0f;
                projectile.impactedSince = now;
                NetworkMessageRaw raw;
                ++projectile.state.sequence;
                EncodeAppPacketRaw(raw, projectile.state);
                Broadcast(NAMTDynamicObjectStateRaw, raw, projectile.state.playerId, kReliable);
                mProjectileStates.push_back(projectile.state);
                RetainServerStuckArrow(it->first);
                ++it;
                continue;
            }
            if (hitPlayerId >= 0) {
                projectile.state.x = previousX + segmentX * playerHitRatio;
                projectile.state.y = previousY + segmentY * playerHitRatio;
                projectile.state.z = previousZ + segmentZ * playerHitRatio;
                const short impactYaw = static_cast<short>(
                    std::atan2(projectile.velocityX, projectile.velocityZ) *
                    (32768.0f / 3.14159265358979323846f));
                NetworkPlayerDamagePacket damage{ projectile.state.playerId, hitPlayerId, 8, impactYaw };
                NetworkMessageRaw damageRaw;
                EncodeAppPacketRaw(damageRaw, damage);
                if (hitPlayerId == 0) {
                    mPlayerDamage.push_back(damage);
                } else {
                    SendToPeer(hitPlayerId, NAMTPlayerDamage, damageRaw, kReliable);
                }
                projectile.state.active = 0;
                NetworkMessageRaw raw;
                ++projectile.state.sequence;
                EncodeAppPacketRaw(raw, projectile.state);
                Broadcast(NAMTDynamicObjectStateRaw, raw, projectile.state.playerId, kReliable);
                mProjectileStates.push_back(projectile.state);
                it = mServerProjectiles.erase(it);
                continue;
            }
        }
        if (now - projectile.lastBroadcast < std::chrono::milliseconds(50)) {
            ++it;
            continue;
        }
        projectile.lastBroadcast = now;
        NetworkMessageRaw raw;
        ++projectile.state.sequence;
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
