#include "SecureTransportChannel.h"

#include <utility>

namespace Game::Multiplayer {

namespace {

const NetMsgFlags kReliable =
    static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

bool IsReliable(NetMsgFlags flags) {
    return (static_cast<int>(flags) & static_cast<int>(NMFGuaranteed)) != 0;
}

Game::Replication::ReplicationPriority PriorityFor(NetAppMessageType type) {
    switch (type) {
        case NAMTPlayerSnapshot:
            return Game::Replication::ReplicationPriority::High;
        case NAMTProjectileState:
        case NAMTFishingState:
        case NAMTLureState:
            return Game::Replication::ReplicationPriority::Normal;
        case NAMTObjectiveState:
        case NAMTStructureState:
        case NAMTStrategicTopology:
            return Game::Replication::ReplicationPriority::Low;
        default:
            return Game::Replication::ReplicationPriority::Normal;
    }
}

} // namespace

SecureTransportChannel::SecureTransportChannel(
    cCryptoSession& clientCrypto,
    ServerSessionManager& serverSessions,
    Game::Replication::ServerReplicationCoordinator& replication)
    : mClientCrypto(clientCrypto), mServerSessions(serverSessions),
      mReplication(replication) {
}

void SecureTransportChannel::SetDelivery(RawTransportDelivery delivery) {
    mDelivery = std::move(delivery);
}

void SecureTransportChannel::ResetCounters() {
    mInboundBytes = 0;
    mOutboundBytes = 0;
}

void SecureTransportChannel::RecordInbound(size_t bytes) {
    mInboundBytes += bytes;
}

bool SecureTransportChannel::PrepareClientMessage(
    char* buffer, int32_t size, std::string& decrypted, const char*& message,
    int32_t& messageSize) {
    message = buffer;
    messageSize = size;
    if (!buffer || size < static_cast<int32_t>(sizeof(NetAppMessageHeader)) ||
        static_cast<size_t>(size) > NET_MAX_ENCRYPTED_BYTES) {
        return false;
    }
    const auto* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (!ValidAppMessageType(header->type)) return false;
    if (header->type == NAMTEncrypted) {
        if (!mClientCrypto.decrypt(buffer, size, decrypted)) return false;
        message = decrypted.data();
        messageSize = static_cast<int32_t>(decrypted.size());
        return true;
    }
    return !mClientCrypto.ready() || header->type == NAMTKeyAccept;
}

bool SecureTransportChannel::PrepareServerMessage(
    int32_t sender, char* buffer, int32_t size, std::string& decrypted,
    const char*& message, int32_t& messageSize) {
    message = buffer;
    messageSize = size;
    if (!buffer || size < static_cast<int32_t>(sizeof(NetAppMessageHeader)) ||
        static_cast<size_t>(size) > NET_MAX_ENCRYPTED_BYTES) {
        return false;
    }
    const auto* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (!ValidAppMessageType(header->type)) return false;
    if (header->type == NAMTEncrypted) {
        cCryptoSession* crypto = mServerSessions.CryptoFor(sender);
        if (!crypto || !crypto->decrypt(buffer, size, decrypted)) return false;
        message = decrypted.data();
        messageSize = static_cast<int32_t>(decrypted.size());
        return true;
    }
    return header->type == NAMTKeyHello;
}

bool SecureTransportChannel::SendRawToServer(
    const std::string& payload, NetMsgFlags flags) {
    if (payload.empty() || !mDelivery.sendToServer ||
        !mDelivery.sendToServer(payload, flags)) {
        return false;
    }
    mOutboundBytes += payload.size();
    return true;
}

bool SecureTransportChannel::SendRawToPeer(
    int32_t peer, const std::string& payload, NetMsgFlags flags) {
    if (peer <= 0 || payload.empty() || !mDelivery.sendToPeer ||
        !mDelivery.sendToPeer(peer, payload, flags)) {
        return false;
    }
    mOutboundBytes += payload.size();
    return true;
}

bool SecureTransportChannel::SendPlainToServer(
    NetAppMessageType type, const NetworkMessageRaw& raw) {
    return SendRawToServer(BuildAppRawMessage(type, raw), kReliable);
}

bool SecureTransportChannel::SendPlainToPeer(
    int32_t peer, NetAppMessageType type, const NetworkMessageRaw& raw) {
    return SendRawToPeer(peer, BuildAppRawMessage(type, raw), kReliable);
}

bool SecureTransportChannel::SendToServer(
    NetAppMessageType type, const NetworkMessageRaw& raw, NetMsgFlags flags) {
    return SendEncryptedPayloadToServer(BuildAppRawMessage(type, raw), flags);
}

bool SecureTransportChannel::SendToPeer(
    int32_t peer, NetAppMessageType type, const NetworkMessageRaw& raw,
    NetMsgFlags flags, Game::Replication::ReplicationStreamKey streamKey) {
    const std::string payload = BuildAppRawMessage(type, raw);
    constexpr size_t kEncryptedEnvelopeOverhead = 64;
    const size_t budgetBytes = payload.size() + kEncryptedEnvelopeOverhead;
    const bool highPriority =
        (static_cast<int>(flags) & static_cast<int>(NMFHighPriority)) != 0;
    const auto submission = mReplication.Submit(
        peer, streamKey, PriorityFor(type), payload, highPriority,
        IsReliable(flags), budgetBytes);
    if (submission == Game::Replication::ReplicationSubmission::Queued) return true;
    if (submission == Game::Replication::ReplicationSubmission::Rejected) return false;
    return SendEncryptedPayloadToPeer(peer, payload, flags);
}

bool SecureTransportChannel::SendEncryptedPayloadToServer(
    const std::string& payload, NetMsgFlags flags) {
    if (!mClientCrypto.ready()) return false;
    std::string encrypted;
    return mClientCrypto.encrypt(payload, encrypted) &&
           SendRawToServer(encrypted, flags);
}

bool SecureTransportChannel::SendEncryptedPayloadToPeer(
    int32_t peer, const std::string& payload, NetMsgFlags flags) {
    cCryptoSession* crypto = mServerSessions.CryptoFor(peer);
    if (!crypto || !crypto->ready()) return false;
    std::string encrypted;
    return crypto->encrypt(payload, encrypted) &&
           SendRawToPeer(peer, encrypted, flags);
}

} // namespace Game::Multiplayer
